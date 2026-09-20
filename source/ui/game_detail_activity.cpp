#include "ui/game_detail_activity.hpp"
#include "app_state.hpp"
#include "config.hpp"
#include "install/xci_installer.hpp"
#include "install/install_progress_state.hpp"

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

using namespace romm::ui;

namespace
{
    std::string FormatBytes(int64_t bytes)
    {
        static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double value = static_cast<double>(bytes);
        int unit = 0;
        while (value >= 1024.0 && unit < 4)
        {
            value /= 1024.0;
            unit++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << " " << units[unit];
        return oss.str();
    }

    std::string CoverCachePath(const romm::api::Rom& rom)
    {
        return std::string(romm::config::COVER_CACHE_DIR) + "/" + std::to_string(rom.id) + ".cover";
    }
}

GameDetailActivity::GameDetailActivity(api::Rom rom)
    : m_rom(std::move(rom)), m_alive(std::make_shared<std::atomic<bool>>(true))
{
}

GameDetailActivity::~GameDetailActivity()
{
    *m_alive = false;
    if (m_workThread.joinable())
        m_workThread.detach();
}

std::string GameDetailActivity::DownloadDestPath() const
{
    return std::string(romm::config::DOWNLOAD_DIR) + "/" + m_rom.fsName;
}

brls::View* GameDetailActivity::createContentView()
{
    return nullptr;
}

void GameDetailActivity::onContentAvailable()
{
    m_pollTask = std::make_unique<PollTask>(this);
    m_pollTask->start();

    this->registerAction("Back", brls::BUTTON_B, [this](brls::View*) {
        Phase phase = m_phase;
        if (phase == Phase::Downloading || phase == Phase::Installing)
            return true; // swallow: don't navigate away mid-transfer

        brls::Application::popActivity();
        return true;
    });

    RebuildContent();
}

void GameDetailActivity::StartDownloadAndInstall()
{
    m_phase = Phase::Downloading;
    m_downloadedBytes = 0;
    m_totalBytes = m_rom.fsSizeBytes;
    RebuildContent();

    auto alive = m_alive;
    m_workThread = std::thread([this, alive]() {
        auto& app = romm::AppState::Instance();

        mkdir("sdmc:/switch", 0777);
        mkdir(romm::config::CONFIG_DIR, 0777);
        mkdir(romm::config::DOWNLOAD_DIR, 0777);

        std::string destPath = DownloadDestPath();
        std::string error;

        bool downloadOk = app.client->DownloadRomContent(
            m_rom, destPath,
            [this, alive](int64_t transferred, int64_t total) {
                m_downloadedBytes = transferred;
                if (total > 0)
                    m_totalBytes = total;
                return alive->load();
            },
            error);

        if (!*alive) return;

        if (!downloadOk)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = error;
            m_phase = Phase::Failed;
            return;
        }

        m_phase = Phase::Installing;

        auto result = romm::install::InstallXci(destPath, romm::install::InstallDestination::SdCard);

        if (!*alive) return;

        if (!result.success)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = result.errorMessage;
            m_phase = Phase::Failed;
            return;
        }

        // Installed content lives in NAND/SD content storage now; no need to
        // keep the multi-gigabyte source file around.
        remove(destPath.c_str());

        m_phase = Phase::Done;
    });
}

void GameDetailActivity::Tick()
{
    Phase phase = m_phase;

    if (phase == Phase::Downloading || phase == Phase::Installing)
    {
        UpdateProgressLabel();
    }

    if (phase != m_lastRenderedPhase)
    {
        m_lastRenderedPhase = phase;
        RebuildContent();
    }
}

