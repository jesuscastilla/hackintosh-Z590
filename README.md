# macOS 26 (Tahoe) Installation Guide — ASUS PRIME Z590-P & AMD Radeon RX 5700 XT

A complete, production-grade, step-by-step deployment guide to installing and running **macOS 26 (macOS Tahoe)** on an **ASUS PRIME Z590-P WIFI** motherboard equipped with an **Intel Core i5-11600K** processor and an **AMD Radeon RX 5700 XT 8 GB** graphics card.

This configuration is built with **OpenCore 1.0.8**, utilizing native Comet Lake CPUID emulation, native AMD Navi 10 Metal 3 GPU acceleration, and specialized kernel routing tailored specifically to this hardware profile.

---

## Legal Disclaimer & Notice

> [!WARNING]
> **Installing macOS on non-Apple hardware violates Apple's End User License Agreement (EULA, Section 2.J)**, which legally restricts macOS solely to Apple-branded computers. Creating or operating a Hackintosh may also infringe upon Apple copyright and intellectual property rights and may be deemed unlawful or restricted depending on your jurisdiction.
>
> This repository, guides, and configuration assets are provided **strictly for educational, research, and non-commercial experimental purposes**. The author and contributors do not condone, promote, or encourage software piracy or license infringement, and assume absolutely no liability or responsibility for any damages, hardware failure, data loss, or legal consequences resulting from the use or misuse of this information. Proceed strictly at your own discretion and risk.

---

## Hardware Specifications

| Component | Hardware Model & Specification | OpenCore / macOS Role |
|---|---|---|
| **Motherboard** | ASUS PRIME Z590-P WIFI (LGA1200, Intel Z590 Chipset) | UEFI firmware with custom DSDT ACPI patches |
| **CPU** | Intel Core i5-11600K (Rocket Lake, 6 Cores / 12 Threads @ 3.9–4.9 GHz) | Comet Lake spoof (`0x0A0655`) via `Kernel -> Emulate` |
| **GPU** | AMD Radeon RX 5700 XT 8 GB GDDR6 (Navi 10 Architecture) | **100% Native Metal 3 acceleration** (`agdpmod=pikera`) |
| **iGPU** | Intel UHD Graphics 750 | Disabled via `DeviceProperties` (`disable-gpu = 01000000`) |
| **RAM** | 32 GB DDR4 3200 MHz (Dual Channel) | Standard XMP Profile 1 enabled |
| **Storage** | Samsung 970 PRO / 860 EVO (NVMe M.2 / SATA SSD) | APFS container with `NVMeFix.kext` optimization |
| **Ethernet** | Realtek RTL8125 2.5 Gbps PCIe Controller | Native 2.5 GbE networking mapped to `en0` via `LucyRTL8125Ethernet.kext` |
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi 802.11ax) | Temporarily disabled in `config.plist` (v2.4.0 Tahoe experimental; wired 2.5 GbE recommended) |
| **Bluetooth** | Intel AX201 Bluetooth 5.2 | Firmware injected via `IntelBluetoothFirmware` + `IntelBTPatcher` + `BlueToolFixup` |
| **Audio** | Realtek ALC897 High Definition Audio | `AppleALC.kext` (layout-id 11) + `SimpleHDA.kext` / HAL driver |
| **SMBIOS** | `iMac20,1` (Retina 5K, 27-inch, 2020) | Best match for 10th/11th Gen Intel Core + AMD Navi 10 |

---

## Critical Architecture Quirks & Settings

This configuration contains key settings specifically required to boot macOS 26 (Tahoe) without kernel panics or ACPI hangs on the ASUS Z590 + RX 5700 XT platform:

1. **GPU Resizable BAR Handling (`ResizeAppleGpuBars = 0`)**:
   * Modern ASUS Z590 motherboards with *Above 4G Decoding* and *ReBAR* enabled allocate high 64-bit BAR address ranges (`0x4200000000` / 264 GB).
   * Without adjustment, macOS kernel initializes `AppleACPIPCI` and hangs indefinitely right after CPU thread enumeration.
   * Setting `Booter -> Quirks -> ResizeAppleGpuBars = 0` forces OpenCore to cap the GPU BAR for macOS to standard size, eliminating the boot stall while keeping ReBAR enabled in BIOS for Windows.

