import plistlib

plist_path = 'D:\\Hackintosh\\EFI\\OC\\config.plist'
with open(plist_path, 'rb') as f:
    plist = plistlib.load(f)

# Fix ACPI -> Add
for item in plist['ACPI']['Add']:
    if 'Arch' in item:
        del item['Arch']

# Fix UEFI -> Drivers
for item in plist['UEFI']['Drivers']:
    item['HideVerbose'] = False
    item['LoadEarly'] = False

# Fix Misc -> Boot -> PickerMode for OpenCanopy
plist['Misc']['Boot']['PickerMode'] = 'External'
plist['Misc']['Boot']['PickerAttributes'] = 17 # Optional but recommended
# Disable HideAuxiliary to make sure they see Recovery without pressing Space
plist['Misc']['Boot']['HideAuxiliary'] = False

with open(plist_path, 'wb') as f:
    plistlib.dump(plist, f)

print("Fixes applied to config.plist")
