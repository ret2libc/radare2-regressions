TDIR=$(shell uname)-$(shell uname -m)

all:
	@[ -d "$(TDIR)" ] && ( cd .. && sh run_tests.sh t.archos/$(TDIR) ) || true
	@echo "Debug tests doesn't work in TRAVIS yet"
