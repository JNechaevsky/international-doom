//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Polina "Aura" N.
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
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//

#include <stdlib.h>
#include <string.h>

#include "z_zone.h"
#include "i_system.h" // I_Realloc
#include "i_video.h"
#include "v_trans.h" // [crispy] blending functions
#include "v_video.h"
#include "m_random.h"

#include "id_vars.h"


// =============================================================================
// SCREEN WIPE PACKAGE
// =============================================================================

static pixel_t *wipe_scr_start;
static pixel_t *wipe_scr_end;
static pixel_t *wipe_scr;
static int     *y;

// [JN] Function pointers to melt and crossfade effects.
static void (*wipe_init) (void);
static const int (*wipe_do) (int ticks);

// [crispy] Additional fail-safe counter for performing crossfade effect.
static int fade_counter;


// -----------------------------------------------------------------------------
// wipe_EnsureBuffers
//  [PN] Lazy allocation / resize of wipe buffers.
// -----------------------------------------------------------------------------

static void wipe_EnsureBuffers (void)
{
    static size_t wipe_capacity = 0;
    const size_t need_area = (size_t)SCREENAREA;

    if (need_area > wipe_capacity)
    {
        wipe_scr_start = (pixel_t *)I_Realloc(wipe_scr_start, need_area * sizeof(*wipe_scr_start));
        wipe_scr_end   = (pixel_t *)I_Realloc(wipe_scr_end, need_area * sizeof(*wipe_scr_end));
        y              =     (int *)I_Realloc(y, need_area * sizeof(*y));
        wipe_capacity  = need_area;
    }
}

// -----------------------------------------------------------------------------
// wipe_initMelt
// -----------------------------------------------------------------------------

static void wipe_initMelt (void)
{
    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    y[0] = -(ID_RealRandom()%16);

    for (int i = 1 ; i < SCREENWIDTH ; i++)
    {
        int r = (ID_RealRandom()%3) - 1;

        y[i] = y[i-1] + r;

        if (y[i] > 0)
        {
            y[i] = 0;
        }
        else
        if (y[i] == -16)
        {
            y[i] = -15;
        }
    }
}

// -----------------------------------------------------------------------------
// wipe_initCrossfade
// -----------------------------------------------------------------------------

static void wipe_initCrossfade (void)
{
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));
    // [JN] Arm fail-safe crossfade counter with...
    // 32 screen screen transitions in TrueColor render,
    // to keep effect smooth enough.
    fade_counter = 32;
}

// -----------------------------------------------------------------------------
// [PN] wipe_initFizzle
// -----------------------------------------------------------------------------

static void wipe_initFizzle (void)
{
    const int scale = vid_resolution;

    memcpy(wipe_scr, wipe_scr_start, SCREENAREA * sizeof(*wipe_scr));

    for (int yy = 0; yy < SCREENHEIGHT; yy += scale)
    {
        for (int xx = 0; xx < SCREENWIDTH; xx += scale)
        {
            const uint8_t burn_value = ID_RealRandom() % 256;

            for (int dy = 0; dy < scale; ++dy)
            {
                for (int dx = 0; dx < scale; ++dx)
                {
                    const int sx = xx + dx;
                    const int sy = yy + dy;

                    if (sx < SCREENWIDTH && sy < SCREENHEIGHT)
                    {
                        y[sy * SCREENWIDTH + sx] = burn_value;
                    }
                }
            }
        }
    }

    fade_counter = 0;
}

// -----------------------------------------------------------------------------
// wipe_doMelt
// -----------------------------------------------------------------------------

