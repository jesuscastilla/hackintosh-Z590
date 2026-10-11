# Hackintosh macOS Sonoma 14.x — ASUS PRIME Z590-P WIFI + i5-11600K + GTX 970

> [!WARNING]
> **DISCLAIMER LEGAL Y DE RESPONSABILIDAD / LEGAL DISCLAIMER:**  
> Este proyecto y la documentación contenida en este repositorio han sido desarrollados **exclusivamente con fines educativos, de investigación, experimentación y comprobación técnica de interoperabilidad de hardware**.  
>  
> La instalación, modificación y ejecución de macOS en hardware que no sea de marca Apple incumple de manera explícita el **Acuerdo de Licencia de Usuario Final (EULA)** de Apple Inc. (específicamente la sección 2.J). En diversas jurisdicciones, esta práctica puede ser considerada una violación de los términos de servicio, propiedad intelectual y derechos de autor.  
>  
> Ni el autor ni los colaboradores promueven la vulneración de licencias ni se hacen responsables de cualquier uso indebido, consecuencias legales, pérdida de datos o daños en el hardware derivados del uso de los archivos, scripts o guías aquí compartidos. Procede bajo tu propia responsabilidad.

---

## 🖥️ Especificaciones Técnicas del Hardware

| Componente | Modelo / Especificación | Estado y Notas |
| :--- | :--- | :--- |
| **Procesador (CPU)** | Intel Core i5-11600K @ 3.90 GHz (Rocket Lake 11ª Gen, 6C/12T) | Spoof de CPUID a Comet Lake (`0x0A0655`) |
| **Placa Base** | ASUS PRIME Z590-P WIFI (LGA1200, Chipset Z590) | Quirks específicos de ASUS (`UpdateSMBIOSMode=Custom`) |
| **Tarjeta Gráfica (dGPU)** | NVIDIA GeForce GTX 970 4GB (Arquitectura Maxwell) | Aceleración gráfica mediante OCLP (Nvidia Web Drivers) |
| **Gráfica Integrada (iGPU)** | Intel UHD Graphics 750 | Deshabilitada (`disable-gpu = 01000000`), sin soporte nativo en macOS |
| **Memoria RAM** | 32 GB DDR4 | Reconocida automáticamente |
| **Almacenamiento** | Samsung SSD 990 PRO NVMe 1TB / Migración a SSD Samsung EVO | Dual Boot con Windows 11 + macOS Sonoma APFS |
| **Ethernet** | Realtek RTL8125 2.5 GbE PCIe | Funcionando al 100% con `LucyRTL8125Ethernet.kext` |
| **Wi-Fi** | Intel Wi-Fi 6 AX201 (CNVi M.2) | Operativo con `itlwm.kext` + HeliPort |
| **Bluetooth** | Intel AX201 Bluetooth | Operativo con `IntelBluetoothFirmware`, `IntelBTPatcher` y `BlueToolFixup` |
| **Audio** | Realtek ALC897 | Salidas de línea y panel frontal con `AppleALC.kext` (`alcid=11`) |
| **Gestor de Arranque** | OpenCore 1.0.x + rEFInd (Tema Glassy) | Menú dual boot automático inmune a resets de NVRAM |
| **SMBIOS** | `iMac19,1` | iMessage, FaceTime y servicios de iCloud operativos |

---

## 📊 Estado de Compatibilidad

* [x] **Aceleración Gráfica:** Plena (Metal / OpenGL parcheado vía OpenCore Legacy Patcher).
* [x] **Red Ethernet 2.5G:** Totalmente funcional a máxima velocidad.
* [x] **Wi-Fi y Bluetooth Intel:** Conexión a redes, periféricos (auriculares, teclado, ratón) operativos.
* [x] **Servicios Apple (iServices):** iCloud, App Store, iMessage y FaceTime funcionales.
* [x] **Mapeo USB:** Puertos USB 2.0, USB 3.0, Tipo-C y cabezales internos mapeados con `UTBMap.kext`.
* [x] **Arranque Dual Limpio:** Menú gráfico rEFInd seleccionable entre Windows 11 y macOS.
* [x] **Protección contra NVRAM Reset:** Arranque persistente configurado en `\EFI\Microsoft\Boot`.
* [!] **Reposo Profundo (S3 Sleep):** Las gráficas Maxwell (GTX 970) bajo Sonoma no soportan la suspensión profunda de hardware (la GPU no despierta). Se utiliza **Reposo de Pantalla (Display Sleep)** para ahorro de energía sin congelación.

---

## ⚙️ Arquitectura de la Configuración OpenCore

### 1. Spoof de CPU (Rocket Lake 11ª Gen)
Apple nunca comercializó ordenadores con procesadores Intel Rocket Lake (11ª generación). Para que el kernel XNU arranque en un i5-11600K, se simula un procesador Comet Lake (10ª generación) en la sección `Kernel -> Emulate`:
* **Cpuid1Data:** `55060A00 00000000 00000000 00000000` (`0x0A0655`)
* **Cpuid1Mask:** `FFFFFFFF 00000000 00000000 00000000`

