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

#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "directive_sequencer.hh"

using namespace NuguCore;

/* Generate dummy directive */
static NuguDirective* directive_new(const char* name_space, const char* name,
    const char* dialog_id, const char* msg_id)
{
    return nugu_directive_new(name_space, name, "1.0", msg_id, dialog_id,
        "ref1", "{}", "[]");
}

static void test_sequencer_policy(void)
{
    DirectiveSequencer seq;
    BlockingPolicy policy;

    /* Policy check */
    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("AudioPlayer", "Play", { BlockingMedium::AUDIO, false }) == true);

    policy = seq.getPolicy("TTS", "Speak");
    g_assert(policy.medium == BlockingMedium::AUDIO);
    g_assert(policy.isBlocking == true);

    policy = seq.getPolicy("AudioPlayer", "Play");
    g_assert(policy.medium == BlockingMedium::AUDIO);
    g_assert(policy.isBlocking == false);

    /* not registered policy: NONE with non-blocking */
    policy = seq.getPolicy("test", "test");
    g_assert(policy.medium == BlockingMedium::NONE);
    g_assert(policy.isBlocking == false);

    /* duplicated policy */
    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::NONE, false }) == false);
    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == false);
    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, false }) == false);
}

class CountAgent : public IDirectiveSequencerListener {
public:
    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        value++;

        /**
         * The directive is not handled in the pre-handle callback.
         */
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        value++;
        return true;
    }

    int value;
};

class CountPrehandleAgent : public IDirectiveSequencerListener {
public:
    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        value++;

        /**
         * The directive is handled in the pre-handle callback.
         */
        return true;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        g_assert(TRUE);

        return true;
    }

    int value;
};

static void test_sequencer_listener(void)
{
    DirectiveSequencer seq;
    CountAgent* counter1 = new CountAgent;
    CountPrehandleAgent* counter2 = new CountPrehandleAgent;
    NuguDirective* ndir;

    /* Simple listener invocation test */
    counter1->value = 0;
    ndir = directive_new("TTS", "test", "dlg1", "msg1");
    seq.addListener("TTS", counter1);
    g_assert(seq.add(ndir) == true);
    g_assert(counter1->value == 2);
    seq.complete(ndir);
    seq.removeListener("TTS", counter1);

    /* Remove listener test */
    counter1->value = 0;
    ndir = directive_new("TTS", "test", "dlg1", "msg1");
    g_assert(seq.add(ndir) == false);
    g_assert(counter1->value == 0);
    nugu_directive_unref(ndir);

    /* prehandle test */
    counter2->value = 0;
    ndir = directive_new("TTS", "test", "dlg1", "msg1");
    seq.addListener("TTS", counter2);
    g_assert(seq.add(ndir) == true);
    g_assert(counter2->value == 1); /* onHandleDirective ignored */
    seq.complete(ndir);
    seq.removeListener("TTS", counter2);

    /* multiple listener (counter1, counter2) test */
    counter1->value = 0;
    counter2->value = 0;
    seq.addListener("TTS", counter1);
    seq.addListener("TTS", counter2);
    ndir = directive_new("TTS", "test", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(counter1->value == 1); /* onHandleDirective ignored */
    g_assert(counter2->value == 1); /* onHandleDirective ignored */
    seq.complete(ndir);
    seq.removeListener("TTS", counter1);
    seq.removeListener("TTS", counter2);

    /* multiple listener (counter2, counter1) test */
    counter1->value = 0;
    counter2->value = 0;
    ndir = directive_new("TTS", "test", "dlg1", "msg1");
    seq.addListener("TTS", counter2);
    seq.addListener("TTS", counter1);
    g_assert(seq.add(ndir) == true);
    g_assert(counter1->value == 0); /* onPreHandleDirective and onHandleDirective ignored */
    g_assert(counter2->value == 1); /* onHandleDirective ignored */
    seq.complete(ndir);
    seq.removeListener("TTS", counter1);
    seq.removeListener("TTS", counter2);

    delete counter1;
    delete counter2;
}

class DummyAgent : public IDirectiveSequencerListener {
public:
    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        /* ignore TTS namespace */
        if (g_strcmp0(nugu_directive_peek_namespace(ndir), "TTS") == 0) {
            value += 100;
            return true;
        }

        /* only 'Dummy' namespace allow */
        g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "Dummy");

        if (g_strcmp0(nugu_directive_peek_name(ndir), "A") == 0) {
            value += 1;
        } else {
            value += 10;
        }

        return true;
    }

    int value;
};

