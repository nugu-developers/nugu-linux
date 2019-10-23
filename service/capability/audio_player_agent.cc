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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "audio_player_agent.hh"
#include "media_player.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

AudioPlayerAgent::AudioPlayerAgent()
    : Capability(CapabilityType::AudioPlayer, capability_version)
    , player(NULL)
    , cur_aplayer_state(AudioPlayerState::IDLE)
    , prev_aplayer_state(AudioPlayerState::IDLE)
    , ps_id("")
    , report_delay_time(-1)
    , report_interval_time(-1)
    , cur_token("")
    , is_finished(false)
{
}

AudioPlayerAgent::~AudioPlayerAgent()
{
    CapabilityManager::getInstance()->removeFocus("cap_audio");
    delete player;
}

void AudioPlayerAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    player = dynamic_cast<IMediaPlayer*>(new MediaPlayer());
    player->addListener(this);

    CapabilityManager::getInstance()->addFocus("cap_audio", NUGU_FOCUS_TYPE_MEDIA, this);

    initialized = true;
}

NuguFocusResult AudioPlayerAgent::onFocus(NuguFocusResource rsrc, void* event)
{
    if (is_paused)
        return NUGU_FOCUS_OK;

    if (!player->play()) {
        nugu_error("play media failed");
        sendEventPlaybackError(AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
            "player can't play");
        return NUGU_FOCUS_FAIL;
    }

    return NUGU_FOCUS_OK;
}

NuguFocusResult AudioPlayerAgent::onUnfocus(NuguFocusResource rsrc, void* event)
{
    if (is_finished)
        return NUGU_FOCUS_REMOVE;

    if (player->state() == MediaPlayerState::STOPPED || player->state() == MediaPlayerState::PAUSED)
        return NUGU_FOCUS_REMOVE;

    if (!player->pause()) {
        nugu_error("pause media failed");
        sendEventPlaybackError(AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
            "player can't pause");
        return NUGU_FOCUS_REMOVE;
    }

    return NUGU_FOCUS_PAUSE;
}

NuguFocusStealResult AudioPlayerAgent::onStealRequest(NuguFocusResource rsrc, void* event, NuguFocusType target_type)
{
    return NUGU_FOCUS_STEAL_ALLOW;
}

void AudioPlayerAgent::play()
{
    sendEventByDisplayInterface("PlayCommandIssued");
}

void AudioPlayerAgent::stop()
{
    sendEventByDisplayInterface("StopCommandIssued");
}

void AudioPlayerAgent::next()
{
    sendEventByDisplayInterface("NextCommandIssued");
}

void AudioPlayerAgent::prev()
{
    sendEventByDisplayInterface("PreviousCommandIssued");
}

void AudioPlayerAgent::pause()
{
    sendEventByDisplayInterface("PauseCommandIssued");
}

void AudioPlayerAgent::resume()
{
    sendEventByDisplayInterface("PlayCommandIssued");
}

void AudioPlayerAgent::seek(int msec)
{
    if (player)
        player->seek(msec * 1000);
}

void AudioPlayerAgent::processDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    message = nugu_directive_peek_json(ndir);
    dname = nugu_directive_peek_name(ndir);

    if (!message || !dname) {
        nugu_error("directive message is not correct");
        destoryDirective(ndir);
        return;
    }

    nugu_dbg("message: %s", message);

    is_paused = strcmp(dname, "Pause") == 0;

    // directive name check
    if (!strcmp(dname, "Play"))
        parsingPlay(message);
    else if (!strcmp(dname, "Pause"))
        parsingPause(message);
    else if (!strcmp(dname, "Stop")) {
        parsingStop(message);
    } else {
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(),
            getVersion().c_str(), dname);
    }

    destoryDirective(ndir);
}

void AudioPlayerAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value aplayer;
    double offset = player->position() * 1000;
    double duration = player->duration() * 1000;

    aplayer["version"] = getVersion();
    aplayer["playerActivity"] = playerActivity(cur_aplayer_state);
    aplayer["offsetInMilliseconds"] = offset;
    if (cur_aplayer_state != AudioPlayerState::IDLE) {
        aplayer["token"] = cur_token;
        if (duration)
            aplayer["durationInMilliseconds"] = duration;
    }

    ctx[getName()] = aplayer;
}

