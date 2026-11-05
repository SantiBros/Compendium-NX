#include "installer.hpp"

#include <algorithm>
#include <stdexcept>

#include "log.hpp"

namespace mm {

PreparedArchive prepareArchive(const fs::path& archive, const fs::path& workDir, Game game, const PathResolver& resolve,
                               const ExtractProgressFn& progress) {
    PreparedArchive out;
    removeTree(workDir);
    fs::create_directories(workDir);
    std::error_code ec;
    if (fs::is_directory(archive, ec)) {
        out.extractDir = archive;  // read in place: nothing to unpack
    } else if (endsWithI(archive.filename().string(), ".cpk")) {
        out.extractDir = workDir / "files";
        copyFileCreatingDirs(archive, out.extractDir / archive.filename());
    } else if (isSupportedArchiveName(archive.filename().string())) {
        out.extractDir = workDir / "files";
        extractArchive(archive, out.extractDir, {}, progress, &out.fileTimes);
        extractNestedArchives(out.extractDir);
    } else {
        throw std::runtime_error("this kind of file can't be installed: " + archive.filename().string());
    }
    out.layout = detectPersonaLayout(out.extractDir, game, resolve);
    const auto& l = out.layout;
    log::info("unpacked " + archive.filename().string() + ": " + std::to_string(l.variants.size()) +
              (l.variants.size() == 1 ? " option" : " options") + (l.pcMod ? " (PC mod)" : ""));
    for (const auto& v : l.variants) {
        size_t inCpks = 0;
        for (const auto& c : v.cpks) inCpks += c.files;
        log::info("  " + (v.name.empty() ? std::string("(main)") : v.name) + ": " + formatName(v.format) + ", " +
                  std::to_string(v.files.size()) + " files, " + std::to_string(v.cpks.size()) + " CPKs (" +
                  std::to_string(inCpks) + " files), " + std::to_string(v.sdFiles.size()) + " SD card files");
        for (const auto& n : v.notes) log::info("    note: " + n);
    }
    for (const auto& w : l.warnings) log::warn("  " + w);
    if (!l.problem.empty()) log::warn("  can't install: " + l.problem);
    return out;
}

InstallSet mergeVariants(const PersonaLayout& layout, const std::vector<size_t>& selected) {
    InstallSet set;
    for (size_t i : selected) {
        if (i >= layout.variants.size()) continue;
        const auto& v = layout.variants[i];
        set.files.insert(set.files.end(), v.files.begin(), v.files.end());
        set.cpks.insert(set.cpks.end(), v.cpks.begin(), v.cpks.end());
        set.sdFiles.insert(set.sdFiles.end(), v.sdFiles.begin(), v.sdFiles.end());
    }
    return set;
}

std::vector<std::string> installNotes(const PersonaLayout& layout, const std::vector<size_t>& selected) {
    std::vector<std::string> notes;
    for (size_t i : selected)
        if (i < layout.variants.size())
            for (const auto& n : layout.variants[i].notes)
                if (std::find(notes.begin(), notes.end(), n) == notes.end()) notes.push_back(n);
    for (const auto& w : layout.warnings) notes.push_back(w);
    return notes;
}

}  // namespace mm
