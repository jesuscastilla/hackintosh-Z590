# Session checkpoint log — Hackintosh (ASUS Z590-P WIFI · iMac20,1 · Tahoe)

> Session: 2026-10-24 (macOS 26.7.1 / Darwin 25.6.0)
> Scope: Stability + Wi-Fi — diagnosis, repo housekeeping, repo↔EFI reconciliation
> Confidential data (serials/MLB/ROM/UUID) always redacted. This file is untracked and never pushed.

---

## Phase 0 — Safety net (completed)

- EFI backup (outside repo): `/Users/jesus/hack-backups/EFI-ESP-20261024-2208/` (41 MB, 140 files, `BOOT/`+`OC/`)
  - `config.plist` sha256 (live = backup): `0195d31cb715075183f3b14d8a98247aa6548635ffdb01afe1dc3d5ad86e0634`
- Repo snapshot: `HEAD = 185ec0d`, tree clean. No commit/tag created.
- Plist backups: `/Users/jesus/hack-backups/plists-20261024/` (`config.local.plist.bak`, `config.plist.bak`, `config.ESP-live.plist`)

## Phase 1 — Repo ↔ EFI reconciliation (completed)

- `config.local.plist` aligned 1:1 with the Live ESP (`sha256` identical, `plutil -lint` OK).
  Stale entry `AirportItlwm → Enabled=true` (wrong Sequoia kext) corrected to `false`.
- Live ESP = source of truth. Public `EFI/OC/config.plist` left sanitized (untouched).

## Part A — Housekeeping (completed, user-approved)

- Checkpoint moved from `Display_Maxwell/display_kext.md` to repo root `display_kext.md`; `Display_Maxwell/` deleted.
- Freeze photo removed from repo and `/Volumes/GITHUB` (a copy remains in `~/Downloads`).

## Freeze photo analysis (PXL_20261024_001510787.jpg)

Last visible lines: `SimpleHDA … Linking com.apple.driver.AppleHDA`, `deferred rematching count 2`,
`Driver com.apple.DriverKit-IOUserDockChannelSerial has crashed 0 time(s)`,
`DK: IOUserDockChannelSerial-… waiting for server …`. Just above: `en1: 802.11d country code set to 'ES'`,
`en1: Band (5 GHz) Supported channels: 36 40 44 48 52 …`.

Conclusion: Wi-Fi (en1) had already initialized → the hang is **after** Wi-Fi init, at the
audio (SimpleHDA/AppleHDA) + DriverKit bring-up stage. So `AirportItlwm` is likely NOT the freeze cause.
`IOUserDockChannelSerial` is healthy in a good boot (pid 351, Apple PTY pseudo-serial).

## Decisions (user, 2026-10-24)

- Audio: use HDMI/DP output. `AppleGFXHDA (550.1)` is loaded and monitor `LU28R55` output exists →
  `SimpleHDA` + `AppleALC` can be disabled without losing HDMI audio (`AppleHDA.kext` is absent in Tahoe).
- Wi-Fi route: **WiFiPatcherPro 1.0.5** (AirportItlwm Ventura + OCLP root patch) — not vanilla, accepted.
  Asset: `Wi-Fi.Patcher.Pro.zip` (75 013 336 B, sha256 `45c0d25044e3a64603fe57e17409898d7f7f4e1d351e8a05b1bdb9143eba6ef6`).
- WPA3 confirmed working by user; router untouched; uses channel 36.
- ReBAR: **do not change**.

## Next steps (pending)

1. Snapshot live EFI (fresh, dated) before running WiFiPatcherPro; ensure boot USB is available.
2. Run WiFiPatcherPro 1.0.5 (it rewrites ESP `config.plist`, installs kext + OCLP root patch, sets `-amfipassbeta`).
3. Re-sync repo `config.local.plist` with the patched ESP `config.plist` afterward (gitignored, safe).
4. Part B stability tests (one variable at a time):
   a. Disable `SimpleHDA` + `AppleALC` (HDMI audio survives via `AppleGFXHDA`) — 10 boots.
   b. AMFI: `amfi=0x80` → `amfi_get_out_of_my_way=0x1` (test).
   c. `EnableWriteUnprotector=false` (redundant with `RebuildAppleMemoryMap=true`).
   d. Clean `/Library/Extensions` leftovers (NVIDIA Web, HighPoint, EPSON, SamsungMagician) with backup.
   e. `SetApfsTrimTimeout=-1` (isolated test).

## Part A2 — Prep for WiFiPatcherPro (completed, 2026-10-24)

- Fresh pre-patch EFI snapshot: `/Users/jesus/hack-backups/EFI-ESP-20261024-pre-patch/` (41 MB, `config.plist` sha256 identical to live).
- WiFiPatcherPro 1.0.5 downloaded to `Kexts_Download/Wi-Fi.Patcher.Pro.zip` (gitignored), sha256 verified `45c0d25044e3a64603fe57e17409898d7f7f4e1d351e8a05b1bdb9143eba6ef6`.
- Boot USB: to be created by user from the known-good live ESP (Wi-Fi disabled).

## Part A3 — Rescue USB (completed, 2026-10-24)

## Part C — WiFiPatcherPro applied (2026-10-24)

