#pragma once

#include <string>

namespace romm::ui
{
    // Blocks (shows the system swkbd applet) until the user confirms or
    // cancels. Returns false if the user cancelled.
    bool ShowSoftwareKeyboard(const std::string& headerText, const std::string& initialText, std::string& outResult, size_t maxLen = 128);
}
