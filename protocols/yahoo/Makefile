###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include ../../Makefile.settings

# [SH] Program variables
objects = yahoo.o crypt.o

CFLAGS += -Wall
LFLAGS += -r

# [SH] Phony targets
all: yahooo.o

.PHONY: all clean distclean

clean:
	rm -f *.o core

distclean: clean

### MAIN PROGRAM

$(objects): ../../Makefile.settings Makefile

$(objects): %.o: %.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

yahooo.o: $(objects)
	@echo '*' Linking yahooo.o
	@$(LD) $(LFLAGS) $(objects) -o yahooo.o
