#include <glib.h>
#include <iostream>

#include <base/nugu_log.h>
#include <clientkit/nugu_client.hh>

using namespace NuguClientKit;

static GMainLoop* loop;

class MediaPlayerListener : public IMediaPlayerListener {
public:
    virtual ~MediaPlayerListener() = default;

    void mediaStateChanged(MediaPlayerState state)
    {
        switch (state) {
        case MediaPlayerState::IDLE:
            std::cout << ">> mediaStateChanged: IDLE" << std::endl;
            break;
        case MediaPlayerState::PAUSED:
            std::cout << ">> mediaStateChanged: PAUSED" << std::endl;
            break;
        case MediaPlayerState::PLAYING:
            std::cout << ">> mediaStateChanged: PLAYING" << std::endl;
            break;
        case MediaPlayerState::PREPARE:
            std::cout << ">> mediaStateChanged: PREPARE" << std::endl;
            break;
        case MediaPlayerState::READY:
            std::cout << ">> mediaStateChanged: READY" << std::endl;
            break;
        case MediaPlayerState::STOPPED:
            std::cout << ">> mediaStateChanged: STOPPED" << std::endl;
            break;
        }
    }

    void mediaEventReport(MediaPlayerEvent event)
    {
        switch (event) {
        case MediaPlayerEvent::INVALID_MEDIA_URL:
            std::cout << ">> mediaEventReport: INVALID_MEDIA_URL" << std::endl;
            break;
        case MediaPlayerEvent::LOADING_MEDIA_FAILED:
            std::cout << ">> mediaEventReport: LOADING_MEDIA_FAILED" << std::endl;
            break;
        case MediaPlayerEvent::LOADING_MEDIA_SUCCESS:
            std::cout << ">> mediaEventReport: LOADING_MEDIA_SUCCESS" << std::endl;
            break;
        case MediaPlayerEvent::PLAYING_MEDIA_FINISHED:
            std::cout << ">> mediaEventReport: PLAYING_MEDIA_FINISHED" << std::endl;
            g_main_loop_quit(loop);
            break;
        case MediaPlayerEvent::PLAYING_MEDIA_UNDERRUN:
            std::cout << ">> mediaEventReport: PLAYING_MEDIA_UNDERRUN" << std::endl;
            break;
        }
    }

    void mediaChanged(const std::string& url)
    {
        std::cout << ">> mediaChanged: " << url << std::endl;
    }

    void durationChanged(int duration)
    {
        std::cout << ">> durationChanged: " << duration << std::endl;
    }

    void positionChanged(int position)
    {
        std::cout << ">> positionChanged: " << position << std::endl;
    }

    void volumeChanged(int volume)
    {
        std::cout << ">> volumeChanged: " << volume << std::endl;
    }

    void muteChanged(int mute)
    {
        std::cout << ">> muteChanged: " << mute << std::endl;
    }
};

int main(int argc, char* argv[])
{
    MediaPlayerListener* player_listener = nullptr;
    IMediaPlayer* player = nullptr;
    NuguClient* nugu_client = nullptr;

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " {media-file}" << std::endl;
        std::cout << std::endl;
        std::cout << "media-file:" << std::endl;
        std::cout << "\tfiles:///{file-path}" << std::endl;
        std::cout << "\thttps://{streaming-url}" << std::endl;
        std::cout << std::endl;
        return 0;
    }

    std::cout << "Simple media file play example!" << std::endl;
    std::cout << " - Media path: " << argv[1] << std::endl
              << std::endl;

    /* Turn off the SDK internal log */
    nugu_log_set_system(NUGU_LOG_SYSTEM_NONE);

    nugu_client = new NuguClient();

    if (!nugu_client->loadPlugins()) {
        std::cout << ">> SDK plugin loading failed." << std::endl;
        goto OUT;
    }

    player_listener = new MediaPlayerListener();
    player = nugu_client->getNuguCoreContainer()->createMediaPlayer();

    player->addListener(player_listener);

    if (player->setSource(argv[1]) == false) {
        std::cout << ">> setSource() failed." << std::endl;
        goto OUT;
    }

    std::cout << std::endl
              << "Play !" << std::endl;
    if (player->play() == false) {
        std::cout << ">> play() failed." << std::endl;
        goto OUT;
    }

    /* Start GMainLoop */
    loop = g_main_loop_new(NULL, FALSE);

    std::cout << "Start the eventloop" << std::endl
              << std::endl;

    g_main_loop_run(loop);

    /* wait until g_main_loop_quit() */
    std::cout << std::endl
              << "Finished" << std::endl;

    g_main_loop_unref(loop);

OUT:
    if (player)
        delete player;

    if (player_listener)
        delete player_listener;

    if (nugu_client) {
        nugu_client->unloadPlugins();
        delete nugu_client;
    }

    return 0;
}
