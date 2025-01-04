//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
// F_finale.c

#include <ctype.h>

#include "doomdef.h"
#include "deh_str.h"
#include "i_swap.h"
#include "i_video.h"
#include "s_sound.h"
#include "v_video.h"
#include "r_local.h"

#include "id_vars.h"
#include "id_func.h"

static int finalestage;                // 0 = text, 1 = art screen
static int finalecount;
static int finaleendcount;

// [JN] Do screen wipe only once after text skipping.
static boolean finale_wipe_done;

#define TEXTSPEED       3
#define TEXTWAIT        250
#define	TEXTEND         25

static const char *finaletext;
static const char *finaleflat;

static int FontABaseLump;

// [JN] Externalized F_DemonScroll variables to allow repeated scrolling.
static int yval;
static int nextscroll;
static int y;

/*
=======================
=
= F_StartFinale
=
=======================
*/

void F_StartFinale(void)
{
    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    automapactive = false;
    finale_wipe_done = false;
    players[consoleplayer].cheatTics = 1;
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageCenteredTics = 1;
    players[consoleplayer].messageCentered = NULL;

    switch (gameepisode)
    {
        case 1:
            finaleflat = DEH_String("FLOOR25");
            finaletext = DEH_String(E1TEXT);
            break;
        case 2:
            finaleflat = DEH_String("FLATHUH1");
            finaletext = DEH_String(E2TEXT);
            break;
        case 3:
            finaleflat = DEH_String("FLTWAWA2");
            finaletext = DEH_String(E3TEXT);
            break;
        case 4:
            finaleflat = DEH_String("FLOOR28");
            finaletext = DEH_String(E4TEXT);
            break;
        case 5:
            finaleflat = DEH_String("FLOOR08");
            finaletext = DEH_String(E5TEXT);
            break;
    }

    finalestage = 0;
    finalecount = 0;
    // [JN] Count intermission/finale text lenght. Once it's fully printed, 
    // no extra "attack/use" button pressing is needed for skipping.
    finaleendcount = strlen(finaletext) * TEXTSPEED + TEXTEND;
    FontABaseLump = W_GetNumForName(DEH_String("FONTA_S")) + 1;
    // [JN] Reset F_DemonScroll variables.
    yval = 0;
    nextscroll = 0;
    y = 0;

//      S_ChangeMusic(mus_victor, true);
    S_StartSong(mus_cptd, true);
}



boolean F_Responder(event_t * event)
{
    if (event->type != ev_keydown)
    {
        return false;
    }
    if (finalestage == 1 && gameepisode == 2)
    {                           // we're showing the water pic, make any key kick to demo mode
        finalestage++;
        /*
        memset((byte *) 0xa0000, 0, SCREENWIDTH * SCREENHEIGHT);
        memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT);
        I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
        */
        return true;
    }
    return false;
}


/*
=======================
=
= F_Ticker
=
=======================
*/

void F_Ticker(void)
{
    // [JN] If we are in single player mode, allow double skipping of
    // finale texts. The first skip is printing all the text,
    // the second is advancing to next state.
    if (singleplayer)
    {
        // [JN] Make PAUSE working properly on text screen
        if (paused)
        {
            return;
        }

        // [JN] Check for skipping. Allow double-press skiping, 
        // but don't skip immediately.
        if (finalecount > 10)
        {
            // [JN] Don't allow skipping by pressing PAUSE button.
            if (players[consoleplayer].cmd.buttons == (BT_SPECIAL | BTS_PAUSE))
            {
                return;
            }

            // [JN] Double-skip by pressing "attack" button.
            if (players[consoleplayer].cmd.buttons & BT_ATTACK && !MenuActive && !finalestage)
            {
                if (!players[consoleplayer].attackdown)
                {
                    if (finalecount >= finaleendcount)
                    {
                        finalestage = 1;
                    }

                    finalecount += finaleendcount;
                    players[consoleplayer].attackdown = true;
                }
                players[consoleplayer].attackdown = true;
            }
            else
            {
                players[consoleplayer].attackdown = false;
            }

            // [JN] Double-skip by pressing "use" button.
            if (players[consoleplayer].cmd.buttons & BT_USE && !MenuActive && !finalestage)
            {
                if (!players[consoleplayer].usedown)
                {
                    if (finalecount >= finaleendcount)
                    {
                        finalestage = 1;
                    }
    
                    finalecount += finaleendcount;
                    players[consoleplayer].usedown = true;
                }
                players[consoleplayer].usedown = true;
            }
            else
            {
                players[consoleplayer].usedown = false;
            }
        }

        // [JN] Force a wipe after skipping text screen.
        if (finalestage && !finale_wipe_done)
        {
            finale_wipe_done = true;
            wipegamestate = -1;
        }

        // Advance animation.
        finalecount++;
    }
    //
    // [JN] Standard Heretic routine, safe for network game and demos.
    //
    else
    {
    finalecount++;
    if (!finalestage
        && finalecount > strlen(finaletext) * TEXTSPEED + TEXTWAIT)
    {
        finalecount = 0;
        if (!finalestage)
        {
            finalestage = 1;
        }

//              wipegamestate = -1;             // force a wipe
/*
		if (gameepisode == 3)
			S_StartMusic (mus_bunny);
*/
    }
    }
}


