#include "cpk.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace mm::cpk {

namespace {

constexpr uint64_t kChunkAlign = 0x800;
constexpr uint64_t kHeaderChunkSize = 0x800;

uint64_t alignUp(uint64_t v, uint64_t a) { return a ? (v + a - 1) / a * a : v; }

[[noreturn]] void corrupt(const std::string& what) { throw std::runtime_error("damaged or unsupported CPK file (" + what + ")"); }

uint16_t be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint64_t be64(const uint8_t* p) { return uint64_t(be32(p)) << 32 | be32(p + 4); }
uint32_t le32(const uint8_t* p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }

void putBe16(std::vector<uint8_t>& b, size_t at, uint16_t v) {
    b[at] = uint8_t(v >> 8);
    b[at + 1] = uint8_t(v);
}
void putBe32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + i] = uint8_t(v >> (24 - 8 * i));
}
void putBe64(std::vector<uint8_t>& b, size_t at, uint64_t v) {
    putBe32(b, at, uint32_t(v >> 32));
    putBe32(b, at + 4, uint32_t(v));
}
void putLe32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + i] = uint8_t(v >> (8 * i));
}

// ------------------------------------------------------------------ @UTF tables

enum Type : uint8_t {
    U8 = 0x0, S8 = 0x1, U16 = 0x2, S16 = 0x3, U32 = 0x4, S32 = 0x5, U64 = 0x6, S64 = 0x7,
    F32 = 0x8, F64 = 0x9, Str = 0xA, Data = 0xB, Guid = 0xC,
};
constexpr uint8_t kFlagName = 0x10, kFlagDefault = 0x20, kFlagRow = 0x40;

uint32_t typeSize(uint8_t t) {
    switch (t) {
        case U8: case S8: return 1;
        case U16: case S16: return 2;
        case U32: case S32: case F32: case Str: return 4;
        case U64: case S64: case F64: case Data: return 8;
        case Guid: return 16;
        default: return 0;
    }
}

struct Value {
    enum Kind { None, Int, Text, Bytes } kind = None;
    uint64_t i = 0;
    std::string s;
    uint32_t dataOffset = 0, dataSize = 0;  // Bytes, when read
    bool operator==(const Value& o) const { return kind == o.kind && i == o.i && s == o.s; }
};

Value intValue(uint64_t v) { return {Value::Int, v, {}, 0, 0}; }
Value textValue(std::string v) { return {Value::Text, 0, std::move(v), 0, 0}; }
Value emptyBytes() { return {Value::Bytes, 0, {}, 0, 0}; }

struct Table {
    std::string name;
    std::vector<std::string> columns;
    std::vector<uint8_t> types;
    std::vector<std::vector<Value>> rows;

    int col(std::string_view n) const {
        for (size_t i = 0; i < columns.size(); ++i)
            if (columns[i] == n) return int(i);
        return -1;
    }
    uint64_t num(size_t row, std::string_view n, uint64_t fallback = 0) const {
        int c = col(n);
        if (c < 0 || row >= rows.size() || rows[row][c].kind != Value::Int) return fallback;
        return rows[row][c].i;
    }
    std::string text(size_t row, std::string_view n) const {
        int c = col(n);
        if (c < 0 || row >= rows.size() || rows[row][c].kind != Value::Text) return {};
        return rows[row][c].s;
    }
};

// CRI's table scrambling (used by some games; P5R's archives are plain).
void unscramble(uint8_t* p, size_t n) {
    uint32_t m = 0x655f;
    const uint32_t t = 0x4115;
    for (size_t i = 0; i < n; ++i) {
        p[i] ^= uint8_t(m & 0xff);
        m *= t;
    }
}

