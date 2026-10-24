# Diagnóstico — Estabilidad + Wi-Fi 5 GHz @ 160 MHz (macOS 26 Tahoe)

> Fecha: 24-10-2026
> Equipo: ASUS PRIME Z590-P WIFI · i5-11600K · RX 5700 XT · 32 GB · OpenCore 1.0.8
> Objetivo: (1) mejorar la **estabilidad** y (2) conseguir que el Wi-Fi (Intel AX201)
> se conecte a la red de **5 GHz a 160 MHz**.
> Estado: **en diagnóstico**. No se ha tocado la EFI ni ningún fichero del repo.

---

## 1. Resumen de lo detectado (evidencia)

| Elemento | Estado |
|---|---|
| Wi-Fi | Intel **AX201** CNVi (`8086:A0F0`) + `AirportItlwm.kext` **v2.4.0** (fork Tahoe) |
| Bluetooth | IntelBluetoothFirmware 2.5.0 + IntelBTPatcher 2.5.0 + BlueToolFixup 2.6.9 |
| SMBIOS | `iMac20,1` · `SecureBootModel=Disabled` · CPU spoof `0x0A0655` (Comet Lake) |
| boot-args | `keepsyms=1 debug=0x100 alcid=11 amfi=0x80 agdpmod=pikera unfairgva=1 shda-lang=es itlwm_cc=ES` |
| Quirks relevantes | `DisableIoMapper=true` · `EnableWriteUnprotector=true` · `RebuildAppleMemoryMap=true` · `SyncRuntimePermissions=true` · `SetupVirtualMap=false` · `ResizeAppleGpuBars=0` · `PowerTimeoutKernelPanic=true` · `SetApfsTrimTimeout=0` |
| Validación | `ocvalidate.exe` → **sin errores** en `config.plist` y `config.local.plist` · los 4 SSDT con checksum OK |
| Disco macOS | Disk 2 (Samsung 970 PRO), EFI 200 MB (partición 1) |

> Nota: `config.plist` del repo está saneado (`REPLACE_WITH_YOUR_…`); los seriales reales
> están en `config.local.plist`. Cualquier cambio hay que hacerlo en **ambos** y validarlo.

---

## 2. Hallazgos clave sobre el 160 MHz (técnico)

1. **El driver SÍ soporta 160 MHz, pero no 802.11ax por defecto.**
   - FAQ oficial: `VHT80 y VHT160 desde v2.0.0`; **HE (802.11ax) deshabilitado**.
   - En el binario del kext están compiladas las rutas: `chan_width=%s support_160=%d
     support_80_80=%d`, `HE negotiated: width=%s`, `HE TLC: ... rates160=%04x/%04x`.

2. **Wi-Fi 6 (HE) se activa con el boot-arg `itlwm_he=1`** (fork `laobamac/itlwm`).
   - Hoy **no** está en `boot-args` → el Mac negocia como máximo **802.11ac VHT160**
     (2x2 ≈ 1733 Mbps PHY, ~900–950 Mbps reales).
   - Con `itlwm_he=1` podría negociar **HE 160** (AX201 2x2 ≈ 2400 Mbps PHY).

3. **Seguridad Wi-Fi soportada por el kext:** WPA2-Personal y WPA2/WPA3 transition con
   PMF opcional. **WPA3-only y PMF obligatorio NO soportados.** AWDL/AirDrop y MLO tampoco.

4. **En España, 160 MHz exige DFS:** los bloques válidos son **36–64** o **100–128**
   (los dos incluyen canales DFS). Los canales **149–165 no son válidos** en el dominio ES.
   Muchos routers vienen en 149+ o en "80 MHz auto" → el Mac nunca negociará 160.

5. **Band steering** (mismo SSID para 2.4 y 5 GHz) es la causa nº1 de "se queda en 2.4 GHz".

6. **No hay Broadcom que dé 160 MHz:** las tarjetas nativas de macOS (BCM94360/BCM943602,
   Fenvi) se quedan en 80 MHz. La única vía al 160 es Intel + itlwm/AirportItlwm.

