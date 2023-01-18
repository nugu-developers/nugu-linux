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
#include <json/json.h>

#include "base/nugu_log.h"

#include "clientkit/nugu_auth.hh"

#define EP_DISCOVERY "/.well-known/oauth-authorization-server/"

namespace NuguClientKit {

static char* base64_decode(const char* orig)
{
    /* add optional padding('==') for base64 decoding */
    gchar* tmp = g_strdup_printf("%s==", orig);
    if (!tmp) {
        nugu_error_nomem();
        return NULL;
    }

    gsize decoded_len = 0;
    gchar* decoded = (gchar*)g_base64_decode(tmp, &decoded_len);
    g_free(tmp);

    return decoded;
}

unsigned int find_grant_types(const Json::Value& grant_types)
{
    unsigned int supported_grant_types = 0;

    if (grant_types.isArray()) {
        Json::ArrayIndex type_len = grant_types.size();
        for (Json::ArrayIndex i = 0; i < type_len; ++i) {
            nugu_dbg("grant_types_supported[%d] = %s", i, grant_types[i].asCString());
            if (grant_types[i].asString() == "authorization_code") {
                supported_grant_types |= GrantType::AUTHORIZATION_CODE;
            } else if (grant_types[i].asString() == "client_credentials") {
                supported_grant_types |= GrantType::CLIENT_CREDENTIALS;
            } else if (grant_types[i].asString() == "refresh_token") {
                supported_grant_types |= GrantType::REFRESH_TOKEN;
            } else if (grant_types[i].asString() == "device_code") {
                supported_grant_types |= GrantType::DEVICE_CODE;
            }
        }
        nugu_dbg("supported_grant_types = 0x%X", supported_grant_types);
    }

    return supported_grant_types;
}

NuguAuth::NuguAuth(const NuguDeviceConfig& config)
    : config(config)
    , supported_grant_types(0)
{
    rest = new NuguHttpRest(config.oauth_server_url);
    rest->addHeader("Content-Type", "application/x-www-form-urlencoded");
}

NuguAuth::~NuguAuth()
{
    if (rest)
        delete rest;
}

bool NuguAuth::discovery(const std::function<void(bool success)> &cb)
{
    if (!rest)
        return false;

    if (config.oauth_client_id.size() == 0) {
        nugu_error("oauth_client_id is empty");
        return false;
    }

    return rest->get(EP_DISCOVERY + config.oauth_client_id, [&, cb](const NuguHttpResponse* resp) {
        if (resp->code != 200) {
            nugu_error("invalid response code %d", resp->code);
            cb(false);
            return;
        }

        if (resp->body == NULL) {
            nugu_error("response body is NULL");
            cb(false);
            return;
        }

        nugu_info("%s", (char*)resp->body);

        Json::Value root;
        Json::Reader reader;
        std::string message((char*)resp->body);

        if (!reader.parse(message, root)) {
            nugu_error("JSON parsing error: %s", message.c_str());
            cb(false);
            return;
        }

        if (root["token_endpoint"].empty()) {
            nugu_error("can't find token_endpoint from response");
            cb(false);
            return;
        }

        size_t url_len = config.oauth_server_url.size();

        ep_token = root["token_endpoint"].asString();
        ep_token.erase(0, url_len);

        ep_authorization = root["authorization_endpoint"].asString();
        ep_authorization.erase(0, url_len);

        uri_gateway_registry = root["device_gateway_registry_uri"].asString();
        uri_template_server = root["template_server_uri"].asString();

        supported_grant_types = find_grant_types(root["grant_types_supported"]);

        cb(true);
    });
}

bool NuguAuth::discovery()
{
    if (!rest)
        return false;

    if (config.oauth_client_id.size() == 0) {
        nugu_error("oauth_client_id is empty");
        return false;
    }

    NuguHttpResponse* resp = rest->get(EP_DISCOVERY + config.oauth_client_id);
    if (!resp) {
        nugu_error("GET request failed");
        return false;
    }

    if (resp->code != 200) {
        nugu_error("invalid response code %d", resp->code);
        return false;
    }

    if (resp->body == NULL) {
        nugu_error("response body is NULL");
        return false;
    }

    nugu_info("%s", (char*)resp->body);

    std::string message((char*)resp->body);
    nugu_http_response_free(resp);

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("JSON parsing error: %s", message.c_str());
        return false;
    }