Table parseTable(std::vector<uint8_t> buf) {
    if (buf.size() >= 4 && std::memcmp(buf.data(), "@UTF", 4) != 0) unscramble(buf.data(), buf.size());
    if (buf.size() < 0x20 || std::memcmp(buf.data(), "@UTF", 4) != 0) corrupt("no @UTF table");
    const uint8_t* p = buf.data();
    uint64_t size = uint64_t(be32(p + 4)) + 8;
    uint64_t rowsOff = uint64_t(be16(p + 10)) + 8;
    uint64_t stringsOff = uint64_t(be32(p + 12)) + 8;
    uint64_t dataOff = uint64_t(be32(p + 16)) + 8;
    uint32_t nameOff = be32(p + 20);
    uint16_t ncols = be16(p + 24);
    uint16_t rowWidth = be16(p + 26);
    uint32_t nrows = be32(p + 28);
    if (size > buf.size() || rowsOff < 0x20 || rowsOff > stringsOff || stringsOff > dataOff || dataOff > size)
        corrupt("table offsets");
    auto str = [&](uint32_t off) -> std::string {
        uint64_t at = stringsOff + off;
        if (at >= dataOff) corrupt("string offset");
        const char* s = reinterpret_cast<const char*>(p + at);
        size_t len = strnlen(s, size_t(dataOff - at));
        return std::string(s, len);
    };
    auto readValue = [&](uint8_t type, uint64_t at) -> Value {
        if (at + typeSize(type) > size) corrupt("value out of bounds");
        const uint8_t* v = p + at;
        switch (type) {
            case U8: case S8: return intValue(v[0]);
            case U16: case S16: return intValue(be16(v));
            case U32: case S32: case F32: return intValue(be32(v));
            case U64: case S64: case F64: return intValue(be64(v));
            case Str: return textValue(str(be32(v)));
            case Data: {
                Value d = emptyBytes();
                d.dataOffset = be32(v);
                d.dataSize = be32(v + 4);
                return d;
            }
            default: return {};
        }
    };

    Table t;
    t.name = str(nameOff);
    struct ColInfo {
        uint8_t flags, type;
        Value constant;
        uint32_t rowOffset;
    };
    std::vector<ColInfo> info;
    uint64_t pos = 0x20;
    uint32_t rowPos = 0;
    for (uint16_t c = 0; c < ncols; ++c) {
        if (pos + 5 > rowsOff) corrupt("column list");
        uint8_t flags = p[pos] & 0xF0, type = p[pos] & 0x0F;
        if (!typeSize(type)) corrupt("column type");
        t.columns.push_back(str(be32(p + pos + 1)));
        t.types.push_back(type);
        pos += 5;
        ColInfo ci{flags, type, {}, 0};
        if (flags & kFlagDefault) {
            ci.constant = readValue(type, pos);
            pos += typeSize(type);
        }
        if (flags & kFlagRow) {
            ci.rowOffset = rowPos;
            rowPos += typeSize(type);
        }
        info.push_back(ci);
    }
    if (rowsOff + uint64_t(nrows) * rowWidth > stringsOff && nrows) corrupt("rows");
    t.rows.resize(nrows);
    for (uint32_t r = 0; r < nrows; ++r) {
        t.rows[r].resize(ncols);
        for (uint16_t c = 0; c < ncols; ++c) {
            const auto& ci = info[c];
            if (ci.flags & kFlagRow)
                t.rows[r][c] = readValue(ci.type, rowsOff + uint64_t(r) * rowWidth + ci.rowOffset);
            else if (ci.flags & kFlagDefault)
                t.rows[r][c] = ci.constant;
        }
    }
    return t;
}

// Columns marked constant are written once (CRI's "default" storage); the rest per row.
struct OutColumn {
    std::string name;
    uint8_t type;
    bool constant = false;
};

