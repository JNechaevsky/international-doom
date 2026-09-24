//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
// Copyright(C) 2025 Polina "Aura" N.
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

#include <stdio.h>
#include <string.h>
#include <math.h>

#define MINIZ_NO_STDIO
#define MINIZ_NO_ZLIB_APIS
#include "miniz.h"

// [PN] Provide prototypes for internal stb_image_write functions to silence -Wmissing-prototypes
static unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);
static unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "doomtype.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_config.h"
#include "m_misc.h"
#include "z_zone.h"

#include "id_vars.h"


// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK


// [JN] Color translation.
byte *dp_translation = NULL;
boolean dp_translucent = false;

// [crispy] array holding palette colors for true color mode
pixel_t *pal_color;

// The screen buffer that the v_video.c code draws to.
static pixel_t *dest_screen = NULL;

// [crispy] resolution-agnostic patch drawing
static fixed_t dx, dxi, dy, dyi;

// [PN] Clean screenshot schedule flag.
boolean cleanshot_pending  = false;

// -----------------------------------------------------------------------------
// V_CopyRect
// [PN] Transposed layout: a rectangle is a run of y-contiguous pixels
// (stride 1) per column, columns spaced SCREENHEIGHT apart. All buffers
// passed here are full-screen-sized and share the transposed layout.
// -----------------------------------------------------------------------------

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty)
{
    // ---- Hot globals cached locally ----
    const int     sw   = SCREENWIDTH;
    const int     sh   = SCREENHEIGHT;
    pixel_t      *dst0 = dest_screen;

#ifdef RANGECHECK
    if (srcx  < 0 || srcx + width  > sw ||
        srcy  < 0 || srcy + height > sh ||
        destx < 0 || destx         > sw ||
        desty < 0 || desty         > sh)
    {
        // keep original semantics: early return instead of I_Error
        return;
    }
#endif

    // Prevent framebuffer overflow (crispy semantics)
    if (destx + width  > sw) width  = sw - destx;
    if (desty + height > sh) height = sh - desty;

    if (width <= 0 || height <= 0)
        return;

    const size_t col_bytes = (size_t)height * sizeof(pixel_t);

    // Same-buffer copy: columns never alias each other in the transposed
    // layout, but overlapping x ranges need a safe traversal direction;
    // y overlap within one column is handled with memmove.
    const boolean same_buffer = (source == dst0);
    const boolean y_overlap   = same_buffer
                             && desty < srcy + height && srcy < desty + height;
    const boolean back_cols   = same_buffer && destx > srcx
                             && destx < srcx + width;

    for (int n = 0; n < width; ++n)
    {
        const int i = back_cols ? width - 1 - n : n;
        pixel_t *const src  = source + (srcx  + i) * sh + srcy;
        pixel_t *const dest = dst0   + (destx + i) * sh + desty;

        if (y_overlap)
            memmove(dest, src, col_bytes);
        else
            memcpy (dest, src, col_bytes);
    }
}

// -----------------------------------------------------------------------------
// V_DrawPatch
// Masks a column based masked pic to the screen.
// -----------------------------------------------------------------------------

