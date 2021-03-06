/* $%BEGINLICENSE%$
 Copyright (C) 2009 Sun Microsystems, Inc
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */

#include <glib.h>
#ifndef WIN32
#include <unistd.h> /* close */
#else
#include <windows.h>
#include <io.h>
#endif
#include <glib/gstdio.h>

#include "chassis-log.h"

#include "string-len.h"

#define g_assert_cmpptr(a, cmp, b) g_assert_cmphex(GPOINTER_TO_UINT(a), cmp, GPOINTER_TO_UINT(b))
#if !GLIB_CHECK_VERSION(2, 20, 0)
#define g_assert_no_error(err) g_assert_cmpptr(err, ==, NULL)
#endif

#define START_TEST(x) void (test_chassis_log_extended_ ## x)(void)
#define TEST(x) g_test_add_func(TESTPATH #x, test_chassis_log_extended_ ## x)

static gchar* create_tmp_file_name() {
	gchar *tmp_file_name;
	GError *error;
	gint dummy_fd = g_file_open_tmp("mysql-proxy-unit-test-log.XXXXXX", &tmp_file_name, &error);

	/* Note: Generating a temporary file in this way is _unsafe_, but the easiest way for a test.
	 * Should someone else create a file with this name, then something else is seriously wrong anyway...
	 */
	if (-1 == dummy_fd) {
		g_printerr("Creating a temporary file name failed: %s (%d)", error->message, error->code);
		g_error_free(error);
		g_free(tmp_file_name);
		g_assert_not_reached();
	}
	close(dummy_fd);
	g_unlink(tmp_file_name);
	return tmp_file_name;
}

static void G_GNUC_UNUSED dump_domain_hash_iter(gpointer key, gpointer value, gpointer user_data) {
	gchar *name = (gchar*)key;
	chassis_log_domain_t *domain = (chassis_log_domain_t*)value;
	GString *desc = g_string_new("");
	(void)user_data; /* silence compiler warning */
	g_string_append_printf(desc, "%p : { name: %s, %s, backend: %p, levels: eff=%d min=%d, auto: %s}",
			(void*)domain,
			domain->name,
			(domain->is_implicit ? "implicit" : "explicit"),
			(void*)domain->backend,
			domain->effective_level, domain->min_level,
			(domain->is_autocreated ? "YES" : "NO"));
	g_printerr("  '%s' -> %s\n", name, desc->str);
	g_string_free(desc, TRUE);
}

static gchar* read_file_contents(gchar *file_name) {
	gchar *log_file_contents;
	gsize size;
	GError *error = NULL;

	if (FALSE == g_file_get_contents(file_name, &log_file_contents, &size, &error)) {
		g_printerr("Could not read log file content: '%s': %s (%d)", file_name, error->message, error->code);
		g_error_free(error);
		g_assert_not_reached();
	}
	return log_file_contents;
}

START_TEST(split_name) {
	const gchar *invalid = NULL;
	const gchar *empty = "";
	const gchar *single = "chassis";
	const gchar *three = "chassis.network.backend";
	gchar **parts;
	gsize num_parts;

	/* invalid input results in NULL */
	parts = chassis_log_extract_hierarchy_names(invalid, &num_parts);
	g_assert_cmpptr(NULL, ==, parts);

	/* empty string (aka root domain) */
	parts = chassis_log_extract_hierarchy_names(empty, &num_parts);
	g_assert_cmpptr(NULL, !=, parts);
	g_assert_cmpint(num_parts, ==, 1);
	g_assert_cmpstr("", ==, parts[0]);
	g_strfreev(parts);

	/* single */
	parts = chassis_log_extract_hierarchy_names(single, &num_parts);
	g_assert_cmpptr(NULL, !=, parts);
	g_assert_cmpint(num_parts, ==, 2);
	g_assert_cmpstr("", ==, parts[0]);
	g_assert_cmpstr("chassis", ==, parts[1]);
	g_strfreev(parts);

	/* three */
	parts = chassis_log_extract_hierarchy_names(three, &num_parts);
	g_assert_cmpptr(NULL, !=, parts);
	g_assert_cmpint(num_parts, ==, 4);
	g_assert_cmpstr("", ==, parts[0]);
	g_assert_cmpstr("chassis", ==, parts[1]);
	g_assert_cmpstr("chassis.network", ==, parts[2]);
	g_assert_cmpstr("chassis.network.backend", ==, parts[3]);
	g_strfreev(parts);
}