std::vector<uint8_t> buildTable(const std::string& name, const std::vector<OutColumn>& cols,
                                const std::vector<std::vector<Value>>& rows) {
    std::vector<uint8_t> strings;
    std::map<std::string, uint32_t> stringAt;
    auto addString = [&](const std::string& s) -> uint32_t {
        auto it = stringAt.find(s);
        if (it != stringAt.end()) return it->second;
        uint32_t off = uint32_t(strings.size());
        strings.insert(strings.end(), s.begin(), s.end());
        strings.push_back(0);
        stringAt.emplace(s, off);
        return off;
    };
    addString("<NULL>");
    uint32_t nameOff = addString(name);
    std::vector<uint32_t> colNames;
    for (const auto& c : cols) colNames.push_back(addString(c.name));
    for (size_t c = 0; c < cols.size(); ++c)
        if (cols[c].type == Str)
            for (const auto& r : rows) addString(r[c].s);

    uint32_t schema = 0, rowWidth = 0;
    for (const auto& c : cols) {
        schema += 5 + (c.constant ? typeSize(c.type) : 0);
        if (!c.constant) rowWidth += typeSize(c.type);
    }
    uint32_t rowsOff = 0x20 + schema;
    uint32_t stringsOff = rowsOff + rowWidth * uint32_t(rows.size());
    uint32_t total = stringsOff + uint32_t(strings.size());
    uint32_t padded = uint32_t(alignUp(total, 8));
    uint32_t dataOff = padded;  // no variable-length data is written

    std::vector<uint8_t> out(padded, 0);
    std::memcpy(out.data(), "@UTF", 4);
    putBe32(out, 4, padded - 8);
    putBe16(out, 8, 1);
    putBe16(out, 10, uint16_t(rowsOff - 8));
    putBe32(out, 12, stringsOff - 8);
    putBe32(out, 16, dataOff - 8);
    putBe32(out, 20, nameOff);
    putBe16(out, 24, uint16_t(cols.size()));
    putBe16(out, 26, uint16_t(rowWidth));
    putBe32(out, 28, uint32_t(rows.size()));

    auto writeValue = [&](size_t at, uint8_t type, const Value& v) {
        switch (type) {
            case U8: case S8: out[at] = uint8_t(v.i); break;
            case U16: case S16: putBe16(out, at, uint16_t(v.i)); break;
            case U32: case S32: putBe32(out, at, uint32_t(v.i)); break;
            case U64: case S64: putBe64(out, at, v.i); break;
            case Str: putBe32(out, at, stringAt.at(v.s)); break;
            case Data: putBe32(out, at, 0); putBe32(out, at + 4, 0); break;
            default: break;
        }
    };
    size_t pos = 0x20;
    for (size_t c = 0; c < cols.size(); ++c) {
        out[pos] = uint8_t((cols[c].constant ? kFlagName | kFlagDefault : kFlagName | kFlagRow) | cols[c].type);
        putBe32(out, pos + 1, colNames[c]);
        pos += 5;
        if (cols[c].constant) {
            if (!rows.empty()) writeValue(pos, cols[c].type, rows[0][c]);
            pos += typeSize(cols[c].type);
        }
    }
    for (size_t r = 0; r < rows.size(); ++r) {
        size_t at = rowsOff + r * rowWidth;
        for (size_t c = 0; c < cols.size(); ++c) {
            if (cols[c].constant) continue;
            writeValue(at, cols[c].type, rows[r][c]);
            at += typeSize(cols[c].type);
        }
    }
    std::memcpy(out.data() + stringsOff, strings.data(), strings.size());
    return out;
}

// "CPK " / "TOC " chunk: 16-byte header, then the table, padded to `padTo`.
std::vector<uint8_t> wrapChunk(const char* magic, const std::vector<uint8_t>& table, uint64_t padTo) {
    std::vector<uint8_t> chunk(0x10 + table.size(), 0);
    std::memcpy(chunk.data(), magic, 4);
    putLe32(chunk, 4, 0xFF);
    putLe32(chunk, 8, uint32_t(table.size()));
    std::memcpy(chunk.data() + 0x10, table.data(), table.size());
    chunk.resize(size_t(alignUp(chunk.size(), padTo)), 0);
    return chunk;
}

std::vector<uint8_t> readAt(std::ifstream& in, uint64_t offset, uint64_t size) {
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.seekg(std::streamoff(offset));
    if (!in.read(reinterpret_cast<char*>(buf.data()), std::streamsize(size))) corrupt("unexpected end of file");
    return buf;
}

Table readChunk(std::ifstream& in, uint64_t offset, const char* magic, uint64_t fileSize) {
    if (offset + 0x10 > fileSize) corrupt(std::string(magic) + " chunk offset");
    auto head = readAt(in, offset, 0x10);
    if (std::memcmp(head.data(), magic, 4) != 0) corrupt(std::string("no ") + magic + " chunk");
    uint64_t size = le32(head.data() + 8);
    if (size == 0 || offset + 0x10 + size > fileSize) size = std::min<uint64_t>(fileSize - offset - 0x10, 64ull << 20);
    return parseTable(readAt(in, offset + 0x10, size));
}

