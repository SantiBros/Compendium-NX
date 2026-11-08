# Changelog

All notable changes to Compendium. Versions 0.0.x are test versions: 0.1.0 will be the first one that works
without problems on a Switch.

## [0.0.4]

**More mods end up where the game looks for them.** After 0.0.3, the file lists of every Persona 5 Royal (Switch)
mod on GameBanana (82 mods, 126 files) went through the app's rules, to find the ones that would go to the wrong
place, like the opening mod did. These are fixed.

### Fixed
- **Folders named after the PC version's CPKs** (`BASE.CPK/`, `EN.CPK/`), which some Aemulus packages have, ended
  up inside `BASE/` (`BASE/BASE.CPK/MODEL/…`), where the game never looks. They're the game's `BASE/` and `EN/`
  folders ("Persona 25th Anniversary Decals").
- **English voices in `SOUND_E/`** went to `BASE/SOUND_E/…`. `SOUND_E` is one of the game's own folders ("Gay
  Joker").
- **Files in the game's `BUSTUP/`, `CUTIN/`, `BUTTON/`, `SCHEDULER/`… folders without their CPK folder** weren't
  recognized and were left out ("Yoshizawa Hair Down"). `SOUND/` and `MOVIE/` without a CPK folder now go where the
  game has them (`BASE/SOUND/…`), not at the top of the archive.
- Mods installed with an earlier version whose files went to `BASE/BASE.CPK/` or `BASE/SOUND_E/` are fixed by
  themselves the first time 0.0.4 opens. Mods whose files were left out need to be installed again.

### Added
- **Exefs patches** that come as a `<build id>.ips` file without their folders ("Max Handheld Resolution") are
  installed in `atmosphere/exefs_patches/`.
- **Files that come without any folder** (`BGM.ACB` on its own) go where the game has a file with the same name, if
  it has it in one place only. This uses the game's own file list (the one-time L + R step).
- **Save files** (`DATA01/DATA.DAT`…) are recognized: instead of being installed as a mod, the app says to copy them
  with a save manager such as JKSV.
- **Emulator patches** (`.pchtxt`): the app says they only work on emulators, instead of "no game files".
- A note when an Aemulus package needs a PC script (`Prebuild.bat`) to prepare its files.

### Changed
- Hidden folders (`.git/`, a `.derp/` folder of PC tools) and source files for pictures and models (`.psd`…) are left
  out of the archive.
- When some files can't be placed, the warning says the app can place more once it has read the game's own files.

## [0.0.3]

**Mods made to be packed on a PC work.** Some mods come as a `PATCH1` folder plus CriWare's `cpkmakec` tool, for
building a `PATCH1.CPK` on a PC; the one that brings back Persona 5's opening is one of them. In 0.0.2 they
installed but never showed in the game.

### Fixed
- **Files of a `PATCH1` folder that holds `EN/`, `BASE/`… go where the game looks for them.** The app kept the
  `PATCH1` folder in their paths (`PATCH1/EN/MOVIE/MOV000.USM` instead of `EN/MOVIE/MOV000.USM`), so the game never
  found them. Mods installed with 0.0.2 are fixed by themselves the first time 0.0.3 opens.
- **A mod's file is no longer hidden by the game's own copy in `PATCH1/`.** The game looks a file up in `PATCH1/`
  first, then in the language's folder, then in `BASE/`. When a mod brings a file in `EN/` or `BASE/` (Persona 5's
  opening is `EN/MOVIE/MOV000.USM`) and the game's update has it in `PATCH1/` (Royal's opening), the game's copy is
  left out of the archive, as it is when such a mod is packed on a PC. With p5rcbt, the mod's file is also put in
  `PATCH1/`. A mod higher in the list that brings its own `PATCH1/` copy still wins.

### Changed
- CriWare's packing tool that some mods ship (`cpkmakec.exe`, its DLLs and `.bat`) is recognized and skipped,
  instead of being reported as unknown files.

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
