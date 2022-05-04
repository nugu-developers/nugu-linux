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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>

#include "base/nugu_http.h"

static const char *SERVER;

static int on_not_reached(NuguHttpRequest *req, const NuguHttpResponse *resp,
			  void *user_data)
{
	g_assert_not_reached();

	return 1;
}

static int on_invalid(NuguHttpRequest *req, const NuguHttpResponse *resp,
		      void *user_data)
{
	g_assert(req != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == -1);

	/* auto free the NuguHttpRequest */
	return 1;
}

static int on_timeout(NuguHttpRequest *req, const NuguHttpResponse *resp,
		      void *user_data)
{
	g_assert(req != NULL);
	g_assert(user_data != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == -1);

	g_main_loop_quit(user_data);

	return 1;
}

/* Response code: 200 (OK) */
static int on_200(NuguHttpRequest *req, const NuguHttpResponse *resp,
		  void *user_data)
{
	NuguHttpResponse *tmp;

	g_assert(req != NULL);
	g_assert(user_data != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == 200);
	g_assert(resp->header_len > 0);
	g_assert(resp->body_len > 0);

	tmp = nugu_http_response_dup(resp);
	g_assert(tmp != NULL);

	g_assert(tmp->code == resp->code);
	g_assert(tmp->body_len == resp->body_len);
	g_assert(tmp->header_len == resp->header_len);
	g_assert_cmpstr(tmp->body, ==, resp->body);
	g_assert_cmpstr(tmp->header, ==, resp->header);

	nugu_http_response_free(tmp);

	g_main_loop_quit(user_data);

	return 0;
}

/* Response code: 201 (Created) */
static int on_201(NuguHttpRequest *req, const NuguHttpResponse *resp,
		  void *user_data)
{
	g_assert(req != NULL);
	g_assert(user_data != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == 201);
	g_assert(resp->header_len > 0);
	g_assert(resp->body_len > 0);

	g_main_loop_quit(user_data);

	return 0;
}

/* Response code: 404 (Not Found) */
static int on_404(NuguHttpRequest *req, const NuguHttpResponse *resp,
		  void *user_data)
{
	g_assert(req != NULL);
	g_assert(user_data != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == 404);

	g_main_loop_quit(user_data);

	return 0;
}

/* Response code: 204 (No content) */
static int on_204(NuguHttpRequest *req, const NuguHttpResponse *resp,
		  void *user_data)
{
	g_assert(req != NULL);
	g_assert(user_data != NULL);
	g_assert(resp != NULL);
	g_assert(resp->code == 204);
	g_assert(resp->header_len > 0);

	g_main_loop_quit(user_data);

	return 0;
}

