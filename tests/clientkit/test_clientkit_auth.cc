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
#include "clientkit/nugu_auth.hh"

#define ENV_AUTH_SERVER "AUTH_SERVER"
#define ENV_CLIENT_ID "CLIENT_ID"
#define ENV_CLIENT_SECRET "CLIENT_SECRET"
#define ENV_REDIRECT_URI "REDIRECT_URI"
#define ENV_AUTHORIZATION_CODE "AUTHORIZATION_CODE"

#define ENV_NUGU_TOKEN "TOKEN"
#define ENV_NUGU_REFRESH_TOKEN "REFRESH_TOKEN"

#define SECS_1YEAR (3600 * 24 * 365)
#define SECS_10MIN (60 * 30)

using namespace NuguClientKit;

static NuguAuth* auth;

static void test_nugu_auth_default()
{
    g_assert(auth != nullptr);
    g_assert(auth->isSupport(GrantType::AUTHORIZATION_CODE) == false);
    g_assert(auth->isSupport(GrantType::CLIENT_CREDENTIALS) == false);
    g_assert(auth->isSupport(GrantType::REFRESH_TOKEN) == false);
    g_assert(auth->isSupport(GrantType::DEVICE_CODE) == false);
    g_assert(auth->generateAuthorizeUrl("").size() == 0);
    g_assert(auth->getAuthorizationCodeToken("", "") == nullptr);
    g_assert(auth->getClientCredentialsToken("") == nullptr);
    g_assert(auth->parseAccessToken(NULL) == false);
    g_assert(auth->refresh(NULL) == false);
    g_assert(auth->isExpired(NULL) == true);
    g_assert(auth->getRemainExpireTime(NULL) == 0);
    g_assert(auth->getGatewayRegistryUri().size() == 0);
    g_assert(auth->getTemplateServerUri().size() == 0);

    NuguDeviceConfig config;
    NuguAuth* tmp_auth = new NuguAuth(config);
    g_assert(tmp_auth != nullptr);
    g_assert(tmp_auth->discovery() == false);
    g_assert(tmp_auth->generateAuthorizeUrl("1234").size() == 0);
    g_assert(tmp_auth->getAuthorizationCodeToken("xxxx", "1234") == nullptr);
    g_assert(tmp_auth->getClientCredentialsToken("1234") == nullptr);
    delete tmp_auth;
}

static void test_nugu_auth_discovery()
{
    g_assert(auth->discovery() == true);
    g_assert(auth->getGatewayRegistryUri().size() != 0);
    g_assert(auth->getTemplateServerUri().size() != 0);
}

static void test_nugu_auth_url()
{
    if (auth->isSupport(GrantType::AUTHORIZATION_CODE) == false)
        return;

    std::string url = auth->generateAuthorizeUrl("1234");
    g_assert(url.size() > 0);
}

static void test_nugu_auth_code()
{
    if (auth->isSupport(GrantType::AUTHORIZATION_CODE) == false)
        return;

    char* code = getenv(ENV_AUTHORIZATION_CODE);
    if (!code)
        return;

    NuguToken* token = auth->getAuthorizationCodeToken(code, "1234");
    g_assert(token != nullptr);

    g_assert(auth->getRemainExpireTime(token) > 0);
    g_assert(auth->isExpired(token) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_10MIN) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_1YEAR) == true);

    delete token;
}

static void test_nugu_auth_refresh()
{
    if (getenv(ENV_NUGU_TOKEN) == NULL || getenv(ENV_NUGU_REFRESH_TOKEN) == NULL)
        return;

    if (auth->isSupport(GrantType::REFRESH_TOKEN) == false)
        return;

    NuguToken* token = new NuguToken();
    g_assert(token != nullptr);

    token->access_token = getenv(ENV_NUGU_TOKEN);
    token->refresh_token = getenv(ENV_NUGU_REFRESH_TOKEN);
    token->ext_srl = "1234";

    g_assert(auth->parseAccessToken(token) == true);
    g_assert(auth->refresh(token) == true);

    g_assert_cmpstr(getenv(ENV_NUGU_TOKEN), !=, token->access_token.c_str());

    g_assert(auth->getRemainExpireTime(token) > 0);
    g_assert(auth->isExpired(token) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_10MIN) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_1YEAR) == true);

    delete token;
}

static void test_nugu_client_cred()
{
    /* Skip the test if the PoC is support AuthorizationCode grant type */
    if (auth->isSupport(GrantType::AUTHORIZATION_CODE) == true)
        return;

    if (auth->isSupport(GrantType::CLIENT_CREDENTIALS) == false)
        return;

    NuguToken* token = auth->getClientCredentialsToken("1234");
    g_assert(token != nullptr);

    g_assert(auth->getRemainExpireTime(token) > 0);
    g_assert(auth->isExpired(token) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_10MIN) == false);
    g_assert(auth->isExpired(token, time(NULL) + SECS_1YEAR) == true);

    delete token;
}

static void test_nugu_auth_finalize()
{
    if (auth)
        delete auth;
}

int main(int argc, char* argv[])
{
    NuguDeviceConfig config;

#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    if (getenv(ENV_AUTH_SERVER) != NULL)
        config.oauth_server_url = getenv(ENV_AUTH_SERVER);
    else
        config.oauth_server_url = "https://api.sktnugu.com";

    if (getenv(ENV_CLIENT_ID) != NULL)
        config.oauth_client_id = getenv(ENV_CLIENT_ID);

    if (getenv(ENV_CLIENT_SECRET) != NULL)
        config.oauth_client_secret = getenv(ENV_CLIENT_SECRET);

    if (getenv(ENV_REDIRECT_URI) != NULL)
        config.oauth_redirect_uri = getenv(ENV_REDIRECT_URI);

    auth = new NuguAuth(config);
    g_test_add_func("/auth/default", test_nugu_auth_default);

    if (config.oauth_client_id.size() > 0) {
        g_test_add_func("/auth/discovery", test_nugu_auth_discovery);

        if (config.oauth_client_secret.size() > 0) {
            if (config.oauth_redirect_uri.size() > 0) {
                g_test_add_func("/auth/auth_url", test_nugu_auth_url);
                g_test_add_func("/auth/auth_code", test_nugu_auth_code);
            }

            g_test_add_func("/auth/refresh", test_nugu_auth_refresh);
            g_test_add_func("/auth/client_cred", test_nugu_client_cred);
        }

        g_test_add_func("/auth/finalize", test_nugu_auth_finalize);
    }

    return g_test_run();
}
