/*
 * MaxwellFramebuffer.cpp — Phase 3: Native IOFramebuffer Subclass for GTX 970 (GM204)
 *
 * Connects macOS 14+ IOGraphicsFamily / WindowServer to the Linux Nouveau
 * GF119/GM200/GM204 EVO display engine (MaxwellEvoDisp) and VBIOS DCB/I2C/EDID.
 *
 * Activation:
 *   Pass `-maxwell_fb` in NVRAM boot-args to attach with IOProbeScore 60000
 *   as the active macOS IOFramebuffer display driver.
 */

#include "MaxwellFramebuffer.hpp"
#include <pexpert/pexpert.h>
#include <IOKit/IOWorkLoop.h>

#define super IOFramebuffer
OSDefineMetaClassAndStructors(org_hackintosh_MaxwellFramebuffer, IOFramebuffer)

bool org_hackintosh_MaxwellFramebuffer::init(OSDictionary *dictionary)
{
    if (!super::init(dictionary)) {
        return false;
    }
    fPCIDevice           = nullptr;
    fBAR0Map             = nullptr;
    fBAR1Map             = nullptr;
    fVRAMRangeMem        = nullptr;
    bzero(&fNvDev, sizeof(fNvDev));
    bzero(&fTopology, sizeof(fTopology));
    bzero(&fEvoState, sizeof(fEvoState));
    bzero(fModes, sizeof(fModes));

    fPrimaryHead         = 0;
    fPrimarySOR          = -1;
    fConnectedOutputIdx  = -1;
    fScanoutOffset       = 0;
    fCursorVRAMOffset    = 0;
    fControllerEnabled   = false;
    fHwCursorInitialized = false;
    fCursorVisible       = false;
    fCursorHotX          = 0;
    fCursorHotY          = 0;
    fModeCount           = 0;
    fCurrentModeID       = 1;
    fCurrentDepth        = 0;

    fVBlankTimer         = nullptr;
    fVBLInterruptProc    = nullptr;
    fVBLInterruptTarget  = nullptr;
    fVBLInterruptRef     = nullptr;
    fVBLInterruptEnabled = false;
    return true;
}

IOService *org_hackintosh_MaxwellFramebuffer::probe(IOService *provider, SInt32 *score)
{
    uint32_t fbFlag = 0;
    if (!PE_parse_boot_argn("-maxwell_fb", &fbFlag, sizeof(fbFlag))) {
        return nullptr;
    }

    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) {
        return nullptr;
    }

    IOService *res = super::probe(provider, score);
    if (res && score) {
        *score = 60000;
    }
    return res;
}

