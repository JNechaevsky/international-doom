//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2024 Julia Nechaevskaya
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

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_trans.h" // [crispy] blending functions
#include "v_video.h"

#include "id_vars.h"


// =============================================================================
// SCREEN WIPE PACKAGE
// =============================================================================

// [JN] Because of odd behavior of going from map to map in one hub,
// we have to call wipe manually instead of relying to gamestate changes.
boolean do_wipe;

static pixel_t *wipe_scr_start;
static pixel_t *wipe_scr_end;
static pixel_t *wipe_scr;

// [crispy] Additional fail-safe counter for performing crossfade effect.
static int fade_counter;


// -----------------------------------------------------------------------------
// wipe_initCrossfade
// -----------------------------------------------------------------------------

static void wipe_initCrossfade (void)
{
    memcpy(wipe_scr, wipe_scr_start, SCREENWIDTH*SCREENHEIGHT*sizeof(*wipe_scr));
    // [JN] Arm fail-safe crossfade counter with...
#ifndef CRISPY_TRUECOLOR
    // 8 screen screen transitions in paletted render,
    // since effect is not really smooth there.
    fade_counter = 8;
#else
    // 32 screen screen transitions in TrueColor render,
    // to keep effect smooth enough.
    fade_counter = 32;
#endif
}

// -----------------------------------------------------------------------------
// wipe_doCrossfade
// -----------------------------------------------------------------------------

static const int wipe_doCrossfade (const int ticks)
{
    pixel_t   *cur_screen = wipe_scr;
    pixel_t   *end_screen = wipe_scr_end;
    const int  pix = SCREENWIDTH*SCREENHEIGHT;
#ifdef CRISPY_TRUECOLOR
    // [JN] Brain-dead correction №1: proper blending alpha value.
    const int  fade_alpha = MIN(fade_counter * 16, 238);
#endif
    boolean changed = false;
    extern int SB_state;

    // [crispy] reduce fail-safe crossfade counter tics
    fade_counter--;

    for (int i = pix; i > 0; i--)
    {
        if (fade_counter)
        {
            changed = true;
#ifndef CRISPY_TRUECOLOR
            *cur_screen = shadowmap[(*cur_screen << 8) + *end_screen];
#else
            *cur_screen = I_BlendOver(*end_screen, *cur_screen, fade_alpha);
#endif
        }
        ++cur_screen;
        ++end_screen;
    }
    
    // [JN] Brain-dead correction №2: prevent small delay after crossfade is over.
    if (fade_counter <= 6)
    {
        SB_state = -1;
    }

    return !changed;
}

// -----------------------------------------------------------------------------
// wipe_exitMelt
// -----------------------------------------------------------------------------

static void wipe_exit (void)
{
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
}

// -----------------------------------------------------------------------------
// wipe_StartScreen
// -----------------------------------------------------------------------------

void wipe_StartScreen (void)
{
    wipe_scr_start = Z_Malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_start), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_start);
}

// -----------------------------------------------------------------------------
// wipe_EndScreen
// -----------------------------------------------------------------------------

void wipe_EndScreen (void)
{
    wipe_scr_end = Z_Malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_end), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);
}

// -----------------------------------------------------------------------------
// wipe_ScreenWipe
// -----------------------------------------------------------------------------

const int wipe_ScreenWipe (const int ticks)
{
    // when zero, stop the wipe
    static boolean go = false;

    // initial stuff
    if (!go)
    {
        go = true;
        wipe_scr = I_VideoBuffer;
        wipe_initCrossfade();
    }

    // final stuff
    if ((*wipe_doCrossfade)(ticks))
    {
        go = false;
        wipe_exit();
    }

    return !go;
}
