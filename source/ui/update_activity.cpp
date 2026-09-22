#include "ui/update_activity.hpp"

#include <sstream>
#include <iomanip>

using namespace romm::ui;

namespace
{
    std::string FormatBytes(int64_t bytes)
    {
        static const char* units[] = {"B", "KB", "MB", "GB"};
        double value = static_cast<double>(bytes);
        int unit = 0;
        while (value >= 1024.0 && unit < 3)
        {
            value /= 1024.0;
            unit++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << " " << units[unit];
        return oss.str();
    }
}

UpdateActivity::UpdateActivity(romm::updater::UpdateInfo info)
    : m_info(std::move(info)), m_alive(std::make_shared<std::atomic<bool>>(true))
{
}

UpdateActivity::~UpdateActivity()
{
    *m_alive = false;
    if (m_workThread.joinable())
        m_workThread.detach();
}

brls::View* UpdateActivity::createContentView()
{
    return nullptr;
}

void UpdateActivity::onContentAvailable()
{
    m_pollTask = std::make_unique<PollTask>(this);
    m_pollTask->start();

    this->registerAction("Later", brls::BUTTON_B, [this](brls::View*) {
        Phase phase = m_phase;
        if (phase == Phase::Downloading)
            return true; // don't let the user back out mid-download
        brls::Application::popActivity();
        return true;
    });

    RebuildContent();
}

void UpdateActivity::StartDownload()
{
    m_phase = Phase::Downloading;

    auto alive = m_alive;
    m_workThread = std::thread([this, alive]() {
        std::string error;
        bool ok = romm::updater::DownloadUpdate(
            m_info,
            [this, alive](int64_t downloaded, int64_t total) {
                m_downloadedBytes = downloaded;
                if (total > 0)
                    m_totalBytes = total;
                return alive->load();
            },
            error);

        if (!*alive) return;

        if (!ok)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = error;
            m_phase = Phase::Error;
            return;
        }

        romm::updater::PrepareRelaunch();
        brls::Application::quit();
    });
}

void UpdateActivity::Tick()
{
    Phase phase = m_phase;

    if (phase == Phase::Downloading && m_progressLabel)
    {
        int64_t done = m_downloadedBytes;
        int64_t total = m_totalBytes;
        std::ostringstream oss;
        oss << "Downloading update... " << FormatBytes(done);
        if (total > 0)
        {
            int percent = static_cast<int>((done * 100) / total);
            oss << " / " << FormatBytes(total) << "  (" << percent << "%)";
        }
        m_progressLabel->setText(oss.str());
    }

    if (phase == m_lastRenderedPhase)
        return;
    m_lastRenderedPhase = phase;
    RebuildContent();
}

void UpdateActivity::RebuildContent()
{
    auto frame = new brls::AppletFrame();
    frame->setTitle("Update Available");

    auto root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setJustifyContent(brls::JustifyContent::CENTER);
    root->setAlignItems(brls::AlignItems::CENTER);
    root->setGrow(1);
    root->setPadding(60, 80, 60, 80);

    std::lock_guard<std::mutex> lock(m_textMutex);
    Phase phase = m_phase;

    if (phase == Phase::Confirm)
    {
        auto label = new brls::Label();
        label->setText("Version " + m_info.version + " is available (" + FormatBytes(m_info.assetSize) + ").");
        label->setFontSize(24);
        label->setMarginBottom(40);
        root->addView(label);

        auto buttonRow = new brls::Box();
        buttonRow->setAxis(brls::Axis::ROW);

        auto updateButton = new brls::Button();
        updateButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        updateButton->setText("Update Now");
        updateButton->setWidth(280);
        updateButton->setHeight(70);
        updateButton->setMarginRight(20);
        updateButton->registerClickAction([this](brls::View*) {
            StartDownload();
            return true;
        });
        buttonRow->addView(updateButton);

        auto laterButton = new brls::Button();
        laterButton->setStyle(&brls::BUTTONSTYLE_DEFAULT);
        laterButton->setText("Later");
        laterButton->setWidth(280);
        laterButton->setHeight(70);
        laterButton->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
        buttonRow->addView(laterButton);

        root->addView(buttonRow);
    }
    else if (phase == Phase::Downloading)
    {
        m_progressLabel = new brls::Label();
        m_progressLabel->setFontSize(22);
        m_progressLabel->setText("Downloading update...");
        root->addView(m_progressLabel);
    }
    else if (phase == Phase::Error)
    {
        auto label = new brls::Label();
        label->setText("Update failed: " + m_errorText);
        label->setFontSize(22);
        label->setMarginBottom(30);
        root->addView(label);

        auto backButton = new brls::Button();
        backButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        backButton->setText("Back");
        backButton->setWidth(280);
        backButton->setHeight(70);
        backButton->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
        root->addView(backButton);
    }

    frame->setContentView(root);
    this->setContentView(frame);

    brls::Application::giveFocus(this->getDefaultFocus());
}
