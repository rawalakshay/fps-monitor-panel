/* Opt-in PS5 home-screen shortcut (title FPSM00001) that opens the settings
 * web app in the PS5 browser. See home_tile.cpp.
 */
#pragma once

#include <ctime>
#include <pthread.h>
#include <string>

/* Held around every ucred authid swap in this process (the overlay inject's
 * and the tile uninstall's): two overlapping save/restore pairs could leave
 * the process on the wrong authid. */
extern pthread_mutex_t g_authid_mutex;

/* Call once before the web server starts. Remembers how long after boot Sony
 * calls must wait (`boot_settle_sec` of uptime) and, when the user turned the
 * shortcut on earlier, restores it in the background. */
void home_tile_start(time_t boot_settle_sec);

/* Ask for the shortcut on or off. Returns at once; one worker thread applies
 * the latest request. */
void home_tile_request(bool on);

struct HomeTileState {
  bool on;           // the marker is there: the user wants the shortcut
  bool busy;         // the worker is still applying a request
  std::string error; // why the last request failed, "" when it did not
};
HomeTileState home_tile_state();
