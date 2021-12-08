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
#include <string>

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

    nugu_dbg("--[BlockingPolicy]--");

    for (auto& item : m) {
        nugu_dbg("\tNamespace: %s", item.first.c_str());

        for (auto& policy : item.second) {
            nugu_dbg("\t\tName: %s, Medium: %d, Blocking: %d",
                policy.first.c_str(),
                policy.second.medium, policy.second.isBlocking);
        }
    }
}

/**
 * Lookup Table - String-NuguDirective map
 *
 *   Table["message-id"] = NuguDirective*
 *
 */
class LookupTable {
public:
    explicit LookupTable(const std::string& title);
    virtual ~LookupTable();

    NuguDirective* find(const std::string& id);
    bool set(const std::string& id, NuguDirective* ndir);
    void remove(const std::string& id);
    void dump();
    void clearWithUnref();

private:
    std::string title;
    std::map<std::string, NuguDirective*> table;
};

LookupTable::LookupTable(const std::string& title)
    : title(title)
{
}

LookupTable::~LookupTable()
{
}

NuguDirective* LookupTable::find(const std::string& id)
{
    NuguDirective* ndir;
    std::map<std::string, NuguDirective*>::const_iterator iter;

    iter = table.find(id);
    if (iter == table.end())
        return NULL;

    ndir = iter->second;
    if (!ndir) {
        nugu_error("internal error: invalid directive");
        return NULL;
    }

    return ndir;
}

bool LookupTable::set(const std::string& id, NuguDirective* ndir)
{
    if (ndir == NULL)
        return false;

    table[id] = ndir;

    return true;
}

void LookupTable::remove(const std::string& id)
{
    table.erase(id);
}

void LookupTable::clearWithUnref()
{
    for (auto& iter : table)
        nugu_directive_unref(iter.second);

    table.clear();
}

void LookupTable::dump()
{
    if (table.size() == 0) {
        nugu_dbg("-- %s: none", title.c_str());
        return;
    }

    nugu_dbg("-- %s items --", title.c_str());

    for (auto& ndir : table) {
        nugu_dbg("\tMessage-Id(%s): %s.%s dialog_id(%s) msg_id(%s)",
            ndir.first.c_str(),
            nugu_directive_peek_namespace(ndir.second),
            nugu_directive_peek_name(ndir.second),
            nugu_directive_peek_dialog_id(ndir.second),
            nugu_directive_peek_msg_id(ndir.second));
    }
}

/**
 * Dialog-DirectiveList Map
 *
 *   DialogListMap["dialog-id-a"] = [
 *     NuguDirective(block), NuguDirective(non-block)
 *   ],
 *   DialogListMap["dialog-id-b"] = [
 *     NuguDirective(block)
 *   ]
 *
 */
class DialogDirectiveList {
public:
    explicit DialogDirectiveList(const std::string& title);
    virtual ~DialogDirectiveList();

    bool push(NuguDirective* ndir);
    bool isEmpty(const char* dialog_id);

    bool remove(NuguDirective* ndir);
    template <typename NOTIFY>
    bool remove(const std::string& dialog_id, NOTIFY notify);
    template <typename NOTIFY>
    bool removeByName(const std::string& dialog_id, const std::string& namespace_name, NOTIFY notify);

    void erase(const char* dialog_id);
    template <typename NOTIFY>
    void clear(NOTIFY notify);
    void dump();
    std::vector<NuguDirective*>* getList(const char* dialog_id);
    NuguDirective* findByMedium(const char* dialog_id, enum nugu_directive_medium medium);
    bool isBlockByPolicy(NuguDirective* target_dir);

    NuguDirective* find(const char* name_space, const char* name);

    using DialogListMap = std::map<std::string, std::vector<NuguDirective*>>;

private:
    std::string title;
    DialogListMap listmap;
};

DialogDirectiveList::DialogDirectiveList(const std::string& title)
    : title(title)
{
}

DialogDirectiveList::~DialogDirectiveList()
{
}

bool DialogDirectiveList::push(NuguDirective* ndir)
{
    const char* dialog_id = nugu_directive_peek_dialog_id(ndir);

    listmap[dialog_id].push_back(ndir);

    return true;
}

bool DialogDirectiveList::isEmpty(const char* dialog_id)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter != listmap.end())
        return false;

    return true;
}

