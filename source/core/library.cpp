#include "library.hpp"

#include <algorithm>
#include <ctime>
#include <set>

#include "cpk.hpp"
#include "nlohmann/json.hpp"

namespace mm {

using json = nlohmann::json;

namespace {

constexpr const char* kStateFile = "state.json";

// Where a CPK path is kept on the SD card: each part made safe for FAT.
fs::path storedPath(const std::string& cpkPath) {
    fs::path out;
    for (const auto& part : pathParts(fs::path(cpkPath))) out /= fatSafeName(part);
    return out;
}

json toJson(const ModRecord& m) {
    return json{
        {"id", m.id},
        {"game", std::string(gameInfo(m.game).key)},
        {"name", m.name},
        {"author", m.author},
        {"version", m.version},
        {"sourceFile", m.sourceFile},
        {"profileUrl", m.profileUrl},
        {"sourceKey", m.sourceKey},
        {"gamebananaModId", m.gamebananaModId},
        {"gamebananaFileId", m.gamebananaFileId},
        {"installedAt", m.installedAt},
        {"variants", m.variants},
        {"format", m.format},
        {"pcMod", m.pcMod},
        {"notes", m.notes},
        {"enabled", m.enabled},
        {"priority", m.priority},
        {"cpkFiles", m.cpkFiles},
        {"bundles", m.bundles},
        {"sdFiles", m.sdFiles},
    };
}

template <typename T>
T get(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    try {
        return it->get<T>();
    } catch (const std::exception&) {
        return fallback;
    }
}

ModRecord fromJson(const json& j) {
    ModRecord m;
    m.id = get<std::string>(j, "id", "");
    m.game = gameFromKey(get<std::string>(j, "game", "p5r")).value_or(Game::P5R);
    m.name = get<std::string>(j, "name", m.id);
    m.author = get<std::string>(j, "author", "");
    m.version = get<std::string>(j, "version", "");
    m.sourceFile = get<std::string>(j, "sourceFile", "");
    m.profileUrl = get<std::string>(j, "profileUrl", "");
    m.sourceKey = get<std::string>(j, "sourceKey", "");
    m.gamebananaModId = get<int64_t>(j, "gamebananaModId", 0);
    m.gamebananaFileId = get<int64_t>(j, "gamebananaFileId", 0);
    m.installedAt = get<int64_t>(j, "installedAt", 0);
    m.variants = get<std::vector<std::string>>(j, "variants", {});
    m.format = get<std::string>(j, "format", "");
    m.pcMod = get<bool>(j, "pcMod", false);
    m.notes = get<std::vector<std::string>>(j, "notes", {});
    m.enabled = get<bool>(j, "enabled", false);
    m.priority = get<int>(j, "priority", 0);
    m.cpkFiles = get<std::vector<std::string>>(j, "cpkFiles", {});
    m.bundles = get<std::vector<std::string>>(j, "bundles", {});
    m.sdFiles = get<std::vector<std::string>>(j, "sdFiles", {});
    return m;
}

void removeFileAndPrune(const fs::path& file, const fs::path& stopAt) {
    std::error_code ec;
    fs::remove(file, ec);
    pruneEmptyDirs(file.parent_path(), stopAt);
}

// Rename, falling back to copy + delete (another drive, or a target in the way).
void moveFile(const fs::path& from, const fs::path& to) {
    fs::create_directories(to.parent_path());
    std::error_code ec;
    if (fs::exists(to)) fs::remove(to, ec);
    fs::rename(from, to, ec);
    if (!ec) return;
    copyFileCreatingDirs(from, to);
    fs::remove(from, ec);
}

}  // namespace

const char* deployModeKey(DeployMode m) { return m == DeployMode::P5rcbt ? "p5rcbt" : "patch1"; }

NotEnoughSpaceError::NotEnoughSpaceError(uint64_t n, uint64_t f)
    : std::runtime_error("There isn't enough space on the SD card: this needs " + formatSize(n) + ", and " +
                         formatSize(f) + " is free. Free some space and try again."),
      needed(n),
      free(f) {}

Library::Library(fs::path sdRoot, fs::path dataDir) : sdRoot_(std::move(sdRoot)), dataDir_(std::move(dataDir)) {}

bool Library::load() {
    mods_.clear();
    games_.clear();
    deployed_.clear();
    backedUp_.clear();
    auto path = dataDir_ / kStateFile;
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        auto tmp = path;
        tmp += ".tmp";
        if (!fs::exists(tmp, ec)) return true;
        fs::rename(tmp, path, ec);
    }
    json j;
    try {
        j = json::parse(readFile(path));
    } catch (const std::exception&) {
        auto bad = path;
        bad += ".corrupt";
        fs::rename(path, bad, ec);
        return false;
    }
    const json modList = j.value("mods", json::array());
    for (const auto& m : modList) {
        auto rec = fromJson(m);
        if (!rec.id.empty()) mods_.push_back(std::move(rec));
    }
    const json games = j.value("games", json::object());
    const json deployed = j.value("deployed", json::object());
    const json backups = j.value("backups", json::object());
    for (const auto& [key, g] : games.items()) {
        auto game = gameFromKey(key);
        if (!game) continue;
        GameState gs;
        gs.mode = get<std::string>(g, "mode", "patch1") == "p5rcbt" ? DeployMode::P5rcbt : DeployMode::Patch1;
        gs.output = get<std::string>(g, "output", "");
        gs.outputKey = get<std::string>(g, "outputKey", "");
        gs.outputSize = get<uint64_t>(g, "outputSize", 0);
        gs.status.target = gs.output;
        gs.status.files = get<size_t>(g, "outputFiles", 0);
        gs.status.bytes = gs.outputSize;
        gs.status.withOriginals = get<bool>(g, "withOriginals", false);
        gs.status.missingOriginals = get<bool>(g, "missingOriginals", false);
        games_[*game] = gs;
    }
    for (const auto& [k, v] : deployed.items())
        if (v.is_string()) deployed_[k] = v.get<std::string>();
    for (const auto& [k, v] : backups.items())
        if (v.is_string()) backedUp_[k] = v.get<std::string>();
    return true;
}

