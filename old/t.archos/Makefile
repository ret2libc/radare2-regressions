TDIR=$(shell uname)-$(shell uname -m)

all:
	@[ -d "$(TDIR)" ] && ( cd ../.. && sh run_tests.sh old/t.archos/$(TDIR) ) || true
