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

#include "nugu_ringbuffer.h"

static void test_ringbuffer_default(void)
{
	NuguRingBuffer *buf;
	char tmp[10] = {
		0,
	};
	int item = 0;

	/* buffer alloc & free */
	buf = nugu_ring_buffer_new(1, 10);
	g_assert(buf != NULL);
	nugu_ring_buffer_free(buf);

	/* item size is 1, item max is 10 */
	buf = nugu_ring_buffer_new(1, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 10 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "1234567890", 10) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 10);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	nugu_ring_buffer_clear_items(buf);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 11 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "12345678901", 11) == -1);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	nugu_ring_buffer_clear_items(buf);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 3 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "123", 3) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 3);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '1' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '2' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '3' && item == 1);

	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 0);

	nugu_ring_buffer_free(buf);
}

static void test_ringbuffer_item(void)
{
	NuguRingBuffer *buf;
	char tmp[10] = {
		0,
	};
	int item = 0;

	/* item size is 1, item max is 10 */
	buf = nugu_ring_buffer_new(1, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 3 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "123", 3) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 3);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '1' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '2' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '3' && item == 1);

	nugu_ring_buffer_free(buf);

	/* item size is 2, item max is 10 */
	buf = nugu_ring_buffer_new(2, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 3 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "123", 3) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 2);
	g_assert_cmpmem(tmp, 2, "12", 2);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 0);

	nugu_ring_buffer_free(buf);

	/* item size is 4, item max is 10 */
	buf = nugu_ring_buffer_new(4, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 3 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "123", 3) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 0);

	/* Fill 3 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "123", 3) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 4);
	g_assert_cmpmem(tmp, 4, "1231", 4);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(item == 0);

	nugu_ring_buffer_free(buf);
}

static void test_ringbuffer_ring(void)
{
	NuguRingBuffer *buf;
	char tmp[10] = {
		0,
	};
	int item = 0;

	/* item size is 1, item max is 10 */
	buf = nugu_ring_buffer_new(1, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 10 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "1234567890", 10) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 10);

	/* Fill 5 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "abcde", 5) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 10);

	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '6' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '7' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '8' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '9' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == '0' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == 'a' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == 'b' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == 'c' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == 'd' && item == 1);
	g_assert(nugu_ring_buffer_read_item(buf, tmp, &item) == 0);
	g_assert(*tmp == 'e' && item == 1);

	g_assert(nugu_ring_buffer_get_count(buf) == 0);

	nugu_ring_buffer_free(buf);
}

static void test_ringbuffer_resize(void)
{
	NuguRingBuffer *buf;

	/* item size is 1, item max is 10 */
	buf = nugu_ring_buffer_new(1, 10);
	g_assert(buf != NULL);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 10);

	/* Fill 10 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "1234567890", 10) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 10);
	nugu_ring_buffer_clear_items(buf);

	/* item size is 2, item max is 20 */
	g_assert(nugu_ring_buffer_resize(buf, 2, 20) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 0);
	g_assert(nugu_ring_buffer_get_maxcount(buf) == 20);

	/* Fill 10 bytes */
	g_assert(nugu_ring_buffer_push_data(buf, "1234567890", 10) == 0);
	g_assert(nugu_ring_buffer_get_count(buf) == 5);

	nugu_ring_buffer_free(buf);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/buffer/default", test_ringbuffer_default);
	g_test_add_func("/buffer/item", test_ringbuffer_item);
	g_test_add_func("/buffer/ring", test_ringbuffer_ring);
	g_test_add_func("/buffer/resize", test_ringbuffer_resize);

	return g_test_run();
}
