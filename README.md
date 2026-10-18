# macOS Installation Guide — ASUS PRIME Z590-P & AMD Radeon RX 5700 XT

A complete, production-grade, step-by-step deployment guide to installing modern **macOS (Sonoma 14.x / Sequoia 15.x / Tahoe 26.x)** on an **ASUS PRIME Z590-P WIFI** motherboard equipped with an **Intel Core i5-11600K** and an **AMD Radeon RX 5700 XT**.

This setup utilizes **OpenCore 1.0.x**, native Comet Lake CPUID hardware emulation, and native AMD Navi 10 Metal 3 GPU acceleration while preserving **100% full System Integrity Protection (SIP)** with an uncompromised Sealed System Volume (`SSV`).

---

## Legal Disclaimer & Notice

> [!WARNING]
> **Installing macOS on non-Apple hardware violates Apple's End User License Agreement (EULA, Section 2.J)**, which legally restricts macOS solely to Apple-branded computers. Creating or operating a Hackintosh may also infringe upon Apple copyright and intellectual property rights and may be deemed unlawful or restricted depending on your jurisdiction.
>
> This repository, guides, and configuration assets are provided **strictly for educational, research, and non-commercial experimental purposes**. The author and contributors do not condone, promote, or encourage software piracy or license infringement, and assume absolutely no liability or responsibility for any damages, hardware failure, data loss, or legal consequences resulting from the use or misuse of this information. Proceed strictly at your own discretion and risk.

---

## Hardware Specifications

| Component | Model / Specification | Hackintosh Configuration Role |
|---|---|---|
| **CPU** | Intel Core i5-11600K (Rocket Lake, 6C/12T @ 3.9–4.9 GHz) | Spoofed to Comet Lake (`0x0A0655`) via `Kernel -> Emulate` |
| **Motherboard** | ASUS PRIME Z590-P WIFI (LGA1200, Intel Z590 Chipset) | Optimized UEFI firmware; `UpdateSMBIOSMode=Custom` |
| **GPU** | AMD Radeon RX 5700 XT 8 GB GDDR6 (Navi 10 Architecture) | **100% Native Metal 3 acceleration** via `agdpmod=pikera` |
| **iGPU** | Intel UHD Graphics 750 | Disabled via `DeviceProperties` (`disable-gpu = 01000000`) |
| **RAM** | 32 GB DDR4 3200 MHz (Dual Channel) | Standard XMP Profile 1 enabled |
| **Storage** | Samsung 970 PRO / 860 EVO (NVMe M.2 / SATA SSD) | APFS container; optimized with `NVMeFix` & `SetApfsTrimTimeout=0` |
| **Ethernet** | Realtek RTL8125 2.5 Gbps PCIe Controller | Fully native 2.5 GbE networking via `LucyRTL8125Ethernet.kext` |
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi 802.11ax) | User-space networking via `itlwm.kext` and `HeliPort.app` |
| **Bluetooth** | Intel AX201 Bluetooth 5.2 | Firmware injected via `IntelBluetoothFirmware` + `BlueToolFixup` |
| **Audio** | Realtek ALC897 High Definition Audio | Native AppleHDA injection via `AppleALC.kext` (`alcid=11`) |
| **SMBIOS** | `iMac20,1` (Retina 5K, 27-inch, 2020) | Native match for 10th/11th Gen Intel + AMD Navi 10 GPU |
| **Security (SIP)** | Fully Enabled (`csr-active-config: 00000000`) | Pristine, cryptographically sealed root filesystem (`SSV`) |

---

## Prerequisites & Requirements

Before beginning the installation process, ensure you have gathered all required hardware, software, and physical connections:

