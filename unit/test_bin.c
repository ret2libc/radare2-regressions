#include <r_bin.h>
#include <r_io.h>
#include <r_list.h>
#include "minunit.h"

bool test_bin_basic(void) {
	RIO *io = r_io_new ();
	RBin *bin = r_bin_new ();
	RBinOptions opt;

	r_bin_options_init (&opt, -1, 0, 0, false);

	r_io_bind (io, &bin->iob);
	bool res = r_bin_open (bin, "../bins/elf/echo", &opt);
	mu_assert ("echo should have been loaded", res);

	RBinObject *o = r_bin_get_object (bin);
	mu_assert_notnull (o, "current object should be set");

	RList *e = r_bin_get_entries (bin);
	mu_assert_notnull (e, "the list of entries should be returned");

	RBinAddr *e0 = r_list_get_n (e, 0);
	mu_assert_notnull (e0, "the entry point should be there");

	mu_assert_eq (e0->vaddr, 0x840, "vaddr should be 0x840");

	RBinFile *bf = r_bin_file_find_by_name (bin, "../bins/elf/echo");
	mu_assert_notnull (bf, "the binfile should be returned");

	bool be = r_bin_is_big_endian (bin);
	mu_assert (!be, "echo is not big endian");



	r_bin_free (bin);
	r_io_free (io);
	mu_end;
}

int all_tests() {
	mu_run_test (test_bin_basic);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests ();
}
