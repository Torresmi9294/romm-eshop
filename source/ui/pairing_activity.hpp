#pragma once

#include <borealis.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "api/romm_client.hpp"

namespace romm::ui
{
    // First-run / re-pair screen. Walks the user through RomM's device
    // authorization flow: show a short code, have them approve it from a
    // browser already logged into RomM, then poll until approved.
    class PairingActivity : public brls::Activity
    {
      public:
        PairingActivity();
        ~PairingActivity() override;

        brls::View* createContentView() override;
        void onContentAvailable() override;

      private:
        enum class Phase
        {
            NeedsServerUrl,
            Initiating,
            WaitingApproval,
            Approved,
            Error,
        };

        class PollTask : public brls::RepeatingTask
        {
          public:
            explicit PollTask(PairingActivity* owner)
                : brls::RepeatingTask(500), m_owner(owner) {}
            void run() override { m_owner->Tick(); }

          private:
            PairingActivity* m_owner;
        };

        std::atomic<Phase> m_phase{Phase::NeedsServerUrl};
        std::unique_ptr<PollTask> m_pollTask;

        std::mutex m_textMutex;
        std::string m_userCode;
        std::string m_verificationUrl;
        std::string m_errorText;

        std::string m_deviceCode;
        int m_intervalSeconds = 5;
        std::chrono::steady_clock::time_point m_expiresAt;
        std::chrono::steady_clock::time_point m_nextPollAt;

        brls::Label* m_statusLabel = nullptr;
        brls::Label* m_codeLabel   = nullptr;
        brls::Label* m_urlLabel    = nullptr;

        void PromptForServerUrl();
        void StartPairing();
        void Tick();
        void RebuildContent();
    };
}