### 1. Hardware Requirements
* **Target Computer**: Desktop PC built with the exact (or closely matching) components listed in the Hardware Specifications above.
* **Storage Drive**: A dedicated internal SSD (e.g., Samsung 970 PRO NVMe or SATA SSD) formatted entirely as GUID Partition Map (GPT). All existing data on this drive will be erased.
* **USB Flash Drive**: A reliable USB 2.0 or USB 3.0 drive of **at least 16 GB** (32 GB recommended). Using a rear USB 2.0 port during boot avoids USB 3.0 handshake timeouts.
* **Display Connection**: Connect your monitor directly to the **AMD Radeon RX 5700 XT** using a DisplayPort or HDMI cable. Do **not** connect the monitor to the motherboard video outputs.

### 2. Mandatory Network Requirement (Wired Ethernet)
> [!IMPORTANT]
> **A PHYSICAL WIRED ETHERNET CONNECTION (RJ-45 CABLE) IS STRICTLY MANDATORY DURING INSTALLATION.**
> 
> * The onboard Intel Wi-Fi 6 AX201 relies on `itlwm.kext`, which functions exclusively through the user-space companion utility **HeliPort.app**.
> * Because `HeliPort.app` cannot run within the macOS Recovery or initial Installer environment, **Wi-Fi will NOT be available during installation**.
> * You must connect an active Ethernet cable to the motherboard's **Realtek 2.5 GbE port**. The installer will automatically recognize the wired connection through `LucyRTL8125Ethernet.kext` to synchronize system time, verify Apple installation packages, and download required recovery payloads.

