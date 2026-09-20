#pragma once

#include <string>

// Persistent app config, stored at CONFIG_PATH on the SD card.
// Holds the RomM server URL and the device-auth token obtained via
// the pairing flow (see api/romm_client.hpp). No password is ever stored.
namespace romm::config
{
    constexpr const char* CONFIG_DIR  = "sdmc:/switch/romm-eshop";
    constexpr const char* CONFIG_PATH = "sdmc:/switch/romm-eshop/config.json";
    constexpr const char* COVER_CACHE_DIR = "sdmc:/switch/romm-eshop/cache/covers";
    constexpr const char* DOWNLOAD_DIR = "sdmc:/switch/romm-eshop/downloads";

    struct Config
    {
        std::string serverUrl;               // e.g. http://192.168.68.68:89
        std::string clientDeviceIdentifier;   // random id generated once, persisted
        std::string deviceId;                 // returned by RomM after pairing
        std::string accessToken;              // bearer token returned by RomM after pairing
        bool paired = false;
    };

    // Loads config.json from the SD card. Returns a default (unpaired) config
    // if the file does not exist or fails to parse.
    Config Load();

    // Persists the config to the SD card, creating CONFIG_DIR if needed.
    bool Save(const Config& config);

    // Generates a stable-for-this-install random device identifier.
    std::string GenerateDeviceIdentifier();
}
