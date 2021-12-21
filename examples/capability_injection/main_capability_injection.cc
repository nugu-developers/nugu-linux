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

#include <glib.h>
#include <iostream>
#include <string>

#include <base/nugu_log.h>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <clientkit/nugu_client.hh>

#include "battery_agent.hh"

using namespace NuguClientKit;
using namespace NuguCapability;

class MyNetwork : public INetworkManagerListener {
public:
    void onStatusChanged(NetworkStatus status) override
    {
        switch (status) {
        case NetworkStatus::DISCONNECTED:
            std::cout << "Network disconnected !" << std::endl;
            break;
        case NetworkStatus::CONNECTING:
            std::cout << "Network connecting..." << std::endl;
            break;
        case NetworkStatus::READY:
            std::cout << "Network ready !" << std::endl;
            break;
        case NetworkStatus::CONNECTED:
            std::cout << "Network connected !" << std::endl;
            break;
        default:
            break;
        }
    }

    void onError(NetworkError error) override
    {
        switch (error) {
        case NetworkError::FAILED:
            std::cout << "Network failed !" << std::endl;
            break;
        case NetworkError::TOKEN_ERROR:
            std::cout << "Token error !" << std::endl;
            break;
        case NetworkError::UNKNOWN:
            std::cout << "Unknown error !" << std::endl;
            break;
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " battery level (ex.40)]" << std::endl;
        return 0;
    }

    std::string battery_level = argv[1];

    /* Turn off the SDK internal log */
    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    auto nugu_client(std::make_shared<NuguClient>());

    /* built-in capability */
    auto system_handler(std::shared_ptr<ISystemHandler>(
        CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>()));

    /* add-on capability for injection */
    auto battery_agent(std::make_shared<BatteryAgent>());
    battery_agent->setBatteryLevel(battery_level);
    battery_agent->setCharging(true);

    /* Register build-in capabilities */
    nugu_client->getCapabilityBuilder()
        ->add(system_handler.get())
        ->add(battery_agent.get())
        ->construct();

    if (!nugu_client->initialize()) {
        std::cout << "SDK Initialization failed." << std::endl;
        return -1;
    }

    auto network_manager_listener(std::make_shared<MyNetwork>());
    auto network_manager(nugu_client->getNetworkManager());
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(getenv("NUGU_TOKEN"));
    network_manager->connect();

    /* Start GMainLoop */
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    std::cout << "Start the eventloop" << std::endl;
    g_main_loop_run(loop);

    /* wait until g_main_loop_quit() */

    g_main_loop_unref(loop);

    nugu_client->deInitialize();

    return EXIT_SUCCESS;
}
