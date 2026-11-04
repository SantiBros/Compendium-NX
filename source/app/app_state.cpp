#include "app_state.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <new>
#include <regex>

#include <webp/decode.h>

#include "cpk.hpp"
#include "installer.hpp"
#include "nlohmann/json.hpp"
#include "p5rcbt.hpp"
#include "platform.hpp"

namespace app {

using json = nlohmann::json;

// ------------------------------------------------------------------ WorkQueue

WorkQueue::WorkQueue(int threads) {
    for (int i = 0; i < threads; ++i)
        threads_.emplace_back([this] {
            for (;;) {
                std::function<void()> job;
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                    if (stop_ && jobs_.empty()) return;
                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                    ++active_;
                }
                try {
                    job();
                } catch (const std::exception& e) {
                    brls::Logger::error("background job failed: {}", e.what());
                }
                --active_;
            }
        });
}

WorkQueue::~WorkQueue() { stop(); }

void WorkQueue::stop() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
        jobs_.clear();
    }
    cv_.notify_all();
    for (auto& t : threads_)
        if (t.joinable()) t.join();
}

void WorkQueue::post(std::function<void()> job) {
    {
        std::lock_guard lock(mutex_);
        if (stop_) return;
        jobs_.push_back(std::move(job));
    }
    cv_.notify_one();
}

// ------------------------------------------------------------------ AppState

AppState& AppState::get() {
    // Never destroyed: shutdown() ends the background threads, and nothing else needs
    // cleaning up at exit.
    static auto* instance = new AppState();
    return *instance;
}

AppState::AppState() = default;


void AppState::init() {
    dataDir_ = platform::sdRoot() / "switch" / "PersonaModManager";
    mm::fs::create_directories(dataDir_);
    std::error_code ec;
    mm::fs::create_directories(inboxDir(), ec);
    for (const auto& g : mm::kGames) gameVersions_[g.game] = platform::gameVersion(g.game);

    http_ = std::make_unique<mm::CurlHttpClient>(platform::userAgent(), platform::caBundle());
    gb_ = std::make_unique<mm::gb::Client>(*http_);
    library_ = std::make_unique<mm::Library>(platform::sdRoot(), dataDir_);
    library_->setFreeSpace(platform::freeSpaceBytes);

    try {
        auto j = json::parse(mm::readFile(dataDir_ / "settings.json"));
        if (auto g = mm::gameFromKey(j.value("game", "p5r"))) game_ = *g;
        if (j.contains("seenCrashReports")) settings_.seenCrashReports = j["seenCrashReports"].get<std::vector<std::string>>();
    } catch (...) {
        // first run or unreadable: defaults
    }

    // The game's own file list, if it has been read before.
    auto list = std::make_shared<mm::GameFileList>();
    if (list->load(dataDir_ / "original" / "p5r" / "files.txt")) fileList_ = list;
    library_->setPathResolver(pathResolver());

    auto staleWork = workDir();
    runFileJob([this, staleWork](mm::Library& lib) {
        mm::removeTree(staleWork);  // leftovers from an install interrupted by a crash or power-off
        lib.load();
        publishSnapshot(lib);
        if (platform::runningAsGame(mm::Game::P5R)) captureGameFiles(lib);
    }, [](const std::string& err) {
        if (!err.empty()) brls::Application::notify("Couldn't read your mod list: " + err);
    });
}

