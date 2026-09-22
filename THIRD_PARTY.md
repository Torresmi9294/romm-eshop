# Third-party code

## Awoo Installer install engine (GPLv3)

`source/vendor/awoo/` contains files copied, unmodified, from
[Huntereb/Awoo-Installer](https://github.com/Huntereb/Awoo-Installer) at commit
`28364422c35efbf0c0ddff196458a6f49b8dff44` (2025-11-15), licensed GPLv3. Some of those files carry their own
original MIT copyright header (Copyright (c) 2017-2018 Adubbz, from the original Tinfoil codebase Awoo
Installer forked); those headers are preserved as-is.

Copied unmodified:
- `include/install/*`, `source/install/{install,install_nsp,install_xci,nsp,xci,sdmc_nsp,sdmc_xci,simple_filesystem}.cpp`
- `include/nx/*`, `source/nx/{content_meta,fs,nca_writer,ncm}.cpp`, `source/nx/ipc/{es,ns_ext}.c`
- `include/data/*`, `source/data/{buffered_placeholder_writer,byte_buffer,byte_stream}.cpp`
- `include/util/{error.hpp,debug.h,crypto.hpp,title_util.hpp,file_util.hpp,network_util.hpp}`,
  `source/util/{crypto,title_util,file_util}.cpp`, `source/util/debug.c`

`source/vendor/awoo/source/shim/` and the corresponding `include/ui/*`, `include/util/{config,lang,util}.hpp`
are **not** from Awoo Installer -- they're a small compatibility layer written for this project, standing in
for Awoo's own Plutonium-based UI, settings, and i18n systems (which we don't use) so the install engine files
above could be vendored byte-for-byte instead of forked and modified. See the comments at the top of each shim
file for exactly what it does and why.

Because this vendored code is GPLv3, romm-eshop as a whole is licensed GPLv3.

## borealis (Apache 2.0)

`external/borealis/` is vendored (plain files, not a git submodule -- private repo, simplest to keep everything
self-contained) from [natinusala/borealis](https://github.com/natinusala/borealis), pinned at commit
`20e2d33b6c4ffce139ce304c503c04f5b94da920`, licensed Apache 2.0. Small compatibility/tuning patches applied on
top:
- `library/lib/extern/nanovg-deko3d/include/nanovg/dk_renderer.hpp`: added missing `#include <optional>` (build
  fix for the current devkitA64/libnx toolchain, not an upstream bug in general use).
- `library/lib/platforms/switch/swkbd.cpp`: removed a call to `swkbdConfigSetStringLenMaxExt`, which no longer
  exists in current libnx (superseded by `swkbdConfigSetStringLenMax`, already called on the line above it;
  same category as the fix above).
- `library/lib/platforms/switch/switch_video.cpp`: bumped `IMAGES_POOL_SIZE` from upstream's 4 MiB to 32 MiB.
  This is a real behavioral change, not a build fix: upstream's default is sized for a typical icon-heavy UI,
  not a game-cover grid. Loading dozens of cover art textures (RomM's small cover is ~180KB as a decoded GPU
  texture; the large one is over 1MB) blew straight through 4 MiB and crashed below the C++ exception layer
  (deko3d's memory pool allocator aborts on exhaustion rather than failing gracefully) -- this is what caused
  the "blank screen" bugs during testing, not application-level bugs.

`romfs/shaders/*.dksh` are borealis's own nanovg-deko3d shaders, precompiled from
`external/borealis/library/lib/extern/nanovg-deko3d/shaders/*.glsl` via devkitPro's `uam` compiler and checked
in as binary assets (nanovg-deko3d ships shader source, not prebuilt `.dksh`, and expects the consuming
project's build to compile them -- checking in the compiled output avoids a fragile Makefile rule for 3 files
that never change).

## RomM API

The RomM REST API shape (`source/api/romm_client.cpp`) was verified directly against a live RomM 5.2.0
server's `/openapi.json`, not guessed from documentation. Endpoint paths or response fields may need small
adjustments on future RomM versions.
