//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//


#pragma once

#include "v_patch.h"
#include "w_wad.h" // [crispy] for lumpindex_t


extern boolean dp_translucent;
extern boolean V_IsPatchLump(const int lump);
extern byte *dp_translation;
extern pixel_t *pal_color;

extern void V_MarkRect(int x, int y, int width, int height);
extern void V_CopyRect(int srcx, int srcy, pixel_t *source, int width, int height, int destx, int desty);
extern void V_DrawPatch(int x, int y, patch_t *patch);
extern void V_DrawShadowedPatch(int x, int y, patch_t *patch);
extern void V_DrawShadowedPatchNoOffsets(int x, int y, patch_t *patch);
extern void V_DrawShadowedPatchOptional(int x, int y, int shadow_type, patch_t *patch);
extern void V_DrawShadowedPatchOptionalFade(int x, int y, int shadow_type, patch_t *patch, int alpha);
extern void V_DrawPatchFullScreen(patch_t *patch, boolean flipped);
extern void V_DrawPatchFlipped(int x, int y, patch_t *patch);
extern void V_DrawTLPatch(int x, int y, patch_t *patch);
extern void V_DrawAltTLPatch(int x, int y, patch_t *patch);
extern void V_DrawFadePatch(int x, int y, const patch_t *restrict patch, int alpha);
extern void V_DrawBlock(int x, int y, int width, int height, pixel_t *src);
extern void V_DrawScaledBlock(int x, int y, int width, int height, byte *src);
extern void V_DrawFilledBox(int x, int y, int w, int h, int c);
extern void V_DrawHorizLine(int x, int y, int w, int c);
extern void V_DrawVertLine(int x, int y, int h, int c);
extern void V_DrawBox(int x, int y, int w, int h, int c);
extern void V_DrawFullscreenRawOrPatch(lumpindex_t index); // [crispy]
// extern void V_DrawRawTiled(int width, int height, int v_max, byte *src, pixel_t *dest);
extern void V_FillFlat(int y_start, int y_stop, int x_start, int x_stop, const byte *src, pixel_t *dest);    // [crispy]
extern void V_Init (void);
extern void V_UseBuffer(pixel_t *buffer);
extern void V_RestoreBuffer(void);
extern void V_ScreenShot(const char *format);
extern void V_DrawMouseSpeedBox(int speed);