static void test_nugu_http_default(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	NuguHttpHeader *header;
	NuguHttpHeader *header2;
	const NuguHttpResponse *resp;

	g_assert(nugu_http_host_new(NULL) == NULL);
	g_assert(nugu_http_get(NULL, NULL, NULL, NULL, NULL) == NULL);
	g_assert(nugu_http_get_sync(NULL, NULL, NULL) == NULL);
	g_assert(nugu_http_post(NULL, NULL, NULL, NULL, 0, NULL, NULL) == NULL);
	g_assert(nugu_http_post_sync(NULL, NULL, NULL, NULL, 0) == NULL);

	header = nugu_http_header_new();
	g_assert(header != NULL);
	g_assert(nugu_http_header_find(header, "asdf") == NULL);

	/* header add */
	g_assert(nugu_http_header_add(header, "asdf", "1234") == 0);
	g_assert_cmpstr(nugu_http_header_find(header, "asdf"), ==, "1234");
	g_assert(nugu_http_header_add(header, "qwer", "5678") == 0);
	g_assert_cmpstr(nugu_http_header_find(header, "qwer"), ==, "5678");

	/* header replace */
	g_assert(nugu_http_header_add(header, "asdf", "0000") == 0);
	g_assert_cmpstr(nugu_http_header_find(header, "asdf"), ==, "0000");

	/* header remove */
	g_assert(nugu_http_header_remove(header, "none") == -1);
	g_assert(nugu_http_header_remove(header, "asdf") == 0);
	g_assert(nugu_http_header_find(header, "asdf") == NULL);
	nugu_http_header_free(header);

	/* header dup */
	header = nugu_http_header_new();
	g_assert(header != NULL);
	g_assert(nugu_http_header_add(header, "asdf", "1234") == 0);
	g_assert_cmpstr(nugu_http_header_find(header, "asdf"), ==, "1234");

	header2 = nugu_http_header_dup(header);
	g_assert(header2 != NULL);
	g_assert_cmpstr(nugu_http_header_find(header2, "asdf"), ==, "1234");

	/* header import */
	g_assert(nugu_http_header_add(header2, "qwer", "5678") == 0);
	g_assert_cmpstr(nugu_http_header_find(header2, "qwer"), ==, "5678");
	g_assert(nugu_http_header_import(header, header2) == 0);
	nugu_http_header_free(header2);

	g_assert_cmpstr(nugu_http_header_find(header, "asdf"), ==, "1234");
	g_assert_cmpstr(nugu_http_header_find(header, "qwer"), ==, "5678");
	nugu_http_header_free(header);

	host = nugu_http_host_new("https://localhost:12345");
	g_assert(host != NULL);

	/* callback is mandatory for async api */
	g_assert(nugu_http_get(host, NULL, NULL, NULL, NULL) == NULL);
	g_assert(nugu_http_post(host, NULL, NULL, NULL, 0, NULL, NULL) == NULL);

	/* invalid url with callback */
	g_assert(nugu_http_get(host, "/", NULL, on_invalid, NULL) != NULL);
	g_assert(nugu_http_post(host, "/", NULL, "1234", 4, on_invalid, NULL) !=
		 NULL);

	/* invalid url with sync call */
	req = nugu_http_get_sync(host, "/", NULL);
	g_assert(req != NULL);
	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == -1);
	nugu_http_request_free(req);

	req = nugu_http_post_sync(host, "/", NULL, "1234", 4);
	g_assert(req != NULL);
	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == -1);
	nugu_http_request_free(req);

	nugu_http_host_free(host);
}

static void test_nugu_http_timeout(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	const NuguHttpResponse *resp;
	GMainLoop *loop;

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	/* set timeout to 200ms secs */
	nugu_http_host_set_timeout(host, 200);

	/* sync */
	req = nugu_http_get_sync(host, "/get_delay2", NULL);
	g_assert(req != NULL);

	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == -1);
	g_assert(resp->header_len == 0);
	g_assert(resp->body_len == 0);

	nugu_http_request_free(req);

	/* async */
	loop = g_main_loop_new(NULL, FALSE);

	req = nugu_http_get(host, "/get_delay2", NULL, on_timeout, loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_host_free(host);
}

static gboolean free_before_invoke(gpointer user_data)
{
	nugu_http_request_free(user_data);

	return FALSE;
}

static gboolean quit_loop(gpointer user_data)
{
	g_main_loop_quit(user_data);

	return FALSE;
}

static void test_nugu_http_invalid_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	/* set timeout to 1 secs */
	nugu_http_host_set_timeout(host, 1000);

	/* async request and free direct */
	req = nugu_http_get(host, "/get_delay2", NULL, on_not_reached, NULL);
	g_assert(req != NULL);
	nugu_http_request_free(req);

	/* async request and free after 100ms */
	loop = g_main_loop_new(NULL, FALSE);
	req = nugu_http_get(host, "/get_delay2", NULL, on_not_reached, NULL);
	g_assert(req != NULL);

	g_timeout_add(100, free_before_invoke, req);
	g_timeout_add(200, quit_loop, loop);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_host_free(host);
}

static void test_nugu_http_get_sync(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	const NuguHttpResponse *resp;

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_get_sync(host, "/get", NULL);
	g_assert(req != NULL);

	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == 200);
	g_assert(resp->header_len > 0);
	g_assert(resp->body_len > 0);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_get_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	NuguHttpHeader *header;
	GMainLoop *loop;

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	/* normal request */
	req = nugu_http_get(host, "/get", NULL, on_200, loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);

	nugu_http_request_free(req);

	/* test with custom header */
	header = nugu_http_header_new();
	nugu_http_header_add(header, "X-test", "0");
	nugu_http_header_add(header, "Y-test", "0");

	req = nugu_http_get(host, "/get", header, on_200, loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);

	nugu_http_header_free(header);
	nugu_http_request_free(req);

	/* test with 404. '/api/users/23' will return 404 error */
	req = nugu_http_get(host, "/get_invalid", NULL, on_404, loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_request_free(req);

	nugu_http_host_free(host);
}