2. **Apple Security Enforcement (`SecureBootModel = Disabled`)**:
   * macOS 26 enforces Apple T2 security verification when using the `iMac20,1` SMBIOS profile.
   * Because non-Apple motherboards lack a physical Apple T2 silicon chip, setting `SecureBootModel` to anything other than `Disabled` triggers a Type 14 Page Fault or Type 13 General Protection kernel panic during boot.

3. **Audio Subsystem in macOS 26 Tahoe (`SimpleHDA.kext` & `amfi=0x80`)**:
   * macOS 26 removed legacy AppleHDA drivers for older Realtek codecs.
   * `SimpleHDA.kext` combined with `SimpleHDA-HAL-1.0.0.pkg` restores full onboard Realtek ALC897 audio support.
   * This requires `amfi=0x80` in `boot-args` and `csr-active-config` set to `030A0000` (`<03 0a 00 00>`).

4. **Primary Network Binding (`en0`)**:
   * `DeviceProperties` injects `built-in = 01` at `PciRoot(0x0)/Pci(0x1C,0x7)/Pci(0x0,0x0)` to guarantee the Realtek 2.5 GbE controller is registered as `en0`, which is mandatory for Apple ID, iCloud, and iServices.

---

## Repository Structure

```text
Hackintosh/
├── Docs/                             # Technical references and diagnostics documentation
├── EFI/
│   ├── BOOT/
│   │   └── BOOTx64.efi               # OpenCore fallback bootloader
│   └── OC/
│       ├── ACPI/
│       │   ├── SSDT-AWAC.aml         # Disables RTC timer AWAC and forces legacy RTC
│       │   ├── SSDT-EC-USBX-DESKTOP.aml # Injects dummy Embedded Controller & USB power
│       │   ├── SSDT-PLUG-DRTNIA.aml  # Injects plugin-type=1 for native XCPM CPU power management
│       │   └── SSDT-RHUB.aml         # Resets USB Root Hub state on ASUS Z590 motherboards
│       ├── Drivers/
│       │   ├── HfsPlus.efi           # High-performance Apple HFS+ filesystem driver
│       │   ├── OpenCanopy.efi        # High-resolution graphical boot menu interface
│       │   ├── OpenRuntime.efi       # Mandatory OpenCore NVRAM & memory management runtime
│       │   └── ResetNvramEntry.efi   # Boot menu entry to clear NVRAM
│       ├── Kexts/
│       │   ├── AirportItlwm.kext     # Intel Wi-Fi driver (v2.4.0 Tahoe)
│       │   ├── AppleALC.kext         # Realtek ALC897 audio patcher (alcid=11)
│       │   ├── BlueToolFixup.kext    # Bluetooth compatibility framework
│       │   ├── IntelBluetoothFirmware.kext # Intel AX201 Bluetooth firmware loader
│       │   ├── IntelBTPatcher.kext   # Intel Bluetooth kernel extension patcher
│       │   ├── Lilu.kext             # Core kernel and kext patcher
│       │   ├── LucyRTL8125Ethernet.kext # Realtek RTL8125 2.5 GbE network driver
│       │   ├── NVMeFix.kext          # Autonomous power state transition (APST) for NVMe SSDs
│       │   ├── SimpleHDA.kext        # Audio driver for macOS 26 Tahoe AppleHDA restoration
│       │   ├── SMCProcessor.kext     # CPU temperature and frequency monitoring
│       │   ├── SMCSuperIO.kext       # Motherboard sensor and fan telemetry
│       │   ├── USBToolBox.kext       # USB framework driver
│       │   ├── UTBMap.kext           # Custom USB port map for ASUS PRIME Z590-P
│       │   ├── VirtualSMC.kext       # Apple SMC hardware emulator
│       │   └── WhateverGreen.kext    # AMD Navi 10 video initialization (agdpmod=pikera)
│       └── config.plist              # OpenCore 1.0.8 master configuration
│   └── refind/                       # rEFInd multi-boot menu (Windows, macOS, GBOS)
│       ├── refind.conf               # Multi-boot configuration with strict manual stanzas
│       └── ...                       # Drivers, icons, and glassy visual theme
├── SSDTs/                            # Compiled ACPI AML reference files
├── Utilities/
│   ├── macrecovery.py                # Apple Recovery image downloader
│   ├── macserial.exe                 # SMBIOS serial number generator
│   ├── ocvalidate.exe                # OpenCore configuration validator
│   ├── Set-Refind.ps1                # Dual-boot UEFI entry configuration script
│   ├── SimpleHDA-HAL-1.0.0.pkg       # Audio HAL plugin installer for macOS 26
│   └── Validate-EFI.ps1              # Automated OpenCore EFI verification suite
├── .gitignore
└── README.md
```

