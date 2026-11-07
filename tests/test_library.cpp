#include "cpk.hpp"
#include "helpers.hpp"
#include "library.hpp"
#include "test.hpp"

using namespace mm;

namespace {

const std::string kPatch = "atmosphere/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK";

struct Env {
    t::TempDir tmp;
    fs::path sd = tmp.path / "sd";
    fs::path data = tmp.path / "sd/switch/Compendium";
    fs::path src = tmp.path / "src";
    Library lib{sd, data};
    int n = 0;

    InstallSet files(std::vector<std::pair<std::string, std::string>> cpkFiles,
                     std::vector<std::pair<std::string, std::string>> sdFiles = {}) {
        InstallSet s;
        for (auto& [path, data] : cpkFiles) {
            auto f = src / std::to_string(n++);
            th::write(f, data);
            s.files.push_back({f, path});
        }
        for (auto& [target, data] : sdFiles) {
            auto f = src / std::to_string(n++);
            th::write(f, data);
            s.sdFiles.push_back({f, target});
        }
        return s;
    }
    ModRecord rec(const std::string& id) {
        ModRecord r;
        r.id = id;
        r.name = id;
        r.game = Game::P5R;
        return r;
    }
    // Contents of a file inside the built archive.
    std::string inCpk(const std::string& target, const std::string& path) {
        auto archive = sd / target;
        for (const auto& e : cpk::read(archive).entries)
            if (cpk::pathKey(e.path()) == cpk::pathKey(path)) {
                auto out = tmp.path / "peek";
                cpk::extract(archive, e, out);
                return th::read(out);
            }
        return "<missing>";
    }
};

}  // namespace

TEST("library: mods build PATCH1.CPK; the one higher in the list wins") {
    Env env;
    env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", "a1"}, {"BASE/SHARED.BIN", "from a"}}), true);
    CHECK(fs::exists(env.sd / kPatch));
    CHECK_EQ(env.inCpk(kPatch, "BASE/A.BIN"), std::string("a1"));
    CHECK(env.lib.buildStatus(Game::P5R).missingOriginals);

    env.lib.add(env.rec("b"), env.files({{"BASE/SHARED.BIN", "from b"}},
                                        {{"atmosphere/contents/01005CA01580E000/cheats/X.txt", "[cheat]"}}),
                true);
    CHECK_EQ(env.inCpk(kPatch, "BASE/SHARED.BIN"), std::string("from b"));  // newest on top
    CHECK_EQ(th::read(env.sd / "atmosphere/contents/01005CA01580E000/cheats/X.txt"), std::string("[cheat]"));
    auto conflicts = env.lib.conflicts(Game::P5R);
    CHECK_EQ(conflicts.size(), size_t(1));
    CHECK_EQ(conflicts[0].modIds[0], std::string("b"));

    env.lib.move("a", +1);
    CHECK_EQ(env.lib.mods(Game::P5R)[0]->id, std::string("a"));
    CHECK_EQ(env.inCpk(kPatch, "BASE/SHARED.BIN"), std::string("from a"));

    env.lib.setEnabled("a", false);
    env.lib.setEnabled("b", false);
    CHECK(!fs::exists(env.sd / kPatch));
    CHECK(!fs::exists(env.sd / "atmosphere/contents/01005CA01580E000/cheats/X.txt"));
}

TEST("library: the game's own PATCH1 files are kept") {
    Env env;
    th::makeCpk(env.lib.originalPatchCpk(Game::P5R), {{"PATCH1/MOVIE/MOV000.USM", "intro"}}, env.tmp.path / "s");
    env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", "a"}}), true);
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/MOVIE/MOV000.USM"), std::string("intro"));
    CHECK(env.lib.buildStatus(Game::P5R).withOriginals);
    CHECK_EQ(env.lib.buildStatus(Game::P5R).files, size_t(2));
}

TEST("library: a PATCH1.CPK copied by hand is backed up, restored, or imported") {
    Env env;
    th::makeCpk(env.sd / kPatch, {{"BASE/HAND.BIN", "by hand"}}, env.tmp.path / "s");
    CHECK_EQ(env.lib.unmanagedOutput(Game::P5R), kPatch);
    env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", "a"}}), true);
    CHECK_EQ(env.inCpk(kPatch, "BASE/HAND.BIN"), std::string("<missing>"));
    env.lib.setEnabled("a", false);
    CHECK_EQ(env.inCpk(kPatch, "BASE/HAND.BIN"), std::string("by hand"));  // put back

    env.lib.importUnmanagedOutput(Game::P5R, "My old mods");
    env.lib.setEnabled("a", true);
    CHECK_EQ(env.inCpk(kPatch, "BASE/HAND.BIN"), std::string("by hand"));
    CHECK_EQ(env.inCpk(kPatch, "BASE/A.BIN"), std::string("a"));
    CHECK_EQ(env.lib.mods(Game::P5R).back()->name, std::string("My old mods"));  // lowest
    env.lib.setEnabled("a", false);
    env.lib.setEnabled(env.lib.mods(Game::P5R).back()->id, false);
    CHECK(!fs::exists(env.sd / kPatch));  // it's a mod now, so off means gone
}