START_TEST(creation) {
	chassis_log_t *log_ext;
	chassis_log_backend_t *backend;
	chassis_log_domain_t *domain;

	/* some checks for correct internal setup of the structures */
	log_ext = chassis_log_new();
	g_assert_cmpptr(log_ext, !=, NULL);
	g_assert_cmpptr(log_ext->domains, !=, NULL);
	g_assert_cmpptr(log_ext->backends, !=, NULL);

	backend = chassis_log_backend_file_new("/tmp/testlog");
	g_assert_cmpptr(backend, !=, NULL);
	g_assert_cmpint(backend->fd, ==, -1);
	g_assert_cmpptr(backend->fd_lock, !=, NULL);
	g_assert_cmpstr(backend->file_path, ==, "/tmp/testlog");
	g_assert_cmpptr(backend->log_func, !=, NULL);

	domain = chassis_log_domain_new("chassis.log.test", G_LOG_LEVEL_MESSAGE, backend);
	g_assert(domain != NULL);
	g_assert_cmpint(domain->is_implicit, ==, FALSE);
	g_assert_cmpint(domain->min_level, ==, G_LOG_LEVEL_MESSAGE);
	g_assert_cmpstr(domain->name, ==, "chassis.log.test");
	g_assert_cmpptr(domain->backend, ==, backend);

	/* this frees all of the domains and backends as well! */
	chassis_log_free(log_ext);
}

START_TEST(register_backend) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *backend = chassis_log_backend_file_new("/tmp/testlog.log");
	chassis_log_backend_t *backend_dup_name = chassis_log_backend_file_new("/tmp/testlog.log");
	chassis_log_backend_t *backend_lookup;
	chassis_log_backend_t *null_backend = chassis_log_backend_file_new(NULL);

	/* check corner cases */
	g_assert_cmpint(chassis_log_register_backend(log_ext, NULL), ==, FALSE);
	g_assert_cmpint(chassis_log_register_backend(log_ext, null_backend), ==, FALSE);
	chassis_log_backend_free(null_backend);

	/* registering the first backend should not fail */
	if (FALSE == chassis_log_register_backend(log_ext, backend)) {
		chassis_log_backend_free(backend);
		g_assert_not_reached();
	}
	
	backend_lookup = g_hash_table_lookup(log_ext->backends, backend->name);
	g_assert_cmpptr(backend, ==, backend_lookup);

	/* registering the same backend twice should not succeed 
	 * Note: don't free backend in else branch!
	 */
	if (TRUE == chassis_log_register_backend(log_ext, backend)) {
		g_assert_not_reached();
	}

	/* registering a backend with the same file path should not succeed, either */
	if (TRUE == chassis_log_register_backend(log_ext, backend_dup_name)) {
		g_assert_not_reached();
	} else {
		chassis_log_backend_free(backend_dup_name);
	}

	chassis_log_free(log_ext);
}

START_TEST(register_domain) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *backend = chassis_log_backend_file_new("/tmp/testlog.log");
	chassis_log_domain_t *root;
	chassis_log_domain_t *root_reg;
	chassis_log_domain_t *a_reg;
	chassis_log_domain_t *aa_reg;
	chassis_log_domain_t *aab;
	chassis_log_domain_t *aab_reg;

	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_ERROR, backend);
	aab = chassis_log_domain_new("a.a.b", G_LOG_LEVEL_INFO, backend);
	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, aab);

	/* for debugging, dump the domain table */
#if 0
	g_hash_table_foreach(log_ext->domains, dump_domain_hash_iter, NULL);
