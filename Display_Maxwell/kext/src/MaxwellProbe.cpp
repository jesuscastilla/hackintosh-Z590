/*
 * MaxwellProbe.cpp — Phase 1 IOKit Driver for NVIDIA GeForce GTX 970 (GM204)
 *
 * Ports core hardware probing routines from Linux drivers/gpu/drm/nouveau/nvkm/:
 *   - engine/device/base.c (NV_PMC_BOOT_0 chipset identification)
 *   - subdev/fb/ramgm200.c (NV_PFB_CSTATUS VRAM capacity detection)
 *   - subdev/therm/gf119.c (NV_PTHERM_TEMP_CELSIUS thermal sensor readout)
 *   - subdev/bios/base.c   (NV_PROM / NV_PRAMIN VBIOS header & DCB table offset)
 *
 * Safety: Only attaches if boot-arg `-maxwell_drv` is present in NVRAM boot-args.
 */

#include "MaxwellProbe.hpp"
#include <pexpert/pexpert.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>

#define super IOService
OSDefineMetaClassAndStructors(org_hackintosh_MaxwellProbe, IOService)

bool org_hackintosh_MaxwellProbe::init(OSDictionary *dictionary)
{
    if (!super::init(dictionary)) {
        return false;
    }
    fPCIDevice = nullptr;
    fBAR0Map   = nullptr;
    bzero(&fNvDev, sizeof(fNvDev));
    bzero(&fTopology, sizeof(fTopology));
    return true;
}

IOService *org_hackintosh_MaxwellProbe::probe(IOService *provider, SInt32 *score)
{
    uint32_t drvFlag = 0;
    if (!PE_parse_boot_argn("-maxwell_drv", &drvFlag, sizeof(drvFlag))) {
        nvkm_info("probe skipped: add '-maxwell_drv' to boot-args to enable NVDAMaxwell\n");
        return nullptr;
    }

    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) {
        return nullptr;
    }

    uint16_t vendor = pci->configRead16(kIOPCIConfigVendorID);
    uint16_t device = pci->configRead16(kIOPCIConfigDeviceID);
    nvkm_info("probe matched PCI %04x:%04x\n", vendor, device);

    return super::probe(provider, score);
}

bool org_hackintosh_MaxwellProbe::start(IOService *provider)
{
    if (!super::start(provider)) {
        return false;
    }

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice) {
        return false;
    }

    fPCIDevice->setMemoryEnable(true);
    fPCIDevice->setBusMasterEnable(true);

    fBAR0Map = fPCIDevice->mapDeviceMemoryWithIndex(NV_PCI_BAR0_INDEX);
    if (!fBAR0Map) {
        nvkm_error("failed to map PCI BAR0!\n");
        return false;
    }

    fNvDev.mmio_base = reinterpret_cast<volatile uint8_t *>(fBAR0Map->getVirtualAddress());
    fNvDev.mmio_phys = fBAR0Map->getPhysicalAddress();
    fNvDev.mmio_size = fBAR0Map->getLength();

    nvkm_info("BAR0 mapped: phys=0x%llx size=0x%llx\n", fNvDev.mmio_phys, fNvDev.mmio_size);

    if (!probeHardwareGM204()) {
        nvkm_error("hardware probe failed\n");
        fBAR0Map->release();
        fBAR0Map = nullptr;
        return false;
    }

    registerService();
    return true;
}

bool org_hackintosh_MaxwellProbe::probeHardwareGM204()
{
    fNvDev.boot0   = nvkm_rd32(&fNvDev, NV_PMC_BOOT_0);
    fNvDev.chipset = (fNvDev.boot0 & NV_PMC_BOOT_0_CHIPSET_MASK) >> NV_PMC_BOOT_0_CHIPSET_SHIFT;

    const char *chipName = "Unknown";
    if (fNvDev.chipset == NV_CHIPSET_GM204) {
        chipName = "GM204 (Maxwell 2.0 - GTX 970/980)";
    } else if (fNvDev.chipset == NV_CHIPSET_GM200) {
        chipName = "GM200 (Maxwell 2.0 - GTX 980 Ti/Titan X)";
    } else if (fNvDev.chipset == NV_CHIPSET_GM206) {
        chipName = "GM206 (Maxwell 2.0 - GTX 950/960)";
    }

    uint32_t vramMiB     = nvkm_rd32(&fNvDev, NV_PFB_CSTATUS);
    fNvDev.vram_bytes    = static_cast<uint64_t>(vramMiB) << 20;
    uint32_t tempCelsius = nvkm_rd32(&fNvDev, NV_PTHERM_TEMP_CELSIUS) & 0x1ffU;
    uint32_t pmcEnable   = nvkm_rd32(&fNvDev, NV_PMC_ENABLE);
    uint32_t dispHeads   = nvkm_rd32(&fNvDev, NV_PDISP_CAP_HEADS) & 0xfU;

    bool vbiosOk = nvkm_bios_parse_topology(&fNvDev, &fTopology, true);

    nvkm_info("Detected %s | BOOT_0=0x%08x | VRAM=%u MiB | Temp=%u C | VBIOS=%s (DCB=0x%04x v%02x outputs=%u)\n",
              chipName, fNvDev.boot0, vramMiB, tempCelsius,
              vbiosOk ? "OK" : "NOT_FOUND",
              fTopology.dcb_offset, fTopology.dcb_version, fTopology.output_count);

    setProperty("NVKM_Architecture", chipName);
    setProperty("NVKM_PMC_BOOT_0", fNvDev.boot0, 32);
    setProperty("NVKM_ChipsetID", fNvDev.chipset, 32);
    setProperty("NVKM_VRAM_MiB", vramMiB, 32);
    setProperty("NVKM_Temp_Celsius", tempCelsius, 32);
    setProperty("NVKM_PMC_Enable", pmcEnable, 32);
    setProperty("NVKM_PDISP_HeadsMask", dispHeads, 32);
    setProperty("NVKM_VBIOS_Valid", vbiosOk);
    setProperty("NVKM_VBIOS_DCB_Offset", fTopology.dcb_offset, 16);
    setProperty("NVKM_VBIOS_DCB_Version", fTopology.dcb_version, 8);
    setProperty("NVKM_DCB_OutputCount", fTopology.output_count, 8);

    if (vbiosOk) {
        publishPhase2Topology();
    }

    if (nvkm_evo_probe_state(&fNvDev, &fEvoState)) {
        publishPhase3EvoState();
    }
    return true;
}

