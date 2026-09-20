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

PairingActivity::PairingActivity() = default;
PairingActivity::~PairingActivity() = default;

brls::View* PairingActivity::createContentView()
{
    return nullptr; // built in RebuildContent() once we know what phase we're in
}

void PairingActivity::onContentAvailable()
{
    m_pollTask = std::make_unique<PollTask>(this);
    m_pollTask->start();

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
        RebuildContent();
    }
    else
    {
        StartPairing();
    }
}

void PairingActivity::StartPairing()
{
    m_phase = Phase::Initiating;
    RebuildContent();

    auto& app = romm::AppState::Instance();
    if (app.config.clientDeviceIdentifier.empty())
        app.config.clientDeviceIdentifier = romm::config::GenerateDeviceIdentifier();

    app.RebuildClient();

    romm::api::DeviceAuthInit init;
    std::string error;
    bool ok = app.client->DeviceAuthInitiate(app.config.clientDeviceIdentifier, init, error);

    std::lock_guard<std::mutex> lock(m_textMutex);
    if (!ok)
    {
        m_errorText = error;
        m_phase = Phase::Error;
        RebuildContent();
        return;
    }

    m_deviceCode = init.deviceCode;
    m_userCode = init.userCode;
    m_verificationUrl = init.verificationUrl;
    m_intervalSeconds = init.intervalSeconds > 0 ? init.intervalSeconds : 5;
    m_expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(init.expiresInSeconds > 0 ? init.expiresInSeconds : 600);
    m_nextPollAt = std::chrono::steady_clock::now() + std::chrono::seconds(m_intervalSeconds);
    m_phase = Phase::WaitingApproval;
    RebuildContent();
}

void PairingActivity::Tick()
{
    if (m_phase != Phase::WaitingApproval)
        return;

    auto now = std::chrono::steady_clock::now();

    if (now >= m_expiresAt)
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_errorText = "Pairing code expired. Please try again.";
        m_phase = Phase::Error;
        RebuildContent();
        return;
    }

    if (now < m_nextPollAt)
        return;

    m_nextPollAt = now + std::chrono::seconds(m_intervalSeconds);

    auto& app = romm::AppState::Instance();
    romm::api::DeviceAuthToken token;
    auto status = app.client->DeviceAuthPoll(m_deviceCode, token);

    switch (status)
    {
        case romm::api::DeviceAuthStatus::Ok:
        {
            app.config.deviceId = token.deviceId;
            app.config.accessToken = token.accessToken;
            app.config.paired = true;
            romm::config::Save(app.config);
            app.RebuildClient();

            m_phase = Phase::Approved;
            m_pollTask->stop();
            brls::Application::pushActivity(new romm::ui::StoreActivity());
            break;
        }
        case romm::api::DeviceAuthStatus::Denied:
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = "Pairing was denied.";
            m_phase = Phase::Error;
            RebuildContent();
            break;
        }
        case romm::api::DeviceAuthStatus::Expired:
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_errorText = "Pairing code expired. Please try again.";
            m_phase = Phase::Error;
            RebuildContent();
            break;
        }
        case romm::api::DeviceAuthStatus::Pending:
        case romm::api::DeviceAuthStatus::NetworkError:
            // Keep waiting; a transient network hiccup shouldn't kill the flow.
            break;
    }
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

        m_urlLabel = new brls::Label();
        m_urlLabel->setText(m_verificationUrl);
        m_urlLabel->setFontSize(24);
        m_urlLabel->setMarginBottom(40);
        root->addView(m_urlLabel);

        auto codeHint = new brls::Label();
        codeHint->setText("Confirm this code matches:");
        codeHint->setFontSize(20);
        root->addView(codeHint);

        m_codeLabel = new brls::Label();
        m_codeLabel->setText(m_userCode);
        m_codeLabel->setFontSize(56);
        m_codeLabel->setMarginTop(10);
        m_codeLabel->setMarginBottom(40);
        root->addView(m_codeLabel);

        m_statusLabel = new brls::Label();
        m_statusLabel->setText("Waiting for approval...");
        m_statusLabel->setFontSize(20);
        root->addView(m_statusLabel);
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
}