#endif
	/* this should be the same as the one we registered */
	root_reg = chassis_log_get_domain(log_ext, CHASSIS_LOG_DEFAULT_DOMAIN);
	g_assert_cmpptr(root, ==, root_reg);
	g_assert_cmpint(root_reg->is_implicit, ==, FALSE);
	/* check that the implicit domain for 'a' has been created */
	a_reg = chassis_log_get_domain(log_ext, "a");
	g_assert_cmpptr(a_reg, !=, NULL);
	g_assert_cmpint(a_reg->is_implicit, ==, TRUE);
	/* check that the implicit domain for 'a.a' has been created */
	aa_reg = chassis_log_get_domain(log_ext, "a.a");
	g_assert_cmpptr(aa_reg, !=, NULL);
	g_assert_cmpint(aa_reg->is_implicit, ==, TRUE);
	/* this should be the same as the one we registered */
	aab_reg = chassis_log_get_domain(log_ext, "a.a.b");
	g_assert_cmpptr(aab, ==, aab_reg);
	g_assert_cmpint(aab_reg->is_implicit, ==, FALSE);

	chassis_log_free(log_ext);
}

START_TEST(effective_level) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *backend = chassis_log_backend_file_new("/tmp/testlog.log");
	chassis_log_domain_t *domain1;
	chassis_log_domain_t *domain2;
	GLogLevelFlags effective_level;

	/*
	 * register a root domain with level ERROR,
	 * and one for "a.b" with level INFO
	 * 
	 * the effective level for:
	 *   "a" should be ERROR
	 *   "a.b" should be INFO
	 */
	domain1 = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_ERROR, backend);
	domain2 = chassis_log_domain_new("a.b", G_LOG_LEVEL_INFO, backend);
	chassis_log_register_domain(log_ext, domain1);
	chassis_log_register_domain(log_ext, domain2);
	
	effective_level = chassis_log_get_effective_level(log_ext, "a.b");
	g_assert_cmpint(effective_level, ==, G_LOG_LEVEL_INFO);

	effective_level = chassis_log_get_effective_level(log_ext, "a");
	g_assert_cmpint(effective_level, ==, G_LOG_LEVEL_ERROR);
	/* for debugging, dump the domain table */
#if 0
	g_hash_table_foreach(log_ext->domains, dump_domain_hash_iter, NULL);
#endif

	/* check for the presence implicit domain instance */
	domain2 = chassis_log_get_domain(log_ext, "a");
	g_assert_cmpptr(domain2, !=, NULL);
	g_assert_cmpint(domain2->is_implicit, ==, TRUE);

	chassis_log_free(log_ext);
}

START_TEST(implicit_backend_one_level) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *default_backend = chassis_log_backend_file_new("/tmp/default.log");
	chassis_log_backend_t *ab_backend = chassis_log_backend_file_new("/tmp/ab.log");
	chassis_log_domain_t *root;
	chassis_log_domain_t *a;
	chassis_log_domain_t *ab;

	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_ERROR, default_backend);
	ab = chassis_log_domain_new("a.b", G_LOG_LEVEL_INFO, ab_backend);
	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, ab);
	/*
	 * check that in a hierarchy with an implicit domain its backend is properly set up
	 * we have one default root domain and one for "a.b". the domain for "a" should log to root's log backend
	 */
	a = chassis_log_get_domain(log_ext, "a");
	g_assert_cmpptr(a->backend, ==, default_backend);

	chassis_log_free(log_ext);
}

/* very similar to the backends_one_level test, but checks for correct resolution across multiple implicit domains */
START_TEST(implicit_backends_multi_level) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *default_backend = chassis_log_backend_file_new("/tmp/default.log");
	chassis_log_backend_t *aaab_backend = chassis_log_backend_file_new("/tmp/aaab.log");
	chassis_log_domain_t *root;
	chassis_log_domain_t *intermediate;
	chassis_log_domain_t *aaab;

	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_ERROR, default_backend);
	aaab = chassis_log_domain_new("a.a.a.b", G_LOG_LEVEL_INFO, aaab_backend);
	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, aaab);
	/*
	 * check that in a hierarchy with multiple implicit domains their backend is properly set up
	 * we have one default root domain and one for "a.a.a.b".
	 * the domains for the one's in between should log to root's log backend
	 */
	intermediate = chassis_log_get_domain(log_ext, "a");
	g_assert_cmpptr(intermediate->backend, ==, default_backend);
	intermediate = chassis_log_get_domain(log_ext, "a.a");
	g_assert_cmpptr(intermediate->backend, ==, default_backend);
	intermediate = chassis_log_get_domain(log_ext, "a.a.a");
	g_assert_cmpptr(intermediate->backend, ==, default_backend);

	chassis_log_free(log_ext);
}

