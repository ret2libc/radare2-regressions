#include <r_util.h>
#include "minunit.h"

static Sdb *setup_sdb(void) {
	Sdb *res = sdb_new0 ();
	sdb_set (res, "ExitProcess", "func", 0);
	sdb_set (res, "strchr", "func", 0);
	sdb_set (res, "__stack_chk_fail", "func", 0);
	return res;
}

bool test_dll_names(void) {
	Sdb *TDB = setup_sdb ();
	char *s;

	s = r_type_func_guess (TDB, "sub.KERNEL32.dll_ExitProcess");
	mu_assert_neq (s, NULL, "result should not be null");
	mu_assert_streq (s, "ExitProcess", "dll_ should be ignored");

	s = r_type_func_guess (TDB, "sub.dll_ExitProcess_32");
	mu_assert_neq (s, NULL, "result should not be null");
	mu_assert_streq (s, "ExitProcess", "number should be ignored");

	s = r_type_func_guess (TDB, "sub.KERNEL32.dll_ExitProcess_32");
	mu_assert_neq (s, NULL, "result should not be null");
	mu_assert_streq (s, "ExitProcess", "dll_ and number should be ignored");

	sdb_free (TDB);
	mu_end;
}

bool test_ignore_prefixes(void) {
	Sdb *TDB = setup_sdb ();
	char *s;

	s = r_type_func_guess (TDB, "fcn.KERNEL32.dll_ExitProcess_32");
	mu_assert_eq (s, NULL, "fcn. names should be ignored");

	s = r_type_func_guess (TDB, "loc.KERNEL32.dll_ExitProcess_32");
	mu_assert_eq (s, NULL, "loc. names should be ignored");

	sdb_free (TDB);
	mu_end;
}

bool test_remove_r2_prefixes(void) {
	Sdb *TDB = setup_sdb ();
	char *s;

	s = r_type_func_guess (TDB, "sym.imp.ExitProcess");
	mu_assert_neq (s, NULL, "sym.imp should be ignored");
	mu_assert_streq (s, "ExitProcess", "sym.imp should be ignored");

	s = r_type_func_guess (TDB, "sym.imp.fcn.ExitProcess");
	mu_assert_neq (s, NULL, "sym.imp.fcn should be ignored");
	mu_assert_streq (s, "ExitProcess", "sym.imp.fcn should be ignored");

	s = r_type_func_guess (TDB, "longprefix.ExitProcess");
	mu_assert_eq (s, NULL, "prefixes longer than 3 should not be ignored");

	sdb_free (TDB);
	mu_end;
}

bool test_autonames(void) {
	Sdb *TDB = setup_sdb ();
	char *s;

	s = r_type_func_guess (TDB, "sub.strchr_123");
	mu_assert_eq (s, NULL, "function that calls common fcns shouldn't be identified as such");

	s = r_type_func_guess (TDB, "sub.__strchr_123");
	mu_assert_eq (s, NULL, "initial _ should not confuse the api");

	s = r_type_func_guess (TDB, "sub.__stack_chk_fail_740");
	mu_assert_eq (s, NULL, "initial _ should not confuse the api");

	s = r_type_func_guess (TDB, "sym.imp.strchr");
	mu_assert_neq (s, NULL, "sym.imp. should be ignored");
	mu_assert_streq (s, "strchr", "strchr should be identified");

	sdb_free (TDB);
	mu_end;
}

bool test_initial_underscore(void) {
	Sdb *TDB = setup_sdb ();
	char *s;

	s = r_type_func_guess (TDB, "sym._strchr");
	mu_assert_neq (s, NULL, "sym._ should be ignored");
	mu_assert_streq (s, "strchr", "strchr should be identified");

	sdb_free (TDB);
	mu_end;
}

int all_tests() {
	mu_run_test (test_ignore_prefixes);
	mu_run_test (test_remove_r2_prefixes);
	mu_run_test (test_dll_names);
	mu_run_test (test_autonames);
	mu_run_test (test_initial_underscore);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
