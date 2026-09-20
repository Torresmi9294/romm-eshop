#include "util/util.hpp"

#include <switch.h>
// tin_ipc.h wraps es.h/ns_ext.h in extern "C" for C++ callers -- es.c/ns_ext.c
// (vendored, plain C) export unmangled names, so this must go through the
// wrapper rather than including es.h/ns_ext.h directly.
#include "nx/ipc/tin_ipc.h"

namespace inst::util
{
    void initInstallServices()
    {
        ncmInitialize();
        nsextInitialize();
        esInitialize();
        splCryptoInitialize();
        splInitialize();
    }

    void deinitInstallServices()
    {
        ncmExit();
        nsextExit();
        esExit();
        splCryptoExit();
        splExit();
    }

    void playAudio(std::string /*path*/)
    {
        // romm-eshop doesn't ship Awoo's bark.wav/awoo.wav sound effects.
        // Intentional no-op; kept so the vendored code's std::thread(playAudio, ...) still links.
    }
}
