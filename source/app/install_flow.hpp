#pragma once

#include <borealis.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gamebanana.hpp"
#include "installer.hpp"
#include "library.hpp"
#include "ui.hpp"

namespace app {

// What the file list says about a file relative to the user's game.
struct FileNote {
    std::string text;
    NVGcolor color;
    bool matches = false;
};
FileNote noteFor(const mm::gb::ModFile& f, mm::Game game, const std::string& installedVersion, bool versionWordIsMod);
bool modUsesVersionWord(const mm::gb::ModDetails& d, mm::Game game);
size_t recommendedFile(const mm::gb::ModDetails& d, mm::Game game, const std::string& installedVersion);
// The library id a file of a mod is installed under.
std::string modIdFor(const mm::gb::ModDetails& d, const mm::gb::ModFile& file);

// The install steps (pick file → download → unpack → pick options → store and build), drawn
// into a pane owned by a page. Used by Browse, Add mods and My mods.
//
// Downloads are kept until the install succeeds, so a failed or cancelled install can be
// retried without downloading again; each attempt unpacks into its own folder.
class InstallFlow {
public:
    InstallFlow(brls::Box* pane, ui::FocusSink* sink);
    ~InstallFlow();

    std::function<void()> onExit;  // the user left the flow (B, or OK on a message)
    bool active() const { return active_; }
    bool busy() const { return busy_; }

    void showLoading(const std::string& text);
    void showFilePicker(const mm::gb::ModDetails& d);
    void install(const mm::gb::ModDetails& d, const mm::gb::ModFile& file);
    void showMessage(const std::string& title, const std::string& body, bool isError);
    // Show a long job's progress (Settings' rebuild, p5rcbt install…) in the pane.
    void showWorking(const std::string& status, float progress = -1);
    void setBusy(bool busy);

private:
    void setWorking(const std::string& status, float progress = -1);
    void showVariantPicker(const mm::gb::ModDetails& d, const mm::gb::ModFile& file, std::shared_ptr<mm::PreparedArchive> prep,
                           mm::fs::path work);
    void finish(const mm::gb::ModDetails& d, const mm::gb::ModFile& file, std::shared_ptr<mm::PreparedArchive> prep,
                std::vector<size_t> variants, mm::fs::path work);
    void clear();
    void leave();

    brls::Box* pane_;
    ui::FocusSink* sink_;
    std::shared_ptr<bool> flowAlive_ = std::make_shared<bool>(true);
    bool active_ = false;
    bool busy_ = false;
    std::shared_ptr<std::atomic<bool>> cancel_;
    brls::Label* workingLabel_ = nullptr;
    ui::ProgressBar* bar_ = nullptr;
    brls::Label* hint_ = nullptr;
};

}  // namespace app
