//
// Copyright(C) 2026 Polina "Aura" N.
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

#pragma once

#include "doomtype.h"


// [PN] Macros to optimize and standardize color calculations in the R_InitColormaps.
//
// CALC_INTENSITY calculates the RGB components from playpal based on intensity settings.
//
// CALC_SATURATION applies saturation correction using values from CALC_INTENSITY along 
// with the a_hi and a_lo coefficients.
//
// CALC_CONTRAST adjusts the contrast of the red, green, and blue channels
// based on the vid_contrast variable. A value of 1.0 preserves the original contrast,
// while values below 1.0 reduce it, and values above 1.0 enhance it. The calculation
// ensures that channel values remain within the valid range [0, 255].
//
// CALC_COLORBLIND applies a colorblindness correction to the red, green, 
// and blue channels based on the provided transformation matrix. The matrix defines
// how much of each original channel (red, green, blue) contributes to the final values.
// This process simulates how colors are perceived by individuals with various types 
// of colorblindness, such as protanopia, deuteranopia, or tritanopia.
// The calculation ensures that the resulting channel values remain within the valid 
// range [0, 255], preserving consistency and avoiding overflow or underflow.
//
// Also, thanks Alaux!

#define CALC_INTENSITY(pal, playpal, index) \
    { pal[0] = (byte)BETWEEN(0, 255, (int)(playpal[3 * (index) + 0] * vid_r_intensity)); \
      pal[1] = (byte)BETWEEN(0, 255, (int)(playpal[3 * (index) + 1] * vid_g_intensity)); \
      pal[2] = (byte)BETWEEN(0, 255, (int)(playpal[3 * (index) + 2] * vid_b_intensity)); }

#define CALC_SATURATION(channels, pal, a_hi, a_lo) \
    { const float one_minus_a_hi = 1.0f - a_hi; \
      channels[0] = (byte)BETWEEN(0, 255, (int)(one_minus_a_hi * pal[0] + a_lo * (pal[1] + pal[2]))); \
      channels[1] = (byte)BETWEEN(0, 255, (int)(one_minus_a_hi * pal[1] + a_lo * (pal[0] + pal[2]))); \
      channels[2] = (byte)BETWEEN(0, 255, (int)(one_minus_a_hi * pal[2] + a_lo * (pal[0] + pal[1]))); }

#define CALC_CONTRAST(channels, contrast) \
    { const float contrast_adjustment = 128 * (1.0f - contrast); \
      channels[0] = (int)BETWEEN(0, 255, (int)(contrast * channels[0] + contrast_adjustment)); \
      channels[1] = (int)BETWEEN(0, 255, (int)(contrast * channels[1] + contrast_adjustment)); \
      channels[2] = (int)BETWEEN(0, 255, (int)(contrast * channels[2] + contrast_adjustment)); }

#define CALC_COLORBLIND(channels, matrix) \
    { const byte r = channels[0]; \
      const byte g = channels[1]; \
      const byte b = channels[2]; \
      channels[0] = (byte)BETWEEN(0, 255, (int)(matrix[0][0] * r + matrix[0][1] * g + matrix[0][2] * b)); \
      channels[1] = (byte)BETWEEN(0, 255, (int)(matrix[1][0] * r + matrix[1][1] * g + matrix[1][2] * b)); \
      channels[2] = (byte)BETWEEN(0, 255, (int)(matrix[2][0] * r + matrix[2][1] * g + matrix[2][2] * b)); }


typedef void (*I_Palette_RebaseFn) (const pixel_t *old_base, const pixel_t *new_base, void *ctx);

extern int I_Palette_GetActive (void);

extern pixel_t *I_Palette_GetColor (int p);
extern pixel_t *I_Palette_GetColormaps (int p);
extern pixel_t *I_Palette_Swap (int idx, pixel_t *current_base, I_Palette_RebaseFn rebase, void *ctx);

extern void I_Palette_Alloc (int numcolormaps);
extern void I_Palette_BuildColor (const byte *playpal_p, pixel_t *out);
extern void I_Palette_BuildOne (const byte *playpal_p, const byte *colormap, int numcolormaps, boolean original_colormap, pixel_t *out);
extern void I_Palette_InitAll (const byte *playpal, const byte *colormap, int numcolormaps, boolean original_colormap);
