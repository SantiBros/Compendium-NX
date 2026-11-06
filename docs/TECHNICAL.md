# How Compendium works

[← Back to the README](../README.md)

Compendium is a Switch homebrew app (libnx, with a [Borealis](https://github.com/xfangfang/borealis) interface) built
on [Patchwork](https://github.com/TheRomeoAlfa/patchwork-nx)'s GameBanana client, downloader and archive handling.
What's new is the Persona side: recognizing what a Persona mod archive holds, keeping a library of mods with a load
order, and writing the CRI CPK archive the game reads, on the console.

## Why a CPK

Persona 5 Royal on Switch keeps its files inside CRI CPK archives in its romfs: `CPK/ALL_USEU.CPK`, `CPK/EN.CPK`,
and from the update `CPK/PATCH1.CPK`, which the game reads with the highest priority. Atmosphère's LayeredFS swaps
whole files of a game's romfs, so a mod can't just put `BASE/BATTLE/GUI/BTL_UI.PAK` on the SD card: the game never
looks for it there. It has to be inside a CPK the game opens, and the usual one is
`atmosphere/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK`.

That leaves two problems the app solves:

- **Building the CPK.** On a PC, Aemulus or CriPakTools pack the mod's files into one. Compendium has its own CPK
  writer, so it happens on the Switch.
- **Several mods.** There's only one `PATCH1.CPK`, so mods that each ship their own would replace each other, and a
  `PATCH1.CPK` made only of mods drops the game's own PATCH1 files. The app builds one archive from every mod that's
  on, in the load order, plus the game's own PATCH1 files.

The other route is [p5rcbt](https://github.com/Raytwo/p5rcbt), a Skyline plugin that makes the game also read CPKs
and folders from `sd:/p5r/bind/`. There the app builds `p5r/bind/Compendium.cpk` and leaves PATCH1.CPK alone. p5rcbt
patches the game's code, so it only works with version 1.0.2.

## Pipeline

```
 GameBanana · GitHub · a link · inbox/ · SimpleModManager
        │  download (libcurl, Mbed TLS)
        ▼
 archive ──unpack (libarchive: zip, 7z, rar; archives inside archives too)──▶ work folder
        │
        ▼  detectPersonaLayout()                       source/core/persona_layout.cpp
 options (variants), each with: CPK files · CPKs · SD card files · notes
        │  the player picks options; mergeVariants()   source/core/installer.cpp
        ▼
 Library::add()  copy into library/p5r/<id>/            source/core/library.cpp
        │
        ▼  Library::apply()
 every enabled mod, lowest priority first, then cpk::build()   source/core/cpk.cpp
        ▼
 atmosphere/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK   (or p5r/bind/Compendium.cpk)
```

## The CPK reader and writer

`source/core/cpk.cpp` reads and writes CRI's format. It was written for the app from the format's public
descriptions (ShrineFox's documentation and PyCriCodecs), and checked by reading back what it writes.

A CPK starts with a `"CPK "` chunk holding the `CpkHeader` table, then a `"TOC "` chunk holding `CpkTocInfo`, one row
per file, then the files' data. The chunks have little-endian headers; the tables inside them are `@UTF` tables,
big-endian, with a column list (name, type, and whether the value is stored once for the whole table or per row),
the rows, a string pool and a data pool. Some files are compressed with CRILAYLA, a backwards LZ scheme with a
256-byte uncompressed prefix.

**Reading** gets the header's `TocOffset`, `ContentOffset`, `Align`, `CpkMode` and `Tvers` (the tool that built it),
then each TOC row's `DirName`, `FileName`, `FileSize` (stored), `ExtractSize` and `FileOffset` (relative to the
TOC). Entries whose stored and extracted sizes differ are CRILAYLA and are decompressed when extracted. Damaged
tables are refused with a message rather than read past their end.

**Writing** makes what CRI's `cpkmakec -mode=FILENAME` makes, which is what Aemulus and CriPakTools build for P5R on
Switch:

```
0x000   "CPK " chunk: CpkHeader (CpkMode 1, Align 0x800, Tvers "Compendium"), padded, "(c)CRI" at its end
0x800   "TOC " chunk: CpkTocInfo, one row per file, sorted the way CRI's tools sort paths
...     the files, each padded to 0x800
```

Files come either from disk (a mod's loose files, stored uncompressed) or from another CPK (the game's PATCH1.CPK, a
mod's own CPK), copied byte for byte, compressed or not, with their sizes. When several sources have the same path
(compared case-insensitively), the last one wins: the app passes the game's files first and the mods from the
lowest priority to the highest. The data is streamed, so only the tables are in memory, whatever the archive's
size. The size is worked out before writing (`predictSize`) to check the free space first, and the archive is
written to `PATCH1.CPK.tmp` and renamed into place, so a failed build never leaves half a CPK where the game reads.

## Recognizing mods

Mod authors pack their work in many shapes. `detectPersonaLayout()` walks the unpacked archive and decides, for each
folder that looks like a mod, what goes where:

| It finds | It reads it as |
|---|---|
| `Package.xml` or `Mod.xml` | An **Aemulus package**: the files under it are at their CPK paths (`BASE/…`, `EN/…`, `PATCH1/…`). Each package in an archive is an option the player can pick. |
| A `.cpk` | A **ready-made CPK**: its entries are used as they are (and merged with other mods). |
| `atmosphere/contents/01005CA01580E000/…` | An **Atmosphère / SimpleModManager folder**: `romfs/CPK/*.CPK` and `romfs/CPK/PATCH1/…` go into the CPK; cheats, `exefs` patches and Skyline plugins are copied to the SD card. |
| `ModConfig.json` with `P5REssentials/CPK/<name>.CPK/…` | A **Reloaded-II PC mod**: the files under the CPK folders are used; `.dll` code and FEmulator folders are left out with a note. |
| `p5r/bind/…` | A **p5rcbt folder**: packed into the CPK. |
| Folders named like the game's (`BASE`, `EN`, `PATCH1`, `SOUND`, `MOVIE`…), `romfs/CPK/PATCH1/`, `PATCH1.CPK_unpacked` | **Loose files**: packed at those paths. |

A file outside any known folder (`BTL_UI.PAK` on its own) is looked up in the game's file list, read during the
one-time step: if the game has a file with that name, the mod's goes to the same path (`PATCH1/` first, then `EN/`,
then `BASE/`…); otherwise it's placed under `BASE/` and the player is told.

Parts that need a PC tool to merge (Aemulus' `.flow`, `.msg`, `.tblpatch`, sprite and binary patches) are left out
with a note, so the rest of the mod still installs. An archive with no game files at all (a mod for another game,
a tool) is refused with what was found.

On GameBanana, each mod usually has several files. `source/core/compat.cpp` reads the file names and descriptions
for game versions ("1.0.2", "v1.0.0", "all versions"), languages and add-on wording, to mark the file that fits the
installed game (read from the system with `nsGetApplicationControlData`) and pick sensible defaults.

## The library

`mm::Library` keeps every installed mod under `switch/Compendium/library/p5r/<id>/`, whether it's on or off:

```
library/p5r/<id>/files/<CPK path>   the mod's loose files
library/p5r/<id>/cpk/<name>.cpk     CPKs it came with
library/p5r/<id>/sd/<SD path>       files that go straight to the SD card
```

`state.json` holds each mod's record (name, author, source, GameBanana ids, the options installed, notes, on or off,
priority) and what was last built. Mods from GameBanana have the id `gb<mod>-<file>`, so installing the same file
again updates the mod in place, keeping its spot in the list.

`Library::apply()` makes the SD card match the list after every change:

1. **SD card files** (cheats, exefs, plugins) are copied for mods that are on and removed for mods that are off.
   A file that was already there and isn't the app's is moved to `backup/` first and put back when no mod needs
   that path any more.
2. **The CPK** is built from the game's own PATCH1 files (when they were read) and every mod that's on. A hash of
   what went in (each file's mod, path and size) is stored with it, so nothing is rebuilt when nothing changed.
   With no mod on, the built CPK is deleted and whatever was there before is put back.

Conflicts are worked out from the same lists: every CPK path or SD path that more than one enabled mod brings, with
the mods in priority order. The interface shows them in My mods and before installing.

A `PATCH1.CPK` that's on the SD card but wasn't built by the app (mods copied by hand, another tool) is never
overwritten: it's moved to `backup/`, or the player imports it as a mod at the bottom of the list.

## The game's own files

To keep the game's PATCH1 files, the app needs the game's `PATCH1.CPK`, which is inside the game's romfs. Homebrew
can open a game's romfs when it runs as that game: holding **R** while starting P5R opens the Homebrew Menu in the
game's place, and holding **L** too keeps Atmosphère from layering mods over it. The app then sees it's running with
P5R's program ID, mounts the romfs (`romfsMountFromCurrentProcess`), copies `CPK/PATCH1.CPK` to
`original/p5r/PATCH1.CPK`, and saves the list of every file in the game's CPKs to `original/p5r/files.txt` for the
path lookup above.

## The Switch side

- **Memory.** Opened with R + a game, homebrew gets nearly all of a game's memory; from the Album, a small applet
  heap. The app checks which (`appletGetAppletType`), uses fewer download threads in the small one, and catches
  running out of memory with a message that explains what to do.
- **Background work.** Downloads run on their own threads, and file work (unpacking, copying, building the CPK) on
  one file thread, so the interface keeps drawing and shows progress. Closing the app waits for the file job that's
  running, so an archive is never left half written.
- **Phone link.** A small HTTP server (`source/app/link_server.cpp`) on the first free port from 8080 serves a page
  with a text box, behind a random path shown in the QR code. It only runs while that screen is open.
- **Mod loader crashes.** When Atmosphère can't load a game's mods for lack of memory, it writes a crash report for
  program `010041544D530000`. The app looks for new ones at start and explains the usual cause, background
  sysmodules, which Settings lists and can turn off (a known list says which ones are needed for some games).
- **Logging.** `switch/Compendium/log.txt` gets a line for every install, build and error, flushed at once so it
  survives a crash; at 512 KB it becomes `log.old.txt`.

## Test builds

The core (`source/core/`) has no Switch or interface code: `comptests` checks it on Linux on every push, and
`compcli` runs it from the command line on real mods. The desktop build runs the whole interface on a computer
against a folder standing in for the SD card, with a small script language for screenshots
([BUILDING.md](BUILDING.md#desktop-test-build)).

## Source layout

```
source/core/            no Switch or interface code
  cpk.*                 CPK reading, writing, CRILAYLA
  persona_layout.*      recognizing mod archives
  installer.*           unpacking an archive, merging the chosen options
  library.*             the mod library, load order, conflicts, building, backups
  games.*               the supported (and planned) games
  gamefiles.*           the game's file list (path lookup)
  compat.*              game versions, languages and add-ons in file names
  p5rcbt.*              p5rcbt's settings file and release
  smm.*                 SimpleModManager's mods folder
  sysmodules.*          background add-ons in atmosphere/contents
  gamebanana.*          GameBanana's API (from Patchwork)
  http_curl.*, archive.*, md5.*, util.*   downloads, archives, files (from Patchwork)
  log.*                 the log file
source/app/             the Borealis interface
  main_activity.*       the window, sidebar and pages
  browse_page.*         Browse and Search
  install_flow.*        choosing a file and options, installing
  add_mods_page.*       Add mods (inbox, SimpleModManager, links, mod numbers)
  my_mods_page.*        My mods
  settings_page.*       Settings
  link_server.*         the phone link
  app_state.*           settings, the library, background work
  platform.*            the Switch-specific parts (and their desktop stand-ins)
  debug_script.*        scripted runs of the desktop build
source/third_party/     nlohmann/json, Nayuki's QR Code generator
library/borealis/       the interface library (submodule)
tests/                  comptests
tools/compcli.cpp       the core from the command line
tools/art/make_art.py   the README's art, the icon and the pictures for announcing a version
tools/make_release.sh   packs a release's zip (switch/Compendium/…)
tools/publish_release.sh  creates and updates the GitHub releases (run by CI)
tools/package/          README.txt for the release zip
resources/              romfs: CA certificates, third-party notices, icon
```
