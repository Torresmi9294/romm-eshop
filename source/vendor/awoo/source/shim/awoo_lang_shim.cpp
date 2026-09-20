#include "util/lang.hpp"

#include <unordered_map>

// romm-eshop shim: resolves the small, fixed set of "key"_lang literals the
// vendored install engine uses. Text matches Awoo Installer's original
// English strings (romfs/lang/en.json) for the keys we actually reference.
std::string RommEshopLangLookup(const std::string& key)
{
    static const std::unordered_map<std::string, std::string> table = {
        {"common.cancel", "Cancel"},
        {"inst.info_page.top_info0", "Installing "},
        {"inst.nca_verify.title", "Invalid NCA signature detected!"},
        {"inst.nca_verify.desc",
         "This game's content did not pass Nintendo signature verification. "
         "This usually means the download is corrupted or incomplete.\n\n"
         "romm-eshop aborts installation in this case to avoid registering "
         "bad content on your console."},
        {"inst.nca_verify.opt1", "Yes, I understand the risks"},
        {"inst.nca_verify.error", "The requested NCA is not properly signed: "},
    };

    auto it = table.find(key);
    if (it != table.end())
        return it->second;
    return key;
}
