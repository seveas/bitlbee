-include ../Makefile.settings
EXTRAPARANEWLINE = 1
# EXTRAPARANEWLINE = 0

all: user-guide.txt user-guide.html help.txt # user-guide.pdf user-guide.ps user-guide.rtf

%.tex: %.db.xml
	xsltproc --stringparam l10n.gentext.default.language "en" --stringparam latex.documentclass.common "" --stringparam latex.babel.language "" --output $@ http://db2latex.sourceforge.net/xsl/docbook.xsl $<

%.txt: %.db.xml
	xmlto --skip-validation txt $<

%.html: %.db.xml
	xsltproc --output $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $<

%.pdf: %.db.xml
	xmlto --skip-validation pdf $<

%.ps: %.db.xml
	xmlto --skip-validation ps $<

help.xml: commands.xml

%.db.xml: %.xml docbook.xsl
	xsltproc --xinclude --output $@ docbook.xsl $< 

help.txt: help.xml help.xsl
	xsltproc --stringparam extraparanewline "$(EXTRAPARANEWLINE)" --xinclude help.xsl $< | sed -e 's/_b_//g' > $@

clean: 
	rm -f *.html *.pdf *.ps *.rtf *.txt *.db.xml

install:
	mkdir -p $(DESTDIR)$(MANDIR)/man8/ $(DESTDIR)$(MANDIR)/man5/
	cp bitlbee.8 $(DESTDIR)$(MANDIR)/man8/
	cp bitlbee.conf.5 $(DESTDIR)$(MANDIR)/man5/

uninstall:
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.8*
	rm -f $(DESTDIR)$(MANDIR)/man8/bitlbee.conf.5*