void GameDetailActivity::UpdateProgressLabel()
{
    if (!m_progressLabel)
        return;

    Phase phase = m_phase;
    std::ostringstream oss;

    if (phase == Phase::Downloading)
    {
        int64_t done = m_downloadedBytes;
        int64_t total = m_totalBytes;
        oss << "Downloading... " << FormatBytes(done);
        if (total > 0)
        {
            int percent = static_cast<int>((done * 100) / total);
            oss << " / " << FormatBytes(total) << "  (" << percent << "%)";
        }
    }
    else if (phase == Phase::Installing)
    {
        auto& progress = romm::install::InstallProgressState::Instance();
        oss << "Installing... " << progress.GetPercent() << "%";
        std::string status = progress.GetStatusText();
        if (!status.empty())
            oss << "\n" << status;
    }

    m_progressLabel->setText(oss.str());
}

void GameDetailActivity::RebuildContent()
{
    auto frame = new brls::AppletFrame();
    frame->setTitle(m_rom.name.empty() ? m_rom.fsName : m_rom.name);

    auto root = new brls::Box();
    root->setAxis(brls::Axis::ROW);
    root->setGrow(1);
    root->setPadding(40, 60, 40, 60);

    auto cover = new brls::Image();
    cover->setWidth(340);
    cover->setHeight(460);
    cover->setScalingType(brls::ImageScalingType::FIT);
    cover->setMarginRight(50);
    std::string coverPath = CoverCachePath(m_rom);
    struct stat st;
    if (stat(coverPath.c_str(), &st) == 0 && st.st_size > 0)
        cover->setImageFromFile(coverPath);
    root->addView(cover);

    auto infoColumn = new brls::Box();
    infoColumn->setAxis(brls::Axis::COLUMN);
    infoColumn->setGrow(1);

    auto sizeLabel = new brls::Label();
    sizeLabel->setText(FormatBytes(m_rom.fsSizeBytes) + "  •  " + m_rom.fsExtension);
    sizeLabel->setFontSize(18);
    sizeLabel->setMarginBottom(20);
    infoColumn->addView(sizeLabel);

    auto scroll = new brls::ScrollingFrame();
    scroll->setHeight(220);
    scroll->setMarginBottom(30);
    auto summary = new brls::Label();
    summary->setText(m_rom.summary.empty() ? "No description available." : m_rom.summary);
    summary->setFontSize(20);
    scroll->setContentView(summary);
    infoColumn->addView(scroll);

    Phase phase = m_phase;

    if (phase == Phase::Idle)
    {
        auto button = new brls::Button();
        button->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        button->setText("Download & Install");
        button->setWidth(400);
        button->setHeight(70);
        button->registerClickAction([this](brls::View*) {
            StartDownloadAndInstall();
            return true;
        });
        infoColumn->addView(button);
    }
    else if (phase == Phase::Downloading || phase == Phase::Installing)
    {
        m_progressLabel = new brls::Label();
        m_progressLabel->setFontSize(22);
        m_progressLabel->setText(phase == Phase::Downloading ? "Downloading..." : "Installing...");
        infoColumn->addView(m_progressLabel);
    }
    else if (phase == Phase::Done)
    {
        auto label = new brls::Label();
        label->setText("Installed! Find it on your Switch's home menu.");
        label->setFontSize(22);
        label->setMarginBottom(20);
        infoColumn->addView(label);

        auto button = new brls::Button();
        button->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        button->setText("Back to Store");
        button->setWidth(300);
        button->setHeight(70);
        button->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
        infoColumn->addView(button);
    }
    else if (phase == Phase::Failed)
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        auto label = new brls::Label();
        label->setText("Failed: " + m_errorText);
        label->setFontSize(20);
        label->setMarginBottom(20);
        infoColumn->addView(label);

        auto retryButton = new brls::Button();
        retryButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        retryButton->setText("Retry");
        retryButton->setWidth(300);
        retryButton->setHeight(70);
        retryButton->registerClickAction([this](brls::View*) {
            StartDownloadAndInstall();
            return true;
        });
        infoColumn->addView(retryButton);
    }

    root->addView(infoColumn);
    frame->setContentView(root);
    this->setContentView(frame);
}
