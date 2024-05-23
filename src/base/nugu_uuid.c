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

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>

#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"
#include "base/nugu_uuid.h"

#define SHA1_SIZE 20
#define MAX_HASH_SIZE 6

static const char *_cached_seed;
static unsigned char _cached_hash[MAX_HASH_SIZE];

int nugu_uuid_fill_random(unsigned char *dest, size_t dest_len)
{
	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(dest_len > 0, -1);

	GRand *rand = g_rand_new();

	for (gsize i = 0; i < dest_len; i++)
		dest[i] = (guchar)g_rand_int_range(rand, 0, 256);

	g_rand_free(rand);

	return 0;
}

int nugu_uuid_convert_base16(const unsigned char *bytes, size_t bytes_len,
			     char *out, size_t out_len)
{
	size_t i;

	g_return_val_if_fail(out_len >= bytes_len * 2, -1);

	for (i = 0; i < bytes_len; i++) {
		uint8_t upper;
		uint8_t lower;

		upper = (bytes[i] & 0xF0) >> 4;
		lower = bytes[i] & 0x0F;

		if (upper > 9)
			out[i * 2] = 'a' + (upper - 10);
		else
			out[i * 2] = '0' + upper;

		if (lower > 9)
			out[i * 2 + 1] = 'a' + (lower - 10);
		else
			out[i * 2 + 1] = '0' + lower;
	}

	if (out_len >= bytes_len * 2 + 1)
		out[bytes_len * 2] = '\0';

	return 0;
}

int nugu_uuid_convert_bytes(const char *base16, size_t base16_len,
			    unsigned char *out, size_t out_len)
{
	size_t i;

	g_return_val_if_fail(base16 != NULL, -1);
	g_return_val_if_fail(base16_len > 0, -1);
	g_return_val_if_fail(out != NULL, -1);
	g_return_val_if_fail(out_len >= base16_len / 2, -1);

	for (i = 0; i < base16_len; i += 2) {
		uint8_t upper;
		uint8_t lower;

		upper = base16[i];
		lower = base16[i + 1];

		if (upper >= 'a')
			upper = upper - 'a' + 10;
		else if (upper >= 'A')
			upper = upper - 'A' + 10;
		else
			upper = upper - '0';

		if (lower >= 'a')
			lower = lower - 'a' + 10;
		else if (lower >= 'A')
			lower = lower - 'A' + 10;
		else
			lower = lower - '0';

		out[i / 2] = (upper << 4) | lower;
	}

	return 0;
}

int nugu_uuid_convert_timespec(const unsigned char *bytes, size_t bytes_len,
			       struct timespec *out_time)
{
	uint64_t t;

	g_return_val_if_fail(bytes != NULL, -1);
	g_return_val_if_fail(bytes_len > 4, -1);
	g_return_val_if_fail(out_time != NULL, -1);

	t = ((uint64_t)bytes[0] << 32) | ((uint64_t)bytes[1] << 24) |
	    (bytes[2] << 16) | (bytes[3] << 8) | bytes[4];
	t += NUGU_BASE_TIMESTAMP_MSEC;

	out_time->tv_sec = t / 1000;
	out_time->tv_nsec = (t % 1000) * 1e+6;

	return 0;
}

/**
 * Generate base16 encoded 32 bytes UUID string
 *
 *  /-------------------------------\
 *  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
 *  |===================+===+=======|
 *  |      time(5)      | v |       ~
 *  |---------------+---+---+-------|
 *  ~  hash(6)      |   random(4)   |
 *  \---------------+---------------/
 *
 * - 5 bytes: time(GMT): EPOCH milliseconds - 1546300800000
 * - 1 bytes: version: 0x1
 * - 6 bytes: hash: cut(sha1(token) 20 bytes, 0, 6 bytes)
 * - 4 bytes: random
 */
char *nugu_uuid_generate_time(void)
{
	unsigned char buf[NUGU_MAX_UUID_SIZE];
	char base16[NUGU_MAX_UUID_SIZE * 2 + 1];
	struct timespec spec;
	const char *seed;

	clock_gettime(CLOCK_REALTIME, &spec);

	seed = nugu_network_manager_peek_token();
	if (seed == NULL) {
		_cached_seed = NULL;
		nugu_uuid_fill_random(_cached_hash, sizeof(_cached_hash));
	} else if (seed != _cached_seed) {
		unsigned char mdbuf[SHA1_SIZE + 1];
		gsize digest_len = SHA1_SIZE;
		GChecksum *checksum;

		checksum = g_checksum_new(G_CHECKSUM_SHA1);
		g_checksum_update(checksum, (const guchar *)seed, strlen(seed));
		g_checksum_get_digest(checksum, mdbuf, &digest_len);
		g_checksum_free(checksum);

		memcpy(_cached_hash, mdbuf, sizeof(_cached_hash));
		_cached_seed = seed;
	}

	nugu_uuid_fill(&spec, _cached_hash, sizeof(_cached_hash), buf,
		       sizeof(buf));

	nugu_uuid_convert_base16(buf, sizeof(buf), base16, sizeof(base16));

	return g_strdup(base16);
}

int nugu_uuid_fill(const struct timespec *time, const unsigned char *hash,
		   size_t hash_len, unsigned char *out, size_t out_len)
{
	uint64_t milliseconds;

	g_return_val_if_fail(time != NULL, -1);
	g_return_val_if_fail(hash != NULL, -1);
	g_return_val_if_fail(hash_len > 0, -1);
	g_return_val_if_fail(out != NULL, -1);
	g_return_val_if_fail(out_len >= NUGU_MAX_UUID_SIZE, -1);

	milliseconds = (uint64_t)(time->tv_sec - NUGU_BASE_TIMESTAMP_SEC) *
		       (uint64_t)1000;
	milliseconds += (time->tv_nsec / 1e+6);

	/* time: 5 bytes */
	out[0] = ((uint64_t)milliseconds & 0xFF00000000) >> 32;
	out[1] = (milliseconds & 0x00FF000000) >> 24;
	out[2] = (milliseconds & 0x0000FF0000) >> 16;
	out[3] = (milliseconds & 0x000000FF00) >> 8;
	out[4] = (milliseconds & 0x00000000FF);

	/* version: 1 byte */
	out[5] = 0x1;

	/* hash: 6 bytes */
	memset(out + 6, 0, 6);
	memcpy(out + 6, hash, (hash_len > 6) ? 6 : hash_len);

	/* random: 4 bytes */
	nugu_uuid_fill_random(out + 12, 4);

	return 0;
}
