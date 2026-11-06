# Contributing

Thanks for helping! Reports from real consoles are the most useful thing right now: Compendium is tested with
automated tests and a PC test build, and consoles are what's missing.

## Reporting a bug

Use the [bug report form](../../issues/new/choose) and attach `switch/Compendium/log.txt` (and `log.old.txt` if
the problem was in an earlier session). If the game or the console showed an Atmosphère error, the newest file in
`atmosphere/crash_reports/` helps too. Please include the app's version, your firmware, Atmosphère and game
versions, and links to the mods involved. English or Spanish are both fine.

## A mod that doesn't work

Use the [mod report form](../../issues/new/choose) with a **link** to the mod's page (not the mod itself) and
`log.txt`, which lists what the app found in the archive. Mods come in many shapes, and every report helps the app
read more of them.

## The one rule: no game files

Never attach, commit or link game files: CPKs from the game (`ALL_USEU.CPK`, the game's `PATCH1.CPK`…), files
extracted from them, or dumps. Don't attach other people's mods either: link to where they're published. Issues and
pull requests that contain game files are closed.

## Pull requests

- Build and run the core tests before sending: `cmake -S . -B build && cmake --build build && ./build/comptests`.
  CI runs them, and builds the `.nro`, on every pull request.
- For interface changes, the desktop test build ([docs/BUILDING.md](docs/BUILDING.md#desktop-test-build)) runs the
  whole app on a computer.
- Support for a new mod shape goes in `source/core/persona_layout.cpp`, with a test in `tests/test_layout.cpp` that
  builds a small made-up mod of that shape.
- Keep changes focused, and describe what you tested (tests, desktop build, console).
- Code style: the existing code's (C++20, 4-space indents, comments that say *why*).

By contributing you agree that your contribution is released under the project's
[GNU General Public License v3.0](LICENSE).
