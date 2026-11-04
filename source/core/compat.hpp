#pragma once

#include <string>
#include <vector>

#include "games.hpp"

namespace mm {

// Pull game versions out of a mod page or file description, e.g. "only tested on
// version 1.0.2" -> {"1.0.2"}. "1.2.x" is returned as "1.2", and a range ("Version 1.1.0 - 1.4.3")
// as one entry, "1.1.0-1.4.3". Only versions the game could plausibly have are returned.
//
// strict: only numbers next to a game word (TotK, BotW, game, update, patch) count. Use it
//         for free-form mod descriptions, which are full of changelog numbers.
// Otherwise a leading "v" or the word "version" also counts (file labels like "v1.2.1"),
// unless versionWordIsModVersion says this mod uses "Version x" for its own releases.
std::vector<std::string> mentionedGameVersions(const std::string& text, Game game, bool strict = false,
                                               bool versionWordIsModVersion = false);

// True if a mod's file labels use "Version x.y" for the mod's own releases (some number
// that can't be a game version, like "Version 2.6.1").
bool versionWordIsModVersion(const std::vector<std::string>& labels, Game game);

// "All Game Update Versions", "works on any version", ...
bool saysAllVersions(const std::string& text);

enum class Compat { Unknown, Likely, Mismatch };

// Compare the installed game version (e.g. "1.2.1") against versions a mod names.
// A mod naming "1.2" matches any 1.2.x; a range "1.1.0-1.4.3" anything from one to the other.
Compat checkCompat(const std::string& installedVersion, const std::vector<std::string>& named);

// Options named per game version ("Defogger (Game Version 1.1.2)", "… 1.2.0") with none for
// the installed version: the option for the newest version not newer than it, if any.
// Needs at least two options naming versions, so a lone version number isn't over-read.
// Returns options.size() when there's nothing sensible to pick.
size_t closestVersionOption(const std::vector<std::string>& options, Game game, const std::string& installedVersion);

// True if file `own` looks like an add-on or variant of file `other` from the same mod
// ("dark_title_screen_defogger.zip" of "defogger.zip"), as opposed to just another version
// of it ("fierce_deity_v102.7z" of "fierce_deity.7z"). Compares names without extensions.
bool isAddOnFileOf(const std::string& own, const std::string& other);

// True for downloads that put the game's original files back ("uninstall_helm_pack.rar",
// "vanilla_files.zip"), meant for undoing a hand-copied install. The manager undoes mods itself.
bool isRestoreOriginalsFile(const std::string& filename, const std::string& description = "");

// How well a file's label suits the console's language (its two-letter code, "en", "de"…):
// 1 if it names that language, -1 if it names another, 0 if it names none.
int languageMatch(const std::string& text, const std::string& language);

}  // namespace mm
