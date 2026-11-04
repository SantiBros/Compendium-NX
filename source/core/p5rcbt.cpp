#include "p5rcbt.hpp"

#include <sstream>
#include <stdexcept>

#include "nlohmann/json.hpp"

namespace mm::p5rcbt {

Config readConfig(const fs::path& sdRoot) {
    Config c;
    std::string text;
    try {
        text = readFile(sdRoot / kConfigPath);
    } catch (const std::exception&) {
        return c;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            auto a = s.find_first_not_of(" \t\r");
            auto b = s.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        };
        auto key = trim(line.substr(0, eq)), value = toLower(trim(line.substr(eq + 1)));
        bool on = value == "true";
        if (key == "logging") c.logging = on;
        else if (key == "uncap_framerate") c.uncapFramerate = on;
        else if (key == "pc_settings") c.pcSettings = on;
    }
    return c;
}

void writeConfig(const fs::path& sdRoot, const Config& c) {
    auto b = [](bool v) { return v ? "true" : "false"; };
    std::string text = std::string("logging = ") + b(c.logging) + "\nuncap_framerate = " + b(c.uncapFramerate) +
                       "\npc_settings = " + b(c.pcSettings) + "\n";
    fs::create_directories((sdRoot / kConfigPath).parent_path());
    writeFileAtomic(sdRoot / kConfigPath, text);
}

bool installed(const fs::path& sdRoot, Game game) {
    std::error_code ec;
    auto dir = sdRoot / fs::path(contentsDir(game));
    if (!fs::exists(dir / "exefs" / "subsdk9", ec)) return false;
    auto plugins = dir / "romfs" / "skyline" / "plugins";
    if (!fs::is_directory(plugins, ec)) return false;
    for (const auto& e : fs::directory_iterator(plugins, ec)) {
        auto name = toLower(e.path().filename().string());
        if (name.find("cbt") != std::string::npos && endsWithI(name, ".nro")) return true;
    }
    return false;
}

Release parseRelease(const std::string& body) {
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) throw std::runtime_error("GitHub returned an invalid response");
    Release r;
    r.tag = j.value("tag_name", "");
    for (const auto& a : j.value("assets", nlohmann::json::array())) {
        auto name = a.value("name", "");
        if (endsWithI(name, ".zip")) {
            r.zipUrl = a.value("browser_download_url", "");
            r.size = a.value("size", int64_t(0));
            break;
        }
    }
    if (r.zipUrl.empty()) throw std::runtime_error("the latest p5rcbt release has no zip to download");
    return r;
}

}  // namespace mm::p5rcbt
