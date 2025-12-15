/**************************************************************************/
/*                                                                        */
/* file:         cas2wav.c                                                */
/* version:      1.31 (April 11, 2016)                                    */
/*                                                                        */
/* description:  This tool exports the contents of a .cas file to a .wav  */
/*               file. The .cas format is the standard format for MSX to  */
/*               emulate tape recorders. The wav can be copied onto a     */
/*               tape to be read by a real MSX.                           */
/*                                                                        */
/*                                                                        */
/*  This program is free software; you can redistribute it and/or modify  */
/*  it under the terms of the GNU General Public License as published by  */
/*  the Free Software Foundation; either version 2, or (at your option)   */
/*  any later version. See COPYING for more details.                      */
/*                                                                        */
/*                                                                        */
/* Copyright 2001-2016 Vincent van Dam (vincentd@erg.verweg.com)          */
/* MultiCPU Copyright 2007 Ramones     (ramones@kurarizeku.net)           */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <math.h>
#include "caslib.h"


/* Preset WAV header template (sizes updated at program end with actual data size) */
WAVE_HEADER waveheader =
{
  { "RIFF" },
  0,                    /* Will be set to actual file size - 8 */
  { "WAVE" },
  { "fmt " },
  16,                   /* Format chunk size (16 bytes for PCM) */
  PCM_WAVE_FORMAT,      /* PCM format identifier */
  MONO,                 /* Single channel audio */
  OUTPUT_FREQUENCY,     /* 43200 Hz sample rate */
  OUTPUT_FREQUENCY,     /* Bytes per second = sample rate for 8-bit mono */
  1,                    /* Block alignment (1 byte per sample for 8-bit mono) */
  8,                    /* 8 bits per sample */
  { "data" },
  0                     /* Will be set to actual audio data size */
};

/* Program arguments structure */
typedef struct {
  char *input_file;
  char *output_file;
  int baudrate;
  int silence_time;  /* Silence duration in samples (default: LONG_SILENCE) */
} ProgramArgs;

/* Display usage information and command-line options */
void showUsage(char *progname)
{
  printf("usage: %s [-2] [-s seconds] <ifile> <ofile>\n"
         " -2   use 2400 baud as output baudrate\n"
         " -s   define gap time (in seconds) between blocks (default 2)\n"
   ,progname);
}

/* Parse command line arguments into ProgramArgs structure */
void parseArguments(int argc, char* argv[], ProgramArgs *args)
{
  /* Initialize with defaults */
  args->input_file = NULL;
  args->output_file = NULL;
  args->baudrate = BAUDRATE;
  args->silence_time = LONG_SILENCE;

  /* Parse command line options */
  for (int i=1; i<argc; i++) {
    if (argv[i][0]=='-' && argv[i][1]!='\0') {

      /* Process option flags */
      if (argv[i][1]=='2' && argv[i][2]=='\0') {
        args->baudrate = 2400;
      }
      else if (argv[i][1]=='s' && argv[i][2]=='\0') {
        /* Custom silence duration - requires next argument */
        if (i+1 >= argc) {
          fprintf(stderr,"%s: option -s requires an argument\n",argv[0]);
          exit(1);
        }
        args->silence_time = OUTPUT_FREQUENCY * atof(argv[++i]);
      }
      else {
        fprintf(stderr,"%s: invalid option '%s'\n",argv[0],argv[i]);
        exit(1);
      }
      continue;
    }

    /* Collect input and output filenames from positional arguments */
    if (args->input_file==NULL) { args->input_file=argv[i]; continue; }
    if (args->output_file==NULL) { args->output_file=argv[i]; continue; }

    /* Too many arguments */
    fprintf(stderr,"%s: too many arguments\n",argv[0]);
    exit(1);
  }

  /* Validate we have both input and output filenames */
  if (args->input_file==NULL || args->output_file==NULL) {
    showUsage(argv[0]);
    exit(1);
  }
}


