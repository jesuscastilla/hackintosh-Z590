# Hackintosh — macOS Sonoma en un ASUS Z590

Mi Hackintosh personal: macOS Sonoma 14.x corriendo sobre un ASUS PRIME Z590-P WIFI con un i5-11600K y una GTX 970, en doble arranque con Windows 11. OpenCore + rEFInd y aceleración gráfica vía OpenCore Legacy Patcher.

## Descargo

Este repositorio es educativo y experimental. Instalar macOS en hardware que no es de Apple incumple el EULA de Apple (sección 2.J) y, según el país, puede rozar lo ilegal. No animo a saltarse licencias ni me hago responsable del uso que le des. Tú decides.

## Hardware

| Componente | Modelo | Notas |
|---|---|---|
| CPU | Intel Core i5-11600K (Rocket Lake) | CPUID espofeado a Comet Lake |
| Placa | ASUS PRIME Z590-P WIFI (LGA1200) | `UpdateSMBIOSMode=Custom` |
| GPU | NVIDIA GTX 970 4 GB (Maxwell) | Aceleración con OCLP (Web Drivers) |
| iGPU | Intel UHD 750 | Desactivada, sin soporte nativo |
| RAM | 32 GB DDR4 | — |
| Disco | Samsung 990 PRO NVMe 1 TB | Windows 11 + macOS (APFS) |
| Ethernet | Realtek RTL8125 2.5 GbE | `LucyRTL8125Ethernet.kext` |
| Wi-Fi | Intel AX201 (CNVi) | `itlwm.kext` + HeliPort |
| Bluetooth | Intel AX201 | `IntelBluetoothFirmware` + `IntelBTPatcher` + `BlueToolFixup` |
| Audio | Realtek ALC897 | `AppleALC.kext` (`alcid=11`) |
| Arranque | OpenCore 1.0.x + rEFInd | Dual boot inmune a resets de NVRAM |
| SMBIOS | `iMac19,1` | iMessage, FaceTime e iCloud OK |

## Qué funciona

- Aceleración gráfica completa (Metal/OpenGL vía OCLP).
- Ethernet 2.5G, Wi-Fi y Bluetooth Intel.
- Servicios de Apple (iCloud, App Store, iMessage, FaceTime).
- Mapeo USB (2.0, 3.0, Tipo-C y cabezales internos).
- Dual boot con menú gráfico rEFInd.

Lo único que se resiste es el **reposo profundo (S3)**: las Maxwell no despiertan bien en Sonoma, así que uso reposo de pantalla y listo.

## Configuración de OpenCore

### Spoof de CPU

Apple nunca sacó equipos con Rocket Lake (11ª gen), así que en `Kernel → Emulate` simulo un Comet Lake:

```
Cpuid1Data: 55060A00 00000000 00000000 00000000   (0x0A0655)
Cpuid1Mask: FFFFFFFF 00000000 00000000 00000000
```

### Tablas ACPI

- `SSDT-AWAC.aml` — habilita el RTC clásico.
- `SSDT-EC-USBX-DESKTOP.aml` — EC dummy y energía USB.
- `SSDT-PLUG-DRTNIA.aml` — gestión de energía del procesador.
- `SSDT-RHUB.aml` — inicializa el root hub USB.

### Kexts

`Lilu` y `VirtualSMC` de base; `WhateverGreen`, `AppleALC`, `SMCProcessor`/`SMCSuperIO`, `LucyRTL8125Ethernet`, `USBToolBox`/`UTBMap`, `itlwm`, la pila Bluetooth de Intel y `NVMeFix`.

### NVRAM y SIP

`boot-args`:

```
-v keepsyms=1 debug=0x100 alcid=11 amfi=0x80 ngfxgl=1 ngfxcompat=1 nvda_drv_vrl=1 ipc_control_port_options=0
```

`amfi=0x80` e `ipc_control_port_options=0` dejan inyectar los drivers heredados de Nvidia; `nvda_drv_vrl=1`, `ngfxcompat=1` y `ngfxgl=1` habilitan los Web Drivers. El SIP (`csr-active-config`) va en `030A0000` para que OCLP parchee el volumen sellado sin desprotegerlo del todo.

## Doble arranque con rEFInd

Para que la BIOS de ASUS no pise las entradas UEFI al resetear la NVRAM:

1. `refind_x64.efi` se coloca en `\EFI\Microsoft\Boot\bootmgfw.efi`, la ruta por defecto que usa el firmware para arrancar Windows.
2. El bootloader original de Windows se renombra a `bootmgfw_orig.efi`.
3. El tema `rEFInd-glassy` vive en `\EFI\Microsoft\Boot\themes\` y `\EFI\refind\themes\`.
4. Dos entradas con iconos propios, una para Windows y otra para macOS.

## Actualizar macOS o Safari

1. Abre OCLP → **Revert Root Patches**.
2. Reinicia (arranca en modo VESA, sin aceleración).
3. Instala la actualización desde *Ajustes → General → Actualización de software*.
4. Tras reiniciar, OCLP → **Start Root Patching** y otro reinicio.

## Replicar el build

> [!IMPORTANT]
> El `config.plist` trae los seriales sanitizados (`GENERATE_YOUR_OWN_SERIAL`, etc.). Antes de arrancar esta EFI o iniciar sesión en tu Apple ID, genera los tuyos con **GenSMBIOS** (modelo `iMac19,1`) y rellena `SystemSerialNumber`, `MLB` y `SystemUUID` en `PlatformInfo → Generic`. En `ROM` pon la MAC de tu Ethernet sin los dos puntos.
