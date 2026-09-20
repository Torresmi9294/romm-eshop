#pragma once

// romm-eshop shim: stands in for Awoo Installer's Plutonium-based instPage
// UI. The real install engine (install_nsp.cpp / install_xci.cpp, vendored
// unmodified below) calls these two functions to report progress; we forward
// them into romm::install::InstallProgressState, which our borealis UI polls.
// See source/install/install_progress_state.hpp.

#include <string>

namespace inst::ui::instPage
{
    void setInstInfoText(std::string ourText);
    void setInstBarPerc(int percentage);
}
