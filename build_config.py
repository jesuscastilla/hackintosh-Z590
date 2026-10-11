import plistlib
import os
import uuid

plist_path = 'D:\\Hackintosh\\EFI\\OC\\config.plist'
with open(plist_path, 'rb') as f:
    plist = plistlib.load(f)

# 1. ACPI
plist['ACPI']['Add'] = [
    {'Arch': 'Any', 'Comment': '', 'Enabled': True, 'Path': 'SSDT-AWAC.aml'},
    {'Arch': 'Any', 'Comment': '', 'Enabled': True, 'Path': 'SSDT-EC-USBX-DESKTOP.aml'},
    {'Arch': 'Any', 'Comment': '', 'Enabled': True, 'Path': 'SSDT-PLUG-DRTNIA.aml'},
    {'Arch': 'Any', 'Comment': '', 'Enabled': True, 'Path': 'SSDT-RHUB.aml'}
]

# 2. Booter Quirks (Comet Lake)
bo = plist['Booter']['Quirks']
bo['AvoidRuntimeDefrag'] = True
bo['DevirtualiseMmio'] = True
bo['EnableSafeModeSlide'] = True
bo['ProvideCustomSlide'] = True
bo['RebuildAppleMemoryMap'] = True
bo['SetupVirtualMap'] = False
bo['SyncRuntimePermissions'] = True

# 3. DeviceProperties
# We spoof CPU as Comet Lake (0x0A0655), but that's in Kernel.
# For GPU, we have a GTX 970 (Maxwell). It requires OCLP later, so we just pass nvda_drv_vrl=1 and disable iGPU.
plist['DeviceProperties']['Add'] = {
    'PciRoot(0x0)/Pci(0x2,0x0)': {
        'disable-gpu': b'\x01\x00\x00\x00' # Disable unsupported UHD 750
    }
}

# 4. Kernel
plist['Kernel']['Add'] = [
    {'Arch': 'x86_64', 'BundlePath': 'Lilu.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/Lilu', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'VirtualSMC.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/VirtualSMC', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'WhateverGreen.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/WhateverGreen', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'AppleALC.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/AppleALC', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'SMCProcessor.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/SMCProcessor', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'SMCSuperIO.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/SMCSuperIO', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'LucyRTL8125Ethernet.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/LucyRTL8125Ethernet', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'AirportItlwm.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/AirportItlwm', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
    {'Arch': 'x86_64', 'BundlePath': 'USBToolBox.kext', 'Comment': '', 'Enabled': True, 'ExecutablePath': 'Contents/MacOS/USBToolBox', 'MaxKernel': '', 'MinKernel': '', 'PlistPath': 'Contents/Info.plist'},
]
# Spoof CPUID as Comet Lake (0x0A0655)
plist['Kernel']['Emulate']['Cpuid1Data'] = b'\x55\x06\x0A\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
plist['Kernel']['Emulate']['Cpuid1Mask'] = b'\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
plist['Kernel']['Emulate']['DummyPowerManagement'] = False

kq = plist['Kernel']['Quirks']
kq['AppleCpuPmCfgLock'] = True # Just in case CFG lock is on
kq['AppleXcpmCfgLock'] = True
kq['CustomSMBIOSGuid'] = True # Needed for UpdateSMBIOSMode=Custom (some Asus boards)
kq['DisableIoMapper'] = True
kq['PanicNoKextDump'] = True
kq['PowerTimeoutKernelPanic'] = True
kq['ProvideCurrentCpuInfo'] = False
kq['XhciPortLimit'] = False # Broken in macOS 11.3+

# 5. Misc
plist['Misc']['Boot']['HideAuxiliary'] = True
plist['Misc']['Boot']['ShowPicker'] = True
plist['Misc']['Boot']['Timeout'] = 5
plist['Misc']['Debug']['AppleDebug'] = True
plist['Misc']['Debug']['ApplePanic'] = True
plist['Misc']['Debug']['Target'] = 67
plist['Misc']['Security']['AllowSetDefault'] = True
plist['Misc']['Security']['BlacklistAppleUpdate'] = True
plist['Misc']['Security']['ScanPolicy'] = 0
plist['Misc']['Security']['SecureBootModel'] = 'Disabled' # Very important for Maxwell/OCLP
plist['Misc']['Security']['Vault'] = 'Optional'

# 6. NVRAM
nv = plist['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
# Adding amfi=0x80 to boot-args is crucial for OCLP on modern macOS
# -v for verbose
# alcid=1 for audio
# ngfxcompat=1 for Nvidia compatibility
nv['boot-args'] = '-v keepsyms=1 debug=0x100 alcid=1 amfi=0x80 ipc_control_port_options=0'
nv['csr-active-config'] = b'\x03\x08\x00\x00' # SIP Disabled (08030000) for OCLP
# UUID needs to be set properly for iServices, but for now we generate random or leave default

# 7. PlatformInfo
plist['PlatformInfo']['Generic']['SystemProductName'] = 'iMac20,1'
plist['PlatformInfo']['Generic']['SystemSerialNumber'] = 'C02CD027PN5T'
plist['PlatformInfo']['Generic']['MLB'] = 'C02012300GU00001M'
plist['PlatformInfo']['Generic']['SystemUUID'] = str(uuid.uuid4()).upper()
plist['PlatformInfo']['Generic']['ROM'] = b'\x11\x22\x33\x44\x55\x66'
plist['PlatformInfo']['UpdateSMBIOSMode'] = 'Custom' # For Asus Z590
plist['PlatformInfo']['Automatic'] = True

# 8. UEFI
plist['UEFI']['Drivers'] = [
    {'Arguments': '', 'Comment': '', 'Enabled': True, 'Path': 'HfsPlus.efi'},
    {'Arguments': '', 'Comment': '', 'Enabled': True, 'Path': 'OpenRuntime.efi'},
    {'Arguments': '', 'Comment': '', 'Enabled': True, 'Path': 'OpenCanopy.efi'},
    {'Arguments': '', 'Comment': '', 'Enabled': True, 'Path': 'ResetNvramEntry.efi'}
]
uq = plist['UEFI']['Quirks']
uq['UnblockFsConnect'] = False

with open(plist_path, 'wb') as f:
    plistlib.dump(plist, f)

print("config.plist updated successfully.")
