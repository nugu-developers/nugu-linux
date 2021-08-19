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

#include <clientkit/capability_interface.hh>

namespace NuguCapability {

using namespace NuguClientKit;

/**
 * @file display_interface.hh
 * @defgroup DisplayInterface DisplayInterface
 * @ingroup SDKNuguCapability
 * @brief Display capability interface
 *
 * Manage rendering and erasing according to metadata delivery and
 * context management policies to show the graphical user interface (GUI) on devices with displays.
 *
 * @{
 */

enum class ControlDirection {
    PREVIOUS, /**< Previous direction */
    NEXT /**< Next direction */
};

enum class ControlType {
    Focus, /**< Focus type */
    Scroll, /**< Scroll type */
};

/**
 * @brief display listener interface
 * @see IDisplayHandler
 */
class IDisplayListener : virtual public ICapabilityListener {
public:
    virtual ~IDisplayListener() = default;
    /**
     * @brief Request rendering by passing metadata so that the device with the display can draw on the screen.
     * @param[in] id display template id
     * @param[in] type display template type
     * @param[in] json_payload template in json format for display
     * @param[in] dialog_id dialog request id
     */
    virtual void renderDisplay(const std::string& id, const std::string& type, const std::string& json_payload, const std::string& dialog_id) = 0;

    /**
     * @brief The SDK will ask you to delete the rendered display on the display according to the service context maintenance policy.
     * @param[in] id display template id
     * @param[in] unconditionally whether clear display unconditionally or not
     * @param[in] has_next whether has next display to render
     * @return true if display is cleared
     */
    virtual bool clearDisplay(const std::string& id, bool unconditionally, bool has_next) = 0;

    /**
     * @brief Request to control the display with type and direction
     * @param[in] id display template id
     * @param[in] type control type
     * @param[in] direction control direction
     */
    virtual void controlDisplay(const std::string& id, ControlType type, ControlDirection direction) = 0;

    /**
     * @brief Request to update the current display
     * @param[in] id display template id
     * @param[in] json_payload template in json format for display
     */
    virtual void updateDisplay(const std::string& id, const std::string& json_payload) = 0;
};

/**
 * @brief display handler interface
 * @see IDisplayListener
 */
class IDisplayHandler : virtual public ICapabilityInterface {
public:
    virtual ~IDisplayHandler() = default;

    /**
     * @brief The user reports that the display was rendered.
     * @param[in] id display template id
     */
    virtual void displayRendered(const std::string& id);

    /**
     * @brief The user reports that the display is cleared.
     * @param[in] id display template id
     */
    virtual void displayCleared(const std::string& id);

    /**
     * @brief The user informs the selected item of the list and reports the token information of the item.
     * @param[in] id display template id
     * @param[in] item_token parsed token from metadata
     * @param[in] postback postback data if the item is selectable
     */
    virtual void elementSelected(const std::string& id, const std::string& item_token, const std::string& postback = "");

    /**
     * @brief The user informs the control result
     * @param[in] id display template id
     * @param[in] type control type
     * @param[in] direction control direction
     */
    virtual void informControlResult(const std::string& id, ControlType type, ControlDirection direction);

    /**
     * @brief Set the IDisplayListener object
     * @param[in] listener listener object
     */
    virtual void setDisplayListener(IDisplayListener* listener);

    /**
     * @brief Remove the IDisplayListener object
     */
    virtual void removeDisplayListener(IDisplayListener* listener);

    /**
     * @brief Stop display rendering hold timer.
     * @param[in] id display template id
     */
    virtual void stopRenderingTimer(const std::string& id);

    /**
     * @brief Refresh display rendering hold timer.
     * @param[in] id display template id
     */
    virtual void refreshRenderingTimer(const std::string& id);
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_DISPLAY_INTERFACE_H__ */
