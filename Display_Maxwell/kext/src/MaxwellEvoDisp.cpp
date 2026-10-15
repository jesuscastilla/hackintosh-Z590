/*
 * MaxwellEvoDisp.cpp — Phase 3 Implementation: GF119/GM200/GM204 EVO Display Engine
 *
 * Direct C/C++ port of Linux Nouveau (`drivers/gpu/drm/nouveau/nvkm/engine/disp/`):
 *   - gf119.c: gf119_head_cnt, gf119_head_state, gf119_sor_state,
 *              gf119_disp_curs_init, gf119_disp_init, gf119_head_vblank_*
 *   - gm200.c: gm200_disp, gm200_sor
 */

#include "../include/MaxwellEvoDisp.h"

static void probe_single_head(const struct nvkm_osx_device *dev,
                              u8 head_id,
                              struct nvkm_evo_head_state *st)
{
    st->head_id   = head_id;
    st->bound_sor = -1;

    u32 hv_total  = nvkm_rd32(dev, NV_PDISP_HEAD_HTOTAL_VTOTAL(head_id));
    u32 hbe_vbe   = nvkm_rd32(dev, NV_PDISP_HEAD_HBLANKE_VBLANKE(head_id));
    u32 hbs_vbs   = nvkm_rd32(dev, NV_PDISP_HEAD_HBLANKS_VBLANKS(head_id));
    u32 vport_out = nvkm_rd32(dev, NV_PDISP_HEAD_VIEWPORT_OUT(head_id));
    u32 pclk_raw  = nvkm_rd32(dev, NV_PDISP_HEAD_PCLK_HZ(head_id));

    st->htotal = (u16)(hv_total & 0xffffU);
    st->vtotal = (u16)((hv_total >> 16) & 0xffffU);

    u16 hblanke = (u16)(hbe_vbe & 0xffffU);
    u16 vblanke = (u16)((hbe_vbe >> 16) & 0xffffU);
    u16 hblanks = (u16)(hbs_vbs & 0xffffU);
    u16 vblanks = (u16)((hbs_vbs >> 16) & 0xffffU);

    u16 vp_w = (u16)(vport_out & 0xffffU);
    u16 vp_h = (u16)((vport_out >> 16) & 0xffffU);

    if (vp_w > 0 && vp_h > 0) {
        st->width  = vp_w;
        st->height = vp_h;
    } else if (hblanks > hblanke && vblanks > vblanke) {
        st->width  = (u16)(hblanks - hblanke);
        st->height = (u16)(vblanks - vblanke);
    }

    st->pixel_clock_khz = (pclk_raw & 0x00ffffffU) / 1000U;
    if (st->htotal > 0 && st->vtotal > 0 && st->pixel_clock_khz > 0) {
        u32 total_px = (u32)st->htotal * (u32)st->vtotal;
        st->refresh_hz = (u16)(((st->pixel_clock_khz * 1000U) + (total_px / 2U)) / total_px);
    }

    u32 fb_off_256 = nvkm_rd32(dev, NV_PDISP_CORE_FB_OFFSET(head_id));
    u32 fb_size    = nvkm_rd32(dev, NV_PDISP_CORE_FB_SIZE(head_id));
    u32 fb_pitch   = nvkm_rd32(dev, NV_PDISP_CORE_FB_STORAGE(head_id));
    u32 fb_fmt     = nvkm_rd32(dev, NV_PDISP_CORE_FB_FORMAT(head_id));

    st->fb_vram_offset = ((u64)fb_off_256) << 8;
    st->fb_format      = fb_fmt;

    if (st->width == 0 && fb_size != 0) {
        st->width  = (u16)(fb_size & 0xffffU);
        st->height = (u16)((fb_size >> 16) & 0xffffU);
    }

    if ((fb_pitch & 0x000fffffU) != 0) {
        st->pitch = (fb_pitch & 0x000fffffU) << 6;
    } else if (st->width > 0) {
        st->pitch = (u32)st->width * 4U;
    }

    u32 curs_ctrl = nvkm_rd32(dev, NV_PDISP_HEAD_CURSOR_CTRL(head_id));
    u32 curs_addr = nvkm_rd32(dev, NV_PDISP_HEAD_CURSOR_ADDR(head_id));
    st->cursor_enabled     = (curs_ctrl & 0x80000000U) != 0;
    st->cursor_vram_offset = ((u64)curs_addr) << 8;

