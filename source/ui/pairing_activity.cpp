#include "ui/pairing_activity.hpp"
#include "ui/software_keyboard.hpp"
#include "ui/store_activity.hpp"
#include "app_state.hpp"
#include "config.hpp"

using namespace romm::ui;

namespace
{
    constexpr const char* DEFAULT_SERVER_HINT = "http://192.168.1.100:8080";
}

PairingActivity::PairingActivity()
    : m_alive(std::make_shared<std::atomic<bool>>(true))
{
}

PairingActivity::~PairingActivity()
{
    *m_alive = false;
    if (m_workThread.joinable())
        m_workThread.detach();
}

brls::View* PairingActivity::createContentView()
{
    return nullptr; // built in RebuildContent() once we know what phase we're in
}

void PairingActivity::onContentAvailable()
{
    m_pollTask = std::make_unique<PollTask>(this);
    m_pollTask->start();

    // Render *something* before any blocking call (swkbd, network) runs --
    // onContentAvailable() executes before borealis's main loop has drawn
    // its first frame, so anything blocking here would otherwise leave the
    // screen blank until it returns.
    RebuildContent();

    auto& app = romm::AppState::Instance();
    if (app.config.serverUrl.empty())
        PromptForServerUrl();
    else
        StartPairing();
}

void PairingActivity::PromptForServerUrl()
{
    auto& app = romm::AppState::Instance();

    std::string result;
    bool ok = ShowSoftwareKeyboard(
        "RomM server address (e.g. http://192.168.1.100:8080)",
        app.config.serverUrl.empty() ? DEFAULT_SERVER_HINT : app.config.serverUrl,
        result, 200);

    if (ok && !result.empty())
    {
        app.config.serverUrl = result;
        StartPairing();
    }
    else if (app.config.serverUrl.empty())
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_errorText = "A server address is required to continue.";
        m_phase = Phase::Error;
    }
    else
    {
        StartPairing();
    }
}

void PairingActivity::StartPairing()
{
    m_phase = Phase::Initiating;

    if (m_workThread.joinable())
        m_workThread.detach();

    auto alive = m_alive;
    m_workThread = std::thread([this, alive]() {
        RunPairingFlow();
    });
}

void PairingActivity::RunPairingFlow()
{
    auto alive = m_alive;
    auto& app = romm::AppState::Instance();

    if (app.config.clientDeviceIdentifier.empty())
        app.config.clientDeviceIdentifier = romm::config::GenerateDeviceIdentifier();

    app.RebuildClient();

    romm::api::DeviceAuthInit init;
    std::string error;
    bool ok = app.client->DeviceAuthInitiate(app.config.clientDeviceIdentifier, init, error);
    if (!*alive) return;

    if (!ok)
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_errorText = error;
        m_phase = Phase::Error;
        return;
    }

    std::string deviceCode = init.deviceCode;
    int intervalSeconds = init.intervalSeconds > 0 ? init.intervalSeconds : 5;
    auto expiresAt = std::chrono::steady_clock::now() +
                      std::chrono::seconds(init.expiresInSeconds > 0 ? init.expiresInSeconds : 600);

    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_userCode = init.userCode;
        m_verificationUrl = init.verificationUrl;
    }
    m_phase = Phase::WaitingApproval;

    while (*alive)
    {
        auto sleepUntil = std::chrono::steady_clock::now() + std::chrono::seconds(intervalSeconds);
        while (*alive && std::chrono::steady_clock::now() < sleepUntil)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!*alive) return;

        if (std::chrono::steady_clock::now() >= expiresAt)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = "Pairing code expired. Please try again.";
            m_phase = Phase::Error;
            return;
        }

        romm::api::DeviceAuthToken token;
        auto status = app.client->DeviceAuthPoll(deviceCode, token);
        if (!*alive) return;

        if (status == romm::api::DeviceAuthStatus::Ok)
        {
            app.config.deviceId = token.deviceId;
            app.config.accessToken = token.accessToken;
            app.config.paired = true;
            romm::config::Save(app.config);
            app.RebuildClient();
            m_phase = Phase::Approved;
            return;
        }
        else if (status == romm::api::DeviceAuthStatus::Denied)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = "Pairing was denied.";
            m_phase = Phase::Error;
            return;
        }
        else if (status == romm::api::DeviceAuthStatus::Expired)
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = "Pairing code expired. Please try again.";
            m_phase = Phase::Error;
            return;
        }
        // Pending or NetworkError: loop again until expiry.
    }
}

void PairingActivity::Tick()
{
    Phase phase = m_phase;
    if (phase == m_lastRenderedPhase)
        return;
    m_lastRenderedPhase = phase;

    if (phase == Phase::Approved)
    {
        m_pollTask->stop();
        brls::Application::pushActivity(new romm::ui::StoreActivity());
        return;
    }

    RebuildContent();
}

void PairingActivity::RebuildContent()
{
    auto frame = new brls::AppletFrame();
    frame->setTitle("RomM eShop — Pair this Switch");

    auto root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setJustifyContent(brls::JustifyContent::CENTER);
    root->setAlignItems(brls::AlignItems::CENTER);
    root->setGrow(1);
    root->setPadding(60, 80, 60, 80);

    std::lock_guard<std::mutex> lock(m_textMutex);
    Phase phase = m_phase;

    if (phase == Phase::Initiating)
    {
        auto label = new brls::Label();
        label->setText("Connecting to your RomM server...");
        label->setFontSize(28);
        root->addView(label);
    }
    else if (phase == Phase::WaitingApproval)
    {
        auto instructions = new brls::Label();
        instructions->setText("On your phone or computer, open this page while logged into RomM:");
        instructions->setFontSize(22);
        instructions->setMarginBottom(20);
        root->addView(instructions);

        auto urlLabel = new brls::Label();
        urlLabel->setText(m_verificationUrl);
        urlLabel->setFontSize(24);
        urlLabel->setMarginBottom(40);
        root->addView(urlLabel);

        auto codeHint = new brls::Label();
        codeHint->setText("Confirm this code matches:");
        codeHint->setFontSize(20);
        root->addView(codeHint);

        auto codeLabel = new brls::Label();
        codeLabel->setText(m_userCode);
        codeLabel->setFontSize(56);
        codeLabel->setMarginTop(10);
        codeLabel->setMarginBottom(40);
        root->addView(codeLabel);

        auto statusLabel = new brls::Label();
        statusLabel->setText("Waiting for approval...");
        statusLabel->setFontSize(20);
        root->addView(statusLabel);
    }
    else if (phase == Phase::Error)
    {
        auto label = new brls::Label();
        label->setText("Couldn't pair: " + m_errorText);
        label->setFontSize(24);
        label->setMarginBottom(30);
        root->addView(label);

        auto retryButton = new brls::Button();
        retryButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        retryButton->setText("Try Again");
        retryButton->setWidth(300);
        retryButton->setHeight(70);
        retryButton->registerClickAction([this](brls::View*) {
            PromptForServerUrl();
            return true;
        });
        root->addView(retryButton);
    }
    else
    {
        auto label = new brls::Label();
        label->setText("Starting...");
        root->addView(label);
    }

    frame->setContentView(root);
    this->setContentView(frame);

    // See StoreActivity::RebuildContent() for why this is needed on every
    // rebuild, not just the first: setContentView() alone doesn't move focus.
    brls::Application::giveFocus(this->getDefaultFocus());
}
