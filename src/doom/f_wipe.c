//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
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
// wipe_doMelt
// -----------------------------------------------------------------------------

static int wipe_doMelt (int ticks)
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
                dy = (y[i] < 16) ? y[i]+1 : ((8 * vid_screenwipe) * vid_hires);

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
// wipe_exitMelt
// -----------------------------------------------------------------------------

static void wipe_exitMelt (void)
{
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

    // initial stuff
    if (!go)
    {
        go = true;
        wipe_scr = I_VideoBuffer;
        wipe_initMelt();
    }

    // do a piece of wipe-in
    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    // final stuff
    if ((*wipe_doMelt)(ticks))
    {
        go = false;
        wipe_exitMelt();
    }

    return !go;
}
