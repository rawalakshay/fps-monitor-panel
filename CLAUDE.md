# CLAUDE.md

## What this is

`fps-monitor-panel` is a standalone PS5 homebrew payload (ELF) that draws an
in-game performance overlay inside SceShellUI: FPS, AVG/LOW, FPS and frame-time
charts, and a CPU/GPU/memory row. The overlay design comes from onionHEN 0.9.0.
GPL-3.0. Upstream credits live only in the README Credits section.

Not a desktop/web app — it runs on a PS5 via the console's ELF loader. There is
no test suite to run locally; verification is a successful cross-compile.

## Build

Requires Linux or WSL (build from WSL on Windows) with the
[PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) plus `cmake`, `ninja`,
`python3`. The SDK must be a full release (needs `target/lib/libc++.a`).

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
./scripts/build.sh            # Release; flags: --debug, --release, --jobs N, --build-dir DIR
```

`build.sh` invokes `prospero-cmake` on `source/` with Ninja, then builds the
`fps_monitor` target. CMake entry point is `source/CMakeLists.txt`; project-wide
CMake variables use the `FPSMON_` prefix (`FPSMON_ROOT`, `FPSMON_OUT_BIN`, ...).

**One deliverable:** `build/bin/fps-monitor.elf`. The ShellUI overlay module
`monitor_shellui.elf` is built first and embedded inside it
(`source/fps_monitor/source/embedded.c`).

## Module layout (and link order)

`source/CMakeLists.txt` adds the subdirectories in dependency order — keep it:
`libhijacker` → `libonion_proc` → `libonion_fps` → `libonion_platform` →
`libonion_ipc` → `libonion_settings` → `libonion_detour` → `libonion_ready` →
`libonion_elfldr` → `libNineS` → `fps_monitor`. `libonion_proc` must precede
`libNineS` (inject path links its symbols).

| Path | What |
| --- | --- |
| `source/fps_monitor/source/` | Host process: FPS sampler threads + ShellUI injector/watchdog (`main.cpp`) |
| `source/fps_monitor/shellui/` | Overlay module entry (`main.cpp`) + `monitor_overlay.cpp` (sampling, scene render loop) |
| `source/shellui/` | Mono helpers, `overlay_scene.hpp` (pure layout), `overlay_scene_renderer.cpp` |
| `source/daemon/` | `daemon_fps.cpp` FPS sampler (DCE scanout, AGC counters, PS4 Gnm detour) |
| `source/libonion_*`, `libhijacker`, `libNineS` | Vendored support libraries |
| `source/platform/ps5/stubs/` | PS5 system-library link stubs |
| `source/include/` | First-party shared headers |
| `source/i18n/` | Notification strings, compiled into `libonion_platform` |
| `third_party/` | cJSON, sha1 |

`libhijacker`/`libNineS`/`libonion_*` are vendored support libraries. Prefer
not to edit them; the owner has allowed edits there for project renames and
for changes that genuinely belong in those libraries.

## i18n

Per-locale JSON in `source/i18n/` (e.g. `en-US.json`); `generate_catalog.py`
compiles them into a catalog built into `libonion_platform`. See
`source/i18n/README.md`. Regenerate after editing strings.

## Runtime notes (for context, not local work)

- Only one monitor instance runs; a second copy exits with "already running".
  It cannot detect other performance overlays.
- The host re-injects the overlay into SceShellUI once per second (survives
  rest-mode resume / ShellUI restarts). Log: `/data/fps-monitor/fps-monitor.log`.
- Config is optional: `config.ini.example` → `/data/fps-monitor/config.ini`
  (ShellUI sees it as `/user/data/fps-monitor/config.ini`), `[overlay]` keys.
  Read at module load; needs `[meta] schema_version=1`. The monitor never writes it.
- Runtime markers live under `/system_tmp/fps-monitor`.

## Conventions

- `.sh` files are LF (enforced by `.gitattributes`); `build/` and `*.elf` are gitignored.
- GPL-3.0. Keep `THIRD_PARTY_NOTICES.md` in sync when adding dependencies.
- Keep upstream copyright headers (e.g. `OnionHEN / LightningMods`) unchanged.
