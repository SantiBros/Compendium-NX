#include "smm.hpp"

#include <algorithm>

namespace mm::smm {

std::vector<FoundMod> findMods(const fs::path& sdRoot, Game game) {
    std::vector<FoundMod> out;
    std::error_code ec;
    auto mods = sdRoot / "mods";
    if (!fs::is_directory(mods, ec)) return out;
    const auto tid = toLower(gameInfo(game).titleId);
    for (const auto& g : fs::directory_iterator(mods, ec)) {
        if (!g.is_directory()) continue;
        std::error_code ec2;
        for (const auto& m : fs::directory_iterator(g.path(), ec2)) {
            if (!m.is_directory()) continue;
            bool forGame = false;
            std::error_code ec3;
            for (const auto& c : fs::directory_iterator(m.path() / "contents", ec3))
                if (c.is_directory() && toLower(c.path().filename().string()) == tid) forGame = true;
            if (!forGame) {
                std::error_code ec4;
                for (const auto& c : fs::directory_iterator(m.path(), ec4))
                    if (c.is_directory() && toLower(c.path().filename().string()) == tid) forGame = true;
            }
            if (forGame) out.push_back({g.path().filename().string(), m.path().filename().string(), m.path()});
        }
    }
    std::sort(out.begin(), out.end(), [](const FoundMod& a, const FoundMod& b) { return a.name < b.name; });
    return out;
}

}  // namespace mm::smm