void org_hackintosh_MaxwellProbe::publishPhase3EvoState()
{
    u8 ph = (fEvoState.primary_head >= 0) ? (u8)fEvoState.primary_head : 0;
    const nvkm_evo_head_state &hs = fEvoState.heads[ph];

    nvkm_info("EVO PDISP state: primaryHead=%u active=%d res=%ux%u@%uHz pitch=%u fbOff=0x%llx SOR=%d\n",
              ph, hs.active ? 1 : 0, hs.width, hs.height, hs.refresh_hz,
              hs.pitch, hs.fb_vram_offset, hs.bound_sor);

    setProperty("NVKM_EVO_HeadMask", fEvoState.head_mask, 8);
    setProperty("NVKM_EVO_PrimaryHead", ph, 8);
    setProperty("NVKM_EVO_ActiveWidth", hs.width, 16);
    setProperty("NVKM_EVO_ActiveHeight", hs.height, 16);
    setProperty("NVKM_EVO_ActiveRefreshHz", hs.refresh_hz, 16);
    setProperty("NVKM_EVO_ActivePitch", hs.pitch, 32);
    setProperty("NVKM_EVO_ScanoutOffset", hs.fb_vram_offset, 64);
    setProperty("NVKM_EVO_BoundSOR", (uint32_t)(hs.bound_sor >= 0 ? hs.bound_sor : 0xff), 8);
}

void org_hackintosh_MaxwellProbe::publishPhase2Topology()
{
    char verStr[32];
    snprintf(verStr, sizeof(verStr), "%02x.%02x.%02x.%02x.%02x",
             fTopology.version_major, fTopology.version_chip,
             fTopology.version_minor, fTopology.version_micro, fTopology.version_patch);
    setProperty("NVKM_VBIOS_Version", verStr);

    OSArray *outArray = OSArray::withCapacity(fTopology.output_count);
    if (!outArray) return;

    for (u8 i = 0; i < fTopology.output_count; ++i) {
        const nvkm_dcb_parsed_output &o = fTopology.outputs[i];
        OSDictionary *dict = OSDictionary::withCapacity(12);
        if (!dict) continue;

        OSString *typeStr = OSString::withCString(nvkm_dcb_output_type_name(o.type));
        OSString *connStr = OSString::withCString(nvkm_dcb_conn_type_name(o.connector_type));
        OSNumber *orNum   = OSNumber::withNumber(o.or_index, 8);
        OSNumber *i2cNum  = OSNumber::withNumber(o.i2c.drive, 8);
        OSNumber *auxNum  = OSNumber::withNumber(o.i2c.auxch, 8);

        if (typeStr) { dict->setObject("OutputType", typeStr); typeStr->release(); }
        if (connStr) { dict->setObject("ConnectorType", connStr); connStr->release(); }
        if (orNum)   { dict->setObject("SOR_Index", orNum); orNum->release(); }
        if (i2cNum)  { dict->setObject("I2C_Port", i2cNum); i2cNum->release(); }
        if (auxNum)  { dict->setObject("DP_AUX_Ch", auxNum); auxNum->release(); }
        dict->setObject("SinkConnected", o.sink_connected ? kOSBooleanTrue : kOSBooleanFalse);

        if (o.edid_valid) {
            OSData *edidData = OSData::withBytes(o.edid, NVKM_EDID_SIZE);
            OSString *monStr = OSString::withCString(o.monitor_name);
            OSNumber *wNum   = OSNumber::withNumber(o.preferred_width, 16);
            OSNumber *hNum   = OSNumber::withNumber(o.preferred_height, 16);
            OSNumber *hzNum  = OSNumber::withNumber(o.preferred_refresh_hz, 16);
            if (edidData) { dict->setObject("EDID", edidData); edidData->release(); }
            if (monStr)   { dict->setObject("MonitorName", monStr); monStr->release(); }
            if (wNum)     { dict->setObject("PreferredWidth", wNum); wNum->release(); }
            if (hNum)     { dict->setObject("PreferredHeight", hNum); hNum->release(); }
            if (hzNum)    { dict->setObject("PreferredRefreshHz", hzNum); hzNum->release(); }
        }

        outArray->setObject(dict);
        dict->release();
    }

    setProperty("NVKM_DCB_Outputs", outArray);
    outArray->release();
}

void org_hackintosh_MaxwellProbe::stop(IOService *provider)
{
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = nullptr;
    }
    fNvDev.mmio_base = nullptr;
    super::stop(provider);
}

void org_hackintosh_MaxwellProbe::free()
{
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = nullptr;
    }
    super::free();
}