START_TEST(open_close_backend) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *backend;
	GError *error = NULL;
	gchar *tmp_file_name;

	tmp_file_name = create_tmp_file_name();

	backend = chassis_log_backend_file_new(tmp_file_name);
	g_assert_cmpptr(backend, !=, NULL);

	if (FALSE == chassis_log_backend_open(backend, &error)) {
		g_printerr("Could not open domain backend '%s': %s (%d)", tmp_file_name, error->message, error->code);
		g_error_free(error);
		g_free(tmp_file_name);
		chassis_log_free(log_ext);
		g_assert_not_reached();
	}

	/* check that we actually have opened a file */
	g_assert_cmpint(backend->fd, !=, -1);
	error = NULL;
	if (FALSE == chassis_log_backend_close(backend, &error)) {
		g_printerr("Could not close domain backend '%s': %s (%d)", tmp_file_name, error->message, error->code);
		g_error_free(error);
		g_free(tmp_file_name);
		chassis_log_free(log_ext);
		g_assert_not_reached();
	}
	/* check that the close has properly cleared the file descriptor */
	g_assert_cmpint(backend->fd, ==, -1);

	g_unlink(tmp_file_name);
	g_free(tmp_file_name);
	chassis_log_free(log_ext);
}

START_TEST(backend_write) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_backend_t *backend;
	gchar *tmp_file_name = create_tmp_file_name();
	GError *error = NULL;
	gchar *log_file_contents;
	
	backend = chassis_log_backend_file_new(tmp_file_name);
	g_assert(backend);
	g_assert_cmpint(TRUE, ==, chassis_log_register_backend(log_ext, backend));

	backend->log_func(backend, G_LOG_LEVEL_MESSAGE, C("foo"));
	error = NULL;
	if (FALSE == chassis_log_backend_close(backend, &error)) {
		g_printerr("Could not close domain backend '%s': %s (%d)", tmp_file_name, error->message, error->code);
		g_error_free(error);
		g_free(tmp_file_name);
		chassis_log_free(log_ext);
		g_assert_not_reached();
	}
	log_file_contents = read_file_contents(tmp_file_name);
	/* backend_write doesn't modify the string, and doesn't append a newline char */
	g_assert_cmpstr("foo\n", ==, log_file_contents);

	g_free(log_file_contents);
	g_unlink(tmp_file_name);
	g_free(tmp_file_name);
	chassis_log_free(log_ext);
}

START_TEST(backend_rotate) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *tmp_file_name = create_tmp_file_name();
	gchar *rotate_file_name = g_strdup_printf("%s.%s", tmp_file_name, "old");
	chassis_log_backend_t *backend;
	GError *error = NULL;
	gchar *log_file_contents;

	/* open a backend, rename the file, then close and open the backend with the original file
	 * after this operation both files should exist and writes to the backend should go to the
	 * file that was re-created by backend_open().
	 */
	
	backend = chassis_log_backend_file_new(tmp_file_name);
	chassis_log_register_backend(log_ext, backend);

	backend->log_func(backend, G_LOG_LEVEL_MESSAGE, C("message to original file"));
	g_rename(tmp_file_name, rotate_file_name);
	backend->log_func(backend, G_LOG_LEVEL_MESSAGE, C("\nsecond message to original file"));
	if (FALSE == chassis_log_backend_reopen(backend, &error)) {
		g_printerr("Could not reopen domain backend '%s': %s (%d)", tmp_file_name, error->message, error->code);
		g_error_free(error);
		g_free(tmp_file_name);
		chassis_log_free(log_ext);
		g_assert_not_reached();
	}
	g_assert_no_error(error);

	backend->log_func(backend, G_LOG_LEVEL_MESSAGE, C("message after rotating file"));

	g_assert(g_file_test(tmp_file_name, G_FILE_TEST_EXISTS));
	g_assert(g_file_test(rotate_file_name, G_FILE_TEST_EXISTS));
	chassis_log_backend_close(backend, NULL);

	/* check the original file's content (which is named .old after the rotation!) */
	log_file_contents = read_file_contents(rotate_file_name);
	g_assert_cmpstr(log_file_contents, ==, "message to original file\n\nsecond message to original file\n");
	g_free(log_file_contents);

	/* check the log file's content after rotation */
	log_file_contents = read_file_contents(tmp_file_name);
	g_assert_cmpstr(log_file_contents, ==, "message after rotating file\n");
	g_free(log_file_contents);

	g_unlink(rotate_file_name);
	g_free(rotate_file_name);
	g_unlink(tmp_file_name);
	g_free(tmp_file_name);
	chassis_log_free(log_ext);
}

