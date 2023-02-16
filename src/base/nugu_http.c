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
#include <errno.h>

#include <glib.h>

#include "curl/curl.h"

#include "base/nugu_log.h"
#include "base/nugu_buffer.h"
#include "base/nugu_http.h"

#include "nugu_curl_log.h"

#define DEFAULT_TIMEOUT (60 * 1000)
#define DEFAULT_CONN_TIMEOUT (10 * 1000)
#define HEADER_CT "Content-Type"
#define HEADER_CT_JSON "application/json"

struct _nugu_http_header {
	GHashTable *map;
};

struct _nugu_http_request {
	NuguHttpHost *host;
	NuguHttpResponse *resp;

	CURL *curl;
	struct curl_slist *header_list;

	char *endpoint;

	NuguBuffer *resp_body;
	NuguBuffer *resp_header;

	void *body;
	size_t body_len;

	NuguHttpCallback callback;
	void *callback_userdata;

	GThread *tid;

	gboolean async_completed;
	gboolean destroy_reserved;

	FILE *fp;
	NuguHttpProgressCallback progress_callback;
	curl_off_t prev_dlnow;
	size_t prev_ratio;
};

struct _nugu_http_host {
	char *url;
	long timeout;
	long conn_timeout;
};

EXPORT_API NuguHttpHost *nugu_http_host_new(const char *url)
{
	struct _nugu_http_host *host;
	size_t url_len;

	g_return_val_if_fail(url != NULL, NULL);

	url_len = strlen(url);
	g_return_val_if_fail(url_len > 0, NULL);

	host = malloc(sizeof(struct _nugu_http_host));
	if (!host) {
		nugu_error_nomem();
		return NULL;
	}

	host->timeout = DEFAULT_TIMEOUT;
	host->conn_timeout = DEFAULT_CONN_TIMEOUT;

	host->url = g_strdup(url);
	if (!host->url) {
		nugu_error_nomem();
		free(host);
		return NULL;
	}

	/* Remove trailing '/' from url */
	if (host->url[url_len - 1] == '/')
		host->url[url_len - 1] = '\0';

	return host;
}

EXPORT_API void nugu_http_host_set_timeout(NuguHttpHost *host, long msecs)
{
	g_return_if_fail(host != NULL);

	host->timeout = msecs;
}

EXPORT_API void nugu_http_host_set_connection_timeout(NuguHttpHost *host,
						      long msecs)
{
	g_return_if_fail(host != NULL);

	host->conn_timeout = msecs;
}

EXPORT_API const char *nugu_http_host_peek_url(NuguHttpHost *host)
{
	g_return_val_if_fail(host != NULL, NULL);

	return host->url;
}

EXPORT_API void nugu_http_host_free(NuguHttpHost *host)
{
	g_return_if_fail(host != NULL);

	g_free(host->url);
	g_free(host);
}

EXPORT_API NuguHttpHeader *nugu_http_header_new(void)
{
	struct _nugu_http_header *header;

	header = malloc(sizeof(struct _nugu_http_header));
	if (!header) {
		nugu_error_nomem();
		return NULL;
	}

	header->map =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return header;
}

EXPORT_API int nugu_http_header_add(NuguHttpHeader *header, const char *key,
				    const char *value)
{
	g_return_val_if_fail(header != NULL, -1);
	g_return_val_if_fail(key != NULL, -1);
	g_return_val_if_fail(value != NULL, -1);

	g_hash_table_insert(header->map, g_strdup(key), g_strdup(value));

	return 0;
}

EXPORT_API int nugu_http_header_remove(NuguHttpHeader *header, const char *key)
{
	g_return_val_if_fail(header != NULL, -1);
	g_return_val_if_fail(key != NULL, -1);

	if (g_hash_table_remove(header->map, key) == FALSE)
		return -1;

	return 0;
}

EXPORT_API const char *nugu_http_header_find(NuguHttpHeader *header,
					     const char *key)
{
	g_return_val_if_fail(header != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);

	return g_hash_table_lookup(header->map, key);
}

EXPORT_API NuguHttpHeader *
nugu_http_header_dup(const NuguHttpHeader *src_header)
{
	NuguHttpHeader *header;

	g_return_val_if_fail(src_header != NULL, NULL);

	header = nugu_http_header_new();
	if (!header)
		return NULL;

	if (nugu_http_header_import(header, src_header) < 0) {
		nugu_http_header_free(header);
		return NULL;
	}

	return header;
}

