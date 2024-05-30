#include <glib.h>

#include <algorithm>
#include <iostream>

#include <base/nugu_log.h>
#include <capability/tts_interface.hh>
#include <clientkit/directive_sequencer_interface.hh>
#include <njson/njson.h>

#include "filter.h"

using namespace NuguClientKit;

struct filter {
    const char* search_text;
    const char* replace_tts;
};

static struct filter filters[] = {
    { "이거못하지", "할 수 있어요" },
    { "누구세요", "필터야" },
    { "노래들려줘", "아는 노래가 없어요" }
};

static char* filter_dialog_id;
static NuguClient* nugu_client = nullptr;

static void setFilterDialogId(const char* dialog_id)
{
    if (filter_dialog_id)
        g_free(filter_dialog_id);

    filter_dialog_id = g_strdup(dialog_id);
}

static bool isFilteredDialogId(const char* dialog_id)
{
    if (!filter_dialog_id)
        return false;

    if (g_strcmp0(dialog_id, filter_dialog_id) == 0)
        return true;

    return false;
}

static const char* getReplacedString(const char* text)
{
    size_t i;

    if (!text)
        return NULL;

    for (i = 0; i < sizeof(filters) / sizeof(struct filter); i++) {
        if (g_strcmp0(text, filters[i].search_text) == 0) {
            std::cout << "Filter: found '" << filters[i].search_text
                      << "', replace response to '" << filters[i].replace_tts
                      << "'" << std::endl;
            return filters[i].replace_tts;
        }
    }

    return NULL;
}

class DirListener : public IDirectiveSequencerListener {
public:
    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        /* Drop all directives corresponding to the dialog id specified by the filter. */
        if (isFilteredDialogId(nugu_directive_peek_dialog_id(ndir))) {
            std::cout << "Filtered: drop directive - "
                      << nugu_directive_peek_namespace(ndir) << ":"
                      << nugu_directive_peek_name(ndir) << std::endl;

            nugu_directive_unref(ndir);
            return true;
        }

        /* Check if the user's speech result matches the filter. */
        if (g_strcmp0(nugu_directive_peek_namespace(ndir), "ASR") != 0)
            return false;

        if (g_strcmp0(nugu_directive_peek_name(ndir), "NotifyResult") != 0)
            return false;

        NJson::Value root;
        NJson::Reader reader;

        if (!reader.parse(nugu_directive_peek_json(ndir), root)) {
            nugu_error("parsing error");
            return false;
        }

        /* Skip the partial result */
        if (root["state"].asString() != "COMPLETE")
            return false;

        /* remove all spaces */
        std::string buf = root["result"].asString();
        buf.erase(remove(buf.begin(), buf.end(), ' '), buf.end());

        const char* replace_text = getReplacedString(buf.c_str());
        if (replace_text == NULL)
            return false;

        setFilterDialogId(nugu_directive_peek_dialog_id(ndir));

        NuguCapability::ITTSHandler* handler = dynamic_cast<NuguCapability::ITTSHandler*>(nugu_client->getCapabilityHandler("TTS"));
        handler->requestTTS(replace_text, "");

        return false;
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        return true;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }
};

class NetworkListener : public INetworkManagerListener {
public:
    void onEventSend(NuguEvent* nev) override
    {
        /* Check if the text command matches the filter. */
        if (g_strcmp0(nugu_event_peek_namespace(nev), "Text") != 0)
            return;

        if (g_strcmp0(nugu_event_peek_name(nev), "TextInput") != 0)
            return;

        NJson::Value root;
        NJson::Reader reader;

        if (!reader.parse(nugu_event_peek_json(nev), root)) {
            nugu_error("parsing error");
            return;
        }

        const char* replace_text = getReplacedString(root["text"].asCString());
        if (replace_text == NULL)
            return;

        setFilterDialogId(nugu_event_peek_dialog_id(nev));

        NuguCapability::ITTSHandler* handler = dynamic_cast<NuguCapability::ITTSHandler*>(nugu_client->getCapabilityHandler("TTS"));
        handler->requestTTS(replace_text, "");
    }
};

static DirListener* dir_listener = nullptr;
static NetworkListener* net_listener = nullptr;

int filter_register(NuguClientKit::NuguClient* client)
{
    nugu_client = client;

    IDirectiveSequencer* sequencer = client->getNuguCoreContainer()->getCapabilityHelper()->getDirectiveSequencer();

    dir_listener = new DirListener();
    sequencer->addListener("ASR", dir_listener);
    sequencer->addListener("TTS", dir_listener);
    sequencer->addListener("Text", dir_listener);
    sequencer->addListener("AudioPlayer", dir_listener);
    sequencer->addListener("Display", dir_listener);

    net_listener = new NetworkListener();
    client->getNetworkManager()->addListener(net_listener);

    return 0;
}

void filter_remove(void)
{
    if (dir_listener) {
        delete dir_listener;
        dir_listener = nullptr;
    }

    if (net_listener) {
        delete net_listener;
        net_listener = nullptr;
    }

    if (filter_dialog_id) {
        g_free(filter_dialog_id);
        filter_dialog_id = NULL;
    }
}
