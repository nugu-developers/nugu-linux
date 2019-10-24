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

#ifndef __NUGU_MOVEMENT_INTERFACE_H__
#define __NUGU_MOVEMENT_INTERFACE_H__

#include <interface/capability/capability_interface.hh>
#include <string>

namespace NuguInterface {

/**
 * @file movement_interface.hh
 * @defgroup MovementInterface MovementInterface
 * @ingroup SDKNuguInterface
 * @brief Movement capability interface
 *
 * Control the device's movement, rotation and dance functions.
 *
 * @{
 */

/**
 * @brief MovementState
 */
enum class MovementState {
    MOVING, /**< The device state is moving */
    ROTATING, /**< The device state is rotating */
    DANCING, /**< The device state is dancing */
    STOP /**< The device state is stop */
};

/**
 * @brief MovementDirection
 */
enum class MovementDirection {
    NONE, /**< Available when the status is dancing or stop. */
    FRONT, /**< Move to front. Available when the status is moving. */
    BACK, /**< Move to back. Available when the status is moving. */
    LEFT, /**< Rotate to left. Available when the status is rotating. */
    RIGHT /**< Rotate to right. Available when the status is rotating. */
};

/**
 * @brief MovementLightColor
 */
enum class MovementLightColor {
    WHITE, /**< White */
    RED, /**< Red */
    BLUE, /**< Blue */
    YELLOW, /**< Yellow */
    GREEN, /**< Green */
    PURPLE, /**< Purple */
    PINK, /**< Pink */
    ORANGE, /**< Orange */
    VIOLET, /**< Violet */
    SKYBLUE /**< Skyblue */
};

/**
 * @brief Movement listener interface
 * @see IMovementHandler
 */
class IMovementListener : public ICapabilityListener {
public:
    virtual ~IMovementListener() = default;

    /**
     * @brief Received a move request from the server
     * @param[in] direction FRONT or BACK
     * @param[in] time_secs time in seconds
     * @param[in] speed 1 ~ 99. Larger numbers mean faster
     * @param[in] distance_cm distance in centimeter
     * @param[in] count repeat count
     * @see IMovementHandler::moveStarted()
     * @see IMovementHandler::moveFinished()
     */
    virtual bool onMove(MovementDirection direction, int time_secs, int speed, int distance_cm, int count) = 0;

    /**
     * @brief Received a rotate request from the server
     * @param[in] direction LEFT or RIGHT
     * @param[in] time_secs time in seconds
     * @param[in] speed 1 ~ 99. Larger numbers mean faster
     * @param[in] angle rotation angle in degrees
     * @param[in] count repeat count
     * @see IMovementHandler::rotateStarted()
     * @see IMovementHandler::rotateFinished()
     */
    virtual void onRotate(MovementDirection direction, int time_secs, int speed, int angle, int count) = 0;

    /**
     * @brief Received a change color request from the server
     * @param[in] color color value
     * @see IMovementHandler::changeColorSucceeded()
     */
    virtual void onChangeLightColor(MovementLightColor color) = 0;

    /**
     * @brief Received a turn on light request from the server
     * @param[in] color color value
     * @see IMovementHandler::turnOnLightSucceeded()
     */
    virtual void onTurnOnLight(MovementLightColor color) = 0;

    /**
     * @brief Received a turn off light request from the server
     * @see IMovementHandler::turnOffLightSucceeded()
     */
    virtual void onTurnOffLight() = 0;

    /**
     * @brief Received a dance request from the server
     * @param[in] count repeat count
     * @see IMovementHandler::danceStarted()
     * @see IMovementHandler::danceFinished()
     */
    virtual void onDance(int count) = 0;

    /**
     * @brief Received a stop request from the server
     */
    virtual void onStop() = 0;
};

/**
 * @brief Movement handler interface
 * @see IMovementListener
 */
class IMovementHandler : public ICapabilityHandler {
public:
    virtual ~IMovementHandler() = default;

    /**
     * @brief Send an event to indicate that the move has started.
     * @see IMovementListener::onMove()
     */
    virtual void moveStarted(MovementDirection direction) = 0;

    /**
     * @brief Send an event to indicate that the move has finished.
     * @see IMovementListener::onMove()
     */
    virtual void moveFinished() = 0;

    /**
     * @brief Send an event to indicate that the rotate has started.
     * @see IMovementListener::onRotate()
     */
    virtual void rotateStarted(MovementDirection direction) = 0;

    /**
     * @brief Send an event to indicate that the rotate has finished.
     * @see IMovementListener::onRotate()
     */
    virtual void rotateFinished() = 0;

    /**
     * @brief Send an event to indicate that the color has changed.
     * @see IMovementListener::onChangeLightColor()
     */
    virtual void changeColorSucceeded(MovementLightColor color) = 0;

    /**
     * @brief Send an event to indicate that the light is on.
     * @see IMovementListener::onTurnOnLight()
     */
    virtual void turnOnLightSucceeded() = 0;

    /**
     * @brief Send an event to indicate that the light is off.
     * @see IMovementListener::onTurnOffLight()
     */
    virtual void turnOffLightSucceeded() = 0;

    /**
     * @brief Send an event to indicate that the dance has started.
     * @see IMovementListener::onDance()
     */
    virtual void danceStarted() = 0;

    /**
     * @brief Send an event to indicate that the dance has finished.
     * @see IMovementListener::onDance()
     */
    virtual void danceFinished() = 0;

    /**
     * @brief Send an event to indicate that the exception has occurred.
     * @param[in] directive directive name. e.g. "Movement.Rotate"
     * @param[in] message custom error message
     */
    virtual void exceptionEncountered(const std::string& directive, const std::string& message) = 0;
};

/**
 * @}
 */

} // NuguInterface

#endif /* __NUGU_MOVEMENT_INTERFACE_H__ */
