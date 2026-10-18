# Hackintosh — macOS Sonoma on ASUS Z590

Personal Hackintosh build: macOS Sonoma 14.x running on an **ASUS PRIME Z590-P WIFI** motherboard with an **Intel Core i5-11600K** and an **NVIDIA GeForce GTX 970 (Maxwell)**, dual-booting with **Windows 11**. Boot management is handled via **OpenCore 1.0.x + rEFInd**, with legacy GPU acceleration provided by **OpenCore Legacy Patcher (OCLP)**.

---

## Disclaimer

> [!WARNING]
> This repository is intended strictly for educational, experimental, and testing purposes. Installing macOS on non-Apple hardware violates Apple's End User License Agreement (EULA, Section 2.J) and may have legal implications depending on your jurisdiction. The author does not endorse piracy or license infringement and assumes no responsibility for any misuse or hardware issues. Proceed at your own discretion.

---

## Hardware Specifications

| Component | Model / Spec | Notes |
|---|---|---|
| **CPU** | Intel Core i5-11600K (Rocket Lake, 6C/12T) | CPUID spoofed to Comet Lake (`0x0A0655`) |
| **Motherboard** | ASUS PRIME Z590-P WIFI (LGA1200) | `UpdateSMBIOSMode=Custom` |
| **GPU** | NVIDIA GeForce GTX 970 4 GB (Maxwell) | Accelerated via OCLP (NVIDIA Web Drivers) |
| **iGPU** | Intel UHD Graphics 750 | Disabled (unsupported in macOS) |
| **RAM** | 32 GB DDR4 | Standard dual-channel |
| **Storage** | Samsung 990 PRO NVMe 1 TB | Windows 11 + macOS Sonoma (APFS) |
| **Ethernet** | Realtek RTL8125 2.5 GbE | `LucyRTL8125Ethernet.kext` |
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi) | `AirportItlwm.kext` (native macOS Wi-Fi integration) |
| **Bluetooth** | Intel AX201 Bluetooth | `IntelBluetoothFirmware` + `IntelBTPatcher` + `BlueToolFixup` |
| **Audio** | Realtek ALC897 | `AppleALC.kext` (`alcid=11`) |
| **Bootloader** | OpenCore 1.0.x + rEFInd | Dual-boot immune to NVRAM / CMOS resets |
| **SMBIOS** | `iMac20,1` | Full iServices (iMessage, FaceTime, iCloud) |
| **macOS** | Sonoma 14.4.x | APFS root with OCLP legacy GPU patches |

---

## Working Features & Known Limitations

### What Works
- **Full Graphics Acceleration**: Metal & OpenGL acceleration via OpenCore Legacy Patcher (Nvidia Web Drivers on Sonoma).
- **Networking**: 2.5 Gbps Ethernet, Intel Wi-Fi, and Intel Bluetooth with Airdrop/audio support.
- **Apple Services**: iCloud, App Store, iMessage, FaceTime.
- **Custom USB Port Mapping**: USB 2.0, USB 3.0, Type-C, and internal headers properly mapped (`UTBMap.kext`).
- **Audio**: Realtek ALC897 with internal speaker, mic, and headphones working.
- **Dual Boot**: Custom rEFInd Glassy boot menu with direct chaining to Windows and OpenCore.

### Known Limitations
- **Deep Sleep (S3)**: NVIDIA Maxwell architecture has known sleep/wake wake-lock issues in modern macOS versions (Sonoma). Display sleep works properly, but deep system sleep is disabled to prevent wake crashes.

---

## OpenCore Configuration

### CPU Spoofing (Rocket Lake)
Because Apple never released Macs with 11th Gen Rocket Lake processors, Comet Lake CPUID spoofing is applied under `Kernel → Emulate`:

```text
Cpuid1Data: 55060A00 00000000 00000000 00000000   (0x0A0655)
Cpuid1Mask: FFFFFFFF 00000000 00000000 00000000
```

### ACPI Patches
- `SSDT-AWAC.aml` — Disables AWAC clock and enables classic RTC.
- `SSDT-EC-USBX-DESKTOP.aml` — Dummy Embedded Controller and USB power regulation.
- `SSDT-PLUG-DRTNIA.aml` — Injects plugin-type=1 for native CPU power management.
- `SSDT-RHUB.aml` — Resets USB Root Hub state on Asus Z590 motherboards.