START_TEST(rotate_all) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *log_file_a = create_tmp_file_name();
	gchar *log_file_b = create_tmp_file_name();
	gchar *rotate_file_name_a = g_strdup_printf("%s.%s", log_file_a, "old");
	gchar *rotate_file_name_b = g_strdup_printf("%s.%s", log_file_b, "old");
	chassis_log_backend_t *backend_a;
	chassis_log_backend_t *backend_b;
	GError *error = NULL;

	backend_a = chassis_log_backend_file_new(log_file_a);
	g_assert_cmpint(TRUE, ==, chassis_log_register_backend(log_ext, backend_a));

	backend_b = chassis_log_backend_file_new(log_file_b);
	g_assert_cmpint(TRUE, ==, chassis_log_register_backend(log_ext, backend_b));

	g_rename(log_file_a, rotate_file_name_a);
	g_rename(log_file_b, rotate_file_name_b);

	chassis_log_reopen(log_ext);

	g_assert(g_file_test(log_file_a, G_FILE_TEST_EXISTS));
	g_assert(g_file_test(rotate_file_name_a, G_FILE_TEST_EXISTS));
	g_assert(g_file_test(log_file_b, G_FILE_TEST_EXISTS));
	g_assert(g_file_test(rotate_file_name_b, G_FILE_TEST_EXISTS));

	chassis_log_backend_close(backend_a, NULL);
	chassis_log_backend_close(backend_b, NULL);

	g_unlink(rotate_file_name_a);
	g_free(rotate_file_name_a);
	g_unlink(rotate_file_name_b);
	g_free(rotate_file_name_b);
	g_unlink(log_file_a);
	g_free(log_file_a);
	g_unlink(log_file_b);
	g_free(log_file_b);

	chassis_log_free(log_ext);
}

START_TEST(log_func_implicit_domain_creation) {
	chassis_log_t *log_ext = chassis_log_new();
	chassis_log_domain_t *root;
	chassis_log_backend_t *backend;
	gchar *tmp_file_name = create_tmp_file_name();
	gchar *log_file_contents;

	g_assert(tmp_file_name);

	backend = chassis_log_backend_file_new(tmp_file_name);
	chassis_log_register_backend(log_ext, backend);
	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_MESSAGE, backend);
	chassis_log_register_domain(log_ext, root);
	
	/* install our default handler, don't bother with registering each log domain individually
	 * revert the fatal mask g_test_init sets.
	 */
	g_log_set_always_fatal(G_LOG_FATAL_MASK);
	g_log_set_default_handler(chassis_log_func, log_ext);
	/* log two messages, one directly going to the root domain (which we set explicitly)
	 * and one on an ad-hoc domain which bubbles up to the root domain
	 * check that both messages get written to disk
	 */
	g_critical("root-direct");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "a.b"
	g_message("root-indirect");
	g_debug("not written to the log file");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN NULL

	/* for debugging, dump the domain table */
#if 0
	g_hash_table_foreach(log_ext->domains, dump_domain_hash_iter, NULL);
#endif

	chassis_log_free(log_ext);

	log_file_contents = read_file_contents(tmp_file_name);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "root-direct"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "root-indirect"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "not written to the log file"), ==, NULL);
	g_free(log_file_contents);
	g_unlink(tmp_file_name);
	g_free(tmp_file_name);
}

