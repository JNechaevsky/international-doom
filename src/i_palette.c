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
// DESCRIPTION:
//  [PN] Shared per-PLAYPAL palette infrastructure for Doom/Heretic/Hexen.
//  INTL truecolor bakes palette+lighting into colormaps[L][k] = ARGB. For
//  a palette flash we need one such table per PLAYPAL palette and swap the
//  active one at flash time. This module owns the storage and the build;
//  game-side r_data.c keeps the derived-table rebase (scalelight/zlight/
//  scalelightfixed), which is game-specific.
//

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_palette.h"
#include "i_truecolor.h"
#include "tables.h"
#include "w_wad.h"
#include "z_zone.h"

#include "id_vars.h"


// [JN] Maximum available PLAYPAL indexes.
// 14 for Doom and Heretic, 28 for Hexen.
#define MAXPALINDEXES 28

// [PN] CALC_* color-correction macros live in i_palette.h (shared with the
// game-side base build for bit-identical results).

static pixel_t *pal_colormaps[MAXPALINDEXES];
static pixel_t *pal_color_p  [MAXPALINDEXES];
static int      pal_numcolormaps = -1;
static int      pal_count = 0;
static int      active_idx = -1;


// -----------------------------------------------------------------------------
// I_Palette_Alloc
//  [PN] Allocate all per-palette colormaps/pal_color buffers at the given
//  NUMCOLORMAPS size. Frees and reallocs if NUMCOLORMAPS changed since the
//  last call (vid_truecolor 0 <-> 1/2). Safe to call repeatedly.
// -----------------------------------------------------------------------------

void I_Palette_Alloc (int numcolormaps)
{
    const int lump = W_GetNumForName("PLAYPAL");
    const int lump_len = (int)W_LumpLength(lump);
    int np = lump_len / 768;
    if (np > MAXPALINDEXES) np = MAXPALINDEXES;
    if (np < 1) np = 1;

    pal_count = np;

    // [PN] If NUMCOLORMAPS changed (vid_truecolor 0 <-> 1/2), free everything
    // so the loop below reallocates at the new size.
    if (pal_numcolormaps != numcolormaps)
    {
        for (int p = 0; p < MAXPALINDEXES; ++p)
        {
            if (pal_colormaps[p]) { Z_Free(pal_colormaps[p]); pal_colormaps[p] = NULL; }
            if (pal_color_p[p])   { Z_Free(pal_color_p[p]);   pal_color_p[p]   = NULL; }
        }
        pal_numcolormaps = numcolormaps;
    }

    const size_t row_bytes = (size_t)(numcolormaps + 1) * 256 * sizeof(pixel_t);
    const size_t pc_bytes  = (size_t)256 * sizeof(pixel_t);

    for (int p = 0; p < np; ++p)
    {
        if (!pal_colormaps[p])
             pal_colormaps[p] = (pixel_t *) Z_Malloc(row_bytes, PU_STATIC, 0);
        if (!pal_color_p[p])
             pal_color_p[p]   = (pixel_t *) Z_Malloc(pc_bytes, PU_STATIC, 0);
    }
}

// -----------------------------------------------------------------------------
// I_Palette_GetColormaps
//  [PN] Get the colormaps buffer for palette p (NULL if not allocated).
//  Returned as pixel_t*; game code assigns to its lighttable_t* typedef.
// -----------------------------------------------------------------------------

pixel_t *I_Palette_GetColormaps (int p)
{
    return (p >= 0 && p < MAXPALINDEXES) ? pal_colormaps[p] : NULL;
}

// -----------------------------------------------------------------------------
// I_Palette_GetColor
//  [PN] Get the pal_color buffer for palette p (NULL if not allocated).
// -----------------------------------------------------------------------------

pixel_t *I_Palette_GetColor (int p)
{
    return (p >= 0 && p < MAXPALINDEXES) ? pal_color_p[p] : NULL;
}

// -----------------------------------------------------------------------------
// I_Palette_BuildOne
//  [PN] Build one colormaps table (NUMCOLORMAPS+1 rows of 256 ARGB, plus the
//  invulnerability row) from the given PLAYPAL palette and COLORMAP lump.
//  'original_colormap' is the game-side flag indicating an unmodified COLORMAP
//  (used only for the invulnerability row synthesis).
// -----------------------------------------------------------------------------