int main(int argc, char* argv[])
{
  FILE *output,*input;
  WriteBuffer wb;   /* Buffered output context */
  uint32_t size;    /* Final wav data size */
  size_t pos;       /* Current position in CAS data */
  bool eof;            /* Set when EOF marker or data block boundary reached */
  unsigned char *cas;  /* CAS file data in memory */
  size_t cas_size;     /* Size of CAS file */
  ProgramArgs args;

  /* Parse command line arguments */
  parseArguments(argc, argv, &args);

  /* Set global baudrate from parsed arguments */
  BAUDRATE = args.baudrate;

  /* Open input file */
  if ((input=fopen(args.input_file,"rb"))==NULL) {
    fprintf(stderr,"%s: failed opening %s\n",argv[0],args.input_file);
    exit(1);
  }

  /* Get file size */
  cas_size = getFileSize(input);
  if (cas_size < 0) {
    fprintf(stderr,"%s: failed to get file size of %s\n",argv[0],args.input_file);
    exit(1);
  }

  /* Load entire CAS file into memory */
  cas = (unsigned char*)malloc(cas_size);
  if (cas == NULL || fread(cas, 1, cas_size, input) != cas_size) {
    fprintf(stderr,"%s: failed reading %s\n",argv[0],args.input_file);
    exit(1);
  }
  fclose(input);

  if ((output=fopen(args.output_file,"wb"))==NULL) {
    fprintf(stderr,"%s: failed writing %s\n",argv[0],args.output_file);
    exit(1);
  }

  /* Initialize write buffer for efficient I/O */
  initWriteBuffer(&wb, output, BAUDRATE, OUTPUT_FREQUENCY);

  /* Write initial WAV header (size fields will be updated at end) */
  fwrite(&waveheader,sizeof(waveheader),1,output);

  pos=0;
  /* Scan CAS file for HEADER markers (8-byte sync pattern) */
  while (pos + sizeof(HEADER) <= cas_size) {
    if (!memcmp(&cas[pos], HEADER, sizeof(HEADER))) {
      /* Header found - read the 10-byte file type identifier */

      /* The MSX BIOS makes a distinction between SYNC_INITIAL (initial sync)
         and SYNC_BLOCK (inter-block sync). Using appropriate headers for each
         type improves compatibility with real hardware tape loaders. */

      pos += sizeof(HEADER);
      if (pos + 10 <= cas_size) {

        /* ASCII file type: multiple data blocks with headers between them */
        if (!memcmp(&cas[pos], ASCII, 10)) {

          writeSilence(&wb, args.silence_time);
          writeSync(&wb,SYNC_INITIAL);
          pos = writeData(cas, cas_size, &wb, pos, &eof);

          /* Process subsequent data blocks until EOF or no more data */
          do {

            writeSilence(&wb,SHORT_SILENCE);
            writeSync(&wb,SYNC_BLOCK);
            pos = writeData(cas, cas_size, &wb, pos + sizeof(HEADER), &eof);

          } while (!eof && pos < cas_size);

        }
        /* Binary/BASIC file type: two-block structure (header block + data block) */
        else if (!memcmp(&cas[pos], BIN, 10) || !memcmp(&cas[pos], BASIC, 10)) {

          writeSilence(&wb, args.silence_time);
          writeSync(&wb,SYNC_INITIAL);
          pos = writeData(cas, cas_size, &wb, pos, &eof);
          writeSilence(&wb,SHORT_SILENCE);
          writeSync(&wb,SYNC_BLOCK);
          pos = writeData(cas, cas_size, &wb, pos + sizeof(HEADER), &eof);

        }
        /* Unknown file type - use single block with initial sync */
        else {

          printf("unknown file type: using long header\n");
          writeSilence(&wb,LONG_SILENCE);
          writeSync(&wb,SYNC_INITIAL);
          pos = writeData(cas, cas_size, &wb, pos, &eof);
        }

      }
      else {
        /* File type identifier read failed; treat as unknown type */
        printf("unknown file type: using initial sync\n");
        writeSilence(&wb, args.silence_time);
        writeSync(&wb,SYNC_INITIAL);
        pos = writeData(cas, cas_size, &wb, pos, &eof);
      }

    } else {
      /* Non-header data found - skip byte and continue (handles corrupted files) */
      fprintf(stderr,"skipping unhandled data\n");
      pos++;
    }
  }

  /* Flush remaining buffered data */
  flushWriteBuffer(&wb);

  /* Update WAV header with final audio data size */
  size = ftell(output)-sizeof(waveheader);
  waveheader.nDataBytes = size;
  waveheader.RiffSize = size;

  /* Rewrite header with correct sizes */
  fseek(output,0,SEEK_SET);
  fwrite(&waveheader,sizeof(waveheader),1,output);

  fclose(output);
  free(cas);

  return 0;
}
