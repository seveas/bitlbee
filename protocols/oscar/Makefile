###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include ../../Makefile.settings

# [SH] Program variables
objects = admin.o adverts.o auth.o bos.o buddylist.o chat.o chatnav.o conn.o ft.o icq.o im.o info.o invite.o meta.o misc.o msgcookie.o popups.o rxhandlers.o rxqueue.o search.o service.o snac.o ssi.o stats.o tlv.o translate.o txqueue.o util.o oscar.o

CFLAGS += -Wall
LFLAGS += -r

# [SH] Phony targets
all: oscarr.o

.PHONY: all clean distclean

clean:
	rm -f *.o core

distclean: clean

### MAIN PROGRAM

$(objects): ../../Makefile.settings Makefile

$(objects): %.o: %.c
	@echo Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

oscarr.o: $(objects)
	$(LD) $(LFLAGS) $(objects) -o oscarr.o
