#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "api/romm_client.hpp"
#include "updater/updater.hpp"

namespace romm::ui
{
    // The "eShop" grid: cover art + title for every Switch ROM on the RomM
    // server. Selecting one pushes GameDetailActivity.
    class StoreActivity : public brls::Activity
    {
      public:
        StoreActivity();
        ~StoreActivity() override;

        brls::View* createContentView() override;
        void onContentAvailable() override;

      private:
        enum class Phase
        {
            Loading,
            Ready,
            Error,
        };

        std::atomic<Phase> m_phase{Phase::Loading};
        std::shared_ptr<std::atomic<bool>> m_alive;
        std::thread m_loadThread;

        std::mutex m_dataMutex;
        std::vector<api::Rom> m_roms;
        std::string m_errorText;

        class PollTask : public brls::RepeatingTask
        {
          public:
            explicit PollTask(StoreActivity* owner)
                : brls::RepeatingTask(500), m_owner(owner) {}
            void run() override { m_owner->Tick(); }

          private:
            StoreActivity* m_owner;
        };
        std::unique_ptr<PollTask> m_pollTask;
        Phase m_lastRenderedPhase = Phase::Loading;

        std::atomic<bool> m_updateAvailable{false};
        std::atomic<bool> m_updatePromptShown{false};
        std::atomic<bool> m_updateCheckStarted{false};
        std::thread m_updateCheckThread;
        updater::UpdateInfo m_updateInfo;
        std::mutex m_updateMutex;

        void StartLoading();
        void StartUpdateCheck();
        void Tick();
        void RebuildContent();
        void RebuildContentUnsafe(); // does the real work; RebuildContent() wraps it in try/catch
    };
}
