import plistlib

p = r"S:\EFI\OC\config.plist"
with open(p, "rb") as f:
    d = plistlib.load(f)

# Only these kexts stay enabled
keep_enabled = {
    "Lilu.kext",              # Foundation
    "VirtualSMC.kext",        # SMC emulation (required for boot)
    "WhateverGreen.kext",     # GPU framework (Nvidia)
    "USBToolBox.kext",        # USB (keyboard/mouse)
    "UTBDefault.kext",        # USB port map
    "LucyRTL8125Ethernet.kext",  # Ethernet
    "AMFIPass.kext",          # OCLP - allows patched drivers
    "CryptexFixup.kext",      # OCLP - Sonoma cryptex
    "CSLVFixup.kext",         # OCLP - library validation fix
    "RSRHelper.kext",         # OCLP - security response compat
    "RestrictEvents.kext",    # OCLP - hardware restrictions bypass
}

for k in d["Kernel"]["Add"]:
    bp = k["BundlePath"]
    if bp in keep_enabled:
        k["Enabled"] = True
    else:
        k["Enabled"] = False

# Disable ALL Block rules
for b in d["Kernel"].get("Block", []):
    b["Enabled"] = False

# Print result
print("ENABLED:")
for k in d["Kernel"]["Add"]:
    if k["Enabled"]:
        print("  + " + k["BundlePath"])

print("\nDISABLED:")
for k in d["Kernel"]["Add"]:
    if not k["Enabled"]:
        print("  - " + k["BundlePath"])

with open(p, "wb") as f:
    plistlib.dump(d, f)
print("\nSaved!")
