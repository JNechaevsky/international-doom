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


typedef union
{
    uint32_t i;
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
} tcpixel_t;

extern uint8_t **additive_lut;

extern void I_InitTCTransMaps (void);

extern const int I_ShadeFactor[];
extern const float I_SaturationPercent[];
extern const double colorblind_matrix[][3][3];


// [JN] Blending alpha values, representing 
// transparency levels from paletted rendering.

// Doom:
#define TRANMAP_ALPHA       0xA8  // 168 (66% opacity)
#define FUZZ_ALPHA          0xD3  // 211 (17% darkening)
#define FUZZTL_ALPHA        0x40  //  64 (25% opacity)

// Heretic and Hexen:
#define TINTTAB_ALPHA       0x60  //  96 (38% opacity)
#define TINTTAB_ALPHA_ALT   0x8E  // 142 (56% opacity)
#define EXTRATL_ALPHA       0x98  // 152 (60% opacity)

// Strife:
#define XLATAB_ALPHA        0xC0  // 192 (75% opacity)
#define XLATAB_ALPHA_ALT    0x40  //  64 (25% opacity)

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
    ((0xFFU << 24) | \
    (additive_lut[((bg_i) & 0xFF)][((fg_i) & 0xFF)] & 0xFF) | \
    ((additive_lut[(((bg_i) >> 8) & 0xFF)][(((fg_i) >> 8) & 0xFF)] & 0xFF) << 8) | \
    ((additive_lut[(((bg_i) >> 16) & 0xFF)][(((fg_i) >> 16) & 0xFF)] & 0xFF) << 16)) \
)

#define I_BlendDark(bg_i, d) ( \
    ((0xFFU << 24) | \
    (((((bg_i) & 0xFF) * (d)) >> 8) & 0xFF) | \
    ((((bg_i) >> 8 & 0xFF) * (d)) >> 8 & 0xFF) << 8 | \
    ((((bg_i) >> 16 & 0xFF) * (d)) >> 8 & 0xFF) << 16) \
)

#define I_BlendDarkGrayscale(bg_i, d) ( \
    ((0xFFU << 24) | \
    (((( (((((bg_i) >> 16) & 0xFF) + (((bg_i) >> 8) & 0xFF) + ((bg_i) & 0xFF)) / 3) * (d)) >> 8) & 0xFF) * 0x010101U)) \
)

#define I_BlendOver(bg_i, fg_i, amount) ( \
    ((0xFFU << 24) | \
    ((((amount) * ((fg_i) & 0xFF) + ((0xFFU - (amount)) * ((bg_i) & 0xFF))) >> 8) & 0xFF) | \
    (((((amount) * (((fg_i) >> 8) & 0xFF) + ((0xFFU - (amount)) * (((bg_i) >> 8) & 0xFF))) >> 8) & 0xFF) << 8) | \
    (((((amount) * (((fg_i) >> 16) & 0xFF) + ((0xFFU - (amount)) * (((bg_i) >> 16) & 0xFF))) >> 8) & 0xFF) << 16)) \
)

#endif

#endif
