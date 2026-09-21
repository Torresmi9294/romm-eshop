#include "api/romm_client.hpp"

#include <curl/curl.h>
#include <jansson.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

namespace romm::api
{
    namespace
    {
        constexpr const char* USER_AGENT = "romm-eshop/0.1.0 (Nintendo Switch)";

        size_t WriteToString(char* ptr, size_t size, size_t nmemb, void* userdata)
        {
            auto* out = static_cast<std::string*>(userdata);
            out->append(ptr, size * nmemb);
            return size * nmemb;
        }

        size_t WriteToFile(char* ptr, size_t size, size_t nmemb, void* userdata)
        {
            auto* file = static_cast<FILE*>(userdata);
            return fwrite(ptr, size, nmemb, file);
        }

        struct DownloadProgressCtx
        {
            int64_t resumeFrom = 0;
            const ProgressCallback* callback = nullptr;
        };

        int XferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
        {
            auto* ctx = static_cast<DownloadProgressCtx*>(clientp);
            if (!ctx->callback || !*ctx->callback)
                return 0;

            int64_t total = dltotal > 0 ? (ctx->resumeFrom + dltotal) : 0;
            int64_t done  = ctx->resumeFrom + dlnow;

            bool keepGoing = (*ctx->callback)(done, total);
            return keepGoing ? 0 : 1; // non-zero aborts the transfer
        }

        // Extracts the RomM/FastAPI {"detail": "..."} error message, if any.
        std::string ExtractDetail(const std::string& body)
        {
            json_error_t error;
            json_t* root = json_loads(body.c_str(), 0, &error);
            if (!root)
                return "";

            std::string detail;
            json_t* detailNode = json_object_get(root, "detail");
            if (json_is_string(detailNode))
                detail = json_string_value(detailNode);

            json_decref(root);
            return detail;
        }

        std::string UrlEncode(const std::string& value)
        {
            CURL* curl = curl_easy_init();
            if (!curl)
                return value;

            char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
            std::string result = encoded ? encoded : value;
            if (encoded)
                curl_free(encoded);
            curl_easy_cleanup(curl);
            return result;
        }
    }

    RommClient::RommClient(std::string serverUrl, std::string accessToken)
        : m_serverUrl(std::move(serverUrl)), m_accessToken(std::move(accessToken))
    {
        // Strip a trailing slash so BuildUrl() never produces a double slash.
        while (!m_serverUrl.empty() && m_serverUrl.back() == '/')
            m_serverUrl.pop_back();
    }

    RommClient::~RommClient() = default;

    std::string RommClient::BuildUrl(const std::string& path) const
    {
        return m_serverUrl + path;
    }

    bool RommClient::DeviceAuthInitiate(const std::string& clientDeviceIdentifier, DeviceAuthInit& out, std::string& errorOut)
    {
        json_t* body = json_object();
        json_object_set_new(body, "client_device_identifier", json_string(clientDeviceIdentifier.c_str()));
        json_object_set_new(body, "name", json_string("Nintendo Switch"));
        json_object_set_new(body, "client", json_string("romm-eshop"));
        json_object_set_new(body, "platform", json_string("switch"));
        json_object_set_new(body, "client_version", json_string("0.1.0"));

        json_t* scopes = json_array();
        json_array_append_new(scopes, json_string("me.read"));
        json_array_append_new(scopes, json_string("roms.read"));
        json_array_append_new(scopes, json_string("platforms.read"));
        json_array_append_new(scopes, json_string("assets.read"));
        json_object_set_new(body, "requested_scopes", scopes);

        char* bodyStr = json_dumps(body, JSON_COMPACT);
        json_decref(body);

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            free(bodyStr);
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        std::string response;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        std::string url = BuildUrl("/api/auth/device/init");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(bodyStr);

        if (res != CURLE_OK)
        {
            errorOut = std::string("Network error: ") + curl_easy_strerror(res);
            return false;
        }

        if (httpCode < 200 || httpCode >= 300)
        {
            std::string detail = ExtractDetail(response);
            errorOut = detail.empty() ? ("Server returned HTTP " + std::to_string(httpCode)) : detail;
            return false;
        }

        json_error_t error;
        json_t* root = json_loads(response.c_str(), 0, &error);
        if (!root)
        {
            errorOut = "Failed to parse server response";
            return false;
        }

        json_t* deviceCode = json_object_get(root, "device_code");
        json_t* userCode    = json_object_get(root, "user_code");
        json_t* verifyPath  = json_object_get(root, "verification_path_complete");
        json_t* expiresIn   = json_object_get(root, "expires_in");
        json_t* interval    = json_object_get(root, "interval");

        if (json_is_string(deviceCode)) out.deviceCode = json_string_value(deviceCode);
        if (json_is_string(userCode)) out.userCode = json_string_value(userCode);
        if (json_is_string(verifyPath)) out.verificationUrl = BuildUrl(json_string_value(verifyPath));
        if (json_is_integer(expiresIn)) out.expiresInSeconds = static_cast<int>(json_integer_value(expiresIn));
        if (json_is_integer(interval)) out.intervalSeconds = static_cast<int>(json_integer_value(interval));

        json_decref(root);
        return true;
    }

