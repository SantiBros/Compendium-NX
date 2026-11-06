# Building Compendium

[← Back to the README](../README.md)

The project builds three ways from one `CMakeLists.txt`:

| Build | What for | Output |
|---|---|---|
| **Switch** | The app players use | `build-switch/Compendium.nro` |
| **Core and tests** (Linux, macOS) | The CPK reader and writer, mod detection and the library, without a console | `build/comptests`, `build/compcli` |
| **Desktop test build** (Linux, macOS) | The whole app in a window, with a folder standing in for the SD card | `build-desktop/Compendium` |

Clone with the submodule (Borealis, the interface library):

```sh
git clone --recursive https://github.com/SantiBros/Compendium-NX
cd Compendium-NX
# already cloned without it:  git submodule update --init --recursive
```

## Building for Switch

You need [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `switch-dev` and `switch-portlibs` (which
bring libcurl, Mbed TLS, libarchive and its compression libraries, libwebp, GLFW and Mesa), and CMake 3.15 or
newer.

```sh
export DEVKITPRO=/opt/devkitpro
cmake -S . -B build-switch -DPLATFORM_SWITCH=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch --target Compendium.nro -j
```

The `.nro` takes its name, author and version from `CMakeLists.txt` and [`VERSION`](../VERSION), and its icon from
`resources/img/icon.jpg`. Everything in `resources/` (the CA certificates for HTTPS, the third-party notices, the
icon) goes into its romfs, with Borealis' own fonts and images.

The [Build workflow](../.github/workflows/build.yml) does the same in the `devkitpro/devkita64` container on every
push, and keeps the `.nro` as an artifact of the run.

## Core and tests

The core (`source/core/`) has no Switch or interface code, so it builds and runs anywhere. You need CMake, a C++20
compiler, libarchive and libcurl (`libarchive-dev libcurl4-openssl-dev` on Debian and Ubuntu).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/comptests
```

`comptests` builds CPKs and reads them back, checks every mod format the app recognizes with small made-up mods,
and runs the library through installs, the load order, the game's own PATCH1 files, p5rcbt mode, backups and the
free-space check on a temporary folder. CI runs it on every push; a change should keep it passing.

`compcli` is the same core from the command line, handy for looking at real mods and CPKs:

```sh
./build/compcli list PATCH1.CPK                 # the files in a CPK
./build/compcli extract PATCH1.CPK out/         # extract it
./build/compcli pack out/ NEW.CPK               # build a CPK from a folder
./build/compcli detect "Some Mod.zip"           # what the app would install from an archive
./build/compcli install sd/ "Some Mod.zip"      # install it into a folder standing in for the SD card
./build/compcli status sd/                      # the mods there, the built CPK and its conflicts
```

## Desktop test build

The whole app on a computer, for working on the interface: the keyboard stands in for the controller (arrow
keys, Enter for A, Escape for B…) and a folder for the SD card. You need libwebp, libarchive, libcurl and OpenGL,
plus the X11 development headers for GLFW, which Borealis builds itself (`xorg-dev` on Debian and Ubuntu).

```sh
cmake -S . -B build-desktop -DPLATFORM_DESKTOP=ON
cmake --build build-desktop -j
cd build-desktop && MM_SD_ROOT=/tmp/fake-sd ./Compendium
```

Environment variables of the test build:

| Variable | What it does |
|---|---|
| `MM_SD_ROOT` | The folder used as the SD card (default: `./sd`) |
| `MM_P5R_VERSION` | The game version the app sees, e.g. `1.0.2` (the console reads it from the system) |
| `MM_GAME_ROMFS_P5R` | A folder with the game's files, to try the one-time step |
| `MM_FREE_SPACE` | The free space the app sees, in bytes |
| `MM_APPLET` | Set to act as if opened from the Album (less memory) |
| `MM_DEBUG_SCRIPT` | Steps to run, for screenshots and checks: `w500` waits, `kdown` / `ka` / `kplus`… press a button, `fText` focuses the item labeled Text (`_` for spaces), `sNAME` saves a screenshot, `q` quits |
| `MM_SHOT_DIR`, `MM_SHOT_CMD` | Where screenshots go, and a command that takes them from outside (for virtual displays) |

The README's screenshots come from this build, with sample listings standing in for GameBanana.

## The README's art

`tools/art/make_art.py` draws the banner, the social preview, the install steps (English and Spanish) and the
app's icon (`docs/images/icon.png`, and `resources/img/icon.jpg` at 256×256), in the same style as the NFS Most
Wanted and Spider-Man 2 ports' art. It needs Python with Pillow, and the font Barlow Condensed (not included):

```sh
python3 tools/art/make_art.py <fonts dir> docs/images/screenshots docs/images
```

The fonts dir holds `BarlowCondensed-<weight>-<normal|italic>.ttf`, for example from the
`@fontsource/barlow-condensed` npm package converted with fontTools.

## Making a release

1. Set the version in [`VERSION`](../VERSION). Versions 0.0.x are test versions, published as pre-releases.
2. Add the version to [`CHANGELOG.md`](../CHANGELOG.md), in players' words.
3. Write the release notes in `docs/releases/v<version>.md`, English then Spanish, in the same order every time:
   one line on what the version brings and the "no game files" box; **"Which file do I download?" /
   "¿Qué archivo descargo?"** (`Compendium.nro` is the only file players need; "Source code" is added by GitHub
   and has nothing to run); what's new; how to install and update; known issues.
4. Push to `main`. The workflow builds the `.nro` and, the first time it sees a version that has notes and no
   release yet, creates the tag `v<version>` and the release with `Compendium.nro` attached and the notes as its
   text. Later pushes with the same version leave the release as it is.

If a build fails, the workflow pushes its log to a branch named `ci-logs-<job>`, so it can be read without
opening the run.

## Third-party code

[Borealis](https://github.com/xfangfang/borealis) (xfangfang's fork) is a submodule in `library/borealis`, pinned to
a commit. `source/third_party/` holds nlohmann/json and Nayuki's QR Code generator. The other libraries come from
devkitPro's packages (Switch) or the system (desktop). Licenses: [THIRD_PARTY.md](../THIRD_PARTY.md).