void Library::save() const {
    json mods = json::array();
    for (const auto& m : mods_) mods.push_back(toJson(m));
    json games = json::object();
    for (const auto& [g, gs] : games_)
        games[std::string(gameInfo(g).key)] = {
            {"mode", deployModeKey(gs.mode)}, {"output", gs.output}, {"outputKey", gs.outputKey},
            {"outputSize", gs.outputSize}, {"outputFiles", gs.status.files},
            {"withOriginals", gs.status.withOriginals}, {"missingOriginals", gs.status.missingOriginals},
        };
    json j = {{"version", 1}, {"mods", mods}, {"games", games}, {"deployed", deployed_}, {"backups", backedUp_}};
    fs::create_directories(dataDir_);
    writeFileAtomic(dataDir_ / kStateFile, j.dump(1));
}

std::vector<const ModRecord*> Library::mods(Game game) const {
    std::vector<const ModRecord*> out;
    for (const auto& m : mods_)
        if (m.game == game) out.push_back(&m);
    std::stable_sort(out.begin(), out.end(), [](const ModRecord* a, const ModRecord* b) { return a->priority > b->priority; });
    return out;
}

const ModRecord* Library::find(const std::string& id) const {
    for (const auto& m : mods_)
        if (m.id == id) return &m;
    return nullptr;
}

ModRecord* Library::findMut(const std::string& id) {
    for (auto& m : mods_)
        if (m.id == id) return &m;
    return nullptr;
}

fs::path Library::modDir(const ModRecord& m) const {
    return dataDir_ / "library" / std::string(gameInfo(m.game).key) / slugify(m.id, 80);
}

fs::path Library::originalPatchCpk(Game game) const {
    return dataDir_ / "original" / std::string(gameInfo(game).key) / "PATCH1.CPK";
}

bool Library::hasOriginalPatch(Game game) const {
    std::error_code ec;
    return fs::is_regular_file(originalPatchCpk(game), ec);
}

