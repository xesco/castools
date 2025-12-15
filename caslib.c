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
  wb->pos = 0;
  wb->baudrate = baudrate;
  wb->output_frequency = output_frequency;
}

/* Flush buffered data to file */
void flushWriteBuffer(WriteBuffer *wb)
{
  if (wb->pos > 0) {
    fwrite(wb->buffer, 1, wb->pos, wb->file);
    wb->pos = 0;
  }
}

/* Write a single byte to buffer, flushing when full */
void putByte(WriteBuffer *wb, unsigned char byte)
{
  wb->buffer[wb->pos++] = byte;
  if (wb->pos >= WRITE_BUFFER_SIZE)
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

/* Generate a single FSK pulse at specified frequency (one complete sine wave cycle) */
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

/* Write a 0-bit: one 1200 Hz pulse */
static void write0(WriteBuffer *wb)
{
  writePulse(wb, LONG_PULSE);
}

/* Write a 1-bit: two 2400 Hz pulses */
static void write1(WriteBuffer *wb)
{
  writePulse(wb, SHORT_PULSE);
  writePulse(wb, SHORT_PULSE);
}

/* Generate synchronization header (continuous 1-bits for MSX BIOS sync)
 * bits: base 1-bit count at 1200 baud (automatically scaled for other rates to maintain duration).
 * Examples with bits=8000:
 *   At 1200 baud:  8000 bits transmitted = ~6.67 seconds
 *   At 2400 baud: 16000 bits transmitted = ~6.67 seconds (same duration) */
void writeSync(WriteBuffer *wb, uint32_t bits)
{
  /* Scale bit count by baudrate/1200 to maintain same duration */
  for (int i = 0; i < (int)(bits * (wb->baudrate / 1200.0)); i++)
    write1(wb);
}

/* Serial encoding: START(0) + 8 bits LSB-first + STOP(1,1) */
void writeByte(WriteBuffer *wb, int byte)
{
  write0(wb);  /* START bit */

  /* DATA: 8 bits, LSB first */
  for (int i = 0; i < 8; i++) {
    if (byte & 1)
      write1(wb);
    else
      write0(wb);
    byte = byte >> 1;
  }

  /* STOP: two 1-bits */
  write1(wb);
  write1(wb);
}

/* Transmit data block until HEADER marker or EOF; sets *eof if EOF_MARKER found
 * Returns the new position after processing.
 * Works with in-memory CAS data for simple and efficient processing. */
size_t writeData(const unsigned char *cas, size_t cas_size, WriteBuffer *wb, size_t pos, bool *eof)
{
  *eof = false;
  
  /* Transmit bytes until we find a HEADER or reach end of data */
  while ((pos + sizeof(HEADER)) <= cas_size) {
    /* Check if current position starts a HEADER */
    if (!memcmp(cas+pos, HEADER, sizeof(HEADER))) {
      return pos;  /* Stop before HEADER */
    }
    
    /* Transmit this byte */
    writeByte(wb, cas[pos]);
    
    /* Check for EOF marker */
    if (cas[pos] == EOF_MARKER) {
      *eof = true;
    }
    pos++;
  }
  
  /* Transmit any remaining bytes at end of file */
  while (pos < cas_size) {
    writeByte(wb, cas[pos]);
    pos++;
  }
  
  return pos;
}

/* Get the size of an open file */
long getFileSize(FILE *file)
{
  long size;
  long current = ftell(file);
  
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, current, SEEK_SET);
  
  return size;
}

