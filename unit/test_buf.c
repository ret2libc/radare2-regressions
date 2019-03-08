#include <r_util.h>
#include <r_io.h>
#include <stdlib.h>
#include "minunit.h"

bool test_r_buf_file() {
	RBuffer *b;
	ut8 buffer[1024] = { 0 };
	int r;
	char filename[] = "r2-XXXXXX";
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	// Prepare file
	int fd = mkstemp (filename);
	mu_assert_neq (fd, -1, "mkstemp failed...");
	write (fd, content, length);
	close (fd);

	b = r_buf_new_file (filename, O_RDWR, 0);
	mu_assert_notnull (b, "r_buf_new_file failed");

	ut64 buf_sz = r_buf_size (b);
	mu_assert_eq (buf_sz, length, "file size should be computed");

	r = r_buf_read (b, buffer, length);
	mu_assert_eq (r, length, "r_buf_read_at failed");
	mu_assert_memeq (buffer, content, length, "r_buf_read_at has corrupted content");

	const char *s = "This is a new content";
	const size_t sl = strlen (s);
	bool res = r_buf_set_bytes (b, s, sl);
	mu_assert ("New content should be written", res);

	r_buf_seek (b, 0, R_BUF_SET);
	r = r_buf_read (b, buffer, sl);
	mu_assert_eq (r, sl, "r_buf_read_at failed");
	mu_assert_memeq (buffer, s, sl, "r_buf_read_at has corrupted content");

	const char *s2 = ", hello world";
	const size_t s2l = strlen (s2);
	res = r_buf_append_string (b, s2);
	mu_assert ("string should be appended", res);

	buf_sz = r_buf_size (b);
	mu_assert_eq (buf_sz, sl + s2l, "file size should be computed");

	res = r_buf_resize (b, 10);
	mu_assert ("file should be resized", res);
	buf_sz = r_buf_size (b);
	mu_assert_eq (buf_sz, 10, "file size should be 10");

	const int rl = r_buf_read_at (b, 1, buffer, sizeof (buffer));
	mu_assert_eq (rl, 9, "only 9 bytes can be read from offset 1");
	mu_assert_memeq (buffer, "his is a ", 9, "read right bytes from offset 1");

	r_buf_set_bytes (b, "World", strlen ("World"));

	const char *s3 = "Hello ";
	res = r_buf_prepend_bytes (b, (const ut8 *)s3, strlen (s3));
	mu_assert ("bytes should be prepended", res);
	char *st = r_buf_to_string (b);
	mu_assert_notnull (st, "string should be there");
	mu_assert_streq (st, "Hello World", "hello world there");
	free (st);

	r_buf_insert_bytes (b, 5, (ut8 *)",", 1);
	char *st2 = r_buf_to_string (b);
	mu_assert_notnull (st2, "string should be there");
	mu_assert_streq (st2, "Hello, World", "comma inserted");
	free (st2);

	int gtlen;
	ut8 *gt = r_buf_get_at (b, 5, &gtlen);
	mu_assert_eq (gtlen, 7, "there are only 7 bytes left after idx 5");
	mu_assert_eq (*gt, (ut8)',', "comma should be there");

	// Cleanup
	r_buf_free (b);
	unlink (filename);
	mu_end;
}

int all_tests() {
	mu_run_test (test_r_buf_file);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
