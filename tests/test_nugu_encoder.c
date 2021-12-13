/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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

#include "base/nugu_encoder.h"

static int _check_put_data;
static NuguAudioProperty prop;

static int dummy_create(NuguEncoderDriver *driver, NuguEncoder *enc,
			NuguAudioProperty property)
{
	return 0;
}

static int dummy_destroy(NuguEncoderDriver *driver, NuguEncoder *enc)
{
	return 0;
}

/**
 * add '<>' to data
 */
static int dummy_encode(NuguEncoderDriver *driver, NuguEncoder *enc,
			int is_last, const void *data, size_t data_len,
			NuguBuffer *out_buf)
{
	g_assert_cmpstr(data, ==, "hello");
	_check_put_data = 1;

	nugu_buffer_add(out_buf, "<", 1);
	nugu_buffer_add(out_buf, data, 5);
	nugu_buffer_add(out_buf, ">", 1);

	return 0;
}

static struct nugu_encoder_driver_ops encoder_driver_ops = {
	.create = dummy_create,
	.encode = dummy_encode,
	.destroy = dummy_destroy
};

static int fail_create(NuguEncoderDriver *driver, NuguEncoder *enc,
			NuguAudioProperty property)
{
	return -1;
}

static struct nugu_encoder_driver_ops fail_ops = {
	.create = fail_create,
	.encode = NULL,
	.destroy = NULL
};

static struct nugu_encoder_driver_ops empty_ops = { .encode = NULL };

static void test_encoder_encode(void)
{
	NuguEncoderDriver *driver;
	NuguEncoder *enc;
	size_t result_length = 0;
	void *output;

	driver = nugu_encoder_driver_new("test", NUGU_ENCODER_TYPE_CUSTOM,
					 &encoder_driver_ops);
	g_assert(driver != NULL);

	enc = nugu_encoder_new(driver, prop);
	g_assert(enc != NULL);
	g_assert(nugu_encoder_driver_free(driver) == -1);

	_check_put_data = 0;
	output = nugu_encoder_encode(enc, 0, "hello", 5, &result_length);
	g_assert(output != NULL);
	g_assert(result_length != 0);
	g_assert(_check_put_data == 1);
	g_assert_cmpstr((char *)output, ==, "<hello>");
	free(output);

	nugu_encoder_free(enc);
	g_assert(nugu_encoder_driver_free(driver) == 0);
}

static void test_encoder_default(void)
{
	NuguEncoderDriver *driver;
	NuguEncoderDriver *driver2;
	NuguEncoderDriver *fail_driver;
	NuguEncoder *enc;
	char *mydata = "test";
	size_t result_length = 999;

	/* invalid input */
	g_assert(nugu_encoder_driver_new(NULL, NUGU_ENCODER_TYPE_CUSTOM,
					 NULL) == NULL);
	g_assert(nugu_encoder_driver_new("test", NUGU_ENCODER_TYPE_CUSTOM,
					 NULL) == NULL);
	g_assert(nugu_encoder_driver_new(NULL, NUGU_ENCODER_TYPE_CUSTOM,
					 &empty_ops) == NULL);
	g_assert(nugu_encoder_driver_register(NULL) < 0);
	g_assert(nugu_encoder_driver_remove(NULL) < 0);
	g_assert(nugu_encoder_driver_find(NULL) == NULL);
	g_assert(nugu_encoder_driver_find("") == NULL);

	driver = nugu_encoder_driver_new("test", NUGU_ENCODER_TYPE_CUSTOM,
					 &empty_ops);
	g_assert(driver != NULL);

	g_assert(nugu_encoder_driver_find("test") == NULL);
	g_assert(nugu_encoder_driver_register(driver) == 0);
	g_assert(nugu_encoder_driver_find("test") == driver);
	g_assert(nugu_encoder_driver_find_bytype(NUGU_ENCODER_TYPE_CUSTOM) ==
		 driver);
	g_assert(nugu_encoder_driver_find_bytype(NUGU_ENCODER_TYPE_OPUS) ==
		 NULL);
	g_assert(nugu_encoder_driver_remove(driver) == 0);
	g_assert(nugu_encoder_driver_find("test") == NULL);

	nugu_encoder_driver_free(driver);

	driver = nugu_encoder_driver_new("test1", NUGU_ENCODER_TYPE_CUSTOM,
					 &empty_ops);
	g_assert(driver != NULL);
	g_assert(nugu_encoder_driver_register(driver) == 0);

	driver2 = nugu_encoder_driver_new("test2", NUGU_ENCODER_TYPE_CUSTOM,
					  &empty_ops);
	g_assert(driver2 != NULL);
	g_assert(nugu_encoder_driver_register(driver2) == 0);

	g_assert(nugu_encoder_driver_find("test1") == driver);
	g_assert(nugu_encoder_driver_find("test2") == driver2);

	g_assert(nugu_encoder_driver_remove(driver) == 0);
	g_assert(nugu_encoder_driver_remove(driver2) == 0);

	g_assert(nugu_encoder_new(NULL, prop) == NULL);
	g_assert(nugu_encoder_encode(NULL, 0, NULL, 0, NULL) == NULL);
	g_assert(nugu_encoder_encode(NULL, 0, "", 0, NULL) == NULL);
	g_assert(nugu_encoder_set_driver_data(NULL, NULL) < 0);
	g_assert(nugu_encoder_get_driver_data(NULL) == NULL);

	/* normal encoding test */
	enc = nugu_encoder_new(driver, prop);
	g_assert(enc != NULL);
	g_assert(nugu_encoder_driver_free(driver) == -1);
	g_assert_cmpstr(nugu_encoder_get_codec(enc), ==, "CUSTOM");

	g_assert(nugu_encoder_set_driver_data(enc, mydata) == 0);
	g_assert(nugu_encoder_get_driver_data(enc) == mydata);

	g_assert(nugu_encoder_encode(enc, 0, NULL, 0, &result_length) == NULL);
	g_assert(result_length == 999);
	g_assert(nugu_encoder_encode(enc, 0, "hh", 2, &result_length) == NULL);
	g_assert(result_length == 999);

	nugu_encoder_free(enc);

	/* always encoder creation fail driver */
	fail_driver = nugu_encoder_driver_new("test2", NUGU_ENCODER_TYPE_CUSTOM,
					      &fail_ops);
	g_assert(fail_driver != NULL);
	g_assert(nugu_encoder_driver_register(fail_driver) == 0);

	enc = nugu_encoder_new(fail_driver, prop);
	g_assert(enc == NULL);

	g_assert(nugu_encoder_driver_free(driver2) == 0);
	g_assert(nugu_encoder_driver_free(driver) == 0);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	prop.channel = 1;
	prop.format = NUGU_AUDIO_FORMAT_S16_LE;
	prop.samplerate = NUGU_AUDIO_SAMPLE_RATE_16K;

	g_test_add_func("/encoder/driver_default", test_encoder_default);
	g_test_add_func("/encoder/encode", test_encoder_encode);

	return g_test_run();
}