    st->active = (st->width >= 640 && st->height >= 480);
    if (st->active && st->refresh_hz == 0) {
        st->refresh_hz = 60;
    }
}

bool nvkm_evo_probe_state(const struct nvkm_osx_device *dev,
                          struct nvkm_evo_disp_state *out_state)
{
    if (!dev || !out_state) return false;
    for (size_t i = 0; i < sizeof(*out_state); ++i) {
        ((u8 *)out_state)[i] = 0;
    }
    out_state->primary_head = -1;

    u32 mask = nvkm_rd32(dev, NV_PDISP_HEAD_MASK_REG) & 0x0fU;
    if (mask == 0) {
        mask = nvkm_rd32(dev, NV_PDISP_CAP_HEADS) & 0x0fU;
    }
    if (mask == 0) {
        mask = 0x0fU;
    }
    out_state->head_mask = (u8)mask;

    for (u8 h = 0; h < NVKM_EVO_MAX_HEADS; ++h) {
        if (!(mask & (1U << h))) continue;
        out_state->head_count++;
        probe_single_head(dev, h, &out_state->heads[h]);
        if (out_state->heads[h].active && out_state->primary_head < 0) {
            out_state->primary_head = (s8)h;
        }
    }

    for (u8 s = 0; s < NVKM_EVO_MAX_SORS; ++s) {
        struct nvkm_evo_sor_state *sor = &out_state->sors[s];
        sor->sor_id = s;
        u32 rd = nvkm_rd32(dev, NV_PDISP_SOR_ARMED_STATE(s));
        sor->heads_mask = (u8)(rd & 0x0fU);
        sor->protocol   = (u8)((rd >> 8) & 0x0fU);
        u32 pwr = nvkm_rd32(dev, NV_PDISP_SOR_PWR(s));
        sor->power_on   = (pwr & 0x00000001U) != 0;

        for (u8 h = 0; h < NVKM_EVO_MAX_HEADS; ++h) {
            if (sor->heads_mask & (1U << h)) {
                out_state->heads[h].bound_sor    = (s8)s;
                out_state->heads[h].sor_protocol = sor->protocol;
            }
        }
    }

    if (out_state->primary_head < 0) {
        out_state->primary_head = 0;
    }
    return true;
}


bool nvkm_evo_disable_underflow(const struct nvkm_osx_device *dev, u8 head_mask)
{
    if (!dev) return false;
    for (u8 h = 0; h < NVKM_EVO_MAX_HEADS; ++h) {
        if (head_mask & (1U << h)) {
            nvkm_mask(dev, NV_PDISP_RG_UNDERFLOW(h), 0x00000111U, 0x00000010U);
        }
    }
    return true;
}

bool nvkm_evo_cursor_init(const struct nvkm_osx_device *dev,
                          u8 head_id,
                          u64 cursor_vram_offset)
{
    if (!dev || head_id >= NVKM_EVO_MAX_HEADS) return false;

    u32 ctl_reg = NV_PDISP_PIO_CURS_CTL(head_id);
    nvkm_wr32(dev, ctl_reg, 0x00010001U);
    for (u32 timeout = 2000; timeout > 0; --timeout) {
        if ((nvkm_rd32(dev, ctl_reg) & 0x00010000U) == 0) {
            break;
        }
        nvkm_udelay(1);
    }

    nvkm_wr32(dev, NV_PDISP_HEAD_CURSOR_ADDR(head_id), (u32)(cursor_vram_offset >> 8));
    nvkm_wr32(dev, NV_PDISP_HEAD_CURSOR_CTRL(head_id), 0x82050000U);
    return true;
}

