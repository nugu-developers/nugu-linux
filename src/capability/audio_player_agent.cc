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

namespace NuguCapability {

static const char* CAPABILITY_NAME = "AudioPlayer";
static const char* CAPABILITY_VERSION = "1.1";

AudioPlayerAgent::AudioPlayerAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
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
{
}

AudioPlayerAgent::~AudioPlayerAgent()
{
}

void AudioPlayerAgent::initialize()
{
    if (initialized) {
        nugu_info("It's already initialized.");
        return;
    }

    player = core_container->createMediaPlayer();
    player->addListener(this);

    capa_helper->addFocus("cap_audio", NUGU_FOCUS_TYPE_MEDIA, this);

    initialized = true;
}

void AudioPlayerAgent::deInitialize()
{
    aplayer_listeners.clear();

    capa_helper->removeFocus("cap_audio");

    if (player) {
        player->removeListener(this);
        delete player;
        player = nullptr;
    }

    initialized = false;
}

NuguFocusResult AudioPlayerAgent::onFocus(void* event)
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

NuguFocusResult AudioPlayerAgent::onUnfocus(void* event)
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

NuguFocusStealResult AudioPlayerAgent::onStealRequest(void* event, NuguFocusType target_type)
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

void AudioPlayerAgent::setFavorite(bool favorite)
{
    std::string ename = "FavoriteCommandIssued";
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
    root["favorite"] = favorite;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

void AudioPlayerAgent::setRepeat(RepeatType repeat)
{
    std::string ename = "RepeatCommandIssued";
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
    switch (repeat) {
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

    sendEvent(ename, getContextInfo(), payload);
}

void AudioPlayerAgent::setShuffle(bool shuffle)
{
    std::string ename = "ShuffleCommandIssued";
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
    root["shuffle"] = shuffle;
    payload = writer.write(root);

    sendEvent(ename, getContextInfo(), payload);
}

bool AudioPlayerAgent::setVolume(int volume)
{
    nugu_dbg("set media player's volume: %d", volume);
    if (!player)
        return false;

    if (player->setVolume(volume))
        return false;

    nugu_dbg("media player's volume(%d) changed..", volume);
    return true;
}

bool AudioPlayerAgent::setMute(bool mute)
{
    nugu_dbg("set media player's mute: %d", mute);
    if (!player)
        return false;

    if (player->setMute(mute))
        return false;

    nugu_dbg("media player's mute(%d) changed..", mute);
    return true;
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
    if (display_listener) {
        bool visible = false;

        if (display_listener->requestLyricsPageAvailable(visible))
            aplayer["lyricsVisible"] = visible;
    }

    ctx[getName()] = aplayer;
}

void AudioPlayerAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (!convert_command.compare("setvolume"))
        player->setVolume(std::stoi(param));
}

void AudioPlayerAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener) {
        setListener(dynamic_cast<IDisplayListener*>(listener));
        addListener(dynamic_cast<IAudioPlayerListener*>(listener));
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

void AudioPlayerAgent::sendEventShowLyricsSucceeded()
{
    sendEventCommon("ShowLyricsSucceeded");
}

void AudioPlayerAgent::sendEventShowLyricsFailed()
{
    sendEventCommon("EventShowLyricsFailed");
}

void AudioPlayerAgent::sendEventHideLyricsSucceeded()
{
    sendEventCommon("HideLyricsSucceeded");
}

void AudioPlayerAgent::sendEventHideLyricsFailed()
{
    sendEventCommon("HideLyricsFailed");
}

void AudioPlayerAgent::sendEventControlLyricsPageSucceeded()
{
    sendEventCommon("ControlLyricsPageSucceeded");
}

void AudioPlayerAgent::sendEventControlLyricsPageFailed()
{
    sendEventCommon("ControlLyricsPageFailed");
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

void AudioPlayerAgent::sendEventCommon(const std::string& ename)
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

bool AudioPlayerAgent::isContentCached(const std::string& key, std::string& playurl)
{
    std::string filepath;

    for (auto aplayer_listener : aplayer_listeners) {
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
    Json::Value attachment;
    Json::Value stream;
    Json::Value report;
    Json::Reader reader;
    std::string source_type;
    std::string url;
    std::string cache_key;
    long offset;
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
        nugu_error("directive message syntex error");
        return;
    }
    play_service_id = root["playServiceId"].asString();
    cache_key = root["cacheKey"].asString();

    source_type = root["sourceType"].asString();
    if (source_type == "ATTACHMENT") {
        attachment = audio_item["attachment"];
        if (attachment.empty()) {
            nugu_error("directive message syntex error");
            return;
        }
        nugu_warn("Not support to play attachment stream yet!!!");
        // has_attachment = true;
        // destoryDirective(ndir);
        return;
    }

    stream = audio_item["stream"];
    if (stream.empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    url = stream["url"].asString();
    offset = stream["offsetInMilliseconds"].asLargestInt();
    token = stream["token"].asString();
    prev_token = stream["expectedPreviousToken"].asString();

    if (url.size() == 0 || token.size() == 0 || play_service_id.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (cache_key.size()) {
        std::string filepath;
        if (isContentCached(cache_key, filepath)) {
            nugu_dbg("the content(key: %s) is cached in %s", cache_key.c_str(), filepath.c_str());
            url = filepath;
        } else {
            for (auto aplayer_listener : aplayer_listeners)
                aplayer_listener->requestContentCache(cache_key, url);
        }
    }

    report = stream["progressReport"];
    if (!report.empty()) {
        report_delay_time = report["progressReportDelayInMilliseconds"].asLargestInt();
        report_interval_time = report["progressReportIntervalInMilliseconds"].asLargestInt();
    }

    std::string playstackctl_ps_id = getPlayServiceIdInStackControl(root["playStackControl"]);

    if (!playstackctl_ps_id.empty()) {
        IPlaySyncManager::DisplayRenderer renderer;
        Json::Value meta = audio_item["metadata"];

        if (!meta.empty() && !meta["template"].empty()) {
            Json::StyledWriter writer;

            std::string id = composeRenderInfo(std::make_tuple(
                play_service_id,
                meta["template"]["type"].asString(),
                writer.write(meta["template"]),
                nugu_directive_peek_dialog_id(getNuguDirective()),
                root["token"].asString()));

            renderer.cap_name = getName();
            renderer.listener = this;
            renderer.display_id = id;
        }

        // sync display rendering with context
        playsync_manager->addContext(playstackctl_ps_id, getName(), std::move(renderer));
    }

    is_finished = false;

    nugu_dbg("= token: %s", token.c_str());
    nugu_dbg("= url: %s", url.c_str());
    nugu_dbg("= offset: %d", offset);
    nugu_dbg("= report_delay_time: %d", report_delay_time);
    nugu_dbg("= report_interval_time: %d", report_interval_time);

    std::string id = getReferrerDialogRequestId();
    // stop previous play for handling prev, next media play
    if (token != prev_token) {
        if (pre_ref_dialog_id.size())
            setReferrerDialogRequestId(pre_ref_dialog_id);

        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }
    }
    setReferrerDialogRequestId(id);
    pre_ref_dialog_id = id;
    cur_token = token;

    ps_id = play_service_id;
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

    capa_helper->requestFocus("cap_audio", NULL);
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
        capa_helper->sendCommand("AudioPlayer", "ASR", "releaseFocus", "");

        if (!player->pause()) {
            nugu_error("pause media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't pause");
        }

        capa_helper->releaseFocus("cap_audio");
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
        playsync_manager->removeContext(playstackctl_ps_id, getName(), !is_finished);
        std::string stacked_ps_id = playsync_manager->getPlayStackItem(getName());

        capa_helper->sendCommand("AudioPlayer", "ASR", "releaseFocus", "");

        if (!stacked_ps_id.empty() && playstackctl_ps_id != stacked_ps_id)
            return;
        else if (stacked_ps_id.empty())
            capa_helper->releaseFocus("cap_audio");

        if (!player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }
    }
}

void AudioPlayerAgent::parsingUpdateMetadata(const char* message)
{
    Json::Value root;
    Json::Value settings;
    Json::Reader reader;
    std::string playserviceid;

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

    if (root["metadata"].empty() || root["metadata"]["template"].empty() || root["metadata"]["template"]["settings"].empty()) {
        nugu_error("directive message syntex error");
        return;
    }

    settings = root["metadata"]["template"]["settings"];
    if (!settings["repeat"].empty()) {
        std::string repeat = settings["repeat"].asString();

        RepeatType type = RepeatType::NONE;
        if (repeat == "ONE")
            type = RepeatType::ONE;
        else if (repeat == "ALL")
            type = RepeatType::ALL;

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->repeatChanged(type);
    }
    if (!settings["favorite"].empty()) {
        bool favorite = settings["favorite"].asBool();

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->favoriteChanged(favorite);
    }
    if (!settings["shuffle"].empty()) {
        bool shuffle = settings["shuffle"].asBool();

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->shuffleChanged(shuffle);
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

    playserviceid = root["playServiceId"].asString();
    if (playserviceid.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }
    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (display_listener) {
        bool ret = display_listener->showLyrics();
        if (ret)
            sendEventShowLyricsSucceeded();
        else
            sendEventShowLyricsFailed();
    }
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

    playserviceid = root["playServiceId"].asString();
    if (playserviceid.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }
    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (display_listener) {
        bool ret = display_listener->hideLyrics();
        if (ret)
            sendEventHideLyricsSucceeded();
        else
            sendEventHideLyricsFailed();
    }
}

void AudioPlayerAgent::parsingControlLyricsPage(const char* message)
{
    Json::Value root;
    Json::Reader reader;
    std::string playserviceid;
    std::string direction;

    if (!reader.parse(message, root)) {
        nugu_error("parsing error");
        return;
    }

    playserviceid = root["playServiceId"].asString();
    direction = root["direction"].asString();
    if (playserviceid.size() == 0 || direction.size() == 0) {
        nugu_error("There is no mandatory data in directive message");
        return;
    }

    if (playserviceid != ps_id) {
        nugu_error("different play service id: %s (current play service id: %s)", playserviceid.c_str(), ps_id.c_str());
        return;
    }

    if (display_listener) {
        ControlLyricsDirection cdir = ControlLyricsDirection::PREVIOUS;
        if (direction == "NEXT")
            cdir = ControlLyricsDirection::NEXT;

        bool ret = display_listener->controlLyrics(cdir);
        if (ret)
            sendEventControlLyricsPageSucceeded();
        else
            sendEventControlLyricsPageFailed();
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
        capa_helper->releaseFocus("cap_audio");
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

void AudioPlayerAgent::onElementSelected(const std::string& item_token)
{
}

IPlaySyncManager* AudioPlayerAgent::getPlaySyncManager()
{
    return playsync_manager;
}

} // NuguCapability
