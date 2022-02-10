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

#ifndef __NUGU_AUTH_H__
#define __NUGU_AUTH_H__

#include <clientkit/nugu_http_rest.hh>
#include <string>
#include <vector>

namespace NuguClientKit {

/**
 * @file nugu_auth.hh
 * @defgroup NuguAuth NuguAuth
 * @ingroup SDKNuguClientKit
 * @brief Authentication management
 *
 * The NuguAuth handles OAuth2 authentication.
 *
 * @{
 */

/**
 * @brief GrantType
 */
enum GrantType {
    AUTHORIZATION_CODE = 0x1, /* OAuth2 Authorization code */
    CLIENT_CREDENTIALS = 0x2, /* OAuth2 Client credentials */
    REFRESH_TOKEN = 0x4, /* OAuth2 refresh token */
    DEVICE_CODE = 0x8 /* OAuth2 device code */
};

/**
 * @brief NuguDeviceConfig
 */
struct NuguDeviceConfig {
    std::string oauth_server_url; /* OAuth2 server url */
    std::string oauth_client_id; /* Client id */
    std::string oauth_client_secret; /* Client secret */
    std::string oauth_redirect_uri; /* Redirect uri */
    std::string device_type_code; /* Device type code */
    std::string poc_id; /* PoC id */
};

/**
 * @brief NuguToken
 */
struct NuguToken {
    std::string access_token; /* OAuth2 access token */
    std::string refresh_token; /* OAuth2 refresh token */
    std::string token_type; /* OAuth2 token type. e.g. "Bearer" */
    int expires_in; /* Expiration time remaining after token issuance (secs) */
    time_t timestamp; /* Token issuance time */

    std::vector<std::string> scope; /* scope */
    int exp; /* Token expiration time (unix timestamp) */
    std::string client_id; /* OAuth2 client id */
    std::string ext_usr; /* user id */
    std::string ext_dvc; /* device id */
    std::string ext_srl; /* device serial */
    std::string ext_poc; /* PoC id */

    bool sid; /* flag for server-initiative-directive support */
};

/**
 * @brief NuguAuth
 */
class NuguAuth {
public:
    explicit NuguAuth(const NuguDeviceConfig& config);
    ~NuguAuth();

    /**
     * @brief OAuth2 discovery to get OAuth2 end-point and server url
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool discovery();

    /**
     * @brief Check whether the requested grant type is supported for the client
     * @param[in] gtype grant type
     * @return result
     * @retval true supported
     * @retval false not supported
     */
    bool isSupport(const GrantType& gtype);

    /**
     * @brief Get OAuth2 authorization url
     * @param[in] device_serial device serial info
     * @return std::string authorization url
     */
    std::string generateAuthorizeUrl(const std::string& device_serial);

    /**
     * @brief Get the token using authorization code (token exchange)
     * @param[in] code OAuth2 authorization code
     * @param[in] device_serial device serial info
     * @return NuguToken* token information
     */
    NuguToken* getAuthorizationCodeToken(const std::string& code, const std::string& device_serial);

    /**
     * @brief Get the token using client credentials
     * @param[in] device_serial device serial info
     * @return NuguToken* token information
     */
    NuguToken* getClientCredentialsToken(const std::string& device_serial);

    /**
     * @brief Parsing the JWT access_token and fill the token information
     * @param[in,out] token token information
     * @return true parsing success
     * @return false parsing failed
     */
    bool parseAccessToken(NuguToken* token);

    /**
     * @brief Refresh the access_token and update the token information
     * @param[in,out] token token information
     * @param[in] device_serial device serial info
     * @return true success
     * @return false failure
     */
    bool refresh(NuguToken* token, const std::string& device_serial = "");

    /**
     * @brief Check the token is expired or not
     * @param token[in] token information
     * @param base_time[in] base timestamp(secs), '0' means the current timestamp
     * @return true token is expired
     * @return false token is not expired
     */
    bool isExpired(const NuguToken* token, time_t base_time = 0);

    /**
     * @brief Get remaining expiration time based on base_time
     * @param token[in] token information
     * @param base_time[in] base timestamp(secs), '0' means the current timestamp
     * @return time_t remain secs. '0' means the token is expired
     */
    time_t getRemainExpireTime(const NuguToken* token, time_t base_time = 0);

    /**
     * @brief Get uri for device gateway registry
     * @return std::string uri
     */
    std::string getGatewayRegistryUri();

    /**
     * @brief Get uri for template server
     * @return std::string uri
     */
    std::string getTemplateServerUri();

private:
    NuguDeviceConfig config;
    NuguHttpRest* rest;
    unsigned int supported_grant_types;
    std::string ep_token;
    std::string ep_authorization;
    std::string uri_gateway_registry;
    std::string uri_template_server;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_AUTH_H__ */
