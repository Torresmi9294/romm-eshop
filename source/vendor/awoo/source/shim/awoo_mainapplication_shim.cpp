#include "ui/MainApplication.hpp"
#include "install/install_progress_state.hpp"

namespace inst::ui
{
    static MainApplication s_instance;
    MainApplication* mainApp = &s_instance;

    int MainApplication::CreateShowDialog(std::string title, std::string desc, std::vector<std::string> /*options*/, bool /*disableBackout*/)
    {
        auto& state = romm::install::InstallProgressState::Instance();
        state.SetVerificationFailed(true);
        state.SetStatusText(title + ": " + desc);
        return 0; // fail closed -- see MainApplication.hpp for why
    }
}
