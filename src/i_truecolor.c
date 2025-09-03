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

#include <stdlib.h> // malloc
#include <string.h> // memcpy
#include "config.h"
#include "i_truecolor.h"
#include "m_fixed.h"
#include "v_video.h"
#include "z_zone.h"

#include "id_vars.h"


// =============================================================================
//
//                       Blending initialization functions
//
// =============================================================================

// -----------------------------------------------------------------------------
// TrueColor blending. 
//
// [PN] Initializes a 511-entry 1D saturation LUT for additive blending.
// The LUT maps all possible sums of two 8-bit color channels (0..255 + 0..255),
// clamping any result above 255 to 255.
// -----------------------------------------------------------------------------

uint8_t additive_lut_32[511];

void I_InitTCTransMaps (void)
{
    for (int i = 0; i < 511; ++i)
    {
        additive_lut_32[i] = (uint8_t)(i > 255 ? 255 : i);
    }
}

// -----------------------------------------------------------------------------
// Paletted blending. 
// 
// [PN] In the TrueColor renderer, all blending operations (translucency,
// additive, etc.) are performed in 32-bit RGBA space. However, for
// compatibility with the classic 8-bit Doom palette and its colormaps,
// the results must be "palettized" back into PLAYPAL indices.
//
// This module implements that by building precomputed lookup tables:
// - A gamma-aware 3D RGB→PAL LUT (rgb_to_pal) that maps quantized
//   truecolor values back to the closest palette entry. The LUT is
//   rebuilt when gamma/brightness changes, but rendering itself uses
//   only fast O(1) lookups.
// - A per-channel additive LUT (additive_lut_8) of size 256×256, which
//   computes bg + (fg * alpha >> 8), clamped to 255, so additive
//   blending reduces to three array lookups.
// 
// In practice: 
//   1. Foreground and background pixels are blended in linear RGB.
//   2. The resulting RGB is quantized and converted to a palette index
//      via rgb_to_pal.
//   3. That index is written through pal_color[], ensuring the output
//      respects both PLAYPAL and the current gamma correction.
// 
// This allows Doom’s original 8-bit translucent style to be reproduced
// within the TrueColor renderer: visual results remain close to the
// classic tables, but all blending math runs fast and consistently.
// -----------------------------------------------------------------------------

// Overlay blending
byte *playpal_trans;  // PLAYPAL copy for 8 bit blending
byte *rgb_to_pal;     // [PN/JN] quantized RGB -> indexed PLAYPAL

// Additive blending
byte *additive_lut_8; // [PN/JN] channaled LUT

// -----------------------------------------------------------------------------
// [PN] I_InitPALTransMaps
//
// Initializes and builds precomputed lookup tables used for translucency
// and additive blending.
//
// This function constructs:
//  1) A gamma-aware 3D RGB->palette LUT (rgb_to_pal), used to map blended
//     truecolor values back to the closest palette index. This is rebuilt
//     on startup and when gamma correction change.
//
//  2) A per-channel additive blending LUT (additive_lut_8) of size 256×256,
//     which computes bg + (fg * alpha >> 8), clamped to 255. This allows
//     fast additive blending in the render loop with a single lookup instead
//     of per-pixel math.
// -----------------------------------------------------------------------------

