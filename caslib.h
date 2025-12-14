#ifndef CASLIB_H
#define CASLIB_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef bool
#define true   1
#define false  0
#define bool   int
#endif

/* CAS file format constants */

/* MSX CAS file HEADER marker (8 bytes) - identifies block boundaries */
extern const char HEADER[8];
/* File type identifiers (10 bytes each) */
extern const char ASCII[10];   /* ASCII text file */
extern const char BIN[10];     /* Binary machine code */
extern const char BASIC[10];   /* BASIC program */

/* Audio output configuration */

/* Audio output sample rate: 43200 Hz, 8-bit mono PCM */
#define OUTPUT_FREQUENCY  43200

/* Silence durations (in samples = bytes) */
#define SHORT_SILENCE     OUTPUT_FREQUENCY      /* 1 second  */
#define LONG_SILENCE      OUTPUT_FREQUENCY * 2  /* 2 seconds */

/* FSK (Frequency Shift Keying) tones for bit encoding */
#define LONG_PULSE        1200   /* 0 bit: 1200 Hz */
#define SHORT_PULSE       2400   /* 1 bit: 2400 Hz */

/* Synchronization header pulse counts at 1200 baud */
#define SYNC_INITIAL      16000   /* Initial sync before first data block */
#define SYNC_BLOCK        4000    /* Sync before subsequent data blocks */

/* Baud rate (bits per second) - can be 1200 or 2400 */
extern int BAUDRATE;

/* WAV file format constants */
#define PCM_WAVE_FORMAT   1
#define MONO              1
#define STEREO            2

/* RIFF/WAV format header structure for audio output
   Specifies 8-bit mono PCM at OUTPUT_FREQUENCY Hz with little-endian byte order */
typedef struct
{
  char      RiffID[4];          /* "RIFF" */
  uint32_t  RiffSize;           /* File size minus 8 bytes */
  char      WaveID[4];          /* "WAVE" */
  char      FmtID[4];           /* "fmt " */
  uint32_t  FmtSize;            /* Format block size (always 16 for PCM) */
  uint16_t  wFormatTag;         /* PCM_WAVE_FORMAT (1) */
  uint16_t  nChannels;          /* MONO (1) or STEREO (2) */
  uint32_t  nSamplesPerSec;     /* Sample rate in Hz */
  uint32_t  nAvgBytesPerSec;    /* Average bytes per second */
  uint16_t  nBlockAlign;        /* Bytes per sample frame */
  uint16_t  wBitsPerSample;     /* 8 bits for our mono output */
  char      DataID[4];          /* "data" */
  uint32_t  nDataBytes;         /* Audio data size in bytes */
} WAVE_HEADER;

/* WAV data chunk header (for files with non-standard chunk ordering) */
typedef struct
{
  char     DataID[4];      /* "data" identifier */
  uint32_t nDataBytes;     /* Size of audio data in bytes */
} WAVE_BLOCK;

/* Function declarations */

/**
 * Write silence to WAV output.
 * Outputs DC offset (value 128) for the specified number of samples.
 * Allows tape mechanical settling between blocks.
 *
 * @param output  Output WAV file pointer
 * @param s       Number of silence samples to write (in 8-bit bytes)
 */
void writeSilence(FILE *output, uint32_t s);

/**
 * Write a single FSK modulated pulse (one complete sine wave cycle).
 * Generates a sine wave at the specified frequency.
 *
 * @param output  Output WAV file pointer
 * @param f       Frequency in Hz (LONG_PULSE=1200 Hz or SHORT_PULSE=2400 Hz)
 */
void writePulse(FILE *output, uint32_t f);

/**
 * Write a synchronization header signal.
 * Generates continuous short pulses (2400 Hz) to allow MSX BIOS to sync
 * to the incoming bit stream.
 *
 * @param output  Output WAV file pointer
 * @param s       Number of short pulses at 1200 baud rate
 */
void writeSync(FILE *output, uint32_t s);

/**
 * Encode and transmit a single byte using FSK serial framing.
 *
 * Framing format:
 *   - 1 START bit (0): one 1200 Hz pulse
 *   - 8 DATA bits (LSB first):
 *       * 0 bit: one 1200 Hz pulse
 *       * 1 bit: two 2400 Hz pulses
 *   - 2 STOP bits (1): four 2400 Hz pulses (2 × 2400 Hz each)
 *
 * @param output  Output WAV file pointer
 * @param byte    Byte value to encode (0-255)
 */
void writeByte(FILE *output, int byte);

/**
 * Transmit a data block from CAS file until encountering a header marker or EOF.
 * Reads 8-byte chunks and encodes each first byte via FSK serial.
 *
 * @param input    Input CAS file pointer
 * @param output   Output WAV file pointer
 * @param position Current file position (updated as bytes are read)
 * @param eof      Set to true if EOF marker (0x1A) is encountered
 */
void writeData(FILE *input, FILE *output, uint32_t *position, bool *eof);

#endif /* CASLIB_H */