struct Prepared {
    std::string path, dir, name;
    uint32_t storedSize = 0, size = 0;
    const Source* src = nullptr;
};

std::vector<Prepared> prepare(const std::vector<Source>& sources) {
    // Last source wins for each path.
    std::map<std::string, size_t> byKey;
    for (size_t i = 0; i < sources.size(); ++i) byKey[pathKey(sources[i].path)] = i;
    std::vector<Prepared> out;
    out.reserve(byKey.size());
    for (const auto& [key, i] : byKey) {
        const auto& s = sources[i];
        Prepared p;
        p.path = s.path;
        std::replace(p.path.begin(), p.path.end(), '\\', '/');
        while (!p.path.empty() && p.path.front() == '/') p.path.erase(0, 1);
        if (p.path.empty() || !isSafeRelativePath(p.path)) throw std::runtime_error("bad file path for a CPK: " + s.path);
        auto slash = p.path.rfind('/');
        p.dir = slash == std::string::npos ? std::string() : p.path.substr(0, slash);
        p.name = slash == std::string::npos ? p.path : p.path.substr(slash + 1);
        if (s.fromArchive()) {
            p.storedSize = s.entry.storedSize;
            p.size = s.entry.size;
        } else {
            std::error_code ec;
            auto sz = fs::file_size(s.file, ec);
            if (ec) throw std::runtime_error("can't read " + s.file.string());
            if (sz > 0xFFFFFFFFull) throw std::runtime_error(p.path + " is too big for a CPK (over 4 GB)");
            p.storedSize = p.size = uint32_t(sz);
        }
        p.src = &s;
        out.push_back(std::move(p));
    }
    std::sort(out.begin(), out.end(), [](const Prepared& a, const Prepared& b) { return comparePaths(a.path, b.path) < 0; });
    return out;
}

std::vector<uint8_t> tocTable(const std::vector<Prepared>& files, const std::vector<uint64_t>& offsets) {
    std::vector<OutColumn> cols = {
        {"DirName", Str}, {"FileName", Str}, {"FileSize", U32}, {"ExtractSize", U32},
        {"FileOffset", U64}, {"ID", U32}, {"UserString", Str, true},
    };
    std::vector<std::vector<Value>> rows;
    rows.reserve(files.size());
    for (size_t i = 0; i < files.size(); ++i)
        rows.push_back({textValue(files[i].dir), textValue(files[i].name), intValue(files[i].storedSize),
                        intValue(files[i].size), intValue(offsets[i]), intValue(i), textValue("<NULL>")});
    return buildTable("CpkTocInfo", cols, rows);
}

struct Layout {
    std::vector<uint8_t> tocChunk;
    uint64_t contentOffset = 0, contentSize = 0, fileSize = 0;
    std::vector<uint64_t> offsets;  // TOC FileOffset per file (relative to the TOC chunk)
};

Layout layout(const std::vector<Prepared>& files, uint16_t align) {
    Layout l;
    l.offsets.assign(files.size(), 0);
    // The table's size doesn't depend on the offsets (fixed-width column), so one sizing pass.
    l.tocChunk = wrapChunk("TOC ", tocTable(files, l.offsets), kChunkAlign);
    l.contentOffset = kHeaderChunkSize + l.tocChunk.size();
    uint64_t at = l.contentOffset;
    for (size_t i = 0; i < files.size(); ++i) {
        l.offsets[i] = at - kHeaderChunkSize;
        at += alignUp(files[i].storedSize, align);
    }
    l.contentSize = at - l.contentOffset;
    l.fileSize = at;
    l.tocChunk = wrapChunk("TOC ", tocTable(files, l.offsets), kChunkAlign);
    if (kHeaderChunkSize + l.tocChunk.size() != l.contentOffset) throw std::logic_error("CPK table size changed");
    return l;
}

