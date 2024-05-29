#include <glib.h>
#include <unistd.h>

#include <iostream>

#define NUGU_LOG_MODULE NUGU_LOG_MODULE_APPLICATION
#include <base/nugu_log.h>

#include <base/nugu_prof.h>
#include <capability/asr_interface.hh>
#include <capability/audio_player_interface.hh>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <capability/text_interface.hh>
#include <capability/tts_interface.hh>
#include <clientkit/nugu_client.hh>

#define DEFAULT_MODEL_PATH NUGU_ASSET_PATH "/model"

using namespace NuguClientKit;
using namespace NuguCapability;

static std::shared_ptr<IASRHandler> asr_handler = nullptr;
static char prof_flag[NUGU_PROF_TYPE_MAX];
static int test_iteration = 0;
static int test_iteration_max = 1;
static int test_delay = 1;
static int test_timeout = 60;
static int test_network_timeout = 60;
static std::string test_name;
static GMainLoop* loop;
static FILE* fp_out;
static guint timer_src;
static guint network_timer_src;

static void _prof_handler(enum nugu_prof_type type, const struct nugu_prof_data* data, void* userdata)
{
    if (prof_flag[type] == 0)
        return;

    nugu_dbg("\t>> %s", nugu_prof_get_type_name(type));
}

static gboolean _network_timeout_cb(gpointer userdata)
{
    nugu_error("Network connection timeout !!!. Stop the tests");

    g_main_loop_quit(loop);

    network_timer_src = 0;

    return FALSE;
}

static gboolean _fail_timeout_cb(gpointer userdata)
{
    nugu_error("Timeout !!!. Stop the tests");

    g_main_loop_quit(loop);

    timer_src = 0;

    return FALSE;
}

static gboolean _delay_timeout_cb(gpointer userdata)
{
    timer_src = g_timeout_add_seconds(test_timeout, _fail_timeout_cb, NULL);

    nugu_info("Start %d/%d (timeout %d secs)", test_iteration, test_iteration_max, test_timeout);

    asr_handler->startRecognition();

    return FALSE;
}

static void test_start(void)
{
    test_iteration++;

    nugu_dbg("Schedule the test %d/%d (start after %d secs)",
        test_iteration, test_iteration_max, test_delay);

    g_timeout_add_seconds(test_delay, _delay_timeout_cb, NULL);
}

static void test_finished(void)
{
    struct nugu_prof_data* tmp;

    if (timer_src)
        g_source_remove(timer_src);

    nugu_info("Finished %d", test_iteration);

    tmp = nugu_prof_get_last_data(NUGU_PROF_TYPE_ASR_RECOGNIZE);
    if (!tmp) {
        nugu_error("Profiling failed");
        g_main_loop_quit(loop);
        return;
    }

    /* CSV data, NOLINTNEXTLINE(cert-err33-c) */
    fprintf(fp_out, "%s,%d,%ld,%d,%d,%d,%d,%d,%s\n",
        test_name.c_str(), test_iteration, tmp->timestamp.tv_sec,
        nugu_prof_get_diff_msec_type(NUGU_PROF_TYPE_ASR_LISTENING_STARTED, NUGU_PROF_TYPE_ASR_END_POINT_DETECTED),
        nugu_prof_get_diff_msec_type(NUGU_PROF_TYPE_ASR_END_POINT_DETECTED, NUGU_PROF_TYPE_ASR_RESULT),
        nugu_prof_get_diff_msec_type(NUGU_PROF_TYPE_ASR_RESULT, NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE),
        nugu_prof_get_diff_msec_type(NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE, NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT),
        nugu_prof_get_diff_msec_type(NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT, NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE),
        tmp->dialog_id);

    free(tmp);

    if (test_iteration >= test_iteration_max) {
        nugu_info("All tests finished");
        g_main_loop_quit(loop);
        return;
    }

    /* Trigger next iteration */
    test_start();
}

