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

#include "audio_player_agent.hh"
#include "base/nugu_log.h"
#include "base/nugu_prof.h"

namespace NuguCapability {

static const char* CAPABILITY_NAME = "AudioPlayer";
static const char* CAPABILITY_VERSION = "1.6";

AudioPlayerAgent::AudioPlayerAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , render_helper(std::unique_ptr<DisplayRenderHelper>(new DisplayRenderHelper()))
    , display_listener(nullptr)
{
}

void AudioPlayerAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    Capability::initialize();

    suspended_stop_policy = false;
    speak_dir = nullptr;
    focus_state = FocusState::NONE;
    is_tts_activate = false;
    stop_reason_by_play_another = false;
    has_play_directive = false;
    play_directive_dialog_id = "";
    receive_new_play_directive = false;
    skip_intermediate_foreground_focus = false;
    cur_aplayer_state = AudioPlayerState::IDLE;
    prev_aplayer_state = AudioPlayerState::IDLE;
    is_paused = false;
    is_paused_by_unfocus = false;
    ps_id = "";
    report_delay_time = -1;
    report_interval_time = -1;
    cur_token = "";
    cur_url = "";
    pre_ref_dialog_id = "";
    cur_dialog_id = "";
    is_finished = false;
    volume_update = false;
    volume = -1;
    template_id = "";

    std::string volume_str;
    if (capa_helper->getCapabilityProperty("Speaker", "music", volume_str))
        volume = std::stoi(volume_str);

    media_player = core_container->createMediaPlayer();
    media_player->addListener(this);
    media_player->setVolume(volume);

    tts_player = core_container->createTTSPlayer();
    tts_player->addListener(this);
    tts_player->setVolume(volume);

    cur_player = media_player;

    addReferrerEvents("PlaybackStarted", "Play");
    addReferrerEvents("PlaybackFinished", "Play");
    addReferrerEvents("PlaybackStopped", "Play");
    addReferrerEvents("PlaybackFailed", "Play");
    addReferrerEvents("ProgressReportDelayElapsed", "Play");
    addReferrerEvents("ProgressReportIntervalElapsed", "Play");
    addReferrerEvents("PlaybackPaused", "Play");
    addReferrerEvents("PlaybackResumed", "Play");
    addReferrerEvents("NextCommandIssued", "Play");
    addReferrerEvents("PreviousCommandIssued", "Play");
    addReferrerEvents("PlayCommandIssued", "Play");
    addReferrerEvents("PauseCommandIssued", "Play");
    addReferrerEvents("StopCommandIssued", "Play");
    addReferrerEvents("FavoriteCommandIssued", "Play");
    addReferrerEvents("RepeatCommandIssued", "Play");
    addReferrerEvents("ShuffleCommandIssued", "Play");
    addReferrerEvents("ShowLyricsSucceeded", "ShowLyrics");
    addReferrerEvents("ShowLyricsFailed", "ShowLyrics");
    addReferrerEvents("HideLyricsSucceeded", "HideLyrics");
    addReferrerEvents("HideLyricsFailed", "HideLyrics");
    addReferrerEvents("ControlLyricsPageSucceeded", "ControlLyricsPage");
    addReferrerEvents("ControlLyricsPageFailed", "ControlLyricsPage");
    addReferrerEvents("RequestPlayCommandIssued", "RequestPlayCommand");
    addReferrerEvents("RequestResumeCommandIssued", "RequestResumeCommand");
    addReferrerEvents("RequestNextCommandIssued", "RequestNextCommand");
    addReferrerEvents("RequestPreviousCommandIssued", "RequestPreviousCommand");
    addReferrerEvents("RequestPauseCommandIssued", "RequestPauseCommand");
    addReferrerEvents("RequestStopCommandIssued", "RequestStopCommand");

