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
static const char* CAPABILITY_VERSION = "1.2";

AudioPlayerAgent::AudioPlayerAgent()
    : Capability(CAPABILITY_NAME, CAPABILITY_VERSION)
    , cur_player(nullptr)
    , media_player(nullptr)
    , tts_player(nullptr)
    , speak_dir(nullptr)
    , is_tts_activate(false)
    , cur_aplayer_state(AudioPlayerState::IDLE)
    , prev_aplayer_state(AudioPlayerState::IDLE)
    , is_paused(false)
    , is_steal_focus(false)
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

    media_player = core_container->createMediaPlayer();
    media_player->addListener(this);

    tts_player = core_container->createTTSPlayer();
    tts_player->addListener(this);

    cur_player = media_player;

    capa_helper->addFocus("cap_audio", NUGU_FOCUS_TYPE_MEDIA, this);

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

    initialized = true;
}

void AudioPlayerAgent::deInitialize()
{
    aplayer_listeners.clear();

    capa_helper->removeFocus("cap_audio");

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

    initialized = false;
}

void AudioPlayerAgent::suspend()
{
    if (suspend_policy == SuspendPolicy::PAUSE) {
        if (cur_aplayer_state == AudioPlayerState::PLAYING)
            capa_helper->releaseFocus("cap_audio");
    } else {
        if (cur_aplayer_state == AudioPlayerState::PLAYING || cur_aplayer_state == AudioPlayerState::PAUSED)
            stop();
    }
}

void AudioPlayerAgent::restore()
{
    if (suspend_policy == SuspendPolicy::PAUSE && cur_aplayer_state == AudioPlayerState::PAUSED)
        capa_helper->requestFocus("cap_audio", NULL);
}

void AudioPlayerAgent::directiveDataCallback(NuguDirective* ndir, void* userdata)
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

void AudioPlayerAgent::onFocus(void* event)
{
    if (is_paused) {
        nugu_warn("AudioPlayer is pause mode caused by directive(PAUSE)");
        return;
    }

    std::string type = is_tts_activate ? "attachment" : "streaming";
    nugu_dbg("cur_aplayer_state[%s] => %d, player->state() => %s", type.c_str(), cur_aplayer_state, cur_player->stateString(cur_player->state()).c_str());

    if (cur_player->state() == MediaPlayerState::PAUSED) {
        if (!cur_player->resume()) {
            nugu_error("resume media(%s) failed", type.c_str());
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
                "player can't resume media");
        }
    } else {
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

NuguFocusResult AudioPlayerAgent::onUnfocus(void* event, NuguUnFocusMode mode)
{
    if (mode == NUGU_UNFOCUS_FORCE) {
        cur_player->stop();
        return NUGU_FOCUS_REMOVE;
    }

    if (is_finished)
        return NUGU_FOCUS_REMOVE;

    if (cur_player->state() == MediaPlayerState::STOPPED || cur_player->state() == MediaPlayerState::PAUSED)
        return NUGU_FOCUS_REMOVE;

    if (!cur_player->pause()) {
        nugu_error("pause media failed");
        sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR,
            "player can't pause");
        return NUGU_FOCUS_REMOVE;
    }

    return NUGU_FOCUS_PAUSE;
}

NuguFocusStealResult AudioPlayerAgent::onStealRequest(void* event, NuguFocusType target_type)
{
    is_steal_focus = true;
    return NUGU_FOCUS_STEAL_ALLOW;
}

