# Generating Unique SMBIOS Credentials

The `config.plist` in this repository ships with **placeholder values** for SMBIOS credentials.  
Before booting macOS, you **must** generate and inject your own unique values.

## Step 1: Generate Serial + MLB

```powershell
cd G:\Hackintosh\Utilities
.\macserial.exe -m "iMac20,1" -n 1
```

This outputs a line in the format: `SystemSerialNumber | MLB`  
Example: `C02GN8ZLPN5T | C02145700GUPHC1UE`

## Step 2: Generate UUID

```powershell
[guid]::NewGuid().ToString().ToUpper()
```

## Step 3: Obtain your Ethernet MAC Address

Find your physical **Realtek 2.5 GbE** MAC address (e.g., `FC:34:97:A6:C2:EA`).  
Convert it to base64 data format for the plist.

### Quick MAC-to-Base64 conversion (PowerShell):

```powershell
$mac = "FC:34:97:A6:C2:EA"
$bytes = $mac.Split(":") | ForEach-Object { [Convert]::ToByte($_, 16) }
[Convert]::ToBase64String($bytes)
```

## Step 4: Verify the Serial

Visit [https://checkcoverage.apple.com](https://checkcoverage.apple.com) and enter your generated serial.

- ✅ **Ideal**: "We're unable to check coverage for this serial number" or "Purchase date not validated"
- ❌ **Do NOT use**: If the site recognizes it as a real Mac with active coverage — generate another one.

## Step 5: Edit config.plist

Open `EFI/OC/config.plist` and replace the placeholders under **PlatformInfo → Generic**:

| Key | Value |
|---|---|
| `SystemProductName` | `iMac20,1` (already set) |
| `SystemSerialNumber` | Your generated serial |
| `MLB` | Your generated MLB (17 characters) |
| `SystemUUID` | Your generated UUID |
| `ROM` | Your 6-byte MAC in base64 data format |

## Step 6: Validate

```powershell
.\Utilities\ocvalidate.exe EFI\OC\config.plist
```

Ensure it returns: `Completed validating config.plist. No issues found.`
