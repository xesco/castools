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
const char HEADER[8]  = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
const char ASCII[10]  = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };  /* ASCII file type marker */
const char BIN[10]    = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };  /* Binary file type marker */
const char BASIC[10]  = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };  /* BASIC file type marker */

/* Baudrate: 1200 or 2400 baud (configurable via command line) */
int BAUDRATE = 1200;

/* Initialize write buffer context */
void initWriteBuffer(WriteBuffer *wb, FILE *file)
{
  wb->file = file;
  wb->position = 0;
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

/* Write silence samples (DC offset 128) for tape mechanical settling */
void writeSilence(WriteBuffer *wb, uint32_t s)
{
  for (uint32_t n = 0; n < s; n++)
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
void writePulse(WriteBuffer *wb, uint32_t f)
{
  uint32_t n;

  /* Initialize sine table on first call */
  if (!sine_table_initialized)
    init_sine_table();

  /* Pulse length in samples: at 1200 baud, 1200Hz=36, 2400Hz=18 */
  double length = OUTPUT_FREQUENCY / (BAUDRATE * (f / 1200.0));

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

/* Generate synchronization pulses (SHORT_PULSE tones scaled by BAUDRATE) */
void writeSync(WriteBuffer *wb, uint32_t s)
{
  /* Scale pulse count by BAUDRATE/1200 to maintain same duration */
  for (int i = 0; i < (int)(s*(BAUDRATE / 1200.0)); i++)
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

  /* STOP: 1 (4×SHORT=2 bits) */
  for (int i = 0; i < 4; i++)
    writePulse(wb, SHORT_PULSE);
}

/* Transmit data block until HEADER marker or EOF; sets *eof if 0x1A found */
void writeData(FILE *input, WriteBuffer *wb, uint32_t *position, bool *eof)
{
  int read;
  int i;
  char buffer[8];

  *eof = false;

  /* Read 8-byte chunks; stop on HEADER, transmit first byte otherwise */
  while ((read = fread(buffer, 1, 8, input)) == 8) {
    if (!memcmp(buffer, HEADER, 8))
      return;  /* Stop at next HEADER */
    writeByte(wb, buffer[0]);
    if (buffer[0] == 0x1a)
      *eof = true;  /* EOF marker */
    fseek(input, ++(*position), SEEK_SET);
  }

  /* Transmit remaining bytes from partial read at EOF */
  for (i = 0; i < read; i++)
    writeByte(wb, buffer[i]);
  if (read > 0 && buffer[0] == 0x1a)
    *eof = true;

  *position += read;

  return;
}