void AppState::captureGameFiles(mm::Library& lib) {
    {
        std::lock_guard lock(statusMutex_);
        backgroundTask_ = "reads the game's own files";
    }
    struct Done {
        AppState* st;
        ~Done() {
            std::lock_guard lock(st->statusMutex_);
            st->backgroundTask_.clear();
        }
    } done{this};
    const auto game = mm::Game::P5R;
    auto orig = lib.originalPatchCpk(game);
    auto tmp = orig;
    tmp += ".new";
    std::string status;
    try {
        platform::copyGameFile(game, std::string(mm::gameInfo(game).patchCpk), tmp);
        // Opened without L, Atmosphère hands over the SD card's PATCH1.CPK: the one we built.
        auto info = mm::cpk::read(tmp);
        if (info.tool == "PersonaModManager") {
            std::error_code ec;
            mm::fs::remove(tmp, ec);
            status = "Mods were on when the game started. Hold L as well as R next time.";
        } else {
            std::error_code ec;
            mm::fs::remove(orig, ec);
            mm::fs::rename(tmp, orig, ec);
            status = "Kept (" + std::to_string(info.entries.size()) + " files)";
        }
        // Without L, the PATCH1.CPK the game sees is ours: list only its other archives then.
        bool ours = status.rfind("Kept", 0) != 0;
        auto paths = platform::gameCpkFileList(game, ours ? "PATCH1.CPK" : "");
        auto listFile = dataDir_ / "original" / "p5r" / "files.txt";
        mm::GameFileList::save(listFile, paths);
        auto fresh = std::make_shared<mm::GameFileList>();
        fresh->load(listFile);
        {
            std::lock_guard lock(statusMutex_);
            fileList_ = fresh;
        }
        lib.setPathResolver(pathResolver());
        lib.rebuild(game);  // now with the game's own PATCH1 files
    } catch (const std::exception& e) {
        status = std::string("Couldn't read them: ") + e.what();
    }
    std::lock_guard lock(statusMutex_);
    originalsStatus_ = status;
}

mm::PathResolver AppState::pathResolver() const {
    std::shared_ptr<mm::GameFileList> list;
    {
        std::lock_guard lock(statusMutex_);
        list = fileList_;
    }
    if (!list || list->empty()) return {};
    return [list](const std::string& p) { return list->resolve(p); };
}

std::string AppState::originalsStatus() const {
    std::lock_guard lock(statusMutex_);
    if (!originalsStatus_.empty()) return originalsStatus_;
    auto it = snapshot_.haveOriginals.find(game_);
    return it != snapshot_.haveOriginals.end() && it->second ? "Kept" : "Not read yet";
}

std::string AppState::gameFilesSteps() {
    return "1. Press + to close the app.\n"
           "2. Hold L and R and start Persona 5 Royal. Keep holding until the Homebrew Menu opens.\n"
           "   (R opens the Homebrew Menu in place of the game; L turns mods off for that launch.)\n"
           "3. Open Persona Mod Manager from the Homebrew Menu. It reads the files by itself.";
}

void AppState::shutdown() {
    saveSettings();
    if (http_) http_->cancelAll();
    net_.stop();
    files_.stop();
}

std::string AppState::backgroundTask() const {
    std::lock_guard lock(statusMutex_);
    return backgroundTask_;
}

void AppState::setGame(mm::Game g) {
    if (g == game_) return;
    game_ = g;
    saveSettings();
    gameChanged_.fire();
}

void AppState::saveSettings() {
    try {
        json j{{"game", std::string(mm::gameInfo(game_).key)}};
        if (settings_.seenCrashReports) j["seenCrashReports"] = *settings_.seenCrashReports;
        mm::writeFileAtomic(dataDir_ / "settings.json", j.dump(1));
    } catch (const std::exception& e) {
        brls::Logger::error("saving settings failed: {}", e.what());
    }
}

int AppState::networkThreads() { return platform::fullMemory() ? 3 : 2; }

std::string AppState::outOfMemoryMessage() {
    std::string text = "Persona Mod Manager ran out of memory.";
    if (!platform::fullMemory())
        text += "\n\nIt was opened from the Album, which leaves it only a little memory. Press + to close it, then "
                "hold R while starting a game and open it from the Homebrew Menu that appears.";
    return text;
}

void AppState::runFileJob(std::function<void(mm::Library&)> job, std::function<void(const std::string&)> done) {
    ++fileJobs_;
    files_.post([this, job = std::move(job), done = std::move(done)] {
        std::string error;
        try {
            job(*library_);
        } catch (const std::bad_alloc&) {
            error = outOfMemoryMessage();
        } catch (const std::exception& e) {
            error = e.what();
        }
        try {
            publishSnapshot(*library_);
        } catch (const std::exception& e) {
            brls::Logger::error("couldn't read the library state: {}", e.what());
        }
        brls::sync([this, done, error] {
            --fileJobs_;
            if (done) done(error);
        });
    });
}

