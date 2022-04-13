/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_buffer.h"

static void test_buffer_default(void)
{
	NuguBuffer *buf;
	char tmp[30] = {
		0,
	};
	const char *peek_test;
	char *test;

	/* buffer alloc & free */
	buf = nugu_buffer_new(10);
	g_assert(buf != NULL);
	g_assert(nugu_buffer_free(buf, 1) == NULL);

	/* buffer free without data */
	buf = nugu_buffer_new(10);
	g_assert(buf != NULL);
	test = nugu_buffer_free(buf, 0);
	g_assert(test != NULL);

	free(test);

	g_assert(nugu_buffer_add(NULL, NULL, 0) == 0);
	g_assert(nugu_buffer_add(NULL, "1234", 0) == 0);
	g_assert(nugu_buffer_add(NULL, "1234", 4) == 0);
	g_assert(nugu_buffer_add_byte(NULL, 0) == 0);
	g_assert(nugu_buffer_set_byte(NULL, 0, 'A') == -1);

	/* 10 bytes buffer */
	buf = nugu_buffer_new(10);
	g_assert(buf != NULL);
	g_assert(nugu_buffer_get_size(buf) == 0);
	g_assert(nugu_buffer_get_alloc_size(buf) == 10);
	g_assert(nugu_buffer_add(buf, NULL, 0) == 0);
	g_assert(nugu_buffer_add(buf, "1234", 0) == 0);

	/* Fill 9 bytes */
	g_assert(nugu_buffer_add(buf, "123456789", 9) == 9);
	g_assert(nugu_buffer_get_size(buf) == 9);
	g_assert(nugu_buffer_get_alloc_size(buf) == 10);
	g_assert_cmpmem(nugu_buffer_peek(buf), 9, "123456789", 9);

	/* Add 1 byte */
	g_assert(nugu_buffer_add(buf, "0", 1) == 1);
	g_assert(nugu_buffer_get_size(buf) == 10);
	g_assert(nugu_buffer_get_alloc_size(buf) == 10);
	g_assert_cmpmem(nugu_buffer_peek(buf), 10, "1234567890", 10);

	/* Add 5 bytes (resize !) */
	g_assert(nugu_buffer_add(buf, "abcde", 5) == 5);
	g_assert(nugu_buffer_get_size(buf) == 15);
	g_assert(nugu_buffer_get_alloc_size(buf) == 20);
	g_assert_cmpmem(nugu_buffer_peek(buf), 15, "1234567890abcde", 15);

	/* Add 30 bytes (resize !) */
	tmp[29] = 'x';
	g_assert(nugu_buffer_add(buf, tmp, 30) == 30);
	g_assert(nugu_buffer_get_size(buf) == 45);
	g_assert(nugu_buffer_get_alloc_size(buf) == 80);

	peek_test = nugu_buffer_peek(buf);
	g_assert(peek_test[44] == 'x');

	g_assert(nugu_buffer_add_byte(buf, 'A') == 1);
	peek_test = nugu_buffer_peek(buf);
	g_assert(peek_test[45] == 'A');

	g_assert(nugu_buffer_add_byte(buf, '\n') == 1);
	peek_test = nugu_buffer_peek(buf);
	g_assert(peek_test[46] == '\n');

	g_assert(nugu_buffer_free(buf, 1) == NULL);
}

static void test_buffer_byte(void)
{
	NuguBuffer *buf;
	int pos;
	const char *peek_test;

	g_assert(nugu_buffer_find_byte(NULL, '0') == NUGU_BUFFER_NOT_FOUND);
	g_assert(nugu_buffer_peek_byte(NULL, 0) == 0);

	buf = nugu_buffer_new(1);
	g_assert(buf != NULL);
	g_assert(nugu_buffer_set_byte(buf, 0, 'a') == 0);
	g_assert(nugu_buffer_set_byte(buf, 1, 'a') == -1);
	g_assert(nugu_buffer_free(buf, 1) == NULL);

	buf = nugu_buffer_new(0);
	g_assert(buf != NULL);

	g_assert(nugu_buffer_add(buf, "abcde", 5) == 5);
	g_assert(nugu_buffer_find_byte(buf, '0') == NUGU_BUFFER_NOT_FOUND);
	g_assert(nugu_buffer_find_byte(buf, 'a') == 0);
	g_assert(nugu_buffer_find_byte(buf, 'b') == 1);
	g_assert(nugu_buffer_find_byte(buf, 'c') == 2);
	g_assert(nugu_buffer_find_byte(buf, 'd') == 3);
	g_assert(nugu_buffer_find_byte(buf, 'e') == 4);
	g_assert(nugu_buffer_find_byte(buf, '\0') == NUGU_BUFFER_NOT_FOUND);

	g_assert(nugu_buffer_peek_byte(buf, 0) == 'a');
	g_assert(nugu_buffer_peek_byte(buf, 1) == 'b');
	g_assert(nugu_buffer_peek_byte(buf, 2) == 'c');
	g_assert(nugu_buffer_peek_byte(buf, 3) == 'd');
	g_assert(nugu_buffer_peek_byte(buf, 4) == 'e');
	g_assert(nugu_buffer_peek_byte(buf, 5) == 0);

	g_assert(nugu_buffer_clear(buf) == 0);
	g_assert(nugu_buffer_add_byte(buf, 'a') == 1);
	g_assert(nugu_buffer_add_byte(buf, 'b') == 1);
	g_assert(nugu_buffer_add_byte(buf, 'c') == 1);
	g_assert(nugu_buffer_add_byte(buf, 'd') == 1);
	g_assert(nugu_buffer_add_byte(buf, '\0') == 1);
	g_assert(nugu_buffer_peek_byte(buf, 0) == 'a');
	g_assert(nugu_buffer_peek_byte(buf, 1) == 'b');
	g_assert(nugu_buffer_peek_byte(buf, 2) == 'c');
	g_assert(nugu_buffer_peek_byte(buf, 3) == 'd');
	g_assert(nugu_buffer_peek_byte(buf, 4) == '\0');
	peek_test = nugu_buffer_peek(buf);
	g_assert_cmpstr(peek_test, ==, "abcd");
	g_assert(nugu_buffer_set_byte(buf, 1, 'B') == 0);
	g_assert_cmpstr(peek_test, ==, "aBcd");

	g_assert(nugu_buffer_free(buf, 1) == NULL);

	buf = nugu_buffer_new(0);
	g_assert(buf != NULL);

	g_assert(nugu_buffer_add(buf, "+OK\r\n", 5) == 5);

	pos = nugu_buffer_find_byte(buf, '\r');
	g_assert(pos == 3);
	g_assert(nugu_buffer_peek_byte(buf, pos + 1) == '\n');

	g_assert(nugu_buffer_clear(buf) == 0);
	g_assert(nugu_buffer_get_size(buf) == 0);
	g_assert(nugu_buffer_find_byte(buf, '\r') == NUGU_BUFFER_NOT_FOUND);

	g_assert(nugu_buffer_free(buf, 1) == NULL);
}

