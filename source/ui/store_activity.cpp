#include "ui/store_activity.hpp"
#include "ui/game_detail_activity.hpp"
#include "ui/update_activity.hpp"
#include "app_state.hpp"
#include "config.hpp"

#include <sstream>
#include <sys/stat.h>

using namespace romm::ui;

namespace
{
    constexpr int CELLS_PER_ROW = 4;
    constexpr float CELL_WIDTH  = 280;
    constexpr float COVER_WIDTH = 260;
    constexpr float COVER_HEIGHT = 350;

    std::string CoverCachePath(const romm::api::Rom& rom)
    {
        return std::string(romm::config::COVER_CACHE_DIR) + "/" + std::to_string(rom.id) + ".cover";
    }
}

StoreActivity::StoreActivity()
    : m_alive(std::make_shared<std::atomic<bool>>(true))
{
}

StoreActivity::~StoreActivity()
{
    *m_alive = false;
    if (m_loadThread.joinable())
        m_loadThread.detach();
    if (m_updateCheckThread.joinable())
        m_updateCheckThread.detach();
}

brls::View* StoreActivity::createContentView()
{
    return nullptr;
}

void StoreActivity::onContentAvailable()
{
    brls::Logger::debug("StoreActivity: onContentAvailable begin");

    m_pollTask = std::make_unique<PollTask>(this);
    m_pollTask->start();

    this->registerAction("Exit", brls::BUTTON_START, [](brls::View*) {
        brls::Application::quit();
        return true;
    });

    RebuildContent();
    brls::Logger::debug("StoreActivity: initial RebuildContent done, starting load");
    StartLoading();
    brls::Logger::debug("StoreActivity: onContentAvailable end");
}

void StoreActivity::StartLoading()
{
    m_phase = Phase::Loading;

    auto alive = m_alive;
    m_loadThread = std::thread([this, alive]() {
        brls::Logger::debug("StoreActivity: load thread started");
        auto& app = romm::AppState::Instance();

        romm::api::Platform platform;
        std::string error;
        brls::Logger::debug("StoreActivity: calling GetSwitchPlatform");
        if (!app.client->GetSwitchPlatform(platform, error))
        {
            brls::Logger::error("StoreActivity: GetSwitchPlatform failed: {}", error);
            if (!*alive) return;
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_errorText = error;
            m_phase = Phase::Error;
            return;
        }
        brls::Logger::debug("StoreActivity: GetSwitchPlatform ok, platform.id={} romCount={}", platform.id, platform.romCount);

        std::vector<romm::api::Rom> roms;
        brls::Logger::debug("StoreActivity: calling GetRoms");
        if (!app.client->GetRoms(platform.id, roms, error))
        {
            brls::Logger::error("StoreActivity: GetRoms failed: {}", error);
            if (!*alive) return;
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_errorText = error;
            m_phase = Phase::Error;
            return;
        }
        brls::Logger::debug("StoreActivity: GetRoms ok, count={}", roms.size());

        mkdir("sdmc:/switch", 0777);
        mkdir(romm::config::CONFIG_DIR, 0777);
        mkdir((std::string(romm::config::CONFIG_DIR) + "/cache").c_str(), 0777); // mkdir isn't recursive
        mkdir(romm::config::COVER_CACHE_DIR, 0777);

        int coverIdx = 0;
        for (auto& rom : roms)
        {
            if (!*alive) return;
            brls::Logger::debug("StoreActivity: downloading cover {}/{} rom.id={}", ++coverIdx, roms.size(), rom.id);
            bool ok = app.client->DownloadCoverArt(rom, CoverCachePath(rom));
            brls::Logger::debug("StoreActivity: cover {} result={}", rom.id, ok);
        }
        brls::Logger::debug("StoreActivity: all covers processed");

        if (!*alive) return;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_roms = std::move(roms);
            m_phase = Phase::Ready;
        }
        brls::Logger::debug("StoreActivity: load thread finished");
    });
}

void StoreActivity::StartUpdateCheck()
{
    auto alive = m_alive;
    m_updateCheckThread = std::thread([this, alive]() {
        // Deliberately runs on its own thread, well after the grid has
        // already rendered successfully (see Tick()) -- not chained onto
        // the load thread. This is the app's first-ever HTTPS/TLS call
        // (everything else talks plain HTTP to the user's own RomM server),
        // and doing that heavy handshake immediately before allocating
        // ~250 UI objects for the grid was crashing the app (confirmed via
        // an Atmosphere crash report -- a User Break/abort on the main
        // thread, right after the update check, inside grid construction).
        // Best-effort and non-blocking either way: a failed/slow check just
        // means no update prompt this session.
        brls::Logger::debug("StoreActivity: calling CheckForUpdate (first HTTPS request)");
        romm::updater::UpdateInfo info;
        bool available = false;
        std::string updateError;
        bool checkOk = romm::updater::CheckForUpdate(info, available, updateError);
        brls::Logger::debug("StoreActivity: CheckForUpdate returned ok={} available={} error=\"{}\"", checkOk, available, updateError);
        if (!*alive) return;
        if (checkOk && available)
        {
            std::lock_guard<std::mutex> lock(m_updateMutex);
            m_updateInfo = info;
            m_updateAvailable = true;
        }
    });
}

