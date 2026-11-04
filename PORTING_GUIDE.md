# Porting an Xbox recomp game to the Nintendo Switch

This is what it took to bring Splinter Cell: Double Agent (SCDA-XBERecomp) to the Switch. It is written to make the
next port faster. Each item comes in the order it matters in a new port, with the failure you get without it and the
fix. The source files named here are in this repository.

Projects worth reading next to this one: `sa2-nx` (memory mapping), `burnout3-sw` (thread priorities), `nfsu2-sw`
(stack sizes), `nfsmw-nx` (NVK with a pipeline cache, no stutter) and `mesa-switch` (NVK for Horizon).

> This repository contains no game data and no code generated from it. Everything game specific comes from your own
> copy of the game at build time (`tools/regen.sh`).

---

## 0. Starting point and layout

A recomp game has four layers. Only the bottom two depend on the platform:

| Layer | Contents | Platform |
|---|---|---|
| `generated/` | the XBE's x86 code as C++ (recompiler `tools/recomp`), made from your own copy, never committed | shared |
| `src/kernel`, `src/xdk`, `src/audio`, `src/gpu` | Xbox kernel, D3D8, DirectSound, mixer | shared |
| `src/port/*.cpp` | window, input, files, threads, memory, audio output | **per platform** |
| renderer | `gpu_vk.cpp` (Vulkan), `gpu_gl.cpp`, D3D12 on Windows | **per API** |

Rule: everything platform specific goes into `src/port/`, behind `port/port.h`. Then the same port runs on Linux
**and** the Switch, and Linux is the quick test bed.

**Order that worked:**
1. Linux build with the null renderer (`SCDA_GPU=null`): does the game boot, do kernel, files and threads work?
2. Switch build with the null renderer: memory, threads, file system and logs on real hardware.
3. Renderer: **Vulkan on NVK right away**, not GL first (section 5).
4. Audio, input, relaunch (menu → mission → menu), performance.

---

## 1. Build setup

- **A cross build cannot run the recompiler.** `tools/regen.sh` generates the C++ on the PC (`generated/game`),
  `switch/build.sh` only compiles it for AArch64.
- `cmake/portable.cmake` covers Linux and the Switch:
  - **`-fsigned-char` on AArch64**: ARM's `char` is unsigned, x86's signed, and recompiled code and XDK
    reimplementations rely on the latter.
  - Renderer by `SCDA_GPU=vk|gl|null`, sources **filtered per backend**: a glob that pulls `vk_device.cpp` into the
    GL build fails to link.
  - Link-time wraps for libnx and SDL (section 3): `--wrap=SDL_SYS_SetThreadPriority`, `--wrap=threadCreate`,
    `--wrap=threadClose`, `--wrap=condvarWaitTimeout`.
- `switch/build.sh` builds **and stages** the NRO: `build/switch-vk/scda.nro` → `switch_sd/switch/scda/scda.nro`.
  A bare `ninja` does not update `switch_sd/`. Vulkan is the default; `SCDA_GPU=gl` builds the GL renderer.
- SD card layout (`sdmc:/switch/scda/`): `scda.nro`, `game/` (your extracted disc), `scda.toml`, `logs/`, `hdd/`
  (the Xbox hard disk: E:, T:, U:, Z:), `shaders.vkb`.
- **Always start with title takeover** (hold R while starting a game): applet mode leaves too little memory.

---

## 2. Memory: the guest's 4 GB (`switch_memory.cpp`)

The Xbox game expects a 32 bit address space of its own; guest address = `base + VA`.

- Reserve the address space in **libnx's ASLR region**, commit it in 2 MB granules.
- **No `svcCreateCodeMemory` per block**: each code-memory object keeps its page list, and after a few dozen the
  kernel runs out (`0xCE01`). Instead, as sa2-nx does: turn a heap block into module code memory with
  `svcMapProcessCodeMemory`, make it read-write, and map that alias into the window with `svcMapProcessMemory`.
