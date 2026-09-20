# RomM eShop

A Nintendo Switch homebrew app (`.nro`) that presents your [RomM](https://github.com/rommapp/romm) server's
Switch library as an eShop-style storefront: browse cover art and descriptions pulled live from RomM, then
download and install a game directly on the console.

Built for personal use on your own jailbroken (CFW) Switch, against your own self-hosted RomM server and your
own game backups. It is not a piracy tool and ships no games or keys.

## How it works

- **Pairing**: uses RomM's built-in OAuth2-style device authorization flow (the same mechanism TV apps use) --
  the Switch shows a short code, you approve it from a browser already logged into RomM. Your RomM password is
  never typed on the Switch or stored anywhere.
- **Browsing**: fetches the Switch platform's ROM list, cover art, and descriptions from RomM's REST API
  (verified against a live RomM 5.2.0 server).
- **Download**: streams the XCI to the SD card with resume support (`Range` requests), showing live progress.
- **Install**: hands the downloaded file to a vendored copy of
  [Awoo Installer](https://github.com/Huntereb/Awoo-Installer)'s install engine (GPLv3), which does the actual
  NCM/ES content registration. This is the same, widely-used code real installers use -- romm-eshop does not
  reimplement NCA/ticket installation itself. See [`THIRD_PARTY.md`](THIRD_PARTY.md).

## Requirements

- A Switch running Atmosphère (or similar CFW) with the homebrew menu.
- A running RomM server with your Switch XCIs already in its library.
- devkitPro (`devkitA64` + `libnx` + `switch-portlibs`) to build.

## Building

```sh
# one-time toolchain setup (MSYS2 + devkitPro pacman repos), see devkitpro.org for details
pacman -S switch-dev switch-portlibs switch-curl switch-sdl2 switch-jansson

git clone --recursive <this repo>
cd romm-eshop
export DEVKITPRO=/opt/devkitpro   # adjust if needed
make
```

Produces `romm-eshop.nro`. Copy it to `/switch/romm-eshop/romm-eshop.nro` on your SD card and launch it from
the Homebrew Menu.

## First run

1. Launch the app. It'll prompt for your RomM server's address (e.g. `http://192.168.1.100:8080`) via the
   system keyboard.
2. It shows a pairing code and a URL. Open that URL on your phone or PC (while logged into RomM) and approve
   the pairing request.
3. The app fetches your Switch library and shows the storefront grid.
4. Select a game to see its cover, description, and size, then **Download & Install**.

Config (server URL + pairing token) is stored at `sdmc:/switch/romm-eshop/config.json`. Cover art is cached at
`sdmc:/switch/romm-eshop/cache/covers/`. Downloads land in `sdmc:/switch/romm-eshop/downloads/` and are deleted
automatically after a successful install.

## Known v1 limitations

- **XCI only, SD card install target.** RomM's Switch library in testing was 100% XCI; NSP install code is
  vendored and compiled in but not exercised by the UI. Games always install to the SD card, not system memory.
- **No progress bar widget.** Progress is shown as text (`Downloading... 1.2 GB / 2.4 GB (52%)`) rather than a
  graphical bar -- borealis (the UI framework) has no stock progress bar component; this is the fastest safe
  option rather than hand-rolling custom-drawn UI blind.
- **NCA signature verification fails closed, silently.** If a downloaded file fails Nintendo signature
  verification (almost always = corrupted/incomplete download), the vendored install engine normally asks "install
  anyway?" via a dialog. borealis has no built-in modal confirm dialog, so romm-eshop always answers "no" and
  aborts with an error instead of risking a cross-thread UI dialog that's never been tested on real hardware. If
  you hit this, redownload the file.
- **Store loading is serial.** On first load, cover art for all games downloads one at a time before the grid
  shows. It's cached after that, so subsequent launches are fast.
- **Not tested on real hardware.** This was built and compiled with the real devkitA64 toolchain and the RomM
  API calls were verified live against a real RomM 5.2.0 server, but no Switch was available to run the actual
  `.nro` end-to-end. Compile-time correctness is verified; runtime behavior on-console is not. Please treat the
  first run as a test and report back anything that misbehaves.

## Project layout

- `source/` -- app code (RomM API client, config, borealis UI activities).
- `source/install/` -- our thin wrapper around the vendored install engine + shared progress state.
- `source/vendor/awoo/` -- files copied unmodified from Awoo Installer's install engine (`install/`, `nx/`,
  `data/`, `util/`), plus a small shim (`shim/`) standing in for Awoo's own UI/config/i18n so the install engine
  didn't need to be modified. See [`THIRD_PARTY.md`](THIRD_PARTY.md) for exactly which files and why.
- `external/borealis/` -- the [borealis](https://github.com/natinusala/borealis) UI framework (submodule).

## License

GPLv3 (inherited from the vendored Awoo Installer install engine -- see [`THIRD_PARTY.md`](THIRD_PARTY.md)).
