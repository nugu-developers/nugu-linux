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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <glib.h>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "base/nugu_log.h"
#include "clientkit/nugu_runner.hh"

namespace NuguClientKit {

class NuguRunnerPrivate {
public:
    NuguRunnerPrivate()
        : done(0)
        , src_id(0)
    {
    }
    ~NuguRunnerPrivate()
    {
        std::lock_guard<std::mutex> guard(m);

        tag_vector.clear();
        type_map.clear();
        method_map.clear();
    }
    static gboolean method_dispatch_cb(void* userdata);
    void add_method(const std::string& tag, NuguRunner::request_method method, ExecuteType type);
    void remove_method(const std::string& tag);

public:
    static std::map<NuguRunner*, NuguRunnerPrivate*> private_map;
    std::vector<std::string> tag_vector;
    std::map<std::string, ExecuteType> type_map;
    std::map<std::string, NuguRunner::request_method> method_map;
    std::mutex m;
    std::condition_variable cv;
    static int unique;
    int done;
    guint src_id;
};

int NuguRunnerPrivate::unique = 0;
std::map<NuguRunner*, NuguRunnerPrivate*> NuguRunnerPrivate::private_map;

gboolean NuguRunnerPrivate::method_dispatch_cb(void* userdata)
{
    NuguRunner* runner = static_cast<NuguRunner*>(userdata);
    NuguRunnerPrivate* d = NuguRunnerPrivate::private_map[runner];

    if (!d) {
        nugu_error("Nugu Runner is something wrong");
        return FALSE;
    }

    std::lock_guard<std::mutex> guard(d->m);

    for (const auto& tag : d->tag_vector) {
        nugu_info("[Method: %s] will execute", tag.c_str());
        if (d->method_map[tag])
            d->method_map[tag]();

        ExecuteType type = d->type_map[tag];
        if (type == ExecuteType::Blocking) {
            d->done = 1;
            nugu_info("[Method: %s] release blocking", tag.c_str());
            d->cv.notify_one();
        }
    }

    d->tag_vector.clear();
    d->method_map.clear();
    d->type_map.clear();
    d->src_id = 0;

    return FALSE;
}

void NuguRunnerPrivate::add_method(const std::string& tag, NuguRunner::request_method method, ExecuteType type)
{
    nugu_info("[Method: %s] is reserved", tag.c_str());

    if (std::find(tag_vector.begin(), tag_vector.end(), tag) == tag_vector.end())
        tag_vector.push_back(tag);

    method_map[tag] = std::move(method);
    type_map[tag] = type;
}

void NuguRunnerPrivate::remove_method(const std::string& tag)
{
    nugu_info("[Method: %s] is removed", tag.c_str());

    auto tag_iter = std::find(tag_vector.begin(), tag_vector.end(), tag);
    if (tag_iter != tag_vector.end())
        tag_vector.erase(tag_iter);

    auto type_iter = type_map.find(tag);
    if (type_iter != type_map.end())
        type_map.erase(type_iter);

    auto method_iter = method_map.find(tag);
    if (method_iter != method_map.end())
        method_map.erase(method_iter);
}

NuguRunner::NuguRunner()
    : d(new NuguRunnerPrivate())
{
    d->private_map[this] = d;
}

NuguRunner::~NuguRunner()
{
    auto private_iter = d->private_map.find(this);
    if (private_iter != d->private_map.end())
        d->private_map.erase(private_iter);

    if (d->src_id > 0)
        g_source_remove(d->src_id);

    delete d;
}

bool NuguRunner::invokeMethod(const std::string& tag, request_method method, ExecuteType type, int timeout)
{
    if (type != ExecuteType::Queued) {
        if (g_main_context_is_owner(g_main_context_default()) == TRUE) {
            nugu_info("[Method: %s] is executed immediately", tag.c_str());
            if (method)
                method();
            return true;
        }
    }

    std::string unique_tag = tag + std::to_string(d->unique++);
    addMethod2Dispatcher(unique_tag, std::move(method), type);

    if (type == ExecuteType::Blocking) {
        std::mutex mtx;
        std::unique_lock<std::mutex> lk(mtx);

        if (timeout <= 0) {
            d->cv.wait(lk);
            return true;
        }

        auto now = std::chrono::system_clock::now();
        if (!d->cv.wait_until(lk, now + std::chrono::milliseconds(timeout * 1000),
                [&]() { return d->done == 1; })) {
            std::lock_guard<std::mutex> guard(d->m);

            nugu_warn("The method(%s) is released blocking by timeout (%d sec)", unique_tag.c_str(), timeout);
            d->remove_method(unique_tag);
            return false;
        }
    }

    return true;
}

void NuguRunner::addMethod2Dispatcher(const std::string& tag, NuguRunner::request_method method, ExecuteType type)
{
    std::lock_guard<std::mutex> guard(d->m);

    d->add_method(tag, std::move(method), type);

    if (!d->src_id)
        d->src_id = g_idle_add(d->method_dispatch_cb, (void*)this);

    d->done = 0;
}

} // NuguClientKit
