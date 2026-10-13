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
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi) | `itlwm.kext` + HeliPort / `AirportItlwm.kext` |
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
