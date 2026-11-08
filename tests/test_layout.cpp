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

TEST("layout: a PATCH1 folder to pack with cpkmakec (Persona 5's opening mod)") {
    t::TempDir tmp;
    th::write(tmp.path / "P5ogINTRO/PATCH1/EN/MOVIE/MOV000.USM", "og");
    th::write(tmp.path / "P5ogINTRO/PATCH1/EN/MOVIE/MOV050.USM", "og2");
    th::write(tmp.path / "P5ogINTRO/CPK.bat", "cpkmakec PATCH1 PATCH1.CPK -mode=FILENAME");
    th::write(tmp.path / "P5ogINTRO/cpkmakec.exe", "x");
    th::write(tmp.path / "P5ogINTRO/CpkMaker.dll", "x");
    th::write(tmp.path / "P5ogINTRO/CpkBinder.dll", "x");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    auto paths = cpkPaths(l.variants[0]);
    CHECK_EQ(paths.size(), size_t(2));
    CHECK_EQ(paths[0], std::string("EN/MOVIE/MOV000.USM"));
    CHECK_EQ(paths[1], std::string("EN/MOVIE/MOV050.USM"));
    CHECK(l.warnings.empty());  // the PC packing tool isn't reported as unknown files
    CHECK_EQ(l.variants[0].notes.size(), size_t(1));

    t::TempDir tmp2;  // a folder named PATCH1 with the game's PATCH1/ paths inside
    th::write(tmp2.path / "Mod/PATCH1/PATCH1/MOVIE/MOV000.USM", "m");
    th::write(tmp2.path / "Other/PATCH1/MOVIE/MOV000.USM", "m");
    auto l2 = detectPersonaLayout(tmp2.path, Game::P5R);
    CHECK_EQ(l2.variants.size(), size_t(2));
    CHECK_EQ(cpkPaths(l2.variants[0])[0], std::string("PATCH1/MOVIE/MOV000.USM"));
    CHECK_EQ(cpkPaths(l2.variants[1])[0], std::string("PATCH1/MOVIE/MOV000.USM"));
}

TEST("layout: folders named after the PC version's CPKs, SOUND_E, and PC tools in hidden folders") {
    t::TempDir tmp;
    th::write(tmp.path / "Decals 1.0/Package.xml", "<Metadata/>");
    th::write(tmp.path / "Decals 1.0/BASE.CPK/MODEL/CHARACTER/DECAL/MORU.DDS", "d");
    th::write(tmp.path / "Decals 1.0/BASE.CPK/MODEL/CHARACTER/DECAL/MORU.psd", "source");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK_EQ(l.variants.size(), size_t(1));
    auto paths = cpkPaths(l.variants[0]);
    CHECK_EQ(paths.size(), size_t(1));
    CHECK_EQ(paths[0], std::string("BASE/MODEL/CHARACTER/DECAL/MORU.DDS"));
    CHECK(l.variants[0].notes.empty());  // nothing guessed

    t::TempDir loose;
    th::write(loose.path / "Mod/EN.CPK/BATTLE/Y.BIN", "y");
    auto l2 = detectPersonaLayout(loose.path, Game::P5R);
    CHECK_EQ(cpkPaths(l2.variants[0])[0], std::string("EN/BATTLE/Y.BIN"));

    t::TempDir voices;  // English voices are in SOUND_E/; PC tools in a hidden folder, and a script Aemulus runs
    th::write(voices.path / "gay joker/Package.xml", "<Metadata/>");
    th::write(voices.path / "gay joker/SOUND_E/SOUND/EVENT/E718_101.ACB", "v");
    th::write(voices.path / "gay joker/BASE/SOUND/BGM.ACB", "b");
    th::write(voices.path / "gay joker/.derp/SonicAudioTools/AcbEditor.pdb", "x");
    th::write(voices.path / "gay joker/.derp/SonicAudioTools/LICENSE.md", "x");
    th::write(voices.path / "gay joker/Prebuild.bat", "AcbEditor …");
    auto l3 = detectPersonaLayout(voices.path, Game::P5R);
    CHECK_EQ(l3.variants.size(), size_t(1));
    auto p3 = cpkPaths(l3.variants[0]);
    CHECK_EQ(p3.size(), size_t(2));
    CHECK_EQ(p3[0], std::string("BASE/SOUND/BGM.ACB"));
    CHECK_EQ(p3[1], std::string("SOUND_E/SOUND/EVENT/E718_101.ACB"));
    CHECK(hasNote(l3.variants[0], "Prebuild.bat"));
    CHECK(l3.warnings.empty());
}