    if (root["token_endpoint"].empty()) {
        nugu_error("can't find token_endpoint from response");
        return false;
    }

    size_t url_len = config.oauth_server_url.size();

    ep_token = root["token_endpoint"].asString();
    ep_token.erase(0, url_len);

    ep_authorization = root["authorization_endpoint"].asString();
    ep_authorization.erase(0, url_len);

    uri_gateway_registry = root["device_gateway_registry_uri"].asString();
    uri_template_server = root["template_server_uri"].asString();

    supported_grant_types = find_grant_types(root["grant_types_supported"]);

    return true;
}

bool NuguAuth::isSupport(const GrantType& gtype)
{
    if (supported_grant_types & gtype)
        return true;

    return false;
}

std::string NuguAuth::generateAuthorizeUrl(const std::string& device_serial)
{
    if (config.oauth_client_id.size() == 0 || config.oauth_redirect_uri.size() == 0) {
        nugu_error("client info is empty");
        return "";
    }

    if (device_serial.size() == 0) {
        nugu_error("device_serial is empty");
        return "";
    }

    gchar* query = g_strdup_printf("?response_type=code&client_id=%s"
                                   "&redirect_uri=%s&data={\"deviceSerialNumber\":\"%s\"}",
        config.oauth_client_id.c_str(), config.oauth_redirect_uri.c_str(), device_serial.c_str());
    if (!query) {
        nugu_error_nomem();
        return "";
    }

    gchar* encoded = g_uri_escape_string(query, "?=&", true);
    if (!encoded) {
        nugu_error_nomem();
        g_free(query);
        return "";
    }

    std::string login_url = config.oauth_server_url + ep_authorization + encoded;
    nugu_dbg("authorize_url: %s", login_url.c_str());

    g_free(query);
    g_free(encoded);

    return login_url;
}

NuguToken* NuguAuth::getAuthorizationCodeToken(const std::string& code,
    const std::string& device_serial)
{
    if (config.oauth_client_id.size() == 0
        || config.oauth_client_secret.size() == 0
        || config.oauth_redirect_uri.size() == 0) {
        nugu_error("client info is empty");
        return nullptr;
    }

    if (code.size() == 0 || device_serial.size() == 0) {
        nugu_error("code or device_serial is empty");
        return nullptr;
    }

    gchar* query = g_strdup_printf("grant_type=authorization_code&code=%s"
                                   "&client_id=%s&client_secret=%s&redirect_uri=%s"
                                   "&data={\"deviceSerialNumber\":\"%s\"}",
        code.c_str(), config.oauth_client_id.c_str(), config.oauth_client_secret.c_str(),
        config.oauth_redirect_uri.c_str(), device_serial.c_str());
    if (!query) {
        nugu_error_nomem();
        return nullptr;
    }

    gchar* encoded = g_uri_escape_string(query, "?=&", true);
    if (!encoded) {
        nugu_error_nomem();
        g_free(query);
        return nullptr;
    }

    std::string body = encoded;

    g_free(query);
    g_free(encoded);

    NuguHttpResponse* resp = rest->post(ep_token, body);
    if (!resp) {
        nugu_error("POST request failed");
        return nullptr;
    }

    if (resp->code != 200) {
        nugu_error("error response code: %d", resp->code);
        if (resp->body_len > 0)
            nugu_error("error response body: %s", (char*)resp->body);

        nugu_http_response_free(resp);
        return nullptr;
    }

    if (resp->body == NULL) {
        nugu_error("response body is NULL");
        return nullptr;
    }

    nugu_info("response: %s", (char*)resp->body);

    std::string message((char*)resp->body);
    nugu_http_response_free(resp);

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("JSON parsing error: %s", message.c_str());
        return nullptr;
    }

    if (!root["access_token"].isString()) {
        nugu_error("can't find access_token from response");
        return nullptr;
    }

    NuguToken* token = new NuguToken();
    if (!token) {
        nugu_error_nomem();
        return nullptr;
    }

    token->access_token = root["access_token"].asString();
    token->timestamp = time(NULL);

    if (root["refresh_token"].isString())
        token->refresh_token = root["refresh_token"].asString();

    if (root["token_type"].isString())
        token->token_type = root["token_type"].asString();

    if (root["expires_in"].isInt())
        token->expires_in = root["expires_in"].asInt();
    else
        token->expires_in = 0;

    parseAccessToken(token);

    return token;
}

