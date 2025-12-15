.PHONY: all install clean

ifneq ($(WINDIR),)
cas2wav_e   = cas2wav.exe
wav2cas_e   = wav2cas.exe
casdir_e    = casdir.exe
else
cas2wav_e   = cas2wav
wav2cas_e   = wav2cas
casdir_e    = casdir
endif

CC = gcc
CFLAGS = -O2 -Wall -fomit-frame-pointer -I.
CLIBS = -lm

all: $(cas2wav_e) $(wav2cas_e) $(casdir_e)

lib/caslib.o: lib/caslib.c lib/caslib.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/clilib.o: lib/clilib.c lib/clilib.h lib/caslib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(cas2wav_e): cas2wav.c lib/caslib.o lib/clilib.o lib/caslib.h lib/clilib.h
	$(CC) $(CFLAGS) cas2wav.c lib/caslib.o lib/clilib.o -o $@ $(CLIBS)

$(wav2cas_e): wav2cas.c lib/caslib.o lib/caslib.h
	$(CC) $(CFLAGS) wav2cas.c lib/caslib.o -o $@ $(CLIBS)

$(casdir_e): casdir.c lib/caslib.o lib/caslib.h
	$(CC) $(CFLAGS) casdir.c lib/caslib.o -o $@ $(CLIBS)

install: all
	cp $(cas2wav_e) $(wav2cas_e) $(casdir_e) /usr/local/bin

uninstall:
	rm -f /usr/local/bin/$(cas2wav_e) /usr/local/bin/$(wav2cas_e) /usr/local/bin/$(casdir_e)

clean:
	rm -f $(cas2wav_e)
	rm -f $(wav2cas_e)
	rm -f $(casdir_e)
	rm -f lib/caslib.o
	rm -f lib/clilib.o
