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
#include <sys/time.h>
#include <unistd.h>

#include "audio_player_agent.hh"
#include "media_player.hh"
#include "nugu_log.h"

namespace NuguCore {

static const std::string capability_version = "1.0";

AudioPlayerAgent::AudioPlayerAgent()
    : Capability(CapabilityType::AudioPlayer, capability_version)
    , player(nullptr)
    , cur_aplayer_state(AudioPlayerState::IDLE)
    , prev_aplayer_state(AudioPlayerState::IDLE)
    , is_paused(false)
    , ps_id("")
    , report_delay_time(-1)
    , report_interval_time(-1)
    , cur_token("")
    , pre_ref_dialog_id("")
    , is_finished(false)
    , display_listener(nullptr)
{
}

AudioPlayerAgent::~AudioPlayerAgent()
{
    display_listener = nullptr;
    aplayer_listeners.clear();

    for (auto info : render_info) {
        delete info.second;
    }
    render_info.clear();

    CapabilityManager::getInstance()->removeFocus("cap_audio");

    if (player) {
        player->removeListener(this);
        delete player;
        player = nullptr;
    }
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
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
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
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
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

void AudioPlayerAgent::parsingDirective(const char* dname, const char* message)
{
    nugu_dbg("message: %s", message);

    is_paused = strcmp(dname, "Pause") == 0;

    // directive name check
    if (!strcmp(dname, "Play"))
        parsingPlay(message);
    else if (!strcmp(dname, "Pause"))
        parsingPause(message);
    else if (!strcmp(dname, "Stop"))
        parsingStop(message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
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

void AudioPlayerAgent::receiveCommand(CapabilityType from, std::string command, const std::string& param)
{
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (!command.compare("setvolume"))
        player->setVolume(atoi(param.c_str()));
}

void AudioPlayerAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener) {
        setListener(dynamic_cast<IDisplayListener*>(listener));
        addListener(dynamic_cast<IAudioPlayerListener*>(listener));
    }
}

void AudioPlayerAgent::displayRendered(const std::string& id)
{
}

void AudioPlayerAgent::displayCleared(const std::string& id)
{
    std::string ps_id = "";

    if (render_info.find(id) != render_info.end()) {
        auto info = render_info[id];
        ps_id = info->ps_id;
        render_info.erase(id);
        delete info;
    }
    playsync_manager->clearPendingContext(ps_id);
}

void AudioPlayerAgent::elementSelected(const std::string& id, const std::string& item_token)
{
}

void AudioPlayerAgent::setListener(IDisplayListener* listener)
{
    if (!listener)
        return;

    display_listener = listener;
}

void AudioPlayerAgent::removeListener(IDisplayListener* listener)
{
    if (display_listener == listener)
        display_listener = nullptr;
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

void AudioPlayerAgent::stopRenderingTimer(const std::string& id)
{
    playsync_manager->clearContextHold();
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

void AudioPlayerAgent::sendEventPlaybackFailed(PlaybackError err, const std::string& reason)
{
    std::string ename = "PlaybackFailed";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong ");
        return;
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["error.type"] = playbackError(err);
    root["error.message"] = reason;
    root["currentPlaybackState.token"] = cur_token;
    root["currentPlaybackState.offsetInMilliseconds"] = std::to_string(offset);
    root["currentPlaybackState.playActivity"] = playerActivity(cur_aplayer_state);
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void AudioPlayerAgent::sendEventProgressReportDelayElapsed()
{
    nugu_dbg("report_delay_time: %d, position: %d", report_delay_time / 1000, player->position());
    sendEventCommon("ProgressReportDelayElapsed");
}

void AudioPlayerAgent::sendEventProgressReportIntervalElapsed()
{
    nugu_dbg("report_interval_time: %d, position: %d", report_interval_time / 1000, player->position());
    sendEventCommon("ProgressReportIntervalElapsed");
}

void AudioPlayerAgent::sendEventByDisplayInterface(const std::string& command)
{
    sendEventCommon(command);
}

void AudioPlayerAgent::sendEventCommon(std::string ename)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void AudioPlayerAgent::parsingPlay(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Value stream;
    Json::Value report;
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
        report_delay_time = report["progressReportDelayInMilliseconds"].asLargestInt();
        report_interval_time = report["progressReportIntervalInMilliseconds"].asLargestInt();
    }

    std::string playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        PlaySyncManager::DisplayRenderer renderer;
        Json::Value meta = audio_item["metadata"];

        if (!meta.empty() && !meta["template"].empty()) {
            PlaySyncManager::DisplayRenderInfo* info;
            Json::StyledWriter writer;
            struct timeval tv;
            std::string id;

            gettimeofday(&tv, NULL);
            id = std::to_string(tv.tv_sec) + std::to_string(tv.tv_usec);

            info = new PlaySyncManager::DisplayRenderInfo;
            info->id = id;
            info->ps_id = ps_id;
            info->type = meta["template"]["type"].asString();
            info->payload = writer.write(meta["template"]);
            info->dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
            info->token = root["token"].asString();
            render_info[id] = info;

            renderer.cap_type = getType();
            renderer.listener = this;
            renderer.display_id = id;
        }

        // sync display rendering with context
        playsync_manager->addContext(playstackctl_ps_id, getType(), renderer);
    }

    is_finished = false;

    nugu_dbg("= token: %s", token.c_str());
    nugu_dbg("= url: %s", url.c_str());
    nugu_dbg("= offset: %d", offset);
    nugu_dbg("= report_delay_time: %d", report_delay_time);
    nugu_dbg("= report_interval_time: %d", report_interval_time);

    std::string id = getReferrerDialoRequestId();
    // stop previous play for handling prev, next media play
    if (token != prev_token) {
        if (pre_ref_dialog_id.size())
            setReferrerDialoRequestId(pre_ref_dialog_id);

        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }
    }
    setReferrerDialoRequestId(id);
    pre_ref_dialog_id = id;
    cur_token = token;

    if (!player->setSource(url)) {
        nugu_error("set source failed");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "can't set source");
        return;
    }

    if (offset >= 0 && !player->seek(offset / 1000)) {
        nugu_error("seek media failed");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "can't seek");
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
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't pause");
        }

