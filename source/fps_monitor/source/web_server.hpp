/* Settings web app: HTTP server on port 1994 inside fps-monitor.elf.
 * GET / serves the page; GET/POST /api/config read and edit the [overlay]
 * keys of /data/fps-monitor/config.ini; GET/POST /api/tile read and switch
 * the home-screen shortcut (home_tile.cpp).
 */
#pragma once

inline constexpr int kWebServerPort = 1994;

/* Start the server on its own detached thread. */
void web_server_start();
