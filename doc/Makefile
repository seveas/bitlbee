-include ../Makefile.settings

all: user-guide.txt user-guide.html help.txt # user-guide.pdf user-guide.ps user-guide.rtf

user-guide.txt: user-guide.xml
	xmlto txt $<

user-guide.html: user-guide.xml
	xsltproc --output $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $<

user-guide.pdf: user-guide.xml
	xmlto pdf $<

user-guide.ps: user-guide.xml
	xmlto ps $<

help.txt: help.xml help.xsl
	xsltproc help.xsl $< | sed -e 's/_b_//g' > $@

clean: 
	rm -f *.html *.pdf *.ps *.rtf *.txt

install:
	mkdir -p $(DESTDIR)$(MANDIR)/man8/ $(DESTDIR)$(MANDIR)/man5/
	cp bitlbee.8 $(DESTDIR)$(MANDIR)/man8/
	cp bitlbee.conf.5 $(DESTDIR)$(MANDIR)/man5/

uninstall:
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.8*
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.conf.5*
