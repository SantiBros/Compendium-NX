#pragma once

#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "games.hpp"
#include "util.hpp"

namespace app {

// Things that differ between the Switch and a desktop dev build.
namespace platform {

void init();
void exit();

// Root of the SD card. On desktop: $MM_SD_ROOT, or ./sd next to the working directory.
mm::fs::path sdRoot();

// Installed version of a game ("1.0.2"), or "" when unknown / not installed.
// On desktop: $MM_P5R_VERSION.
std::string gameVersion(mm::Game game);

uint64_t freeSpaceBytes();
std::string userAgent();
// Certificate bundle shipped with the app ("" = system store).
std::string caBundle();
// A data file shipped with the app (romfs on Switch, ./resources on desktop).
mm::fs::path resourcePath(const std::string& name);

// Whether the app runs in place of the game (opened by holding R while starting it), so it
// can read the game's own files. On desktop: when $MM_GAME_ROMFS_P5R is set.
bool runningAsGame(mm::Game game);

// Copy one of the game's own files (a path in its romfs, "CPK/PATCH1.CPK") to `dest`.
// Only while running in place of the game. With L held too at launch, Atmosphère's
// LayeredFS is off, so these are the game's files and not the SD card's. Throws with a
// reason. On desktop: from $MM_GAME_ROMFS_P5R.
void copyGameFile(mm::Game game, const std::string& romfsPath, const mm::fs::path& dest,
                  const std::function<void(uint64_t done, uint64_t total)>& progress = {});

// The paths in the game's own CPKs (romfs CPK/*.CPK), read from their file tables only.
// Same conditions as copyGameFile. `skipCpk` names an archive to leave out.
std::vector<std::string> gameCpkFileList(mm::Game game, const std::string& skipCpk = "");

// Whether the app has the console's full memory: opened by holding R while starting a game,
// rather than from the Album. On desktop: unless $MM_APPLET is set.
bool fullMemory();

// Calendar time in the console's time zone (newlib's localtime() gives UTC on the Switch).
std::tm localTime(int64_t unixSeconds);

}  // namespace platform
}  // namespace app