- Root patch applied: new blessed APFS snapshot `com.apple.bless.D8819635-…` (XID 146960) on MACOS.
- ESP (`disk1s1`) unmounted by the app and now requires **root** to mount (`storagekitd: skipping EFI partition owned by uid 0`). Data intact (fsck: 462 files, exit 0).
- boot-args in NVRAM still the pre-patch values until the next reboot (OC re-applies `config.plist` boot-args).

## Part C2 — What WiFiPatcherPro changed + re-sync (2026-10-24)

Changes applied by the app to the ESP `config.plist`:
- `AirportItlwm → Enabled=true` (kept the Sequoia 2.4.0-alpha binary, sha unchanged).
- Kexts added: `IOSkywalkFamily`, `IO80211FamilyLegacy`, `AMFIPass 1.4.1`, `BlueWakeFixup 1.0.0`.
- `Kernel→Block`: `com.apple.iokit.IOSkywalkFamily` → `Exclude` (MinKernel 23.0.0) — injects own Skywalk, blocks native.
- DeviceProperty `PciRoot(0x0)/Pci(0x14,0x3)` `compatible = pci14e4,43a0` (Broadcom BCM4360 spoof).
- `boot-args` += `-amfipassbeta`.
- `csr-active-config` 0x0A03 → 0x0803.
- Root patch: new blessed APFS snapshot on MACOS.

Repo re-sync done:
- `EFI/OC/config.local.plist` ← live ESP config (sha256 `9e63914e…`), `plutil -lint` OK.
- 4 new kexts copied into `EFI/OC/Kexts/`.
- Public `EFI/OC/config.plist` aligned & sanitized (MLB/SysSerial = `REPLACE_WITH_YOUR_…`, ROM = 6 zero bytes, UUID empty) — redacted diff vs live = empty.

## Part D — Recovery after failed boot (2026-10-24)

- The WiFiPatcherPro/OCLP patched config **does not boot**: hangs at `IOUserDockChannelSerial waiting for server` + `SimpleHDA: AppleHDA failed (0xdc008008)` → watchdog panic. Same hang as with plain `AirportItlwm` enabled.
- Disk renumbering shifted after USB boot (macOS ESP is now `disk2s1`, not `disk1s1`). ESP intact.
- Running system = original sealed snapshot `FCA457EF` (OCLP bless snapshot gone) → system clean.
- Repo reverted to pre-patch (`0195d31c…`), patched variant preserved in `~/hack-backups/wifipatcherpro-variant/` (config only; kexts recoverable from the app zip).

## Part E — Wi-Fi retry + HE + TRIM (2026-10-24)

- Re-enabled `AirportItlwm` (Sequoia 2.4.0-alpha) in both configs (`Enabled=true`).
- `boot-args` += `itlwm_he=1` (802.11ax/HE → aim 160 MHz).
- `Kernel→Quirks→SetApfsTrimTimeout = -1` (disable APFS TRIM at boot → faster boot; NVMeFix handles TRIM in background).
- `SimpleHDA`/`AppleALC` remain disabled (removes the suspected hang chain). HDMI/DP audio via `AppleGFXHDA`.
- Note: HDMI/DP volume slider is fixed by design (no software volume) — expected, control volume on the monitor/speakers.
- Risk: `AirportItlwm` previously hung boot when `SimpleHDA` was present; retesting with audio kexts removed. Rescue USB ready.

- `SimpleHDA` + `AppleALC` disabled in `config.local.plist` and `config.plist` (HDMI audio goes via `AppleGFXHDA`, which is detected as default output).
- HDMI/DP audio of the GPU is fully attached (default output, 48 kHz) but silent per user; monitor is Samsung LU28R55 (likely no built-in speakers — only 3.5 mm out). Connector detected as DP (`connector-type=0x800`) with HDMI cable.
- Pending: restore ESP (disk2s1) to pre-patch + SimpleHDA/AppleALC-off, reboot, then re-approach Wi-Fi.

- Rescue USB intentionally keeps the pre-patch (rollback) EFI.

- Pending: `sudo diskutil mount disk1s1` → inspect patched config → re-sync repo `config.local.plist`.


- The 256 GB pendrive was already a macOS recovery USB (`com.apple.recovery.boot/BaseSystem.dmg` + an old OpenCore).
- Kept `com.apple.recovery.boot` intact; backed up the old USB OpenCore to `/Users/jesus/hack-backups/usb-gbos-efi-20261024/`.
- Replaced the USB OpenCore (`/Volumes/GBOS/EFI/BOOT` + `OC`) with the **current known-good live EFI** (config.plist sha256 `0195d31c…0634`, OpenCore 1.0.8, Wi-Fi disabled) — verified byte-identical.
- Boot via ASUS F8/F12 menu → pick the USB → OpenCore.



## Final working state (2026-10-25)

- Wi-Fi: `AirportItlwm 2.4.0` (Sequoia) — **802.11ax HE160, ch 36, Tx 2401 Mbps, MCS 11**.
- `boot-args`: `... itlwm_cc=ES itlwm_he=1` (no `-amfipassbeta`).
- `SimpleHDA` + `AppleALC` disabled; HDMI/DP audio via `AppleGFXHDA` (volume fixed by design).
- `SetApfsTrimTimeout = 1`.
- Removed OCLP kexts: `IOSkywalkFamily`, `IO80211FamilyLegacy`, `AMFIPass`, `BlueWakeFixup`.
- Router: 5 GHz ch 36 @ 160 MHz, MLO off (was ch 149 + MLO -> blocked ES 5 GHz).
