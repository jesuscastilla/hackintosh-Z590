/*
 * MaxwellBiosI2C.h — Phase 2: Linux nvkm VBIOS / DCB v4.1 / Connector / I2C & DP-AUX Port
 *
 * Ported from Linux drivers/gpu/drm/nouveau/nvkm/:
 *   - subdev/bios/base.c, bit.c, dcb.c, conn.c, i2c.c
 *   - subdev/i2c/busgf119.c, auxgm200.c
 */

#ifndef MAXWELL_BIOS_I2C_H
#define MAXWELL_BIOS_I2C_H

#include "gm204_regs.h"
#include "nvkm_osx_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NVKM_MAX_DCB_OUTPUTS        16
#define NVKM_MAX_I2C_PORTS          16
#define NVKM_EDID_SIZE              128
#define NVKM_DCB_SIGNATURE          0x4edcbdcbU

/* DCB Output Types (Linux: subdev/bios/dcb.h) */
enum nvkm_dcb_output_type {
    DCB_OUTPUT_ANALOG   = 0x0,
    DCB_OUTPUT_TV       = 0x1,
    DCB_OUTPUT_TMDS     = 0x2,  /* DVI-D / HDMI */
    DCB_OUTPUT_LVDS     = 0x3,
    DCB_OUTPUT_DP       = 0x6,  /* DisplayPort */
    DCB_OUTPUT_EOL      = 0xe,
    DCB_OUTPUT_UNUSED   = 0xf
};

/* DCB Connector Physical Types (Linux: subdev/bios/conn.h) */
enum nvkm_dcb_conn_type {
    DCB_CONN_VGA        = 0x00,
    DCB_CONN_DVI_I      = 0x30,
    DCB_CONN_DVI_D      = 0x31,
    DCB_CONN_DP         = 0x46,
    DCB_CONN_EDP        = 0x47,
    DCB_CONN_HDMI_A     = 0x61,
    DCB_CONN_HDMI_C     = 0x63,
    DCB_CONN_UNUSED     = 0xff
};

/* DCB I2C Entry Types (Linux: subdev/bios/i2c.h) */
enum nvkm_dcb_i2c_type {
    DCB_I2C_NV04_BIT    = 0x00,
    DCB_I2C_NV4E_BIT    = 0x04,
    DCB_I2C_NVIO_BIT    = 0x05,
    DCB_I2C_NVIO_AUX    = 0x06,
    DCB_I2C_PMGR        = 0x80,
    DCB_I2C_UNUSED      = 0xff
};

/* Parsed CCB / I2C Entry */
struct nvkm_dcb_i2c_entry {
    u8 type;
    u8 drive;   /* PMGR I2C port index (0x00d014 + drive * 0x20), or 0xff */
    u8 auxch;   /* PMGR DP AUX channel (0x00d900 + auxch * 0x50), or 0xff */
};

/* Parsed DCB Output + Physical Connector + Probed Monitor Info */
struct nvkm_dcb_parsed_output {
    u8   index;
    u8   type;          /* enum nvkm_dcb_output_type */
    u8   connector_idx;
    u8   connector_type;/* enum nvkm_dcb_conn_type */
    u8   i2c_index;
    u8   heads_mask;
    u8   or_index;      /* SOR / DAC index */
    u8   sor_link;
    u8   dp_link_bw;
    u8   dp_link_lanes;
    u8   hpd_pin;
    struct nvkm_dcb_i2c_entry i2c;

    /* Live Probed Status & EDID Summary */
    bool sink_connected;
    bool edid_valid;
    u8   edid[NVKM_EDID_SIZE];
    char monitor_name[16];
    u16  preferred_width;
    u16  preferred_height;
    u16  preferred_refresh_hz;
};

/* Complete Phase 2 VBIOS & Display Topology Report */
struct nvkm_bios_topology {
    u32  vbios_base;        /* NV_PRAMIN_BASE or NV_PROM_BASE */
    u8   version_major;
    u8   version_chip;
    u8   version_minor;
    u8   version_micro;
    u8   version_patch;
    u16  bit_offset;
    u16  dcb_offset;
    u8   dcb_version;
    u8   output_count;
    struct nvkm_dcb_parsed_output outputs[NVKM_MAX_DCB_OUTPUTS];
};

/* Core Phase 2 Functions */
const char *nvkm_dcb_output_type_name(u8 type);
const char *nvkm_dcb_conn_type_name(u8 conn_type);

bool nvkm_bios_parse_topology(const struct nvkm_osx_device *dev,
                              struct nvkm_bios_topology *topo,
                              bool probe_i2c_edid);

bool nvkm_gf119_i2c_read_edid(const struct nvkm_osx_device *dev,
                              u8 drive_port,
                              u8 out_edid[NVKM_EDID_SIZE]);

bool nvkm_gm200_aux_check_sink(const struct nvkm_osx_device *dev, u8 aux_ch);

bool nvkm_gm200_aux_read_edid(const struct nvkm_osx_device *dev,
                              u8 aux_ch,
                              u8 out_edid[NVKM_EDID_SIZE]);

bool nvkm_edid_parse_summary(const u8 edid[NVKM_EDID_SIZE],
                             char out_name[16],
                             u16 *out_width,
                             u16 *out_height,
                             u16 *out_refresh_hz);

#ifdef __cplusplus
}
#endif

#endif /* MAXWELL_BIOS_I2C_H */
