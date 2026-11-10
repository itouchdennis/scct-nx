# How to build scct-nx (Windows and Linux)
scct-nx is Splinter Cell: Chaos Theory (Xbox) for the Nintendo Switch. There is no ready-made download of the game for
the Switch, and there never will be, not even a locked or encrypted one: you make it yourself, on your own computer,
from **your own, legally acquired copy** of the Xbox game. The scripts below do that for you in a few clicks. Nothing
of the game is downloaded or uploaded.

> [!WARNING]
> **Early work.** The game reaches its title screen; the menus, the missions and the switch between them are not
> tested yet. The builder image the scripts download is published with the first release: until then, build it on
> your computer with `SCCT_LOCAL=1` (Linux, takes long: it builds the Switch's Vulkan driver), or build by hand as the
> [README](https://github.com/itouchdennis/scct-nx#readme) describes.

**What you need**
- Your copy of Splinter Cell: Chaos Theory for the Xbox, as a disc image (`.iso` or `.xiso`; a full disc dump works
  too) or as the extracted disc (the folder with `sc3.xbe`, `Default/` and `Media/`). Made from the Europe release
  (English, French, German, Spanish, Italian).
- A Nintendo Switch that runs homebrew (the Homebrew Menu).
- About 20 GB of free disk space and an internet connection (the first run downloads the builder, several GB).
- [Docker](https://www.docker.com/): the builder runs inside it, so nothing else gets installed on your computer.

The first build takes a while (from about 20 minutes to over an hour, depending on your PC). Later builds are much
quicker.

## Windows (10 or 11)
**Windows has not been tested yet.** `build-nro-windows.bat` comes from scda-nx, whose Windows builder has not been run
on a real Windows PC either. If you try it, please report how it went in an issue.

1. Install [Docker Desktop](https://docs.docker.com/desktop/setup/install/windows-install/) and start it once. Let it
   set up WSL 2 if it asks, and restart your PC if it asks.
2. Download `build-nro-windows.bat` and put it in a folder of its own, for example `C:\scct-nx\`.
3. Double-click `build-nro-windows.bat` and choose your game's `.iso` (or the `sc3.xbe` in your extracted disc). You
   can also drag the `.iso` or the folder onto the `.bat`.
   - If Windows warns about the file ("Windows protected your PC"): click **More info**, then **Run anyway**. It is a
     plain text file, so you can open it in Notepad and read what it does first.
4. Wait until it says **Your SD card folder**. A window opens with the folder `switch`.
5. Go on with [On the Switch](#on-the-switch).

## Linux
1. Install Docker ([how](https://docs.docker.com/engine/install/)) and allow your user to use it:
   ```sh
   sudo usermod -aG docker $USER
   ```
   Then log out and back in.
2. Download `build-nro-docker.sh`, make it runnable and start it with your game:
   ```sh
   chmod +x build-nro-docker.sh
   ./build-nro-docker.sh "/path/to/your Splinter Cell Chaos Theory.iso"
   ```
   The extracted disc's folder works as well as the `.iso`.
3. Wait until it says **Your SD card folder**: it is `out/switch` beside the script.
4. Go on with [On the Switch](#on-the-switch).

## On the Switch
1. Copy the folder `switch` (from `out\` beside the script) onto the root of your SD card. It holds
   `switch/scct/scct.nro` and the game files in `switch/scct/game/`.
2. Start the Homebrew Menu with **title takeover**: hold **R** while you start any game, not from the album. The
   album (applet mode) leaves too little memory.
3. Start **Splinter Cell: Chaos Theory** from the list.

On the first start the shaders are built while you play, so things can be missing or see-through for a moment. They
are saved on the SD card and load at once from then on.

scct-nx keeps everything in `sdmc:/switch/scct/`, apart from scda-nx (Double Agent) in `sdmc:/switch/scda/`: both
can be on the same card.

## Good to know
- **Please do not share the `.nro`.** It holds code made from your copy of the game, so it is for you only.
- **Read before you run.** Both scripts are plain text. If you are skeptical (that is a good thing), open them and read
  what they do first, or build by hand as the [README](https://github.com/itouchdennis/scct-nx#readme) describes.
- **An icon in the Homebrew Menu:** put a 256x256 JPEG named `icon.jpg` beside the script before building.
- **Updates:** run the script again. It fetches the newest builder and rebuilds only what changed. The NRO it
  replaces is kept in `out/BAK/`, named after its version (`scct_v0.1.0.nro`).
- **Disk space:** the builder and the build cache live in Docker. To remove them:
  `docker volume rm scct-nx-cache` and `docker image rm ghcr.io/itouchdennis/scct-nx-builder:latest`.
- **Logs for a bug report:** add `logging = true` to `sdmc:/switch/scct/scct.toml`; the log is written to
  `sdmc:/switch/scct/logs/`. `profiling = true` writes performance measurements instead (frame time, threads,
  where the time goes) without the full log.
- **Clocks:** the port runs Nintendo's official clocks; overclocking is up to tools such as sys-clk (see the README).
- **Game code optimization:** `-O3` by default. `SCCT_CODE_OPT=-O2` before the script builds it as before, for
  comparing; a change rebuilds the game code.
- **macOS** may work with Docker Desktop too, but it has not been tested (and on Apple Silicon it runs emulated and slowly).
