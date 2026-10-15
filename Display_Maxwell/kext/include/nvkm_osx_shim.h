/*
 * nvkm_osx_shim.h — Linux Nouveau (nvkm) to macOS XNU/IOKit Compatibility Layer
 *
 * Allows C/C++ code ported from Linux drivers/gpu/drm/nouveau/nvkm/ to run
 * inside a macOS 14+ IOKit kernel extension (KEXT) with minimal source changes.
 */

#ifndef NVKM_OSX_SHIM_H
#define NVKM_OSX_SHIM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __APPLE__
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/OSByteOrder.h>
#endif

/* Linux Fixed-Width Integer Types */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

/* Linux Error Codes */
#ifndef EINVAL
#define EINVAL  22
#endif
#ifndef ENOMEM
#define ENOMEM  12
#endif
#ifndef ENODEV
#define ENODEV  19
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 60
#endif
#ifndef EBUSY
#define EBUSY   16
#endif

/* Bit Manipulation Helpers from Linux <linux/bitops.h> */
#ifndef BIT
#define BIT(nr)                 (1UL << (nr))
#endif
#ifndef BIT_ULL
#define BIT_ULL(nr)             (1ULL << (nr))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))
#endif

/* Minimal nvkm_device Wrapper representing a mapped NVIDIA GPU in IOKit */
struct nvkm_osx_device {
    volatile uint8_t *mmio_base;    /* Virtual address of mapped BAR0 */
    uint64_t          mmio_phys;    /* Physical address of BAR0 */
    uint64_t          mmio_size;    /* Size of BAR0 in bytes (typically 16 MiB) */
    uint32_t          boot0;        /* Cached NV_PMC_BOOT_0 */
    uint32_t          chipset;      /* e.g. 0x124 for GM204 */
    uint64_t          vram_bytes;   /* Total VRAM detected from NV_PFB_CSTATUS */
};

/* =========================================================================
 * MMIO Register Accessors (Equivalent to Linux nvkm_rd32 / nvkm_wr32 / nvkm_mask)
 * ========================================================================= */
static inline u8 nvkm_rd08(const struct nvkm_osx_device *dev, u32 addr)
{
    if (!dev || !dev->mmio_base || addr >= dev->mmio_size) {
        return 0xFF;
    }
    return dev->mmio_base[addr];
}

static inline void nvkm_wr08(const struct nvkm_osx_device *dev, u32 addr, u8 val)
{
    if (!dev || !dev->mmio_base || addr >= dev->mmio_size) {
        return;
    }
    dev->mmio_base[addr] = val;
}

static inline u16 nvkm_rd16(const struct nvkm_osx_device *dev, u32 addr)
{
    if (!dev || !dev->mmio_base || (addr + sizeof(u16)) > dev->mmio_size) {
        return 0xFFFF;
    }
#ifdef __APPLE__
    return OSReadLittleInt16((const volatile void *)dev->mmio_base, addr);
#else
    return *(const volatile u16 *)(dev->mmio_base + addr);
#endif
}

static inline void nvkm_wr16(const struct nvkm_osx_device *dev, u32 addr, u16 val)
{
    if (!dev || !dev->mmio_base || (addr + sizeof(u16)) > dev->mmio_size) {
        return;
    }
#ifdef __APPLE__
    OSWriteLittleInt16((volatile void *)dev->mmio_base, addr, val);
#else
    *(volatile u16 *)(dev->mmio_base + addr) = val;
#endif
}

static inline u32 nvkm_rd32(const struct nvkm_osx_device *dev, u32 addr)
{
    if (!dev || !dev->mmio_base || (addr + sizeof(u32)) > dev->mmio_size) {
        return 0xFFFFFFFFU;
    }
#ifdef __APPLE__
    return OSReadLittleInt32((const volatile void *)dev->mmio_base, addr);
#else
    return *(const volatile u32 *)(dev->mmio_base + addr);
#endif
}

static inline void nvkm_wr32(const struct nvkm_osx_device *dev, u32 addr, u32 val)
{
    if (!dev || !dev->mmio_base || (addr + sizeof(u32)) > dev->mmio_size) {
        return;
    }
#ifdef __APPLE__
    OSWriteLittleInt32((volatile void *)dev->mmio_base, addr, val);
#else
    *(volatile u32 *)(dev->mmio_base + addr) = val;
#endif
}

static inline u32 nvkm_mask(const struct nvkm_osx_device *dev, u32 addr, u32 mask, u32 val)
{
    u32 temp = nvkm_rd32(dev, addr);
    nvkm_wr32(dev, addr, (temp & ~mask) | val);
    return temp;
}

/* =========================================================================
 * Timing & Logging Helpers
 * ========================================================================= */
#ifdef __APPLE__
#define nvkm_udelay(us)         IODelay(us)
#define nvkm_mdelay(ms)         IOSleep(ms)
#define nvkm_info(fmt, ...)     IOLog("[NVDAMaxwell INFO] " fmt, ##__VA_ARGS__)
#define nvkm_warn(fmt, ...)     IOLog("[NVDAMaxwell WARN] " fmt, ##__VA_ARGS__)
#define nvkm_error(fmt, ...)    IOLog("[NVDAMaxwell ERR ] " fmt, ##__VA_ARGS__)
#else
#include <stdio.h>
#define nvkm_udelay(us)         ((void)(us))
#define nvkm_mdelay(ms)         ((void)(ms))
#define nvkm_info(fmt, ...)     printf("[NVDAMaxwell INFO] " fmt, ##__VA_ARGS__)
#define nvkm_warn(fmt, ...)     printf("[NVDAMaxwell WARN] " fmt, ##__VA_ARGS__)
#define nvkm_error(fmt, ...)    printf("[NVDAMaxwell ERR ] " fmt, ##__VA_ARGS__)
#endif

#endif /* NVKM_OSX_SHIM_H */
