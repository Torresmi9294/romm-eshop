#include "ui/software_keyboard.hpp"

#include <switch.h>
#include <vector>

namespace romm::ui
{
    bool ShowSoftwareKeyboard(const std::string& headerText, const std::string& initialText, std::string& outResult, size_t maxLen)
    {
        SwkbdConfig config;
        Result rc = swkbdCreate(&config, 0);
        if (R_FAILED(rc))
            return false;

        swkbdConfigMakePresetDefault(&config);
        swkbdConfigSetHeaderText(&config, headerText.c_str());
        swkbdConfigSetInitialText(&config, initialText.c_str());
        swkbdConfigSetStringLenMax(&config, static_cast<u32>(maxLen));

        std::vector<char> buffer(maxLen + 1, 0);
        rc = swkbdShow(&config, buffer.data(), buffer.size());
        swkbdClose(&config);

        if (R_FAILED(rc))
            return false;

        outResult = std::string(buffer.data());
        return true;
    }
}