TEST("layout: more of the game's folders (BUSTUP, CUTIN, SOUND…) are recognized") {
    t::TempDir tmp;
    th::write(tmp.path / "SUMIRE OVERLAY/BUSTUP/B010_000_01.BIN", "b");
    th::write(tmp.path / "SUMIRE OVERLAY/CUTIN/UTARC/0_MAIN_0/CUTINMAIN_0_11_00.000", "c");
    th::write(tmp.path / "SUMIRE OVERLAY/MODEL/CHARACTER/0010/C0010_001_00.GMD", "m");
    th::write(tmp.path / "Music/SOUND/BGM.AWB", "s");
    auto l = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK(l.warnings.empty());
    CHECK_EQ(l.variants.size(), size_t(2));
    std::vector<std::string> all;
    for (const auto& v : l.variants)
        for (const auto& p : cpkPaths(v)) all.push_back(p);
    std::sort(all.begin(), all.end());
    CHECK_EQ(all.size(), size_t(4));
    CHECK_EQ(all[0], std::string("BASE/BUSTUP/B010_000_01.BIN"));
    CHECK_EQ(all[1], std::string("BASE/CUTIN/UTARC/0_MAIN_0/CUTINMAIN_0_11_00.000"));
    CHECK_EQ(all[3], std::string("BASE/SOUND/BGM.AWB"));

    auto withList = detectPersonaLayout(tmp.path, Game::P5R, [](const std::string& p) {
        return p == "BUSTUP/B010_000_01.BIN" ? std::string("EN/BUSTUP/B010_000_01.BIN") : std::string();
    });
    bool found = false;
    for (const auto& v : withList.variants)
        for (const auto& p : cpkPaths(v)) found = found || p == "EN/BUSTUP/B010_000_01.BIN";
    CHECK(found);
}

TEST("layout: save files, exefs patches and emulator patches") {
    t::TempDir save;
    th::write(save.path / "DATA11/DATA.DAT", "s");
    th::write(save.path / "DATA12/DATA.DAT", "s");
    th::write(save.path / "SYSTEM/SYSTEM.DAT", "s");
    auto l = detectPersonaLayout(save.path, Game::P5R);
    CHECK(l.variants.empty());
    CHECK(l.problem.find("save file") != std::string::npos);
    CHECK(l.warnings.empty());

    t::TempDir mixed;
    th::write(mixed.path / "Mod/BASE/A.BIN", "a");
    th::write(mixed.path / "Mod/DATA01/DATA.DAT", "s");
    auto lm = detectPersonaLayout(mixed.path, Game::P5R);
    CHECK_EQ(lm.variants.size(), size_t(1));
    CHECK_EQ(lm.warnings.size(), size_t(1));
    CHECK(lm.warnings[0].find("save data") != std::string::npos);

    t::TempDir ips;
    th::write(ips.path / "Persona 5 Royal - Max Handheld Resolution/"
                         "708BCACA873EA32BD82E00E83A1E26BEA97E7C88000000000000000000000000.ips", "PATCH");
    auto li = detectPersonaLayout(ips.path, Game::P5R);
    CHECK(li.problem.empty());
    CHECK_EQ(li.variants.size(), size_t(1));
    CHECK(li.variants[0].format == ModFormat::Atmosphere);
    CHECK_EQ(li.variants[0].sdFiles.size(), size_t(1));
    CHECK_EQ(li.variants[0].sdFiles[0].target,
             std::string("atmosphere/exefs_patches/Persona-5-Royal-Max-Handheld-Resolution/"
                         "708BCACA873EA32BD82E00E83A1E26BEA97E7C88000000000000000000000000.ips"));
    CHECK(hasNote(li.variants[0], "game version"));

    t::TempDir emu;
    th::write(emu.path / "60fps/exefs/FPS.pchtxt", "@nsobid-…");
    auto le = detectPersonaLayout(emu.path, Game::P5R);
    CHECK(le.variants.empty());
    CHECK(le.problem.find("emulators") != std::string::npos);
    CHECK(le.warnings.empty());
}

TEST("layout: a file without any folder goes where the game has one with its name") {
    t::TempDir tmp;
    th::write(tmp.path / "TWEWYBGMmod/BGM.ACB", "a");
    th::write(tmp.path / "TWEWYBGMmod/BGM.AWB", "b");
    auto none = detectPersonaLayout(tmp.path, Game::P5R);
    CHECK(none.variants.empty());
    CHECK_EQ(none.warnings.size(), size_t(1));
    CHECK(none.warnings[0].find("game's own files") != std::string::npos);

    auto l = detectPersonaLayout(tmp.path, Game::P5R, [](const std::string& p) {
        if (p == "BGM.ACB") return std::string("BASE/SOUND/BGM.ACB");
        if (p == "BGM.AWB") return std::string("BASE/SOUND/BGM.AWB");
        return std::string();
    });
    CHECK(l.problem.empty());
    CHECK(l.warnings.empty());
    CHECK_EQ(l.variants.size(), size_t(1));
    auto paths = cpkPaths(l.variants[0]);
    CHECK_EQ(paths[0], std::string("BASE/SOUND/BGM.ACB"));
    CHECK_EQ(paths[1], std::string("BASE/SOUND/BGM.AWB"));
    CHECK(hasNote(l.variants[0], "same name"));
}