### 3. Software & Environment Requirements
* **Host Computer**: A working PC running Windows 10/11, macOS, or Linux to prepare the USB flash drive and configure the EFI.
* **Python 3.x**: Installed on the host machine to run OpenCore utilities ([Python Official Website](https://www.python.org/)).
* **ProperTree**: Cross-platform GUI editor for modifying OpenCore plist files ([ProperTree GitHub](https://github.com/corpnewt/ProperTree)).
* **GenSMBIOS**: Script to generate valid Apple serial numbers ([GenSMBIOS GitHub](https://github.com/corpnewt/GenSMBIOS)).

---

## 1. ASUS Motherboard BIOS Configuration

Restart your PC and repeatedly press `Del` or `F2` to enter the UEFI BIOS setup. Press `F7` to switch to **Advanced Mode**, then configure the following settings:

### Settings to Disable
* **Fast Boot** (`Boot -> Fast Boot` -> **Disabled**): Prevents the firmware from bypassing hardware re-initialization required by macOS.
* **Secure Boot** (`Boot -> Secure Boot -> OS Type` -> **Other OS**): Disables Microsoft-exclusive Secure Boot enforcement to allow OpenCore's bootloader.
* **CSM (Compatibility Support Module)** (`Boot -> CSM (Compatibility Support Module)` -> **Disabled**): Forces native UEFI GOP video initialization and disables legacy 16-bit BIOS emulation.
* **Intel SGX** (`Advanced -> CPU Configuration -> Software Guard Extensions (SGX)` -> **Disabled**): Unused by macOS and known to cause memory map conflicts.
* **CFG Lock** (`Advanced -> CPU Configuration -> CFG Lock` -> **Disabled**): Unlocks MSR `0xE2`, allowing native CPU power management without requiring `AppleCpuPmCfgLock` kernel quirks.
* **Serial / COM Port** (`Advanced -> Onboard Devices Configuration -> Serial Port Configuration` -> **Disabled**): Prevents boot stalls caused by legacy serial polling.

### Settings to Enable
* **VT-x (Intel Virtualization Technology)** (`Advanced -> CPU Configuration` -> **Enabled**): Enables virtualization required by the macOS kernel and hypervisors.
* **Above 4G Decoding** (`Advanced -> PCI Subsystem Settings` -> **Enabled**): Allows 64-bit PCI devices to allocate resources in high memory address spaces; mandatory for modern GPUs.
* **Resize BAR Support** (`Advanced -> PCI Subsystem Settings` -> **Disabled** or **Auto**): OpenCore manages ReBAR natively via `ResizeGpuBars`.
* **Hyper-Threading** (`Advanced -> CPU Configuration` -> **Enabled**): Enables all 12 virtual threads on the i5-11600K.
* **XHCI Hand-off** (`Advanced -> USB Configuration` -> **Enabled**): Ensures the operating system takes full control of the USB controller from the UEFI firmware.
* **Primary Display**: Set to **PCIE / PEG** (`Advanced -> System Agent (SA) Configuration -> Graphics Configuration -> Primary Display` -> **PEG Slot**).

Press `F10` to save changes and restart.

---

## 2. EFI Architecture & Kext Stack

The repository provides a clean, pre-configured OpenCore 1.0.8 EFI tailored to this hardware specification:

```text
EFI/
├── BOOT/
│   └── BOOTx64.efi                    # Primary OpenCore fallback bootloader
└── OC/
    ├── ACPI/
    │   ├── SSDT-AWAC.aml              # Disables RTC timer AWAC and forces legacy RTC
    │   ├── SSDT-EC-USBX-DESKTOP.aml   # Creates dummy Embedded Controller & USB power regulation
    │   ├── SSDT-PLUG-DRTNIA.aml       # Injects plugin-type=1 for native XCPM CPU power management
    │   └── SSDT-RHUB.aml              # Resets USB Root Hub state on ASUS Z590 motherboards
    ├── Drivers/
    │   ├── AudioDxe.efi               # UEFI boot chime and startup sound driver
    │   ├── HfsPlus.efi                # High-performance Apple HFS+ filesystem driver
    │   ├── OpenCanopy.efi             # High-resolution graphical boot menu interface
    │   ├── OpenPartitionDxe.efi       # Advanced partition scheme support
    │   ├── OpenRuntime.efi            # Mandatory OpenCore NVRAM & memory management runtime
    │   └── ResetNvramEntry.efi        # Dedicated boot picker entry for clearing NVRAM
    ├── Kexts/
    │   ├── Lilu.kext                  # Core arbitrary kernel and kext patcher
    │   ├── VirtualSMC.kext            # Apple SMC hardware emulator
    │   ├── SMCProcessor.kext          # CPU temperature and frequency monitoring
    │   ├── SMCSuperIO.kext            # Motherboard sensor and fan telemetry
    │   ├── WhateverGreen.kext         # Video card management and display output patching
    │   ├── AppleALC.kext              # Realtek ALC897 audio patcher (layout-id alcid=11)
    │   ├── LucyRTL8125Ethernet.kext   # Realtek RTL8125 2.5 GbE network driver
    │   ├── NVMeFix.kext               # Autonomous power state transition (APST) for NVMe SSDs
    │   ├── itlwm.kext                 # Intel Wi-Fi driver (user-space via HeliPort)
    │   ├── IntelBluetoothFirmware.kext# Intel AX201 Bluetooth firmware loader
    │   ├── IntelBTPatcher.kext        # Intel Bluetooth kernel extension patcher
    │   ├── BlueToolFixup.kext         # Bluetooth stack compatibility injector for modern macOS
    │   ├── USBToolBox.kext            # Kernel framework for custom USB port mapping
    │   └── UTBMap.kext                # Tailored USB port mapping for the ASUS PRIME Z590-P
    └── config.plist                   # Master OpenCore configuration file
```

---

## 3. Generating Your Unique SMBIOS Identifiers (Mandatory)

> [!IMPORTANT]
> To ensure Apple Services (iCloud, App Store, iMessage, FaceTime, AirDrop) function correctly and avoid account blacklisting, you **must** generate your own distinct serial numbers before booting.

1. Download and extract [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS).
2. Execute `GenSMBIOS.bat` (Windows) or `GenSMBIOS.command` (macOS/Linux).
3. Type `1` and press `Enter` to download the latest `MacSerial` binary.
4. Type `3` and press `Enter` (*Generate SMBIOS*), then input the exact model name:
   ```text
   iMac20,1
   ```
5. The utility will generate a block containing:
   * **Type**: `iMac20,1`
   * **Serial**: e.g., `C02XXXXXXXXX`
   * **Board Serial (MLB)**: e.g., `C02XXXXXXXXXXXXXX`
   * **SmUUID**: e.g., `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`
   * **Apple ROM**: Physical MAC address
6. Open your `EFI/OC/config.plist` using [ProperTree](https://github.com/corpnewt/ProperTree).
7. Navigate to `PlatformInfo -> Generic` and populate the fields:
   * `SystemProductName`: `iMac20,1`
   * `SystemSerialNumber`: Insert your generated `Serial`.
   * `MLB`: Insert your generated `Board Serial`.
   * `SystemUUID`: Insert your generated `SmUUID`.
   * `ROM`: Enter your physical Realtek 2.5 GbE Ethernet MAC address as 12 hexadecimal characters without colons (find it in Windows via `getmac /v` or in BIOS under Onboard Devices).
8. Save the file (`Ctrl + S` or `Cmd + S`).

---

## 4. Creating the Official macOS Bootable USB Drive

Use a standard single-partition USB drive formatted as FAT32 or Mac OS Extended.

### Method A: From a Mac or Existing macOS Environment (Recommended)
1. Download the macOS installer application (e.g., `Install macOS Sonoma.app` or `Install macOS Sequoia.app`) from the Mac App Store.
2. Insert your 16 GB+ USB flash drive.
3. Open **Disk Utility**, click **View -> Show All Devices**, select the top-level USB disk drive, click **Erase**, and format it with:
   * **Name**: `MyVolume`
   * **Format**: `Mac OS Extended (Journaled)`
   * **Scheme**: `GUID Partition Map`
4. Open **Terminal** and execute the official Apple creation utility:
   ```bash
   sudo /Applications/Install\ macOS\ Sonoma.app/Contents/Resources/createinstallmedia --volume /Volumes/MyVolume
   ```
5. Once completed, mount the hidden EFI partition of the USB drive:
   ```bash
   sudo diskutil mount diskXs1
   ```
   *(Replace `diskXs1` with your USB drive's EFI slice identifier obtained via `diskutil list`).*
6. Copy the complete `EFI` directory from this repository into the root of the mounted EFI partition so that the file path is `EFI/OC/OpenCore.efi`.

### Method B: From Windows (Cross-Platform Recovery Method)
1. Insert your 16 GB+ USB flash drive.
2. Open **Disk Management**, format the drive as a single **FAT32** partition using a **GUID Partition Map (GPT)**, and label it `OPENCORE`.
3. Download [OpenCorePkg Official Release](https://github.com/acidanthera/OpenCorePkg/releases).
4. Navigate to `OpenCorePkg/Utilities/macrecovery/` in PowerShell and download the official Apple Recovery files:
   ```powershell
   python .\macrecovery.py -b Mac-075A03B3ED9C4E0B -m 00000000000000000 download
   ```
5. Move the resulting `com.apple.recovery.boot` folder into the root of your USB flash drive.
6. Copy the entire `EFI` folder from this repository into the root of the USB flash drive.

---

## 5. Step-by-Step Installation Walkthrough

### Step 5.1: Initial Boot & NVRAM Reset
1. Ensure your **Ethernet cable is securely plugged in** to the rear Realtek 2.5 GbE port.
2. Insert the USB drive into a rear USB 2.0/3.0 port on the motherboard I/O shield.
3. Turn on the PC and immediately tap `F8` to display the ASUS Boot Device selection menu.
4. Select your USB drive with the prefix **UEFI:** (e.g., `UEFI: SanDisk, Partition 1`).
5. In the OpenCore Boot Picker:
   * Press `Space` if auxiliary tools are hidden.
   * Highlight **Reset NVRAM** and press `Enter`. The computer will immediately reboot; this purges any stale motherboard memory entries.
   * Tap `F8` again upon reboot, re-select the USB drive, and choose **Install macOS** (or `macOS Base System`).
   * A stream of verbose kernel messages (`-v`) will scroll across the screen. This is normal and will lead to the graphical recovery interface within 1–2 minutes.

### Step 5.2: Disk Partitioning in Disk Utility
1. On the macOS Recovery menu, select **Disk Utility** and click **Continue**.
2. Click the **View** icon in the upper-left corner and select **Show All Devices** (`Cmd + 2`).
3. Under the *Internal* section, select the root parent drive of your destination disk (e.g., `Samsung SSD 970 PRO 512GB`), **NOT** an existing sub-partition.
4. Click the **Erase** button in the top toolbar and configure:
   * **Name**: `Macintosh HD`
   * **Format**: `APFS`
   * **Scheme**: `GUID Partition Map`
5. Click **Erase**. Disk Utility will create the container and APFS volumes. When finished, close Disk Utility.

### Step 5.3: Running the macOS Installer
1. Back on the main menu, select **Install macOS** and click **Continue**.
2. Agree to the Apple Software License terms.
3. Select **Macintosh HD** as the destination drive and click **Continue**.
4. The initial installation phase will copy essential files to your internal drive (typically 15–25 minutes).
5. The computer will restart automatically several times:
   * **Reboot 1**: OpenCore will automatically boot into `macOS Installer`. Allow it to boot without manual intervention.
   * **Reboot 2 & 3**: OpenCore will boot into `Macintosh HD`. Progress bars will display "About X minutes remaining".
   * **Crucial**: **Do NOT unplug the USB drive during these restarts.**

### Step 5.4: Initial Onboarding & Setup Assistant
1. When the macOS Setup Assistant appears, select your Country/Region, Keyboard Layout, and Accessibility preferences.
2. Under Network, your wired connection will already be active and connected.
3. Sign in with your Apple ID or select *Set Up Later*.
4. Complete the remaining steps to land on the macOS desktop.

---

## 6. Post-Installation & Internal EFI Deployment

To boot macOS independently without relying on the USB flash drive:

1. Open **Terminal** in macOS.
2. List all connected disks and identify the EFI partition of both your USB drive and your internal SSD:
   ```bash
   diskutil list
   ```
3. Mount both EFI partitions (substitute `X` and `Y` with your actual disk identifiers):
   ```bash
   sudo diskutil mount diskXs1    # USB flash drive EFI
   sudo diskutil mount diskYs1    # Internal SSD EFI (e.g., disk0s1)
   ```
4. Copy the entire `EFI` directory from the USB EFI partition to the internal SSD EFI partition:
   ```bash
   cp -R /Volumes/EFI/EFI /Volumes/EFI\ 1/
   ```
5. Confirm that `/Volumes/EFI\ 1/EFI/OC/OpenCore.efi` is present on the internal disk.
6. Safely unmount both EFI partitions:
   ```bash
   sudo diskutil unmount /Volumes/EFI
   sudo diskutil unmount /Volumes/EFI\ 1
   ```
7. Eject and physically remove the USB drive.
8. Reboot the computer, press `F2` to enter BIOS, navigate to `Boot -> Boot Option Priorities`, and set `OpenCore` or `UEFI OS (Samsung SSD...)` as **Boot Option #1**. Save and restart.

---

## 7. Peripheral Configuration & Hardware Verification

### 1. Full Graphics Acceleration (AMD Radeon RX 5700 XT)
* Open ** -> About This Mac -> More Info -> Graphics / Displays**.
* Confirm that **AMD Radeon RX 5700 XT 8 GB** is recognized with **Metal: Supported (Metal 3)**.
* Verify in Terminal that hardware video acceleration is fully operational:
  ```bash
  system_profiler SPDisplaysDataType | grep "Metal"
  ```
* DRM streaming in Safari (Apple TV+, Netflix 4K) and HEVC/H.264 video encoding operate natively through WhateverGreen's `agdpmod=pikera unfairgva=1`.

### 2. Wi-Fi Setup via HeliPort
* Download the latest release of [HeliPort.app](https://github.com/OpenIntelWireless/HeliPort/releases).
* Drag `HeliPort.app` into your `/Applications` directory.
* Launch **HeliPort**. A Wi-Fi antenna icon will appear in the top menu bar.
* Click the icon, select your wireless network, enter your password, and connect.
* Configure HeliPort to launch automatically on boot: navigate to **System Settings -> General -> Login Items** and add `HeliPort` to the *Open at Login* list.

### 3. Bluetooth Configuration
* Onboard Intel AX201 Bluetooth operates natively via `IntelBluetoothFirmware.kext` and `BlueToolFixup.kext`.
* Verify that Bluetooth is enabled under **System Settings -> Bluetooth**, then pair your wireless accessories (AirPods, Magic Mouse, Magic Keyboard).

### 4. Audio Verification (Realtek ALC897)
* Navigate to **System Settings -> Sound -> Output**.
* Verify that *Internal Speakers* (rear green jack), *Headphones* (front panel), and *DisplayPort/HDMI Audio* (routed via the RX 5700 XT HD Audio Controller) are listed and functional.
* Input microphone testing can be performed in **Sound -> Input**.

---

## 8. Credits & Third-Party Resources

This Hackintosh configuration is made possible by the dedicated developers in the open-source community:

| Project | Author / Maintainer | Description & Official Repository |
|---|---|---|
| **OpenCorePkg** | [Acidanthera](https://github.com/acidanthera) | Leading UEFI bootloader and kernel patcher: [OpenCorePkg](https://github.com/acidanthera/OpenCorePkg) |
| **Lilu** | [Acidanthera](https://github.com/acidanthera) | Dynamic arbitrary kernel and kext patching framework: [Lilu](https://github.com/acidanthera/Lilu) |
| **WhateverGreen** | [Acidanthera](https://github.com/acidanthera) | Graphics card initialization and DRM management: [WhateverGreen](https://github.com/acidanthera/WhateverGreen) |
| **VirtualSMC** | [Acidanthera](https://github.com/acidanthera) | Advanced Apple SMC hardware emulation framework: [VirtualSMC](https://github.com/acidanthera/VirtualSMC) |
| **AppleALC** | [Acidanthera](https://github.com/acidanthera) | High Definition Audio patcher for non-native codecs: [AppleALC](https://github.com/acidanthera/AppleALC) |
| **LucyRTL8125Ethernet** | [Mieze](https://github.com/Mieze) | Native macOS driver for Realtek RTL8125 2.5 GbE NICs: [LucyRTL8125Ethernet](https://github.com/Mieze/LucyRTL8125Ethernet) |
| **NVMeFix** | [Acidanthera](https://github.com/acidanthera) | Apple NVMe storage power management and APST driver: [NVMeFix](https://github.com/acidanthera/NVMeFix) |
| **itlwm & HeliPort** | [OpenIntelWireless](https://github.com/OpenIntelWireless) | Intel Wi-Fi drivers and companion status bar client: [itlwm](https://github.com/OpenIntelWireless/itlwm) & [HeliPort](https://github.com/OpenIntelWireless/HeliPort) |
| **IntelBluetoothFirmware** | [OpenIntelWireless](https://github.com/OpenIntelWireless) | Intel Bluetooth firmware loader and kernel patchers: [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) |
| **BlueToolFixup** | [Acidanthera](https://github.com/acidanthera) | Bluetooth subsystem compatibility provider for modern macOS: [BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM) |
| **USBToolBox** | [USBToolBox](https://github.com/USBToolBox) | Cross-platform USB port mapping and configuration tool: [USBToolBox](https://github.com/USBToolBox/tool) |
| **GenSMBIOS** | [CorpNewt](https://github.com/corpnewt) | Automated SMBIOS data generator: [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS) |
| **ProperTree** | [CorpNewt](https://github.com/corpnewt) | Cross-platform GUI plist configuration editor: [ProperTree](https://github.com/corpnewt/ProperTree) |
| **Dortania Guides** | [Dortania](https://github.com/dortania) | Comprehensive OpenCore technical documentation: [Dortania Guide](https://dortania.github.io/OpenCore-Install-Guide/) |

---

> [!NOTE]
> For release downloads, version tags, and historic technical changelogs, please visit [GitHub Releases](https://github.com/jesuscastilla/hackintosh-Z590/releases).
