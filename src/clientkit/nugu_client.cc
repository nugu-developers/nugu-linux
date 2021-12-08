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

NuguClient::CapabilityBuilder* NuguClient::CapabilityBuilder::add(ICapabilityInterface* cap_instance)
{
    client_impl->registerCapability(cap_instance);

    return this;
}

bool NuguClient::CapabilityBuilder::construct()
{
    return (client_impl->create() == 0);
}

NuguClient::NuguClient()
    : impl(std::unique_ptr<NuguClientImpl>(new NuguClientImpl()))
{
    cap_builder = new CapabilityBuilder(impl.get());
}

NuguClient::~NuguClient()
{
    if (cap_builder) {
        delete cap_builder;
        cap_builder = nullptr;
    }
}

void NuguClient::setWakeupWord(const std::string& wakeup_word)
{
    impl->setWakeupWord(wakeup_word);
}

NuguClient::CapabilityBuilder* NuguClient::getCapabilityBuilder()
{
    return cap_builder;
}

bool NuguClient::loadPlugins(const std::string& path)
{
    return impl->loadPlugins(path);
}

void NuguClient::unloadPlugins(void)
{
    impl->unloadPlugins();
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

INetworkManager* NuguClient::getNetworkManager()
{
    return impl->getNetworkManager();
}

ICapabilityInterface* NuguClient::getCapabilityHandler(const std::string& cname)
{
    return impl->getCapabilityHandler(cname);
}

} // NuguClientKit
