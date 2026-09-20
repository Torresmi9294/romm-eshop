#pragma once

// romm-eshop shim: stands in for Awoo Installer's i18n system. The vendored
// install engine uses the `"key"_lang` literal purely for user-facing status
// strings; we just resolve the handful of keys it actually uses to their
// English text (see lang.cpp) instead of pulling in Awoo's full lang.json
// infrastructure.

#include <string>

std::string RommEshopLangLookup(const std::string& key);

inline std::string operator""_lang(const char* key, size_t size)
{
    return RommEshopLangLookup(std::string(key, size));
}
