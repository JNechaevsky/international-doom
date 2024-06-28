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
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
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
// wipe_shittyColMajorXform
// -----------------------------------------------------------------------------

static void wipe_shittyColMajorXform (dpixel_t *array)
{
    const int width = SCREENWIDTH/2;
    dpixel_t *dest = (dpixel_t*) Z_Malloc(width*SCREENHEIGHT*sizeof(*dest), PU_STATIC, 0);

    for (int y = 0 ; y < SCREENHEIGHT ; y++)
    {
        for (int x = 0 ; x < width ; x++)
        {
            dest[x*SCREENHEIGHT+y] = array[y*width+x];
        }
    }

    memcpy(array, dest, width*SCREENHEIGHT*sizeof(*dest));

    Z_Free(dest);
}

// -----------------------------------------------------------------------------
// wipe_initMelt
// -----------------------------------------------------------------------------

static void wipe_initMelt (void)
{
    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, SCREENWIDTH*SCREENHEIGHT*sizeof(*wipe_scr));

    // makes this wipe faster (in theory)
    // to have stuff in column-major format
    wipe_shittyColMajorXform((dpixel_t*)wipe_scr_start);
    wipe_shittyColMajorXform((dpixel_t*)wipe_scr_end);

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    y = (int *) Z_Malloc(SCREENWIDTH*sizeof(int), PU_STATIC, 0);
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
// wipe_doMelt
// -----------------------------------------------------------------------------

static const int wipe_doMelt (int ticks)
{
    int j;
    int dy;
    int idx;
    const int width = SCREENWIDTH/2;

    dpixel_t *s;
    dpixel_t *d;
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

                s = &((dpixel_t *)wipe_scr_end)[i*SCREENHEIGHT+y[i]];
                d = &((dpixel_t *)wipe_scr)[y[i]*width+i];
                idx = 0;

                for (j = dy ; j ; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }

                y[i] += dy;
                s = &((dpixel_t *)wipe_scr_start)[i*SCREENHEIGHT];
                d = &((dpixel_t *)wipe_scr)[y[i]*width+i];
                idx = 0;

                for (j=SCREENHEIGHT-y[i];j;j--)
                {
                    d[idx] = *(s++);
                    idx += width;
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
    const int  pix = SCREENWIDTH*SCREENHEIGHT;
    boolean changed = false;

    // [crispy] reduce fail-safe crossfade counter tics
    if (--fade_counter > 0)
    {
        // [JN] Keep solid background to prevent blending with empty space.
        V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_start);

        for (int i = pix; i > 0; i--)
        {
            if (*cur_screen != *end_screen && fade_counter)
            {
                changed = true;
#ifndef CRISPY_TRUECOLOR
                *cur_screen = shadowmap[(*cur_screen << 8) + *end_screen];
#else
                *cur_screen = I_BlendOver(*end_screen, *cur_screen, alpha_table[fade_counter]);
#endif
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
    if (vid_screenwipe < 3)  // [JN] y is not allocated in crossfade wipe.
    Z_Free(y);
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
    {
        wipe_init = wipe_initCrossfade;
        wipe_do = wipe_doCrossfade;
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
        wipe_exit();
    }

    return !go;
}
