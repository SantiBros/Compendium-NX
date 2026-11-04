#pragma once
// Shared test helpers: tiny files, a reference CRILAYLA encoder, CPKs built for fixtures.

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "cpk.hpp"
#include "util.hpp"

namespace th {

namespace fs = std::filesystem;

inline void write(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << data;
}

inline std::string read(const fs::path& p) { return mm::readFile(p); }

// A CPK at `out` holding {path, contents} pairs (built with the code under test).
inline void makeCpk(const fs::path& out, const std::vector<std::pair<std::string, std::string>>& files,
                    const fs::path& scratch) {
    std::vector<mm::cpk::Source> src;
    int n = 0;
    for (const auto& [path, data] : files) {
        auto f = scratch / ("src" + std::to_string(n++));
        write(f, data);
        src.push_back({path, f, {}, {}});
    }
    mm::cpk::build(out, src);
}

// CRILAYLA encoder for tests, written from the decoder's rules: the stream is read from its
// last byte backwards, MSB first; the output is filled from its end.
inline std::vector<uint8_t> crilayla(const std::string& file) {
    std::vector<uint8_t> prefix(file.begin(), file.begin() + 0x100);
    std::vector<uint8_t> d(file.begin() + 0x100, file.end());
    std::vector<bool> bits;
    auto put = [&](uint32_t v, int n) {
        for (int i = n - 1; i >= 0; --i) bits.push_back((v >> i) & 1);
    };
    // Positions are indices into the full output (prefix + d); fill from the end.
    std::vector<uint8_t> full(prefix);
    full.insert(full.end(), d.begin(), d.end());
    long end = long(full.size()) - 1;
    long pos = end;
    while (pos >= 0x100) {
        long bestLen = 0, bestDist = 0;
        for (long dist = 0; dist < 8192 && pos + dist + 3 <= end; ++dist) {
            long len = 0;
            while (pos - len >= 0x100 && full[size_t(pos - len)] == full[size_t(pos - len + dist + 3)] && len < 300) ++len;
            if (len > bestLen) {
                bestLen = len;
                bestDist = dist;
            }
        }
        if (bestLen >= 3) {
            put(1, 1);
            put(uint32_t(bestDist), 13);
            uint32_t rest = uint32_t(bestLen - 3);
            static const int vle[4] = {2, 3, 5, 8};
            int level = 0;
            for (; level < 4; ++level) {
                uint32_t max = (1u << vle[level]) - 1;
                if (rest < max) {
                    put(rest, vle[level]);
                    rest = 0;
                    break;
                }
                put(max, vle[level]);
                rest -= max;
            }
            if (level == 4) {
                while (rest >= 255) {
                    put(255, 8);
                    rest -= 255;
                }
                put(rest, 8);
            }
            pos -= bestLen;
        } else {
            put(0, 1);
            put(full[size_t(pos)], 8);
            --pos;
        }
    }
    std::vector<uint8_t> packed((bits.size() + 7) / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i)
        if (bits[i]) packed[i / 8] |= uint8_t(0x80 >> (i % 8));
    std::vector<uint8_t> stream(packed.rbegin(), packed.rend());
    std::vector<uint8_t> out(0x10, 0);
    std::memcpy(out.data(), "CRILAYLA", 8);
    uint32_t usize = uint32_t(d.size()), hoff = uint32_t(stream.size());
    for (int i = 0; i < 4; ++i) {
        out[8 + i] = uint8_t(usize >> (8 * i));
        out[12 + i] = uint8_t(hoff >> (8 * i));
    }
    out.insert(out.end(), stream.begin(), stream.end());
    out.insert(out.end(), prefix.begin(), prefix.end());
    return out;
}

}  // namespace th
