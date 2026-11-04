#pragma once

// CRI CPK archives, the containers Persona games keep their files in (ALL_USEU.CPK,
// PATCH1.CPK…). Reading (file table, extraction, CRILAYLA decompression) and writing
// filename-mode archives like CRI's own cpkmakec -mode=FILENAME, which is what Aemulus and
// CriPakTools build for Persona 5 Royal on Switch.
//
// Layout of what build() writes:
//   0x000   "CPK " chunk: the CpkHeader @UTF table, padded to 0x800, "(c)CRI" at its end
//   0x800   "TOC " chunk: the CpkTocInfo @UTF table (one row per file), padded to 0x800
//   ...     file data, each file padded to the archive's alignment (0x800)
// @UTF tables are big-endian; chunk headers are little-endian. TOC FileOffset values are
// relative to the TOC chunk.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "util.hpp"

namespace mm::cpk {

struct Entry {
    std::string dir;          // "BASE/BATTLE" (may be empty)
    std::string name;         // "BTL.PAK"
    uint64_t offset = 0;      // absolute offset of the stored bytes in the .cpk
    uint32_t storedSize = 0;  // bytes in the archive (TOC FileSize)
    uint32_t size = 0;        // bytes once extracted (TOC ExtractSize)
    uint32_t id = 0;
    std::string path() const;  // "BASE/BATTLE/BTL.PAK"
    bool compressed() const { return storedSize != size; }
};

struct Info {
    std::vector<Entry> entries;  // in TOC order
    uint32_t mode = 0;           // CpkMode: 1 = file names only
    uint16_t align = 0;
    bool sorted = false;
    std::string tool;  // Tvers: the program that built it
    uint64_t fileSize = 0;
};

// Read a CPK's file table. Throws std::runtime_error with a message for the user.
Info read(const fs::path& cpk);

// Return false to cancel.
using ProgressFn = std::function<bool(uint64_t done, uint64_t total)>;

// Write one entry's contents to `dest` (CRILAYLA-compressed entries are decompressed).
void extract(const fs::path& cpk, const Entry& e, const fs::path& dest);
// Extract every entry under `destDir`, at its path in the archive. Unsafe paths ("..",
// absolute) are skipped. Returns the paths written, relative to destDir.
std::vector<std::string> extractAll(const fs::path& cpk, const fs::path& destDir, const ProgressFn& progress = {});

bool isCrilayla(const uint8_t* data, size_t size);
// Throws std::runtime_error on corrupt data.
std::vector<uint8_t> decompressCrilayla(const uint8_t* data, size_t size);

// One file of an archive to build.
struct Source {
    std::string path;  // in the archive, '/'-separated: "BASE/BATTLE/BTL.PAK"
    fs::path file;     // a file on disk, stored as is (uncompressed)…
    fs::path fromCpk;  // …or an entry of another CPK, copied verbatim (compressed or not)
    Entry entry;
    bool fromArchive() const { return !fromCpk.empty(); }
};

struct BuildOptions {
    uint16_t align = 0x800;
    std::string tool = "PersonaModManager";
};

struct BuildResult {
    size_t files = 0;
    uint64_t bytes = 0;
};

// Build a filename-mode CPK at `out` (overwritten). Of sources with the same path
// (case-insensitive) the last one is used. Streams the data: nothing is held in memory but
// the tables. Callers write to a temporary name and rename it into place.
BuildResult build(const fs::path& out, std::vector<Source> sources, const BuildOptions& opts = {},
                  const ProgressFn& progress = {});

// Size of the archive build() would write for these sources (for free-space checks).
uint64_t predictSize(std::vector<Source> sources, const BuildOptions& opts = {});

// The order CRI's tools sort TOC entries in: case-insensitive, '\\' as '/'.
int comparePaths(std::string_view a, std::string_view b);

// "BASE/BATTLE/BTL.PAK" -> lower-case key for comparing archive paths.
std::string pathKey(std::string_view path);

}  // namespace mm::cpk