bool org_hackintosh_MaxwellFramebuffer::start(IOService *provider)
{
    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice) {
        return false;
    }

    fPCIDevice->setMemoryEnable(true);
    fPCIDevice->setBusMasterEnable(true);

    fBAR0Map = fPCIDevice->mapDeviceMemoryWithIndex(NV_PCI_BAR0_INDEX);
    if (!fBAR0Map) {
        nvkm_error("MaxwellFramebuffer: failed to map PCI BAR0\n");
        return false;
    }

    fBAR1Map = fPCIDevice->mapDeviceMemoryWithIndex(NV_PCI_BAR1_INDEX);
    if (!fBAR1Map) {
        nvkm_error("MaxwellFramebuffer: failed to map PCI BAR1 (VRAM aperture)\n");
        fBAR0Map->release();
        fBAR0Map = nullptr;
        return false;
    }

    fNvDev.mmio_base  = reinterpret_cast<volatile uint8_t *>(fBAR0Map->getVirtualAddress());
    fNvDev.mmio_phys  = fBAR0Map->getPhysicalAddress();
    fNvDev.mmio_size  = fBAR0Map->getLength();
    fNvDev.boot0      = nvkm_rd32(&fNvDev, NV_PMC_BOOT_0);
    fNvDev.chipset    = (fNvDev.boot0 & NV_PMC_BOOT_0_CHIPSET_MASK) >> NV_PMC_BOOT_0_CHIPSET_SHIFT;
    uint32_t vramMiB  = nvkm_rd32(&fNvDev, NV_PFB_CSTATUS);
    fNvDev.vram_bytes = static_cast<uint64_t>(vramMiB) << 20;

    /* Probe Phase 2 VBIOS DCB/I2C/EDID topology & Phase 3 EVO PDISP armed state */
    nvkm_bios_parse_topology(&fNvDev, &fTopology, true);
    nvkm_evo_probe_state(&fNvDev, &fEvoState);

    fPrimaryHead   = (fEvoState.primary_head >= 0) ? (UInt8)fEvoState.primary_head : 0;
    fPrimarySOR    = fEvoState.heads[fPrimaryHead].bound_sor;
    fScanoutOffset = fEvoState.heads[fPrimaryHead].fb_vram_offset;

    for (u8 i = 0; i < fTopology.output_count; ++i) {
        if (fTopology.outputs[i].edid_valid || fTopology.outputs[i].sink_connected) {
            fConnectedOutputIdx = (SInt8)i;
            break;
        }
    }

    /* Reserve top 64 KiB of BAR1 aperture for 64x64 ARGB8888 Hardware Cursor */
    UInt64 bar1Len = fBAR1Map->getLength();
    if (bar1Len >= 0x100000ULL) {
        fCursorVRAMOffset = (bar1Len - 0x10000ULL) & ~0xffULL;
    }

    buildModeTable();

    setProperty("IOFBDependentID", fPCIDevice->getRegistryEntryID(), 64);
    setProperty("IOFBDependentIndex", (UInt64)0, 32);
    setProperty("model", "NVIDIA GeForce GTX 970 (Maxwell GM204 EVO)");

void org_hackintosh_MaxwellFramebuffer::stop(IOService *provider)
{
    if (fVBlankTimer) {
        fVBlankTimer->cancelTimeout();
        IOWorkLoop *wl = getWorkLoop();
        if (wl) wl->removeEventSource(fVBlankTimer);
        fVBlankTimer->release();
        fVBlankTimer = nullptr;
    }
    if (fVRAMRangeMem) {
        fVRAMRangeMem->release();
        fVRAMRangeMem = nullptr;
    }
    if (fBAR1Map) {
        fBAR1Map->release();
        fBAR1Map = nullptr;
    }
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = nullptr;
    }
    fNvDev.mmio_base = nullptr;
    super::stop(provider);
}

void org_hackintosh_MaxwellFramebuffer::free()
{
    if (fVRAMRangeMem) {
        fVRAMRangeMem->release();
        fVRAMRangeMem = nullptr;
    }
    if (fBAR1Map) {
        fBAR1Map->release();
        fBAR1Map = nullptr;
    }
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = nullptr;
    }
    super::free();
}

void org_hackintosh_MaxwellFramebuffer::addUniqueMode(UInt32 w, UInt32 h, UInt32 hz, UInt32 flags)
{
    if (w < 640 || h < 480 || fModeCount >= MAXWELL_MAX_DISPLAY_MODES) return;
    if (hz == 0) hz = 60;

    for (UInt32 i = 0; i < fModeCount; ++i) {
        if (fModes[i].width == w && fModes[i].height == h && fModes[i].refreshHz == hz) {
            fModes[i].flags |= flags;
            return;
        }
    }

    MaxwellModeEntry &m = fModes[fModeCount];
    m.modeID    = (IODisplayModeID)(fModeCount + 1);
    m.width     = w;
    m.height    = h;
    m.refreshHz = hz;
    m.pitch     = (w * 4U + 63U) & ~63U; /* 64-byte aligned pitch for GF119/GM204 */
    m.flags     = flags;
    fModeCount++;
}

