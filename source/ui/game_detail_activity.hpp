#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "api/romm_client.hpp"

namespace romm::ui
{
    // Game detail / "product page": cover art, description, size, and the
    // Download & Install flow (download to SD, then hand off to the vendored
    // install engine). One screen mirrors the eShop's own purchase flow.
    class GameDetailActivity : public brls::Activity
    {
      public:
        explicit GameDetailActivity(api::Rom rom);
        ~GameDetailActivity() override;

        brls::View* createContentView() override;
        void onContentAvailable() override;

      private:
        enum class Phase
        {
            Idle,
            Downloading,
            Installing,
            Done,
            Failed,
        };

        api::Rom m_rom;
        std::atomic<Phase> m_phase{Phase::Idle};
        std::shared_ptr<std::atomic<bool>> m_alive;
        std::thread m_workThread;

        std::atomic<int64_t> m_downloadedBytes{0};
        std::atomic<int64_t> m_totalBytes{0};

        std::mutex m_textMutex;
        std::string m_errorText;

        class PollTask : public brls::RepeatingTask
        {
          public:
            explicit PollTask(GameDetailActivity* owner)
                : brls::RepeatingTask(250), m_owner(owner) {}
            void run() override { m_owner->Tick(); }

          private:
            GameDetailActivity* m_owner;
        };
        std::unique_ptr<PollTask> m_pollTask;
        Phase m_lastRenderedPhase = Phase::Idle;
        brls::Label* m_progressLabel = nullptr;

        std::string DownloadDestPath() const;
        void StartDownloadAndInstall();
        void Tick();
        void RebuildContent();
        void RebuildContentUnsafe(); // does the real work; RebuildContent() wraps it in try/catch
        void UpdateProgressLabel();
    };
}