START_TEST(force_log_all) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *log_file_root =  create_tmp_file_name();
	gchar *log_file_aab = create_tmp_file_name();
	gchar *log_file_b  = create_tmp_file_name();
	gchar *log_file_contents;
	chassis_log_domain_t *root, *aab, *b;
	chassis_log_backend_t *backend_root, *backend_aab, *backend_b;

	/* install our default handler, don't bother with registering each log domain individually
	 * revert the fatal mask g_test_init sets.
	 */
	g_log_set_always_fatal(G_LOG_FATAL_MASK);
	g_log_set_default_handler(chassis_log_func, log_ext);

	/* set up explicit backends for:
	 *  - root domain
	 *  - "a.a.b"
	 *  - "b"
	 * We will write to "a.a.b" and "a.a", thus the backends for root and "a.a.b" should contain messages, but "b" should be empty.
	 * Then we will broadcast a log message to all backends and check that it ended up in all of them.
	 */
	backend_root = chassis_log_backend_file_new(log_file_root);
	backend_aab = chassis_log_backend_file_new(log_file_aab);
	backend_b = chassis_log_backend_file_new(log_file_b);
	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_MESSAGE, backend_root);
	aab = chassis_log_domain_new("a.a.b", G_LOG_LEVEL_MESSAGE, backend_aab);
	b = chassis_log_domain_new("b", G_LOG_LEVEL_DEBUG, backend_b);
	
	chassis_log_register_backend(log_ext, backend_root);
	chassis_log_register_backend(log_ext, backend_aab);
	chassis_log_register_backend(log_ext, backend_b);

	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, aab);
	chassis_log_register_domain(log_ext, b);
	
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "a.a.b"

	g_message("aab-direct");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "a.a"

	g_message("aa-root-indirect");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN NULL
	
	chassis_log_force_log_all(log_ext, "broadcast");
	/* free the log structure, to ensure that all files are flushed and closed. */
	chassis_log_free(log_ext);
	
	/* root log file should contain "aa-root-indirect" and "broadcast" */
	log_file_contents = read_file_contents(log_file_root);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "aa-root-indirect"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "broadcast"), !=, NULL);
	g_free(log_file_contents);
	
	/* "a.a.b" should contain "aab-direct" and "broadcast" */
	log_file_contents = read_file_contents(log_file_aab);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "aab-direct"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "broadcast"), !=, NULL);
	g_free(log_file_contents);
	
	/* "b" should not contain either "aa-root-indirect" nor "aab-direct", but must have "broadcast" */
	log_file_contents = read_file_contents(log_file_b);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "aa-root-indirect"), ==, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "aab-direct"), ==, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "broadcast"), !=, NULL);
	g_free(log_file_contents);

	g_unlink(log_file_root);
	g_free(log_file_root);
	g_unlink(log_file_aab);
	g_free(log_file_aab);
	g_unlink(log_file_b);
	g_free(log_file_b);
}

START_TEST(coalescing) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *log_file_root =  create_tmp_file_name();
	gchar *log_file_contents;
	gchar *broadcast_first, *broadcast_last;
	chassis_log_domain_t *root;
	chassis_log_backend_t *backend_root;

	/* install our default handler, don't bother with registering each log domain individually
	 * revert the fatal mask g_test_init sets.
	 */
	g_log_set_always_fatal(G_LOG_FATAL_MASK);
	g_log_set_default_handler(chassis_log_func, log_ext);

	backend_root = chassis_log_backend_file_new(log_file_root);
	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_MESSAGE, backend_root);
	
	chassis_log_register_backend(log_ext, backend_root);
	chassis_log_register_domain(log_ext, root);

	/* log six messages:
	 *  - "repeat" on MESSAGE
	 *  - "repeat" on DEBUG
	 *  - "repeat" on MESSAGE
	 *  - "no-repeat" on MESSAGE
	 *  - "broadcast" is broadcasted to all backends
	 *  - "broadcast" is broadcasted to all backends
	 * 
	 * We expect the first message to be printed immediately, the second will be ignored and the third is hidden because it will be
	 * coalesced with the first.
	 * The fourth message will trigger a report that the last-logged message was repeated one time.
	 * Both broadcast will not be coalesced, but are written verbatim.
	 */
	g_message("repeat");
	g_debug("repeat");
	g_message("repeat");
	g_message("no-repeat");
	chassis_log_force_log_all(log_ext, "broadcast");
	chassis_log_force_log_all(log_ext, "broadcast");
	
	chassis_log_free(log_ext);

	/* root log file should contain "repeat", "last message repeated 1 times" and "no-repeat" entries */
	log_file_contents = read_file_contents(log_file_root);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "repeat"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "last message repeated 1 times"), !=, NULL);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "no-repeat"), !=, NULL);
	/* we can't strstr the log file twice because we would find the first one both times.
	 * since we expect exactly two different occurrences, we look from the beginning and end: those two strings must not be at the same
	 * position. In fact, they must be ordered, with first coming before last (since they are in one memory region this works).
	 */
	broadcast_first = g_strstr_len(log_file_contents, -1, "broadcast");
	broadcast_last = g_strrstr(log_file_contents, "broadcast");
	g_assert_cmpptr(broadcast_first, <, broadcast_last);
	g_free(log_file_contents);
	
	g_unlink(log_file_root);
	g_free(log_file_root);
}

