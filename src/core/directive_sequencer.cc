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

#include <algorithm>

#include "base/nugu_log.h"
#include "base/nugu_network_manager.h"
#include "base/nugu_prof.h"

#include "directive_sequencer.hh"

namespace NuguCore {

static void dump_policies(const DirectiveSequencer::BlockingPolicyMap& m)
{
    if (m.size() == 0) {
        nugu_dbg("Empty blocking policy");
        return;
    }

    nugu_info("--[BlockingPolicy]--");

    for (auto& item : m) {
        nugu_dbg("\tNamespace: %s", item.first.c_str());

        for (auto& policy : item.second) {
            nugu_dbg("\t\tName: %s, Medium: %d, Blocking: %d", policy.first.c_str(),
                policy.second.medium, policy.second.isBlocking);
        }
    }
}

static void dump_dialog_queue(const DirectiveSequencer::DialogQueueMap& audio, const DirectiveSequencer::DialogQueueMap& visual)
{
    if (audio.size() == 0) {
        nugu_dbg("Empty DialogQueue for Medium::AUDIO");
    } else {
        nugu_info("--[DialogQueue for Medium::AUDIO]--");

        for (auto& queue : audio) {
            nugu_dbg("\tDialog-id: '%s'", queue.first.c_str());
            for (auto& ndir : queue.second) {
                nugu_dbg("\t\t%s.%s", nugu_directive_peek_namespace(ndir),
                    nugu_directive_peek_name(ndir));
            }
        }
    }

    if (visual.size() == 0) {
        nugu_dbg("Empty DialogQueue for Medium::VISUAL");
    } else {
        nugu_info("--[DialogQueue for Medium::VISUAL]--");

        for (auto& queue : visual) {
            nugu_dbg("\tDialog-id: '%s'", queue.first.c_str());
            for (auto& ndir : queue.second) {
                nugu_dbg("\t\t%s.%s", nugu_directive_peek_namespace(ndir),
                    nugu_directive_peek_name(ndir));
            }
        }
    }
}

static void dump_msgid(const std::map<std::string, NuguDirective*>& m)
{
    if (m.size() == 0) {
        nugu_dbg("Empty msgid lookup table");
        return;
    }

    nugu_info("--[msgid lookup table]--");

    for (auto& ndir : m) {
        nugu_dbg("\tMessage-id(%s): %s.%s dialog_id(%s)", ndir.first.c_str(),
            nugu_directive_peek_namespace(ndir.second),
            nugu_directive_peek_name(ndir.second),
            nugu_directive_peek_dialog_id(ndir.second));
    }
}

DirectiveSequencer::DirectiveSequencer()
    : idler_src(0)
{
    nugu_network_manager_set_directive_callback(onDirective, this);
    nugu_network_manager_set_attachment_callback(onAttachment, this);
}

DirectiveSequencer::~DirectiveSequencer()
{
    /* Cancel the pending idler */
    if (idler_src != 0)
        g_source_remove(idler_src);

    dump_policies(policy_map);
    dump_dialog_queue(audio_map, visual_map);
    dump_msgid(msgid_directive_map);

    /* Remove scheduled(but not yet handled) directives */
    for (auto& ndir : scheduled_list) {
        nugu_directive_unref(ndir);
    }

    nugu_network_manager_set_directive_callback(NULL, NULL);
    nugu_network_manager_set_attachment_callback(NULL, NULL);
}

/* Callback by network-manager */
void DirectiveSequencer::onDirective(NuguDirective* ndir, void* userdata)
{
    DirectiveSequencer* sequencer = static_cast<DirectiveSequencer*>(userdata);

    if (sequencer->add(ndir) == false)
        nugu_directive_unref(ndir);
}

/* Callback by network-manager */
void DirectiveSequencer::onAttachment(const char* parent_msg_id, int seq,
    int is_end, const char* media_type, size_t length,
    const void* data, void* userdata)
{
    DirectiveSequencer* sequencer = static_cast<DirectiveSequencer*>(userdata);
    NuguDirective* ndir;
    DirectiveSequencer::MsgidDirectiveMap::const_iterator iter;

    iter = sequencer->msgid_directive_map.find(parent_msg_id);
    if (iter == sequencer->msgid_directive_map.end()) {
        nugu_error("can't find directive for attachment parent_message_id('%s')", parent_msg_id);
        return;
    }

    ndir = iter->second;
    if (!ndir) {
        nugu_error("internal error: invalid directive");
        return;
    }

    if (seq == 0)
        nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT,
            nugu_directive_peek_dialog_id(ndir),
            nugu_directive_peek_msg_id(ndir), NULL);

