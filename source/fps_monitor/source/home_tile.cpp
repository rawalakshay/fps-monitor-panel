/* Opt-in PS5 home-screen shortcut: a registered app, FPSM00001, whose
 * sce_sys/param.json deeplinkUri opens the PS5 browser on the settings web
 * app (port 1994). Its folder is /data/fps-monitor/tile/FPSM00001/
 * (sce_sys/param.json, sce_sys/icon0.png, a placeholder eboot.bin).
 *
 * The registration is ported from ps5upload by phantomptr (GPL-3.0),
 * payload/src/register.c (register_title_from_path, mount_title_nullfs,
 * unregister_title, register_module_init_impl) and payload/src/webui_tile.c
 * (the tile's files and its request worker), cut down to this one tile.
 *
 * shortcut: no app.db reads or writes (ps5upload checks, hides and sweeps the
 * tile's app.db rows): a tile Sony refuses to uninstall stays on the home
 * screen until the user deletes it there; port that sweep if users report it.
 */
#include "home_tile.hpp"

#include "home_tile_plan.hpp"
#include "web_server.hpp"

#include <onion/log.h>

#include <ps5/kernel.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

extern "C" {
extern const uint8_t fps_monitor_tile_icon_start[];
extern const uint8_t fps_monitor_tile_icon_end[];
}

pthread_mutex_t g_authid_mutex = PTHREAD_MUTEX_INITIALIZER;