static void test_sequencer_simple_add(void)
{
    DirectiveSequencer seq;
    DummyAgent* dummy = new DummyAgent;
    NuguDirective* ndir;

    seq.addListener("TTS", dummy);
    seq.addListener("Dummy", dummy);

    /* No corresponding listener */
    dummy->value = 0;
    ndir = directive_new("Invalid", "test", "dlg1", "msg1");
    g_assert(seq.add(ndir) == false);
    g_assert(dummy->value == 0);
    nugu_directive_unref(ndir);

    /* Corresponding listener */
    dummy->value = 0;
    ndir = directive_new("TTS", "invalid", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(dummy->value == 100);
    seq.complete(ndir);

    /* handled: case-2 */
    dummy->value = 0;
    ndir = directive_new("Dummy", "invalid", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(dummy->value == 10);
    seq.complete(ndir);

    /* handled: case-3 */
    dummy->value = 0;
    ndir = directive_new("Dummy", "A", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(dummy->value == 1);
    seq.complete(ndir);

    /* Remove 'TTS' listener: No corresponding listener */
    seq.removeListener("TTS", dummy);

    dummy->value = 0;
    ndir = directive_new("TTS", "invalid", "dlg1", "msg1");
    g_assert(seq.add(ndir) == false);
    g_assert(dummy->value == 0);
    nugu_directive_unref(ndir);

    delete dummy;
}

class TTSAgent : public IDirectiveSequencerListener {
public:
    TTSAgent(DirectiveSequencer* seq, GMainLoop* loop)
        : seq(seq)
        , loop(loop)
        , speak_dir(nullptr)
        , idler(0)
    {
    }
    virtual ~TTSAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
        g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "TTS");
        cancel_flag++;
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        /* Allow only 'TTS' namespace */
        g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "TTS");
        value += 1;

        if (g_strcmp0(nugu_directive_peek_name(ndir), "Speak") == 0) {
            /**
             * Multiple dialog-id test case:
             *  - Remove previous dialog-id related directive
             */
            if (speak_dir != nullptr) {
                /* The dialog-id must be different */
                g_assert_cmpstr(nugu_directive_peek_dialog_id(speak_dir), !=, nugu_directive_peek_dialog_id(ndir));

                /* Cancel(destroy) the previous dialog */
                cancel_flag = 0;
                seq->cancel(nugu_directive_peek_dialog_id(speak_dir));
                g_assert(cancel_flag != 0);

                if (idler)
                    g_source_remove(idler);
            }

            speak_dir = ndir;

            /* Unblock the blocking directive after 100ms */
            idler = g_timeout_add(100, onTimeout, this);
        } else if (g_strcmp0(nugu_directive_peek_name(ndir), "Quit") == 0) {
            seq->complete(ndir);
            g_main_loop_quit(loop);
        } else {
            seq->complete(ndir);
        }

        return true;
    }

    static gboolean onTimeout(gpointer userdata)
    {
        TTSAgent* agent = static_cast<TTSAgent*>(userdata);
        agent->idler = 0;
        agent->speakFinished();

        return FALSE;
    }

    void speakFinished(void)
    {
        g_assert(speak_dir != nullptr);
        g_assert(value == 1);

        seq->complete(speak_dir);

        speak_dir = nullptr;
    }

    int value;
    DirectiveSequencer* seq;
    GMainLoop* loop;
    NuguDirective* speak_dir;
    guint idler;
    int cancel_flag;
};

class AudioPlayerAgent : public IDirectiveSequencerListener {
public:
    AudioPlayerAgent(DirectiveSequencer* seq, GMainLoop* loop)
        : seq(seq)
        , loop(loop)
    {
    }
    virtual ~AudioPlayerAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "AudioPlayer");
        value += 1;

        if (g_strcmp0(nugu_directive_peek_name(ndir), "Play") == 0) {
            seq->complete(ndir);
            g_main_loop_quit(loop);
        } else {
            seq->complete(ndir);
        }

        return true;
    }

    int value;
    DirectiveSequencer* seq;
    GMainLoop* loop;
};

