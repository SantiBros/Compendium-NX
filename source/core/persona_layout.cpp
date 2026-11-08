#include "persona_layout.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

#include "archive.hpp"
#include "cpk.hpp"

namespace mm {

namespace {

// P5R (Switch) keeps everything in ALL_USEU.CPK and PATCH1.CPK under these top folders; the
// game looks a file up by its full path, so a mod's file must sit under the same one. SOUND_E has the English
// voices (SOUND_E/SOUND/EVENT/…); the other sounds and the movies are in BASE/SOUND/, EN/MOVIE/….
const std::set<std::string> kCpkRoots = {"base", "patch1", "en", "fr", "it", "de", "es", "sound_e"};

// The folders the game looks a file up in, highest first: PATCH1/, the language's, BASE/. A mod's folder named
// PATCH1 with one of these inside is a PATCH1.CPK to pack ("PATCH1/EN/MOVIE/MOV000.USM" is EN/MOVIE/…).
const std::set<std::string> kLayerFolders = {"patch1", "base", "en", "fr", "it", "de", "es"};

// CriWare's packing tool for PCs, which some mods ship to build their PATCH1.CPK; the app doesn't need it.
const std::set<std::string> kCpkToolFiles = {"cpkmakec.exe", "cpkmaker.dll", "cpkbinder.dll", "cpkmakec.exe.config"};

// Folders found right under BASE/ (and the language folders). A mod laid out from one of
// these is missing its CPK folder.
const std::set<std::string> kGameFolders = {
    "aicalc", "battle",   "bustup",    "button", "calendar", "camp",   "cutin",  "dungeon", "event",
    "event_data", "facility", "field", "font",   "image",    "init",   "minigame", "model", "movie",
    "mypalace", "resource", "scheduler", "script", "shop",   "sound",  "system", "texture", "title",
    "tutorial",
};

// Source files for pictures and models that some mods ship next to the game files; never part of the game.
const std::set<std::string> kAuthoringExt = {".psd", ".pdn", ".xcf", ".kra", ".blend", ".blend1"};

// Aemulus' build skips these (they're merged by its PC tools, or are package metadata).
const std::set<std::string> kAemulusSkipExt = {
    ".aem", ".tblpatch", ".xml", ".png", ".jpg", ".7z", ".bat", ".txt", ".zip", ".json", ".tbp",
    ".rar", ".exe", ".dll", ".flow", ".msg", ".back", ".bp", ".pnach",
};

// Files that never go anywhere: readmes, previews, metadata.
const std::set<std::string> kDocExt = {".txt", ".md", ".png", ".jpg", ".jpeg", ".webp", ".gif", ".pdf",
                                       ".url", ".html", ".htm", ".xml", ".json", ".ini", ".rtf", ".docx"};

struct FileRec {
    fs::path abs;
    std::string rel;
    std::vector<std::string> parts;  // as in the archive
    std::vector<std::string> lower;
    bool used = false;
};

std::string join(const std::vector<std::string>& parts, size_t from, size_t to) {
    std::string out;
    for (size_t i = from; i < to && i < parts.size(); ++i) {
        if (!out.empty()) out += '/';
        out += parts[i];
    }
    return out;
}

std::string ext(const std::string& name) {
    auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : toLower(name.substr(dot));
}

bool isJunk(const FileRec& f) {
    // __MACOSX, and hidden folders and files (.git/, .DS_Store, a ".derp" folder of PC tools).
    for (const auto& p : f.lower)
        if (p == "__macosx" || (!p.empty() && p[0] == '.')) return true;
    const auto& n = f.lower.back();
    return n == "thumbs.db" || n == "desktop.ini" || kAuthoringExt.count(ext(n));
}

// DATA01/DATA.DAT…: a slot of the game's save data.
bool isSaveSlot(const FileRec& f) {
    if (f.lower.size() < 2 || f.lower.back() != "data.dat") return false;
    const auto& d = f.lower[f.lower.size() - 2];
    return d.size() == 6 && d.rfind("data", 0) == 0 && std::isdigit((unsigned char)d[4]) && std::isdigit((unsigned char)d[5]);
}

// "708BCACA873EA32B….ips": an exefs patch named by the build id of the game's program it changes.
bool isBuildIdPatch(const std::string& lowerName) {
    if (ext(lowerName) != ".ips") return false;
    auto stem = lowerName.substr(0, lowerName.size() - 4);
    return stem.size() >= 16 && stem.size() <= 64 &&
           std::all_of(stem.begin(), stem.end(), [](char c) { return std::isxdigit((unsigned char)c); });
}

// A folder named after one of the PC version's CPKs ("BASE.CPK", "EN.CPK"), as some packages have: on the
// Switch that's the folder of the same name in the game's CPKs. "base.cpk" -> "BASE"; "" for anything else.
std::string pcCpkFolder(const std::string& lowerName) {
    if (lowerName.size() <= 4 || lowerName.compare(lowerName.size() - 4, 4, ".cpk") != 0) return {};
    auto stem = lowerName.substr(0, lowerName.size() - 4);
    return stem != "patch1" && kCpkRoots.count(stem) ? toUpper(stem) : std::string();
}

bool isDoc(const FileRec& f) {
    auto e = ext(f.lower.back());
    return kDocExt.count(e) || f.lower.back().rfind("preview", 0) == 0 || f.lower.back().rfind("readme", 0) == 0;
}

struct Builder {
    std::map<std::string, size_t> byRoot;  // "<detector>|<root folder>" -> variant index
    std::vector<PersonaVariant> variants;
    std::vector<std::string> roots;        // shown names
    std::map<size_t, size_t> guessed;      // variant -> files put in BASE/ by guess