std::string Library::outputTarget(Game game, DeployMode mode) {
    if (mode == DeployMode::P5rcbt) return "p5r/bind/Compendium.cpk";
    return contentsDir(game) + "/romfs/" + std::string(gameInfo(game).patchCpk);
}

DeployMode Library::mode(Game game) const {
    auto* gs = gameStateIf(game);
    return gs ? gs->mode : DeployMode::Patch1;
}

const BuildStatus& Library::buildStatus(Game game) const {
    static const BuildStatus none;
    auto* gs = gameStateIf(game);
    return gs ? gs->status : none;
}

void Library::renumber(std::vector<ModRecord*> ordered) {
    int n = int(ordered.size());
    for (int i = 0; i < n; ++i) ordered[size_t(i)]->priority = n - i;
}

std::vector<const ModRecord*> Library::enabledAscending(Game game) const {
    auto all = mods(game);
    std::vector<const ModRecord*> out;
    for (auto it = all.rbegin(); it != all.rend(); ++it)
        if ((*it)->enabled) out.push_back(*it);
    return out;
}

const ModRecord& Library::add(ModRecord record, const InstallSet& files, bool enable, const ProgressFn& progress) {
    if (record.id.empty()) throw std::invalid_argument("mod without an id");
    if (!record.installedAt) record.installedAt = int64_t(std::time(nullptr));
    record.cpkFiles.clear();
    record.bundles.clear();
    record.sdFiles.clear();

    uint64_t total = 0, done = 0;
    std::error_code ec;
    for (const auto& f : files.files) total += fs::file_size(f.source, ec);
    for (const auto& c : files.cpks) total += fs::file_size(c.cpk, ec);
    for (const auto& s : files.sdFiles) total += fs::file_size(s.source, ec);
    checkSpace(total);

    // Stage the files next to the library, then swap them in, so a failed copy leaves the
    // installed version as it was.
    auto dir = modDir(record);
    auto staging = dir;
    staging += ".new";
    removeTree(staging);
    auto step = [&](const fs::path& src, const std::string& what) {
        if (progress) progress(done, total, what);
        done += fs::file_size(src, ec);
    };
    std::map<std::string, std::string> cpkSeen;  // path key -> path (last one wins)
    for (const auto& f : files.files) cpkSeen[cpk::pathKey(f.cpkPath)] = f.cpkPath;
    for (const auto& f : files.files) {
        if (cpkSeen[cpk::pathKey(f.cpkPath)] != f.cpkPath) continue;
        step(f.source, f.cpkPath);
        copyFileCreatingDirs(f.source, staging / "files" / storedPath(f.cpkPath));
    }
    // Several sources may name the same path: keep the last (sorted for a stable state file).
    for (const auto& [key, path] : cpkSeen) record.cpkFiles.push_back(path);
    std::sort(record.cpkFiles.begin(), record.cpkFiles.end(),
              [](const std::string& a, const std::string& b) { return cpk::comparePaths(a, b) < 0; });
    size_t n = 0;
    for (const auto& c : files.cpks) {
        auto name = std::to_string(n++) + "_" + fatSafeName(c.cpk.filename().string());
        step(c.cpk, c.cpk.filename().string());
        copyFileCreatingDirs(c.cpk, staging / "cpk" / name);
        record.bundles.push_back(name);
    }
    std::set<std::string> sdSeen;
    for (const auto& s : files.sdFiles) {
        if (!isSafeRelativePath(s.target) || !sdSeen.insert(toLower(s.target)).second) continue;
        step(s.source, s.target);
        copyFileCreatingDirs(s.source, staging / "sd" / fs::path(s.target));
        record.sdFiles.push_back(s.target);
    }

    auto* existing = findMut(record.id);
    if (existing) {
        record.priority = existing->priority;
        if (!enable) enable = existing->enabled;
    }
    if (existing) {
        // The old version's SD card files: deleted here, so apply() puts the new ones down
        // (and gives back what was there before for files the new version no longer has).
        std::set<std::string> keep;
        for (const auto& t : record.sdFiles) keep.insert(toLower(t));
        for (auto it = deployed_.begin(); it != deployed_.end();) {
            if (it->second != record.id) {
                ++it;
                continue;
            }
            std::string target = it->first;
            for (const auto& t : existing->sdFiles)
                if (toLower(t) == it->first) target = t;
            removeFileAndPrune(sdRoot_ / fs::path(target), sdRoot_);
            if (!keep.count(it->first)) restoreBackup(target);
            it = deployed_.erase(it);
        }
    }
    removeTree(dir);
    fs::create_directories(dir.parent_path());
    fs::rename(staging, dir, ec);
    if (ec) throw std::runtime_error("couldn't store the mod: " + ec.message());
    bundleCache_.clear();

    record.enabled = enable;
    if (existing) {
        *existing = record;
    } else {
        int top = 0;
        for (const auto& m : mods_)
            if (m.game == record.game) top = std::max(top, m.priority);
        record.priority = top + 1;  // new mods go on top
        mods_.push_back(record);
    }
    apply(progress);
    return *find(record.id);
}