static void test_sequencer_simple_blocking(void)
{
    DirectiveSequencer seq;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    TTSAgent* tts = new TTSAgent(&seq, loop);
    AudioPlayerAgent* audio = new AudioPlayerAgent(&seq, loop);

    seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true });
    seq.addPolicy("AudioPlayer", "Play", { BlockingMedium::AUDIO, false });

    seq.addListener("TTS", tts);
    seq.addListener("AudioPlayer", audio);

    tts->value = 0;
    audio->value = 0;

    /* Handle audio medium (block) */
    seq.add(directive_new("TTS", "Speak", "dlg1", "msg1"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);

    /* Pending audio medium (non-block) */
    seq.add(directive_new("AudioPlayer", "Play", "dlg1", "msg2"));
    g_assert(audio->value == 0);

    /* TTS.Speak(block) is completed after timer */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* All directives are handled */
    g_assert(tts->value == 1);
    g_assert(audio->value == 1);

    delete tts;
    delete audio;
}

class VisualAgent : public IDirectiveSequencerListener {
public:
    VisualAgent(DirectiveSequencer* seq, GMainLoop* loop)
        : seq(seq)
        , loop(loop)
    {
    }
    virtual ~VisualAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        g_assert_cmpstr(nugu_directive_peek_namespace(ndir), ==, "Visual");
        value += 1;

        if (g_strcmp0(nugu_directive_peek_name(ndir), "Block") == 0) {
            block_dir = ndir;

            /* Unblock the blocking directive after 100ms */
            g_timeout_add(100, onTimeout, this);
        } else if (g_strcmp0(nugu_directive_peek_name(ndir), "Signal") == 0) {
            seq->complete(ndir);
            g_main_loop_quit(loop);
        } else if (g_strcmp0(nugu_directive_peek_name(ndir), "Nonblock") == 0) {
            seq->complete(ndir);
        }

        return true;
    }

    static gboolean onTimeout(gpointer userdata)
    {
        VisualAgent* agent = static_cast<VisualAgent*>(userdata);

        /* Done: "Nonblock", "Block", Pending: "Signal" */
        g_assert(agent->value == 2);

        agent->blockFinished();

        return FALSE;
    }

    void blockFinished(void)
    {
        seq->complete(block_dir);
    }

    int value;
    DirectiveSequencer* seq;
    NuguDirective* block_dir;
    GMainLoop* loop;
};

