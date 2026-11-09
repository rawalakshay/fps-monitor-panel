# fps-monitor-panel

An in-game performance overlay for the PS5, delivered as a single homebrew
payload: frame rate, frame times and CPU / GPU / memory load drawn on top of
any running game.

## Features

- **Four detail levels**
  - `minimal`: a small FPS pill.
  - `compact`: one line with FPS and system load.
  - `panel` (default): a corner panel with a large FPS number, AVG / LOW, a
    10 s FPS chart and a CPU / GPU / memory row.
  - `full`: currently the same as `panel` (reserved).
- **FPS, AVG and LOW** for the current game session. A new game, or the same
  game relaunched, starts a fresh session.
- **Charts**: a 10 s FPS chart in the panel and an optional 7.5 s frame-time
  graph along the bottom edge of the screen.
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
| `opacity` | `0`, `40`, `65`, `85` | `65` | Background opacity in percent; `0` hides the background. |
| `margin` | `0`, `5`, `10`, `20` | `10` | Gap from the screen edges, in pixels. |
| `system` | `true`, `false` | `true` | CPU / GPU / memory row (`compact`, `panel`, `full`). |
| `frame_graph` | `true`, `false` | `false` | 7.5 s frame-time graph along the bottom edge (every level). |
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
frame_graph=true
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