void Library::setEnabled(const std::string& id, bool enabled, const ProgressFn& progress) {
    auto* m = findMut(id);
    if (!m || m->enabled == enabled) return;
    m->enabled = enabled;
    apply(progress);
}

void Library::remove(const std::string& id, const ProgressFn& progress) {
    auto* m = findMut(id);
    if (!m) return;
    auto dir = modDir(*m);
    Game game = m->game;
    m->enabled = false;
    apply(progress);
    mods_.erase(std::remove_if(mods_.begin(), mods_.end(), [&](const ModRecord& r) { return r.id == id; }), mods_.end());
    removeTree(dir);
    bundleCache_.clear();
    std::vector<ModRecord*> ordered;
    for (auto* r : mods(game)) ordered.push_back(findMut(r->id));
    renumber(ordered);
    save();
}

void Library::move(const std::string& id, int delta, const ProgressFn& progress) {
    auto* m = find(id);
    if (!m || !delta) return;
    auto list = mods(m->game);
    std::vector<ModRecord*> ordered;
    for (auto* r : list) ordered.push_back(findMut(r->id));
    auto it = std::find_if(ordered.begin(), ordered.end(), [&](ModRecord* r) { return r->id == id; });
    long idx = long(it - ordered.begin());
    long to = std::clamp(idx - delta, 0L, long(ordered.size()) - 1);
    if (to == idx) return;
    auto* rec = ordered[size_t(idx)];
    ordered.erase(ordered.begin() + idx);
    ordered.insert(ordered.begin() + to, rec);
    renumber(ordered);
    apply(progress);
}

void Library::setMode(Game game, DeployMode mode, const ProgressFn& progress) {
    if (this->mode(game) == mode) return;
    gameState(game).mode = mode;
    apply(progress);
}

std::vector<std::string> Library::bundleEntries(const ModRecord& m, const std::string& bundle) const {
    auto path = (modDir(m) / "cpk" / bundle).string();
    auto it = bundleCache_.find(path);
    if (it != bundleCache_.end()) return it->second;
    std::vector<std::string> out;
    try {
        for (const auto& e : cpk::read(path).entries) out.push_back(e.path());
    } catch (const std::exception&) {
    }
    bundleCache_[path] = out;
    return out;
}

std::vector<Conflict> Library::conflicts(Game game) const {
    std::map<std::string, Conflict> byKey;
    auto note = [&](const std::string& path, const ModRecord& m, bool inCpk) {
        auto key = (inCpk ? "c:" : "s:") + cpk::pathKey(path);
        auto& c = byKey[key];
        if (c.path.empty()) {
            c.path = path;
            c.inCpk = inCpk;
        }
        if (c.modIds.empty() || c.modIds.back() != m.id) c.modIds.push_back(m.id);
    };
    for (const auto* m : mods(game)) {  // highest first
        if (!m->enabled) continue;
        for (const auto& p : m->cpkFiles) note(p, *m, true);
        for (const auto& b : m->bundles)
            for (const auto& p : bundleEntries(*m, b)) note(p, *m, true);
        for (const auto& s : m->sdFiles) note(s, *m, false);
    }
    std::vector<Conflict> out;
    for (auto& [k, c] : byKey)
        if (c.modIds.size() > 1) out.push_back(std::move(c));
    return out;
}