    if (is_end) {
        nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_LAST_ATTACHMENT,
            nugu_directive_peek_dialog_id(ndir),
            nugu_directive_peek_msg_id(ndir), NULL);

        nugu_directive_close_data(ndir);
    }

    nugu_directive_set_media_type(ndir, media_type);
    nugu_directive_add_data(ndir, length, (unsigned char*)data);
}

gboolean DirectiveSequencer::onNext(gpointer userdata)
{
    DirectiveSequencer* sequencer = static_cast<DirectiveSequencer*>(userdata);

    nugu_dbg("idle callback: process next directives");

    for (auto& ndir : sequencer->scheduled_list)
        sequencer->handleDirective(ndir);

    sequencer->idler_src = 0;
    sequencer->scheduled_list.clear();

    return FALSE;
}

bool DirectiveSequencer::complete(NuguDirective* ndir)
{
    const char* name_space = nugu_directive_peek_namespace(ndir);
    const char* name = nugu_directive_peek_name(ndir);
    const char* msg_id = nugu_directive_peek_msg_id(ndir);

    /* Remove from lookup table */
    DirectiveSequencer::MsgidDirectiveMap::const_iterator msgid_iter = msgid_directive_map.find(msg_id);
    if (msgid_iter != msgid_directive_map.end())
        msgid_directive_map.erase(msgid_iter);

    BlockingPolicy policy = getPolicy(name_space, name);

    nugu_dbg("directive completed: '%s.%s' (medium: %d, isBlocking: %d)",
        name_space, name, policy.medium, policy.isBlocking);

    DirectiveSequencer::DialogQueueMap* qmap = nullptr;

    if (policy.medium == BlockingMedium::AUDIO) {
        qmap = &audio_map;
    } else if (policy.medium == BlockingMedium::VISUAL) {
        qmap = &visual_map;
    } else {
        /* NONE type does not need to check the queue */
        nugu_directive_unref(ndir);
        return true;
    }

    const char* dialog_id = nugu_directive_peek_dialog_id(ndir);
    DirectiveSequencer::DialogQueueMap::iterator iter = qmap->find(dialog_id);

    /* There is no queue for the dialog */
    if (iter == qmap->end()) {
        nugu_directive_unref(ndir);
        return true;
    }

    /* The directive does not match the first entry in the queue. */
    if (iter->second.front() != ndir) {
        nugu_error("not yet handled blocking directive(%s)", dialog_id);
        return false;
    }

    /* Remove the blocking directive from the queue */
    iter->second.pop_front();

    dump_dialog_queue(audio_map, visual_map);

    /* The queue corresponding to the dialog is empty */
    if (iter->second.size() == 0) {
        nugu_dbg("no pending directives. remove the dialog(%s) from map", dialog_id);
        qmap->erase(dialog_id);
        nugu_directive_unref(ndir);
        return true;
    }

    nugu_directive_unref(ndir);

    /* Add next directive to scheduled list to handle at next idle time */
    scheduled_list.push_back(iter->second.front());

    if (idler_src == 0)
        idler_src = g_idle_add(onNext, this);

    return true;
}

bool DirectiveSequencer::preHandleDirective(NuguDirective* ndir)
{
    const char* name_space = nugu_directive_peek_namespace(ndir);

    for (auto& listener : listeners_map[name_space]) {
        if (listener->onPreHandleDirective(ndir) == true) {
            nugu_info("The '%s.%s' directive pre-handled", name_space,
                nugu_directive_peek_name(ndir));
            return true;
        }
    }

    return false;
}

