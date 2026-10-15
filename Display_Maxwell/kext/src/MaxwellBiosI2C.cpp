/*
 * MaxwellBiosI2C.cpp — Phase 2 Implementation: VBIOS DCB v4.1, Connector & I2C/AUX EDID
 *
 * Direct C/C++ port of Linux Nouveau (`drivers/gpu/drm/nouveau/nvkm/`):
 *   - subdev/bios/base.c, bit.c, dcb.c, conn.c, i2c.c
 *   - subdev/i2c/busgf119.c (PMGR I2C bit-banging at 0x00d014 + drive * 0x20)
 *   - subdev/i2c/auxgm200.c (PMGR DP AUX at 0x00d900 + aux * 0x50)
 */

#include "../include/MaxwellBiosI2C.h"

static inline u8 bios_rd08(const struct nvkm_osx_device *dev, u32 base, u32 off)
{
    return nvkm_rd08(dev, base + off);
}

static inline u16 bios_rd16(const struct nvkm_osx_device *dev, u32 base, u32 off)
{
    return nvkm_rd16(dev, base + off);
}

static inline u32 bios_rd32(const struct nvkm_osx_device *dev, u32 base, u32 off)
{
    return nvkm_rd32(dev, base + off);
}

const char *nvkm_dcb_output_type_name(u8 type)
{
    switch (type) {
    case DCB_OUTPUT_ANALOG: return "ANALOG";
    case DCB_OUTPUT_TV:     return "TV";
    case DCB_OUTPUT_TMDS:   return "TMDS (DVI/HDMI)";
    case DCB_OUTPUT_LVDS:   return "LVDS";
    case DCB_OUTPUT_DP:     return "DP";
    default:                return "UNKNOWN";
    }
}

const char *nvkm_dcb_conn_type_name(u8 conn_type)
{
    switch (conn_type) {
    case DCB_CONN_VGA:      return "VGA";
    case DCB_CONN_DVI_I:    return "DVI-I";
    case DCB_CONN_DVI_D:    return "DVI-D";
    case DCB_CONN_DP:       return "DisplayPort";
    case DCB_CONN_EDP:      return "eDP";
    case DCB_CONN_HDMI_A:   return "HDMI-A";
    case DCB_CONN_HDMI_C:   return "HDMI-C";
    default:                return "Unknown";
    }
}

/* Locate BIT signature ("\xff\xb8BIT") & extract version from BIT 'i' entry */
static void parse_bit_version(const struct nvkm_osx_device *dev,
                              u32 base,
                              struct nvkm_bios_topology *topo)
{
    for (u32 i = 0; i < 0x10000U - 6U; ++i) {
        if (bios_rd08(dev, base, i)     == 0xff &&
            bios_rd08(dev, base, i + 1) == 0xb8 &&
            bios_rd08(dev, base, i + 2) == 'B'  &&
            bios_rd08(dev, base, i + 3) == 'I'  &&
            bios_rd08(dev, base, i + 4) == 'T') {
            topo->bit_offset = (u16)i;
            u8 entry_size = bios_rd08(dev, base, i + 9);
            u8 entries    = bios_rd08(dev, base, i + 10);
            u32 entry     = i + 12;
            while (entries--) {
                if (bios_rd08(dev, base, entry) == 'i') {
                    u16 len = bios_rd16(dev, base, entry + 2);
                    u16 off = bios_rd16(dev, base, entry + 4);
                    if (len >= 4) {
                        topo->version_major = bios_rd08(dev, base, off + 3);
                        topo->version_chip  = bios_rd08(dev, base, off + 2);
                        topo->version_minor = bios_rd08(dev, base, off + 1);
                        topo->version_micro = bios_rd08(dev, base, off + 0);
                        topo->version_patch = (len >= 5) ? bios_rd08(dev, base, off + 4) : 0;
                    }
                    return;
                }
                entry += entry_size;
            }
            return;
        }
    }
}

