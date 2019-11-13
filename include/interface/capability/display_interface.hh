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

#ifndef __NUGU_DISPLAY_INTERFACE_H__
#define __NUGU_DISPLAY_INTERFACE_H__

#include <iostream>
#include <list>
#include <string>

#include <interface/capability/capability_interface.hh>

namespace NuguInterface {

/**
 * @file display_interface.hh
 * @defgroup DisplayInterface DisplayInterface
 * @ingroup SDKNuguInterface
 * @brief Display capability interface
 *
 * Manage rendering and erasing according to metadata delivery and
 * context management policies to show the graphical user interface (GUI) on devices with displays.
 *
 * @{
 */

/**
 * @brief display listener interface
 * @see IDisplayHandler
 */
class IDisplayListener : public ICapabilityListener {
public:
    virtual ~IDisplayListener() = default;
    /**
     * @brief Request rendering by passing metadata so that the device with the display can draw on the screen.
     * @param[in] id display template id
     * @param[in] type display template type
     * @param[in] json template in json format for display
     * @param[in] dialog_id dialog request id
     * @see IAudioPlayerListener::renderDisplay()
     */
    virtual void renderDisplay(const std::string& id, const std::string& type, const std::string& json, const std::string& dialog_id) = 0;

    /**
     * @brief The SDK will ask you to delete the rendered display on the display according to the service context maintenance policy.
     * @param[in] unconditionally whether clear display unconditionally or not
     * @param[in] id display template id
     * @return true if display is cleared
     * @see IAudioPlayerListener::clearDisplay()
     */
    virtual bool clearDisplay(const std::string& id, bool unconditionally) = 0;
};

/**
 * @brief display handler interface
 * @see IDisplayListener
 */
class IDisplayHandler : public ICapabilityHandler {
public:
    virtual ~IDisplayHandler() = default;

    /**
     * @brief The user reports that the display was rendered.
     * @param[in] id display template id
     */
    virtual void displayRendered(const std::string& id) = 0;

    /**
     * @brief The user reports that the display is cleared.
     * @param[in] id display template id
     */
    virtual void displayCleared(const std::string& id) = 0;

    /**
     * @brief The user informs the selected item of the list and reports the token information of the item.
     * @param[in] id display template id
     * @param[in] item_token parsed token from metadata
     */
    virtual void elementSelected(const std::string& id, const std::string& item_token) = 0;

    /**
     * @brief Set the Listener object
     * @param[in] listener listener object
     */
    virtual void setListener(IDisplayListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] listener listener object
     */
    virtual void removeListener(IDisplayListener* listener) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_DISPLAY_INTERFACE_H__ */
