//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2015-2024 Fabian Greffrath
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2025 Julia Nechaevskaya
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	[crispy] Truecolor rendering
//

#pragma once

#include <stdint.h>
#include "doomtype.h"
#include "v_video.h" // pal_color[]

#include "config.h"


// TrueColor blending:
extern uint8_t additive_lut_32[511];
extern uint8_t shadow_alpha;
extern uint8_t fuzz_alpha;

extern void I_InitTCTransMaps (void);

// Paletted blending:
#define PAL_BITS 5   // 5: 32x32x32 (~32 KB). Сan use 6 (64^3 ~256 KB) for higher accuracy.
#define PAL_STEPS    (1 << PAL_BITS)
#define PAL_LUT_SIZE (1 << (3 * PAL_BITS)) // 2^(3*PAL_BITS)

extern byte *additive_lut_8;
extern byte *rgb_to_pal;

static inline byte RGB_TO_PAL(int r, int g, int b)
{
    const int qr = r >> (8 - PAL_BITS);
    const int qg = g >> (8 - PAL_BITS);
    const int qb = b >> (8 - PAL_BITS);
    const int idx = (qr << (2 * PAL_BITS)) | (qg << PAL_BITS) | qb;
    return rgb_to_pal[idx];
}

extern void I_InitPALTransMaps (void);

// Color corrections:
extern const int I_ShadeFactor[];
extern const float I_SaturationPercent[];
extern const double colorblind_matrix[][3][3];


// -----------------------------------------------------------------------------
//
//                    Blending lambdas with variable alphas
//
// -----------------------------------------------------------------------------

//
// I_BlendAdd
//

#define I_BlendAdd_32(bg, fg) ( \
    (0xFF000000U) | \
    (additive_lut_32[((bg) & 0xFF) + ((fg) & 0xFF)]) |                     \
    (additive_lut_32[(((bg) & 0xFF00) + ((fg) & 0xFF00)) >> 8] << 8) |     \
    (additive_lut_32[(((bg) & 0xFF0000) + ((fg) & 0xFF0000)) >> 16] << 16) \
)

#define I_BlendAdd_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ additive_lut_8[((((bg) & 0x00FF0000u) >> 8) | (((fg) & 0x00FF0000u) >> 16))], \
        /* G */ additive_lut_8[( (bg) & 0x0000FF00u)        | (((fg) & 0x0000FF00u) >> 8)  ], \
        /* B */ additive_lut_8[((((bg) & 0x000000FFu) << 8) |  ((fg) & 0x000000FFu)) ]        \
    ) ] \
)

//
// I_BlendOver
//

#define I_BlendOver_32(bg, fg, amount) ( \
    (0xFF000000U) | \
    ((((amount) * ((fg) & 0xFF00FF) + ((0xFFU - (amount)) * ((bg) & 0xFF00FF))) >> 8) & 0xFF00FF) | \
    ((((amount) * ((fg) & 0x00FF00) + ((0xFFU - (amount)) * ((bg) & 0x00FF00))) >> 8) & 0x00FF00)   \
)

#define I_BlendOver_8(bg, fg, amount) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((fg) & 0x00FF00FFu) * (uint32_t)(amount) + ((bg) & 0x00FF00FFu) * (0xFFu - (uint32_t)(amount))) >> 8) >> 16) & 0xFFu), \
        /* G */ ((((((fg) & 0x0000FF00u) * (uint32_t)(amount) + ((bg) & 0x0000FF00u) * (0xFFu - (uint32_t)(amount))) >> 8) >>  8) & 0xFFu), \
        /* B */ ((((((fg) & 0x00FF00FFu) * (uint32_t)(amount) + ((bg) & 0x00FF00FFu) * (0xFFu - (uint32_t)(amount))) >> 8)      ) & 0xFFu)  \
    ) ] \
)

//
// I_BlendDark
//

#define I_BlendDark_32(bg, amount) ( \
    ((0xFF000000U) | \
    (((((bg) & 0x00FF00FF) * (amount)) >> 8) & 0x00FF00FF) | \
    (((((bg) & 0x0000FF00) * (amount)) >> 8) & 0x0000FF00))  \
)