void I_Palette_BuildOne (const byte *playpal_p, const byte *colormap,
                         int numcolormaps, boolean original_colormap,
                         pixel_t *out)
{
    const int saturation = BETWEEN(0, 200, vid_saturation);
    const float a_hi = (saturation < 100) ? I_SaturationPercent[saturation]
                                          : -(float)(saturation - 100) * 0.0066f;
    const float a_lo = a_hi * 0.5f;
    const float one_minus_ah = 1.0f - a_hi;

    if (vid_truecolor)
    {
        const byte *const restrict gtab = gammatable[vid_gamma];
        byte pal_gamma[256][3];

        for (int k = 0; k < 256; ++k)
        {
            float pr = playpal_p[3 * k + 0] * vid_r_intensity;
            float pg = playpal_p[3 * k + 1] * vid_g_intensity;
            float pb = playpal_p[3 * k + 2] * vid_b_intensity;
            float r = one_minus_ah * pr + a_lo * (pg + pb);
            float g = one_minus_ah * pg + a_lo * (pr + pb);
            float b = one_minus_ah * pb + a_lo * (pr + pg);
            pal_gamma[k][0] = gtab[(byte)BETWEEN(0, 255, (int)r)];
            pal_gamma[k][1] = gtab[(byte)BETWEEN(0, 255, (int)g)];
            pal_gamma[k][2] = gtab[(byte)BETWEEN(0, 255, (int)b)];
        }

        const double cMul = (double)vid_contrast;
        const double cAdj = 128.0 * (1.0 - (double)vid_contrast);
        const double (*cbm)[3] = colorblind_matrix[a11y_colorblind];

        if (vid_truecolor == 2)
        {
            const int map_rows = 32;
            const byte *const restrict src0 = &colormap[0];
            const byte *const restrict src1 = &colormap[(map_rows - 1) * 256];

            for (int c = 0; c < numcolormaps; ++c)
            {
                const double t = (numcolormaps > 1)
                    ? (double)c / (double)(numcolormaps - 1) : 0.0;
                const double w0 = 1.0 - t;
                pixel_t *const restrict row = &out[c * 256];

                for (int i = 0; i < 256; ++i)
                {
                    const byte k0i = src0[i];
                    const byte k1i = src1[i];
                    double R = (double)pal_gamma[k0i][0] * w0 + (double)pal_gamma[k1i][0] * t;
                    double G = (double)pal_gamma[k0i][1] * w0 + (double)pal_gamma[k1i][1] * t;
                    double B = (double)pal_gamma[k0i][2] * w0 + (double)pal_gamma[k1i][2] * t;
                    R = cMul * R + cAdj;
                    G = cMul * G + cAdj;
                    B = cMul * B + cAdj;
                    double r2 = cbm[0][0] * R + cbm[0][1] * G + cbm[0][2] * B;
                    double g2 = cbm[1][0] * R + cbm[1][1] * G + cbm[1][2] * B;
                    double b2 = cbm[2][0] * R + cbm[2][1] * G + cbm[2][2] * B;
                    const byte r8 = (byte)BETWEEN(0, 255, (int)r2);
                    const byte g8 = (byte)BETWEEN(0, 255, (int)g2);
                    const byte b8 = (byte)BETWEEN(0, 255, (int)b2);
                    row[i] = 0xff000000 | (r8 << 16) | (g8 << 8) | b8;
                }
            }
        }
        else
        {
            const int map_rows = 32;

            for (int c = 0; c < numcolormaps; ++c)
            {
                const double src_pos = (numcolormaps > 1)
                    ? (double)c * (double)(map_rows - 1) / (double)(numcolormaps - 1) : 0.0;
                const int c0 = (int)src_pos;
                const int c1 = c0 < (map_rows - 1) ? c0 + 1 : c0;
                const double t = src_pos - (double)c0;
                const double w0 = 1.0 - t;
                const byte *const restrict s0 = &colormap[c0 * 256];
                const byte *const restrict s1 = &colormap[c1 * 256];
                pixel_t *const restrict row = &out[c * 256];

                for (int i = 0; i < 256; ++i)
                {
                    const byte k0i = s0[i];
                    const byte k1i = s1[i];
                    double R = (double)pal_gamma[k0i][0] * w0 + (double)pal_gamma[k1i][0] * t;
                    double G = (double)pal_gamma[k0i][1] * w0 + (double)pal_gamma[k1i][1] * t;
                    double B = (double)pal_gamma[k0i][2] * w0 + (double)pal_gamma[k1i][2] * t;
                    R = cMul * R + cAdj;
                    G = cMul * G + cAdj;
                    B = cMul * B + cAdj;
                    double r2 = cbm[0][0] * R + cbm[0][1] * G + cbm[0][2] * B;
                    double g2 = cbm[1][0] * R + cbm[1][1] * G + cbm[1][2] * B;
                    double b2 = cbm[2][0] * R + cbm[2][1] * G + cbm[2][2] * B;
                    const byte r8 = (byte)BETWEEN(0, 255, (int)r2);
                    const byte g8 = (byte)BETWEEN(0, 255, (int)g2);
                    const byte b8 = (byte)BETWEEN(0, 255, (int)b2);
                    row[i] = 0xff000000 | (r8 << 16) | (g8 << 8) | b8;
                }
            }
        }
    }
    else
    {
        int j = 0;

        for (int c = 0; c < numcolormaps; c++)
        {
            for (int i = 0; i < 256; i++)
            {
                byte pal[3], channels[3];
                CALC_INTENSITY(pal, playpal_p, colormap[c * 256 + i]);
                CALC_SATURATION(channels, pal, a_hi, a_lo);
                CALC_CONTRAST(channels, vid_contrast);
                CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);

                byte r = gammatable[vid_gamma][channels[0]] & ~3;
                byte g = gammatable[vid_gamma][channels[1]] & ~3;
                byte b = gammatable[vid_gamma][channels[2]] & ~3;

                out[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
            }
        }
    }

    // [PN] Invulnerability row at out[numcolormaps * 256].
    {
        pixel_t *const restrict invrow = &out[numcolormaps * 256];

        for (int i = 0; i < 256; i++)
        {
            byte src_r, src_g, src_b;

            if (a11y_invul)
            {
                const byte gray = (byte)((playpal_p[3 * i + 0]
                                        + playpal_p[3 * i + 1]
                                        + playpal_p[3 * i + 2]) / 3);
                src_r = src_g = src_b = gray;
            }
            else if (original_colormap)
            {
                const byte gray =
                    (byte)(0xff - (byte)(0.299 * playpal_p[3 * i + 0]
                                       + 0.587 * playpal_p[3 * i + 1]
                                       + 0.114 * playpal_p[3 * i + 2]));
                src_r = src_g = src_b = gray;
            }
            else
            {
                const byte idx = colormap[32 * 256 + i];
                src_r = playpal_p[3 * idx + 0];
                src_g = playpal_p[3 * idx + 1];
                src_b = playpal_p[3 * idx + 2];
            }

            byte pal[3], channels[3];
            pal[0] = (byte)BETWEEN(0, 255, (int)(src_r * vid_r_intensity));
            pal[1] = (byte)BETWEEN(0, 255, (int)(src_g * vid_g_intensity));
            pal[2] = (byte)BETWEEN(0, 255, (int)(src_b * vid_b_intensity));
            CALC_SATURATION(channels, pal, a_hi, a_lo);
            CALC_CONTRAST(channels, vid_contrast);
            CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);

            byte r = gammatable[vid_gamma][channels[0]];
            byte g = gammatable[vid_gamma][channels[1]];
            byte b = gammatable[vid_gamma][channels[2]];

            if (!vid_truecolor) { r &= ~3; g &= ~3; b &= ~3; }

            invrow[i] = 0xff000000 | (r << 16) | (g << 8) | b;
        }
    }
}

