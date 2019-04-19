#include <r_util.h>
#include <r_io.h>
#include <stdlib.h>
#include "minunit.h"

bool test_buf(RBuffer *b) {
	ut8 buffer[1024] = { 0 };
	const char *content = "Something To\nSay Here..";
	const int length = 23;
	int r;

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

	r_buf_seek (b, 0, R_BUF_SET);
	r = r_buf_read (b, buffer, 3);
	mu_assert_eq (r, 3, "r_buf_read_at failed");
	mu_assert_memeq (buffer, "Thi", 3, "r_buf_read_at has corrupted content");
	r = r_buf_read (b, buffer, 5);
	mu_assert_eq (r, 5, "r_buf_read_at failed");
	mu_assert_memeq (buffer, "s is ", 5, "r_buf_read_at has corrupted content");

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

	r = r_buf_seek (b, 0x100, R_BUF_SET);
	mu_assert_eq (r, 0x100, "moving seek out of current length");
	r = r_buf_write (b, "mydata", 6);
	mu_assert_eq (r, 6, "writes 6 bytes");
	r = r_buf_read_at (b, 0xf0, buffer, sizeof (buffer));
	mu_assert_eq (r, 0x16, "read 16 bytes at the end of gap and new data");
	mu_assert_memeq (buffer, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16, "first bytes should be 0");
	mu_assert_memeq (buffer + 0x10, "mydata", 6, "then there is mydata");

	r_buf_set_bytes (b, "Hello", 5);
	RBuffer *sec_buf = r_buf_new_with_bytes (" second", 7);
	res = r_buf_append_buf (b, sec_buf);
	mu_assert ("append buf should succeed", res);
	char *st3 = r_buf_to_string (b);
	mu_assert_streq (st3, "Hello second", "append buf correctly");
	free (st3);
	r_buf_free (sec_buf);

	sec_buf = r_buf_new_with_bytes ("123456789", 9);
	res = r_buf_append_buf_slice (b, sec_buf, 5, 3);
	mu_assert ("append buf slice should succeed", res);
	char *st4 = r_buf_to_string (b);
	mu_assert_streq (st4, "Hello second678", "append buf slice correctly");
	free (st4);
	r_buf_free (sec_buf);

	return MU_PASSED;
}

bool test_r_buf_file() {
	RBuffer *b;
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

	if (test_buf (b) != MU_PASSED) {
		mu_fail ("test failed");
	}

	// Cleanup
	r_buf_free (b);
	unlink (filename);
	mu_end;
}

bool test_r_buf_bytes() {
	RBuffer *b;
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	b = r_buf_new_with_bytes ((const ut8 *)content, length);
	mu_assert_notnull (b, "r_buf_new_with_bytes failed");

	if (test_buf (b) != MU_PASSED) {
		mu_fail ("test failed");
	}

	// Cleanup
	r_buf_free (b);
	mu_end;
}

bool test_r_buf_mmap() {
	RBuffer *b;
	char filename[] = "r2-XXXXXX";
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	// Prepare file
	int fd = mkstemp (filename);
	mu_assert_neq (fd, -1, "mkstemp failed...");
	write (fd, content, length);
	close (fd);

	b = r_buf_new_mmap (filename, R_PERM_RW);
	mu_assert_notnull (b, "r_buf_new_mmap failed");

	if (test_buf (b) != MU_PASSED) {
		mu_fail ("test failed");
	}

	// Cleanup
	r_buf_free (b);
	mu_end;
}

bool test_r_buf_io() {
	RBuffer *b;
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	RIO *io = r_io_new ();
	RIODesc *desc = r_io_open_at (io, "file:///tmp/r-buf-io.test", R_PERM_RW | R_PERM_CREAT, 0644, 0);
	mu_assert_notnull (desc, "file should be opened for writing");

	bool res = r_io_write_at (io, 0, content, length);
	mu_assert ("initial content should be written", res);

	RIOBind bnd;
	r_io_bind (io, &bnd);

	b = r_buf_new_with_io(&bnd, desc->fd);
	mu_assert_notnull (b, "r_buf_new_file failed");

	if (test_buf (b) != MU_PASSED) {
		mu_fail ("test failed");
	}

	// Cleanup
	r_buf_free (b);
	r_io_close (io);
	r_io_free (io);
	mu_end;
}

