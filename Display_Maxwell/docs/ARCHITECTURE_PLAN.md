# Arquitectura y Plan de Portabilidad: NVIDIA GTX 970 (`GM204`) Linux `nvkm` -> macOS 14+ (`IOKit`)

## 1. Identificación del Hardware Objetivo
- **GPU**: NVIDIA GeForce GTX 970
- **Chipset**: `GM204` (`NV124`, familia Maxwell 2.0)
- **PCI Vendor / Device ID**: `0x10de` / `0x13c2` (`IOPCIMatch = 0x13c210de`)
- **Valor esperado en `NV_PMC_BOOT_0` (`BAR0 + 0x000000`)**: `0x124000a1` (Arquitectura `0x120` Maxwell, Implementación `0x4` -> `GM204`)
- **Distribución de BARs PCI**:
  - `BAR0` (Index 0, Offset `0x10`): `16 MB` MMIO Register Space (`0x000000` - `0xffffff`).
  - `BAR1` (Index 1, Offset `0x14`, 64-bit): Apertura de VRAM mapeable por CPU (`128 MB` / `256 MB`).
  - `BAR3` (Index 2, Offset `0x1c`, 64-bit): Apertura de RAMIN / canales e instancias del sistema.

---

## 2. Mapa de Registros MMIO (`BAR0`) de `GM204` (Extraído de Linux `nvkm`)

| Bloque (`subdev`/`engine`) | Rango `BAR0` | Función en `GM204` | Fuente de Referencia en `linux_ref/nvkm/` |
| :--- | :--- | :--- | :--- |
| **`PMC` (Master Control)** | `0x000000 - 0x000fff` | Identificación de silicio (`0x000000`), habilitación de sub-motores (`0x000200` `PMC_ENABLE`) e interrupciones (`0x000100`). | `engine/device/base.c`, `subdev/mc/gm200.c` |
| **`PBUS` & `PTIMER`** | `0x001000 - 0x009fff` | Control del bus PCI, GPIOs (`0x00d610`), I2C (`0x00d000`) y reloj de precisión en nanosegundos (`0x009400` / `0x009410`). | `subdev/timer/gk20a.c`, `subdev/i2c/gm200.c` |
| **`PTHERM` (Thermal/Fan)** | `0x020000 - 0x020fff` | Sensor térmico interno de la GPU (`0x020400`), umbrales de protección y control PWM/tacómetro de ventiladores. | `subdev/therm/gm200.c`, `subdev/therm/gf119.c` |
| **`PFB` (Framebuffer/VRAM)** | `0x100000 - 0x100fff` | Tamaño total de VRAM (`0x10020c` `PFB_CSTATUS` en MB), controladores de memoria GDDR5 y particiones L2/ROP. | `subdev/fb/gm200.c`, `subdev/fb/ramgm200.c` |
| **`PPWR` / `PMU` (Falcon)** | `0x10a000 - 0x10afff` | Microcontrolador Falcon de gestión de energía y carga de microcódigo firmado ACR (`Secboot`). | `subdev/pmu/gm200.c`, `subdev/acr/gm200.c` |
| **`PROM` (VBIOS ROM)** | `0x300000 - 0x3fffff` | Imagen shadow de la VBIOS (`0x55aa` + tablas `BIT` y `DCB` de conectores DisplayPort/HDMI/DVI). | `subdev/bios/base.c`, `subdev/bios/dcb.c` |
| **`PGRAPH` (3D/2D Engine)** | `0x400000 - 0x4fffff` | Motor de aceleración 2D/3D/Compute Maxwell (`FECS` / `GPCCS` Falcon microcode). | `engine/gr/gm200.c`, `engine/gr/gf100.c` |
| **`PDISP` (EVO Display)** | `0x610000 - 0x6fffff` | Motor de pantalla **EVO (`nv50_disp`)**: 4 `HEAD`s (CRTCs), `SOR` (HDMI/DP), `DAC` (DVI-I), canales DMA `core`/`base`/`ovly`/`curs` (Hardware Cursor). | `engine/disp/gm200.c`, `engine/disp/gf119.c`, `head.c`, `ior.c`, `outp.c` |
| **`PRAMIN` (Instance Mem)** | `0x700000 - 0x7fffff` | Ventana de 1 MB al final de la VRAM (contiene copia activa de VBIOS e instancias de tablas de página). | `subdev/bios/base.c` |

---

## 3. Equivalencia de Primitivas: Linux `nvkm` -> macOS 14+ `IOKit` (`nvkm_osx_shim.h`)

| Primitiva en Linux `nvkm` | Equivalente en macOS `IOKit` (`NVDAMaxwell.kext`) |
| :--- | :--- |
| `nvkm_rd32(device, addr)` | `OSReadLittleInt32(bar0VirtBase, addr)` sobre `IOMemoryMap` de `BAR0` |
| `nvkm_wr32(device, addr, val)` | `OSWriteLittleInt32(bar0VirtBase, addr, val)` |
| `nvkm_mask(device, addr, m, v)` | Lectura `rd32`, aplicación de `((old & ~m) \| v)` y escritura `wr32` |
| `nvkm_rd08(device, addr)` | `((volatile uint8_t *)bar0VirtBase)[addr]` |
| `udelay(us)` / `mdelay(ms)` | `IODelay(us)` / `IOSleep(ms)` |
| `kzalloc(size, GFP_KERNEL)` | `IOMallocZero(size)` (o `IOMalloc(size)` + `bzero`) |
| `kfree(ptr)` | `IOFree(ptr, size)` |
| `nvkm_info` / `nvkm_error` | `IOLog("[NVDAMaxwell] ...")` |

---

## 4. Fases de Desarrollo y Pruebas en `Display_Maxwell`

1. **Fase 1 (`MaxwellProbe` - Lista en `kext/src/`)**:
   - Kext `IOService` protegido por el boot-arg `-maxwell_drv`.
   - Mapea `BAR0`, verifica el chip `GM204` (`0x124000a1`), lee VRAM total (`PFB_CSTATUS`), temperatura del die (`PTHERM`), estado de motores (`PMC_ENABLE`) y cabecera VBIOS (`PROM`), publicando todo en `IORegistry`.
2. **Fase 2 (Parser VBIOS `DCB` + Lectura de EDID vía `subdev/bios` e `i2c`)**:
   - Compilar `subdev/bios/base.c`, `dcb.c` y `conn.c` contra `nvkm_osx_shim.h` dentro del kext para enumerar los conectores físicos de la GTX 970 y extraer el EDID de los monitores conectados.
3. **Fase 3 (`MaxwellFramebuffer` - Subclase de `IOFramebuffer` + Motor `PDISP` EVO)**:
   - Portar `engine/disp/gf119.c` y `gm200.c` para ofrecer cambio de resolución nativo, frecuencia de refresco, DPMS (reposo de monitor) y **Hardware Cursor** en macOS 14+.
4. **Fase 4 (Carga de Firmware `ACR` / `PMU` Falcon y Control de Ventiladores/Clocks)**:
   - Usar los binarios de `firmware/gm204/` para inicializar el Falcon en modo seguro (`secboot`) y gestionar estados de energía (`P-states`).
5. **Fase 5 (Canales DMA `PFIFO` / Aceleración 2D Blit / Puente con `IOAccelerator`)**:
   - Pruebas de envío de comandos DMA a través de `IOUserClient`.