static void test_nugu_http_post_sync(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	const NuguHttpResponse *resp;
	const char *data = "{ \"name\": \"mike\" }";

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_post_sync(host, "/post", NULL, data, strlen(data));
	g_assert(req != NULL);

	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == 201);
	g_assert(resp->header_len > 0);
	g_assert(resp->body_len > 0);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_post_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;
	const char *data = "{ \"name\": \"mike\" }";

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_post(host, "/post", NULL, data, strlen(data), on_201,
			     loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_put_sync(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	const NuguHttpResponse *resp;
	const char *data = "{ \"name\": \"test\" }";

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_put_sync(host, "/put", NULL, data, strlen(data));
	g_assert(req != NULL);

	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == 200);
	g_assert(resp->header_len > 0);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_put_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;
	const char *data = "{ \"name\": \"test\" }";

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_put(host, "/put", NULL, data, strlen(data), on_200,
			    loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_delete_sync(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	const NuguHttpResponse *resp;

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_delete_sync(host, "/delete", NULL);
	g_assert(req != NULL);

	resp = nugu_http_request_response_get(req);
	g_assert(resp != NULL);
	g_assert(resp->code == 204);
	g_assert(resp->header_len > 0);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static void test_nugu_http_delete_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	req = nugu_http_delete(host, "/delete", NULL, on_204, loop);
	g_assert(req != NULL);

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_request_free(req);
	nugu_http_host_free(host);
}

static int _exit_count;
static int _request_count;

static int on_resp(NuguHttpRequest *req, const NuguHttpResponse *resp,
		   void *user_data)
{
	g_assert(req != NULL);
	g_assert(resp != NULL);
	g_assert(user_data != NULL);

	g_assert(resp->code != -1);
	g_assert(resp->header_len > 0);

	_exit_count++;

	if (_exit_count >= _request_count)
		g_main_loop_quit(user_data);

	return 1;
}

static void test_nugu_http_multiple_async(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;

	_exit_count = 0;
	_request_count = 0;

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(SERVER);
	g_assert(host != NULL);

	_exit_count = 0;
	_request_count = 0;

	req = nugu_http_get(host, "/get", NULL, on_resp, loop);
	g_assert(req != NULL);
	_request_count++;

	req = nugu_http_get(host, "/get", NULL, on_resp, loop);
	g_assert(req != NULL);
	_request_count++;

	req = nugu_http_get(host, "/get", NULL, on_resp, loop);
	g_assert(req != NULL);
	_request_count++;

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	g_assert(_exit_count == _request_count);

	nugu_http_host_free(host);
}

#define TEST_DOWNLOAD_HTTP_HOST "https://raw.githubusercontent.com"
#define TEST_DOWNLOAD_HTTP_PATH "/nugu-developers/nugu-linux/master/README.md"
#define TEST_DOWNLOAD_LOCAL_PATH_FAIL "/tmp/nugu_http_download_fail.dat"
#define TEST_DOWNLOAD_LOCAL_PATH_OK "/tmp/nugu_http_download_ok.dat"

static int on_download_fail(NuguHttpRequest *req, const NuguHttpResponse *resp,
			    void *user_data)
{
	struct stat statbuf;

	g_assert(req != NULL);
	g_assert(resp != NULL);
	g_assert(user_data != NULL);

	_exit_count++;

	if (_exit_count >= _request_count)
		g_main_loop_quit(user_data);

	g_assert(resp->code != 200);
	g_assert(stat(TEST_DOWNLOAD_LOCAL_PATH_FAIL, &statbuf) == 0);
	g_assert(statbuf.st_size > 0);
	unlink(TEST_DOWNLOAD_LOCAL_PATH_FAIL);

	return 1;
}

static int on_download_done(NuguHttpRequest *req, const NuguHttpResponse *resp,
			    void *user_data)
{
	struct stat statbuf;

	g_assert(req != NULL);
	g_assert(resp != NULL);
	g_assert(user_data != NULL);

	_exit_count++;

	if (_exit_count >= _request_count)
		g_main_loop_quit(user_data);

	g_assert(stat(TEST_DOWNLOAD_LOCAL_PATH_OK, &statbuf) == 0);
	g_assert(statbuf.st_size > 0);
	unlink(TEST_DOWNLOAD_LOCAL_PATH_OK);

	return 1;
}

static void on_download_progress(NuguHttpRequest *req,
				 const NuguHttpResponse *resp,
				 size_t downloaded, size_t total,
				 void *user_data)
{
	g_assert(req != NULL);
	g_assert(resp != NULL);
	g_assert(user_data != NULL);

	g_assert(downloaded <= total);
}

static void test_nugu_http_download(void)
{
	NuguHttpHost *host;
	NuguHttpRequest *req;
	GMainLoop *loop;

	_exit_count = 0;
	_request_count = 0;

	loop = g_main_loop_new(NULL, FALSE);

	host = nugu_http_host_new(TEST_DOWNLOAD_HTTP_HOST);
	g_assert(host != NULL);

	req = nugu_http_download(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	g_assert(req == NULL);

	req = nugu_http_download(host, NULL, NULL, NULL, NULL, NULL, NULL);
	g_assert(req == NULL);

	req = nugu_http_download(host, TEST_DOWNLOAD_HTTP_PATH, NULL, NULL,
				 NULL, NULL, NULL);
	g_assert(req == NULL);

	req = nugu_http_download(host, TEST_DOWNLOAD_HTTP_PATH,
				 TEST_DOWNLOAD_LOCAL_PATH_OK, NULL, NULL, NULL,
				 NULL);
	g_assert(req == NULL);

	req = nugu_http_download(host, TEST_DOWNLOAD_HTTP_PATH, "/a/b/c/d",
				 NULL, on_download_done, NULL, loop);
	g_assert(req == NULL);

	req = nugu_http_download(host, "/_____", TEST_DOWNLOAD_LOCAL_PATH_FAIL,
				 NULL, on_download_fail, NULL, loop);
	g_assert(req != NULL);
	_request_count++;

	req = nugu_http_download(host, TEST_DOWNLOAD_HTTP_PATH,
				 TEST_DOWNLOAD_LOCAL_PATH_OK, NULL,
				 on_download_done, on_download_progress, loop);
	g_assert(req != NULL);
	_request_count++;

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	nugu_http_host_free(host);
}

int main(int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
#endif

	g_test_init(&argc, &argv, NULL);
	g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

	/*
	 * If the "HTTP_TEST_SERVER" environment value is not set,
	 * the HTTP test is skipped.
	 *
	 * e.g.
	 *     node test_server.js &
	 *     export HTTP_TEST_SERVER=http://localhost:3000
	 */
	if (getenv("HTTP_TEST_SERVER") == NULL)
		return g_test_run();

	SERVER = getenv("HTTP_TEST_SERVER");

	nugu_http_init();

	g_test_add_func("/http/default", test_nugu_http_default);
	g_test_add_func("/http/timeout", test_nugu_http_timeout);
	g_test_add_func("/http/invalid_async", test_nugu_http_invalid_async);
	g_test_add_func("/http/get_sync", test_nugu_http_get_sync);
	g_test_add_func("/http/get_async", test_nugu_http_get_async);
	g_test_add_func("/http/post_sync", test_nugu_http_post_sync);
	g_test_add_func("/http/post_async", test_nugu_http_post_async);
	g_test_add_func("/http/put_sync", test_nugu_http_put_sync);
	g_test_add_func("/http/put_async", test_nugu_http_put_async);
	g_test_add_func("/http/delete_sync", test_nugu_http_delete_sync);
	g_test_add_func("/http/delete_async", test_nugu_http_delete_async);
	g_test_add_func("/http/multiple_async", test_nugu_http_multiple_async);
	g_test_add_func("/http/download", test_nugu_http_download);

	return g_test_run();
}