- The **physical windows** (on the Xbox `0x80000000` and `0xF0000000` are the same pages) need the same alias;
  `svcMapProcessMemory` refuses heap memory as its source (`0xD401`).
- A decommitted range is zeroed in place, as a fresh commit reads.
- Check the log for `memory: syscall hints …` and `physical windows … alias`.

---

## 3. Threads, priorities, cores

| Problem | Fix |
|---|---|
| `std::thread::detach` throws "Function not implemented" | provide newlib's `pthread_detach` hook, which libnx leaves out |
| every thread lands on core 0 | every thread prefers core 0 and may run on cores 0 to 2 (core 3 is the system's and does not time-slice the default priority); the thread that presents takes core 2 for its own and the render thread core 1, both at 0x2B (`port::TakeCore`). Dealt round robin, the two often shared a core while another stood idle |
| the process may hold ~96 kernel threads, ended detached threads keep theirs | close ended detached threads yourself (`threadCreate`/`threadClose` wrapped) |
| SDL's audio "TIME_CRITICAL" becomes 0x3B on the Switch (time-sliced), audio crackles | wrap `SDL_SYS_SetThreadPriority`: TIME_CRITICAL → **0x2B** (as burnout3-sw) |
| guest threads need big stacks | 16 MB per guest thread (as nfsu2-sw), `SCDA_STACK_MB` overrides it |
| clocks | set them one by one through `clkrst` (the FastLoad boost drops the GPU to 76.8 MHz): Nintendo's official ones (CPU 1020, GPU 384 handheld / 768 docked, memory 1331 / 1600 MHz) with a CPU setting up to 1785. The system puts its own back on docking, after the HOME menu and sleep: set them again on a mode change and from an applet hook (`AppletHookType_OnFocusState`, `OnResume`, ...), never by watching the clocks, so an overclocking tool (sys-clk, Horizon OC) keeps its own. The game's own code limits the frame rate, so the CPU clock is what buys frames |
| waiting wakes a core for nothing | sleep in one go and let a wait that is not alertable wake once a second at most (APCs need the short slices only when alertable); in SCDA 2 ms sleep slices and 50 ms wait wake-ups woke the cores ~1,400 times a second |

---

## 4. File system

Each of these cost a crash or an empty file on the Switch's SD card:

- `std::filesystem` queries, and even `stat`/`fstat`, fail with **EIO** at times: get sizes in steps, `fstat` →
  `lseek` to the end → probing.
- **A file open for writing cannot be opened again**: ask its size through the open handle, or `profile.sav` reads
  as 0 bytes.
- New file sizes through `ftruncate`; log short reads and writes.
- Open read-only where only reading.
- **`fsync` the log on every flush**, or it is empty after a hang.
- **Seek only when the stream stands elsewhere.** An `fseek` empties newlib's buffer: one before every `NtReadFile`
  and `NtWriteFile` sent each of the title's writes to the card on its own. Seek again after a write when reading,
  after a size query, and after a truncation; flush the other open streams of a file before opening it again, and
  every open file in `HalReturnToFirmware` (the process ends without flushing).

---

## 5. Renderer: Vulkan on NVK from the start

**Lesson from SCDA: GL on switch-mesa 20.1 is a dead end.** It keeps neither program binaries nor a disk cache, so
every start links every shader again and the game stutters at each new material. Program binaries, a shader list
replayed at start and linker threads all failed to fix it. nfsmw-nx runs smoothly on NVK with a pipeline cache, and
the same worked here.

### Building NVK (once)
- `switch/nvk/build-nvk.sh` builds mesa-switch's NVK on Linux without Docker. It needs devkitA64, meson, ninja,
  clang/llvm/libclc/spirv-llvm-translator, rust with `rust-aarch64-gnu`, bindgen, cbindgen, and
  `pacman -S switch-libexpat switch-zlib switch-libzstd` from devkitPro.
- `MESA=/path/to/mesa-switch switch/nvk/build-nvk.sh` installs to `build/nvk-sdk/opt/devkitpro/portlibs/switch`,
  where `switch/build.sh` looks for `lib/libvulkan.a` (or set `SCDA_NVK_SDK`).

### Linking it in
- **Static, no loader**: only `vkGetInstanceProcAddr` is linked, everything else is loaded through it
  (`gpu::vk::fn`). `find_package(Vulkan)` does not work: its config wants libelf, which devkitPro has not.
- Link list: `libvulkan.a expat zstd z nx` (nx twice) `stdc++ m`.
- **Stubs** (`switch_stubs.c`): libSDL2.a pulls in EGL and NVK pulls in libelf (its CUBIN parser, never run on the
  Switch); both are served by stubs that fail cleanly.
- **No SDL window** (`SCDA_NO_WINDOW`): the surface comes from `nwindowGetDefault` + `nwindowSetDimensions` +
  `vkCreateViSurfaceNN`. SDL only does input and audio.

### Backend design that worked
- **Vulkan 1.3 + `VK_EXT_shader_object`**: all state dynamic, no pipeline explosion over D3D8 state combinations;
  push descriptors.
- Images stay in the GENERAL layout with coarse barriers: simple, and fast enough on NVK.
- Negative viewport height for D3D conventions.
- **Persistent shader cache** (`shaders.vkb` on the card), keyed by HLSL hash + entry point, with the driver's
  identity in its header: after a Mesa update it is rebuilt by itself.
- **Async shaders**: 2 builder threads on the Switch (3 on a PC) with 8 MB stacks; a draw whose shader is not built
  yet is skipped. The first time this shows as briefly missing or see-through objects instead of stutter; from the
  second start on they come from the cache ("200 shaders kept … made in 9 ms").
- A binary the driver refuses goes back to "kept" and its key is dropped, so its HLSL is generated again.
- A ring buffer in 4 segments, submits with serial/fence (`kSubmits=8`), `kFramesQueued=2`.
- Vertex uploads **before** the render pass begins.
- NOOVERWRITE locks get the newest finished copy from a ring of mapped read-back buffers: a synchronous read-back
  each frame held the Switch at 8 fps.
- Shared parts in `gpu_common.cpp` (constants, shader keys, present pass), so GL and Vulkan do not drift apart.
- **NOOVERWRITE, done right:** a surface locked in the last ~1800 presents (small ones) gets a copy every present,
  and `DeliverLate` asks the copy's fence itself (each slot keeps its command buffer's `VkFence`). Copies made only
  at a lock, and seen finished only at the next present, left every lock waiting while a level starts (25 ms each).
