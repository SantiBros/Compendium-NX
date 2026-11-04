#include "my_mods_page.hpp"

#include <ctime>
#include <optional>

#include "app_state.hpp"
#include "platform.hpp"
#include "compat.hpp"
#include "install_flow.hpp"
#include "p5rcbt.hpp"
#include "ui.hpp"

namespace app {

namespace {
// Shown while a change waits for the job ahead of it on the SD card.
std::string waitingText() {
    auto task = AppState::get().backgroundTask();
    return task.empty() ? "Waiting for the current install to finish…" : "Waiting while the app " + task + "…";
}
}  // namespace

MyModsPage::MyModsPage() : brls::Box(brls::Axis::ROW) {
    setGrow(1);

    auto* left = ui::vbox();
    left->setWidth(500);
    left->setShrink(0);
    left->setPadding(20, 14, 0, 16);
    left->setLineRight(1);
    left->setLineColor(ui::line());
    auto* header = ui::hbox();
    header->setMargins(0, 28, 10, 6);  // right: lines up with the rows
    header->addView(ui::label("Load order", 22, ui::text()));
    header->addView(ui::grow());
    header->addView(ui::label("top of the list wins", 15, ui::muted()));
    left->addView(header);

    list_ = ui::vbox();
    list_->setPaddingRight(28);  // keeps rows clear of the scroll bar
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setContentView(list_);
    left->addView(scroll);
    addView(left);

    pane_ = ui::vbox();
    pane_->setGrow(1);
    pane_->setPadding(24, 30, 20, 30);
    pane_->setBackgroundColor(ui::panelBg());
    addView(pane_);

    sink_ = new ui::FocusSink();
    addView(sink_);
    flow_ = std::make_unique<InstallFlow>(pane_, sink_);
    flow_->onExit = [this] { rebuild(); };
    libSub_ = AppState::get().libraryChanged()->subscribe([this] {
        if (!busy_ && !flow_->active())
            rebuildWhenSafe();
        else
            dirty_ = true;  // catch up when the job or the install screen is done
    });
    rebuild();
}

MyModsPage::~MyModsPage() {
    *alive_ = false;
    AppState::get().libraryChanged()->unsubscribe(libSub_);
}

brls::View* MyModsPage::getDefaultFocus() { return list_->getDefaultFocus(); }

void MyModsPage::rebuild() {
    auto& st = AppState::get();
    auto mods = st.mods(st.game());
    dirty_ = false;
    bool hadFocus = ui::focusInside(this) || ui::focusParked();
    ui::parkFocusIfInside(this, sink_);
    list_->clearViews();
    focusedRow_ = nullptr;
    clearPane();
    if (!st.snapshot().loaded) {
        auto* loading = ui::label("Loading your mods…", 16, ui::muted());
        loading->setMargins(10, 0, 0, 6);
        list_->addView(loading);
        return;  // rebuilt when the list arrives
    }

    // Downloads whose install failed or was abandoned.
    brls::View* firstPending = nullptr;
    auto addPendingRows = [&] {
        auto pending = st.pendingInstalls(st.game());
        if (pending.empty()) return;
        auto* head = ui::label("Downloaded, not installed", 16, ui::muted());
        head->setMargins(16, 0, 8, 6);
        list_->addView(head);
        for (const auto& p : pending) {
            auto* row = ui::focusRow();
        ui::tappable(row);
            row->addView(ui::label(ui::icon::file, 24, ui::accent()));
            auto* text = ui::vbox();
            text->setMarginLeft(12);
            text->setGrow(1);
            text->setShrink(1);
            text->addView(ui::label(p.details.summary.name, 18, ui::text()));
            auto* sub = ui::label(p.file.filename + " · " + formatFileSize(p.file.size), 14, ui::muted());
            sub->setMarginTop(3);
            text->addView(sub);
            row->addView(text);
            row->getFocusEvent()->subscribe([this, p](brls::View*) {
                if (paneFree()) showPending(p);
            });
            row->registerClickAction([this, p](brls::View*) {
                paneToFlow().install(p.details, p.file);
                return true;
            });
            row->updateActionHint(brls::BUTTON_A, "Finish installing");
            row->registerAction("Delete download", brls::BUTTON_X, [this, p](brls::View*) {
                discardPending(p);
                return true;
            });
            list_->addView(row);
            if (!firstPending) firstPending = row;
        }
    };

    if (mods.empty() && !st.pendingInstalls(st.game()).empty()) {
        addPendingRows();
        if (hadFocus && firstPending) brls::Application::giveFocus(firstPending);
        showPending(st.pendingInstalls(st.game()).front());
        return;
    }
    if (mods.empty()) {
        auto* row = ui::focusRow();
        ui::tappable(row, ui::Tap::Act);
        row->addView(ui::label(ui::icon::browse, 24, ui::accent()));
        auto* l = ui::label("No mods yet. Open Browse to find some.", 17, ui::text());
        l->setMarginLeft(12);
        row->addView(l);
        list_->addView(row);
        pane_->addView(ui::label("Mods you install show up here. You can turn each one on or off, and choose which wins when two mods change the same file.", 18, ui::muted(), false));
        if (hadFocus) brls::Application::giveFocus(row);
        return;
    }

    std::set<std::string> conflicted;
    for (const auto& c : st.snapshot().conflicts.at(st.game()))
        for (const auto& id : c.modIds) conflicted.insert(id);

    brls::View* focusTarget = nullptr;
    int n = 1;
    for (const auto* m : mods) {
        std::string id = m->id;
        auto* row = ui::focusRow();
        ui::tappable(row);
        row->addView(ui::label(std::to_string(n++), 15, ui::muted()));
        auto* name = ui::label(m->name, 18, m->enabled ? ui::text() : ui::muted());
        name->setMarginLeft(14);
        name->setGrow(1);
        name->setShrink(1);
        row->addView(name);
        if (conflicted.count(id) && m->enabled) row->addView(ui::label(ui::icon::warning, 22, ui::warn()));
        // On/off pill
        auto* pill = ui::hbox();
        pill->setJustifyContent(brls::JustifyContent::CENTER);
        pill->setWidth(64);
        pill->setHeight(30);
        pill->setCornerRadius(15);
        pill->setMarginLeft(10);
        pill->setBackgroundColor(m->enabled ? ui::accent() : ui::line());
        pill->addView(ui::label(m->enabled ? "On" : "Off", 15, m->enabled ? nvgRGB(0x10, 0x28, 0x2d) : ui::muted()));
        row->addView(pill);

        row->getFocusEvent()->subscribe([this, id, row](brls::View*) {
            focusedId_ = id;
            focusedRow_ = row;
            if (paneFree()) showDetails(id);
        });
        row->registerClickAction([this, id](brls::View*) {
            toggle(id);
            return true;
        });
        row->updateActionHint(brls::BUTTON_A, m->enabled ? "Turn off" : "Turn on");
        row->registerAction("Move up", brls::BUTTON_LB, [this, id](brls::View*) {
            move(id, +1);
            return true;
        });
        row->registerAction("Move down", brls::BUTTON_RB, [this, id](brls::View*) {
            move(id, -1);
            return true;
        });
        row->registerAction("Remove", brls::BUTTON_X, [this, id](brls::View*) {
            remove(id);
            return true;
        });
        list_->addView(row);
        if (id == focusedId_ || !focusTarget) focusTarget = row;
    }
    addPendingRows();
    if (hadFocus && focusTarget) brls::Application::giveFocus(focusTarget);
    showDetails(focusedId_.empty() ? mods.front()->id : focusedId_);
}

void MyModsPage::rebuildWhenSafe() {
    // While a dialog is open it will hand focus back to one of our rows when it closes, so
    // those rows must survive: catch up once focus is back (onChildFocusGained).
    auto* focus = brls::Application::getCurrentFocus();
    if (focus && focus->getParentActivity() != getParentActivity())
        dirty_ = true;
    else
        rebuild();
}

void MyModsPage::clearPane() {
    *detailsToken_ = false;  // late GameBanana details for the old pane are dropped
    ui::parkFocusIfInside(pane_, sink_);
    pane_->clearViews();
}

InstallFlow& MyModsPage::paneToFlow() {
    *detailsToken_ = false;
    return *flow_;
}

bool MyModsPage::paneFree() {
    if (!flow_->active()) return true;
    if (flow_->busy()) return false;
    // Moving back into the list abandons an install that's waiting for a choice.
    flow_->onExit = nullptr;
    flow_ = std::make_unique<InstallFlow>(pane_, sink_);
    flow_->onExit = [this] { rebuild(); };
    if (dirty_ && !busy_) {
        // Something was installed meanwhile: show it in the list (next frame, focus is moving).
        dirty_ = false;
        auto alive = alive_;
        brls::sync([this, alive] {
            if (*alive) rebuild();
        });
    }
    return true;
}

void MyModsPage::onChildFocusGained(brls::View* directChild, brls::View* focusedView) {
    brls::Box::onChildFocusGained(directChild, focusedView);
    // Catch up on changes made while a dialog had focus, once it's safe (next frame).
    if (dirty_ && !busy_ && !flow_->active()) {
        dirty_ = false;
        auto alive = alive_;
        brls::sync([this, alive] {
            if (*alive) rebuild();
        });
    }
}

void MyModsPage::showPending(const PendingInstall& p) {
    clearPane();
    pane_->addView(ui::label(p.details.summary.name, 26, ui::text()));
    auto* meta = ui::label(p.file.filename + " · " + formatFileSize(p.file.size), 16, ui::muted());
    meta->setMargins(4, 0, 16, 0);
    pane_->addView(meta);
    pane_->addView(ui::label(
        "This file was downloaded but isn't installed yet. The install may have failed, been cancelled, "
        "or the app closed part-way.\n\nPress A to finish installing it (no need to download again), "
        "or X to delete the download and free the space.",
        16, nvgRGB(0xc8, 0xc8, 0xc8), false));
}

void MyModsPage::discardPending(const PendingInstall& p) {
    auto* dialog = new brls::Dialog("Delete the download of " + p.details.summary.name + " (" +
                                    formatFileSize(p.file.size) + ")?");
    dialog->addButton("Cancel", [] {});
    dialog->addButton("Delete", [this, fileId = p.file.id] {
        runJob("Deleting…", [fileId](mm::Library&, const auto&) { AppState::get().discardPending(fileId); });
    });
    dialog->open();
}


void MyModsPage::showDetails(const std::string& id) {
    auto& st = AppState::get();
    const auto* m = st.findMod(id);
    clearPane();
    if (!m) return;

    auto* content = ui::vbox();
    content->setPadding(0, 22, 20, 0);
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setContentView(content);
    pane_->addView(scroll);
    scroll->registerAction("Back to list", brls::BUTTON_B, [this](brls::View*) {
        if (focusedRow_) brls::Application::giveFocus(focusedRow_);
        return true;
    });

    content->addView(ui::label(m->name, 26, ui::text()));
    size_t count = m->cpkFiles.size() + m->sdFiles.size();
    std::string what = m->bundles.empty() ? std::to_string(count) + (count == 1 ? " file" : " files")
                                          : std::to_string(m->bundles.size()) + (m->bundles.size() == 1 ? " CPK" : " CPKs") +
                                                (count ? " + " + std::to_string(count) + " files" : "");
    std::string meta = (m->author.empty() ? "" : "by " + m->author + " · ") + (m->format.empty() ? "" : m->format + " · ") + what +
                       " · " + (m->enabled ? "on" : "off");
    auto* metaL = ui::label(meta, 16, ui::muted(), false);
    metaL->setMargins(4, 0, 16, 0);
    content->addView(metaL);

    auto addNote = [content](const std::string& text, NVGcolor color, NVGcolor bg) {
        auto* box = ui::hbox();
        box->setBackgroundColor(bg);
        box->setCornerRadius(4);
        box->setPadding(10, 12, 10, 12);
        box->setMarginBottom(8);
        auto* l = ui::label(text, 16, color, false);
        l->setGrow(1);
        l->setShrink(1);
        box->addView(l);
        content->addView(box);
    };
    auto nameOf = [&st](const std::string& modId) {
        auto* other = st.findMod(modId);
        return other ? other->name : modId;
    };

    std::map<std::string, int> overrides, overriddenBy;
    for (const auto& c : st.snapshot().conflicts.at(m->game)) {
        auto pos = std::find(c.modIds.begin(), c.modIds.end(), id);
        if (pos == c.modIds.end()) continue;
        if (pos == c.modIds.begin())
            for (auto it = c.modIds.begin() + 1; it != c.modIds.end(); ++it) overrides[*it]++;
        else
            overriddenBy[c.modIds.front()]++;
    }
    auto files = [](int n) { return std::to_string(n) + (n == 1 ? " file" : " files"); };
    for (auto& [other, n] : overrides)
        addNote(std::string(ui::icon::warning) + "  Overrides " + files(n) + " from " + nameOf(other) + ". Move " + nameOf(other) +
                    " above it if you want that mod to win.",
                ui::warn(), ui::warnBg());
    for (auto& [other, n] : overriddenBy)
        addNote(std::string(ui::icon::warning) + "  " + files(n) + " replaced by " + nameOf(other) + ", which is higher in the list.",
                ui::warn(), ui::warnBg());
    if (m->pcMod)
        addNote(std::string(ui::icon::pc) + "  Made for the PC version and adapted. If something looks wrong in the game, turn it off.",
                ui::accent(), ui::accentDim());
    if (m->id == mm::p5rcbt::kModId) {
        const auto& v = st.gameVersion(m->game);
        if (!v.empty() && v != mm::p5rcbt::kRequiredGameVersion)
            addNote(std::string(ui::icon::warning) + "  p5rcbt only works with game version " + mm::p5rcbt::kRequiredGameVersion +
                        "; you have " + v + ". Update the game, or turn p5rcbt off.",
                    ui::warn(), ui::warnBg());
        else
            addNote(std::string(ui::icon::info) + "  The plugin that loads CPKs from p5r/bind/ and adds a 60 FPS patch. Its settings are "
                                                  "in Settings.",
                    ui::accent(), ui::accentDim());
    }
    for (const auto& n : m->notes) addNote(std::string(ui::icon::info) + "  " + n, ui::muted(), ui::rowBg());

    char date[32] = "";
    std::time_t t = static_cast<std::time_t>(m->installedAt);
    if (t) {
        auto tm = platform::localTime(t);  // the console's time zone (newlib's localtime is UTC there)
        std::strftime(date, sizeof date, "%b %d, %Y", &tm);
    }
    std::string info = "Installed " + std::string(date);
    if (!m->sourceFile.empty()) info += "\nFile: " + m->sourceFile;
    if (!m->version.empty()) info += "\nMod version: " + m->version;
    if (!m->variants.empty()) {
        info += "\nOptions:";
        for (const auto& v : m->variants) info += " " + v + ";";
        info.pop_back();
    }
    auto* infoL = ui::label(info, 16, ui::muted(), false);
    infoL->setMarginTop(8);
    content->addView(infoL);
    showModPage(*m, content);
}

void MyModsPage::showModPage(const mm::ModRecord& m, brls::Box* content) {
    bool gb = m.sourceKey.rfind("gb:", 0) == 0 && m.gamebananaModId;
    if (!gb) return;  // imported or from a link: no page
    mm::gb::ModSummary summary;
    summary.id = m.gamebananaModId;
    summary.sourceKey = m.sourceKey;
    summary.name = m.name;
    summary.author = m.author;
    summary.profileUrl = m.profileUrl;

    auto* heading = ui::label(std::string("About this mod · ") + std::string("GameBanana"), 16, ui::muted());
    heading->setMargins(22, 0, 8, 0);
    content->addView(heading);
    auto* body = ui::vbox();
    body->addView(new brls::ProgressSpinner());
    content->addView(body);

    auto token = std::make_shared<bool>(true);
    detailsToken_ = token;
    auto alive = alive_;
    auto wanted = [alive, token] { return *alive && *token; };
    auto fill = [body, wanted](const mm::gb::ModDetails& d) {
        if (!wanted()) return;
        body->clearViews();  // only the spinner: focus can't be in it
        if (!d.screenshotUrls.empty()) {
            auto* hero = new brls::Image();
            hero->setHeight(240);
            hero->setCornerRadius(6);
            hero->setBackgroundColor(ui::thumbBg());
            hero->setScalingType(brls::ImageScalingType::FILL);
            hero->setClipsToBounds(true);
            hero->setMarginBottom(10);
            body->addView(hero);
            AppState::get().loadImage(hero, d.screenshotUrls[0], wanted);
        }
        auto paras = paragraphs(stripHtml(d.descriptionHtml, 12000));
        if (paras.size() > 1) body->addView(ui::label("Press → to read the full description", 16, ui::muted()));
        for (size_t i = 0; i < paras.size(); ++i) {
            // One focusable chunk per paragraph, so it can be scrolled through.
            auto* chunk = ui::vbox();
            chunk->setFocusable(true);
            chunk->setHideHighlightBackground(true);
            chunk->setHighlightPadding(4);
            chunk->setMarginTop(i == 0 ? 10 : 8);
            chunk->addView(ui::label(paras[i], 16, nvgRGB(0xc8, 0xc8, 0xc8), false));
            body->addView(chunk);
        }
    };
    if (auto d = AppState::get().cachedDetails(m.sourceKey)) {
        fill(*d);
        return;
    }
    AppState::get().runNetwork([summary, fill, wanted, body] {
        try {
            auto d = AppState::get().fetchDetails(summary);
            brls::sync([d, fill] {
                AppState::get().cacheDetails(d);
                fill(d);
            });
        } catch (const std::exception& e) {
            brls::sync([wanted, body] {
                if (!wanted()) return;
                body->clearViews();
                body->addView(ui::label("Couldn't load the mod's page. Check your internet connection.", 16, ui::muted(), false));
            });
        }
    });
}

void MyModsPage::runJob(const std::string& status, std::function<void(mm::Library&, const mm::ProgressFn&)> job) {
    if (busy_) return;
    busy_ = true;
    AppState::get().setInstallBusy(true);
    clearPane();
    auto busy = ui::busyBox(status,
                            AppState::get().fileJobRunning() ? waitingText()
                                                             : "Please wait. Don't close the app or turn off the Switch until this finishes.",
                            "Please wait until the files are written");
    auto* box = busy.box;
    pane_->addView(box);
    auto alive = alive_;
    brls::sync([this, alive, box] {
        if (*alive && busy_) brls::Application::giveFocus(box);
    });
    brls::Application::giveFocus(box);

    auto shown = std::make_shared<int>(-1);
    mm::ProgressFn progress = [this, alive, busy, shown, status](uint64_t done, uint64_t total, const std::string& what) {
        if (!total) return;
        int step = static_cast<int>(done * 50 / total);
        if (step == *shown && done != total) return;
        *shown = step;
        brls::sync([this, alive, busy, done, total, status, what]() mutable {
            if (!*alive || !busy_) return;
            busy.status->setText(what.rfind("Building ", 0) == 0 ? what + "…" : status);
            busy.bar->setVisibility(brls::Visibility::VISIBLE);
            busy.bar->setProgress(float(done) / float(total));
            busy.count->setText(formatSize(done) + " / " + formatSize(total));
        });
    };
    AppState::get().runFileJob([job = std::move(job), progress](mm::Library& lib) { job(lib, progress); },
                               [this, alive](const std::string& error) {
                                   AppState::get().setInstallBusy(false);
                                   if (!*alive) return;
                                   busy_ = false;
                                   if (!error.empty()) brls::Application::notify("Something went wrong: " + error);
                                   rebuildWhenSafe();
                               });
}

void MyModsPage::toggle(const std::string& id) {
    const auto* m = AppState::get().findMod(id);
    if (!m) return;
    bool enable = !m->enabled;
    runJob(enable ? "Turning on " + m->name + "…" : "Turning off " + m->name + "…",
           [id, enable](mm::Library& lib, const auto& p) { lib.setEnabled(id, enable, p); });
}

void MyModsPage::move(const std::string& id, int delta) {
    runJob("Updating load order…", [id, delta](mm::Library& lib, const auto& p) { lib.move(id, delta, p); });
}

void MyModsPage::remove(const std::string& id) {
    const auto* m = AppState::get().findMod(id);
    if (!m) return;
    bool onlyCopy = m->id.rfind("local-existing-", 0) == 0 || m->sourceKey.rfind("smm:", 0) == 0;
    std::string text = "Remove " + m->name + "? Its files are deleted from the SD card.";
    text += onlyCopy ? " This is the only copy of these files the app has." : " You can download it again from Browse.";
    auto* dialog = new brls::Dialog(text);
    dialog->addButton("Cancel", [] {});
    dialog->addButton("Remove", [this, id, name = m->name] {
        if (focusedId_ == id) focusedId_.clear();
        runJob("Removing " + name + "…", [id](mm::Library& lib, const auto& p) { lib.remove(id, p); });
    });
    dialog->open();
}

}  // namespace app