void I_InitPALTransMaps(void)
{
    static uint8_t pal_r[256], pal_g[256], pal_b[256]; // from pal_color (gamma-applied)
    static uint8_t diffR[PAL_STEPS][256];
    static uint8_t diffG[PAL_STEPS][256];
    static uint8_t diffB[PAL_STEPS][256];
    static uint8_t step_val[PAL_STEPS];                // quantized 0..255

    // Init overlay blending (RGB->Palette LUT)
    static boolean pal_init = false;
    if (!pal_init)
    {
        const byte *src = W_CacheLumpName("PLAYPAL", PU_STATIC);

        playpal_trans = Z_Malloc(256 * 3, PU_STATIC, 0);
        memcpy(playpal_trans, src, 256 * 3);
        W_ReleaseLumpName("PLAYPAL");
        pal_init = true;
    }

    if (!rgb_to_pal)
        rgb_to_pal = Z_Malloc(PAL_LUT_SIZE, PU_STATIC, 0);

    // Prepare quantized step values and per-channel difference tables
    static boolean step_vals_init = false;
    if (!step_vals_init)
    {
        for (int q = 0; q < PAL_STEPS; ++q)
            step_val[q] = (uint8_t)((q * 255) / (PAL_STEPS - 1));
        step_vals_init = true;
    }

    // Extract RGB components from gamma-corrected pal_color
    for (int i = 0; i < 256; ++i)
    {
        const pixel_t pc = pal_color[i];
        pal_r[i] = (uint8_t)((pc >> 16) & 0xFF);
        pal_g[i] = (uint8_t)((pc >>  8) & 0xFF);
        pal_b[i] = (uint8_t)( pc        & 0xFF);
    }
    
    // Build diffR/diffG/diffB tables for PAL_STEPS
    for (int qr = 0; qr < PAL_STEPS; ++qr)
    {
        const int Rq = step_val[qr];
        for (int i = 0; i < 256; ++i)
        {
            const int d = Rq - pal_r[i];
            diffR[qr][i] = (uint8_t)(d < 0 ? -d : d);
        }
    }
    for (int qg = 0; qg < PAL_STEPS; ++qg)
    {
        const int Gq = step_val[qg];
        for (int i = 0; i < 256; ++i)
        {
            const int d = Gq - pal_g[i];
            diffG[qg][i] = (uint8_t)(d < 0 ? -d : d);
        }
    }
    for (int qb = 0; qb < PAL_STEPS; ++qb)
    {
        const int Bq = step_val[qb];
        for (int i = 0; i < 256; ++i)
        {
            const int d = Bq - pal_b[i];
            diffB[qb][i] = (uint8_t)(d < 0 ? -d : d);
        }
    }

    // Temporary buffer for pre-summing R+G diffs per palette index
    uint16_t sumRG[256];

    // Main 3D loop over quantized RGB space
    const int shift1 = PAL_BITS;
    const int shift2 = 2 * PAL_BITS;

    for (int qr = 0; qr < PAL_STEPS; ++qr)
    {
        const uint8_t *restrict dR = diffR[qr];
    
        for (int qg = 0; qg < PAL_STEPS; ++qg)
        {
            const uint8_t *restrict dG = diffG[qg];
    
            // Pre-sum R+G
            uint16_t *restrict sum_ptr = sumRG;
            const uint8_t *restrict dR_ptr = dR;
            const uint8_t *restrict dG_ptr = dG;
            for (int i = 0; i < 256; ++i)
                *sum_ptr++ = (uint16_t)(*dR_ptr++) + (uint16_t)(*dG_ptr++);
    
            const int base = (qr << shift2) | (qg << shift1);
    
            // Start from previous best - strong warm-up for best_diff
            int prev_best = 0;
    
            for (int qb = 0; qb < PAL_STEPS; ++qb)
            {
                const uint8_t *restrict dB = diffB[qb];
    
                // Initialize candidate from previous qb
                int best = prev_best;
                uint16_t best_diff = (uint16_t)(sumRG[best] + dB[best]);
    
                // Loop unrolled by 4
                const uint16_t *restrict sr = sumRG;
                const uint8_t  *restrict db = dB;
    
                for (int i = 0; i < 256; i += 4)
                {
                    uint16_t d0 = (uint16_t)(sr[0] + db[0]);
                    if (d0 < best_diff) { best = i + 0; best_diff = d0; if (!d0) goto got_best; }
    
                    uint16_t d1 = (uint16_t)(sr[1] + db[1]);
                    if (d1 < best_diff) { best = i + 1; best_diff = d1; if (!d1) goto got_best; }
    
                    uint16_t d2 = (uint16_t)(sr[2] + db[2]);
                    if (d2 < best_diff) { best = i + 2; best_diff = d2; if (!d2) goto got_best; }
    
                    uint16_t d3 = (uint16_t)(sr[3] + db[3]);
                    if (d3 < best_diff) { best = i + 3; best_diff = d3; if (!d3) goto got_best; }
    
                    sr += 4; db += 4;
                }
    
            got_best:
                rgb_to_pal[base + qb] = (byte)best;
                prev_best = best; // Local "inertia" along the B axis
            }
        }
    }

    // Init additive blending LUT
    static boolean additive_lut_ready = false;
    if (!additive_lut_ready)
    {
        additive_lut_8 = Z_Malloc(256 * 256, PU_STATIC, 0);

        // Foreground contribution factor (0..256)
        const int a = 192; // ~75% foreground
        uint8_t fgA[256];

        for (int fg = 0; fg < 256; ++fg)
        {
            fgA[fg] = (uint8_t)((fg * a) >> 8);
        }

        byte *lut_ptr = additive_lut_8;

        for (int bg = 0; bg < 256; ++bg)
        {
            for (int fg = 0; fg < 256; ++fg)
            {
                int sum = bg + fgA[fg];
                *lut_ptr++ = (byte)(sum > 255 ? 255 : sum);
            }
        }
    
        additive_lut_ready = true;
    }
}


