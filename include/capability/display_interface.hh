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

enum class ControlLyricsDirection {
    PREVIOUS,
    NEXT
};

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
     * @param[in] id display template id
     * @param[in] unconditionally whether clear display unconditionally or not
     * @return true if display is cleared
     * @see IAudioPlayerListener::clearDisplay()
     */
    virtual bool clearDisplay(const std::string& id, bool unconditionally) = 0;

    /**
     * @brief SDK request information about device's lyrics page available
     * @param[out] visible show lyrics page visible
     * @return return device's lyrics page available
     */
    virtual bool requestLyricsPageAvailable(bool& visible) = 0;

    /**
     * @brief Request to the user to show the lyrics page. (only use AudioPlayerAgent)
     * @return return true if show lyrics success, otherwise false.
     */
    virtual bool showLyrics() = 0;

    /**
     * @brief Request to the user to hide the lyrics page. (only use AudioPlayerAgent)
     * @return return true if hide lyrics success, otherwise false.
     */
    virtual bool hideLyrics() = 0;

    /**
     * @brief Request to the user to control the lyrics page. (only use AudioPlayerAgent)
     * @param[in] direction move direction in the lyrics page
     * @return return true if control lyrics success, otherwise false.
     */
    virtual bool controlLyrics(ControlLyricsDirection direction) = 0;
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

    /**
     * @brief stop display rendering hold timer.
     * @param[in] id display template id
     */
    virtual void stopRenderingTimer(const std::string& id) = 0;
};

/**
 * @}
 */

} // NuguCapability

#endif /* __NUGU_DISPLAY_INTERFACE_H__ */
