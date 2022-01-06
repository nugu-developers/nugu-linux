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

#ifndef __NUGU_SESSION_INTERFACE_H__
#define __NUGU_SESSION_INTERFACE_H__

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file session_interface.hh
 * @defgroup SessionInterface SessionInterface
 * @ingroup SDKNuguCapability
 * @brief Session capability interface
 *
 * It's for managing session info between play and client.
 *
 * @{
 */

/**
 * @brief Session State list
 * @see ISessionListener::onState
 */
enum class SessionState {
    ACTIVE, /**< Session is activated */
    INACTIVE /**< Session is deactivated */
};

/**
 * @brief session listener interface
 * @see ISessionHandler
 */
class ISessionListener : virtual public ICapabilityListener {
public:
    virtual ~ISessionListener() = default;

    /**
     * @brief Receive callback when the session state is changed.
     * @param[in] state session state
     * @param[in] dialog_id dialog request id for Session
     */
    virtual void onState(SessionState state, const std::string& dialog_id) = 0;
};

/**
 * @brief session handler interface
 * @see ISessionListener
 */
class ISessionHandler : virtual public ICapabilityInterface {
public:
    virtual ~ISessionHandler() = default;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_SESSION_INTERFACE_H__ */
