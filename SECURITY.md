# Security

Battery is a fork of [BatteryHub](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor). This file
covers this fork only.

## Reporting a problem

Please report security problems privately: open the repository's **Security** tab and choose
**Report a vulnerability**. Do not open a public issue for them.

Most relevant here:

- the updater: anything that could make the app install a file that is not this fork's signed release
  (`src/update_*.cpp`, `scripts/sign-release.ps1`, `docs/releasing.md`);
- the hardware readings and device drivers: anything that writes to a device or opens more than it reads.

A problem that also exists in BatteryHub should be reported to BatteryHub as well.

## What the app does and does not do

- Network: only the update check, once a day, to GitHub's API for this repository. No device or battery
  information is sent. Updates are installed only when `SHA256SUMS.txt` is signed with this fork's release
  key, whose public half is in `src/update_public_key.h`.
- Devices: drivers send read-only queries. The app never writes settings, lighting or firmware.
- No administrator rights, no service, no driver.
