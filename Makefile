###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include Makefile.settings

# Program variables
objects = irc.o bitlbee.o user.o nick.o set.o commands.o crypting.o help.o account.o ini.o conf.o
subdirs = protocols

# Expansion of variables
subdirobjs = $(foreach dir,$(subdirs),$(dir)/$(dir).o)
CFLAGS += -Wall

all: $(OUTFILE)

uninstall: uninstall-bin uninstall-doc
	@echo -e '\n\nmake uninstall does not remove files in '$(DESTDIR)$(ETCDIR)', you can use make uninstall-etc to do that.\n\n'

install: install-bin install-doc
	@if ! [ -d $(DESTDIR)$(CONFIG) ]; then echo -e '\n\nThe configuration directory $(DESTDIR)$(CONFIG) does not exist yet, don'\''t forget to create it!'; fi
	@if ! [ -e $(DESTDIR)$(ETCDIR)/bitlbee.conf ]; then echo -e '\n\nNo files are installed in '$(DESTDIR)$(ETCDIR)' by make install. Run make install-etc to do that.'; fi
	@echo -e '\n'

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
	find . -name 'DEADJOE' -o -name '*.orig' -o -name '*.rej' -o -name '*~' | xargs rm -f

install-doc:
	$(MAKE) -C doc install

uninstall-doc:
	$(MAKE) -C doc uninstall

install-bin:
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	chmod 0755 $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(DATADIR)/help.txt # Prevent help function from breaking in running sessions
	install -m 0755 $(OUTFILE) $(DESTDIR)$(BINDIR)/$(OUTFILE)
	install -m 0644 help.txt $(DESTDIR)$(DATADIR)/help.txt

uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(OUTFILE)
	rm -f $(DESTDIR)$(DATADIR)/help.txt
	rmdir $(DESTDIR)$(DATADIR)

install-etc:
	mkdir -p $(DESTDIR)$(ETCDIR)
	install -m 0644 motd.txt $(DESTDIR)$(ETCDIR)/motd.txt
	install -m 0644 bitlbee.conf $(DESTDIR)$(ETCDIR)/bitlbee.conf

uninstall-etc:
	rm -f $(DESTDIR)$(ETCDIR)/motd.txt
	rm -f $(DESTDIR)$(ETCDIR)/bitlbee.conf
	-rmdir $(DESTDIR)$(ETCDIR)

tar:
	fakeroot debian/rules clean || make distclean
	x=`pwd | sed -e 's/\/.*\///'`; \
	cd ..; \
	tar czf $$x.tar.gz --exclude=debian $$x

$(subdirs):
	@$(MAKE) -C $@ $(MAKECMDGOALS)

$(objects): %.o: %.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

$(objects): Makefile Makefile.settings config.h

$(OUTFILE): $(subdirs) $(objects)
	@echo '*' Linking $(OUTFILE)
	@$(CC) $(objects) $(subdirobjs) -o $(OUTFILE) $(LFLAGS) $(EFLAGS)
ifndef DEBUG
	@echo '*' Stripping $(OUTFILE)
	@-$(STRIP) $(OUTFILE)
endif