    PersonaVariant& variant(const std::string& kind, const std::string& root, ModFormat format) {
        auto key = kind + "|" + root;
        auto it = byRoot.find(key);
        if (it != byRoot.end()) return variants[it->second];
        byRoot[key] = variants.size();
        variants.push_back({});
        variants.back().format = format;
        roots.push_back(root);
        return variants.back();
    }
    size_t indexOf(const PersonaVariant& v) const { return size_t(&v - variants.data()); }
};

// A path below the CPK folders ("BATTLE/x.bin") -> where the game has it ("BASE/BATTLE/x.bin").
std::string placeInCpk(const std::string& path, const PathResolver& resolve, Builder& b, PersonaVariant& v) {
    auto slash = path.find('/');
    auto first = toLower(slash == std::string::npos ? path : path.substr(0, slash));
    if (auto top = pcCpkFolder(first); !top.empty() && slash != std::string::npos) return top + path.substr(slash);
    if (kCpkRoots.count(first)) return path;
    if (resolve) {
        auto found = resolve(path);
        if (!found.empty()) return found;
    }
    ++b.guessed[b.indexOf(v)];
    return "BASE/" + path;
}

}  // namespace

bool isCpkRootFolder(const std::string& name) { return kCpkRoots.count(toLower(name)) > 0; }

const char* formatName(ModFormat f) {
    switch (f) {
        case ModFormat::Aemulus: return "Aemulus package";
        case ModFormat::LooseFiles: return "Game files";
        case ModFormat::ReadyCpk: return "CPK archive";
        case ModFormat::ReloadedPc: return "PC mod (Reloaded-II), adapted";
        case ModFormat::P5rcbtFolder: return "p5rcbt folder";
        case ModFormat::Atmosphere: return "Atmosphère files";
    }
    return "";
}

PersonaLayout detectPersonaLayout(const fs::path& root, Game game, const PathResolver& resolve) {
    PersonaLayout out;
    const auto tid = toLower(gameInfo(game).titleId);
    const std::string tidUpper(gameInfo(game).titleId);

    std::vector<FileRec> files;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        FileRec f;
        f.abs = it->path();
        f.rel = relativeTo(it->path(), root);
        f.parts = pathParts(fs::path(f.rel));
        if (f.parts.empty()) continue;
        for (const auto& p : f.parts) f.lower.push_back(toLower(p));
        if (isJunk(f)) continue;
        // A nested archive that was unpacked next to itself: its contents count, not it.
        if (isSupportedArchiveName(f.parts.back()) && fs::is_directory(f.abs.string() + ".extracted")) continue;
        files.push_back(std::move(f));
    }
    if (files.empty()) {
        out.problem = "The archive is empty.";
        return out;
    }
    std::sort(files.begin(), files.end(), [](const FileRec& a, const FileRec& b) { return a.rel < b.rel; });

