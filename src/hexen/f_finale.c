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


// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "i_system.h"
#include "i_timer.h"  // [JN] TICRATE
#include "i_video.h"
#include "m_misc.h"  // [JN] M_snprintf
#include "p_local.h"
#include "s_sound.h"
#include <ctype.h>
#include "v_video.h"
#include "i_swap.h"
#include "am_map.h"
#include "ct_chat.h"
#include "v_trans.h"

#include "id_func.h"


// MACROS ------------------------------------------------------------------

#define	TEXTSPEED	3
#define	TEXTWAIT	250

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void TextWrite(void);
static void DrawPic(void);
static void InitializeFade(boolean fadeIn);
static void DeInitializeFade(void);
static void FadePic(void);
static char *GetFinaleText(int sequence);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int FinaleStage;
static int FinaleCount;
static int FinaleEndCount;
static int FinaleLumpNum;
static int FontABaseLump;
static char *FinaleText;

// [JN] Tics representing opacity value for blending function.
static int BlendTic;

// CODE --------------------------------------------------------------------

//===========================================================================
//
// F_StartFinale
//
//===========================================================================

void F_StartFinale(void)
{
    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    viewactive = false;
    automapactive = false;
    CT_ClearMessage(&players[consoleplayer]);

    FinaleStage = 0;
    FinaleCount = 0;
    FinaleText = GetFinaleText(0);
    FinaleEndCount = 70;
    FinaleLumpNum = W_GetNumForName("FINALE1");
    FontABaseLump = W_GetNumForName("FONTA_S") + 1;
    // [JN] Force palette reset to clear pain, bonus, and poison effects.
    I_SetPalette(0);
    InitializeFade(1);

//      S_ChangeMusic(mus_victor, true);
    S_StartSongName("hall", false);     // don't loop the song
}

//===========================================================================
//
// F_Responder
//
//===========================================================================

boolean F_Responder(event_t * event)
{
    return false;
}

//===========================================================================
//
// F_Ticker
//
//===========================================================================

void F_Ticker(void)
{
    FinaleCount++;
    if (FinaleStage < 5 && FinaleCount >= FinaleEndCount)
    {
        FinaleCount = 0;
        FinaleStage++;
        switch (FinaleStage)
        {
            case 1:            // Text 1
                FinaleEndCount = strlen(FinaleText) * TEXTSPEED + TEXTWAIT;
                break;
            case 2:            // Pic 2, Text 2
                FinaleText = GetFinaleText(1);
                FinaleEndCount = strlen(FinaleText) * TEXTSPEED + TEXTWAIT;
                FinaleLumpNum = W_GetNumForName("FINALE2");
                S_StartSongName("orb", false);
                break;
            case 3:            // Pic 2 -- Fade out
                FinaleEndCount = 70;
                DeInitializeFade();
                InitializeFade(0);
                break;
            case 4:            // Pic 3 -- Fade in
                FinaleLumpNum = W_GetNumForName("FINALE3");
                FinaleEndCount = 71;
                DeInitializeFade();
                InitializeFade(1);
                S_StartSongName("chess", true);
                break;
            case 5:            // Pic 3 , Text 3
                FinaleText = GetFinaleText(2);
                DeInitializeFade();
                break;
            default:
                break;
        }
        return;
    }
    if (FinaleStage == 0 || FinaleStage == 3 || FinaleStage == 4)
    {
        FadePic();
    }
    // [JN] Fade in (from-black-to-normal) on 0 and 4 stages,
    // and fade out (from normal-to-black) on 3 stage.
    if (FinaleStage == 0 || FinaleStage == 4)
    {
        BlendTic += 4;
        if (BlendTic > 255)
        {
            BlendTic = 255;
        }
    }
    if (FinaleStage == 3)
    {
        BlendTic -= 4;
        if (BlendTic < 0)
        {
            BlendTic = 0;
        }
    }
}

//===========================================================================
//
// TextWrite
//
//===========================================================================

static void TextWrite(void)
{
    int count;
    char *ch;
    int c;
    int cx, cy;
    patch_t *w;

    V_DrawFullscreenRawOrPatch(FinaleLumpNum); // [crispy]
    if (FinaleStage == 5)
    {                           // Chess pic, draw the correct character graphic
        if (netgame)
        {
            V_DrawPatch(20, 0, W_CacheLumpName("chessall", PU_CACHE));
        }
        else if (PlayerClass[consoleplayer])
        {
            V_DrawPatch(60, 0, W_CacheLumpNum(W_GetNumForName("chessc")
                                              + PlayerClass[consoleplayer] -
                                              1, PU_CACHE));
        }
    }
    // Draw the actual text
    if (FinaleStage == 5)
    {
        cy = 135;
    }
    else
    {
        cy = 5;
    }
    cx = 20;
    ch = FinaleText;
    count = (FinaleCount - 10) / TEXTSPEED;
    if (count < 0)
    {
        count = 0;
    }
    for (; count; count--)
    {
        c = *ch++;
        if (!c)
        {
            break;
        }
        if (c == '\n')
        {
            cx = 20;
            cy += 9;
            continue;
        }
        if (c < 32)
        {
            continue;
        }
        c = toupper(c);
        if (c == 32)
        {
            cx += 5;
            continue;
        }
        w = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
        if (cx + SHORT(w->width) > SCREENWIDTH)
        {
            break;
        }
        V_DrawShadowedPatchOptional(cx, cy, 1, w);
        cx += SHORT(w->width);
    }
}

