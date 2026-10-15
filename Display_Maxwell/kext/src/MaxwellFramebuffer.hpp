/*
 * MaxwellFramebuffer.hpp — Phase 3: Native IOFramebuffer Subclass for GTX 970 (GM204)
 */

#ifndef MAXWELL_FRAMEBUFFER_HPP
#define MAXWELL_FRAMEBUFFER_HPP

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOTimerEventSource.h>
#include "../include/gm204_regs.h"
#include "../include/nvkm_osx_shim.h"
#include "../include/MaxwellBiosI2C.h"
#include "../include/MaxwellEvoDisp.h"

#define MAXWELL_MAX_DISPLAY_MODES   12

struct MaxwellModeEntry {
    IODisplayModeID modeID;
    UInt32          width;
    UInt32          height;
    UInt32          refreshHz;
    UInt32          pitch;
    UInt32          flags;
};

class org_hackintosh_MaxwellFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(org_hackintosh_MaxwellFramebuffer)

private:
    IOPCIDevice            *fPCIDevice;
    IOMemoryMap            *fBAR0Map;
    IOMemoryMap            *fBAR1Map;
    IODeviceMemory         *fVRAMRangeMem;
    nvkm_osx_device         fNvDev;
    nvkm_bios_topology      fTopology;
    nvkm_evo_disp_state     fEvoState;

    UInt8                   fPrimaryHead;
    SInt8                   fPrimarySOR;
    SInt8                   fConnectedOutputIdx;
    UInt64                  fScanoutOffset;
    UInt64                  fCursorVRAMOffset;
    bool                    fControllerEnabled;
    bool                    fHwCursorInitialized;
    bool                    fCursorVisible;
    SInt32                  fCursorHotX;
    SInt32                  fCursorHotY;

    MaxwellModeEntry        fModes[MAXWELL_MAX_DISPLAY_MODES];
    UInt32                  fModeCount;
    IODisplayModeID         fCurrentModeID;
    IOIndex                 fCurrentDepth;

    IOTimerEventSource     *fVBlankTimer;
    IOFBInterruptProc       fVBLInterruptProc;
    OSObject               *fVBLInterruptTarget;
    void                   *fVBLInterruptRef;
    bool                    fVBLInterruptEnabled;

    void                    buildModeTable();
    void                    addUniqueMode(UInt32 w, UInt32 h, UInt32 hz, UInt32 flags);
    const MaxwellModeEntry *findModeByID(IODisplayModeID modeID) const;
    static void             vblankTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                    handleVBlankTick();

public:
    virtual bool            init(OSDictionary *dict = nullptr) override;
    virtual IOService      *probe(IOService *provider, SInt32 *score) override;
    virtual bool            start(IOService *provider) override;
    virtual void            stop(IOService *provider) override;
    virtual void            free() override;

    virtual IOReturn        enableController() override;
    virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory *getVRAMRange() override;
    virtual const char     *getPixelFormats() override;
    virtual IOItemCount     getDisplayModeCount() override;
    virtual IOReturn        getDisplayModes(IODisplayModeID *allDisplayModes) override;
    virtual IOReturn        getInformationForDisplayMode(IODisplayModeID mode,
                                                         IODisplayModeInformation *info) override;
    virtual UInt64          getPixelFormatForDisplayMode(IODisplayModeID mode, IOIndex depth) override;
    virtual IOReturn        getPixelInformation(IODisplayModeID mode, IOIndex depth,
                                                IOPixelAperture aperture,
                                                IOPixelInformation *pixelInfo) override;
    virtual IOReturn        getCurrentDisplayMode(IODisplayModeID *mode, IOIndex *depth) override;
    virtual IOReturn        setDisplayMode(IODisplayModeID mode, IOIndex depth) override;
    virtual IOReturn        setStartupDisplayMode(IODisplayModeID mode, IOIndex depth) override;
    virtual IOReturn        getStartupDisplayMode(IODisplayModeID *mode, IOIndex *depth) override;

    virtual IOItemCount     getConnectionCount() override;
    virtual IOReturn        getAttribute(IOSelect attr, uintptr_t *val) override;
    virtual IOReturn        setAttribute(IOSelect attr, uintptr_t val) override;
    virtual IOReturn        getAttributeForConnection(IOIndex conn, IOSelect attr, uintptr_t *val) override;
    virtual IOReturn        setAttributeForConnection(IOIndex conn, IOSelect attr, uintptr_t val) override;
    virtual bool            hasDDCConnect(IOIndex conn) override;
    virtual IOReturn        getDDCBlock(IOIndex conn, UInt32 blk, IOSelect type,
                                        IOOptionBits opts, UInt8 *data, IOByteCount *len) override;

    virtual IOReturn        setCursorImage(void *cursorImage) override;
    virtual IOReturn        setCursorState(SInt32 x, SInt32 y, bool visible) override;
    virtual IOReturn        registerForInterruptType(IOSelect type, IOFBInterruptProc proc,
                                                     OSObject *target, void *ref, void **intRef) override;
    virtual IOReturn        unregisterInterrupt(void *intRef) override;
    virtual IOReturn        setInterruptState(void *intRef, UInt32 state) override;
};

#endif /* MAXWELL_FRAMEBUFFER_HPP */