static void usage(const char* cmd)
{
    printf("Profiling measurement tool using pre-recorded voice files.\n\n");
    printf("Usage: %s [OPTIONS] -i <input-file> -o <output-file>\n", cmd);
    printf(" -i <input-file>\tPre-recorded voice file (Raw PCM: 16000Hz, mono, s16le)\n");
    printf(" -o <output-file>\tFile to save reporting results (CSV format)\n");
    printf(" -n count\t\tNumber of test iterations. (default = %d)\n", test_iteration_max);
    printf(" -m model_path\t\tSet the ASR Model path. (default = %s)\n", DEFAULT_MODEL_PATH);
    printf(" -d delay\t\tDelay between each test iteration. (seconds, default = %d)\n", test_delay);
    printf(" -t timeout\t\tTimeout for each tests. (seconds, default = %d)\n", test_timeout);
    printf(" -c name\t\tName for testcase. (default = input-file)\n");
    printf("\nPlease set the token using the NUGU_TOKEN environment variable.\n");
}

class MyTTSListener : public ITTSListener {
public:
    virtual ~MyTTSListener() = default;

    void onTTSState(TTSState state, const std::string& dialog_id) override
    {
        switch (state) {
        case TTSState::TTS_SPEECH_FINISH:
            test_finished();
            break;
        default:
            break;
        }
    }

    void onTTSText(const std::string& text, const std::string& dialog_id) override
    {
    }

    void onTTSCancel(const std::string& dialog_id) override
    {
    }
};

class MyASR : public IASRListener {
public:
    virtual ~MyASR() = default;

    void onState(ASRState state, const std::string& dialog_id, ASRInitiator initiator) override
    {
    }

    void onNone(const std::string& dialog_id) override
    {
    }

    void onPartial(const std::string& text, const std::string& dialog_id) override
    {
    }

    void onComplete(const std::string& text, const std::string& dialog_id) override
    {
    }

    void onError(ASRError error, const std::string& dialog_id, bool listen_timeout_fail_beep) override
    {
        switch (error) {
        case ASRError::RESPONSE_TIMEOUT:
            nugu_error("ASR response timeout (dialog id: %s)", dialog_id.c_str());
            break;
        case ASRError::LISTEN_TIMEOUT:
            nugu_error("ASR listen timeout (dialog id: %s)", dialog_id.c_str());
            break;
        case ASRError::LISTEN_FAILED:
            nugu_error("ASR listen failed (dialog id: %s)", dialog_id.c_str());
            break;
        case ASRError::RECOGNIZE_ERROR:
            nugu_error("ASR recognition error (dialog id: %s)", dialog_id.c_str());
            break;
        case ASRError::UNKNOWN:
            nugu_error("ASR unknown error (dialog id: %s)", dialog_id.c_str());
            break;
        }
    }

    void onCancel(const std::string& dialog_id) override
    {
    }

    void setExpectSpeechState(bool is_es_state)
    {
    }
};

class MyNetwork : public INetworkManagerListener {
public:
    void onStatusChanged(NetworkStatus status) override
    {
        switch (status) {
        case NetworkStatus::DISCONNECTED:
            nugu_error("Network disconnected");
            g_main_loop_quit(loop);
            break;
        case NetworkStatus::CONNECTING:
            nugu_dbg("Network connecting");
            break;
        case NetworkStatus::READY:
            nugu_dbg("Network ready");
            if (network_timer_src) {
                g_source_remove(network_timer_src);
                network_timer_src = 0;
            }
            test_start();
            break;
        case NetworkStatus::CONNECTED:
            nugu_dbg("Network connected");
            if (network_timer_src) {
                g_source_remove(network_timer_src);
                network_timer_src = 0;
            }
            test_start();
            break;
        default:
            break;
        }
    }

    void onError(NetworkError error) override
    {
        switch (error) {
        case NetworkError::FAILED:
            nugu_error("Network failed");
            g_main_loop_quit(loop);
            break;
        case NetworkError::TOKEN_ERROR:
            nugu_error("Token error");
            g_main_loop_quit(loop);
            break;
        case NetworkError::UNKNOWN:
            nugu_error("Unknown network error");
            break;
        }
    }
};

