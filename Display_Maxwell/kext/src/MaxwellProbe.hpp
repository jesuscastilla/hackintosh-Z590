/*
 * MaxwellProbe.hpp — Phase 1 & Phase 2 IOKit Driver for NVIDIA GeForce GTX 970 (GM204)
 *
 * Maps PCI BAR0 using Linux Nouveau (nvkm) register logic, identifies GM204 silicon,
 * reads VRAM size, GPU die temperature, parses VBIOS BIT/DCB v4.1 connector & I2C
 * topology, probes monitor EDIDs, and publishes live telemetry into the macOS IORegistry.
 */

#ifndef MAXWELL_PROBE_HPP
#define MAXWELL_PROBE_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include "../include/gm204_regs.h"
#include "../include/nvkm_osx_shim.h"
#include "../include/MaxwellBiosI2C.h"
#include "../include/MaxwellEvoDisp.h"

class org_hackintosh_MaxwellProbe : public IOService
{
    OSDeclareDefaultStructors(org_hackintosh_MaxwellProbe)

private:
    IOPCIDevice            *fPCIDevice;
    IOMemoryMap            *fBAR0Map;
    nvkm_osx_device         fNvDev;
    nvkm_bios_topology      fTopology;
    nvkm_evo_disp_state     fEvoState;

    bool                    probeHardwareGM204();
    void                    publishPhase2Topology();
    void                    publishPhase3EvoState();

public:
    virtual bool            init(OSDictionary *dictionary = nullptr) override;
    virtual IOService      *probe(IOService *provider, SInt32 *score) override;
    virtual bool            start(IOService *provider) override;
    virtual void            stop(IOService *provider) override;
    virtual void            free() override;
};

#endif /* MAXWELL_PROBE_HPP */