    Builder b;
    std::set<std::string> skippedAemulus, skippedPc;
    bool pcCode = false;

    // 0. Save data (DATA01/DATA.DAT…, with SYSTEM/SYSTEM.DAT): it goes in the game's save, not in its CPKs.
    size_t saveFiles = 0;
    bool hasSlots = std::any_of(files.begin(), files.end(), isSaveSlot);
    for (auto& f : files) {
        bool system = f.lower.size() >= 2 && f.lower.back() == "system.dat" && f.lower[f.lower.size() - 2] == "system";
        if (isSaveSlot(f) || (hasSlots && system)) {
            f.used = true;
            ++saveFiles;
        }
    }

    // 1. Atmosphère layouts: [atmosphere/]contents/<title id>/… and exefs_patches/…
    for (auto& f : files) {
        const auto& L = f.lower;
        for (size_t i = 0; i + 1 < L.size(); ++i) {
            if (L[i] == "exefs_patches" && i + 2 < L.size()) {
                size_t start = (i > 0 && L[i - 1] == "atmosphere") ? i - 1 : i;
                auto& v = b.variant("atmo", join(f.parts, 0, start), ModFormat::Atmosphere);
                v.sdFiles.push_back({f.abs, "atmosphere/exefs_patches/" + join(f.parts, i + 1, L.size())});
                f.used = true;
                break;
            }
            if (L[i] != tid) continue;
            size_t start = i;
            if (i > 0 && L[i - 1] == "contents") start = (i > 1 && L[i - 2] == "atmosphere") ? i - 2 : i - 1;
            std::vector<std::string> rest(f.parts.begin() + long(i) + 1, f.parts.end());
            std::vector<std::string> restL(L.begin() + long(i) + 1, L.end());
            auto& v = b.variant("atmo", join(f.parts, 0, start), ModFormat::Atmosphere);
            if (restL.size() >= 3 && restL[0] == "romfs" && restL[1] == "cpk") {
                if (restL.size() == 3 && ext(restL[2]) == ".cpk") {
                    v.cpks.push_back({f.abs, 0});
                } else if (restL[2] == "patch1" && restL.size() > 3) {
                    // Aemulus' output folder before it packs PATCH1.CPK.
                    v.files.push_back({f.abs, join(rest, 3, rest.size())});
                } else {
                    v.sdFiles.push_back({f.abs, contentsDir(game) + "/" + join(rest, 0, rest.size())});
                }
            } else {
                v.sdFiles.push_back({f.abs, "atmosphere/contents/" + tidUpper + "/" + join(rest, 0, rest.size())});
            }
            f.used = true;
            break;
        }
    }

    // 1b. An exefs patch without its folders ("<build id>.ips"): it goes in atmosphere/exefs_patches/<name>/.
    std::set<size_t> exefsVariants;
    for (auto& f : files) {
        if (f.used || !isBuildIdPatch(f.lower.back())) continue;
        auto folder = f.parts.size() > 1 ? f.parts[f.parts.size() - 2] : std::string("Compendium");
        auto& v = b.variant("atmo", join(f.parts, 0, f.parts.size() - 1), ModFormat::Atmosphere);
        v.sdFiles.push_back({f.abs, "atmosphere/exefs_patches/" + slugify(folder) + "/" + f.parts.back()});
        exefsVariants.insert(b.indexOf(v));
        f.used = true;
    }

    // 2. p5rcbt: p5r/bind/<name>.cpk, p5r/bind/<folder>/…, p5r/cfg/cbt.toml
    for (auto& f : files) {
        if (f.used) continue;
        const auto& L = f.lower;
        for (size_t i = 0; i + 2 < L.size(); ++i) {
            if (L[i] != "p5r") continue;
            auto& v = b.variant("cbt", join(f.parts, 0, i), ModFormat::P5rcbtFolder);
            if (L[i + 1] == "cfg") {
                v.sdFiles.push_back({f.abs, "p5r/" + join(f.parts, i + 1, L.size())});
            } else if (L[i + 1] == "bind" && L.size() == i + 3 && ext(L[i + 2]) == ".cpk") {
                v.cpks.push_back({f.abs, 0});
            } else if (L[i + 1] == "bind" && L.size() > i + 3) {
                // Bind folders hold paths without the CPK folder (p5rcbt's README).
                v.files.push_back({f.abs, placeInCpk(join(f.parts, i + 3, L.size()), resolve, b, v)});
            } else {
                continue;
            }
            f.used = true;
            break;
        }
    }

