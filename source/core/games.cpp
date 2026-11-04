#include "games.hpp"

#include <stdexcept>

#include "util.hpp"

namespace mm {

const GameInfo& gameInfo(Game g) {
    for (const auto& info : kGames)
        if (info.game == g) return info;
    throw std::logic_error("unknown game");
}

std::optional<Game> gameFromKey(std::string_view key) {
    for (const auto& info : kGames)
        if (info.key == key) return info.game;
    return std::nullopt;
}

std::optional<Game> gameFromTitleId(std::string_view titleId) {
    for (const auto& info : kGames)
        if (iequals(info.titleId, titleId)) return info.game;
    return std::nullopt;
}

std::vector<std::string> titleIdsFor(Game game) { return {std::string(gameInfo(game).titleId)}; }

std::string contentsDir(Game game) { return "atmosphere/contents/" + std::string(gameInfo(game).titleId); }

}  // namespace mm