namespace {

#define TILE_PARENT "/data/fps-monitor/tile"
#define TILE_DIR TILE_PARENT "/" FPSMON_TILE_ID
#define TILE_USER_APP "/user/app/" FPSMON_TILE_ID
#define TILE_APPMETA "/user/appmeta/" FPSMON_TILE_ID

/* Written after an add, deleted after a remove: the user wants the tile.
 * Outside the tile folder, which the nullfs exposes to Sony. */
constexpr const char *kMarker = TILE_PARENT "/registered";
constexpr const char *kSystemEx = "/system_ex/app/" FPSMON_TILE_ID;
/* ps5upload's tile tracker: mount.lnk under another name, because
 * ShadowMount+ releases the bind of any /user/app/<id>/mount.lnk within
 * seconds (measured by ps5upload on FW 13.60). */
constexpr const char *kTracker = TILE_USER_APP "/fps-monitor.lnk";
constexpr const char *kAppInstUtil =
    "/system/common/lib/libSceAppInstUtil.sprx";
/* ps5upload SONY_API_POST_SLEEP_US: lets Sony's stub release its locks. */
constexpr useconds_t kSonyPostSleepUs = 200 * 1000;
constexpr uint64_t kShellCoreAuthid = 0x3800000000000010ULL;
/* register_title_from_path refuses a folder without one; nothing runs it,
 * the deeplinkUri opens the browser. */
constexpr char kEboot[] =
    "fps-monitor-panel home-screen tile: opens the settings web app\n";

/* ---- Sony's installer, resolved at first use (register_module_init_impl).
 * Not linked: ps5upload saw link-time imports of these sprx hang payload
 * start on some firmware. */
using InstallTitleDirFn = int (*)(const char *, const char *, void *);
/* FW 13.x no longer exports InstallTitleDir; InstallAll registers every app
 * staged under /user/app that the console does not know yet. */
using InstallAllFn = int (*)(void *);
/* Spelled "UnInstall" (capital I). ps5upload once called the non-existent
 * "sceAppInstUtilAppUnInstallTitle": dlsym returned NULL and uninstall
 * silently did nothing. (title_id, NULL, NULL). */
using UninstallFn = int (*)(const char *, int *, int *);
using InitFn = int (*)();

struct Sony {
  InstallTitleDirFn install_title_dir;
  InstallAllFn install_all;
  UninstallFn uninstall;
  InitFn init;
};
Sony g_sony{};
pthread_once_t g_sony_once = PTHREAD_ONCE_INIT;

void *sony_sym(const char *name) {
  void *p = dlsym(RTLD_DEFAULT, name);
  if (!p)
    LOG_DEBUG("fps-monitor: home tile %s not exported on this firmware", name);
  return p;
}

void sony_init() {
  dlerror();
  if (!dlopen(kAppInstUtil, RTLD_LAZY)) { // kept loaded for the process
    const char *e = dlerror();
    LOG_WARN("fps-monitor: home tile dlopen %s failed: %s", kAppInstUtil,
             e ? e : "unknown");
  }
  g_sony.install_title_dir = reinterpret_cast<InstallTitleDirFn>(
      sony_sym("sceAppInstUtilAppInstallTitleDir"));
  g_sony.install_all =
      reinterpret_cast<InstallAllFn>(sony_sym("sceAppInstUtilAppInstallAll"));
  g_sony.uninstall =
      reinterpret_cast<UninstallFn>(sony_sym("sceAppInstUtilAppUnInstall"));
  g_sony.init = reinterpret_cast<InitFn>(sony_sym("sceAppInstUtilInitialize"));
  LOG_INFO("fps-monitor: home tile APIs: InstallTitleDir=%d InstallAll=%d "
           "UnInstall=%d Initialize=%d",
           g_sony.install_title_dir != nullptr, g_sony.install_all != nullptr,
           g_sony.uninstall != nullptr, g_sony.init != nullptr);
}

std::string fmt(const char *f, unsigned v) {
  char buf[160];
  snprintf(buf, sizeof(buf), f, v);
  return buf;
}

/* ---- Files ---- */

bool is_dir(const char *p) {
  struct stat st {};
  return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

bool mkdir_p(const char *path, mode_t mode) {
  char buf[512];
  const size_t n = strlen(path);
  if (n == 0 || n + 1 > sizeof(buf))
    return false;
  memcpy(buf, path, n + 1);
  for (size_t i = 1; i <= n; ++i) {
    if (buf[i] != '/' && buf[i] != '\0')
      continue;
    buf[i] = '\0';
    if (mkdir(buf, mode) != 0 && errno != EEXIST) {
      LOG_ERROR("fps-monitor: home tile mkdir %s failed errno=%d", buf, errno);
      return false;
    }
    buf[i] = i < n ? '/' : '\0';
  }
  return true;
}

/* `path` holds exactly these bytes. */
bool same_bytes(const char *path, const uint8_t *data, size_t len) {
  uint8_t buf[4096];
  struct stat st {};
  size_t at = 0;
  const int fd = open(path, O_RDONLY);
  if (fd < 0)
    return false;
  bool same = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
              static_cast<size_t>(st.st_size) == len;
  while (same && at < len) {
    const ssize_t r = read(fd, buf, len - at < sizeof(buf) ? len - at : sizeof(buf));
    if (r < 0 && errno == EINTR)
      continue;
    same = r > 0 && memcmp(buf, data + at, static_cast<size_t>(r)) == 0;
    at += r > 0 ? static_cast<size_t>(r) : 0;
  }
  close(fd);
  return same;
}

/* `<path>.tmp` in the same directory, fsync, rename over `path`. */
bool write_atomic(const char *path, const uint8_t *data, size_t len) {
  char tmp[600];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  const int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    LOG_ERROR("fps-monitor: home tile open %s failed errno=%d", tmp, errno);
    return false;
  }
  size_t done = 0;
  while (done < len) {
    const ssize_t w = write(fd, data + done, len - done);
    if (w < 0 && errno == EINTR)
      continue;
    if (w <= 0)
      break;
    done += static_cast<size_t>(w);
  }
  bool ok = done == len && fsync(fd) == 0;
  ok = close(fd) == 0 && ok;
  if (ok && rename(tmp, path) == 0)
    return true;
  LOG_ERROR("fps-monitor: home tile write %s failed errno=%d", path, errno);
  unlink(tmp);
  return false;
}

struct TileFile {
  const char *dir;
  const char *name;
  int blob; // 0 param.json, 1 icon0.png, 2 eboot.bin
};

/* The tile folder, and the copies register_title_from_path stages for Sony:
 * sce_sys into /user/app/<id>/sce_sys, icon0.png next to it, the param and
 * images into /user/appmeta/<id>. Written from memory, not copied. */
constexpr TileFile kTileFiles[] = {
    {TILE_DIR "/sce_sys", "param.json", 0},
    {TILE_DIR "/sce_sys", "icon0.png", 1},
    {TILE_DIR, "eboot.bin", 2},
};
constexpr TileFile kStagedFiles[] = {
    {TILE_USER_APP "/sce_sys", "param.json", 0},
    {TILE_USER_APP "/sce_sys", "icon0.png", 1},
    {TILE_USER_APP, "icon0.png", 1},
    {TILE_APPMETA, "param.json", 0},
    {TILE_APPMETA, "icon0.png", 1},
};

/* Writes each file whose bytes differ. Files rewritten, or -1 (logged). */
int sync_files(std::span<const TileFile> files) {
  const std::string param = fpsmon::tile::param_json(kWebServerPort);
  const struct {
    const uint8_t *data;
    size_t len;
  } blobs[] = {
      {reinterpret_cast<const uint8_t *>(param.data()), param.size()},
      {fps_monitor_tile_icon_start,
       static_cast<size_t>(fps_monitor_tile_icon_end -
                           fps_monitor_tile_icon_start)},
      {reinterpret_cast<const uint8_t *>(kEboot), sizeof(kEboot) - 1},
  };
  int changed = 0;
  for (const TileFile &f : files) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", f.dir, f.name);
    const auto &b = blobs[f.blob];
    if (same_bytes(path, b.data, b.len))
      continue;
    if (!mkdir_p(f.dir, 0755) || !write_atomic(path, b.data, b.len))
      return -1;
    ++changed;
  }
  return changed;
}

