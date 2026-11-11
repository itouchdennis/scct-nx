# scct-nx: Splinter Cell: Chaos Theory for the Nintendo Switch
An experimental, unofficial Nintendo Switch (homebrew, libnx) port of the original Xbox version of Splinter Cell: Chaos
Theory, as a static recompilation. It renders through Vulkan on mesa-switch's NVK and runs on Linux as well, which
serves as its test bed.

It starts from [scda-nx](https://github.com/itouchdennis/scda-nx), the Switch port of Splinter Cell: Double Agent.
Both games use the same engine and the same Xbox SDK (XDK 1.0.5849). The recompiler, the Xbox kernel and XDK
reimplementation, the Vulkan renderer on NVK and the Switch layer all come from there.
[PORTING_GUIDE.md](PORTING_GUIDE.md) describes what it took and the checklist this port follows.

> [!NOTE]
> **AI-written, human-tested.** The code this port adds is written by Claude, Anthropic's AI model, directed and tested
> on a real Nintendo Switch by a human. What it builds on is credited below (see [Credits](#credits)).

> [!WARNING]
> **Early work.** On the Switch the game runs from the menus through the briefing into the first mission. Much is
> not tested yet. See [Status](#status).

> [!IMPORTANT]
> **You must own a legally acquired copy of the game.** This repository contains no game data, no assets and no code
> generated from the game: everything game specific is made at build time from the files of your own copy. This project
> does not support piracy in any form. Do not ask for game files, ISOs or prebuilt game code, and do not share them in
> issues or pull requests; such requests will be closed.

This project is not affiliated with or endorsed by Ubisoft, Microsoft or Nintendo. All trademarks belong to their
respective owners.

## Contents
- [Status](#status)
- [No prebuilt NROs](#no-prebuilt-nros)
- [How to build it: Windows and Linux in a few clicks](#how-to-build-it-windows-and-linux-in-a-few-clicks)
- [The easy way without Docker: one command (Arch Linux)](#the-easy-way-without-docker-one-command-arch-linux)
- [Building for the Nintendo Switch by hand (Arch Linux)](#building-for-the-nintendo-switch-by-hand-arch-linux)
- [Clocks and overclocking](#clocks-and-overclocking)
- [Credits](#credits)

## Status
- [x] recompiler configuration for `sc3.xbe`, the campaign that the disc's `default.xbe` (a launcher) starts
- [x] boots on Linux with Vulkan to the title screen
- [x] boots on the Switch
- [x] menus, the briefing and into a mission (relaunch)
- [x] the settings as an overlay (L + R + D-pad right)
- [ ] a whole mission, back to the menu
- [ ] sound, input, saves
- [ ] the builder image and a first release

Versus (`sc3_adv.xbe`) and the launcher are not part of it.

## No prebuilt NROs
There are no prebuilt `.nro` files, and there will not be any, not even locked or encrypted ones. A built NRO holds the
game's own code, recompiled from the XBE, so sharing one would mean sharing the game. Everyone builds their own from
their own copy, as below. Please do not upload or ask for built NROs.

## How to build it: Windows and Linux in a few clicks
The builders do everything inside [Docker](https://www.docker.com/): your game stays on your computer, and the NRO is
made there. [docs/HOW_TO_BUILD.md](docs/HOW_TO_BUILD.md) is the step-by-step guide.

> [!NOTE]
> The scripts download the builder image `ghcr.io/itouchdennis/scct-nx-builder`, published from `main`. If it cannot
> be downloaded, `SCCT_LOCAL=1 ./build-nro-docker.sh ...` builds it on your computer (Linux; it builds the Switch's
> Vulkan driver, which takes long), or build by hand as below.

- **Windows:** `build-nro-windows.bat`, double-click it and choose your `.iso` (untested on Windows so far).
- **Linux:** `./build-nro-docker.sh "/path/to/your game.iso"`.

Then copy `out/switch` onto the root of your SD card and start **Splinter Cell: Chaos Theory** from the Homebrew Menu
with title takeover (hold R while starting any game). Everything lives in `sdmc:/switch/scct/`, apart from
Double Agent's `sdmc:/switch/scda/`.

## The easy way without Docker: one command (Arch Linux)
`build-nro.sh` does every step below by itself: it installs what is missing (asking for your password), sets up
devkitPro, builds the Switch's Vulkan driver once, extracts your disc image, recompiles the game from it and leaves a
folder ready for the SD card. Nothing of the game is downloaded or uploaded; everything is made on your computer from
your own copy, and kept in `~/.cache/scct-nx` for the next run.

> [!CAUTION]
> **Only use this script if you are feeling lazy.** It runs `sudo`, edits `/etc/pacman.conf` and downloads and builds
> code from the internet. Never blindly trust a script from the internet, this one included. If you are even a little
> skeptical (and being skeptical is a good thing these days), read [build-nro.sh](build-nro.sh) first, or skip it
> and run the steps from [Building for the Nintendo Switch by hand](#building-for-the-nintendo-switch-by-hand-arch-linux)
> one by one, so you see and approve everything that happens.
```sh
git clone https://github.com/itouchdennis/scct-nx.git
cd scct-nx
./build-nro.sh "/path/to/your Splinter Cell Chaos Theory.iso"
```
The extracted disc's folder (with `sc3.xbe`) works as well as an `.iso`. The first run can take a while, depending on
your PC (the Vulkan driver and the whole game are compiled); later runs only rebuild what changed. When it is done,
copy `out/switch` onto the root of your SD card and start the game from the Homebrew Menu with title takeover (hold R
while starting any game). For an icon in the Homebrew Menu, put a 256x256 JPEG at `switch/icon.jpg` before building.

## Building for the Nintendo Switch by hand (Arch Linux)
What `build-nro.sh` does, step by step. Each block can be pasted into a terminal as it is. Expect the first build to
take a while: NVK, the recompiler and the whole game are compiled.

### 1. Host packages
Compilers and build tools, plus what Mesa's NVK needs to build (LLVM, libclc and the SPIR-V tools for its shader
compiler, Rust with the AArch64 target, bindgen and cbindgen):
```sh
sudo pacman -S --needed base-devel git cmake ninja meson python python-mako python-yaml python-packaging python-ply \
    flex bison clang llvm libclc spirv-llvm-translator spirv-tools glslang \
    rust rust-aarch64-gnu rust-bindgen cbindgen sdl2-compat rsync
```

### 2. devkitPro
devkitPro's pacman repositories, as [devkitPro describes them](https://devkitpro.org/wiki/devkitPro_pacman):
```sh
sudo pacman-key --recv BC26F752D25B92CE272E0F44F7FD5492264BB9D0 --keyserver keyserver.ubuntu.com
sudo pacman-key --lsign BC26F752D25B92CE272E0F44F7FD5492264BB9D0
sudo pacman -U https://pkg.devkitpro.org/devkitpro-keyring.pkg.tar.zst
grep -q '^\[dkp-libs\]' /etc/pacman.conf || printf '\n[dkp-libs]\nServer = https://pkg.devkitpro.org/packages\n\n[dkp-linux]\nServer = https://pkg.devkitpro.org/packages/linux/$arch/\n' | sudo tee -a /etc/pacman.conf >/dev/null
sudo pacman -Syu
```
Then the Switch toolchain and the libraries the port and NVK link:
```sh
sudo pacman -S --needed switch-dev switch-sdl2 switch-libexpat switch-zlib switch-libzstd
source /etc/profile.d/devkit-env.sh
```

### 3. Sources
This repository and mesa-switch (NVK for the Switch), side by side. The port was tested with mesa-switch at commit
`1a8c1a66d6f`:
```sh
mkdir -p ~/switch && cd ~/switch
git clone https://github.com/itouchdennis/scct-nx.git
git clone https://github.com/danfromtico/mesa-switch.git
git -C mesa-switch checkout 1a8c1a66d6f
```

### 4. NVK
Builds mesa-switch's Vulkan driver once, into `scct-nx/build/nvk-sdk`, where the NRO build finds it:
```sh
cd ~/switch/scct-nx
MESA=~/switch/mesa-switch switch/nvk/build-nvk.sh
```

### 5. Your game files
Copy the **extracted** disc of your own copy of the game into `assets/`, so that `assets/sc3.xbe` sits beside
`Default/` and `Media/` (an ISO is not enough here: `extract-xiso -x your.iso -d assets` extracts it). This should
list the file:
```sh
ls ~/switch/scct-nx/assets/sc3.xbe
```

### 6. The NRO
Recompiles the game from `assets/sc3.xbe` on the first run (`tools/regen.sh`), then builds and stages the NRO:
```sh
cd ~/switch/scct-nx
switch/build.sh
```
The result is `switch_sd/switch/scct/scct.nro`. To give it an icon in hbmenu, put a 256x256 JPEG at
`switch/icon.jpg` before building; git ignores it, so it stays out of the repository.

### 7. On the Switch
Copy `scct.nro` to `sdmc:/switch/scct/` and the extracted disc to `sdmc:/switch/scct/game/`, then start it from
hbmenu with title takeover (hold R while starting a game); applet mode leaves too little memory.

On the first start the shaders are built while you play: objects can be missing or see-through for a moment. They
are kept on the SD card (`shaders.vkb`) and load at once from then on.

Logging is off by default. To write the full log (and `boot.txt`) to `sdmc:/switch/scct/logs/`, for a bug report
for example, add `logging = true` to `sdmc:/switch/scct/scct.toml`. A crash still writes `crash.txt` either way.

### Settings
The port's settings are in the game's own options menu, as in Double Agent: **Options > Recomp Settings** (display,
graphics, general) and **Options > Controller** (the sticks, vibration and the buttons). A lists a row's values,
Left and Right step them. **L + R + D-pad right** (F10 on a keyboard) opens the same settings as an overlay anywhere.
They are saved in `sdmc:/switch/scct/scct.toml`.

### On Linux
The same sources build a Linux executable, the quick test bed: `cmake -S . -B build/linux-vk -G Ninja
-DCMAKE_BUILD_TYPE=Release -DSCDA_GPU=vk && ninja -C build/linux-vk`, with the extracted disc in `assets/`. It
recompiles `sc3.xbe` itself. Settings are in `scct.toml` beside the executable; `pause_on_focus_loss = false` keeps
it running when its window has no focus.

## Clocks and overclocking
The port runs the Switch at Nintendo's official clocks for games (CPU 1020 MHz, GPU 384 MHz handheld or 768 MHz
docked, memory 1331 or 1600 MHz) and sets them again after docking, the HOME menu and sleep. Overclocking is left to
your own tools, such as [sys-clk](https://github.com/retronx-team/sys-clk) or Horizon OC. For Double Agent these
handheld values were tested and recommended: CPU 1428, GPU 460, memory 1600 MHz.

## Credits
The recompilation approach, the Xbox kernel and XDK reimplementation and the renderer come from the
[SCDA-XBERecomp](https://github.com/TGP482/SCDA-XBERecomp) project by TGP482 and Joshhhuaaa, made with the
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool, under the MIT license (see [LICENSE](LICENSE)). The
Linux and Switch port is [scda-nx](https://github.com/itouchdennis/scda-nx). Please report Chaos Theory's issues here,
not upstream.
