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


/* [PN] Used by blending functions from Crispy Doom
typedef union
{
    uint32_t i;
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    } rgba; // [PN] Name for the structure to comply with C90/C99 standards
} tcpixel_t;
*/

extern uint8_t   additive_lut_32[511];
extern uint8_t   shadow_alpha;
extern uint8_t   fuzz_alpha;
extern void I_InitTCTransMaps (void);

#define PAL_BITS 5   // 5: 32x32x32 (~32 KB). Сan use 6 (64^3 ~256 KB) for higher accuracy.
#define PAL_STEPS    (1 << PAL_BITS)
#define PAL_LUT_SIZE (1 << (3 * PAL_BITS)) // 2^(3*PAL_BITS)
extern byte *rgb_to_pal;
static inline byte RGB_TO_PAL(int r, int g, int b)
{
    const int qr = r >> (8 - PAL_BITS);
    const int qg = g >> (8 - PAL_BITS);
    const int qb = b >> (8 - PAL_BITS);
    const int idx = (qr << (2 * PAL_BITS)) | (qg << PAL_BITS) | qb;
    return rgb_to_pal[idx];
}
extern byte *playpal_trans;
extern byte *additive_lut_8;
extern void I_InitPALTransMaps (void);

extern const int I_ShadeFactor[];
extern const float I_SaturationPercent[];
extern const double colorblind_matrix[][3][3];


// -----------------------------------------------------------------------------
//
//                        Main blending lambda-macroses
//
// -----------------------------------------------------------------------------

//
// I_BlendAdd
//

#define I_BlendAdd_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (additive_lut_32[((bg_i) & 0xFF) + ((fg_i) & 0xFF)]) | \
    (additive_lut_32[(((bg_i) & 0xFF00) + ((fg_i) & 0xFF00)) >> 8] << 8) | \
    (additive_lut_32[(((bg_i) & 0xFF0000) + ((fg_i) & 0xFF0000)) >> 16] << 16) \
)

#define I_BlendAdd_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ additive_lut_8[ ((((bg) & 0x00FF0000u) >> 8) | (((fg) & 0x00FF0000u) >> 16)) ], \
        /* G */ additive_lut_8[ ( (bg) & 0x0000FF00u)        | (((fg) & 0x0000FF00u) >> 8)   ], \
        /* B */ additive_lut_8[ ((((bg) & 0x000000FFu) << 8) |  ((fg) & 0x000000FFu)) ]         \
    ) ] \
)

//
// I_BlendOver
//

#define I_BlendOver_32(bg_i, fg_i, amount) ( \
    (0xFF000000U) | \
    ((((amount) * ((fg_i) & 0xFF00FF) + ((0xFFU - (amount)) * ((bg_i) & 0xFF00FF))) >> 8) & 0xFF00FF) | \
    ((((amount) * ((fg_i) & 0x00FF00) + ((0xFFU - (amount)) * ((bg_i) & 0x00FF00))) >> 8) & 0x00FF00) \
)

#define I_BlendOver_8(bg, fg, amount) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((fg) & 0x00FF00FFu) * (uint32_t)(amount) + ((bg) & 0x00FF00FFu) * (0xFFu - (uint32_t)(amount))) >> 8) >> 16) & 0xFFu), \
        /* G */ ((((((fg) & 0x0000FF00u) * (uint32_t)(amount) + ((bg) & 0x0000FF00u) * (0xFFu - (uint32_t)(amount))) >> 8) >>  8) & 0xFFu), \
        /* B */ ((((((fg) & 0x00FF00FFu) * (uint32_t)(amount) + ((bg) & 0x00FF00FFu) * (0xFFu - (uint32_t)(amount))) >> 8)      ) & 0xFFu) \
    ) ] \
)

//
// I_BlendDark
//

#define I_BlendDark_32(bg_i, d) ( \
    ((0xFF000000U) | \
    (((((bg_i) & 0x00FF00FF) * (d)) >> 8) & 0x00FF00FF) | \
    (((((bg_i) & 0x0000FF00) * (d)) >> 8) & 0x0000FF00)) \
)

#define I_BlendDark_8(bg_i, d) ( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((bg_i) & 0x00FF00FFu) * (uint32_t)(d)) >> 8) >> 16) & 0xFFu), \
        /* G */ ((((((bg_i) & 0x0000FF00u) * (uint32_t)(d)) >> 8) >>  8) & 0xFFu), \
        /* B */ (((((bg_i)  & 0x00FF00FFu) * (uint32_t)(d)) >> 8)        & 0xFFu)  \
    ) ] \
)

//
// I_BlendDarkGrayscale
//

#define I_BlendDarkGrayscale_32(bg_i, d) ( \
    ((0xFF000000U) | \
    ((((((bg_i & 0xFF) + ((bg_i >> 8) & 0xFF) + ((bg_i >> 16) & 0xFF)) * (d)) >> 8) / 3) & 0xFF) * 0x010101U) \
)