/* rm -r, bounded depth (the tile folders are two levels deep). */
bool remove_tree(const char *path, int depth = 0) {
  struct stat st {};
  if (lstat(path, &st) != 0)
    return errno == ENOENT;
  if (!S_ISDIR(st.st_mode))
    return unlink(path) == 0 || errno == ENOENT;
  DIR *d = depth <= 4 ? opendir(path) : nullptr;
  if (!d)
    return false;
  bool ok = true;
  while (const dirent *e = readdir(d)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    char child[512];
    const int k = snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
    ok = k > 0 && static_cast<size_t>(k) < sizeof(child) &&
         remove_tree(child, depth + 1) && ok;
  }
  closedir(d);
  return (rmdir(path) == 0 || errno == ENOENT) && ok;
}

bool remove_logged(const char *path) {
  if (remove_tree(path))
    return true;
  LOG_ERROR("fps-monitor: home tile delete %s failed errno=%d", path, errno);
  return false;
}

/* ---- nullfs bind at /system_ex/app/<id> (mount_title_nullfs) ---- */

bool mount_live() {
  struct statfs sfs {};
  return statfs(kSystemEx, &sfs) == 0 && strcmp(sfs.f_mntonname, kSystemEx) == 0;
}

iovec iov_str(const char *s) {
  return {const_cast<char *>(s), strlen(s) + 1};
}

/* Flip a mounted filesystem to read-write in place. FW 13.x mounts
 * /system_ex read-only, so the mountpoint mkdir failed with EROFS. */
int remount_rw(const char *mnt) {
  struct statfs sfs {};
  if (statfs(mnt, &sfs) != 0 || strcmp(sfs.f_mntonname, mnt) != 0)
    return -1;
  iovec iov[] = {iov_str("fstype"), iov_str(sfs.f_fstypename),
                 iov_str("fspath"), iov_str(mnt),
                 iov_str("from"),   iov_str(sfs.f_mntfromname)};
  return nmount(iov, sizeof(iov) / sizeof(iov[0]), MNT_UPDATE);
}

