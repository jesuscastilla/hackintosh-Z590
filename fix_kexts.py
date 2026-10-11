import plistlib

p = r"S:\EFI\OC\config.plist"
with open(p, "rb") as f:
    d = plistlib.load(f)

# Disable kexts that are likely conflicting
# These are for Broadcom Wi-Fi (old Macs) - we have Intel Wi-Fi (itlwm)
disable_list = [
    "IOSkywalkFamily.kext",
    "IO80211FamilyLegacy.kext",
    "IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext",
    "BlueToolFixup.kext",
]

for k in d["Kernel"]["Add"]:
    bp = k["BundlePath"]
    if bp in disable_list:
        k["Enabled"] = False
        print("Disabled: " + bp)

# Disable the Block rule for IOSkywalkFamily too
for b in d["Kernel"].get("Block", []):
    ident = b.get("Identifier", "")
    if "IOSkywalk" in ident:
        b["Enabled"] = False
        print("Disabled Block: " + ident)

# Print final enabled kexts
print("\nEnabled kexts:")
for k in d["Kernel"]["Add"]:
    if k["Enabled"]:
        print("  " + k["BundlePath"])

print("\nBoot-args: " + d["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"]["boot-args"])

with open(p, "wb") as f:
    plistlib.dump(d, f)
print("\nSaved!")