void AppState::publishSnapshot(mm::Library& lib) {
    auto snap = std::make_shared<LibrarySnapshot>();
    for (const auto& g : mm::kGames) {
        for (const auto* m : lib.mods(g.game)) {
            snap->mods.push_back(*m);
            if (!m->sourceKey.empty()) snap->installedKeys.insert(m->sourceKey);
        }
        snap->conflicts[g.game] = lib.conflicts(g.game);
        snap->builds[g.game] = lib.buildStatus(g.game);
        snap->modes[g.game] = lib.mode(g.game);
        snap->unmanaged[g.game] = lib.unmanagedOutput(g.game);
        snap->haveOriginals[g.game] = lib.hasOriginalPatch(g.game);
    }
    snap->p5rcbtInstalled = mm::p5rcbt::installed(lib.sdRoot(), mm::Game::P5R);
    snap->smmMods = mm::smm::findMods(lib.sdRoot(), mm::Game::P5R);
    snap->loaded = true;
    brls::sync([this, snap] {
        snapshot_ = std::move(*snap);
        libraryChanged_.fire();
    });
}

std::vector<const mm::ModRecord*> AppState::mods(mm::Game g) const {
    std::vector<const mm::ModRecord*> out;
    for (const auto& m : snapshot_.mods)
        if (m.game == g) out.push_back(&m);
    return out;
}

const mm::ModRecord* AppState::findMod(const std::string& id) const {
    for (const auto& m : snapshot_.mods)
        if (m.id == id) return &m;
    return nullptr;
}

std::optional<mm::gb::ModDetails> AppState::cachedDetails(const std::string& sourceKey) const {
    auto it = details_.find(sourceKey);
    if (it == details_.end()) return std::nullopt;
    return it->second;
}

void AppState::cacheDetails(const mm::gb::ModDetails& d) {
    if (details_.size() > 200) details_.clear();
    details_[d.summary.sourceKey] = d;
}

mm::gb::ModDetails AppState::fetchDetails(const mm::gb::ModSummary& s) { return gb_->details(s.id); }

mm::gb::ModDetails AppState::resolveLink(const std::string& rawUrl, mm::Game game) {
    std::string url = rawUrl;
    while (!url.empty() && (url.back() == ' ' || url.back() == '\n' || url.back() == '\r')) url.pop_back();
    // A bare number is a GameBanana mod (the end of its link).
    if (!url.empty() && url.find_first_not_of("0123456789") == std::string::npos) url = "https://gamebanana.com/mods/" + url;
    static const std::regex gbRe(R"(^https?://(www\.)?gamebanana\.com/(mods|dl)/(\d+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(url, m, gbRe) && m[2].str() == "mods") {
        mm::gb::ModDetails d;
        try {
            d = gb_->details(std::stoll(m[3].str()));
        } catch (const mm::HttpStatusError& e) {
            if (e.status() == 404) throw std::runtime_error("There's no GameBanana mod with that number.");
            throw;
        }
        if (d.summary.gameId && d.summary.gameId != mm::gameInfo(game).gamebananaId)
            d.summary.category = "Other version of the game: " + d.summary.category;  // e.g. a PC mod, adapted
        if (d.summary.nsfw && settings_.hideNsfw) throw std::runtime_error("GameBanana marks this mod as NSFW, so it's hidden.");
        return d;
    }
    // GitHub releases: the newest release's first zip/7z/rar.
    static const std::regex ghRe(R"(^https?://github\.com/([^/]+)/([^/#?]+)/?(releases/?(latest)?)?/?$)", std::regex::icase);
    if (std::regex_search(url, m, ghRe)) {
        auto body = http_->get("https://api.github.com/repos/" + m[1].str() + "/" + m[2].str() + "/releases/latest");
        auto j = json::parse(body, nullptr, false);
        mm::gb::ModDetails d;
        d.summary.sourceKey = "gh:" + m[1].str() + "/" + m[2].str();
        d.summary.id = mm::stableId(d.summary.sourceKey);
        d.summary.name = m[2].str();
        d.summary.author = m[1].str();
        d.summary.profileUrl = "https://github.com/" + m[1].str() + "/" + m[2].str();
        if (j.is_object()) {
            d.version = j.value("tag_name", "");
            d.descriptionHtml = j.value("body", "");
            for (const auto& a : j.value("assets", json::array())) {
                auto name = a.value("name", "");
                if (!mm::isSupportedArchiveName(name) && !mm::endsWithI(name, ".cpk")) continue;
                mm::gb::ModFile f;
                f.filename = name;
                f.downloadUrl = a.value("browser_download_url", "");
                f.size = a.value("size", int64_t(0));
                f.id = mm::stableId(f.downloadUrl);
                f.version = d.version;
                d.files.push_back(f);
            }
        }
        if (d.files.empty()) throw std::runtime_error("That GitHub project's latest release has no archive to install.");
        return d;
    }
    if (url.rfind("http", 0) != 0) throw std::runtime_error("That doesn't look like a link or a GameBanana mod number.");
    // Anything else: a direct download. Name it after the file.
    std::string path = url.substr(0, url.find_first_of("?#"));
    std::string filename = mm::urlDecode(path.substr(path.rfind('/') + 1));
    if (filename.empty() || filename.find('.') == std::string::npos) filename = "download.zip";
    mm::gb::ModDetails d;
    d.summary.sourceKey = "url:" + path;
    d.summary.id = mm::stableId(d.summary.sourceKey);
    d.summary.name = filename.substr(0, filename.rfind('.'));
    auto host = url.find("//") + 2;
    d.summary.author = url.substr(host, url.find('/', host) - host);
    d.summary.profileUrl = url;
    mm::gb::ModFile f;
    f.id = mm::stableId(url);
    f.filename = filename;
    f.downloadUrl = url;
    f.size = http_->contentLength(url);
    f.description = "From a link";
    d.files.push_back(f);
    return d;
}

