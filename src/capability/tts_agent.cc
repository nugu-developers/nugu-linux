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

#include <string.h>

#include "base/nugu_log.h"
#include "base/nugu_uuid.h"

#include "tts_agent.hh"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "TTS";
static const char* CAPABILITY_VERSION = "1.0";

TTSAgent::TTSAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , player(nullptr)
    , cur_state(MediaPlayerState::IDLE)
    , cur_token("")
    , is_finished(false)
    , speak_dir(nullptr)
    , dialog_id("")
    , ps_id("")
    , tts_listener(nullptr)
    , tts_engine(NUGU_TTS_ENGINE)
{
}

TTSAgent::~TTSAgent()
{
}

void TTSAgent::setAttribute(TTSAttribute&& attribute)
{
    if (!attribute.tts_engine.empty())
        tts_engine = attribute.tts_engine;
}

void TTSAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    player = core_container->createTTSPlayer();
    player->addListener(this);

    capa_helper->addFocus("cap_tts", NUGU_FOCUS_TYPE_TTS, this);

    addReferrerEvents("SpeechStarted", "Speak");
    addReferrerEvents("SpeechFinished", "Speak");
    addReferrerEvents("SpeechStopped", "Speak");
    addReferrerEvents("SpeechPlay", "TTSAgent.requestTTS");

    initialized = true;
}

void TTSAgent::deInitialize()
{
    if (speak_dir) {
        nugu_directive_set_data_callback(speak_dir, NULL, NULL);
        destroyDirective(speak_dir);
        speak_dir = NULL;
    }

    if (player) {
        player->removeListener(this);
        delete player;
    }

    initialized = false;
    capa_helper->removeFocus("cap_tts");
}

void TTSAgent::suspend()
{
    if (cur_state == MediaPlayerState::PLAYING)
        capa_helper->releaseFocus("cap_tts");
}

void TTSAgent::directiveDataCallback(NuguDirective* ndir, void* userdata)
{
    getAttachmentData(ndir, userdata);
}

void TTSAgent::getAttachmentData(NuguDirective* ndir, void* userdata)
{
    TTSAgent* tts = static_cast<TTSAgent*>(userdata);
    unsigned char* buf;
    size_t length = 0;

    buf = nugu_directive_get_data(ndir, &length);
    if (buf) {
        tts->player->write_audio((const char*)buf, length);
        free(buf);
    }

    if (nugu_directive_is_data_end(ndir)) {
        tts->player->write_done();
        tts->destroyDirective(ndir);
        tts->speak_dir = NULL;
    }
}

void TTSAgent::onFocus(void* event)
{
    player->play();

    if (nugu_directive_get_data_size(speak_dir) > 0)
        getAttachmentData(speak_dir, this);

    if (speak_dir)
        nugu_directive_set_data_callback(speak_dir, directiveDataCallback, this);
}

NuguFocusResult TTSAgent::onUnfocus(void* event, NuguUnFocusMode mode)
{
    MediaPlayerState pre_state = cur_state;

    player->stop();

    playsync_manager->removeContext(playstackctl_ps_id, getName(), !is_finished);

    if (tts_listener && pre_state != MediaPlayerState::STOPPED)
        tts_listener->onTTSCancel(dialog_id);

    return NUGU_FOCUS_REMOVE;
}

NuguFocusStealResult TTSAgent::onStealRequest(void* event, NuguFocusType target_type)
{
    if (target_type <= NUGU_FOCUS_TYPE_TTS)
        return NUGU_FOCUS_STEAL_ALLOW;
    else
        return NUGU_FOCUS_STEAL_REJECT;
}

void TTSAgent::stopTTS()
{
    MediaPlayerState pre_state = cur_state;

    if (speak_dir) {
        nugu_directive_set_data_callback(speak_dir, NULL, NULL);
        destroyDirective(speak_dir);
        speak_dir = NULL;
    }

    if (player)
        player->stop();

    if (tts_listener && pre_state != MediaPlayerState::STOPPED && pre_state != MediaPlayerState::IDLE)
        tts_listener->onTTSCancel(dialog_id);
}

std::string TTSAgent::requestTTS(const std::string& text, const std::string& play_service_id, const std::string& referrer_id)
{
    std::string token;
    char* uuid;

    uuid = nugu_uuid_generate_time();
    token = uuid;
    free(uuid);

    if(referrer_id.size())
        setReferrerDialogRequestId("TTSAgent.requestTTS", referrer_id);

    std::string id = sendEventSpeechPlay(token, text, play_service_id);
    nugu_dbg("user request dialog id: %s", id.c_str());

    setReferrerDialogRequestId("TTSAgent.requestTTS", "");
    return id;
}

bool TTSAgent::setVolume(int volume)
{
    nugu_dbg("set pcm player's volume: %d", volume);

    if (!player || player->setVolume(volume) != 0)
        return false;

    nugu_dbg("pcm player's volume(%d) changed..", volume);
    return true;
}

void TTSAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    // directive name check
    if (!strcmp(dname, "Speak")) {
        parsingSpeak(message);
    } else if (!strcmp(dname, "Stop")) {
        parsingStop(message);
    } else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
    }
}

void TTSAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value tts;

    tts["engine"] = tts_engine;
    tts["version"] = getVersion();
    switch (cur_state) {
    case MediaPlayerState::IDLE:
        tts["ttsActivity"] = "IDLE";
        break;
    case MediaPlayerState::STOPPED:
        tts["ttsActivity"] = "STOPPED";
        break;
    case MediaPlayerState::READY:
    case MediaPlayerState::PLAYING:
        tts["ttsActivity"] = "PLAYING";
        break;
    default:
        tts["ttsActivity"] = "STOPPED";
        break;
    }

    if (is_finished)
        tts["ttsActivity"] = "FINISHED";

    ctx[getName()] = tts;
}

void TTSAgent::sendEventSpeechStarted(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size())
        sendEventCommon("SpeechStarted", token, std::move(cb));

    if (tts_listener)
        tts_listener->onTTSState(TTSState::TTS_SPEECH_START, dialog_id);
}

void TTSAgent::sendEventSpeechFinished(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size())
        sendEventCommon("SpeechFinished", token, std::move(cb));

    if (tts_listener)
        tts_listener->onTTSState(TTSState::TTS_SPEECH_FINISH, dialog_id);
}

void TTSAgent::sendEventSpeechStopped(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size())
        sendEventCommon("SpeechStopped", token, std::move(cb));
}

std::string TTSAgent::sendEventSpeechPlay(const std::string& token, const std::string& text, const std::string& play_service_id, EventResultCallback cb)
{
    std::string ename = "SpeechPlay";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    std::string skml = text;

    if (text.size() == 0)
        return "";

    if (text.find("<skml", 0) == std::string::npos)
        skml = "<skml domain=\"general\">" + text + "</skml>";

    root["format"] = "SKML";
    root["text"] = skml;
    root["token"] = token;
    if (play_service_id.size())
        root["playServiceId"] = play_service_id;
    else
        root["playServiceId"] = ps_id;
    payload = writer.write(root);

    return sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void TTSAgent::sendEventCommon(const std::string& ename, const std::string& token, EventResultCallback cb)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    root["token"] = token;
    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void TTSAgent::parsingSpeak(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string format;
    std::string text;
    std::string token;
    std::string play_service_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    format = root["format"].asString();
    text = root["text"].asString();
    token = root["token"].asString();
    play_service_id = root["playServiceId"].asString();

    if (format.size() == 0 || text.size() == 0 || token.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    has_attachment = true;

    stopTTS();

    playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        playsync_manager->addContext(playstackctl_ps_id, getName());
    }

    cur_token = token;
    dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
    ps_id = play_service_id;

    is_finished = false;
    speak_dir = getNuguDirective();

    capa_helper->requestFocus("cap_tts", NULL);

    if (tts_listener)
        tts_listener->onTTSText(text, dialog_id);
}

void TTSAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string token;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    token = root["token"].asString();
    ps_id = root["playServiceId"].asString();

    if (token.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (cur_token.compare(token)) {
        nugu_error("the token(%s) is not valid", token.c_str());
        return;
    }

    stopTTS();
}

void TTSAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        tts_listener = dynamic_cast<ITTSListener*>(clistener);
}

void TTSAgent::mediaStateChanged(MediaPlayerState state)
{
    cur_state = state;

    switch (state) {
    case MediaPlayerState::PLAYING:
        sendEventSpeechStarted(cur_token);
        break;
    case MediaPlayerState::STOPPED:
        if (is_finished)
            sendEventSpeechFinished(cur_token);
        else
            sendEventSpeechStopped(cur_token);
        break;
    default:
        break;
    }
}

void TTSAgent::mediaEventReport(MediaPlayerEvent event)
{
    switch (event) {
    case MediaPlayerEvent::LOADING_MEDIA_FAILED:
        nugu_warn("LOADING_MEDIA_FAILED");
        break;
    case MediaPlayerEvent::LOADING_MEDIA_SUCCESS:
        nugu_dbg("LOADING_MEDIA_SUCCESS");
        break;
    case MediaPlayerEvent::PLAYING_MEDIA_FINISHED:
        nugu_dbg("PLAYING_MEDIA_FINISHED");
        is_finished = true;
        mediaStateChanged(MediaPlayerState::STOPPED);
        capa_helper->releaseFocus("cap_tts");
        break;
    default:
        break;
    }
}

void TTSAgent::mediaChanged(const std::string& url)
{
}

void TTSAgent::durationChanged(int duration)
{
}

void TTSAgent::positionChanged(int position)
{
}

void TTSAgent::volumeChanged(int volume)
{
}

void TTSAgent::muteChanged(int mute)
{
}

} // NuguCapability
