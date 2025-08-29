//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
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

#include <stdlib.h>
#include "doomdef.h"
#include "deh_str.h"
#include "r_local.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_trans.h" // [crispy] blending functions

#include "id_vars.h"


// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Draw translucent column, overlay blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumn_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap  = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest =
        ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    const int a  = 96;      // 0..256 (I_BlendOver_96)
    const int ia = 256 - a; // 0..256

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];

        // Источник в truecolor с учётом brightmap
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        // Смешиваем R|B вместе и G отдельно (по маскам).
        // Вариант без 64-бит, быстрый, допускает переполнение мод 2^32 (обычно ок на x86):
        // Если хочешь «безопасно», раскомментируй 64-бит блок ниже.

        // --- быстрый 32-битный ---
        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        // --- безопасный 64-битный (чуть дороже, раскомментируй при желании) ---
        // uint32_t rb = (uint32_t)(((((uint64_t)fgRB) * a + ((uint64_t)bgRB) * ia) >> 8) & 0x00FF00FFu));
        // uint32_t g8 = (uint32_t)(((((uint64_t)fgG ) * a + ((uint64_t)bgG ) * ia) >> 8) & 0x0000FF00u));

        int r = (int)(rb >> 16) & 0xFF;
        int g = (int)(g8 >> 8)  & 0xFF;
        int b = (int)(rb       & 0xFF);

        // Мэппинг в палитру через 3D-LUT
        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);

        // Финальный цвет через текущий pal_color (как и раньше)
        const pixel_t blended = pal_color[pal_index];

        dest[0]           = blended;
        dest[screenwidth] = blended;

        dest   += screenwidth * step;
        frac   += fracstep;
        y_start += step;
    }

    // Хвост на нечётной высоте
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        int r = (int)(rb >> 16) & 0xFF;
        int g = (int)(g8 >> 8)  & 0xFF;
        int b = (int)(rb       & 0xFF);

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        dest[0] = pal_color[pal_index];
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Draw translucent column, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumnLow_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers
    const byte *restrict const sourcebase  = dc_source;
    const byte *restrict const brightmap   = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Two destination columns
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    const int a  = 96;      // 0..256 (I_BlendOver_96)
    const int ia = 256 - a; // 0..256

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // ---- Column 1 (blend with its own background) ----
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (int)(rb >> 16) & 0xFF;
            int g = (int)(g8 >> 8)  & 0xFF;
            int b = (int)(rb       & 0xFF);

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // ---- Column 2 (its own background) ----
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (int)(rb >> 16) & 0xFF;
            int g = (int)(g8 >> 8)  & 0xFF;
            int b = (int)(rb       & 0xFF);

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        // Next pair of lines/advance
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    // Final single line (odd height): draw one line for both columns
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (int)(rb >> 16) & 0xFF;
            int g = (int)(g8 >> 8)  & 0xFF;
            int b = (int)(rb       & 0xFF);

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest1[0] = pal_color[pal_index];
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (int)(rb >> 16) & 0xFF;
            int g = (int)(g8 >> 8)  & 0xFF;
            int b = (int)(rb       & 0xFF);

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest2[0] = pal_color[pal_index];
        }
    }
}










// ===================================================================

void R_DrawTLAddColumn_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest =
        ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        // Извлекаем каналы
        const int fg_r = (fg >> 16) & 0xFF;
        const int fg_g = (fg >> 8)  & 0xFF;
        const int fg_b =  fg        & 0xFF;

        const int bg_r = (bg >> 16) & 0xFF;
        const int bg_g = (bg >> 8)  & 0xFF;
        const int bg_b =  bg        & 0xFF;

        // Быстрое аддитивное смешение через LUT по каждому каналу
        const int r = addchan_lut[(bg_r << 8) | fg_r];
        const int g = addchan_lut[(bg_g << 8) | fg_g];
        const int b = addchan_lut[(bg_b << 8) | fg_b];

        // Мэппинг в палитру (гамма-осведомлённый)
        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);

        // Пишем базовый палитровый цвет (уже gamma-corrected)
        const pixel_t blended = pal_color[pal_index];

        dest[0]           = blended;
        dest[screenwidth] = blended;

        dest   += screenwidth * step;
        frac   += fracstep;
        y_start += step;
    }

    // Хвост при нечётной высоте
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        const int fg_r = (fg >> 16) & 0xFF;
        const int fg_g = (fg >> 8)  & 0xFF;
        const int fg_b =  fg        & 0xFF;

        const int bg_r = (bg >> 16) & 0xFF;
        const int bg_g = (bg >> 8)  & 0xFF;
        const int bg_b =  bg        & 0xFF;

        const int r = addchan_lut[(bg_r << 8) | fg_r];
        const int g = addchan_lut[(bg_g << 8) | fg_g];
        const int b = addchan_lut[(bg_b << 8) | fg_b];

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        dest[0] = pal_color[pal_index];
    }
}

