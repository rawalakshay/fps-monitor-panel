/* Host tests for the home-screen shortcut's pure helpers (home_tile_plan.hpp)
 * and its web API helpers (web_config.hpp). Built and run by scripts/test.sh. */
#undef NDEBUG
#include "home_tile_plan.hpp"
#include "web_config.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace fpsmon;

int main() {
  /* param.json: the exact bytes the console reads */
  assert(tile::param_json(1994) ==
         "{\"applicationCategoryType\":66048,\"titleId\":\"FPSM00001\","
         "\"deeplinkUri\":\"http://127.0.0.1:1994/\","
         "\"localizedParameters\":{\"defaultLanguage\":\"en-US\","
         "\"en-US\":{\"titleName\":\"FPS-Monitor-Panel\"}}}");

  /* Sony's install return codes */
  static_assert(tile::install_ok(0));
  static_assert(tile::install_ok(static_cast<int>(0x80990002u))); // restored
  static_assert(!tile::install_ok(-1));
  static_assert(!tile::install_ok(static_cast<int>(0x80990015u)));

  /* startup with the marker present */
  using tile::Startup;
  static_assert(tile::startup_plan(false, false) == Startup::Remount);
  static_assert(tile::startup_plan(false, true) == Startup::None);
  static_assert(tile::startup_plan(true, false) == Startup::Register);
  static_assert(tile::startup_plan(true, true) == Startup::Register);

  /* POST /api/tile body, validated like /api/config */
  web::Updates u;
  assert(web::parse_updates("on=true", u, web::kTileKeys).empty());
  assert(u.size() == 1 && u[0].second == "true");
  assert(web::parse_updates("on=false", u, web::kTileKeys).empty());
  assert(u[0].second == "false");
  assert(web::parse_updates("", u, web::kTileKeys) == "empty body");
  assert(web::parse_updates("on", u, web::kTileKeys) == "malformed body");
  assert(web::parse_updates("on=1", u, web::kTileKeys) ==
         "invalid value for on");
  assert(web::parse_updates("on=true&on=false", u, web::kTileKeys) ==
         "invalid value for on");
  assert(web::parse_updates("enabled=true", u, web::kTileKeys) ==
         "unknown key enabled");
  assert(web::parse_updates("on=true", u) == "unknown key on"); // config keys

  /* GET /api/tile reply */
  assert(web::tile_json(true, false, "") ==
         "{\"on\":true,\"busy\":false,\"error\":\"\"}");
  assert(web::tile_json(false, true, "refused \"x\"") ==
         "{\"on\":false,\"busy\":true,\"error\":\"refused \\\"x\\\"\"}");

  std::puts("tile_test: ok");
  return 0;
}