EXPORT_API int nugu_http_header_import(NuguHttpHeader *header,
				       const NuguHttpHeader *from)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail(header != NULL, -1);
	g_return_val_if_fail(from != NULL, -1);

	g_hash_table_iter_init(&iter, from->map);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_hash_table_insert(header->map, g_strdup(key),
				    g_strdup(value));
	}

	return 0;
}

EXPORT_API void nugu_http_header_free(NuguHttpHeader *header)
{
	g_return_if_fail(header != NULL);

	if (header->map) {
		g_hash_table_destroy(header->map);
		header->map = NULL;
	}

	g_free(header);
}

static NuguHttpResponse *nugu_http_response_new(NuguHttpRequest *req)
{
	struct _nugu_http_response *resp;

	g_return_val_if_fail(req != NULL, NULL);

	resp = malloc(sizeof(struct _nugu_http_response));
	if (!resp) {
		nugu_error_nomem();
		return NULL;
	}

	resp->code = -1;
	resp->body = NULL;
	resp->body_len = 0;
	resp->header = NULL;
	resp->header_len = 0;

	return resp;
}

EXPORT_API NuguHttpResponse *
nugu_http_response_dup(const NuguHttpResponse *orig)
{
	struct _nugu_http_response *resp;

	g_return_val_if_fail(orig != NULL, NULL);

	resp = calloc(1, sizeof(struct _nugu_http_response));
	if (!resp) {
		nugu_error_nomem();
		return NULL;
	}

	resp->code = orig->code;

	if (orig->body_len > 0) {
		resp->body = malloc(orig->body_len + 1);
		memcpy((void *)resp->body, orig->body, orig->body_len);
		resp->body_len = orig->body_len;
		((char *)resp->body)[resp->body_len] = '\0';
	}

	if (orig->header_len > 0) {
		resp->header = malloc(orig->header_len + 1);
		memcpy((void *)resp->header, orig->header, orig->header_len);
		resp->header_len = orig->header_len;
		((char *)resp->header)[resp->header_len] = '\0';
	}

	return resp;
}

static void _nugu_http_response_free_internal(NuguHttpResponse *resp)
{
	g_return_if_fail(resp != NULL);

	memset(resp, 0, sizeof(struct _nugu_http_response));
	g_free(resp);
}

EXPORT_API void nugu_http_response_free(NuguHttpResponse *resp)
{
	g_return_if_fail(resp != NULL);

	if (resp->body)
		free((void *)resp->body);

	if (resp->header)
		free((void *)resp->header);

	_nugu_http_response_free_internal(resp);
}

static size_t _on_recv_header(void *ptr, size_t size, size_t nmemb,
			      void *user_data)
{
	NuguHttpRequest *req = user_data;

	if (req->resp->code == -1) {
		long code = 0;

		curl_easy_getinfo(req->curl, CURLINFO_RESPONSE_CODE, &code);
		if (code != 0)
			req->resp->code = code;
	}

	return nugu_buffer_add(req->resp_header, ptr, size * nmemb);
}

static size_t _on_recv_write(void *ptr, size_t size, size_t nmemb,
			     void *user_data)
{
	NuguHttpRequest *req = user_data;

	return nugu_buffer_add(req->resp_body, ptr, size * nmemb);
}

static size_t _on_recv_filewrite(void *ptr, size_t size, size_t nmemb,
				 void *user_data)
{
	NuguHttpRequest *req = user_data;
	size_t written;

	if (!req->fp)
		return 0;

	written = fwrite(ptr, size, nmemb, req->fp);
	if (written != size * nmemb)
		nugu_error("fwrite() failed: %s", strerror(errno));

	return written;
}

static size_t _on_progress(void *user_data, curl_off_t dltotal,
			   curl_off_t dlnow, curl_off_t ultotal,
			   curl_off_t ulnow)
{
	NuguHttpRequest *req = user_data;
	size_t ratio = 0;

	if (req->prev_dlnow == dlnow)
		return 0;

	if (dltotal > 0) {
		ratio = (dlnow * 100) / dltotal;
		if (req->prev_ratio == ratio)
			return 0;
	}

	if (req->progress_callback)
		req->progress_callback(req, req->resp, dlnow, dltotal,
				       req->callback_userdata);
	else
		nugu_dbg("download %zd / %zd bytes (%zd%%)", dlnow, dltotal,
			 ratio);

	req->prev_dlnow = dlnow;
	req->prev_ratio = ratio;

	return 0;
}

