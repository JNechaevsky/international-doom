//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2023 Julia Nechaevskaya
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
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//

#include <stdio.h>
#include <string.h>
#include <math.h>

#define MINIZ_NO_STDIO
#define MINIZ_NO_ZLIB_APIS
#include "miniz.h"

#include "i_system.h"
#include "doomtype.h"
#include "deh_str.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_config.h"
#include "m_misc.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "id_vars.h"


// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK

// Blending table used for fuzzpatch, etc.
// Only used in Heretic/Hexen
byte *tinttable = NULL;

// [JN] Blending tables for different translucency effects:
byte *tintmap = NULL;    // Used for sprites (80%)
byte *shadowmap = NULL;  // Used for shadowed texts (50%)
byte *fuzzmap = NULL;    // Used for translucent fuzz (30%)

// [JN] Color translation.
byte *dp_translation = NULL;
boolean dp_translucent = false;
#ifdef CRISPY_TRUECOLOR
extern pixel_t *colormaps;
#endif

// The screen buffer that the v_video.c code draws to.

static pixel_t *dest_screen = NULL;

int dirtybox[4]; 


//
// V_MarkRect 
// 

void V_MarkRect(int x, int y, int width, int height) 
{ 
    // If we are temporarily using an alternate screen, do not 
    // affect the update box.

    if (dest_screen == I_VideoBuffer)
    {
        M_AddToBox (dirtybox, x, y); 
        M_AddToBox (dirtybox, x + width-1, y + height-1); 
    }
} 

//
// V_CopyRect 
// 

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty)
{ 
    pixel_t *src;
    pixel_t *dest;
 
#ifdef RANGECHECK 
    if (srcx < 0
     || srcx + width > SCREENWIDTH
     || srcy < 0
     || srcy + height > SCREENHEIGHT 
     || destx < 0
     || destx /* + width */ > SCREENWIDTH
     || desty < 0
     || desty /* + height */ > SCREENHEIGHT)
    {
        // [JN] Note: should be I_Error, but use return instead
        // until status bar background buffer gets rewritten values.
        // I_Error ("Bad V_CopyRect");
        return;
    }
#endif 

    // [crispy] prevent framebuffer overflow
    if (destx + width > SCREENWIDTH)
	width = SCREENWIDTH - destx;
    if (desty + height > SCREENHEIGHT)
	height = SCREENHEIGHT - desty;

    V_MarkRect(destx, desty, width, height); 
 
    src = source + SCREENWIDTH * srcy + srcx; 
    dest = dest_screen + SCREENWIDTH * desty + destx; 

    for ( ; height>0 ; height--) 
    { 
        memcpy(dest, src, width * sizeof(*dest));
        src += SCREENWIDTH; 
        dest += SCREENWIDTH; 
    } 
} 

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
//

// [crispy] four different rendering functions
// for each possible combination of dp_translation and dp_translucent:
// (1) normal, opaque patch
static const inline pixel_t drawpatchpx00 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return source;}
#else
{return colormaps[source];}
#endif
// (2) color-translated, opaque patch
static const inline pixel_t drawpatchpx01 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return dp_translation[source];}
#else
{return colormaps[dp_translation[source]];}
#endif
// (3) normal, translucent patch
static const inline pixel_t drawpatchpx10 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return tintmap[(dest<<8)+source];}
#else
{return I_BlendOver(dest, colormaps[source]);}
#endif
// (4) color-translated, translucent patch
static const inline pixel_t drawpatchpx11 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return tintmap[(dest<<8)+dp_translation[source]];}
#else
{return I_BlendOver(dest, colormaps[dp_translation[source]]);}
#endif
// [JN] The shadow of the patch.
static const inline pixel_t drawshadow (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return shadowmap[(dest<<8)];}
#else
{return I_BlendDark(dest, 0x80);} // [JN] 128 (50%) of 256 full translucency.
#endif

