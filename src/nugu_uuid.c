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

static int _convert_base16(const unsigned char *input, size_t input_len,
			    char *out, size_t output_len)
{
	size_t i;

	if (output_len < input_len * 2) {
		nugu_error("buffer size not enough");
		return -1;
	}

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

	return 0;
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

	_convert_base16(buf, sizeof(buf), base16, sizeof(base16));

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
	unsigned char buf[16];
	char base16[33];
	struct timespec spec;
	long milliseconds;
	time_t seconds;
	const char *seed;

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
	buf[4] = 0;

	RAND_status();
	RAND_bytes(buf + 5, 3);

	seed = nugu_config_get(NUGU_CONFIG_KEY_TOKEN);
	if (seed) {
		unsigned char mdbuf[SHA_DIGEST_LENGTH];

		SHA1((unsigned char *)seed, strlen(seed), mdbuf);
		memcpy(buf + 8, mdbuf, 8);
	} else {
		RAND_status();
		RAND_bytes(buf + 8, 8);
	}

	_convert_base16(buf, sizeof(buf), base16, sizeof(base16));

	return strdup(base16);
}
