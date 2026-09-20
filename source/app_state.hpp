#pragma once

#include <memory>
#include "config.hpp"
#include "api/romm_client.hpp"

// Process-wide singleton holding the loaded config and the RomM API client.
// Deliberately simple (no DI framework) -- this is a small, single-window app.
namespace romm
{
    class AppState
    {
      public:
        static AppState& Instance();

        config::Config config;
        std::unique_ptr<api::RommClient> client;

        // (Re)creates `client` from the current `config`.
        void RebuildClient();

      private:
        AppState() = default;
    };
}
