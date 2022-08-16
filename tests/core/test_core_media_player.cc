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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/nugu_player.h"
#include "media_player.hh"
#include "mock/media_player_mock.h"
#include "mock/nugu_timer_mock.h"

#define TIMER_ELAPSE_MSEC(t)                                \
    {                                                       \
        for (int elap_msec = 0; elap_msec < t; elap_msec++) \
            fake_timer_elapse();                            \
    }

#define TIME_UNIT_SECOND 1000
#define TIME_UNIT_MINUTE 60 * TIME_UNIT_SECOND

using namespace NuguCore;

typedef struct {
    MediaPlayer* player;
} mplayerFixture;

static void setup(mplayerFixture* fixture, gconstpointer user_data)
{
    test_player_driver_initialize();
    fixture->player = new MediaPlayer();
}

static void teardown(mplayerFixture* fixture, gconstpointer user_data)
{
    delete fixture->player;
    test_player_driver_deinitialize();
}

#define G_TEST_ADD_FUNC(name, func) \
    g_test_add(name, mplayerFixture, NULL, setup, func, teardown);

static void test_mediaplayer_default(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1min = TIME_UNIT_MINUTE;

    player->setSource("http://test_content_1min");
    g_assert(player->state() == MediaPlayerState::PREPARE);
    g_assert(player->duration() == 0);
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1min);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 60);

    // check position for each 500 ms
    test_player_set_position(player->getNuguPlayer(), 500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 1);

    test_player_set_position(player->getNuguPlayer(), 1500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 1);

    test_player_set_position(player->getNuguPlayer(), 2000);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 2);
}

static void test_mediaplayer_playback_finished(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1min = TIME_UNIT_MINUTE;

    player->setSource("http://test_content_1min");
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1min);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 60);

    // ignore test during 0 ~ 59 sec
    TIMER_ELAPSE_MSEC(59 * TIME_UNIT_SECOND);

    // 59 sec
    test_player_set_position(player->getNuguPlayer(), TIME_UNIT_MINUTE - TIME_UNIT_SECOND);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 59);

    test_player_set_position(player->getNuguPlayer(), TIME_UNIT_MINUTE);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_END_OF_STREAM);
}

static void test_mediaplayer_playback_buffering(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1min = TIME_UNIT_MINUTE;

    player->setSource("http://test_content_1min");
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1min);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 60);

    // buffering delay: 200 ms
    test_player_set_position(player->getNuguPlayer(), 300);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 800);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1300);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 1);
}

static void test_mediaplayer_pause_resume_content(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1min = TIME_UNIT_MINUTE;

    player->setSource("http://test_content_1min");
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1min);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 60);

    // check position for each 1 sec
    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(TIME_UNIT_SECOND);
    g_assert(player->position() == 1);

    player->pause();
    g_assert(player->state() == MediaPlayerState::PAUSED);
    TIMER_ELAPSE_MSEC(TIME_UNIT_SECOND);
    g_assert(player->position() == 1);

    player->resume();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_position(player->getNuguPlayer(), 2000);
    TIMER_ELAPSE_MSEC(TIME_UNIT_SECOND);
    g_assert(player->position() == 2);
}

static void test_mediaplayer_play_stop_content(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1min = TIME_UNIT_MINUTE;

    player->setSource("http://test_content_1min");
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1min);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 60);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(TIME_UNIT_SECOND);
    g_assert(player->position() == 1);

    player->stop();
    g_assert(player->state() == MediaPlayerState::STOPPED);

    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);

    test_player_set_position(player->getNuguPlayer(), 500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 1);
}

static void test_mediaplayer_short_content(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1sec = TIME_UNIT_SECOND + 100; // 1.1 sec

    player->setSource("http://test_content_1sec");
    g_assert(player->state() == MediaPlayerState::PREPARE);
    g_assert(player->duration() == 0);
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1sec);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 1);

    test_player_set_position(player->getNuguPlayer(), 500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 1);

    test_player_set_position(player->getNuguPlayer(), 1100);
    TIMER_ELAPSE_MSEC(100);
    g_assert(player->position() == 1);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_END_OF_STREAM);
}

static void test_mediaplayer_short_content_with_timer_delay(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1sec = TIME_UNIT_SECOND + 100; // 1.1 sec
    int timer_delay_100ms = 100;

    player->setSource("http://test_content_1sec");
    g_assert(player->state() == MediaPlayerState::PREPARE);
    g_assert(player->duration() == 0);
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1sec);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 1);

    test_player_set_position(player->getNuguPlayer(), 500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    // software timer delay: 100 msec
    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(500 - timer_delay_100ms);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1100);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_END_OF_STREAM);
}

static void test_mediaplayer_short_content_with_timer_polling_faster(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1sec = TIME_UNIT_SECOND + 100; // 1.1 sec
    int timer_faster_50ms = 50;

    player->setSource("http://test_content_1sec");
    g_assert(player->state() == MediaPlayerState::PREPARE);
    g_assert(player->duration() == 0);
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1sec);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 1);

    test_player_set_position(player->getNuguPlayer(), 500);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000 - timer_faster_50ms);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(50);
    // can't get position 1 sec because the polling time is not reached.
    g_assert(player->position() == 0);

    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_END_OF_STREAM);
}

static void test_mediaplayer_short_content_with_buffering(mplayerFixture* fixture, gconstpointer ignored)
{
    MediaPlayer* player = fixture->player;
    int time_1sec = TIME_UNIT_SECOND + 100; // 1.1 sec

    player->setSource("http://test_content_1sec");
    g_assert(player->state() == MediaPlayerState::PREPARE);
    g_assert(player->duration() == 0);
    player->play();
    g_assert(player->state() == MediaPlayerState::PLAYING);
    test_player_set_duration(player->getNuguPlayer(), time_1sec);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_MEDIA_LOADED);
    g_assert(player->duration() == 1);

    // buffering delay: 200 ms
    test_player_set_position(player->getNuguPlayer(), 300);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 800);
    TIMER_ELAPSE_MSEC(500);
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1000);
    TIMER_ELAPSE_MSEC(200);
    // can't get position 1 sec because the polling time is not reached.
    g_assert(player->position() == 0);

    test_player_set_position(player->getNuguPlayer(), 1100);
    test_player_notify_event(player->getNuguPlayer(), NUGU_MEDIA_EVENT_END_OF_STREAM);
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    G_TEST_ADD_FUNC("/core/MediaPlayer/Default", test_mediaplayer_default);
    G_TEST_ADD_FUNC("/core/MediaPlayer/PlaybackFinished", test_mediaplayer_playback_finished);
    G_TEST_ADD_FUNC("/core/MediaPlayer/Buffering", test_mediaplayer_playback_buffering);
    G_TEST_ADD_FUNC("/core/MediaPlayer/PauseResumeContent", test_mediaplayer_pause_resume_content);
    G_TEST_ADD_FUNC("/core/MediaPlayer/PlayStopContent", test_mediaplayer_play_stop_content);
    G_TEST_ADD_FUNC("/core/MediaPlayer/ShortContent", test_mediaplayer_short_content);
    G_TEST_ADD_FUNC("/core/MediaPlayer/ShortContentWithTimerDelay", test_mediaplayer_short_content_with_timer_delay);
    G_TEST_ADD_FUNC("/core/MediaPlayer/ShortContentWithTimerPollingFaster", test_mediaplayer_short_content_with_timer_polling_faster);
    G_TEST_ADD_FUNC("/core/MediaPlayer/ShortContentWithBuffering", test_mediaplayer_short_content_with_buffering);

    return g_test_run();
}
