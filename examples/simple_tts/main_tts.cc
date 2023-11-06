#include <glib.h>
#include <iostream>

#include <base/nugu_log.h>
#include <capability/audio_player_interface.hh>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <capability/tts_interface.hh>
#include <clientkit/nugu_client.hh>

using namespace NuguClientKit;
using namespace NuguCapability;

static std::shared_ptr<NuguClient> nugu_client;
static std::shared_ptr<ITTSHandler> tts_handler = nullptr;

static std::string text_value;
static GMainLoop* loop;

void test_start()
{
    std::cout << "Send the text command: " << text_value << std::endl;
    tts_handler->requestTTS(text_value, "");
}

class MyTTSListener : public ITTSListener {
public:
    virtual ~MyTTSListener() = default;

    void onTTSState(TTSState state, const std::string& dialog_id) override
    {
        switch (state) {
        case TTSState::TTS_SPEECH_FINISH:
            g_main_loop_quit(loop);
            break;
        default:
            break;
        }
    }

    void onTTSText(const std::string& text, const std::string& dialog_id) override
    {
        std::cout << "TTS: " << text << std::endl;
    }

    void onTTSCancel(const std::string& dialog_id) override
    {
    }
};

class MyNetwork : public INetworkManagerListener {
public:
    void onStatusChanged(NetworkStatus status) override
    {
        switch (status) {
        case NetworkStatus::DISCONNECTED:
            std::cout << "Network disconnected !" << std::endl;
            break;
        case NetworkStatus::CONNECTING:
            std::cout << "Network connecting..." << std::endl;
            break;
        case NetworkStatus::READY:
            std::cout << "Network connected !" << std::endl;
            test_start();
            break;
        case NetworkStatus::CONNECTED:
            std::cout << "Network connected !" << std::endl;
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
            std::cout << "Network failed !" << std::endl;
            break;
        case NetworkError::TOKEN_ERROR:
            std::cout << "Token error !" << std::endl;
            break;
        case NetworkError::UNKNOWN:
            std::cout << "Unknown error !" << std::endl;
            break;
        }
    }
};

int main(int argc, char* argv[])
{
    const char* env_token;

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " Text string" << std::endl;
        return 0;
    }

    env_token = getenv("NUGU_TOKEN");
    if (env_token == NULL) {
        std::cout << "Please set the token using the NUGU_TOKEN environment variable." << std::endl;
        return -1;
    }

    text_value = argv[1];

    std::cout << "Simple text-to-speech example!" << std::endl;
    std::cout << " - Text string: " << text_value << std::endl;

    /* Turn off the SDK internal log */
    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    nugu_client = std::make_shared<NuguClient>();

    /* Create System capability */
    auto system_handler(std::shared_ptr<ISystemHandler>(
        CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>()));

    /* Create a TTS capability */
    auto tts_listener(std::make_shared<MyTTSListener>());
    tts_handler = std::shared_ptr<ITTSHandler>(
        CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(tts_listener.get()));

    /* Register build-in capabilities */
    nugu_client->getCapabilityBuilder()
        ->add(system_handler.get())
        ->add(tts_handler.get())
        ->construct();

    if (!nugu_client->initialize()) {
        std::cout << "SDK Initialization failed." << std::endl;
        return -1;
    }

    /* Network manager */
    auto network_manager_listener(std::make_shared<MyNetwork>());

    auto network_manager(nugu_client->getNetworkManager());
    network_manager->addListener(network_manager_listener.get());
    network_manager->setToken(env_token);
    network_manager->connect();

    /* Start GMainLoop */
    loop = g_main_loop_new(NULL, FALSE);

    std::cout << "Start the eventloop" << std::endl
              << std::endl;
    g_main_loop_run(loop);

    /* wait until g_main_loop_quit() */

    g_main_loop_unref(loop);

    nugu_client->deInitialize();

    return 0;
}