### Kernel Extensions (Kexts)
The build relies on 15 clean, stable kexts:
1. `Lilu.kext` — Core patching engine.
2. `VirtualSMC.kext` — Advanced Apple SMC emulator.
3. `SMCProcessor.kext` — CPU temperature and core monitoring.
4. `SMCSuperIO.kext` — Fan speed and motherboard sensor monitoring.
5. `WhateverGreen.kext` — Video card management and display output patching.
6. `AppleALC.kext` — Audio patcher for Realtek ALC897.
7. `LucyRTL8125Ethernet.kext` — Realtek 2.5 GbE network driver.
8. `NVMeFix.kext` — NVMe power management and stability fixes.
9. `AirportItlwm.kext` / `itlwm.kext` — Intel Wi-Fi drivers.
10. `BlueToolFixup.kext` — Bluetooth injector for macOS Monterey+.
11. `IntelBluetoothFirmware.kext` — Firmware loader for Intel Bluetooth.
12. `IntelBTPatcher.kext` — Extended patches for Intel Bluetooth.
13. `USBToolBox.kext` — USB mapping framework.
14. `UTBMap.kext` — Tailored USB port mapping for ASUS PRIME Z590-P.

### NVRAM & SIP Boot Arguments
```text
-v keepsyms=1 debug=0x100 alcid=11 amfi=0x80 ngfxgl=1 ngfxcompat=1 nvda_drv_vrl=1 ipc_control_port_options=0
```

- `amfi=0x80` & `ipc_control_port_options=0`: Enables legacy unsigned driver loading required by OpenCore Legacy Patcher for Maxwell GPUs.
- `nvda_drv_vrl=1`, `ngfxcompat=1`, `ngfxgl=1`: Enables Nvidia Web Drivers with OpenGL/Metal fallback.
- `alcid=11`: Sets the layout-id for the ALC897 audio codec.
- `csr-active-config`: Set to `030A0000` (`<data>AwoAAA==</data>`) to permit OCLP root volume modifications without fully compromising system integrity.

### OpenCore Picker
- `ShowPicker`: `true`
- `Timeout`: `5` seconds
- `Resolution`: `1920x1080@60`
- `ProvideCurrentCpuInfo`: `false`

---

## Dual Boot Setup with rEFInd (NVRAM-Reset Persistent)

To prevent the ASUS motherboard from wiping the boot entries or reverting exclusively to Windows on NVRAM/CMOS resets:

1. **Fallback Path Substitution**:
   `\EFI\Microsoft\Boot\bootmgfw.efi` is replaced with `refind_x64.efi`. When the motherboard loses NVRAM or resets its boot priority, it defaults to the standard Windows loader path, which launches rEFInd automatically.
2. **Original Windows Bootloader**:
   The genuine Windows Boot Manager binary is preserved at `\EFI\Microsoft\Boot\bootmgfw_orig.efi`.
