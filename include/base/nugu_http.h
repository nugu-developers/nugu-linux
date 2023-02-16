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

#ifndef __NUGU_HTTP_H__
#define __NUGU_HTTP_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nugu_http.h
 * @defgroup NuguHttp NuguHttp
 * @ingroup SDKBase
 * @brief HTTP request management
 *
 * The NuguHttp handles REST(GET/POST/PUT/DELETE) requests using HTTP.
 *
 * @{
 */

/**
 * @brief HTTP Host object
 */
typedef struct _nugu_http_host NuguHttpHost;

/**
 * @brief HTTP Header object
 */
typedef struct _nugu_http_header NuguHttpHeader;

/**
 * @brief HTTP Request object
 */
typedef struct _nugu_http_request NuguHttpRequest;

/**
 * @brief HTTP Response object
 */
typedef struct _nugu_http_response NuguHttpResponse;

/**
 * @brief HTTP request types
 */
enum NuguHttpRequestType {
	NUGU_HTTP_REQUEST_GET, /* GET Request */
	NUGU_HTTP_REQUEST_POST, /* POST Request */
	NUGU_HTTP_REQUEST_PUT, /* PUT Request */
	NUGU_HTTP_REQUEST_DELETE, /* DELETE Request */
};

/**
 * @brief HTTP Response object
 * @see NuguHttpResponse
 */
struct _nugu_http_response {
	const void *body; /* Response body */
	size_t body_len; /* Response body length */
	const void *header; /* Response header */
	size_t header_len; /* Response header length */
	long code; /* Response code (200, 404, ... -1) */
};

/**
 * @brief Callback prototype for receiving async HTTP response
 * @return Whether to automatically free memory for NuguHttpRequest
 * @retval 1 automatically free memory after return the callback
 * @retval 0 free the memory manually by the nugu_http_request_free().
 */
typedef int (*NuguHttpCallback)(NuguHttpRequest *req,
				const NuguHttpResponse *resp, void *user_data);

/**
 * @brief Callback prototype for HTTP download progress
 */
typedef void (*NuguHttpProgressCallback)(NuguHttpRequest *req,
					 const NuguHttpResponse *resp,
					 size_t downloaded, size_t total,
					 void *user_data);

/**
 * @brief Initialize HTTP module (curl_global_init)
 */
void nugu_http_init(void);

/**
 * @brief Create a new HTTP host object
 * @param[in] url host url
 * @return HTTP host object
 * @see nugu_http_host_free()
 * @see nugu_http_peek_url();
 */
NuguHttpHost *nugu_http_host_new(const char *url);

/**
 * @brief Set timeout to host
 * @param[in] host host object
 * @param[in] msecs millisecond
 */
void nugu_http_host_set_timeout(NuguHttpHost *host, long msecs);

/**
 * @brief Set connection timeout to host
 * @param[in] host host object
 * @param[in] msecs millisecond
 */
void nugu_http_host_set_connection_timeout(NuguHttpHost *host, long msecs);

/**
 * @brief Get url from HTTP host object
 * @param[in] host host object
 * @return url
 */
const char *nugu_http_host_peek_url(NuguHttpHost *host);

/**
 * @brief Destroy the host object
 * @param[in] host host object
 * @see nugu_http_host_new()
 */
void nugu_http_host_free(NuguHttpHost *host);

/**
 * @brief Create a new HTTP header object
 * @return HTTP header object
 * @see nugu_http_header_free()
 */
NuguHttpHeader *nugu_http_header_new(void);

/**
 * @brief Add a key-value string to header object
 * @param[in] header header object
 * @param[in] key key string, e.g. "Content-Type"
 * @param[in] value value string, e.g. "application/json"
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_http_header_remove()
 */
int nugu_http_header_add(NuguHttpHeader *header, const char *key,
			 const char *value);

/**
 * @brief Remove a key-value string from header object
 * @param[in] header header object
 * @param[in] key key string
 * @return result
 * @retval 0 success
 * @retval -1 failure
 * @see nugu_http_header_add()
 */
int nugu_http_header_remove(NuguHttpHeader *header, const char *key);

/**
 * @brief Find value from header object
 * @param[in] header header object
 * @param[in] key key string
 * @return value string
 */
const char *nugu_http_header_find(NuguHttpHeader *header, const char *key);

/**
 * @brief Duplicate the http header object
 * @param[in] src_header source header object
 * @return duplicated new header object
 * @see nugu_http_header_new()
 */
NuguHttpHeader *nugu_http_header_dup(const NuguHttpHeader *src_header);

/**
 * @brief Import header data from other header object
 * @param[in] header header object
 * @param[in] from header object to import
 * @return result
 * @retval 0 success
 * @retval -1 failure
 */
int nugu_http_header_import(NuguHttpHeader *header, const NuguHttpHeader *from);

/**
 * @brief Destroy the header object
 * @param[in] header header object
 * @see nugu_http_header_new()
 */
void nugu_http_header_free(NuguHttpHeader *header);

/**
 * @brief HTTP async request
 * @param[in] type request type
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @param[in] callback callback function to receive response
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *nugu_http_request(enum NuguHttpRequestType type,
				   NuguHttpHost *host, const char *path,
				   NuguHttpHeader *header, const void *body,
				   size_t body_len, NuguHttpCallback callback,
				   void *user_data);

/**
 * @brief HTTP sync request
 * @param[in] type request type
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @return HTTP request object
 * @see nugu_http_request_free()
 * @see nugu_http_request_response_get()
 */
NuguHttpRequest *nugu_http_request_sync(enum NuguHttpRequestType type,
					NuguHttpHost *host, const char *path,
					NuguHttpHeader *header,
					const void *body, size_t body_len);

/**
 * @brief A convenient API for HTTP GET async requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] callback callback function to receive response
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *nugu_http_get(NuguHttpHost *host, const char *path,
			       NuguHttpHeader *header,
			       NuguHttpCallback callback, void *user_data);

/**
 * @brief A convenient API for HTTP GET sync requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @return HTTP request object
 * @see nugu_http_request_free()
 * @see nugu_http_request_response_get()
 */
NuguHttpRequest *nugu_http_get_sync(NuguHttpHost *host, const char *path,
				    NuguHttpHeader *header);

/**
 * @brief A convenient API for HTTP POST async requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @param[in] callback callback function to receive response
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *nugu_http_post(NuguHttpHost *host, const char *path,
				NuguHttpHeader *header, const void *body,
				size_t body_len, NuguHttpCallback callback,
				void *user_data);

/**
 * @brief A convenient API for HTTP POST sync requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @return HTTP request object
 * @see nugu_http_request_free()
 * @see nugu_http_request_response_get()
 */
NuguHttpRequest *nugu_http_post_sync(NuguHttpHost *host, const char *path,
				     NuguHttpHeader *header, const void *body,
				     size_t body_len);

/**
 * @brief A convenient API for HTTP PUT async requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @param[in] callback callback function to receive response
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *nugu_http_put(NuguHttpHost *host, const char *path,
			       NuguHttpHeader *header, const void *body,
			       size_t body_len, NuguHttpCallback callback,
			       void *user_data);

/**
 * @brief A convenient API for HTTP PUT sync requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] body body data to send
 * @param[in] body_len length of body data
 * @return HTTP request object
 * @see nugu_http_request_free()
 * @see nugu_http_request_response_get()
 */
NuguHttpRequest *nugu_http_put_sync(NuguHttpHost *host, const char *path,
				    NuguHttpHeader *header, const void *body,
				    size_t body_len);

/**
 * @brief A convenient API for HTTP DELETE async requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @param[in] callback callback function to receive response
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *nugu_http_delete(NuguHttpHost *host, const char *path,
				  NuguHttpHeader *header,
				  NuguHttpCallback callback, void *user_data);

/**
 * @brief A convenient API for HTTP DELETE sync requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] header header object
 * @return HTTP request object
 * @see nugu_http_request_free()
 * @see nugu_http_request_response_get()
 */
NuguHttpRequest *nugu_http_delete_sync(NuguHttpHost *host, const char *path,
				       NuguHttpHeader *header);

/**
 * @brief A convenient API for HTTP file download async requests
 * @param[in] host host object
 * @param[in] path url path
 * @param[in] dest_path path to save the file
 * @param[in] header header object
 * @param[in] callback callback function to receive response
 * @param[in] progress_callback callback function to receive download progress
 * @param[in] user_data data to pass to the user callback
 * @return HTTP request object
 * @see nugu_http_request_free()
 */
NuguHttpRequest *
nugu_http_download(NuguHttpHost *host, const char *path, const char *dest_path,
		   NuguHttpHeader *header, NuguHttpCallback callback,
		   NuguHttpProgressCallback progress_callback, void *user_data);

/**
 * @brief Destroy the HTTP request object
 * @param[in] req request object
 * @see nugu_http_request()
 * @see nugu_http_request_sync()
 */
void nugu_http_request_free(NuguHttpRequest *req);

/**
 * @brief Duplicate the HTTP response object
 * @param[in] orig Original response object
 * @return HTTP response object
 * @see nugu_http_response_free()
 */
NuguHttpResponse *nugu_http_response_dup(const NuguHttpResponse *orig);

/**
 * @brief Destroy the HTTP response object
 * @param[in] resp response object
 * @see nugu_http_response_dup()
 */
void nugu_http_response_free(NuguHttpResponse *resp);

/**
 * @brief Get the HTTP response from request object
 * @param[in] req request object
 * @return HTTP response object
 */
const NuguHttpResponse *nugu_http_request_response_get(NuguHttpRequest *req);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
