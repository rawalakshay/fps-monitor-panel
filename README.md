# fps-monitor-panel

An in-game performance overlay for the PS5, delivered as a single homebrew
payload: frame rate, frame times and CPU / GPU / memory load drawn on top of
any running game.

![fps-monitor panel and frame-time graph over a Tekken 8 fight at a locked 60 FPS](docs/screenshots/tekken8-fight.jpg)

## Screenshots

Captured on a PS5 with the default settings.

The image above is a Tekken 8 fight at a locked 60: flat frame times, 95 % on
target, session stats after 1:59.

| Tekken 8 main menu (PS5) | Red Dead Redemption 2 (PS4) | Tekken 8 cutscene (PS5) |
| --- | --- | --- |
| ![Panel at a 60 FPS target in the Tekken 8 menu; two short drops show as amber bars and ticks](docs/screenshots/tekken8-menu.jpg) | ![Panel on a PS4 title at its 30 FPS target; slow stretches show amber in the FPS chart and the stability bar](docs/screenshots/rdr2-ps4.jpg) | ![Cutscene with frame-time spikes up to 185 ms in the bottom graph, values printed in red](docs/screenshots/tekken8-cutscene.jpg) |
| Short drops show as amber bars with ticks under them; STABILITY stays at 90 % on target. | PS4 games are measured too. The PS4 pill and TARGET 30 are detected automatically. | Stalls past the graph's scale print their length in red (MAX 184.7 ms). |

## The overlay at a glance

Layout illustrations of each part of the overlay. The numbers are examples.

| Panel, on target | Panel, during a drop |
| --- | --- |
| <img src="docs/ui/ui-panel.png" alt="Panel layout at a locked 60 FPS: header with PS5 and TARGET 60 pills, 60 FPS with AVG and LOW over the last 10 s, FRAME time, session row, FPS and frame-time charts, stability bar, CPU, GPU and memory row, and the github.com/rawalakshay footer" width="340"> | <img src="docs/ui/ui-panel-drop.png" alt="The same panel during a drop: 44 FPS in red, a red HITCH 240 ms badge, red and amber bars, and the stability bar at 85 % on target" width="340"> |

**Frame-time graph** (`frame_graph=true`, the default): 7.5 s of frame times centred along the bottom of the screen.

<img src="docs/ui/ui-graph.png" alt="Frame-time graph: FRAME TIME, LAST 7.5 S, MAX 240.0 ms, a row of blue bars with one 240 ms stall labelled in red" width="100%">

**Compact strip** (`detail=compact`): the same numbers in one row along the top edge.

<img src="docs/ui/ui-strip.png" alt="Compact strip: 60 FPS, FRAME 16.8 ms, AVG 59.9, LOW 59.0, MAX 16.9 ms, a mini frame-time chart, CPU 21 % 72° and GPU 57° 97 W" width="100%">

**Themes** (`theme=`): classic, mono, aurora and vice.

<img src="docs/ui/ui-themes.png" alt="Four theme swatches: classic in blue, mono in grey and white, aurora in cyan with a left edge bar, vice in pink with a pink and orange top bar" width="100%">

## Features

- **Four detail levels**
  - `minimal`: a small FPS pill.
  - `compact`: one strip with FPS, frame time, AVG, LOW, MAX frame time, a
    mini frame-time chart and CPU / GPU temperatures.
  - `panel` (default): a corner panel with a large FPS number, AVG / LOW over
    the last 10 s, the current frame time, a session row, a 10 s FPS chart, a
    frame-time chart, a stability bar and a CPU / GPU / memory row.
  - `full`: currently the same as `panel` (reserved).
- **FPS, AVG and LOW** for the current game session. A new game, or the same
  game relaunched, starts a fresh session.
- **Window labels**: every number says what it covers (FPS over 0.5 s, AVG /
  LOW over the last 10 s, chart bars of 0.25 s).
- **Session row**: session time, session AVG and LOW, the share of time below
  target and the number of drops.
- **Hitch badge**: a red `HITCH` value when a frame in the last second takes
  longer than 100 ms.