/* Parse CCB/I2C entry (Linux: subdev/bios/i2c.c dcb_i2c_parse) */
static void parse_dcb_i2c_entry(const struct nvkm_osx_device *dev,
                                u32 base, u16 dcb, u8 dcb_ver, u8 i2c_idx,
                                struct nvkm_dcb_i2c_entry *out)
{
    out->type  = DCB_I2C_UNUSED;
    out->drive = DCB_I2C_UNUSED;
    out->auxch = DCB_I2C_UNUSED;

    if (dcb_ver < 0x30) return;
    u16 i2c_tbl = bios_rd16(dev, base, dcb + 4);
    if (!i2c_tbl) return;

    u8 ver = bios_rd08(dev, base, i2c_tbl + 0);
    u8 hdr = bios_rd08(dev, base, i2c_tbl + 1);
    u8 cnt = bios_rd08(dev, base, i2c_tbl + 2);
    u8 len = bios_rd08(dev, base, i2c_tbl + 3);
    if (i2c_idx >= cnt) return;

    u16 ent = i2c_tbl + hdr + (i2c_idx * len);
    if (ver >= 0x41) {
        u32 val   = bios_rd32(dev, base, ent);
        u8  i2c_p = val & 0x1fU;
        u8  aux_p = (val >> 5) & 0x1fU;
        if (i2c_p == 0x1fU && aux_p == 0x1fU) return;
        out->type  = DCB_I2C_PMGR;
        out->drive = (i2c_p == 0x1fU) ? DCB_I2C_UNUSED : i2c_p;
        out->auxch = (aux_p == 0x1fU) ? DCB_I2C_UNUSED : aux_p;
    }
}

