# Battery (Wireless Battery)

Zek's fork (`ahizechi/Battery`, renamed from Wireless-Device-Battery-Monitor) of
`krisgfx9/Wireless-Device-Battery-Monitor`. Both assistants build; Claude also reviews.
Facts below were read from README.md, CONTRIBUTING.md, Makefile, build.bat and
`.github/workflows` on 18 Nov 2026; nothing was built or run on this PC.

## Project facts

- Purpose: shows battery levels of wireless mice, headsets, keyboards and controllers on the
  Windows taskbar (layered child window of `Shell_TrayWnd`) or as tray icons; `--list`,
  `--dump`, `--quit` command-line options.
- Stack: C++17, Win32 only, no third-party dependencies, MinGW-w64 (g++, windres). Bundles
  HeadsetControl 4.3.0 (GPL; its source ships with releases).
- Package manager: none.
- Entry points: `src/main.cpp`, `src/devices.cpp`, `src/widget.cpp`, `src/hid.cpp`; one driver
  per vendor in `src/drivers/`; resources in `res/`; installer in `installer/` (Inno Setup).
- External services and stored data: none at runtime (no network). Release builds download the
  pinned HeadsetControl and verify its checksum.
- Hard invariants (CONTRIBUTING.md): drivers send read-only requests only, match devices by exact
  vendor/product ID or a protocol-unique collection, time out (about 1 s; at most 4 s per
  collection for documented push protocols), and credit their protocol source in a header comment.
  Never write settings, lighting or modes. Generic fallbacks only touch unclaimed devices.
- Sensitive areas: anything that adds network access, launches programs, reads input, clipboard or
  screen, adds startup entries or binaries, or changes build/release; the PR safety check flags these.

## Verified commands (from config; not run here)

- Build: `make` (MSYS2 MINGW64) or `build.bat` (g++ and windres on PATH) -> `WirelessBattery.exe`.
- Test: `make test` (Rapoo protocol and driver tests).
- Installer and portable zip: `powershell -File installer\build-installer.ps1 -Version <x.y.z>`
  (needs Inno Setup).
- CI: `.github/workflows/build.yml` on windows-latest builds, tests and packages; a `v*` tag
  publishes a release. `pr-safety.yml` runs `scripts/pr-safety.sh` on pull requests.
- Not available on this PC: MinGW-w64/MSYS2 and Inno Setup are not installed.

## Project conventions

- 4-space indent, about 120-column lines, keep the binary small.
- Commit messages must not contain `Co-authored-by` lines crediting AI tools: the PR safety check
  fails on them.
- Local release builds go in the ignored `dist\` folder (1.0.6 installer kept there).

## Release facts

Pushing a `v*` tag makes GitHub Actions publish the installer, portable zip and HeadsetControl
source. Releases need Zek's explicit request.