bool DialogDirectiveList::remove(NuguDirective* ndir)
{
    if (ndir == NULL)
        return false;

    const char* dialog_id = nugu_directive_peek_dialog_id(ndir);
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return true;

    std::vector<NuguDirective*> dlist = iter->second;

    /* remove the directive from directive list */
    dlist.erase(
        std::remove_if(dlist.begin(), dlist.end(),
            [ndir](const NuguDirective* target_ndir) {
                return (ndir == target_ndir);
            }),
        dlist.end());

    iter->second = dlist;

    if (dlist.size() == 0)
        listmap.erase(dialog_id);

    return true;
}

template <typename NOTIFY>
bool DialogDirectiveList::remove(const std::string& dialog_id, NOTIFY notify)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return true;

    for (auto& item : iter->second)
        notify(item);

    listmap.erase(dialog_id);

    return true;
}

template <typename NOTIFY>
bool DialogDirectiveList::removeByName(const std::string& dialog_id, const std::string& namespace_name, NOTIFY notify)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return true;

    std::vector<NuguDirective*> dlist = iter->second;

    /* remove the directive from directive list */
    dlist.erase(
        std::remove_if(dlist.begin(), dlist.end(),
            [namespace_name, notify](NuguDirective* ndir) {
                std::string temp = nugu_directive_peek_namespace(ndir);
                temp.append(".").append(nugu_directive_peek_name(ndir));

                if (namespace_name == temp) {
                    notify(ndir);
                    return true;
                }

                return false;
            }),
        dlist.end());

    iter->second = dlist;

    if (dlist.size() == 0)
        listmap.erase(dialog_id);

    return true;
}

NuguDirective* DialogDirectiveList::findByMedium(const char* dialog_id,
    enum nugu_directive_medium medium)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return NULL;

    for (auto& ndir : iter->second) {
        if (nugu_directive_get_blocking_medium(ndir) == medium)
            return ndir;
    }

    return NULL;
}

bool DialogDirectiveList::isBlockByPolicy(NuguDirective* target_dir)
{
    DialogListMap::iterator iter = listmap.find(nugu_directive_peek_dialog_id(target_dir));
    if (iter == listmap.end())
        return false;

    for (auto& ndir : iter->second) {
        if (nugu_directive_get_blocking_medium(ndir)
            != nugu_directive_get_blocking_medium(target_dir))
            continue;

        if (nugu_directive_is_blocking(ndir) == 1)
            return true;
    }

    return false;
}

std::vector<NuguDirective*>* DialogDirectiveList::getList(const char* dialog_id)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return NULL;

    return &(iter->second);
}

void DialogDirectiveList::erase(const char* dialog_id)
{
    DialogListMap::iterator iter = listmap.find(dialog_id);
    if (iter == listmap.end())
        return;

    iter->second.clear();
    listmap.erase(dialog_id);
}

template <typename NOTIFY>
void DialogDirectiveList::clear(NOTIFY notify)
{
    for (auto& list : listmap) {
        nugu_dbg("\tDialog-id: '%s'", list.first.c_str());
        for (auto& ndir : list.second)
            notify(ndir);
    }

    listmap.clear();
}

NuguDirective* DialogDirectiveList::find(const char* name_space, const char* name)
{
    if (!name_space || !name)
        return NULL;

    dump();

    for (auto& list : listmap) {
        for (auto& ndir : list.second) {
            if (g_strcmp0(name_space, nugu_directive_peek_namespace(ndir)) != 0)
                continue;

            if (g_strcmp0(name, nugu_directive_peek_name(ndir)) == 0)
                return ndir;
        }
    }

    return NULL;
}

void DialogDirectiveList::dump()
{
    if (listmap.size() == 0) {
        nugu_dbg("-- %s: none", title.c_str());
        return;
    }

    nugu_dbg("-- %s items --", title.c_str());

    for (auto& list : listmap) {
        nugu_dbg("\tDialog-id: '%s'", list.first.c_str());
        for (auto& ndir : list.second) {
            nugu_dbg("\t\t'%s.%s' (%s) (%s/%s)",
                nugu_directive_peek_namespace(ndir),
                nugu_directive_peek_name(ndir),
                nugu_directive_peek_msg_id(ndir),
                nugu_directive_get_blocking_medium_string(ndir),
                (nugu_directive_is_blocking(ndir) == 1) ? "Block" : "Non-block");
        }
    }
}

/**
 * DirectiveSequencer
 */
DirectiveSequencer::DirectiveSequencer()
    : idler_src(0)
{
    msgid_lookup = new LookupTable("Msg-id Lookup table");
    pending = new DialogDirectiveList("PendingList");
    active = new DialogDirectiveList("ActiveList");

    last_cancel_dialog_id = "";

    nugu_network_manager_set_directive_callback(onDirective, this);
    nugu_network_manager_set_attachment_callback(onAttachment, this);
}

