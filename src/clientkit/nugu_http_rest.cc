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

#include "base/nugu_log.h"

#include "clientkit/nugu_http_rest.hh"

namespace NuguClientKit {

static NuguHttpHeader* make_header(const NuguHttpHeader* common_header,
    const NuguHttpHeader* additional_header)
{
    NuguHttpHeader* header = nullptr;

    if (common_header)
        header = nugu_http_header_dup(common_header);

    if (additional_header) {
        if (header)
            nugu_http_header_import(header, additional_header);
        else
            header = nugu_http_header_dup(additional_header);
    }

    return header;
}

NuguHttpRest::NuguHttpRest(const std::string& url)
    : common_header(nullptr)
{
    host = nugu_http_host_new(url.c_str());
}

NuguHttpRest::~NuguHttpRest()
{
    nugu_http_host_free(host);

    if (common_header)
        nugu_http_header_free(common_header);
}

void NuguHttpRest::setTimeout(long msecs)
{
    nugu_http_host_set_timeout(host, msecs);
}

void NuguHttpRest::setConnectionTimeout(long msecs)
{
    nugu_http_host_set_connection_timeout(host, msecs);
}

std::string NuguHttpRest::getUrl()
{
    return nugu_http_host_peek_url(host);
}

bool NuguHttpRest::addHeader(const std::string& key, const std::string& value)
{
    if (common_header == nullptr)
        common_header = nugu_http_header_new();

    if (nugu_http_header_add(common_header, key.c_str(), value.c_str()) < 0)
        return false;

    return true;
}

bool NuguHttpRest::removeHeader(const std::string& key)
{
    if (common_header == nullptr)
        return false;

    if (nugu_http_header_remove(common_header, key.c_str()) < 0)
        return false;

    return true;
}


std::string NuguHttpRest::findHeader(const std::string& key)
{
    const char *value;

    if (common_header == nullptr)
        return "";

    value = nugu_http_header_find(common_header, key.c_str());
    if (!value)
        return "";

    return value;
}

int NuguHttpRest::response_callback(NuguHttpRequest* req,
    const NuguHttpResponse* resp, void* user_data)
{
    struct pending_async_data *pdata = static_cast<struct pending_async_data*>(user_data);

    if (!pdata) {
        nugu_error("pdata is NULL");
        return 1;
    }

    pdata->cb(resp);
    delete pdata;

    return 1;
}

NuguHttpResponse* NuguHttpRest::get(const std::string& path,
    const NuguHttpHeader* additional_header)
{
    NuguHttpRequest* req;
    NuguHttpResponse* resp;
    NuguHttpHeader* header = make_header(common_header, additional_header);

    req = nugu_http_get_sync(host, path.c_str(), header);
    if (req == NULL) {
        nugu_error("nugu_http_get_sync() failed");
        if (header)
            nugu_http_header_free(header);
        return NULL;
    }

    if (header)
        nugu_http_header_free(header);

    resp = nugu_http_response_dup(nugu_http_request_response_get(req));
    nugu_http_request_free(req);

    if (resp->code == -1) {
        nugu_error("nugu_http_get_sync() failed");
        nugu_http_response_free(resp);
        return NULL;
    }

    return resp;
}

bool NuguHttpRest::get(const std::string& path, ResponseCallback cb)
{
    return get(path, nullptr, std::move(cb));
}

bool NuguHttpRest::get(const std::string& path,
    const NuguHttpHeader* additional_header, ResponseCallback cb)
{
    NuguHttpRequest* req;
    NuguHttpHeader* header = make_header(common_header, additional_header);
    struct pending_async_data *pdata;

    pdata = new struct pending_async_data;
    pdata->cb = std::move(cb);

    req = nugu_http_get(host, path.c_str(), header, response_callback, pdata);
    if (req == NULL) {
        nugu_error("nugu_http_get() failed");
        if (header)
            nugu_http_header_free(header);
        delete pdata;
        return false;
    }

    if (header)
        nugu_http_header_free(header);

    return true;
}

NuguHttpResponse* NuguHttpRest::post(const std::string& path, const std::string& body,
    const NuguHttpHeader* additional_header)
{
    NuguHttpRequest* req;
    NuguHttpResponse* resp;
    NuguHttpHeader* header = make_header(common_header, additional_header);

    req = nugu_http_post_sync(host, path.c_str(), header, body.c_str(), body.size());
    if (req == NULL) {
        nugu_error("nugu_http_post_sync() failed");
        if (header)
            nugu_http_header_free(header);
        return NULL;
    }

    if (header)
        nugu_http_header_free(header);

    resp = nugu_http_response_dup(nugu_http_request_response_get(req));
    nugu_http_request_free(req);

    if (resp->code == -1) {
        nugu_error("nugu_http_post_sync() failed");
        nugu_http_response_free(resp);
        return NULL;
    }

    return resp;
}

bool NuguHttpRest::post(const std::string& path, const std::string& body, ResponseCallback cb)
{
    return post(path, body, nullptr, std::move(cb));
}

bool NuguHttpRest::post(const std::string& path, const std::string& body,
    const NuguHttpHeader* additional_header, ResponseCallback cb)
{
    NuguHttpRequest* req;
    NuguHttpHeader* header = make_header(common_header, additional_header);
    struct pending_async_data *pdata;

    pdata = new struct pending_async_data;
    pdata->cb = std::move(cb);

    req = nugu_http_post(host, path.c_str(), header, body.c_str(), body.size(), response_callback, pdata);
    if (req == NULL) {
        nugu_error("nugu_http_post() failed");
        if (header)
            nugu_http_header_free(header);
        delete pdata;
        return false;
    }

    if (header)
        nugu_http_header_free(header);

    return true;
}

NuguHttpResponse* NuguHttpRest::put(const std::string& path, const std::string& body,
    const NuguHttpHeader* additional_header)
{
    NuguHttpRequest* req;
    NuguHttpResponse* resp;
    NuguHttpHeader* header = make_header(common_header, additional_header);

    req = nugu_http_put_sync(host, path.c_str(), header, body.c_str(), body.size());
    if (req == NULL) {
        nugu_error("nugu_http_put_sync() failed");
        if (header)
            nugu_http_header_free(header);
        return NULL;
    }

    if (header)
        nugu_http_header_free(header);

    resp = nugu_http_response_dup(nugu_http_request_response_get(req));
    nugu_http_request_free(req);

    if (resp->code == -1) {
        nugu_error("nugu_http_put_sync() failed");
        nugu_http_response_free(resp);
        return NULL;
    }

    return resp;
}

bool NuguHttpRest::put(const std::string& path, const std::string& body, ResponseCallback cb)
{
    return put(path, body, nullptr, std::move(cb));
}

bool NuguHttpRest::put(const std::string& path, const std::string& body,
    const NuguHttpHeader* additional_header, ResponseCallback cb)
{
    NuguHttpRequest* req;
    NuguHttpHeader* header = make_header(common_header, additional_header);
    struct pending_async_data *pdata;

    pdata = new struct pending_async_data;
    pdata->cb = std::move(cb);

    req = nugu_http_put(host, path.c_str(), header, body.c_str(), body.size(), response_callback, pdata);
    if (req == NULL) {
        nugu_error("nugu_http_put() failed");
        if (header)
            nugu_http_header_free(header);
        delete pdata;
        return false;
    }

    if (header)
        nugu_http_header_free(header);

    return true;
}

NuguHttpResponse* NuguHttpRest::del(const std::string& path,
    const NuguHttpHeader* additional_header)
{
    NuguHttpRequest* req;
    NuguHttpResponse* resp;
    NuguHttpHeader* header = make_header(common_header, additional_header);

    req = nugu_http_delete_sync(host, path.c_str(), header);
    if (req == NULL) {
        nugu_error("nugu_http_delete_sync() failed");
        if (header)
            nugu_http_header_free(header);
        return NULL;
    }

    if (header)
        nugu_http_header_free(header);

    resp = nugu_http_response_dup(nugu_http_request_response_get(req));
    nugu_http_request_free(req);

    if (resp->code == -1) {
        nugu_error("nugu_http_delete_sync() failed");
        nugu_http_response_free(resp);
        return NULL;
    }

    return resp;
}

bool NuguHttpRest::del(const std::string& path, ResponseCallback cb)
{
    return del(path, nullptr, std::move(cb));
}

bool NuguHttpRest::del(const std::string& path,
    const NuguHttpHeader* additional_header, ResponseCallback cb)
{
    NuguHttpRequest* req;
    NuguHttpHeader* header = make_header(common_header, additional_header);
    struct pending_async_data *pdata;

    pdata = new struct pending_async_data;
    pdata->cb = std::move(cb);

    req = nugu_http_delete(host, path.c_str(), header, response_callback, pdata);
    if (req == NULL) {
        nugu_error("nugu_http_delete() failed");
        if (header)
            nugu_http_header_free(header);
        delete pdata;
        return false;
    }

    if (header)
        nugu_http_header_free(header);

    return true;
}

} // NuguClientKit