    DeviceAuthStatus RommClient::DeviceAuthPoll(const std::string& deviceCode, DeviceAuthToken& out)
    {
        json_t* body = json_object();
        json_object_set_new(body, "device_code", json_string(deviceCode.c_str()));
        char* bodyStr = json_dumps(body, JSON_COMPACT);
        json_decref(body);

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            free(bodyStr);
            return DeviceAuthStatus::NetworkError;
        }

        std::string response;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        std::string url = BuildUrl("/api/auth/device/token");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(bodyStr);

        if (res != CURLE_OK)
            return DeviceAuthStatus::NetworkError;

        if (httpCode >= 200 && httpCode < 300)
        {
            json_error_t error;
            json_t* root = json_loads(response.c_str(), 0, &error);
            if (!root)
                return DeviceAuthStatus::NetworkError;

            json_t* accessToken = json_object_get(root, "access_token");
            json_t* deviceId     = json_object_get(root, "device_id");
            if (json_is_string(accessToken)) out.accessToken = json_string_value(accessToken);
            if (json_is_string(deviceId)) out.deviceId = json_string_value(deviceId);

            json_decref(root);
            return DeviceAuthStatus::Ok;
        }

        // Follows RFC 8628 device-flow error codes.
        std::string detail = ExtractDetail(response);
        if (detail == "authorization_pending" || detail == "slow_down")
            return DeviceAuthStatus::Pending;
        if (detail == "expired_token")
            return DeviceAuthStatus::Expired;
        if (detail == "access_denied")
            return DeviceAuthStatus::Denied;

