$ErrorActionPreference = 'Stop'

# Get firmware entries
$output = bcdedit /enum firmware
$refindGuid = $null
$currentGuid = $null

foreach ($line in $output) {
    if ($line -match "^identifier\s+({\S+})") {
        $currentGuid = $matches[1]
    }
    if ($line -match "(?i)^description\s+.*refind.*" -or $line -match "(?i)^path\s+.*refind.*") {
        if ($currentGuid) {
            $refindGuid = $currentGuid
        }
    }
}

if ($refindGuid) {
    Write-Host "Found rEFInd with GUID: $refindGuid"
    bcdedit /set "{fwbootmgr}" displayorder $refindGuid /addfirst
    Write-Host "Successfully set rEFInd as the first boot option!"
} else {
    Write-Host "rEFInd boot entry not found in NVRAM. Attempting to create one..."
    # We mount the EFI partition
    mountvol S: /S
    if (Test-Path "S:\EFI\refind\refind_x64.efi") {
        $newGuidRaw = bcdedit /copy "{bootmgr}" /d "rEFInd Boot Manager"
        if ($newGuidRaw -match "({.*})") {
            $newGuid = $matches[1]
            bcdedit /set $newGuid path "\EFI\refind\refind_x64.efi"
            bcdedit /set "{fwbootmgr}" displayorder $newGuid /addfirst
            Write-Host "Successfully created and set rEFInd as the first boot option!"
        }
    } else {
        Write-Host "rEFInd not found in S:\EFI\refind\refind_x64.efi"
    }
}
Write-Host "Press any key to continue..."
$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
