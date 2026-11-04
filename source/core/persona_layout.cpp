#include "persona_layout.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "archive.hpp"
#include "cpk.hpp"

namespace mm {

namespace {

// P5R (Switch) keeps everything in ALL_USEU.CPK and PATCH1.CPK under these top folders; the
// game looks a file up by its full path, so a mod's file must sit under the same one.
const std::set<std::string> kCpkRoots = {"base", "patch1", "sound", "movie", "en", "fr", "it", "de", "es"};

// Folders found right under BASE/ (and the language folders). A mod laid out from one of
// these is missing its CPK folder.
const std::set<std::string> kGameFolders = {
    "aicalc", "battle", "calendar", "camp", "dungeon", "event", "event_data", "facility", "field",
    "font", "init", "minigame", "model", "script", "shop", "system", "title", "texture",
};

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
    for (const auto& p : f.lower)
        if (p == "__macosx" || p == ".git") return true;
    const auto& n = f.lower.back();
    return n == ".ds_store" || n == "thumbs.db" || n == "desktop.ini" || (!n.empty() && n[0] == '.');
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
        f.rel = fs::relative(it->path(), root).generic_string();
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
        bool inSkippedFolder = false;
        for (size_t i = depth; i + 1 < f.lower.size(); ++i)
            if (f.lower[i] == "tblpatches" || f.lower[i] == "binarypatches" || f.lower[i] == "spdpatches" ||
                f.lower[i] == "texture_override")
                inSkippedFolder = true;
        if (inSkippedFolder || e == ".flow" || e == ".msg" || e == ".tblpatch" || e == ".tbp" || e == ".bp") {
            skippedAemulus.insert(inSkippedFolder ? "patch folders" : e);
            continue;
        }
        if (kAemulusSkipExt.count(e) || name.rfind("preview", 0) == 0) continue;
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
            if (unpackedCpk || aemulusOut) {
                rootEnd = i + 1;
                break;
            }
        }
        if (rootEnd == std::string::npos)
            for (size_t i = 0; i + 1 < L.size(); ++i)
                if (kCpkRoots.count(L[i])) {
                    rootEnd = i;
                    break;
                }
        if (rootEnd == std::string::npos) continue;
        auto& v = b.variant("loose", join(f.parts, 0, rootEnd), ModFormat::LooseFiles);
        v.files.push_back({f.abs, join(f.parts, rootEnd, f.parts.size())});
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
                               example + (unknown > 1 ? ", …)" : ")"));

    if (out.variants.empty()) {
        if (pcCode)
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
