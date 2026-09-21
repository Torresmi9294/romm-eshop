#pragma once

#include <borealis.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "api/romm_client.hpp"

namespace romm::ui
{
    // First-run / re-pair screen. Walks the user through RomM's device
    // authorization flow: show a short code, have them approve it from a
    // browser already logged into RomM, then poll until approved.
    //
    // All network I/O (initiate + poll) runs on a background worker thread.
    // This matters at startup specifically: onContentAvailable() runs before
    // borealis's main loop has rendered its first frame, so anything
    // blocking called from there (a slow/hung network call) would leave the
    // screen blank -- nothing gets drawn until that call returns. Running it
    // on a worker thread lets the "Connecting..." frame actually render
    // immediately, and keeps that guarantee for the rest of the flow too.
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
        Phase m_lastRenderedPhase = Phase::NeedsServerUrl;

        std::shared_ptr<std::atomic<bool>> m_alive;
        std::thread m_workThread;

        std::mutex m_textMutex;
        std::string m_userCode;
        std::string m_verificationUrl;
        std::string m_errorText;

        void PromptForServerUrl();
        void StartPairing();       // spawns the worker thread
        void RunPairingFlow();     // runs on the worker thread: initiate + poll loop
        void Tick();               // UI thread: re-renders when phase changes
        void RebuildContent();
    };
}
