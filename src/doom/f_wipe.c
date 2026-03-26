//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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
#include "f_wipe.h"
#include "m_random.h"

#include "id_vars.h"


// =============================================================================
// SCREEN WIPE PACKAGE
// =============================================================================

static pixel_t *wipe_scr_start;
static pixel_t *wipe_scr_end;
static pixel_t *wipe_scr;
static int     *y;
static int     *y_prev;
static int      wipe_columns;

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
    static size_t wipe_columns_capacity = 0;
    const size_t need_area = (size_t)SCREENAREA;
    const size_t need_columns = (size_t)SCREENWIDTH;

    if (need_area > wipe_capacity)
    {
        wipe_scr_start = (pixel_t *)I_Realloc(wipe_scr_start, need_area * sizeof(*wipe_scr_start));
        wipe_scr_end   = (pixel_t *)I_Realloc(wipe_scr_end, need_area * sizeof(*wipe_scr_end));
        y              =     (int *)I_Realloc(y, need_area * sizeof(*y));
        wipe_capacity  = need_area;
    }

    if (need_columns > wipe_columns_capacity)
    {
        y_prev = (int *)I_Realloc(y_prev, need_columns * sizeof(*y_prev));
        wipe_columns_capacity = need_columns;
    }
}

// -----------------------------------------------------------------------------
// wipe_initMelt
// -----------------------------------------------------------------------------

static void wipe_initMelt (void)
{
    wipe_columns = SCREENWIDTH;

    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    y[0] = -(ID_RealRandom()%16);

    for (int i = 1 ; i < wipe_columns ; i++)
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

    memcpy(y_prev, y, wipe_columns * sizeof(*y_prev));
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
// wipe_renderMelt
// -----------------------------------------------------------------------------

static void wipe_renderMelt (void)
{
    // [PN] Melt state is simulated in 320x200 space and scaled for current resolution.
    const int vertblocksize = SCREENHEIGHT * 100 / ORIGHEIGHT;
    const int horizblocksize = SCREENWIDTH * 100 / wipe_columns;
    int tail_start = 0;

    memcpy(wipe_scr, wipe_scr_end, SCREENAREA * sizeof(*wipe_scr));

    for (int col = 0; col < wipe_columns; ++col)
    {
        int currcol = (col * horizblocksize) / 100;
        const int currcolend = ((col + 1) * horizblocksize) / 100;
        int current = y[col];

        if (vid_uncapped_fps)
        {
            const int delta = y[col] - y_prev[col];
            current = y_prev[col] + (int)(delta * FIXED2DOUBLE(fractionaltic));
        }

        if (current < 0)
        {
            for (; currcol < currcolend; ++currcol)
            {
                const pixel_t *source = wipe_scr_start + currcol;
                pixel_t       *dest   = wipe_scr + currcol;

                for (int row = 0; row < SCREENHEIGHT; ++row)
                {
                    *dest = *source;
                    dest += SCREENWIDTH;
                    source += SCREENWIDTH;
                }
            }
        }
        else if (current < ORIGHEIGHT)
        {
            const int currrow = (current * vertblocksize) / 100;

            for (; currcol < currcolend; ++currcol)
            {
                const pixel_t *source = wipe_scr_start + currcol;
                pixel_t       *dest   = wipe_scr + currcol + (currrow * SCREENWIDTH);

                for (int row = 0; row < SCREENHEIGHT - currrow; ++row)
                {
                    *dest = *source;
                    dest += SCREENWIDTH;
                    source += SCREENWIDTH;
                }
            }
        }

        tail_start = currcolend;
    }

    if (tail_start < SCREENWIDTH)
    {
        int current = y[wipe_columns - 1];

        if (vid_uncapped_fps)
        {
            const int delta = y[wipe_columns - 1] - y_prev[wipe_columns - 1];
            current = y_prev[wipe_columns - 1] + (int)(delta * FIXED2DOUBLE(fractionaltic));
        }

        if (current < 0)
        {
            for (int currcol = tail_start; currcol < SCREENWIDTH; ++currcol)
            {
                const pixel_t *source = wipe_scr_start + currcol;
                pixel_t       *dest   = wipe_scr + currcol;

                for (int row = 0; row < SCREENHEIGHT; ++row)
                {
                    *dest = *source;
                    dest += SCREENWIDTH;
                    source += SCREENWIDTH;
                }
            }
        }
        else if (current < ORIGHEIGHT)
        {
            const int currrow = (current * vertblocksize) / 100;

            for (int currcol = tail_start; currcol < SCREENWIDTH; ++currcol)
            {
                const pixel_t *source = wipe_scr_start + currcol;
                pixel_t       *dest   = wipe_scr + currcol + (currrow * SCREENWIDTH);

                for (int row = 0; row < SCREENHEIGHT - currrow; ++row)
                {
                    *dest = *source;
                    dest += SCREENWIDTH;
                    source += SCREENWIDTH;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// wipe_doMelt
// -----------------------------------------------------------------------------

static const int wipe_doMelt (int ticks)
{
    boolean done = true;

    if (ticks > 0)
    {
        while (ticks--)
        {
            memcpy(y_prev, y, wipe_columns * sizeof(*y_prev));

            for (int col = 0; col < wipe_columns; ++col)
            {
                if (y_prev[col] < 0)
                {
                    y[col] = y_prev[col] + 1;
                    done = false;
                }
                else if (y_prev[col] < ORIGHEIGHT)
                {
                    // [PN] Accelerate after warm-up rows; speed still scales via vid_screenwipe.
                    int dy = (y_prev[col] < 16) ? y_prev[col] + 1 : (8 * vid_screenwipe);
                    int next = y_prev[col] + dy;

                    if (next > ORIGHEIGHT)
                    {
                        next = ORIGHEIGHT;
                    }

                    y[col] = next;
                    done = false;
                }
                else
                {
                    y[col] = ORIGHEIGHT;
                }
            }
        }
    }
    else
    {
        for (int col = 0; col < wipe_columns; ++col)
        {
            done = done && (y[col] >= ORIGHEIGHT);
        }
    }

    if (done)
    {
        // [PN] Final frame must be exact end-screen; avoid sub-tic interpolation residue.
        memcpy(y_prev, y, wipe_columns * sizeof(*y_prev));
        memcpy(wipe_scr, wipe_scr_end, SCREENAREA * sizeof(*wipe_scr));
        return true;
    }

    wipe_renderMelt();

    return false;
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
