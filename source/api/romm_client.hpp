#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// Thin client for the subset of the RomM (https://github.com/rommapp/romm) REST
// API this app needs: device pairing (OAuth2-style device authorization grant),
// listing the Switch platform's ROMs, fetching cover art, and downloading XCI
// content. Verified directly against a live RomM 5.2.0 server's /openapi.json.
namespace romm::api
{
    enum class DeviceAuthStatus
    {
        Ok,
        Pending,   // user hasn't approved yet, keep polling
        Denied,
        Expired,
        NetworkError,
    };

    struct DeviceAuthInit
    {
        std::string deviceCode;
        std::string userCode;
        std::string verificationUrl; // fully-qualified, ready to show to the user
        int expiresInSeconds = 0;
        int intervalSeconds  = 5;
    };

    struct DeviceAuthToken
    {
        std::string accessToken;
        std::string deviceId;
    };

    struct Platform
    {
        int id = 0;
        std::string fsSlug;
        std::string name;
        int romCount = 0;
    };

    struct Rom
    {
        int id = 0;
        std::string name;
        std::string summary;
        std::string fsName;      // actual filename to request from /content/
        std::string fsExtension; // "xci" or "nsp"
        int64_t fsSizeBytes = 0;
        std::string pathCoverLarge; // server-relative path, empty if none
        std::string pathCoverSmall;
    };

    // Called periodically during a download/upload with (downloaded, total) bytes.
    // Return false to abort the transfer.
    using ProgressCallback = std::function<bool(int64_t transferred, int64_t total)>;

    class RommClient
    {
      public:
        explicit RommClient(std::string serverUrl, std::string accessToken = "");
        ~RommClient();

        void SetAccessToken(std::string token) { m_accessToken = std::move(token); }
        const std::string& GetServerUrl() const { return m_serverUrl; }

        // --- Pairing (device authorization grant) ---
        // Scopes requested are fixed to the read-only set this app needs:
        // me.read, roms.read, platforms.read, assets.read
        bool DeviceAuthInitiate(const std::string& clientDeviceIdentifier, DeviceAuthInit& out, std::string& errorOut);
        DeviceAuthStatus DeviceAuthPoll(const std::string& deviceCode, DeviceAuthToken& out);

        // --- Library browsing ---
        bool GetSwitchPlatform(Platform& out, std::string& errorOut);
        bool GetRoms(int platformId, std::vector<Rom>& out, std::string& errorOut);

        // --- Assets / content ---
        // Downloads the rom's large cover art to destPath (only if not already cached).
        bool DownloadCoverArt(const Rom& rom, const std::string& destPath);

        // Downloads (or resumes, using Range) the rom's game file to destPath.
        // destPath is written incrementally; a partially-downloaded file will be resumed.
        bool DownloadRomContent(const Rom& rom, const std::string& destPath, const ProgressCallback& onProgress, std::string& errorOut);

      private:
        std::string m_serverUrl;
        std::string m_accessToken;

        std::string BuildUrl(const std::string& path) const;
    };
}
