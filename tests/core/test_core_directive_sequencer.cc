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

    /* Same handler for different namespaces. */
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

            /* Unblock the blocking directive after 500ms */
            idler = g_timeout_add(500, onTimeout, this);
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

            /* Unblock the blocking directive after 500ms */
            g_timeout_add(500, onTimeout, this);
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

    return g_test_run();
}
