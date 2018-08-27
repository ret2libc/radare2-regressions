VERSION=2.5.0
DESTDIR?=/
PREFIX?=/usr/local
BINDIR=$(DESTDIR)/$(PREFIX)/bin
PWD=$(shell pwd)
PKG=radare2-regressions
TAR=tar -cvf
TAREXT=tar.xz
CZ=xz -f

TDIRS=$(shell ls -d t*| grep -v tmp) bins
LIBDIR=$(DESTDIR)/$(PREFIX)/lib

-include config.mk


PULLADDR=https://github.com/radare/radare2-regressions.git

do:
	-git pull ${PULLADDR}
	-$(MAKE) overlay-apply
	$(SHELL) run_tests.sh

overlay:
	if [ -f ../old/t/overlay ]; then \
		$(SHELL) overlay.sh create ; \
	fi

apply-overlay overlay-apply:
	$(SHELL) overlay.sh apply

all:
	-$(MAKE) overlay-apply
	$(MAKE) alltargets

alltargets: formats io unit_tests

radare2:
	@if [ -f ../binr/radare2/radare2 ]; then $(SHELL) run_tests.sh ./old/t ; fi

archos:
	@$(MAKE) -C old/t.archos
dbg.linux:
	$(SHELL) run_tests.sh old/t.archos/Linux

commands:
	$(SHELL) run_tests.sh

io:
	$(SHELL) run_tests.sh old/t.io

formats: format.elf format.mach0 format.others format.pdb format.pe
format.elf:
	$(SHELL) run_tests.sh old/t.formats/elf
format.mach0:
	$(SHELL) run_tests.sh old/t.formats/mach0
format.others:
	$(SHELL) run_tests.sh old/t.formats/others
format.pdb:
	$(SHELL) run_tests.sh old/t.formats/pdb
format.pe:
	$(SHELL) run_tests.sh old/t.formats/pe


keystone:
	$(SHELL) run_tests.sh old/t.extras/keystone

swf:
	$(SHELL) run_tests.sh old/t.extras/swf

m68k-extras:
	$(SHELL) run_tests.sh old/t.extras/m68k

olly-extras:
	$(SHELL) run_tests.sh old/t.extras/x86_olly

dwarf:
	$(SHELL) run_tests.sh old/t.extras/dwarf

broken:
	grep BROKEN=1 t -r -l

clean:
	rm -rf tmp

symstall:
	mkdir -p $(BINDIR)
	chmod +x r2-v r2r
	ln -fs $(PWD)/r2-v $(BINDIR)/r2-v
	ln -fs $(PWD)/r2r $(BINDIR)/r2r

#sed -e 's,@R2RDIR@,$(PWD),g' < $(PWD)/r2-v > $(BINDIR)/r2-v
#sed -e 's,@R2RDIR@,$(PWD),g' < $(PWD)/r2r > $(BINDIR)/r2r
install:
	mkdir -p $(BINDIR)
	sed -e 's,@R2RDIR@,$(LIBDIR)/radare2-regressions,g' < $(PWD)/r2-v > $(BINDIR)/r2-v
	sed -e 's,@R2RDIR@,$(LIBDIR)/radare2-regressions,g' < $(PWD)/r2r > $(BINDIR)/r2r
	chmod +x $(BINDIR)/r2-v
	chmod +x $(BINDIR)/r2r
	mkdir -p $(LIBDIR)/radare2-regressions
	cp -rf $(TDIRS) $(LIBDIR)/radare2-regressions
	cp -rf *.sh $(LIBDIR)/radare2-regressions

uninstall:
	rm -f $(BINDIR)/r2r
	rm -f $(BINDIR)/r2-v
	rm -rf $(LIBDIR)/radare2-regressions

unit_tests:
	@make -C ./unit all
	@./run_unit.sh

tested:
	@grep -re FILE= t*  | cut -d : -f 2- | sed -e 's/^.*bins\///g' |sort -u | grep -v FILE

untested:
	@${MAKE} -s tested > .a
	@${MAKE} -s allbins > .b
	@diff -ru .a .b | grep ^+ | grep -v +++ | cut -c 2-
	@rm -f .a .b

js-tests:
	cd new && npm install
	cd new && node bin/r2r.js

allbins:
	find bins -type f

dist:
	git clone . $(PKG)-$(VERSION)
	rm -rf $(PKG)-$(VERSION)/.git
	$(TAR) "$(PKG)-${VERSION}.tar" "$(PKG)-$(VERSION)"
	${CZ} "$(PKG)-${VERSION}.tar"

.PHONY: all clean allbins dist
