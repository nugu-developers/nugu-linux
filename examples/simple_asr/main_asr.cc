#include <glib.h>
#include <iostream>

#include <base/nugu_log.h>
#include <capability/asr_interface.hh>
#include <capability/audio_player_interface.hh>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <capability/text_interface.hh>
#include <capability/tts_interface.hh>
#include <clientkit/nugu_client.hh>

using namespace NuguClientKit;
using namespace NuguCapability;

static std::shared_ptr<NuguClient> nugu_client;
static IASRHandler* asr_handler = nullptr;

class MyTTSListener : public ITTSListener {
public:
    virtual ~MyTTSListener() = default;

    void onTTSState(TTSState state, const std::string& dialog_id) override
    {
    }

    void onTTSText(const std::string& text, const std::string& dialog_id) override
    {
        std::cout << "TTS: " << text << std::endl;
    }

    void onTTSCancel(const std::string& dialog_id)
    {
    }
};

class MyASR : public IASRListener {
public:
    virtual ~MyASR() = default;

    void onState(ASRState state)
    {
        switch (state) {
        case ASRState::IDLE:
            std::cout << "ASR Idle" << std::endl;
            break;
        case ASRState::EXPECTING_SPEECH:
            std::cout << "ASR Expecting speech" << std::endl;
            break;
        case ASRState::LISTENING:
            std::cout << "ASR Listening... Speak please !" << std::endl;
            break;
        case ASRState::RECOGNIZING:
            std::cout << "ASR Recognizing..." << std::endl;
            break;
        case ASRState::BUSY:
            std::cout << "ASR Processing..." << std::endl;
            break;
        }
    }

    void onNone()
    {
        std::cout << "ASR no recognition result" << std::endl;
    }

    void onPartial(const std::string& text)
    {
        std::cout << "ASR partial result: " << text << std::endl;
    }

    void onComplete(const std::string& text)
    {
        std::cout << "ASR complete result: " << text << std::endl;
    }

    void onError(ASRError error)
    {
        switch (error) {
        case ASRError::RESPONSE_TIMEOUT:
            std::cout << "ASR response timeout" << std::endl;
            break;
        case ASRError::LISTEN_TIMEOUT:
            std::cout << "ASR listen timeout" << std::endl;
            break;
        case ASRError::LISTEN_FAILED:
            std::cout << "ASR listen failed" << std::endl;
            break;
        case ASRError::RECOGNIZE_ERROR:
            std::cout << "ASR recognition error" << std::endl;
            break;
        case ASRError::UNKNOWN:
            std::cout << "ASR unknown error" << std::endl;
            break;
        }
    }

    void onCancel()
    {
        std::cout << "ASR canceled" << std::endl;
    }

    void setExpectSpeechState(bool is_es_state)
    {
        std::cout << "ASR expect speech state: " << is_es_state << std::endl;
    }
};

class MyNetwork : public INetworkManagerListener {
public:
    void onStatusChanged(NetworkStatus status)
    {
        switch (status) {
        case NetworkStatus::DISCONNECTED:
            std::cout << "Network disconnected !" << std::endl;
            break;
        case NetworkStatus::CONNECTED:
            std::cout << "Network connected !" << std::endl;
            std::cout << "Start ASR Recognition !" << std::endl;
            asr_handler->startRecognition();
            break;
        case NetworkStatus::CONNECTING:
            std::cout << "Network connecting..." << std::endl;
            break;
        default:
            break;
        }
    }

    void onError(NetworkError error)
    {
        switch (error) {
        case NetworkError::TOKEN_ERROR:
            break;
        case NetworkError::UNKNOWN:
            break;
        }
    }
};

int main()
{
    if (getenv("NUGU_TOKEN") == NULL) {
        std::cout << "Please set the token using the NUGU_TOKEN environment variable." << std::endl;
        return -1;
    }

    std::cout << "Simple ASR example!" << std::endl;

    /* Turn off the SDK internal log */
    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    nugu_client = std::make_shared<NuguClient>();

    /* Create an ASR capability with model file path */
    auto my_asr_listener(std::make_shared<MyASR>());
    asr_handler = CapabilityFactory::makeCapability<ASRAgent, IASRHandler>(my_asr_listener.get());
    asr_handler->setAttribute(ASRAttribute { "/var/lib/nugu/model", "CLIENT", "PARTIAL" });

    /* Create a TTS capability */
    auto tts_listener(std::make_shared<MyTTSListener>());
    ITTSHandler* tts_handler = CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(tts_listener.get());

    /* Register build-in capabilities */
    nugu_client->getCapabilityBuilder()
        ->add(CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>())
        ->add(CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>())
        ->add(CapabilityFactory::makeCapability<TextAgent, ITextHandler>())
        ->add(tts_handler)
        ->add(asr_handler)
        ->construct();

    if (!nugu_client->initialize()) {
        std::cout << "SDK Initialization failed." << std::endl;
        return -1;
    }

    /* Network manager */
    auto network_manager_listener(std::make_shared<MyNetwork>());

    INetworkManager* network_manager = nugu_client->getNetworkManager();
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(getenv("NUGU_TOKEN"));
    network_manager->connect();

    /* Start GMainLoop */
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    std::cout << "Start the eventloop" << std::endl
              << std::endl;
    g_main_loop_run(loop);

    /* wait until g_main_loop_quit() */

    g_main_loop_unref(loop);

    nugu_client->deInitialize();

    return 0;
}