void R_DrawTLAddColumnLow_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // ---- Column 1 ----
        {
            const pixel_t bg = *dest1;

            const int fg_r = (fg >> 16) & 0xFF;
            const int fg_g = (fg >> 8)  & 0xFF;
            const int fg_b =  fg        & 0xFF;

            const int bg_r = (bg >> 16) & 0xFF;
            const int bg_g = (bg >> 8)  & 0xFF;
            const int bg_b =  bg        & 0xFF;

            const int r = addchan_lut[(bg_r << 8) | fg_r];
            const int g = addchan_lut[(bg_g << 8) | fg_g];
            const int b = addchan_lut[(bg_b << 8) | fg_b];

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // ---- Column 2 ----
        {
            const pixel_t bg = *dest2;

            const int fg_r = (fg >> 16) & 0xFF;
            const int fg_g = (fg >> 8)  & 0xFF;
            const int fg_b =  fg        & 0xFF;

            const int bg_r = (bg >> 16) & 0xFF;
            const int bg_g = (bg >> 8)  & 0xFF;
            const int bg_b =  bg        & 0xFF;

            const int r = addchan_lut[(bg_r << 8) | fg_r];
            const int g = addchan_lut[(bg_g << 8) | fg_g];
            const int b = addchan_lut[(bg_b << 8) | fg_b];

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        // Переход к следующей паре строк
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    // Последняя строка при нечётной высоте
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;

            const int fg_r = (fg >> 16) & 0xFF;
            const int fg_g = (fg >> 8)  & 0xFF;
            const int fg_b =  fg        & 0xFF;

            const int bg_r = (bg >> 16) & 0xFF;
            const int bg_g = (bg >> 8)  & 0xFF;
            const int bg_b =  bg        & 0xFF;

            const int r = addchan_lut[(bg_r << 8) | fg_r];
            const int g = addchan_lut[(bg_g << 8) | fg_g];
            const int b = addchan_lut[(bg_b << 8) | fg_b];

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest1[0] = pal_color[pal_index];
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;

            const int fg_r = (fg >> 16) & 0xFF;
            const int fg_g = (fg >> 8)  & 0xFF;
            const int fg_b =  fg        & 0xFF;

            const int bg_r = (bg >> 16) & 0xFF;
            const int bg_g = (bg >> 8)  & 0xFF;
            const int bg_b =  bg        & 0xFF;

            const int r = addchan_lut[(bg_r << 8) | fg_r];
            const int g = addchan_lut[(bg_g << 8) | fg_g];
            const int b = addchan_lut[(bg_b << 8) | fg_b];

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest2[0] = pal_color[pal_index];
        }
    }
}

























void R_DrawTranslatedTLColumn_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    const int a  = 96;      // 0..256 (I_BlendOver_96)
    const int ia = 256 - a; // 0..256

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];
        const pixel_t bg = *dest;

        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        int r = (rb >> 16) & 0xFF;
        int g = (g8 >> 8)  & 0xFF;
        int b =  rb        & 0xFF;

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        const pixel_t blended = pal_color[pal_index];

        dest[0]           = blended;
        dest[screenwidth] = blended;

        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];
        const pixel_t bg = *dest;

        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        int r = (rb >> 16) & 0xFF;
        int g = (g8 >> 8)  & 0xFF;
        int b =  rb        & 0xFF;

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        dest[0] = pal_color[pal_index];
    }
}

void R_DrawTranslatedTLColumnLow_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const int x = dc_x << 1;

    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    const int a  = 96;      // 0..256 (I_BlendOver_96)
    const int ia = 256 - a; // 0..256

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];

        // --- Column 1 ---
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // --- Column 2 ---
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];

        // Column 1
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest1[0] = pal_color[pal_index];
        }

        // Column 2
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest2[0] = pal_color[pal_index];
        }
    }
}





















void R_DrawExtraTLColumn_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap  = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    const int a  = 142;         // ≈56% opacity
    const int ia = 256 - a;

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        int r = (rb >> 16) & 0xFF;
        int g = (g8 >> 8)  & 0xFF;
        int b =  rb        & 0xFF;

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        const pixel_t blended = pal_color[pal_index];

        dest[0]           = blended;
        dest[screenwidth] = blended;

        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        const uint32_t fgRB = fg & 0x00FF00FFu;
        const uint32_t bgRB = bg & 0x00FF00FFu;
        const uint32_t fgG  = fg & 0x0000FF00u;
        const uint32_t bgG  = bg & 0x0000FF00u;

        uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
        uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

        int r = (rb >> 16) & 0xFF;
        int g = (g8 >> 8)  & 0xFF;
        int b =  rb        & 0xFF;

        const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
        dest[0] = pal_color[pal_index];
    }
}

void R_DrawExtraTLColumnLow_8(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    const int x = dc_x << 1;

    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap  = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    const int a  = 142;
    const int ia = 256 - a;

    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // --- Column 1 ---
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // --- Column 2 ---
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            const pixel_t blended = pal_color[pal_index];

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest1[0] = pal_color[pal_index];
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;

            const uint32_t fgRB = fg & 0x00FF00FFu;
            const uint32_t bgRB = bg & 0x00FF00FFu;
            const uint32_t fgG  = fg & 0x0000FF00u;
            const uint32_t bgG  = bg & 0x0000FF00u;

            uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
            uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

            int r = (rb >> 16) & 0xFF;
            int g = (g8 >> 8)  & 0xFF;
            int b =  rb        & 0xFF;

            const byte pal_index = RGB8_TO_PAL_FAST(r, g, b);
            dest2[0] = pal_color[pal_index];
        }
    }
}


