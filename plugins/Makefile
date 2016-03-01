-include ../Makefile.settings

ifdef _SRCDIR_
_SRCDIR_ := $(_SRCDIR_)lib/
endif

.PHONY: clean distclean install

clean:
	rm -f *.so

distclean: clean
	rm -f .depend

$(plugins): ../Makefile.settings Makefile

$(PLUGINS): %.so: $(_SRCDIR_)%.c
	@echo '*' Building plugin $@
	@$(CC) $(CFLAGS) -fPIC -shared $(LDFLAGS) $< -o $@ $(FLAGS_$@)

install:
	mkdir -p $(DESTDIR)$(PLUGINDIR)
	$(INSTALL) -m 0755 -t $(DESTDIR)$(PLUGINDIR) $(PLUGINS)

-include .depend/*.d
