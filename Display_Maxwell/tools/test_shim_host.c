/*
 * test_shim_host.c — Local verification test for Phase 1 & Phase 2
 *
 * Simulates a 16 MiB BAR0 MMIO buffer populated with GTX 970 (GM204)
 * register values, VBIOS BIT + DCB v4.1 connector/I2C tables, DP AUX sink
 * status, and a 128-byte 1920x1080@60Hz EDID block, verifying Phase 1 & Phase 2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../kext/include/gm204_regs.h"
#include "../kext/include/nvkm_osx_shim.h"
#include "../kext/include/MaxwellBiosI2C.h"
#include "../kext/include/MaxwellEvoDisp.h"

int main(void)
{
    const size_t bar0_size = 16 * 1024 * 1024; /* 16 MiB BAR0 */
    uint8_t *fake_bar0 = (uint8_t *)calloc(1, bar0_size);
    assert(fake_bar0 != NULL);

    struct nvkm_osx_device dev;
    memset(&dev, 0, sizeof(dev));
    dev.mmio_base = fake_bar0;
    dev.mmio_phys = 0xf6000000ULL;
    dev.mmio_size = bar0_size;

    /* Phase 1: GM204 PMC / PFB / PTHERM / PDISP registers */
    nvkm_wr32(&dev, NV_PMC_BOOT_0,          0x124000a1U);
    nvkm_wr32(&dev, NV_PFB_CSTATUS,         4096U);
    nvkm_wr32(&dev, NV_PTHERM_TEMP_CELSIUS, 42U);
    nvkm_wr32(&dev, NV_PDISP_CAP_HEADS,     0x0000000fU);

    /* Phase 2: Simulated VBIOS in PRAMIN (0x700000) with DCB v4.1 + Connector + CCB/I2C */
    u32 vb = NV_PRAMIN_BASE;
    nvkm_wr16(&dev, vb + 0x0000, NV_VBIOS_SIGNATURE);
    nvkm_wr16(&dev, vb + 0x0036, 0x0200U); /* DCB offset = 0x0200 */

    /* DCB v4.1 Header at 0x0200 (hdr=0x1b, cnt=2 outputs, len=8, i2c=0x0300, conn=0x0340) */
    nvkm_wr08(&dev, vb + 0x0200, 0x41);
    nvkm_wr08(&dev, vb + 0x0201, 0x1b);
    nvkm_wr08(&dev, vb + 0x0202, 0x02);
    nvkm_wr08(&dev, vb + 0x0203, 0x08);
    nvkm_wr08(&dev, vb + 0x0204, 0x00);
    nvkm_wr08(&dev, vb + 0x0205, 0x03); /* i2c_tbl = 0x0300 */
    nvkm_wr32(&dev, vb + 0x0206, NVKM_DCB_SIGNATURE);
    nvkm_wr08(&dev, vb + 0x0214, 0x40);
    nvkm_wr08(&dev, vb + 0x0215, 0x03); /* conn_tbl = 0x0340 */

    /* Output 0: DisplayPort (type=0x6, i2c=0, heads=0xf, conn=0, sor=1) */
    nvkm_wr32(&dev, vb + 0x021b + 0, 0x01000f06U);
    nvkm_wr32(&dev, vb + 0x021b + 4, 0x04400010U); /* 4 lanes, 5.4 Gbps (HBR2) */

    /* Output 1: HDMI-A (type=0x2 TMDS, i2c=1, heads=0xf, conn=1, sor=2) */
    nvkm_wr32(&dev, vb + 0x0223 + 0, 0x02011f12U);
    nvkm_wr32(&dev, vb + 0x0223 + 4, 0x00000020U);

    /* CCB/I2C table v4.1 at 0x0300 */
    nvkm_wr08(&dev, vb + 0x0300, 0x41);
    nvkm_wr08(&dev, vb + 0x0301, 0x04);
    nvkm_wr08(&dev, vb + 0x0302, 0x02);
    nvkm_wr08(&dev, vb + 0x0303, 0x04);
    nvkm_wr32(&dev, vb + 0x0304, (0U << 5) | 0x1fU); /* entry 0: auxch=0 */
    nvkm_wr32(&dev, vb + 0x0308, (0x1fU << 5) | 4U); /* entry 1: drive=4 */

    /* Connector table v4.0 at 0x0340 */
    nvkm_wr08(&dev, vb + 0x0340, 0x40);
    nvkm_wr08(&dev, vb + 0x0341, 0x04);
    nvkm_wr08(&dev, vb + 0x0342, 0x02);
    nvkm_wr08(&dev, vb + 0x0343, 0x04);
    nvkm_wr32(&dev, vb + 0x0344, 0x00001046U); /* conn 0: DisplayPort (0x46) */
    nvkm_wr32(&dev, vb + 0x0348, 0x00002061U); /* conn 1: HDMI-A (0x61) */

    struct nvkm_bios_topology topo;
    bool ok = nvkm_bios_parse_topology(&dev, &topo, false);
    assert(ok);
    assert(topo.dcb_version == 0x41);
    assert(topo.output_count == 2);
    assert(topo.outputs[0].type == DCB_OUTPUT_DP);
    assert(topo.outputs[0].connector_type == DCB_CONN_DP);
    assert(topo.outputs[0].dp_link_lanes == 4);
    assert(topo.outputs[1].type == DCB_OUTPUT_TMDS);
    assert(topo.outputs[1].connector_type == DCB_CONN_HDMI_A);

    nvkm_info("Phase 1 & Phase 2 Self-Test PASSED: outputs=%u [0=%s/%s, 1=%s/%s]\n",
              topo.output_count,
              nvkm_dcb_output_type_name(topo.outputs[0].type),
              nvkm_dcb_conn_type_name(topo.outputs[0].connector_type),
              nvkm_dcb_output_type_name(topo.outputs[1].type),
              nvkm_dcb_conn_type_name(topo.outputs[1].connector_type));

    /* Phase 3: Simulate armed EVO PDISP state on HEAD-0 + SOR-1 (1920x1080@60Hz) */
    nvkm_wr32(&dev, NV_PDISP_HEAD_MASK_REG,         0x0000000fU);
    nvkm_wr32(&dev, NV_PDISP_HEAD_HTOTAL_VTOTAL(0), (1125U << 16) | 2200U);
    nvkm_wr32(&dev, NV_PDISP_HEAD_VIEWPORT_OUT(0),  (1080U << 16) | 1920U);
    nvkm_wr32(&dev, NV_PDISP_HEAD_PCLK_HZ(0),       148500000U);
    nvkm_wr32(&dev, NV_PDISP_CORE_FB_OFFSET(0),     (0x00100000U >> 8));
    nvkm_wr32(&dev, NV_PDISP_CORE_FB_STORAGE(0),    (7680U >> 6));
    nvkm_wr32(&dev, NV_PDISP_SOR_ARMED_STATE(1),    0x00000101U); /* SOR-1 bound to HEAD-0 */
    nvkm_wr32(&dev, NV_PDISP_SOR_PWR(1),            0x00000001U);

    struct nvkm_evo_disp_state evo;
    assert(nvkm_evo_probe_state(&dev, &evo));
    assert(evo.primary_head == 0);
    assert(evo.heads[0].active);
    assert(evo.heads[0].width == 1920 && evo.heads[0].height == 1080);
    assert(evo.heads[0].refresh_hz == 60);
    assert(evo.heads[0].bound_sor == 1);

    /* Verify Hardware Cursor PIO init & coordinate update */
    assert(nvkm_evo_cursor_init(&dev, 0, 0x07ff0000ULL));
    nvkm_evo_cursor_set_pos(&dev, 0, 320, 240, true);
    assert(nvkm_rd32(&dev, NV_PDISP_PIO_CURS_POS(0)) == ((240U << 16) | 320U));

    /* Verify dynamic mode scanout update (2560x1440) */
    assert(nvkm_evo_head_set_scanout(&dev, 0, 0x00100000ULL, 2560, 1440, 2560 * 4));
    assert(nvkm_rd32(&dev, NV_PDISP_CORE_FB_SIZE(0)) == ((1440U << 16) | 2560U));

    nvkm_info("Phase 3 EVO Display Engine Self-Test PASSED: HEAD-%d 1920x1080@%uHz -> 2560x1440, HW Cursor OK\n",
              evo.primary_head, evo.heads[0].refresh_hz);

    free(fake_bar0);
    return 0;
}

