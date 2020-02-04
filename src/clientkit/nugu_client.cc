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

#include "clientkit/nugu_client.hh"

#include "nugu_client_impl.hh"

namespace NuguClientKit {

NuguClient::CapabilityBuilder::CapabilityBuilder(NuguClientImpl* client_impl)
{
    this->client_impl = client_impl;
}

NuguClient::CapabilityBuilder::~CapabilityBuilder()
{
}

NuguClient::CapabilityBuilder* NuguClient::CapabilityBuilder::add(const std::string& cname, ICapabilityListener* clistener, ICapabilityInterface* cap_instance)
{
    client_impl->registerCapability(cname, std::make_pair(cap_instance, clistener));

    return this;
}

bool NuguClient::CapabilityBuilder::construct()
{
    return (client_impl->create() == 0);
}

NuguClient::NuguClient()
{
    impl = std::unique_ptr<NuguClientImpl>(new NuguClientImpl());
    cap_builder = std::unique_ptr<CapabilityBuilder>(new CapabilityBuilder(impl.get()));
}

NuguClient::~NuguClient()
{
}

void NuguClient::setListener(INuguClientListener* listener)
{
    impl->setListener(listener);
}

void NuguClient::setWakeupWord(const std::string& wakeup_word)
{
    impl->setWakeupWord(wakeup_word);
}

NuguClient::CapabilityBuilder* NuguClient::getCapabilityBuilder()
{
    return cap_builder.get();
}

bool NuguClient::initialize(void)
{
    return impl->initialize();
}

void NuguClient::deInitialize(void)
{
    impl->deInitialize();
}

INuguCoreContainer* NuguClient::getNuguCoreContainer()
{
    return impl->getNuguCoreContainer();
}

ICapabilityInterface* NuguClient::getCapabilityHandler(const std::string& cname)
{
    return impl->getCapabilityHandler(cname);
}

INetworkManager* NuguClient::getNetworkManager()
{
    return impl->getNetworkManager();
}

} // NuguClientKit
