#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Self-updater: checks this repo's GitHub releases (public -- no auth
// needed) for a newer .nro, downloads it, and stages it to replace the
// installed copy on next launch. Deliberately has no borealis/UI dependency,
// matching api/romm_client.hpp's separation -- the UI layer drives this and
// owns all user-facing state.
namespace romm::updater
{
    struct UpdateInfo
    {
        std::string version;     // e.g. "0.1.6" (leading 'v' stripped)
        std::string downloadUrl;
        int64_t assetSize = 0;
    };

    // Queries GitHub's public releases API. `updateAvailable` is only
    // meaningful when this returns true.
    bool CheckForUpdate(UpdateInfo& out, bool& updateAvailable, std::string& errorOut);

    using ProgressCallback = std::function<bool(int64_t downloaded, int64_t total)>;

    // Downloads the new .nro and replaces the installed copy
    // (sdmc:/switch/romm-eshop/romm-eshop.nro) on success. The app is still
    // running its old code in RAM at this point -- nothing takes effect
    // until the process actually exits and gets relaunched. Call
    // PrepareRelaunch() and then quit the app afterward.
    bool DownloadUpdate(const UpdateInfo& info, const ProgressCallback& onProgress, std::string& errorOut);

    // Tells the loader (hbmenu/hbloader) to launch the just-updated install
    // path the moment this process exits. Call this right before quitting.
    void PrepareRelaunch();
}
