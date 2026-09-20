#pragma once

#include <atomic>
#include <mutex>
#include <string>

// Thread-safe progress channel between the (vendored, unmodified) Awoo
// Installer install engine — which runs on a background worker thread and
// reports progress via the inst::ui shim — and our borealis UI, which polls
// this state from the main thread via a brls::RepeatingTask.
//
// Kept deliberately dumb (percent + status text) rather than a callback,
// because the install engine's UI calls happen deep inside vendored,
// unmodified third-party code we don't want to touch.
namespace romm::install
{
    class InstallProgressState
    {
      public:
        static InstallProgressState& Instance();

        void Reset();

        void SetPercent(int percent);
        int GetPercent() const;

        void SetStatusText(const std::string& text);
        std::string GetStatusText() const;

        // Set when the vendored engine hits an NCA signature mismatch and we
        // fail closed (see awoo shim MainApplication::CreateShowDialog). The
        // installer wrapper surfaces this as a clear error afterward.
        void SetVerificationFailed(bool failed);
        bool GetVerificationFailed() const;

      private:
        std::atomic<int> m_percent{0};
        std::atomic<bool> m_verificationFailed{false};

        mutable std::mutex m_textMutex;
        std::string m_statusText;
    };
}
