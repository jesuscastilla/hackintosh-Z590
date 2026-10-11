import plistlib
import zipfile
import os
import shutil

OCLP_KEXTS_DIR = r"D:\Hackintosh\OCLP-src\OpenCore-Legacy-Patcher-main\payloads\Kexts\Acidanthera"
OCLP_MISC_DIR = r"D:\Hackintosh\OCLP-src\OpenCore-Legacy-Patcher-main\payloads\Kexts\Misc"
EFI_KEXTS = r"S:\EFI\OC\Kexts"
CONFIG = r"S:\EFI\OC\config.plist"
TEMP = r"D:\Hackintosh\oclp_temp"

# Kexts we need to extract and add for OCLP root patches to work
NEEDED_KEXTS = {
    "CryptexFixup": {
        "zip": os.path.join(OCLP_KEXTS_DIR, "CryptexFixup-v1.0.4-RELEASE.zip"),
        "kext_name": "CryptexFixup.kext",
        "executable": "Contents/MacOS/CryptexFixup",
        "min_kernel": "22.0.0",
    },
    "CSLVFixup": {
        "zip": os.path.join(OCLP_KEXTS_DIR, "CSLVFixup-v2.6.1.zip"),
        "kext_name": "CSLVFixup.kext",
        "executable": "Contents/MacOS/CSLVFixup",
        "min_kernel": "",
    },
    "RSRHelper": {
        "zip": os.path.join(OCLP_KEXTS_DIR, "RSRHelper-v1.0.2-RELEASE.zip"),
        "kext_name": "RSRHelper.kext",
        "executable": "Contents/MacOS/RSRHelper",
        "min_kernel": "22.4.0",
    },
    "RestrictEvents": {
        "zip": os.path.join(OCLP_KEXTS_DIR, "RestrictEvents-v1.1.7-RELEASE.zip"),
        "kext_name": "RestrictEvents.kext",
        "executable": "Contents/MacOS/RestrictEvents",
        "min_kernel": "",
    },
    "AutoPkgInstaller": {
        "zip": os.path.join(OCLP_KEXTS_DIR, "AutoPkgInstaller-v1.0.4-RELEASE.zip"),
        "kext_name": "AutoPkgInstaller.kext",
        "executable": "Contents/MacOS/AutoPkgInstaller",
        "min_kernel": "",
    },
}

# Clean temp
if os.path.exists(TEMP):
    shutil.rmtree(TEMP)
os.makedirs(TEMP)

# Load config
with open(CONFIG, "rb") as f:
    config = plistlib.load(f)

existing_kexts = [k["BundlePath"] for k in config["Kernel"]["Add"]]
print(f"Existing kexts: {existing_kexts}")

for name, info in NEEDED_KEXTS.items():
    kext_name = info["kext_name"]
    
    if kext_name in existing_kexts:
        print(f"[SKIP] {kext_name} already in config")
        continue
    
    # Extract
    extract_dir = os.path.join(TEMP, name)
    with zipfile.ZipFile(info["zip"], "r") as z:
        z.extractall(extract_dir)
    
    # Find and copy kext
    kext_src = None
    for root, dirs, files in os.walk(extract_dir):
        if kext_name in dirs:
            kext_src = os.path.join(root, kext_name)
            break
    
    if not kext_src:
        print(f"[ERROR] Could not find {kext_name} in zip")
        continue
    
    kext_dst = os.path.join(EFI_KEXTS, kext_name)
    if os.path.exists(kext_dst):
        shutil.rmtree(kext_dst)
    shutil.copytree(kext_src, kext_dst)
    print(f"[OK] Copied {kext_name} to EFI")
    
    # Add to config
    entry = {
        "Arch": "Any",
        "BundlePath": kext_name,
        "Comment": f"OCLP - {name}",
        "Enabled": True,
        "ExecutablePath": info["executable"],
        "MaxKernel": "",
        "MinKernel": info["min_kernel"],
        "PlistPath": "Contents/Info.plist",
    }
    config["Kernel"]["Add"].append(entry)
    print(f"[OK] Added {kext_name} to config.plist")

# Also add revpatch=sbvmm to boot-args (needed for OCLP on Sonoma)
boot_args = config["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"]["boot-args"]
if "revpatch=sbvmm" not in boot_args:
    boot_args += " revpatch=sbvmm"
    config["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"]["boot-args"] = boot_args
    print("[OK] Added revpatch=sbvmm to boot-args")

# Make sure -x is NOT in boot-args (no safe mode)
boot_args = config["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"]["boot-args"]
boot_args = boot_args.replace(" -x", "").replace("-x ", "")
config["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"]["boot-args"] = boot_args

print(f"\nFinal boot-args: {boot_args}")
print(f"Final kexts: {[k['BundlePath'] for k in config['Kernel']['Add'] if k['Enabled']]}")

with open(CONFIG, "wb") as f:
    plistlib.dump(config, f)

print("\n[DONE] Config saved successfully!")