---

## 1. ASUS PRIME Z590-P BIOS Configuration

Reboot your PC and press `Del` or `F2` to enter the UEFI BIOS setup. Press `F7` to switch to **Advanced Mode**, then configure the following settings:

### Settings to Disable
* **Fast Boot** (`Boot -> Fast Boot` -> **Disabled**): Prevents the firmware from skipping full hardware initialization.
* **Secure Boot** (`Boot -> Secure Boot -> OS Type` -> **Other OS**): Disables Windows-exclusive Secure Boot enforcement.
* **CSM (Compatibility Support Module)** (`Boot -> CSM` -> **Disabled**): Forces UEFI Native GOP video initialization and disables legacy 16-bit BIOS emulation.
* **Intel SGX** (`Advanced -> CPU Configuration -> SGX` -> **Disabled**): Incompatible with macOS memory mappings.
* **CFG Lock** (`Advanced -> CPU Configuration -> CFG Lock` -> **Disabled**): Unlocks MSR `0xE2` for native XCPM CPU power management.
* **Serial / COM Port** (`Advanced -> Onboard Devices -> Serial Port` -> **Disabled**): Prevents boot hangs caused by legacy serial polling.

### Settings to Enable
* **VT-x (Intel Virtualization)** (`Advanced -> CPU Configuration` -> **Enabled**): Required for hypervisors and macOS kernel subsystem.
* **VT-d (Intel Directed I/O)** (`Advanced -> System Agent (SA) Configuration` -> **Enabled**): OpenCore manages direct I/O mapping natively.
* **Above 4G Decoding** (`Advanced -> PCI Subsystem Settings` -> **Enabled**): Allows 64-bit PCI devices to allocate resources in high memory ranges.
* **Resize BAR Support** (`Advanced -> PCI Subsystem Settings` -> **Enabled** or **Auto**): OpenCore manages GPU BAR sizing via `ResizeAppleGpuBars = 0`.
* **Hyper-Threading** (`Advanced -> CPU Configuration` -> **Enabled**): Enables all 12 threads on the i5-11600K.
* **XHCI Hand-off** (`Advanced -> USB Configuration` -> **Enabled**): Ensures macOS takes full ownership of the USB controllers.
* **Primary Display**: Set to **PCIE / PEG** (`Advanced -> System Agent (SA) Configuration -> Graphics Configuration -> Primary Display` -> **PEG Slot**).

Press `F10` to save changes and restart.

---

## 2. Generating Unique SMBIOS Credentials (Mandatory)

To use iCloud, the App Store, and iServices without account conflicts, generate your own distinct serial numbers before booting macOS:

