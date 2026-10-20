## 2026-10-20 - [Fix hardcoded Wi-Fi passwords in itlwm config]
**Vulnerability:** Hardcoded Wi-Fi passwords and SSIDs found in `itlwm.kext/Contents/Info.plist` and `EFI/OC/Kexts/itlwm.kext/Contents/Info.plist`.
**Learning:** Configurations in OpenCore kexts can sometimes store sensitive network credentials plainly.
**Prevention:** Avoid embedding hardcoded sensitive information directly into source files or configuration templates. Use placeholders or environment-specific injection securely if necessary.