 /* Home-screen shortcut: pure helpers (standard library only, host-tested by
 * tests/host/tile_test.cpp). The console side is home_tile.cpp.
 */
#pragma once

#include <cstdint>
#include <string>

#define FPSMON_TILE_ID "FPSM00001"

namespace fpsmon::tile {

/* sce_sys/param.json. Category 66048 with a deeplinkUri: selecting the tile
 * opens the PS5 browser on that URI (ps5upload's web UI tile does the same). */
inline std::string param_json(int port) {
  return "{\"applicationCategoryType\":66048,\"titleId\":\"" FPSMON_TILE_ID
         "\",\"deeplinkUri\":\"http://127.0.0.1:" +
         std::to_string(port) +
         "/\",\"localizedParameters\":{\"defaultLanguage\":\"en-US\","
         "\"en-US\":{\"titleName\":\"FPS-Monitor-Panel\"}}}";
}

/* Sony's install return code: 0 installed, 0x80990002 already installed
 * ("restored"). Both are success. */
constexpr bool install_ok(int rc) {
  return rc == 0 || static_cast<uint32_t>(rc) == 0x80990002u;
}

/* At payload start with the marker present. The nullfs bind does not survive
 * a reboot, but Sony's registration does, so re-mounting is enough (no Sony
 * call). A changed file means the staged sce_sys copies are stale: register
 * again. A live bind means this boot already has it (payload re-sent). */
enum class Startup { None, Remount, Register };

constexpr Startup startup_plan(bool files_changed, bool mount_live) {
  if (files_changed)
    return Startup::Register;
  return mount_live ? Startup::None : Startup::Remount;
}

} // namespace fpsmon::tile
