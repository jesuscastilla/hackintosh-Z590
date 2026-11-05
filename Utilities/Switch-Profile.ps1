<#
.SYNOPSIS
    Cambia el perfil OpenCore activo (Intel / WCN) en el ESP interno.

.DESCRIPTION
    - Resuelve el ESP dinámicamente (partición EFI del Samsung 990 PRO),
      porque los discos se renumeran entre arranques (no usar disk0s1 fijo).
    - Valida el config de origen con ocvalidate antes de copiarlo.
    - Crea un backup completo del ESP en EFI_Backup_YYYYMMDD-HHMM (gitignored).
    - Copia config.local.plist (Intel) o config.wcn.local.plist (WCN) del repo
      al ESP como config.plist y deja el espejo config.<perfil>.plist.
    - Valida el config.plist resultante en el ESP.

.EXAMPLE
    .\Utilities\Switch-Profile.ps1 -Profile Intel
    .\Utilities\Switch-Profile.ps1 -Profile WCN

.NOTES
    No reinicia el equipo; reiniciar manualmente tras ejecutar.
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Intel', 'WCN')]
    [string]$Profile,

    [switch]$SkipBackup
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ocvalidate = Join-Path $root 'Utilities\ocvalidate.exe'

function Resolve-EspRoot {
    $disk = Get-Disk -ErrorAction SilentlyContinue |
        Where-Object { $_.FriendlyName -match '990 PRO' -and $_.BusType -eq 'NVMe' } |
        Select-Object -First 1
    if (-not $disk) {
        throw 'No se encontró el disco Samsung 990 PRO (interno).'
    }
    $esp = Get-Partition -DiskNumber $disk.Number -ErrorAction SilentlyContinue |
        Where-Object { $_.GptType -eq '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}' } |
        Select-Object -First 1
    if (-not $esp) {
        throw 'No se encontró la partición EFI (ESP) en el disco interno.'
    }
    $accessPath = $esp.AccessPaths | Where-Object { $_ -like '\\?\Volume*' } | Select-Object -First 1
    if (-not $accessPath) {
        throw 'El ESP no tiene una ruta de volumen accesible.'
    }
    return $accessPath.TrimEnd('\') + '\'
}

$sourceMap = @{
    Intel = Join-Path $root 'EFI\OC\config.local.plist'
    WCN   = Join-Path $root 'EFI\OC\config.wcn.local.plist'
}
$mirrorMap = @{
    Intel = 'config.intel.plist'
    WCN   = 'config.wcn.plist'
}

$source = $sourceMap[$Profile]
if (-not (Test-Path -LiteralPath $source)) {
    throw "No existe el config de origen: $source"
}
if (-not (Test-Path -LiteralPath $ocvalidate)) {
    throw "No se encontró ocvalidate.exe: $ocvalidate"
}

$espRoot = Resolve-EspRoot
$espOc = Join-Path $espRoot 'EFI\OC'
Write-Host "ESP: $espRoot" -ForegroundColor Cyan
Write-Host "Perfil: $Profile" -ForegroundColor Cyan
Write-Host "Origen: $source" -ForegroundColor Cyan

Write-Host 'Validando el config de origen...' -ForegroundColor White
$result = & $ocvalidate $source 2>&1
if ($result -notmatch 'No issues found') {
    $result | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw "ocvalidate falló en $source"
}
Write-Host '  [OK] config de origen válido' -ForegroundColor Green

if (-not $SkipBackup) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmm'
    $backupDir = Join-Path $root "EFI_Backup_$stamp"
    Write-Host "Creando backup del ESP en $backupDir ..." -ForegroundColor White
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    Get-ChildItem -LiteralPath $espRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $backupDir -Recurse -Force -ErrorAction Continue
    }
    Write-Host '  [OK] backup creado' -ForegroundColor Green
}

$target = Join-Path $espOc 'config.plist'
$mirror = Join-Path $espOc $mirrorMap[$Profile]

if ($PSCmdlet.ShouldProcess($target, "Activar perfil $Profile")) {
    Copy-Item -LiteralPath $source -Destination $target -Force
    Copy-Item -LiteralPath $source -Destination $mirror -Force
    Write-Host '  [OK] config copiado al ESP' -ForegroundColor Green
}

Write-Host 'Validando el config.plist resultante en el ESP...' -ForegroundColor White
$espResult = & $ocvalidate $target 2>&1
if ($espResult -notmatch 'No issues found') {
    $espResult | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    throw 'ocvalidate falló en el config del ESP'
}
Write-Host '  [OK] config.plist del ESP válido' -ForegroundColor Green

Write-Host ''
Write-Host "Perfil $Profile activo en $target" -ForegroundColor Green
Write-Host 'Reinicia el equipo para aplicar el cambio.' -ForegroundColor Yellow