std::vector<uint8_t> headerChunk(const std::vector<Prepared>& files, const Layout& l, const BuildOptions& o) {
    uint64_t packed = 0, data = 0;
    std::set<std::string> dirs;
    for (const auto& f : files) {
        packed += f.storedSize;
        data += f.size;
        if (!f.dir.empty()) dirs.insert(f.dir);
    }
    std::vector<OutColumn> cols = {
        {"UpdateDateTime", U64}, {"FileSize", U64}, {"ContentOffset", U64}, {"ContentSize", U64},
        {"TocOffset", U64}, {"TocSize", U64}, {"TocCrc", U32}, {"HtocOffset", U64}, {"HtocSize", U64},
        {"EtocOffset", U64}, {"EtocSize", U64}, {"ItocOffset", U64}, {"ItocSize", U64}, {"ItocCrc", U32},
        {"GtocOffset", U64}, {"GtocSize", U64}, {"GtocCrc", U32}, {"HgtocOffset", U64}, {"HgtocSize", U64},
        {"EnabledPackedSize", U64}, {"EnabledDataSize", U64}, {"TotalDataSize", U64}, {"Tocs", U32},
        {"Files", U32}, {"Groups", U32}, {"Attrs", U32}, {"TotalFiles", U32}, {"Directories", U32},
        {"Updates", U32}, {"Version", U16}, {"Revision", U16}, {"Align", U16}, {"Sorted", U16},
        {"EnableFileName", U16}, {"EID", U16}, {"CpkMode", U32}, {"Tvers", Str}, {"Comment", Str},
        {"Codec", U32}, {"DpkItoc", U32}, {"EnableTocCrc", U16}, {"EnableFileCrc", U16}, {"CrcMode", U32},
        {"CrcTable", Data},
    };
    std::vector<Value> row = {
        intValue(0), intValue(l.fileSize), intValue(l.contentOffset), intValue(l.contentSize),
        intValue(kHeaderChunkSize), intValue(l.tocChunk.size()), intValue(0), intValue(0), intValue(0),
        intValue(0), intValue(0), intValue(0), intValue(0), intValue(0),
        intValue(0), intValue(0), intValue(0), intValue(0), intValue(0),
        intValue(packed), intValue(data), intValue(l.contentSize), intValue(1),
        intValue(files.size()), intValue(0), intValue(0), intValue(files.size()), intValue(dirs.size()),
        intValue(0), intValue(7), intValue(14), intValue(o.align), intValue(1),
        intValue(1), intValue(0), intValue(1), textValue(o.tool), textValue(""),
        intValue(0), intValue(0), intValue(0), intValue(0), intValue(0),
        emptyBytes(),
    };
    auto chunk = wrapChunk("CPK ", buildTable("CpkHeader", cols, {row}), kChunkAlign);
    if (chunk.size() != kHeaderChunkSize) throw std::logic_error("CPK header too big");
    std::memcpy(chunk.data() + kHeaderChunkSize - 6, "(c)CRI", 6);
    return chunk;
}

void copyRange(std::ifstream& in, uint64_t offset, uint64_t size, std::ofstream& out, std::vector<char>& buf,
               const std::function<void(uint64_t)>& advanced) {
    in.seekg(std::streamoff(offset));
    while (size) {
        auto n = size_t(std::min<uint64_t>(size, buf.size()));
        if (!in.read(buf.data(), std::streamsize(n))) throw std::runtime_error("read error");
        if (!out.write(buf.data(), std::streamsize(n))) throw std::runtime_error("write error (is the SD card full?)");
        size -= n;
        if (advanced) advanced(n);
    }
}

}  // namespace

std::string Entry::path() const { return dir.empty() ? name : dir + "/" + name; }

int comparePaths(std::string_view a, std::string_view b) {
    auto norm = [](char c) { return c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c))); };
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        char x = norm(a[i]), y = norm(b[i]);
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

std::string pathKey(std::string_view path) {
    std::string k(path);
    std::replace(k.begin(), k.end(), '\\', '/');
    while (!k.empty() && k.front() == '/') k.erase(0, 1);
    return toLower(k);
}

