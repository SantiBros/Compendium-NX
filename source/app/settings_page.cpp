#include "settings_page.hpp"

#include <ctime>

#include "app_state.hpp"
#include "debug_script.hpp"
#include "p5rcbt.hpp"
#include "platform.hpp"
#include "sysmodules.hpp"
#include "ui.hpp"

namespace app {

namespace {

uint64_t dirSize(const mm::fs::path& dir) {
    uint64_t total = 0;
    std::error_code ec;
    for (mm::fs::recursive_directory_iterator it(dir, ec), end; it != end; it.increment(ec))
        if (it->is_regular_file(ec)) total += it->file_size(ec);
    return total;
}

std::string modeName(mm::DeployMode m) {
    return m == mm::DeployMode::P5rcbt ? "p5rcbt (Skyline plugin)" : "PATCH1.CPK (no plugin)";
}

}  // namespace

SettingsPage::SettingsPage() : brls::Box(brls::Axis::COLUMN) {
    setGrow(1);
    auto* content = ui::vbox();
    content->setPadding(24, 60, 24, 40);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setContentView(content);
    addView(scroll);
    scroll_ = scroll;

    auto section = [content](const std::string& title) {
        auto* l = ui::label(title, 16, ui::muted());
        l->setMargins(30, 0, 8, 4);
        content->addView(l);
        return l;
    };
    auto note = [content](const std::string& text, NVGcolor color = ui::muted()) {
        auto* l = ui::label(text, 14, color, false);
        l->setMargins(10, 4, 0, 4);
        content->addView(l);
        return l;
    };

    auto& st = AppState::get();
    const auto game = st.game();
    const auto& snap = st.snapshot();
    const auto mode = snap.modes.count(game) ? snap.modes.at(game) : mm::DeployMode::Patch1;
    const auto build = snap.builds.count(game) ? snap.builds.at(game) : mm::BuildStatus{};
    const auto& gameName = mm::gameInfo(game).displayName;

    // ---------------------------------------------------------------- how mods are loaded
    section("How mods reach the game · " + std::string(gameName));
    auto* modeCell = new brls::DetailCell();
    modeCell->setText("Load mods with");
    modeCell->setDetailText(modeName(mode));
    modeCell->registerClickAction([this, modeCell, game](brls::View*) {
        const auto& modes = AppState::get().snapshot().modes;
        auto current = modes.count(game) ? modes.at(game) : mm::DeployMode::Patch1;
        auto next = current == mm::DeployMode::Patch1 ? mm::DeployMode::P5rcbt : mm::DeployMode::Patch1;
        std::string text = next == mm::DeployMode::P5rcbt
                               ? "Load mods with p5rcbt? Your mods are packed into p5r/bind/Compendium.cpk, which the p5rcbt "
                                 "plugin adds to the game's files, and the game's own PATCH1.CPK stays as it is.\n\n"
                                 "p5rcbt (a Skyline plugin) has to be installed (below) and only works with game version " +
                                     std::string(mm::p5rcbt::kRequiredGameVersion) + "."
                               : "Load mods with PATCH1.CPK? Your mods are packed into the game's PATCH1.CPK (in "
                                 "atmosphere/contents), which Atmosphère loads by itself. Works on any game version, no plugin needed.";
        auto* dialog = new brls::Dialog(text);
        dialog->addButton("Cancel", [] {});
        dialog->addButton("Switch", [this, modeCell, next, game] {
            runJob(modeCell, "Moving your mods…",
                   [next, game](mm::Library& lib, auto progress) {
                       lib.setMode(game, next, [progress](uint64_t d, uint64_t t, const std::string&) { progress(d, t); });
                   },
                   "Mods now load with " + modeName(next), [modeCell, next] { modeCell->setDetailText(modeName(next)); });
        });
        dialog->open();
        return true;
    });
    content->addView(modeCell);
    if (!build.target.empty())
        note("Built: " + build.target + " · " + std::to_string(build.files) + " files · " + formatSize(build.bytes));
    else
        note("Nothing built yet: turn on a mod in My mods.");
    if (mode == mm::DeployMode::P5rcbt && !snap.p5rcbtInstalled)
        note(std::string(ui::icon::warning) + "  p5rcbt isn't installed, so the game won't load these mods. Install it below.", ui::warn());

    // ---------------------------------------------------------------- the game's own files
    section("The game's own files");
    auto* origCell = new brls::DetailCell();
    origCell->setText("PATCH1.CPK and file list");
    origCell->setDetailText(st.originalsStatus());
    origCell->registerClickAction([](brls::View*) {
        auto* dialog = new brls::Dialog(
            "When it builds PATCH1.CPK, the app keeps the files the game's own PATCH1.CPK has (like an intro movie), and "
            "it uses the game's file list to put files of mods that leave out their CPK folder in the right place.\n\n"
            "To let it read them, once (and again after a game update):\n\n" + AppState::gameFilesSteps());
        dialog->addButton("OK", [] {});
        dialog->open();
        return true;
    });
    content->addView(origCell);
    if (mode == mm::DeployMode::Patch1 && build.missingOriginals)
        note(std::string(ui::icon::info) + "  Your PATCH1.CPK is built without the game's own PATCH1 files. Press A above to see how "
                                           "to read them.",
             ui::accent());

    // ---------------------------------------------------------------- p5rcbt
    section("p5rcbt · Skyline plugin by Raytwo");
    note("Loads CPKs from p5r/bind/ next to the game's files, and adds a 60 FPS patch and the PC version's settings menu. "
         "Only for game version " + std::string(mm::p5rcbt::kRequiredGameVersion) + " (you have " +
         (st.gameVersion(game).empty() ? std::string("an unknown version") : st.gameVersion(game)) + ").");
    auto* install = new brls::DetailCell();
    install->setText(snap.p5rcbtInstalled ? "Update p5rcbt" : "Install p5rcbt");
    install->setDetailText("From GitHub");
    install->registerClickAction([this, install](brls::View*) {
        runJob(install, "Downloading p5rcbt…",
               [](mm::Library& lib, auto progress) { AppState::get().installP5rcbt(lib, progress); },
               "p5rcbt is installed. It shows in My mods, where you can turn it off.");
        return true;
    });
    content->addView(install);
    if (snap.p5rcbtInstalled) {
        auto cfg = mm::p5rcbt::readConfig(platform::sdRoot());
        auto save = [](std::function<void(mm::p5rcbt::Config&)> change) {
            auto c = mm::p5rcbt::readConfig(platform::sdRoot());
            change(c);
            try {
                mm::p5rcbt::writeConfig(platform::sdRoot(), c);
                brls::Application::notify("Saved. It applies the next time the game starts.");
            } catch (const std::exception& e) {
                brls::Application::notify(std::string("Couldn't save: ") + e.what());
            }
        };
        auto* fps = new brls::BooleanCell();
        fps->init("60 FPS", cfg.uncapFramerate, [save](bool on) { save([on](auto& c) { c.uncapFramerate = on; }); });
        content->addView(fps);
        auto* pc = new brls::BooleanCell();
        pc->init("PC version's settings menu", cfg.pcSettings, [save](bool on) { save([on](auto& c) { c.pcSettings = on; }); });
        content->addView(pc);
        auto* log = new brls::BooleanCell();
        log->init("Log file loading (for mod makers)", cfg.logging, [save](bool on) { save([on](auto& c) { c.logging = on; }); });
        content->addView(log);
    }

    // NSFW: kept out of sight so children don't stumble on it. Holding L + R + minus for
    // 3 seconds on this page shows it until the app closes; every launch starts hidden.
    nsfwViews_.push_back(section("Browsing"));
    auto* nsfw = new brls::BooleanCell();
    nsfw->init("Hide NSFW mods", AppState::get().settings().hideNsfw, [](bool on) { AppState::get().settings().hideNsfw = on; });
    content->addView(nsfw);
    nsfwViews_.push_back(nsfw);
    auto* nsfwNote = ui::label("Uses GameBanana's own content ratings: mods with nudity or sexual themes are hidden from Browse, "
                               "Search and links. Turns back on every time the app starts.",
                               14, ui::muted(), false);
    nsfwNote->setMargins(10, 4, 0, 4);
    content->addView(nsfwNote);
    nsfwViews_.push_back(nsfwNote);
    if (!AppState::get().settings().nsfwSettingRevealed) {
        for (auto* v : nsfwViews_) v->setVisibility(brls::Visibility::GONE);
        holdTimer_.setCallback([this] {
            auto held = [](brls::ControllerButton b) { return brls::Application::getControllerState().buttons[b] || debug::isHeld(b); };
            bool all = held(brls::BUTTON_LB) && held(brls::BUTTON_RB) && held(brls::BUTTON_BACK);
            heldMs_ = all ? heldMs_ + 100 : 0;
            if (heldMs_ >= 3000) revealNsfwSetting();
        });
        holdTimer_.start(100);
    }

    section("Background add-ons");
    auto modules = mm::listSysmodules(platform::sdRoot());
    if (modules.empty()) {
        auto* none = ui::label("No add-ons (sysmodules) found in atmosphere/contents.", 14, ui::muted(), false);
        none->setMargins(10, 4, 0, 4);
        content->addView(none);
    }
    for (const auto& m : modules) {
        auto* cell = new brls::BooleanCell();
        cell->init(m.name, m.enabled, [id = m.titleId, name = m.name, needed = m.needed](bool on) {
            try {
                mm::setSysmoduleEnabled(platform::sdRoot(), id, on);
                if (!on && needed)
                    brls::Application::notify(name + " is off. Some games may not start without it. Restart the Switch to apply.");
                else
                    brls::Application::notify(name + (on ? " turned on" : " turned off") + ". Restart the Switch to apply.");
            } catch (const std::exception& e) {
                brls::Application::notify("Couldn't change " + name + ": " + e.what());
            }
        });
        content->addView(cell);
        if (!m.about.empty() || m.needed) {
            auto* about = ui::label(m.about + (m.needed ? " · Needed to start some games" : ""), 14, ui::muted(), false);
            about->setMargins(10, 4, 4, 4);
            content->addView(about);
        }
    }
    auto* addonsNote = ui::label(
        "These start when the Switch boots, and changes apply after a restart.\n"
        "Game crashing with an Atmosphère error (Program ID 010041544D530000) when mods are on? That's usually "
        "not the mod: add-ons can use up the memory Atmosphère needs to load mods, or not match your Atmosphère "
        "version. Turn them off, restart and try the game. If it works, turn them back on one at a time "
        "(restarting each time) to find the one causing it.",
        14, ui::muted(), false);
    addonsNote->setMargins(10, 4, 0, 4);
    content->addView(addonsNote);

    section("Storage");
    auto* cache = new brls::DetailCell();
    auto cacheDirs = [] {
        auto& st = AppState::get();
        return dirSize(st.downloadsDir()) + dirSize(st.workDir());
    };
    cache->setText("Delete unfinished downloads");
    cache->setDetailText(formatSize(cacheDirs()));
    cache->registerClickAction([cache, cacheDirs, alive = alive_](brls::View*) {
        auto& st = AppState::get();
        if (st.fileJobRunning()) {
            brls::Application::notify("Wait for the current install to finish");
            return true;
        }
        cache->setDetailText("Deleting…");
        st.runFileJob(
            [](mm::Library&) {
                mm::removeTree(AppState::get().downloadsDir());
                mm::removeTree(AppState::get().workDir());
            },
            [cache, cacheDirs, alive](const std::string&) {
                if (*alive) cache->setDetailText(formatSize(cacheDirs()));
                brls::Application::notify("Leftover downloads deleted");
            });
        return true;
    });
    content->addView(cache);

    auto* rebuild = new brls::DetailCell();
    rebuild->setText("Build the game's mod files again");
    rebuild->setDetailText("Fixes missing or damaged files");
    rebuild->registerClickAction([this, rebuild, game](brls::View*) {
        runJob(rebuild, "Building again…",
               [game](mm::Library& lib, auto progress) {
                   lib.rebuild(game, [progress](uint64_t d, uint64_t t, const std::string&) { progress(d, t); });
               },
               "Mod files are up to date");
        return true;
    });
    content->addView(rebuild);

    auto* where = new brls::DetailCell();
    where->setText("Mod library folder");
    where->setDetailText("SD:/switch/Compendium");
    content->addView(where);
    note("If something goes wrong, send switch/Compendium/log.txt with your report: it lists what was installed and "
         "built, and any errors.");

    section("About");
    auto* about = ui::label(
        std::string(APP_TITLE) + " " + APP_VERSION + " by SantiBros: installs and manages mods for Persona games on Switch, "
        "starting with Persona 5 Royal. Mods come from GameBanana (Switch and PC pages), GitHub, links and the SD card, and "
        "are packed into CPK files on the console: no PC needed.\n"
        "Mods belong to their authors. They're only active while running Atmosphère; holding L while starting the game "
        "turns them off for that launch.\n"
        "Based on Patchwork by TheRomeoAlfa and SimpleModManager by nadrino. p5rcbt is by Raytwo. CPK and mod formats as "
        "documented by ShrineFox, Aemulus and the CriPakTools/PyCriCodecs projects.\n"
        "Free software under the GNU GPL v3. Source code and third-party licenses: github.com/SantiBros/Compendium-NX",
        16, ui::muted(), false);
    about->setMargins(4, 4, 0, 4);
    content->addView(about);
}

SettingsPage::~SettingsPage() { *alive_ = false; }

void SettingsPage::runJob(brls::View* cell, const std::string& status,
                          std::function<void(mm::Library&, std::function<void(uint64_t, uint64_t)>)> job, const std::string& doneText,
                          std::function<void()> onSuccess) {
    auto& st = AppState::get();
    if (st.installBusy()) {
        brls::Application::notify("Wait for the current change to finish");
        return;
    }
    st.setInstallBusy(true);
    auto busy = ui::busyBox(status, "Please wait. Don't close the app or turn off the Switch until this finishes.",
                            "Please wait until the files are written");
    scroll_->setVisibility(brls::Visibility::GONE);
    addView(busy.box);
    brls::Application::giveFocus(busy.box);
    auto alive = alive_;
    st.runFileJob(
        [busy, alive, job](mm::Library& lib) {
            job(lib, [busy, alive](uint64_t done, uint64_t total) mutable {
                brls::sync([busy, alive, done, total]() mutable {
                    if (!*alive || !total) return;
                    busy.bar->setVisibility(brls::Visibility::VISIBLE);
                    busy.bar->setProgress(float(done) / float(total));
                    busy.count->setText(formatSize(done) + " / " + formatSize(total));
                });
            });
        },
        [this, alive, cell, box = busy.box, doneText, onSuccess](const std::string& error) {
            AppState::get().setInstallBusy(false);
            if (!*alive) return;
            scroll_->setVisibility(brls::Visibility::VISIBLE);
            brls::Application::giveFocus(cell);
            removeView(box);
            if (error.empty()) {
                if (onSuccess) onSuccess();
                brls::Application::notify(doneText);
            } else {
                auto* dialog = new brls::Dialog("That didn't work: " + error);
                dialog->addButton("OK", [] {});
                dialog->open();
            }
        });
}

void SettingsPage::revealNsfwSetting() {
    holdTimer_.stop();
    AppState::get().settings().nsfwSettingRevealed = true;
    for (auto* v : nsfwViews_) v->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(nsfwViews_[1]);
}

}  // namespace app
