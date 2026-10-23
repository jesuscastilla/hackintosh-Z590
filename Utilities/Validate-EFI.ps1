<#
.SYNOPSIS
    Validates the OpenCore EFI configuration for the Hackintosh workspace.

.DESCRIPTION
    Runs a series of automated checks:
    1. ocvalidate against config.plist (and config.local.plist if present)
    2. SMBIOS credential safety check
    3. Kext existence verification (every enabled kext declared in config exists on disk)
    4. SSDT/ACPI existence verification
    5. UEFI Driver existence verification
    6. OpenCanopy resources verification (if PickerMode is External)

.EXAMPLE
    cd G:\Hackintosh
    .\Utilities\Validate-EFI.ps1
#>

param(
    [string]$ConfigFile = "EFI\OC\config.plist"
)

$ErrorActionPreference = 'Continue'
$root = Split-Path $PSScriptRoot -Parent
$targetConfig = Join-Path $root $ConfigFile
$ocvalidate = Join-Path $root "Utilities\ocvalidate.exe"

$pass = 0
$fail = 0
$warn = 0

function Write-Check {
    param([string]$Status, [string]$Message)
    switch ($Status) {
        "PASS" { Write-Host "  [OK]   $Message" -ForegroundColor Green; $script:pass++ }
        "FAIL" { Write-Host "  [FAIL] $Message" -ForegroundColor Red; $script:fail++ }
        "WARN" { Write-Host "  [WARN] $Message" -ForegroundColor Yellow; $script:warn++ }
    }
}

Write-Host ""
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host "  Hackintosh EFI Validation - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "  Target: $ConfigFile" -ForegroundColor Cyan
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host ""

# --- 1. ocvalidate -----------------------------------------------------------
Write-Host "1. OpenCore Config Validation (ocvalidate)" -ForegroundColor White
if (Test-Path $ocvalidate) {
    $result = & $ocvalidate $targetConfig 2>&1
    if ($result -match "No issues found") {
        Write-Check "PASS" "ocvalidate: No issues found in $ConfigFile"
    } else {
        Write-Check "FAIL" "ocvalidate reported issues:"
        $result | ForEach-Object { Write-Host "         $_" -ForegroundColor Red }
    }
} else {
    Write-Check "WARN" "ocvalidate.exe not found at $ocvalidate"
}

# --- 2. Also test config.local.plist if validating public config.plist -------
$localConfig = Join-Path $root "EFI\OC\config.local.plist"
if ($ConfigFile -eq "EFI\OC\config.plist" -and (Test-Path $localConfig)) {
    Write-Host ""
    Write-Host "1b. OpenCore Local Config Validation (config.local.plist)" -ForegroundColor White
    $localResult = & $ocvalidate $localConfig 2>&1
    if ($localResult -match "No issues found") {
        Write-Check "PASS" "ocvalidate: No issues found in config.local.plist"
    } else {
        Write-Check "FAIL" "ocvalidate reported issues in config.local.plist:"
        $localResult | ForEach-Object { Write-Host "         $_" -ForegroundColor Red }
    }
}

# --- 3. SMBIOS Credential Safety --------------------------------------------
Write-Host ""
Write-Host "2. SMBIOS Credential Safety" -ForegroundColor White
$content = Get-Content $targetConfig -Raw

$placeholders = @("REPLACE_WITH_YOUR_MLB", "REPLACE_WITH_YOUR_SERIAL")
$hasPlaceholders = $false
foreach ($ph in $placeholders) {
    if ($content -match [regex]::Escape($ph)) {
        $hasPlaceholders = $true
    }
}

if ($ConfigFile -match "local") {
    if ($hasPlaceholders) {
        Write-Check "WARN" "Local config still contains placeholder values"
    } else {
        Write-Check "PASS" "Local config contains personalized credentials"
    }
} else {
    if ($hasPlaceholders) {
        Write-Check "PASS" "Public config contains placeholders (safe for public repository)"
    } else {
        Write-Check "FAIL" "Real SMBIOS credentials might be present in public config.plist!"
    }
}

# --- 4. Kext Existence ------------------------------------------------------
Write-Host ""
Write-Host "3. Kext File Verification" -ForegroundColor White
$kextsDir = Join-Path $root "EFI\OC\Kexts"

[xml]$plist = Get-Content $targetConfig
$kernelAdd = $plist.plist.dict.SelectNodes("key[.='Kernel']/following-sibling::dict[1]/key[.='Add']/following-sibling::array[1]/dict")