void DirectiveSequencer::handleDirective(NuguDirective* ndir)
{
    const char* name_space = nugu_directive_peek_namespace(ndir);
    const char* name = nugu_directive_peek_name(ndir);

    nugu_dbg("process the directive '%s.%s'", name_space, name);
    nugu_directive_set_active(ndir, 1);

    for (auto& listener : listeners_map[name_space]) {
        if (listener->onHandleDirective(ndir) == false) {
            nugu_error("fail to handle '%s.%s' directive", name_space, name);
            complete(ndir);
            return;
        }
    }
}

void DirectiveSequencer::cancelDirective(NuguDirective* ndir)
{
    const char* name_space = nugu_directive_peek_namespace(ndir);
    const char* name = nugu_directive_peek_name(ndir);

    nugu_dbg("cancel the directive '%s.%s'", name_space, name);

    for (auto& listener : listeners_map[name_space]) {
        listener->onCancelDirective(ndir);
    }
}

bool DirectiveSequencer::add(NuguDirective* ndir)
{
    if (ndir == nullptr) {
        nugu_error("ndir is NULL");
        return false;
    }

    const char* name_space = nugu_directive_peek_namespace(ndir);
    const char* name = nugu_directive_peek_name(ndir);

    if (listeners_map.find(name_space) == listeners_map.end()) {
        nugu_error("can't find '%s' capability agent", name_space);
        return false;
    }

    BlockingPolicy policy = getPolicy(name_space, name);

    nugu_dbg("receive '%s.%s' directive (medium: %d, isBlocking: %s)",
        name_space, name, policy.medium,
        (policy.isBlocking == true) ? "True" : "False");

    /* Stop propagation if the directive was pre-processed */
    if (preHandleDirective(ndir))
        return true;

    /**
     * Add the directive to the lookup table to quickly find the corresponding
     * directive when receiving an attachment.
     */
    msgid_directive_map[nugu_directive_peek_msg_id(ndir)] = ndir;

    DialogQueueMap* qmap = nullptr;

    if (policy.medium == BlockingMedium::AUDIO) {
        qmap = &audio_map;
    } else if (policy.medium == BlockingMedium::VISUAL) {
        qmap = &visual_map;
    } else {
        nugu_dbg("BlockingMedium::NONE type - handle immediately");
        handleDirective(ndir);
        return true;
    }

    const char* dialog_id = nugu_directive_peek_dialog_id(ndir);

    DialogQueueMap::iterator iter = qmap->find(dialog_id);

    /* There are already blocking directive in the queue */
    if (iter != qmap->end()) {
        nugu_dbg("push '%s.%s' to queue due to the previous blocking directive.", name_space, name);
        iter->second.push_back(ndir);
        dump_dialog_queue(audio_map, visual_map);
        return true;
    }

    nugu_dbg("Handle the directive immediately due to empty queue");

    if (policy.isBlocking) {
        nugu_dbg("push '%s.%s' to queue due to blocking policy", name_space, name);
        (*qmap)[dialog_id].push_back(ndir);
    }

    dump_dialog_queue(audio_map, visual_map);

    handleDirective(ndir);

    return true;
}

void DirectiveSequencer::addListener(const std::string& name_space,
    IDirectiveSequencerListener* listener)
{
    if (listener == nullptr)
        return;

    auto iterator = listeners_map.find(name_space);
    if (iterator == listeners_map.end()) {
        listeners_map[name_space].push_back(listener);
    } else {
        if (std::find(iterator->second.begin(), iterator->second.end(), listener) == iterator->second.end()) {
            iterator->second.push_back(listener);
        }
    }
}

