#pragma once

// romm-eshop shim: stands in for Awoo Installer's util/util.hpp. Only the
// pieces the vendored install engine actually calls.

#include <string>

namespace inst::util
{
    // Opens the NCM/ES/etc services the install engine needs. Must be called
    // before constructing an install task and matched with a later call to
    // deinitInstallServices(). See source/install/xci_installer.cpp.
    void initInstallServices();
    void deinitInstallServices();

    // No-op: we don't ship the bark.wav/awoo.wav sound effects Awoo plays on
    // install failure/success. Kept so the vendored code's std::thread call
    // still links.
    void playAudio(std::string path);
}