std::string AudioPlayerAgent::getContextInfo()
{
    Json::Value ctx;
    CapabilityManager* cmanager = CapabilityManager::getInstance();

    updateInfoForContext(ctx);
    return cmanager->makeContextInfo(ctx);
}

void AudioPlayerAgent::receiveCommand(CapabilityType from, std::string command, std::string param)
{
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (!command.compare("setvolume")) {
        player->setVolume(atoi(param.c_str()));
    }
}

void AudioPlayerAgent::receiveCommandAll(std::string command, std::string param)
{
}

void AudioPlayerAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener)
        addListener(dynamic_cast<IAudioPlayerListener*>(listener));
}

void AudioPlayerAgent::addListener(IAudioPlayerListener* listener)
{
    auto iterator = std::find(aplayer_listeners.begin(), aplayer_listeners.end(), listener);

    if (iterator == aplayer_listeners.end())
        aplayer_listeners.push_back(listener);
}

void AudioPlayerAgent::removeListener(IAudioPlayerListener* listener)
{
    auto iterator = std::find(aplayer_listeners.begin(), aplayer_listeners.end(), listener);

    if (iterator != aplayer_listeners.end())
        aplayer_listeners.erase(iterator);
}

void AudioPlayerAgent::sendEventPlaybackStarted()
{
    sendEventCommon("PlaybackStarted");
}

void AudioPlayerAgent::sendEventPlaybackFinished()
{
    sendEventCommon("PlaybackFinished");
}

void AudioPlayerAgent::sendEventPlaybackStopped()
{
    sendEventCommon("PlaybackStopped");
}

void AudioPlayerAgent::sendEventPlaybackPaused()
{
    sendEventCommon("PlaybackPaused");
}

void AudioPlayerAgent::sendEventPlaybackResumed()
{
    sendEventCommon("PlaybackResumed");
}

void AudioPlayerAgent::sendEventPlaybackError(PlaybackError err, std::string reason)
{
    Json::StyledWriter writer;
    Json::Value root;
    NuguEvent* event;
    std::string event_json;
    long offset = player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong ");
        return;
    }

    event = nugu_event_new(getName().c_str(), "PlaybackFailed",
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["error.type"] = playbackError(err);
    root["error.message"] = reason;
    root["currentPlaybackState.token"] = cur_token;
    root["currentPlaybackState.offsetInMilliseconds"] = std::to_string(offset);
    root["currentPlaybackState.playActivity"] = playerActivity(cur_aplayer_state);
    event_json = writer.write(root);

    nugu_event_set_json(event, event_json.c_str());

    sendEvent(event);

    nugu_event_free(event);
}

void AudioPlayerAgent::sendEventProgressReportDelayElapsed()
{
    nugu_dbg("report_delay_time: %d, position: %d", report_delay_time / 1000,
        player->position());
    sendEventCommon("ProgressReportDelayElapsed");
}

void AudioPlayerAgent::sendEventProgressReportIntervalElapsed()
{
    nugu_dbg("report_interval_time: %d, position: %d",
        report_interval_time / 1000, player->position());
    sendEventCommon("ProgressReportIntervalElapsed");
}

void AudioPlayerAgent::sendEventByDisplayInterface(std::string command)
{
    sendEventCommon(command);
}

void AudioPlayerAgent::sendEventCommon(std::string ename)
{
    Json::StyledWriter writer;
    Json::Value root;
    NuguEvent* event;
    std::string event_json;
    long offset = player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    event = nugu_event_new(getName().c_str(), ename.c_str(),
        getVersion().c_str());

    nugu_event_set_context(event, getContextInfo().c_str());

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    event_json = writer.write(root);

    nugu_event_set_json(event, event_json.c_str());

    sendEvent(event);

    nugu_event_free(event);
}

