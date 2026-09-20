#include "app_state.hpp"

namespace romm
{
    AppState& AppState::Instance()
    {
        static AppState instance;
        return instance;
    }

    void AppState::RebuildClient()
    {
        client = std::make_unique<api::RommClient>(config.serverUrl, config.accessToken);
    }
}