// -----------------------------------------------------------------------------
// I_Palette_BuildColor
//  [PN] Build pal_color (256 ARGB, no lighting) from a PLAYPAL palette.
// -----------------------------------------------------------------------------

void I_Palette_BuildColor (const byte *playpal_p, pixel_t *out)
{
    const int saturation = BETWEEN(0, 200, vid_saturation);
    const float a_hi = (saturation < 100) ? I_SaturationPercent[saturation]
                                          : -(float)(saturation - 100) * 0.0066f;
    const float a_lo = a_hi * 0.5f;

    for (int i = 0; i < 256; ++i)
    {
        byte pal[3], channels[3];
        CALC_INTENSITY(pal, playpal_p, i);
        CALC_SATURATION(channels, pal, a_hi, a_lo);
        CALC_CONTRAST(channels, vid_contrast);
        CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);

        const byte r = gammatable[vid_gamma][channels[0]];
        const byte g = gammatable[vid_gamma][channels[1]];
        const byte b = gammatable[vid_gamma][channels[2]];

        out[i] = 0xff000000 | (r << 16) | (g << 8) | b;
    }
}

// -----------------------------------------------------------------------------
// I_Palette_InitAll
//  [PN] Allocate + build every per-palette slot in one call.
// -----------------------------------------------------------------------------

void I_Palette_InitAll (const byte *playpal, const byte *colormap,
                        int numcolormaps, boolean original_colormap)
{
    I_Palette_Alloc(numcolormaps);

    for (int p = 0; p < pal_count; ++p)
    {
        const byte *const pal_p = playpal + p * 768;
        pixel_t *const cm = pal_colormaps[p];
        pixel_t *const pc = pal_color_p[p];

        if (cm) I_Palette_BuildOne(pal_p, colormap, numcolormaps, original_colormap, cm);
        if (pc) I_Palette_BuildColor(pal_p, pc);
    }
}

// -----------------------------------------------------------------------------
// I_Palette_InitAll
//  [PN] Swap the active palette to idx. 'current_base' is the game's current
//  colormaps pointer (used to detect no-op and to compute the delta passed to
//  'rebase'). Returns the new base pointer on success, or NULL if idx is
//  invalid or already active. Caller is responsible for assigning the result
//  to its own 'colormaps' global and reading I_Palette_GetColor(idx) for the
//  matching pal_color.
// -----------------------------------------------------------------------------

pixel_t *I_Palette_Swap (int idx, pixel_t *current_base,
                         I_Palette_RebaseFn rebase, void *ctx)
{
    if (idx < 0 || idx >= pal_count)
        return NULL;

    pixel_t *const new_cm = pal_colormaps[idx];

    if (!new_cm || new_cm == current_base)
        return NULL;

    if (rebase)
        rebase(current_base, new_cm, ctx);

    active_idx = idx;
    return new_cm;
}

// -----------------------------------------------------------------------------
// I_Palette_GetActive
//  [PN] Index of the last palette successfully swapped to (-1 if none yet).
// -----------------------------------------------------------------------------

int I_Palette_GetActive (void)
{
    return active_idx;
}
