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

#ifndef __NUGU_CLIENT_LISTENER_H__
#define __NUGU_CLIENT_LISTENER_H__

namespace NuguClientKit {

/**
 * @file nugu_client_listener.hh
 * @defgroup NuguClient NuguClientInterface
 * @ingroup SDKNuguClientKit
 * @brief Nugu Client Interface
 *
 * @{
 */

/**
 * @brief nugu client listener interface
 */
class INuguClientListener {
public:
    virtual ~INuguClientListener() = default;

    /**
     * @brief Report when nugu client is initialized.
     * @param[in] userdata pass the userdata
     */
    virtual void onInitialized(void* userdata) = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_CLIENT_LISTENER_H__ */
