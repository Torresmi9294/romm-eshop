#include "updater/updater.hpp"
#include "version.hpp"

#include <curl/curl.h>
#include <jansson.h>
#include <switch.h>
#include <cstdio>
#include <sys/stat.h>

namespace romm::updater
{
    namespace
    {
        constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/Torresmi9294/romm-eshop/releases/latest";
        constexpr const char* INSTALL_PATH = "sdmc:/switch/romm-eshop/romm-eshop.nro";
        constexpr const char* STAGING_DIR = "sdmc:/switch/romm-eshop/update";
        constexpr const char* STAGING_PATH = "sdmc:/switch/romm-eshop/update/romm-eshop-new.nro";
        // switch-curl (mbedtls-backed) has no system trust store, so HTTPS
        // needs an explicit CA bundle. Bundled from https://curl.se/ca/cacert.pem.
        // Never disable peer verification here instead -- this code downloads
        // and executes a new binary; a MITM'd update is a real compromise, not
        // a cosmetic bug.
        constexpr const char* CA_BUNDLE = "romfs:/cacert.pem";

        std::string UserAgent()
        {
            return std::string("romm-eshop/") + romm::APP_VERSION;
        }

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

        int XferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
        {
            auto* cb = static_cast<const ProgressCallback*>(clientp);
            if (!cb || !*cb)
                return 0;
            bool keepGoing = (*cb)(dlnow, dltotal);
            return keepGoing ? 0 : 1;
        }

        bool ParseVersion(const std::string& v, int& major, int& minor, int& patch)
        {
            major = minor = patch = 0;
            return std::sscanf(v.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 1;
        }

        bool IsNewer(const std::string& latest, const std::string& current)
        {
            int lMaj, lMin, lPat, cMaj, cMin, cPat;
            if (!ParseVersion(latest, lMaj, lMin, lPat) || !ParseVersion(current, cMaj, cMin, cPat))
                return latest != current;
            if (lMaj != cMaj) return lMaj > cMaj;
            if (lMin != cMin) return lMin > cMin;
            return lPat > cPat;
        }
    }

    bool CheckForUpdate(UpdateInfo& out, bool& updateAvailable, std::string& errorOut)
    {
        updateAvailable = false;

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        std::string response;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
        headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");

        std::string userAgent = UserAgent();

        curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_URL);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

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
            errorOut = "GitHub returned HTTP " + std::to_string(httpCode);
            return false;
        }

        json_error_t jerr;
        json_t* root = json_loads(response.c_str(), 0, &jerr);
        if (!root)
        {
            errorOut = "Failed to parse GitHub response";
            return false;
        }

        json_t* tagNode = json_object_get(root, "tag_name");
        json_t* assetsNode = json_object_get(root, "assets");
        if (!json_is_string(tagNode) || !json_is_array(assetsNode))
        {
            errorOut = "Unexpected GitHub response shape";
            json_decref(root);
            return false;
        }

        std::string tag = json_string_value(tagNode);
        if (!tag.empty() && tag[0] == 'v')
            tag = tag.substr(1);

        std::string downloadUrl;
        int64_t assetSize = 0;
        size_t idx;
        json_t* asset;
        json_array_foreach(assetsNode, idx, asset)
        {
            json_t* nameNode = json_object_get(asset, "name");
            if (json_is_string(nameNode) && std::string(json_string_value(nameNode)) == "romm-eshop.nro")
            {
                json_t* urlNode = json_object_get(asset, "browser_download_url");
                json_t* sizeNode = json_object_get(asset, "size");
                if (json_is_string(urlNode)) downloadUrl = json_string_value(urlNode);
                if (json_is_integer(sizeNode)) assetSize = json_integer_value(sizeNode);
                break;
            }
        }

        json_decref(root);

        if (downloadUrl.empty())
        {
            errorOut = "Latest release has no romm-eshop.nro asset";
            return false;
        }

        out.version = tag;
        out.downloadUrl = downloadUrl;
        out.assetSize = assetSize;
        updateAvailable = IsNewer(tag, romm::APP_VERSION);
        return true;
    }

    bool DownloadUpdate(const UpdateInfo& info, const ProgressCallback& onProgress, std::string& errorOut)
    {
        mkdir("sdmc:/switch", 0777);
        mkdir("sdmc:/switch/romm-eshop", 0777);
        mkdir(STAGING_DIR, 0777);

        FILE* file = fopen(STAGING_PATH, "wb");
        if (!file)
        {
            errorOut = "Failed to open staging file for writing";
            return false;
        }

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            fclose(file);
            errorOut = "Failed to initialize HTTP client";
            return false;
        }

        std::string userAgent = UserAgent();

        curl_easy_setopt(curl, CURLOPT_URL, info.downloadUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, XferInfoCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &onProgress);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        fclose(file);

        if (res != CURLE_OK)
        {
            errorOut = std::string("Network error: ") + curl_easy_strerror(res);
            remove(STAGING_PATH);
            return false;
        }
        if (httpCode < 200 || httpCode >= 300)
        {
            errorOut = "Server returned HTTP " + std::to_string(httpCode);
            remove(STAGING_PATH);
            return false;
        }

        struct stat st;
        if (info.assetSize > 0 && (stat(STAGING_PATH, &st) != 0 || st.st_size != info.assetSize))
        {
            errorOut = "Downloaded update size mismatch -- possibly corrupted";
            remove(STAGING_PATH);
            return false;
        }

        // We're executing from RAM already -- the loader reads the whole NRO
        // into memory before running it -- so overwriting the on-disk file
        // while running is safe. Standard Switch homebrew self-update idiom.
        remove(INSTALL_PATH);
        if (rename(STAGING_PATH, INSTALL_PATH) != 0)
        {
            errorOut = "Failed to replace the installed copy";
            return false;
        }

        return true;
    }

    void PrepareRelaunch()
    {
        if (envHasNextLoad())
            envSetNextLoad(INSTALL_PATH, "");
    }
}
