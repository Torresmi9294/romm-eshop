#include "ui/instPage.hpp"
#include "install/install_progress_state.hpp"

namespace inst::ui::instPage
{
    void setInstInfoText(std::string ourText)
    {
        romm::install::InstallProgressState::Instance().SetStatusText(ourText);
    }

    void setInstBarPerc(int percentage)
    {
        romm::install::InstallProgressState::Instance().SetPercent(percentage);
    }
}