- **Render scale** (`app::RenderScale`): every target but the shadow map at the scale; what reads or writes guest
  memory (read-backs, NOOVERWRITE copies, CPU uploads) goes through a scratch image at the console's size, and
  CopyRects blits with each target's own scale. **Set the scale at `Init`**, before any target: applied at the first
  present it made every target new and black, and the loading screen after a relaunch (a picture the CPU wrote once)
  flickered black. A later change rescales each target with its picture blitted over. The GPU idled at 640x480 on the
  Switch, so 960x720 cost little; SMAA and anisotropic filtering cost more.
- **The hand-over to the render thread** compared ~6 KB of state per draw (a tenth of the game's thread): compare 16
  bytes at a time (NEON on ARM, SSE on x86), and mark writes instead of comparing where the writers are few. The
  vertex shader constants mark the registers set (`MarkVsc`), the transforms count their writes; two check builds
  that compared all the same and counted the changes a mark missed (0 in millions) came before anything went
  uncompared.
- **No heap calls per draw**: the vertex input descriptions were two vectors a draw, 100,000 heap calls a second on
  newlib's one lock, which the game's thread waits on too. Fixed arrays of 16.
- NVK has no memory type that is host cached **and** coherent: asking for one first is no shortage; say nothing.

### Traps found with GL that hold for any API
- Vertex uploads must reach the **highest vertex a draw reads**, zeros past it: Mesa reads beyond and crashes.
- Keep D3D's front face winding as it is, or objects show their insides.
- `invariant` positions (`gl_Position invariant`, or SPIR-V's) for EQUAL depth passes, or the lighting flickers.

### Testing on a PC
- `build/linux-vk` on RADV is the quick test bed for the same backend code.
- `SCDA_VK_VALIDATE=1` turns on the validation layers (where installed).
- `SCDA_SHOT_EVERY=150` takes screenshots, which can be checked without looking at a screen.

---

## 6. Audio

The mix (DirectSound emulation, ADPCM, HRTF, reverb) is the same on every platform. On the Switch only the output
matters (`sdl_audio.cpp`):

- 5.1 → stereo fold scaled by `1/(1+0.707)`, then a **limiter** with 2 ms look-ahead instead of a hard clamp.
- An own pump thread renders 10 ms buffers (`audio::kBlockFrames`) into `SDL_QueueAudio` at TIME_CRITICAL (→ 0x2B
  by the wrap), **a block every 10 ms by the clock**, a few blocks ahead. Rendering as fast as the queue emptied gave
  bursts of four blocks each time SDL's Switch driver took its 2,048 frames (43 ms).
- **The trap that mattered: SDL's Switch driver.** devkitPro's `SDL_switchaudio.c` plays only **two** wave buffers
  and notices one finishing only every 5 ms (`audrenWaitFrame`). At `samples = 512` that left ~5 ms to refill; when
  that was missed the gap sounded like **hard clipping** on steady bass (ambience, menu sounds). The queue in front
  of SDL does not help, and nothing shows in the log. **Fix: `want.samples = 2048` on the Switch**, ~37 ms of
  slack. That ended the crackling.
- Finding it faster than we did:
  - "output starved" or "stream ran dry" in the log means the queue or the game; nothing there and still crackling
    means the driver.
  - `SCDA_AUDIO_DUMP=<file>` captures the mix before the limiter on a PC (raw interleaved float32). Check peak, RMS
    and sample jumps with numpy. Here the mix was clean (peak ~0.06 in the menu), so the problem was behind it.
  - Every 10 s the output logs "audio: the mix peaked at …" when the limiter acted; seen often, the mix is too hot.
- Should 2048 not be enough: bypass SDL and play through libnx's `audout` with more buffers.
- **Music through a short looping buffer.** SCDA streams its music into a 48 ms looping stereo buffer it refills from
  the write cursor `GetCurrentPosition` gives (speech and ambience are streams of packets and were fine). Two traps
  made the music sound doubled, a little behind itself, and clicking:
  1. the write cursor stood 256 frames past the play cursor, inside the 480 the mixer reads at a time, so each block
     ended with the data of one loop before: put it past a whole mixer block at the voice's rate (`WriteLead`);
  2. the bursts above ran the play cursor 40 ms through a buffer the game can write 36 ms of ahead: pace the mixer by
     the clock.
  Music only, in the menu and in game, and speech clean: think of looping buffers first.
- **Measure the mix, do not guess it.** `audio_dump = true` in `scda.toml` writes the fold to `logs/audio.f32` (raw
  float, 48 kHz, interleaved). With numpy: the autocorrelation shows a delayed copy as a narrow peak at its delay,
  and clicks (sample steps far above the local mean step) taken modulo the mixer block (480) and the looping
  buffer's length show where they come from. The clicks went from ~90 to 2 in 20 s, and the 6 s period of the rest
  pointed at something outside the mixer.
- With `logging = true`, `dsound: playing` lines list every stream and stereo buffer once a second with its level,
  mix bins and position, and stream starts list the streams already playing. Stereo voices alternate their slots
  between the two channels; mapping speaker bins by side instead made the menu's music worse, so that stays.

---

## 7. Relaunch (menu ↔ mission)

Xbox games restart between menu and mission (`HalReturnToFirmware`). On the Switch hbloader starts the next NRO
through `envSetNextLoad` **in the same process, at the same address, on the same heap**: whatever the old run
leaves behind, the new one finds. This was the hardest part of the port:

1. **Pause** every other thread (Horizon cannot end another thread).
2. Give back their **stack mirrors**, with exactly the size `threadCreate` mapped (stack + reent + TLS, as libnx's
   `threadClose` computes it). `Thread::stack_sz` alone is refused by `svcUnmapMemory`, and every relaunch kept
   40 × 16 MB.
3. Release the guest mappings.
4. **Close the nvdrv and audren sessions yourself**, not with `nvExit`/`audrenExit`, whose cleanup waits forever;
   also **libnx's private clone of the nvdrv session** (`g_nvSrvClone`, its address read from `nvIoctl`'s adrp/add),
   the transfer memory handles and the sockets. Then wait (up to 3 s) for the services to let go of the heap, and
   mark the GPU's buffers cached again (otherwise ~600 MB stay uncached per mission).
5. **Wake every condvar** a paused thread still waits on (`condvarWaitTimeout` wrapped). The kernel keeps waits by
   address, and the next run puts its objects at the same addresses: its `notify_one` woke the old render thread
   instead, and the mission hung on its loading screen.
6. Leave without exit handlers a paused thread could hold up.
7. In the new run: the heap is the **largest plain read/write stretch** of what hbloader hands over; pages still
   uncached are marked cached first.
8. Launch data travels in `hdd/launch.bin`, the last image through `PersistDisplay`.

Diagnosis: `logs/boot.txt` keeps the whole relaunch chain, each step of the leave unbuffered, the blocked heap by
kind, and an "alive" every 10 s.

---

## 8. Input and the rest

- **No SDL text input**: it opens the system keyboard.
- 720p by default (native in handheld mode). Changing between 720p and 1080p: `nwindowSetDimensions` works only while
  the window holds no buffers, so destroy the swap chain first; compare against the size asked for, not the one
  made, or the swap chain is rebuilt every frame and the picture flickers.
- **Sticks:** Xbox games cut a dead zone off each stick axis (0.3 here), made for sticks that reach both ends on a
  diagonal. The Switch's round sticks give 0.71 on each at 45° and the camera turned slowly in between: give the
  right stick a round dead zone, stretch the circle to the square, and lift each axis past the game's dead zone.
- **Gyro:** libnx's six-axis sensors (`hidGetSixAxisSensorStates`), Nintendo's axes (x right, y forward, z up) to
  SDL's as (x, z, −y); left and right came out turned the wrong way, so an "invert gyro X" defaults on. A calibration
  that waits for the controller to lie still must read the sensors in the menu too and give up after a few seconds,
  or the gyro stays dead.
- The A/B and X/Y swap for the Switch's layout, and settings that do nothing on the Switch hidden.
- Errors before the game starts go to the **error applet**, so the player sees why nothing starts.

---

## 9. Diagnostics (build them early, they paid off every time)

- **Crash handler**: host and guest state into the log, addresses relative to the NRO's start → `addr2line -e
  scda.elf`; `crash.txt` and `boot.txt` in `logs/`.
- **Watchdog**: each guest thread's last indirect call and a stack excerpt every 15 s.
- Debug builds trace the first 3000 kernel calls.
- **Frame time breakdown** every 10 s: the title's work against its waits for the GPU, read-backs and flushes, plus
  the renderer's ms per phase. It tells at once whether the CPU or the GPU is the limit.
- `nxlink -s`: stderr and the log go to the PC live.

---

## 10. Performance and battery: measure first (`profiling = true`)

`profiling = true` in `scda.toml` (apart from `logging`) writes a lean `logs/scda.log` of measurements alone (full
logging writes every file opened and slows the very thread measured) and `logs/profile.txt`
(`switch_profile.cpp`). Every 10 s:

- **Threads and cores**: each named thread's share of a core and its preferred core (`svcGetInfo`
  `InfoType_ThreadTickCount` on its handle); sums per core over 100% mean threads compete.
- **A sampling profiler** on the three busiest threads, ~500 times a second: `svcSetThreadActivity` pauses the
  thread, `svcGetThreadContext3` reads it, it runs on. Offsets from the module base (`svcQueryMemory` on a function
  of your own; `__start__` reads 0) resolve against the build's `scda.elf` with `nm`/`addr2line`, so keep the ELF of
  every build you measure. **A thread blocked in a system call reports the `svc` itself as its PC**: for such samples
  the return addresses on its stack (bl/blr targets, within its stack region) show who called, up to the emulated
  kernel's functions.
- **Files**: opens, misses, queries, reads and writes by Xbox path, the busiest first. In SCDA they showed that file
  IO costs at loading and saving (language files opened hundreds of times, a save written in 4 KB pieces), not in
  play (~150 KB/s of streamed ambience).
- **Heap**: `malloc`, `calloc`, `realloc` and `memalign` wrapped at link time, every call counted and one in 32
  sampled with its stack, in a fixed table (the counting may not allocate); and `mallinfo` against libnx's heap
  (`fake_heap_start`/`end`). The system's own figure counts the whole heap as used from the start. SCDA used ~800 MB
  of a 3.2 GB heap in a level.
- **Waiting**: Sleep(0) yields, polls, wake-ups of waits, sleep slices, stalls and the frame cap's spin.
- **Battery**: `psmGetRawBatteryChargePercentage` to a thousandth of a percent, as % an hour over the window and the
  run; a few minutes unplugged compare two builds (the Switch gives no current).

**The method that worked:** one fixed spot (a quicksave in a busy place), stand still 60 s, compare the 10 s frame
time lines between builds; change one thing at a time; and for anything that skips work (comparing state, say),
first a check build that does it all the same and counts what the shortcut would miss.

**What it found in SCDA** (handheld, 1785 MHz): 42.7 → 40.2 ms a frame. The game's thread is the limit, ~65% of it
recompiled code with a flat profile; `-O3` for the game code gave 3.4% (`SCDA_CODE_OPT`, `-O2` to compare; LTO is
built but not measured). The GPU waited 0%. Battery: waking the cores less changed nothing where the game cannot hold
its frame rate; the clocks, the screen and the GPU drain it (~25% an hour).

---

## 11. Checklist for the next game

- [ ] `tools/regen.sh` with the new XBE and a recompiler config; the Linux build with `SCDA_GPU=null` boots
- [ ] paths, NRO name, title ID and the relaunch target per XBE in `switch_platform.cpp`
- [ ] Switch build with `null`: memory mapping, threads, files and logs work on hardware
- [ ] take over the Vulkan backend (`gpu_vk.cpp`, `vk_device.*`, `gpu_common.*`); first on a PC with RADV, then NVK
- [ ] name the shader cache file per game; async shaders on
- [ ] audio: `samples = 2048` on the Switch, the priority wrap in place; check a dump on a PC
- [ ] relaunch menu → mission → menu at least twice in a row (the leaks show from the third run on)
- [ ] saves: write → restart the game → load (the "second open" trap)
- [ ] game specific patches (skipped intros, HUD, split screen) go into `src/game/`, not the port
- [ ] music: listen in the menu and in play for doubling or clicks (looping buffers: write cursor, mixer pacing);
      check with `audio_dump`
- [ ] `profiling = true` at a fixed spot early: which thread limits, on which core, what it waits for
- [ ] clocks set again after the HOME menu and sleep; the right stick's dead zone round
- [ ] commit messages in Conventional Commits form, reverts too (`fix(scope): back to ...`, not git's "Revert ..."):
      the CI checks every commit pushed

**What saves time next time:** skip GL and start with NVK, take the relaunch logic from here as it is, and set the
audio buffer size, the write cursor and the mixer's pacing right from the start. Those were the longest detours
here. For performance, build the profiler before guessing: in SCDA the first guess (thread placement) gave little,
and the measured ones (state hand-over, heap calls, `-O3`) gave the rest.
