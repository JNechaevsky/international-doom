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
//  Shared save preview helpers for Doom / Heretic / Hexen.
//

#include <string.h>
#include "v_savepreview.h"


// -----------------------------------------------------------------------------
// V_SavePreview_RequestCapture
//  [PN] Mark cache dirty and request a single capture on the next frame.
// -----------------------------------------------------------------------------

void V_SavePreview_RequestCapture(v_savepreview_cache_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->cache_valid = false;
    state->capture_requested = true;
}

boolean V_SavePreview_IsReady(const v_savepreview_cache_t *state)
{
    // Save pipeline may proceed once no capture request is pending.
    return (state == NULL) || !state->capture_requested;
}

// -----------------------------------------------------------------------------
// V_SavePreview_Build
//  [PN] Build a paletted thumbnail from the world viewport,
//  cropped to non-wide width.
// -----------------------------------------------------------------------------

static boolean V_SavePreview_Build(byte *thumb,
                                   const pixel_t *video_buffer,
                                   int screen_width,
                                   int screen_height,
                                   int view_x,
                                   int view_y,
                                   int view_w,
                                   int view_h,
                                   int nonwide_width,
                                   v_savepreview_to_pal_fn to_pal,
                                   void *user_data)
{
    int src_x = view_x;
    int src_y = view_y;
    int src_w = view_w;
    int src_h = view_h;

    if (thumb == NULL || video_buffer == NULL || to_pal == NULL)
    {
        return false;
    }

    // Crop to non-widescreen world width (centered) for stable menu preview.
    if (nonwide_width > 0 && src_w > nonwide_width)
    {
        src_x += (src_w - nonwide_width) / 2;
        src_w = nonwide_width;
    }

    // Clamp source rectangle to current framebuffer.
    if (src_x < 0)
    {
        src_w += src_x;
        src_x = 0;
    }
    if (src_y < 0)
    {
        src_h += src_y;
        src_y = 0;
    }
    if (src_x + src_w > screen_width)
    {
        src_w = screen_width - src_x;
    }
    if (src_y + src_h > screen_height)
    {
        src_h = screen_height - src_y;
    }

    if (src_w <= 0 || src_h <= 0)
    {
        return false;
    }

    for (int y = 0; y < V_SAVEPREVIEW_HEIGHT; ++y)
    {
        const int sy = src_y + (y * src_h) / V_SAVEPREVIEW_HEIGHT;

        for (int x = 0; x < V_SAVEPREVIEW_WIDTH; ++x)
        {
            const int sx = src_x + (x * src_w) / V_SAVEPREVIEW_WIDTH;
            const pixel_t px = video_buffer[sy * screen_width + sx];

            thumb[y * V_SAVEPREVIEW_WIDTH + x] = to_pal(px, user_data);
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// V_SavePreview_UpdateCache
//  [PN] Execute pending capture request and refresh cached thumbnail once.
// -----------------------------------------------------------------------------

void V_SavePreview_UpdateCache(v_savepreview_cache_t *state,
                               const pixel_t *video_buffer,
                               int screen_width,
                               int screen_height,
                               int view_x,
                               int view_y,
                               int view_w,
                               int view_h,
                               int nonwide_width,
                               v_savepreview_to_pal_fn to_pal,
                               void *user_data)
{
    if (state == NULL || !state->capture_requested)
    {
        return;
    }

    state->cache_valid = V_SavePreview_Build(state->cache,
                                             video_buffer,
                                             screen_width,
                                             screen_height,
                                             view_x,
                                             view_y,
                                             view_w,
                                             view_h,
                                             nonwide_width,
                                             to_pal,
                                             user_data);
    state->capture_requested = false;
}

// -----------------------------------------------------------------------------
// V_SavePreview_CopyOrBlack
//  [PN] Return cached preview if present, otherwise provide black fallback pixels.
// -----------------------------------------------------------------------------

void V_SavePreview_CopyOrBlack(const v_savepreview_cache_t *state, byte *out_preview)
{
    if (out_preview == NULL)
    {
        return;
    }

    if (state != NULL && state->cache_valid)
    {
        memcpy(out_preview, state->cache, V_SAVEPREVIEW_SIZE);
    }
    else
    {
        memset(out_preview, 0, V_SAVEPREVIEW_SIZE);
    }
}

// -----------------------------------------------------------------------------
// V_SavePreview_WriteFooter
//  [PN] Write fixed footer metadata ("ISVP", version, dimensions, data size).
//  "ISVP" = "Inter Save View Preview".
// -----------------------------------------------------------------------------

void V_SavePreview_WriteFooter(byte footer[V_SAVEPREVIEW_FOOTER_SIZE])
{
    if (footer == NULL)
    {
        return;
    }

    footer[0] = 'I';
    footer[1] = 'S';
    footer[2] = 'V';
    footer[3] = 'P';
    footer[4] = V_SAVEPREVIEW_VERSION;
    footer[5] = V_SAVEPREVIEW_WIDTH;
    footer[6] = V_SAVEPREVIEW_HEIGHT;
    footer[7] = 0; // reserved
    footer[8]  = (byte)(V_SAVEPREVIEW_SIZE & 0xff);
    footer[9]  = (byte)((V_SAVEPREVIEW_SIZE >> 8) & 0xff);
    footer[10] = (byte)((V_SAVEPREVIEW_SIZE >> 16) & 0xff);
    footer[11] = (byte)((V_SAVEPREVIEW_SIZE >> 24) & 0xff);
}

// -----------------------------------------------------------------------------
// V_SavePreview_ReadFromFile
//  [PN] Read preview block from save tail; validate footer and bounds first.
// -----------------------------------------------------------------------------

boolean V_SavePreview_ReadFromFile(FILE *fp, byte *preview)
{
    byte footer[V_SAVEPREVIEW_FOOTER_SIZE];
    long file_size;
    long data_pos;
    unsigned int data_len;

    if (fp == NULL || preview == NULL)
    {
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        return false;
    }

    file_size = ftell(fp);
    if (file_size < V_SAVEPREVIEW_FOOTER_SIZE)
    {
        return false;
    }

    if (fseek(fp, file_size - V_SAVEPREVIEW_FOOTER_SIZE, SEEK_SET) != 0)
    {
        return false;
    }

    if (fread(footer, 1, V_SAVEPREVIEW_FOOTER_SIZE, fp) != V_SAVEPREVIEW_FOOTER_SIZE)
    {
        return false;
    }

    if (footer[0] != 'I' || footer[1] != 'S' || footer[2] != 'V' || footer[3] != 'P')
    {
        return false;
    }
    if (footer[4] != V_SAVEPREVIEW_VERSION)
    {
        return false;
    }
    if (footer[5] != V_SAVEPREVIEW_WIDTH || footer[6] != V_SAVEPREVIEW_HEIGHT)
    {
        return false;
    }

    data_len = (unsigned int)footer[8]
             | ((unsigned int)footer[9] << 8)
             | ((unsigned int)footer[10] << 16)
             | ((unsigned int)footer[11] << 24);

    if (data_len != V_SAVEPREVIEW_SIZE)
    {
        return false;
    }

    data_pos = file_size - V_SAVEPREVIEW_FOOTER_SIZE - (long)data_len;
    if (data_pos < 0)
    {
        return false;
    }

    if (fseek(fp, data_pos, SEEK_SET) != 0)
    {
        return false;
    }

    return fread(preview, 1, V_SAVEPREVIEW_SIZE, fp) == V_SAVEPREVIEW_SIZE;
}
