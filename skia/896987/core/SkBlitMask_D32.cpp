/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Sk4px.h"
#include "SkBlitMask.h"
#include "SkColor.h"
#include "SkColorData.h"
#include "SkOpts.h"

SkBlitMask::BlitLCD16RowProc SkBlitMask::BlitLCD16RowFactory(bool isOpaque) {
    BlitLCD16RowProc proc = PlatformBlitRowProcs16(isOpaque);
    if (proc) {
        return proc;
    }

    if (isOpaque) {
        return  SkBlitLCD16OpaqueRow;
    } else {
        return  SkBlitLCD16Row;
    }
}

static void D32_LCD16_Proc(void* SK_RESTRICT dst, size_t dstRB,
                           const void* SK_RESTRICT mask, size_t maskRB,
                           SkColor color, int width, int height) {

    SkPMColor*        dstRow = (SkPMColor*)dst;
    const uint16_t* srcRow = (const uint16_t*)mask;
    SkPMColor       opaqueDst;

    SkBlitMask::BlitLCD16RowProc proc = nullptr;
    bool isOpaque = (0xFF == SkColorGetA(color));
    proc = SkBlitMask::BlitLCD16RowFactory(isOpaque);
    SkASSERT(proc != nullptr);

    if (isOpaque) {
        opaqueDst = SkPreMultiplyColor(color);
    } else {
        opaqueDst = 0;  // ignored
    }

    do {
        proc(dstRow, srcRow, color, width, opaqueDst);
        dstRow = (SkPMColor*)((char*)dstRow + dstRB);
        srcRow = (const uint16_t*)((const char*)srcRow + maskRB);
    } while (--height != 0);
}

///////////////////////////////////////////////////////////////////////////////

