#include <fstream>

#include "cpk.hpp"
#include "helpers.hpp"
#include "test.hpp"

using namespace mm;

TEST("cpk: build and read back, sorted and aligned") {
    t::TempDir tmp;
    auto out = tmp.path / "PATCH1.CPK";
    th::makeCpk(out,
                {{"PATCH1/MOVIE/MOV000.USM", std::string(5000, 'm')},
                 {"BASE/BATTLE/TABLE/UNIT.TBL", "unit"},
                 {"base/battle/a.bin", "aaa"},
                 {"EN/INIT/x.bmd", ""}},
                tmp.path);
    auto info = cpk::read(out);
    CHECK_EQ(info.entries.size(), size_t(4));
    CHECK_EQ(info.mode, 1u);
    CHECK_EQ(info.align, 0x800);
    CHECK(info.sorted);
    // Case-insensitive order: base/battle/a.bin < BASE/BATTLE/TABLE/… < EN/… < PATCH1/…
    CHECK_EQ(info.entries[0].path(), std::string("base/battle/a.bin"));
    CHECK_EQ(info.entries[1].path(), std::string("BASE/BATTLE/TABLE/UNIT.TBL"));
    CHECK_EQ(info.entries[2].path(), std::string("EN/INIT/x.bmd"));
    CHECK_EQ(info.entries[3].path(), std::string("PATCH1/MOVIE/MOV000.USM"));
    for (const auto& e : info.entries) {
        CHECK_EQ(e.offset % 0x800, uint64_t(0));
        CHECK(!e.compressed());
    }
    CHECK_EQ(info.entries[3].size, 5000u);
    CHECK_EQ(info.fileSize, uint64_t(fs::file_size(out)));
    // The header chunk ends with CRI's signature, like cpkmakec's.
    auto raw = th::read(out);
    CHECK_EQ(raw.substr(0, 4), std::string("CPK "));
    CHECK_EQ(raw.substr(0x7FA, 6), std::string("(c)CRI"));
    CHECK_EQ(raw.substr(0x800, 4), std::string("TOC "));

    auto dir = tmp.path / "x";
    auto written = cpk::extractAll(out, dir);
    CHECK_EQ(written.size(), size_t(4));
    CHECK_EQ(th::read(dir / "BASE/BATTLE/TABLE/UNIT.TBL"), std::string("unit"));
    CHECK_EQ(th::read(dir / "PATCH1/MOVIE/MOV000.USM"), std::string(5000, 'm'));
    CHECK_EQ(th::read(dir / "EN/INIT/x.bmd"), std::string());
}

TEST("cpk: the last source for a path wins, whatever its case") {
    t::TempDir tmp;
    th::write(tmp.path / "a", "first");
    th::write(tmp.path / "b", "second");
    auto out = tmp.path / "o.cpk";
    auto r = cpk::build(out, {{"BASE/X.BIN", tmp.path / "a", {}, {}}, {"base/x.bin", tmp.path / "b", {}, {}}});
    CHECK_EQ(r.files, size_t(1));
    auto info = cpk::read(out);
    CHECK_EQ(info.entries.size(), size_t(1));
    cpk::extract(out, info.entries[0], tmp.path / "got");
    CHECK_EQ(th::read(tmp.path / "got"), std::string("second"));
    CHECK_EQ(cpk::predictSize({{"BASE/X.BIN", tmp.path / "a", {}, {}}}), uint64_t(fs::file_size(out)));
}

TEST("cpk: entries are copied from another archive verbatim") {
    t::TempDir tmp;
    auto first = tmp.path / "first.cpk";
    th::makeCpk(first, {{"PATCH1/MOVIE/MOV000.USM", "movie"}, {"BASE/A.BIN", "a"}}, tmp.path);
    auto info = cpk::read(first);
    th::write(tmp.path / "mod", "modded");
    std::vector<cpk::Source> src;
    for (const auto& e : info.entries) src.push_back({e.path(), {}, first, e});
    src.push_back({"BASE/A.BIN", tmp.path / "mod", {}, {}});
    auto out = tmp.path / "out.cpk";
    cpk::build(out, src);
    auto dir = tmp.path / "x";
    cpk::extractAll(out, dir);
    CHECK_EQ(th::read(dir / "PATCH1/MOVIE/MOV000.USM"), std::string("movie"));
    CHECK_EQ(th::read(dir / "BASE/A.BIN"), std::string("modded"));
}

TEST("cpk: CRILAYLA entries decompress") {
    std::string file(0x100, 'H');
    for (int i = 0; i < 4000; ++i) file += char("persona5royal"[i % 13] + (i / 997));
    file += std::string(700, 'z');
    auto packed = th::crilayla(file);
    CHECK(packed.size() < file.size());
    CHECK(cpk::isCrilayla(packed.data(), packed.size()));
    auto out = cpk::decompressCrilayla(packed.data(), packed.size());
    CHECK_EQ(std::string(out.begin(), out.end()), file);
}

TEST("cpk: compressed entries are extracted decompressed and copied as stored") {
    t::TempDir tmp;
    std::string file(0x100, 'p');
    for (int i = 0; i < 3000; ++i) file += char('a' + i % 7);
    auto packed = th::crilayla(file);
    th::write(tmp.path / "packed", std::string(packed.begin(), packed.end()));
    // A source CPK whose entry claims a larger extracted size, as CRI's compressed entries do.
    auto first = tmp.path / "first.cpk";
    cpk::build(first, {{"BASE/C.BIN", tmp.path / "packed", {}, {}}});
    auto info = cpk::read(first);
    auto e = info.entries[0];
    e.size = uint32_t(file.size());
    auto out = tmp.path / "out.cpk";
    cpk::build(out, {{"BASE/C.BIN", {}, first, e}});
    auto again = cpk::read(out);
    CHECK_EQ(again.entries[0].storedSize, uint32_t(packed.size()));
    CHECK_EQ(again.entries[0].size, uint32_t(file.size()));
    cpk::extract(out, again.entries[0], tmp.path / "got");
    CHECK_EQ(th::read(tmp.path / "got"), file);
}

TEST("cpk: damaged files are refused with a message") {
    t::TempDir tmp;
    th::write(tmp.path / "bad.cpk", std::string(100, 'x'));
    bool threw = false;
    try {
        cpk::read(tmp.path / "bad.cpk");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}
