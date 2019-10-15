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

#include <openssl/rand.h>
#include <openssl/sha.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "nugu_uuid.h"
#include "nugu_log.h"
#include "nugu_config.h"

#define BASE_TIMESTAMP 1546300800 /* GMT: 2019/1/1 00:00:00 */

static int _phase = -1;

static void _convert_base16(const unsigned char *input, size_t input_len,
			    char *out)
{
	size_t i;

	for (i = 0; i < input_len; i++) {
		uint8_t upper;
		uint8_t lower;

		upper = (input[i] & 0xF0) >> 4;
		lower = input[i] & 0x0F;

		if (upper > 9)
			out[i * 2] = 'a' + (upper - 10);
		else
			out[i * 2] = '0' + upper;

		if (lower > 9)
			out[i * 2 + 1] = 'a' + (lower - 10);
		else
			out[i * 2 + 1] = '0' + lower;
	}

	out[input_len * 2] = '\0';
}

static void _convert_bytes(const char *base16, size_t input_len,
			   unsigned char *out)
{
	size_t i;

	for (i = 0; i < input_len; i += 2) {
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
}

/**
 * Generate base16 format 16 bytes UUID string
 * - 8 bytes: random
 */
EXPORT_API char *nugu_uuid_generate_short(void)
{
	unsigned char buf[8];
	char base16[17];

	RAND_status();
	RAND_bytes(buf, 8);

	_convert_base16(buf, 8, base16);

	return strdup(base16);
}

/**
 * Generate base16 format 32 bytes UUID string
 * - 4 bytes: time
 * - 1 bytes: phase
 * - 3 bytes: random
 * - 8 bytes: cut(sha1(device-id) 20 bytes, 0, 8 bytes)
 */
EXPORT_API char *nugu_uuid_generate_time(void)
{
	unsigned char mdbuf[SHA_DIGEST_LENGTH];
	unsigned char buf[16];
	char base16[33];
	struct timespec spec;
	long milliseconds;
	time_t seconds;
	char *seed;

	if (_phase == -1) {
		char *uuid_phase = nugu_config_get(NUGU_CONFIG_KEY_UUID_PHASE);

		if (uuid_phase) {
			_phase = atoi(uuid_phase);
			free(uuid_phase);
		}
	}

	/**
	 * 0.1 seconds unit is used.
	 *  : (seconds / 10) + (milliseconds / 100)
	 */
	clock_gettime(CLOCK_REALTIME, &spec);
	milliseconds = round(spec.tv_nsec / 1.0e6) / 100;
	seconds = (spec.tv_sec - BASE_TIMESTAMP) * 10 + milliseconds;

	buf[0] = (seconds & 0xFF000000) >> 24;
	buf[1] = (seconds & 0x00FF0000) >> 16;
	buf[2] = (seconds & 0x0000FF00) >> 8;
	buf[3] = (seconds & 0x000000FF);
	buf[4] = _phase;

	RAND_status();
	RAND_bytes(buf + 5, 3);

	seed = nugu_config_get(NUGU_CONFIG_KEY_TOKEN);
	if (seed) {
		SHA1((unsigned char *)seed, strlen(seed), mdbuf);
		free(seed);
		memcpy(buf + 8, mdbuf, 8);
	} else {
		RAND_status();
		RAND_bytes(buf + 8, 8);
	}

	_convert_base16(buf, 16, base16);

	return strdup(base16);
}

EXPORT_API void nugu_dump_timeuuid(const char *uuid)
{
	time_t cur_time;
	unsigned char buf[16];
	struct tm tt;
	char timestr[30];

	_convert_bytes(uuid, 32, buf);

	cur_time = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
	cur_time = BASE_TIMESTAMP + cur_time / 10;
	localtime_r(&cur_time, &tt);
	asctime_r(&tt, timestr);

	/* Remove new-line */
	if (strlen(timestr) > 0)
		timestr[strlen(timestr) - 1] = '\0';

	nugu_dbg("Phase: %d, Time:%d (UTC %s)", buf[4], cur_time, timestr);
}