void V_DrawPatch(int x, int y, patch_t *patch)
{
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    byte *source;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;
    const boolean  use_tc  = vid_truecolor;

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Fast-path flags/pointers (constant for the whole call).
    const boolean use_trans = dp_translucent;
    const byte *restrict xlat = dp_translation;

    // Position patch (Crispy-Doom style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Left clipping in fixed-point column space.
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // Compute top pointer of the first column on the destination buffer.
    desttop = dst_screen
            + ((x * ldx) >> FRACBITS) * sh
            + ((y * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Iterate columns in fixed-point (scaled drawing).
    for (; col < (w << FRACBITS); x++, col += ldxi, desttop += sh)
    {
        int topdelta = -1;

        // Right clipping: nothing more to draw on this scanline.
        if (x >= sw)
            break;

        // Column pointer from patch data.
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Compute starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;
            dest   = desttop + ((topdelta * ldy) >> FRACBITS);
            count  = (column->length * ldy) >> FRACBITS;

            // Bottom clip against screen height.
            if (top + count > sh)
                count = sh - top;

            // If entirely below the screen, stop scanning this column early.
            if (count < 1)
                break;

            // Top clip once (remove per-pixel "if (top++ >= 0)").
            if (top < 0)
            {
                const int skip = -top;
                if (skip >= count)
                {
                    // Whole post is above the screen; advance to next post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }
                dest   += skip;
                srccol += ldyi * skip;
                count  -= skip;
                // top is effectively 0 now
            }

            // Hot loop: four specialized paths (opaque/translated/translucent).
            if (!use_trans)            // Opaque
            {
                if (!xlat)             // Opaque, no translation
                {
                    while (count--)
                    {
                        *dest++ = pal[source[srccol >> FRACBITS]];
                        srccol += ldyi;
                    }
                }
                else                    // Opaque, translated
                {
                    while (count--)
                    {
                        *dest++ = pal[xlat[source[srccol >> FRACBITS]]];
                        srccol += ldyi;
                    }
                }
            }
            else                        // Translucent (50% over dest)
            {
                if (!xlat)              // Translucent, no translation
                {
                    while (count--)
                    {
                        const byte s = source[srccol >> FRACBITS];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
                else                    // Translucent, translated
                {
                    while (count--)
                    {
                        const byte s = xlat[source[srccol >> FRACBITS]];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatch
//  Masks a column based masked pic to the screen.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatch(int x, int y, patch_t *patch)
{
    int count, count2;
    int col;
    column_t *column;
    pixel_t *desttop, *desttop2;
    pixel_t *dest, *dest2;
    byte *source;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;
    const boolean  use_tc  = vid_truecolor;
    const int      vres    = vid_resolution; // used by shadow clamp

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Patch flags/pointers (constant for the whole call).
    const boolean use_trans = dp_translucent;
    const byte *restrict xlat = dp_translation;

    // Position patch (Crispy-style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Left clipping in fixed-point column space.
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // [PN] Transposed: main column pointer plus shadow pointer offset by
    // (+2, +2) in screen space -> x and y contributions separately.
    desttop  = dst_screen + ((x * ldx) >> FRACBITS) * sh + ((y * ldy) >> FRACBITS);
    desttop2 = dst_screen + (((x + 2) * ldx) >> FRACBITS) * sh + (((y + 2) * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Precompute the bottom limit for the shadow (accounts for +2*vres shift).
    const int sh_limit_shadow = sh - (2 * vres);

    // Iterate columns in fixed-point (scaled drawing).
    for ( ; col < (w << FRACBITS) ; x++, col += ldxi, desttop += sh, desttop2 += sh)
    {
        int topdelta = -1;

        // Right clipping: nothing more to draw on this scanline.
        if (x >= sw)
            break;

        // Column pointer from patch data.
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Compute starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;

            dest   = desttop  + ((topdelta * ldy) >> FRACBITS);
            dest2  = desttop2 + ((topdelta * ldy) >> FRACBITS);

            count  = (column->length * ldy) >> FRACBITS;
            count2 = count;

            // Bottom clip against screen height for shadow and patch.
            if (top + count2 > sh_limit_shadow)
                count2 = sh_limit_shadow - top;
            if (top + count > sh)
                count = sh - top;

            // Nothing left to draw for this post?
            if (count < 1 || count2 < 1)
            {
                column = (column_t *)((byte *)column + column->length + 4);
                continue;
            }

            // Top clip once for both shadow and patch (removes per-pixel 'top>=0').
            if (top < 0)
            {
                const int skip = -top;

                if (skip >= count || skip >= count2)
                {
                    // Entire post above the screen in either stream; skip post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }

                dest   += skip;
                dest2  += skip;
                srccol += ldyi * skip;

                count  -= skip;
                count2 -= skip;

                // top is effectively 0 now
            }

            // --- Draw shadow first (special routine), no per-pixel top checks ---
            // Source index is not needed for darkening; we blend a constant alpha.
            {
                int n = count2;
                if (use_tc)
                {
                    while (n--) { *dest2 = I_BlendDark_32(*dest2, shadow_alpha); dest2++; }
                }
                else
                {
                    while (n--) { *dest2 = I_BlendDark_8 (*dest2, shadow_alpha); dest2++; }
                }
            }

            // --- Draw the actual patch (four specialized fast paths) ---
            if (!use_trans)            // Opaque
            {
                if (!xlat)             // Opaque, no translation
                {
                    int n = count;
                    while (n--)
                    {
                        *dest++ = pal[source[srccol >> FRACBITS]];
                        srccol += ldyi;
                    }
                }
                else                    // Opaque, translated
                {
                    int n = count;
                    while (n--)
                    {
                        *dest++ = pal[xlat[source[srccol >> FRACBITS]]];
                        srccol += ldyi;
                    }
                }
            }
            else                        // Translucent over destination
            {
                if (!xlat)              // Translucent, no translation
                {
                    int n = count;
                    while (n--)
                    {
                        const byte s = source[srccol >> FRACBITS];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
                else                    // Translucent, translated
                {
                    int n = count;
                    while (n--)
                    {
                        const byte s = xlat[source[srccol >> FRACBITS]];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatchNoOffsets
//  Zero-out predefined sprite offsets to use only function-defined ones.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatchNoOffsets(int x, int y, patch_t *patch)
{
    patch->topoffset = 0;
    patch->leftoffset = 0;
    V_DrawShadowedPatch(x, y, patch);
}

// -----------------------------------------------------------------------------
// V_DrawShadowedOptional
//  Draws patch with shadow if "Text casts shadows" feature is enabled.
//   dest  - main patch, drawed second on top of shadow.
//   dest2 - shadow, drawed first below main patch.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatchOptional(int x, int y, int shadow_type, patch_t *patch)
{
    int count, count2, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    pixel_t *dest2;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;
    const boolean  use_tc  = vid_truecolor;
    const int      vres    = vid_resolution;
    const boolean  do_shadow = msg_text_shadows;

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Patch flags/pointers (constant for the whole call).
    const boolean use_trans = dp_translucent;
    const byte *restrict xlat = dp_translation;

    // Shadow blend function (picked once, inlined).
    const int sa = shadow_alpha; (void)shadow_type; // keep param; alpha is the same

    // Shadow placement helper: (+1 vres, +1 vres) screen offset in transposed address space.
    const int shadow_shift = (sh + 1) * vres;

    // Position patch (Crispy-style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Left clipping in fixed-point column space.
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // Compute top pointer of the first column on the destination buffer.
    desttop = dst_screen + ((x * ldx) >> FRACBITS) * sh + ((y * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Shadow bottom limit (accounts for vertical shift by vres).
    const int sh_limit_shadow = sh - (1 * vres);

    // Iterate columns in fixed-point (scaled drawing).
    for ( ; col < (w << FRACBITS) ; x++, col += ldxi, desttop += sh)
    {
        int topdelta = -1;

        // Right clipping: nothing more to draw on this scanline.
        if (x >= sw)
            break;

        // Column pointer from patch data.
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;

            dest  = desttop + ((topdelta * ldy) >> FRACBITS);
            dest2 = dest + shadow_shift;

            count  = (column->length * ldy) >> FRACBITS;
            count2 = count;

            // Bottom clip: shadow vs main patch have different limits.
            if (top + count2 > sh_limit_shadow)
                count2 = sh_limit_shadow - top;
            if (top + count > sh)
                count = sh - top;

            // Nothing left for this post? Skip to the next post.
            if (count < 1 || count2 < 1)
            {
                column = (column_t *)((byte *)column + column->length + 4);
                continue;
            }

            // Top clip once (removes per-pixel 'if (top++>=0)' branches).
            if (top < 0)
            {
                const int skip = -top;

                if (skip >= count || skip >= count2)
                {
                    // Whole post is above the screen; advance to next post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }

                dest   += skip;
                dest2  += skip;
                srccol += ldyi * skip;
                count  -= skip;
                count2 -= skip;
                // top is effectively 0 now
            }

            // --- Draw shadow first (optional), no per-pixel top checks ---
            if (do_shadow)
            {
                // Note: previously used drawshadow_*; now inlined dark blend.
                // Source index was not used for darkening; we blend a constant alpha.
                int n = count2;
                if (use_tc)
                {
                    while (n--) { *dest2 = I_BlendDark_32(*dest2, sa); dest2++; }
                }
                else
                {
                    while (n--) { *dest2 = I_BlendDark_8 (*dest2, sa); dest2++; }
                }
            }

            // --- Draw the actual patch (four specialized fast paths) ---
            if (!use_trans)            // Opaque
            {
                if (!xlat)             // Opaque, no translation
                {
                    int n = count;
                    while (n--)
                    {
                        *dest++ = pal[source[srccol >> FRACBITS]];
                        srccol += ldyi;
                    }
                }
                else                    // Opaque, translated
                {
                    int n = count;
                    while (n--)
                    {
                        *dest++ = pal[xlat[source[srccol >> FRACBITS]]];
                        srccol += ldyi;
                    }
                }
            }
            else                        // Translucent over destination
            {
                if (!xlat)              // Translucent, no translation
                {
                    int n = count;
                    while (n--)
                    {
                        const byte s = source[srccol >> FRACBITS];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
                else                    // Translucent, translated
                {
                    int n = count;
                    while (n--)
                    {
                        const byte s = xlat[source[srccol >> FRACBITS]];
                        const pixel_t fg = pal[s];
                        *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                       : I_BlendOver128_8 (*dest, fg);
                        srccol += ldyi;
                        dest++;
                    }
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatchOptionalFade
//  [PN] Draw patch and optional shadow with explicit alpha fade (0..255).
// -----------------------------------------------------------------------------

void V_DrawShadowedPatchOptionalFade(int x, int y, int shadow_type, patch_t *patch, int alpha)
{
    int count, count2, col; 
    column_t *column; 
    pixel_t *desttop, *dest; 
    byte *source; 
    pixel_t *dest2; 
    int w; 
 
    const int      sw       = SCREENWIDTH; 
    const int      sh       = SCREENHEIGHT; 
    const int      ws_delta = WIDESCREENDELTA; 
    pixel_t *restrict dst_screen = dest_screen; 
    const pixel_t *restrict pal  = pal_color; 
    const boolean  use_tc  = vid_truecolor; 
    const int      vres    = vid_resolution; 
    const boolean  do_shadow = msg_text_shadows; 
 
    const fixed_t ldx  = dx; 
    const fixed_t ldy  = dy; 
    const fixed_t ldxi = dxi; 
    const fixed_t ldyi = dyi; 
 
    const byte *restrict xlat = dp_translation; 
 
    if (alpha <= 0) 
    { 
        return; 
    } 
 
    if (alpha >= 255) 
    { 
        V_DrawShadowedPatchOptional(x, y, shadow_type, patch); 
        return; 
    } 
 
    // For I_BlendDark: lower amount means darker shadow.
    // Therefore, to fade shadow out with text alpha we must move amount towards 255
    // as alpha goes to 0 (instead of towards 0).
    const int sa = 255 - (((255 - shadow_alpha) * alpha) / 255); (void)shadow_type; 
 
    const int shadow_shift = (sh + 1) * vres;
 
    y -= SHORT(patch->topoffset); 
    x -= SHORT(patch->leftoffset); 
    x += ws_delta; 
 
    col = 0; 
    if (x < 0) 
    { 
        col += ldxi * ((-x * ldx) >> FRACBITS); 
        x = 0; 
    } 
 
    desttop = dst_screen + ((x * ldx) >> FRACBITS) * sh + ((y * ldy) >> FRACBITS); 
 
    w = SHORT(patch->width); 
    x = (x * ldx) >> FRACBITS; 
 
    const int sh_limit_shadow = sh - (1 * vres); 
 
    for ( ; col < (w << FRACBITS) ; x++, col += ldxi, desttop += sh) 
    { 
        int topdelta = -1; 
 
        if (x >= sw) 
            break; 
 
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS])); 
 
        while (column->topdelta != 0xff) 
        { 
            int top; 
            int srccol = 0; 
 
            if (column->topdelta <= topdelta) 
                topdelta += column->topdelta; 
            else 
                topdelta = column->topdelta; 
 
            top    = ((y + topdelta) * ldy) >> FRACBITS; 
            source = (byte *)column + 3; 
 
            dest  = desttop + ((topdelta * ldy) >> FRACBITS); 
            dest2 = dest + shadow_shift; 
 
            count  = (column->length * ldy) >> FRACBITS; 
            count2 = count; 
 
            if (top + count2 > sh_limit_shadow) 
                count2 = sh_limit_shadow - top; 
            if (top + count > sh) 
                count = sh - top; 
 
            if (count < 1 || count2 < 1) 
            { 
                column = (column_t *)((byte *)column + column->length + 4); 
                continue; 
            } 
 
            if (top < 0) 
            { 
                const int skip = -top; 
 
                if (skip >= count || skip >= count2) 
                { 
                    column = (column_t *)((byte *)column + column->length + 4); 
                    continue; 
                } 
 
                dest   += skip; 
                dest2  += skip; 
                srccol += ldyi * skip; 
                count  -= skip; 
                count2 -= skip; 
            } 
 
            if (do_shadow && sa > 0) 
            { 
                int n = count2; 
                if (use_tc) 
                { 
                    while (n--) { *dest2 = I_BlendDark_32(*dest2, sa); dest2++; } 
                } 
                else 
                { 
                    while (n--) { *dest2 = I_BlendDark_8(*dest2, sa); dest2++; } 
                } 
            } 
 
            if (!xlat) 
            { 
                int n = count; 
                while (n--) 
                { 
                    const byte s = source[srccol >> FRACBITS]; 
                    const pixel_t fg = pal[s]; 
                    *dest = use_tc ? I_BlendOver_32(*dest, fg, alpha) 
                                   : I_BlendOver_8(*dest, fg, alpha); 
                    srccol += ldyi; 
                    dest++; 
                } 
            } 
            else 
            { 
                int n = count; 
                while (n--) 
                { 
                    const byte s = xlat[source[srccol >> FRACBITS]]; 
                    const pixel_t fg = pal[s]; 
                    *dest = use_tc ? I_BlendOver_32(*dest, fg, alpha) 
                                   : I_BlendOver_8(*dest, fg, alpha); 
                    srccol += ldyi; 
                    dest++; 
                } 
            } 
 
            column = (column_t *)((byte *)column + column->length + 4); 
        } 
    } 
}

// -----------------------------------------------------------------------------
// V_DrawPatchFullScreen
// -----------------------------------------------------------------------------

void V_DrawPatchFullScreen(patch_t *patch, boolean flipped)
{
    const int x = DivRoundClosest((ORIGWIDTH - SHORT(patch->width)), 2);
    static int black = -1;

    patch->leftoffset = 0;
    patch->topoffset = 0;

    if (black == -1)
    {
        black = I_MapRGB(0x00, 0x00, 0x00);
    }

    // [crispy] fill pillarboxes in widescreen mode
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, black);
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

// -----------------------------------------------------------------------------
// V_DrawPatchFlipped
//  Masks a column based masked pic to the screen.
//  Flips horizontally, e.g. to mirror face.
// -----------------------------------------------------------------------------

void V_DrawPatchFlipped(int x, int y, patch_t *patch)
{
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    byte *source;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Position patch (Crispy-style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Left clipping in fixed-point column space.
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // Compute top pointer of the first column on the destination buffer.
    desttop = dst_screen
            + ((x * ldx) >> FRACBITS) * sh
            + ((y * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Iterate columns in fixed-point (scaled drawing), mirrored horizontally.
    for ( ; col < (w << FRACBITS) ; x++, col += ldxi, desttop += sh)
    {
        int topdelta = -1;

        // Right clipping: nothing more to draw on this scanline.
        if (x >= sw)
            break;

        // Column pointer from patch data, mirrored: w-1-(col>>FRACBITS)
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[w - 1 - (col >> FRACBITS)]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Compute starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;
            dest   = desttop + ((topdelta * ldy) >> FRACBITS);
            count  = (column->length * ldy) >> FRACBITS;

            // Bottom clip against screen height.
            if (top + count > sh)
                count = sh - top;

            // If entirely below the screen, stop scanning this column early.
            if (count < 1)
                break;

            // Top clip once (remove per-pixel "if (top++ >= 0)").
            if (top < 0)
            {
                const int skip = -top;
                if (skip >= count)
                {
                    // Whole post is above the screen; advance to next post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }
                dest   += skip;
                srccol += ldyi * skip;
                count  -= skip;
                // top is effectively 0 now
            }

            // Hot loop: opaque draw, no translation/translucency by design.
            {
                int n = count;
                while (n--)
                {
                    *dest++ = pal[source[srccol >> FRACBITS]];
                    srccol += ldyi;
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }

}

// -----------------------------------------------------------------------------
// V_DrawTLPatch
//  Masks a column based translucent masked pic to the screen.
// -----------------------------------------------------------------------------

void V_DrawTLPatch(int x, int y, patch_t * patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;
    const boolean  use_tc  = vid_truecolor;
    const int      vres    = vid_resolution;

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Always translucent, no translation/coloring here.

    // Position patch (Crispy-style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Strict in-bounds guard (kept from original).
    if (x < 0
     || x + SHORT(patch->width)  > (sw / vres)
     || y < 0
     || y + SHORT(patch->height) > (sh / vres))
    {
        // Note: intentional early return (see original comment about resolution toggling).
        return;
    }

    // Left clipping in fixed-point column space (still do it for scaled operations).
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // Compute top pointer of the first column on the destination buffer.
    desttop = dst_screen
            + ((x * ldx) >> FRACBITS) * sh
            + ((y * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Iterate columns in fixed-point (scaled drawing).
    for (; col < (w << FRACBITS); x++, col += ldxi, desttop += sh)
    {
        int topdelta = -1;

        // Right clipping (safety; guard above should already ensure in-bounds).
        if (x >= sw)
            break;

        // Column pointer from patch data.
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;
            dest   = desttop + ((topdelta * ldy) >> FRACBITS);
            count  = (column->length * ldy) >> FRACBITS;

            // Bottom clip against screen height.
            if (top + count > sh)
                count = sh - top;

            // Nothing left to draw for this post?
            if (count < 1)
            {
                column = (column_t *)((byte *)column + column->length + 4);
                continue;
            }

            // Top clip once (remove per-pixel "if (top++ >= 0)").
            if (top < 0)
            {
                const int skip = -top;
                if (skip >= count)
                {
                    // Whole post is above the screen; advance to next post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }
                dest   += skip;
                srccol += ldyi * skip;
                count  -= skip;
                // top is effectively 0 now
            }

            // Hot loop: translucent over destination (no translation).
            {
                int n = count;
                while (n--)
                {
                    const byte s = source[srccol >> FRACBITS];
                    const pixel_t fg = pal[s];
                    *dest = use_tc ? I_BlendOver64_32(*dest, fg)
                                   : I_BlendOver64_8(*dest, fg);
                    srccol += ldyi;
                    dest++;
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawAltTLPatch
//  Masks a column based translucent masked pic to the screen.
// -----------------------------------------------------------------------------

void V_DrawAltTLPatch(int x, int y, patch_t * patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst_screen = dest_screen;
    const pixel_t *restrict pal  = pal_color;
    const boolean  use_tc  = vid_truecolor;
    const int      vres    = vid_resolution;

    const fixed_t ldx  = dx;
    const fixed_t ldy  = dy;
    const fixed_t ldxi = dxi;
    const fixed_t ldyi = dyi;

    // Position patch (Crispy-style offsets + widescreen).
    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);
    x += ws_delta; // horizontal widescreen offset

    // Strict in-bounds guard (kept from original intent).
    if (x < 0
     || x + SHORT(patch->width)  > (sw / vres)
     || y < 0
     || y + SHORT(patch->height) > (sh / vres))
    {
        // Render may try to draw before SCREENWIDTH/HEIGHT are updated after a res toggle.
        return;
    }

    // Left clipping in fixed-point column space (scaled drawing).
    col = 0;
    if (x < 0)
    {
        col += ldxi * ((-x * ldx) >> FRACBITS);
        x = 0;
    }

    // Compute top pointer of the first column on the destination buffer.
    desttop = dst_screen
            + ((x * ldx) >> FRACBITS) * sh
            + ((y * ldy) >> FRACBITS);

    w = SHORT(patch->width);

    // Convert x to screen-space pixels once; inner loop just ++x.
    x = (x * ldx) >> FRACBITS;

    // Iterate columns in fixed-point (scaled drawing).
    for (; col < (w << FRACBITS); x++, col += ldxi, desttop += sh)
    {
        int topdelta = -1;

        // Right clipping (safety).
        if (x >= sw)
            break;

        // Column pointer from patch data.
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        // Step through the posts in a column.
        while (column->topdelta != 0xff)
        {
            int top;
            int srccol = 0;

            // DeePsea tall-patch support (accumulating topdelta).
            if (column->topdelta <= topdelta)
                topdelta += column->topdelta;
            else
                topdelta = column->topdelta;

            // Starting y and pointers for this post.
            top    = ((y + topdelta) * ldy) >> FRACBITS;
            source = (byte *)column + 3;
            dest   = desttop + ((topdelta * ldy) >> FRACBITS);
            count  = (column->length * ldy) >> FRACBITS;

            // Bottom clip against screen height.
            if (top + count > sh)
                count = sh - top;

            // Nothing left to draw for this post?
            if (count < 1)
            {
                column = (column_t *)((byte *)column + column->length + 4);
                continue;
            }

            // Top clip once (remove per-pixel "if (top++ >= 0)").
            if (top < 0)
            {
                const int skip = -top;
                if (skip >= count)
                {
                    // Whole post is above the screen; advance to next post.
                    column = (column_t *)((byte *)column + column->length + 4);
                    continue;
                }
                dest   += skip;
                srccol += ldyi * skip;
                count  -= skip;
                // top is effectively 0 now
            }

            // Hot loop: alternate translucency over destination (no translation).
            {
                int n = count;
                while (n--)
                {
                    const byte s = source[srccol >> FRACBITS];
                    const pixel_t fg = pal[s];
                    // Alternate translucency curve (lighter weight than 50%).
                    *dest = use_tc ? I_BlendOver128_32(*dest, fg)
                                   : I_BlendOver128_8(*dest, fg);
                    srccol += ldyi;
                    dest++;
                }
            }

            // Next post in this column.
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawFadePatch
//  Draws translucent patch with given alpha value. For TrueColor only.
// -----------------------------------------------------------------------------

void V_DrawFadePatch (int x, int y, const patch_t *restrict patch, int alpha)
{
    // ---- Hot globals cached locally ----
    const int      sw        = SCREENWIDTH;
    const int      sh        = SCREENHEIGHT;
    const int      vres      = vid_resolution;
    const int      ws_delta  = WIDESCREENDELTA;
    pixel_t *restrict dst    = dest_screen;
    const pixel_t *restrict pal = pal_color;
    const fixed_t  ldx  = dx;
    const fixed_t  ldy  = dy;
    const fixed_t  ldxi = dxi;
    const fixed_t  ldyi = dyi;

    // Quick rejects / normalization for alpha
    if (alpha <= 0)
        return;
    if (alpha > 255)
        alpha = 255;

    // Position (Crispy offsets + widescreen)
    x += ws_delta - SHORT(patch->leftoffset);
    y -= SHORT(patch->topoffset);

    const int pw = SHORT(patch->width);
    const int ph = SHORT(patch->height);
    const int vw = sw / vres;
    const int vh = sh / vres;

    // Strict in-bounds guard (as in original)
    if (x < 0 || x + pw > vw || y < 0 || y + ph > vh)
        return;

    // Precompute column start on destination
    pixel_t *restrict desttop =
        dst + ((x * ldx) >> FRACBITS) * sh + ((y * ldy) >> FRACBITS);

    // Build a 256-entry LUT of truecolor pixels once per call, accounting for translation.
    // This removes per-pixel translation branches and extra indirections.
    pixel_t src_lut[256];
    if (dp_translation)
    {
        const byte *restrict xlat = dp_translation;
        for (int i = 0; i < 256; ++i)
            src_lut[i] = pal[xlat[i]];
    }
    else
    {
        for (int i = 0; i < 256; ++i)
            src_lut[i] = pal[i];
    }

    // Fast path if alpha == 255: plain opaque copy of translated pal color.
    if (alpha == 255)
    {
        for (int col = 0; col < (pw << FRACBITS); col += ldxi, desttop += sh)
        {
            const column_t *restrict column =
                (const column_t *)((const byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

            while (column->topdelta != 0xff)
            {
                const int count = (column->length * ldy) >> FRACBITS;
                pixel_t *restrict dest =
                    desttop + ((column->topdelta * ldy) >> FRACBITS);
                const byte *restrict source = (const byte *)column + 3;

                // No need for top/bottom clipping here because of strict in-bounds guard above.
                int n = count, srccol = 0;
                while (n--)
                {
                    const byte s = source[srccol >> FRACBITS];
                    *dest++ = src_lut[s];
                    srccol += ldyi;
                }

                column = (const column_t *)((const byte *)column + column->length + 4);
            }
        }
        return;
    }

    // General path: alpha-blend over destination (TrueColor).
    for (int col = 0; col < (pw << FRACBITS); col += ldxi, desttop += sh)
    {
        const column_t *restrict column =
            (const column_t *)((const byte *)patch + LONG(patch->columnofs[col >> FRACBITS]));

        while (column->topdelta != 0xff)
        {
            const int count = (column->length * ldy) >> FRACBITS;
            pixel_t *restrict dest =
                desttop + ((column->topdelta * ldy) >> FRACBITS);
            const byte *restrict source = (const byte *)column + 3;

            int n = count, srccol = 0;
            while (n--)
            {
                const byte s = source[srccol >> FRACBITS];
                *dest = I_BlendOver_32(*dest, src_lut[s], alpha);
                srccol += ldyi;
                dest++;
            }

            column = (const column_t *)((const byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawBlock
//  Draw a linear block of pixels into the view buffer.
// -----------------------------------------------------------------------------

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src)
{ 
    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw   = SCREENWIDTH;
    const int      sh   = SCREENHEIGHT;
    const int      vres = vid_resolution;
    pixel_t *restrict dst_screen_local = dest_screen;

#ifdef RANGECHECK
    if (x < 0 || x + width > sw || y < 0 || y + height > sh)
    {
        I_Error("Bad V_DrawBlock");
    }
#endif

    // Trivial reject
    if (width <= 0 || height <= 0 || src == NULL)
        return;

    // [PN] Transposed: src and dst share the screen layout (column pitch
    // SCREENHEIGHT). dest[0] corresponds to block origin (x, y*vres).
    const int y0 = y * vres;

    // Fast path: whole-screen blit → one big memcpy
    if (x == 0 && y == 0 && width == sw && height == sh)
    {
        const size_t bytes = (size_t)SCREENAREA * sizeof(pixel_t);
        memcpy(dst_screen_local, src, bytes);
        return;
    }

    // General path: column-by-column memcpy (contiguous y runs).
    for (int i = 0; i < width; ++i)
    {
        memcpy(dst_screen_local + (x + i) * sh + y0,
               src + (size_t)i * sh,
               (size_t)height * sizeof(pixel_t));
    }
} 

// -----------------------------------------------------------------------------
// V_DrawScaledBlock
//  [crispy] scaled version of V_DrawBlock()
//  [PN] src is a row-major byte-indexed buffer; the destination screen is
//  transposed, so output is written column-by-column.
// -----------------------------------------------------------------------------

void V_DrawScaledBlock(int x, int y, int width, int height, byte *src)
{
    // ---- Hot globals cached locally (helps register allocation) ----
    const int      sw       = SCREENWIDTH;
    const int      sh       = SCREENHEIGHT;
    const int      vres     = vid_resolution;
    const int      ws_delta = WIDESCREENDELTA;
    pixel_t *restrict dst   = dest_screen;
    const pixel_t *restrict pal = pal_color;

    // Widescreen horizontal offset
    x += ws_delta;

    // Destination rect in screen pixels
    const int rx = x * vres;
    const int ry = y * vres;
    const int rw = width  * vres;
    const int rh = height * vres;

    // Clip to screen
    int dx0 = rx < 0 ? 0 : rx;
    int dy0 = ry < 0 ? 0 : ry;
    int dx1 = rx + rw; if (dx1 > sw) dx1 = sw;
    int dy1 = ry + rh; if (dy1 > sh) dy1 = sh;

    const int dw = dx1 - dx0;
    const int dh = dy1 - dy0;
    if (dw <= 0 || dh <= 0)
        return;

    // Offsets of the clipped block relative to the un-clipped scaled origin
    const int xoff = dx0 - rx;  // 0..vres-1
    const int yoff = dy0 - ry;  // 0..vres-1

    // Vertical accumulator start (same for every column)
    const int src_y0 = (vres > 1) ? (yoff / vres) : yoff;
    const int hy0    = (vres > 1) ? (yoff % vres) : 0;

    // Horizontal mapping accumulators start at the clipped origin
    int src_x = (vres > 1) ? (xoff / vres) : xoff;
    int hx    = (vres > 1) ? (xoff % vres) : 0;

    pixel_t *restrict dest_col = dst + dx0 * sh + dy0;

    for (int j = 0; j < dw; ++j)
    {
        const byte *restrict src_pix = src + src_x; // stride 'width' per row
        pixel_t *restrict d = dest_col;
        int src_y = src_y0;
        int hy    = hy0;

        // Inner loop: contiguous down the column, nearest-neighbor in y
        for (int i = 0; i < dh; ++i)
        {
            *d++ = pal[src_pix[src_y * width]];

            if (++hy == vres)
            {
                hy = 0;
                ++src_y;
            }
        }

        dest_col += sh;

        if (++hx == vres)
        {
            hx = 0;
            ++src_x;
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawFilledBox
// -----------------------------------------------------------------------------

void V_DrawFilledBox(int x, int y, int w, int h, int c)
{
    // ---- Hot globals cached locally ----
    const int      sw = SCREENWIDTH;
    const int      sh = SCREENHEIGHT;
    pixel_t *restrict screen = I_VideoBuffer;

#ifdef RANGECHECK
    if (x < 0 || x + w > sw || y < 0 || y + h > sh)
    {
        I_Error("Bad V_DrawFilledBox");
    }
#endif

    if (w <= 0 || h <= 0)
        return;

    pixel_t *restrict col0 = screen + x * sh + y;
    const pixel_t color = (pixel_t)c;

    // Fill the first column (unrolled)
    {
        pixel_t *d = col0;
        int n = h;

        while (n >= 8)
        {
            d[0] = color; d[1] = color; d[2] = color; d[3] = color;
            d[4] = color; d[5] = color; d[6] = color; d[7] = color;
            d += 8; n -= 8;
        }
        for (; n > 0; n--) // Tail loop, safe & warning-free
            *d++ = color;
    }

    if (w == 1)
        return;

    // Copy the filled column to the remaining columns using memcpy
    {
        const size_t col_bytes = (size_t)h * sizeof(*col0);
        pixel_t *restrict dst = col0 + sh;

        for (int x1 = 1; x1 < w; ++x1)
        {
            memcpy(dst, col0, col_bytes);
            dst += sh;
        }
    }
}

void V_DrawHorizLine(int x, int y, int w, int c)
{
    pixel_t *buf;
    int x1;

    // [crispy] prevent framebuffer overflows
    if (x + w > (unsigned)SCREENWIDTH)
	w = SCREENWIDTH - x;

    buf = I_VideoBuffer + x * SCREENHEIGHT + y;

    for (x1 = 0; x1 < w; ++x1)
    {
        *buf = c;
        buf += SCREENHEIGHT;
    }
}

void V_DrawVertLine(int x, int y, int h, int c)
{
    pixel_t *buf;
    int y1;

    buf = I_VideoBuffer + x * SCREENHEIGHT + y;

    for (y1 = 0; y1 < h; ++y1)
    {
        *buf++ = c;
    }
}

void V_DrawBox(int x, int y, int w, int h, int c)
{
    V_DrawHorizLine(x, y, w, c);
    V_DrawHorizLine(x, y+h-1, w, c);
    V_DrawVertLine(x, y, h, c);
    V_DrawVertLine(x+w-1, y, h, c);
}
 
static void V_DrawRawScreen(byte *raw, int size)
{
    int width = size / ORIGHEIGHT;
    int x = ((SCREENWIDTH / vid_resolution) - width) / 2 - WIDESCREENDELTA;
    static int black = -1;

    if (black == -1)
    {
        black = I_MapRGB(0x00, 0x00, 0x00);
    }

    // [crispy] pillar boxing
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, black);
    }

    V_DrawScaledBlock(x, 0, width, ORIGHEIGHT, raw);
}

// [crispy] For Heretic and Hexen widescreen support of replacement TITLE,
// HELP1, etc. These lumps are normally 320 x 200 raw graphics. If the lump
// size is larger than expected, proceed as if it were a patch.
void V_DrawFullscreenRawOrPatch(lumpindex_t index)
{
    patch_t *patch;

    patch = W_CacheLumpNum(index, PU_CACHE);

    int size = W_LumpLength(index);

    if (V_IsPatchLump(index))
    {
        V_DrawPatchFullScreen(patch, false);
    }
    else if (size % 200 == 0)
    {
        V_DrawRawScreen((byte*)patch, size);
    }
    else
    {
        I_Error("Invalid fullscreen graphic.");
    }
}

// [JN] Draws tiled raw screen of any given size, with support for any rendering.
// Used for automap background drawing in Heretic/Hexen games.
/*
void V_DrawRawTiled(int width, int height, int v_max, byte *src, pixel_t *dest)
{
    int x, y;

    for (int i = 0; i < v_max; i++)
    {
        for (int j = 0; j < SCREENWIDTH ; j++)
        {
            x = j % width;
            y = i % height;
            *dest++ = pal_color[src[width * y + x]];
        }
    }
}
*/

// -----------------------------------------------------------------------------
// V_FillFlat
//  [crispy] Unified function of flat filling. Used for intermission
//  and finale screens, view border and status bar's wide screen mode.
//  [PN] Avoid per-pixel divisions by using accumulators; cache hot globals.
//  [PN] Transposed: destination columns are pitched by SCREENHEIGHT and each
//  column's y-run is written contiguously.
// -----------------------------------------------------------------------------

void V_FillFlat(int y_start, int y_stop, int x_start, int x_stop,
                const byte *src, pixel_t *dest)
{
    // ---- Hot globals cached locally ----
    const int      vres = vid_resolution;
    const int      sh   = SCREENHEIGHT;
    const pixel_t *restrict pal = pal_color;

    const int dw = x_stop - x_start;
    const int dh = y_stop - y_start;
    if (dw <= 0 || dh <= 0)
        return;

    // Initial source X index and accumulator (constant per column)
    int src_x = (vres > 1) ? ((x_start / vres) & 63) : (x_start & 63);
    int hx    = (vres > 1) ?  (x_start % vres)       : 0;

    pixel_t *restrict dcol = dest;

    for (int i = 0; i < dw; ++i)
    {
        pixel_t *restrict d = dcol;

        // Initial source Y index and accumulator for this column
        int src_y = (vres > 1) ? ((y_start / vres) & 63) : (y_start & 63);
        int hy    = (vres > 1) ?  (y_start % vres)       : 0;

        for (int j = 0; j < dh; ++j)
        {
            *d++ = pal[src[(src_y << 6) + src_x]];

            if (++hy == vres)
            {
                hy = 0;
                src_y = (src_y + 1) & 63;
            }
        }

        dcol += sh;

        if (++hx == vres)
        {
            hx = 0;
            src_x = (src_x + 1) & 63;
        }
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
        dxi = (ORIGWIDTH << FRACBITS) / NONWIDEWIDTH + 1;  // [JN] +1 for multiple resolutions
        dy = (SCREENHEIGHT << FRACBITS) / ORIGHEIGHT;
        dyi = (ORIGHEIGHT << FRACBITS) / SCREENHEIGHT + 1; // [JN] +1 for multiple resolutions
    }
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

static void WritePNGfile (const char *filename)
{
    byte *data;
    byte *rgb_data = NULL;
    int width, height;
    size_t png_data_size = 0;
    void *pPNG_data = NULL;
    size_t pixel_count;

    I_RenderReadPixels(&data, &width, &height);
    {
        pixel_count = (size_t) width * (size_t) height;
        rgb_data = malloc(pixel_count * 3);

        // [PN] Prefer RGB output: screenshots should not carry transparency.
        if (rgb_data)
        {
            for (size_t i = 0; i < pixel_count; ++i)
            {
                const size_t src = i * 4;
                const size_t dst = i * 3;
                rgb_data[dst + 0] = data[src + 0];
                rgb_data[dst + 1] = data[src + 1];
                rgb_data[dst + 2] = data[src + 2];
            }

            // [PN] Using the _ex version to explicitly set compression level
            // (screenshots_png_compression) and vertical flip (MZ_FALSE = do not flip).
            pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(
                rgb_data, width, height, 3, &png_data_size, screenshots_png_compression, MZ_FALSE);
        }

        // [PN] Fallback to RGBA if RGB conversion buffer cannot be allocated.
        if (!pPNG_data)
        {
            pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(
                data, width, height, 4, &png_data_size, screenshots_png_compression, MZ_FALSE);
        }

        if (pPNG_data)
        {
            FILE *handle = M_fopen(filename, "wb");

            if (handle != NULL)
            {
                fwrite(pPNG_data, 1, png_data_size, handle);
                fclose(handle);
            }
            mz_free(pPNG_data);
        }
    }
    free(rgb_data);
    free(data);
}

// -----------------------------------------------------------------------------
// stbi_write_fwrite_callback
//  [PN] Callback for stb_image_write: writes into an already opened FILE*.
//  This lets us keep using our own M_fopen / screenshotdir file layer
//  instead of stb's internal stdio.
// -----------------------------------------------------------------------------

static void stbi_write_fwrite_callback(void *context, void *data, int size)
{
    FILE *f = (FILE *)context;

    if (f != NULL && data != NULL && size > 0)
    {
        fwrite(data, 1, (size_t)size, f);
    }
}

// -----------------------------------------------------------------------------
// WriteJPEGfile
//  [PN] Saves a JPEG screenshot using stb_image_write.
//  JPEG ignores the alpha channel, so we can pass RGBA (comp=4) directly
//  from I_RenderReadPixels without converting to RGB — saves a malloc
//  and a loop over all pixels.
// -----------------------------------------------------------------------------

static void WriteJPEGfile(const char *filename)
{
    byte *data;
    int   width, height;
    FILE *handle;

    I_RenderReadPixels(&data, &width, &height);

    handle = M_fopen(filename, "wb");

    if (handle != NULL)
    {
        // comp = 4 (RGBA), quality = screenshots_jpeg_quality (1 ... 100).
        // Alpha is silently discarded by the JPEG encoder.
        stbi_write_jpg_to_func(stbi_write_fwrite_callback,
                               handle,
                               width,
                               height,
                               4,
                               data,
                               screenshots_jpg_quality);
        fclose(handle);
    }

    free(data);
}

//
// V_ScreenShot
//

void V_ScreenShot(const char *format)
{
    int i;
    char lbmname[16]; // haleyjd 20110213: BUG FIX - 12 is too small!
    char *file;
    const char *ext;
    boolean     use_jpeg;
    
    // [PN] Pick the extension from the cvar, fall back to PNG on garbage.
    if (screenshots_format != NULL && (!strcasecmp(screenshots_format, "jpg")))
    {
        ext      = "jpg";
        use_jpeg = true;
    }
    else
    {
        ext      = "png";
        use_jpeg = false;
    }

    // find a file name to save it to

    for (i=0; i<=9999; i++)
    {
        M_snprintf(lbmname, sizeof(lbmname), format, i, ext);
        // [JN] Construct full path to screenshot file.
        file = M_StringJoin(screenshotdir, lbmname, NULL);

        if (!M_FileExists(file))
        {
            if (use_jpeg)
            WriteJPEGfile(file);
            else
            WritePNGfile(file);
            free(file);
            return;
        }
        free(file);
    }

    I_Error ("V_ScreenShot: Couldn't create a screenshot.");
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

    red = I_MapRGB(0xff, 0x00, 0x00);
    white = I_MapRGB(0xff, 0xff, 0xff);
    yellow = I_MapRGB(0xff, 0xff, 0x00);

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

    white = I_MapRGB(0xff, 0xff, 0xff);

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

    bgcolor = I_MapRGB(0x77, 0x77, 0x77);
    bordercolor = I_MapRGB(0x55, 0x55, 0x55);
    black = I_MapRGB(0x00, 0x00, 0x00);

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

// [FG] check if the lump can be a Doom patch
// taken from PrBoom+ prboom2/src/r_patch.c:L350-L390

boolean V_IsPatchLump(const int lump)
{
    int size;
    int width, height;
    const patch_t *patch;
    boolean result;

    if (lump < 0)
        return false;

    size = lumpinfo[lump]->cache_size > 0 ? lumpinfo[lump]->cache_size
                                          : W_LumpLength(lump);

    // Minimum length of a valid Doom patch
    if (size < 13)
    {
        return false;
    }

    patch = (const patch_t *) W_CacheLumpNum(lump, PU_CACHE);

    // [PN] Raw PNG data is not a Doom patch (converted PNGs arrive here as patch_t).
    if (!memcmp(patch, "\211PNG\r\n\032\n", 8))
    {
        return false;
    }

    width = SHORT(patch->width);
    height = SHORT(patch->height);

    result = (height > 0 && height <= 16384 && width > 0 && width <= 16384 &&
              width < size / 4);

    if (result)
    {
        // The dimensions seem like they might be valid for a patch, so
        // check the column directory for extra security. All columns
        // must begin after the column directory, and none of them must
        // point past the end of the patch.
        int x;

        for (x = 0; x < width; x++)
        {
            unsigned int ofs = LONG(patch->columnofs[x]);

            // Need one byte for an empty column
            // (but there's patches that don't know that!)
            if (ofs < (unsigned int) width * 4 + 8 ||
                ofs >= (unsigned int) size)
            {
                result = false;
                break;
            }
        }
    }

    return result;
}
