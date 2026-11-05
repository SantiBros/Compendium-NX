#pragma once

// Compendium's log file (switch/Compendium/log.txt): what was installed, built and what went
// wrong, for bug reports. Kept small: when it passes the size limit it's renamed to
// log.old.txt and a new one is started. Safe to call from any thread; does nothing until
// open() is called (the tests and compcli don't write one).

#include <functional>
#include <string>

#include "util.hpp"

namespace mm::log {

void open(const fs::path& file, uint64_t maxBytes = 512 * 1024);
void close();
// Gives the time shown at the start of each line ("" = none).
void setClock(std::function<std::string()> clock);

void write(const char* level, const std::string& text);
inline void info(const std::string& text) { write("INFO", text); }
inline void warn(const std::string& text) { write("WARN", text); }
inline void error(const std::string& text) { write("ERROR", text); }

}  // namespace mm::log
