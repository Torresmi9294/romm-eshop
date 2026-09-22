#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "updater/updater.hpp"

namespace romm::ui
{
    // Shown when StoreActivity finds a newer release available. Lets the
    // user download it and relaunch into it, or skip.
    class UpdateActivity : public brls::Activity
    {
      public:
        explicit UpdateActivity(updater::UpdateInfo info);
        ~UpdateActivity() override;

        brls::View* createContentView() override;
        void onContentAvailable() override;

      private:
        enum class Phase
        {
            Confirm,
            Downloading,
            Error,
        };

        updater::UpdateInfo m_info;
        std::atomic<Phase> m_phase{Phase::Confirm};
        Phase m_lastRenderedPhase = Phase::Confirm;

        std::shared_ptr<std::atomic<bool>> m_alive;
        std::thread m_workThread;

        std::atomic<int64_t> m_downloadedBytes{0};
        std::atomic<int64_t> m_totalBytes{0};

        std::mutex m_textMutex;
        std::string m_errorText;

        class PollTask : public brls::RepeatingTask
        {
          public:
            explicit PollTask(UpdateActivity* owner)
                : brls::RepeatingTask(250), m_owner(owner) {}
            void run() override { m_owner->Tick(); }

          private:
            UpdateActivity* m_owner;
        };
        std::unique_ptr<PollTask> m_pollTask;
        brls::Label* m_progressLabel = nullptr;

        void StartDownload();
        void Tick();
        void RebuildContent();
    };
}