void org_hackintosh_MaxwellFramebuffer::buildModeTable()
{
    fModeCount = 0;
    const UInt32 kDefaultFlags = kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag;
    const UInt32 kStdFlags     = kDisplayModeValidFlag | kDisplayModeSafeFlag;

    /* 1. Active GOP / EVO Head Armed Mode (highest priority as boot mode) */
    const nvkm_evo_head_state &hs = fEvoState.heads[fPrimaryHead];
    if (hs.active && hs.width >= 640 && hs.height >= 480) {
        addUniqueMode(hs.width, hs.height, hs.refresh_hz, kDefaultFlags | kDisplayModeNativeFlag);
    }

    /* 2. Native Preferred Timing Mode from Phase 2 Monitor EDID (DTD) */
    if (fConnectedOutputIdx >= 0) {
        const nvkm_dcb_parsed_output &out = fTopology.outputs[fConnectedOutputIdx];
        if (out.edid_valid && out.preferred_width >= 640 && out.preferred_height >= 480) {
            addUniqueMode(out.preferred_width, out.preferred_height,
                          out.preferred_refresh_hz ? out.preferred_refresh_hz : 60,
                          kDefaultFlags | kDisplayModeNativeFlag);
        }
    }

    /* 3. Standard Desktop Resolutions (720p, 1080p@60/120/144, 1440p@60/120/144, 4K@60) */
    addUniqueMode(1920, 1080,  60, (fModeCount == 0) ? kDefaultFlags : kStdFlags);
    addUniqueMode(1920, 1080, 120, kStdFlags);
    addUniqueMode(1920, 1080, 144, kStdFlags);
    addUniqueMode(2560, 1440,  60, kStdFlags);
    addUniqueMode(2560, 1440, 120, kStdFlags);
    addUniqueMode(2560, 1440, 144, kStdFlags);
    addUniqueMode(3840, 2160,  60, kStdFlags);
    addUniqueMode(1680, 1050,  60, kStdFlags);
    addUniqueMode(1600,  900,  60, kStdFlags);
    addUniqueMode(1280,  720,  60, kStdFlags);

    fCurrentModeID = 1;
    fCurrentDepth  = 0;
}

const MaxwellModeEntry *org_hackintosh_MaxwellFramebuffer::findModeByID(IODisplayModeID modeID) const
{
    for (UInt32 i = 0; i < fModeCount; ++i) {
        if (fModes[i].modeID == modeID) {
            return &fModes[i];
        }
    }
    return nullptr;
}

IOReturn org_hackintosh_MaxwellFramebuffer::enableController()
{
    if (fControllerEnabled) {
        return kIOReturnSuccess;
    }

    /* Disable GK104/GM204 RG underflow reporting per Linux gf119_disp_init */
    nvkm_evo_disable_underflow(&fNvDev, fEvoState.head_mask);

    /* Initialize GF119/GM204 Hardware Cursor PIO channel if BAR1 space is available */
    if (fCursorVRAMOffset != 0) {
        fHwCursorInitialized = nvkm_evo_cursor_init(&fNvDev, fPrimaryHead, fCursorVRAMOffset);
    }

    /* Setup VBlank timer event source for WindowServer VSync pacing */
    IOWorkLoop *wl = getWorkLoop();
    if (wl && !fVBlankTimer) {
        fVBlankTimer = IOTimerEventSource::timerEventSource(
            this, &org_hackintosh_MaxwellFramebuffer::vblankTimerFired);
        if (fVBlankTimer) {
            wl->addEventSource(fVBlankTimer);
            fVBlankTimer->setTimeoutMS(16);
        }
    }

    fControllerEnabled = true;
    nvkm_info("MaxwellFramebuffer controller enabled on HEAD-%u (SOR=%d, modes=%u, hwCursor=%d)\n",
              fPrimaryHead, fPrimarySOR, (unsigned)fModeCount, fHwCursorInitialized ? 1 : 0);
    return kIOReturnSuccess;
}

    setProperty("VRAM,totalMB", vramMiB, 32);


IODeviceMemory *org_hackintosh_MaxwellFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    if (aperture != kIOFBSystemAperture || !fBAR1Map) {
        return nullptr;
    }

    const MaxwellModeEntry *m = findModeByID(fCurrentModeID);
    UInt64 fbBytes = m ? ((UInt64)m->pitch * (UInt64)m->height) : (1920ULL * 1080ULL * 4ULL);
    IOPhysicalAddress physBase = fBAR1Map->getPhysicalAddress() + (IOPhysicalAddress)fScanoutOffset;

    return IODeviceMemory::withSubRange(
        fPCIDevice->getDeviceMemoryWithIndex(NV_PCI_BAR1_INDEX),
        (IOPhysicalAddress)fScanoutOffset,
        (IOByteCount)fbBytes);
    (void)physBase;
}

