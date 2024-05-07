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

#ifndef __NUGU_RUNNER_IMPL_H__
#define __NUGU_RUNNER_IMPL_H__

#include "clientkit/nugu_runner.hh"

namespace NuguCore {

class NuguRunnerImplPrivate;

class NuguRunnerImpl {
public:
    NuguRunnerImpl();
    virtual ~NuguRunnerImpl();

    bool invokeMethod(const std::string& tag,
        NuguClientKit::NuguRunner::request_method method,
        NuguClientKit::ExecuteType type = NuguClientKit::ExecuteType::Auto,
        int timeout = 0);

private:
    void addMethod2Dispatcher(const std::string& tag,
        NuguClientKit::NuguRunner::request_method method,
        NuguClientKit::ExecuteType type);

private:
    NuguRunnerImplPrivate* d;
};

} // NuguCore

#endif // __NUGU_RUNNER_IMPL_H__
