-include ../Makefile.settings

all: user-guide.txt user-guide.html # user-guide.pdf user-guide.ps user-guide.rtf

user-guide.txt: user-guide.xml
	xmlto txt user-guide.xml

user-guide.html: user-guide.xml
	xmlto html-nochunks user-guide.xml

user-guide.pdf: user-guide.xml
	xmlto pdf user-guide.xml

user-guide.rtf: user-guide.xml
	xmlto rtf user-guide.xml

user-guide.ps: user-guide.xml
	xmlto ps user-guide.xml

clean: 
	rm -f *.html *.pdf *.ps *.rtf *.txt

install:
	mkdir -p $(DESTDIR)$(MANDIR)/man8/ $(DESTDIR)$(MANDIR)/man5/
	cp bitlbee.8 $(DESTDIR)$(MANDIR)/man8/
	cp bitlbee.conf.5 $(DESTDIR)$(MANDIR)/man5/

uninstall:
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.8*
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.conf.5*