/*
=======================
=
= F_TextWrite
=
=======================
*/


void F_TextWrite(void)
{
    byte *src;
    pixel_t *dest;
    int count;
    const char *ch;
    int c;
    int cx, cy;
    patch_t *w;

//
// erase the entire screen to a tiled background
//
    src = W_CacheLumpName(finaleflat, PU_CACHE);
    dest = I_VideoBuffer;

    // [crispy] use unified flat filling function
    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);

//      V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);

//
// draw some of the text onto the screen
//
    cx = 20;
    cy = 5;
    ch = finaletext;

    count = (finalecount - 10) / TEXTSPEED;
    if (count < 0)
        count = 0;
    for (; count; count--)
    {
        c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = 20;
            cy += 9;
            continue;
        }

        c = toupper(c);
        if (c < 33)
        {
            cx += 5;
            continue;
        }

        w = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
        if (cx + SHORT(w->width) > SCREENWIDTH)
            break;
        V_DrawShadowedPatchOptional(cx, cy, 1, w);
        cx += SHORT(w->width);
    }

}


void F_DrawPatchCol(int x, patch_t * patch, int col)
{
    column_t *column;
    byte *source;
    pixel_t *dest, *desttop;
    int count;

    column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));
    desttop = I_VideoBuffer + x;

// step through the posts in a column

    while (column->topdelta != 0xff)
    {
        source = (byte *) column + 3;
        dest = desttop + column->topdelta * SCREENWIDTH;
        count = column->length;

        while (count--)
        {
            *dest = *source++;
            dest += SCREENWIDTH;
        }
        column = (column_t *) ((byte *) column + column->length + 4);
    }
}

/*
==================
=
= F_DemonScroll
=
==================
*/

void F_DemonScroll(void)
{
    int i1 = W_GetNumForName(DEH_String("FINAL1"));
    int i2 = W_GetNumForName(DEH_String("FINAL2"));

    // [JN] assume that FINAL1 and FINAL2 are in RAW format
    if ((W_LumpLength(i1) == 64000) && (W_LumpLength(i2) == 64000))
    {
        byte *DemonBuffer = Z_Malloc(W_LumpLength(i1) + W_LumpLength(i2), PU_STATIC, NULL);
        byte *p1 = W_CacheLumpNum(i1, PU_LEVEL);
        byte *p2 = W_CacheLumpNum(i2, PU_LEVEL);

        memcpy(DemonBuffer, p2, W_LumpLength(i2));
        memcpy(DemonBuffer + W_LumpLength(i2), p1, W_LumpLength(i1));

        // [rfomin] show first screen for a while
        if (finalecount < 70)
        {
            V_DrawScaledBlock(0, 0, ORIGWIDTH, ORIGHEIGHT, DemonBuffer + 64000);
            nextscroll = finalecount;
            yval = 0;
            return;
        }

        if (yval < 64000)
        {
            // [rfomin] scroll up one line at a time until only the top screen shows
            V_DrawScaledBlock(0, 0, ORIGWIDTH, ORIGHEIGHT, DemonBuffer + 64000 - yval);
    
            if (finalecount >= nextscroll)
            {
                yval += ORIGWIDTH; // [rfomin] move up one line
                nextscroll = finalecount + 3; // [rfomin] don't scroll too fast
            }
        }
        else
        {
            // [rfomin] finished scrolling
            V_DrawScaledBlock(0, 0, ORIGWIDTH, ORIGHEIGHT, DemonBuffer);
        }
        Z_Free(DemonBuffer);
    }
    // [crispy] assume that FINAL1 and FINAL2 are in patch format
    else
    {
        patch_t *patch1 = W_CacheLumpName(DEH_String("FINAL1"), PU_LEVEL);
        patch_t *patch2 = W_CacheLumpName(DEH_String("FINAL2"), PU_LEVEL);

        if (finalecount < 70)
        {
            V_DrawPatchFullScreen(patch1, false);
            nextscroll = finalecount;
            return;
        }

        if (yval < 64000)
        {
            int x = ((SCREENWIDTH / vid_resolution) - SHORT(patch1->width)) / 2
                  - WIDESCREENDELTA; // [crispy]

            // [crispy] pillar boxing
            if (x > -WIDESCREENDELTA)
            {
                V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
            }

            V_DrawPatch(x, y - 200, patch2);
            V_DrawPatch(x, 0 + y, patch1);

            // [rfomin] don't scroll too fast
            // [JN] and do not use "return" here to keep screen buffer active
            if (finalecount >= nextscroll)
            {
                y++;
                yval += ORIGWIDTH;
                nextscroll = finalecount + 3;
            }
        }
        else
        {
            // else, we'll just sit here and wait, for now
            V_DrawPatchFullScreen(patch2, false);
        }
    }
}

