//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2015-2024 Fabian Greffrath
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

#ifndef __I_TRUECOLOR__
#define __I_TRUECOLOR__

#include "config.h"

#ifdef CRISPY_TRUECOLOR

#include <stdint.h>


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

extern uint8_t   additive_lut[511];
extern uint8_t   shadow_alpha;
extern uint8_t   fuzz_alpha;

extern void I_InitTCTransMaps (void);

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
    (additive_lut[((bg_i) & 0xFF) + ((fg_i) & 0xFF)]) | \
    (additive_lut[(((bg_i) >> 8) & 0xFF) + (((fg_i) >> 8) & 0xFF)] << 8) | \
    (additive_lut[(((bg_i) >> 16) & 0xFF) + (((fg_i) >> 16) & 0xFF)] << 16) \
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

// [PN] Fixed-alpha shift-weighted approximation of alpha blending ("overlay-lite").
//
// These macros implement a fast alternative to standard alpha blending
// using fixed, preselected alpha values (e.g., 25%, 38%, 56%, etc).
// Unlike I_BlendOver(...), these macros do not take alpha as a parameter.
// Instead, the alpha value is embedded in the macro as hardcoded constants.
//
// Classic alpha blending uses:
//   result = (fg * alpha + bg * (255 - alpha)) >> 8
//
// These macros approximate the effect using simplified integer math:
//   result ≈ (fg * A + bg * B) >> shift
//
// Coefficients A, B and the shift amount are selected to approximate
// the visual output of real alpha blending while significantly reducing
// arithmetic complexity.
//
// Advantages:
// - No runtime multiplications by variable alpha
// - No subtraction of (255 - alpha)
// - No divisions by 255
// - Ideal for performance-critical inner loops with few alpha levels
//
// Limitations:
// - Visual result is approximate, not exact
// - Only suitable for fixed, known alpha values
//
// [JN] Blending alpha values representing transparency
// levels from paletted rendering.

// TRANMAP_ALPHA ≈ 168 (66% opacity): fg * 3 + bg * 1 >> 2
#define I_BlendOver_168(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 3 + ((bg_i) & 0xFF00FF)) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 3 + ((bg_i) & 0x00FF00)) >> 2) & 0x00FF00) \
)

// FUZZTL_ALPHA ≈ 64 (25% opacity): fg * 1 + bg * 3 >> 2
#define I_BlendOver_64(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF) * 3) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00) * 3) >> 2) & 0x00FF00) \
)

// TINTTAB_ALPHA ≈ 96 (38% opacity): fg * 1 + bg * 2 >> 2
#define I_BlendOver_96(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF) * 2) >> 2) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00) * 2) >> 2) & 0x00FF00) \
)

// TINTTAB_ALPHA_ALT ≈ 142 (56% opacity): fg * 9 + bg * 7 >> 4
#define I_BlendOver_142(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) * 9 + ((bg_i) & 0xFF00FF) * 7) >> 4) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) * 9 + ((bg_i) & 0x00FF00) * 7) >> 4) & 0x00FF00) \
)

// XLATAB_ALPHA ≈ 192 (75% opacity): fg * 3 + bg * 1 >> 2
#define I_BlendOver_192(bg_i, fg_i) I_BlendOver_168(bg_i, fg_i)

// [PN] Fastest algorithm for 50% opacity (unused)
/*
#define I_BlendOver50(bg_i, fg_i) ( \
    (0xFF000000U) | \
    (((((fg_i) & 0xFF00FF) + ((bg_i) & 0xFF00FF)) >> 1) & 0xFF00FF) | \
    (((((fg_i) & 0x00FF00) + ((bg_i) & 0x00FF00)) >> 1) & 0x00FF00) \
)
*/

#endif

#endif
