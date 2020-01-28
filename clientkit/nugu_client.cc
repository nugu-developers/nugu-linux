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

NuguClient::CapabilityBuilder* NuguClient::CapabilityBuilder::add(const CapabilityType ctype, ICapabilityListener* clistener, ICapabilityInterface* cap_instance)
{
    client_impl->registerCapability(ctype, std::make_pair(cap_instance, clistener));

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

void NuguClient::setConfig(NuguConfig::Key key, const std::string& value)
{
    impl->setConfig(key, value);
}

void NuguClient::setListener(INuguClientListener* listener)
{
    impl->setListener(listener);
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

IWakeupHandler* NuguClient::getWakeupHandler()
{
    return impl->getWakeupHandler();
}

IASRHandler* NuguClient::getASRHandler()
{
    return dynamic_cast<IASRHandler*>(impl->getCapabilityHandler(CapabilityType::ASR));
}

ITTSHandler* NuguClient::getTTSHandler()
{
    return dynamic_cast<ITTSHandler*>(impl->getCapabilityHandler(CapabilityType::TTS));
}

IAudioPlayerHandler* NuguClient::getAudioPlayerHandler()
{
    return dynamic_cast<IAudioPlayerHandler*>(impl->getCapabilityHandler(CapabilityType::AudioPlayer));
}

IDisplayHandler* NuguClient::getDisplayHandler()
{
    return dynamic_cast<IDisplayHandler*>(impl->getCapabilityHandler(CapabilityType::Display));
}

ITextHandler* NuguClient::getTextHandler()
{
    return dynamic_cast<ITextHandler*>(impl->getCapabilityHandler(CapabilityType::Text));
}

IExtensionHandler* NuguClient::getExtensionHandler()
{
    return dynamic_cast<IExtensionHandler*>(impl->getCapabilityHandler(CapabilityType::Extension));
}

IDelegationHandler* NuguClient::getDelegationHandler()
{
    return dynamic_cast<IDelegationHandler*>(impl->getCapabilityHandler(CapabilityType::Delegation));
}

ISystemHandler* NuguClient::getSystemHandler()
{
    return dynamic_cast<ISystemHandler*>(impl->getCapabilityHandler(CapabilityType::System));
}

IMicHandler* NuguClient::getMicHandler()
{
    return dynamic_cast<IMicHandler*>(impl->getCapabilityHandler(CapabilityType::Mic));
}

INetworkManager* NuguClient::getNetworkManager()
{
    return impl->getNetworkManager();
}

IMediaPlayer* NuguClient::createMediaPlayer()
{
    return impl->createMediaPlayer();
}

} // NuguClientKit