    // 3. Aemulus packages: the deepest folder with Package.xml / Mod.xml above each file.
    std::set<std::string> packages;
    std::set<size_t> prebuild;  // packages with a Prebuild.bat
    for (const auto& f : files)
        if (f.lower.back() == "package.xml" || f.lower.back() == "mod.xml")
            packages.insert(toLower(join(f.parts, 0, f.parts.size() - 1)));
    for (auto& f : files) {
        if (f.used) continue;
        size_t depth = std::string::npos;
        for (size_t d = f.parts.size(); d-- > 0;)
            if (packages.count(toLower(join(f.parts, 0, d)))) {
                depth = d;
                break;
            }
        if (depth == std::string::npos) continue;
        auto& v = b.variant("aem", join(f.parts, 0, depth), ModFormat::Aemulus);
        f.used = true;
        const auto& name = f.lower.back();
        auto e = ext(name);
        if (name == "prebuild.bat") prebuild.insert(b.indexOf(v));
        bool inSkippedFolder = false;
        for (size_t i = depth; i + 1 < f.lower.size(); ++i)
            if (f.lower[i] == "tblpatches" || f.lower[i] == "binarypatches" || f.lower[i] == "spdpatches" ||
                f.lower[i] == "texture_override")
                inSkippedFolder = true;
        if (inSkippedFolder || e == ".flow" || e == ".msg" || e == ".tblpatch" || e == ".tbp" || e == ".bp") {
            skippedAemulus.insert(inSkippedFolder ? "patch folders" : e);
            continue;
        }
        if (kAemulusSkipExt.count(e) || isDoc(f)) continue;
        v.files.push_back({f.abs, placeInCpk(join(f.parts, depth, f.parts.size()), resolve, b, v)});
    }

    // 4. Reloaded-II mods for the PC version.
    std::set<std::string> reloaded;
    for (const auto& f : files)
        if (f.lower.back() == "modconfig.json") reloaded.insert(toLower(join(f.parts, 0, f.parts.size() - 1)));
    for (auto& f : files) {
        if (f.used) continue;
        size_t depth = std::string::npos;
        for (size_t d = f.parts.size(); d-- > 0;)
            if (reloaded.count(toLower(join(f.parts, 0, d)))) {
                depth = d;
                break;
            }
        if (depth == std::string::npos) continue;
        auto& v = b.variant("rel", join(f.parts, 0, depth), ModFormat::ReloadedPc);
        out.pcMod = true;
        f.used = true;
        const auto& L = f.lower;
        auto e = ext(L.back());
        if (e == ".dll" || e == ".exe" || e == ".pdb" || e == ".deps") {
            pcCode = true;
            continue;
        }
        // P5REssentials/CPK/<anything>.CPK/<path in the game's CPK> or …/CPK/<name>.cpk
        size_t ess = std::string::npos;
        for (size_t i = depth; i + 2 < L.size(); ++i)
            if (L[i] == "p5ressentials" && L[i + 1] == "cpk") {
                ess = i;
                break;
            }
        if (ess != std::string::npos) {
            if (L.size() == ess + 3 && e == ".cpk") {
                v.cpks.push_back({f.abs, 0});
            } else if (L.size() > ess + 3) {
                v.files.push_back({f.abs, placeInCpk(join(f.parts, ess + 3, L.size()), resolve, b, v)});
            }
            continue;
        }
        bool emulated = false;
        for (size_t i = depth; i < L.size(); ++i)
            if (L[i] == "femulator" || L[i] == "bf" || L[i] == "bmd" || L[i] == "spd" || L[i] == "pak") emulated = true;
        if (emulated)
            skippedPc.insert("FEmulator");
        else if (!isDoc(f))
            skippedPc.insert(f.parts[depth < f.parts.size() - 1 ? depth : f.parts.size() - 1]);
    }