    addBlockingPolicy("Play", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("Stop", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("Pause", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestPlayCommand", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestResumeCommand", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestNextCommand", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestPreviousCommand", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestPauseCommand", { BlockingMedium::AUDIO, false });
    addBlockingPolicy("RequestStopCommand", { BlockingMedium::AUDIO, false });

    playsync_manager->addListener(getName(), this);

    initialized = true;
}

void AudioPlayerAgent::deInitialize()
{
    if (media_player) {
        media_player->removeListener(this);
        delete media_player;
        media_player = nullptr;
    }

    if (tts_player) {
        tts_player->removeListener(this);
        delete tts_player;
        tts_player = nullptr;
    }

    cur_player = nullptr;

    playsync_manager->removeListener(getName());
    render_helper->clear();

    initialized = false;
}

void AudioPlayerAgent::suspend()
{
    if (suspended_stop_policy) {
        nugu_dbg("already do suspend action");
        return;
    }

    nugu_dbg("suspend_policy[%d], focus_state => %s", suspend_policy, focus_manager->getStateString(focus_state).c_str());

    if (suspend_policy == SuspendPolicy::STOP) {
        if (focus_state != FocusState::NONE)
            playsync_manager->releaseSyncImmediately(ps_id, getName());

        suspended_stop_policy = true;
    } else {
        if (focus_state == FocusState::FOREGROUND)
            executeOnBackgroundAction();
    }
}

void AudioPlayerAgent::restore()
{
    if (suspend_policy == SuspendPolicy::STOP && !suspended_stop_policy) {
        nugu_dbg("do not suspend action yet");
        return;
    }

    nugu_dbg("suspend_policy[%d], focus_state => %s", suspend_policy, focus_manager->getStateString(focus_state).c_str());

    if (focus_state == FocusState::FOREGROUND)
        executeOnForegroundAction();

    suspended_stop_policy = false;
}

void AudioPlayerAgent::preprocessDirective(NuguDirective* ndir)
{
    const char* dname;
    const char* message;

    if (!ndir
        || !(dname = nugu_directive_peek_name(ndir))
        || !(message = nugu_directive_peek_json(ndir))) {
        nugu_error("The directive info is not exist.");
        return;
    }

    if (!strcmp(dname, "Play")) {
        if (routine_manager->isConditionToStop(ndir))
            routine_manager->stop();

        std::string playstackctl_ps_id = getPlayServiceIdInStackControl(message);

        playsync_manager->hasActivity(playstackctl_ps_id, PlayStackActivity::Media)
            ? playsync_manager->startSync(playstackctl_ps_id, getName(), composeRenderInfo(ndir, message))
            : playsync_manager->prepareSync(playstackctl_ps_id, ndir);
    }
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
    else if (!strcmp(dname, "UpdateMetadata"))
        parsingUpdateMetadata(message);
    else if (!strcmp(dname, "ShowLyrics"))
        parsingShowLyrics(message);
    else if (!strcmp(dname, "HideLyrics"))
        parsingHideLyrics(message);
    else if (!strcmp(dname, "ControlLyricsPage"))
        parsingControlLyricsPage(message);
    else if (!strcmp(dname, "RequestPlayCommand"))
        parsingRequestPlayCommand(dname, message);
    else if (!strcmp(dname, "RequestResumeCommand")
        || !strcmp(dname, "RequestNextCommand")
        || !strcmp(dname, "RequestPreviousCommand")
        || !strcmp(dname, "RequestPauseCommand")
        || !strcmp(dname, "RequestStopCommand"))
        parsingRequestOthersCommand(dname, message);
    else
        nugu_warn("%s[%s] is not support %s directive", getName().c_str(), getVersion().c_str(), dname);
}

void AudioPlayerAgent::updateInfoForContext(Json::Value& ctx)
{
    Json::Value aplayer;
    double offset = cur_player->position() * 1000;
    double duration = cur_player->duration() * 1000;

    aplayer["version"] = getVersion();
    aplayer["playerActivity"] = playerActivity(cur_aplayer_state);
    aplayer["offsetInMilliseconds"] = offset;
    if (ps_id.size())
        aplayer["playServiceId"] = ps_id;
    if (cur_aplayer_state != AudioPlayerState::IDLE && cur_token.size()) {
        aplayer["token"] = cur_token;
        if (duration)
            aplayer["durationInMilliseconds"] = duration;
    }
    if (display_listener) {
        bool visible = false;
        if (display_listener->requestLyricsPageAvailable(template_id, visible))
            aplayer["lyricsVisible"] = visible;
    }

    ctx[getName()] = aplayer;
}

bool AudioPlayerAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (!convert_command.compare("setvolume")) {
        cur_player->setVolume(std::stoi(param));
    } else {
        nugu_error("invalid command: %s", command.c_str());
        return false;
    }

    return true;
}

void AudioPlayerAgent::receiveCommandAll(const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (convert_command == "network_disconnected") {
        skip_intermediate_foreground_focus = false;
        return;
    }

    if (receive_new_play_directive)
        return;

    if (convert_command == "receive_directive_group") {
        if (param.find("AudioPlayer.Play") != std::string::npos)
            has_play_directive = true;
    } else if (convert_command == "directive_dialog_id") {
        if (has_play_directive && param != play_directive_dialog_id)
            receive_new_play_directive = true;
    }
}

void AudioPlayerAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener) {
        addListener(dynamic_cast<IAudioPlayerListener*>(listener));
        setListener(dynamic_cast<IAudioPlayerDisplayListener*>(listener));
    }
}

void AudioPlayerAgent::addListener(IAudioPlayerListener* listener)
{
    auto iterator = std::find(aplayer_listeners.begin(), aplayer_listeners.end(), listener);

    if (iterator == aplayer_listeners.end())
        aplayer_listeners.emplace_back(listener);
}

void AudioPlayerAgent::removeListener(IAudioPlayerListener* listener)
{
    auto iterator = std::find(aplayer_listeners.begin(), aplayer_listeners.end(), listener);

    if (iterator != aplayer_listeners.end())
        aplayer_listeners.erase(iterator);
}

