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

#include <stdlib.h> // malloc
#include "config.h"

#ifdef CRISPY_TRUECOLOR

#include "i_truecolor.h"
#include "m_fixed.h"

#include "id_vars.h"



// [PN] Initializes a 511-entry 1D saturation LUT for additive blending.
// The LUT maps all possible sums of two 8-bit color channels (0..255 + 0..255),
// clamping any result above 255 to 255.
uint8_t additive_lut[511];

void I_InitTCTransMaps (void)
{
    for (int i = 0; i < 511; ++i)
    {
        additive_lut[i] = (uint8_t)(i > 255 ? 255 : i);
    }
}

// [PN] All original human-readable blending functions from Crispy Doom
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
*/

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

#endif
