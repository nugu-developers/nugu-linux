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

#ifndef __NUGU_SESSION_MANAGER_INTERFACE_H__
#define __NUGU_SESSION_MANAGER_INTERFACE_H__

#include <json/json.h>
#include <string>

namespace NuguClientKit {

/**
 * @file session_manager_interface.hh
 * @defgroup SessionManagerInterface SessionManagerInterface
 * @ingroup SDKNuguClientKit
 * @brief SessionManager Interface
 *
 * Interface of SessionManager which manages session info.
 *
 * @{
 */

/**
 * @brief Model for containing session info.
 * @see ISessionManager
 */
typedef struct {
    std::string session_id; /**< session id */
    std::string ps_id; /**< play service id */
} Session;

/**
 * @brief SessionManager interface
 */
class ISessionManager {
public:
    virtual ~ISessionManager() = default;

    /**
     * @brief Set Session object which is received by Session Interface.
     * @param[in] dialog_id dialog request id for mapping Session object
     * @param[in] session Session object
     */
    virtual void set(const std::string& dialog_id, Session&& session) = 0;

    /**
     * @brief Activate Session which is mapped with dialog request id.
     * @param[in] dialog_id dialog request id for Session
     */
    virtual void activate(const std::string& dialog_id) = 0;

    /**
     * @brief Deactivate Session which is mapped with dialog request id.
     * @param[in] dialog_id dialog request id for Session
     */
    virtual void deactivate(const std::string& dialog_id) = 0;

    /**
     * @brief Get current active session info which is composed by session list.
     * @return session info which is formatted to json type
     */
    virtual Json::Value getActiveSessionInfo() = 0;

    /**
     * @brief Clear all session info.
     */
    virtual void clear() = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_SESSION_MANAGER_INTERFACE_H__ */
