## 2026-10-19 - Removed Hardcoded Wi-Fi Credentials from Kext Plists
**Vulnerability:** Found hardcoded Wi-Fi SSIDs and plaintext passwords inside `Info.plist` files of `itlwm.kext`.
**Learning:** Developers often forget to scrub sensitive credentials like Wi-Fi passwords from configuration files (like kext plists) before committing them to open-source or shared repositories. This specific repository is for an OpenCore EFI setup where the Wi-Fi credentials are meant for personal use.
**Prevention:** Scrub or parameterize all configuration files meant to be shared publicly. Credentials should either be added by the end user locally or provided through environment variables/secure storage mechanisms if applicable.
