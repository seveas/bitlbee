###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include Makefile.settings

# Program variables
objects = irc.o bitlbee.o user.o nick.o set.o commands.o crypting.o help.o
subdirs = protocols

# Expansion of variables
subdirobjs = $(foreach dir,$(subdirs),$(dir)/$(dir).o)
CFLAGS += -Wall

all: $(OUTFILE)

uninstall: uninstall-bin uninstall-doc
install: install-bin install-doc
	@if ! [ -d $(DESTDIR)$(CONFIG) ]; then echo -en '\n\nThe configuration directory $(DESTDIR)$(CONFIG) does not exist yet, don'\''t forget to create it!\n\n\n'; fi

.PHONY:   install   install-bin   install-etc   install-doc \
        uninstall uninstall-bin uninstall-etc uninstall-doc \
        all clean distclean tar $(subdirs)

Makefile.settings:
	@echo
	@echo Run ./configure to create Makefile.settings, then rerun make
	@echo

clean: $(subdirs)
	rm -f *.o $(OUTFILE) core utils/bitlbeed

distclean: clean $(subdirs)
	rm -f Makefile.settings config.h

install-doc:
	$(MAKE) -C doc install

uninstall-doc:
	$(MAKE) -C doc uninstall

install-bin:
	install -p -d -m 0755 -o root -g 0 $(DESTDIR)$(BINDIR)
	install -p -d -m 0755 -o root -g 0 $(DESTDIR)$(DATADIR)
	install -p -m 0755 -o root -g 0 $(OUTFILE) $(DESTDIR)$(BINDIR)/$(OUTFILE)
	install -p -m 0644 -o root -g 0 help.txt $(DESTDIR)$(DATADIR)/help.txt

uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(OUTFILE)
	rm -f $(DESTDIR)$(DATADIR)/help.txt
	rmdir $(DESTDIR)$(DATADIR)

tar:
	fakeroot debian/rules clean
	x=`pwd | sed -e 's/\/.*\///'`; \
	cd ..; \
	tar czf $$x.tar.gz --exclude=CVS --exclude=debian $$x

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(objects): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(objects): Makefile Makefile.settings config.h

$(OUTFILE): $(subdirs) $(objects)
	$(CC) $(objects) $(subdirobjs) -o $(OUTFILE) $(LFLAGS)
ifndef DEBUG
	-$(STRIP) $(OUTFILE)
endif
