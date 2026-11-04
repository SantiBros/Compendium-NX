#pragma once

// Works out what a Persona mod archive holds once it's unpacked, whatever shape its author
// gave it, and where each file goes:
//
//   - Aemulus packages (a folder with Package.xml / Mod.xml): the files mirror the game's CPK
//     paths ("BASE/BATTLE/…", "EN/INIT/…", "PATCH1/…"), as Aemulus copies them.
//   - Ready-made CPKs (PATCH1.CPK from an Aemulus build, or any mod .cpk): their files.
//   - Loose files laid out like the game's CPKs, including Aemulus' output folder
//     (romfs/CPK/PATCH1/…) and CriPakTools' "PATCH1.CPK_unpacked".
//   - Atmosphère layouts (atmosphere/contents/<title id>/…), as SimpleModManager keeps them:
//     PATCH1.CPK inside is read as above; cheats, exefs and Skyline plugins go to the SD card.
//   - p5rcbt's folders (p5r/bind/…).
//   - Reloaded-II mods for the PC version (ModConfig.json, P5REssentials/CPK/<name>.CPK/…):
//     adapted, since most of their files are the same as the Switch version's.
//
// Parts that need a PC tool to merge (Aemulus' .flow/.msg/.tblpatch, Reloaded's FEmulator,
// PC code mods) are left out with a note saying so.

#include <functional>
#include <string>
#include <vector>

#include "games.hpp"
#include "util.hpp"

namespace mm {

// A file of the mod that goes into the game's CPK.
struct CpkFile {
    fs::path source;      // absolute, inside the unpacked archive
    std::string cpkPath;  // "BASE/BATTLE/TABLE/UNIT.TBL"
};

// A CPK that came with the mod; its files are used as they are.
struct CpkBundle {
    fs::path cpk;
    size_t files = 0;
};

// A file copied straight to the SD card (cheats, exefs, Skyline plugins, p5rcbt settings).
struct SdFile {
    fs::path source;
    std::string target;  // relative to the SD root
};

enum class ModFormat { Aemulus, LooseFiles, ReadyCpk, ReloadedPc, P5rcbtFolder, Atmosphere };
const char* formatName(ModFormat f);

struct PersonaVariant {
    std::string name;  // the folder it came from, when the archive has several options
    ModFormat format = ModFormat::LooseFiles;
    std::vector<CpkFile> files;
    std::vector<CpkBundle> cpks;
    std::vector<SdFile> sdFiles;
    std::vector<std::string> notes;  // what was left out or guessed, for the user
    bool empty() const { return files.empty() && cpks.empty() && sdFiles.empty(); }
};

struct PersonaLayout {
    std::vector<PersonaVariant> variants;
    std::vector<std::string> warnings;
    std::string problem;  // set when nothing can be installed: why
    bool pcMod = false;   // made for the PC version and adapted
};

// Finds where the game keeps a file given without its CPK folder ("BATTLE/x.bin" ->
// "BASE/BATTLE/x.bin"), from the game's own file list; "" if it doesn't have it.
using PathResolver = std::function<std::string(const std::string& path)>;

PersonaLayout detectPersonaLayout(const fs::path& extractedRoot, Game game, const PathResolver& resolve = {});

// Top-level folders of P5R's CPKs ("BASE", "EN", "PATCH1", "SOUND", "MOVIE"…).
bool isCpkRootFolder(const std::string& name);

}  // namespace mm
