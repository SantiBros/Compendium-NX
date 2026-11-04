#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "games.hpp"
#include "persona_layout.hpp"
#include "util.hpp"

namespace mm {

// How the mods reach the game.
enum class DeployMode {
    // Build the game's PATCH1.CPK (with the game's own PATCH1 files kept) and let
    // Atmosphère's LayeredFS load it. Works on its own, on any game version. The default.
    Patch1,
    // Build one CPK into sd:/p5r/bind/, which the p5rcbt plugin (Skyline) adds to the game's
    // files. Leaves PATCH1.CPK alone; needs p5rcbt installed and game version 1.0.2.
    P5rcbt,
};
const char* deployModeKey(DeployMode m);

struct ModRecord {
    std::string id;  // "gb<modId>-<fileId>" for GameBanana, "local-<slug>" otherwise
    Game game = Game::P5R;
    std::string name;
    std::string author;
    std::string version;
    std::string sourceFile;  // archive it came from
    std::string profileUrl;
    std::string sourceKey;  // "gb:<id>", "url:<link>", "file:<name>"
    int64_t gamebananaModId = 0;
    int64_t gamebananaFileId = 0;
    int64_t installedAt = 0;  // unix seconds
    std::vector<std::string> variants;  // the archive's options that were installed
    std::string format;                 // what kind of mod it was (formatName)
    bool pcMod = false;                 // adapted from the PC version
    std::vector<std::string> notes;     // what was left out or guessed when installing
    bool enabled = false;
    int priority = 0;  // higher wins when mods change the same file
    std::vector<std::string> cpkFiles;  // archive paths of its loose files (kept under files/)
    std::vector<std::string> bundles;   // CPKs it came with (kept under cpk/, by file name)
    std::vector<std::string> sdFiles;   // files copied to the SD card, relative to its root (kept under sd/)
};

struct Conflict {
    std::string path;                 // CPK path, or SD card path for sdFiles
    std::vector<std::string> modIds;  // highest priority (the one used) first
    bool inCpk = true;
};

// What a mod's chosen options install (see mergeVariants).
struct InstallSet {
    std::vector<CpkFile> files;
    std::vector<CpkBundle> cpks;
    std::vector<SdFile> sdFiles;
};

// The CPK the library built for a game, as last applied.
struct BuildStatus {
    std::string target;     // SD-relative path of the archive ("" when there's none)
    size_t files = 0;       // files in it
    uint64_t bytes = 0;
    bool withOriginals = false;    // the game's own PATCH1 files are in it (PATCH1 mode)
    bool missingOriginals = false; // PATCH1 mode without the game's own PATCH1.CPK kept
};

using ProgressFn = std::function<void(uint64_t done, uint64_t total, const std::string& current)>;

// The SD card doesn't have room for a change, worked out before anything was written.
struct NotEnoughSpaceError : std::runtime_error {
    NotEnoughSpaceError(uint64_t needed, uint64_t free);
    uint64_t needed, free;
};

// Owns the mod library on the SD card and keeps what the game loads in sync with which
// mods are on. Layout under `dataDir`:
//   state.json                          records, settings and what's deployed
//   library/<game>/<id>/files/<path>    a mod's loose files, at their CPK paths
//   library/<game>/<id>/cpk/<name>      CPKs a mod came with
//   library/<game>/<id>/sd/<target>     a mod's files for the SD card
//   original/<game>/PATCH1.CPK          the game's own PATCH1.CPK (PATCH1 mode keeps its files)
//   backup/<target>                     files that were on the SD card before (restored later)
class Library {
public:
    Library(fs::path sdRoot, fs::path dataDir);

    // Reads state.json. An unreadable file is kept as state.json.corrupt and the library
    // starts empty (returns false).
    bool load();
    void save() const;

    std::vector<const ModRecord*> mods(Game game) const;  // highest priority first
    const ModRecord* find(const std::string& id) const;

    // Store a mod (replacing one with the same id, keeping its place) and optionally turn it on.
    const ModRecord& add(ModRecord record, const InstallSet& files, bool enable, const ProgressFn& progress = {});
    void setEnabled(const std::string& id, bool enabled, const ProgressFn& progress = {});
    void remove(const std::string& id, const ProgressFn& progress = {});
    void move(const std::string& id, int delta, const ProgressFn& progress = {});  // +1 = up (wins more)

    std::vector<Conflict> conflicts(Game game) const;

    DeployMode mode(Game game) const;
    void setMode(Game game, DeployMode mode, const ProgressFn& progress = {});

    // Where the game's own PATCH1.CPK is kept, and whether it is.
    fs::path originalPatchCpk(Game game) const;
    bool hasOriginalPatch(Game game) const;

    // Where the built archive goes in a mode, relative to the SD root.
    static std::string outputTarget(Game game, DeployMode mode);

    // A PATCH1.CPK (or p5rcbt CPK) on the SD card that this library didn't build: mods
    // installed by hand or by another tool. Empty if none.
    std::string unmanagedOutput(Game game) const;
    // Adopt it as a mod (lowest priority, on), so it can be turned off and back on.
    const ModRecord& importUnmanagedOutput(Game game, const std::string& name, const ProgressFn& progress = {});

    // Make the SD card match the enabled mods. Called by the mutators above; also a "repair".
    void apply(const ProgressFn& progress = {});
    // Build the game's archive again even if nothing seems to have changed.
    void rebuild(Game game, const ProgressFn& progress = {});

    const BuildStatus& buildStatus(Game game) const;

    using FreeSpaceFn = std::function<uint64_t()>;
    void setFreeSpace(FreeSpaceFn fn) { freeSpace_ = std::move(fn); }

    // Finds a file's CPK folder from the game's file list (for mods missing it).
    void setPathResolver(PathResolver r) { resolver_ = std::move(r); }
    const PathResolver& pathResolver() const { return resolver_; }

    fs::path modDir(const ModRecord& m) const;
    const fs::path& sdRoot() const { return sdRoot_; }
    const fs::path& dataDir() const { return dataDir_; }

private:
    struct GameState {
        DeployMode mode = DeployMode::Patch1;
        std::string output;     // SD-relative path of the archive we built ("" = none)
        std::string outputKey;  // what it was built from
        uint64_t outputSize = 0;
        BuildStatus status;
    };
    ModRecord* findMut(const std::string& id);
    void renumber(std::vector<ModRecord*> ordered);
    std::vector<const ModRecord*> enabledAscending(Game game) const;
    std::vector<std::string> bundleEntries(const ModRecord& m, const std::string& bundle) const;
    void applySdFiles(const ProgressFn& progress);
    void applyCpk(Game game, bool force, const ProgressFn& progress);
    void removeOutput(GameState& gs);
    void backupFile(const std::string& target);
    void restoreBackup(const std::string& target);
    void checkSpace(uint64_t needed) const;
    GameState& gameState(Game g) { return games_[g]; }
    const GameState* gameStateIf(Game g) const {
        auto it = games_.find(g);
        return it == games_.end() ? nullptr : &it->second;
    }

    fs::path sdRoot_, dataDir_;
    std::vector<ModRecord> mods_;
    std::map<Game, GameState> games_;
    std::map<std::string, std::string> deployed_;  // lower-case SD target -> mod id
    std::map<std::string, std::string> backedUp_;  // lower-case target -> target as stored
    FreeSpaceFn freeSpace_;
    PathResolver resolver_;
    mutable std::map<std::string, std::vector<std::string>> bundleCache_;
};

}  // namespace mm