//===========================================================================
//
// InitializeFade
//
//===========================================================================

static void InitializeFade(boolean fadeIn)
{
    // [JN] Reset blending tic to initial value.
    BlendTic = fadeIn ? 0 : 255;
}

//===========================================================================
//
// DeInitializeFade
//
//===========================================================================

static void DeInitializeFade(void)
{
}

//===========================================================================
//
// FadePic
//
//===========================================================================

static void FadePic(void)
{
}

//===========================================================================
//
// DrawPic
//
//===========================================================================

static void DrawPic(void)
{
    V_DrawFullscreenRawOrPatch(FinaleLumpNum); // [crispy]
    if (FinaleStage == 4 || FinaleStage == 5)
    {                           // Chess pic, draw the correct character graphic
        if (netgame)
        {
            V_DrawPatch(20, 0, W_CacheLumpName("chessall", PU_CACHE));
        }
        else if (PlayerClass[consoleplayer])
        {
            V_DrawPatch(60, 0, W_CacheLumpNum(W_GetNumForName("chessc")
                                              + PlayerClass[consoleplayer] -
                                              1, PU_CACHE));
        }
    }
    // [JN] Apply true color blending on top of patch drawing functions.
    for (int y = 0; y < SCREENAREA; y++)
    {
        I_VideoBuffer[y] = I_BlendDark(I_VideoBuffer[y], BlendTic);
    }
}

// -----------------------------------------------------------------------------
// F_DrawTotalTime
// [JN] Draw total time on finale screens.
// -----------------------------------------------------------------------------

static void F_DrawTotalTime (void)
{
    const int worldTimer = players[consoleplayer].worldTimer;
    const int hours = worldTimer / (3600 * TICRATE);
    const int mins = worldTimer / (60 * TICRATE) % 60;
    const float secs = (float)(worldTimer % (60 * TICRATE)) / TICRATE;
    char n[32];

    if (hours)
    {
        M_snprintf(n, sizeof(n), "TOTAL TIME: %02i:%02i:%05.02f", hours, mins, secs);
    }
    else
    {
        M_snprintf(n, sizeof(n), "TOTAL TIME: %02i:%05.02f", mins, secs);
    }

    dp_translucent = true;
    MN_DrTextACentered(n, 188, cr[CR_MENU_DARK4]);
    dp_translucent = false;
}

//===========================================================================
//
// F_Drawer
//
//===========================================================================

void F_Drawer(void)
{
    switch (FinaleStage)
    {
        case 0:                // Fade in initial finale screen
            DrawPic();
            break;
        case 1:
        case 2:
            TextWrite();
            F_DrawTotalTime(); // [JN] Draw total time.
            break;
        case 3:                // Fade screen out
            DrawPic();
            break;
        case 4:                // Fade in chess screen
            DrawPic();
            break;
        case 5:
            TextWrite();
            break;
    }

    // [crispy] demo timer widget
    if (((demoplayback && (demo_timer == 1 || demo_timer == 3))
    ||   (demorecording && (demo_timer == 2 || demo_timer == 3))))
    {
        ID_DemoTimer(demo_timerdir ? (deftotaldemotics - defdemotics) : defdemotics);
    }

    // [crispy] demo progress bar
    if (demoplayback && demo_bar)
    {
        ID_DemoBar();
    }
}

//==========================================================================
//
// GetFinaleText
//
//==========================================================================

static char *GetFinaleText(int sequence)
{
    const char *msgLumpName;
    int msgSize;
    int msgLump;
    static const char *winMsgLumpNames[] = {
        "win1msg",
        "win2msg",
        "win3msg"
    };

    msgLumpName = winMsgLumpNames[sequence];
    msgLump = W_GetNumForName(msgLumpName);
    msgSize = W_LumpLength(msgLump);
    if (msgSize >= MAX_INTRMSN_MESSAGE_SIZE)
    {
        I_Error("Finale message too long (%s)", msgLumpName);
    }
    W_ReadLump(msgLump, ClusterMessage);
    ClusterMessage[msgSize] = 0;        // Append terminator
    return ClusterMessage;
}
