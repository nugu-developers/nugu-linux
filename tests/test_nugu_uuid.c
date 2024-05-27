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
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include <glib.h>

#include "base/nugu_uuid.h"
#include "base/nugu_log.h"

/* "2020-01-22 01:15:59.039" */
#define TEST1 "07c41c48bf01cb0eb29b006139b1abbb"
static unsigned char BYTES1[] = { 0x07, 0xc4, 0x1c, 0x48, 0xbf, 0x01,
				  0xcb, 0x0e, 0xb2, 0x9b, 0x00, 0x61,
				  0x39, 0xb1, 0xab, 0xbb };

/* "2020-01-22 01:16:49.627" */
#define TEST2 "07c41d0e5b01e7f40b3c68ce7da57153"
static unsigned char BYTES2[] = { 0x07, 0xc4, 0x1d, 0x0e, 0x5b, 0x01,
				  0xe7, 0xf4, 0x0b, 0x3c, 0x68, 0xce,
				  0x7d, 0xa5, 0x71, 0x53 };

/* "2020-01-23 00:43:41.516" */
#define TEST3 "07c925144c014c0f32f1f282026f4e73"
static unsigned char BYTES3[] = { 0x07, 0xc9, 0x25, 0x14, 0x4c, 0x01,
				  0x4c, 0x0f, 0x32, 0xf1, 0xf2, 0x82,
				  0x02, 0x6f, 0x4e, 0x73 };

static void test_nugu_uuid_base16(void)
{
	unsigned char buf[NUGU_MAX_UUID_SIZE];
	char base16_buf[NUGU_MAX_UUID_SIZE * 2 + 1];

	/* Convert base16-encoded 32 bytes to raw 16 bytes */
	g_assert(nugu_uuid_convert_bytes(TEST1, strlen(TEST1), buf,
					 sizeof(buf)) == 0);
	g_assert_cmpmem(buf, sizeof(buf), BYTES1, sizeof(BYTES1));

	g_assert(nugu_uuid_convert_bytes(TEST2, strlen(TEST2), buf,
					 sizeof(buf)) == 0);
	g_assert_cmpmem(buf, sizeof(buf), BYTES2, sizeof(BYTES2));

	g_assert(nugu_uuid_convert_bytes(TEST3, strlen(TEST3), buf,
					 sizeof(buf)) == 0);
	g_assert_cmpmem(buf, sizeof(buf), BYTES3, sizeof(BYTES3));

	/* Convert raw 16 bytes to base16-encoded 32 bytes */
	g_assert(nugu_uuid_convert_base16(BYTES1, sizeof(BYTES1), base16_buf,
					  sizeof(base16_buf)) == 0);
	g_assert_cmpmem(base16_buf, sizeof(base16_buf), TEST1, sizeof(TEST1));

	g_assert(nugu_uuid_convert_base16(BYTES2, sizeof(BYTES2), base16_buf,
					  sizeof(base16_buf)) == 0);
	g_assert_cmpmem(base16_buf, sizeof(base16_buf), TEST2, sizeof(TEST2));

	g_assert(nugu_uuid_convert_base16(BYTES3, sizeof(BYTES3), base16_buf,
					  sizeof(base16_buf)) == 0);
	g_assert_cmpmem(base16_buf, sizeof(base16_buf), TEST3, sizeof(TEST3));
}

static void test_nugu_uuid_time(void)
{
	unsigned char buf[NUGU_MAX_UUID_SIZE];
	unsigned char outbuf[NUGU_MAX_UUID_SIZE];
	struct tm t_tm;
	time_t t_sec;
	gint64 msec;
	char timestr[32];
	unsigned char dummy_hash[6] = { '0', '0', '0', '0', '0', '0' };

	/* Convert 5 bytes to integer from NUGU_BASE_TIMESTAMP_MSEC */
	g_assert(nugu_uuid_convert_bytes(TEST1, strlen(TEST1), buf,
					 sizeof(buf)) == 0);

	g_assert(nugu_uuid_convert_msec(buf, sizeof(buf), &msec) == 0);
	g_assert(msec == 1579655759039);
	t_sec = msec / 1000;
#ifdef _WIN32
	gmtime_s(&t_tm, &t_sec);
#else
	gmtime_r(&t_sec, &t_tm);
#endif
	g_assert(strftime(timestr, sizeof(timestr), "%F %T", &t_tm) != 0);
	g_assert_cmpstr(timestr, ==, "2020-01-22 01:15:59");

	/* Convert timespec to 5 bytes */
	g_assert(nugu_uuid_fill(msec, dummy_hash, sizeof(dummy_hash) - 5,
				outbuf, sizeof(outbuf)) == 0);
	g_assert_cmpmem(buf, 5, outbuf, 5);

	/* Convert 5 bytes to integer from NUGU_BASE_TIMESTAMP_MSEC */
	g_assert(nugu_uuid_convert_bytes(TEST2, strlen(TEST2), buf,
					 sizeof(buf)) == 0);
	g_assert(nugu_uuid_convert_msec(buf, sizeof(buf), &msec) == 0);
	g_assert(msec == 1579655809627);
	t_sec = msec / 1000;
#ifdef _WIN32
	gmtime_s(&t_tm, &t_sec);
#else
	gmtime_r(&t_sec, &t_tm);
#endif
	g_assert(strftime(timestr, sizeof(timestr), "%F %T", &t_tm) != 0);
	g_assert_cmpstr(timestr, ==, "2020-01-22 01:16:49");

	/* Convert timespec to 5 bytes */
	g_assert(nugu_uuid_fill(msec, dummy_hash, sizeof(dummy_hash) - 5,
				outbuf, sizeof(outbuf)) == 0);
	g_assert_cmpmem(buf, 5, outbuf, 5);

	/* Convert 5 bytes to integer from NUGU_BASE_TIMESTAMP_MSEC */
	g_assert(nugu_uuid_convert_bytes(TEST3, strlen(TEST3), buf,
					 sizeof(buf)) == 0);
	g_assert(nugu_uuid_convert_msec(buf, sizeof(buf), &msec) == 0);
	g_assert(msec == 1579740221516);
	t_sec = msec / 1000;
#ifdef _WIN32
	gmtime_s(&t_tm, &t_sec);
#else
	gmtime_r(&t_sec, &t_tm);
#endif
	g_assert(strftime(timestr, sizeof(timestr), "%F %T", &t_tm) != 0);
	g_assert_cmpstr(timestr, ==, "2020-01-23 00:43:41");

	/* Convert timespec to 5 bytes */
	g_assert(nugu_uuid_fill(msec, dummy_hash, sizeof(dummy_hash) - 5,
				outbuf, sizeof(outbuf)) == 0);
	g_assert_cmpmem(buf, 5, outbuf, 5);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/nugu_uuid/base16", test_nugu_uuid_base16);
	g_test_add_func("/nugu_uuid/time", test_nugu_uuid_time);

	return g_test_run();
}