Info read(const fs::path& cpk) {
    std::ifstream in(cpk, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + cpk.filename().string());
    std::error_code ec;
    uint64_t fileSize = fs::file_size(cpk, ec);
    if (ec || fileSize < 0x10) corrupt("too small");
    auto header = readChunk(in, 0, "CPK ", fileSize);
    if (header.rows.empty()) corrupt("empty header");

    Info info;
    info.fileSize = fileSize;
    info.mode = uint32_t(header.num(0, "CpkMode"));
    info.align = uint16_t(header.num(0, "Align"));
    info.sorted = header.num(0, "Sorted") != 0;
    info.tool = header.text(0, "Tvers");
    uint64_t tocOffset = header.num(0, "TocOffset");
    uint64_t contentOffset = header.num(0, "ContentOffset");
    if (!tocOffset)
        throw std::runtime_error("this CPK has no file names (it lists files by number only), so its files can't be used");
    uint64_t base = contentOffset ? std::min(tocOffset, contentOffset) : tocOffset;

    auto toc = readChunk(in, tocOffset, "TOC ", fileSize);
    info.entries.reserve(toc.rows.size());
    for (size_t r = 0; r < toc.rows.size(); ++r) {
        Entry e;
        e.dir = toc.text(r, "DirName");
        e.name = toc.text(r, "FileName");
        if (e.dir == "<NULL>") e.dir.clear();
        e.storedSize = uint32_t(toc.num(r, "FileSize"));
        e.size = uint32_t(toc.num(r, "ExtractSize", e.storedSize));
        e.offset = toc.num(r, "FileOffset") + base;
        e.id = uint32_t(toc.num(r, "ID", r));
        if (e.name.empty() || e.offset + e.storedSize > fileSize) corrupt("file table entry " + std::to_string(r));
        info.entries.push_back(std::move(e));
    }
    return info;
}

bool isCrilayla(const uint8_t* data, size_t size) { return size >= 0x10 && std::memcmp(data, "CRILAYLA", 8) == 0; }

std::vector<uint8_t> decompressCrilayla(const uint8_t* in, size_t inSize) {
    if (!isCrilayla(in, inSize)) throw std::runtime_error("not CRILAYLA data");
    const uint32_t uncompressedSize = le32(in + 8);
    const uint32_t headerOffset = le32(in + 12);
    // Layout: 16-byte header, compressed stream (headerOffset bytes, read backwards from its
    // end), then the first 0x100 bytes of the file stored as they are.
    if (uint64_t(headerOffset) + 0x10 + 0x100 > inSize) corrupt("CRILAYLA header");
    std::vector<uint8_t> out(size_t(uncompressedSize) + 0x100);
    std::memcpy(out.data(), in + 0x10 + headerOffset, 0x100);

    int64_t inPos = int64_t(0x10) + headerOffset - 1;
    const int64_t inStart = 0x10;
    uint8_t bitPool = 0;
    int bitsLeft = 0;
    auto bits = [&](int count) -> uint32_t {
        uint32_t v = 0;
        int produced = 0;
        while (produced < count) {
            if (bitsLeft == 0) {
                if (inPos < inStart) corrupt("CRILAYLA stream");
                bitPool = in[inPos--];
                bitsLeft = 8;
            }
            int take = std::min(bitsLeft, count - produced);
            v = (v << take) | ((bitPool >> (bitsLeft - take)) & ((1u << take) - 1));
            bitsLeft -= take;
            produced += take;
        }
        return v;
    };

    const int64_t outEnd = int64_t(0x100) + uncompressedSize - 1;
    uint64_t written = 0;
    static constexpr int vle[4] = {2, 3, 5, 8};
    while (written < uncompressedSize) {
        if (bits(1)) {
            int64_t ref = outEnd - int64_t(written) + int64_t(bits(13)) + 3;
            uint64_t length = 3;
            int level = 0;
            for (; level < 4; ++level) {
                uint32_t part = bits(vle[level]);
                length += part;
                if (part != (1u << vle[level]) - 1) break;
            }
            if (level == 4) {
                uint32_t part;
                do {
                    part = bits(8);
                    length += part;
                } while (part == 0xFF);
            }
            for (uint64_t i = 0; i < length && written < uncompressedSize; ++i) {
                if (ref > outEnd || ref < 0) corrupt("CRILAYLA reference");
                out[size_t(outEnd - int64_t(written))] = out[size_t(ref--)];
                ++written;
            }
        } else {
            out[size_t(outEnd - int64_t(written))] = uint8_t(bits(8));
            ++written;
        }
    }
    return out;
}