// [crispy] TINTTAB rendering functions:
// (1) normal, translucent patch
static const inline pixel_t drawtinttab0 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return tinttable[(dest<<8)+source];}
#else
{return I_BlendOverTinttab(dest, colormaps[source]);}
#endif
// (2) translucent shadow only
static const inline pixel_t drawtinttab1 (const pixel_t dest, const pixel_t source)
#ifndef CRISPY_TRUECOLOR
{return tinttable[(dest<<8)];}
#else
{return I_BlendDark(dest, 0xB4);}
#endif


// [crispy] array of function pointers holding the different rendering functions
typedef const pixel_t drawpatchpx_t (const pixel_t dest, const pixel_t source);
static drawpatchpx_t *const drawpatchpx_a[2][2] = {{drawpatchpx11, drawpatchpx10}, {drawpatchpx01, drawpatchpx00}};
static drawpatchpx_t *const drawshadow_a = drawshadow;
// [crispy] array of function pointers holding the different TINTTAB rendering functions
static drawpatchpx_t *const drawtinttab_a[2] = {drawtinttab0, drawtinttab1};
static fixed_t dx, dxi, dy, dyi;

void V_DrawPatch(int x, int y, patch_t *patch)
{ 
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    byte *source;
    int w;

    // [crispy] four different rendering functions
    drawpatchpx_t *const drawpatchpx = drawpatchpx_a[!dp_translucent][!dp_translation];

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += WIDESCREENDELTA; // [crispy] horizontal widescreen offset

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    if (x < 0)
    {
	col += dxi * ((-x * dx) >> FRACBITS);
	x = 0;
    }

    desttop = dest_screen + ((y * dy) >> FRACBITS) * SCREENWIDTH + ((x * dx) >> FRACBITS);

    w = SHORT(patch->width);

    // convert x to screen position
    x = (x * dx) >> FRACBITS;

    for ( ; col<w << FRACBITS ; x++, col+=dxi, desttop++)
    {
        int topdelta = -1;

        // [crispy] too far right / width
        if (x >= SCREENWIDTH)
        {
            break;
        }

        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            int top, srccol = 0;
            // [crispy] support for DeePsea tall patches
            if (column->topdelta <= topdelta)
            {
                topdelta += column->topdelta;
            }
            else
            {
                topdelta = column->topdelta;
            }
            top = ((y + topdelta) * dy) >> FRACBITS;
            source = (byte *)column + 3;
            dest = desttop + ((topdelta * dy) >> FRACBITS)*SCREENWIDTH;
            count = (column->length * dy) >> FRACBITS;

            // [crispy] too low / height
            if (top + count > SCREENHEIGHT)
            {
                count = SCREENHEIGHT - top;
            }

            // [crispy] nothing left to draw?
            if (count < 1)
            {
                break;
            }

            while (count--)
            {
                // [crispy] too high
                if (top++ >= 0)
                {
                    *dest = drawpatchpx(*dest, source[srccol >> FRACBITS]);
                }
                srccol += dyi;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatch
// [JN] Masks a column based masked pic to the screen.
//  dest  - main patch, drawed second on top of shadow.
//  dest2 - shadow, drawed first below main patch.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatch(int x, int y, patch_t *patch)
{
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest, *dest2;
    byte *source;
    int w;

    // [JN] Simplify math for shadow placement.
    const int shadow_shift = (SCREENWIDTH + 1) << vid_hires;
    // [JN] Patch itself: opaque, can be colored.
    drawpatchpx_t *const drawpatchpx = drawpatchpx_a[!dp_translucent][!dp_translation];
    // [JN] Shadow: 50% translucent, no coloring used at all.
    drawpatchpx_t *const drawpatchpx2 = drawshadow_a;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += WIDESCREENDELTA; // [crispy] horizontal widescreen offset

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    if (x < 0)
    {
	col += dxi * ((-x * dx) >> FRACBITS);
	x = 0;
    }

    desttop = dest_screen + ((y * dy) >> FRACBITS) * SCREENWIDTH + ((x * dx) >> FRACBITS);

    w = SHORT(patch->width);

    // convert x to screen position
    x = (x * dx) >> FRACBITS;

    for ( ; col<w << FRACBITS ; x++, col+=dxi, desttop++)
    {
        int topdelta = -1;

        // [crispy] too far right / width
        if (x >= SCREENWIDTH)
        {
            break;
        }

        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            int top, srccol = 0;
            // [crispy] support for DeePsea tall patches
            if (column->topdelta <= topdelta)
            {
                topdelta += column->topdelta;
            }
            else
            {
                topdelta = column->topdelta;
            }
            top = ((y + topdelta) * dy) >> FRACBITS;
            source = (byte *)column + 3;
            dest = desttop + ((topdelta * dy) >> FRACBITS)*SCREENWIDTH;
            dest2 = dest + shadow_shift;
            count = (column->length * dy) >> FRACBITS;

            // [crispy] too low / height
            if (top + count > (SCREENHEIGHT-2))
            {
                count = (SCREENHEIGHT-2) - top;
            }

            // [crispy] nothing left to draw?
            if (count < 1)
            {
                break;
            }

            while (count--)
            {
                if (msg_text_shadows)
                {
                    *dest2 = drawpatchpx2(*dest2, source[srccol >> FRACBITS]);
                    dest2 += SCREENWIDTH;
                }

                // [crispy] too high
                if (top++ >= 0)
                {
                    *dest = drawpatchpx(*dest, source[srccol >> FRACBITS]);
                }
                srccol += dyi;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

void V_DrawShadowedPatchRaven(int x, int y, patch_t *patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    pixel_t *desttop2, *dest2;
    int w;

    // [crispy] four different rendering functions
    drawpatchpx_t *const drawpatchpx = drawpatchpx_a[!dp_translucent][!dp_translation];
    // [crispy] shadow, no coloring or color-translation are used
    drawpatchpx_t *const drawpatchpx2 = drawtinttab_a[!dp_translation];

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += WIDESCREENDELTA; // [crispy] horizontal widescreen offset

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    if (x < 0)
    {
	col += dxi * ((-x * dx) >> FRACBITS);
	x = 0;
    }

    desttop = dest_screen + ((y * dy) >> FRACBITS) * SCREENWIDTH + ((x * dx) >> FRACBITS);
    desttop2 = dest_screen + (((y + 2) * dy) >> FRACBITS) * SCREENWIDTH + (((x + 2) * dx) >> FRACBITS);

    w = SHORT(patch->width);

    // convert x to screen position
    x = (x * dx) >> FRACBITS;

    for ( ; col<w << FRACBITS ; x++, col+=dxi, desttop++, desttop2++)
    {
        int topdelta = -1;

        // [crispy] too far right / width
        if (x >= SCREENWIDTH)
        {
            break;
        }

        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            int top, srccol = 0;
            // [crispy] support for DeePsea tall patches
            if (column->topdelta <= topdelta)
            {
                topdelta += column->topdelta;
            }
            else
            {
                topdelta = column->topdelta;
            }
            top = ((y + topdelta) * dy) >> FRACBITS;
            source = (byte *)column + 3;
            dest = desttop + ((topdelta * dy) >> FRACBITS)*SCREENWIDTH;
            dest2 = desttop2 + ((column->topdelta * dy) >> FRACBITS) * SCREENWIDTH;
            count = (column->length * dy) >> FRACBITS;

            // [crispy] too low / height
            if (top + count > (SCREENHEIGHT-2))
            {
                count = (SCREENHEIGHT-2) - top;
            }

            // [crispy] nothing left to draw?
            if (count < 1)
            {
                break;
            }

            while (count--)
            {
                // [JN] TODO 
                //if (msg_text_shadows)
                {
                    *dest2 = drawpatchpx2(*dest2, source[srccol >> FRACBITS]);
                    dest2 += SCREENWIDTH;
                }

                // [crispy] too high
                if (top++ >= 0)
                {
                    *dest = drawpatchpx(*dest, source[srccol >> FRACBITS]);
                }
                srccol += dyi;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}


void V_DrawPatchFullScreen(patch_t *patch, boolean flipped)
{
    int x = ((SCREENWIDTH >> vid_hires) - SHORT(patch->width)) / 2 - WIDESCREENDELTA;

    patch->leftoffset = 0;
    patch->topoffset = 0;

    // [crispy] fill pillarboxes in widescreen mode
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
    }

    if (flipped)
    {
        V_DrawPatchFlipped(x, 0, patch);
    }
    else
    {
        V_DrawPatch(x, 0, patch);
    }
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//

void V_DrawPatchFlipped(int x, int y, patch_t *patch)
{
    int count;
    int col; 
    column_t *column; 
    pixel_t *desttop;
    pixel_t *dest;
    byte *source; 
    int w; 
 
    y -= SHORT(patch->topoffset); 
    x -= SHORT(patch->leftoffset); 
    x += WIDESCREENDELTA; // [crispy] horizontal widescreen offset

    V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    if (x < 0)
    {
	col += dxi * ((-x * dx) >> FRACBITS);
	x = 0;
    }

    desttop = dest_screen + ((y * dy) >> FRACBITS) * SCREENWIDTH + ((x * dx) >> FRACBITS);

    w = SHORT(patch->width);

    // convert x to screen position
    x = (x * dx) >> FRACBITS;

    for ( ; col<w << FRACBITS ; x++, col+=dxi, desttop++)
    {
        int topdelta = -1;

        // [crispy] too far left
        if (x < 0)
        {
            continue;
        }

        // [crispy] too far right / width
        if (x >= SCREENWIDTH)
        {
            break;
        }

        column = (column_t *)((byte *)patch + LONG(patch->columnofs[w-1-(col >> FRACBITS)]));

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            int top, srccol = 0;
            // [crispy] support for DeePsea tall patches
            if (column->topdelta <= topdelta)
            {
                topdelta += column->topdelta;
            }
            else
            {
                topdelta = column->topdelta;
            }
            top = ((y + topdelta) * dy) >> FRACBITS;
            source = (byte *)column + 3;
            dest = desttop + ((topdelta * dy) >> FRACBITS)*SCREENWIDTH;
            count = (column->length * dy) >> FRACBITS;

            // [crispy] too low / height
            if (top + count > SCREENHEIGHT)
            {
                count = SCREENHEIGHT - top;
            }

            // [crispy] nothing left to draw?
            if (count < 1)
            {
                break;
            }

            while (count--)
            {
                // [crispy] too high
                if (top++ >= 0)
                {
#ifndef CRISPY_TRUECOLOR
                    *dest = source[srccol >> FRACBITS];
#else
                    *dest = colormaps[source[srccol >> FRACBITS]];
#endif
                }
                srccol += dyi;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

//
// V_DrawTLPatch
//
// Masks a column based translucent masked pic to the screen.
//

void V_DrawTLPatch(int x, int y, patch_t * patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += WIDESCREENDELTA; // [crispy] horizontal widescreen offset

    if (x < 0
     || x + SHORT(patch->width) > (SCREENWIDTH >> vid_hires)
     || y < 0
     || y + SHORT(patch->height) > (SCREENHEIGHT >> vid_hires))
    {
        I_Error("Bad V_DrawTLPatch");
    }

    col = 0;
    desttop = dest_screen + ((y * dy) >> FRACBITS) * SCREENWIDTH + ((x * dx) >> FRACBITS);

    w = SHORT(patch->width);
    for (; col < w << FRACBITS; x++, col+=dxi, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col >> FRACBITS]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            int srccol = 0;
            source = (byte *) column + 3;
            dest = desttop + ((column->topdelta * dy) >> FRACBITS) * SCREENWIDTH;
            count = (column->length * dy) >> FRACBITS;

            while (count--)
            {
                *dest = tinttable[*dest + ((source[srccol >> FRACBITS]) << 8)];
                srccol += dyi;
                dest += SCREENWIDTH;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// [JN] V_FillFlat
// Fill background with given flat, independent from rendering resolution.
// -----------------------------------------------------------------------------

void V_FillFlat (const char *lump)
{
    int x, y;
    byte    *src = W_CacheLumpName(DEH_String(lump), PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    for (y = 0; y < SCREENHEIGHT; y++)
    {
        for (x = 0; x < SCREENWIDTH; x++)
        {
#ifndef CRISPY_TRUECOLOR
            *dest++ = src[(((y >> vid_hires) & 63) << 6) 
                         + ((x >> vid_hires) & 63)];
#else
            *dest++ = colormaps[src[(((y >> vid_hires) & 63) << 6) 
                               + ((x >> vid_hires) & 63)]];
#endif
        }
    }
}

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src)
{ 
    pixel_t *dest;
 
#ifdef RANGECHECK 
    if (x < 0
     || x + width >SCREENWIDTH
     || y < 0
     || y + height > SCREENHEIGHT)
    {
	I_Error ("Bad V_DrawBlock");
    }
#endif 
 
    V_MarkRect (x, y, width, height); 
 
    // dest = dest_screen + y * SCREENWIDTH + x; 
    dest = dest_screen + (y << vid_hires) * SCREENWIDTH + x;

    while (height--) 
    { 
	memcpy (dest, src, width * sizeof(*dest));
	src += width; 
	dest += SCREENWIDTH; 
    } 
} 

void V_DrawFilledBox(int x, int y, int w, int h, int c)
{
    pixel_t *buf, *buf1;
    int x1, y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1)
    {
        buf1 = buf;

        for (x1 = 0; x1 < w; ++x1)
        {
            *buf1++ = c;
        }

        buf += SCREENWIDTH;
    }
}

void V_DrawHorizLine(int x, int y, int w, int c)
{
    pixel_t *buf;
    int x1;

    // [crispy] prevent framebuffer overflows
    if (x + w > (unsigned)SCREENWIDTH)
	w = SCREENWIDTH - x;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (x1 = 0; x1 < w; ++x1)
    {
        *buf++ = c;
    }
}

void V_DrawVertLine(int x, int y, int h, int c)
{
    pixel_t *buf;
    int y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1)
    {
        *buf = c;
        buf += SCREENWIDTH;
    }
}

void V_DrawBox(int x, int y, int w, int h, int c)
{
    V_DrawHorizLine(x, y, w, c);
    V_DrawHorizLine(x, y+h-1, w, c);
    V_DrawVertLine(x, y, h, c);
    V_DrawVertLine(x+w-1, y, h, c);
}

//
// Draw a "raw" screen (lump containing raw data to blit directly
// to the screen)
//

void V_CopyScaledBuffer(pixel_t *dest, byte *src, size_t size)
{
    int i, j, index;

#ifdef RANGECHECK
    if (size > ORIGWIDTH * ORIGHEIGHT)
    {
        I_Error("Bad V_CopyScaledBuffer");
    }
#endif

    // [crispy] Fill pillarboxes in widescreen mode. Needs to be two separate
    // pillars to allow for Heretic finale vertical scrolling.
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, WIDESCREENDELTA << vid_hires, SCREENHEIGHT, 0);
        V_DrawFilledBox(SCREENWIDTH - (WIDESCREENDELTA << vid_hires), 0,
                        WIDESCREENDELTA << vid_hires, SCREENHEIGHT, 0);
    }

    index = ((size / ORIGWIDTH) << vid_hires) * SCREENWIDTH - 1;

    if (size % ORIGWIDTH)
    {
        // [crispy] Handles starting in the middle of a row.
        index += ((size % ORIGWIDTH) + WIDESCREENDELTA) << vid_hires;
    }
    else
    {
        index -= WIDESCREENDELTA << vid_hires;
    }

    while (size--)
    {
        for (i = 0; i <= vid_hires; i++)
        {
            for (j = 0; j <= vid_hires; j++)
            {
#ifndef CRISPY_TRUECOLOR
                *(dest + index - (j * SCREENWIDTH) - i) = *(src + size);
#else
                *(dest + index - (j * SCREENWIDTH) - i) = colormaps[src[size]];
#endif
            }
        }
        if (size % ORIGWIDTH == 0)
        {
            index -= 2 * (WIDESCREENDELTA << vid_hires)
                     + vid_hires * SCREENWIDTH;
        }
        index -= 1 + vid_hires;
    }
}
 
void V_DrawRawScreen(byte *raw)
{
    V_CopyScaledBuffer(dest_screen, raw, ORIGWIDTH * ORIGHEIGHT);
}

//
// Load tint table from TINTTAB lump.
//

void V_LoadTintTable(void)
{
    tinttable = W_CacheLumpName("TINTTAB", PU_STATIC);
}

// [crispy] For Heretic and Hexen widescreen support of replacement TITLE,
// HELP1, etc. These lumps are normally 320 x 200 raw graphics. If the lump
// size is larger than expected, proceed as if it were a patch.
void V_DrawFullscreenRawOrPatch(lumpindex_t index)
{
    patch_t *patch;

    patch = W_CacheLumpNum(index, PU_CACHE);

    if (W_LumpLength(index) == ORIGWIDTH * ORIGHEIGHT)
    {
        V_DrawRawScreen((byte*)patch);
    }
    else if ((SHORT(patch->height) == 200) && (SHORT(patch->width) >= 320))
    {
        V_DrawPatchFullScreen(patch, false);
    }
    else
    {
        I_Error("Invalid fullscreen graphic.");
    }
}

//
// V_Init
// 
void V_Init (void) 
{ 
    // [crispy] initialize resolution-agnostic patch drawing
    if (NONWIDEWIDTH && SCREENHEIGHT)
    {
        dx = (NONWIDEWIDTH << FRACBITS) / ORIGWIDTH;
        dxi = (ORIGWIDTH << FRACBITS) / NONWIDEWIDTH;
        dy = (SCREENHEIGHT << FRACBITS) / ORIGHEIGHT;
        dyi = (ORIGHEIGHT << FRACBITS) / SCREENHEIGHT;
    }
    // no-op!
    // There used to be separate screens that could be drawn to; these are
    // now handled in the upper layers.
}

// Set the buffer that the code draws to.

void V_UseBuffer(pixel_t *buffer)
{
    dest_screen = buffer;
}

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void)
{
    dest_screen = I_VideoBuffer;
}

//
// SCREEN SHOTS
//


//
// WritePNGfile
//

void WritePNGfile (char *filename)
{
    byte *data;
    int width, height;
    size_t png_data_size = 0;

    I_RenderReadPixels(&data, &width, &height);
    {
        void *pPNG_data = tdefl_write_image_to_png_file_in_memory(data, width, height, 4, &png_data_size);

        if (!pPNG_data)
        {
            return;
        }
        else
        {
            FILE *handle = M_fopen(filename, "wb");
            fwrite(pPNG_data, 1, png_data_size, handle);
            fclose(handle);
            mz_free(pPNG_data);
        }
    }

    free(data);
}

//
// V_ScreenShot
//

void V_ScreenShot(char *format)
{
    int i;
    char lbmname[16]; // haleyjd 20110213: BUG FIX - 12 is too small!
    char *file;
    
    // find a file name to save it to

    for (i=0; i<=9999; i++)
    {
        M_snprintf(lbmname, sizeof(lbmname), format, i, "png");
        // [JN] Construct full path to screenshot file.
        file = M_StringJoin(screenshotdir, lbmname, NULL);

        if (!M_FileExists(file))
        {
            break;      // file doesn't exist
        }
    }

    if (i == 10000)
    {
        I_Error ("V_ScreenShot: Couldn't create a PNG");
    }

    WritePNGfile(file);
}

#define MOUSE_SPEED_BOX_WIDTH  120
#define MOUSE_SPEED_BOX_HEIGHT 9
#define MOUSE_SPEED_BOX_X (SCREENWIDTH - MOUSE_SPEED_BOX_WIDTH - 10)
#define MOUSE_SPEED_BOX_Y 15

//
// V_DrawMouseSpeedBox
//

static void DrawAcceleratingBox(int speed)
{
    int red, white, yellow;
    int original_speed;
    int redline_x;
    int linelen;

#ifndef CRISPY_TRUECOLOR
    red = I_GetPaletteIndex(0xff, 0x00, 0x00);
    white = I_GetPaletteIndex(0xff, 0xff, 0xff);
    yellow = I_GetPaletteIndex(0xff, 0xff, 0x00);
#else
    red = I_MapRGB(0xff, 0x00, 0x00);
    white = I_MapRGB(0xff, 0xff, 0xff);
    yellow = I_MapRGB(0xff, 0xff, 0x00);
#endif

    // Calculate the position of the red threshold line when calibrating
    // acceleration.  This is 1/3 of the way along the box.

    redline_x = MOUSE_SPEED_BOX_WIDTH / 3;

    if (speed >= mouse_threshold)
    {
        // Undo acceleration and get back the original mouse speed
        original_speed = speed - mouse_threshold;
        original_speed = (int) (original_speed / mouse_acceleration);
        original_speed += mouse_threshold;

        linelen = (original_speed * redline_x) / mouse_threshold;
    }
    else
    {
        linelen = (speed * redline_x) / mouse_threshold;
    }

    // Horizontal "thermometer"
    if (linelen > MOUSE_SPEED_BOX_WIDTH - 1)
    {
        linelen = MOUSE_SPEED_BOX_WIDTH - 1;
    }

    if (linelen < redline_x)
    {
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1,
                        MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        linelen, white);
    }
    else
    {
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1,
                        MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        redline_x, white);
        V_DrawHorizLine(MOUSE_SPEED_BOX_X + redline_x,
                        MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        linelen - redline_x, yellow);
    }

    // Draw acceleration threshold line
    V_DrawVertLine(MOUSE_SPEED_BOX_X + redline_x, MOUSE_SPEED_BOX_Y + 1,
                   MOUSE_SPEED_BOX_HEIGHT - 2, red);
}

// Highest seen mouse turn speed. We scale the range of the thermometer
// according to this value, so that it never exceeds the range. Initially
// this is set to a 1:1 setting where 1 pixel = 1 unit of speed.
static int max_seen_speed = MOUSE_SPEED_BOX_WIDTH - 1;

static void DrawNonAcceleratingBox(int speed)
{
    int white;
    int linelen;

#ifndef CRISPY_TRUECOLOR
    white = I_GetPaletteIndex(0xff, 0xff, 0xff);
#else
    white = I_MapRGB(0xff, 0xff, 0xff);
#endif

    if (speed > max_seen_speed)
    {
        max_seen_speed = speed;
    }

    // Draw horizontal "thermometer":
    linelen = speed * (MOUSE_SPEED_BOX_WIDTH - 1) / max_seen_speed;

    V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1,
                    MOUSE_SPEED_BOX_Y + MOUSE_SPEED_BOX_HEIGHT / 2,
                    linelen, white);
}

void V_DrawMouseSpeedBox(int speed)
{
    int bgcolor, bordercolor, black;

    // If the mouse is turned off, don't draw the box at all.
    if (!usemouse)
    {
        return;
    }

    // Get palette indices for colors for widget. These depend on the
    // palette of the game being played.

#ifndef CRISPY_TRUECOLOR
    bgcolor = I_GetPaletteIndex(0x77, 0x77, 0x77);
    bordercolor = I_GetPaletteIndex(0x55, 0x55, 0x55);
    black = I_GetPaletteIndex(0x00, 0x00, 0x00);
#else
    bgcolor = I_MapRGB(0x77, 0x77, 0x77);
    bordercolor = I_MapRGB(0x55, 0x55, 0x55);
    black = I_MapRGB(0x00, 0x00, 0x00);
#endif

    // Calculate box position

    V_DrawFilledBox(MOUSE_SPEED_BOX_X, MOUSE_SPEED_BOX_Y,
                    MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT, bgcolor);
    V_DrawBox(MOUSE_SPEED_BOX_X, MOUSE_SPEED_BOX_Y,
              MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT, bordercolor);
    V_DrawHorizLine(MOUSE_SPEED_BOX_X + 1, MOUSE_SPEED_BOX_Y + 4,
                    MOUSE_SPEED_BOX_WIDTH - 2, black);

    // If acceleration is used, draw a box that helps to calibrate the
    // threshold point.
    if (mouse_threshold > 0 && fabs(mouse_acceleration - 1) > 0.01)
    {
        DrawAcceleratingBox(speed);
    }
    else
    {
        DrawNonAcceleratingBox(speed);
    }
}

