/*
 * gm204_regs.h — NVIDIA Maxwell GM204 (NV124 / GTX 970) MMIO Register Offsets
 * Derived from Linux kernel drivers/gpu/drm/nouveau/nvkm/ (MIT License)
 */

#ifndef GM204_REGS_H
#define GM204_REGS_H

#include <stdint.h>

/* PCI Configuration Space */
#define NV_PCI_VENDOR_NVIDIA            0x10de
#define NV_PCI_DEVICE_GTX970            0x13c2
#define NV_PCI_DEVICE_GTX980            0x13c0
#define NV_PCI_BAR0_INDEX               0       /* Offset 0x10: 16MB MMIO */
#define NV_PCI_BAR1_INDEX               1       /* Offset 0x14: VRAM Aperture (64-bit) */
#define NV_PCI_BAR3_INDEX               2       /* Offset 0x1C: RAMIN Aperture (64-bit) */

/* =========================================================================
 * NV_PMC (Master Control) — BAR0 0x000000 - 0x000FFF
 * Ref: linux_ref/nvkm/engine/device/base.c & subdev/mc/gm200.c
 * ========================================================================= */
#define NV_PMC_BOOT_0                   0x00000000U
#define NV_PMC_BOOT_0_ARCH_MASK         0x1f000000U
#define NV_PMC_BOOT_0_ARCH_SHIFT        20
#define NV_PMC_BOOT_0_IMPL_MASK         0x00f00000U
#define NV_PMC_BOOT_0_IMPL_SHIFT        20
#define NV_PMC_BOOT_0_CHIPSET_MASK      0x1ff00000U
#define NV_PMC_BOOT_0_CHIPSET_SHIFT     20
#define NV_CHIPSET_GM200                0x120U
#define NV_CHIPSET_GM204                0x124U
#define NV_CHIPSET_GM206                0x126U

#define NV_PMC_INTR_0                   0x00000100U
#define NV_PMC_INTR_EN_0                0x00000140U
#define NV_PMC_ENABLE                   0x00000200U

/* =========================================================================
 * NV_PTIMER (High-Resolution GPU Timer) — BAR0 0x009000 - 0x009FFF
 * Ref: linux_ref/nvkm/subdev/timer/gk20a.c
 * ========================================================================= */
#define NV_PTIMER_TIME_0                0x00009400U /* Low 32 bits (ns) */
#define NV_PTIMER_TIME_1                0x00009410U /* High 32 bits (ns) */

/* =========================================================================
 * NV_PTHERM (Thermal Sensor & Fan PWM) — BAR0 0x020000 - 0x020FFF
 * Ref: linux_ref/nvkm/subdev/therm/gf119.c & gm200.c
 * ========================================================================= */
#define NV_PTHERM_TEMP_SENSOR_RAW       0x00020008U
#define NV_PTHERM_TEMP_CELSIUS          0x00020400U /* Signed integer Celsius on GF119/GM20x */
#define NV_PTHERM_PWM_CTRL              0x000200d8U
#define NV_PTHERM_PWM_DUTY              0x000200dcU

/* =========================================================================
 * NV_PFB (Framebuffer & VRAM Controller) — BAR0 0x100000 - 0x100FFF
 * Ref: linux_ref/nvkm/subdev/fb/gf100.c & ramgm200.c
 * ========================================================================= */
#define NV_PFB_CSTATUS                  0x0010020cU /* Total VRAM in MiB (bits 31:0) */
#define NV_PFB_CFG0                     0x00100200U

/* =========================================================================
 * NV_PPWR / PMU Falcon — BAR0 0x10A000 - 0x10AFFF
 * Ref: linux_ref/nvkm/subdev/pmu/gm200.c
 * ========================================================================= */
#define NV_PPWR_FALCON_IRQSSET          0x0010a000U
#define NV_PPWR_FALCON_CPUCTL           0x0010a100U
#define NV_PPWR_FALCON_BOOTVEC          0x0010a104U
#define NV_PPWR_FALCON_HWCFG            0x0010a108U

/* =========================================================================
 * NV_PROM / NV_PRAMIN (VBIOS Shadow Image)
 * Ref: linux_ref/nvkm/subdev/bios/base.c
 * ========================================================================= */
#define NV_PBUS_PCI_NV_20               0x00001850U
#define NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED (1U << 0)
#define NV_PROM_BASE                    0x00300000U /* 64KB - 1MB VBIOS ROM window */
#define NV_PRAMIN_BASE                  0x00700000U /* 1MB VRAM instance window */
#define NV_VBIOS_SIGNATURE              0xaa55U     /* Little-endian 0x55, 0xAA */

/* =========================================================================
 * NV_PDISP (EVO Display Engine — GF119 / GM200 / GM204)
 * Ref: linux_ref/nvkm/engine/disp/gf119.c & gm200.c
 * ========================================================================= */
#define NV_PDISP_BASE                   0x00610000U
#define NV_PDISP_CAP_HEADS              0x0061002cU /* Bitmask of active CRTC heads */
#define NV_PDISP_VGA_CRTC_BASE          0x00619400U
#define NV_PDISP_HASH_BASE              0x00610200U
#define NV_PDISP_CORE_CHN_CTL           0x00610490U
#define NV_PDISP_CURS_CHN_CTL(head)     (0x006104e0U + ((head) * 0x10U)) /* Hardware Cursor */
#define NV_PDISP_CURSOR_POS(head)       (0x00640084U + ((head) * 0x800U))

#endif /* GM204_REGS_H */