#define I_BlendDarkGrayscale_8(bg, d) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R=G=B=y */ \
        (((((77u   * (uint32_t)(d)) >> 8) * (((bg) >> 16) & 0xFFu)) + \
           (((150u * (uint32_t)(d)) >> 8) * (((bg) >>  8) & 0xFFu)) + \
           (((29u  * (uint32_t)(d)) >> 8) * ( (bg)        & 0xFFu))) >> 8), \
        (((((77u   * (uint32_t)(d)) >> 8) * (((bg) >> 16) & 0xFFu)) + \
           (((150u * (uint32_t)(d)) >> 8) * (((bg) >>  8) & 0xFFu)) + \
           (((29u  * (uint32_t)(d)) >> 8) * ( (bg)        & 0xFFu))) >> 8), \
        (((((77u   * (uint32_t)(d)) >> 8) * (((bg) >> 16) & 0xFFu)) + \
           (((150u * (uint32_t)(d)) >> 8) * (((bg) >>  8) & 0xFFu)) + \
           (((29u  * (uint32_t)(d)) >> 8) * ( (bg)        & 0xFFu))) >> 8) \
    ) ] \
)

// -----------------------------------------------------------------------------
//
//                       Blending lambdas with given values
//
// -----------------------------------------------------------------------------

//
// [PN] FUZZTL_ALPHA ≈ 64 (25% opacity)
//

#define I_BlendOver64_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF) * 3) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00) * 3) >> 2) & 0x00FF00) \
)

#define I_BlendOver64_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ (((((((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu) + ((fg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ (((((((bg) & 0x0000FF00u) << 1) + ((bg) & 0x0000FF00u) + ((fg) & 0x0000FF00u)) >> 2) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((bg)  & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu) + ((fg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu)        & 0xFFu \
    ) ] \
)

//
// [PN] TINTTAB_ALPHA ≈ 96 (38% opacity)
//

#define I_BlendOver96_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 3 + ((bg_i) & 0xFF00FF) * 5) >> 3) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 3 + ((bg_i) & 0x00FF00) * 5) >> 3) & 0x00FF00) \
)

#define I_BlendOver96_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) << 1) + ((fg) & 0x00FF00FFu)) + \
                     (((bg) & 0x00FF00FFu) << 2) + ((bg) & 0x00FF00FFu)) >> 3) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((((fg) & 0x0000FF00u) << 1) + ((fg) & 0x0000FF00u)) + \
                     (((bg) & 0x0000FF00u) << 2) + ((bg) & 0x0000FF00u)) >> 3) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((((fg) & 0x00FF00FFu) << 1) + ((fg) & 0x00FF00FFu)) + \
                     (((bg) & 0x00FF00FFu) << 2) + ((bg) & 0x00FF00FFu)) >> 3) & 0x00FF00FFu)      ) & 0xFFu \
    ) ] \
)

//
// [PN] Fastest algorithm for 50% opacity
//

#define I_BlendOver128_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF)) >> 1) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00)) >> 1) & 0x00FF00) \
)

#define I_BlendOver128_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((fg) & 0x00FF00FFu) + ((bg) & 0x00FF00FFu)) >> 1) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((fg) & 0x0000FF00u) + ((bg) & 0x0000FF00u)) >> 1) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ (((((fg) & 0x00FF00FFu) + ((bg) & 0x00FF00FFu)) >> 1) & 0x00FF00FFu) & 0xFFu \
    ) ] \
)

//
// [PN] TINTTAB_ALPHA_ALT ≈ 142 (56% opacity)
//

#define I_BlendOver142_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 9 + ((bg_i) & 0xFF00FF) * 7) >> 4) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 9 + ((bg_i) & 0x00FF00) * 7) >> 4) & 0x00FF00) \
)

#define I_BlendOver142_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) << 3) + ((fg)  & 0x00FF00FFu)) + \
                     (((bg) & 0x00FF00FFu) << 2) + (((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu)) >> 4) & 0x00FF00FFu) >> 16) & 0xFFu, \
        /* G */ ((((((((fg) & 0x0000FF00u) << 3) + ((fg)  & 0x0000FF00u)) + \
                     (((bg) & 0x0000FF00u) << 2) + (((bg) & 0x0000FF00u) << 1) + ((bg) & 0x0000FF00u)) >> 4) & 0x0000FF00u) >>  8) & 0xFFu, \
        /* B */ ((((((((fg) & 0x00FF00FFu) << 3) + ((fg)  & 0x00FF00FFu)) + \
                     (((bg) & 0x00FF00FFu) << 2) + (((bg) & 0x00FF00FFu) << 1) + ((bg) & 0x00FF00FFu)) >> 4) & 0x00FF00FFu)      ) & 0xFFu \
    ) ] \
)

//
// [PN] TRANMAP_ALPHA ≈ 168 (66% opacity)
//

#define I_BlendOver168_32(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 3 + ((bg_i) & 0xFF00FF)) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 3 + ((bg_i) & 0x00FF00)) >> 2) & 0x00FF00) \
)

#define I_BlendOver168_8(bg, fg) \
( \
    pal_color[ RGB_TO_PAL( \
        /* R */ ((((((((fg) & 0x00FF00FFu) * 3u) + ((bg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu) >> 16) & 0xFFu), \
        /* G */ ((((((((fg) & 0x0000FF00u) * 3u) + ((bg) & 0x0000FF00u)) >> 2) & 0x0000FF00u) >>  8) & 0xFFu), \
        /* B */ (((((((fg)  & 0x00FF00FFu) * 3u) + ((bg) & 0x00FF00FFu)) >> 2) & 0x00FF00FFu)       & 0xFFu)  \
    ) ] \
)