bool test_r_buf_bytes_steal() {
	RBuffer *b;
	const char *content = "Something To\nSay Here..";
	const int length = 23;

	b = r_buf_new_with_bytes ((const ut8 *)content, length);
	mu_assert_notnull (b, "r_buf_new_file failed");
	char *s = r_buf_to_string (b);
	mu_assert_streq (s, content, "content is right");
	free (s);

	// Cleanup
	r_buf_free (b);
	mu_end;
}

bool test_r_buf_format() {
	RBuffer *b = r_buf_new ();
	uint16_t a[] = {0xdead, 0xbeef, 0xcafe, 0xbabe};
	char buf[4 * sizeof (uint16_t)];

	r_buf_fwrite (b, (ut8 *)a, "4s", 1);
	r_buf_read_at (b, 0, buf, sizeof (buf));
	mu_assert_memeq (buf, "\xad\xde\xef\xbe\xfe\xca\xbe\xba", sizeof(buf), "fwrite");

	r_buf_fread_at (b, 0, (ut8 *)a, "S", 4);
	mu_assert_eq (a[0], 0xadde, "first");
	mu_assert_eq (a[1], 0xefbe, "second");
	mu_assert_eq (a[2], 0xfeca, "third");
	mu_assert_eq (a[3], 0xbeba, "fourth");

	r_buf_free (b);
	mu_end;
}

bool test_r_buf_with_buf() {
	char filename[] = "r2-XXXXXX";
	const char *content = "Something To\nSay Here..";
	const int length = 23;
	RBuffer *buf = r_buf_new_with_bytes (content, length);

	RBuffer *b = r_buf_new_with_buf (buf);
	mu_assert_notnull (b, "r_buf_new_with_buf failed");
	r_buf_free (buf);

	if (test_buf (b) != MU_PASSED) {
		mu_fail ("r_buf_with_buf failed");
	}

	// Cleanup
	r_buf_free (b);
	mu_end;
}

bool test_r_buf_slice() {
	char filename[] = "r2-XXXXXX";
	const char *content = "AAAAAAAAAASomething To\nSay Here..BBBBBBBBBB";
	const int length = strlen (content);
	RBuffer *buf = r_buf_new_with_bytes (content, length);
	char buffer[1024];

	RBuffer *b = r_buf_new_slice (buf, 10, 23);
	mu_assert_notnull (b, "r_buf_new_slice failed");

	ut64 buf_sz = r_buf_size (b);
	mu_assert_eq (buf_sz, 23, "file size should be computed");

	int r = r_buf_read_at (b, 0, buffer, 23);
	mu_assert_eq (r, 23, "r_buf_read_at failed");
	mu_assert_memeq (buffer, "Something To\nSay Here..", 23, "r_buf_read_at has corrupted content");

	r_buf_seek (b, 3, R_BUF_SET);
	r = r_buf_read (b, buffer, 3);
	mu_assert_eq (r, 3, "only 3 read");
	mu_assert_memeq (buffer, (ut8 *)"eth", 3, "base should be considered");

	r = r_buf_read (b, buffer, 40);
	mu_assert_eq (r, 23 - 6, "consider limit");

	bool res = r_buf_resize (b, 30);
	mu_assert ("file should be resized", res);
	buf_sz = r_buf_size (b);
	mu_assert_eq (buf_sz, 30, "file size should be 30");

	// Cleanup
	r_buf_free (b);
	r_buf_free (buf);
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
	mu_run_test (test_r_buf_file);
	mu_run_test (test_r_buf_bytes);
	mu_run_test (test_r_buf_mmap);
	mu_run_test (test_r_buf_with_buf);
	mu_run_test (test_r_buf_slice);
	mu_run_test (test_r_buf_io);
	mu_run_test (test_r_buf_bytes_steal);
	mu_run_test (test_r_buf_format);
	mu_run_test (test_r_buf_get_string);
	mu_run_test (test_r_buf_get_string_nothing);
	return tests_passed != tests_run;
}

int main(int argc, char **argv) {
	return all_tests();
}
