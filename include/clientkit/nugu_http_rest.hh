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

#ifndef __NUGU_HTTP_REST_H__
#define __NUGU_HTTP_REST_H__

#include <functional>
#include <string>

#include <base/nugu_http.h>

namespace NuguClientKit {

/**
 * @file nugu_http_rest.hh
 * @defgroup NuguHttpRest NuguHttpRest
 * @ingroup SDKNuguClientKit
 * @brief HTTP REST request management
 *
 * The NuguHttpRest handles REST(GET/POST/PUT/DELETE) requests using HTTP.
 *
 * @{
 */

/**
 * @brief NUGU HTTP Rest
 */
class NuguHttpRest {
public:
    explicit NuguHttpRest(const std::string& url);
    virtual ~NuguHttpRest();

    /**
     * @brief Callback prototype for receiving async response
     */
    typedef std::function<void(const NuguHttpResponse* resp)> ResponseCallback;

    /**
     * @brief Set timeout to host
     * @param[in] msecs millisecond
     */
    void setTimeout(long msecs);

    /**
     * @brief Set connection timeout to host
     * @param[in] msecs millisecond
     */
    void setConnectionTimeout(long msecs);

    /**
     * @brief Get the url of host
     * @return url
     */
    std::string getUrl();

    /**
     * @brief Add a key-value string to common header
     * @param[in] key key string, e.g. "Content-Type"
     * @param[in] value value string, e.g. "application/json"
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool addHeader(const std::string& key, const std::string& value);

    /**
     * @brief Remove a key-value from common header
     * @param[in] key key string, e.g. "Content-Type"
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool removeHeader(const std::string& key);

    /**
     * @brief Find a value for key from common header
     * @param[in] key key string, e.g. "Content-Type"
     * @return value
     */
    std::string findHeader(const std::string& key);

    /**
     * @brief HTTP GET requests
     * @param[in] path url path
     * @param[in] additional_header additional header
     * @return HTTP response object
     * @see nugu_http_response_free()
     */
    NuguHttpResponse* get(const std::string& path,
        const NuguHttpHeader* additional_header = nullptr);

    /**
     * @brief HTTP GET async requests
     * @param[in] path url path
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool get(const std::string& path, ResponseCallback cb);

    /**
     * @brief HTTP GET async requests
     * @param[in] path url path
     * @param[in] additional_header additional header
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool get(const std::string& path,
        const NuguHttpHeader* additional_header, ResponseCallback cb);

    /**
     * @brief HTTP POST requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] additional_header additional header
     * @return HTTP response object
     * @see nugu_http_response_free()
     */
    NuguHttpResponse* post(const std::string& path, const std::string& body,
        const NuguHttpHeader* additional_header = nullptr);

    /**
     * @brief HTTP POST async requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool post(const std::string& path, const std::string& body, ResponseCallback cb);

    /**
     * @brief HTTP POST async requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] additional_header additional header
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool post(const std::string& path, const std::string& body,
        const NuguHttpHeader* additional_header, ResponseCallback cb);

    /**
     * @brief HTTP PUT requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] additional_header additional header
     * @return HTTP response object
     * @see nugu_http_response_free()
     */
    NuguHttpResponse* put(const std::string& path, const std::string& body,
        const NuguHttpHeader* additional_header = nullptr);

    /**
     * @brief HTTP PUT async requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool put(const std::string& path, const std::string& body, ResponseCallback cb);

    /**
     * @brief HTTP PUT async requests
     * @param[in] path url path
     * @param[in] body body data to send
     * @param[in] additional_header additional header
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool put(const std::string& path, const std::string& body,
        const NuguHttpHeader* additional_header, ResponseCallback cb);

    /**
     * @brief HTTP DELETE requests
     * @param[in] path url path
     * @param[in] additional_header additional header
     * @return HTTP response object
     * @see nugu_http_response_free()
     */
    NuguHttpResponse* del(const std::string& path,
        const NuguHttpHeader* additional_header = nullptr);

    /**
     * @brief HTTP DELETE async requests
     * @param[in] path url path
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool del(const std::string& path, ResponseCallback cb);

    /**
     * @brief HTTP DELETE async requests
     * @param[in] path url path
     * @param[in] additional_header additional header
     * @param[in] cb callback function to receive response
     * @return result
     * @retval true success
     * @retval false failure
     */
    bool del(const std::string& path,
        const NuguHttpHeader* additional_header, ResponseCallback cb);

private:
    static int response_callback(NuguHttpRequest* req,
        const NuguHttpResponse* resp, void* user_data);

    NuguHttpHost* host;
    NuguHttpHeader* common_header;
    struct pending_async_data {
        ResponseCallback cb;
    };
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_HTTP_REST_H__ */