void AudioPlayerAgent::parsingPlay(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Value stream;
    Json::Value report;
    Json::Value meta;
    Json::Reader reader;
    std::string url;
    long offset;
    std::string token;
    std::string prev_token;
    std::string temp;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    audio_item = root["audioItem"];
    if (audio_item.empty()) {
        nugu_error("directive message syntex error");
        return;
    }
    ps_id = root["playServiceId"].asString();

    stream = audio_item["stream"];
    if (stream.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    url = stream["url"].asString();
    offset = stream["offsetInMilliseconds"].asLargestInt();
    token = stream["token"].asString();
    prev_token = stream["expectedPreviousToken"].asString();

    if (url.size() == 0 || token.size() == 0 || ps_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    report = stream["progressReport"];
    if (!report.empty()) {
        report_delay_time = report["progressReportDelayInMilliseconds"]
                                .asLargestInt();
        report_interval_time = report["progressReportIntervalInMilliseconds"]
                                   .asLargestInt();
    }

    PlaySyncManager::DisplayRenderer renderer;
    meta = audio_item["metadata"];

    if (!meta.empty() && !meta["template"].empty()) {
        Json::StyledWriter writer;
        renderer.cap_type = getType();
        renderer.listener = this;
        renderer.render_info = std::make_pair<std::string, std::string>("AudioPlayer", writer.write(meta["template"]));
    }

    // sync display rendering with context
    playsync_manager->addContext(ps_id, getType(), renderer);

    is_finished = false;

    nugu_dbg("= token: %s", token.c_str());
    nugu_dbg("= url: %s", url.c_str());
    nugu_dbg("= offset: %d", offset);
    nugu_dbg("= report_delay_time: %d", report_delay_time);
    nugu_dbg("= report_interval_time: %d", report_interval_time);

    // stop previous play for handling prev, next media play
    if (token != prev_token) {
        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackError(
                AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't stop");
        }
    }
    cur_token = token;

    if (!player->setSource(url)) {
        nugu_error("set source failed");
        sendEventPlaybackError(
            AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
            "can't set source");
        return;
    }

    if (offset >= 0 && !player->seek(offset / 1000)) {
        nugu_error("seek media failed");
        sendEventPlaybackError(
            AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
            "can't seek");
        return;
    }

    CapabilityManager::getInstance()->requestFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK, NULL);
}

void AudioPlayerAgent::parsingPause(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Reader reader;
    std::string playserviceid;
    std::string active_playserviceid;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playserviceid = root["playServiceId"].asString();

    if (playserviceid.size()) {
        CapabilityManager::getInstance()->sendCommand(CapabilityType::AudioPlayer, CapabilityType::ASR, "releaseFocus", "");

        if (!player->pause()) {
            nugu_error("pause media failed");
            sendEventPlaybackError(
                AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't pause");
        }

        CapabilityManager::getInstance()->releaseFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK);
    }
}

void AudioPlayerAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Reader reader;
    std::string playserviceid;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playserviceid = root["playServiceId"].asString();

    if (playserviceid.size()) {
        playsync_manager->removeContext(playserviceid, getType(), !is_finished);
        std::string stacked_ps_id = playsync_manager->getPlayStackItem(getType());

        CapabilityManager::getInstance()->sendCommand(CapabilityType::AudioPlayer, CapabilityType::ASR, "releaseFocus", "");

        if (!stacked_ps_id.empty() && playserviceid != stacked_ps_id)
            return;
        else if (stacked_ps_id.empty())
            CapabilityManager::getInstance()->releaseFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK);

        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackError(
                AudioPlayerAgent::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't stop");
        }
    }
}

std::string AudioPlayerAgent::playbackError(PlaybackError error)
{
    std::string err_str;

    switch (error) {
    case MEDIA_ERROR_INVALID_REQUEST:
        err_str = "MEDIA_ERROR_INVALID_REQUEST";
        break;
    case MEDIA_ERROR_SERVICE_UNAVAILABLE:
        err_str = "MEDIA_ERROR_SERVICE_UNAVAILABLE";
        break;
    case MEDIA_ERROR_INTERNAL_SERVER_ERROR:
        err_str = "MEDIA_ERROR_INTERNAL_SERVER_ERROR";
        break;
    case MEDIA_ERROR_INTERNAL_DEVICE_ERROR:
        err_str = "MEDIA_ERROR_INTERNAL_DEVICE_ERROR";
        break;
    default:
        err_str = "MEDIA_ERROR_UNKNOWN";
        break;
    }
    return err_str;
}