/* "" or what failed (logged). Clears any mount already there first. */
std::string mount_nullfs() {
  if (mkdir("/system_ex/app", 0755) != 0 && errno != EEXIST) {
    LOG_ERROR("fps-monitor: home tile mkdir /system_ex/app failed errno=%d",
              errno);
    return "could not create /system_ex/app";
  }
  int mk = mkdir(kSystemEx, 0755);
  if (mk != 0 && errno == EROFS) {
    if (remount_rw("/system_ex") == 0)
      LOG_INFO("fps-monitor: home tile remounted read-only /system_ex rw");
    else
      LOG_WARN("fps-monitor: home tile /system_ex is read-only and the rw "
               "remount failed errno=%d",
               errno);
    mk = mkdir(kSystemEx, 0755);
  }
  if (mk != 0 && errno != EEXIST) {
    LOG_ERROR("fps-monitor: home tile mkdir %s failed errno=%d", kSystemEx,
              errno);
    return "could not create the mount point";
  }
  if (mount_live() && unmount(kSystemEx, 0) != 0 && errno != EINVAL &&
      errno != ENOENT && unmount(kSystemEx, MNT_FORCE) != 0 &&
      errno != EINVAL && errno != ENOENT) {
    LOG_ERROR("fps-monitor: home tile unmount stale %s failed errno=%d",
              kSystemEx, errno);
    return "could not clear the old mount";
  }
  iovec iov[] = {iov_str("fstype"), iov_str("nullfs"), iov_str("from"),
                 iov_str(TILE_DIR), iov_str("fspath"), iov_str(kSystemEx)};
  if (nmount(iov, 6, 0) != 0) {
    LOG_ERROR("fps-monitor: home tile nullfs mount %s failed errno=%d",
              kSystemEx, errno);
    return "could not mount the tile folder";
  }
  /* The bind must surface eboot.bin; a mount without it fails at launch. */
  if (access("/system_ex/app/" FPSMON_TILE_ID "/eboot.bin", F_OK) != 0) {
    LOG_ERROR("fps-monitor: home tile mount at %s shows no eboot.bin",
              kSystemEx);
    (void)unmount(kSystemEx, MNT_FORCE);
    return "the tile mount shows no eboot.bin";
  }
  return "";
}

/* Nothing mounted (EINVAL / ENOENT) counts as done. */
bool unmount_nullfs() {
  if (unmount(kSystemEx, 0) == 0 || errno == EINVAL || errno == ENOENT)
    return true;
  if (unmount(kSystemEx, MNT_FORCE) == 0 || errno == EINVAL || errno == ENOENT)
    return true;
  LOG_ERROR("fps-monitor: home tile unmount %s failed errno=%d", kSystemEx,
            errno);
  return false;
}

/* ---- add / restore / remove: "" on success, else the error for the page */

