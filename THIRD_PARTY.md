# Third-party software

Compendium is built with the following software, with thanks to its authors. The full license texts are in
[`resources/THIRD_PARTY_NOTICES.txt`](resources/THIRD_PARTY_NOTICES.txt), which is also inside the `.nro`.

## Projects Compendium is based on

| Project | Used for | License |
|---|---|---|
| [Patchwork](https://github.com/TheRomeoAlfa/patchwork-nx) by TheRomeoAlfa | The GameBanana client, downloader, archive unpacking, app structure and interface code, adapted | GPL-3.0 |
| [SimpleModManager](https://github.com/nadrino/SimpleModManager) by nadrino | Its mod folder layout (`sd:/mods/<game>/<mod>/`), read by the app, and its way of keeping mods apart and copying the enabled ones into Atmosphère's folders | GPL-3.0 |

## In the `.nro`

| Component | Used for | License |
|---|---|---|
| [Borealis](https://github.com/xfangfang/borealis) (xfangfang's fork), a submodule in `library/borealis` | The interface | Apache-2.0 |
| [Yoga](https://github.com/facebook/yoga), [{fmt}](https://github.com/fmtlib/fmt), [Tweeny](https://github.com/mobius3/tweeny), [switch-libpulsar](https://github.com/p-sam/switch-libpulsar), [libromfs](https://github.com/WerWolv/libromfs), [libretro-common](https://github.com/libretro/libretro-common) (with Borealis) | Layout, text formatting, animations, system sounds, resources, file helpers | MIT |
| [NanoVG](https://github.com/memononen/nanovg), [TinyXML-2](https://github.com/leethomason/tinyxml2) (with Borealis) | Drawing, Borealis' layout files | zlib |
| [Material Icons](https://github.com/google/material-design-icons) (with Borealis) | Icons | Apache-2.0 |
| [GLFW](https://www.glfw.org) | Window and input | zlib |
| [libnx](https://github.com/switchbrew/libnx) | Nintendo Switch homebrew library | ISC |
| [JSON for Modern C++](https://github.com/nlohmann/json) (`source/third_party/nlohmann`) | Settings, the library's state, GameBanana's and GitHub's answers | MIT |
| [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki (`source/third_party/qrcodegen`) | The phone link's QR code | MIT |
| [curl](https://curl.se) (libcurl) | Downloads | curl license (MIT-style) |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | HTTPS | Apache-2.0 or GPL-2.0-or-later |
| [libarchive](https://libarchive.org) | Unpacking `.zip`, `.7z` and `.rar` | BSD-2-Clause (and others) |
| [Zstandard](https://github.com/facebook/zstd), [LZ4](https://github.com/lz4/lz4), [XZ Utils](https://tukaani.org/xz/), [bzip2](https://sourceware.org/bzip2/), [zlib](https://zlib.net) | Compression formats inside archives | BSD-3-Clause, BSD-2-Clause, 0BSD, bzip2 license, zlib |
| [Expat](https://libexpat.github.io) | XML, for libarchive | MIT |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | Mod pictures in WebP | BSD-3-Clause |
| [Mesa](https://mesa3d.org) (EGL, glapi) and libdrm_nouveau, devkitPro's Switch ports | OpenGL on the Switch's GPU | MIT |
| Mozilla's CA certificate bundle (`resources/cacert.pem`, from [curl](https://curl.se/docs/caextract.html)) | Checking HTTPS certificates | MPL-2.0 |

libnx, curl, Mbed TLS, libarchive and its compression libraries, libwebp, GLFW and Mesa come from
[devkitPro](https://devkitpro.org)'s packages and are not in this repository.

## Downloaded on request, not included

| Project | For |
|---|---|
| [p5rcbt](https://github.com/Raytwo/p5rcbt) by Raytwo | The optional Skyline plugin, downloaded from its GitHub releases only when the player installs it from Settings |

## Used as references, not included

| Project | For |
|---|---|
| [ShrineFox's documentation](https://docs.shrinefox.com) | Persona 5 Royal's files and mod formats |
| [Aemulus Mod Manager](https://github.com/TekkaGB/AemulusModManager) by TekkaGB | How Aemulus packages are laid out and built |
| [PyCriCodecs](https://github.com/Youjose/PyCriCodecs) | CRI's CPK and @UTF formats |

## Artwork in this repository

- The banner, the social preview, the install steps and the icon (`docs/images/`, `resources/img/icon.jpg`, drawn by
  [`tools/art/make_art.py`](tools/art/make_art.py)) use the font [Barlow Condensed](https://github.com/jpt/barlow) by
  Jeremy Tribby, under the SIL Open Font License 1.1. The font itself is not included.
- The screenshots in `docs/images/screenshots/` show the app's PC test build with made-up sample mods (their names
  and pictures were made for the screenshots). Persona and its characters belong to ATLUS and SEGA; none of the
  games' art is in this repository.