DirectiveSequencer::~DirectiveSequencer()
{
    nugu_network_manager_set_directive_callback(NULL, NULL);
    nugu_network_manager_set_attachment_callback(NULL, NULL);

    delete pending;
    delete active;

    msgid_lookup->clearWithUnref();
    delete msgid_lookup;
}

void DirectiveSequencer::reset()
{
    /* Cancel the pending idler */
    if (idler_src != 0)
        g_source_remove(idler_src);
    idler_src = 0;

    dump_policies(policy_map);
    pending->dump();
    active->dump();
    msgid_lookup->dump();

    policy_map.clear();
    scheduled_list.clear();
    msgid_lookup->clearWithUnref();
    pending->clear([](NuguDirective* ndir) {});
    active->clear([](NuguDirective* ndir) {});

    last_cancel_dialog_id = "";
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

    ndir = sequencer->msgid_lookup->find(parent_msg_id);
    if (!ndir)
        return;

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
    std::vector<NuguDirective*> list = sequencer->scheduled_list;

    sequencer->scheduled_list.clear();
    sequencer->idler_src = 0;

    nugu_dbg("idle callback: process next directives (%d)", list.size());

    for (auto& ndir : list)
        sequencer->handleDirective(ndir);

    return FALSE;
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

    nugu_info("process the directive '%s.%s'", name_space, name);
    nugu_directive_set_active(ndir, 1);

    active->dump();

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

    nugu_info("cancel the directive '%s.%s'", name_space, name);

    for (auto& listener : listeners_map[name_space]) {
        listener->onCancelDirective(ndir);
    }
}

void DirectiveSequencer::nextDirective(const std::string& dialog_id)
{
    nugu_dbg("search next pending item");

    std::vector<NuguDirective*>* list = pending->getList(dialog_id.c_str());
    if (list == NULL) {
        nugu_dbg("- no pending items");
        return;
    }

    pending->dump();

    for (auto& next_dir : *list) {
        nugu_dbg("- check '%s.%s' (%s) (%s/%s)",
            nugu_directive_peek_namespace(next_dir),
            nugu_directive_peek_name(next_dir),
            nugu_directive_peek_msg_id(next_dir),
            nugu_directive_get_blocking_medium_string(next_dir),
            (nugu_directive_is_blocking(next_dir) == 1) ? "Block" : "Non-block");

        if (active->findByMedium(dialog_id.c_str(), NUGU_DIRECTIVE_MEDIUM_ANY) != NULL) {
            nugu_dbg("  - Directive[ANY] is already in progress.");
            break;
        }

        if (active->isBlockByPolicy(next_dir) == true) {
            nugu_dbg("  - Same medium is already in progress.");
            continue;
        }

        if (nugu_directive_get_blocking_medium(next_dir) == NUGU_DIRECTIVE_MEDIUM_ANY) {
            if (active->isEmpty(dialog_id.c_str()) == false) {
                nugu_dbg("  - Another directive is already in progress.");
                break;
            }
        }

        nugu_dbg("  - Push to active + scheduled list");
        active->push(next_dir);

        /* Add next directive to scheduled list to handle at next idle time */
        std::vector<NuguDirective*>::iterator iter = std::find(scheduled_list.begin(), scheduled_list.end(), next_dir);
        if (iter == scheduled_list.end())
            scheduled_list.push_back(next_dir);
    }

    std::for_each(scheduled_list.begin(), scheduled_list.end(),
        [this](NuguDirective* scheduled_dir) {
            this->pending->remove(scheduled_dir);
        });

    nugu_dbg("- search done");

    if (idler_src == 0)
        idler_src = g_idle_add(onNext, this);
}