1. Open PowerShell inside the `Utilities\` folder:
   ```powershell
   cd G:\Hackintosh\Utilities
   .\macserial.exe -m "iMac20,1" -n 1
   ```
   This will output a line formatted as `SystemSerialNumber | MLB` (e.g. `C02GN8ZLPN5T | C02145700GUPHC1UE`).

2. Generate a new System UUID:
   ```powershell
   [guid]::NewGuid().ToString().ToUpper()
   ```

3. Obtain your physical Realtek 2.5 GbE MAC address (e.g. `FC:34:97:A6:C2:EA`).

4. Open `EFI/OC/config.plist` using ProperTree or a text editor and enter these values under **PlatformInfo -> Generic**:
   * `SystemProductName`: `iMac20,1`
   * `SystemSerialNumber`: Your generated serial number
   * `MLB`: Your generated MLB board serial (17 characters)
   * `SystemUUID`: Your generated UUID
   * `ROM`: Your 6-byte Ethernet MAC address in data format (e.g., `<data>/DSXpsLq</data>`)

5. Validate the configuration using `ocvalidate.exe`:
   ```powershell
   .\ocvalidate.exe ..\EFI\OC\config.plist
   ```
   Ensure it returns: `Completed validating config.plist in X ms. No issues found.`

---

## 3. Creating the Bootable macOS 26 USB Installer

### Method A: From an Existing Mac or macOS Environment
1. Download the macOS 26 installer application to `/Applications`.
2. Format a 16 GB+ USB flash drive with **GUID Partition Map** and **Mac OS Extended (Journaled)**, labeled `MyVolume`.
3. Run `createinstallmedia` in Terminal:
   ```bash
   sudo /Applications/Install\ macOS\ 26.app/Contents/Resources/createinstallmedia --volume /Volumes/MyVolume
   ```
4. Mount the USB EFI partition:
   ```bash
   sudo diskutil mount diskXs1
   ```
5. Copy the entire `EFI` folder from this repository to the root of the USB EFI partition (`/Volumes/EFI/EFI/OC/OpenCore.efi`).

### Method B: From Windows (macrecovery)
1. Format a 16 GB+ USB flash drive as **FAT32** with **GUID Partition Map (GPT)** and label it `OPENCORE`.
2. Open PowerShell in `Utilities\`:
   ```powershell
   python .\macrecovery.py -b Mac-075A03B3ED9C4E0B -m 00000000000000000 download
   ```
3. Move the downloaded `com.apple.recovery.boot` folder to the root of your USB flash drive.
4. Copy the entire `EFI` folder from this repository to the root of your USB flash drive.

---

## 4. Step-by-Step Installation Walkthrough

### Step 4.1: Initial Boot & NVRAM Reset
1. Ensure your **Ethernet cable is firmly connected** to the rear Realtek 2.5 GbE port.
2. Plug the USB installer into a rear motherboard USB port (avoid front panel hubs).
3. Power on the PC and immediately press `F8` to open the ASUS Boot Device selection menu.
4. Select the entry starting with **UEFI:** corresponding to your USB drive.
5. In the OpenCore Boot Picker:
   * Select **Reset NVRAM** and press `Enter`. The PC will reboot immediately.
   * Press `F8` again upon reboot, re-select the USB drive, and select **Install macOS 26** (or `macOS Base System`).
   * Verbose kernel logs (`-v`) will display on the screen; within 1–2 minutes, the graphical installer recovery environment will load.

### Step 4.2: Drive Partitioning
1. In the Recovery window, select **Disk Utility** and click **Continue**.
2. Click **View -> Show All Devices** (`Cmd + 2`).
3. Select the top-level internal SSD (e.g. `Samsung SSD 970 PRO 512GB`), **not** a sub-volume.
4. Click **Erase** in the top toolbar:
   * **Name**: `Macintosh HD`
   * **Format**: `APFS`
   * **Scheme**: `GUID Partition Map`
5. Click **Erase** to apply, then close Disk Utility once completed.

### Step 4.3: Running the macOS 26 Installer
1. Select **Install macOS 26** and click **Continue**.
2. Accept the software license terms.
3. Select **Macintosh HD** as the destination drive and click **Continue**.
4. The initial installation phase will copy files to your SSD (approx. 15–20 minutes).
5. The PC will automatically reboot several times:
   * **Reboot 1**: OpenCore automatically selects `macOS Installer`. Allow it to boot without touching any keys.
   * **Reboot 2 & 3**: OpenCore boots from `Macintosh HD`. The screen will display progress bars ("About X minutes remaining").
   * **Do NOT unplug the USB drive during these reboots.**

### Step 4.4: Setup Assistant & Onboarding
1. Select your Region, Language, and Accessibility settings.
2. Wired 2.5 GbE Ethernet is recognized automatically.
3. Sign in with your Apple ID or select *Set Up Later*.
4. Finish the wizard to reach the macOS desktop.

---

## 5. Post-Installation & Internal SSD EFI Deployment

To boot macOS independently without the USB drive:

1. Open **Terminal** in macOS.
2. Identify the EFI partitions of both your USB drive and internal SSD:
   ```bash
   diskutil list
   ```
3. Mount both EFI partitions (replace `X` and `Y` with your actual disk numbers):
   ```bash
   sudo diskutil mount diskXs1    # USB drive EFI
   sudo diskutil mount diskYs1    # Internal SSD EFI (e.g., disk0s1)
   ```
4. Copy the `EFI` directory from the USB drive to the internal SSD EFI:
   ```bash
   cp -R /Volumes/EFI/EFI /Volumes/EFI\ 1/
   ```
5. Confirm that `/Volumes/EFI\ 1/EFI/OC/OpenCore.efi` is present on the internal SSD.
6. Unmount both partitions:
   ```bash
   sudo diskutil unmount /Volumes/EFI
   sudo diskutil unmount /Volumes/EFI\ 1
   ```
7. Eject the USB drive, reboot into BIOS (`F2`), navigate to `Boot -> Boot Option Priorities`, and set `OpenCore` or `UEFI OS` as **Boot Option #1**.

---

## 6. Hardware Verification & Post-Install Setup

### 1. Full Graphics Acceleration (AMD Radeon RX 5700 XT)
* Go to ** -> About This Mac -> More Info -> Graphics / Displays**.
* Confirm **AMD Radeon RX 5700 XT 8 GB** is active with **Metal: Supported (Metal 3)**.
* Check hardware video decoding and acceleration in Terminal:
  ```bash
  system_profiler SPDisplaysDataType | grep "Metal"
  ```
* DRM streaming in Safari (Apple TV+, Netflix 4K) operates natively via WhateverGreen with `agdpmod=pikera unfairgva=1`.

### 2. Audio Installation (`SimpleHDA-HAL-1.0.0.pkg`)
* To enable full Realtek ALC897 High Definition Audio on macOS 26 Tahoe, install the HAL driver included in `Utilities/SimpleHDA-HAL-1.0.0.pkg`.
* Double-click `SimpleHDA-HAL-1.0.0.pkg` inside macOS, follow the on-screen installer prompts, and reboot.
* In **System Settings -> Sound -> Output**, verify that *Internal Speakers* (green rear jack), *Headphones* (front panel), and *DisplayPort/HDMI Audio* are functional.

### 3. Network Configuration & Ethernet Binding (`en0`)
* The Realtek RTL8125 2.5 Gbps Ethernet port is driven natively by `LucyRTL8125Ethernet.kext`.
* In **System Settings -> Network**, confirm Ethernet is active and green.
* If Ethernet was ever assigned to `en1` instead of `en0` (due to previous network testing), reset the network interfaces cache in Terminal:
  ```bash
  sudo rm -f /Library/Preferences/SystemConfiguration/NetworkInterfaces.plist
  sudo rm -f /Library/Preferences/SystemConfiguration/preferences.plist
  ```
  Reboot the system; macOS will recreate the database and bind the built-in Realtek 2.5 GbE adapter as `en0`.

### 4. Wi-Fi & Bluetooth Status under macOS 26 (Tahoe)
* **Bluetooth**: Onboard Intel AX201 Bluetooth 5.2 is fully operational via `IntelBluetoothFirmware.kext`, `IntelBTPatcher.kext`, and `BlueToolFixup.kext`. Devices such as AirPods, Magic Mouse, and wireless keyboards pair and function smoothly.
* **Wi-Fi**: The onboard Intel Wi-Fi 6 AX201 driver (`AirportItlwm.kext` v2.4.0 Tahoe) is currently disabled in `config.plist` due to a known kernel ioctl incompatibility during boot in macOS 26 Tahoe. Driver support for Intel CNVi Wi-Fi under macOS 26 Tahoe is in active community development. **The wired 2.5 GbE Ethernet connection (`en0`) is strongly recommended for high-bandwidth and production stability.**

---

## 7. Credits & Open-Source Acknowledgments

* [Acidanthera](https://github.com/acidanthera) — OpenCorePkg, Lilu, WhateverGreen, VirtualSMC, AppleALC, NVMeFix, BrcmPatchRAM
* [Mieze](https://github.com/Mieze) — LucyRTL8125Ethernet driver
* [OpenIntelWireless](https://github.com/OpenIntelWireless) & [laobamac](https://github.com/laobamac) — Intel Bluetooth & AirportItlwm drivers
* [CorpNewt](https://github.com/corpnewt) — ProperTree, GenSMBIOS
* [Dortania](https://github.com/dortania) — OpenCore Installation documentation
* [SimpleHDA](https://github.com) — macOS Tahoe AppleHDA restoration driver and HAL plugin
