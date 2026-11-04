#include "platform.hpp"

#include <sys/statvfs.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "cpk.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace app::platform {

void init() {
#ifdef __SWITCH__
    nsInitialize();
#endif
}

void exit() {
#ifdef __SWITCH__
    nsExit();
#endif
}

mm::fs::path sdRoot() {
#ifdef __SWITCH__
    return "sdmc:/";
#else
    if (const char* env = std::getenv("MM_SD_ROOT")) return env;
    return mm::fs::current_path() / "sd";
#endif
}

std::string gameVersion(mm::Game game) {
#ifdef __SWITCH__
    auto data = std::make_unique<NsApplicationControlData>();
    u64 size = 0;
    u64 tid = std::stoull(std::string(mm::gameInfo(game).titleId), nullptr, 16);
    if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, data.get(), sizeof(*data), &size)))
        return "";
    return data->nacp.display_version;
#else
    (void)game;
    const char* env = std::getenv("MM_P5R_VERSION");
    return env ? env : "";
#endif
}

uint64_t freeSpaceBytes() {
#ifndef __SWITCH__
    if (const char* env = std::getenv("MM_FREE_SPACE")) return std::strtoull(env, nullptr, 10);
#endif
    struct statvfs st {};
    auto root = sdRoot().string();
    if (statvfs(root.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
}

std::string caBundle() {
#ifdef __SWITCH__
    return "romfs:/cacert.pem";
#else
    std::error_code ec;
    return mm::fs::exists("./resources/cacert.pem", ec) ? "./resources/cacert.pem" : "";
#endif
}

mm::fs::path resourcePath(const std::string& name) {
#ifdef __SWITCH__
    return mm::fs::path("romfs:/") / name;
#else
    return mm::fs::path("./resources") / name;
#endif
}

std::string userAgent() {
#ifdef __SWITCH__
    return "Mozilla/5.0 (Nintendo Switch; Compendium/" APP_VERSION ")";
#else
    return "Mozilla/5.0 (compatible; Compendium/" APP_VERSION ")";
#endif
}

bool runningAsGame(mm::Game game) {
#ifdef __SWITCH__
    u64 current = 0;
    u64 wanted = std::stoull(std::string(mm::gameInfo(game).titleId), nullptr, 16);
    return R_SUCCEEDED(svcGetInfo(&current, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0)) && current == wanted;
#else
    (void)game;
    return std::getenv("MM_GAME_ROMFS_P5R") != nullptr;
#endif
}

namespace {

// The game's romfs, mounted while this object lives.
struct GameFs {
    mm::fs::path root;
    explicit GameFs(mm::Game game) {
        if (!runningAsGame(game))
            throw std::runtime_error("Open the app by holding L and R while starting " +
                                     std::string(mm::gameInfo(game).displayName) + ", so it can read the game's files.");
#ifdef __SWITCH__
        Result rc = romfsMountFromCurrentProcess("gamefs");
        if (R_FAILED(rc))
            throw std::runtime_error("Couldn't open the game's files (error " + std::to_string(2000 + R_MODULE(rc)) + "-" +
                                     std::to_string(R_DESCRIPTION(rc)) + ").");
        root = "gamefs:/";
#else
        root = std::getenv("MM_GAME_ROMFS_P5R");
#endif
    }
    ~GameFs() {
#ifdef __SWITCH__
        romfsUnmount("gamefs");
#endif
    }
    GameFs(const GameFs&) = delete;
    GameFs& operator=(const GameFs&) = delete;
};

}  // namespace

void copyGameFile(mm::Game game, const std::string& romfsPath, const mm::fs::path& dest,
                  const std::function<void(uint64_t, uint64_t)>& progress) {
    GameFs gfs(game);
    // As text: "gamefs:/" isn't a root std::filesystem knows.
    auto from = gfs.root.string() + romfsPath;
    std::unique_ptr<FILE, int (*)(FILE*)> in(std::fopen(from.c_str(), "rb"), &std::fclose);
    if (!in) throw std::runtime_error("The game has no " + romfsPath + ".");
    std::fseek(in.get(), 0, SEEK_END);
    long size = std::ftell(in.get());
    std::fseek(in.get(), 0, SEEK_SET);
    mm::fs::create_directories(dest.parent_path());
    auto tmp = dest;
    tmp += ".tmp";
    {
        std::unique_ptr<FILE, int (*)(FILE*)> out(std::fopen(tmp.c_str(), "wb"), &std::fclose);
        if (!out) throw std::runtime_error("Couldn't write to the SD card.");
        std::vector<char> buf(1 << 20);
        uint64_t done = 0;
        for (size_t n; (n = std::fread(buf.data(), 1, buf.size(), in.get())) > 0;) {
            if (std::fwrite(buf.data(), 1, n, out.get()) != n) throw std::runtime_error("Couldn't write to the SD card (is it full?).");
            done += n;
            if (progress) progress(done, size > 0 ? uint64_t(size) : 0);
        }
        if (std::ferror(in.get())) throw std::runtime_error("Couldn't read " + romfsPath + ".");
    }
    std::error_code ec;
    mm::fs::remove(dest, ec);
    mm::fs::rename(tmp, dest, ec);
    if (ec) throw std::runtime_error("Couldn't keep the copy of " + romfsPath + ".");
}

std::vector<std::string> gameCpkFileList(mm::Game game, const std::string& skipCpk) {
    GameFs gfs(game);
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : mm::fs::directory_iterator(gfs.root / "CPK", ec)) {
        auto name = e.path().filename().string();
        if (!mm::endsWithI(name, ".cpk") || (!skipCpk.empty() && mm::iequals(name, skipCpk))) continue;
        for (const auto& entry : mm::cpk::read(e.path()).entries) out.push_back(entry.path());
    }
    if (out.empty()) throw std::runtime_error("Couldn't find the game's CPK files.");
    return out;
}

bool fullMemory() {
#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    return at == AppletType_Application || at == AppletType_SystemApplication;
#else
    return std::getenv("MM_APPLET") == nullptr;
#endif
}

std::tm localTime(int64_t unixSeconds) {
    std::tm tm{};
    std::time_t t = static_cast<std::time_t>(unixSeconds);
#ifdef __SWITCH__
    TimeCalendarTime cal{};
    TimeCalendarAdditionalInfo info{};
    if (unixSeconds > 0 && R_SUCCEEDED(timeToCalendarTimeWithMyRule(static_cast<u64>(unixSeconds), &cal, &info))) {
        tm.tm_year = cal.year - 1900;
        tm.tm_mon = cal.month - 1;
        tm.tm_mday = cal.day;
        tm.tm_hour = cal.hour;
        tm.tm_min = cal.minute;
        tm.tm_sec = cal.second;
        tm.tm_wday = static_cast<int>(info.wday);
        return tm;
    }
    gmtime_r(&t, &tm);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

}  // namespace app::platform