bool DirectiveSequencer::complete(NuguDirective* ndir)
{
    if (!ndir)
        return false;

    std::string dialog_id = nugu_directive_peek_dialog_id(ndir);
    std::vector<NuguDirective*>::iterator iter;

    nugu_info("complete '%s.%s' (medium: %s, isBlocking: %d)",
        nugu_directive_peek_namespace(ndir),
        nugu_directive_peek_name(ndir),
        nugu_directive_get_blocking_medium_string(ndir),
        nugu_directive_is_blocking(ndir));

    active->remove(ndir);
    active->dump();

    iter = std::find(scheduled_list.begin(), scheduled_list.end(), ndir);
    if (iter != scheduled_list.end()) {
        nugu_dbg("- remove from the scheduled list");
        scheduled_list.erase(iter);
    }

    msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
    nugu_directive_unref(ndir);

    nextDirective(dialog_id);

    return true;
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

    if (last_cancel_dialog_id == nugu_directive_peek_dialog_id(ndir)) {
        if (g_strcmp0(name_space, "System") == 0
            && g_strcmp0(name, "NoDirective") == 0) {
            nugu_info("allow System.NoDirective for canceled dialog");
        } else {
            nugu_info("ignore directive %s.%s (canceled dialog_id %s)",
                name_space, name, last_cancel_dialog_id.c_str());
            return false;
        }
    }

    assignPolicy(ndir);

    nugu_info("receive '%s.%s' directive (%s/%s)", name_space, name,
        nugu_directive_get_blocking_medium_string(ndir),
        (nugu_directive_is_blocking(ndir) == 1) ? "Block" : "Non-block");

    /**
     * Stop propagation if the directive was pre-processed.
     * Pre-handled directive must be dereferenced in corresponding listener.
     */
    if (preHandleDirective(ndir))
        return true;

    /**
     * Set the directive to the lookup-table to quickly find the corresponding
     * directive when receiving an attachment.
     */
    msgid_lookup->set(nugu_directive_peek_msg_id(ndir), ndir);

    active->dump();
    pending->dump();

    const char* dialog_id = nugu_directive_peek_dialog_id(ndir);

    if (active->findByMedium(dialog_id, NUGU_DIRECTIVE_MEDIUM_ANY) != NULL) {
        nugu_dbg("- Directive[ANY] is already in progress.");
        pending->push(ndir);
        pending->dump();
        return true;
    }

    if (pending->findByMedium(dialog_id, NUGU_DIRECTIVE_MEDIUM_ANY) != NULL) {
        nugu_dbg("- Directive[ANY] is already pending.");
        pending->push(ndir);
        pending->dump();
        return true;
    }

    if (active->isBlockByPolicy(ndir) == true) {
        nugu_dbg("- Same medium is already in progress.");
        pending->push(ndir);
        pending->dump();
        return true;
    }

    /* ANY medium can blocked by Audio/Visual/None/Any block policy */
    if (nugu_directive_get_blocking_medium(ndir) == NUGU_DIRECTIVE_MEDIUM_ANY) {
        if (active->isEmpty(dialog_id) == false) {
            nugu_dbg("- Another directive is already in progress.");
            pending->push(ndir);
            pending->dump();
            return true;
        }
    }

    active->push(ndir);
    handleDirective(ndir);

    return true;
}

bool DirectiveSequencer::cancel(const std::string& dialog_id, bool cancel_active_directive)
{
    nugu_info("cancel all directives for dialog_id(%s)", dialog_id.c_str());
    last_cancel_dialog_id = dialog_id;

    /* remove directive from scheduled list */
    auto new_end = std::remove_if(scheduled_list.begin(), scheduled_list.end(),
        [dialog_id](NuguDirective* ndir) {
            return (dialog_id.compare(nugu_directive_peek_dialog_id(ndir)) == 0);
        });
    scheduled_list.erase(new_end, scheduled_list.end());

    /* Remove from pending list with cancel notify */
    pending->remove(dialog_id, [=](NuguDirective* ndir) {
        msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
        cancelDirective(ndir);
        nugu_directive_unref(ndir);
    });
    pending->dump();

    if (cancel_active_directive) {
        /* Remove from active list with cancel notify */
        active->remove(dialog_id, [=](NuguDirective* ndir) {
            msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
            cancelDirective(ndir);
            nugu_directive_unref(ndir);
        });
        active->dump();
    }

    msgid_lookup->dump();

    return true;
}

