/*
 * Copyright (c) 2022 SK Telecom Co., Ltd. All rights reserved.
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

#include <glib.h>

#include "base/nugu_log.h"
#include "clientkit/nugu_http_rest.hh"

using namespace NuguClientKit;

static const char* SERVER;

static void test_nugu_http_rest_get()
{
    NuguHttpRest myserver(SERVER);
    NuguHttpResponse* resp;

    myserver.setTimeout(1 * 1000);
    myserver.setConnectionTimeout(500);
    myserver.addHeader("common-key", "common-value");

    /* Sync request */
    resp = myserver.get("/get");
    g_assert(resp != NULL);
    g_assert(resp->code == 200);
    g_assert(resp->header_len > 0);
    g_assert(resp->body_len > 0);
    g_assert_cmpstr((char*)resp->body, ==, "Ok");
    nugu_http_response_free(resp);

    /* Sync request with additional header */
    NuguHttpHeader* extra_header = nugu_http_header_new();
    nugu_http_header_add(extra_header, "extra-key", "extra-value");

    resp = myserver.get("/get", extra_header);
    g_assert(resp != NULL);
    g_assert(resp->code == 200);
    g_assert(resp->header_len > 0);
    g_assert(resp->body_len > 0);
    g_assert_cmpstr((char*)resp->body, ==, "Ok");
    nugu_http_response_free(resp);
    nugu_http_header_free(extra_header);

    /* Async request */
    GMainLoop* loop;
    loop = g_main_loop_new(NULL, FALSE);
    int check_value = 0;

    myserver.get("/get", [&](const NuguHttpResponse* resp) {
        g_assert(resp != NULL);
        g_assert(resp->code == 200);
        g_assert(resp->header_len > 0);
        g_assert(resp->body_len > 0);
        g_assert_cmpstr((char*)resp->body, ==, "Ok");

        check_value = 1;

        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(check_value == 1);

    /* Async request with additional header */
    loop = g_main_loop_new(NULL, FALSE);
    check_value = 0;

    extra_header = nugu_http_header_new();
    nugu_http_header_add(extra_header, "extra-key", "extra-value");

    myserver.get("/get", extra_header, [&](const NuguHttpResponse* resp) {
        g_assert(resp != NULL);
        g_assert(resp->code == 200);
        g_assert(resp->header_len > 0);
        g_assert(resp->body_len > 0);
        g_assert_cmpstr((char*)resp->body, ==, "Ok");

        check_value = 1;

        g_main_loop_quit(loop);
    });

    nugu_http_header_free(extra_header);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(check_value == 1);
}

static void test_nugu_http_rest_post()
{
    NuguHttpRest myserver(SERVER);

    /* Sync request */
    NuguHttpResponse* resp = myserver.post("/post", "{'key':'value'}");
    g_assert(resp != NULL);
    g_assert(resp->code == 201);
    g_assert(resp->header_len > 0);
    g_assert(resp->body_len > 0);
    g_assert_cmpstr((char*)resp->body, ==, "added");
    nugu_http_response_free(resp);

    /* Async request */
    GMainLoop* loop;
    loop = g_main_loop_new(NULL, FALSE);
    int check_value = 0;

    myserver.post("/post", "{'key':'value'}", [&](const NuguHttpResponse* resp) {
        g_assert(resp != NULL);
        g_assert(resp->code == 201);
        g_assert(resp->header_len > 0);
        g_assert(resp->body_len > 0);
        g_assert_cmpstr((char*)resp->body, ==, "added");

        check_value = 1;

        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(check_value == 1);
}

static void test_nugu_http_rest_put()
{
    NuguHttpRest myserver(SERVER);

    /* Sync request */
    NuguHttpResponse* resp = myserver.put("/put", "{'key':'value'}");
    g_assert(resp != NULL);
    g_assert(resp->code == 200);
    g_assert(resp->header_len > 0);
    g_assert(resp->body_len > 0);
    g_assert_cmpstr((char*)resp->body, ==, "updated");
    nugu_http_response_free(resp);

    /* Async request */
    GMainLoop* loop;
    loop = g_main_loop_new(NULL, FALSE);
    int check_value = 0;

    myserver.put("/put", "{'key':'value'}", [&](const NuguHttpResponse* resp) {
        g_assert(resp != NULL);
        g_assert(resp->code == 200);
        g_assert(resp->header_len > 0);
        g_assert(resp->body_len > 0);
        g_assert_cmpstr((char*)resp->body, ==, "updated");

        check_value = 1;

        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(check_value == 1);
}

static void test_nugu_http_rest_del()
{
    NuguHttpRest myserver(SERVER);
    NuguHttpResponse* resp;

    myserver.setTimeout(1 * 1000);
    myserver.setConnectionTimeout(500);
    myserver.addHeader("common-key", "common-value");

    /* Sync request */
    resp = myserver.del("/delete");
    g_assert(resp != NULL);
    g_assert(resp->code == 204);
    g_assert(resp->header_len > 0);
    g_assert(resp->body_len == 0);
    nugu_http_response_free(resp);

    /* Async request */
    GMainLoop* loop;
    loop = g_main_loop_new(NULL, FALSE);
    int check_value = 0;

    myserver.del("/delete", [&](const NuguHttpResponse* resp) {
        g_assert(resp != NULL);
        g_assert(resp->code == 204);
        g_assert(resp->header_len > 0);
        g_assert(resp->body_len == 0);

        check_value = 1;

        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(check_value == 1);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
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
        return 0;

    SERVER = getenv("HTTP_TEST_SERVER");

    g_test_add_func("/http_rest/get", test_nugu_http_rest_get);
    g_test_add_func("/http_rest/post", test_nugu_http_rest_post);
    g_test_add_func("/http_rest/put", test_nugu_http_rest_put);
    g_test_add_func("/http_rest/del", test_nugu_http_rest_del);

    return g_test_run();
}
