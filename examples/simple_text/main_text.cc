#include <glib.h>
#include <iostream>

#include <base/nugu_log.h>
#include <capability/audio_player_interface.hh>
#include <capability/capability_factory.hh>
#include <capability/system_interface.hh>
#include <capability/text_interface.hh>
#include <capability/tts_interface.hh>
#include <clientkit/nugu_client.hh>

using namespace NuguClientKit;
using namespace NuguCapability;

static std::shared_ptr<NuguClient> nugu_client;
static std::shared_ptr<ITextHandler> text_handler = nullptr;

static std::string text_value;

void test_start()
{
    std::cout << "Send the text command: " << text_value << std::endl;
    text_handler->requestTextInput(text_value);
}

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
            std::cout << "Network ready !" << std::endl;
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
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " Text-Command" << std::endl;
        return 0;
    }

    if (getenv("NUGU_TOKEN") == NULL) {
        std::cout << "Please set the token using the NUGU_TOKEN environment variable." << std::endl;
        return -1;
    }

    text_value = argv[1];

    std::cout << "Simple text command example!" << std::endl;
    std::cout << " - Text-Command: " << text_value << std::endl;

    /* Turn off the SDK internal log */
    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    nugu_client = std::make_shared<NuguClient>();

    /* Create System, AudioPlayer capability default */
    auto system_handler(std::shared_ptr<ISystemHandler>(
        CapabilityFactory::makeCapability<SystemAgent, ISystemHandler>()));
    auto audio_player_handler(std::shared_ptr<IAudioPlayerHandler>(
        CapabilityFactory::makeCapability<AudioPlayerAgent, IAudioPlayerHandler>()));

    /* Create a Text capability */
    text_handler = std::shared_ptr<ITextHandler>(
        CapabilityFactory::makeCapability<TextAgent, ITextHandler>());

    /* Create a TTS capability */
    auto tts_listener(std::make_shared<MyTTSListener>());
    auto tts_handler(std::shared_ptr<ITTSHandler>(
        CapabilityFactory::makeCapability<TTSAgent, ITTSHandler>(tts_listener.get())));

    /* Register build-in capabilities */
    nugu_client->getCapabilityBuilder()
        ->add(system_handler.get())
        ->add(audio_player_handler.get())
        ->add(tts_handler.get())
        ->add(text_handler.get())
        ->construct();

    if (!nugu_client->initialize()) {
        std::cout << "SDK Initialization failed." << std::endl;
        return -1;
    }

    /* Network manager */
    auto network_manager_listener(std::make_shared<MyNetwork>());

    auto network_manager(nugu_client->getNetworkManager());
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