### 2. Tablas ACPI
* `SSDT-AWAC.aml`: Habilita el RTC clásico en placas modernas serie 400/500 desactivando AWAC.
* `SSDT-EC-USBX-DESKTOP.aml`: Provee un dispositivo Embedded Controller dummy (`EC`) y gestión de energía para USB.
* `SSDT-PLUG-DRTNIA.aml`: Permite a macOS gestionar la energía del procesador (plugin-type = 1).
* `SSDT-RHUB.aml`: Inicialización del Root Hub USB en la placa base.

### 3. Kexts Clave
1. `Lilu.kext` & `VirtualSMC.kext`: Capa base del sistema y emulación SMC.
2. `WhateverGreen.kext`: Control de framebuffers y compatibilidad de GPU.
3. `AppleALC.kext`: Inyección de audio (layout `alcid=11`).
4. `SMCProcessor.kext` & `SMCSuperIO.kext`: Monitorización térmica de CPU y ventiladores.
5. `LucyRTL8125Ethernet.kext`: Driver nativo para la controladora Realtek 2.5GbE.
6. `USBToolBox.kext` & `UTBMap.kext`: Inyección limpia de puertos USB con cabezales internos (`255`) para Bluetooth.
7. `itlwm.kext`: Driver para la tarjeta Wi-Fi Intel AX201.
8. `IntelBluetoothFirmware.kext`, `IntelBTPatcher.kext`, `BlueToolFixup.kext`: Pila completa de Bluetooth Intel en Sonoma.
9. `NVMeFix.kext`: Gestión de energía autónoma para discos NVMe.

### 4. Parches de NVRAM y SIP para OCLP
* **boot-args:**  
  `-v keepsyms=1 debug=0x100 alcid=11 amfi=0x80 ngfxgl=1 ngfxcompat=1 nvda_drv_vrl=1 ipc_control_port_options=0`
  * `amfi=0x80` & `ipc_control_port_options=0`: Imprescindibles para permitir la inyección de librerías de controladores heredados en Sonoma.
  * `nvda_drv_vrl=1`, `ngfxcompat=1`, `ngfxgl=1`: Habilitación y compatibilidad de controladores Nvidia Web Drivers.
* **csr-active-config (SIP):**  
  Configurado en `030A0000` (`0xa03`). Permite a OpenCore Legacy Patcher modificar el volumen APFS sellado sin desproteger completamente el sistema. Se añade `csr-active-config` en `NVRAM -> Delete` para garantizar que la placa aplique el valor en cada arranque.

---

## 🎨 Doble Arranque con rEFInd (Tema Glassy)

Para evitar que el BIOS de ASUS sobreescriba las entradas UEFI tras un reinicio de NVRAM o actualización de Windows:
1. `refind_x64.efi` se sitúa en `\EFI\Microsoft\Boot\bootmgfw.efi` (la ruta predeterminada del firmware para arrancar Windows).
2. El archivo original de Windows se renombra a `bootmgfw_orig.efi`.
3. El tema `rEFInd-glassy` se aloja directamente en `\EFI\Microsoft\Boot\themes\rEFInd-glassy` y en `\EFI\refind\themes\rEFInd-glassy`.
4. El archivo de configuración define dos entradas limpias con iconos personalizados para Windows y macOS.

---

## 🔄 Flujo de Actualización del Sistema y Safari (OCLP)

Cuando macOS Sonoma muestra una actualización de Safari o del sistema:
1. Abrir **OpenCore Legacy Patcher**.
2. Seleccionar **Post-Install Root Patch** y pulsar en **Revert Root Patches**.
3. Reiniciar el sistema (iniciará temporalmente en modo gráfico VESA básico sin aceleración).
4. Ir a **Ajustes del Sistema > General > Actualización de software** e instalar la actualización con el precinto del sistema restaurado.
5. Tras el reinicio, abrir **OpenCore Legacy Patcher**, pulsar en **Start Root Patching** y reiniciar para restaurar la aceleración gráfica.

---

## ⚠️ Instrucciones para Replicar este Repositorio

> [!IMPORTANT]
> **Generación de credenciales únicas para iServices:**  
> Por motivos de seguridad y privacidad, el archivo `config.plist` de este repositorio tiene los números de serie sanitizados con marcadores de posición (`GENERATE_YOUR_OWN_SERIAL`, `GENERATE_YOUR_OWN_MLB`, etc.).  
>  
> Antes de arrancar la EFI en tu ordenador o iniciar sesión en tu cuenta de Apple:
> 1. Descarga la herramienta **GenSMBIOS**.
> 2. Selecciona el modelo **iMac19,1**.
> 3. Rellena los campos `SystemSerialNumber`, `MLB` y `SystemUUID` en la sección `PlatformInfo -> Generic`.
> 4. Inyecta en el campo `ROM` la dirección MAC de tu tarjeta de red Ethernet sin dos puntos (`:`).