#define I_BlendDark_8(bg, amount) ( \
    pal_color[ RGB_TO_PAL( \
        /* R */ (((((bg) & 0x00FF0000u) >> 16) * ((uint32_t)(amount) + ((amount) >> 4)) + 0x80) >> 8) & 0xFFu, \
        /* G */ (((((bg) & 0x0000FF00u) >>  8) * ((uint32_t)(amount) + ((amount) >> 4)) + 0x80) >> 8) & 0xFFu, \
        /* B */ ((( (bg) & 0x000000FFu)        * ((uint32_t)(amount) + ((amount) >> 4)) + 0x80) >> 8) & 0xFFu  \
    ) ] \
)

//
// I_BlendDarkGrayscale
//

#define I_BlendDarkGrayscale_32(bg, amount) ( \
    ((0xFF000000U) | \
    ((((((bg & 0xFF) + ((bg >> 8) & 0xFF) + ((bg >> 16) & 0xFF)) * (amount)) >> 8) / 3) & 0xFF) * 0x010101U) \
)

#define I_BlendDarkGrayscale_8(bg, amount) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R=G=B=y */ \
        (((((77u   * (uint32_t)(amount)) >> 8) * (((bg) >> 16) & 0xFFu)) +       \
           (((150u * (uint32_t)(amount)) >> 8) * (((bg) >>  8) & 0xFFu)) +       \
           (((29u  * (uint32_t)(amount)) >> 8) * ( (bg)        & 0xFFu))) >> 8), \
        (((((77u   * (uint32_t)(amount)) >> 8) * (((bg) >> 16) & 0xFFu)) +       \
           (((150u * (uint32_t)(amount)) >> 8) * (((bg) >>  8) & 0xFFu)) +       \
           (((29u  * (uint32_t)(amount)) >> 8) * ( (bg)        & 0xFFu))) >> 8), \
        (((((77u   * (uint32_t)(amount)) >> 8) * (((bg) >> 16) & 0xFFu)) +       \
           (((150u * (uint32_t)(amount)) >> 8) * (((bg) >>  8) & 0xFFu)) +       \
           (((29u  * (uint32_t)(amount)) >> 8) * ( (bg)        & 0xFFu))) >> 8)  \
    ) ] \
)


// -----------------------------------------------------------------------------
//
//                     Blending lambdas with fixed alphas                       
//
// -----------------------------------------------------------------------------

//
// [PN] FUZZTL_ALPHA ≈ 64 (25% opacity)
//

#define I_BlendOver64_32(bg, fg) ( \
    (0xFF000000U) | \
    (((((fg) & 0xFF00FF) + ((bg) & 0xFF00FF) * 3) >> 2) & 0xFF00FF) | \
    (((((fg) & 0x00FF00) + ((bg) & 0x00FF00) * 3) >> 2) & 0x00FF00)   \
)

#define I_BlendOver64_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ (((((((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu) + ((fg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ (((((((bg) & 0x0000FF00u) << 1) + ((bg) & 0x0000FF00u) + ((fg) & 0x0000FF00u)) >> 2) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((bg)  & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu) + ((fg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu)        & 0xFFu  \
    ) ] \
)

//
// [PN] TINTTAB_ALPHA ≈ 96 (38% opacity)
//

#define I_BlendOver96_32(bg, fg) ( \
    (0xFF000000U) | \
    (((((fg) & 0xFF00FF) * 3 + ((bg) & 0xFF00FF) * 5) >> 3) & 0xFF00FF) | \
    (((((fg) & 0x00FF00) * 3 + ((bg) & 0x00FF00) * 5) >> 3) & 0x00FF00)   \
)

#define I_BlendOver96_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) << 1) + ((fg) & 0x00FF00FFu)) +                                    \
                     (((bg) & 0x00FF00FFu) << 2) + ((bg) & 0x00FF00FFu)) >> 3) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((((fg) & 0x0000FF00u) << 1) + ((fg) & 0x0000FF00u)) +                                    \
                     (((bg) & 0x0000FF00u) << 2) + ((bg) & 0x0000FF00u)) >> 3) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((((fg) & 0x00FF00FFu) << 1) + ((fg) & 0x00FF00FFu)) +                                    \
                     (((bg) & 0x00FF00FFu) << 2) + ((bg) & 0x00FF00FFu)) >> 3) & 0x00FF00FFu)      ) & 0xFFu  \
    ) ] \
)

