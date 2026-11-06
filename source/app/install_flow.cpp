#include "install_flow.hpp"

#include <algorithm>
#include <ctime>
#include <regex>
#include <set>

#include "app_state.hpp"
#include "compat.hpp"
#include "cpk.hpp"
#include "log.hpp"
#include "md5.hpp"
#include "platform.hpp"

namespace app {

using mm::gb::ModDetails;
using mm::gb::ModFile;

namespace {
bool isLocalFile(const ModFile& f) { return f.downloadUrl.rfind("file://", 0) == 0; }
}  // namespace

bool modUsesVersionWord(const ModDetails& d, mm::Game game) {
    std::vector<std::string> labels;
    for (const auto& f : d.files) labels.push_back(f.description + " " + f.version);
    return mm::versionWordIsModVersion(labels, game);
}

FileNote noteFor(const ModFile& f, mm::Game game, const std::string& installed, bool versionWordIsMod) {
    if (mm::isRestoreOriginalsFile(f.filename, f.description)) return {"Original files · not needed", ui::muted()};
    if (mm::saysAllVersions(f.description)) return {"Works on all game versions", ui::ok(), true};
    auto named = mm::mentionedGameVersions(f.description, game, false, versionWordIsMod);
    switch (mm::checkCompat(installed, named)) {
        case mm::Compat::Likely: return {"Matches your game", ui::ok(), true};
        case mm::Compat::Mismatch: return {"For game " + named.front(), ui::warn()};
        case mm::Compat::Unknown: break;
    }
    return {formatFileSize(f.size), ui::muted()};
}

// The file we'd pick for the user: one made for their game version, else the newest; Switch
// files over PC ones; the main file over add-ons.
size_t recommendedFile(const ModDetails& d, mm::Game game, const std::string& installed) {
    size_t best = 0;
    int bestScore = -1000;
    bool versionWordIsMod = modUsesVersionWord(d, game);
    for (size_t i = 0; i < d.files.size(); ++i) {
        const auto& f = d.files[i];
        int score = 0;
        auto desc = mm::toLower(f.description + " " + f.filename);
        score += 2 * mm::languageMatch(f.description + " " + f.filename, "en");
        if (mm::isRestoreOriginalsFile(f.filename, f.description)) score -= 20;
        if (noteFor(f, game, installed, versionWordIsMod).matches) score += 8;
        if (desc.find("switch") != std::string::npos || desc.find("nx") != std::string::npos) score += 3;
        if (desc.find("pc") != std::string::npos || desc.find("steam") != std::string::npos || desc.find("reloaded") != std::string::npos)
            score -= 2;
        if (desc.find("not recommend") != std::string::npos) score -= 2;
        static const std::regex mainFile(R"(\bmain\s+(file|download|mod)\b)");
        if (std::regex_search(desc, mainFile)) score += 1;
        bool isVariantOfAnother = false;
        for (size_t j = 0; j < d.files.size(); ++j)
            if (j != i && mm::isAddOnFileOf(f.filename, d.files[j].filename)) isVariantOfAnother = true;
        if (!isVariantOfAnother) score += 1;
        if (score > bestScore || (score == bestScore && f.dateAdded > d.files[best].dateAdded)) {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

std::string modIdFor(const ModDetails& d, const ModFile& file) {
    if (d.summary.sourceKey.rfind("gb:", 0) == 0) return "gb" + std::to_string(d.summary.id) + "-" + std::to_string(file.id);
    return "src-" + mm::slugify(d.summary.sourceKey, 60) + "-" + std::to_string(file.id % 1000000007);
}

InstallFlow::InstallFlow(brls::Box* pane, ui::FocusSink* sink) : pane_(pane), sink_(sink) {}

InstallFlow::~InstallFlow() {
    *flowAlive_ = false;
    if (cancel_) *cancel_ = true;
    setBusy(false);
}

void InstallFlow::setBusy(bool busy) {
    if (busy == busy_) return;
    busy_ = busy;
    AppState::get().setInstallBusy(busy);
}

void InstallFlow::clear() {
    ui::parkFocusIfInside(pane_, sink_);
    pane_->clearViews();
    workingLabel_ = nullptr;
    bar_ = nullptr;
    hint_ = nullptr;
}

void InstallFlow::leave() {
    if (busy_) return;
    active_ = false;
    cancel_.reset();
    if (onExit) onExit();
}

void InstallFlow::showLoading(const std::string& text) {
    active_ = true;
    setWorking(text);
}

void InstallFlow::showWorking(const std::string& status, float progress) {
    active_ = true;
    setWorking(status, progress);
}

void InstallFlow::setWorking(const std::string& status, float progress) {
    if (!workingLabel_) {
        clear();
        auto* box = ui::vbox();
        box->setGrow(1);
        box->setJustifyContent(brls::JustifyContent::CENTER);
        box->setAlignItems(brls::AlignItems::CENTER);
        box->addView(new brls::ProgressSpinner());
        workingLabel_ = ui::label(status, 20, ui::text(), false);
        workingLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        workingLabel_->setMarginTop(16);
        box->addView(workingLabel_);
        bar_ = new ui::ProgressBar(360);
        bar_->setMarginTop(14);
        box->addView(bar_);
        hint_ = ui::label("", 15, ui::muted(), false);
        hint_->setMarginTop(12);
        hint_->setMaxWidth(420);
        hint_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        box->addView(hint_);
        // Focusable so B works and focus doesn't stay on the list behind.
        box->setFocusable(true);
        box->setHideHighlight(true);
        box->registerAction("Cancel", brls::BUTTON_B, [this](brls::View*) {
            if (!busy_) {
                leave();
            } else if (cancel_) {
                *cancel_ = true;
                if (workingLabel_) workingLabel_->setText("Cancelling…");
            } else {
                brls::Application::notify("Copying to the SD card can't be stopped part-way. It's almost done.");
            }
            return true;
        });
        // Stay here until the work finishes or is cancelled: leaving would drop the install.
        for (auto button : {brls::BUTTON_NAV_LEFT, brls::BUTTON_NAV_RIGHT, brls::BUTTON_NAV_UP, brls::BUTTON_NAV_DOWN})
            box->registerAction("", button, [](brls::View*) { return true; }, true);
        // + would quit the app mid-install.
        box->registerAction("", brls::BUTTON_START, [](brls::View*) {
            brls::Application::notify("Finish or cancel the install first");
            return true;
        }, true);
        pane_->addView(box);
        brls::Application::giveFocus(box);
    } else {
        workingLabel_->setText(status);
    }
    // Downloading and unpacking can be stopped; installing and merging can't.
    hint_->setText(busy_ && !cancel_ ? "Please wait. Don't close the app or turn off the Switch until this finishes."
                                     : "Press B to cancel");
    bar_->setVisibility(progress >= 0 ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    if (progress >= 0) bar_->setProgress(progress);
}

void InstallFlow::showMessage(const std::string& title, const std::string& body, bool isError) {
    active_ = true;
    setBusy(false);
    clear();
    auto* box = ui::vbox();
    box->setGrow(1);
    box->setFocusable(true);
    box->setHideHighlight(true);
    bool setup = title.rfind("One-time setup", 0) == 0;
    box->addView(ui::label(std::string(isError ? ui::icon::warning : setup ? ui::icon::info : ui::icon::check) + "  " + title, 24,
                           isError ? ui::warn() : setup ? ui::accent() : ui::ok()));
    auto* b = ui::label(body, 17, ui::text(), false);
    b->setMarginTop(14);
    box->addView(b);
    ui::tappable(box, ui::Tap::Act);
    box->registerAction("OK", brls::BUTTON_A, [this](brls::View*) {
        leave();
        return true;
    });
    box->registerAction("Back", brls::BUTTON_B, [this](brls::View*) {
        leave();
        return true;
    });
    pane_->addView(box);
    brls::Application::giveFocus(box);
}

void InstallFlow::showFilePicker(const ModDetails& d) {
    active_ = true;
    clear();
    auto& st = AppState::get();
    auto game = st.game();
    const auto& installed = st.gameVersion(game);

    if (d.files.empty()) {
        showMessage("Nothing to install", "This mod has no downloadable files on GameBanana.", true);
        return;
    }

    pane_->addView(ui::label("Install " + d.summary.name, 24, ui::text()));
    auto* sub = ui::label("Choose a file. The highlighted one is the best match for your game.", 16, ui::muted(), false);
    sub->setMargins(6, 0, 16, 0);
    pane_->addView(sub);

    auto* list = ui::vbox();
    list->setPaddingRight(28);  // keeps rows clear of the scroll bar
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setContentView(list);
    list->registerAction("Back", brls::BUTTON_B, [this](brls::View*) {
        leave();
        return true;
    });
    pane_->addView(scroll);

    size_t rec = recommendedFile(d, game, installed);
    bool versionWordIsMod = modUsesVersionWord(d, game);
    brls::View* recRow = nullptr;
    for (size_t i = 0; i < d.files.size(); ++i) {
        const auto file = d.files[i];
        auto note = noteFor(file, game, installed, versionWordIsMod);
        bool downloaded = st.isDownloaded(file);
        const auto* installedMod = st.findMod(modIdFor(d, file));
        auto* row = ui::focusRow();
        ui::tappable(row);
        row->addView(ui::label(ui::icon::file, 24, ui::muted()));
        auto* text = ui::vbox();
        text->setGrow(1);
        text->setShrink(1);
        text->setMarginLeft(12);
        text->addView(ui::label(file.filename, 17, ui::text()));
        auto descText = file.description.empty() ? formatFileSize(file.size) : file.description + " · " + formatFileSize(file.size);
        auto* desc = ui::label(descText, 14, ui::muted());
        desc->setMarginTop(3);
        text->addView(desc);
        row->addView(text);
        auto* n = installedMod ? ui::label(std::string(ui::icon::check) + " Installed", 15, ui::ok())
                  : downloaded ? ui::label("Downloaded", 15, ui::accent())
                               : ui::label((note.matches ? std::string(ui::icon::check) + " " : "") + note.text, 15, note.color);
        n->setMarginLeft(10);
        row->addView(n);
        auto details = d;
        std::string installedState = installedMod ? (installedMod->enabled ? "on" : "off") : "";
        row->registerClickAction([this, details, file, installedState](brls::View*) {
            if (installedState.empty()) {
                install(details, file);
                return true;
            }
            auto* dialog = new brls::Dialog(file.filename + " is already installed (turned " + installedState +
                                            " in My mods). Download and install it again?");
            dialog->addButton("Cancel", [] {});
            dialog->addButton("Install again", [this, details, file] { install(details, file); });
            dialog->open();
            return true;
        });
        row->updateActionHint(brls::BUTTON_A, installedMod ? "Install again" : downloaded ? "Finish installing" : "Install");
        list->addView(row);
        if (i == rec) recRow = row;
    }
    brls::Application::giveFocus(recRow);
}

void InstallFlow::install(const ModDetails& d, const ModFile& file) {
    active_ = true;
    if (!file.avClean) {
        showMessage("File blocked", "GameBanana's virus scan flagged this file, so it won't be installed.", true);
        return;
    }
    auto& st = AppState::get();
    bool cached = isLocalFile(file) || st.isDownloaded(file);
    // Rough need: the archive, its unpacked files, the library copy and the rebuilt CPK.
    uint64_t free = st.freeSpace();
    if (file.size > 0 && free > 0) {
        uint64_t need = static_cast<uint64_t>(file.size) * (cached ? 3 : 4);
        if (free < need) {
            showMessage("Not enough free space",
                        "Installing this needs about " + formatSize(need) + " free on the SD card, and there's " +
                            formatSize(free) + ". Remove some mods or other files and try again.",
                        true);
            return;
        }
    }
    setBusy(true);
    cancel_ = std::make_shared<std::atomic<bool>>(false);
    setWorking(cached ? "Unpacking…" : "Downloading…", 0);

    PendingInstall pending{st.game(), d, file, st.archivePathFor(file)};
    mm::log::info("install \"" + d.summary.name + "\" (" + d.summary.sourceKey + "), file " + file.filename +
                  (cached ? " (already downloaded)" : " from " + file.downloadUrl));
    auto work = st.workDirFor(file.id);
    auto prep = std::make_shared<mm::PreparedArchive>();
    auto cancel = cancel_;
    auto alive = flowAlive_;
    auto resolver = st.pathResolver();
    auto update = [this, alive, cancel](std::string msg, float pct) {
        brls::sync([this, alive, cancel, msg, pct] {
            if (*alive && !*cancel && workingLabel_) setWorking(msg, pct);
        });
    };

    st.runFileJob(
        [=](mm::Library&) {
            auto& state = AppState::get();
            if (!cached) {
                state.savePending(pending);  // remember it even if the app closes mid-download
                int lastPct = -1;
                try {
                    state.http().download(file.downloadUrl, pending.archive, [&](uint64_t done, uint64_t total) {
                        if (!total && file.size > 0) total = static_cast<uint64_t>(file.size);
                        int pct = total ? static_cast<int>(done * 100 / total) : -1;
                        if (pct != lastPct) {
                            lastPct = pct;
                            update(pct >= 0 ? "Downloading… " + std::to_string(pct) + "%" : "Downloading… " + formatSize(done),
                                   pct >= 0 ? pct / 100.0f : -1);
                        }
                        return !*cancel;
                    });
                } catch (...) {
                    state.discardPending(file.id);
                    throw;
                }
                if (!file.md5.empty()) {
                    update("Checking download…", -1);
                    if (!mm::iequals(mm::md5File(pending.archive), file.md5)) {
                        state.discardPending(file.id);
                        throw std::runtime_error("The download was damaged (its checksum doesn't match GameBanana's). Try again.");
                    }
                }
            }
            update("Unpacking… 0%", 0);
            int lastPct = -1;
            try {
                *prep = mm::prepareArchive(pending.archive, work, pending.game, resolver,
                                           [&](uint64_t done, uint64_t total, const std::string&) {
                                               int pct = total ? static_cast<int>(done * 100 / total) : 0;
                                               if (pct != lastPct) {
                                                   lastPct = pct;
                                                   update("Unpacking… " + std::to_string(pct) + "%", pct / 100.0f);
                                               }
                                               return !*cancel;
                                           });
            } catch (...) {
                mm::removeTree(work);  // the download stays for another try
                throw;
            }
        },
        [=, this](const std::string& error) {
            if (!*alive) return;
            setBusy(false);
            if (*cancel) {
                mm::removeTree(work);
                leave();
                return;
            }
            if (!error.empty()) {
                bool kept = !isLocalFile(file) && AppState::get().isDownloaded(file);
                showMessage("Install failed",
                            error + (kept ? "\n\nThe download was kept. Try again from here or from My mods; it won't download again." : ""),
                            true);
                return;
            }
            const auto& layout = prep->layout;
            if (layout.variants.empty()) {
                mm::removeTree(work);
                showMessage("Can't install this file", layout.problem, true);
                return;
            }
            if (layout.variants.size() == 1)
                finish(d, file, prep, {0}, work);
            else
                showVariantPicker(d, file, prep, work);
        });
}

void InstallFlow::showVariantPicker(const ModDetails& d, const ModFile& file, std::shared_ptr<mm::PreparedArchive> prep,
                                    mm::fs::path work) {
    clear();
    pane_->addView(ui::label("Choose options", 24, ui::text()));
    auto* sub = ui::label("This file contains several versions or add-ons. Pick the ones you want, then press + to install. "
                          "Where two of them change the same file, the one lower in this list wins.",
                          16, ui::muted(), false);
    sub->setMargins(6, 0, 16, 0);
    pane_->addView(sub);

    const auto& variants = prep->layout.variants;
    auto selected = std::make_shared<std::vector<bool>>(variants.size(), false);
    auto game = AppState::get().game();
    const auto& installed = AppState::get().gameVersion(game);
    bool picked = false;
    for (size_t i = 0; i < variants.size(); ++i)
        if (mm::checkCompat(installed, mm::mentionedGameVersions(variants[i].name, game)) == mm::Compat::Likely) {
            (*selected)[i] = true;
            picked = true;
        }
    if (!picked) (*selected)[0] = true;

    auto* list = ui::vbox();
    list->setPaddingRight(28);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setContentView(list);
    pane_->addView(scroll);

    auto install = [this, d, file, prep, selected, work](brls::View*) {
        std::vector<size_t> chosen;
        for (size_t i = 0; i < selected->size(); ++i)
            if ((*selected)[i]) chosen.push_back(i);
        if (chosen.empty()) {
            brls::Application::notify("Pick at least one option");
            return true;
        }
        finish(d, file, prep, chosen, work);
        return true;
    };
    list->registerAction("Back", brls::BUTTON_B, [this, work](brls::View*) {
        mm::removeTree(work);
        leave();
        return true;
    });
    list->registerAction("Install", brls::BUTTON_START, install);

    auto boxes = std::make_shared<std::vector<brls::Label*>>(variants.size(), nullptr);
    brls::View* first = nullptr;
    for (size_t i = 0; i < variants.size(); ++i) {
        const auto& v = variants[i];
        auto* row = ui::focusRow();
        ui::tappable(row, ui::Tap::Act);
        auto* box = ui::label((*selected)[i] ? ui::icon::boxOn : ui::icon::boxOff, 26, ui::accent());
        (*boxes)[i] = box;
        row->addView(box);
        auto* text = ui::vbox();
        text->setGrow(1);
        text->setShrink(1);
        text->setMarginLeft(12);
        text->addView(ui::label(v.name, 18, ui::text()));
        auto* kind = ui::label(mm::formatName(v.format), 14, ui::muted());
        kind->setMarginTop(3);
        text->addView(kind);
        row->addView(text);
        size_t count = v.files.size() + v.sdFiles.size();
        for (const auto& c : v.cpks) count += c.files;
        row->addView(ui::label(std::to_string(count) + (count == 1 ? " file" : " files"), 15, ui::muted()));
        row->registerClickAction([selected, boxes, i](brls::View*) {
            (*selected)[i] = !(*selected)[i];
            (*boxes)[i]->setText((*selected)[i] ? ui::icon::boxOn : ui::icon::boxOff);
            return true;
        });
        row->updateActionHint(brls::BUTTON_A, "Select");
        list->addView(row);
        if (!first) first = row;
    }
    auto* button = new brls::Button();
    button->setText("Install selected");
    button->setMarginTop(12);
    button->registerClickAction(install);
    list->addView(button);
    for (const auto& w : prep->layout.warnings) {
        auto* l = ui::label(std::string(ui::icon::warning) + " " + w, 14, ui::muted(), false);
        l->setMarginTop(6);
        list->addView(l);
    }
    brls::Application::giveFocus(first);
}

void InstallFlow::finish(const ModDetails& d, const ModFile& file, std::shared_ptr<mm::PreparedArchive> prep,
                         std::vector<size_t> variants, mm::fs::path work) {
    auto& st = AppState::get();
    auto newId = modIdFor(d, file);
    std::vector<std::string> others;
    std::string otherNames;
    for (const auto* m : st.mods(st.game()))
        if (!d.summary.sourceKey.empty() && m->sourceKey == d.summary.sourceKey && m->id != newId) {
            others.push_back(m->id);
            otherNames += (otherNames.empty() ? "" : ", ") + m->sourceFile;
        }

    auto run = [this, d, file, prep, variants, newId, work](std::vector<std::string> replace) {
        auto& st = AppState::get();
        mm::ModRecord r;
        r.id = newId;
        r.game = st.game();
        r.name = d.summary.name;
        r.author = d.summary.author;
        r.version = file.version.empty() ? d.version : file.version;
        r.sourceFile = file.filename;
        r.profileUrl = d.summary.profileUrl;
        r.sourceKey = d.summary.sourceKey;
        if (d.summary.sourceKey.rfind("gb:", 0) == 0) {
            r.gamebananaModId = d.summary.id;
            r.gamebananaFileId = file.id;
        }
        const auto& layout = prep->layout;
        if (layout.variants.size() > 1)
            for (auto i : variants) r.variants.push_back(layout.variants.at(i).name);
        r.format = mm::formatName(layout.variants.at(variants.front()).format);
        r.pcMod = layout.pcMod;
        r.notes = mm::installNotes(layout, variants);
        auto files = mm::mergeVariants(layout, variants);
        size_t count = files.files.size() + files.sdFiles.size() + files.cpks.size();
        setBusy(true);
        cancel_.reset();  // copying can't be cancelled half-way; the library rolls back on failure
        setWorking("Installing " + std::to_string(count) + (count == 1 ? " file…" : " files…"), 0);
        auto alive = flowAlive_;
        st.runFileJob(
            [this, alive, r, files, replace, work, fileId = file.id, local = isLocalFile(file)](mm::Library& lib) {
                for (const auto& id : replace) lib.remove(id);
                lib.add(r, files, true, [this, alive](uint64_t done, uint64_t total, const std::string& what) {
                    float pct = total ? float(done) / float(total) : -1;
                    std::string text = what.rfind("Building ", 0) == 0 ? what + "…" : "Installing…";
                    brls::sync([this, alive, text, pct] {
                        if (*alive && workingLabel_) setWorking(text, pct);
                    });
                });
                mm::removeTree(work);
                if (!local) AppState::get().discardPending(fileId);
            },
            [this, alive, d, file](const std::string& error) {
                if (!*alive) {
                    brls::Application::notify(error.empty() ? d.summary.name + " installed" : "Install of " + d.summary.name + " failed");
                    return;
                }
                setBusy(false);
                if (!error.empty()) {
                    showMessage("Install failed",
                                error + (isLocalFile(file) ? std::string()
                                                           : "\n\nThe download was kept. Try again from here or from My mods; it won't download again."),
                                true);
                    return;
                }
                auto& st = AppState::get();
                const auto* m = st.findMod(modIdFor(d, file));
                const auto& build = st.snapshot().builds.at(st.game());
                std::string body = d.summary.name + " is installed and turned on. Start the game to try it.";
                if (!build.target.empty())
                    body += "\n\nThe game's mods were packed into " + mm::fs::path(build.target).filename().string() + " (" +
                            std::to_string(build.files) + " files).";
                if (m && m->pcMod) body += "\n\nThis is a PC mod, adapted for the Switch: if something looks wrong in the game, turn it off in My mods.";
                if (m)
                    for (const auto& n : m->notes) body += "\n\n" + std::string(ui::icon::info) + " " + n;
                if (build.missingOriginals)
                    body += "\n\nTip: let the app read the game's own PATCH1.CPK once (Settings → The game's own files), so its files are "
                            "kept in the one built for your mods.";
                body += "\n\nYou can turn it off or change its load order in My mods.";
                showMessage("Installed", body, false);
            });
    };

    // Before copying anything, say which installed mods this one will cover up.
    auto checkConflicts = [this, d, prep, variants, newId, run, work](std::vector<std::string> replace) {
        auto& st = AppState::get();
        std::set<std::string> paths;
        auto set = mm::mergeVariants(prep->layout, variants);
        for (const auto& f : set.files) paths.insert(mm::cpk::pathKey(f.cpkPath));
        for (const auto& c : set.cpks) try {
                for (const auto& e : mm::cpk::read(c.cpk).entries) paths.insert(mm::cpk::pathKey(e.path()));
            } catch (...) {
            }
        std::vector<std::string> clashing;
        for (const auto* m : st.mods(st.game())) {
            bool replaced = std::find(replace.begin(), replace.end(), m->id) != replace.end();
            if (!m->enabled || replaced || m->id == newId) continue;
            for (const auto& p : m->cpkFiles)
                if (paths.count(mm::cpk::pathKey(p))) {
                    clashing.push_back(m->name);
                    break;
                }
        }
        if (clashing.empty()) {
            run(replace);
            return;
        }
        std::string names;  // "A", "A and B", "A, B and C"
        for (size_t i = 0; i < clashing.size(); ++i)
            names += (i == 0 ? "" : i + 1 == clashing.size() ? " and " : ", ") + clashing[i];
        auto* dialog = new brls::Dialog("Before you install " + d.summary.name + ":\n\nIt changes files that " + names +
                                        (clashing.size() == 1 ? " also changes." : " also change.") +
                                        " The mod higher in My mods wins; new installs go to the top.");
        dialog->addButton("Install", [run, replace] { run(replace); });
        dialog->addButton("Cancel", [this, work] {
            mm::removeTree(work);
            leave();
        });
        dialog->setCancelable(false);
        dialog->open();
    };

    if (others.empty()) {
        checkConflicts({});
        return;
    }
    auto* dialog = new brls::Dialog("You already have " + otherNames + " from this mod. Replace it with " + file.filename + "?");
    dialog->addButton("Replace", [checkConflicts, others] { checkConflicts(others); });
    dialog->addButton("Keep both", [checkConflicts] { checkConflicts({}); });
    dialog->setCancelable(false);
    dialog->open();
}

}  // namespace app