void extract(const fs::path& cpk, const Entry& e, const fs::path& dest) {
    std::ifstream in(cpk, std::ios::binary);
    if (!in) throw std::runtime_error("can't open " + cpk.filename().string());
    fs::create_directories(dest.parent_path());
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("can't write " + dest.string());
    if (e.compressed() && e.storedSize >= 0x10) {
        auto head = readAt(in, e.offset, 8);
        if (std::memcmp(head.data(), "CRILAYLA", 8) == 0) {
            auto stored = readAt(in, e.offset, e.storedSize);
            auto data = decompressCrilayla(stored.data(), stored.size());
            if (!out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size())))
                throw std::runtime_error("write error (is the SD card full?)");
            return;
        }
    }
    std::vector<char> buf(1 << 20);
    copyRange(in, e.offset, e.storedSize, out, buf, {});
}

std::vector<std::string> extractAll(const fs::path& cpk, const fs::path& destDir, const ProgressFn& progress) {
    auto info = read(cpk);
    std::vector<std::string> written;
    uint64_t total = 0, done = 0;
    for (const auto& e : info.entries) total += e.storedSize;
    for (const auto& e : info.entries) {
        auto path = e.path();
        if (!isSafeRelativePath(path)) continue;
        extract(cpk, e, destDir / fs::path(path));
        written.push_back(path);
        done += e.storedSize;
        if (progress && !progress(done, total)) throw std::runtime_error("cancelled");
    }
    return written;
}

uint64_t predictSize(std::vector<Source> sources, const BuildOptions& opts) {
    auto files = prepare(sources);
    return layout(files, opts.align).fileSize;
}

BuildResult build(const fs::path& out, std::vector<Source> sources, const BuildOptions& opts, const ProgressFn& progress) {
    auto files = prepare(sources);
    auto l = layout(files, opts.align);
    auto header = headerChunk(files, l, opts);

    if (out.has_parent_path()) fs::create_directories(out.parent_path());
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    if (!o) throw std::runtime_error("can't write " + out.string());
    auto put = [&](const void* p, size_t n) {
        if (!o.write(static_cast<const char*>(p), std::streamsize(n)))
            throw std::runtime_error("write error (is the SD card full?)");
    };
    put(header.data(), header.size());
    put(l.tocChunk.data(), l.tocChunk.size());

    uint64_t total = 0, done = 0;
    for (const auto& f : files) total += f.storedSize;
    std::vector<char> buf(1 << 20);
    std::vector<char> zeros(opts.align ? opts.align : 1, 0);
    auto advanced = [&](uint64_t n) {
        done += n;
        if (progress && !progress(done, total)) throw std::runtime_error("cancelled");
    };
    // Keep the last-used archive open: original-file sources come from one CPK in a row.
    std::unique_ptr<std::ifstream> archive;
    fs::path archivePath;
    for (const auto& f : files) {
        const Source& s = *f.src;
        if (s.fromArchive()) {
            if (!archive || archivePath != s.fromCpk) {
                archive = std::make_unique<std::ifstream>(s.fromCpk, std::ios::binary);
                archivePath = s.fromCpk;
                if (!*archive) throw std::runtime_error("can't open " + s.fromCpk.string());
            }
            copyRange(*archive, s.entry.offset, f.storedSize, o, buf, advanced);
        } else {
            std::ifstream in(s.file, std::ios::binary);
            if (!in) throw std::runtime_error("can't open " + s.file.string());
            copyRange(in, 0, f.storedSize, o, buf, advanced);
        }
        uint64_t pad = alignUp(f.storedSize, opts.align) - f.storedSize;
        if (pad) put(zeros.data(), size_t(pad));
    }
    o.close();
    if (!o) throw std::runtime_error("write error (is the SD card full?)");
    return {files.size(), l.fileSize};
}

}  // namespace mm::cpk
