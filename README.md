<div align="center">

[![English](https://img.shields.io/badge/lang-English-d62027?style=for-the-badge)](README.md)
[![Español](https://img.shields.io/badge/idioma-Espa%C3%B1ol-121212?style=for-the-badge)](README.es.md)

<img src="resources/img/icon.jpg" width="128" alt="Compendium icon">

# Compendium

*A mod manager for Persona games on Switch, named after the Compendium, where the games keep your Personas to summon again.*

**Install and manage mods for Persona games on a modded Nintendo Switch, right on the console. No PC needed.**<br>
Browse GameBanana, download a mod, and the app packs it into the game's files for you: it builds the
`.CPK` archive Persona 5 Royal reads, on the Switch itself.

[![Latest release](https://img.shields.io/github/v/release/SantiBros/Compendium-NX?include_prereleases&style=for-the-badge&label=download&color=d62027&logo=github&logoColor=white)](../../releases)
![Status: testing](https://img.shields.io/badge/status-testing-fac775?style=for-the-badge)
![Nintendo Switch homebrew](https://img.shields.io/badge/Nintendo%20Switch-homebrew-121212?style=for-the-badge)
![Atmosphère](https://img.shields.io/badge/CFW-Atmosph%C3%A8re-121212?style=for-the-badge)
[![License: GPL v3](https://img.shields.io/badge/license-GPL%20v3-121212?style=for-the-badge)](LICENSE)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-support-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/santibros)

[Install](#how-to-install) · [Using it](#using-it) · [Mod formats](#which-mods-work) · [How mods are loaded](#how-mods-reach-the-game) · [Troubleshooting](#troubleshooting) · [Building](#building) · [Credits](#credits)

</div>

> [!IMPORTANT]
> **No game files are included, in this repository or in the releases.** You need your own copy of the game.
> Mods belong to their authors and are downloaded from where they publish them.

## Games

| Game | Status |
|---|---|
| **Persona 5 Royal** | ✅ Supported (this version) |
| Persona 4 Golden, Persona 3 Portable, Persona 5 Strikers, Persona 5 Tactica, Persona 4 Arena Ultimax | Coming later |

## ⚠️ Before you start

- **Use at your own risk.** A bad or incompatible mod can crash the game. Back up your saves (for example with JKSV).
- **Going online with custom firmware can get your console banned.** Mod on an emuMMC and keep Nintendo's servers blocked (for example with 90DNS).
- **Turning mods off always works:** turn them off in My mods, or hold **L** while starting the game to play one launch without any mods.

## Features

- **Browse and search GameBanana** from the console: Persona 5 Royal's **Switch** mods, and its **PC** mods (switch with L/R), with pictures and descriptions.
- **Builds the CPK on the console.** Your mods are packed into the game's `PATCH1.CPK` (or a CPK for p5rcbt), keeping the game's own PATCH1 files.
- **Many mod formats:** Aemulus packages, ready-made `PATCH1.CPK` files, Atmosphère / SimpleModManager folders, p5rcbt folders and loose game files ([below](#which-mods-work)).
- **PC mods adapted:** Reloaded-II mods for the PC version (P5R Essentials) are converted for the Switch when you install them.
- **Load order and conflicts:** when two mods change the same file, the one higher in My mods wins, and the app tells you which files clash.
- **On/off without deleting:** a copy of every mod is kept, so turning one off rebuilds the game's files without it.
- **Add mods from anywhere:** GameBanana (browse, or type a mod's number), GitHub projects (their latest release), a link sent from your phone (scan a QR code), or archives copied to `switch/Compendium/inbox`.
- **Brings in your existing mods:** mods kept for **SimpleModManager** (`sd:/mods/…`) and a `PATCH1.CPK` you copied by hand can be added to the list.
- **p5rcbt support:** install the p5rcbt Skyline plugin from Settings and change its options (60 FPS, the PC version's settings menu).
- **Keeps your files safe:** anything already on the SD card where the app writes is set aside and put back when your mods are off.

## How to install

1. Download **`Compendium.nro`** from the [releases page](../../releases) (not "Source code").
2. Copy it to `sd:/switch/Compendium/Compendium.nro`.
3. Open the Homebrew Menu and start **Compendium**. For the most memory, open the Homebrew Menu by holding **R** while starting a game, rather than from the Album.

To update, replace the `.nro`. Your mods and settings are kept.

### One-time step: the game's own files (recommended)

`PATCH1.CPK` comes with the game's update and holds a few of its files. So the app can keep them when it builds
the one for your mods, let it read them once (and again after a game update):

1. Press **+** to close the app.
2. Hold **L and R** and start Persona 5 Royal. Keep holding until the Homebrew Menu opens.
   (R opens the Homebrew Menu in place of the game; L turns mods off for that launch.)
3. Open Compendium. It reads the files by itself and tells you when it's done.

It also reads the game's file list, which it uses to place files of mods that leave out their CPK folder.

## Using it

| Page | What it does |
|---|---|
| **Browse** | GameBanana's Persona 5 Royal mods. **L/R**: Switch mods or PC mods · **Y**: change the order · **X**: search · **A**: install |
| **Search** | Search by name or author, or type a mod's number (the end of its GameBanana link) |
| **Add mods** | Files in `switch/Compendium/inbox`, SimpleModManager's mods, a `PATCH1.CPK` copied by hand, a link from your phone, or a mod number |
| **My mods** | Your load order (top wins). **A**: on/off · **L/R**: move up/down · **X**: remove |
| **Settings** | How mods are loaded, the game's own files, p5rcbt, background add-ons, storage |

## Which mods work

| What the mod comes as | What the app does |
|---|---|
| **Aemulus package** (a folder with `Package.xml`), the usual format for P5R Switch mods | Its files go into the CPK at the paths they have, as Aemulus does |
| **A ready-made `PATCH1.CPK`** or any mod `.cpk` | Its files are used as they are, merged with your other mods |
| **Atmosphère or SimpleModManager folders** (`atmosphere/contents/01005CA01580E000/…`) | The CPK inside is merged; cheats, exefs patches and plugins go to the SD card |
| **Loose game files** (`BASE/…`, `EN/…`, `PATCH1/…`, Aemulus' `romfs/CPK/PATCH1/…`, CriPakTools' `PATCH1.CPK_unpacked`) | Packed into the CPK |
| **p5rcbt folders** (`p5r/bind/…`) | Packed into the CPK |
| **PC mods for Reloaded-II** (`ModConfig.json`, `P5REssentials/CPK/…`) | Adapted: their game files are packed into the CPK. Most PC mods' files work on Switch too, but not all |

Not supported yet (the app says so when you install): parts that Aemulus merges on a PC (`.flow`, `.msg`,
`.tblpatch`, sprite and binary patches), Reloaded-II's FEmulator merging, and PC code mods (`.dll`), which can't
run on the Switch. The rest of such a mod is still installed.

## How mods reach the game

Choose in **Settings → Load mods with**:

- **PATCH1.CPK (no plugin)**, the default. The app builds `atmosphere/contents/01005CA01580E000/romfs/CPK/PATCH1.CPK`
  with your mods and the game's own PATCH1 files, and Atmosphère's LayeredFS loads it. Works on any game version.
- **p5rcbt (Skyline plugin).** The app builds `p5r/bind/Compendium.cpk`, which [p5rcbt](https://github.com/Raytwo/p5rcbt)
  adds to the game's files. The game's PATCH1.CPK stays untouched. p5rcbt only works with game version **1.0.2**;
  install it from Settings.

## Where things are on the SD card

| Path | What |
|---|---|
| `switch/Compendium/Compendium.nro` | the app |
| `switch/Compendium/library/` | a copy of every installed mod |
| `switch/Compendium/inbox/` | put archives here to install them from Add mods |
| `switch/Compendium/original/` | the game's own PATCH1.CPK and file list |
| `switch/Compendium/backup/` | your own files the app set aside (put back when mods are off) |
| `switch/Compendium/state.json` | what's installed and built |
| `switch/Compendium/log.txt` | what the app did and any errors: send it with bug reports |

## Troubleshooting

- **A mod doesn't show in the game:** check it's on in My mods and above mods that change the same files. Then Settings → *Build the game's mod files again*.
- **The game crashes at start after installing a mod:** turn off the mods you added last, one at a time. Check the mod's page: some are for another game version, or for PC only.
- **"Couldn't reach GameBanana" / secure connection errors:** check the Switch's date and time, then your Wi-Fi.
- **Atmosphère error with Program ID 010041544D530000:** usually background add-ons using the memory Atmosphère needs to load mods. Turn them off in Settings and restart.
- **p5rcbt mode does nothing:** p5rcbt needs game version 1.0.2; check that it's installed and on in My mods.
- **Something goes wrong:** please open an [issue](../../issues) and attach `switch/Compendium/log.txt` (and, if the app crashed, the newest file from `atmosphere/crash_reports`).

## Building

Switch build: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `switch-dev` and `switch-portlibs`.

```bash
git clone --recursive https://github.com/SantiBros/Compendium-NX
cd Compendium-NX
export DEVKITPRO=/opt/devkitpro
cmake -S . -B build-switch -DPLATFORM_SWITCH=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch --target Compendium.nro
```

Core and tests on a computer (needs CMake, libarchive and curl): `cmake -S . -B build && cmake --build build && ./build/comptests`.
`build/compcli` lists, extracts and packs CPKs, and shows how a mod archive would be installed.

## Credits

- [**Patchwork**](https://github.com/TheRomeoAlfa/patchwork-nx) by TheRomeoAlfa: this app is based on it (GameBanana client, downloads, archives, interface).
- [**SimpleModManager**](https://github.com/nadrino/SimpleModManager) by nadrino: its mod folders can be brought in, and its approach to managing LayeredFS mods.
- [**p5rcbt**](https://github.com/Raytwo/p5rcbt) by Raytwo: the optional plugin.
- [ShrineFox](https://docs.shrinefox.com), [Aemulus](https://github.com/TekkaGB/AemulusModManager) and [PyCriCodecs](https://github.com/Youjose/PyCriCodecs): documentation of P5R's mod formats and CRI's CPK format.
- [Borealis](https://github.com/xfangfang/borealis), libcurl, libarchive and the other libraries listed in [THIRD_PARTY_NOTICES](resources/THIRD_PARTY_NOTICES.txt).

Persona is a trademark of ATLUS / SEGA. This is an unofficial fan project, not affiliated with or endorsed by
ATLUS, SEGA, Nintendo or GameBanana.

## License

[GNU GPL v3](LICENSE), like Patchwork and SimpleModManager it builds on.

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/santibros)
