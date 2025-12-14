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

/* Display usage information and command-line options */
void showUsage(char *progname)
{
  printf("usage: %s [-2] [-s seconds] <ifile> <ofile>\n"
         " -2   use 2400 baud as output baudrate\n"
         " -s   define gap time (in seconds) between blocks (default 2)\n"
   ,progname);
}


int main(int argc, char* argv[])
{
  FILE *output,*input;
  WriteBuffer wb;   /* Buffered output context */
  uint32_t size,position;
  int  i,j;
  int  stime = -1;  /* Silence time between blocks (-1 = use default) */
  bool eof;         /* Set when EOF marker or data block boundary reached */
  char buffer[10];

  char *ifile = NULL;  /* Input CAS filename */
  char *ofile = NULL;  /* Output WAV filename */

  /* Parse command line options */
  for (i=1; i<argc; i++) {

    if (argv[i][0]=='-') {

      for(j=1;j && argv[i][j]!='\0';j++)

        switch(argv[i][j]) {

        case '2': BAUDRATE=2400; break;      /* Use 2400 baud instead of 1200 */
        case 's': stime=atof(argv[++i]); j=-1; break;  /* Custom silence duration */

        default:
          fprintf(stderr,"%s: invalid option\n",argv[0]);
          exit(1);
        }

      continue;
    }

    /* Collect input and output filenames from positional arguments */
    if (ifile==NULL) { ifile=argv[i]; continue; }
    if (ofile==NULL) { ofile=argv[i]; continue; }

    /* Too many arguments */
    fprintf(stderr,"%s: invalid option\n",argv[0]);
    exit(1);
  }

  /* Validate we have both input and output filenames */
  if (ifile==NULL || ofile==NULL) { showUsage(argv[0]); exit(1); }

  /* Open input and output files */
  if ((input=fopen(ifile,"rb"))==NULL) {
    fprintf(stderr,"%s: failed opening %s\n",argv[0],argv[1]);
    exit(1);
  }

  if ((output=fopen(ofile,"wb"))==NULL) {
    fprintf(stderr,"%s: failed writing %s\n",argv[0],argv[2]);
    exit(1);
  }

  /* Initialize write buffer for efficient I/O */
  initWriteBuffer(&wb, output);

  /* Write initial WAV header (size fields will be updated at end) */
  fwrite(&waveheader,sizeof(waveheader),1,output);

  position=0;
  /* Scan CAS file for HEADER markers (8-byte sync pattern) */
  while (fread(buffer,1,8,input)==8) {

    if (!memcmp(buffer,HEADER,8)) {
      /* Header found - read the 10-byte file type identifier */

      /* The MSX BIOS makes a distinction between SYNC_INITIAL (initial sync)
         and SYNC_BLOCK (inter-block sync). Using appropriate headers for each
         type improves compatibility with real hardware tape loaders. */

      position+=8;
      if (fread(buffer,1,10,input) == 10) {

        /* ASCII file type: multiple data blocks with headers between them */
        if (!memcmp(buffer,ASCII,10)) {

          fseek(input,position,SEEK_SET);
          writeSilence(&wb,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
          writeSync(&wb,SYNC_INITIAL);
          writeData(input,&wb,&position,&eof);

          /* Process subsequent data blocks until EOF or no more data */
          do {

            position+=8; fseek(input,position,SEEK_SET);
            writeSilence(&wb,SHORT_SILENCE);
            writeSync(&wb,SYNC_BLOCK);
            writeData(input,&wb,&position,&eof);

          } while (!eof && !feof(input));

        }
        /* Binary/BASIC file type: two-block structure (header block + data block) */
        else if (!memcmp(buffer,BIN,10) || !memcmp(buffer,BASIC,10)) {

          fseek(input,position,SEEK_SET);
          writeSilence(&wb,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
          writeSync(&wb,SYNC_INITIAL);
          writeData(input,&wb,&position,&eof);
          writeSilence(&wb,SHORT_SILENCE);
          writeSync(&wb,SYNC_BLOCK);
          position+=8; fseek(input,position,SEEK_SET);
          writeData(input,&wb,&position,&eof);

        }
        /* Unknown file type - use single block with initial sync */
        else {

          printf("unknown file type: using long header\n");
          fseek(input,position,SEEK_SET);
          writeSilence(&wb,LONG_SILENCE);
          writeSync(&wb,SYNC_INITIAL);
          writeData(input,&wb,&position,&eof);
        }

      }
      else {
        /* File type identifier read failed; treat as unknown type */
        printf("unknown file type: using initial sync\n");
        fseek(input,position,SEEK_SET);
        writeSilence(&wb,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
        writeSync(&wb,SYNC_INITIAL);
        writeData(input,&wb,&position,&eof);
      }

    } else {
      /* Non-header data found - skip byte and continue (handles corrupted files) */
      fprintf(stderr,"skipping unhandled data\n");
      position++;
    }

    fseek(input,position,SEEK_SET);
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
  fclose(input);

  return 0;
}
