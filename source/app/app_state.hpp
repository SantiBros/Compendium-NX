#pragma once

#include <borealis.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "gamebanana.hpp"
#include "gamefiles.hpp"
#include "library.hpp"
#include "smm.hpp"

namespace app {

// Fixed set of threads draining a queue of jobs.
class WorkQueue {
public:
    explicit WorkQueue(int threads);
    ~WorkQueue();
    // Drop the waiting jobs, wait for the running ones and end the threads.
    void stop();
    void post(std::function<void()> job);

private:
    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<int> active_{0};
    bool stop_ = false;
};

// Copy of the library state for the UI thread; the real Library is only touched by the file
// worker so long installs never block drawing.
struct LibrarySnapshot {
    std::vector<mm::ModRecord> mods;  // all games, highest priority first within a game
    std::map<mm::Game, std::vector<mm::Conflict>> conflicts;
    std::map<mm::Game, mm::BuildStatus> builds;
    std::map<mm::Game, mm::DeployMode> modes;
    std::map<mm::Game, std::string> unmanaged;  // a PATCH1.CPK the manager didn't build
    std::map<mm::Game, bool> haveOriginals;     // the game's own PATCH1.CPK is kept
    std::set<std::string> installedKeys;        // sourceKey of every installed mod
    bool p5rcbtInstalled = false;
    std::vector<mm::smm::FoundMod> smmMods;     // SimpleModManager's Persona mods on the SD card
    bool loaded = false;
};

// A mod file that finished downloading but isn't installed yet (failed or abandoned
// install). Kept so trying again doesn't download it again.
struct PendingInstall {
    mm::Game game = mm::Game::P5R;
    mm::gb::ModDetails details;
    mm::gb::ModFile file;
    mm::fs::path archive;
};

struct Settings {
    // Hide mods GameBanana treats as NSFW. Not saved: every launch starts hidden, and the
    // setting only appears after holding L + R + minus on the Settings page.
    std::atomic<bool> hideNsfw{true};
    bool nsfwSettingRevealed = false;
    // Atmosphère mod-loader crash reports already told about; null until the first check.
    std::optional<std::vector<std::string>> seenCrashReports;
};

class AppState {
public:
    static AppState& get();

    void init();
    void shutdown();

    // ---- current game
    mm::Game game() const { return game_; }
    void setGame(mm::Game g);
    brls::VoidEvent* gameChanged() { return &gameChanged_; }
    const std::string& gameVersion(mm::Game g) const { return gameVersions_.at(g); }

    // ---- network (runs on a small thread pool; callbacks must hop back with brls::sync)
    mm::gb::Client& gamebanana() { return *gb_; }
    mm::HttpClient& http() { return *http_; }
    void runNetwork(std::function<void()> job) { net_.post(std::move(job)); }

    // ---- library: mutate only inside runFileJob; read the snapshot on the UI thread
    void runFileJob(std::function<void(mm::Library&)> job, std::function<void(const std::string& error)> done);
    static std::string outOfMemoryMessage();
    bool fileJobRunning() const { return fileJobs_ > 0; }
    std::string backgroundTask() const;
    void setInstallBusy(bool busy) { installBusy_ += busy ? 1 : -1; }
    bool installBusy() const { return installBusy_ > 0; }

    const LibrarySnapshot& snapshot() const { return snapshot_; }
    brls::VoidEvent* libraryChanged() { return &libraryChanged_; }
    std::vector<const mm::ModRecord*> mods(mm::Game g) const;
    const mm::ModRecord* findMod(const std::string& id) const;

    // ---- caches (UI thread only)
    std::optional<mm::gb::ModDetails> cachedDetails(const std::string& sourceKey) const;
    void cacheDetails(const mm::gb::ModDetails& d);

    // ---- sources (network; call from runNetwork, not the UI thread)
    mm::gb::ModDetails fetchDetails(const mm::gb::ModSummary& s);
    // A pasted link or a GameBanana number -> something installable. Throws with a message.
    mm::gb::ModDetails resolveLink(const std::string& url, mm::Game game);
    // Install or update p5rcbt from its latest GitHub release (file thread).
    void installP5rcbt(mm::Library& lib, const std::function<void(uint64_t, uint64_t)>& progress);

    void loadImage(brls::Image* image, const std::string& url, std::function<bool()> stillWanted);

    // ---- the game's own files
    // What the app knows of them, for Settings ("Kept", "Not read yet"…).
    std::string originalsStatus() const;
    // The steps to let the app read the game's files.
    static std::string gameFilesSteps();

    // ---- settings
    Settings& settings() { return settings_; }
    void saveSettings();

    const mm::fs::path& dataDir() const { return dataDir_; }
    mm::fs::path downloadsDir() const { return dataDir_ / "downloads"; }
    mm::fs::path workDir() const { return dataDir_ / "work"; }
    mm::fs::path inboxDir() const { return dataDir_ / "inbox"; }
    mm::fs::path workDirFor(int64_t fileId) const { return workDir() / std::to_string(fileId); }
    mm::fs::path archivePathFor(const mm::gb::ModFile& f) const;
    bool isDownloaded(const mm::gb::ModFile& f) const;
    std::vector<PendingInstall> pendingInstalls(mm::Game g) const;
    std::optional<PendingInstall> pendingFor(const std::string& sourceKey) const;
    void savePending(const PendingInstall& p) const;
    void discardPending(int64_t fileId) const;
    uint64_t freeSpace() const;

    // Finds a file's CPK folder from the game's own file list (when it has been read).
    mm::PathResolver pathResolver() const;

private:
    AppState();
    void publishSnapshot(mm::Library& lib);
    void captureGameFiles(mm::Library& lib);  // file thread, when running in place of the game

    mm::Game game_ = mm::Game::P5R;
    std::map<mm::Game, std::string> gameVersions_;
    mm::fs::path dataDir_;
    std::unique_ptr<mm::HttpClient> http_;
    std::unique_ptr<mm::gb::Client> gb_;
    std::unique_ptr<mm::Library> library_;
    LibrarySnapshot snapshot_;
    Settings settings_;

    WorkQueue net_{networkThreads()};
    static int networkThreads();
    WorkQueue files_{1};
    std::atomic<int> fileJobs_{0};
    int installBusy_ = 0;

    brls::VoidEvent gameChanged_;
    brls::VoidEvent libraryChanged_;

    std::map<std::string, mm::gb::ModDetails> details_;
    mutable std::mutex statusMutex_;
    std::string backgroundTask_;   // guarded by statusMutex_
    std::string originalsStatus_;  // guarded by statusMutex_
    std::shared_ptr<mm::GameFileList> fileList_;  // guarded by statusMutex_

    std::list<std::pair<std::string, std::shared_ptr<std::string>>> images_;
};

// Text helpers shared by the views.
std::string formatSize(uint64_t bytes);
std::string formatFileSize(int64_t bytes);  // "size unknown" for 0 / -1
std::string formatCount(int64_t n);
std::string formatDateTime(int64_t unixSeconds, bool inConsoleZone = true);
std::string stripHtml(const std::string& html, size_t maxLen = 600);
std::vector<std::string> paragraphs(const std::string& text, size_t maxLen = 360);

}  // namespace app