3. **rEFInd Glassy Theme**:
   The custom `rEFInd-glassy` theme is installed under both `\EFI\Microsoft\Boot\themes\` and `\EFI\refind\themes\`.
4. **Boot Stanzas**:
   - **Windows Entry**: Launches `/EFI/Microsoft/Boot/bootmgfw_orig.efi`.
   - **macOS Entry**: Launches `/EFI/OC/OpenCore.efi`.

---

## Updating macOS or Safari

Because the Nvidia Maxwell GPU requires OCLP root patches on the sealed system snapshot, follow these steps when updating macOS or Safari:

1. Open **OpenCore Legacy Patcher** → Click **Revert Root Patches**.
2. Reboot the computer (the system will boot in unaccelerated basic VESA mode).
3. Install the update via **System Settings → General → Software Update**.
4. Once the update completes and macOS reboots, open **OpenCore Legacy Patcher** → Click **Start Root Patching**.
5. Reboot to restore full graphics acceleration.

---

## Replicating This Build

> [!IMPORTANT]
> The included `config.plist` has all private serial numbers sanitized with placeholders (`GENERATE_YOUR_OWN_SERIAL`, etc.).
> 
> Before booting or signing into your Apple ID:
> 1. Download **GenSMBIOS**.
> 2. Generate a new serial set for the model **`iMac20,1`**.
> 3. Fill in `SystemSerialNumber`, `MLB`, and `SystemUUID` under `PlatformInfo → Generic`.
> 4. Under `ROM`, insert your motherboard's physical Ethernet MAC address (as hex bytes without colons).

---

## Maintenance & Changelog

### [14-10-2026] — Windows EFI Partition Purge and Reconstruction
- **ESP Sanitization**: Completely wiped all non-Windows assets from the primary NVMe EFI System Partition (Disk 0, Partition 1). Removed residual rEFInd installations, custom glassy themes, OpenCore debug logs, and macOS filesystem artifacts (`.Spotlight-V100`, `.fseventsd`, etc.).
- **Partition Type Correction**: Corrected Disk 0 Partition 1 GPT type from Basic Data Partition (`ebd0a0a2-b9e5-4433-87c0-68b6b72699c7`) to standard EFI System Partition GUID (`c12a7328-f81f-11d2-ba4b-00a0c93ec93b`).
- **Filesystem Health**: Executed `chkdsk /F` on the EFI volume, resolving dirty flags and restoring operational health to `Healthy / OK`.
- **UEFI Boot Reconstruction**: Re-deployed authentic Windows 11 bootloader binaries and generated a pristine BCD hive using `bcdboot C:\Windows /s <ESP> /f UEFI`. Re-registered Windows Recovery Environment (WinRE) via `reagentc`.
- **NVRAM Firmware Cleanup**: Purged stale NVRAM boot entries (`rEFInd`, duplicate `OpenCore` pointing to Disk 0, and duplicate `Windows Boot Manager` on Disk 3). Placed genuine `Windows Boot Manager` (`\EFI\Microsoft\Boot\bootmgfw.efi`) at the primary position in `{fwbootmgr}` displayorder.

### [15-10-2026] — macOS 15 Sequoia USB Installer & OpenCore Boot Setup
- **USB Media Provisioning**: Cleaned and partitioned USB drive (Disk 4, SanDisk Ultra 3.0 64 GB) using a dual-partition GPT scheme:
  - **Partition 1 (OPENCORE, 5 GB, FAT32)**: Configured as primary UEFI boot volume containing OpenCore 1.0.8 (`\EFI\BOOT`, `\EFI\OC`) and the macOS 15 Sequoia recovery installer (`\com.apple.recovery.boot\BaseSystem.dmg` and `BaseSystem.chunklist`).
  - **Partition 2 (SEQUOIA, 52 GB, exFAT)**: Configured as high-capacity data volume storing the complete offline installer payload (`InstallAssistant.pkg` 15.6 GB, build 24H32 / macOS 15.8.1) and redundant recovery files.
- **Sequoia-Specific EFI Tuning**:
  - Disabled `AirportItlwm.kext` (v2.3.0 for Sonoma) to prevent immediate kernel panic during boot caused by macOS 15 Sequoia's rewritten `IO80211` / `IOSkywalkFamily` architecture; wired network remains natively enabled via `LucyRTL8125Ethernet.kext`.
  - Disabled non-essential Bluetooth kexts (`IntelBluetoothFirmware.kext`, `IntelBTPatcher.kext`, `BlueToolFixup.kext`) for installer stability.
  - Enabled verbose boot (`-v`) in `NVRAM -> boot-args` to ensure full console feedback during installer boot.
  - Retained working hardware identifiers (iMac20,1 SMBIOS, valid Serial/MLB/UUID/ROM) and Comet Lake CPUID spoof (`0x0A0655`) for the Intel Core i5-11600K.

### [16-10-2026] — macOS & EFI Migration Guide to Samsung 860 EVO (Clonezilla)
- **Migration Documentation**: Authored comprehensive printable migration guide for cloning macOS Sonoma and OpenCore EFI from the SPCC SATA SSD to the Samsung SSD 860 EVO using Clonezilla Live (`device-device` / `disk_to_local_disk`).
- **Safety & Provisioning**: Explicit NVMe disk isolation protocol (`nvme0n1` Windows 11 and `nvme1n1` storage protection).
- **Post-Clone Workflow**: Step-by-step instructions for mandatory SPCC SSD physical disconnection (preventing APFS UUID collisions), direct internal SATA attachment, OpenCore NVRAM reset, GPT secondary header relocation (`diskutil repairDisk`), APFS container expansion to 500 GB (`diskutil apfs resizeContainer`), and network/keychain purging for iServices activation.

### [17-10-2026] — macOS Tahoe 26.7.1 USB Installer & Unaccelerated Graphics OpenCore Setup
- **USB Media Provisioning**: Re-partitioned Disk 4 (SanDisk Ultra 3.0 64 GB) using a dual-partition GPT layout:
  - **Partition 1 (OPENCORE, 5 GB, FAT32)**: Configured as UEFI boot volume containing OpenCore EFI (`\EFI\BOOT`, `\EFI\OC`) and macOS Tahoe recovery installer payload (`\com.apple.recovery.boot\BaseSystem.dmg` and `BaseSystem.chunklist`, build 25G241 / 26.7.1).
  - **Partition 2 (TAHOE_DATA, 52.28 GB, exFAT)**: Configured as high-capacity data volume storing offline installer assets (`InstallAssistant.pkg` 18.38 GB).
- **Unaccelerated Graphics Configuration (Pre-RX 5700 XT)**:
  - Preserved working macOS Sonoma / Maxwell GPU configuration as `G:\Hackintosh\EFI\OC\config.sonoma_maxwell.plist`.
  - Configured `boot-args` for unaccelerated VESA/GOP mode: `-v keepsyms=1 debug=0x100 alcid=11 nv_disable=1 -radvesa`. Stripped legacy Nvidia Web Driver / OCLP root patch flags (`amfi=0x80 ngfxgl=1 ngfxcompat=1 nvda_drv_vrl=1 ipc_control_port_options=0`) since root patches were reverted and the physical GPU is temporarily the GTX 970 awaiting the user's AMD Radeon RX 5700 XT physical installation.
  - Set `csr-active-config` to `03080000` (permissive SIP for installer setup) and `prev-lang:kbd` to `es-ES:87`.
- **Kext Auditing & Tahoe Compatibility**:
  - Confirmed all core Acidanthera kexts are up to date: `Lilu` 1.7.2, `VirtualSMC` 1.3.8, `WhateverGreen` 1.7.1, `AppleALC` 1.9.8, `LucyRTL8125Ethernet` 1.2.3, `USBToolBox` 1.1, `UTBMap` 1.1.
  - Disabled `AirportItlwm.kext` (v2.3.0 for Sonoma) in `Kernel -> Add` to eliminate `IOSkywalkFamily` kernel panics in macOS 15+/26; network connectivity provided via onboard Realtek RTL8125 2.5 GbE Ethernet (`LucyRTL8125Ethernet.kext`).
  - Disabled Bluetooth kexts (`IntelBluetoothFirmware.kext`, `IntelBTPatcher.kext`, `BlueToolFixup.kext`) during installation for optimal stability.
  - Verified `MinDate=0` / `MinVersion=0` (APFS driver compatibility) and `ScanPolicy=0` / `SecureBootModel=Disabled`.

### [18-10-2026] — EFI Injections for AMD Radeon RX 5700 XT & Peripheral Re-activation
- **Boot-Args Enhancement (Current `config.plist`)**:
  - Injected `agdpmod=pikera` (required for Navi 10 / RX 5000 series board-id policy matching) and `unfairgva=1` (DRM and hardware video acceleration fix).
  - Maintained `nv_disable=1 -radvesa` strictly for temporary VESA stability while the physical GTX 970 remains installed.
- **Peripheral & Network Stack Activation**:
  - Enabled `itlwm.kext` (v2.3.0) under `Kernel -> Add` to allow clean, non-root-patch Wi-Fi via HeliPort, avoiding `IOSkywalkFamily` kernel panics on modern macOS.
  - Re-enabled the full Intel Bluetooth stack: `IntelBluetoothFirmware.kext`, `IntelBTPatcher.kext`, and `BlueToolFixup.kext`.
- **Dedicated RX 5700 XT Profile (`EFI/OC/config.rx5700xt.plist`)**:
  - Created standalone drop-in configuration for when the physical AMD Radeon RX 5700 XT is installed.
  - Completely stripped `nv_disable=1` and `-radvesa` for immediate native Metal 3 acceleration upon card insertion.
  - Injected PCIe device properties under `DeviceProperties -> Add`:
    - `PciRoot(0x0)/Pci(0x1,0x0)/Pci(0x0,0x0)`: Injected `model = AMD Radeon RX 5700 XT`, `device_type = VGA compatible controller`, and `hda-gfx = onboard-1`.
    - `PciRoot(0x0)/Pci(0x1,0x0)/Pci(0x0,0x1)`: Injected `model = AMD Radeon RX 5700 XT HD Audio Controller`, `device_type = Audio device`, and `hda-gfx = onboard-1`.
  - Re-established 100% native System Integrity Protection (SIP) `csr-active-config = 00000000` (`<data>AAAAAA==</data>`), eliminating all unsigned driver allowances now that OCLP root patches are no longer required.
- **Configuration Verification**:
  - Validated both `config.plist` and `config.rx5700xt.plist` against OpenCore 1.0.8 specifications using `ocvalidate.exe`; zero structural errors or warnings reported.


