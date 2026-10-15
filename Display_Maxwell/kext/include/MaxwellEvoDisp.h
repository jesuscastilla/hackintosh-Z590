/*
 * MaxwellEvoDisp.h — Phase 3: Linux Nouveau GF119/GM200/GM204 EVO Display Engine Port
 *
 * Ported from Linux drivers/gpu/drm/nouveau/nvkm/engine/disp/:
 *   - gf119.c (gf119_disp_init, gf119_head_state, gf119_disp_curs, gf119_head_vblank_*)
 *   - gm200.c (gm200_sor_route_get, gm200_sor_dp, gm200_disp)
 */

#ifndef MAXWELL_EVO_DISP_H
#define MAXWELL_EVO_DISP_H

#include "gm204_regs.h"
#include "nvkm_osx_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NVKM_EVO_MAX_HEADS              4
#define NVKM_EVO_MAX_SORS               4
#define NVKM_EVO_CURSOR_SIZE            64
#define NVKM_EVO_CURSOR_BYTES           (NVKM_EVO_CURSOR_SIZE * NVKM_EVO_CURSOR_SIZE * 4U)

/* =========================================================================
 * GF119 / GM200 / GM204 EVO Display Engine MMIO Register Offsets
 * ========================================================================= */
#define NV_PDISP_HEAD_MASK_REG          0x00022448U /* Bitmask of physical heads (gf119_head_cnt) */
#define NV_PDISP_INTR_VBLANK_STAT       0x006100c0U /* Bit(head) VBlank status */
#define NV_PDISP_INTR_VBLANK_EN         0x006100ccU /* Bit(head) VBlank enable */
#define NV_PDISP_VBIOS_OWNER            0x006100acU
#define NV_PDISP_VGA_OWNER              0x006194e8U

/* Per-Head RG / Scanline / Underflow Registers (hoff = head * 0x800) */
#define NV_PDISP_RG_UNDERFLOW(h)        (0x00616308U + ((u32)(h) * 0x800U))
#define NV_PDISP_RG_SCANLINE(h)         (0x00616340U + ((u32)(h) * 0x800U))

/* Per-Head Armed Core State Cache (hoff300 = head * 0x300, gf119_head_state) */
#define NV_PDISP_HEAD_HTOTAL_VTOTAL(h)  (0x00640414U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_HSYNC_VSYNC(h)    (0x00640418U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_HBLANKE_VBLANKE(h)(0x0064041cU + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_HBLANKS_VBLANKS(h)(0x00640420U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_PCLK_HZ(h)        (0x00640450U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_VIEWPORT_OUT(h)   (0x00640468U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_CURSOR_CTRL(h)    (0x00640480U + ((u32)(h) * 0x300U))
#define NV_PDISP_HEAD_CURSOR_ADDR(h)    (0x00640484U + ((u32)(h) * 0x300U))

/* Per-Head Primary Framebuffer Plane in Core State Cache (hoff20 = head * 0x20) */
#define NV_PDISP_CORE_FB_OFFSET(h)      (0x00640180U + ((u32)(h) * 0x020U))
#define NV_PDISP_CORE_FB_SIZE(h)        (0x00640188U + ((u32)(h) * 0x020U))
#define NV_PDISP_CORE_FB_STORAGE(h)     (0x0064018cU + ((u32)(h) * 0x020U))
#define NV_PDISP_CORE_FB_FORMAT(h)      (0x00640190U + ((u32)(h) * 0x020U))
#define NV_PDISP_FB_FORMAT_XRGB8888     0x0000cf00U
#define NV_PDISP_FB_FORMAT_ARGB8888     0x0000d100U

/* Per-Head Cursor PIO Channel (user = 13 + head, gf119_disp_curs) */
#define NV_PDISP_PIO_CURS_CTL(h)        (0x00610490U + ((13U + (u32)(h)) * 0x010U))
#define NV_PDISP_PIO_CURS_UPDATE(h)     (0x00690080U + ((13U + (u32)(h)) * 0x1000U))
#define NV_PDISP_PIO_CURS_POS(h)        (0x00690084U + ((13U + (u32)(h)) * 0x1000U))

/* Per-SOR Armed State & Power Control (sor = 0..3) */
#define NV_PDISP_SOR_ARMED_STATE(s)     (0x00640254U + ((u32)(s) * 0x040U))
#define NV_PDISP_SOR_PWR(s)             (0x0061c004U + ((u32)(s) * 0x800U))

/* Armed State of a Single EVO Display Head */
struct nvkm_evo_head_state {
    u8   head_id;
    bool active;
    u16  width;
    u16  height;
    u16  htotal;
    u16  vtotal;
    u32  pitch;
    u32  pixel_clock_khz;
    u16  refresh_hz;
    u64  fb_vram_offset;
    u32  fb_format;
    u64  cursor_vram_offset;
    bool cursor_enabled;
    s8   bound_sor;
    u8   sor_protocol;
};

/* Armed State of a Single SOR (Serial Output Resource) */
struct nvkm_evo_sor_state {
    u8   sor_id;
    u8   heads_mask;
    u8   protocol;
    bool power_on;
};

/* Complete EVO Display Engine State Snapshot */
struct nvkm_evo_disp_state {
    u8   head_mask;
    u8   head_count;
    s8   primary_head;
    struct nvkm_evo_head_state heads[NVKM_EVO_MAX_HEADS];
    struct nvkm_evo_sor_state  sors[NVKM_EVO_MAX_SORS];
};

/* Phase 3 EVO Display Engine Functions */
bool nvkm_evo_probe_state(const struct nvkm_osx_device *dev,
                          struct nvkm_evo_disp_state *out_state);

bool nvkm_evo_disable_underflow(const struct nvkm_osx_device *dev,
                                u8 head_mask);

bool nvkm_evo_cursor_init(const struct nvkm_osx_device *dev,
                          u8 head_id,
                          u64 cursor_vram_offset);

void nvkm_evo_cursor_set_pos(const struct nvkm_osx_device *dev,
                             u8 head_id,
                             s16 x,
                             s16 y,
                             bool visible);

bool nvkm_evo_cursor_upload_argb(volatile uint8_t *bar1_virt,
                                 u64 bar1_size,
                                 u64 cursor_vram_offset,
                                 const uint32_t *argb_pixels,
                                 u16 width,
                                 u16 height);

bool nvkm_evo_head_set_scanout(const struct nvkm_osx_device *dev,
                               u8 head_id,
                               u64 fb_vram_offset,
                               u16 width,
                               u16 height,
                               u32 pitch);

bool nvkm_evo_sor_set_power(const struct nvkm_osx_device *dev,
                            u8 sor_id,
                            bool power_on);

bool nvkm_evo_vblank_poll(const struct nvkm_osx_device *dev,
                          u8 head_id,
                          u16 *out_scanline);

#ifdef __cplusplus
}
#endif

#endif /* MAXWELL_EVO_DISP_H */