std::string add_tile() {
  if (sync_files(kTileFiles) < 0)
    return "could not write the tile files";
  if (sync_files(kStagedFiles) < 0)
    return "could not stage the tile in /user/app";
  if (std::string err = mount_nullfs(); !err.empty())
    return err;
  const int fd = open(kTracker, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  const bool tracked = fd >= 0 && write(fd, TILE_DIR, sizeof(TILE_DIR) - 1) ==
                                      static_cast<ssize_t>(sizeof(TILE_DIR) - 1);
  if (fd >= 0)
    close(fd);
  if (!tracked) {
    LOG_ERROR("fps-monitor: home tile write %s failed errno=%d", kTracker,
              errno);
    (void)unmount_nullfs();
    return "could not write the tracking link";
  }
  pthread_once(&g_sony_once, sony_init);
  if (!g_sony.install_title_dir && !g_sony.install_all) {
    /* ps5upload leaves the bind and the staged files: a retry reuses them. */
    LOG_ERROR("fps-monitor: home tile: no app install API on this firmware "
              "(sceAppInstUtilAppInstallTitleDir / AppInstallAll)");
    return "this firmware has no app install API";
  }
  if (g_sony.init)
    (void)g_sony.init(); // idempotent
  const char *api =
      g_sony.install_title_dir ? "AppInstallTitleDir" : "AppInstallAll";
  /* A hang inside Sony's installer leaves this as the last tile line. */
  LOG_INFO("fps-monitor: home tile files staged and bound; calling %s", api);
  const int rc = g_sony.install_title_dir
                     ? g_sony.install_title_dir(FPSMON_TILE_ID, "/user/app/",
                                                nullptr)
                     : g_sony.install_all(nullptr);
  usleep(kSonyPostSleepUs);
  if (!fpsmon::tile::install_ok(rc)) {
    LOG_ERROR("fps-monitor: home tile %s rc=%#x", api,
              static_cast<unsigned>(rc));
    (void)unmount_nullfs(); // no app.db row: do not leak the bind
    return fmt("the console refused to install it (0x%08x)",
               static_cast<unsigned>(rc));
  }
  const int mfd = open(kMarker, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (mfd < 0) {
    LOG_ERROR("fps-monitor: home tile write %s failed errno=%d", kMarker,
              errno);
    return "added, but could not save that it is on";
  }
  close(mfd);
  LOG_INFO("fps-monitor: home tile %s added via %s rc=%#x", FPSMON_TILE_ID,
           api, static_cast<unsigned>(rc));
  return "";
}

/* After a reboot: Sony still has the title, only the bind is gone. */
std::string restore_tile() {
  const int changed = sync_files(kTileFiles);
  if (changed < 0)
    return "could not write the tile files";
  switch (fpsmon::tile::startup_plan(changed > 0, mount_live())) {
  case fpsmon::tile::Startup::None:
    LOG_DEBUG("fps-monitor: home tile already bound this boot");
    return "";
  case fpsmon::tile::Startup::Remount:
    if (const std::string err = mount_nullfs(); err.empty()) {
      LOG_INFO("fps-monitor: home tile re-mounted after reboot (no Sony call)");
      return "";
    } else {
      LOG_WARN("fps-monitor: home tile re-mount failed (%s); registering "
               "it again",
               err.c_str());
    }
    break;
  case fpsmon::tile::Startup::Register:
    LOG_INFO("fps-monitor: home tile files changed; registering it again");
    break;
  }
  return add_tile();
}

/* sceAppInstUtilAppUnInstall under ShellCore's authid, which the installer
 * requires (ps5upload: rejected on FW 9.60+ without it). Swapped for the
 * call only: ps5upload saw a permanent ShellCore authid on a non-ShellCore
 * pid black-screen the console. */
int uninstall_as_shellcore() {
  const pid_t pid = getpid();
  pthread_mutex_lock(&g_authid_mutex);
  const uint64_t saved = kernel_get_ucred_authid(pid);
  if (saved == 0)
    LOG_WARN("fps-monitor: home tile cannot read the authid; uninstalling "
             "with the current one");
  else if (kernel_set_ucred_authid(pid, kShellCoreAuthid) != 0)
    LOG_WARN("fps-monitor: home tile authid swap failed; uninstalling with "
             "%#llx",
             static_cast<unsigned long long>(saved));
  const int rc = g_sony.uninstall(FPSMON_TILE_ID, nullptr, nullptr);
  /* FW 13.x: a register through AppInstallAll leaves two install records
   * and one uninstall removes one. Repeat while Sony reports success. */
  if (rc == 0 && !g_sony.install_title_dir)
    for (int again = 0; again < 2; ++again) {
      usleep(kSonyPostSleepUs);
      if (g_sony.uninstall(FPSMON_TILE_ID, nullptr, nullptr) != 0)
        break;
    }
  bool restored = saved == 0;
  for (int attempt = 0; !restored && attempt < 3; ++attempt) {
    (void)kernel_set_ucred_authid(pid, saved);
    restored = kernel_get_ucred_authid(pid) == saved;
  }
  pthread_mutex_unlock(&g_authid_mutex);
  if (!restored)
    LOG_ERROR("fps-monitor: home tile could not restore authid %#llx",
              static_cast<unsigned long long>(saved));
  usleep(kSonyPostSleepUs);
  return rc;
}

std::string remove_tile(bool retried = false) {
  if (access(kMarker, F_OK) != 0 && !is_dir(TILE_USER_APP)) {
    LOG_DEBUG("fps-monitor: home tile not registered; nothing to remove");
    return "";
  }
  pthread_once(&g_sony_once, sony_init);
  if (!g_sony.uninstall) {
    LOG_ERROR("fps-monitor: home tile: sceAppInstUtilAppUnInstall not "
              "exported on this firmware; keeping the shortcut");
    return "this firmware has no uninstall API; the shortcut was kept";
  }
  /* The bind goes first (ps5upload's order), so Sony never sees a stale
   * one. */
  if (!unmount_nullfs())
    return "could not unmount the tile; the shortcut was kept";
  LOG_INFO("fps-monitor: home tile unbound; calling AppUnInstall");
  const int rc = uninstall_as_shellcore();
  /* Sony may have dropped the title already (Options > Delete on the home
   * screen leaves our marker), and its "not installed" code is unknown:
   * register it again and uninstall once more. A real refusal fails twice. */
  if (rc != 0 && !retried && add_tile().empty()) {
    LOG_WARN("fps-monitor: home tile uninstall rc=%#x; re-registered, "
             "retrying once",
             static_cast<unsigned>(rc));
    return remove_tile(true);
  }
  if (rc != 0) {
    /* Keep the files and bind them again: a working tile, not a greyed-out
     * one whose folder is gone. */
    LOG_ERROR("fps-monitor: home tile sceAppInstUtilAppUnInstall rc=%#x; "
              "keeping the shortcut",
              static_cast<unsigned>(rc));
    if (const std::string err = mount_nullfs(); !err.empty())
      LOG_ERROR("fps-monitor: home tile re-mount after the refusal failed: %s",
                err.c_str());
    return fmt("the console refused to remove it (0x%08x); the shortcut was "
               "kept",
               static_cast<unsigned>(rc));
  }
  /* The staged copy must go too: FW 13.x AppInstallAll would install it
   * again with the next app registered that way. */
  bool ok = remove_logged(TILE_USER_APP);
  ok = remove_logged(TILE_APPMETA) && ok;
  ok = remove_logged(TILE_DIR) && ok;
  if (unlink(kMarker) != 0 && errno != ENOENT) {
    LOG_ERROR("fps-monitor: home tile delete %s failed errno=%d", kMarker,
              errno);
    ok = false;
  }
  if (!ok)
    return "removed, but some files could not be deleted (see the log)";
  LOG_INFO("fps-monitor: home tile %s removed", FPSMON_TILE_ID);
  return "";
}

/* ---- The worker (webui_tile_request): one detached thread applies the
 * latest request, so add and remove never overlap and a fast on/off/on ends
 * in the last state asked for. ---- */

enum class Want { Off, On, Restore };

pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
unsigned g_gen = 0, g_done = 0;
Want g_want = Want::Off;
bool g_running = false, g_busy = false;
std::string g_error;
time_t g_settle_sec = 0;

time_t uptime() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec;
}

void *worker(void *) {
  /* Sony calls right at boot are what kBootSettleSec avoids for the inject;
   * a request this early waits too. */
  if (const time_t up = uptime(); up < g_settle_sec)
    LOG_INFO("fps-monitor: home tile waits %ld s for the console to settle",
             static_cast<long>(g_settle_sec - up));
  for (time_t up; (up = uptime()) < g_settle_sec;)
    sleep(static_cast<unsigned>(g_settle_sec - up));
  pthread_mutex_lock(&g_mu);
  while (g_done != g_gen) {
    const unsigned gen = g_gen;
    const Want want = g_want;
    pthread_mutex_unlock(&g_mu);
    LOG_INFO("fps-monitor: home tile %s",
             want == Want::Off  ? "removing"
             : want == Want::On ? "adding"
                                : "restoring after start");
    std::string err = want == Want::Off  ? remove_tile()
                      : want == Want::On ? add_tile()
                                         : restore_tile();
    if (!err.empty())
      LOG_WARN("fps-monitor: home tile failed: %s", err.c_str());
    pthread_mutex_lock(&g_mu);
    g_done = gen;
    g_error = std::move(err);
  }
  g_busy = g_running = false;
  pthread_mutex_unlock(&g_mu);
  return nullptr;
}

void request(Want want) {
  pthread_mutex_lock(&g_mu);
  ++g_gen;
  g_want = want;
  g_busy = true;
  if (!g_running) {
    pthread_t thr = nullptr;
    if (const int rc = pthread_create(&thr, nullptr, worker, nullptr)) {
      LOG_ERROR("fps-monitor: home tile thread failed to start rc=%d", rc);
      g_done = g_gen;
      g_busy = false;
      g_error = "could not start the shortcut thread";
    } else {
      pthread_detach(thr);
      g_running = true;
    }
  }
  pthread_mutex_unlock(&g_mu);
}

} // namespace

void home_tile_start(time_t boot_settle_sec) {
  g_settle_sec = boot_settle_sec;
  if (access(kMarker, F_OK) == 0)
    request(Want::Restore);
}

void home_tile_request(bool on) { request(on ? Want::On : Want::Off); }

HomeTileState home_tile_state() {
  const bool on = access(kMarker, F_OK) == 0;
  pthread_mutex_lock(&g_mu);
  HomeTileState s{on, g_busy, g_error};
  pthread_mutex_unlock(&g_mu);
  return s;
}
