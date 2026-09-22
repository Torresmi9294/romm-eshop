#include <borealis.hpp>
#include <curl/curl.h>
#include <switch.h>

#include "app_state.hpp"
#include "config.hpp"
#include "ui/pairing_activity.hpp"
#include "ui/store_activity.hpp"

int main(int argc, char* argv[])
{
    // borealis's own userAppInit() (called by libnx before main(), see
    // external/borealis/library/lib/platforms/switch/switch_wrapper.c)
    // already calls nxlinkStdio() -- redirecting stdout/stderr to whichever
    // PC pushed this build via `nxlink`. Calling it again here would just
    // open a second, redundant socket.

    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);

    // curl_global_init() is not thread-safe and must run once, before any
    // other libcurl call, from a single thread. Several activities spawn
    // worker threads that use curl (romm_client.cpp), so this can't be left
    // to libcurl's lazy auto-init inside curl_easy_init() -- two threads
    // racing to lazily init it for the first time is a real bug, not a
    // theoretical one.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        curl_global_cleanup();
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("RomM eShop");
    brls::Application::setGlobalQuit(false); // we register our own START handler per-activity

    auto& app = romm::AppState::Instance();
    app.config = romm::config::Load();
    app.RebuildClient();

    // StoreActivity is always the root activity, and PairingActivity (when
    // needed) is pushed on top of it -- never the other way around. Pairing
    // used to push a *new* StoreActivity on top of itself on success, which
    // meant PairingActivity (and every lambda its buttons captured `this`
    // in) stayed alive in memory for the rest of the session instead of
    // being destroyed: borealis's Activity stack only supports popping the
    // top activity, so once something was pushed on top of PairingActivity,
    // there was no way to pop *it* specifically anymore. With the root/overlay
    // order flipped, a successful pairing can just popActivity() to reveal
    // the StoreActivity that was already underneath, which is what actually
    // destroys PairingActivity correctly.
    brls::Application::pushActivity(new romm::ui::StoreActivity());
    if (!app.config.paired)
        brls::Application::pushActivity(new romm::ui::PairingActivity());

    while (brls::Application::mainLoop())
        ;

    curl_global_cleanup();
    return EXIT_SUCCESS;
}
