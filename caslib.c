/**************************************************************************/
/*                                                                        */
/* file:         caslib.c                                                 */
/* description:  Library for MSX CAS to WAV conversion                    */
/*               Refactored and clarified versions of cas2wav functions   */
/*                                                                        */
/**************************************************************************/

#include "caslib.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* CAS file format constants (definitions) */
const char HEADER[8]  = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };            /* Block sync marker */
const char ASCII[10]  = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };  /* ASCII file type marker */
const char BIN[10]    = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };  /* Binary file type marker */
const char BASIC[10]  = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };  /* BASIC file type marker */

/* MSX tape EOF marker (Ctrl-Z) */
#define EOF_MARKER 0x1A

/* Baudrate: 1200 or 2400 baud (configurable via command line) */
int BAUDRATE = 1200;

/* Initialize write buffer context */
void initWriteBuffer(WriteBuffer *wb, FILE *file, int baudrate, int output_frequency)
{
  wb->file = file;
  wb->position = 0;
  wb->baudrate = baudrate;
  wb->output_frequency = output_frequency;
}

/* Flush buffered data to file */
void flushWriteBuffer(WriteBuffer *wb)
{
  if (wb->position > 0) {
    fwrite(wb->buffer, 1, wb->position, wb->file);
    wb->position = 0;
  }
}

/* Write a single byte to buffer, flushing when full */
void putByte(WriteBuffer *wb, unsigned char byte)
{
  wb->buffer[wb->position++] = byte;
  if (wb->position >= WRITE_BUFFER_SIZE)
    flushWriteBuffer(wb);
}

/* Write silence samples (DC offset 128) for tape mechanical settling
 * Note: sample_count is in PCM samples (bytes).
 * At 43200 Hz: 1 sample = 1/43200 second = ~23 microseconds */
void writeSilence(WriteBuffer *wb, uint32_t sample_count)
{
  for (uint32_t n = 0; n < sample_count; n++)
    putByte(wb, 128);
}

/* 360-entry sine lookup table (1 cycle @ 1-degree resolution) */
static unsigned char sine_table[360];
static int sine_table_initialized = 0;

/* Initialize sine table once, converting sin() to unsigned 8-bit PCM */
static void init_sine_table(void)
{
  if (sine_table_initialized)
    return;

  /* Generate 360 sine samples (1 complete cycle at 1-degree resolution) */
  for (int i = 0; i < 360; i++) {
    /* sin(angle) * 127.0 + 128.0 converts [-1,1] to unsigned [1,255] */
    double angle = 2.0 * M_PI * i / 360.0;
    sine_table[i] = (unsigned char)(sin(angle) * 127.0 + 128.0);
  }
  sine_table_initialized = 1;
}

/* Generate FSK pulse: 0 bit→1200Hz (36 samples), 1 bit→2400Hz (18 samples) */
void writePulse(WriteBuffer *wb, uint32_t freq)
{
  uint32_t n;

  /* Initialize sine table on first call */
  if (!sine_table_initialized)
    init_sine_table();

  /* Pulse length in samples: at 1200 baud, 1200Hz=36, 2400Hz=18 */
  double length = wb->output_frequency / (wb->baudrate * (freq / 1200.0));

  /* Table step: 360/length. For 1200Hz step=10, for 2400Hz step=20 */
  double table_step = 360.0 / length;

  /* Generate and output each sample of the sine wave using lookup table */
  for (n = 0; n < (uint32_t)length; n++) {
    /* Math guarantees table_index < 360, no modulo needed */
    unsigned int table_index = (unsigned int)(n * table_step);
    unsigned char pcm_sample = sine_table[table_index];
    putByte(wb, pcm_sample);
  }
}

/* Generate synchronization pulses (SHORT_PULSE tones scaled by BAUDRATE)
 * Note: pulse_count is in complete sine wave cycles.
 * At 1200 baud: 1 pulse (2400 Hz) = 18 samples = ~417 microseconds
 * At 2400 baud: 1 pulse (2400 Hz) =  9 samples = ~208 microseconds */
void writeSync(WriteBuffer *wb, uint32_t pulse_count)
{
  /* Scale pulse count by baudrate/1200 to maintain same duration */
  for (int i = 0; i < (int)(pulse_count*(wb->baudrate / 1200.0)); i++)
    writePulse(wb, SHORT_PULSE);
}

/* Serial encoding: START(0) + 8 bits LSB-first + STOP(1); bit-1→2400Hz, bit-0→1200Hz */
void writeByte(WriteBuffer *wb, int byte)
{
  /* START: 0 */
  writePulse(wb, LONG_PULSE);

  /* DATA: 8 bits LSB-first; bit-1→2×SHORT, bit-0→LONG */
  for (int i = 0; i < 8; i++) {
    if (byte & 1) {
      /* Bit=1: two 2400Hz pulses */
      writePulse(wb, SHORT_PULSE);
      writePulse(wb, SHORT_PULSE);
    } else {
      /* Bit=0: one 1200Hz pulse */
      writePulse(wb, LONG_PULSE);
    }
    /* Next bit */
    byte = byte >> 1;
  }

  /* STOP: 11 (4×SHORT=2 bits) */
  for (int i = 0; i < 4; i++)
    writePulse(wb, SHORT_PULSE);
}

/* Transmit data block until HEADER marker or EOF; sets *eof if EOF_MARKER found
 *
 * Uses an 8-byte sliding window in memory:
 * - Read bytes one at a time (no seeking overhead)
 * - Maintain last 8 bytes in memory window
 * - When window fills, transmit oldest byte before adding new one
 * - Check for HEADER pattern after each byte added */
void writeData(FILE *input, WriteBuffer *wb, uint32_t *position, bool *eof)
{
  char window[8] = {0};
  int window_size = 0;
  
  *eof = false;

  while (1) {
    int byte = fgetc(input);
    if (byte == EOF)
      break;
    
    /* If window is full, transmit oldest byte before adding new one */
    if (window_size == 8) {
      writeByte(wb, window[0]);
      if (window[0] == EOF_MARKER)
        *eof = true;
      (*position)++;
      /* Shift window left to make room */
      memmove(window, window + 1, 7);
      window_size = 7;
    }
    
    /* Add new byte to end of window */
    window[window_size++] = byte;
    
    /* Check if window now contains complete HEADER */
    if (window_size == 8 && !memcmp(window, HEADER, 8)) {
      /* Found HEADER - position file pointer before it and stop */
      fseek(input, *position, SEEK_SET);
      return;
    }
  }
  
  /* Transmit any remaining bytes in window at EOF */
  for (int i = 0; i < window_size; i++) {
    writeByte(wb, window[i]);
    if (window[i] == EOF_MARKER)
      *eof = true;
  }
  *position += window_size;
}