void AppState::installP5rcbt(mm::Library& lib, const std::function<void(uint64_t, uint64_t)>& progress) {
    auto release = mm::p5rcbt::parseRelease(http_->get(mm::p5rcbt::kReleaseApi));
    auto zip = downloadsDir() / ("p5rcbt-" + mm::slugify(release.tag) + ".zip");
    mm::fs::create_directories(downloadsDir());
    http_->download(release.zipUrl, zip, [&](uint64_t done, uint64_t total) {
        if (progress) progress(done, total);
        return true;
    });
    auto prep = mm::prepareArchive(zip, workDir() / "p5rcbt", mm::Game::P5R);
    std::vector<size_t> all;
    for (size_t i = 0; i < prep.layout.variants.size(); ++i) all.push_back(i);
    auto files = mm::mergeVariants(prep.layout, all);
    files.files.clear();  // only the plugin itself (and its settings) go on the SD card
    files.cpks.clear();
    if (files.sdFiles.empty()) throw std::runtime_error("The p5rcbt download didn't contain the plugin.");
    mm::ModRecord rec;
    rec.id = mm::p5rcbt::kModId;
    rec.game = mm::Game::P5R;
    rec.name = "p5rcbt (Skyline plugin)";
    rec.author = "Raytwo";
    rec.version = release.tag;
    rec.sourceFile = zip.filename().string();
    rec.profileUrl = "https://github.com/Raytwo/p5rcbt";
    rec.sourceKey = "gh:Raytwo/p5rcbt";
    rec.format = "Plugin";
    lib.add(rec, files, true);
    mm::removeTree(workDir() / "p5rcbt");
    std::error_code ec;
    mm::fs::remove(zip, ec);
}

namespace {

bool isWebp(const std::string& data) {
    return data.size() > 12 && data.compare(0, 4, "RIFF") == 0 && data.compare(8, 4, "WEBP") == 0;
}

// Borealis decodes JPEG/PNG itself; many GameBanana previews are WebP, which we decode.
void setImageData(brls::Image* image, const std::string& data) {
    if (!isWebp(data)) {
        image->setImageFromMem(reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()));
        return;
    }
    int w = 0, h = 0;
    uint8_t* rgba = WebPDecodeRGBA(reinterpret_cast<const uint8_t*>(data.data()), data.size(), &w, &h);
    if (!rgba) return;
    int tex = nvgCreateImageRGBA(brls::Application::getNVGContext(), w, h, 0, rgba);
    WebPFree(rgba);
    if (tex) image->innerSetImage(tex);
}

}  // namespace