std::string AudioPlayerAgent::playerActivity(AudioPlayerState state)
{
    std::string activity;

    switch (state) {
    case AudioPlayerState::IDLE:
        activity = "IDLE";
        break;
    case AudioPlayerState::PLAYING:
        activity = "PLAYING";
        break;
    case AudioPlayerState::PAUSED:
        activity = "PAUSED";
        break;
    case AudioPlayerState::STOPPED:
        activity = "STOPPED";
        break;
    case AudioPlayerState::FINISHED:
        activity = "FINISHED";
        break;
    }

    return activity;
}

void AudioPlayerAgent::mediaStateChanged(MediaPlayerState state)
{
    switch (state) {
    case MediaPlayerState::IDLE:
        cur_aplayer_state = AudioPlayerState::IDLE;
        break;
    case MediaPlayerState::PREPARE:
    case MediaPlayerState::READY:
        break;
    case MediaPlayerState::PLAYING:
        if (prev_aplayer_state == AudioPlayerState::PAUSED)
            sendEventPlaybackResumed();
        else
            sendEventPlaybackStarted();
        cur_aplayer_state = AudioPlayerState::PLAYING;
        break;
    case MediaPlayerState::PAUSED:
        sendEventPlaybackPaused();
        cur_aplayer_state = AudioPlayerState::PAUSED;
        break;
    case MediaPlayerState::STOPPED:
        CapabilityManager::getInstance()->releaseFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK);
        if (is_finished) {
            sendEventPlaybackFinished();
            cur_aplayer_state = AudioPlayerState::FINISHED;
        } else {
            sendEventPlaybackStopped();
            cur_aplayer_state = AudioPlayerState::STOPPED;
        }
        break;
    default:
        break;
    }
    nugu_info("[audioplayer state] %s -> %s", playerActivity(prev_aplayer_state).c_str(), playerActivity(cur_aplayer_state).c_str());

    if (prev_aplayer_state == cur_aplayer_state)
        return;

    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->mediaStateChanged(cur_aplayer_state);

    prev_aplayer_state = cur_aplayer_state;
}

void AudioPlayerAgent::mediaEventReport(MediaPlayerEvent event)
{
    switch (event) {
    case MediaPlayerEvent::INVALID_MEDIA:
        nugu_warn("INVALID_MEDIA");
        sendEventPlaybackError(AudioPlayerAgent::MEDIA_ERROR_INVALID_REQUEST,
            "media source is wrong");
        break;
    case MediaPlayerEvent::LOADING_MEDIA:
        nugu_warn("LOADING_MEDIA");
        sendEventPlaybackError(
            AudioPlayerAgent::MEDIA_ERROR_SERVICE_UNAVAILABLE,
            "player can't play the media");
        break;
    default:
        break;
    }
}

void AudioPlayerAgent::mediaFinished()
{
    is_finished = true;
    mediaStateChanged(MediaPlayerState::STOPPED);
}

void AudioPlayerAgent::mediaLoaded()
{
}

void AudioPlayerAgent::mediaChanged(std::string url)
{
}

void AudioPlayerAgent::durationChanged(int duration)
{
    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->durationChanged(duration);
}

void AudioPlayerAgent::positionChanged(int position)
{
    if (report_delay_time > 0 && ((report_delay_time / 1000) == position))
        sendEventProgressReportDelayElapsed();

    if (report_interval_time > 0 && ((position % (int)(report_interval_time / 1000)) == 0))
        sendEventProgressReportIntervalElapsed();

    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->positionChanged(position);
}

void AudioPlayerAgent::volumeChanged(int volume)
{
}

void AudioPlayerAgent::muteChanged(int mute)
{
}

void AudioPlayerAgent::onSyncContext(std::string ps_id, std::pair<std::string, std::string> render_info)
{
    nugu_dbg("AudioPlayer sync context");

    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->renderDisplay(render_info.first, render_info.second);
}

void AudioPlayerAgent::onReleaseContext(std::string ps_id)
{
    nugu_dbg("AudioPlayer release context");

    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->clearDisplay();
}

} // NuguCore
