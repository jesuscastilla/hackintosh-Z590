# Set-Refind.ps1 — Ensures rEFInd is the primary bootloader intercepting Windows Boot Manager
$ErrorActionPreference = 'Stop'

Write-Host "Configuring rEFInd as the default UEFI bootloader..." -ForegroundColor Cyan

# 1. Ensure {bootmgr} executes rEFInd
bcdedit /set "{bootmgr}" path "\EFI\refind\refind_x64.efi"
bcdedit /set "{bootmgr}" description "rEFInd Boot Manager"

# 2. Set {bootmgr} as the first priority in UEFI firmware boot order
bcdedit /set "{fwbootmgr}" displayorder "{bootmgr}" /addfirst

Write-Host "Successfully configured rEFInd as default and primary bootloader!" -ForegroundColor Green
