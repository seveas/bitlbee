###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include ../../Makefile.settings

# [SH] Program variables
objects = expat.o genhash.o hashtable.o jconn.o jid.o jpacket.o jutil.o karma.o log.o pool.o pproxy.o rate.o socket.o str.o xhash.o xmlnode.o xmlparse.o xmlrole.o xmltok.o xstream.o jabber.o

CFLAGS += -Wall
LFLAGS += -r

# [SH] Phony targets
all: jabberr.o

.PHONY: all clean distclean

clean:
	rm -f *.o core

distclean: clean

### MAIN PROGRAM

$(objects): ../../Makefile.settings Makefile

$(objects): %.o: %.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

jabberr.o: $(objects)
	@echo '*' Linking jabberr.o
	@$(LD) $(LFLAGS) $(objects) -o jabberr.o