static void _curl_perform(NuguHttpRequest *req)
{
	CURLcode ret;

	nugu_dbg("start curl_easy_perform: %p", req);

	ret = curl_easy_perform(req->curl);
	if (ret != CURLE_OK) {
		nugu_error("curl_easy_perform failed: %s", curl_easy_strerror(ret));
		req->resp->code = -1;
		return;
	}

	curl_easy_getinfo(req->curl, CURLINFO_RESPONSE_CODE, &req->resp->code);

	nugu_dbg("request %p got response code: %d", req, req->resp->code);

	req->resp->header_len = nugu_buffer_get_size(req->resp_header);
	if (req->resp->header_len > 0)
		req->resp->header = nugu_buffer_peek(req->resp_header);
	nugu_buffer_add(req->resp_header, "\0", 1);

	req->resp->body_len = nugu_buffer_get_size(req->resp_body);
	if (req->resp->body_len > 0)
		req->resp->body = nugu_buffer_peek(req->resp_body);
	nugu_buffer_add(req->resp_body, "\0", 1);

	if (req->fp) {
		fclose(req->fp);
		req->fp = NULL;
	}
}

static gboolean _on_thread_request_done(gpointer user_data)
{
	struct _nugu_http_request *req = user_data;
	int req_free;

	req->async_completed = TRUE;
	if (req->destroy_reserved) {
		nugu_dbg("destroy the request due to reserved request");
		nugu_http_request_free(req);
		return FALSE;
	}

	req_free = req->callback(req, req->resp, req->callback_userdata);
	if (req_free)
		nugu_http_request_free(req);

	return FALSE;
}

static gpointer _on_thread_request(gpointer user_data)
{
	struct _nugu_http_request *req = user_data;

	_curl_perform(req);

	g_idle_add(_on_thread_request_done, req);

	return NULL;
}

static void _curl_perform_thread(NuguHttpRequest *req)
{
	req->tid = g_thread_new("curl_perform", _on_thread_request, req);
}

static void _map_row(gpointer key, gpointer value, gpointer user_data)
{
	NuguHttpRequest *req = user_data;
	char *header_str;

	header_str = g_strdup_printf("%s: %s", (char *)key, (char *)value);
	req->header_list = curl_slist_append(req->header_list, header_str);
	g_free(header_str);
}

static NuguHttpRequest *_request_new(NuguHttpHost *host, const char *path,
				     NuguHttpHeader *header)
{
	struct _nugu_http_request *req;

	req = calloc(1, sizeof(struct _nugu_http_request));
	if (!req) {
		nugu_error_nomem();
		return NULL;
	}

	req->curl = curl_easy_init();
	if (!req->curl) {
		nugu_error("curl_easy_init() failed");
		free(req);
		return NULL;
	}

	if (header)
		g_hash_table_foreach(header->map, _map_row, req);

	req->resp_body = nugu_buffer_new(0);
	if (!req->resp_body) {
		nugu_http_request_free(req);
		return NULL;
	}

	req->resp_header = nugu_buffer_new(0);
	if (!req->resp_header) {
		nugu_http_request_free(req);
		return NULL;
	}

	req->resp = nugu_http_response_new(req);
	if (!req->resp) {
		nugu_http_request_free(req);
		return NULL;
	}

	nugu_dbg("request(%p) created", req);

	req->host = host;

	if (path[0] != '/')
		req->endpoint = g_strdup_printf("%s/%s", host->url, path);
	else
		req->endpoint = g_strdup_printf("%s%s", host->url, path);

	if (req->header_list)
		curl_easy_setopt(req->curl, CURLOPT_HTTPHEADER,
				 req->header_list);

	if (host->timeout > 0) {
		curl_easy_setopt(req->curl, CURLOPT_TIMEOUT_MS, host->timeout);
		nugu_dbg("- timeout: %ld msecs", host->timeout);
	}

	if (host->conn_timeout > 0) {
		curl_easy_setopt(req->curl, CURLOPT_CONNECTTIMEOUT_MS,
				 host->conn_timeout);
		nugu_dbg("- connection timeout: %ld msecs", host->conn_timeout);
	}

	curl_easy_setopt(req->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(req->curl, CURLOPT_URL, req->endpoint);
	curl_easy_setopt(req->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, _on_recv_write);
	curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, req);
	curl_easy_setopt(req->curl, CURLOPT_HEADERFUNCTION, _on_recv_header);
	curl_easy_setopt(req->curl, CURLOPT_HEADERDATA, req);
	curl_easy_setopt(req->curl, CURLOPT_DEBUGFUNCTION,
			 nugu_curl_debug_callback);
	curl_easy_setopt(req->curl, CURLOPT_DEBUGDATA, req);

	return req;
}

