-include ../Makefile.settings

all: user-guide.txt user-guide.html # user-guide.pdf user-guide.ps user-guide.rtf

user-guide.txt: user-guide.sgml
	sgmltools --backend txt user-guide.sgml

user-guide.html: user-guide.sgml
	sgmltools --backend onehtml user-guide.sgml

user-guide.pdf: user-guide.sgml
	sgmltools --backend pdf user-guide.sgml

user-guide.rtf: user-guide.sgml
	sgmltools --backend rtf user-guide.sgml

user-guide.ps: user-guide.sgml
	sgmltools --backend ps user-guide.sgml

clean: 
	rm -f *.html *.pdf *.ps *.rtf *.txt

install:
	mkdir -p $(DESTDIR)$(MANDIR)/man8/ $(DESTDIR)$(MANDIR)/man5/
	cp bitlbee.8 $(DESTDIR)$(MANDIR)/man8/
	cp bitlbee.conf.5 $(DESTDIR)$(MANDIR)/man5/

uninstall:
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.8*
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.conf.5*