/*
==================
=
= F_DrawUnderwater
=
==================
*/

void F_DrawUnderwater(void)
{
    static boolean underwawa = false;
    const char *lumpname;
    byte *palette;

    // The underwater screen has its own palette, which is rather annoying.
    // The palette doesn't correspond to the normal palette. Because of
    // this, we must regenerate the lookup tables used in the video scaling
    // code.

    switch (finalestage)
    {
        case 1:
            if (!underwawa)
            {
                underwawa = true;
                V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
                lumpname = DEH_String("E2PAL");
                palette = W_CacheLumpName(lumpname, PU_STATIC);
#ifndef CRISPY_TRUECOLOR
                I_SetPalette(palette);
#else
                R_SetUnderwaterPalette(palette);
#endif
                W_ReleaseLumpName(lumpname);
                V_DrawFullscreenRawOrPatch(W_GetNumForName(DEH_String("E2END")));
            }
            paused = false;
            MenuActive = false;
            askforquit = false;

            break;
        case 2:
            if (underwawa)
            {
#ifndef CRISPY_TRUECOLOR
                lumpname = DEH_String("PLAYPAL");
                palette = W_CacheLumpName(lumpname, PU_STATIC);
                I_SetPalette(palette);
                W_ReleaseLumpName(lumpname);
#else
                R_InitColormaps();
#endif
                underwawa = false;
            }
            V_DrawFullscreenRawOrPatch(W_GetNumForName(DEH_String("TITLE")));
            //D_StartTitle(); // go to intro/demo mode.
    }
}


#if 0
/*
==================
=
= F_BunnyScroll
=
==================
*/

void F_BunnyScroll(void)
{
    int scrolled, x;
    patch_t *p1, *p2;
    char name[10];
    int stage;
    static int laststage;

    p1 = W_CacheLumpName("PFUB2", PU_LEVEL);
    p2 = W_CacheLumpName("PFUB1", PU_LEVEL);

    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT);

    scrolled = 320 - (finalecount - 230) / 2;
    if (scrolled > 320)
        scrolled = 320;
    if (scrolled < 0)
        scrolled = 0;

    for (x = 0; x < SCREENWIDTH; x++)
    {
        if (x + scrolled < 320)
            F_DrawPatchCol(x, p1, x + scrolled);
        else
            F_DrawPatchCol(x, p2, x + scrolled - 320);
    }

    if (finalecount < 1130)
        return;
    if (finalecount < 1180)
    {
        V_DrawPatch((SCREENWIDTH - 13 * 8) / 2, (SCREENHEIGHT - 8 * 8) / 2, 0,
                    W_CacheLumpName("END0", PU_CACHE));
        laststage = 0;
        return;
    }

    stage = (finalecount - 1180) / 5;
    if (stage > 6)
        stage = 6;
    if (stage > laststage)
    {
        S_StartSound(NULL, sfx_pistol);
        laststage = stage;
    }

    M_snprintf(name, sizeof(name), "END%i", stage);
    V_DrawPatch((SCREENWIDTH - 13 * 8) / 2, (SCREENHEIGHT - 8 * 8) / 2,
                W_CacheLumpName(name, PU_CACHE));
}
#endif

/*
=======================
=
= F_Drawer
=
=======================
*/

void F_Drawer(void)
{
    if (!finalestage)
        F_TextWrite();
    else
    {
        switch (gameepisode)
        {
            case 1:
                if (gamemode == shareware)
                {
                    V_DrawFullscreenRawOrPatch(W_GetNumForName("ORDER"));
                }
                else
                {
                    V_DrawFullscreenRawOrPatch(W_GetNumForName("CREDIT"));
                }
                break;
            case 2:
                F_DrawUnderwater();
                break;
            case 3:
                F_DemonScroll();
                break;
            case 4:            // Just show credits screen for extended episodes
            case 5:
                V_DrawFullscreenRawOrPatch(W_GetNumForName("CREDIT"));
                break;
        }
    }

    // [crispy] demo timer widget
    if (demoplayback && (demo_timer == 1 || demo_timer == 3))
    {
        ID_DemoTimer(demo_timerdir ? (deftotaldemotics - defdemotics) : defdemotics);
    }
    else if (demorecording && (demo_timer == 2 || demo_timer == 3))
    {
        ID_DemoTimer(leveltime);
    }

    // [crispy] demo progress bar
    if (demoplayback && demo_bar)
    {
        ID_DemoBar();
    }
}
