#include "log.hpp"

#include <cstdio>
#include <mutex>

namespace mm::log {

namespace {
std::mutex mutex;
std::FILE* out = nullptr;
fs::path path;
uint64_t limit = 0;
uint64_t written = 0;
std::function<std::string()> clockFn;

void rotateIfFull() {
    if (!out || written < limit) return;
    std::fclose(out);
    out = nullptr;
    auto old = path.parent_path() / "log.old.txt";
    std::error_code ec;
    fs::remove(old, ec);
    fs::rename(path, old, ec);
    out = std::fopen(path.string().c_str(), "ab");
    written = 0;
}
}  // namespace

void open(const fs::path& file, uint64_t maxBytes) {
    std::lock_guard lock(mutex);
    if (out) std::fclose(out);
    path = file;
    limit = maxBytes;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    written = fs::exists(file, ec) ? fs::file_size(file, ec) : 0;
    out = std::fopen(file.string().c_str(), "ab");
    rotateIfFull();
}

void close() {
    std::lock_guard lock(mutex);
    if (out) std::fclose(out);
    out = nullptr;
}

void setClock(std::function<std::string()> clock) {
    std::lock_guard lock(mutex);
    clockFn = std::move(clock);
}

void write(const char* level, const std::string& text) {
    std::lock_guard lock(mutex);
    if (!out) return;
    std::string line;
    if (clockFn) line = clockFn() + " ";
    line += std::string("[") + level + "] " + text;
    // One entry per line: a message with line breaks is indented after the first.
    std::string clean;
    for (char c : line) {
        if (c == '\r') continue;
        clean += c;
        if (c == '\n') clean += "    ";
    }
    clean += "\n";
    std::fwrite(clean.data(), 1, clean.size(), out);
    std::fflush(out);  // the app may be closed or crash at any time
    written += clean.size();
    rotateIfFull();
}

}  // namespace mm::log
