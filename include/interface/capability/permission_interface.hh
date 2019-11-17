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

#ifndef __NUGU_PERMISSION_INTERFACE_H__
#define __NUGU_PERMISSION_INTERFACE_H__

#include <list>
#include <map>
#include <set>

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file permission_interface.hh
 * @defgroup PermissionInterface PermissionInterface
 * @ingroup SDKNuguInterface
 * @brief Permission capability interface
 *
 * It is used to handle about permission request. If sdk request permission, user application could grant or deny,
 * then has to send permission states to sdk.
 *
 * @{
 */

namespace Permission {
    /**
    * @brief Permission Type
    */
    enum class Type {
        LOCATION /**< Permission about LOCATION */
    };

    /**
    * @brief Permission State
    */
    enum class State {
        UNDETERMINED, /**< not check whether permission granted */
        GRANTED, /**< permission granted by user */
        DENIED, /**< permission denied by user */
        NOT_SUPPORTED /**< not support such permission */
    };

    const std::map<Type, std::string> TypeMap {
        { Type::LOCATION, "LOCATION" }
    };

    const std::map<State, std::string> StateMap {
        { State::UNDETERMINED, "UNDETERMINED" },
        { State::GRANTED, "GRANTED" },
        { State::DENIED, "DENIED" },
        { State::NOT_SUPPORTED, "NOT_SUPPORTED" }
    };
}

typedef struct {
    Permission::Type type;
    Permission::State state;
} PermissionState;

/**
 * @brief permission listener interface
 */
class IPermissionListener : public ICapabilityListener {
public:
    virtual ~IPermissionListener() = default;

    /**
     * @brief Request context information about permission state from user application.
     * @param[in] permission_list context information about permission state
     */
    virtual void requestContext(std::list<PermissionState>& permission_list) = 0;

    /**
     * @brief Request required permissions which are needed to specific play service.
     * @param[in] permission_set required permissions
     */
    virtual void requestPermission(const std::set<Permission::Type>& permission_set) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_PERMISSION_INTERFACE_H__ */