//
// [PN] Fastest algorithm for 50% opacity
//

#define I_BlendOver128_32(bg, fg) ( \
    (0xFF000000U) |                                               \
    (((((fg) & 0xFF00FF) + ((bg) & 0xFF00FF)) >> 1) & 0xFF00FF) | \
    (((((fg) & 0x00FF00) + ((bg) & 0x00FF00)) >> 1) & 0x00FF00)   \
)

#define I_BlendOver128_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((fg) & 0x00FF00FFu) + ((bg) & 0x00FF00FFu)) >> 1) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((fg) & 0x0000FF00u) + ((bg) & 0x0000FF00u)) >> 1) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ (((((fg)  & 0x00FF00FFu) + ((bg) & 0x00FF00FFu)) >> 1) & 0x00FF00FFu) & 0xFFu         \
    ) ] \
)

//
// [PN] TINTTAB_ALPHA_ALT ≈ 142 (56% opacity)
//

#define I_BlendOver142_32(bg, fg) ( \
    (0xFF000000U) |                                                       \
    (((((fg) & 0xFF00FF) * 9 + ((bg) & 0xFF00FF) * 7) >> 4) & 0xFF00FF) | \
    (((((fg) & 0x00FF00) * 9 + ((bg) & 0x00FF00) * 7) >> 4) & 0x00FF00)   \
)

#define I_BlendOver142_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) << 3) + ((fg)  & 0x00FF00FFu)) +                                                                 \
                     (((bg) & 0x00FF00FFu) << 2) + (((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu)) >> 4) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((((fg) & 0x0000FF00u) << 3) + ((fg)  & 0x0000FF00u)) +                                                                 \
                     (((bg) & 0x0000FF00u) << 2) + (((bg) & 0x0000FF00u) << 1) + ((bg) & 0x0000FF00u)) >> 4) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((((fg) & 0x00FF00FFu) << 3) + ((fg)  & 0x00FF00FFu)) +                                                                 \
                     (((bg) & 0x00FF00FFu) << 2) + (((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu)) >> 4) & 0x00FF00FFu)      ) & 0xFFu  \
    ) ] \
)

//
// [PN] TRANMAP_ALPHA ≈ 168 (66% opacity)
//

#define I_BlendOver168_32(bg, fg) ( \
    (0xFF000000U) | \
    (((((fg) & 0xFF00FF) * 3 + ((bg) & 0xFF00FF)) >> 2) & 0xFF00FF) | \
    (((((fg) & 0x00FF00) * 3 + ((bg) & 0x00FF00)) >> 2) & 0x00FF00)   \
)