// [JN] Shadow alpha value for shadowed patches, and fuzz
// alpha value for fuzz effect drawing based on contrast.
uint8_t shadow_alpha;
uint8_t fuzz_alpha;

// [JN] Shade factor used for menu and automap background shading.
const int I_ShadeFactor[] =
{
    240, 224, 208, 192, 176, 160, 144, 112, 96, 80, 64, 48, 32
};

// [JN] Saturation percent array.
// 0.66 = 0% saturation, 0.0 = 100% saturation.
const float I_SaturationPercent[100] =
{
    0.660000f, 0.653400f, 0.646800f, 0.640200f, 0.633600f,
    0.627000f, 0.620400f, 0.613800f, 0.607200f, 0.600600f,
    0.594000f, 0.587400f, 0.580800f, 0.574200f, 0.567600f,
    0.561000f, 0.554400f, 0.547800f, 0.541200f, 0.534600f,
    0.528000f, 0.521400f, 0.514800f, 0.508200f, 0.501600f,
    0.495000f, 0.488400f, 0.481800f, 0.475200f, 0.468600f,
    0.462000f, 0.455400f, 0.448800f, 0.442200f, 0.435600f,
    0.429000f, 0.422400f, 0.415800f, 0.409200f, 0.402600f,
    0.396000f, 0.389400f, 0.382800f, 0.376200f, 0.369600f,
    0.363000f, 0.356400f, 0.349800f, 0.343200f, 0.336600f,
    0.330000f, 0.323400f, 0.316800f, 0.310200f, 0.303600f,
    0.297000f, 0.290400f, 0.283800f, 0.277200f, 0.270600f,
    0.264000f, 0.257400f, 0.250800f, 0.244200f, 0.237600f,
    0.231000f, 0.224400f, 0.217800f, 0.211200f, 0.204600f,
    0.198000f, 0.191400f, 0.184800f, 0.178200f, 0.171600f,
    0.165000f, 0.158400f, 0.151800f, 0.145200f, 0.138600f,
    0.132000f, 0.125400f, 0.118800f, 0.112200f, 0.105600f,
    0.099000f, 0.092400f, 0.085800f, 0.079200f, 0.072600f,
    0.066000f, 0.059400f, 0.052800f, 0.046200f, 0.039600f,
    0.033000f, 0.026400f, 0.019800f, 0.013200f, 0
};


// =============================================================================
//
//                                 Colorblind
//
// =============================================================================

// [JN] Color matrices to emulate colorblind modes.
// Original source: http://web.archive.org/web/20081014161121/http://www.colorjack.com/labs/colormatrix/
// Converted from 100.000 ... 0 range to 1.00000 ... 0 to support Doom palette format.
const double colorblind_matrix[][3][3] = {
    { {1.00000, 0.00000, 0.00000}, {0.00000, 1.00000, 0.00000}, {0.00000, 0.00000, 1.00000} }, // No colorblind
    { {0.56667, 0.43333, 0.00000}, {0.55833, 0.44167, 0.00000}, {0.00000, 0.24167, 0.75833} }, // Protanopia
    { {0.81667, 0.18333, 0.00000}, {0.33333, 0.66667, 0.00000}, {0.00000, 0.12500, 0.87500} }, // Protanomaly
    { {0.62500, 0.37500, 0.00000}, {0.70000, 0.30000, 0.00000}, {0.00000, 0.30000, 0.70000} }, // Deuteranopia
    { {0.80000, 0.20000, 0.00000}, {0.25833, 0.74167, 0.00000}, {0.00000, 0.14167, 0.85833} }, // Deuteranomaly
    { {0.95000, 0.05000, 0.00000}, {0.00000, 0.43333, 0.56667}, {0.00000, 0.47500, 0.52500} }, // Tritanopia
    { {0.96667, 0.03333, 0.00000}, {0.00000, 0.73333, 0.26667}, {0.00000, 0.18333, 0.81667} }, // Tritanomaly
    { {0.29900, 0.58700, 0.11400}, {0.29900, 0.58700, 0.11400}, {0.29900, 0.58700, 0.11400} }, // Achromatopsia
    { {0.61800, 0.32000, 0.06200}, {0.16300, 0.77500, 0.06200}, {0.16300, 0.32000, 0.51600} }, // Achromatomaly
};


