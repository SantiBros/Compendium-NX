#pragma once

// p5rcbt (github.com/Raytwo/p5rcbt): a Skyline plugin for Persona 5 Royal on Switch that
// loads CPKs and folders from sd:/p5r/bind/ next to the game's own files, and adds a 60 FPS
// patch and the PC version's settings menu. Only for game version 1.0.2.

#include <cstdint>
#include <string>

#include "games.hpp"
#include "util.hpp"

namespace mm::p5rcbt {

inline constexpr const char* kReleaseApi = "https://api.github.com/repos/Raytwo/p5rcbt/releases/latest";
inline constexpr const char* kConfigPath = "p5r/cfg/cbt.toml";
inline constexpr const char* kRequiredGameVersion = "1.0.2";
inline constexpr const char* kModId = "tool-p5rcbt";

struct Config {
    bool logging = false;
    bool uncapFramerate = true;  // its 60 FPS patch
    bool pcSettings = false;     // the PC version's settings menu
};

Config readConfig(const fs::path& sdRoot);
void writeConfig(const fs::path& sdRoot, const Config& c);

// Whether the plugin and Skyline are on the SD card.
bool installed(const fs::path& sdRoot, Game game);

struct Release {
    std::string tag;
    std::string zipUrl;
    int64_t size = 0;
};
// GitHub's "latest release" answer. Throws if it has no zip.
Release parseRelease(const std::string& githubJson);

}  // namespace mm::p5rcbt
