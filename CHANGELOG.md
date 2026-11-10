# Changelog
There are no prebuilt NROs in any release: each release holds the source and the build scripts. You build the NRO
yourself, from your own copy of the game ([docs/HOW_TO_BUILD.md](docs/HOW_TO_BUILD.md)).

scct-nx starts from [scda-nx](https://github.com/itouchdennis/scda-nx) v0.4.9; its changelog has the history of the
shared code.

## v0.1.0
Not released yet.
- `sc3.xbe`, the campaign, recompiled: the XDK functions and data the host replaces or reads found by their code in
  Double Agent's, which uses the same XDK.
- Boots on Linux with Vulkan to the title screen.
- Kernel: a set auto-reset event goes to the thread waiting for it, as on the Xbox; Chaos Theory's loading screen
  otherwise kept the level from ever loading.
- Its own folder on the Switch, `sdmc:/switch/scct/` (`scct.nro`, `scct.toml`), beside Double Agent's.
- The builders (Docker for Windows and Linux, `build-nro.sh` for Arch) and the guides for Chaos Theory.
