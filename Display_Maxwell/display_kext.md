# Display_Maxwell — Log

Log of changes, attempts and modifications for the Maxwell GPU display-driver work.

## Checkpoints

### 2026-10-13
- Initial setup: created `Display_Maxwell/` folder and `display_kext.md`.

### 2026-10-15
- **Phase 0 & Phase 1 Foundation — Linux `nvkm` (`GM204`) Source Extraction, Firmware & IOKit Kext Skeleton**:
  - Designed a 5-phase architecture plan (`docs/ARCHITECTURE_PLAN.md`) to port Linux Nouveau (`drivers/gpu/drm/nouveau/nvkm/`) modules for NVIDIA GeForce GTX 970 (`GM204` / `NV124`, PCI ID `10de:13c2`) to macOS 14+ (`IOKit`).
  - Downloaded 38 upstream Linux kernel `nvkm` reference files into `linux_ref/nvkm/` covering `GM204`/`GM200`/`GF119`:
    - `engine/device/` (`base.c`, `priv.h`, `user.c` — `NV124` descriptor & BAR0 layout)
    - `engine/disp/` (`gm200.c`, `gf119.c`, `base.c`, `head.c`, `ior.c`, `outp.c`, `priv.h` — EVO display engine, SOR/DP/HDMI, hardware cursor)
    - `engine/fifo/` & `engine/gr/` (`gm200.c`, `gf100.c`, `gf100.h`, `base.c`)
    - `subdev/bios/` (`base.c`, `dcb.c`, `conn.c`, `init.c` — VBIOS & DCB connector parser)
    - `subdev/i2c/` (`gm200.c`, `base.c` — DDC/I2C & DisplayPort AUX for EDID)
    - `subdev/fb/`, `subdev/mc/`, `subdev/pmu/`, `subdev/acr/`, `subdev/therm/`, `subdev/timer/`
  - Downloaded all 15 official `linux-firmware` redistributable Falcon/ACR & PGRAPH blobs (`acr/bl.bin`, `ucode_load.bin`, `ucode_unload.bin`, and `gr/fecs_*`, `gpccs_*`, `sw_*`) into `firmware/gm204/`.
  - Created the Linux-to-IOKit compatibility shim (`kext/include/nvkm_osx_shim.h`) and GM204 MMIO register map (`kext/include/gm204_regs.h`).
  - Implemented the Phase 1 IOKit kernel extension skeleton (`kext/Info.plist`, `kext/src/MaxwellProbe.hpp`, `kext/src/MaxwellProbe.cpp`) gated behind the `-maxwell_drv` boot-arg for safe testing:
    - Maps `BAR0` (`16 MiB` MMIO), reads `NV_PMC_BOOT_0` (`0x124000a1`), `NV_PFB_CSTATUS` (VRAM MiB), `NV_PTHERM_TEMP_CELSIUS` (die temperature), `NV_PDISP_CAP_HEADS`, and VBIOS/DCB header from `NV_PRAMIN`/`NV_PROM`, publishing live properties to `IORegistry`.
  - Added user-space diagnostic utility (`tools/maxwell_info.c`), host simulation test (`tools/test_shim_host.c`), and macOS 14+ build script (`build/Makefile`).
- **Phase 2 Implementation — VBIOS BIT/DCB v4.1 Connector & CCB/I2C Parser + PMGR I2C/DP-AUX EDID Reader**:
  - Downloaded 4 additional Linux Nouveau reference sources (`subdev/bios/i2c.c`, `subdev/bios/bit.c`, `subdev/i2c/busgf119.c`, `subdev/i2c/auxgm200.c`) into `linux_ref/nvkm/` (total: 42 reference files).
  - Created `kext/include/MaxwellBiosI2C.h` and `kext/src/MaxwellBiosI2C.cpp` porting:
    - VBIOS `BIT` `'i'` version parser and `DCB v4.1` (`0x4edcbdcb`) output entry parser (`dcb_outp_parse`: `type`, `or_index`, `sor_link`, `heads_mask`, `dp_link_bw`, `dp_link_lanes`).
    - Physical connector table parser (`nvbios_connEp`: `DVI-I`, `DVI-D`, `HDMI-A`, `DisplayPort`, `HPD` pin mapping).
    - CCB/I2C table v4.1 parser (`dcb_i2c_parse`: `DCB_I2C_PMGR` drive port and DisplayPort `auxch` channel mapping).
    - PMGR I2C bit-banging engine (`nvkm_gf119_i2c_read_edid` at `0x00d014 + drive * 0x20`) and GM200 DP AUX channel sink detector / EDID reader (`nvkm_gm200_aux_check_sink`, `nvkm_gm200_aux_read_edid` at `0x00d900 + aux * 0x50`).
    - EDID 1.3/1.4 checksum, monitor name (`0xFC` descriptor) and preferred Detailed Timing Descriptor (`DTD` width, height, refresh rate) decoder (`nvkm_edid_parse_summary`).
  - Integrated Phase 2 into `MaxwellProbe.hpp` and `MaxwellProbe.cpp` (`publishPhase2Topology`), publishing `NVKM_VBIOS_Version`, `NVKM_DCB_OutputCount`, and the `NVKM_DCB_Outputs` array (including per-port `EDID`, `MonitorName`, `PreferredWidth`, `PreferredHeight`, `PreferredRefreshHz`) into `IORegistry`.
  - Updated `build/Makefile` and `tools/test_shim_host.c` to build and test both Phase 1 and Phase 2.


