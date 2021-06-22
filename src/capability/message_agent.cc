/*
 * Copyright (c) 2021 SK Telecom Co., Ltd. All rights reserved.
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

#include "message_agent.hh"
#include "base/nugu_log.h"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "Message";
static const char* CAPABILITY_VERSION = "1.4";

MessageAgent::MessageAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
{
}

void MessageAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    tts_player = core_container->createTTSPlayer();
    tts_player->addListener(this);

    initialized = true;
}

void MessageAgent::deInitialize()
{
    if (tts_player) {
        tts_player->removeListener(this);
        delete tts_player;
        tts_player = nullptr;
    }

    initialized = false;
}

void MessageAgent::setCapabilityListener(ICapabilityListener* clistener)
{
    if (clistener)
        message_listener = dynamic_cast<IMessageListener*>(clistener);
}

void MessageAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value message;

    message["version"] = getVersion();
    message["readActivity"] = getCurrentTTSState();
    if (tts_token.size())
        message["token"] = tts_token;
    if (template_data.size())
        message["template"] = template_data;

    ctx[getName()] = message;
}

void MessageAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    if (!strcmp(dname, "SendCandidates"))
        parsingSendCandidates(message);
    else if (!strcmp(dname, "SendMessage"))
        parsingSendMessage(message);
    else if (!strcmp(dname, "GetMessage"))
        parsingGetMessage(message);
    else if (!strcmp(dname, "ReadMessage"))
        parsingReadMessage(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void MessageAgent::directiveDataCallback(NuguDirective* ndir, int seq, void* userdata)
{
    getAttachmentData(ndir, userdata);
}

void MessageAgent::getAttachmentData(NuguDirective* ndir, void* userdata)
{
    MessageAgent* agent = static_cast<MessageAgent*>(userdata);
    unsigned char* buf;
    size_t length = 0;

    buf = nugu_directive_get_data(ndir, &length);
    if (buf) {
        agent->tts_player->write_audio((const char*)buf, length);
        free(buf);
    }

    if (nugu_directive_is_data_end(ndir)) {
        agent->tts_player->write_done();
        agent->destroyDirective(ndir);
        agent->speak_dir = nullptr;
    }
}

void MessageAgent::stopTTS()
{
    if (speak_dir) {
        destroyDirective(speak_dir, false);
        speak_dir = nullptr;
    }

    tts_player->stop();
}

void MessageAgent::candidatesListed(const std::string& payload)
{
    // Todo: save template_data for context
    sendEvent("CandidatesListed", getContextInfo(), payload);
}

void MessageAgent::sendMessageSucceeded(const std::string& payload)
{
    sendEvent("SendMessageSucceeded", getContextInfo(), payload);
}

void MessageAgent::sendMessageFailed(const std::string& payload)
{
    sendEvent("SendMessageFailed", getContextInfo(), payload);
}

void MessageAgent::getMessageSucceeded(const std::string& payload)
{
    sendEvent("GetMessageSucceeded", getContextInfo(), payload);
}

void MessageAgent::getMessageFailed(const std::string& payload)
{
    sendEvent("GetMessageFailed", getContextInfo(), payload);
}

void MessageAgent::parsingSendCandidates(const char* message)
{
    if (message_listener)
        message_listener->processSendCandidataes(message);
}

void MessageAgent::parsingSendMessage(const char* message)
{
    if (message_listener)
        message_listener->processSendMessage(message);
}

void MessageAgent::parsingGetMessage(const char* message)
{
    if (message_listener)
        message_listener->processGetMessage(message);
}

void MessageAgent::parsingReadMessage(const char* message)
{
    Json::Value root;
    Json::Reader reader;

    std::string play_service_id;
    std::string received_time;
    std::string token;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    play_service_id = root["playServiceId"].asString();
    token = root["token"].asString();
    if (play_service_id.empty() || token.empty()) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    stopTTS();

    received_time = root["receivedTime"].asString();
    tts_token = token;

    destroy_directive_by_agent = true;
    speak_dir = nullptr;
    is_finished = false;
    tts_ps_id = play_service_id;

    if (focus_state == FocusState::FOREGROUND)
        executeOnForegroundAction();
    else
        focus_manager->requestFocus(INFO_FOCUS_TYPE, CAPABILITY_NAME, this);
}

void MessageAgent::sendEventReadMessageFinished(EventResultCallback cb)
{
    Json::Value root;
    Json::StyledWriter writer;

    std::string payload;

    root["playServiceId"] = tts_ps_id;
    root["token"] = tts_token;

    payload = writer.write(root);

    sendEvent("ReadMessageFinished", getContextInfo(), payload, std::move(cb));
}

void MessageAgent::mediaStateChanged(MediaPlayerState state)
{
    cur_state = state;
}

void MessageAgent::mediaEventReport(MediaPlayerEvent event)
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
        sendEventReadMessageFinished();
        focus_manager->releaseFocus(INFO_FOCUS_TYPE, CAPABILITY_NAME);
        break;
    default:
        break;
    }
}

void MessageAgent::mediaChanged(const std::string& url)
{
}

void MessageAgent::durationChanged(int duration)
{
}

void MessageAgent::positionChanged(int position)
{
}

void MessageAgent::volumeChanged(int volume)
{
}

void MessageAgent::muteChanged(int mute)
{
}

void MessageAgent::onFocusChanged(FocusState state)
{
    nugu_info("Focus Changed(%s -> %s)", focus_manager->getStateString(focus_state).c_str(), focus_manager->getStateString(state).c_str());

    switch (state) {
    case FocusState::FOREGROUND:
        executeOnForegroundAction();
        break;
    case FocusState::BACKGROUND:
        focus_manager->releaseFocus(INFO_FOCUS_TYPE, CAPABILITY_NAME);
        break;
    case FocusState::NONE:
        stopTTS();
        break;
    }

    focus_state = state;
}

void MessageAgent::executeOnForegroundAction()
{
    nugu_dbg("executeOnForegroundAction()");

    tts_player->play();

    if (speak_dir) {
        if (nugu_directive_get_data_size(speak_dir) > 0)
            getAttachmentData(speak_dir, this);

        nugu_directive_set_data_callback(speak_dir, directiveDataCallback, this);
    }
}

std::string MessageAgent::getCurrentTTSState()
{
    std::string tts_state;

    switch (cur_state) {
    case MediaPlayerState::IDLE:
        tts_state = "IDLE";
        break;
    case MediaPlayerState::STOPPED:
        tts_state = "STOPPED";
        break;
    case MediaPlayerState::READY:
    case MediaPlayerState::PLAYING:
        tts_state = "PLAYING";
        break;
    default:
        tts_state = "STOPPED";
        break;
    }

    if (is_finished)
        tts_state = "FINISHED";

    return tts_state;
}

} // NuguCapability