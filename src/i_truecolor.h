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


// [PN] Converted functions to macros for optimization:
//
// 1. Eliminating function call overhead:
// - Macros replace function calls with "inlined" content directly at the point of use.
// - This removes the need for parameter passing via the stack and returning values, 
//   which is critical for performance-intensive tasks like graphics processing. 
//
// 2. Inline computations and compiler optimizations:
// - The compiler can better optimize the expanded macro code by reducing redundancy 
//   and reusing computed results.
// - This allows for improved loop unrolling, register allocation, and removal 
//   of unnecessary operations.
//
// 3. Bitwise operations and shifts:
// - Macros often leverage bitwise operations (&, |, >>, <<) to extract or manipulate
//   specific data (e.g., color channels).
// - These operations are extremely efficient at the processor level, as they are
//   handled directly by ALU (Arithmetic Logic Unit) without additional complexity.
// Example breakdown:
// - To extract the blue channel from a 32-bit integer bg_i, we use (bg_i & 0xFF).
//   This isolates the lowest 8 bits, which represent blue in ARGB.
// - To extract green, the integer is shifted right by 8 bits (bg_i >> 8) and masked
//   with 0xFF to isolate the next 8 bits ((bg_i >> 8) & 0xFF).
// - To construct a new color, the channels are calculated independently and then
//   combined using bitwise OR (|) and shifts (<<) to place them back in the correct position.
//
// 4. Scalability and readability:
// - While macros improve performance, they may sacrifice some readability and debugging
//   ease compared to functions.
// - It's essential to structure macros clearly and avoid redundant computations to maintain
//   balance between performance and maintainability.
// - All original human-readable blending functions from Crispy Doom are preserved
//   as commented examples in i_truecolor.c file.

#define I_BlendAdd(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (additive_lut_32[((bg_i) & 0xFF) + ((fg_i) & 0xFF)]) | \
    (additive_lut_32[(((bg_i) & 0xFF00) + ((fg_i) & 0xFF00)) >> 8] << 8) | \
    (additive_lut_32[(((bg_i) & 0xFF0000) + ((fg_i) & 0xFF0000)) >> 16] << 16) \
)

#define I_BlendDark(bg_i, d) ( \
    ((0xFF000000U) | \
    (((((bg_i) & 0x00FF00FF) * (d)) >> 8) & 0x00FF00FF) | \
    (((((bg_i) & 0x0000FF00) * (d)) >> 8) & 0x0000FF00)) \
)

#define I_BlendDarkGrayscale(bg_i, d) ( \
    ((0xFF000000U) | \
    ((((((bg_i & 0xFF) + ((bg_i >> 8) & 0xFF) + ((bg_i >> 16) & 0xFF)) * (d)) >> 8) / 3) & 0xFF) * 0x010101U) \
)

#define I_BlendOver(bg_i, fg_i, amount) ( \
    (0xFF000000U) | \
    ((((amount) * ((fg_i) & 0xFF00FF) + ((0xFFU - (amount)) * ((bg_i) & 0xFF00FF))) >> 8) & 0xFF00FF) | \
    ((((amount) * ((fg_i) & 0x00FF00) + ((0xFFU - (amount)) * ((bg_i) & 0x00FF00))) >> 8) & 0x00FF00) \
)

// -----------------------------------------------------------------------------
// TrueColor blending helper functions for overlay and additive translucency
// -----------------------------------------------------------------------------

// [PN] TRANMAP_ALPHA ≈ 168 (66% opacity): fg * 3 + bg * 1 >> 2
#define I_BlendOver_168(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 3 + ((bg_i) & 0xFF00FF)) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 3 + ((bg_i) & 0x00FF00)) >> 2) & 0x00FF00) \
)

// [PN] FUZZTL_ALPHA ≈ 64 (25% opacity): fg * 1 + bg * 3 >> 2
#define I_BlendOver_64(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF) * 3) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00) * 3) >> 2) & 0x00FF00) \
)

// [PN] TINTTAB_ALPHA ≈ 96 (38% opacity): fg * 1 + bg * 2 >> 2
#define I_BlendOver_96(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 3 + ((bg_i) & 0xFF00FF) * 5) >> 3) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 3 + ((bg_i) & 0x00FF00) * 5) >> 3) & 0x00FF00) \
)

// [PN] TINTTAB_ALPHA_ALT ≈ 142 (56% opacity): fg * 9 + bg * 7 >> 4
#define I_BlendOver_142(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 9 + ((bg_i) & 0xFF00FF) * 7) >> 4) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 9 + ((bg_i) & 0x00FF00) * 7) >> 4) & 0x00FF00) \
)

// [PN] XLATAB_ALPHA ≈ 192 (75% opacity): fg * 3 + bg * 1 >> 2
#define I_BlendOver_192(bg_i, fg_i) I_BlendOver_168(bg_i, fg_i)

// [PN] Fastest algorithm for 50% opacity
#define I_BlendOver_128(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF)) >> 1) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00)) >> 1) & 0x00FF00) \
)

// -----------------------------------------------------------------------------
// Paletted blending helper functions for overlay and additive translucency
// -----------------------------------------------------------------------------

// [PN] Fast approximation (3/4 instead of 168/256).
static inline pixel_t I_BlendOver_168_8 (uint32_t bg, uint32_t fg)
{
    // Mix R|B together (mask 0x00FF00FF) and G separately (mask 0x0000FF00)
    // Approximate weights: fg = 3/4, bg = 1/4 -> (3*fg + bg) >> 2
    const uint32_t rb = (((fg & 0x00FF00FFu) * 3u + (bg & 0x00FF00FFu)) >> 2) & 0x00FF00FFu;
    const uint32_t g8 = (((fg & 0x0000FF00u) * 3u + (bg & 0x0000FF00u)) >> 2) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

// [PN] Fast approximation (1/4 instead of exact 64/256).
static inline pixel_t I_BlendOver_64_8 (uint32_t bg, uint32_t fg)
{
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

// [PN] Fast path for a = 96 (fg = 3/8, bg = 5/8). Exact weights via shifts/adds,
// no general multiplies. Packs R|B and G separately; minimal temporaries.
static inline pixel_t I_BlendOver_96_8 (uint32_t bg, uint32_t fg)
{
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

// [PN] Fast approximation for a=142 (fg ≈ 9/16, bg ≈ 7/16).
static inline pixel_t I_BlendOver_142_8 (uint32_t bg, uint32_t fg)
{
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

// [PN] Additive via LUT (exact a=192/256), packed-index fast path.
// Forms (bg<<8 | fg) per channel without extracting scalars.
static inline pixel_t I_BlendAdd_8 (uint32_t bg, uint32_t fg)
{
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
