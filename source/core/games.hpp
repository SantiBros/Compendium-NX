#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mm {

// Games the manager installs mods for. Only Persona 5 Royal is supported in this version;
// the other Persona games on Switch are listed in kPlannedGames so the app can say so.
enum class Game { P5R };

struct GameInfo {
    Game game;
    std::string_view key;          // short id used in folder names and state files
    std::string_view displayName;
    std::string_view titleId;      // upper-case hex, as Atmosphère expects
    int gamebananaId;              // the game's Switch page on GameBanana
    // Where mods' CPK contents go when the manager builds the game's patch archive
    // (LayeredFS mode), relative to the game's romfs.
    std::string_view patchCpk;
    // Versions the game has shipped (for the version checks on mod pages).
    int maxMinorVersion;
};

inline constexpr std::array<GameInfo, 1> kGames = {{
    {Game::P5R, "p5r", "Persona 5 Royal", "01005CA01580E000", 17354, "CPK/PATCH1.CPK", 0},
}};

// Persona games on Switch that aren't supported yet (shown as "coming later").
struct PlannedGame {
    std::string_view displayName;
};
inline constexpr std::array<PlannedGame, 5> kPlannedGames = {{
    {"Persona 4 Golden"},
    {"Persona 3 Portable"},
    {"Persona 5 Strikers"},
    {"Persona 5 Tactica"},
    {"Persona 4 Arena Ultimax"},
}};

const GameInfo& gameInfo(Game g);
std::optional<Game> gameFromKey(std::string_view key);
std::optional<Game> gameFromTitleId(std::string_view titleId);  // case-insensitive
std::vector<std::string> titleIdsFor(Game game);

// "atmosphere/contents/<title id>" for the game, relative to the SD root.
std::string contentsDir(Game game);

}  // namespace mm
