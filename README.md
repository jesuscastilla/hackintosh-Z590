# macOS Installation Guide — ASUS PRIME Z590-P & AMD Radeon RX 5700 XT

A complete, production-grade step-by-step guide to installing modern **macOS (Sonoma / Sequoia / Tahoe)** on an **ASUS PRIME Z590-P WIFI** motherboard powered by an **Intel Core i5-11600K** and an **AMD Radeon RX 5700 XT**. 

This build utilizes **OpenCore 1.0.x**, native Comet Lake CPUID emulation, and native AMD Navi 10 graphics acceleration with **100% full System Integrity Protection (SIP)** enabled.

---

## Legal Disclaimer & Notice

> [!WARNING]
> **Installing macOS on non-Apple hardware violates Apple's End User License Agreement (EULA, Section 2.J)**, which legally restricts macOS to Apple-branded hardware. Doing so may also infringe upon Apple copyright and trademark protections, and may be considered unlawful or prohibited depending on your jurisdiction.
>
> This repository, guides, and configuration assets are provided **strictly for educational, research, and non-commercial experimental purposes**. The author and contributors do not condone, promote, or encourage software piracy or license infringement, and assume absolutely no liability or responsibility for any damages, hardware failure, data loss, or legal consequences resulting from the use or misuse of this information. Proceed strictly at your own discretion.

---

## Hardware Specifications

| Component | Model / Specification | Configuration Role |
|---|---|---|
| **CPU** | Intel Core i5-11600K (Rocket Lake, 6C/12T) | Spoofed to Comet Lake (`0x0A0655`) via `Kernel -> Emulate` |
| **Motherboard** | ASUS PRIME Z590-P WIFI (LGA1200) | Firmware tuned for OpenCore; `UpdateSMBIOSMode=Custom` |
| **GPU** | AMD Radeon RX 5700 XT 8 GB GDDR6 (Navi 10) | **100% Native Metal 3 acceleration** via `agdpmod=pikera` |
| **iGPU** | Intel UHD Graphics 750 | Disabled via `DeviceProperties` (`disable-gpu = 01000000`) |
| **RAM** | 32 GB DDR4 (Dual Channel) | Standard XMP Profile |
| **Storage** | Samsung 860 EVO / NVMe SSD | APFS macOS target volume |
| **Ethernet** | Realtek RTL8125 2.5 GbE | Driven by `LucyRTL8125Ethernet.kext` |
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi) | Driven by `itlwm.kext` managed with `HeliPort.app` |
| **Bluetooth** | Intel AX201 Bluetooth | `IntelBluetoothFirmware` + `IntelBTPatcher` + `BlueToolFixup` |
| **Audio** | Realtek ALC897 | Native AppleHDA patching via `AppleALC.kext` (`alcid=11`) |
| **SMBIOS** | `iMac20,1` | Native match for 10th/11th Gen Intel + AMD Navi GPU |
| **SIP Status** | Fully Enabled (`csr-active-config: 00000000`) | Pristine, sealed system volume (`SSV`) without root patches |

---

## 1. ASUS Motherboard BIOS Settings

Reboot your computer and press `Del` or `F2` to enter the UEFI BIOS setup. Press `F7` to switch to **Advanced Mode**, then configure the following settings:

### Disable
* **Fast Boot** (`Boot -> Fast Boot`)
* **Secure Boot** (`Boot -> Secure Boot -> OS Type`: change from *Windows UEFI* to **Other OS**)
* **CSM (Compatibility Support Module)** (`Boot -> CSM -> Launch CSM`: **Disabled**)
* **Intel SGX** (`Advanced -> CPU Configuration -> Software Guard Extensions`: **Disabled**)
* **CFG Lock** (`Advanced -> CPU Configuration -> CFG Lock`: **Disabled**)
* **Serial / COM Port** (`Advanced -> Onboard Devices Configuration`: **Disabled**)