void nvkm_evo_cursor_set_pos(const struct nvkm_osx_device *dev,
                             u8 head_id,
                             s16 x,
                             s16 y,
                             bool visible)
{
    if (!dev || head_id >= NVKM_EVO_MAX_HEADS) return;

    u16 ux = (x < 0) ? 0 : (u16)x;
    u16 uy = (y < 0) ? 0 : (u16)y;
    u32 packed = ((u32)uy << 16) | (u32)ux;

    nvkm_wr32(dev, NV_PDISP_PIO_CURS_POS(head_id), packed);
    nvkm_wr32(dev, NV_PDISP_PIO_CURS_UPDATE(head_id), 0x00000001U);
    nvkm_wr32(dev, NV_PDISP_CURSOR_POS(head_id), packed);
    nvkm_mask(dev, NV_PDISP_HEAD_CURSOR_CTRL(head_id),
              0x80000000U, visible ? 0x80000000U : 0x00000000U);
}

bool nvkm_evo_cursor_upload_argb(volatile uint8_t *bar1_virt,
                                 u64 bar1_size,
                                 u64 cursor_vram_offset,
                                 const uint32_t *argb_pixels,
                                 u16 width,
                                 u16 height)
{
    if (!bar1_virt || !argb_pixels) return false;
    if ((cursor_vram_offset + NVKM_EVO_CURSOR_BYTES) > bar1_size) return false;

    volatile uint32_t *dst = (volatile uint32_t *)(bar1_virt + cursor_vram_offset);
    for (u16 row = 0; row < NVKM_EVO_CURSOR_SIZE; ++row) {
        for (u16 col = 0; col < NVKM_EVO_CURSOR_SIZE; ++col) {
            uint32_t px = 0x00000000U;
            if (row < height && col < width) {
                px = argb_pixels[(size_t)row * width + col];
            }
            dst[(size_t)row * NVKM_EVO_CURSOR_SIZE + col] = px;
        }
    }
    return true;
}

bool nvkm_evo_head_set_scanout(const struct nvkm_osx_device *dev,
                               u8 head_id,
                               u64 fb_vram_offset,
                               u16 width,
                               u16 height,
                               u32 pitch)
{
    if (!dev || head_id >= NVKM_EVO_MAX_HEADS || width == 0 || height == 0) return false;

    nvkm_wr32(dev, NV_PDISP_CORE_FB_OFFSET(head_id), (u32)(fb_vram_offset >> 8));
    nvkm_wr32(dev, NV_PDISP_CORE_FB_SIZE(head_id),   ((u32)height << 16) | (u32)width);
    nvkm_wr32(dev, NV_PDISP_CORE_FB_STORAGE(head_id),(pitch >> 6));
    nvkm_wr32(dev, NV_PDISP_CORE_FB_FORMAT(head_id), NV_PDISP_FB_FORMAT_XRGB8888);
    nvkm_wr32(dev, NV_PDISP_HEAD_VIEWPORT_OUT(head_id), ((u32)height << 16) | (u32)width);
    return true;
}

bool nvkm_evo_sor_set_power(const struct nvkm_osx_device *dev,
                            u8 sor_id,
                            bool power_on)
{
    if (!dev || sor_id >= NVKM_EVO_MAX_SORS) return false;
    u32 reg = NV_PDISP_SOR_PWR(sor_id);

    for (u32 t = 1000; t > 0; --t) {
        if ((nvkm_rd32(dev, reg) & 0x80000000U) == 0) break;
        nvkm_udelay(1);
    }
    nvkm_mask(dev, reg, 0x80000001U, 0x80000000U | (power_on ? 1U : 0U));
    for (u32 t = 1000; t > 0; --t) {
        if ((nvkm_rd32(dev, reg) & 0x80000000U) == 0) break;
        nvkm_udelay(1);
    }
    return true;
}

bool nvkm_evo_vblank_poll(const struct nvkm_osx_device *dev,
                          u8 head_id,
                          u16 *out_scanline)
{
    if (!dev || head_id >= NVKM_EVO_MAX_HEADS) return false;
    u32 rg = nvkm_rd32(dev, NV_PDISP_RG_SCANLINE(head_id));
    if (out_scanline) {
        *out_scanline = (u16)(rg & 0xffffU);
    }
    u32 bit = (1U << head_id);
    u32 stat = nvkm_rd32(dev, NV_PDISP_INTR_VBLANK_STAT);
    if (stat & bit) {
        nvkm_wr32(dev, NV_PDISP_INTR_VBLANK_STAT, bit);
        return true;
    }
    return (rg & 0x00010000U) != 0;
}