void Library::checkSpace(uint64_t needed) const {
    if (!freeSpace_ || !needed) return;
    uint64_t free = freeSpace_();
    if (free && needed + (16ull << 20) > free) throw NotEnoughSpaceError(needed, free);
}

void Library::backupFile(const std::string& target) {
    auto src = sdRoot_ / fs::path(target);
    std::error_code ec;
    if (!fs::exists(src, ec)) return;
    auto key = toLower(target);
    auto dst = dataDir_ / "backup" / fs::path(target);
    if (backedUp_.count(key)) {
        // Already have the original; this file is stale (e.g. left by an interrupted change).
        fs::remove(src, ec);
        return;
    }
    moveFile(src, dst);
    backedUp_[key] = target;
}

void Library::restoreBackup(const std::string& target) {
    auto key = toLower(target);
    auto it = backedUp_.find(key);
    if (it == backedUp_.end()) return;
    auto src = dataDir_ / "backup" / fs::path(it->second);
    std::error_code ec;
    if (fs::exists(src, ec)) moveFile(src, sdRoot_ / fs::path(it->second));
    pruneEmptyDirs(src.parent_path(), dataDir_ / "backup");
    backedUp_.erase(it);
}

void Library::applySdFiles(const ProgressFn& progress) {
    // target (lower-case) -> {mod, target}; mods applied lowest priority first so higher ones win.
    std::map<std::string, std::pair<std::string, std::string>> desired;
    for (const auto& info : kGames)
        for (const auto* m : enabledAscending(info.game))
            for (const auto& t : m->sdFiles) desired[toLower(t)] = {m->id, t};

    // Take off what's no longer wanted, or now comes from another mod.
    for (auto it = deployed_.begin(); it != deployed_.end();) {
        auto want = desired.find(it->first);
        if (want != desired.end() && want->second.first == it->second) {
            ++it;
            continue;
        }
        const auto* owner = find(it->second);
        std::string target = it->first;
        if (owner)
            for (const auto& t : owner->sdFiles)
                if (toLower(t) == it->first) target = t;
        removeFileAndPrune(sdRoot_ / fs::path(target), sdRoot_);
        if (want == desired.end()) restoreBackup(target);
        it = deployed_.erase(it);
    }

    uint64_t total = desired.size(), done = 0;
    for (const auto& [key, owner] : desired) {
        ++done;
        const auto* m = find(owner.first);
        auto dst = sdRoot_ / fs::path(owner.second);
        std::error_code ec;
        if (deployed_.count(key) && fs::exists(dst, ec)) continue;
        if (fs::exists(dst, ec) && !deployed_.count(key)) backupFile(owner.second);
        if (progress) progress(done, total, owner.second);
        copyFileCreatingDirs(modDir(*m) / "sd" / fs::path(owner.second), dst);
        deployed_[key] = owner.first;
    }
}

void Library::removeOutput(GameState& gs) {
    if (gs.output.empty()) return;
    removeFileAndPrune(sdRoot_ / fs::path(gs.output), sdRoot_);
    restoreBackup(gs.output);
    gs.output.clear();
    gs.outputKey.clear();
    gs.outputSize = 0;
    gs.status = {};
}