### Enable
* **VT-x (Intel Virtualization Technology)** (`Advanced -> CPU Configuration`: **Enabled**)
* **Above 4G Decoding** (`Advanced -> PCI Subsystem Settings`: **Enabled**)
* **Resize BAR Support** (`Advanced -> PCI Subsystem Settings`: **Disabled** or **Auto**; controlled via OpenCore)
* **Hyper-Threading** (`Advanced -> CPU Configuration`: **Enabled**)
* **XHCI Hand-off** (`Advanced -> USB Configuration`: **Enabled**)
* **Primary Display**: Set to **PCIE / PEG** (AMD Radeon RX 5700 XT)

Save changes and exit (`F10`).

---

## 2. EFI Architecture & Kernel Extensions

The provided EFI repository is structured to deliver full hardware functionality without requiring unsigned system kexts or breaking the macOS Sealed System Volume:

```text
EFI/
├── BOOT/
│   └── BOOTx64.efi
└── OC/
    ├── ACPI/
    │   ├── SSDT-AWAC.aml              # Disables AWAC and forces legacy RTC
    │   ├── SSDT-EC-USBX-DESKTOP.aml   # Injects dummy EC and USB power regulator
    │   ├── SSDT-PLUG-DRTNIA.aml       # CPU power management (plugin-type=1)
    │   └── SSDT-RHUB.aml              # Resets USB Root Hub for ASUS Z590
    ├── Drivers/
    │   ├── AudioDxe.efi
    │   ├── HfsPlus.efi
    │   ├── OpenCanopy.efi
    │   ├── OpenPartitionDxe.efi
    │   └── OpenRuntime.efi
    ├── Kexts/
    │   ├── Lilu.kext                  # Patching engine
    │   ├── VirtualSMC.kext            # Apple SMC emulator
    │   ├── SMCProcessor.kext          # CPU temperature telemetry
    │   ├── SMCSuperIO.kext            # Fan & motherboard sensor telemetry
    │   ├── WhateverGreen.kext         # Graphics management
    │   ├── AppleALC.kext              # ALC897 audio support
    │   ├── LucyRTL8125Ethernet.kext   # 2.5 GbE Ethernet driver
    │   ├── NVMeFix.kext               # Apple NVMe power management (APST)
    │   ├── itlwm.kext                 # Intel Wi-Fi driver (clean user-space networking)
    │   ├── IntelBluetoothFirmware.kext# Intel Bluetooth firmware loader
    │   ├── IntelBTPatcher.kext        # Intel Bluetooth patcher
    │   ├── BlueToolFixup.kext         # Bluetooth injector for macOS Monterey+
    │   ├── USBToolBox.kext            # USB framework
    │   └── UTBMap.kext                # Tailored USB 2.0 / 3.0 / Type-C port map
    └── config.plist                   # Master OpenCore configuration
```

---

## 3. Generating Your Unique SMBIOS (Mandatory)

> [!IMPORTANT]
> To use Apple Services (iCloud, App Store, iMessage, FaceTime), you must populate `PlatformInfo -> Generic` in `config.plist` with unique serial numbers. Never share your serials publicly.

1. Download [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS).
2. Run `GenSMBIOS.bat` (Windows) or `GenSMBIOS.command` (macOS/Linux).
3. Select option **1** to download the latest MacSerial library.
4. Select option **3** (*Generate SMBIOS*) and enter the model:
   ```text
   iMac20,1
   ```
