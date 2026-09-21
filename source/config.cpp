#include "config.hpp"

#include <jansson.h>
#include <sys/stat.h>
#include <cstdio>
#include <random>
#include <sstream>
#include <iomanip>

namespace romm::config
{
    static void EnsureDir(const std::string& path)
    {
        mkdir(path.c_str(), 0777);
    }

    Config Load()
    {
        Config cfg;

        json_error_t error;
        json_t* root = json_load_file(CONFIG_PATH, 0, &error);
        if (!root)
        {
            cfg.clientDeviceIdentifier = GenerateDeviceIdentifier();
            return cfg;
        }

        json_t* serverUrl = json_object_get(root, "server_url");
        json_t* clientId   = json_object_get(root, "client_device_identifier");
        json_t* deviceId    = json_object_get(root, "device_id");
        json_t* accessToken = json_object_get(root, "access_token");
        json_t* paired      = json_object_get(root, "paired");

        if (json_is_string(serverUrl))
            cfg.serverUrl = json_string_value(serverUrl);
        if (json_is_string(clientId))
            cfg.clientDeviceIdentifier = json_string_value(clientId);
        if (json_is_string(deviceId))
            cfg.deviceId = json_string_value(deviceId);
        if (json_is_string(accessToken))
            cfg.accessToken = json_string_value(accessToken);
        if (json_is_boolean(paired))
            cfg.paired = json_is_true(paired);

        json_decref(root);

        if (cfg.clientDeviceIdentifier.empty())
            cfg.clientDeviceIdentifier = GenerateDeviceIdentifier();

        return cfg;
    }

    bool Save(const Config& config)
    {
        EnsureDir("sdmc:/switch");
        EnsureDir(CONFIG_DIR);
        EnsureDir(std::string(CONFIG_DIR) + "/cache"); // mkdir isn't recursive -- COVER_CACHE_DIR nests one level deeper
        EnsureDir(COVER_CACHE_DIR);
        EnsureDir(DOWNLOAD_DIR);

        json_t* root = json_object();
        json_object_set_new(root, "server_url", json_string(config.serverUrl.c_str()));
        json_object_set_new(root, "client_device_identifier", json_string(config.clientDeviceIdentifier.c_str()));
        json_object_set_new(root, "device_id", json_string(config.deviceId.c_str()));
        json_object_set_new(root, "access_token", json_string(config.accessToken.c_str()));
        json_object_set_new(root, "paired", json_boolean(config.paired));

        int rc = json_dump_file(root, CONFIG_PATH, JSON_INDENT(2));
        json_decref(root);

        return rc == 0;
    }

    std::string GenerateDeviceIdentifier()
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;

        std::ostringstream oss;
        oss << "switch-" << std::hex << std::setfill('0')
            << std::setw(16) << dist(gen)
            << std::setw(16) << dist(gen);
        return oss.str();
    }
}
