/*
 * maxwell_info.c — User-space diagnostic CLI for NVDAMaxwell.kext on macOS 14+
 *
 * Queries IORegistry for the `org_hackintosh_MaxwellProbe` service and prints
 * all hardware telemetry read from the GTX 970 (GM204) via Linux nvkm logic.
 *
 * Compile on macOS:
 *   clang -Wall -O2 maxwell_info.c -framework IOKit -framework CoreFoundation -o maxwell_info
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

static void printIntProp(io_service_t service, CFStringRef key, const char *label, bool hex)
{
    CFTypeRef ref = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0);
    if (ref && CFGetTypeID(ref) == CFNumberGetTypeID()) {
        uint64_t val = 0;
        CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt64Type, &val);
        if (hex) {
            printf("  %-24s : 0x%08llx\n", label, (unsigned long long)val);
        } else {
            printf("  %-24s : %llu\n", label, (unsigned long long)val);
        }
    }
    if (ref) CFRelease(ref);
}

int main(void)
{
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("org_hackintosh_MaxwellProbe"));

    if (!service) {
        printf("[maxwell_info] NVDAMaxwell.kext service not found in IORegistry.\n");
        printf("  -> Ensure NVDAMaxwell.kext is loaded and '-maxwell_drv' is set in boot-args.\n");
        return 1;
    }

    printf("=== NVDAMaxwell.kext — GTX 970 (GM204) Live Telemetry ===\n");
    printIntProp(service, CFSTR("NVKM_PMC_BOOT_0"),        "NV_PMC_BOOT_0",        true);
    printIntProp(service, CFSTR("NVKM_ChipsetID"),         "Chipset ID (NV124)",   true);
    printIntProp(service, CFSTR("NVKM_VRAM_MiB"),          "Detected VRAM (MiB)",  false);
    printIntProp(service, CFSTR("NVKM_Temp_Celsius"),      "GPU Core Temp (C)",    false);
    printIntProp(service, CFSTR("NVKM_PMC_Enable"),        "NV_PMC_ENABLE",        true);
    printIntProp(service, CFSTR("NVKM_PDISP_HeadsMask"),   "PDISP Heads Mask",     true);
    printIntProp(service, CFSTR("NVKM_VBIOS_DCB_Offset"),  "VBIOS DCB Offset",     true);
    printIntProp(service, CFSTR("NVKM_VBIOS_DCB_Version"), "VBIOS DCB Version",    true);
    printIntProp(service, CFSTR("NVKM_DCB_OutputCount"),   "DCB Output Count",     false);
    printIntProp(service, CFSTR("NVKM_EVO_PrimaryHead"),   "EVO Primary Head",     false);
    printIntProp(service, CFSTR("NVKM_EVO_ActiveWidth"),   "EVO Active Width",     false);
    printIntProp(service, CFSTR("NVKM_EVO_ActiveHeight"),  "EVO Active Height",    false);
    printIntProp(service, CFSTR("NVKM_EVO_ActiveRefreshHz"),"EVO Active RefreshHz",false);
    printIntProp(service, CFSTR("NVKM_EVO_ActivePitch"),   "EVO Active Pitch",     false);
    printIntProp(service, CFSTR("NVKM_EVO_ScanoutOffset"), "EVO Scanout Offset",   true);
    printIntProp(service, CFSTR("NVKM_EVO_BoundSOR"),      "EVO Bound SOR",        false);

    IOObjectRelease(service);
    return 0;
}
#else
int main(void)
{
    printf("maxwell_info is designed to be compiled and run on macOS 14+.\n");
    return 0;
}
#endif