bool DirectiveSequencer::cancel(const std::string& dialog_id, std::set<std::string> groups)
{
    nugu_info("cancel specific directives for dialog_id(%s)", dialog_id.c_str());
    for (const auto& name : groups) {
        nugu_dbg(" - %s", name.c_str());
    }

    pending->dump();
    active->dump();

    /* remove directive from scheduled list */
    auto new_end = std::remove_if(scheduled_list.begin(), scheduled_list.end(),
        [dialog_id, groups](NuguDirective* ndir) {
            if (dialog_id.compare(nugu_directive_peek_dialog_id(ndir)) != 0)
                return false;

            nugu_dbg("- found '%s.%s' (%s) from scheduled list",
                nugu_directive_peek_namespace(ndir),
                nugu_directive_peek_name(ndir),
                nugu_directive_peek_msg_id(ndir));

            for (const auto& name : groups) {
                std::string temp = nugu_directive_peek_namespace(ndir);
                temp.append(".").append(nugu_directive_peek_name(ndir));

                if (temp == name) {
                    nugu_dbg("  -> remove from scheduled list");
                    return true;
                }
            }

            return false;
        });
    scheduled_list.erase(new_end, scheduled_list.end());

    std::vector<NuguDirective*> remove_list;

    for (const auto& name : groups) {
        nugu_dbg("- find '%s' from active and pending list", name.c_str());

        /* Remove from active list with cancel notify */
        active->removeByName(dialog_id, name, [this, &remove_list](NuguDirective* ndir) {
            nugu_dbg("  - found '%s.%s' from active list",
                nugu_directive_peek_namespace(ndir), nugu_directive_peek_name(ndir));

            msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
            remove_list.push_back(ndir);
        });

        /* Remove from pending list with cancel notify */
        pending->removeByName(dialog_id, name, [this, &remove_list](NuguDirective* ndir) {
            nugu_dbg("  - found '%s.%s' from pending list",
                nugu_directive_peek_namespace(ndir), nugu_directive_peek_name(ndir));

            msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
            remove_list.push_back(ndir);
        });
    }

    for (auto& ndir : remove_list) {
        nugu_dbg("- cancel '%s.%s' directive", nugu_directive_peek_namespace(ndir),
            nugu_directive_peek_name(ndir));

        cancelDirective(ndir);

        nugu_dbg("- remove '%s.%s' directive", nugu_directive_peek_namespace(ndir),
            nugu_directive_peek_name(ndir));
        nugu_directive_unref(ndir);
    }

    pending->dump();
    active->dump();

    /* If the list of scheduled and active in dialog-id is empty, prepare the next pending directive */
    bool needNext = true;

    std::for_each(scheduled_list.begin(), scheduled_list.end(),
        [dialog_id, &needNext](NuguDirective* scheduled_dir) {
            if (dialog_id == nugu_directive_peek_dialog_id(scheduled_dir))
                needNext = false;
        });

    if (needNext == true && active->isEmpty(dialog_id.c_str()) == false)
        needNext = false;

    if (needNext)
        nextDirective(dialog_id);

    return true;
}

bool DirectiveSequencer::cancelAll(bool cancel_active_directive)
{
    nugu_info("cancel all directives");
    last_cancel_dialog_id = "";

    /* remove directive from scheduled list */
    scheduled_list.clear();

    /* Remove from pending list with cancel notify */
    pending->clear([=](NuguDirective* ndir) {
        msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
        cancelDirective(ndir);
        nugu_directive_unref(ndir);
    });

    pending->dump();

    if (cancel_active_directive) {
        /* Remove from active list with cancel notify */
        active->clear([=](NuguDirective* ndir) {
            msgid_lookup->remove(nugu_directive_peek_msg_id(ndir));
            cancelDirective(ndir);
            nugu_directive_unref(ndir);
        });
        active->dump();
    }

    msgid_lookup->dump();

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

void DirectiveSequencer::assignPolicy(NuguDirective* ndir)
{
    int is_block = 0;
    enum nugu_directive_medium medium = NUGU_DIRECTIVE_MEDIUM_NONE;
    BlockingPolicy policy = getPolicy(nugu_directive_peek_namespace(ndir),
        nugu_directive_peek_name(ndir));

    if (policy.isBlocking)
        is_block = 1;

    if (policy.medium == BlockingMedium::AUDIO)
        medium = NUGU_DIRECTIVE_MEDIUM_AUDIO;
    else if (policy.medium == BlockingMedium::VISUAL)
        medium = NUGU_DIRECTIVE_MEDIUM_VISUAL;
    else if (policy.medium == BlockingMedium::ANY)
        medium = NUGU_DIRECTIVE_MEDIUM_ANY;

    nugu_directive_set_blocking_policy(ndir, medium, is_block);
}

const std::string& DirectiveSequencer::getCanceledDialogId()
{
    return last_cancel_dialog_id;
}

const NuguDirective* DirectiveSequencer::findPending(const std::string& name_space, const std::string& name)
{
    nugu_dbg("find '%s.%s' from scheduled_list", name_space.c_str(), name.c_str());
    for (auto& ndir : scheduled_list) {
        if (g_strcmp0(name_space.c_str(), nugu_directive_peek_namespace(ndir)) != 0)
            continue;

        if (g_strcmp0(name.c_str(), nugu_directive_peek_name(ndir)) == 0)
            return ndir;
    }

    nugu_dbg("find '%s.%s' from pending", name_space.c_str(), name.c_str());
    return pending->find(name_space.c_str(), name.c_str());
}

}
