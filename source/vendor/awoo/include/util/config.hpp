#pragma once

// romm-eshop shim: stands in for Awoo Installer's util/config.hpp. Only the
// three flags the vendored install engine actually reads are provided.
// gayMode/appDir only gate an optional install-complete sound effect, which
// we don't ship, so gayMode is fixed true (silences it) and appDir points
// nowhere.

#include <string>

namespace inst::config
{
    // Verify each NCA's Nintendo signature before installing it. Keep this
    // on: it's the only integrity check standing between a corrupted/partial
    // download and content actually being registered on the console.
    extern bool validateNCAs;

    extern bool gayMode;
    extern std::string appDir;
}
