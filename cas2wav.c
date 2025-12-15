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
#include <string.h>
#include "lib/caslib.h"
#include "lib/clilib.h"

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

int main(int argc, char* argv[])
{
  FILE *output;
  WriteBuffer wb;   /* Buffered output context */
  uint32_t size;    /* Final wav data size */
  size_t pos;       /* Current position in CAS data */
  bool eof;            /* Set when EOF marker or data block boundary reached */
  unsigned char *cas;  /* CAS file data in memory */
  size_t cas_size;     /* Size of CAS file */
  ProgramArgs args;

  /* Parse command line arguments */
  parseArguments(argc, argv, &args);

  /* Load CAS file and prepare output */
  loadAndPrepareFiles(argv[0], &args, &cas, &cas_size, &output, &wb);

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
