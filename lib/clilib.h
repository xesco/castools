#ifndef CLILIB_H
#define CLILIB_H

#include <stdio.h>
#include <stddef.h>
#include "caslib.h"

/* Program arguments structure */
typedef struct {
  char *input_file;     /* Input CAS filename */
  char *output_file;    /* Output WAV filename */
  int baudrate;         /* Baud rate: 1200 or 2400 */
  int silence_time;     /* Silence duration in samples (default: LONG_SILENCE) */
} ProgramArgs;

/**
 * Display usage information and command-line options.
 *
 * @param progname Program name (typically argv[0])
 */
void showUsage(char *progname);

/**
 * Parse command line arguments into ProgramArgs structure.
 * Exits with error message if arguments are invalid.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param args Output structure to populate
 */
void parseArguments(int argc, char* argv[], ProgramArgs *args);

/**
 * Load CAS file into memory and prepare output file with write buffer.
 * Opens input file, reads it into memory, opens output file, and initializes write buffer.
 *
 * @param progname Program name for error messages
 * @param args Program arguments containing file names and baudrate
 * @param cas Output pointer to allocated CAS data buffer
 * @param cas_size Output size of CAS data
 * @param output Output file handle for WAV output
 * @param wb Write buffer to initialize
 */
void loadAndPrepareFiles(const char *progname, ProgramArgs *args, 
                         unsigned char **cas, size_t *cas_size,
                         FILE **output, WriteBuffer *wb);

#endif /* CLILIB_H */
