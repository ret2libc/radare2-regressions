#include <r_flag.h>
#include "minunit.h"

bool test_r_flag_get_set(void) {
	RFlag *flags;
	RFlagItem *fi;

	flags = r_flag_new ();
	mu_assert_notnull (flags, "r_flag_new () failed");

	r_flag_set (flags, "foo", 1024, 50);
	fi = r_flag_get_i (flags, 1024);
	mu_assert_notnull (fi, "cannot find 'foo' flag at 1024");

	r_flag_set (flags, "foo", 300LL, 0);
	fi = r_flag_get_i (flags, 0);
	mu_assert_null (fi, "found a flag at 0 while there is none");
	fi = r_flag_get_i (flags, 300LL);
	mu_assert_notnull (fi, "cannot find 'foo' flag at 300LL");

	fi = r_flag_get (flags, "foo");
	mu_assert_notnull (fi, "cannot find 'foo' flag");

	r_flag_free (flags);
	mu_end;
}

int all_tests() {
	mu_run_test (test_r_flag_get_set);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
