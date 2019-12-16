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

#include "base/nugu_decoder.h"

static int _check_put_data;

static int dummy_create(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	return 0;
}

static int dummy_destroy(NuguDecoderDriver *driver, NuguDecoder *dec)
{
	return 0;
}

/**
 * add '[]' to data
 */
static int dummy_decode(NuguDecoderDriver *driver, NuguDecoder *dec,
			const void *data, size_t data_len, NuguBuffer *out_buf)
{
	g_assert_cmpstr(data, ==, "hello");
	_check_put_data = 1;

	nugu_buffer_add(out_buf, "<", 1);
	nugu_buffer_add(out_buf, data, 5);
	nugu_buffer_add(out_buf, ">", 1);

	return 0;
}

static struct nugu_decoder_driver_ops decoder_driver_ops = {
	.create = dummy_create,
	.decode = dummy_decode,
	.destroy = dummy_destroy
};

static void test_decoder_decode(void)
{
	NuguDecoderDriver *driver;
	NuguDecoder *dec;
	size_t result_length = 0;
	void *output;

	driver = nugu_decoder_driver_new("test", DECODER_TYPE_CUSTOM,
					 &decoder_driver_ops);
	g_assert(driver != NULL);

	dec = nugu_decoder_new(driver, NULL);
	g_assert(dec != NULL);
	g_assert(nugu_decoder_driver_free(driver) == -1);

	_check_put_data = 0;
	output = nugu_decoder_decode(dec, "hello", 5, &result_length);
	g_assert(output != NULL);
	g_assert(result_length != 0);
	g_assert(_check_put_data == 1);
	g_assert_cmpstr((char *)output, ==, "<hello>");
	free(output);

	nugu_decoder_free(dec);
	g_assert(nugu_decoder_driver_free(driver) == 0);
}

static struct nugu_decoder_driver_ops empty_ops = { .decode = NULL };

static void test_decoder_default(void)
{
	NuguDecoderDriver *driver;
	NuguDecoderDriver *driver2;
	NuguDecoder *dec;
	char *mydata = "test";
	size_t result_length = 999;

	g_assert(nugu_decoder_driver_new(NULL, DECODER_TYPE_CUSTOM, NULL) ==
		 NULL);
	g_assert(nugu_decoder_driver_new("test", DECODER_TYPE_CUSTOM, NULL) ==
		 NULL);
	g_assert(nugu_decoder_driver_new(NULL, DECODER_TYPE_CUSTOM,
					 &empty_ops) == NULL);
	g_assert(nugu_decoder_driver_register(NULL) < 0);
	g_assert(nugu_decoder_driver_remove(NULL) < 0);
	g_assert(nugu_decoder_driver_find(NULL) == NULL);
	g_assert(nugu_decoder_driver_find("") == NULL);

	driver = nugu_decoder_driver_new("test", DECODER_TYPE_CUSTOM,
					 &empty_ops);
	g_assert(driver != NULL);

	g_assert(nugu_decoder_driver_find("test") == NULL);
	g_assert(nugu_decoder_driver_register(driver) == 0);
	g_assert(nugu_decoder_driver_find("test") == driver);
	g_assert(nugu_decoder_driver_find_bytype(DECODER_TYPE_CUSTOM) ==
		 driver);
	g_assert(nugu_decoder_driver_find_bytype(DECODER_TYPE_OPUS) == NULL);
	g_assert(nugu_decoder_driver_remove(driver) == 0);
	g_assert(nugu_decoder_driver_find("test") == NULL);

	nugu_decoder_driver_free(driver);

	driver = nugu_decoder_driver_new("test1", DECODER_TYPE_CUSTOM,
					 &empty_ops);
	g_assert(driver != NULL);
	g_assert(nugu_decoder_driver_register(driver) == 0);

	driver2 = nugu_decoder_driver_new("test2", DECODER_TYPE_CUSTOM,
					  &empty_ops);
	g_assert(driver2 != NULL);
	g_assert(nugu_decoder_driver_register(driver2) == 0);

	g_assert(nugu_decoder_driver_find("test1") == driver);
	g_assert(nugu_decoder_driver_find("test2") == driver2);

	g_assert(nugu_decoder_driver_remove(driver) == 0);
	g_assert(nugu_decoder_driver_remove(driver2) == 0);

	g_assert(nugu_decoder_new(NULL, NULL) == NULL);
	g_assert(nugu_decoder_decode(NULL, NULL, 0, NULL) == NULL);
	g_assert(nugu_decoder_decode(NULL, "", 0, NULL) == NULL);
	g_assert(nugu_decoder_set_userdata(NULL, NULL) < 0);
	g_assert(nugu_decoder_get_userdata(NULL) == NULL);

	dec = nugu_decoder_new(driver, NULL);
	g_assert(dec != NULL);
	g_assert(nugu_decoder_driver_free(driver) == -1);

	g_assert(nugu_decoder_get_pcm(dec) == NULL);
	g_assert(nugu_decoder_set_userdata(dec, mydata) == 0);
	g_assert(nugu_decoder_get_userdata(dec) == mydata);

	g_assert(nugu_decoder_decode(dec, NULL, 0, &result_length) == NULL);
	g_assert(result_length == 999);
	g_assert(nugu_decoder_decode(dec, "test", 4, &result_length) == NULL);
	g_assert(result_length == 999);

	nugu_decoder_free(dec);

	g_assert(nugu_decoder_driver_free(driver2) == 0);
	g_assert(nugu_decoder_driver_free(driver) == 0);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	g_test_add_func("/decoder/driver_default", test_decoder_default);
	g_test_add_func("/decoder/decode", test_decoder_decode);

	return g_test_run();
}
