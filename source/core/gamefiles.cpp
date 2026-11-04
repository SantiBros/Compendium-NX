#include "gamefiles.hpp"

#include <algorithm>
#include <sstream>

#include "cpk.hpp"

namespace mm {

namespace {
// Which top folder wins when the game has a file in several: the update's, then English
// text, then the shared data.
int rank(const std::string& full) {
    static const char* order[] = {"patch1/", "en/", "base/", "sound/", "movie/", "fr/", "it/", "de/", "es/"};
    auto lower = toLower(full);
    for (int i = 0; i < 9; ++i)
        if (lower.rfind(order[i], 0) == 0) return i;
    return 9;
}
}  // namespace

bool GameFileList::load(const fs::path& listFile) {
    byRest_.clear();
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
        ++count_;
    }
    for (auto& [k, v] : byRest_)
        std::stable_sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) { return rank(a) < rank(b); });
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
    auto it = byRest_.find(cpk::pathKey(rest));
    return it == byRest_.end() || it->second.empty() ? std::string() : it->second.front();
}

}  // namespace mm