void AppState::loadImage(brls::Image* image, const std::string& url, std::function<bool()> stillWanted) {
    if (url.empty()) return;
    for (auto it = images_.begin(); it != images_.end(); ++it) {
        if (it->first == url) {
            images_.splice(images_.begin(), images_, it);  // mark as recently used
            setImageData(image, *it->second);
            return;
        }
    }
    runNetwork([this, image, url, stillWanted] {
        auto data = std::make_shared<std::string>();
        try {
            *data = http_->get(url);
        } catch (const std::exception& e) {
            brls::Logger::warning("image {} failed: {}", url, e.what());
            return;
        }
        brls::sync([this, image, url, stillWanted, data] {
            images_.emplace_front(url, data);
            while (images_.size() > 80) images_.pop_back();
            if (stillWanted && stillWanted()) setImageData(image, *data);
        });
    });
}

uint64_t AppState::freeSpace() const { return platform::freeSpaceBytes(); }

// ------------------------------------------------------------------ pending installs

mm::fs::path AppState::archivePathFor(const mm::gb::ModFile& f) const {
    if (f.downloadUrl.rfind("file://", 0) == 0) return f.downloadUrl.substr(7);  // already on the SD card
    // slugify keeps the extension, which prepareArchive checks.
    return downloadsDir() / (std::to_string(f.id) + "-" + mm::slugify(f.filename, 80));
}

bool AppState::isDownloaded(const mm::gb::ModFile& f) const {
    std::error_code ec;
    auto size = mm::fs::file_size(archivePathFor(f), ec);
    return !ec && size > 0 && (f.size <= 0 || static_cast<int64_t>(size) == f.size);
}

namespace {

json fileToJson(const mm::gb::ModFile& f) {
    return {{"id", f.id}, {"filename", f.filename}, {"size", f.size}, {"downloadUrl", f.downloadUrl},
            {"md5", f.md5}, {"version", f.version}, {"description", f.description}, {"dateAdded", f.dateAdded}};
}

mm::gb::ModFile fileFromJson(const json& j) {
    mm::gb::ModFile f;
    f.id = j.value("id", int64_t{0});
    f.filename = j.value("filename", "");
    f.size = j.value("size", int64_t{0});
    f.downloadUrl = j.value("downloadUrl", "");
    f.md5 = j.value("md5", "");
    f.version = j.value("version", "");
    f.description = j.value("description", "");
    f.dateAdded = j.value("dateAdded", int64_t{0});
    return f;
}

}  // namespace

void AppState::savePending(const PendingInstall& p) const {
    const auto& d = p.details;
    json j{{"game", std::string(mm::gameInfo(p.game).key)},
           {"modId", d.summary.id},
           {"sourceKey", d.summary.sourceKey},
           {"name", d.summary.name},
           {"author", d.summary.author},
           {"category", d.summary.category},
           {"thumbnailUrl", d.summary.thumbnailUrl},
           {"profileUrl", d.summary.profileUrl},
           {"version", d.version},
           {"descriptionHtml", d.descriptionHtml.substr(0, 6000)},
           {"file", fileToJson(p.file)}};
    mm::writeFileAtomic(downloadsDir() / (std::to_string(p.file.id) + ".json"), j.dump(1));
}

std::vector<PendingInstall> AppState::pendingInstalls(mm::Game g) const {
    std::vector<PendingInstall> out;
    std::error_code ec;
    for (mm::fs::directory_iterator it(downloadsDir(), ec), end; it != end; it.increment(ec)) {
        if (it->path().extension() != ".json") continue;
        try {
            auto j = json::parse(mm::readFile(it->path()));
            PendingInstall p;
            p.game = mm::gameFromKey(j.value("game", "p5r")).value_or(mm::Game::P5R);
            if (p.game != g) continue;
            p.details.summary.id = j.value("modId", int64_t{0});
            p.details.summary.sourceKey = j.value("sourceKey", "gb:" + std::to_string(p.details.summary.id));
            p.details.summary.name = j.value("name", "");
            p.details.summary.author = j.value("author", "");
            p.details.summary.category = j.value("category", "");
            p.details.summary.thumbnailUrl = j.value("thumbnailUrl", "");
            p.details.summary.profileUrl = j.value("profileUrl", "");
            p.details.version = j.value("version", "");
            p.details.descriptionHtml = j.value("descriptionHtml", "");
            p.file = fileFromJson(j.value("file", json::object()));
            p.details.files = {p.file};
            p.archive = archivePathFor(p.file);
            if (isDownloaded(p.file)) out.push_back(std::move(p));
        } catch (...) {
            // unreadable sidecar: ignore it
        }
    }
    return out;
}

