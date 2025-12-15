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

/* CAS file type enumeration */
typedef enum {
  FILE_TYPE_ASCII,      /* ASCII text file */
  FILE_TYPE_BINARY,     /* Binary or BASIC program */
  FILE_TYPE_UNKNOWN     /* Unknown/unrecognized type */
} FileType;

/* Audio output configuration */

/* Audio output sample rate: 43200 Hz, 8-bit mono PCM */
#define OUTPUT_FREQUENCY  43200

/* Silence durations (in samples = bytes) */
#define SHORT_SILENCE     OUTPUT_FREQUENCY      /* 1 second  */
#define LONG_SILENCE      OUTPUT_FREQUENCY * 2  /* 2 seconds */

/* FSK (Frequency Shift Keying) tones for bit encoding
 * Each 0-bit = 1 pulse  at 1200 Hz
 * Each 1-bit = 2 pulses at 2400 Hz */
#define LONG_PULSE        1200   /* 0 bit: 1200 Hz */
#define SHORT_PULSE       2400   /* 1 bit: 2400 Hz */

/* Synchronization header: number of 1-bits at 1200 baud
 * MSX tape format specifies 16000 and 4000 SHORT_PULSE (2400 Hz) cycles.
 * Since each 1-bit = 2 pulses, we use 16000/2 and 4000/2 bit counts. */
#define SYNC_INITIAL      8000    /* 16000/2: Initial sync before first block (~6.67 sec) */
#define SYNC_BLOCK        2000    /*  4000/2: Block sync between blocks (~1.67 sec) */

/* Buffered I/O configuration */
#define WRITE_BUFFER_SIZE 16384  /* 16KB buffer for optimal performance */

/* Write buffer context for batched output */
typedef struct {
  FILE *file;
  unsigned char buffer[WRITE_BUFFER_SIZE];
  size_t pos;
  int baudrate;
  int output_frequency;
} WriteBuffer;

/**
 * Initialize a write buffer for buffered output.
 * Sets up the buffer context with file handle and encoding parameters.
 *
 * @param wb               Write buffer to initialize
 * @param file             Output file handle
 * @param baudrate         Baud rate (1200 or 2400)
 * @param output_frequency Sample rate in Hz (typically OUTPUT_FREQUENCY)
 */
void initWriteBuffer(WriteBuffer *wb, FILE *file, int baudrate, int output_freq);

/**
 * Flush any remaining data in the buffer to the output file.
 * Should be called before closing the file or when forcing data to disk.
 *
 * @param wb Write buffer context
 */
void flushWriteBuffer(WriteBuffer *wb);

/**
 * Write a single byte to the buffer, flushing automatically when full.
 * This is the core buffered write primitive used by all output functions.
 *
 * @param wb   Write buffer context
 * @param byte Byte value to write (0-255)
 */
void putByte(WriteBuffer *wb, unsigned char byte);

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
 * @param wb           Write buffer context
 * @param sample_count Number of silence samples to write (in 8-bit bytes)
 */
void writeSilence(WriteBuffer *wb, uint32_t sample_count);

/**
 * Write a single FSK modulated pulse (one complete sine wave cycle).
 * Generates a sine wave at the specified frequency.
 *
 * @param wb      Write buffer context
 * @param freq    Frequency in Hz (LONG_PULSE=1200 Hz or SHORT_PULSE=2400 Hz)
 */
void writePulse(WriteBuffer *wb, uint32_t freq);

/**
 * Write a synchronization header signal.
 * Generates continuous 1-bits (2400 Hz pulses) to allow MSX BIOS to sync
 * to the incoming bit stream.
 *
 * @param wb   Write buffer context
 * @param bits Number of 1-bits at 1200 baud (scaled proportionally for other rates)
 */
void writeSync(WriteBuffer *wb, uint32_t bits);

/**
 * Encode and transmit a single byte using FSK serial framing.
 *
 * Framing format:
 *   - 1 START bit (0)
 *   - 8 DATA bits (LSB first)
 *   - 2 STOP bits (1, 1)
 *
 * Bit encoding (FSK):
 *   - 0 bit: one 1200 Hz pulse
 *   - 1 bit: two 2400 Hz pulses
 *
 * @param wb   Write buffer context
 * @param byte Byte value to encode (0-255)
 */
void writeByte(WriteBuffer *wb, int byte);

/**
 * Transmit a data block from in-memory CAS data until encountering a header marker or EOF.
 *
 * @param cas      Pointer to CAS file data in memory
 * @param cas_size Total size of CAS file in bytes
 * @param wb       Write buffer context for output
 * @param pos Current position in CAS data (updated as bytes are transmitted)
 * @param eof Set to true if EOF marker (0x1A) is encountered
 */
size_t writeData(const unsigned char *cas, size_t cas_size, WriteBuffer *wb, size_t pos, bool *eof);

/**
 * Get the size of an open file.
 * Uses fseek/ftell and restores the original file position.
 *
 * @param file Open file handle
 * @return File size in bytes, or -1 on error
 */
long getFileSize(FILE *file);

/**
 * Identify the CAS file type from the 10-byte type identifier.
 *
 * @param data Pointer to the 10-byte file type identifier
 * @return FileType enum value indicating the file type
 */
FileType identifyFileType(const unsigned char *data);

/**
 * Update WAV file header with final audio data size.
 * Calculates the data size, updates the header structure, and rewrites it to the file.
 *
 * @param file Output file handle (must be positioned after writing all audio data)
 * @param header Pointer to WAV header structure to update and write
 */
void updateWavHeader(FILE *file, WAVE_HEADER *header);

#endif /* CASLIB_H */
