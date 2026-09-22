#pragma once

// Keep in sync with APP_VERSION in the Makefile (there's no clean way to
// share one source of truth between Make and C++ here without extra
// tooling, so it's manual -- both get bumped together on release).
namespace romm
{
    constexpr const char* APP_VERSION = "0.1.5";
}