static void test_sequencer_multi_blocking(void)
{
    DirectiveSequencer seq;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    TTSAgent* tts = new TTSAgent(&seq, loop);
    AudioPlayerAgent* audio = new AudioPlayerAgent(&seq, loop);
    VisualAgent* visual = new VisualAgent(&seq, loop);

    seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true });
    seq.addPolicy("AudioPlayer", "Dummy", { BlockingMedium::AUDIO, false });
    seq.addPolicy("Visual", "Nonblock", { BlockingMedium::VISUAL, false });
    seq.addPolicy("Visual", "Block", { BlockingMedium::VISUAL, true });
    seq.addPolicy("Visual", "Signal", { BlockingMedium::VISUAL, false });

    seq.addListener("TTS", tts);
    seq.addListener("AudioPlayer", audio);
    seq.addListener("Visual", visual);

    tts->value = 0;
    audio->value = 0;
    visual->value = 0;

    /* Handle audio medium (block) */
    seq.add(directive_new("TTS", "Speak", "dlg1", "msg1"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);
    g_assert(visual->value == 0);

    /* Pending audio medium (non-block) */
    seq.add(directive_new("AudioPlayer", "Dummy", "dlg1", "msg2"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);
    g_assert(visual->value == 0);

    /* Handle visual medium (non-block) */
    seq.add(directive_new("Visual", "Nonblock", "dlg1", "msg3"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);
    g_assert(visual->value == 1);

    /* Handle visual medium (block) */
    seq.add(directive_new("Visual", "Block", "dlg1", "msg4"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);
    g_assert(visual->value == 2);

    /* Pending visual medium (non-block) */
    seq.add(directive_new("Visual", "Signal", "dlg1", "msg5"));
    g_assert(tts->value == 1);
    g_assert(audio->value == 0);
    g_assert(visual->value == 2);

    /* TTS.Speak(block) and Visual.Block(block) are completed after timer */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* All directives are handled */
    g_assert(tts->value == 1);
    g_assert(audio->value == 1);
    g_assert(visual->value == 3);

    delete tts;
    delete audio;
    delete visual;
}

static void test_sequencer_multi_dialog(void)
{
    DirectiveSequencer seq;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    TTSAgent* tts = new TTSAgent(&seq, loop);

    seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true });
    seq.addPolicy("TTS", "Quit", { BlockingMedium::AUDIO, false });
    seq.addListener("TTS", tts);

    tts->value = 0;

    /* Multiple dialog-id with non-block directive */
    seq.add(directive_new("TTS", "Dummy", "dlg1", "msg1"));
    g_assert(tts->value == 1);
    seq.add(directive_new("TTS", "Dummy", "dlg2", "msg2"));
    g_assert(tts->value == 2);

    tts->value = 0;

    /* dialog 1 (block + non-block) */
    seq.add(directive_new("TTS", "Speak", "dlg1", "msg3"));
    g_assert(tts->value == 1);
    seq.add(directive_new("TTS", "Quit", "dlg1", "msg4"));
    g_assert(tts->value == 1);

    /* dialog 2 (block + non-block) */
    seq.add(directive_new("TTS", "Speak", "dlg2", "msg5"));
    g_assert(tts->value == 2);
    seq.add(directive_new("TTS", "Quit", "dlg2", "msg6"));
    g_assert(tts->value == 2);
    tts->value--;

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_assert(tts->value == 2);

    delete tts;
}

static void test_sequencer_reset(void)
{
    DirectiveSequencer seq;
    CountAgent* counter1 = new CountAgent;
    NuguDirective* ndir;
    BlockingPolicy policy;

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("AudioPlayer", "Play", { BlockingMedium::AUDIO, false }) == true);

    policy = seq.getPolicy("TTS", "Speak");
    g_assert(policy.medium == BlockingMedium::AUDIO);
    g_assert(policy.isBlocking == true);

    policy = seq.getPolicy("AudioPlayer", "Play");
    g_assert(policy.medium == BlockingMedium::AUDIO);
    g_assert(policy.isBlocking == false);

    seq.addListener("Test", counter1);
    counter1->value = 0;
    ndir = directive_new("Test", "1", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(counter1->value == 2);
    seq.complete(ndir);

    /* Reset all state without listeners */
    seq.reset();

    /* Policy info removed */
    policy = seq.getPolicy("TTS", "Speak");
    g_assert(policy.medium == BlockingMedium::NONE);
    g_assert(policy.isBlocking == false);

    policy = seq.getPolicy("AudioPlayer", "Play");
    g_assert(policy.medium == BlockingMedium::NONE);
    g_assert(policy.isBlocking == false);

    /* Listener info existed */
    counter1->value = 0;
    ndir = directive_new("Test", "1", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    g_assert(counter1->value == 2);

    seq.complete(ndir);
    delete counter1;
}

typedef struct _CallbackData {
    gpointer agent;
    NuguDirective* ndir;
} CallbackData;

class ASyncAgent : public IDirectiveSequencerListener {
public:
    ASyncAgent(DirectiveSequencer* seq, GMainLoop* loop, std::string& buffer)
        : seq(seq)
        , loop(loop)
        , buffer(buffer)
    {
    }
    virtual ~ASyncAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        CallbackData* cd = new CallbackData();
        cd->agent = this;
        cd->ndir = ndir;

        /* Append start('S') mark */
        std::string temp = "[S:";
        temp.append(nugu_directive_peek_msg_id(ndir));
        temp.append("]");

        buffer.append(temp);
        g_idle_add(onIdle, cd);
        return true;
    }

    static gboolean onIdle(gpointer userdata)
    {
        CallbackData* cd = static_cast<CallbackData*>(userdata);
        ASyncAgent* agent = static_cast<ASyncAgent*>(cd->agent);
        bool isExit = false;

        if (g_strcmp0(nugu_directive_peek_msg_id(cd->ndir), "msgquit") == 0)
            isExit = true;

        /* Append end('E') mark */
        std::string temp = "[E:";
        temp.append(nugu_directive_peek_msg_id(cd->ndir));
        temp.append("]");

        agent->buffer.append(temp);
        agent->seq->complete(cd->ndir);

        delete cd;

        if (isExit)
            g_main_loop_quit(agent->loop);

        return FALSE;
    }

    DirectiveSequencer* seq;
    GMainLoop* loop;
    std::string& buffer;
};

class SyncAgent : public IDirectiveSequencerListener {
public:
    SyncAgent(DirectiveSequencer* seq, GMainLoop* loop, std::string& buffer)
        : seq(seq)
        , loop(loop)
        , buffer(buffer)
    {
    }
    virtual ~SyncAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        /* Append start('S') mark */
        std::string temp = "[S:";
        temp.append(nugu_directive_peek_msg_id(ndir));
        temp.append("]");

        /* Append end('E') mark */
        temp.append("[E:");
        temp.append(nugu_directive_peek_msg_id(ndir));
        temp.append("]");

        buffer.append(temp);

        if (g_strcmp0(nugu_directive_peek_msg_id(ndir), "msgquit") == 0) {
            seq->complete(ndir);
            g_main_loop_quit(loop);
        } else {
            seq->complete(ndir);
        }

        return true;
    }

    DirectiveSequencer* seq;
    GMainLoop* loop;
    std::string& buffer;
};