std::string AudioPlayerAgent::play()
{
    std::string id = sendEventByDisplayInterface("PlayCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::stop()
{
    std::string id = sendEventByDisplayInterface("StopCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::next()
{
    std::string id = sendEventByDisplayInterface("NextCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::prev()
{
    std::string id = sendEventByDisplayInterface("PreviousCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::pause()
{
    std::string id = sendEventByDisplayInterface("PauseCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::resume()
{
    std::string id = sendEventByDisplayInterface("PlayCommandIssued");
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

void AudioPlayerAgent::seek(int msec)
{
    if (cur_player)
        cur_player->seek(msec * 1000);
}

std::string AudioPlayerAgent::setFavorite(bool favorite)
{
    std::string ename = "FavoriteCommandIssued";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["favorite"] = favorite;
    payload = writer.write(root);

    std::string id = sendEvent(ename, getContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::setRepeat(RepeatType repeat)
{
    std::string ename = "RepeatCommandIssued";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
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

    std::string id = sendEvent(ename, getContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

std::string AudioPlayerAgent::setShuffle(bool shuffle)
{
    std::string ename = "ShuffleCommandIssued";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    root["shuffle"] = shuffle;
    payload = writer.write(root);

    std::string id = sendEvent(ename, getContextInfo(), payload);
    nugu_dbg("user request dialog id: %s", id.c_str());
    return id;
}

bool AudioPlayerAgent::setVolume(int volume)
{
    nugu_dbg("set media player's volume: %d", volume);
    if (!cur_player)
        return false;

    if (!cur_player->setVolume(volume))
        return false;

    nugu_dbg("media player's volume(%d) changed..", volume);
    return true;
}

bool AudioPlayerAgent::setMute(bool mute)
{
    nugu_dbg("set media player's mute: %d", mute);
    if (!cur_player)
        return false;

    if (cur_player->setMute(mute))
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
    else if (!strcmp(dname, "RequestPlayCommand"))
        parsingRequestPlayCommand(dname, message);
    else if (!strcmp(dname, "RequestResumeCommand")
        || !strcmp(dname, "RequestNextCommand")
        || !strcmp(dname, "RequestPreviousCommand")
        || !strcmp(dname, "RequestPauseCommand")
        || !strcmp(dname, "RequestStopCommand"))
        sendEventByRequestDirective(dname);
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
    if (cur_aplayer_state != AudioPlayerState::IDLE) {
        aplayer["token"] = cur_token;
        if (duration)
            aplayer["durationInMilliseconds"] = duration;
    }

    ctx[getName()] = aplayer;
}

void AudioPlayerAgent::receiveCommand(const std::string& from, const std::string& command, const std::string& param)
{
    std::string convert_command;
    convert_command.resize(command.size());
    std::transform(command.cbegin(), command.cend(), convert_command.begin(), ::tolower);

    if (!convert_command.compare("setvolume"))
        cur_player->setVolume(std::stoi(param));
}

void AudioPlayerAgent::setCapabilityListener(ICapabilityListener* listener)
{
    if (listener) {
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

void AudioPlayerAgent::sendEventPlaybackStarted(EventResultCallback cb)
{
    sendEventCommon("PlaybackStarted", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackFinished(EventResultCallback cb)
{
    sendEventCommon("PlaybackFinished", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackStopped(EventResultCallback cb)
{
    sendEventCommon("PlaybackStopped", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackPaused(EventResultCallback cb)
{
    sendEventCommon("PlaybackPaused", std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackResumed(EventResultCallback cb)
{
    sendEventCommon("PlaybackResumed", std::move(cb));
}

void AudioPlayerAgent::sendEventShowLyricsSucceeded(EventResultCallback cb)
{
    sendEventCommon("ShowLyricsSucceeded", std::move(cb));
}

void AudioPlayerAgent::sendEventShowLyricsFailed(EventResultCallback cb)
{
    sendEventCommon("EventShowLyricsFailed", std::move(cb));
}

void AudioPlayerAgent::sendEventHideLyricsSucceeded(EventResultCallback cb)
{
    sendEventCommon("HideLyricsSucceeded", std::move(cb));
}

void AudioPlayerAgent::sendEventHideLyricsFailed(EventResultCallback cb)
{
    sendEventCommon("HideLyricsFailed", std::move(cb));
}

void AudioPlayerAgent::sendEventControlLyricsPageSucceeded(EventResultCallback cb)
{
    sendEventCommon("ControlLyricsPageSucceeded", std::move(cb));
}

void AudioPlayerAgent::sendEventControlLyricsPageFailed(EventResultCallback cb)
{
    sendEventCommon("ControlLyricsPageFailed", std::move(cb));
}

void AudioPlayerAgent::sendEventRequestPlayCommandIssued(const std::string& dname, const std::string& payload, EventResultCallback cb)
{
    std::string ename = dname + "Issued";
    sendEvent(ename, getContextInfo(), payload, std::move(cb));
}

void AudioPlayerAgent::sendEventByRequestDirective(const std::string& dname, EventResultCallback cb)
{
    std::string ename = dname + "Issued";
    sendEventCommon(ename, std::move(cb));
}

void AudioPlayerAgent::sendEventPlaybackFailed(PlaybackError err, const std::string& reason, EventResultCallback cb)
{
    std::string ename = "PlaybackFailed";
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

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

std::string AudioPlayerAgent::sendEventByDisplayInterface(const std::string& command, EventResultCallback cb)
{
    return sendEventCommon(command, std::move(cb));
}

std::string AudioPlayerAgent::sendEventCommon(const std::string& ename, EventResultCallback cb)
{
    std::string payload = "";
    Json::StyledWriter writer;
    Json::Value root;
    long offset = cur_player->position() * 1000;

    if (offset < 0 && cur_token.size() == 0) {
        nugu_error("there is something wrong [%s]", ename.c_str());
        return "";
    }

    root["token"] = cur_token;
    root["playServiceId"] = ps_id;
    root["offsetInMilliseconds"] = std::to_string(offset);
    payload = writer.write(root);

    return sendEvent(ename, getContextInfo(), payload, std::move(cb));
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
    stream = audio_item["stream"];
    if (stream.empty()) {
        nugu_error("directive message syntex error");
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
        nugu_dbg("Receive Attachment media");
        has_attachment = true;
    } else {
        nugu_dbg("Receive Streaming media");
        if (url.size() == 0) {
            nugu_error("There is no mandatory data in directive message");
            return;
        }
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
        playsync_manager->addContext(playstackctl_ps_id, getName());
    }

    is_finished = false;
    is_steal_focus = false;

    if (speak_dir) {
        nugu_directive_set_data_callback(speak_dir, NULL, NULL);
        destroyDirective(speak_dir);
        speak_dir = nullptr;
    }

    nugu_dbg("= token: %s", token.c_str());
    nugu_dbg("= url: %s", url.c_str());
    nugu_dbg("= offset: %d", offset);
    nugu_dbg("= report_delay_time: %d", report_delay_time);
    nugu_dbg("= report_interval_time: %d", report_interval_time);

    std::string dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
    std::string dname = nugu_directive_peek_name(getNuguDirective());
    // Different tokens mean different media play requests.
    if (token != cur_token
        /* Temporary add condition: Service(keyword news) is always same token on 2020/03/12 */
        || has_attachment) {

        if (pre_ref_dialog_id.size())
            setReferrerDialogRequestId(dname, pre_ref_dialog_id);

        if (!cur_player->stop()) {
            nugu_error("stop media failed");
            sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't stop");
        }

        std::string id = nugu_directive_peek_dialog_id(getNuguDirective());
        setReferrerDialogRequestId(dname, id);
        pre_ref_dialog_id = id;
    } else {
        setReferrerDialogRequestId(dname, pre_ref_dialog_id);
    }
    cur_dialog_id = dialog_id;
    cur_token = token;
    ps_id = play_service_id;

    if (has_attachment) {
        cur_player = dynamic_cast<IMediaPlayer*>(tts_player);
        is_tts_activate = true;
        speak_dir = getNuguDirective();
    } else {
        cur_player = media_player;
        is_tts_activate = false;
    }

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

        // hold context about 10m' and remove it, if there are no action.
        playsync_manager->removeContextLater(playserviceid, getName(), PAUSE_CONTEXT_HOLD_TIME);

        if (cur_player->state() == MediaPlayerState::PAUSED && cur_aplayer_state == AudioPlayerState::PAUSED)
            sendEventPlaybackPaused();
        else {
            cur_dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
            if (!cur_player->pause()) {
                nugu_error("pause media failed");
                sendEventPlaybackFailed(PlaybackError::MEDIA_ERROR_INTERNAL_DEVICE_ERROR, "player can't pause");
            }
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

        if (speak_dir) {
            nugu_directive_set_data_callback(speak_dir, NULL, NULL);
            destroyDirective(speak_dir);
            speak_dir = nullptr;
        }

        cur_dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());
        if (!cur_player->stop()) {
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

    std::string dialog_id = nugu_directive_peek_dialog_id(getNuguDirective());

    settings = root["metadata"]["template"]["settings"];
    if (!settings["repeat"].empty()) {
        std::string repeat = settings["repeat"].asString();

        RepeatType type = RepeatType::NONE;
        if (repeat == "ONE")
            type = RepeatType::ONE;
        else if (repeat == "ALL")
            type = RepeatType::ALL;

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->repeatChanged(type, dialog_id);
    }
    if (!settings["favorite"].empty()) {
        bool favorite = settings["favorite"].asBool();

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->favoriteChanged(favorite, dialog_id);
    }
    if (!settings["shuffle"].empty()) {
        bool shuffle = settings["shuffle"].asBool();

        for (auto aplayer_listener : aplayer_listeners)
            aplayer_listener->shuffleChanged(shuffle, dialog_id);
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
        cur_aplayer_state = AudioPlayerState::PLAYING;
        if (!is_steal_focus) {
            if (prev_aplayer_state == AudioPlayerState::PAUSED)
                sendEventPlaybackResumed();
            else
                sendEventPlaybackStarted();
        }
        is_steal_focus = false;
        break;
    case MediaPlayerState::PAUSED:
        cur_aplayer_state = AudioPlayerState::PAUSED;
        if (!is_paused && !is_steal_focus)
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
        is_steal_focus = false;
        break;
    default:
        break;
    }
    nugu_info("[audioplayer state] %s -> %s", playerActivity(prev_aplayer_state).c_str(), playerActivity(cur_aplayer_state).c_str());

    if (prev_aplayer_state == cur_aplayer_state)
        return;

    for (auto aplayer_listener : aplayer_listeners)
        aplayer_listener->mediaStateChanged(cur_aplayer_state, cur_dialog_id);

    if (is_paused && cur_aplayer_state == AudioPlayerState::PAUSED) {
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
        capa_helper->releaseFocus("cap_audio");
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

} // NuguCapability