/* Parse Connector Table Entry (Linux: subdev/bios/conn.c nvbios_connEp) */
static void parse_dcb_conn_entry(const struct nvkm_osx_device *dev,
                                 u32 base, u16 dcb, u8 dcb_ver, u8 dcb_hdr,
                                 u8 conn_idx, u8 *out_type, u8 *out_hpd)
{
    *out_type = DCB_CONN_UNUSED;
    *out_hpd  = 0;
    if (dcb_ver < 0x30 || dcb_hdr < 0x16) return;

    u16 conn_tbl = bios_rd16(dev, base, dcb + 0x14);
    if (!conn_tbl) return;

    u8 ver = bios_rd08(dev, base, conn_tbl + 0);
    u8 hdr = bios_rd08(dev, base, conn_tbl + 1);
    u8 cnt = bios_rd08(dev, base, conn_tbl + 2);
    u8 len = bios_rd08(dev, base, conn_tbl + 3);

/* =========================================================================
 * PMGR I2C Bit-Banging Engine (Linux: subdev/i2c/busgf119.c)
 * Register: 0x00d014 + (drive * 0x20)
 * ========================================================================= */
static inline u32 gf119_i2c_reg(u8 drive)
{
    return 0x00d014U + ((u32)drive * 0x20U);
}

static inline void i2c_drive_scl(const struct nvkm_osx_device *dev, u32 reg, int hi)
{
    nvkm_mask(dev, reg, 0x00000001U, hi ? 0x00000001U : 0U);
    nvkm_udelay(5);
}

static inline void i2c_drive_sda(const struct nvkm_osx_device *dev, u32 reg, int hi)
{
    nvkm_mask(dev, reg, 0x00000002U, hi ? 0x00000002U : 0U);
    nvkm_udelay(5);
}

static inline int i2c_sense_sda(const struct nvkm_osx_device *dev, u32 reg)
{
    return (nvkm_rd32(dev, reg) & 0x00000020U) ? 1 : 0;
}

static void i2c_start(const struct nvkm_osx_device *dev, u32 reg)
{
    i2c_drive_sda(dev, reg, 1);
    i2c_drive_scl(dev, reg, 1);
    i2c_drive_sda(dev, reg, 0);
    i2c_drive_scl(dev, reg, 0);
}

static void i2c_stop(const struct nvkm_osx_device *dev, u32 reg)
{
    i2c_drive_sda(dev, reg, 0);
    i2c_drive_scl(dev, reg, 1);
    i2c_drive_sda(dev, reg, 1);
}

static bool i2c_write_byte(const struct nvkm_osx_device *dev, u32 reg, u8 byte)
{
    for (int b = 7; b >= 0; --b) {
        i2c_drive_sda(dev, reg, (byte >> b) & 1);
        i2c_drive_scl(dev, reg, 1);
        i2c_drive_scl(dev, reg, 0);
    }
    i2c_drive_sda(dev, reg, 1);
    i2c_drive_scl(dev, reg, 1);
    int nack = i2c_sense_sda(dev, reg);
    i2c_drive_scl(dev, reg, 0);
    return (nack == 0);
}

static u8 i2c_read_byte(const struct nvkm_osx_device *dev, u32 reg, bool send_ack)
{
    u8 val = 0;
    i2c_drive_sda(dev, reg, 1);
    for (int b = 7; b >= 0; --b) {
        i2c_drive_scl(dev, reg, 1);
        if (i2c_sense_sda(dev, reg)) {
            val |= (u8)(1U << b);
        }
        i2c_drive_scl(dev, reg, 0);
    }
    i2c_drive_sda(dev, reg, send_ack ? 0 : 1);
    i2c_drive_scl(dev, reg, 1);
    i2c_drive_scl(dev, reg, 0);
    i2c_drive_sda(dev, reg, 1);
    return val;
}

bool nvkm_gf119_i2c_read_edid(const struct nvkm_osx_device *dev,
                              u8 drive_port,
                              u8 out_edid[NVKM_EDID_SIZE])
{
    if (!dev || drive_port >= NVKM_MAX_I2C_PORTS) return false;
    u32 reg = gf119_i2c_reg(drive_port);

    nvkm_wr32(dev, reg, 0x00000007U);
    nvkm_udelay(10);

    i2c_start(dev, reg);
    if (!i2c_write_byte(dev, reg, 0xA0U) || !i2c_write_byte(dev, reg, 0x00U)) {
        i2c_stop(dev, reg);
        return false;
    }

    i2c_start(dev, reg);
    if (!i2c_write_byte(dev, reg, 0xA1U)) {
        i2c_stop(dev, reg);
        return false;
    }

    for (int i = 0; i < NVKM_EDID_SIZE; ++i) {
        out_edid[i] = i2c_read_byte(dev, reg, (i + 1) < NVKM_EDID_SIZE);
    }
    i2c_stop(dev, reg);
    return true;
}

/* =========================================================================
 * GM200 / GM204 DisplayPort AUX Channel (Linux: subdev/i2c/auxgm200.c)
 * Base: 0x00d900 + (aux_ch * 0x50)
 * ========================================================================= */
bool nvkm_gm200_aux_check_sink(const struct nvkm_osx_device *dev, u8 aux_ch)
{
    if (!dev || aux_ch >= 8) return false;
    u32 stat = nvkm_rd32(dev, 0x00d958U + ((u32)aux_ch * 0x50U));
    return (stat & 0x10000000U) != 0;
}

bool nvkm_gm200_aux_read_edid(const struct nvkm_osx_device *dev,
                              u8 aux_ch,
                              u8 out_edid[NVKM_EDID_SIZE])
{
    if (!nvkm_gm200_aux_check_sink(dev, aux_ch)) return false;
    u32 base = (u32)aux_ch * 0x50U;

    nvkm_mask(dev, 0x00d968U + base, 0x00010000U, 0x00000000U);
    for (u32 chunk = 0; chunk < NVKM_EDID_SIZE; chunk += 16) {
        for (u32 i = 0; i < 16; i += 4) {
            u32 word = nvkm_rd32(dev, 0x00d940U + base + i);
            out_edid[chunk + i + 0] = (u8)(word & 0xffU);
            out_edid[chunk + i + 1] = (u8)((word >> 8) & 0xffU);
            out_edid[chunk + i + 2] = (u8)((word >> 16) & 0xffU);
            out_edid[chunk + i + 3] = (u8)((word >> 24) & 0xffU);
        }
    }
    nvkm_mask(dev, 0x00d968U + base, 0x00010000U, 0x00010000U);
    return true;
}

    if ((ver != 0x30 && ver != 0x40 && ver != 0x41) || conn_idx >= cnt) return;

    u16 ent = conn_tbl + hdr + (conn_idx * len);
    *out_type = bios_rd08(dev, base, ent + 0x00);
    u8 hpd = (bios_rd08(dev, base, ent + 0x01) & 0x30U) >> 4;
    if (len >= 4) {
        hpd |= (bios_rd08(dev, base, ent + 0x02) & 0x03U) << 2;

/* =========================================================================
 * EDID 1.3 / 1.4 Header, Checksum, Monitor Name & Preferred DTD Parser
 * ========================================================================= */
bool nvkm_edid_parse_summary(const u8 edid[NVKM_EDID_SIZE],
                             char out_name[16],
                             u16 *out_width,
                             u16 *out_height,
                             u16 *out_refresh_hz)
{
    static const u8 kHeader[8] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
    for (int i = 0; i < 8; ++i) {
        if (edid[i] != kHeader[i]) return false;
    }

    u8 sum = 0;
    for (int i = 0; i < NVKM_EDID_SIZE; ++i) sum += edid[i];
    if (sum != 0) return false;

    if (out_name) {
        out_name[0] = '\0';
        for (int d = 0; d < 4; ++d) {
            int off = 54 + (d * 18);
            if (edid[off] == 0 && edid[off + 1] == 0 && edid[off + 3] == 0xfc) {
                int pos = 0;
                for (int c = 0; c < 13 && pos < 15; ++c) {
                    char ch = (char)edid[off + 5 + c];
                    if (ch == '\n' || ch == '\r' || ch == '\0') break;
                    out_name[pos++] = ch;
                }
                out_name[pos] = '\0';
            }
        }
    }

    u16 pclk_10khz = (u16)edid[54] | ((u16)edid[55] << 8);
    if (pclk_10khz != 0) {
        u16 hactive = (u16)edid[56] | (((u16)(edid[58] >> 4) & 0x0fU) << 8);
        u16 hblank  = (u16)edid[57] | (((u16)edid[58] & 0x0fU) << 8);
        u16 vactive = (u16)edid[59] | (((u16)(edid[61] >> 4) & 0x0fU) << 8);
        u16 vblank  = (u16)edid[60] | (((u16)edid[61] & 0x0fU) << 8);
        u32 htotal  = (u32)hactive + (u32)hblank;
        u32 vtotal  = (u32)vactive + (u32)vblank;
        if (out_width)  *out_width  = hactive;
        if (out_height) *out_height = vactive;
        if (out_refresh_hz && htotal > 0 && vtotal > 0) {
            u32 hz = ((u32)pclk_10khz * 10000U + ((htotal * vtotal) / 2U)) / (htotal * vtotal);
            *out_refresh_hz = (u16)hz;
        }
    }
    return true;
}

/* =========================================================================
 * Main Phase 2 Entry Point: VBIOS + DCB v4.1 + Connectors + I2C/AUX Probe
 * ========================================================================= */
bool nvkm_bios_parse_topology(const struct nvkm_osx_device *dev,
                              struct nvkm_bios_topology *topo,
                              bool probe_i2c_edid)
{
    if (!dev || !topo) return false;
    for (size_t i = 0; i < sizeof(*topo); ++i) {
        ((u8 *)topo)[i] = 0;
    }

    const u32 bases[2] = { NV_PRAMIN_BASE, NV_PROM_BASE };
    u32 base = 0;
    for (size_t i = 0; i < ARRAY_SIZE(bases); ++i) {
        if (bios_rd16(dev, bases[i], 0x0000) == NV_VBIOS_SIGNATURE) {
            base = bases[i];
            break;
        }
    }
    if (!base) return false;
    topo->vbios_base = base;

    parse_bit_version(dev, base, topo);

    u16 dcb = bios_rd16(dev, base, 0x0036);
    if (!dcb) return false;

    u8 ver = bios_rd08(dev, base, dcb + 0);
    u8 hdr = bios_rd08(dev, base, dcb + 1);
    u8 cnt = bios_rd08(dev, base, dcb + 2);
    u8 len = bios_rd08(dev, base, dcb + 3);
    if (ver < 0x30 || ver >= 0x42 || bios_rd32(dev, base, dcb + 6) != NVKM_DCB_SIGNATURE) {
        return false;
    }

    topo->dcb_offset  = dcb;
    topo->dcb_version = ver;

    for (u8 idx = 0; idx < cnt && topo->output_count < NVKM_MAX_DCB_OUTPUTS; ++idx) {
        u16 outp_off = dcb + hdr + (idx * len);
        u32 conn     = bios_rd32(dev, base, outp_off + 0x00);
        u32 conf     = bios_rd32(dev, base, outp_off + 0x04);
        if (conn == 0x00000000U || conn == 0xffffffffU) break;

        u8 type = conn & 0x0fU;
        if (type == DCB_OUTPUT_UNUSED) continue;
        if (type == DCB_OUTPUT_EOL)    break;

        struct nvkm_dcb_parsed_output *o = &topo->outputs[topo->output_count++];
        o->index         = idx;
        o->type          = type;
        o->i2c_index     = (conn >> 4)  & 0x0fU;
        o->heads_mask    = (conn >> 8)  & 0x0fU;
        o->connector_idx = (conn >> 12) & 0x0fU;
        o->or_index      = (conn >> 24) & 0x0fU;
        o->sor_link      = (conf >> 4)  & 0x03U;

        if (ver >= 0x40 && type == DCB_OUTPUT_DP) {
            switch (conf & 0x00e00000U) {
            case 0x00000000U: o->dp_link_bw = 0x06; break;
            case 0x00200000U: o->dp_link_bw = 0x0a; break;
            case 0x00400000U: o->dp_link_bw = 0x14; break;
            default:          o->dp_link_bw = 0x1e; break;
            }
            u8 lanes = (conf >> 24) & 0x0fU;
            o->dp_link_lanes = (lanes >= 4) ? 4 : ((lanes >= 2) ? 2 : 1);
        }

        parse_dcb_conn_entry(dev, base, dcb, ver, hdr, o->connector_idx,
                             &o->connector_type, &o->hpd_pin);
        parse_dcb_i2c_entry(dev, base, dcb, ver, o->i2c_index, &o->i2c);

        if (probe_i2c_edid) {
            if (o->type == DCB_OUTPUT_DP && o->i2c.auxch != DCB_I2C_UNUSED) {
                o->sink_connected = nvkm_gm200_aux_check_sink(dev, o->i2c.auxch);
                if (o->sink_connected && nvkm_gm200_aux_read_edid(dev, o->i2c.auxch, o->edid)) {
                    o->edid_valid = nvkm_edid_parse_summary(o->edid, o->monitor_name,
                        &o->preferred_width, &o->preferred_height, &o->preferred_refresh_hz);
                }
            } else if (o->i2c.drive != DCB_I2C_UNUSED) {
                if (nvkm_gf119_i2c_read_edid(dev, o->i2c.drive, o->edid)) {
                    o->edid_valid = nvkm_edid_parse_summary(o->edid, o->monitor_name,
                        &o->preferred_width, &o->preferred_height, &o->preferred_refresh_hz);
                    o->sink_connected = o->edid_valid;
                }
            }
        }
    }
    return true;
}

        hpd |= (bios_rd08(dev, base, ent + 0x03) & 0x07U) << 4;
    }
    *out_hpd = hpd;
}