#define I_BlendOver168_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) * 3u) + ((bg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu) >> 16) & 0xFFu), \
        /* G */ ((((((((fg) & 0x0000FF00u) * 3u) + ((bg) & 0x0000FF00u)) >> 2) & 0x0000FF00u) >>  8) & 0xFFu), \
        /* B */ (((((((fg)  & 0x00FF00FFu) * 3u) + ((bg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu)       & 0xFFu)  \
    ) ] \
)


// -----------------------------------------------------------------------------
//
//     All original human-readable blending functions from Crispy and Inter
//
// -----------------------------------------------------------------------------

//
// I_BlendAdd
//

/*
const uint32_t I_BlendAdd (const uint32_t bg_i, const uint32_t fg_i)
{
    tcpixel_t bg, fg, ret;

    bg.i = bg_i;
    fg.i = fg_i;

    ret.a = 0xFFU;
    ret.r = additive_lut[bg.r][fg.r];
    ret.g = additive_lut[bg.g][fg.g];
    ret.b = additive_lut[bg.b][fg.b];

    return ret.i;
}

static inline pixel_t I_BlendAdd_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Additive via LUT (exact a=192/256), packed-index fast path.
    // Forms (bg<<8 | fg) per channel without extracting scalars.
    const uint8_t *const lut = additive_lut_8;

    // Indices: high byte = bg_chan, low byte = fg_chan
    const uint32_t idxR = ((bg & 0x00FF0000u) >> 8) | ((fg & 0x00FF0000u) >> 16);
    const uint32_t idxG =  (bg & 0x0000FF00u)       | ((fg & 0x0000FF00u) >> 8);
    const uint32_t idxB = ((bg & 0x000000FFu) << 8) |  (fg & 0x000000FFu);

    // Fast per-channel LUT (exact weight baked in), then palette map
    const int r = lut[idxR];
    const int g = lut[idxG];
    const int b = lut[idxB];

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL(r, g, b) ];
}

//
// I_BlendOver
//

const uint32_t I_BlendOver (const uint32_t bg_i, const uint32_t fg_i, const int amount)
{
    tcpixel_t bg, fg, ret;

    bg.i = bg_i;
    fg.i = fg_i;

    ret.a = 0xFFU;
    ret.r = (amount * fg.r + (0XFFU - amount) * bg.r) >> 8;
    ret.g = (amount * fg.g + (0XFFU - amount) * bg.g) >> 8;
    ret.b = (amount * fg.b + (0XFFU - amount) * bg.b) >> 8;

    return ret.i;
}

const pixel_t I_BlendOver_8 (pixel_t bg, pixel_t fg, int a)
{
    const int ia = 256 - a; // 0..256

    // Mix R|B together and G separately (using masks)
    const uint32_t bgRB = bg & 0x00FF00FFu;
    const uint32_t fgRB = fg & 0x00FF00FFu;
    const uint32_t bgG  = bg & 0x0000FF00u;
    const uint32_t fgG  = fg & 0x0000FF00u;

    const uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
    const uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

    const int r = (rb >> 16) & 0xFF;
    const int g = (g8 >> 8)  & 0xFF;
    const int b =  rb        & 0xFF;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL(r, g, b) ];
}

//
// I_BlendDark
//

const uint32_t I_BlendDark (const uint32_t bg_i, const int d)
{
    tcpixel_t bg, ret;

    bg.i = bg_i;

    ret.a = 0xFFU;
    ret.r = (bg.r * d) >> 8;
    ret.g = (bg.g * d) >> 8;
    ret.b = (bg.b * d) >> 8;

    return ret.i;
}

static inline pixel_t I_BlendDark_8 (uint32_t bg, int d)
{
    // [PN] Paletted darken: multiply RGB by factor d (0..255) and map via RGB->PAL LUT.

    // Scale packed R|B together and G separately
    const uint32_t rb = (((bg & 0x00FF00FFu) * (uint32_t)d) >> 8) & 0x00FF00FFu;
    const uint32_t g8 = (((bg & 0x0000FF00u) * (uint32_t)d) >> 8) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

//
// I_BlendDarkGrayscale
//

const uint32_t I_BlendDarkGrayscale (const uint32_t bg_i, const int d)
{
    tcpixel_t bg, ret;
    uint8_t r, g, b, gray;

    bg.i = bg_i;

    r = bg.r;
    g = bg.g;
    b = bg.b;
    // [PN] Do not use Rec. 601 formula here: 
    // gray = (((r * 299 + g * 587 + b * 114) / 1000) * d) >> 8;
    // Weights are equalized to balance all color contributions equally.
    gray = (((r + g + b) / 3) * d) >> 8;

    ret.a = 0xFFU;
    ret.r = ret.g = ret.b = gray;

    return ret.i;
}

static inline pixel_t I_BlendDarkGrayscale_8 (uint32_t bg, int d)
{
    // [PN] Paletted grayscale darken (exact-ish, still cheap).
    // Classic luma with integer weights: 77/150/29 (sum 256).
    // Fold 'd' into weights: Wr=(77*d)>>8, Wg=(150*d)>>8, Wb=(29*d)>>8.
    // => y = (Wr*R + Wg*G + Wb*B) >> 8

    const uint32_t r = (bg >> 16) & 0xFFu;
    const uint32_t g = (bg >>  8) & 0xFFu;
    const uint32_t b =  bg        & 0xFFu;

    const uint32_t Wr = (77u  * (uint32_t)d) >> 8;   // 0.299 * d
    const uint32_t Wg = (150u * (uint32_t)d) >> 8;   // 0.587 * d
    const uint32_t Wb = (29u  * (uint32_t)d) >> 8;   // 0.114 * d

    const uint32_t y = (Wr*r + Wg*g + Wb*b) >> 8;

    return pal_color[ RGB_TO_PAL(y, y, y) ];
}

// ----------------------------------------------
// Optimized paletted functions with fixed alphas
// ----------------------------------------------

static inline pixel_t I_BlendOver_64_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Fast approximation (1/4 instead of exact 64/256).

    // Pack R|B together (0x00FF00FF) and G separately (0x0000FF00)
    const uint32_t bgRB = bg & 0x00FF00FFu;
    const uint32_t fgRB = fg & 0x00FF00FFu;
    const uint32_t bgG  = bg & 0x0000FF00u;
    const uint32_t fgG  = fg & 0x0000FF00u;

    // Approximate weights: fg = 1/4, bg = 3/4  ->  (fg + 3*bg) >> 2
    // Use (x<<1)+x for *3 to avoid a general multiply (micro-optimization).
    const uint32_t rb = ((((bgRB << 1) + bgRB) + fgRB) >> 2) & 0x00FF00FFu;
    const uint32_t g8 = ((((bgG  << 1) + bgG ) + fgG ) >> 2) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

static inline pixel_t I_BlendOver_96_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Fast path for a = 96 (fg = 3/8, bg = 5/8). Exact weights via shifts/adds,
    // no general multiplies. Packs R|B and G separately; minimal temporaries.

    // Pack R|B together (0x00FF00FF) and G separately (0x0000FF00)
    const uint32_t bgRB = bg & 0x00FF00FFu;
    const uint32_t fgRB = fg & 0x00FF00FFu;
    const uint32_t bgG  = bg & 0x0000FF00u;
    const uint32_t fgG  = fg & 0x0000FF00u;

    // Exact: (3*fg + 5*bg) >> 3   because 96/256 = 3/8 and 160/256 = 5/8.
    // Use (x<<1)+x for *3 and (x<<2)+x for *5.
    const uint32_t rb = ((((fgRB << 1) + fgRB) + ((bgRB << 2) + bgRB)) >> 3) & 0x00FF00FFu;
    const uint32_t g8 = ((((fgG  << 1) + fgG ) + ((bgG  << 2) + bgG )) >> 3) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

static inline pixel_t I_BlendOver_128_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Paletted 50% over (fg/bg average). Cheapest possible:
    // per-channel average with packed RB and separate G; no multiplications.

    // Average R|B together and G separately; carries are fine (>>1), then mask lanes.
    const uint32_t rb = (((fg & 0x00FF00FFu) + (bg & 0x00FF00FFu)) >> 1) & 0x00FF00FFu;
    const uint32_t g8 = (((fg & 0x0000FF00u) + (bg & 0x0000FF00u)) >> 1) & 0x0000FF00u;

    // Map into 8-bit palette via 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

static inline pixel_t I_BlendOver_142_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Fast approximation for a=142 (fg ≈ 9/16, bg ≈ 7/16).

    // Pack R|B together (0x00FF00FF) and G separately (0x0000FF00)
    const uint32_t bgRB = bg & 0x00FF00FFu;
    const uint32_t fgRB = fg & 0x00FF00FFu;
    const uint32_t bgG  = bg & 0x0000FF00u;
    const uint32_t fgG  = fg & 0x0000FF00u;

    // Approx: (9*fg + 7*bg) >> 4
    // 9*x = (x<<3) + x;   7*x = (x<<2) + (x<<1) + x
    const uint32_t rb = ((((fgRB << 3) + fgRB) + ((bgRB << 2) + (bgRB << 1) + bgRB)) >> 4) & 0x00FF00FFu;
    const uint32_t g8 = ((((fgG  << 3) + fgG ) + ((bgG  << 2) + (bgG  << 1) + bgG )) >> 4) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

static inline pixel_t I_BlendOver_168_8 (uint32_t bg, uint32_t fg)
{
    // [PN] Fast approximation (3/4 instead of 168/256).

    // Mix R|B together (mask 0x00FF00FF) and G separately (mask 0x0000FF00)
    // Approximate weights: fg = 3/4, bg = 1/4 -> (3*fg + bg) >> 2
    const uint32_t rb = (((fg & 0x00FF00FFu) * 3u + (bg & 0x00FF00FFu)) >> 2) & 0x00FF00FFu;
    const uint32_t g8 = (((fg & 0x0000FF00u) * 3u + (bg & 0x0000FF00u)) >> 2) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

*/