std::string AudioPlayerAgent::play()
{
    std::string id = sendEventByMediaPlayerControl("PlayCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::stop(bool direct_access)
{
    std::string id;

    if (direct_access) {
        if (cur_token.empty()) {
            nugu_warn("there is no media content in the playlist.");
            return "";
        }
        nugu_dbg("user request to stop media directly");
        playsync_manager->releaseSyncImmediately(ps_id, getName());
    } else {
        id = sendEventByMediaPlayerControl("StopCommandIssued");
        skip_intermediate_foreground_focus = !id.empty();
        nugu_dbg("user request dialog id: %s", id.c_str());
    }
    return id;
}

std::string AudioPlayerAgent::next()
{
    std::string id = sendEventByMediaPlayerControl("NextCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::prev()
{
    std::string id = sendEventByMediaPlayerControl("PreviousCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::pause(bool direct_access)
{
    std::string id;

    if (direct_access) {
        nugu_dbg("user request to pause media directly");
        if (cur_token.empty()) {
            nugu_warn("there is no media content in the playlist.");
            return "";
        }

        if (cur_aplayer_state != AudioPlayerState::PLAYING) {
            nugu_warn("there is no playing media content in the playlist.");
            return "";
        }

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::PAUSE_BY_APPLICATION, cur_dialog_id);

        if (!cur_player->pause()) {
            nugu_error("pause media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't pause");
        }
    } else {
        id = sendEventByMediaPlayerControl("PauseCommandIssued");
        skip_intermediate_foreground_focus = !id.empty();
        nugu_dbg("user request dialog id: %s", id.c_str());
    }
    return id;
}

std::string AudioPlayerAgent::resume(bool direct_access)
{
    std::string id;

    if (direct_access) {
        nugu_dbg("user request to resume media directly");
        if (cur_token.empty()) {
            nugu_warn("there is no media content in the playlist.");
            return "";
        }

        if (!cur_player->resume()) {
            nugu_error("resume media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't resume media");
        }
    } else {
        id = sendEventByMediaPlayerControl("PlayCommandIssued");
        nugu_dbg("user request dialog id: %s", id.c_str());
    }
    return id;
}

void AudioPlayerAgent::seek(int msec)
{
    if (cur_player)
        cur_player->seek(msec * 1000);
}

std::string AudioPlayerAgent::requestFavoriteCommand(bool current_favorite)
{
    std::string ename = "FavoriteCommandIssued";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return "";
    }

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["playServiceId"] = ps_id;
    root["favorite"] = current_favorite;
    payload = writer.write(root);

    std::string id = sendEvent(ename, capa_helper->makeAllContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::requestRepeatCommand(RepeatType current_repeat)
{
    std::string ename = "RepeatCommandIssued";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return "";
    }

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["playServiceId"] = ps_id;
    switch (current_repeat) {
    case RepeatType::ONE:
        root["repeat"] = "ONE";
        break;
    case RepeatType::ALL:
        root["repeat"] = "ALL";
        break;
    default:
        root["repeat"] = "NONE";
        break;
    }
    payload = writer.write(root);

    std::string id = sendEvent(ename, capa_helper->makeAllContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::requestShuffleCommand(bool current_shuffle)
{
    std::string ename = "ShuffleCommandIssued";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return "";
    }

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["playServiceId"] = ps_id;
    root["shuffle"] = current_shuffle;
    payload = writer.write(root);

    std::string id = sendEvent(ename, capa_helper->makeAllContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

bool AudioPlayerAgent::setVolume(int volume)
{
    nugu_dbg("set media player's volume: %d", volume);
    if (this->volume == volume)
        return true;

    this->volume = volume;
    volume_update = true;

    if (focus_state == FocusState::FOREGROUND)
        checkAndUpdateVolume();
    else
        nugu_dbg("Reserved to change media player's volume(%d)", volume);

    return true;
}

bool AudioPlayerAgent::setMute(bool mute)
{
    nugu_dbg("set media player's mute: %d", mute);
    if (!cur_player)
        return false;

    if (!cur_player->setMute(mute))
        return false;

    nugu_dbg("media player's mute(%d) changed..", mute);
    return true;
}

void AudioPlayerAgent::displayRendered(const std::string& id)
{
    // TODO: integrate with playsync and session manager
}

void AudioPlayerAgent::displayCleared(const std::string& id)
{
    // TODO: integrate with playsync and session manager
}

void AudioPlayerAgent::elementSelected(const std::string& id, const std::string& item_token, const std::string& postback)
{
    // ignore
}

void AudioPlayerAgent::informControlResult(const std::string& id, ControlType type, ControlDirection direction)
{
    sendEventControlLyricsPageSucceeded(direction == ControlDirection::NEXT ? "NEXT" : "PREVIOUS");
}

void AudioPlayerAgent::setListener(IDisplayListener* listener)
{
    if (listener == nullptr)
        return;

    display_listener = dynamic_cast<IAudioPlayerDisplayListener*>(listener);
    render_helper->setDisplayListener(display_listener);
}

void AudioPlayerAgent::removeListener(IDisplayListener* listener)
{
    if (listener == nullptr)
        return;

    auto audio_display_listener = dynamic_cast<IAudioPlayerDisplayListener*>(listener);

    if (audio_display_listener == display_listener)
        display_listener = nullptr;
}

void AudioPlayerAgent::stopRenderingTimer(const std::string& id)
{
    // TODO: integrate with playsync and session manager
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
        cur_aplayer_state = AudioPlayerState::PLAYING;
        playsync_manager->clearHolding();

        if (is_paused_by_unfocus) {
            nugu_dbg("force setting previous state for notify to application");
            prev_aplayer_state = AudioPlayerState::PAUSED;
        } else {
            if (prev_aplayer_state == AudioPlayerState::PAUSED)
                sendEventPlaybackResumed();
            else
                sendEventPlaybackStarted();
        }
        is_paused_by_unfocus = false;
        break;
    case MediaPlayerState::PAUSED:
        cur_aplayer_state = AudioPlayerState::PAUSED;

        if (is_paused_by_unfocus) {
            playsync_manager->continueRelease();
            playsync_manager->releaseSyncLater(ps_id, getName());
        }

        if (!is_paused && !is_paused_by_unfocus)
            sendEventPlaybackPaused();
        break;
    case MediaPlayerState::STOPPED:
        if (is_finished) {
            cur_aplayer_state = AudioPlayerState::FINISHED;
            sendEventPlaybackFinished();
        } else {
            cur_aplayer_state = AudioPlayerState::STOPPED;
            sendEventPlaybackStopped();
        }
        is_paused = false;
        is_paused_by_unfocus = false;
        break;
    default:
        break;
    }
    nugu_info("[audioplayer state] %s -> %s", playerActivity(prev_aplayer_state).c_str(), playerActivity(cur_aplayer_state).c_str());

    if (prev_aplayer_state == cur_aplayer_state)
        return;

    for (const auto& aplayer_listener : aplayer_listeners)
        aplayer_listener->mediaStateChanged(cur_aplayer_state, cur_dialog_id);

    if ((is_paused || is_paused_by_unfocus) && cur_aplayer_state == AudioPlayerState::PAUSED) {
        nugu_info("[audioplayer state] skip to save prev_aplayer_state");
        return;
    }

    prev_aplayer_state = cur_aplayer_state;
}

void AudioPlayerAgent::mediaEventReport(MediaPlayerEvent event)
{
    switch (event) {
    case MediaPlayerEvent::INVALID_MEDIA_URL:
        nugu_warn("INVALID_MEDIA_URL");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INVALID_REQUEST, "media source is wrong");

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::INVALID_URL, cur_dialog_id);
        break;
    case MediaPlayerEvent::LOADING_MEDIA_FAILED:
        nugu_warn("LOADING_MEDIA_FAILED");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_SERVICE_UNAVAILABLE, "player can't load the media");

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::LOAD_FAILED, cur_dialog_id);
        break;
    case MediaPlayerEvent::LOADING_MEDIA_SUCCESS:
        nugu_dbg("LOADING_MEDIA_SUCCESS");

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::LOAD_DONE, cur_dialog_id);

        break;
    case MediaPlayerEvent::PLAYING_MEDIA_FINISHED:
        nugu_dbg("PLAYING_MEDIA_FINISHED");
        is_finished = true;
        mediaStateChanged(MediaPlayerState::STOPPED);
        break;
    case MediaPlayerEvent::PLAYING_MEDIA_UNDERRUN:
        for (const auto& aplayer_listener : aplayer_listeners) {
            aplayer_listener->mediaEventReport(AudioPlayerEvent::UNDERRUN, cur_dialog_id);
        }
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
    for (const auto& aplayer_listener : aplayer_listeners)
        aplayer_listener->durationChanged(duration);
}

void AudioPlayerAgent::positionChanged(int position)
{
    if (report_delay_time > 0 && ((report_delay_time / 1000) == position))
        sendEventProgressReportDelayElapsed();

    if (report_interval_time > 0 && ((position % (int)(report_interval_time / 1000)) == 0))
        sendEventProgressReportIntervalElapsed();

    for (const auto& aplayer_listener : aplayer_listeners)
        aplayer_listener->positionChanged(position);
}

void AudioPlayerAgent::volumeChanged(int volume)
{
}

void AudioPlayerAgent::muteChanged(int mute)
{
}

void AudioPlayerAgent::onFocusChanged(FocusState state)
{
    nugu_info("Focus Changed(%s -> %s)", focus_manager->getStateString(focus_state).c_str(), focus_manager->getStateString(state).c_str());

    switch (state) {
    case FocusState::FOREGROUND:
        executeOnForegroundAction();
        break;
    case FocusState::BACKGROUND:
        executeOnBackgroundAction();
        break;
    case FocusState::NONE:
        if (speak_dir) {
            destroyDirective(speak_dir);
            speak_dir = nullptr;
        }

        nugu_prof_mark(NUGU_PROF_TYPE_AUDIO_FINISHED);
        if (!cur_player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }

        // TODO: integrate with playsync and session manager
        cur_url = template_id = "";
        clearContext();
        break;
    }
    focus_state = state;
}

void AudioPlayerAgent::onSyncState(const std::string& ps_id, PlaySyncState state, void* extra_data)
{
    if (state == PlaySyncState::Synced)
        template_id = render_helper->renderDisplay(extra_data);
    else if (state == PlaySyncState::Released) {
        focus_manager->releaseFocus(MEDIA_FOCUS_TYPE, CAPABILITY_NAME);
        render_helper->clearDisplay(extra_data, playsync_manager->hasNextPlayStack());
    }
}

void AudioPlayerAgent::onDataChanged(const std::string& ps_id, std::pair<void*, void*> extra_datas)
{
    template_id = render_helper->updateDisplay(extra_datas, playsync_manager->hasNextPlayStack());
}

std::string AudioPlayerAgent::sendEventCommon(const std::string& ename, EventResultCallback cb, bool include_all_context)
{
    Json::FastWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 || cur_token.size() == 0 || ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);

    return sendEvent(ename, include_all_context ? capa_helper->makeAllContextInfo() : getContextInfo(), writer.write(root), std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackStarted(EventResultCallback cb)
{
    sendEventCommon("PlaybackStarted", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackFinished(EventResultCallback cb)
{
    sendEventCommon("PlaybackFinished", std::move(cb), true);
}

void AudioPlayerAgent::sendEventPlaybackStopped(EventResultCallback cb)
{
    std::string ename = "PlaybackStopped";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 || cur_token.size() == 0 || ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["reason"] = stop_reason_by_play_another ? "PLAY_ANOTHER" : "STOP";
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackPaused(EventResultCallback cb)
{
    sendEventCommon("PlaybackPaused", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackResumed(EventResultCallback cb)
{
    sendEventCommon("PlaybackResumed", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackFailed(PlaybackError err, const std::string& reason, EventResultCallback cb)
{
    std::string ename = "PlaybackFailed";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 || cur_token.size() == 0 || ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["error"]["type"] = playbackError(err);
    root["error"]["message"] = reason;
    root["currentPlaybackState"]["token"] = cur_token;
    root["currentPlaybackState"]["offsetInMilliseconds"] = std::to_string(offset);
    root["currentPlaybackState"]["playActivity"] = playerActivity(cur_aplayer_state);
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventProgressReportDelayElapsed(EventResultCallback cb)
{
    nugu_dbg("report_delay_time: %d, position: %d", report_delay_time / 1000, cur_player->position());
    sendEventCommon("ProgressReportDelayElapsed", std::move(cb));
}

void AudioPlayerAgent::sendEventProgressReportIntervalElapsed(EventResultCallback cb)
{
    nugu_dbg("report_interval_time: %d, position: %d", report_interval_time / 1000, cur_player->position());
    sendEventCommon("ProgressReportIntervalElapsed", std::move(cb));
}

std::string AudioPlayerAgent::sendEventByMediaPlayerControl(const std::string& command, EventResultCallback cb)
{
    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return "";
    }

    return sendEventCommon(command, std::move(cb), true);
}

void AudioPlayerAgent::sendEventShowLyricsSucceeded(EventResultCallback cb)
{
    std::string ename = "ShowLyricsSucceeded";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventShowLyricsFailed(EventResultCallback cb)
{
    std::string ename = "EventShowLyricsFailed";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventHideLyricsSucceeded(EventResultCallback cb)
{
    std::string ename = "HideLyricsSucceeded";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventHideLyricsFailed(EventResultCallback cb)
{
    std::string ename = "HideLyricsFailed";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (ps_id.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventControlLyricsPageSucceeded(const std::string& direction, EventResultCallback cb)
{
    std::string ename = "ControlLyricsPageSucceeded";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (ps_id.size() == 0 || direction.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    root["direction"] = direction;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventControlLyricsPageFailed(const std::string& direction, EventResultCallback cb)
{
    std::string ename = "ControlLyricsPageFailed";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    if (ps_id.size() == 0 || direction.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return;
    }

    root["playServiceId"] = ps_id;
    root["direction"] = direction;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventByRequestOthersDirective(const std::string& dname, EventResultCallback cb)
{
    std::string ename = dname + "Issued";
    sendEventCommon(ename, std::move(cb), true);
}

void AudioPlayerAgent::sendEventRequestPlayCommandIssued(const std::string& dname, const std::string& payload, EventResultCallback cb)
{
    std::string ename = dname + "Issued";
    sendEvent(ename, capa_helper->makeAllContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventRequestCommandFailed(const std::string& play_service_id, const std::string& type, const std::string& reason, EventResultCallback cb)
{
    std::string ename = "RequestCommandFailed";
    std::string payload = "";
    Json::FastWriter writer;
    Json::Value root;

    root["token"] = cur_token;
    root["playServiceId"] = play_service_id;
    root["error"]["type"] = type;
    root["error"]["message"] = reason;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::notifyEventResponse(const std::string& msg_id, const std::string& data, bool success)
{
    skip_intermediate_foreground_focus = false;
}

bool AudioPlayerAgent::isContentCached(const std::string& key, std::string& playurl)
{
    std::string filepath;

    for (const auto& aplayer_listener : aplayer_listeners) {
        if (aplayer_listener->requestToGetCachedContent(key, filepath)) {
            playurl = filepath;
            return true;
        }
    }
    return false;
}

void AudioPlayerAgent::parsingPlay(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Value stream;
    Json::Value report;
    Json::Reader reader;
    std::string source_type;
    std::string url;
    std::string cache_key;
    long offset;
    bool new_content;
    std::string token;
    std::string prev_token;
    std::string temp;
    std::string play_service_id;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    audio_item = root["audioItem"];
    if (audio_item.empty()) {
        nugu_error("directive message syntax error");
        return;
    }
    play_service_id = root["playServiceId"].asString();
    cache_key = root["cacheKey"].asString();

    source_type = root["sourceType"].asString();
    stream = audio_item["stream"];
    if (stream.empty()) {
        nugu_error("directive message syntax error");
        return;
    }

    url = stream["url"].asString();
    offset = stream["offsetInMilliseconds"].asLargestInt();
    token = stream["token"].asString();
    prev_token = stream["expectedPreviousToken"].asString();

    if (token.size() == 0 || play_service_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (source_type == "ATTACHMENT") {
        destroy_directive_by_agent = true;

        nugu_dbg("    token => %s", token.c_str());
        nugu_dbg("cur_token => %s", cur_token.c_str());
        nugu_dbg("   offset => %d", offset);

        new_content = (token != cur_token) || (offset == 0);
        nugu_dbg("Receive Attachment media (new_content: %d)", new_content);
    } else {
        if (url.size() == 0) {
            nugu_error("There is no mandatory data in directive message");
            return;
        }

        nugu_dbg("    url => %s", url.c_str());
        nugu_dbg("cur_url => %s", cur_url.c_str());
        nugu_dbg(" offset => %d", offset);

        new_content = (url != cur_url) || (offset == 0);
        nugu_dbg("Receive Streaming media (new_content: %d)", new_content);
        cur_url = url;
    }

    if (cache_key.size()) {
        std::string filepath;
        if (isContentCached(cache_key, filepath)) {
            nugu_dbg("the content(key: %s) is cached in %s", cache_key.c_str(), filepath.c_str());
            url = filepath;
        } else {
            for (const auto& aplayer_listener : aplayer_listeners)
                aplayer_listener->requestContentCache(cache_key, url);
        }
    }

    report = stream["progressReport"];
    if (!report.empty()) {
        report_delay_time = report["progressReportDelayInMilliseconds"].asLargestInt();
        report_interval_time = report["progressReportIntervalInMilliseconds"].asLargestInt();
    }

    playsync_manager->startSync(getPlayServiceIdInStackControl(root["playStackControl"]), getName());

    is_finished = false;
    is_paused_by_unfocus = false;
    speak_dir = nullptr;

    nugu_dbg("= token: %s", token.c_str());
    nugu_dbg("= url: %s", url.c_str());
    nugu_dbg("= offset: %d", offset);
    nugu_dbg("= report_delay_time: %d", report_delay_time);
    nugu_dbg("= report_interval_time: %d", report_interval_time);

    std::string dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
    std::string dname = nugu_directive_peek_name(getNuguDirective());

    if (new_content) {
        if (pre_ref_dialog_id.size())
            setReferrerDialogRequestId(dname, pre_ref_dialog_id);

        nugu_prof_mark(NUGU_PROF_TYPE_AUDIO_FINISHED);

        stop_reason_by_play_another = (ps_id == play_service_id);

        if (!cur_player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }

        stop_reason_by_play_another = false;

        std::string id = nugu_directive_peek_dialog_id(getNuguDirective());
        setReferrerDialogRequestId(dname, id);
        pre_ref_dialog_id = id;
    } else {
        setReferrerDialogRequestId(dname, pre_ref_dialog_id);
    }
    cur_dialog_id = dialog_id;
    cur_token = token;
    ps_id = play_service_id;

    if (destroy_directive_by_agent) {
        cur_player = dynamic_cast<IMediaPlayer*>(tts_player);
        is_tts_activate = true;
        speak_dir = getNuguDirective();
    } else {
        cur_player = media_player;
        is_tts_activate = false;
    }

    if (new_content) {
        if (!cur_player->setSource(url)) {
            nugu_error("set source failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "can't set source");
            return;
        }
        if (offset >= 0 && !cur_player->seek(offset / 1000)) {
            nugu_error("seek media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "can't seek");
            return;
        }
    }

    play_directive_dialog_id = dialog_id;
    receive_new_play_directive = false;
    has_play_directive = false;

    if (focus_state == FocusState::FOREGROUND)
        executeOnForegroundAction();
    else
        focus_manager->requestFocus(MEDIA_FOCUS_TYPE, CAPABILITY_NAME, this);
}

void AudioPlayerAgent::parsingPause(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Reader reader;
    std::string playserviceid;
    std::string active_playserviceid;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playserviceid = root["playServiceId"].asString();

    if (playserviceid.size()) {
        // hold context about 10m' and remove it, if there are no action.
        playsync_manager->releaseSyncLater(playserviceid, getName());

        cur_dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::PAUSE_BY_DIRECTIVE, cur_dialog_id);

        if (!cur_player->pause()) {
            nugu_error("pause media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't pause");
        }
        if (prev_aplayer_state != AudioPlayerState::PAUSED) {
            nugu_dbg("audio player state is paused!!");
            prev_aplayer_state = cur_aplayer_state = AudioPlayerState::PAUSED;
            sendEventPlaybackPaused();
        }

        capa_helper->sendCommand("AudioPlayer", "ASR", "releaseFocus", "");
    }
}

void AudioPlayerAgent::parsingStop(const char* message)
{
    Json::Value root;
    Json::Value audio_item;
    Json::Reader reader;
    std::string playstackctl_ps_id;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        is_finished
            ? playsync_manager->releaseSync(playstackctl_ps_id, getName())
            : playsync_manager->releaseSyncImmediately(playstackctl_ps_id, getName());

        capa_helper->sendCommand("AudioPlayer", "ASR", "releaseFocus", "");
    }

    clearContext();
}

void AudioPlayerAgent::parsingUpdateMetadata(const char* message)
{
    Json::Value root;
    Json::Value settings;
    Json::Reader reader;
    Json::StyledWriter writer;
    std::string playserviceid;

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playserviceid = root["playServiceId"].asString();
    if (playserviceid.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }
    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (root["metadata"].empty() || root["metadata"]["template"].empty()
        || root["metadata"]["template"]["content"].empty()
        || root["metadata"]["template"]["content"]["settings"].empty()) {
        nugu_error("directive message syntax error");
        return;
    }

    std::string dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());

    settings = root["metadata"]["template"]["settings"];
    if (!settings["repeat"].empty()) {
        std::string repeat = settings["repeat"].asString();

        RepeatType type = RepeatType::NONE;
        if (repeat == "ONE")
            type = RepeatType::ONE;
        else if (repeat == "ALL")
            type = RepeatType::ALL;

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->repeatChanged(type, dialog_id);
    }
    if (!settings["favorite"].empty()) {
        bool favorite = settings["favorite"].asBool();

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->favoriteChanged(favorite, dialog_id);
    }
    if (!settings["shuffle"].empty()) {
        bool shuffle = settings["shuffle"].asBool();

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->shuffleChanged(shuffle, dialog_id);
    }

    if (!template_id.empty()) {
        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->updateMetaData(template_id, writer.write(root["metadata"]));
    }
}

void AudioPlayerAgent::parsingShowLyrics(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string playserviceid;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    playserviceid = root["playServiceId"].asString();
    if (playserviceid.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }
    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (display_listener)
        display_listener->showLyrics(template_id);
}

void AudioPlayerAgent::parsingHideLyrics(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string playserviceid;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    playserviceid = root["playServiceId"].asString();
    if (playserviceid.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }
    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (display_listener)
        display_listener->hideLyrics(template_id);
}

void AudioPlayerAgent::parsingControlLyricsPage(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string playserviceid;
    std::string direction;
    ControlDirection control_direction;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (cur_token.empty()) {
        nugu_warn("there is no media content in the playlist.");
        return;
    }

    playserviceid = root["playServiceId"].asString();
    direction = root["direction"].asString();
    if (playserviceid.size() == 0 || direction.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (direction == "NEXT")
        control_direction = ControlDirection::NEXT;
    else
        control_direction = ControlDirection::PREVIOUS;

    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        sendEventControlLyricsPageFailed(direction);
        return;
    }

    if (display_listener)
        display_listener->controlDisplay(template_id, ControlType::Scroll, control_direction);
}

void AudioPlayerAgent::parsingRequestPlayCommand(const char* dname, const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string play_service_id;
    std::string payload;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    play_service_id = root["playServiceId"].asString();

    if (play_service_id.size() == 0 || root["tracks"].empty() || root["tracks"].size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    payload = message;
    sendEventRequestPlayCommandIssued(dname, payload);
}

void AudioPlayerAgent::parsingRequestOthersCommand(const char* dname, const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string play_service_id;
    std::string err_type;
    std::string err_reason;
    long offset = cur_player->position() * 1000;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    if (cur_aplayer_state == AudioPlayerState::IDLE || cur_aplayer_state == AudioPlayerState::STOPPED) {
        err_type = "INVALID_COMMAND";
        err_reason = "current state: " + playerActivity(cur_aplayer_state);
        nugu_error("Error type: %s, reason: %s", err_type.c_str(), err_reason.c_str());
        sendEventRequestCommandFailed(play_service_id, err_type, err_reason);
        return;
    }

    if (cur_token.size() == 0) {
        err_type = "UNKNOWN_ERROR";
        err_reason = "token is null or empty";
        nugu_error("Error type: %s, reason: %s", err_type.c_str(), err_reason.c_str());
        sendEventRequestCommandFailed(play_service_id, err_type, err_reason);
        return;
    }

    if (offset < 0) {
        err_type = "UNKNOWN_ERROR";
        err_reason = "offsetInMilliseconds is null";
        nugu_error("Error type: %s, reason: %s", err_type.c_str(), err_reason.c_str());
        sendEventRequestCommandFailed(play_service_id, err_type, err_reason);
        return;
    }

    sendEventByRequestOthersDirective(dname);
}

DisplayRenderInfo* AudioPlayerAgent::composeRenderInfo(NuguDirective* ndir, const char* message)
{
    Json::Value root;
    Json::Reader reader;
    Json::Value meta;
    Json::StyledWriter writer;

    if (!reader.parse(message, root)
        || ((meta = root["audioItem"]["metadata"]).empty() || meta["template"].empty())) {
        nugu_warn("no rendering info");
        return nullptr;
    }

    // if it has Display, skip to render AudioPlayer's template
    if (std::string { nugu_directive_peek_groups(ndir) }.find("Display") != std::string::npos) {
        nugu_warn("It has the separated display. So skip to parse render info.");
        return nullptr;
    }

    return render_helper->getRenderInfoBuilder()
        ->setType(meta["template"]["type"].asString())
        ->setView(writer.write(meta["template"]))
        ->setDialogId(nugu_directive_peek_dialog_id(ndir))
        ->build();
}

void AudioPlayerAgent::clearContext()
{
    nugu_info("clear AudioPlayer's Context");
    ps_id = cur_token = "";
}

void AudioPlayerAgent::checkAndUpdateVolume()
{
    if (volume_update && media_player && tts_player) {
        nugu_dbg("media player's volume(%d) is changed", volume);
        media_player->setVolume(volume);
        tts_player->setVolume(volume);
        volume_update = false;
    }
}

bool AudioPlayerAgent::hasToSkipForegroundAction()
{
    if (suspended_stop_policy) {
        nugu_warn("AudioPlayer is suspended with stop policy, please restore AudioPlayer.");
        return true;
    }

    if (interaction_control_manager->isMultiTurnActive()) {
        nugu_info("The multi-turn is active. So, it ignore intermediate foreground focus.");
        return true;
    }

    if (routine_manager->isRoutineAlive()) {
        nugu_info("Routine is alive. So, it ignore intermediate foreground focus.");
        return true;
    }

    if (is_paused) {
        nugu_warn("AudioPlayer is pause mode caused by directive(PAUSE)");
        return true;
    }

    if (skip_intermediate_foreground_focus) {
        nugu_warn("Skip intermediate foreground focus");
        return true;
    }

    return false;
}

void AudioPlayerAgent::executeOnForegroundAction()
{
    if (hasToSkipForegroundAction())
        return;

    nugu_dbg("executeOnForegroundAction()");

    std::string type = is_tts_activate ? "attachment" : "streaming";
    nugu_dbg("cur_aplayer_state[%s] => %d, player->state() => %s, is_finished: %d", type.c_str(), cur_aplayer_state, cur_player->stateString(cur_player->state()).c_str(), is_finished);

    checkAndUpdateVolume();

    if (receive_new_play_directive) {
        nugu_dbg("The media has been expired and does not play.");
    } else if (cur_player->state() == MediaPlayerState::STOPPED && is_finished) {
        nugu_dbg("The media has already been played.");
    } else if (cur_player->state() == MediaPlayerState::PAUSED) {
        if (!cur_player->resume()) {
            nugu_error("resume media(%s) failed", type.c_str());
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't resume media");
        }
    } else {
        nugu_prof_mark(NUGU_PROF_TYPE_AUDIO_STARTED);

        if (!cur_player->play()) {
            nugu_error("play media(%s) failed", type.c_str());
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't play media");
        }
        if (is_tts_activate) {
            if (nugu_directive_get_data_size(speak_dir) > 0)
                getAttachmentData(speak_dir, this);

            if (speak_dir)
                nugu_directive_set_data_callback(speak_dir, directiveDataCallback, this);
        }
    }
}

void AudioPlayerAgent::executeOnBackgroundAction()
{
    nugu_dbg("executeOnBackgroundAction()");

    nugu_dbg("is_finished: %d, cur_player->state(): %d", is_finished, cur_player->state());

    MediaPlayerState state = cur_player->state();

    if (state == MediaPlayerState::PLAYING || state == MediaPlayerState::PREPARE
        || state == MediaPlayerState::READY) {
        is_paused_by_unfocus = true;

        for (const auto& aplayer_listener : aplayer_listeners)
            aplayer_listener->mediaEventReport(AudioPlayerEvent::PAUSE_BY_FOCUS, cur_dialog_id);

        if (!cur_player->pause()) {
            nugu_error("pause media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't pause");
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

void AudioPlayerAgent::directiveDataCallback(NuguDirective* ndir, int seq, void* userdata)
{
    getAttachmentData(ndir, userdata);
}

void AudioPlayerAgent::getAttachmentData(NuguDirective* ndir, void* userdata)
{
    AudioPlayerAgent* agent = static_cast<AudioPlayerAgent*>(userdata);
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
} // NuguCapability
