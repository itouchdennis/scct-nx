# Changelog
There are no prebuilt NROs in any release: each release holds the source and the build scripts. You build the NRO
yourself, from your own copy of the game ([docs/HOW_TO_BUILD.md](docs/HOW_TO_BUILD.md)).

scct-nx starts from [scda-nx](https://github.com/itouchdennis/scda-nx) v0.4.9; its changelog has the history of the
shared code.

## v0.1.1
The first release (v0.1.0 was never released). On the Switch the game runs from the menus through the briefing into
the first mission.
- `sc3.xbe`, the campaign, recompiled: the XDK functions and data the host replaces or reads found by their code in
  Double Agent's, which uses the same XDK.
- Boots on Linux with Vulkan to the title screen.
- Kernel: a set auto-reset event goes to the thread waiting for it, as on the Xbox; Chaos Theory's loading screen
  otherwise kept the level from ever loading.
- Its own folder on the Switch, `sdmc:/switch/scct/` (`scct.nro`, `scct.toml`), beside Double Agent's.
- The builders (Docker for Windows and Linux, `build-nro.sh` for Arch) and the guides for Chaos Theory.
- Files: the SD card's open files no longer run out: the handles of one file share one stream (the autosave after
  the briefing crashed).
- The settings panel as an overlay: L + R + D-pad right on a controller (F10 on a keyboard) opens it, B closes it.
  Display, graphics, general and controller settings as Double Agent's, the frame rate counter, and the controller's
  buttons rebound by the Xbox controls; what needs Double Agent's game patches (field of view, gyro look, stick
  sensitivity) waits for Chaos Theory's own.