static NuguHttpRequest *_request_new_body(NuguHttpHost *host, const char *path,
					  NuguHttpHeader *header,
					  const void *body, size_t body_len)
{
	NuguHttpRequest *req;
	NuguHttpHeader *tmp_header = NULL;

	if (header == NULL)
		tmp_header = nugu_http_header_new();
	else
		tmp_header = header;

	if (nugu_http_header_find(tmp_header, HEADER_CT) == NULL)
		nugu_http_header_add(tmp_header, HEADER_CT, HEADER_CT_JSON);

	req = _request_new(host, path, tmp_header);
	if (!req) {
		if (tmp_header != header)
			nugu_http_header_free(tmp_header);

		return NULL;
	}

	if (tmp_header != header)
		nugu_http_header_free(tmp_header);

	req->body = malloc(body_len);
	req->body_len = body_len;
	memcpy(req->body, body, body_len);

	curl_easy_setopt(req->curl, CURLOPT_POSTFIELDS, req->body);
	curl_easy_setopt(req->curl, CURLOPT_POSTFIELDSIZE, req->body_len);

	return req;
}

EXPORT_API NuguHttpRequest *
nugu_http_request(enum NuguHttpRequestType type, NuguHttpHost *host,
		  const char *path, NuguHttpHeader *header, const void *body,
		  size_t body_len, NuguHttpCallback callback, void *user_data)
{
	struct _nugu_http_request *req;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	if (type == NUGU_HTTP_REQUEST_POST || type == NUGU_HTTP_REQUEST_PUT) {
		g_return_val_if_fail(body != NULL, NULL);
		g_return_val_if_fail(body_len > 0, NULL);
	}

	if (body != NULL && body_len > 0) {
		req = _request_new_body(host, path, header, body, body_len);
		if (!req)
			return NULL;
	} else {
		req = _request_new(host, path, header);
		if (!req)
			return NULL;
	}

	req->callback = callback;
	req->callback_userdata = user_data;

	if (type == NUGU_HTTP_REQUEST_POST)
		curl_easy_setopt(req->curl, CURLOPT_POST, 1L);
	else if (type == NUGU_HTTP_REQUEST_PUT)
		curl_easy_setopt(req->curl, CURLOPT_CUSTOMREQUEST, "PUT");
	else if (type == NUGU_HTTP_REQUEST_DELETE)
		curl_easy_setopt(req->curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	_curl_perform_thread(req);

	return req;
}

EXPORT_API NuguHttpRequest *
nugu_http_request_sync(enum NuguHttpRequestType type, NuguHttpHost *host,
		       const char *path, NuguHttpHeader *header,
		       const void *body, size_t body_len)
{
	struct _nugu_http_request *req;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);

	if (type == NUGU_HTTP_REQUEST_POST || type == NUGU_HTTP_REQUEST_PUT) {
		g_return_val_if_fail(body != NULL, NULL);
		g_return_val_if_fail(body_len > 0, NULL);
	}

	if (body != NULL && body_len > 0) {
		req = _request_new_body(host, path, header, body, body_len);
		if (!req)
			return NULL;
	} else {
		req = _request_new(host, path, header);
		if (!req)
			return NULL;
	}

	if (type == NUGU_HTTP_REQUEST_POST)
		curl_easy_setopt(req->curl, CURLOPT_POST, 1L);
	else if (type == NUGU_HTTP_REQUEST_PUT)
		curl_easy_setopt(req->curl, CURLOPT_CUSTOMREQUEST, "PUT");
	else if (type == NUGU_HTTP_REQUEST_DELETE)
		curl_easy_setopt(req->curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	_curl_perform(req);

	return req;
}

EXPORT_API NuguHttpRequest *nugu_http_get(NuguHttpHost *host, const char *path,
					  NuguHttpHeader *header,
					  NuguHttpCallback callback,
					  void *user_data)
{
	return nugu_http_request(NUGU_HTTP_REQUEST_GET, host, path, header,
				 NULL, 0, callback, user_data);
}

EXPORT_API NuguHttpRequest *
nugu_http_get_sync(NuguHttpHost *host, const char *path, NuguHttpHeader *header)
{
	return nugu_http_request_sync(NUGU_HTTP_REQUEST_GET, host, path, header,
				      NULL, 0);
}

EXPORT_API NuguHttpRequest *nugu_http_post(NuguHttpHost *host, const char *path,
					   NuguHttpHeader *header,
					   const void *body, size_t body_len,
					   NuguHttpCallback callback,
					   void *user_data)
{
	return nugu_http_request(NUGU_HTTP_REQUEST_POST, host, path, header,
				 body, body_len, callback, user_data);
}

EXPORT_API NuguHttpRequest *
nugu_http_post_sync(NuguHttpHost *host, const char *path,
		    NuguHttpHeader *header, const void *body, size_t body_len)
{
	return nugu_http_request_sync(NUGU_HTTP_REQUEST_POST, host, path,
				      header, body, body_len);
}

EXPORT_API NuguHttpRequest *nugu_http_put(NuguHttpHost *host, const char *path,
					  NuguHttpHeader *header,
					  const void *body, size_t body_len,
					  NuguHttpCallback callback,
					  void *user_data)
{
	return nugu_http_request(NUGU_HTTP_REQUEST_PUT, host, path, header,
				 body, body_len, callback, user_data);
}

EXPORT_API NuguHttpRequest *
nugu_http_put_sync(NuguHttpHost *host, const char *path, NuguHttpHeader *header,
		   const void *body, size_t body_len)
{
	return nugu_http_request_sync(NUGU_HTTP_REQUEST_PUT, host, path, header,
				      body, body_len);
}

EXPORT_API NuguHttpRequest *
nugu_http_delete(NuguHttpHost *host, const char *path, NuguHttpHeader *header,
		 NuguHttpCallback callback, void *user_data)
{
	return nugu_http_request(NUGU_HTTP_REQUEST_DELETE, host, path, header,
				 NULL, 0, callback, user_data);
}

EXPORT_API NuguHttpRequest *nugu_http_delete_sync(NuguHttpHost *host,
						  const char *path,
						  NuguHttpHeader *header)
{
	return nugu_http_request_sync(NUGU_HTTP_REQUEST_DELETE, host, path,
				      header, NULL, 0);
}

EXPORT_API NuguHttpRequest *
nugu_http_download(NuguHttpHost *host, const char *path, const char *dest_path,
		   NuguHttpHeader *header, NuguHttpCallback callback,
		   NuguHttpProgressCallback progress_callback, void *user_data)
{
	struct _nugu_http_request *req;
	FILE *fp;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);
	g_return_val_if_fail(dest_path != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	fp = fopen(dest_path, "wb");
	if (!fp) {
		nugu_error("fopen(%s) failed: %s", dest_path, strerror(errno));
		return NULL;
	}

	req = _request_new(host, path, header);
	if (!req) {
		fclose(fp);
		return NULL;
	}

	req->fp = fp;
	curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, _on_recv_filewrite);
	curl_easy_setopt(req->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(req->curl, CURLOPT_XFERINFODATA, req);
	curl_easy_setopt(req->curl, CURLOPT_XFERINFOFUNCTION, _on_progress);

	/**
	 * Logs are turned off to reduce log messages when a progress callback
	 * is provided.
	 */
	if (progress_callback)
		curl_easy_setopt(req->curl, CURLOPT_VERBOSE, 0L);

	req->callback = callback;
	req->progress_callback = progress_callback;
	req->callback_userdata = user_data;

	_curl_perform_thread(req);

	return req;
}

EXPORT_API const NuguHttpResponse *
nugu_http_request_response_get(NuguHttpRequest *req)
{
	g_return_val_if_fail(req != NULL, NULL);

	return req->resp;
}

EXPORT_API void nugu_http_request_free(NuguHttpRequest *req)
{
	g_return_if_fail(req != NULL);

	if (req->callback && req->async_completed == FALSE) {
		req->destroy_reserved = TRUE;
		nugu_dbg("req(%p) destroy is reserved", req);
		return;
	}

	nugu_dbg("req(%p) destroy", req);

	if (req->fp)
		fclose(req->fp);

	if (req->tid)
		g_thread_join(req->tid);

	if (req->header_list)
		curl_slist_free_all(req->header_list);

	if (req->curl)
		curl_easy_cleanup(req->curl);

	if (req->resp)
		_nugu_http_response_free_internal(req->resp);

	if (req->resp_body)
		nugu_buffer_free(req->resp_body, TRUE);

	if (req->resp_header)
		nugu_buffer_free(req->resp_header, TRUE);

	if (req->body)
		free(req->body);

	g_free(req->endpoint);
	g_free(req);
}

EXPORT_API void nugu_http_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
}