int main(int argc, char* argv[])
{
    std::shared_ptr<NuguClient> nugu_client;
    int c;
    std::string input_file;
    std::string output_file;
    std::string model_path = DEFAULT_MODEL_PATH;
    const char* env_token;

    while ((c = getopt(argc, argv, "i:o:n:m:d:t:c:")) != -1) {
        switch (c) {
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'n':
            test_iteration_max = std::strtol(optarg, nullptr, 10);
            break;
        case 'm':
            model_path = optarg;
            break;
        case 'd':
            test_delay = std::strtol(optarg, nullptr, 10);
            break;
        case 't':
            test_timeout = std::strtol(optarg, nullptr, 10);
            break;
        case 'c':
            test_name = optarg;
            break;
        default:
            usage(argv[0]);
            return -1;
        }
    }

    env_token = getenv("NUGU_TOKEN");
    if (env_token == NULL) {
        printf("Please set the token using the NUGU_TOKEN environment variable.\n");
        return -1;
    }

    if (input_file.size() == 0 || output_file.size() == 0) {
        usage(argv[0]);
        return -1;
    }

    fp_out = fopen(input_file.c_str(), "rb");
    if (!fp_out) {
        printf("Fail to open input file '%s'\n", input_file.c_str());
        return -1;
    }

    if (fclose(fp_out))
        printf("fclose() failed\n");

    if (test_name.size() == 0)
        test_name = input_file;

    setenv("NUGU_RECORDING_FROM_FILE", input_file.c_str(), 1);

    fp_out = fopen(output_file.c_str(), "w");
    if (!fp_out) {
        printf("Fail to open output file '%s'\n", output_file.c_str());
        return -1;
    }

    /* CSV header, NOLINTNEXTLINE(cert-err33-c) */
    fprintf(fp_out, "Testcase,Num,timestamp,%s,%s,%s,%s,%s,Dialog_Request_ID\n",
        nugu_prof_get_type_name(NUGU_PROF_TYPE_ASR_END_POINT_DETECTED),
        nugu_prof_get_type_name(NUGU_PROF_TYPE_ASR_RESULT),
        nugu_prof_get_type_name(NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE),
        nugu_prof_get_type_name(NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT),
        nugu_prof_get_type_name(NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE));

    /* Mark the types to print the profiling trace log */
    prof_flag[NUGU_PROF_TYPE_ASR_LISTENING_STARTED] = 1;
    prof_flag[NUGU_PROF_TYPE_ASR_END_POINT_DETECTED] = 1;
    prof_flag[NUGU_PROF_TYPE_ASR_LAST_ATTACHMENT] = 1;
    prof_flag[NUGU_PROF_TYPE_ASR_RESULT] = 1;
    prof_flag[NUGU_PROF_TYPE_TTS_SPEAK_DIRECTIVE] = 1;
    prof_flag[NUGU_PROF_TYPE_TTS_FIRST_ATTACHMENT] = 1;
    prof_flag[NUGU_PROF_TYPE_TTS_FIRST_PCM_WRITE] = 1;
    nugu_prof_set_callback(_prof_handler, NULL);

    nugu_log_set_prefix_fields(NUGU_LOG_PREFIX_TIMESTAMP);
    nugu_log_set_modules(NUGU_LOG_MODULE_APPLICATION);

    nugu_client = std::make_shared<NuguClient>();

    /* Create System, AudioPlayer, Text capability default */
    auto system_handler = std::shared_ptr<ISystemHandler>(
        CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>());
    auto audio_player_handler = std::shared_ptr<IAudioPlayerHandler>(
        CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>());

    /* Create an ASR capability with model file path */
    auto my_asr_listener(std::make_shared<MyASR>());
    asr_handler = std::shared_ptr<IASRHandler>(
        CapabilityFactory::makeCapability<ASRAgent, IASRHandler>(my_asr_listener.get()));
    asr_handler->setAttribute(ASRAttribute { model_path, "CLIENT", "COMPLETE" });

    /* Create a TTS capability */
    auto tts_listener = std::make_shared<MyTTSListener>();
    auto tts_handler = std::shared_ptr<ITTSHandler>(
        CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(tts_listener.get()));

    /* Register build-in capabilities */
    nugu_client->getCapabilityBuilder()
        ->add(system_handler.get())
        ->add(audio_player_handler.get())
        ->add(tts_handler.get())
        ->add(asr_handler.get())
        ->construct();

    if (!nugu_client->initialize()) {
        printf("SDK Initialization failed.\n");
        return -1;
    }

    /* Network manager */
    auto network_manager_listener(std::make_shared<MyNetwork>());

    auto network_manager(nugu_client->getNetworkManager());
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(env_token);

    network_timer_src = g_timeout_add_seconds(test_network_timeout, _network_timeout_cb, NULL);
    network_manager->connect();

    /* Start GMainLoop */
    loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(loop);

    /* wait until g_main_loop_quit() */

    g_main_loop_unref(loop);

    nugu_client->deInitialize();

    if (fp_out) {
        if (fclose(fp_out))
            printf("fclose() failed\n");
    }

    return 0;
}