START_TEST(coalescing_domain_names) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *log_file_root =  create_tmp_file_name();
	gchar *log_file_contents;
	chassis_log_domain_t *root, *aa, *ab;
	chassis_log_backend_t *backend_root;

	/* install our default handler, don't bother with registering each log domain individually
	 * revert the fatal mask g_test_init sets.
	 */
	g_log_set_always_fatal(G_LOG_FATAL_MASK);
	g_log_set_default_handler(chassis_log_func, log_ext);

	backend_root = chassis_log_backend_file_new(log_file_root);
	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_MESSAGE, backend_root);
	aa = chassis_log_domain_new("a.a", G_LOG_LEVEL_MESSAGE, backend_root);
	ab = chassis_log_domain_new("a.b", G_LOG_LEVEL_MESSAGE, backend_root);
	
	chassis_log_register_backend(log_ext, backend_root);
	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, aa);
	chassis_log_register_domain(log_ext, ab);

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN CHASSIS_LOG_DEFAULT_DOMAIN
	g_message("repeat");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "a.a"
	g_warning("repeat");
	g_warning("repeat");	/* test that any repeated messages from the same domain are collapsed properly */
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "a.b"
	g_message("repeat");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "unrelated"
	g_message("no-repeat");
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN NULL

	chassis_log_free(log_ext);

	log_file_contents = read_file_contents(log_file_root);
	g_print("log file:\n%s", log_file_contents);
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "[global] (message) repeat"), !=, NULL);
	/* the domain names are in a hash, we can't rely on their order when printed */
	if (NULL != g_strstr_len(log_file_contents, -1, "[a.a, a.b] last message repeated 3 times") &&
			NULL != g_strstr_len(log_file_contents, -1, "[a.b, a.a] last message repeated 3 times")) {
		g_assert_cmpstr("duplicated domain names", ==, "but they should be collapsed");
	}
	g_assert_cmpptr(g_strstr_len(log_file_contents, -1, "[unrelated] (message) no-repeat"), !=, NULL);

	g_free(log_file_contents);
	g_unlink(log_file_root);
	g_free(log_file_root);
}

