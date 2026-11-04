#include <algorithm>

#include "helpers.hpp"
#include "persona_layout.hpp"
#include "test.hpp"

using namespace mm;

namespace {
std::vector<std::string> cpkPaths(const PersonaVariant& v) {
    std::vector<std::string> out;
    for (const auto& f : v.files) out.push_back(f.cpkPath);
    std::sort(out.begin(), out.end());
    return out;
}
bool hasNote(const PersonaVariant& v, const std::string& part) {
    for (const auto& n : v.notes)
        if (n.find(part) != std::string::npos) return true;
    return false;
}
}  // namespace

TEST("layout: Aemulus package") {
    t::TempDir tmp;
    th::write(tmp.path / "Colorful Pack/Package.xml", "<Metadata/>");
    th::write(tmp.path / "Colorful Pack/Preview.png", "png");
    th::write(tmp.path / "Colorful Pack/BASE/BATTLE/GUI/x.bin", "x");
    th::write(tmp.path / "Colorful Pack/EN/INIT/y.bmd", "y");
    th::write(tmp.path / "Colorful Pack/BASE/SCRIPT/z.flow", "flow");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK(l.problem.empty());
    CHECK_EQ(l.variants.size(), size_t(1));
    const auto& v = l.variants[0];
    CHECK(v.format == ModFormat::Aemulus);
    auto paths = cpkPaths(v);
    CHECK_EQ(paths.size(), size_t(2));
    CHECK_EQ(paths[0], std::string("BASE/BATTLE/GUI/x.bin"));
    CHECK_EQ(paths[1], std::string("EN/INIT/y.bmd"));
    CHECK(hasNote(v, "Aemulus merges on a PC"));
    CHECK(v.name.empty());
}

TEST("layout: SimpleModManager / Atmosphère folders with a PATCH1.CPK and cheats") {
    t::TempDir tmp;
    th::makeCpk(tmp.path / "mods/Persona 5 Royal/Mod/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK",
                {{"BASE/A.BIN", "a"}, {"EN/B.BMD", "b"}}, tmp.path / "scratch");
    fs::remove_all(tmp.path / "scratch");
    th::write(tmp.path / "mods/Persona 5 Royal/Mod/contents/01005CA01580E000/cheats/0123456789ABCDEF.txt", "[60fps]");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    const auto& v = l.variants[0];
    CHECK(v.format == ModFormat::Atmosphere);
    CHECK_EQ(v.cpks.size(), size_t(1));
    CHECK_EQ(v.cpks[0].files, size_t(2));
    CHECK_EQ(v.sdFiles.size(), size_t(1));
    CHECK_EQ(v.sdFiles[0].target, std::string("atmosphere/contents/01005CA01580E000/cheats/0123456789ABCDEF.txt"));
}

TEST("layout: Aemulus' unpacked output folder and CriPakTools' PATCH1.CPK_unpacked") {
    t::TempDir tmp;
    th::write(tmp.path / "mods/romfs/CPK/PATCH1/BASE/a.bin", "a");
    th::write(tmp.path / "mods/romfs/CPK/PATCH1/PATCH1/MOVIE/MOV000.USM", "m");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    auto paths = cpkPaths(l.variants[0]);
    CHECK_EQ(paths.size(), size_t(2));
    CHECK_EQ(paths[0], std::string("BASE/a.bin"));
    CHECK_EQ(paths[1], std::string("PATCH1/MOVIE/MOV000.USM"));

    t::TempDir tmp2;
    th::write(tmp2.path / "PATCH1.CPK_unpacked/PATCH1/INIT/f.bin", "f");
    auto l2 = detectPersonaLayout(tmp2.path, Game::P5R);
    CHECK_EQ(l2.variants.size(), size_t(1));
    CHECK_EQ(cpkPaths(l2.variants[0])[0], std::string("PATCH1/INIT/f.bin"));
}

TEST("layout: options in separate folders become separate choices") {
    t::TempDir tmp;
    th::write(tmp.path / "Mod/Option A/BASE/x.bin", "a");
    th::write(tmp.path / "Mod/Option B/BASE/x.bin", "b");
    th::write(tmp.path / "Mod/readme.txt", "hi");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(2));
    CHECK_EQ(l.variants[0].name, std::string("Option A"));
    CHECK_EQ(l.variants[1].name, std::string("Option B"));
    CHECK(l.warnings.empty());
}

TEST("layout: files without their CPK folder go to BASE, or where the game has them") {
    t::TempDir tmp;
    th::write(tmp.path / "Cool Mod/BATTLE/q.bin", "q");
    th::write(tmp.path / "Cool Mod/FIELD/r.bin", "r");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    auto paths = cpkPaths(l.variants[0]);
    CHECK_EQ(paths[0], std::string("BASE/BATTLE/q.bin"));
    CHECK(hasNote(l.variants[0], "put in BASE/"));

    auto withList = detectPersonaLayout(tmp.path, Game::P5R, [](const std::string& p) {
        return p == "BATTLE/q.bin" ? std::string("EN/BATTLE/q.bin") : std::string();
    });
    auto paths2 = cpkPaths(withList.variants[0]);
    CHECK_EQ(paths2[0], std::string("BASE/FIELD/r.bin"));
    CHECK_EQ(paths2[1], std::string("EN/BATTLE/q.bin"));
}

TEST("layout: Reloaded-II PC mods are adapted; their code is left out") {
    t::TempDir tmp;
    th::write(tmp.path / "p5r.ui.mod/ModConfig.json", "{}");
    th::write(tmp.path / "p5r.ui.mod/P5REssentials/CPK/Mod.CPK/BATTLE/z.bin", "z");
    th::write(tmp.path / "p5r.ui.mod/P5REssentials/CPK/Mod.CPK/EN/T.BMD", "t");
    th::write(tmp.path / "p5r.ui.mod/p5r.ui.mod.dll", "MZ");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK(l.pcMod);
    CHECK_EQ(l.variants.size(), size_t(1));
    const auto& v = l.variants[0];
    CHECK(v.format == ModFormat::ReloadedPc);
    auto paths = cpkPaths(v);
    CHECK_EQ(paths[0], std::string("BASE/BATTLE/z.bin"));
    CHECK_EQ(paths[1], std::string("EN/T.BMD"));
    CHECK(hasNote(v, "code for the PC game"));

    t::TempDir code;
    th::write(code.path / "m/ModConfig.json", "{}");
    th::write(code.path / "m/m.dll", "MZ");
    auto only = detectPersonaLayout(code.path, Game::P5R);
    CHECK(only.variants.empty());
    CHECK(only.problem.find("PC code mod") != std::string::npos);
}

TEST("layout: p5rcbt bind folders and CPKs") {
    t::TempDir tmp;
    th::write(tmp.path / "p5r/bind/MyMod/BATTLE/w.bin", "w");
    th::makeCpk(tmp.path / "p5r/bind/other.cpk", {{"BASE/O.BIN", "o"}}, tmp.path / "scratch");
    fs::remove_all(tmp.path / "scratch");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    const auto& v = l.variants[0];
    CHECK(v.format == ModFormat::P5rcbtFolder);
    CHECK_EQ(v.cpks.size(), size_t(1));
    CHECK_EQ(cpkPaths(v)[0], std::string("BASE/BATTLE/w.bin"));
}

TEST("layout: nothing usable") {
    t::TempDir tmp;
    th::write(tmp.path / "readme.txt", "hello");
    th::write(tmp.path / "stuff/thing.dat", "?");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK(l.variants.empty());
    CHECK(!l.problem.empty());
    CHECK_EQ(l.warnings.size(), size_t(1));
}
