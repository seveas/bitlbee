###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include Makefile.settings

# [SH] Program variables
objects = irc.o bitlbee.o user.o nick.o set.o commands.o crypting.o
subdirs = protocols

# Expansion of variables
subdirobjs = $(foreach dir,$(subdirs),$(dir)/$(dir).o)
CFLAGS += -Wall

# [SH] Phony targets
all: $(OUTFILE)

install: install-bin install-etc install-doc
uninstall: uninstall-bin uninstall-etc uninstall-doc

	# [SH] .PHONY is more efficient
.PHONY:   install   install-bin   install-etc   install-doc \
        uninstall uninstall-bin uninstall-etc uninstall-doc \
        all clean distclean tar $(subdirs)

Makefile.settings:
	@echo
	@echo Run ./configure to create Makefile.settings, then rerun make
	@echo

clean: $(subdirs)
	rm -f *.o $(OUTFILE) core

distclean: clean $(subdirs)
	rm -f Makefile.settings config.h

install-doc:
	$(MAKE) -C doc install

uninstall-doc:
	$(MAKE) -C doc uninstall

install-etc:

uninstall-etc:

install-bin:
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(OUTFILE) $(DESTDIR)$(BINDIR)/$(OUTFILE)

uninstall-bin:
	rm -f $(BINDIR)/$(OUTFILE)

tar:
	fakeroot debian/rules clean
	x=`pwd | sed -e 's/\/.*\///'`; \
	cd ..; \
	tar czf $$x.tar.gz --exclude=CVS --exclude=debian $$x

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)

### MAIN PROGRAM

$(objects): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(objects): Makefile Makefile.settings config.h

$(OUTFILE): $(subdirs) $(objects)
	$(CC) $(objects) $(subdirobjs) -o $(OUTFILE) $(LFLAGS)
ifndef DEBUG
	-$(STRIP) $(OUTFILE)
endif
