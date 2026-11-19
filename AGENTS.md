# Battery (BatteryHub, formerly Wireless Battery)

Zek's fork (`ahizechi/Battery`, renamed from Wireless-Device-Battery-Monitor) of
`krisgfx9/Wireless-Device-Battery-Monitor`; upstream renamed the app BatteryHub in v1.1.0.
Both assistants build; Claude also reviews. Facts below were read from README.md,
CONTRIBUTING.md, Makefile, build.bat and `.github/workflows` (18 Nov 2026, rechecked after the
19 Nov upstream merge); nothing was built or run on this PC.

## Fork

- Remote `upstream` = the original (push disabled). Sync by merging `upstream/main` into an
  `ai/` branch; never rewrite upstream history.
- Fork-only change: no charging bolt on the taskbar (`src/widget.cpp`, `mark = it.full`); the
  tooltip and tray colour still show charging, and the full-charge check mark is kept.
- The updater (`src/update_core.h`, `src/update_win.cpp`) checks the original's releases, signed
  with its maintainer's key, so installing an update replaces a fork build with upstream's.

## Project facts

- Purpose: shows battery levels of wireless mice, headsets, keyboards and controllers on the
  Windows taskbar (layered child window of `Shell_TrayWnd`) or as tray icons; `--list`,
  `--dump`, `--quit` command-line options.
- Stack: C++17, Win32 only, no third-party dependencies, MinGW-w64 (g++, windres). Bundles
  HeadsetControl 4.3.0 (GPL; its source ships with releases).
- Package manager: none.
- Entry points: `src/main.cpp`, `src/devices.cpp`, `src/widget.cpp`, `src/hid.cpp`; one driver
  per vendor in `src/drivers/`; resources in `res/`; installer in `installer/` (Inno Setup).
- External services and stored data: network only for the menu-driven update check (GitHub
  releases API; installs only setups listed in a maintainer-signed SHA256SUMS.txt, downloaded to
  `%LOCALAPPDATA%`). Reads a Phone Link card file for phone battery; keeps local battery history
  (can be turned off). Release builds download the pinned HeadsetControl and verify its checksum.
- Also: translations in `lang/`, `--json` output, a Linux build (`src/linux/`, GTK4 tray).
- Hard invariants (CONTRIBUTING.md): drivers send read-only requests only, match devices by exact
  vendor/product ID or a protocol-unique collection, time out (about 1 s; at most 4 s per
  collection for documented push protocols), and credit their protocol source in a header comment.
  Never write settings, lighting or modes. Generic fallbacks only touch unclaimed devices.
- Sensitive areas: anything that adds network access, launches programs, reads input, clipboard or
  screen, adds startup entries or binaries, or changes build/release; the PR safety check flags these.

## Verified commands (from config; not run here)

- Build: `make` (MSYS2 MINGW64) or `build.bat` (g++ and windres on PATH) -> `BatteryHub.exe`.
- Test: `make test` (driver, history, alert, updater and release-signature tests).
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
