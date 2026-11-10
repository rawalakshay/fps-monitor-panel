/* Home-screen tile icon (source/fps_monitor/assets/tile_icon0.png, 512x512),
 * written by home_tile.cpp. Path comes from CMake, like web_page.c.
 */
#ifndef FPS_MONITOR_TILE_ICON
#define FPS_MONITOR_TILE_ICON "../assets/tile_icon0.png"
#endif

__asm__(
    ".global fps_monitor_tile_icon_start\n"
    ".type   fps_monitor_tile_icon_start, @object\n"
    ".global fps_monitor_tile_icon_end\n"
    ".type   fps_monitor_tile_icon_end, @object\n"
    ".align  16\n"
    "fps_monitor_tile_icon_start:\n"
    ".incbin \"" FPS_MONITOR_TILE_ICON "\"\n"
    "fps_monitor_tile_icon_end:\n");
