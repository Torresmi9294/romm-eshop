#pragma once

// romm-eshop shim: stands in for Awoo Installer's MainApplication /
// CreateShowDialog. The vendored install engine only calls this in one
// place: when strict NCA signature validation (inst::config::validateNCAs)
// detects a mismatch, to ask "install anyway?". Borealis (the UI framework
// this app is built on) has no built-in modal confirm dialog, and this call
// happens on a background worker thread, so rather than build a risky
// cross-thread modal we fail closed: the mismatch is logged to the install
// progress status text and the install is aborted, exactly as if the user
// had chosen "cancel". See romm::install::InstallProgressState.

#include <string>
#include <vector>
#include <filesystem> // install_nsp.cpp / install_xci.cpp use std::filesystem::exists()
                      // after including this header, without including <filesystem> themselves.

namespace inst::ui
{
    class MainApplication
    {
      public:
        // Always returns 0 ("cancel") -- see file comment above.
        int CreateShowDialog(std::string title, std::string desc, std::vector<std::string> options, bool disableBackout);
    };

    extern MainApplication* mainApp;
}
