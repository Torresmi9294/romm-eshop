#include "install/install_progress_state.hpp"

namespace romm::install
{
    InstallProgressState& InstallProgressState::Instance()
    {
        static InstallProgressState instance;
        return instance;
    }

    void InstallProgressState::Reset()
    {
        m_percent = 0;
        m_verificationFailed = false;
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_statusText.clear();
    }

    void InstallProgressState::SetPercent(int percent)
    {
        m_percent = percent;
    }

    int InstallProgressState::GetPercent() const
    {
        return m_percent;
    }

    void InstallProgressState::SetStatusText(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_statusText = text;
    }

    std::string InstallProgressState::GetStatusText() const
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        return m_statusText;
    }

    void InstallProgressState::SetVerificationFailed(bool failed)
    {
        m_verificationFailed = failed;
    }

    bool InstallProgressState::GetVerificationFailed() const
    {
        return m_verificationFailed;
    }
}
