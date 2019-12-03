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

#ifndef __NUGU_DELEGATION_INTERFACE_H__
#define __NUGU_DELEGATION_INTERFACE_H__

#include <string>

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file delegation_interface.hh
 * @defgroup DelegationInterface DelegationInterface
 * @ingroup SDKNuguInterface
 * @brief Delegation capability interface
 *
 * It is used in case which another application needs to send and receive data by SDK.
 *
 * @{
 */

/**
 * @brief delegation listener interface
 * @see IDelegationHandler
 */
class IDelegationListener : public ICapabilityListener {
public:
    virtual ~IDelegationListener() = default;

    /**
     * @brief receive result of delegate directive
     * @param[in] app_id information for executing target application
     * @param[in] ps_id identifier for request play service
     * @param[in] data data which is needed to handle by application
     */
    virtual void delegate(const std::string& app_id, const std::string& ps_id, const std::string& data) = 0;

    /**
     * @brief Request context information from the external application.
     * @param[in] ps_id identifier for request play service
     * @param[in] data context information
     * @return true if request succeed
     */
    virtual bool requestContext(std::string& ps_id, std::string& data) = 0;
};

/**
 * @brief delegation handler interface
 * @see IDelegationListener
 */
class IDelegationHandler : virtual public ICapabilityInterface {
public:
    virtual ~IDelegationHandler() = default;

    /**
     * @brief request required play service
     * @return true if request succeed
     */
    virtual bool request() = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_DELEGATION_INTERFACE_H__ */
