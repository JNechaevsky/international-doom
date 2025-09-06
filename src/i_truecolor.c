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
//  1) A gamma-aware 3D RGB->palette LUT (rgb_to_pal). Each quantized RGB
//     cell (PAL_STEPS³) is mapped to the nearest palette index by L1
//     (Manhattan) distance in gamma-corrected space.
//     - Per-channel diff tables (diffR/G/B) are built once.
//     - For each quantized (R,G), we precompute sumRG[i] = |Rq-Ri|+|Gq-Gi|.
//     - Palette indices are bucket-sorted by sumRG for fast ascending order.
//     - For each Bq, the search is pruned using a lower bound
//       (sumRG[idx] + min(|Bq-Bi|)) and warm-started from the previous best.
//       This reduces comparisons by ~50% compared to the naive O(256) scan.
//
//  2) A per-channel additive blending LUT (additive_lut_8) of size 256×256,
//     which computes bg + (fg * alpha >> 8), clamped to 255. This allows
//     fast additive blending in the render loop with a single lookup instead
//     of per-pixel math.
// -----------------------------------------------------------------------------

void I_InitPALTransMaps(void)
{
    // Static caches
    static uint8_t pal_r[256], pal_g[256], pal_b[256]; // from pal_color (gamma-applied)
    static uint8_t diffR[PAL_STEPS][256];
    static uint8_t diffG[PAL_STEPS][256];
    static uint8_t diffB[PAL_STEPS][256];
    static uint8_t step_val[PAL_STEPS];                // quantized 0..255
    static uint8_t dB_min[PAL_STEPS];                  // min |Bq - pal_b[i]| per qb

    // Init overlay blending (one-time)
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

    // Prepare quantized step values (one-time)
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

    // Build per-channel |quant - pal| tables (O(PAL_STEPS * 256))
    // Branchless abs would also work, but with uint8_t the compiler already optimizes it well.
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
        uint8_t minv = 255;
        for (int i = 0; i < 256; ++i)
        {
            const int d = Bq - pal_b[i];
            const uint8_t v = (uint8_t)(d < 0 ? -d : d);
            diffB[qb][i] = v;
            if (v < minv)
                minv = v;
        }
        dB_min[qb] = minv; // Precomputed lower bound for pruning
    }

    // Temporary buffers reused per (qr,qg)
    uint16_t sumRG[256];        // pre-summed R+G differences per palette index
    uint8_t  order[256];        // palette indices sorted by sumRG ascending
    uint16_t buckets[511];      // histogram 0..510 for bucket-sort

    // Main 3D loop over quantized RGB space
    const int shift1 = PAL_BITS;
    const int shift2 = 2 * PAL_BITS;

    for (int qr = 0; qr < PAL_STEPS; ++qr)
    {
        const uint8_t *restrict dR = diffR[qr];

        for (int qg = 0; qg < PAL_STEPS; ++qg)
        {
            const uint8_t *restrict dG = diffG[qg];

            // 1) sumRG[i] = dR[i] + dG[i]
            for (int i = 0; i < 256; ++i)
                sumRG[i] = (uint16_t)dR[i] + (uint16_t)dG[i];

            // 2) bucket-sort indices by sumRG (0..510) — O(256 + 511)
            //    initialize buckets with zeros
            for (int v = 0; v <= 510; ++v)
                buckets[v] = 0;

            for (int i = 0; i < 256; ++i)
                ++buckets[sumRG[i]];

            // prefix sums -> starting offsets
            uint16_t acc = 0;
            for (int v = 0; v <= 510; ++v)
            {
                const uint16_t cnt = buckets[v];
                buckets[v] = acc;
                acc = (uint16_t)(acc + cnt);
            }

            // build order[] array
            for (int i = 0; i < 256; ++i)
            {
                const uint16_t v = sumRG[i];
                order[buckets[v]++] = (uint8_t)i;
            }

            const int base = (qr << shift2) | (qg << shift1);

            // 3) Along B axis — fast search with early cutoff
            int prev_best = 0;

            for (int qb = 0; qb < PAL_STEPS; ++qb)
            {
                const uint8_t *restrict dB     = diffB[qb];
                const uint16_t dBmin           = (uint16_t)dB_min[qb];

                // initial candidate comes from previous qb (warm start)
                int      best      = prev_best;
                uint16_t best_diff = (uint16_t)(sumRG[best] + dB[best]);

                // lower bound for subsequent candidates:
                // if sumRG[idx] + dBmin >= best_diff, no further improvement
                // process indices in ascending sumRG order
                for (int k = 0; k < 256; ++k)
                {
                    const uint8_t idx = order[k];
                    const uint16_t s  = sumRG[idx];

                    if ((uint16_t)(s + dBmin) >= best_diff)
                        break;

                    // compute exact distance only for the "head" of the list
                    const uint16_t cand = (uint16_t)(s + dB[idx]);
                    if (cand < best_diff)
                    {
                        best_diff = cand;
                        best      = idx;
                        if (!cand) // perfect match (distance == 0)
                            break;
                    }
                }

                rgb_to_pal[base + qb] = (byte)best;
                prev_best = best; // inertia along B axis (reuse last best)
            }
        }
    }

    // Init additive blending LUT (one-time)
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
