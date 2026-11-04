#include "compat.hpp"
#include "gamebanana.hpp"
#include "gamefiles.hpp"
#include "helpers.hpp"
#include "p5rcbt.hpp"
#include "smm.hpp"
#include "test.hpp"

using namespace mm;

TEST("p5rcbt: settings file round trip") {
    t::TempDir tmp;
    auto c = p5rcbt::readConfig(tmp.path);
    CHECK(c.uncapFramerate);
    CHECK(!c.pcSettings);
    c.pcSettings = true;
    c.uncapFramerate = false;
    p5rcbt::writeConfig(tmp.path, c);
    auto again = p5rcbt::readConfig(tmp.path);
    CHECK(again.pcSettings);
    CHECK(!again.uncapFramerate);
    CHECK(!p5rcbt::installed(tmp.path, Game::P5R));
    th::write(tmp.path / "atmosphere/contents/01005CA01580E000/exefs/subsdk9", "x");
    th::write(tmp.path / "atmosphere/contents/01005CA01580E000/romfs/skyline/plugins/libp5rcbt.nro", "x");
    CHECK(p5rcbt::installed(tmp.path, Game::P5R));
}

TEST("p5rcbt: GitHub release") {
    auto r = p5rcbt::parseRelease(R"({"tag_name":"v0.1.0","assets":[{"name":"release.zip","size":227111,
        "browser_download_url":"https://github.com/Raytwo/p5rcbt/releases/download/v0.1.0/release.zip"}]})");
    CHECK_EQ(r.tag, std::string("v0.1.0"));
    CHECK_EQ(r.size, int64_t(227111));
    CHECK(r.zipUrl.find("release.zip") != std::string::npos);
}

TEST("game file list: finds a file's CPK folder") {
    t::TempDir tmp;
    GameFileList::save(tmp.path / "files.txt", {"BASE/BATTLE/A.BIN", "EN/BATTLE/A.BIN", "BASE/FIELD/F.BIN"});
    GameFileList list;
    CHECK(list.load(tmp.path / "files.txt"));
    CHECK_EQ(list.size(), size_t(3));
    CHECK_EQ(list.resolve("battle/a.bin"), std::string("EN/BATTLE/A.BIN"));  // English text first
    CHECK_EQ(list.resolve("FIELD/F.BIN"), std::string("BASE/FIELD/F.BIN"));
    CHECK_EQ(list.resolve("NOPE/X"), std::string());
}

TEST("SimpleModManager mods are found on the SD card") {
    t::TempDir tmp;
    th::write(tmp.path / "mods/Persona 5 Royal/HD UI/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK", "x");
    th::write(tmp.path / "mods/Zelda/Other/contents/0100000000010000/romfs/a", "x");
    auto found = smm::findMods(tmp.path, Game::P5R);
    CHECK_EQ(found.size(), size_t(1));
    CHECK_EQ(found[0].name, std::string("HD UI"));
}

TEST("GameBanana: P5R (Switch) list URL and saved responses parse") {
    auto url = gb::Client::listUrl(Game::P5R, gb::Sort::Newest, 1, 20, 0);
    CHECK(url.find("Generic_Game%5D=17354") != std::string::npos);
    auto page = gb::parseModList(th::read(fs::path(MM_FIXTURES) / "gb_list.json"));
    CHECK(!page.mods.empty());
    auto d = gb::parseModDetails(th::read(fs::path(MM_FIXTURES) / "gb_mod.json"), 1);
    CHECK(!d.files.empty());
}

TEST("version checks know P5R's versions") {
    auto v = mentionedGameVersions("Only tested on P5R 1.0.2", Game::P5R, true);
    CHECK_EQ(v.size(), size_t(1));
    if (!v.empty()) CHECK_EQ(v[0], std::string("1.0.2"));
    CHECK(checkCompat("1.0.2", {"1.0.2"}) == Compat::Likely);
    CHECK(checkCompat("1.0.0", {"1.0.2"}) == Compat::Mismatch);
}