TEST("library: p5rcbt mode builds into p5r/bind and leaves PATCH1.CPK alone") {
    Env env;
    th::makeCpk(env.lib.originalPatchCpk(Game::P5R), {{"PATCH1/MOVIE/MOV000.USM", "intro"}}, env.tmp.path / "s");
    env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", "a"}}), true);
    env.lib.setMode(Game::P5R, DeployMode::P5rcbt);
    CHECK(!fs::exists(env.sd / kPatch));
    auto bind = Library::outputTarget(Game::P5R, DeployMode::P5rcbt);
    CHECK_EQ(env.inCpk(bind, "BASE/A.BIN"), std::string("a"));
    CHECK_EQ(env.inCpk(bind, "PATCH1/MOVIE/MOV000.USM"), std::string("<missing>"));
    env.lib.setMode(Game::P5R, DeployMode::Patch1);
    CHECK(!fs::exists(env.sd / bind));
    CHECK(fs::exists(env.sd / kPatch));
}

TEST("library: a mod's EN/ or BASE/ file isn't hidden by a PATCH1/ copy below it") {
    // The game reads PATCH1/MOVIE/MOV000.USM before EN/MOVIE/MOV000.USM.
    Env env;
    th::makeCpk(env.lib.originalPatchCpk(Game::P5R), {{"PATCH1/MOVIE/MOV000.USM", "royal"}, {"PATCH1/X.BIN", "x"}},
                env.tmp.path / "s");
    env.lib.add(env.rec("og"), env.files({{"EN/MOVIE/MOV000.USM", "og"}}), true);
    CHECK_EQ(env.inCpk(kPatch, "EN/MOVIE/MOV000.USM"), std::string("og"));
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/MOVIE/MOV000.USM"), std::string("<missing>"));
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/X.BIN"), std::string("x"));  // the rest of the game's files stay

    // A mod above it that brings its own PATCH1/ copy wins, as the load order says.
    env.lib.add(env.rec("top"), env.files({{"PATCH1/MOVIE/MOV000.USM", "top"}}), true);
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/MOVIE/MOV000.USM"), std::string("top"));
    env.lib.move("top", -1);  // now below "og": its PATCH1/ copy would hide og's file
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/MOVIE/MOV000.USM"), std::string("<missing>"));
    CHECK_EQ(env.inCpk(kPatch, "EN/MOVIE/MOV000.USM"), std::string("og"));

    // p5rcbt leaves the game's PATCH1.CPK in place, so the mod's file goes in PATCH1/ too.
    env.lib.setMode(Game::P5R, DeployMode::P5rcbt);
    auto bind = Library::outputTarget(Game::P5R, DeployMode::P5rcbt);
    CHECK_EQ(env.inCpk(bind, "PATCH1/MOVIE/MOV000.USM"), std::string("og"));
    CHECK_EQ(env.inCpk(bind, "EN/MOVIE/MOV000.USM"), std::string("og"));
}

TEST("library: files 0.0.2 filed under PATCH1/EN/… move to EN/… when the list is loaded") {
    Env env;
    env.lib.add(env.rec("og"), env.files({{"PATCH1/EN/MOVIE/MOV000.USM", "og"}, {"BASE/A.BIN", "a"}}), true);
    Library again{env.sd, env.data};
    CHECK(again.load());
    CHECK_EQ(again.pathsFixedOnLoad(), size_t(1));
    CHECK(!again.outdatedBuild());
    again.apply();
    CHECK_EQ(env.inCpk(kPatch, "EN/MOVIE/MOV000.USM"), std::string("og"));
    CHECK_EQ(env.inCpk(kPatch, "PATCH1/EN/MOVIE/MOV000.USM"), std::string("<missing>"));
    CHECK_EQ(env.inCpk(kPatch, "BASE/A.BIN"), std::string("a"));
    Library third{env.sd, env.data};
    CHECK(third.load());
    CHECK_EQ(third.pathsFixedOnLoad(), size_t(0));  // saved
}

TEST("library: state survives a restart; updating a mod replaces its files") {
    Env env;
    env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", "v1"}}, {{"p5r/cfg/x.txt", "1"}}), true);
    Library again(env.sd, env.data);
    CHECK(again.load());
    CHECK_EQ(again.mods(Game::P5R).size(), size_t(1));
    CHECK(again.mods(Game::P5R)[0]->enabled);
    CHECK_EQ(again.buildStatus(Game::P5R).target, kPatch);
    auto rec = env.rec("a");
    rec.installedAt = 2;
    again.add(rec, env.files({{"BASE/A.BIN", "v2"}}), true);
    CHECK_EQ(env.inCpk(kPatch, "BASE/A.BIN"), std::string("v2"));
    CHECK(!fs::exists(env.sd / "p5r/cfg/x.txt"));
    again.remove("a");
    CHECK(again.mods(Game::P5R).empty());
    CHECK(!fs::exists(env.sd / kPatch));
}

TEST("library: SD files that were there before come back") {
    Env env;
    th::write(env.sd / "p5r/cfg/cbt.toml", "mine");
    env.lib.add(env.rec("a"), env.files({}, {{"p5r/cfg/cbt.toml", "mod's"}}), true);
    CHECK_EQ(th::read(env.sd / "p5r/cfg/cbt.toml"), std::string("mod's"));
    env.lib.remove("a");
    CHECK_EQ(th::read(env.sd / "p5r/cfg/cbt.toml"), std::string("mine"));
}

TEST("library: not enough space is caught before writing") {
    Env env;
    env.lib.setFreeSpace([] { return uint64_t(1024); });
    bool threw = false;
    try {
        env.lib.add(env.rec("a"), env.files({{"BASE/A.BIN", std::string(4096, 'x')}}), true);
    } catch (const NotEnoughSpaceError&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(!fs::exists(env.sd / kPatch));
}