static const int wipe_doMelt (int ticks)
{
    int j;
    int dy;
    const int width = SCREENWIDTH/2;
    boolean	done = true;

    while (ticks--)
    {
        for (int i = 0 ; i < width ; i++)
        {
            if (y[i]<0)
            {
                y[i]++; done = false;
            }
            else
            if (y[i] < SCREENHEIGHT)
            {
                dy = (y[i] < 16) ? y[i]+1 : ((8 * vid_screenwipe) * vid_resolution);

                if (y[i]+dy >= SCREENHEIGHT)
                {
                    dy = SCREENHEIGHT - y[i];
                }

                // [PN] Row-major: copy the falling part from end-screen column (i)
                // source starts at row y[i], destination at the same row
                const dpixel_t *s1 = &((dpixel_t *)wipe_scr_end)[ y[i] * width + i ];
                dpixel_t       *d1 = &((dpixel_t *)wipe_scr    )[ y[i] * width + i ];
                for (j = dy; j; --j)
                {
                    *d1 = *s1;
                    d1 += width;  // next row
                    s1 += width;  // next row
                }

                y[i] += dy;
                
                // [PN] Row-major: fill the area above with start-screen column (i)
                const dpixel_t *s2 = &((dpixel_t *)wipe_scr_start)[i]; // row 0
                dpixel_t       *d2 = &((dpixel_t *)wipe_scr      )[y[i] * width + i ];
                for (j = SCREENHEIGHT - y[i]; j; --j)
                {
                    *d2 = *s2;
                    d2 += width;  // next row
                    s2 += width;  // next row
                }

                done = false;
            }
        }
    }

    return done;
}

// -----------------------------------------------------------------------------
// wipe_doCrossfade
// -----------------------------------------------------------------------------

static const uint8_t alpha_table[] = {
      0,   8,  16,  24,  32,  40,  48,  56,
     64,  72,  80,  88,  96, 104, 112, 120,
    128, 136, 144, 152, 160, 168, 176, 184,
    192, 200, 208, 216, 224, 232, 240, 248,
};

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

        if (vid_truecolor)
        {
            for (int i = 0; i < pix; i++)  // [PN] Modified index to standard loop
            {
                if (*cur_screen != *end_screen && fade_counter)
                {
                    *cur_screen = I_BlendOver_32(*end_screen, *cur_screen, alpha_table[fade_counter]);
                    changed = true;
                }
                ++cur_screen;
                ++end_screen;
            }
        }
        else
        {
            for (int i = 0; i < pix; i++)  // [PN] Modified index to standard loop
            {
                if (*cur_screen != *end_screen && fade_counter)
                {
                    *cur_screen = I_BlendOver_8(*end_screen, *cur_screen, alpha_table[fade_counter]);
                    changed = true;
                }
                ++cur_screen;
                ++end_screen;
            }
        }
    }

    return !changed;
}

// -----------------------------------------------------------------------------
// [PN] wipe_doFizzle
// -----------------------------------------------------------------------------

static const int wipe_doFizzle (const int ticks)
{
    pixel_t *cur_screen = wipe_scr;
    const pixel_t *end_screen = wipe_scr_end;

    fade_counter += 8;

    for (int i = 0; i < SCREENAREA; ++i)
    {
        if (y[i] <= fade_counter)
        {
            cur_screen[i] = end_screen[i];
        }
    }

    V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr);
    return fade_counter >= 256;
}

// -----------------------------------------------------------------------------
// wipe_StartScreen
// -----------------------------------------------------------------------------

void wipe_StartScreen (void)
{
    wipe_EnsureBuffers();
    I_ReadScreen(wipe_scr_start);
}

// -----------------------------------------------------------------------------
// wipe_EndScreen
// -----------------------------------------------------------------------------

void wipe_EndScreen (void)
{
    wipe_EnsureBuffers();
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_start); // restore start scr.
}

// -----------------------------------------------------------------------------
// wipe_ScreenWipe
// -----------------------------------------------------------------------------

const int wipe_ScreenWipe (const int ticks)
{
    // when zero, stop the wipe
    static boolean go = false;

    // [JN] Initialize function pointers to melt and crossfade effects.
    if (vid_screenwipe < 3)
    {
        wipe_init = wipe_initMelt;
        wipe_do = wipe_doMelt;
    }
    else
    if (vid_screenwipe == 3)
    {
        wipe_init = wipe_initCrossfade;
        wipe_do = wipe_doCrossfade;
    }
    else
    if (vid_screenwipe == 4)
    {
        wipe_init = wipe_initFizzle;
        wipe_do = wipe_doFizzle;
    }

    // initial stuff
    if (!go)
    {
        go = true;
        wipe_scr = I_VideoBuffer;
        wipe_init();
    }

    // final stuff
    if ((*wipe_do)(ticks))
    {
        go = false;
    }

    return !go;
}
