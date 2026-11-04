#pragma once

#include <vector>

#include "archive.hpp"
#include "library.hpp"
#include "persona_layout.hpp"

namespace mm {

struct PreparedArchive {
    fs::path extractDir;
    PersonaLayout layout;
    FileTimes fileTimes;  // of the outer archive's files, relative to extractDir
};

// Unpack a downloaded file into `workDir` (wiped first) and work out where its files go.
// Takes zip / 7z / rar archives, a bare .cpk, or a folder (SimpleModManager's mods, the inbox).
PreparedArchive prepareArchive(const fs::path& archive, const fs::path& workDir, Game game,
                               const PathResolver& resolve = {}, const ExtractProgressFn& progress = {});

// The files of the chosen options, together. Later choices win where they overlap.
InstallSet mergeVariants(const PersonaLayout& layout, const std::vector<size_t>& selected);

// The notes of the chosen options and the layout's warnings, for the mod's record.
std::vector<std::string> installNotes(const PersonaLayout& layout, const std::vector<size_t>& selected);

}  // namespace mm
