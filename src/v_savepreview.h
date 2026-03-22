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

#ifndef __V_SAVEPREVIEW_H__
#define __V_SAVEPREVIEW_H__

#include <stdio.h>

#include "doomtype.h"

#define V_SAVEPREVIEW_WIDTH        72
#define V_SAVEPREVIEW_HEIGHT       45
#define V_SAVEPREVIEW_SIZE         (V_SAVEPREVIEW_WIDTH * V_SAVEPREVIEW_HEIGHT)
#define V_SAVEPREVIEW_FOOTER_SIZE  12
#define V_SAVEPREVIEW_VERSION      1

typedef byte (*v_savepreview_to_pal_fn)(pixel_t pixel, void *user_data);

typedef struct
{
    byte cache[V_SAVEPREVIEW_SIZE];
    boolean cache_valid;
    boolean capture_requested;
} v_savepreview_cache_t;

void V_SavePreview_RequestCapture(v_savepreview_cache_t *state);
boolean V_SavePreview_IsReady(const v_savepreview_cache_t *state);

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
                               void *user_data);

void V_SavePreview_CopyOrBlack(const v_savepreview_cache_t *state,
                               byte *out_preview);

void V_SavePreview_WriteFooter(byte footer[V_SAVEPREVIEW_FOOTER_SIZE]);
boolean V_SavePreview_ReadFromFile(FILE *fp, byte *preview);

#endif
