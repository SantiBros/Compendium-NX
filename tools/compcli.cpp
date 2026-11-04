// compcli: the manager's core on a computer, for testing mods and archives.
//
//   compcli list <file.cpk>                 files in a CPK
//   compcli extract <file.cpk> <folder>     unpack a CPK
//   compcli pack <folder> <out.cpk>         pack a folder (its paths become the CPK's)
//   compcli detect <archive|folder>         what a mod archive holds and where it would go
//   compcli install <sd root> <archive>     install into a fake SD card (all options, on)
//   compcli status <sd root>                the mods installed there and the built archive

#include <iostream>

#include "cpk.hpp"
#include "installer.hpp"
#include "library.hpp"

using namespace mm;

static int usage() {
    std::cerr << "usage: compcli list|extract|pack|detect|install|status …\n";
    return 2;
}

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    std::string cmd = argv[1];
    try {
        if (cmd == "list") {
            auto info = cpk::read(argv[2]);
            std::cout << info.entries.size() << " files, mode " << info.mode << ", align " << info.align << ", built by "
                      << (info.tool.empty() ? "?" : info.tool) << "\n";
            for (const auto& e : info.entries)
                std::cout << e.path() << "  " << e.size << (e.compressed() ? " (compressed " + std::to_string(e.storedSize) + ")" : "")
                          << "\n";
        } else if (cmd == "extract" && argc >= 4) {
            auto files = cpk::extractAll(argv[2], argv[3]);
            std::cout << files.size() << " files\n";
        } else if (cmd == "pack" && argc >= 4) {
            std::vector<cpk::Source> src;
            fs::path root = argv[2];
            for (const auto& e : fs::recursive_directory_iterator(root))
                if (e.is_regular_file()) src.push_back({fs::relative(e.path(), root).generic_string(), e.path(), {}, {}});
            auto r = cpk::build(argv[3], src);
            std::cout << r.files << " files, " << formatSize(r.bytes) << "\n";
        } else if (cmd == "detect") {
            auto work = fs::temp_directory_path() / "compcli-detect";
            auto prep = prepareArchive(argv[2], work, Game::P5R);
            const auto& l = prep.layout;
            if (!l.problem.empty()) std::cout << "can't install: " << l.problem << "\n";
            for (const auto& w : l.warnings) std::cout << "warning: " << w << "\n";
            for (const auto& v : l.variants) {
                std::cout << "option \"" << v.name << "\" (" << formatName(v.format) << "): " << v.files.size()
                          << " files, " << v.cpks.size() << " CPKs, " << v.sdFiles.size() << " SD card files\n";
                for (const auto& f : v.files) std::cout << "  cpk  " << f.cpkPath << "\n";
                for (const auto& c : v.cpks) std::cout << "  cpk  " << c.cpk.filename().string() << " (" << c.files << " files)\n";
                for (const auto& s : v.sdFiles) std::cout << "  sd   " << s.target << "\n";
                for (const auto& n : v.notes) std::cout << "  note " << n << "\n";
            }
            removeTree(work);
        } else if (cmd == "install" && argc >= 4) {
            fs::path sd = argv[2];
            Library lib(sd, sd / "switch/Compendium");
            lib.load();
            auto work = sd / "switch/Compendium/work/cli";
            auto prep = prepareArchive(argv[3], work, Game::P5R);
            if (prep.layout.variants.empty()) throw std::runtime_error(prep.layout.problem);
            std::vector<size_t> all;
            for (size_t i = 0; i < prep.layout.variants.size(); ++i) all.push_back(i);
            ModRecord rec;
            rec.id = "local-" + slugify(fs::path(argv[3]).stem().string());
            rec.name = fs::path(argv[3]).stem().string();
            rec.notes = installNotes(prep.layout, all);
            rec.format = formatName(prep.layout.variants[0].format);
            lib.add(rec, mergeVariants(prep.layout, all), true);
            removeTree(work);
            std::cout << "installed " << rec.id << "\n";
        } else if (cmd == "status") {
            fs::path sd = argv[2];
            Library lib(sd, sd / "switch/Compendium");
            lib.load();
            for (const auto* m : lib.mods(Game::P5R))
                std::cout << (m->enabled ? "[on]  " : "[off] ") << m->priority << "  " << m->name << "  (" << m->cpkFiles.size()
                          << " files, " << m->bundles.size() << " CPKs)\n";
            const auto& s = lib.buildStatus(Game::P5R);
            std::cout << "built: " << (s.target.empty() ? "nothing" : s.target) << ", " << s.files << " files, "
                      << formatSize(s.bytes) << (s.missingOriginals ? " (without the game's own PATCH1 files)" : "") << "\n";
            for (const auto& c : lib.conflicts(Game::P5R)) std::cout << "conflict: " << c.path << " (" << c.modIds[0] << " wins)\n";
        } else {
            return usage();
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