static void test_sequencer_any1(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("TTS", async);
    seq.addListener("Utility", async);
    seq.addListener("Extension", async);
    seq.addListener("Text", async);

    ndir = directive_new("TTS", "Speak", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Extension", "Action", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Text", "TextRedirect", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
}

static void test_sequencer_any1_sync(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);
    SyncAgent* sync = new SyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("TTS", async);
    seq.addListener("Utility", async);
    seq.addListener("Extension", sync);
    seq.addListener("Text", sync);

    ndir = directive_new("TTS", "Speak", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Extension", "Action", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Text", "TextRedirect", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
    delete sync;
}

static void test_sequencer_any2(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][S:msg2][E:msg1][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("AudioPlayer", "Play", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);
    g_assert(seq.addPolicy("Display", "Visual", { BlockingMedium::VISUAL, true }) == true);

    seq.addListener("TTS", async);
    seq.addListener("AudioPlayer", async);
    seq.addListener("Utility", async);
    seq.addListener("Dummy", async);
    seq.addListener("Display", async);

    ndir = directive_new("AudioPlayer", "Play", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Display", "Visual", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("AudioPlayer", "Play", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
}

static void test_sequencer_any2_sync(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);
    SyncAgent* sync = new SyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("AudioPlayer", "Play", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);
    g_assert(seq.addPolicy("Display", "Visual", { BlockingMedium::VISUAL, true }) == true);

    seq.addListener("TTS", async);
    seq.addListener("AudioPlayer", sync);
    seq.addListener("Utility", async);
    seq.addListener("Dummy", sync);
    seq.addListener("Display", sync);

    ndir = directive_new("AudioPlayer", "Play", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Display", "Visual", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("AudioPlayer", "Play", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
    delete sync;
}

static void test_sequencer_any3(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][S:msg2][E:msg1][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4]"
                           "[S:msgquit][E:msg4][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("Utility", async);
    seq.addListener("Dummy", async);

    ndir = directive_new("Dummy", "None", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
}

static void test_sequencer_any3_sync(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);
    SyncAgent* sync = new SyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("Utility", async);
    seq.addListener("Dummy", sync);

    ndir = directive_new("Dummy", "None", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
    delete sync;
}

static void test_sequencer_any4(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("Utility", async);
    seq.addListener("Dummy", async);

    ndir = directive_new("Utility", "Block", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /*
     * Debug messages comparing expected and actual results.
     * nugu_error("\n%s\n%s", expected.c_str(), buffer.c_str());
     */

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
}

static void test_sequencer_any4_sync(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg2][E:msg2]"
                           "[S:msg3][E:msg3][S:msg4][E:msg4]"
                           "[S:msgquit][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);
    SyncAgent* sync = new SyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("Utility", async);
    seq.addListener("Dummy", sync);

    ndir = directive_new("Utility", "Block", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Dummy", "None", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);
    ndir = directive_new("Utility", "Block", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /*
     * Debug messages comparing expected and actual results.
     * nugu_error("\n%s\n%s", expected.c_str(), buffer.c_str());
     */

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
    delete sync;
}

class IgnoreAgent : public IDirectiveSequencerListener {
public:
    IgnoreAgent(GMainLoop* loop, std::string& buffer)
        : loop(loop)
        , buffer(buffer)
    {
    }
    virtual ~IgnoreAgent() = default;

    bool onPreHandleDirective(NuguDirective* ndir) override
    {
        return false;
    }

    void onCancelDirective(NuguDirective* ndir) override
    {
        /* Append cancel('C') mark */
        std::string temp = "[C:";
        temp.append(nugu_directive_peek_msg_id(ndir));
        temp.append("]");

        buffer.append(temp);
    }

    bool onHandleDirective(NuguDirective* ndir) override
    {
        /* Append start('S') mark */
        std::string temp = "[S:";
        temp.append(nugu_directive_peek_msg_id(ndir));
        temp.append("]");

        buffer.append(temp);

        if (g_strcmp0(nugu_directive_peek_msg_id(ndir), "msgquit") == 0)
            g_main_loop_quit(loop);

        return true;
    }

    GMainLoop* loop;
    std::string& buffer;
};

static void test_sequencer_cancel1(void)
{
    DirectiveSequencer seq;
    NuguDirective* ndir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][E:msg1][S:msg4][S:msgquit]"
                           "[E:msg4][E:msgquit]";
    ASyncAgent* async = new ASyncAgent(&seq, loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("TTS", async);
    seq.addListener("PhoneCall", async);
    seq.addListener("Utility", async);
    seq.addListener("Dummy", async);

    ndir = directive_new("TTS", "Speak", "dlg1", "msg1");
    g_assert(seq.add(ndir) == true);

    /**
     * Group cancel after 'TTS.Speak'.
     * ('TTS.Speak' is first completed by the async agent.)
     * 'Dummy.alive' (msg4) is activated after this idle callback.
     */
    g_idle_add(
        [](gpointer userdata) -> int {
            DirectiveSequencer* seq = (DirectiveSequencer*)userdata;

            seq->cancel("dlg1", { "PhoneCall.MakeCall", "Utility.Block", "Dummy.dead" });

            return FALSE;
        },
        &seq);

    ndir = directive_new("Utility", "Block", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("PhoneCall", "MakeCall", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Dummy", "alive", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Dummy", "dead", "dlg1", "msg5");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Dummy", "quit", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete async;
}

static void test_sequencer_cancel2(void)
{
    DirectiveSequencer seq;
    NuguDirective *ndir, *speak_dir;
    std::string buffer;
    std::string expected = "[S:msg1][C:msg3][C:msg2][S:msg4][S:msg5][C:msg4][C:msg5]";

    IgnoreAgent* agent = new IgnoreAgent(NULL, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("TTS", agent);
    seq.addListener("Utility", agent);
    seq.addListener("PhoneCall", agent);

    speak_dir = directive_new("TTS", "Speak", "dlg1", "msg1");
    g_assert(seq.add(speak_dir) == true);

    ndir = directive_new("Utility", "Block", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("PhoneCall", "MakeCall", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);

    /* Groups are sorted in ascending order by default. */
    seq.cancel("dlg1", { "Utility.Block", "PhoneCall.MakeCall" });
    seq.complete(speak_dir);

    ndir = directive_new("TTS", "Dummy", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("TTS", "Dummy", "dlg1", "msg5");
    g_assert(seq.add(ndir) == true);

    /* Cancel all(msg4, msg5) directives */
    seq.cancel("dlg1");

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete agent;
}

struct cancel3_data {
    DirectiveSequencer* seq;
    NuguDirective* speak_dir;
};

static void test_sequencer_cancel3(void)
{
    DirectiveSequencer seq;
    NuguDirective *ndir, *speak_dir;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    std::string buffer;
    std::string expected = "[S:msg1][C:msg3][C:msg2][C:msg4][S:msgquit][C:msgquit]";
    struct cancel3_data data;

    IgnoreAgent* agent = new IgnoreAgent(loop, buffer);

    g_assert(seq.addPolicy("TTS", "Speak", { BlockingMedium::AUDIO, true }) == true);
    g_assert(seq.addPolicy("Utility", "Block", { BlockingMedium::ANY, true }) == true);

    seq.addListener("TTS", agent);
    seq.addListener("Utility", agent);
    seq.addListener("Extension", agent);
    seq.addListener("Text", agent);

    speak_dir = directive_new("TTS", "Speak", "dlg1", "msg1");
    g_assert(seq.add(speak_dir) == true);

    ndir = directive_new("Utility", "Block", "dlg1", "msg2");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Extension", "Action", "dlg1", "msg3");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Utility", "Block", "dlg1", "msg4");
    g_assert(seq.add(ndir) == true);

    ndir = directive_new("Text", "TextRedirect", "dlg1", "msgquit");
    g_assert(seq.add(ndir) == true);

    data.seq = &seq;
    data.speak_dir = speak_dir;

    /**
     * Cancel the 'Extension.Action' and Complete the 'TTS.Speak'.
     * 'Utility.Block' (msg2) is activated after this idle callback.
     */
    g_idle_add(
        [](gpointer userdata) -> int {
            struct cancel3_data* data = (struct cancel3_data*)userdata;
            std::set<std::string> groups = { "Extension.Action" };

            data->seq->cancel("dlg1", groups);
            data->seq->complete(data->speak_dir);

            return FALSE;
        },
        &data);

    /**
     * Cancel 'Utility.Block' to clear blocking status.
     * 'Text.TextRedirect' (msgquit) is activated after this idle callback.
     */
    g_idle_add(
        [](gpointer userdata) -> int {
            struct cancel3_data* data = (struct cancel3_data*)userdata;
            std::set<std::string> groups = { "Utility.Block" };

            /* Cancel the 'msg2' and 'msg4' directives */
            data->seq->cancel("dlg1", groups);

            return FALSE;
        },
        &data);

    /* Start mainloop */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    /* Cancel all remain directives */
    seq.cancel("dlg1");

    /* check directive handle sequence */
    g_assert(buffer == expected);

    delete agent;
}

int main(int argc, char* argv[])
{
#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif

    g_test_init(&argc, &argv, (void*)NULL);
    g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);

    g_test_add_func("/core/DirectiveSequencer/policy", test_sequencer_policy);
    g_test_add_func("/core/DirectiveSequencer/listener", test_sequencer_listener);
    g_test_add_func("/core/DirectiveSequencer/simple_add", test_sequencer_simple_add);
    g_test_add_func("/core/DirectiveSequencer/simple_blocking", test_sequencer_simple_blocking);
    g_test_add_func("/core/DirectiveSequencer/multi_blocking", test_sequencer_multi_blocking);
    g_test_add_func("/core/DirectiveSequencer/multi_dialog", test_sequencer_multi_dialog);
    g_test_add_func("/core/DirectiveSequencer/reset", test_sequencer_reset);
    g_test_add_func("/core/DirectiveSequencer/any1", test_sequencer_any1);
    g_test_add_func("/core/DirectiveSequencer/any1_sync", test_sequencer_any1_sync);
    g_test_add_func("/core/DirectiveSequencer/any2", test_sequencer_any2);
    g_test_add_func("/core/DirectiveSequencer/any2_sync", test_sequencer_any2_sync);
    g_test_add_func("/core/DirectiveSequencer/any3", test_sequencer_any3);
    g_test_add_func("/core/DirectiveSequencer/any3_sync", test_sequencer_any3_sync);
    g_test_add_func("/core/DirectiveSequencer/any4", test_sequencer_any4);
    g_test_add_func("/core/DirectiveSequencer/any4_sync", test_sequencer_any4_sync);
    g_test_add_func("/core/DirectiveSequencer/cancel1", test_sequencer_cancel1);
    g_test_add_func("/core/DirectiveSequencer/cancel2", test_sequencer_cancel2);
    g_test_add_func("/core/DirectiveSequencer/cancel3", test_sequencer_cancel3);

    return g_test_run();
}
