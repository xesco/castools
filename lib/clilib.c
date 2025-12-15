/**************************************************************************/
/*                                                                        */
/* file:         clilib.c                                                 */
/* description:  Command-line interface utilities for castools            */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "clilib.h"

/* Baudrate constants */
#define BAUDRATE_STD   1200
#define BAUDRATE_FAST  2400

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
  args->baudrate = BAUDRATE_STD;
  args->silence_time = LONG_SILENCE;

  /* Parse command line options */
  for (int i=1; i<argc; i++) {
    if (argv[i][0]=='-' && argv[i][1]!='\0') {

      /* Process option flags */
      if (argv[i][1]=='2' && argv[i][2]=='\0') {
        args->baudrate = BAUDRATE_FAST;
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

/* Load CAS file into memory and prepare output file with write buffer */
void loadAndPrepareFiles(const char *progname, ProgramArgs *args, 
                         unsigned char **cas, size_t *cas_size,
                         FILE **output, WriteBuffer *wb)
{
  FILE *input;

  /* Open input file */
  if ((input=fopen(args->input_file,"rb"))==NULL) {
    fprintf(stderr,"%s: failed opening %s\n",progname,args->input_file);
    exit(1);
  }

  /* Get file size */
  *cas_size = getFileSize(input);
  if (*cas_size < 0) {
    fprintf(stderr,"%s: failed to get file size of %s\n",progname,args->input_file);
    exit(1);
  }

  /* Load entire CAS file into memory */
  *cas = (unsigned char*)malloc(*cas_size);
  if (*cas == NULL || fread(*cas, 1, *cas_size, input) != *cas_size) {
    fprintf(stderr,"%s: failed reading %s\n",progname,args->input_file);
    exit(1);
  }
  fclose(input);

  if ((*output=fopen(args->output_file,"wb"))==NULL) {
    fprintf(stderr,"%s: failed writing %s\n",progname,args->output_file);
    exit(1);
  }

  /* Initialize write buffer for efficient I/O */
  initWriteBuffer(wb, *output, args->baudrate, OUTPUT_FREQUENCY);
}
