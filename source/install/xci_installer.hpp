#pragma once

#include <string>

// Installs a locally-downloaded XCI onto the console, using Awoo Installer's
// vendored NCM/ES install engine (source/vendor/awoo) unmodified. Progress
// and status text are published to InstallProgressState (see
// install_progress_state.hpp) for the UI to poll; call this from a
// background thread, never from the UI thread.
namespace romm::install
{
    enum class InstallDestination
    {
        SdCard,
        SystemMemory,
    };

    struct InstallResult
    {
        bool success = false;
        std::string errorMessage;
    };

    // xciPath must be a full sdmc:/... path to a complete, already-downloaded
    // XCI file.
    InstallResult InstallXci(const std::string& xciPath, InstallDestination destination);
}