static void test_buffer_shift(void)
{
	NuguBuffer *buf;
	int pos;

	g_assert(nugu_buffer_shift_left(NULL, 0) == -1);

	buf = nugu_buffer_new(0);
	g_assert(buf != NULL);

	g_assert(nugu_buffer_add(buf, "1234567890", 10) == 10);
	g_assert(nugu_buffer_shift_left(buf, 3) == 0);
	g_assert(nugu_buffer_get_size(buf) == 7);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "4567890");

	g_assert(nugu_buffer_shift_left(buf, 1) == 0);
	g_assert(nugu_buffer_get_size(buf) == 6);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "567890");

	g_assert(nugu_buffer_shift_left(buf, 100) == 0);
	g_assert(nugu_buffer_get_size(buf) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "");

	g_assert(nugu_buffer_add(buf, "+OK 4\r\n", 7) == 7);
	g_assert(nugu_buffer_add(buf, "1234\r\n", 6) == 6);

	/* strip line with '\r\n' */
	pos = nugu_buffer_find_byte(buf, '\r');
	g_assert(pos == 5);
	g_assert(nugu_buffer_peek_byte(buf, pos + 1) == '\n');
	g_assert(nugu_buffer_shift_left(buf, (pos + 1) + 1) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "1234\r\n");

	/* strip line with '\r\n' */
	pos = nugu_buffer_find_byte(buf, '\r');
	g_assert(pos == 4);
	g_assert(nugu_buffer_peek_byte(buf, pos + 1) == '\n');
	g_assert(nugu_buffer_shift_left(buf, (pos + 1) + 1) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "");

	g_assert(nugu_buffer_free(buf, 1) == NULL);
}

static void test_buffer_pop(void)
{
	NuguBuffer *buf;
	char *tmp;

	buf = nugu_buffer_new(0);
	g_assert(buf != NULL);

	g_assert(nugu_buffer_add(buf, "1234567890", 10) == 10);

	tmp = nugu_buffer_pop(buf, 0);
	g_assert(tmp != NULL);
	g_assert(nugu_buffer_get_size(buf) == 0);
	g_assert_cmpstr(tmp, ==, "1234567890");
	free(tmp);

	g_assert(nugu_buffer_add(buf, "1234567890", 10) == 10);
	tmp = nugu_buffer_pop(buf, 4);
	g_assert(tmp != NULL);
	g_assert(nugu_buffer_get_size(buf) == 6);
	g_assert_cmpstr(tmp, ==, "1234");
	free(tmp);

	g_assert(nugu_buffer_free(buf, 1) == NULL);
}

static void test_buffer_clear_from(void)
{
	NuguBuffer *buf;

	buf = nugu_buffer_new(0);
	g_assert(buf != NULL);

	g_assert(nugu_buffer_add(buf, "1234567890", 10) == 10);

	g_assert(nugu_buffer_clear_from(buf, 20) == -1);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "1234567890");

	g_assert(nugu_buffer_clear_from(buf, 10) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "1234567890");

	g_assert(nugu_buffer_clear_from(buf, 9) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "123456789");

	g_assert(nugu_buffer_clear_from(buf, 1) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "1");

	g_assert(nugu_buffer_clear_from(buf, 0) == 0);
	g_assert_cmpstr(nugu_buffer_peek(buf), ==, "");

	g_assert(nugu_buffer_free(buf, 1) == NULL);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/buffer/default", test_buffer_default);
	g_test_add_func("/buffer/byte", test_buffer_byte);
	g_test_add_func("/buffer/shift", test_buffer_shift);
	g_test_add_func("/buffer/pop", test_buffer_pop);
	g_test_add_func("/buffer/clearfrom", test_buffer_clear_from);

	return g_test_run();
}