void Library::applyCpk(Game game, bool force, const ProgressFn& progress) {
    auto& gs = gameState(game);
    const auto target = outputTarget(game, gs.mode);
    if (!gs.output.empty() && toLower(gs.output) != toLower(target)) removeOutput(gs);

    std::vector<cpk::Source> sources;
    std::string key = std::string(deployModeKey(gs.mode)) + "\n";
    std::error_code ec;
    bool any = false;
    BuildStatus status;
    if (gs.mode == DeployMode::Patch1) {
        if (hasOriginalPatch(game)) {
            auto orig = originalPatchCpk(game);
            for (const auto& e : cpk::read(orig).entries) sources.push_back({e.path(), {}, orig, e});
            key += "original " + std::to_string(fs::file_size(orig, ec)) + "\n";
            status.withOriginals = true;
        } else {
            status.missingOriginals = true;
        }
    }
    for (const auto* m : enabledAscending(game)) {
        auto dir = modDir(*m);
        for (const auto& b : m->bundles) {
            auto path = dir / "cpk" / b;
            for (const auto& e : cpk::read(path).entries) sources.push_back({e.path(), {}, path, e});
            key += m->id + " " + std::to_string(m->installedAt) + " cpk " + b + " " +
                   std::to_string(fs::file_size(path, ec)) + "\n";
            any = true;
        }
        for (const auto& p : m->cpkFiles) {
            auto file = dir / "files" / storedPath(p);
            sources.push_back({p, file, {}, {}});
            key += m->id + " " + std::to_string(m->installedAt) + " " + p + " " +
                   std::to_string(fs::file_size(file, ec)) + "\n";
            any = true;
        }
    }
    if (!any) {
        removeOutput(gs);
        return;
    }
    auto keyHash = std::to_string(stableId(key));
    auto dst = sdRoot_ / fs::path(target);
    if (!force && keyHash == gs.outputKey && gs.output == target && fs::exists(dst, ec) &&
        fs::file_size(dst, ec) == gs.outputSize)
        return;

    // Something we didn't build is where the archive goes (mods copied by hand): keep it.
    if (gs.output != target && fs::exists(dst, ec)) backupFile(target);

    uint64_t size = cpk::predictSize(sources);
    if (freeSpace_) {
        uint64_t free = freeSpace_();
        uint64_t existing = fs::exists(dst, ec) ? fs::file_size(dst, ec) : 0;
        if (free && size + (16ull << 20) > free + existing) throw NotEnoughSpaceError(size, free + existing);
        if (free && size + (16ull << 20) > free) fs::remove(dst, ec);  // room only once the old one is gone
    }
    auto tmp = dst;
    tmp += ".tmp";
    const std::string what = "Building " + dst.filename().string();
    auto result = cpk::build(tmp, sources, {}, [&](uint64_t done, uint64_t total) {
        if (progress) progress(done, total, what);
        return true;
    });
    fs::remove(dst, ec);
    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove(tmp, ec);
        throw std::runtime_error("couldn't put the new " + dst.filename().string() + " in place");
    }
    gs.output = target;
    gs.outputKey = keyHash;
    gs.outputSize = result.bytes;
    status.target = target;
    status.files = result.files;
    status.bytes = result.bytes;
    gs.status = status;
}

void Library::apply(const ProgressFn& progress) {
    applySdFiles(progress);
    for (const auto& info : kGames) applyCpk(info.game, false, progress);
    save();
}

void Library::rebuild(Game game, const ProgressFn& progress) {
    applySdFiles(progress);
    applyCpk(game, true, progress);
    save();
}

std::string Library::unmanagedOutput(Game game) const {
    auto target = outputTarget(game, mode(game));
    auto* gs = gameStateIf(game);
    std::error_code ec;
    if ((gs && gs->output == target) || !fs::exists(sdRoot_ / fs::path(target), ec)) return {};
    return target;
}

const ModRecord& Library::importUnmanagedOutput(Game game, const std::string& name, const ProgressFn& progress) {
    auto target = unmanagedOutput(game);
    if (target.empty()) throw std::runtime_error("there's nothing to import");
    ModRecord rec;
    rec.id = "local-existing-" + slugify(fs::path(target).filename().string());
    rec.game = game;
    rec.name = name;
    rec.sourceFile = fs::path(target).filename().string();
    rec.format = formatName(ModFormat::ReadyCpk);
    rec.enabled = true;
    auto dir = modDir(rec);
    auto file = std::string("0_") + fatSafeName(fs::path(target).filename().string());
    moveFile(sdRoot_ / fs::path(target), dir / "cpk" / file);
    rec.bundles.push_back(file);
    // Lowest priority: what was there before goes under everything installed since.
    for (auto& m : mods_)
        if (m.game == game) ++m.priority;
    rec.priority = 1;
    if (auto* existing = findMut(rec.id))
        *existing = rec;
    else
        mods_.push_back(rec);
    bundleCache_.clear();
    apply(progress);
    return *find(rec.id);
}

}  // namespace mm
