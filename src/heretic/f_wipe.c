//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2025 Julia Nechaevskaya
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
#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_trans.h" // [crispy] blending functions
#include "v_video.h"

#include "id_vars.h"


// =============================================================================
// SCREEN WIPE PACKAGE
// =============================================================================

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
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));
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

#ifdef CRISPY_TRUECOLOR
static const uint8_t alpha_table[] = {
      0,   8,  16,  24,  32,  40,  48,  56,
     64,  72,  80,  88,  96, 104, 112, 120,
    128, 136, 144, 152, 160, 168, 176, 184,
    192, 200, 208, 216, 224, 232, 240, 248,
};
#endif

static const int wipe_doCrossfade (const int ticks)
{
    pixel_t   *cur_screen = wipe_scr;
    pixel_t   *end_screen = wipe_scr_end;
    const int  pix = SCREENAREA;
    boolean changed = false;

    // [crispy] reduce fail-safe crossfade counter tics
    if (--fade_counter > 0)
    {
        // [JN] Keep solid background to prevent blending with empty space.
        V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_start);

        for (int i = 0; i < pix; i++)  // [PN] Modified index to standard loop
        {
            if (*cur_screen != *end_screen && fade_counter)
            {
#ifndef CRISPY_TRUECOLOR
                *cur_screen = shadowmap[(*cur_screen << 8) + *end_screen];
#else
                *cur_screen = I_BlendOver(*end_screen, *cur_screen, alpha_table[fade_counter]);
#endif
                // [PN] Moved to the end of the condition to ensure
                // it's set only when an actual pixel change occurs.
                changed = true;
            }
            ++cur_screen;
            ++end_screen;
        }
    }

    return !changed;
}

// -----------------------------------------------------------------------------
// wipe_exitMelt
// -----------------------------------------------------------------------------

static void wipe_exit (void)
{
    // [JN] Blit final screen.
    V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_end);
    free(wipe_scr_start);
    free(wipe_scr_end);
}

// -----------------------------------------------------------------------------
// wipe_StartScreen
// -----------------------------------------------------------------------------

void wipe_StartScreen (void)
{
    wipe_scr_start = malloc(SCREENAREA * sizeof(*wipe_scr_start));
    I_ReadScreen(wipe_scr_start);
}

// -----------------------------------------------------------------------------
// wipe_EndScreen
// -----------------------------------------------------------------------------

void wipe_EndScreen (void)
{
    wipe_scr_end = malloc(SCREENAREA * sizeof(*wipe_scr_end));
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