void DirectiveSequencer::removeListener(const std::string& name_space,
    IDirectiveSequencerListener* listener)
{
    if (listener == nullptr)
        return;

    auto iterator = listeners_map.find(name_space);
    if (iterator == listeners_map.end()) {
        nugu_error("can't find the namespace '%s'", name_space.c_str());
        return;
    }

    auto listener_iter = std::find(iterator->second.begin(), iterator->second.end(), listener);
    if (listener_iter == iterator->second.end()) {
        nugu_error("can't find the listener %p", listener);
        return;
    }

    iterator->second.erase(listener_iter);

    if (listeners_map[name_space].begin() == listeners_map[name_space].end())
        listeners_map.erase(name_space);
}

bool DirectiveSequencer::addPolicy(const std::string& name_space,
    const std::string& name, BlockingPolicy policy)
{
    BlockingPolicyMap::const_iterator iter = policy_map.find(name_space);
    if (iter != policy_map.end()) {
        std::map<std::string, BlockingPolicy>::const_iterator j = iter->second.find(name);
        if (j != iter->second.end()) {
            nugu_error("'%s.%s' policy already exist", name_space.c_str(), name.c_str());
            return false;
        }
    }

    policy_map[name_space].insert(std::make_pair(name, policy));

    nugu_dbg("add policy: '%s.%s' medium=%d, blocking=%d",
        name_space.c_str(), name.c_str(), policy.medium, policy.isBlocking);

    return true;
}

BlockingPolicy DirectiveSequencer::getPolicy(const std::string& name_space,
    const std::string& name)
{
    BlockingPolicy default_policy = { BlockingMedium::NONE, false };

    BlockingPolicyMap::const_iterator iter = policy_map.find(name_space);
    if (iter == policy_map.end())
        return default_policy;

    std::map<std::string, BlockingPolicy>::const_iterator j = iter->second.find(name);
    if (j == iter->second.end())
        return default_policy;

    return j->second;
}

bool DirectiveSequencer::cancel(const std::string& dialog_id)
{
    DialogQueueMap::iterator audio_iter;
    DialogQueueMap::iterator visual_iter;
    DirectiveSequencer::MsgidDirectiveMap::const_iterator msgid_iter;

    nugu_dbg("cancel all directives for dialog_id(%s)", dialog_id.c_str());

    /* remove directive from scheduled list */
    auto new_end = std::remove_if(scheduled_list.begin(), scheduled_list.end(),
        [dialog_id](NuguDirective* ndir) {
            return (dialog_id.compare(nugu_directive_peek_dialog_id(ndir)) == 0);
        });
    scheduled_list.erase(new_end, scheduled_list.end());

    audio_iter = audio_map.find(dialog_id);
    visual_iter = visual_map.find(dialog_id);

    if (audio_iter == audio_map.end() && visual_iter == visual_map.end()) {
        nugu_error("can't find pending directives for dialog_id(%s)", dialog_id.c_str());
        return true;
    }

    dump_msgid(msgid_directive_map);

    /* remove audio queue */
    if (audio_iter != audio_map.end()) {
        for (auto& ndir : audio_iter->second) {
            /* Remove from lookup table */
            msgid_iter = msgid_directive_map.find(nugu_directive_peek_msg_id(ndir));
            if (msgid_iter != msgid_directive_map.end())
                msgid_directive_map.erase(msgid_iter);

            cancelDirective(ndir);

            /* Destroy the directive */
            nugu_directive_unref(ndir);
        }

        /* Clear the queue */
        audio_iter->second.clear();

        audio_map.erase(dialog_id);
    }

    /* remove visual queue */
    if (visual_iter != visual_map.end()) {
        for (auto& ndir : visual_iter->second) {
            /* Remove from lookup table */
            msgid_iter = msgid_directive_map.find(nugu_directive_peek_msg_id(ndir));
            if (msgid_iter != msgid_directive_map.end())
                msgid_directive_map.erase(msgid_iter);

            cancelDirective(ndir);

            /* Destroy the directive */
            nugu_directive_unref(ndir);
        }

        /* Clear the queue */
        visual_iter->second.clear();

        visual_map.erase(dialog_id);
    }

    dump_msgid(msgid_directive_map);

    dump_dialog_queue(audio_map, visual_map);

    return true;
}

}