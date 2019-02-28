#include <r_util.h>
#include <stdlib.h>
#include "minunit.h"

bool test_r_buf_file_new() {
	RBuffer *b;
	char buffer[1024] = { 0 };
	int r;
	char filename[] = "r2-XXXXXX";
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	// Prepare file
	int fd = mkstemp (filename);
	mu_assert_neq (fd, -1, "mkstemp failed...");
	write (fd, content, length);
	close (fd);

	b = r_buf_new_file (filename, 0);
	mu_assert_notnull (b, "r_buf_new_file failed");

	r = r_buf_read_at (b, R_BUF_CUR, (ut8 *)buffer, length);
	mu_assert_eq (r, length, "r_buf_read_at failed");

	mu_assert_streq (buffer, content, "r_buf_read_at has corrupted content");

	// Cleanup
	unlink (filename);

	mu_end;
}

int all_tests() {
	mu_run_test (test_r_buf_file_new);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