foreach ($entry in $kernelAdd) {
    $enabledNode = $entry.SelectSingleNode("key[.='Enabled']/following-sibling::*[1]")
    $isEnabled = $enabledNode.Name -eq "true"

    $bundleNode = $entry.SelectSingleNode("key[.='BundlePath']/following-sibling::string[1]")
    if ($bundleNode) {
        $bundlePath = $bundleNode.InnerText
        $fullPath = Join-Path $kextsDir $bundlePath
        $exists = Test-Path $fullPath

        if ($isEnabled -and $exists) {
            Write-Check "PASS" "$bundlePath"
        } elseif ($isEnabled -and -not $exists) {
            Write-Check "FAIL" "$bundlePath - MISSING from EFI/OC/Kexts/"
        }
    }
}

# --- 5. ACPI/SSDT Existence -------------------------------------------------
Write-Host ""
Write-Host "4. ACPI/SSDT File Verification" -ForegroundColor White
$acpiDir = Join-Path $root "EFI\OC\ACPI"

$acpiAdd = $plist.plist.dict.SelectNodes("key[.='ACPI']/following-sibling::dict[1]/key[.='Add']/following-sibling::array[1]/dict")

foreach ($entry in $acpiAdd) {
    $enabledNode = $entry.SelectSingleNode("key[.='Enabled']/following-sibling::*[1]")
    $isEnabled = $enabledNode.Name -eq "true"

    $pathNode = $entry.SelectSingleNode("key[.='Path']/following-sibling::string[1]")
    if ($pathNode) {
        $filePath = $pathNode.InnerText
        $fullPath = Join-Path $acpiDir $filePath
        $exists = Test-Path $fullPath

        if ($isEnabled -and $exists) {
            Write-Check "PASS" "$filePath"
        } elseif ($isEnabled -and -not $exists) {
            Write-Check "FAIL" "$filePath - MISSING from EFI/OC/ACPI/"
        }
    }
}

# --- 6. Driver Existence ----------------------------------------------------
Write-Host ""
Write-Host "5. UEFI Driver Verification" -ForegroundColor White
$driversDir = Join-Path $root "EFI\OC\Drivers"

$driverEntries = $plist.plist.dict.SelectNodes("key[.='UEFI']/following-sibling::dict[1]/key[.='Drivers']/following-sibling::array[1]/dict")

foreach ($entry in $driverEntries) {
    $enabledNode = $entry.SelectSingleNode("key[.='Enabled']/following-sibling::*[1]")
    $isEnabled = $enabledNode.Name -eq "true"

    $pathNode = $entry.SelectSingleNode("key[.='Path']/following-sibling::string[1]")
    if ($pathNode) {
        $filePath = $pathNode.InnerText
        $fullPath = Join-Path $driversDir $filePath
        $exists = Test-Path $fullPath

        if ($isEnabled -and $exists) {
            Write-Check "PASS" "$filePath"
        } elseif ($isEnabled -and -not $exists) {
            Write-Check "FAIL" "$filePath - MISSING from EFI/OC/Drivers/"
        }
    }
}

# --- 7. OpenCanopy Graphical Resources Check ---------------------------------
Write-Host ""
Write-Host "6. OpenCanopy Graphical Resources Verification" -ForegroundColor White
$resourcesDir = Join-Path $root "EFI\OC\Resources"
$hasFont = Test-Path (Join-Path $resourcesDir "Font\Font_1x.png")
$hasImage = Test-Path (Join-Path $resourcesDir "Image\Acidanthera")

if ($hasFont -and $hasImage) {
    Write-Check "PASS" "OpenCanopy Fonts and Themes (Acidanthera) are installed"
} else {
    Write-Check "WARN" "OpenCanopy resources may be incomplete in EFI/OC/Resources/"
}

# --- Summary ----------------------------------------------------------------
Write-Host ""
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host "  Results: $pass passed | $fail failed | $warn warnings" -ForegroundColor Cyan
Write-Host "===========================================================" -ForegroundColor Cyan
Write-Host ""

if ($fail -gt 0) {
    Write-Host "  [!] EFI has issues that must be fixed before booting!" -ForegroundColor Red
    exit 1
} else {
    Write-Host "  [+] EFI is verified and ready!" -ForegroundColor Green
    exit 0
}
