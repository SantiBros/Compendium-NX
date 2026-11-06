# Changelog

All notable changes to Compendium. Versions 0.0.x are test versions: 0.1.0 will be the first one confirmed on
Switch consoles.

## Unreleased

### Changed
- **New icon**, in the same style as the README's art.
- The warning before installing a mod that changes the same files as several of yours now reads "A and B also
  change" instead of "A, B also change".

## 0.0.1 (first test version)

Not tested on a Switch yet: expect bugs. If something fails, send `switch/Compendium/log.txt`.

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