    // 5. Any other CPK: its files are used as they are.
    for (auto& f : files) {
        if (f.used || ext(f.lower.back()) != ".cpk") continue;
        auto& v = b.variant("cpk", join(f.parts, 0, f.parts.size()), ModFormat::ReadyCpk);
        v.cpks.push_back({f.abs, 0});
        f.used = true;
    }

    // 6. Loose files under the CPK folders. CriPakTools' "PATCH1.CPK_unpacked" and Aemulus'
    //    "romfs/CPK/PATCH1" are the archive itself; otherwise the first BASE/EN/PATCH1… folder.
    for (auto& f : files) {
        if (f.used) continue;
        const auto& L = f.lower;
        size_t rootEnd = std::string::npos;  // files' paths start after this many parts
        for (size_t i = 0; i + 1 < L.size(); ++i) {
            bool unpackedCpk = L[i].rfind("patch1.cpk", 0) == 0 && L[i] != "patch1.cpk";
            bool aemulusOut = L[i] == "patch1" && i >= 1 && L[i - 1] == "cpk";
            // A PATCH1 folder holding BASE/, EN/… (or PATCH1/) is what goes in the archive, not part of the paths.
            bool patchToPack = L[i] == "patch1" && i + 2 < L.size() && kLayerFolders.count(L[i + 1]);
            if (unpackedCpk || aemulusOut || patchToPack) {
                rootEnd = i + 1;
                break;
            }
        }
        if (rootEnd == std::string::npos)
            for (size_t i = 0; i + 1 < L.size(); ++i)
                if (kCpkRoots.count(L[i]) || !pcCpkFolder(L[i]).empty()) {
                    rootEnd = i;
                    break;
                }
        if (rootEnd == std::string::npos) continue;
        auto& v = b.variant("loose", join(f.parts, 0, rootEnd), ModFormat::LooseFiles);
        v.files.push_back({f.abs, placeInCpk(join(f.parts, rootEnd, f.parts.size()), resolve, b, v)});
        f.used = true;
    }

    // 7. Loose files without their CPK folder ("BATTLE/…").
    for (auto& f : files) {
        if (f.used || isDoc(f)) continue;
        const auto& L = f.lower;
        for (size_t i = 0; i + 1 < L.size(); ++i) {
            if (!kGameFolders.count(L[i])) continue;
            auto& v = b.variant("loose", join(f.parts, 0, i), ModFormat::LooseFiles);
            v.files.push_back({f.abs, placeInCpk(join(f.parts, i, L.size()), resolve, b, v)});
            f.used = true;
            break;
        }
    }

    // 8. Any other file the game has under the same name, in one place: put there ("TWEWY/BGM.ACB" is
    //    BASE/SOUND/BGM.ACB). Needs the game's file list.
    std::map<size_t, size_t> byName;
    bool emulatorPatch = false;
    for (auto& f : files) {
        if (f.used || isDoc(f)) continue;
        if (ext(f.lower.back()) == ".pchtxt") {
            emulatorPatch = true;
            f.used = true;
            continue;
        }
        if (!resolve) continue;
        auto found = resolve(f.parts.back());
        if (found.empty()) continue;
        auto& v = b.variant("loose", join(f.parts, 0, f.parts.size() - 1), ModFormat::LooseFiles);
        v.files.push_back({f.abs, found});
        ++byName[b.indexOf(v)];
        f.used = true;
    }

    // Count the files in each CPK that came with the mod (and drop unreadable ones).
    for (auto& v : b.variants) {
        std::vector<CpkBundle> ok;
        for (auto& c : v.cpks) {
            try {
                c.files = cpk::read(c.cpk).entries.size();
                ok.push_back(c);
            } catch (const std::exception& e) {
                v.notes.push_back(c.cpk.filename().string() + " couldn't be read: " + e.what());
            }
        }
        v.cpks = std::move(ok);
    }