- **Platform pill**: `PS4` or `PS5` next to the detected FPS target.
- **Charts**: a 10 s FPS chart with drop ticks under the bars that miss the
  target, a frame-time chart in the panel, and a stability bar that splits the
  last 10 s into on target, slow and drop.
- **Frame-time graph**: a 7.5 s graph centred along the bottom edge of the
  screen (the top edge when the overlay sits at the bottom), on by default.
  Frames past the chart's scale print their value in red.
- **Themes**: `classic` (default), `mono`, `aurora` and `vice` colour palettes.
- **System row**: CPU usage and temperature, GPU temperature and power draw,
  and memory in use.
- **Placement**: top or bottom edge, left / center / right, adjustable margin,
  font size and background opacity.
- **Survives rest mode**: the overlay is re-injected into the system UI after
  rest-mode resume or a system UI restart.

### How the frame rate is measured

The payload never loads a module into the game.

- **PS5 games**: it reads the display scanout counters (`/dev/dce`) and the
  frame counters of the game's already-loaded graphics driver
  (`libSceAgcDriver.sprx`) from outside the game process.
- **PS4 games** (backward compatibility): it places a small remote hook on the
  game's GNM submit-and-flip call to count presented frames.

The overlay itself is drawn by a module injected into SceShellUI (the system
UI), not into the game.

## Requirements

- A jailbroken PS5 that can run payloads via an ELF loader.
- A way to send an ELF to that loader over the network (for example `nc` or
  any payload sender).

## Install and run