bool SkBlitMask::BlitColor(const SkPixmap& device, const SkMask& mask,
                           const SkIRect& clip, SkColor color) {
    int x = clip.fLeft, y = clip.fTop;

    if (device.colorType() == kN32_SkColorType && mask.fFormat == SkMask::kA8_Format) {
        SkOpts::blit_mask_d32_a8(device.writable_addr32(x,y), device.rowBytes(),
                                 (const SkAlpha*)mask.getAddr(x,y), mask.fRowBytes,
                                 color, clip.width(), clip.height());
        return true;
    }

    if (device.colorType() == kN32_SkColorType && mask.fFormat == SkMask::kLCD16_Format) {
        // TODO: Is this reachable code?  Seems like no.
        D32_LCD16_Proc(device.writable_addr32(x,y), device.rowBytes(),
                       mask.getAddr(x,y), mask.fRowBytes,
                       color, clip.width(), clip.height());
        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static void A8_RowProc_Blend(
        SkPMColor* SK_RESTRICT dst, const void* maskIn, const SkPMColor* SK_RESTRICT src, int count) {
    const uint8_t* SK_RESTRICT mask = static_cast<const uint8_t*>(maskIn);

#ifndef SK_SUPPORT_LEGACY_A8_MASKBLITTER
    Sk4px::MapDstSrcAlpha(count, dst, src, mask,
        [](const Sk4px& d, const Sk4px& s, const Sk4px& aa) {
            const auto s_aa = s.approxMulDiv255(aa);
            return s_aa + d.approxMulDiv255(s_aa.alphas().inv());
        });
#else
    for (int i = 0; i < count; ++i) {
        if (mask[i]) {
            dst[i] = SkBlendARGB32(src[i], dst[i], mask[i]);
        }
    }
#endif
}

// expand the steps that SkAlphaMulQ performs, but this way we can
//  exand.. add.. combine
// instead of
// expand..combine add expand..combine
//
#define EXPAND0(v, m, s)    ((v) & (m)) * (s)
#define EXPAND1(v, m, s)    (((v) >> 8) & (m)) * (s)
#define COMBINE(e0, e1, m)  ((((e0) >> 8) & (m)) | ((e1) & ~(m)))

static void A8_RowProc_Opaque(
        SkPMColor* SK_RESTRICT dst, const void* maskIn, const SkPMColor* SK_RESTRICT src, int count) {
    const uint8_t* SK_RESTRICT mask = static_cast<const uint8_t*>(maskIn);

#ifndef SK_SUPPORT_LEGACY_A8_MASKBLITTER
    Sk4px::MapDstSrcAlpha(count, dst, src, mask,
        [](const Sk4px& d, const Sk4px& s, const Sk4px& aa) {
            return (s * aa + d * aa.inv()).div255();
        });
#else
    for (int i = 0; i < count; ++i) {
        int m = mask[i];
        if (m) {
            m += (m >> 7);
#if 1
            // this is slightly slower than the expand/combine version, but it
            // is much closer to the old results, so we use it for now to reduce
            // rebaselining.
            dst[i] = SkAlphaMulQ(src[i], m) + SkAlphaMulQ(dst[i], 256 - m);
#else
            uint32_t v = src[i];
            uint32_t s0 = EXPAND0(v, rbmask, m);
            uint32_t s1 = EXPAND1(v, rbmask, m);
            v = dst[i];
            uint32_t d0 = EXPAND0(v, rbmask, m);
            uint32_t d1 = EXPAND1(v, rbmask, m);
            dst[i] = COMBINE(s0 + d0, s1 + d1, rbmask);
#endif
        }
    }
#endif // SK_SUPPORT_LEGACY_A8_MASKBLITTER
}

static int upscale31To255(int value) {
    value = (value << 3) | (value >> 2);
    return value;
}

static int src_alpha_blend(int src, int dst, int srcA, int mask) {

    return dst + SkAlphaMul(src - SkAlphaMul(srcA, dst), mask);
}

static void LCD16_RowProc_Blend(
        SkPMColor* SK_RESTRICT dst, const void* maskIn, const SkPMColor* SK_RESTRICT src, int count) {
    const uint16_t* SK_RESTRICT mask = static_cast<const uint16_t*>(maskIn);
    for (int i = 0; i < count; ++i) {
        uint16_t m = mask[i];
        if (0 == m) {
            continue;
        }

        SkPMColor s = src[i];
        SkPMColor d = dst[i];

        int srcA = SkGetPackedA32(s);
        int srcR = SkGetPackedR32(s);
        int srcG = SkGetPackedG32(s);
        int srcB = SkGetPackedB32(s);

        srcA += srcA >> 7;

        /*  We want all of these in 5bits, hence the shifts in case one of them
         *  (green) is 6bits.
         */
        int maskR = SkGetPackedR16(m) >> (SK_R16_BITS - 5);
        int maskG = SkGetPackedG16(m) >> (SK_G16_BITS - 5);
        int maskB = SkGetPackedB16(m) >> (SK_B16_BITS - 5);

        maskR = upscale31To255(maskR);
        maskG = upscale31To255(maskG);
        maskB = upscale31To255(maskB);

        int dstR = SkGetPackedR32(d);
        int dstG = SkGetPackedG32(d);
        int dstB = SkGetPackedB32(d);

        // LCD blitting is only supported if the dst is known/required
        // to be opaque
        dst[i] = SkPackARGB32(0xFF,
                              src_alpha_blend(srcR, dstR, srcA, maskR),
                              src_alpha_blend(srcG, dstG, srcA, maskG),
                              src_alpha_blend(srcB, dstB, srcA, maskB));
    }
}

static void LCD16_RowProc_Opaque(
        SkPMColor* SK_RESTRICT dst, const void* maskIn, const SkPMColor* SK_RESTRICT src, int count) {
    const uint16_t* SK_RESTRICT mask = static_cast<const uint16_t*>(maskIn);
    for (int i = 0; i < count; ++i) {
        uint16_t m = mask[i];
        if (0 == m) {
            continue;
        }

        SkPMColor s = src[i];
        SkPMColor d = dst[i];

        int srcR = SkGetPackedR32(s);
        int srcG = SkGetPackedG32(s);
        int srcB = SkGetPackedB32(s);

        /*  We want all of these in 5bits, hence the shifts in case one of them
         *  (green) is 6bits.
         */
        int maskR = SkGetPackedR16(m) >> (SK_R16_BITS - 5);
        int maskG = SkGetPackedG16(m) >> (SK_G16_BITS - 5);
        int maskB = SkGetPackedB16(m) >> (SK_B16_BITS - 5);

        // Now upscale them to 0..32, so we can use blend32
        maskR = SkUpscale31To32(maskR);
        maskG = SkUpscale31To32(maskG);
        maskB = SkUpscale31To32(maskB);

        int dstR = SkGetPackedR32(d);
        int dstG = SkGetPackedG32(d);
        int dstB = SkGetPackedB32(d);

        // LCD blitting is only supported if the dst is known/required
        // to be opaque
        dst[i] = SkPackARGB32(0xFF,
                              SkBlend32(srcR, dstR, maskR),
                              SkBlend32(srcG, dstG, maskG),
                              SkBlend32(srcB, dstB, maskB));
    }
}

SkBlitMask::RowProc SkBlitMask::RowFactory(SkColorType ct,
                                           SkMask::Format format,
                                           RowFlags flags) {
// make this opt-in until chrome can rebaseline
    RowProc proc = PlatformRowProcs(ct, format, flags);
    if (proc) {
        return proc;
    }

    static const RowProc gProcs[] = {
        (RowProc)A8_RowProc_Blend,      (RowProc)A8_RowProc_Opaque,
        (RowProc)LCD16_RowProc_Blend,   (RowProc)LCD16_RowProc_Opaque,
    };

    switch (ct) {
        case kN32_SkColorType: {
            size_t index;
            switch (format) {
                case SkMask::kA8_Format:    index = 0; break;
                case SkMask::kLCD16_Format: index = 2; break;
                default:
                    return nullptr;
            }
            if (flags & kSrcIsOpaque_RowFlag) {
                index |= 1;
            }
            SkASSERT(index < SK_ARRAY_COUNT(gProcs));
            return gProcs[index];
        }
        default:
            break;
    }
    return nullptr;
}