// =============================================================================
//
//     All original human-readable blending functions from Crispy and Inter
//
// =============================================================================

//
// TrueColor functions:
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

//
// Paletted functions:
//

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

const pixel_t TLBlendAdd (pixel_t bg, pixel_t fg)
{
    // Extract channels
    const int bg_r = (bg >> 16) & 0xFF;
    const int bg_g = (bg >> 8)  & 0xFF;
    const int bg_b =  bg        & 0xFF;
    const int fg_r = (fg >> 16) & 0xFF;
    const int fg_g = (fg >> 8)  & 0xFF;
    const int fg_b =  fg        & 0xFF;

    // Fast additive blending via LUT per channel
    const int r = additive_lut_8[(bg_r << 8) | fg_r];
    const int g = additive_lut_8[(bg_g << 8) | fg_g];
    const int b = additive_lut_8[(bg_b << 8) | fg_b];

    // Map to palette (gamma-aware)
    return pal_color[ RGB_TO_PAL(r, g, b) ];
}

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

// [PN] Paletted 50% over (fg/bg average). Cheapest possible:
// per-channel average with packed RB and separate G; no multiplications.
static inline pixel_t I_BlendOver_128_8 (uint32_t bg, uint32_t fg)
{
    // Average R|B together and G separately; carries are fine (>>1), then mask lanes.
    const uint32_t rb = (((fg & 0x00FF00FFu) + (bg & 0x00FF00FFu)) >> 1) & 0x00FF00FFu;
    const uint32_t g8 = (((fg & 0x0000FF00u) + (bg & 0x0000FF00u)) >> 1) & 0x0000FF00u;

    // Map into 8-bit palette via 3D LUT
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

// [PN] Paletted darken: multiply RGB by factor d (0..255) and map via RGB->PAL LUT.
static inline pixel_t I_BlendDark_8 (uint32_t bg, int d)
{
    // Scale packed R|B together and G separately
    const uint32_t rb = (((bg & 0x00FF00FFu) * (uint32_t)d) >> 8) & 0x00FF00FFu;
    const uint32_t g8 = (((bg & 0x0000FF00u) * (uint32_t)d) >> 8) & 0x0000FF00u;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL((rb >> 16) & 0xFFu, (g8 >> 8) & 0xFFu, rb & 0xFFu) ];
}

// [PN] Paletted grayscale darken (exact-ish, still cheap).
// Classic luma with integer weights: 77/150/29 (sum 256).
// Fold 'd' into weights: Wr=(77*d)>>8, Wg=(150*d)>>8, Wb=(29*d)>>8.
// => y = (Wr*R + Wg*G + Wb*B) >> 8
static inline pixel_t I_BlendDarkGrayscale_8 (uint32_t bg, int d)
{
    const uint32_t r = (bg >> 16) & 0xFFu;
    const uint32_t g = (bg >>  8) & 0xFFu;
    const uint32_t b =  bg        & 0xFFu;

    const uint32_t Wr = (77u  * (uint32_t)d) >> 8;   // 0.299 * d
    const uint32_t Wg = (150u * (uint32_t)d) >> 8;   // 0.587 * d
    const uint32_t Wb = (29u  * (uint32_t)d) >> 8;   // 0.114 * d

    const uint32_t y = (Wr*r + Wg*g + Wb*b) >> 8;

    return pal_color[ RGB_TO_PAL(y, y, y) ];
}
*/