1. Download `fps-monitor.elf` from the releases page, or build it (see
   [Building from source](#building-from-source)).
2. Send `fps-monitor.elf` to the console's ELF loader. A
   `[FPS Monitor] started` notification confirms it is running.
3. Start a game. The panel appears in the top-left corner.

Notes:

- The payload keeps running until the console is turned off. After rest mode
  it re-injects the overlay on its own; after a full reboot, send it again.
- Only one instance runs. Sending it a second time shows
  `[FPS Monitor] already running` and the new copy exits.
- **Do not run it alongside another performance overlay** (for example the
  game monitor bar of a full homebrew toolkit). It cannot detect other
  overlays, and both would draw on screen at the same time.

## Configuration

Configuration is optional; without a config file every key uses its default.
The payload never creates or rewrites the file.

1. Copy [`config.ini.example`](config.ini.example) to
   `/data/fps-monitor/config.ini` on the console (the folder is created on the
   first run, next to the log).
2. Keep the `[meta]` section: a file without `schema_version=1` is ignored.
3. Edit the `[overlay]` keys below.

The overlay reads the file when it is loaded into the system UI, so changes
take effect after you reboot the console and send the payload again. Invalid
values fall back to the default.

| Key | Allowed values | Default | Effect |
| --- | --- | --- | --- |
| `detail` | `minimal`, `compact`, `panel`, `full` | `panel` | How much the overlay shows (see [Features](#features)). |
| `edge` | `top`, `bottom` | `top` | Screen edge the overlay is anchored to. |
| `align` | `left`, `center`, `right` | `left` | Position along that edge. |
| `font_size` | `small`, `medium`, `large` | `medium` | Overlay text size. |
| `opacity` | `0`, `40`, `65`, `85` | `85` | Background opacity in percent; `0` hides the background. |
| `margin` | `0`, `5`, `10`, `20` | `20` | Gap from the screen edges, in pixels. |
| `system` | `true`, `false` | `true` | CPU / GPU / memory row (`panel`, `full`); CPU and GPU cells in `compact`. |
| `frame_graph` | `true`, `false` | `true` | 7.5 s frame-time graph along the bottom edge (every level). |
| `frame_graph_opacity` | `0`, `40`, `65`, `85` | `85` | Background opacity of the frame-time graph, in percent. |
| `theme` | `classic`, `mono`, `aurora`, `vice` | `classic` | Colour palette. `aurora` adds a cyan edge bar, `vice` a pink and orange top bar. |
| `fps_pill` | `true`, `false` | `false` | Also show the FPS pill in the opposite corner (`compact`, `panel`, `full`). |
| `brand` | `true`, `false` | `false` | Show the "4aLse Benchmarks" header on the panel. |

Example:

```ini
[meta]
schema_version=1

[overlay]
detail=compact
edge=bottom
align=right
opacity=40
frame_graph=false
theme=aurora
```

The overlay is always on while the payload runs, so `enabled=false` has no
effect. If `opacity` is not set, `background=false` also hides the background.
Other `[overlay]` keys are ignored.

## Troubleshooting

The log is written to `/data/fps-monitor/fps-monitor.log`.

| Notification | Meaning |
| --- | --- |
| `[FPS Monitor] started` | The payload is running and will load the overlay. |
| `[FPS Monitor] already running` | An instance is already running; the new copy exited. |
| `[FPS Monitor] overlay failed to load` | Injecting the overlay into the system UI failed. The payload tries 3 times, then waits until the system UI restarts (reboot, or rest mode and back). Check the log. |

- **The panel says "NO GAME DETECTED"**: no frames are being measured. It
  appears in menus and loading screens and clears once the game renders.
- **Settings are ignored**: check the path (`/data/fps-monitor/config.ini`),
  that `[meta]` has `schema_version=1`, and that the values match the table
  above. Reboot and send the payload again after editing.
- **Two overlays on screen**: another performance overlay is running. Use only
  one.

## Building from source

Requirements:

- Linux, or WSL on Windows.
- The [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk), a full
  release that includes the C++ runtime (`target/lib/libc++.a`).
- `cmake`, `ninja` and `python3`.

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
./scripts/build.sh
```

| Flag | Effect |
| --- | --- |
| `--release` | Release build (default). |
| `--debug` | Debug build (defines `DEBUG`). |
| `--jobs N` | Parallel build jobs (default: CPU count). |
| `--build-dir DIR` | Build directory (default: `./build`). |

Output: `build/bin/fps-monitor.elf`. The system UI module
(`monitor_shellui.elf`) is built first and embedded inside it, so this is the
only file to send.

On Windows, build from a WSL shell. `.gitattributes` keeps `*.sh` files LF.

## Project layout

| Path | Contents |
| --- | --- |
| `source/fps_monitor/source/` | Host payload: FPS sampler threads and the system UI injector / watchdog. |
| `source/fps_monitor/shellui/` | Overlay module entry point and `monitor_overlay.cpp` (sampling, render loop). |
| `source/shellui/` | Mono helpers, scene layout (`overlay_scene.hpp`) and renderer. |
| `source/daemon/` | FPS sampler (`daemon_fps.cpp`): scanout, AGC counters, PS4 GNM hook. |
| `source/libonion_*`, `source/libhijacker`, `source/libNineS` | Vendored support libraries: process access, injection, settings, logging, notifications. |
| `source/i18n/` | Notification strings, compiled into `libonion_platform`. |
| `source/platform/ps5/stubs/` | PS5 system library link stubs. |
| `third_party/` | cJSON, SHA-1. |
| `scripts/build.sh` | Build script. |
| `docs/screenshots/` | README screenshots. |
| `docs/ui/` | README layout illustrations. |

## Credits

- Based on **HyperHEN** by **LightningMods**. The overlay, FPS sampler and
  support libraries come from it.
- Overlay design from **onionHEN 0.9.0** by **LightningMods**.
- FPS sampling approach follows **PHU Games Tools** by
  [ArkSama](https://github.com/ArkSama).
- ELF loading and ptrace helpers by **John Törnblom** (GPL-3.0-or-later).
- **cJSON** by Dave Gamble and cJSON contributors (MIT).
- **SHA-1 in C** by Steve Reid (public domain).
- **inih** by Ben Hoyt (BSD-3-Clause).
- **Hacker Disassembler Engine 64** by Vyacheslav Patkov.

Full notices: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## License

GPL-3.0. See [`LICENSE`](LICENSE). Third-party components keep their own
terms, listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