NuguToken* NuguAuth::getClientCredentialsToken(const std::string& device_serial)
{
    if (config.oauth_client_id.size() == 0 || config.oauth_client_secret.size() == 0) {
        nugu_error("client info is empty");
        return nullptr;
    }

    if (device_serial.size() == 0) {
        nugu_error("device_serial is empty");
        return nullptr;
    }

    gchar* query = g_strdup_printf("grant_type=client_credentials&client_id=%s"
                                   "&client_secret=%s&data={\"deviceSerialNumber\":\"%s\"}",
        config.oauth_client_id.c_str(), config.oauth_client_secret.c_str(), device_serial.c_str());
    if (!query) {
        nugu_error_nomem();
        return nullptr;
    }

    gchar* encoded = g_uri_escape_string(query, "?=&", true);
    if (!encoded) {
        nugu_error_nomem();
        g_free(query);
        return nullptr;
    }

    std::string body = encoded;

    g_free(query);
    g_free(encoded);

    NuguHttpResponse* resp = rest->post(ep_token, body);
    if (!resp) {
        nugu_error("POST request failed");
        return nullptr;
    }

    if (resp->code != 200) {
        nugu_error("error response code: %d", resp->code);
        if (resp->body_len > 0)
            nugu_error("error response body: %s", (char*)resp->body);

        nugu_http_response_free(resp);
        return nullptr;
    }

    if (resp->body == NULL) {
        nugu_error("response body is NULL");
        return nullptr;
    }

    nugu_info("response: %s", (char*)resp->body);

    std::string message((char*)resp->body);
    nugu_http_response_free(resp);

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("JSON parsing error: %s", message.c_str());
        return nullptr;
    }

    if (!root["access_token"].isString()) {
        nugu_error("can't find access_token from response");
        return nullptr;
    }

    NuguToken* token = new NuguToken();
    if (!token) {
        nugu_error_nomem();
        return nullptr;
    }

    token->access_token = root["access_token"].asString();
    token->timestamp = time(NULL);

    if (root["refresh_token"].isString())
        token->refresh_token = root["refresh_token"].asString();

    if (root["token_type"].isString())
        token->token_type = root["token_type"].asString();

    if (root["expires_in"].isInt())
        token->expires_in = root["expires_in"].asInt();
    else
        token->expires_in = 0;

    parseAccessToken(token);

    return token;
}

bool NuguAuth::parseAccessToken(NuguToken* token)
{
    if (!token)
        return false;

    if (token->access_token.size() == 0)
        return false;

    /* Parsing JWT */
    gchar** splitted_text;
    splitted_text = g_strsplit(token->access_token.c_str(), ".", -1);
    if (!splitted_text || !splitted_text[1]) {
        nugu_error("invalid access_token");
        return false;
    }

    gchar* decoded = base64_decode(splitted_text[1]);
    g_strfreev(splitted_text);

    if (!decoded) {
        nugu_error("base64 decoding failed");
        return false;
    }

    nugu_dbg("token: %s", decoded);

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(decoded, root)) {
        nugu_error("JSON parsing error: %s", decoded);
        g_free(decoded);
        return false;
    }

    g_free(decoded);

    if (root["exp"].isInt()) {
        token->exp = root["exp"].asInt();
        nugu_dbg("exp: %d", token->exp);
    } else {
        token->exp = 0;
    }

    if (root["client_id"].isString()) {
        token->client_id = root["client_id"].asString();
        nugu_dbg("client_id: %s", token->client_id.c_str());
    } else {
        token->client_id = "";
    }

    token->sid = false;
    token->scope.clear();

    Json::Value scope = root["scope"];
    if (scope.isArray()) {
        Json::ArrayIndex scope_len = scope.size();
        for (Json::ArrayIndex i = 0; i < scope_len; ++i) {
            nugu_dbg("scope[%d] = %s", i, scope[i].asCString());
            token->scope.push_back(scope[i].asCString());

            if (scope[i].asString() == "device:S.I.D.") {
                token->sid = true;
                break;
            }
        }
    }

    token->ext_usr = "";
    token->ext_dvc = "";
    token->ext_srl = "";
    token->ext_poc = "";

    if (root["ext"].isObject()) {
        Json::Value ext = root["ext"];

        if (ext["usr"].isString()) {
            token->ext_usr = ext["usr"].asString();
            nugu_dbg("ext[usr]: %s", token->ext_usr.c_str());
        }

        if (ext["usr"].isString()) {
            token->ext_dvc = ext["dvc"].asString();
            nugu_dbg("ext[dvc]: %s", token->ext_dvc.c_str());
        }

        if (ext["usr"].isString()) {
            token->ext_srl = ext["srl"].asString();
            nugu_dbg("ext[srl]: %s", token->ext_srl.c_str());
        }

        if (ext["usr"].isString()) {
            token->ext_poc = ext["poc"].asString();
            nugu_dbg("ext[poc]: %s", token->ext_poc.c_str());
        }
    }

    return true;
}