5. Open `EFI/OC/config.plist` using [ProperTree](https://github.com/corpnewt/ProperTree) and paste the generated values under `PlatformInfo -> Generic`:
   * `SystemProductName`: `iMac20,1`
   * `SystemSerialNumber`: Your generated Serial
   * `MLB`: Your generated Board Serial
   * `SystemUUID`: Your generated UUID
   * `ROM`: Your physical Realtek 2.5 GbE Ethernet MAC address (12 hexadecimal characters, without colons, e.g., `A1B2C3D4E5F6`).

---

## 4. Creating the macOS USB Installer

### Option A: From macOS
1. Download macOS from the App Store or via `InstallAssistant.pkg`.
2. Insert a 16 GB+ USB flash drive and format it via **Disk Utility** as **Mac OS Extended (Journaled)** with a **GUID Partition Map**, naming the volume `MyVolume`.
3. Open Terminal and run the official Apple installer command:
   ```bash
   sudo /Applications/Install\ macOS\ Sonoma.app/Contents/Resources/createinstallmedia --volume /Volumes/MyVolume
   ```
4. Once created, mount the USB's EFI partition and copy the repository's `EFI` folder into the root of the EFI partition.

### Option B: From Windows (Dual-Partition Boot Media)
1. Partition the USB flash drive with a GPT scheme into two volumes:
   * **Partition 1 (EFI/BOOT, 5 GB, FAT32)**: Copy the `EFI` folder and the `com.apple.recovery.boot` payload.
   * **Partition 2 (INSTALLER_DATA, NTFS or exFAT)**: Store offline installation packages.

---

## 5. Step-by-Step Installation Walkthrough

### Step 5.1: Booting OpenCore
1. Insert the USB installer into a USB 2.0 or 3.0 port on the motherboard rear I/O panel.
2. Turn on the PC and tap `F8` to access the ASUS UEFI Boot Menu.
3. Select your USB drive with the prefix **UEFI:** (e.g., `UEFI: SanDisk Ultra...`).
4. At the OpenCore boot picker menu:
   * Select **Reset NVRAM** on your very first boot to clear any motherboard NVRAM conflicts.
   * After the automatic reboot, select **Install macOS** (or the macOS Base System recovery entry).

### Step 5.2: Formatting the Target Drive
1. When the macOS Installer UI appears, select **Disk Utility** and click **Continue**.
2. Click **View** (top-left menu) and choose **Show All Devices**.
3. Select the physical internal drive (e.g., *Samsung SSD 860 EVO* or your dedicated NVMe SSD), NOT a sub-volume.
4. Click **Erase** and configure:
   * **Name**: `Macintosh HD`
   * **Format**: `APFS`
   * **Scheme**: `GUID Partition Map`
5. Click **Erase**, wait for completion, and close Disk Utility.

### Step 5.3: Running the Installer
1. Click **Install macOS** and accept Apple's terms and conditions.
2. Choose **Macintosh HD** as the destination drive and click **Continue**.
3. The installer will begin copying system assets. During installation, the machine will restart 3 to 4 times:
   * **Reboot 1 & 2**: OpenCore will automatically select `macOS Installer` or `Macintosh HD`. Let the system reboot automatically without unplugging the drive.
   * **Reboot 3**: The system will boot into the completed macOS volume.

### Step 5.4: Initial macOS Setup
1. Complete the Setup Assistant (select Region, Keyboard, and Network).
2. You can sign into your Apple ID or skip and sign in later from System Settings.

---

## 6. Post-Installation & EFI Deployment

To boot macOS independently without the USB drive inserted:

1. Open **Terminal** in macOS.
2. Identify the disk identifiers of your USB drive and internal SSD EFI partitions:
   ```bash
   diskutil list
   ```
3. Mount both EFI partitions (replace `X` and `Y` with the corresponding disk slice numbers):
   ```bash
   sudo diskutil mount diskXs1    # USB EFI
   sudo diskutil mount diskYs1    # Internal SSD EFI
   ```
4. Copy the entire `EFI` folder from the USB EFI partition to the internal SSD EFI partition:
   ```bash
   cp -R /Volumes/EFI/EFI /Volumes/EFI\ 1/
   ```
5. Eject the USB drive, reboot your PC, enter the BIOS (`F2`), and set the internal disk's `OpenCore` or `UEFI OS` as the **#1 Boot Priority**.

---

## 7. Peripheral Configuration & Performance Verification

### Graphics Acceleration (AMD Radeon RX 5700 XT)
* Go to ** -> About This Mac -> More Info -> Graphics / Displays**.
* Confirm that **AMD Radeon RX 5700 XT 8 GB** is recognized with **Metal: Supported (Metal 3)**.
* Hardware video decoding (HEVC/H.264) and DRM streaming in Safari are handled natively via `agdpmod=pikera unfairgva=1`.

### Wi-Fi Connectivity (HeliPort)
Because macOS rewritten network architectures (`IOSkywalkFamily`) require compromising SIP to inject legacy drivers, this build uses **itlwm + HeliPort**:
1. Download the latest release of [HeliPort.app](https://github.com/OpenIntelWireless/HeliPort).
2. Drag `HeliPort.app` to your `/Applications` folder.
3. Open `HeliPort` (and set it to launch on login under *System Settings -> General -> Login Items*).
4. Click the Wi-Fi icon in the top menu bar to scan, select, and connect to wireless networks with full WPA2/WPA3 support.

### Bluetooth
* Onboard Intel AX201 Bluetooth operates natively through the paired `IntelBluetoothFirmware` and `BlueToolFixup` kernel extensions.
* Verify connectivity in **System Settings -> Bluetooth**.

### Audio (Realtek ALC897)
* Integrated audio, line-in, and headphone jacks are enabled via `AppleALC.kext` with layout-id `alcid=11`.
* Digital HDMI/DisplayPort audio is routed through the AMD RX 5700 XT HD Audio Controller.

---

## 8. Credits & Third-Party Resources

This Hackintosh build relies on outstanding open-source projects created and maintained by the hackintosh community:

| Project | Developer / Maintainer | Description & Repository Link |
|---|---|---|
| **OpenCorePkg** | [Acidanthera](https://github.com/acidanthera) | Leading UEFI bootloader and kernel patcher: [OpenCorePkg](https://github.com/acidanthera/OpenCorePkg) |
| **Lilu** | [Acidanthera](https://github.com/acidanthera) | Arbitrary kext and process patching engine: [Lilu](https://github.com/acidanthera/Lilu) |
| **WhateverGreen** | [Acidanthera](https://github.com/acidanthera) | Comprehensive graphics management: [WhateverGreen](https://github.com/acidanthera/WhateverGreen) |
| **VirtualSMC** | [Acidanthera](https://github.com/acidanthera) | Advanced Apple SMC emulator and sensor framework: [VirtualSMC](https://github.com/acidanthera/VirtualSMC) |
| **AppleALC** | [Acidanthera](https://github.com/acidanthera) | Native macOS HD audio patcher: [AppleALC](https://github.com/acidanthera/AppleALC) |
| **LucyRTL8125Ethernet** | [Mieze](https://github.com/Mieze) | macOS driver for Realtek RTL8125 2.5 GbE NICs: [LucyRTL8125Ethernet](https://github.com/Mieze/LucyRTL8125Ethernet) |
| **NVMeFix** | [Acidanthera](https://github.com/acidanthera) | Apple NVMe storage power management driver: [NVMeFix](https://github.com/acidanthera/NVMeFix) |
| **itlwm & HeliPort** | [OpenIntelWireless](https://github.com/OpenIntelWireless) | Intel Wi-Fi drivers and companion status bar UI: [itlwm](https://github.com/OpenIntelWireless/itlwm) & [HeliPort](https://github.com/OpenIntelWireless/HeliPort) |
| **IntelBluetoothFirmware** | [OpenIntelWireless](https://github.com/OpenIntelWireless) | Intel Bluetooth firmware loader and kernel patchers: [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) |
| **BlueToolFixup** | [Acidanthera](https://github.com/acidanthera) | Bluetooth stack compatibility provider: [BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM) |
| **USBToolBox** | [USBToolBox](https://github.com/USBToolBox) | Cross-platform USB port mapping utility: [USBToolBox](https://github.com/USBToolBox/tool) |
| **GenSMBIOS** | [CorpNewt](https://github.com/corpnewt) | Automated SMBIOS data generator: [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS) |
| **ProperTree** | [CorpNewt](https://github.com/corpnewt) | Cross-platform GUI plist editor: [ProperTree](https://github.com/corpnewt/ProperTree) |
| **Dortania Guides** | [Dortania](https://github.com/dortania) | Definitive OpenCore configuration documentation: [Dortania Guide](https://dortania.github.io/OpenCore-Install-Guide/) |

---

> [!NOTE]
> For detailed changelogs, version history, and release notes, visit [GitHub Releases](https://github.com/jesuscastilla/hackintosh-Z590/releases).

