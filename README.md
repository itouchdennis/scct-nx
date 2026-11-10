# scct-nx: Splinter Cell: Chaos Theory for the Nintendo Switch
An experimental, unofficial Nintendo Switch (homebrew, libnx) port of the original Xbox version of Splinter Cell: Chaos
Theory, as a static recompilation. **Early work: it does not run yet.**

It starts from [scda-nx](https://github.com/itouchdennis/scda-nx), the Switch port of Splinter Cell: Double Agent.
Both games use the same engine and the same Xbox SDK (XDK 1.0.5849). The recompiler, the Xbox kernel and XDK
reimplementation, the Vulkan renderer on mesa-switch's NVK and the Switch layer all come from there.
[PORTING_GUIDE.md](PORTING_GUIDE.md) describes what it took and the checklist this port follows.

> [!NOTE]
> **AI-written, human-tested.** The code this port adds is written by Claude, Anthropic's AI model, directed and tested
> on a real Nintendo Switch by a human. What it builds on is credited below.

> [!IMPORTANT]
> **You must own a legally acquired copy of the game.** This repository contains no game data, no assets and no code
> generated from the game: everything game specific is made at build time from the files of your own copy. There are
> no prebuilt NROs, and there will not be any, not even locked or encrypted ones: a built NRO holds the game's own code.
> Do not ask for game files, ISOs or prebuilt game code, and do not share them in issues or pull requests.

This project is not affiliated with or endorsed by Ubisoft, Microsoft or Nintendo. All trademarks belong to their
respective owners.

## Status
- [ ] recompiler configuration for the game's programs (`sc3.xbe`, the campaign; `default.xbe`, `sc3_adv.xbe` and the
      others as they turn out to be needed)
- [ ] boots on Linux with the null renderer
- [ ] boots on the Switch with the null renderer
- [ ] renders with Vulkan on NVK, sound, input
- [ ] menu to mission and back (relaunch)
- [ ] build scripts and a first release

## Credits
The recompilation approach, the Xbox kernel and XDK reimplementation and the renderer come from the
[SCDA-XBERecomp](https://github.com/TGP482/SCDA-XBERecomp) project by TGP482 and Joshhhuaaa, made with the
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool, under the MIT license (see [LICENSE](LICENSE)). The
Linux and Switch port is [scda-nx](https://github.com/itouchdennis/scda-nx).