7. **160 MHz y estabilidad están reñidos:** 160 con DFS puede degradar a 80 por radar y es
   más sensible a RSSI/interferencias. Perfil estable recomendado: 80 MHz en 36–48.

---

## 3. Plan por fases

### Fase 0 — Red de seguridad
1. Copia de la EFI real (Disk 2 / part 1) a carpeta fechada o USB.
2. Copia fechada de `config.local.plist` y `config.plist`.
3. USB de arranque OpenCore funcional.

### Fase 1 — Diagnóstico (macOS) — pendiente de ejecutar
Ver apartado 5 (script copiable).

### Fase 2 — Wi-Fi 5 GHz / 160 MHz
1. **Router primero:** SSID 5G separado · 20/40/80/160 · canal **36–64** (o 100–128) con
   DFS · **WPA2-Personal AES** (sin WPA3-only, sin PMF obligatorio) · band steering y
   "airtime fairness" desactivados.
2. **Antenas/ubicación:** a 160 MHz se necesita RSSI ≳ −60/−65 dBm; antenas SMA firmes.
3. **macOS:** borrar el perfil guardado de la 5G y reconectar · 5G primera en redes
   preferidas · si hay microcortes, desactivar IPv6 en Wi-Fi (fix conocido #884).
4. **Verificación:** `sudo wdutil info` → `Channel: 44 (5GHz, 160MHz)`.
5. **Driver:** probar `itlwm_he=1` (HE 160) y, si resulta inestable, volver a VHT160.

### Fase 3 — Estabilidad (aislar causa, 1 variable por vez)
1. Wi-Fi apagado 48 h (Ethernet) → ¿desaparecen los cuelgues?
2. Bluetooth apagado → descartar BlueToolFixup/IntelBTPatcher.
3. `pmset sleepnow` ×5 → sleep/wake.
4. Carga sostenida (descarga grande, `yes>/dev/null` ×12, Cinebench).
5. **ReBAR OFF** en BIOS (`ResizeAppleGpuBars=-1`) vs ON.
6. **XMP OFF** (2133) o `memtest86` (ya está en `Misc → Tools`, deshabilitado).

### Fase 4 — Validación y documentación
`ocvalidate` tras cada cambio → arranque con USB de rescate → soak 72 h.

---

## 4. Candidatos concretos de `config.plist` (solo cuando se apruebe)

| Clave | Actual | Propuesto | Motivo | Riesgo |
|---|---|---|---|---|
| `Kernel→Quirks→PanicNoKextDump` | `true` | `false` | ver kext culpable en panic | nulo |
| `Booter→Quirks→EnableWriteUnprotector` | `true` | `false` | redundante con `RebuildAppleMemoryMap` | bajo |
| `Kernel→Quirks→SetApfsTrimTimeout` | `0` | `-1` | TRIM APFS errático/lento | bajo |
| `Kernel→Quirks→PowerTimeoutKernelPanic` | `true` | `false` (test) | evita panic por timeout de energía | bajo |
| `NVRAM→boot-args` | `debug=0x100` | quitar tras diagnosticar | `0x100` = no dormir en panic | nulo |
| `boot-args` | `itlwm_cc=ES` | `00`/`US` (test) | dominio regulatorio 5 GHz | bajo |
| `boot-args` | (sin HE) | `itlwm_he=1` (test) | activar 802.11ax/HE 160 | bajo/medio |
| `Kernel→Add→AirportItlwm` | `Min/MaxKernel` vacíos | `25.0.0` / `25.99.99` | fijar kext a Darwin 25 (recomendado por el fork) | nulo |

---

## 5. Script de diagnóstico (ejecutar en Terminal de macOS)

Conéctate **primero al SSID que usas normalmente** (el 5 GHz) y pega el bloque completo.
Todo es de solo lectura; genera un informe en el Escritorio.

```bash
sudo -v
OUT="$HOME/Desktop/hack-diag-$(date +%Y%m%d-%H%M).txt"
{
echo "########## 1. SISTEMA ##########"
sw_vers
uname -a
sysctl -n hw.model machdep.cpu.brand_string hw.ncpu hw.memsize
uptime

echo; echo "########## 2. OPENCORE / SIP / BOOT-ARGS ##########"
nvram 4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:opencore-version
nvram -p | grep -E "boot-args|csr-active-config"
csrutil status

echo; echo "########## 3. KEXTS DE TERCEROS CARGADOS ##########"
sudo kmutil showloaded --list-only | grep -vi "com.apple"

echo; echo "########## 4. WIFI ##########"
networksetup -listallhardwareports
WIFI=$(networksetup -listallhardwareports | awk '/Wi-Fi/{getline; print $2}')
echo "Interfaz Wi-Fi detectada: ${WIFI:-NO DETECTADA}"
echo "--- wdutil info (canal / ancho / PHY / RSSI / Tx rate) ---"
sudo wdutil info
echo "--- airport -s (redes visibles; puede no existir en Tahoe) ---"
AP=/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport
if [ -x "$AP" ]; then sudo "$AP" -s; else echo "airport no disponible en este macOS"; fi
echo "--- SPAirPortDataType (primeras 90 lineas) ---"
system_profiler SPAirPortDataType 2>/dev/null | sed -n '1,90p'
if [ -n "$WIFI" ]; then echo "--- ifconfig $WIFI ---"; ifconfig "$WIFI" | head -25; fi

echo; echo "########## 5. PANICS / CRASHES ##########"
ls -lt /Library/Logs/DiagnosticReports 2>/dev/null | head -15
for f in $(ls -t /Library/Logs/DiagnosticReports/*.panic 2>/dev/null | head -2); do
  echo "=== $f ==="; sed -n '1,80p' "$f"
done

echo; echo "########## 6. LOG WIFI (ultimos 30 min) ##########"
log show --last 30m --style compact --predicate 'eventMessage CONTAINS[c] "itlwm" OR eventMessage CONTAINS[c] "iwx"' 2>/dev/null | tail -150

echo; echo "########## 7. ENERGIA / SLEEP ##########"
pmset -g custom
echo "--- eventos wake/sleep ---"
sudo pmset -g log | grep -Ei "wake|sleep|failure" | tail -40

echo; echo "########## 8. GPU / AUDIO / NVMe ##########"
system_profiler SPDisplaysDataType 2>/dev/null | sed -n '1,35p'
system_profiler SPAudioDataType 2>/dev/null | sed -n '1,45p'
system_profiler SPNVMeDataType 2>/dev/null | sed -n '1,35p'

echo; echo "########## 9. HAL / PARCHEOS / EXTRAS ##########"
ls -1 /Library/Audio/Plug-Ins/HAL 2>/dev/null
ls -1 /Library/Extensions 2>/dev/null
ls -1 /Applications | grep -i patch
} > "$OUT" 2>&1

echo "==> Informe: $OUT"
open -R "$OUT"
```

### Qué mirar en cada apartado

| Apartado | Qué aporta |
|---|---|
| **4 (wdutil)** | `Channel`, ancho (80/160), `PHY Mode` (ac/ax), `RSSI`, `Tx Rate` |
| **4 (SPAirPortDataType)** | red actual + redes visibles (¿aparece tu 5G?) |
| **5 (panics)** | `Kernel Extensions in backtrace` → culpable (Wi-Fi, audio, GPU…) |
| **6 (log)** | `HE enabled/negotiated`, `chan_width=`, `support_160=`, `disassociated` |
| **7 (pmset)** | errores en wake/sleep |
| **3 (kexts)** | que carguen AirportItlwm, SimpleHDA, NVMeFix, UTBMap |

### Datos del router necesarios
- Marca / modelo / firmware.
- Banda 5 GHz: canal, ancho (20/40/80/160), seguridad (WPA2 / WPA2-WPA3 mixto / WPA3-only), PMF.
- ¿Band steering / mismo SSID para 2.4 y 5 GHz?
- Distancia del Mac al router.

---

## 6. Reglas aplicadas (repo)

- No se toca la **EFI del sistema** sin permiso explícito.
- Cualquier cambio en ficheros del repo requiere aprobación previa.
- `README.md` permanece como guía limpia (sin changelog).
- Releases / commits solo cuando se pidan explícitamente.


