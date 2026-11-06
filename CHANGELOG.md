# Changelog

All notable changes to Compendium. Versions 0.0.x are test versions: 0.1.0 will be the first one that works
without problems on a Switch.

## [0.0.2]

**Mods install on the Switch.** In 0.0.1 the app opened and browsed GameBanana on a Switch, but every install, of a
simple mod or a big one, stopped with "filesystem error: cannot make canonical path: Invalid argument". 0.0.2 fixes
that. And the release is now a zip with the `switch` folder ready to copy onto the SD card.

### Fixed
- **Installing a mod no longer fails on the Switch.** After unpacking a mod, the app worked out each file's path
  inside the archive with a function that asks the file system for the full, "canonical" path, which the Switch's
  SD card driver doesn't support. It works the paths out from their text now, and a check in the automated build
  keeps those functions out of the code. Mods that were downloaded by 0.0.1 install without downloading again.

### Changed
- **The release is a zip** (`Compendium-0.0.2.zip`) holding `switch/Compendium/` with the app, a short guide in
  English and Spanish (`README.txt`), the licenses and the `inbox` folder: copy the `switch` folder to the root of
  the SD card.
- **New icon**, in the same style as the README's art.
- The warning before installing a mod that changes the same files as several of yours reads "A and B also change"
  instead of "A, B also change".

## [0.0.1]

**The first test version.** It opened and browsed GameBanana's mods on a Switch, but installing mods failed (fixed
in 0.0.2).

### Added
- Persona 5 Royal: browse and search GameBanana's Switch and PC mods, download and install them on the console.
- Builds the game's `PATCH1.CPK` on the Switch from the mods that are on (keeping the game's own PATCH1 files once
  they've been read), or a CPK in `p5r/bind/` for the p5rcbt plugin.
- Installs Aemulus packages, ready-made CPKs, Atmosphère and SimpleModManager folders, p5rcbt folders and loose game
  files, and adapts Reloaded-II mods made for the PC version.
- Load order, conflict warnings, turning mods on and off without deleting them.
- Add mods from the SD card inbox, SimpleModManager's mods folder, a `PATCH1.CPK` copied by hand, GitHub releases, a
  GameBanana mod number, or a link sent from a phone.
- Installs and configures p5rcbt (60 FPS, PC settings menu).
- Writes a log file (`switch/Compendium/log.txt`) with what was installed and built, and any errors.
