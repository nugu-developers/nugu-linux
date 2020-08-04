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
#include "base/nugu_prof.h"
#include "base/nugu_uuid.h"

#include "tts_agent.hh"

namespace NuguCapability {

#define TTS_FIRST_ATTACHMENT_LIMIT 4200

static const char* CAPABILITY_NAME = "TTS";
static const char* CAPABILITY_VERSION = "1.2";

TTSAgent::TTSAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , player(nullptr)
    , cur_state(MediaPlayerState::IDLE)
    , focus_state(FocusState::NONE)
    , cur_token("")
    , is_finished(false)
    , volume_update(false)
    , volume(-1)
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

    std::string volume_str;
    if (capa_helper->getCapabilityProperty("Speaker", "voice_command", volume_str))
        volume = std::stoi(volume_str);

    player = core_container->createTTSPlayer();
    player->addListener(this);
    player->setVolume(volume);

    addReferrerEvents("SpeechStarted", "Speak");
    addReferrerEvents("SpeechFinished", "Speak");
    addReferrerEvents("SpeechStopped", "Speak");
    addReferrerEvents("SpeechPlay", "TTSAgent.requestTTS");

    addBlockingPolicy("Speak", { BlockingMedium::AUDIO, true });

    initialized = true;
}

void TTSAgent::deInitialize()
{
    if (speak_dir) {
        nugu_directive_remove_data_callback(speak_dir);
        destroyDirective(speak_dir);
        speak_dir = NULL;
    }

    if (player) {
        player->removeListener(this);
        delete player;
    }

    initialized = false;
}

void TTSAgent::suspend()
{
    nugu_dbg("suspend_policy[%d], focus_state => %s", suspend_policy, focus_manager->getStateString(focus_state).c_str());

    // TODO: need to manage the context
    focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
}

void TTSAgent::directiveDataCallback(NuguDirective* ndir, int seq, void* userdata)
{
    getAttachmentData(ndir, seq, userdata);
}

void TTSAgent::getAttachmentData(NuguDirective* ndir, int seq, void* userdata)
{
    TTSAgent* tts = static_cast<TTSAgent*>(userdata);
    unsigned char* buf;
    size_t length = 0;

    buf = nugu_directive_get_data(ndir, &length);
    if (buf) {
        if (seq == 0 && length > TTS_FIRST_ATTACHMENT_LIMIT) {
            nugu_dbg("first attachment is too big(%d > %d)", length, TTS_FIRST_ATTACHMENT_LIMIT);
            tts->player->write_audio((const char*)buf, TTS_FIRST_ATTACHMENT_LIMIT);
            tts->player->write_audio((const char*)buf + TTS_FIRST_ATTACHMENT_LIMIT, length - TTS_FIRST_ATTACHMENT_LIMIT);
        } else {
            tts->player->write_audio((const char*)buf, length);
        }
        free(buf);
    }

    if (nugu_directive_is_data_end(ndir)) {
        tts->player->write_done();

        nugu_dbg("tts player state: %d", tts->player->state());
        if (tts->player->state() == MediaPlayerState::IDLE) {
            nugu_warn("TTS play fails, force release the focus");
            tts->focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
        }
    }
}

void TTSAgent::onFocusChanged(FocusState state)
{
    nugu_info("Focus Changed(%s -> %s)", focus_manager->getStateString(focus_state).c_str(), focus_manager->getStateString(state).c_str());

    switch (state) {
    case FocusState::FOREGROUND:
        executeOnForegroundAction();
        break;
    case FocusState::BACKGROUND:
        focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
        break;
    case FocusState::NONE:
        stopTTS();

        if (playstack_manager->getPlayStackLayer(playstackctl_ps_id) != PlayStackLayer::Media)
            playstack_manager->remove(playstackctl_ps_id);

        break;
    }
    focus_state = state;
}

void TTSAgent::executeOnForegroundAction()
{
    nugu_dbg("executeOnForegroundAction()");

    checkAndUpdateVolume();

    if (!player->play()) {
        nugu_error("play() failed");
        if (speak_dir) {
            nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_FAILED,
                nugu_directive_peek_dialog_id(speak_dir),
                nugu_directive_peek_msg_id(speak_dir), NULL);
        }
    }

    if (speak_dir) {
        if (nugu_directive_get_data_size(speak_dir) > 0)
            getAttachmentData(speak_dir, 0, this);

        nugu_directive_set_data_callback(speak_dir, directiveDataCallback, this);
    }
}