    for (size_t i = 0; i < b.variants.size(); ++i) {
        auto& v = b.variants[i];
        if (auto g = b.guessed.find(i); g != b.guessed.end() && g->second)
            v.notes.push_back(std::to_string(g->second) + (g->second == 1 ? " file wasn't" : " files weren't") +
                              " in a CPK folder (BASE, EN, PATCH1…), so " + (g->second == 1 ? "it was" : "they were") +
                              " put in BASE/, where most of the game's files are.");
        if (auto n = byName.find(i); n != byName.end())
            v.notes.push_back(std::to_string(n->second) + (n->second == 1 ? " file came" : " files came") +
                              " without its folder; " + (n->second == 1 ? "it was" : "they were") +
                              " put where the game has a file with the same name.");
        if (exefsVariants.count(i))
            v.notes.push_back("A patch for the game's program: Atmosphère applies it only to the game version it was made "
                              "for.");
        if (prebuild.count(i))
            v.notes.push_back("This package runs a PC script (Prebuild.bat) to prepare some of its files before Aemulus "
                              "builds it, which can't run on the Switch: part of the mod may not work. If the mod has "
                              "another version, try that one.");
        if (v.format == ModFormat::Aemulus && !skippedAemulus.empty())
            v.notes.push_back("Parts that Aemulus merges on a PC (scripts, text, table and sprite patches) were left out; "
                              "the rest of the package is installed.");
        if (v.format == ModFormat::ReloadedPc) {
            if (pcCode) v.notes.push_back("This PC mod includes code for the PC game, which can't run on the Switch; it was left out.");
            if (skippedPc.count("FEmulator"))
                v.notes.push_back("Files Reloaded-II merges while the PC game runs (FEmulator) were left out.");
            v.notes.push_back("Made for the PC version: most PC mods' files work on Switch too, but not all of them.");
        }
        std::string name = b.roots[i];
        auto slash = name.rfind('/');
        v.name = slash == std::string::npos ? name : name.substr(slash + 1);
        if (v.name.empty()) v.name = formatName(v.format);
    }
    bool cpkTool = false;
    for (auto& f : files)
        if (!f.used && (kCpkToolFiles.count(f.lower.back()) ||
                        (ext(f.lower.back()) == ".bat" && f.lower.back().find("cpk") != std::string::npos))) {
            f.used = true;
            cpkTool = true;
        }
    if (cpkTool)
        for (auto& v : b.variants)
            if (v.format == ModFormat::LooseFiles)
                v.notes.push_back("The archive comes with CriWare's tool for building PATCH1.CPK on a PC; "
                                  "it isn't needed, the app builds the CPK itself.");
    std::erase_if(b.variants, [](const PersonaVariant& v) { return v.empty(); });
    out.variants = std::move(b.variants);
    if (out.variants.size() == 1) out.variants[0].name.clear();

    size_t unknown = 0;
    std::string example;
    for (const auto& f : files)
        if (!f.used && !isDoc(f)) {
            if (!unknown) example = f.rel;
            ++unknown;
        }
    if (unknown)
        out.warnings.push_back(std::to_string(unknown) + (unknown == 1 ? " file wasn't" : " files weren't") +
                               " recognized as part of the game and " + (unknown == 1 ? "is" : "are") + " left out (" +
                               example + (unknown > 1 ? ", …)" : ")") +
                               (resolve ? std::string()
                                        : ". Once the app has read the game's own files, it can place more files."));
    const char* kSaveHelp = "copy it with a save manager such as JKSV (and back up your own save first).";
    const char* kEmulator = "made for emulators (Yuzu, Ryujinx): .pchtxt patches don't work on a Switch.";
    if (saveFiles && !out.variants.empty())
        out.warnings.push_back(std::string("The archive also has save data, which isn't installed: ") + kSaveHelp);
    if (emulatorPatch && !out.variants.empty())
        out.warnings.push_back(std::string("Part of this mod is ") + kEmulator);

    if (out.variants.empty()) {
        if (saveFiles)
            out.problem = std::string("This is a save file, not a mod: it goes in the game's save data. To use it, ") +
                          kSaveHelp;
        else if (emulatorPatch)
            out.problem = std::string("This mod is ") + kEmulator;
        else if (pcCode)
            out.problem = "This is a PC code mod (Reloaded-II): it changes the PC game's program, which can't run on the Switch.";
        else if (!skippedAemulus.empty())
            out.problem = "This Aemulus package only has parts that Aemulus merges on a PC (scripts, text or table "
                          "patches), which can't be applied on the console yet.";
        else
            out.problem = "No Persona 5 Royal files were found in this archive.";
    }
    return out;
}

}  // namespace mm
