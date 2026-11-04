#pragma once

// The game's own file list: every path in its CPKs (ALL_USEU.CPK, PATCH1.CPK), read once
// from the game and kept as a text file. Used to put mod files that come without their CPK
// folder ("BATTLE/…") where the game has them ("BASE/BATTLE/…", "EN/BATTLE/…").

#include <string>
#include <unordered_map>
#include <vector>

#include "util.hpp"

namespace mm {

class GameFileList {
public:
    bool load(const fs::path& listFile);  // false if missing or empty
    static void save(const fs::path& listFile, const std::vector<std::string>& paths);
    // Every path in a CPK.
    static std::vector<std::string> fromCpk(const fs::path& cpk);

    bool empty() const { return byRest_.empty(); }
    size_t size() const { return count_; }
    // "BATTLE/x.bin" -> "BASE/BATTLE/x.bin" (the folder the game reads it from), or "".
    std::string resolve(const std::string& pathWithoutRoot) const;

private:
    // lower-case path below its top folder -> the full paths that end with it
    std::unordered_map<std::string, std::vector<std::string>> byRest_;
    size_t count_ = 0;
};

}  // namespace mm
