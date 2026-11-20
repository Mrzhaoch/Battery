# Battery (BatteryHub, formerly Wireless Battery)

Zek's fork (`ahizechi/Battery`, renamed from Wireless-Device-Battery-Monitor) of
`krisgfx9/Wireless-Device-Battery-Monitor`; upstream renamed the app BatteryHub in v1.1.0.
Both assistants build; Claude also reviews. Facts below were read from README.md,
CONTRIBUTING.md, Makefile, build.bat and `.github/workflows` (18 Nov 2026, rechecked after the
19 Nov upstream merge); nothing was built or run on this PC.

## Fork

- Remote `upstream` = the original (push disabled). Sync by merging `upstream/main` into an
  `ai/` branch; never rewrite upstream history. `.github/upstream-version` records the BatteryHub
  release last merged; `upstream-watch.yml` opens one issue when its daily check finds a newer
  one (never merges). After a sync, bump that file and close the issue.
- Fork-only changes: an Apple-style level bar under every battery percentage (text colour, green
  charging / full, red low; number stays text colour), not upstream's bolt / check
  (`src/widget.cpp` `Capsule`, menu *Show charging (green bar)*, registry `ShowCharging`;
  WLMouse Beast MAX cable `36A7:A881` added, cables share their receiver's key), and hardware
  readings after the batteries (`src/hardware.*`, pure logic in `src/hardware_model.h` with
  `tests/hardware_model_test.cpp`; `WidgetItem::tag`/`warm`; menu *Hardware on the taskbar*,
  registry `HardwareReadings` bitmask, default 0; `HardwareRefreshSeconds` 1/2/5, default 5;
  paused while the display is off via `GUID_CONSOLE_DISPLAY_STATE`). Defaults show only batteries;
  HeadsetControl (`ReadHeadsetControl`) is off by default because it runs a program. No CPU temperature and no kernel driver, by Zek's choice
  (19 Nov 2026); TraySense (AGPL) inspired it but none of its code is used.
- Zek's rule for this fork: minimal RAM, and anything switched off is not read at all. Sources:
  `PollOptions` (`devices.h`; registry `ReadBluetooth`, `ReadPhoneLink`, `ReadXInput`,
  `ReadHeadsetControl`), hidden devices skipped by VID:PID (`PausedDevices`; drivers must set
  `DeviceStatus::pid`, which `ctx.Add` does), BLE watcher stopped by `EndBlePoll()` when unused.
  New features follow the same rule: off = nothing loaded, opened or polled.
- Battery history (window, recording, time-to-empty estimate) was removed on Zek's request
  (19 Nov 2026); upstream merges will bring `battery_history.*`, `history_model.h` and
  `runtime_estimate.h` changes back as conflicts: keep them deleted. Translations stay (~0.1 MB).
- Updater: `updates::Repository = "ahizechi/Battery"` (also `scripts/sign-release.ps1`, and
  `build.yml` builds `stable` only there). It checks once a day, failures included.
  `src/update_public_key.h` holds only Zek's release key (19 Nov 2026; the original's was removed).
  The encrypted private key is `%USERPROFILE%\.wirelessbattery\release-key.wbk` on Zek's PC, never in
  the repo; each release is signed there with `scripts/sign-release.ps1` (Zek's password,
  docs/releasing.md). The repo is public, as the GitHub releases API requires.

## Project facts

- Purpose: shows battery levels of wireless mice, headsets, keyboards and controllers on the
  Windows taskbar (layered child window of `Shell_TrayWnd`) or as tray icons; `--list`,
  `--dump`, `--quit` command-line options.
- Stack: C++17, Win32 only, no third-party dependencies, MinGW-w64 (g++, windres). Bundles
  HeadsetControl 4.3.0 (GPL; its source ships with releases).
- Package manager: none.
- Entry points: `src/main.cpp`, `src/devices.cpp`, `src/widget.cpp`, `src/hid.cpp`; one driver
  per vendor in `src/drivers/`; resources in `res/`; installer in `installer/` (Inno Setup).
- External services and stored data: network only for the update check (GitHub releases API of
  `ahizechi/Battery`; installs only setups listed in a SHA256SUMS.txt signed with Zek's key,
  downloaded to `%LOCALAPPDATA%`). Reads a Phone Link card file for phone battery. Release builds
  download the pinned HeadsetControl and verify its checksum.
- Also: translations in `lang/`, `--json` output, a Linux build (`src/linux/`, GTK4 tray).
- Hard invariants (CONTRIBUTING.md): drivers send read-only requests only, match devices by exact
  vendor/product ID or a protocol-unique collection, time out (about 1 s; at most 4 s per
  collection for documented push protocols), and credit their protocol source in a header comment.
  Never write settings, lighting or modes. Generic fallbacks only touch unclaimed devices.
- Sensitive areas: anything that adds network access, launches programs, reads input, clipboard or
  screen, adds startup entries or binaries, or changes build/release; the PR safety check flags these.

## Verified commands (from config; not run here)

- Build: `make` (MSYS2 MINGW64) or `build.bat` (g++ and windres on PATH) -> `BatteryHub.exe`.
- Test: `make test` (driver, hardware, alert, updater and release-signature tests).
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
source. Releases need Zek's explicit request. Use `scripts\release.ps1 -Version x.y.z` (tag, wait for
CI, check sums, sign, upload signature; docs/releasing.md); signing needs Zek's key password.