bool NuguAuth::refresh(NuguToken* token, const std::string& device_serial)
{
    std::string serial;

    if (!token)
        return false;

    if (config.oauth_client_id.size() == 0 || config.oauth_client_secret.size() == 0) {
        nugu_error("client info is empty");
        return false;
    }

    if (token->refresh_token.size() == 0) {
        nugu_error("refresh_token is empty");
        return false;
    }

    if (device_serial.size() == 0) {
        if (token->ext_srl.size() == 0) {
            nugu_error("can't find device serial info");
            return false;
        } else {
            serial = token->ext_srl;
        }
    } else {
        serial = device_serial;
    }

    gchar* query = g_strdup_printf("grant_type=refresh_token&refresh_token=%s&client_id=%s"
                                   "&client_secret=%s&data={\"deviceSerialNumber\":\"%s\"}",
        token->refresh_token.c_str(), config.oauth_client_id.c_str(),
        config.oauth_client_secret.c_str(), serial.c_str());
    if (!query) {
        nugu_error_nomem();
        return false;
    }

    gchar* encoded = g_uri_escape_string(query, "?=&", true);
    if (!encoded) {
        nugu_error_nomem();
        g_free(query);
        return false;
    }

    std::string body = encoded;

    g_free(query);
    g_free(encoded);

    NuguHttpResponse* resp = rest->post(ep_token, body);
    if (!resp) {
        nugu_error("POST request failed");
        return false;
    }

    if (resp->code != 200) {
        nugu_error("error response code: %d", resp->code);
        if (resp->body_len > 0)
            nugu_error("error response body: %s", (char*)resp->body);

        nugu_http_response_free(resp);
        return false;
    }

    if (resp->body == NULL) {
        nugu_error("response body is NULL");
        return false;
    }

    nugu_info("response: %s", (char*)resp->body);

    std::string message((char*)resp->body);
    nugu_http_response_free(resp);

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root)) {
        nugu_error("JSON parsing error: %s", message.c_str());
        return false;
    }

    if (!root["access_token"].isString()) {
        nugu_error("can't find access_token from response");
        return false;
    }

    token->access_token = root["access_token"].asString();
    token->timestamp = time(NULL);

    if (root["refresh_token"].isString())
        token->refresh_token = root["refresh_token"].asString();

    if (root["token_type"].isString())
        token->token_type = root["token_type"].asString();

    if (root["expires_in"].isInt())
        token->expires_in = root["expires_in"].asInt();
    else
        token->expires_in = 0;

    parseAccessToken(token);

    return true;
}

bool NuguAuth::isExpired(const NuguToken* token, time_t base_time)
{
    time_t remainTime = getRemainExpireTime(token, base_time);
    if (remainTime == 0)
        return true;

    return false;
}

time_t NuguAuth::getRemainExpireTime(const NuguToken* token, time_t base_time)
{
    time_t expire_time;

    if (!token)
        return 0;

    expire_time = token->exp;
    if (expire_time == 0) {
        if (token->timestamp == 0 || token->expires_in == 0) {
            nugu_error("Unable to check whether token has expired");
            return 0;
        }

        expire_time = token->timestamp + token->expires_in;
    }

    if (base_time == 0)
        base_time = time(NULL);

    if (expire_time >= base_time)
        return expire_time - base_time;

    return 0;
}

std::string NuguAuth::getGatewayRegistryUri()
{
    return uri_gateway_registry;
}

std::string NuguAuth::getTemplateServerUri()
{
    return uri_template_server;
}

} // NuguClientKit
