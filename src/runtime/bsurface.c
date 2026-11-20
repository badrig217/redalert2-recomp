/* BSurface::Get_Pixel, native fast path (run_lift.py bsurface_fast_patches).
 * Shared by every host (Windows, Linux, macOS, the web): the patch calls it from
 * the lifted code whichever host links it. */
#include <stdint.h>

#include "recomp_target.h"

/* Guest addresses as host pointers: the same on Windows; through win32hle
 * off it, where on a 64-bit host (macOS) the guest's memory is one block. */
#ifdef _WIN32
#define GUEST_PTR(va) ((void *)(uintptr_t)(uint32_t)(va))
#else
#include "win32hle.h"
#define GUEST_PTR(va) hle_ptr((uint32_t)(va))
#endif
#define RD32(va) (*(volatile uint32_t *)GUEST_PTR(va))
#define RD16(va) (*(volatile uint16_t *)GUEST_PTR(va))
#define RD8(va)  (*(volatile uint8_t *)GUEST_PTR(va))

/* ---- BSurface pixel read, native fast path ------------------------------
 * 0x007BAE60 (Surface slot 10, Get_Pixel(POINT*)) is what the voxel plotter
 * (0x00756860) calls twice per sample. Run as lifted it is six virtual
 * calls, each a dispatch through the recomp: Lock (which calls Bytes_Per_Pixel
 * and Get_Pitch, which calls Get_Width), Bytes_Per_Pixel again, Unlock.
 * For a plain BSurface those are field reads: bpp [this+0x10], buffer
 * [this+0x14], width [this+4], and the lock count [this+0xC] up and down
 * again. Only when the vtable still holds BSurface's own methods for slots
 * 23, 24, 28, 29 and 31 (XSurface and DSurface override some) does this
 * answer; otherwise it returns 0 and the lifted code runs unchanged. A null
 * buffer also falls through, so the lifted code keeps its own behaviour.
 * run_lift.py bsurface_fast_patches. game.exe (Red Alert 2) has the same
 * methods 0x280 lower; its BSurface vtable is 0x0079AEF8 (gamemd 0x007E2070). */
#ifdef RA2_TARGET_GAME
#define BSURF_LOCK   0x00411370u   /* slot 23 */
#define BSURF_UNLOCK 0x004112F0u   /* slot 24 */
#define BSURF_BPP    0x004113B0u   /* slot 28 */
#define BSURF_PITCH  0x004113C0u   /* slot 29 */
#define BSURF_WIDTH  0x004112C0u   /* slot 31, Surface's */
#else
#define BSURF_LOCK   0x004115F0u
#define BSURF_UNLOCK 0x00411570u
#define BSURF_BPP    0x00411630u
#define BSURF_PITCH  0x00411640u
#define BSURF_WIDTH  0x00411540u
#endif
int ra2_bsurf_getpixel(uint32_t self, uint32_t pt, uint32_t* out) {
    uint32_t vt = RD32(self), bpp, base, addr;
    if (RD32(vt + 0x5C) != BSURF_LOCK || RD32(vt + 0x60) != BSURF_UNLOCK ||
        RD32(vt + 0x70) != BSURF_BPP || RD32(vt + 0x74) != BSURF_PITCH ||
        RD32(vt + 0x7C) != BSURF_WIDTH)
        return 0;
    base = RD32(self + 0x14);
    if (!base) return 0;
    bpp = RD32(self + 0x10);
    addr = base + RD32(pt) * bpp + RD32(pt + 4) * (RD32(self + 4) * bpp);
    *out = bpp == 2 ? (uint32_t)RD16(addr) : (uint32_t)RD8(addr);
    return 1;
}
