###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include ../../Makefile.settings

# [SH] Program variables
objects = msn.o

CFLAGS += -Wall

# [SH] Phony targets
all: msn.o

.PHONY: all clean distclean

clean:
	rm -f *.o core

distclean: clean

### MAIN PROGRAM

$(objects): ../../Makefile.settings Makefile

$(objects): %.o: %.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@