void TTSAgent::stopTTS()
{
    MediaPlayerState pre_state = cur_state;

    if (speak_dir) {
        nugu_directive_remove_data_callback(speak_dir);
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

    if (referrer_id.size())
        setReferrerDialogRequestId("TTSAgent.requestTTS", referrer_id);

    std::string id = sendEventSpeechPlay(token, text, play_service_id);
    nugu_dbg("user request dialog id: %s", id.c_str());

    setReferrerDialogRequestId("TTSAgent.requestTTS", "");
    return id;
}

bool TTSAgent::setVolume(int volume)
{
    nugu_dbg("set pcm player's volume: %d", volume);

    if (this->volume == volume)
        return true;

    this->volume = volume;
    volume_update = true;

    if (focus_state == FocusState::FOREGROUND)
        checkAndUpdateVolume();
    else
        nugu_dbg("Reserved to change pcm player's volume(%d)", volume);
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

    if (tts_engine.size())
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

    if (cur_token.size())
        tts["token"] = cur_token;

    ctx[getName()] = tts;
}

void TTSAgent::sendEventSpeechStarted(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size()) {
        CapabilityEvent event("SpeechStarted", this);

        sendEventCommon(&event, token, std::move(cb));

        nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_STARTED,
            event.getDialogRequestId().c_str(),
            event.getMessageId().c_str(), NULL);
    }

    if (tts_listener)
        tts_listener->onTTSState(TTSState::TTS_SPEECH_START, dialog_id);
}

void TTSAgent::sendEventSpeechFinished(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size()) {
        CapabilityEvent event("SpeechFinished", this);

        sendEventCommon(&event, token, std::move(cb));

        nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_FINISHED,
            event.getDialogRequestId().c_str(),
            event.getMessageId().c_str(), NULL);
    }

    if (tts_listener)
        tts_listener->onTTSState(TTSState::TTS_SPEECH_FINISH, dialog_id);
}

void TTSAgent::sendEventSpeechStopped(const std::string& token, EventResultCallback cb)
{
    if (ps_id.size()) {
        CapabilityEvent event("SpeechStopped", this);

        sendEventCommon(&event, token, std::move(cb));

        nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_STOPPED,
            event.getDialogRequestId().c_str(),
            event.getMessageId().c_str(), NULL);
    }
}

std::string TTSAgent::sendEventSpeechPlay(const std::string& token, const std::string& text, const std::string& play_service_id, EventResultCallback cb)
{
    std::string ename = "SpeechPlay";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    std::string skml = text;

    if (text.size() == 0 || token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

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

void TTSAgent::sendEventCommon(CapabilityEvent* event, const std::string& token, EventResultCallback cb)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;

    if (!event) {
        nugu_error("event is NULL");
        return;
    }

    if (token.size() == 0) {
        nugu_error("there is something wrong [%s]", event->getName().c_str());
        return;
    }

    root["token"] = token;
    if (ps_id.size())
        root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(event, getContextInfo(), payload, std::move(cb));
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
    speak_dir = nullptr;

    // set referrer id previous dialog_id
    std::string dname = nugu_directive_peek_name(getNuguDirective());
    if (dialog_id.size())
        setReferrerDialogRequestId(dname, dialog_id);

    stopTTS();

    cur_token = token;
    ps_id = play_service_id;

    is_finished = false;
    speak_dir = getNuguDirective();
    dialog_id = nugu_directive_peek_dialog_id(speak_dir);

    // set referrer id new dialog_id
    setReferrerDialogRequestId(dname, dialog_id);

    std::string tmp_playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);
    playstack_manager->add(tmp_playstackctl_ps_id, speak_dir);

    // It need to preserve previous playstack_ps_id, if a new received playstack_ps_id is not exist.
    if (!tmp_playstackctl_ps_id.empty())
        playstackctl_ps_id = tmp_playstackctl_ps_id;

    nugu_prof_mark_data(NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE, dialog_id.c_str(),
        nugu_directive_peek_msg_id(speak_dir), NULL);

    if (focus_state == FocusState::FOREGROUND)
        executeOnForegroundAction();
    else
        focus_manager->requestFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME, this);

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

    if (!root["playServiceId"].empty())
        ps_id = root["playServiceId"].asString();

    focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
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
        focus_manager->releaseFocus(DIALOG_FOCUS_TYPE, CAPABILITY_NAME);
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

void TTSAgent::checkAndUpdateVolume()
{
    if (volume_update && player) {
        nugu_dbg("pcm player's volume(%d) is changed", volume);
        player->setVolume(volume);
        volume_update = false;
    }
}

} // NuguCapability
