#include "gamefiles.hpp"

#include <algorithm>
#include <sstream>

#include "cpk.hpp"

namespace mm {

namespace {
// Which top folder wins when the game has a file in several: the update's, then English
// text and voices, then the shared data.
int rank(const std::string& full) {
    static const char* order[] = {"patch1/", "en/", "sound_e/", "base/", "sound/", "movie/", "fr/", "it/", "de/", "es/"};
    auto lower = toLower(full);
    for (int i = 0; i < 10; ++i)
        if (lower.rfind(order[i], 0) == 0) return i;
    return 10;
}

std::string restOf(const std::string& full) { return toLower(full.substr(full.find('/') + 1)); }
}  // namespace

bool GameFileList::load(const fs::path& listFile) {
    byRest_.clear();
    byName_.clear();
    count_ = 0;
    std::string text;
    try {
        text = readFile(listFile);
    } catch (const std::exception&) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto slash = line.find('/');
        if (slash == std::string::npos) continue;
        byRest_[toLower(line.substr(slash + 1))].push_back(line);
        byName_[toLower(line.substr(line.rfind('/') + 1))].push_back(line);
        ++count_;
    }
    auto byRank = [](const std::string& a, const std::string& b) { return rank(a) < rank(b); };
    for (auto& [k, v] : byRest_) std::stable_sort(v.begin(), v.end(), byRank);
    for (auto& [k, v] : byName_) std::stable_sort(v.begin(), v.end(), byRank);
    return count_ > 0;
}

void GameFileList::save(const fs::path& listFile, const std::vector<std::string>& paths) {
    std::string text;
    for (const auto& p : paths) text += p + "\n";
    fs::create_directories(listFile.parent_path());
    writeFileAtomic(listFile, text);
}

std::vector<std::string> GameFileList::fromCpk(const fs::path& cpkFile) {
    std::vector<std::string> out;
    for (const auto& e : cpk::read(cpkFile).entries) out.push_back(e.path());
    return out;
}

std::string GameFileList::resolve(const std::string& rest) const {
    auto key = cpk::pathKey(rest);
    auto slash = key.find('/');
    if (slash != std::string::npos)
        if (auto it = byRest_.find(key.substr(slash + 1)); it != byRest_.end())
            for (const auto& full : it->second)
                if (cpk::pathKey(full) == key) return full;
    if (auto it = byRest_.find(key); it != byRest_.end() && !it->second.empty()) return it->second.front();
    if (slash != std::string::npos) return {};
    auto it = byName_.find(key);
    if (it == byName_.end() || it->second.empty()) return {};
    // In one folder only (maybe under several top folders, like EN/ and BASE/): the one the game reads.
    auto first = restOf(it->second.front());
    for (const auto& full : it->second)
        if (restOf(full) != first) return {};
    return it->second.front();
}

}  // namespace mm