        CapabilityManager::getInstance()->releaseFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK);
    }
}

void AudioPlayerAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Reader reader;
    std::string playstackctl_ps_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        playsync_manager->removeContext(playstackctl_ps_id, getType(), !is_finished);
        std::string stacked_ps_id = playsync_manager->getPlayStackItem(getType());

        CapabilityManager::getInstance()->sendCommand(CapabilityType::AudioPlayer, CapabilityType::ASR, "releaseFocus", "");

        if (!stacked_ps_id.empty() && playstackctl_ps_id != stacked_ps_id)
            return;
        else if (stacked_ps_id.empty())
            CapabilityManager::getInstance()->releaseFocus("cap_audio", NUGU_FOCUS_RESOURCE_SPK);

        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
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
        prev_aplayer_state = AudioPlayerState::IDLE;
        return;
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
    case MediaPlayerEvent::INVALID_MEDIA_URL:
        nugu_warn("INVALID_MEDIA_URL");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INVALID_REQUEST, "media source is wrong");
        break;
    case MediaPlayerEvent::LOADING_MEDIA_FAILED:
        nugu_warn("LOADING_MEDIA_FAILED");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_SERVICE_UNAVAILABLE, "player can't play the media");
        break;
    case MediaPlayerEvent::LOADING_MEDIA_SUCCESS:
        nugu_dbg("LOADING_MEDIA_SUCCESS");
        break;
    case MediaPlayerEvent::PLAYING_MEDIA_FINISHED:
        nugu_dbg("PLAYING_MEDIA_FINISHED");
        is_finished = true;
        mediaStateChanged(MediaPlayerState::STOPPED);
        break;
    default:
        break;
    }
}

void AudioPlayerAgent::mediaChanged(const std::string& url)
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

void AudioPlayerAgent::onSyncDisplayContext(const std::string& id)
{
    nugu_dbg("AudioPlayer sync context");

    if (render_info.find(id) == render_info.end())
        return;

    display_listener->renderDisplay(id, render_info[id]->type, render_info[id]->payload, render_info[id]->dialog_id);
}

bool AudioPlayerAgent::onReleaseDisplayContext(const std::string& id, bool unconditionally)
{
    nugu_dbg("AudioPlayer release context");

    if (render_info.find(id) == render_info.end())
        return true;

    bool ret = display_listener->clearDisplay(id, unconditionally);
    if (ret || unconditionally) {
        auto info = render_info[id];
        render_info.erase(id);
        delete info;
    }

    if (unconditionally && !ret)
        nugu_warn("should clear display if unconditionally is true!!");

    return ret;
}

} // NuguCore
