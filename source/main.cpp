#include <borealis.hpp>

#include "app_state.hpp"
#include "config.hpp"
#include "ui/pairing_activity.hpp"
#include "ui/store_activity.hpp"

int main(int argc, char* argv[])
{
    brls::Logger::setLogLevel(brls::LogLevel::INFO);

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("RomM eShop");
    brls::Application::setGlobalQuit(false); // we register our own START handler per-activity

    auto& app = romm::AppState::Instance();
    app.config = romm::config::Load();
    app.RebuildClient();

    if (app.config.paired)
        brls::Application::pushActivity(new romm::ui::StoreActivity());
    else
        brls::Application::pushActivity(new romm::ui::PairingActivity());

    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