START_TEST(effective_level_correction) {
	chassis_log_t *log_ext = chassis_log_new();
	gchar *log_file_root =  create_tmp_file_name();
	gchar *log_file_abcd = create_tmp_file_name();
	gchar *log_file_a  = create_tmp_file_name();
	chassis_log_domain_t *root, *abcd, *a, *ab, *abc;
	chassis_log_backend_t *backend_root, *backend_abcd, *backend_a;

	/* When registering domains we need to take into account that a single new domain can affect multiple implicit
	 * domains in their effective levels, namely all implicit domains below it, up until the first implicit domain found on each path.
	 * 
	 * The root domain must always be explicit, because otherwise the implicit domains would have no backend and no effective level.
	 * For example, given these explicit domains:
	 *   "", level CRITICAL, backend "root.log" (the root domain)
	 *   "a.b.c.d", level DEBUG, backend "abcd.log"
	 * The effective level and backend for "a", "a.b", "a.b.c" is CRITICAL, backend "root.log".
	 * When we now register an explicit domain for "a" with:
	 *   "a", level WARNING, backend "a.log",
	 * the effective level and backend for "a.b" and "a.b.c" changes to WARNING, backend "a.log".
	 * 
	 * This support is necessary to allow registering domains without sorting them topologically first.
	 */

	backend_root = chassis_log_backend_file_new(log_file_root);
	backend_abcd = chassis_log_backend_file_new(log_file_abcd);
	chassis_log_register_backend(log_ext, backend_root);
	chassis_log_register_backend(log_ext, backend_abcd);
	root = chassis_log_domain_new(CHASSIS_LOG_DEFAULT_DOMAIN, G_LOG_LEVEL_CRITICAL, backend_root);
	abcd = chassis_log_domain_new("a.b.c.d", G_LOG_LEVEL_DEBUG, backend_abcd);
	chassis_log_register_domain(log_ext, root);
	chassis_log_register_domain(log_ext, abcd);

	/* check the implicit domains that were created, by checking against a known explicit one */
	root = chassis_log_get_domain(log_ext, CHASSIS_LOG_DEFAULT_DOMAIN);
	g_assert_cmpint(root->is_implicit, ==, FALSE);
	a = chassis_log_get_domain(log_ext, "a");
	g_assert_cmpptr(root->backend, ==, a->backend);
	g_assert_cmpint(a->is_implicit, ==, TRUE);
	g_assert_cmpint(a->effective_level, ==, G_LOG_LEVEL_CRITICAL);

	ab = chassis_log_get_domain(log_ext, "a.b");
	g_assert_cmpptr(root->backend, ==, ab->backend);
	g_assert_cmpint(ab->is_implicit, ==, TRUE);
	g_assert_cmpint(ab->effective_level, ==, G_LOG_LEVEL_CRITICAL);

	abc = chassis_log_get_domain(log_ext, "a.b.c");
	g_assert_cmpptr(root->backend, ==, abc->backend);
	g_assert_cmpint(abc->is_implicit, ==, TRUE);
	g_assert_cmpint(abc->effective_level, ==, G_LOG_LEVEL_CRITICAL);

	/* add a new domain for "a"
	 * this must cause an update to the effective levels of "a.b" and "a.b.c",
	 * but note that you have to "get" the domains again, because the updates might be lazy!
	 */
	backend_a = chassis_log_backend_file_new(log_file_a);
	chassis_log_register_backend(log_ext, backend_a);
	a = chassis_log_domain_new("a", G_LOG_LEVEL_WARNING, backend_a);
	chassis_log_register_domain(log_ext, a);

	a = chassis_log_get_domain(log_ext, "a");
	ab = chassis_log_get_domain(log_ext, "a.b");
	g_assert_cmpptr(a->backend, ==, ab->backend);
	g_assert_cmpint(a->effective_level, ==, G_LOG_LEVEL_WARNING);
	g_assert_cmpint(a->is_implicit, ==, FALSE);
	g_assert_cmpint(ab->is_implicit, ==, TRUE);
	g_assert_cmpint(ab->effective_level, ==, G_LOG_LEVEL_WARNING);
	
	abc = chassis_log_get_domain(log_ext, "a.b.c");
	g_assert_cmpptr(a->backend, ==, abc->backend);
	g_assert_cmpint(abc->is_implicit, ==, TRUE);
	g_assert_cmpint(abc->effective_level, ==, G_LOG_LEVEL_WARNING);
	
	/* finally make sure the explicit domain we registered has not been changed */
	abcd = chassis_log_get_domain(log_ext, "a.b.c.d");
	g_assert_cmpint(abcd->is_implicit, ==, FALSE);
	g_assert_cmpptr(abcd->backend, ==, backend_abcd);
	g_assert_cmpint(abcd->effective_level, ==, G_LOG_LEVEL_DEBUG);

	chassis_log_free(log_ext);
	/* free and unlink the files */
	g_unlink(log_file_root);
	g_free(log_file_root);
	g_unlink(log_file_abcd);
	g_free(log_file_abcd);
	g_unlink(log_file_a);
	g_free(log_file_a);
}

int main(int argc, char **argv) {
	g_thread_init(NULL);
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

#define TESTPATH "/core/log/extended/"
	TEST(split_name);
	TEST(creation);
	TEST(register_backend);
	TEST(register_domain);
	TEST(effective_level);
	TEST(implicit_backend_one_level);
	TEST(implicit_backends_multi_level);
	TEST(open_close_backend);
	TEST(backend_write);
#ifndef WIN32 /* these tests will not work on Windows: cannot rename open files */
	TEST(backend_rotate);
	TEST(rotate_all);
#endif
	TEST(log_func_implicit_domain_creation);
	TEST(force_log_all);
	TEST(coalescing);
	TEST(coalescing_domain_names);
	TEST(effective_level_correction);
	return g_test_run();
}

