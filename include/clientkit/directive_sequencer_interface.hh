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

#ifndef __NUGU_DIRECTIVE_SEQUENCER_INTERFACE_H__
#define __NUGU_DIRECTIVE_SEQUENCER_INTERFACE_H__

#include <base/nugu_directive.h>

#include <set>

namespace NuguClientKit {

/**
 * @file directive_sequencer_interface.hh
 * @defgroup DirectiveSequencerInterface DirectiveSequencerInterface
 * @ingroup SDKNuguClientKit
 * @brief Directive Sequencer Interface
 *
 * DirectiveSequencer delivers the directives received from the network to
 * the CapabilityAgent according to the dialog-id and blocking policy.
 *
 * @{
 */

/**
 * @brief BlockingMedium
 */
enum class BlockingMedium {
    AUDIO = NUGU_DIRECTIVE_MEDIUM_AUDIO, /**< Audio related medium */
    VISUAL = NUGU_DIRECTIVE_MEDIUM_VISUAL, /**< Visual related medium */
    NONE = NUGU_DIRECTIVE_MEDIUM_NONE, /**< None medium */
    ANY = NUGU_DIRECTIVE_MEDIUM_ANY /**< Any medium */
};

/**
 * @brief BlockingPolicy
 */
typedef struct {
    BlockingMedium medium; /**< BlockingMedium */
    bool isBlocking; /**< true: Blocking, false: Non-blocking */
} BlockingPolicy;

/**
 * @brief IDirectiveSequencerListener
 * @see IDirectiveSequencer
 */
class IDirectiveSequencerListener {
public:
    virtual ~IDirectiveSequencerListener() = default;

    /**
     * @brief Notify the directive to handle in advance
     * @param[in] ndir NuguDirective object
     * @return Stop propagation or not.
     * @retval true The directive is handled. Stop propagating to other listeners.
     * @retval false The directive is not handled.
     */
    virtual bool onPreHandleDirective(NuguDirective* ndir) = 0;

    /**
     * @brief Notify the directive to handle
     * @param[in] ndir NuguDirective object
     * @return Directive processing result
     * @retval true The directives were handled normally.
     * @retval false There was a problem with the directive processing.
     */
    virtual bool onHandleDirective(NuguDirective* ndir) = 0;

    /**
     * @brief Notify the directive to cancel
     * @param[in] ndir NuguDirective object
     */
    virtual void onCancelDirective(NuguDirective* ndir) = 0;
};

/**
 * @brief IDirectiveSequencer
 * @see IDirectiveSequencerListener
 */
class IDirectiveSequencer {
public:
    virtual ~IDirectiveSequencer() = default;

    /**
     * @brief Add the Listener object
     * @param[in] name_space directive namespace
     * @param[in] listener listener object
     */
    virtual void addListener(const std::string& name_space, IDirectiveSequencerListener* listener) = 0;

    /**
     * @brief Remove the Listener object
     * @param[in] name_space directive namespace
     * @param[in] listener listener object
     */
    virtual void removeListener(const std::string& name_space, IDirectiveSequencerListener* listener) = 0;

    /**
     * @brief Add the BlockingPolicy
     * @param[in] name_space directive namespace
     * @param[in] name directive name
     * @param[in] policy BlockingPolicy
     * @return Result of adding policy
     * @retval true success
     * @retval false failure
     */
    virtual bool addPolicy(const std::string& name_space, const std::string& name, BlockingPolicy policy) = 0;

    /**
     * @brief Get the BlockingPolicy for namespace.name
     * @param[in] name_space directive namespace
     * @param[in] name directive name
     * @return BlockingPolicy
     */
    virtual BlockingPolicy getPolicy(const std::string& name_space, const std::string& name) = 0;

    /**
     * @brief Cancel all pending directives related to the dialog_id.
     * The canceled directives are freed.
     * @param[in] dialog_id dialog-request-id
     * @param[in] cancel_active_directive cancel including currently active directives
     * @return result
     * @retval true success
     * @retval false failure
     */
    virtual bool cancel(const std::string& dialog_id, bool cancel_active_directive = true) = 0;

    /**
     * @brief Cancels specific pending directives related to the dialog_id.
     * The canceled directives are freed.
     * @param[in] dialog_id dialog-request-id
     * @param[in] groups list of directives('{Namespace}.{Name}')
     * @return result
     * @retval true success
     * @retval false failure
     */
    virtual bool cancel(const std::string& dialog_id, std::set<std::string> groups) = 0;

    /**
     * @brief Complete the blocking directive. The NuguDirective object will be destroyed.
     * If there are pending directives, those directives will be processed at the next idle time.
     * @param[in] ndir NuguDirective object
     * @return result
     * @retval true success
     * @retval false failure
     */
    virtual bool complete(NuguDirective* ndir) = 0;

    /**
     * @brief Add new directive to sequencer
     * @param[in] ndir NuguDirective object
     * @return result
     * @retval true success
     * @retval false failure
     */
    virtual bool add(NuguDirective* ndir) = 0;

    /**
     * @brief Get the last canceled dialog_id
     * @return dialog_id
     */
    virtual const std::string& getCanceledDialogId() = 0;

    /**
     * @brief Find directive from pending list
     * @param[in] name_space directive namespace
     * @param[in] name directive name
     * @return NuguDirective object or NULL
     */
    virtual const NuguDirective* findPending(const std::string& name_space, const std::string& name) = 0;
};

/**
 * @}
 */

} // NuguClientKit

#endif /* __NUGU_DIRECTIVE_SEQUENCER_INTERFACE_H__ */
