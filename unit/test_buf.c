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
	r_buf_free (b);
	mu_end;
}

bool test_r_buf_get_string(void) {
	ut8 *ch = malloc (128);
	memset(ch, 'A', 127);
	ch[127] = '\0';
	RBuffer *b = r_buf_new_with_bytes (ch, 128);
	char *s = r_buf_get_string (b, 100);
	mu_assert_streq (s, ch + 100, "the string is the same");
	free (s);
	s = r_buf_get_string (b, 0);
	mu_assert_streq (s, ch, "the string is the same");
	free (s);
	s = r_buf_get_string (b, 127);
	mu_assert_streq (s, "\x00", "the string is empty");
	free (s);
	r_buf_free (b);
	free (ch);
	mu_end;
}

bool test_r_buf_get_string_nothing(void) {
	RBuffer *b = r_buf_new_with_bytes ((ut8 *)"\x33\x22", 2);
	char *s = r_buf_get_string (b, 0);
	mu_assert_null (s, "there is no string in the buffer (no null terminator)");
	r_buf_append_bytes (b, (ut8 *)"\x00", 1);
	s = r_buf_get_string (b, 0);
	mu_assert_streq (s, "\x33\x22", "now there is a string because of the null terminator");
	free (s);
	r_buf_free (b);
	mu_end;
}

int all_tests() {
	mu_run_test (test_r_buf_file_new);
	mu_run_test (test_r_buf_get_string);
	mu_run_test (test_r_buf_get_string_nothing);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