std::optional<PendingInstall> AppState::pendingFor(const std::string& sourceKey) const {
    for (auto& p : pendingInstalls(game_))
        if (p.details.summary.sourceKey == sourceKey) return p;
    return std::nullopt;
}

void AppState::discardPending(int64_t fileId) const {
    std::error_code ec;
    for (mm::fs::directory_iterator it(downloadsDir(), ec), end; it != end; it.increment(ec)) {
        auto name = it->path().filename().string();
        auto prefix = std::to_string(fileId);
        if (name == prefix + ".json" || name.rfind(prefix + "-", 0) == 0) mm::removeTree(it->path());
    }
    mm::removeTree(workDirFor(fileId));
}

// ------------------------------------------------------------------ text helpers

std::string formatFileSize(int64_t bytes) { return bytes > 0 ? formatSize(static_cast<uint64_t>(bytes)) : "size unknown"; }

std::string formatSize(uint64_t bytes) { return mm::formatSize(bytes); }

std::string formatCount(int64_t n) {
    char buf[32];
    if (n >= 1000000)
        std::snprintf(buf, sizeof buf, "%.1fM", n / 1e6);
    else if (n >= 1000)
        std::snprintf(buf, sizeof buf, "%.1fk", n / 1e3);
    else
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(n));
    return buf;
}

std::string formatDateTime(int64_t unixSeconds, bool inConsoleZone) {
    std::tm tm{};
    if (inConsoleZone) {
        tm = platform::localTime(unixSeconds);
    } else {
        std::time_t t = static_cast<std::time_t>(unixSeconds);
        localtime_r(&t, &tm);
    }
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int hour = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
    char buf[48];
    std::snprintf(buf, sizeof buf, "%s %d, %d %d:%02d %s", months[(tm.tm_mon % 12 + 12) % 12], tm.tm_mday, tm.tm_year + 1900, hour,
                  tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    return buf;
}

std::string stripHtml(const std::string& html, size_t maxLen) {
    static const std::regex br("<br\\s*/?>|</?(p|div|li|ul|ol|h[1-6]|blockquote|tr)\\b[^>]*>", std::regex::icase);
    static const std::regex tags("<[^>]*>");
    static const std::regex spaces("[ \\t]+");
    static const std::regex blankLines("\\n{3,}");
    auto s = std::regex_replace(html, br, "\n");
    s = std::regex_replace(s, tags, "");
    // &amp; last, and each replacement scanned past, so "&amp;lt;" stays "&lt;".
    for (auto [from, to] : {std::pair{"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#039;", "'"}, {"&nbsp;", " "}, {"&amp;", "&"}}) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, std::string(from).size(), to);
            pos += std::string(to).size();
        }
    }
    s = std::regex_replace(s, spaces, " ");
    s = std::regex_replace(s, blankLines, "\n\n");
    while (!s.empty() && (s.front() == '\n' || s.front() == ' ')) s.erase(s.begin());
    if (s.size() > maxLen) {
        s.resize(maxLen);
        // don't cut a UTF-8 sequence in half
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
        if (!s.empty() && (static_cast<unsigned char>(s.back()) & 0x80)) s.pop_back();
        s += "…";
    }
    return s;
}

// Break a description into chunks short enough to scroll through one at a time.
std::vector<std::string> paragraphs(const std::string& text, size_t maxLen) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find("\n\n", start);
        auto para = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? text.size() : end + 2;
        while (para.size() > maxLen) {
            // Prefer to cut after a sentence or line, else at a space.
            size_t cut = para.rfind(". ", maxLen);
            if (cut == std::string::npos || cut < maxLen / 3) cut = para.rfind('\n', maxLen);
            if (cut == std::string::npos || cut < maxLen / 3) cut = para.rfind(' ', maxLen);
            if (cut == std::string::npos || cut == 0) cut = maxLen;
            out.push_back(para.substr(0, cut + 1));
            para.erase(0, cut + 1);
            while (!para.empty() && (para[0] == ' ' || para[0] == '\n')) para.erase(0, 1);
        }
        if (!para.empty() && para.find_first_not_of(" \n") != std::string::npos) out.push_back(para);
    }
    return out;
}

}  // namespace app