        // Unknown 4xx: treat as pending so a transient hiccup doesn't kill the
        // flow early. The caller enforces the real expiry client-side.
        return DeviceAuthStatus::Pending;
    }

    bool RommClient::GetSwitchPlatform(Platform& out, std::string& errorOut)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        std::string response;
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + m_accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = BuildUrl("/api/platforms");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            errorOut = std::string("Network error: ") + curl_easy_strerror(res);
            return false;
        }
        if (httpCode < 200 || httpCode >= 300)
        {
            std::string detail = ExtractDetail(response);
            errorOut = detail.empty() ? ("Server returned HTTP " + std::to_string(httpCode)) : detail;
            return false;
        }

        json_error_t error;
        json_t* root = json_loads(response.c_str(), 0, &error);
        if (!root || !json_is_array(root))
        {
            errorOut = "Failed to parse platform list";
            if (root) json_decref(root);
            return false;
        }

        size_t index;
        json_t* item;
        bool found = false;
        json_array_foreach(root, index, item)
        {
            json_t* fsSlug = json_object_get(item, "fs_slug");
            if (!json_is_string(fsSlug) || strcmp(json_string_value(fsSlug), "switch") != 0)
                continue;

            json_t* id       = json_object_get(item, "id");
            json_t* name      = json_object_get(item, "name");
            json_t* romCount  = json_object_get(item, "rom_count");

            if (json_is_integer(id)) out.id = static_cast<int>(json_integer_value(id));
            out.fsSlug = "switch";
            if (json_is_string(name)) out.name = json_string_value(name);
            if (json_is_integer(romCount)) out.romCount = static_cast<int>(json_integer_value(romCount));
            found = true;
            break;
        }

        json_decref(root);

        if (!found)
        {
            errorOut = "No Switch platform found on this RomM server";
            return false;
        }
        return true;
    }

    bool RommClient::GetRoms(int platformId, std::vector<Rom>& out, std::string& errorOut)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        std::string response;
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + m_accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = BuildUrl("/api/roms?platform_ids=" + std::to_string(platformId) +
                                    "&limit=1000&offset=0"
                                    "&with_char_index=false&with_filter_values=false"
                                    "&with_rom_id_index=false&with_total=false"
                                    "&order_by=name&order_dir=asc");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            errorOut = std::string("Network error: ") + curl_easy_strerror(res);
            return false;
        }
        if (httpCode < 200 || httpCode >= 300)
        {
            std::string detail = ExtractDetail(response);
            errorOut = detail.empty() ? ("Server returned HTTP " + std::to_string(httpCode)) : detail;
            return false;
        }

        json_error_t error;
        json_t* root = json_loads(response.c_str(), 0, &error);
        if (!root)
        {
            errorOut = "Failed to parse rom list";
            return false;
        }

        json_t* items = json_object_get(root, "items");
        if (!json_is_array(items))
        {
            errorOut = "Unexpected rom list response shape";
            json_decref(root);
            return false;
        }

        size_t index;
        json_t* item;
        json_array_foreach(items, index, item)
        {
            Rom rom;
            json_t* id            = json_object_get(item, "id");
            json_t* name           = json_object_get(item, "name");
            json_t* summary        = json_object_get(item, "summary");
            json_t* fsName         = json_object_get(item, "fs_name");
            json_t* fsExtension    = json_object_get(item, "fs_extension");
            json_t* fsSizeBytes    = json_object_get(item, "fs_size_bytes");
            json_t* pathCoverLarge = json_object_get(item, "path_cover_large");
            json_t* pathCoverSmall = json_object_get(item, "path_cover_small");

            if (json_is_integer(id)) rom.id = static_cast<int>(json_integer_value(id));
            if (json_is_string(name)) rom.name = json_string_value(name);
            else if (json_is_string(fsName)) rom.name = json_string_value(fsName);
            if (json_is_string(summary)) rom.summary = json_string_value(summary);
            if (json_is_string(fsName)) rom.fsName = json_string_value(fsName);
            if (json_is_string(fsExtension)) rom.fsExtension = json_string_value(fsExtension);
            if (json_is_integer(fsSizeBytes)) rom.fsSizeBytes = json_integer_value(fsSizeBytes);
            if (json_is_string(pathCoverLarge)) rom.pathCoverLarge = json_string_value(pathCoverLarge);
            if (json_is_string(pathCoverSmall)) rom.pathCoverSmall = json_string_value(pathCoverSmall);

            out.push_back(std::move(rom));
        }

        json_decref(root);
        return true;
    }

    bool RommClient::DownloadCoverArt(const Rom& rom, const std::string& destPath)
    {
        std::string sourcePath = !rom.pathCoverLarge.empty() ? rom.pathCoverLarge : rom.pathCoverSmall;
        if (sourcePath.empty())
            return false;

        // RomM's path_cover_* includes a "?ts=2026-08-21 16:51:47" style
        // cache-busting query string with an unencoded space, which curl's
        // URL parser can reject outright. We cache covers locally forever,
        // so the cache-busting param serves no purpose here -- just drop it.
        auto queryPos = sourcePath.find('?');
        if (queryPos != std::string::npos)
            sourcePath = sourcePath.substr(0, queryPos);

        struct stat st;
        if (stat(destPath.c_str(), &st) == 0 && st.st_size > 0)
            return true; // already cached

        CURL* curl = curl_easy_init();
        if (!curl)
            return false;

        FILE* file = fopen(destPath.c_str(), "wb");
        if (!file)
        {
            curl_easy_cleanup(curl);
            return false;
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + m_accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = BuildUrl(sourcePath);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(file);

        bool ok = (res == CURLE_OK) && (httpCode >= 200 && httpCode < 300);
        if (!ok)
            remove(destPath.c_str());
        return ok;
    }

    bool RommClient::DownloadRomContent(const Rom& rom, const std::string& destPath, const ProgressCallback& onProgress, std::string& errorOut)
    {
        std::string tmpPath = destPath + ".part";

        int64_t resumeFrom = 0;
        struct stat st;
        if (stat(tmpPath.c_str(), &st) == 0)
            resumeFrom = st.st_size;

        FILE* file = fopen(tmpPath.c_str(), resumeFrom > 0 ? "ab" : "wb");
        if (!file)
        {
            errorOut = "Failed to open destination file for writing";
            return false;
        }

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            fclose(file);
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + m_accessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = BuildUrl("/api/roms/" + std::to_string(rom.id) + "/content/" + UrlEncode(rom.fsName));

        DownloadProgressCtx ctx;
        ctx.resumeFrom = resumeFrom;
        ctx.callback = &onProgress;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, XferInfoCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
        if (resumeFrom > 0)
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(resumeFrom));

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(file);

        if (res == CURLE_ABORTED_BY_CALLBACK)
        {
            errorOut = "Download cancelled";
            return false;
        }
        if (res != CURLE_OK)
        {
            errorOut = std::string("Network error: ") + curl_easy_strerror(res);
            return false;
        }
        // 200 = full content from offset 0, 206 = partial content honoring our Range/resume request.
        if (httpCode != 200 && httpCode != 206)
        {
            errorOut = "Server returned HTTP " + std::to_string(httpCode);
            return false;
        }

        remove(destPath.c_str());
        if (rename(tmpPath.c_str(), destPath.c_str()) != 0)
        {
            errorOut = "Failed to finalize downloaded file";
            return false;
        }

        return true;
    }
}