void StoreActivity::Tick()
{
    if (m_phase == Phase::Ready && m_updateAvailable && !m_updatePromptShown)
    {
        m_updatePromptShown = true;
        std::lock_guard<std::mutex> lock(m_updateMutex);
        brls::Application::pushActivity(new romm::ui::UpdateActivity(m_updateInfo));
    }

    if (m_phase != m_lastRenderedPhase)
    {
        m_lastRenderedPhase = m_phase.load();
        RebuildContent();

        if (m_lastRenderedPhase == Phase::Ready && !m_updateCheckStarted)
        {
            m_updateCheckStarted = true;
            StartUpdateCheck();
        }
    }
}

void StoreActivity::RebuildContent()
{
    try
    {
        RebuildContentUnsafe();
    }
    catch (const std::exception& e)
    {
        brls::Logger::error("StoreActivity: RebuildContent threw: {}", e.what());
        auto frame = new brls::AppletFrame();
        frame->setTitle("Error");
        auto root = new brls::Box();
        root->setAxis(brls::Axis::COLUMN);
        root->setJustifyContent(brls::JustifyContent::CENTER);
        root->setAlignItems(brls::AlignItems::CENTER);
        root->setGrow(1);
        root->setPadding(60, 80, 60, 80);
        auto label = new brls::Label();
        label->setText(std::string("Something went wrong showing the store: ") + e.what());
        label->setFontSize(22);
        root->addView(label);
        frame->setContentView(root);
        this->setContentView(frame);
    }

    brls::Application::giveFocus(this->getDefaultFocus());
}

void StoreActivity::RebuildContentUnsafe()
{
    brls::Logger::debug("StoreActivity: RebuildContent begin");
    auto frame = new brls::AppletFrame();
    frame->setTitle("RomM eShop");

    std::lock_guard<std::mutex> lock(m_dataMutex);
    Phase phase = m_phase;

    if (phase == Phase::Loading)
    {
        auto root = new brls::Box();
        root->setAxis(brls::Axis::COLUMN);
        root->setJustifyContent(brls::JustifyContent::CENTER);
        root->setAlignItems(brls::AlignItems::CENTER);
        root->setGrow(1);

        auto label = new brls::Label();
        label->setText("Loading your library...");
        label->setFontSize(28);
        root->addView(label);

        frame->setContentView(root);
    }
    else if (phase == Phase::Error)
    {
        auto root = new brls::Box();
        root->setAxis(brls::Axis::COLUMN);
        root->setJustifyContent(brls::JustifyContent::CENTER);
        root->setAlignItems(brls::AlignItems::CENTER);
        root->setGrow(1);
        root->setPadding(60, 80, 60, 80);

        auto label = new brls::Label();
        label->setText("Couldn't load your library: " + m_errorText);
        label->setFontSize(24);
        label->setMarginBottom(30);
        root->addView(label);

        auto retryButton = new brls::Button();
        retryButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        retryButton->setText("Retry");
        retryButton->setWidth(300);
        retryButton->setHeight(70);
        retryButton->registerClickAction([this](brls::View*) {
            StartLoading();
            return true;
        });
        root->addView(retryButton);

        frame->setContentView(root);
    }
    else // Ready
    {
        auto scroll = new brls::ScrollingFrame();
        scroll->setGrow(1);

        auto grid = new brls::Box();
        grid->setAxis(brls::Axis::COLUMN);
        grid->setPadding(30, 40, 30, 40);

        brls::Box* row = nullptr;

        for (size_t i = 0; i < m_roms.size(); i++)
        {
            if (i % CELLS_PER_ROW == 0)
            {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                row->setMarginBottom(30);
                grid->addView(row);
            }

            const romm::api::Rom& rom = m_roms[i];

            auto cell = new brls::Box();
            cell->setAxis(brls::Axis::COLUMN);
            cell->setWidth(CELL_WIDTH);
            cell->setAlignItems(brls::AlignItems::CENTER);
            cell->setMarginRight(20);
            cell->setFocusable(true);

            auto cover = new brls::Image();
            cover->setWidth(COVER_WIDTH);
            cover->setHeight(COVER_HEIGHT);
            cover->setScalingType(brls::ImageScalingType::FIT);
            std::string coverPath = CoverCachePath(rom);
            struct stat st;
            if (stat(coverPath.c_str(), &st) == 0 && st.st_size > 0)
                cover->setImageFromFile(coverPath);
            cell->addView(cover);

            auto title = new brls::Label();
            title->setText(rom.name.empty() ? rom.fsName : rom.name);
            title->setFontSize(18);
            title->setWidth(COVER_WIDTH);
            title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            title->setMarginTop(10);
            cell->addView(title);

            romm::api::Rom romCopy = rom;
            cell->registerClickAction([romCopy](brls::View*) {
                brls::Logger::debug("StoreActivity: cell clicked, rom.id={} name=\"{}\"", romCopy.id, romCopy.name);
                brls::Application::pushActivity(new romm::ui::GameDetailActivity(romCopy));
                brls::Logger::debug("StoreActivity: pushActivity returned");
                return true;
            });

            row->addView(cell);
        }

        scroll->setContentView(grid);
        frame->setContentView(scroll);
    }

    this->setContentView(frame);
    brls::Logger::debug("StoreActivity: RebuildContent end (setContentView done)");
}