IODeviceMemory *org_hackintosh_MaxwellFramebuffer::getVRAMRange()
{
    if (!fVRAMRangeMem && fPCIDevice) {
        IODeviceMemory *bar1 = fPCIDevice->getDeviceMemoryWithIndex(NV_PCI_BAR1_INDEX);
        if (bar1) {
            bar1->retain();
            fVRAMRangeMem = bar1;
        }
    }
    if (fVRAMRangeMem) {
        fVRAMRangeMem->retain();
    }
    return fVRAMRangeMem;
}

const char *org_hackintosh_MaxwellFramebuffer::getPixelFormats()
{
    static const char kFormats[] = IO32BitDirectPixels "\0\0";
    return kFormats;
}

IOItemCount org_hackintosh_MaxwellFramebuffer::getDisplayModeCount()
{
    return fModeCount;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    if (!allDisplayModes) {
        return kIOReturnBadArgument;
    }
    for (UInt32 i = 0; i < fModeCount; ++i) {
        allDisplayModes[i] = fModes[i].modeID;
    }
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getInformationForDisplayMode(
    IODisplayModeID displayMode,
    IODisplayModeInformation *info)
{
    if (!info) {
        return kIOReturnBadArgument;
    }
    const MaxwellModeEntry *m = findModeByID(displayMode);
    if (!m) {
        return kIOReturnUnsupportedMode;
    }

    bzero(info, sizeof(*info));
    info->nominalWidth  = m->width;
    info->nominalHeight = m->height;
    info->refreshRate   = (m->refreshHz << 16); /* 16.16 fixed point Hz */
    info->maxDepthIndex = 0;
    info->flags         = m->flags;
    if (fHwCursorInitialized) {
        info->imageWidth  = NVKM_EVO_CURSOR_SIZE;
        info->imageHeight = NVKM_EVO_CURSOR_SIZE;
    }
    return kIOReturnSuccess;
}

UInt64 org_hackintosh_MaxwellFramebuffer::getPixelFormatForDisplayMode(
    IODisplayModeID displayMode,
    IOIndex depth)
{
    (void)displayMode;
    (void)depth;
    return 0;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getPixelInformation(
    IODisplayModeID displayMode,
    IOIndex depth,
    IOPixelAperture aperture,
    IOPixelInformation *pixelInfo)
{
    if (!pixelInfo || aperture != kIOFBSystemAperture || depth != 0) {
        return kIOReturnBadArgument;
    }
    const MaxwellModeEntry *m = findModeByID(displayMode);
    if (!m) {
        return kIOReturnUnsupportedMode;
    }

    bzero(pixelInfo, sizeof(*pixelInfo));
    pixelInfo->bytesPerRow       = m->pitch;
    pixelInfo->bytesPerPlane     = m->pitch * m->height;
    pixelInfo->bitsPerPixel      = 32;
    pixelInfo->pixelType         = kIORGBDirectPixels;
    pixelInfo->componentCount    = 3;
    pixelInfo->bitsPerComponent  = 8;
    pixelInfo->componentMasks[0] = 0x00ff0000U; /* R */
    pixelInfo->componentMasks[1] = 0x0000ff00U; /* G */
    pixelInfo->componentMasks[2] = 0x000000ffU; /* B */
    strlcpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof(pixelInfo->pixelFormat));
    pixelInfo->activeWidth       = m->width;
    pixelInfo->activeHeight      = m->height;
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getCurrentDisplayMode(
    IODisplayModeID *displayMode,
    IOIndex *depth)
{
    if (displayMode) *displayMode = fCurrentModeID;
    if (depth)       *depth       = fCurrentDepth;
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::setDisplayMode(
    IODisplayModeID displayMode,
    IOIndex depth)
{
    const MaxwellModeEntry *m = findModeByID(displayMode);
    if (!m || depth != 0) {
        return kIOReturnUnsupportedMode;
    }

    nvkm_evo_head_set_scanout(&fNvDev, fPrimaryHead, fScanoutOffset,
                              (u16)m->width, (u16)m->height, m->pitch);
    fCurrentModeID = displayMode;
    fCurrentDepth  = depth;

    nvkm_info("setDisplayMode: %ux%u @ %uHz (pitch=%u)\n",
              (unsigned)m->width, (unsigned)m->height, (unsigned)m->refreshHz, (unsigned)m->pitch);
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::setStartupDisplayMode(
    IODisplayModeID displayMode,
    IOIndex depth)
{
    return setDisplayMode(displayMode, depth);
}

IOReturn org_hackintosh_MaxwellFramebuffer::getStartupDisplayMode(
    IODisplayModeID *displayMode,
    IOIndex *depth)
{
    if (displayMode) *displayMode = 1;
    if (depth)       *depth       = 0;

IOItemCount org_hackintosh_MaxwellFramebuffer::getConnectionCount()
{
    return 1;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getAttribute(IOSelect attr, uintptr_t *val)
{
    if (attr == kIOHardwareCursorAttribute) {
        if (val) *val = fHwCursorInitialized ? 1 : 0;
        return kIOReturnSuccess;
    }
    return super::getAttribute(attr, val);
}

IOReturn org_hackintosh_MaxwellFramebuffer::setAttribute(IOSelect attr, uintptr_t val)
{
    return super::setAttribute(attr, val);
}

IOReturn org_hackintosh_MaxwellFramebuffer::getAttributeForConnection(
    IOIndex conn,
    IOSelect attr,
    uintptr_t *val)
{
    if (conn != 0) return kIOReturnBadArgument;

    switch (attr) {
    case kConnectionEnable:
    case kConnectionCheckEnable:
        if (val) *val = 1;
        return kIOReturnSuccess;
    case kConnectionSupportsHLDDCSense:
    case kConnectionSupportsAppleSense:
        if (val) *val = (fConnectedOutputIdx >= 0) ? 1 : 0;
        return kIOReturnSuccess;
    default:
        return super::getAttributeForConnection(conn, attr, val);
    }
}

IOReturn org_hackintosh_MaxwellFramebuffer::setAttributeForConnection(
    IOIndex conn,
    IOSelect attr,
    uintptr_t val)
{
    if (conn != 0) return kIOReturnBadArgument;

    if (attr == kConnectionPower) {
        if (fPrimarySOR >= 0) {
            nvkm_evo_sor_set_power(&fNvDev, (u8)fPrimarySOR, val != 0);
        }
        return kIOReturnSuccess;
    }
    return super::setAttributeForConnection(conn, attr, val);
}

bool org_hackintosh_MaxwellFramebuffer::hasDDCConnect(IOIndex conn)
{
    if (conn != 0 || fConnectedOutputIdx < 0) {
        return false;
    }
    return fTopology.outputs[fConnectedOutputIdx].edid_valid;
}

IOReturn org_hackintosh_MaxwellFramebuffer::getDDCBlock(
    IOIndex conn,
    UInt32 blk,
    IOSelect type,
    IOOptionBits opts,
    UInt8 *data,
    IOByteCount *len)
{
    (void)opts;
    if (conn != 0 || blk != 1 || type != kIODDCBlockTypeEDID || !data || !len) {
        return kIOReturnUnsupported;
    }
    if (fConnectedOutputIdx < 0 || !fTopology.outputs[fConnectedOutputIdx].edid_valid) {
        return kIOReturnNoDevice;
    }
    if (*len < NVKM_EDID_SIZE) {
        return kIOReturnNoSpace;
    }

    bcopy(fTopology.outputs[fConnectedOutputIdx].edid, data, NVKM_EDID_SIZE);
    *len = NVKM_EDID_SIZE;
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::setCursorImage(void *cursorImage)
{
    if (!fHwCursorInitialized || !fBAR1Map || !cursorImage) {
        return kIOReturnUnsupported;
    }

    IOHardwareCursorDescriptor desc;
    bzero(&desc, sizeof(desc));
    desc.majorVersion = kHardwareCursorDescriptorMajorVersion;
    desc.minorVersion = kHardwareCursorDescriptorMinorVersion;
    desc.width        = NVKM_EVO_CURSOR_SIZE;
    desc.height       = NVKM_EVO_CURSOR_SIZE;
    desc.bitDepth     = 32;

    uint32_t cursorBuf[NVKM_EVO_CURSOR_SIZE * NVKM_EVO_CURSOR_SIZE];
    bzero(cursorBuf, sizeof(cursorBuf));

    IOHardwareCursorInfo info;
    bzero(&info, sizeof(info));
    info.majorVersion       = kHardwareCursorInfoMajorVersion;
    info.minorVersion       = kHardwareCursorInfoMinorVersion;
    info.hardwareCursorData = reinterpret_cast<UInt8 *>(cursorBuf);

    if (!convertCursorImage(cursorImage, &desc, &info)) {
        return kIOReturnUnsupported;
    }

    fCursorHotX = info.hotSpotX;
    fCursorHotY = info.hotSpotY;

    volatile uint8_t *bar1Virt = reinterpret_cast<volatile uint8_t *>(fBAR1Map->getVirtualAddress());
    nvkm_evo_cursor_upload_argb(bar1Virt, fBAR1Map->getLength(), fCursorVRAMOffset,
                                cursorBuf, NVKM_EVO_CURSOR_SIZE, NVKM_EVO_CURSOR_SIZE);
    return kIOReturnSuccess;
}


IOReturn org_hackintosh_MaxwellFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible)
{
    if (!fHwCursorInitialized) {
        return kIOReturnUnsupported;
    }
    fCursorVisible = visible;
    nvkm_evo_cursor_set_pos(&fNvDev, fPrimaryHead,
                            (s16)(x - fCursorHotX), (s16)(y - fCursorHotY), visible);
    return kIOReturnSuccess;
}

IOReturn org_hackintosh_MaxwellFramebuffer::registerForInterruptType(
    IOSelect type,
    IOFBInterruptProc proc,
    OSObject *target,
    void *ref,
    void **intRef)
{
    if (type == kIOFBVBLInterruptType) {
        fVBLInterruptProc    = proc;
        fVBLInterruptTarget  = target;
        fVBLInterruptRef     = ref;
        fVBLInterruptEnabled = true;
        if (intRef) *intRef  = &fVBLInterruptProc;
        return kIOReturnSuccess;
    }
    return super::registerForInterruptType(type, proc, target, ref, intRef);
}

IOReturn org_hackintosh_MaxwellFramebuffer::unregisterInterrupt(void *intRef)
{
    if (intRef == &fVBLInterruptProc) {
        fVBLInterruptEnabled = false;
        fVBLInterruptProc    = nullptr;
        return kIOReturnSuccess;
    }
    return super::unregisterInterrupt(intRef);
}

IOReturn org_hackintosh_MaxwellFramebuffer::setInterruptState(void *intRef, UInt32 state)
{
    if (intRef == &fVBLInterruptProc) {
        fVBLInterruptEnabled = (state != 0);
        return kIOReturnSuccess;
    }
    return super::setInterruptState(intRef, state);
}

void org_hackintosh_MaxwellFramebuffer::vblankTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    (void)sender;
    org_hackintosh_MaxwellFramebuffer *fb = OSDynamicCast(org_hackintosh_MaxwellFramebuffer, owner);
    if (fb) {
        fb->handleVBlankTick();
    }
}

void org_hackintosh_MaxwellFramebuffer::handleVBlankTick()
{
    u16 scanline = 0;
    nvkm_evo_vblank_poll(&fNvDev, fPrimaryHead, &scanline);

    if (fVBLInterruptEnabled && fVBLInterruptProc) {
        fVBLInterruptProc(fVBLInterruptTarget, fVBLInterruptRef);
    }

    if (fVBlankTimer) {
        const MaxwellModeEntry *m = findModeByID(fCurrentModeID);
        UInt32 hz = (m && m->refreshHz > 0) ? m->refreshHz : 60;
        UInt32 ms = (1000U / hz);
        if (ms < 4) ms = 4;
        fVBlankTimer->setTimeoutMS(ms);
    }
}

    return kIOReturnSuccess;
}

    if (!super::start(provider)) {
        return false;
    }
    return true;
}
