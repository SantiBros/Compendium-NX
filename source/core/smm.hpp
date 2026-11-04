#pragma once

// SimpleModManager (github.com/nadrino/SimpleModManager) keeps mods on the SD card as
// mods/<game name>/<mod name>/contents/<title id>/…, and copies the enabled ones into
// atmosphere/. Its Persona mods can be brought into this manager from there.

#include <string>
#include <vector>

#include "games.hpp"
#include "util.hpp"

namespace mm::smm {

struct FoundMod {
    std::string gameFolder;  // as SimpleModManager names it
    std::string name;        // the mod's folder name
    fs::path dir;
};

// SimpleModManager mods for `game` on the SD card (folders holding contents/<title id>/).
std::vector<FoundMod> findMods(const fs::path& sdRoot, Game game);

}  // namespace mm::smm
