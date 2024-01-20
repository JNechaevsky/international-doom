//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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


#include <stdio.h>
#include <stdint.h>

#include "i_timer.h"
#include "m_misc.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomdef.h"
#include "p_local.h"
#include "r_local.h"

#include "id_vars.h"
#include "id_func.h"


// =============================================================================
//
//                        Render Counters and Widgets
//
// =============================================================================

ID_Render_t IDRender;
ID_Widget_t IDWidget;

char ID_Level_Time[64];
char ID_Total_Time[64];
char ID_Local_Time[64];

enum
{
    widget_kills,
    widget_items,
    widget_secret
} widgetcolor_t;

static byte *ID_WidgetColor (const int i)
{
    switch (i)
    {
        case widget_kills:
        {
            return
                IDWidget.totalkills == 0 ? cr[CR_GREEN] :
                IDWidget.kills == 0 ? cr[CR_RED] :
                IDWidget.kills < IDWidget.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN];
            break;
        }
        case widget_items:
        {
            return
                IDWidget.totalitems == 0 ? cr[CR_GREEN] :
                IDWidget.items == 0 ? cr[CR_RED] :
                IDWidget.items < IDWidget.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN];
            break;
        }
        case widget_secret:
        {
            return
                IDWidget.totalsecrets == 0 ? cr[CR_GREEN] :
                IDWidget.secrets == 0 ? cr[CR_RED] :
                IDWidget.secrets < IDWidget.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN];
            break;
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// ID_LeftWidgets.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void ID_LeftWidgets (void)
{
    //
    // Located on the top
    //
    if (widget_location == 1)
    {
        // K/I/S stats
        if (widget_kis == 1
        || (widget_kis == 2 && automapactive))
        {
            if (!deathmatch)
            {
                char str1[16];  // kills
                char str2[16];  // items
                char str3[16];  // secret

                // Kills:
                MN_DrTextA("K:", 0 - WIDESCREENDELTA, 10, cr[CR_GRAY]);
                sprintf(str1, "%d/%d", IDWidget.kills, IDWidget.totalkills);
                MN_DrTextA(str1, 0 - WIDESCREENDELTA + 16, 10, ID_WidgetColor(widget_kills));

                // Items:
                MN_DrTextA("I:", 0 - WIDESCREENDELTA, 20, cr[CR_GRAY]);
                sprintf(str2, "%d/%d", IDWidget.items, IDWidget.totalitems);
                MN_DrTextA(str2, 0 - WIDESCREENDELTA + 16, 20, ID_WidgetColor(widget_items));

                // Secret:
                MN_DrTextA("S:", 0 - WIDESCREENDELTA, 30, cr[CR_GRAY]);
                sprintf(str3, "%d/%d", IDWidget.secrets, IDWidget.totalsecrets);
                MN_DrTextA(str3, 0 - WIDESCREENDELTA + 16, 30, ID_WidgetColor(widget_secret));
            }
            else
            {
                char str1[16] = {0};  // Green
                char str2[16] = {0};  // Yellow
                char str3[16] = {0};  // Red
                char str4[16] = {0};  // Blue

                // Green
                if (playeringame[0])
                {
                    MN_DrTextA("G:", 0 - WIDESCREENDELTA, 10, cr[CR_GREEN]);
                    sprintf(str1, "%d", IDWidget.frags_g);
                    MN_DrTextA(str1, 0 - WIDESCREENDELTA + 16, 10, cr[CR_GREEN]);
                }
                // Yellow
                if (playeringame[1])
                {
                    MN_DrTextA("Y:", 0 - WIDESCREENDELTA, 20, cr[CR_YELLOW]);
                    sprintf(str2, "%d", IDWidget.frags_y);
                    MN_DrTextA(str2, 0 - WIDESCREENDELTA + 16, 20, cr[CR_YELLOW]);
                }
                // Red
                if (playeringame[2])
                {
                    MN_DrTextA("R:", 0 - WIDESCREENDELTA, 30, cr[CR_RED]);
                    sprintf(str3, "%d", IDWidget.frags_r);
                    MN_DrTextA(str3, 0 - WIDESCREENDELTA + 16, 30, cr[CR_RED]);
                }
                // Blue
                if (playeringame[3])
                {
                    MN_DrTextA("B:", 0 - WIDESCREENDELTA, 40, cr[CR_BLUE2]);
                    sprintf(str4, "%d", IDWidget.frags_b);
                    MN_DrTextA(str4, 0 - WIDESCREENDELTA + 16, 40, cr[CR_BLUE2]);
                }
            }
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            MN_DrTextA("TIME", 0 - WIDESCREENDELTA, 40, cr[CR_GRAY]);
            MN_DrTextA(ID_Level_Time, 0 - WIDESCREENDELTA, 50, cr[CR_LIGHTGRAY]);
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            MN_DrTextA("TOTAL", 0 - WIDESCREENDELTA, 60, cr[CR_GRAY]);
            MN_DrTextA(ID_Total_Time, 0 - WIDESCREENDELTA, 70, cr[CR_LIGHTGRAY]);
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];
            
            MN_DrTextA("X:", 0 - WIDESCREENDELTA, 80, cr[CR_GRAY]);
            MN_DrTextA("Y:", 0 - WIDESCREENDELTA, 90, cr[CR_GRAY]);
            MN_DrTextA("ANG:", 0 - WIDESCREENDELTA, 100, cr[CR_GRAY]);

            sprintf(str, "%d", IDWidget.x);
            MN_DrTextA(str, 0 - WIDESCREENDELTA + 16, 80, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.y);
            MN_DrTextA(str, 0 - WIDESCREENDELTA + 16, 90, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.ang);
            MN_DrTextA(str, 0 - WIDESCREENDELTA + 32, 100, cr[CR_GREEN]);
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];

            // Sprites
            MN_DrTextA("SPR:", 0 - WIDESCREENDELTA, 110, cr[CR_GRAY]);
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            MN_DrTextA(spr, 32 - WIDESCREENDELTA, 110, cr[CR_GREEN]);

            // Segments
            MN_DrTextA("SEG:", 0 - WIDESCREENDELTA, 120, cr[CR_GRAY]);
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            MN_DrTextA(seg, 32 - WIDESCREENDELTA, 120, cr[CR_GREEN]);

            // Openings
            MN_DrTextA("OPN:", 0 - WIDESCREENDELTA, 130, cr[CR_GRAY]);
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            MN_DrTextA(opn, 32 - WIDESCREENDELTA, 130, cr[CR_GREEN]);

            // Planes
            MN_DrTextA("PLN:", 0 - WIDESCREENDELTA, 140, cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            MN_DrTextA(vis, 32 - WIDESCREENDELTA, 140, cr[CR_GREEN]);
        }
    }
    //
    // Located at the bottom
    //
    else
    {
        int yy = 0;

        // Shift widgets one line up if Level Name widget is set to "always".
        if (widget_levelname && !automapactive)
        {
            yy -= 10;
        }
        // Move widgets slightly more down when using a fullscreen status bar.
        if (dp_screen_size > 10 && (!automapactive || automap_overlay))
        {
            yy += 13;
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];
            const int yy1 = widget_coords ? 0 : 45;

            // Sprites
            MN_DrTextA("SPR:", 0 - WIDESCREENDELTA, 26 + yy1, cr[CR_GRAY]);
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            MN_DrTextA(spr, 32 - WIDESCREENDELTA, 26 + yy1, cr[CR_GREEN]);

            // Segments
            MN_DrTextA("SEG:", 0 - WIDESCREENDELTA, 36 + yy1, cr[CR_GRAY]);
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            MN_DrTextA(seg, 32 - WIDESCREENDELTA, 36 + yy1, cr[CR_GREEN]);

            // Openings
            MN_DrTextA("OPN:", 0 - WIDESCREENDELTA, 46 + yy1, cr[CR_GRAY]);
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            MN_DrTextA(opn, 32 - WIDESCREENDELTA, 46 + yy1, cr[CR_GREEN]);

            // Planes
            MN_DrTextA("PLN:", 0 - WIDESCREENDELTA, 56 + yy1, cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            MN_DrTextA(vis, 32 - WIDESCREENDELTA, 56 + yy1, cr[CR_GREEN]);
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            MN_DrTextA("X:", 0 - WIDESCREENDELTA, 76, cr[CR_GRAY]);
            MN_DrTextA("Y:", 0 - WIDESCREENDELTA, 86, cr[CR_GRAY]);
            MN_DrTextA("ANG:", 0 - WIDESCREENDELTA, 96, cr[CR_GRAY]);

            sprintf(str, "%d", IDWidget.x);
            MN_DrTextA(str, 16 - WIDESCREENDELTA, 76, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.y);
            MN_DrTextA(str, 16 - WIDESCREENDELTA, 86, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.ang);
            MN_DrTextA(str, 32 - WIDESCREENDELTA, 96, cr[CR_GREEN]);
        }

        if (automapactive)
        {
            yy -= 10;
        }

        // K/I/S stats
        if (widget_kis == 1
        || (widget_kis == 2 && automapactive))
        {
            if (!deathmatch)
            {
                char str1[8], str2[16];  // kills
                char str3[8], str4[16];  // items
                char str5[8], str6[16];  // secret
        
                // Kills:
                sprintf(str1, "K ");
                MN_DrTextA(str1, 0 - WIDESCREENDELTA, 146 + yy, cr[CR_GRAY]);
                sprintf(str2, "%d/%d ", IDWidget.kills, IDWidget.totalkills);
                MN_DrTextA(str2, 0 - WIDESCREENDELTA + MN_TextAWidth(str1), 146 + yy, ID_WidgetColor(widget_kills));

                // Items:
                sprintf(str3, "I ");
                MN_DrTextA(str3, 0 - WIDESCREENDELTA + MN_TextAWidth(str1) + MN_TextAWidth(str2), 146 + yy, cr[CR_GRAY]);

                sprintf(str4, "%d/%d ", IDWidget.items, IDWidget.totalitems);
                MN_DrTextA(str4, 0 - WIDESCREENDELTA     +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3), 146 + yy, ID_WidgetColor(widget_items));

                // Secret:
                sprintf(str5, "S ");
                MN_DrTextA(str5, 0 - WIDESCREENDELTA     +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4), 146 + yy, cr[CR_GRAY]);

                sprintf(str6, "%d/%d", IDWidget.secrets, IDWidget.totalsecrets);
                MN_DrTextA(str6, 0 - WIDESCREENDELTA     +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) + 
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4) +
                                     MN_TextAWidth(str5), 146 + yy, ID_WidgetColor(widget_secret));
            }
            else
            {
                char str1[8] = {0}, str2[16] = {0};  // Green
                char str3[8] = {0}, str4[16] = {0};  // Yellow
                char str5[8] = {0}, str6[16] = {0};  // Red
                char str7[8] = {0}, str8[16] = {0};  // Blue

                // Green
                if (playeringame[0])
                {
                    sprintf(str1, "G ");
                    MN_DrTextA(str1, 0 - WIDESCREENDELTA, 146 + yy, cr[CR_GREEN]);

                    sprintf(str2, "%d ", IDWidget.frags_g);
                    MN_DrTextA(str2, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1), 146 + yy, cr[CR_GREEN]);
                }
                // Yellow
                if (playeringame[1])
                {
                    sprintf(str3, "Y ");
                    MN_DrTextA(str3, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2),
                                         146 + yy, cr[CR_YELLOW]);

                    sprintf(str4, "%d ", IDWidget.frags_y);
                    MN_DrTextA(str4, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2) +
                                         MN_TextAWidth(str3),
                                         146 + yy, cr[CR_YELLOW]);
                }
                // Red
                if (playeringame[2])
                {
                    sprintf(str5, "R ");
                    MN_DrTextA(str5, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2) +
                                         MN_TextAWidth(str3) +
                                         MN_TextAWidth(str4),
                                         146 + yy, cr[CR_RED]);

                    sprintf(str6, "%d ", IDWidget.frags_r);
                    MN_DrTextA(str6, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2) +
                                         MN_TextAWidth(str3) +
                                         MN_TextAWidth(str4) +
                                         MN_TextAWidth(str5),
                                         146 + yy, cr[CR_RED]);
                }
                // Blue
                if (playeringame[3])
                {
                    sprintf(str7, "B ");
                    MN_DrTextA(str7, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2) +
                                         MN_TextAWidth(str3) +
                                         MN_TextAWidth(str4) +
                                         MN_TextAWidth(str5) +
                                         MN_TextAWidth(str6),
                                         146 + yy, cr[CR_BLUE2]);

                    sprintf(str8, "%d ", IDWidget.frags_b);
                    MN_DrTextA(str8, 0 - WIDESCREENDELTA +
                                         MN_TextAWidth(str1) +
                                         MN_TextAWidth(str2) +
                                         MN_TextAWidth(str3) +
                                         MN_TextAWidth(str4) +
                                         MN_TextAWidth(str5) +
                                         MN_TextAWidth(str6) +
                                         MN_TextAWidth(str7),
                                         146 + yy, cr[CR_BLUE2]);
                }
            }
        }

        if (widget_kis)
        {
            yy -= 10;
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            char stra[8];

            sprintf(stra, "TOTAL ");
            MN_DrTextA(stra, 0 - WIDESCREENDELTA, 146 + yy, cr[CR_GRAY]);
            MN_DrTextA(ID_Total_Time, 0 - WIDESCREENDELTA + MN_TextAWidth(stra), 146 + yy, cr[CR_LIGHTGRAY]);
        }

        if (widget_totaltime)
        {
            yy -= 10;
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            char stra[8];

            sprintf(stra, "TIME ");
            MN_DrTextA(stra, 0 - WIDESCREENDELTA, 146 + yy, cr[CR_GRAY]);
            MN_DrTextA(ID_Level_Time, 0 - WIDESCREENDELTA + MN_TextAWidth(stra), 146 + yy, cr[CR_LIGHTGRAY]);
        }

    }
}

// -----------------------------------------------------------------------------
// ID_RightWidgets.
//  [JN] Draw actual frames per second value.
//  Some MN_TextAWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void ID_RightWidgets (void)
{
    int yy = 0;

    // [JN] If demo timer is active and running,
    // shift FPS and time widgets one line down.
    if ((demoplayback && (demo_timer == 1 || demo_timer == 3))
    ||  (demorecording && (demo_timer == 2 || demo_timer == 3)))
    {
        yy += 10;
    }

    // [JN] FPS counter
    if (vid_showfps)
    {
        char fps[8];
        char fps_str[4];

        sprintf(fps, "%d", id_fps_value);
        sprintf(fps_str, "FPS");

        MN_DrTextA(fps, ORIGWIDTH + WIDESCREENDELTA - 11 - MN_TextAWidth(fps) 
                                  - MN_TextAWidth(fps_str), 10 + yy, cr[CR_LIGHTGRAY_DARK1]);

        MN_DrTextA(fps_str, ORIGWIDTH + WIDESCREENDELTA - 7 - MN_TextAWidth(fps_str), 10 + yy, cr[CR_LIGHTGRAY_DARK1]);

        yy += 10;
    }

    // [JN] Local time. Time gathered in G_Ticker.
    if (msg_local_time)
    {
        MN_DrTextA(ID_Local_Time, ORIGWIDTH + WIDESCREENDELTA - 7
                              - MN_TextAWidth(ID_Local_Time), 10 + yy, cr[CR_GRAY]);
    }
}

// -----------------------------------------------------------------------------
// ID_HealthColor, ID_DrawTargetsHealth
//  [JN] Indicates and colorizes current target's health.
// -----------------------------------------------------------------------------

static byte *ID_HealthColor (const int val1, const int val2)
{
    return
        val1 <= val2/4 ? cr[CR_RED]    :
        val1 <= val2/2 ? cr[CR_YELLOW] :
                         cr[CR_GREEN]  ;
}

void ID_DrawTargetsHealth (void)
{
    char str[16];
    player_t *player = &players[displayplayer];

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);

    if (widget_health == 1)  // Top
    {
        MN_DrTextACentered(str, 10, ID_HealthColor(player->targetsheath,
                                                   player->targetsmaxheath));
    }
    else
    if (widget_health == 2)  // Top + name
    {
        MN_DrTextACentered(player->targetsname, 10, ID_HealthColor(player->targetsheath,
                                                                   player->targetsmaxheath));
        MN_DrTextACentered(str, 20, ID_HealthColor(player->targetsheath,
                                                   player->targetsmaxheath));
    }
    else
    if (widget_health == 3)  // Bottom
    {
        MN_DrTextACentered(str, 145, ID_HealthColor(player->targetsheath,
                                                    player->targetsmaxheath));
    }
    else
    if (widget_health == 4)  // Bottom + name
    {
        MN_DrTextACentered(player->targetsname, 145, ID_HealthColor(player->targetsheath,
                                                     player->targetsmaxheath));
        MN_DrTextACentered(str, 135, ID_HealthColor(player->targetsheath,
                                                    player->targetsmaxheath));
    }
}

// =============================================================================
//
//                                 Crosshair
//
// =============================================================================

// -----------------------------------------------------------------------------
// [JN] Crosshair graphical patches in Doom GFX format.
// -----------------------------------------------------------------------------

static const byte xhair_cross1[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x2A, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 
    0x3E, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 
    0x03, 0x01, 0x17, 0x17, 0x17, 0xFF, 0x03, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 
    0xFF, 0x00, 0x02, 0x17, 0x17, 0x1F, 0x1F, 0x05, 0x02, 0x1F, 0x1F, 0x17, 
    0x17, 0xFF, 0xFF, 0x03, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 0x03, 0x01, 0x17, 
    0x17, 0x17, 0xFF
};

static const byte xhair_cross2[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 
    0x34, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0x03, 0x01, 0x17, 0x17, 0x17, 0xFF, 0x02, 0x03, 0x17, 0x17, 
    0x1F, 0x17, 0x17, 0xFF, 0x03, 0x01, 0x17, 0x17, 0x17, 0xFF, 0xFF, 0xFF
};

static const byte xhair_x[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0x3C, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 
    0xFF, 0x01, 0x01, 0x17, 0x17, 0x17, 0x05, 0x01, 0x17, 0x17, 0x17, 0xFF, 
    0x02, 0x01, 0x1F, 0x1F, 0x1F, 0x04, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 
    0x02, 0x01, 0x1F, 0x1F, 0x1F, 0x04, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 0x01, 
    0x01, 0x17, 0x17, 0x17, 0x05, 0x01, 0x17, 0x17, 0x17, 0xFF, 0xFF
};

static const byte xhair_circle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 
    0x43, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 
    0xFF, 0x02, 0x03, 0x17, 0x17, 0x1F, 0x17, 0x17, 0xFF, 0x01, 0x01, 0x17, 
    0x17, 0x17, 0x05, 0x01, 0x17, 0x17, 0x17, 0xFF, 0x01, 0x01, 0x1F, 0x1F, 
    0x1F, 0x05, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 0x01, 0x01, 0x17, 0x17, 0x17, 
    0x05, 0x01, 0x17, 0x17, 0x17, 0xFF, 0x02, 0x03, 0x17, 0x17, 0x1F, 0x17, 
    0x17, 0xFF, 0xFF
};

static const byte xhair_angle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 
    0x2F, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0x03, 0x03, 0x1F, 0x1F, 0x17, 0x12, 0x12, 0xFF, 0x03, 
    0x01, 0x17, 0x17, 0x17, 0xFF, 0x03, 0x01, 0x12, 0x12, 0x12, 0xFF, 0xFF
};

static const byte xhair_triangle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 
    0x3D, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x4A, 0x00, 0x00, 0x00, 
    0xFF, 0x05, 0x01, 0x12, 0x12, 0x12, 0xFF, 0x04, 0x02, 0x17, 0x17, 0x17, 
    0x17, 0xFF, 0x03, 0x01, 0x1F, 0x1F, 0x1F, 0x05, 0x01, 0x1F, 0x1F, 0x1F, 
    0xFF, 0x04, 0x02, 0x17, 0x17, 0x17, 0x17, 0xFF, 0x05, 0x01, 0x12, 0x12, 
    0x12, 0xFF, 0xFF
};

static const byte xhair_dot[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 
    0x2D, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0x03, 0x01, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF
};

// -----------------------------------------------------------------------------
// ID_CrosshairShape
//  [JN] Decides which patch should be drawn, depending on "xhair_draw" variable.
// -----------------------------------------------------------------------------

static patch_t *ID_CrosshairShape (void)
{
    return
        xhair_draw == 1 ? (patch_t*) &xhair_cross1   :
        xhair_draw == 2 ? (patch_t*) &xhair_cross2   :
        xhair_draw == 3 ? (patch_t*) &xhair_x        :
        xhair_draw == 4 ? (patch_t*) &xhair_circle   :
        xhair_draw == 5 ? (patch_t*) &xhair_angle    :
        xhair_draw == 6 ? (patch_t*) &xhair_triangle :
                          (patch_t*) &xhair_dot;
}

// -----------------------------------------------------------------------------
// ID_CrosshairColor
//  [JN] Coloring routine, depending on "xhair_color" variable.
// -----------------------------------------------------------------------------

static byte *ID_CrosshairColor (int type)
{
    const player_t *player = &players[displayplayer];

    switch (type)
    {
        case 0:
        {
            // Static/uncolored.
            return
                cr[CR_RED];
            break;
        }
        case 1:
        {
            // Health.
            // Values are same to status bar coloring (ST_WidgetColor).
            return
                player->health >= 67 ? cr[CR_GREEN]  :
                player->health >= 34 ? cr[CR_YELLOW] :
                                       cr[CR_RED]    ;
            break;
        }
        case 2:
        {
            // Target highlight.
            // "linetarget" is gathered via intercept-safe call 
            // of P_AimLineAttack in G_Ticker.
            return
                linetarget ? cr[CR_BLUE2] : cr[CR_RED];
            break;
        }
        case 3:
        {
            // Target highlight+health.
            return
                linetarget           ? cr[CR_BLUE2]  :
                player->health >= 67 ? cr[CR_GREEN]  :
                player->health >= 34 ? cr[CR_YELLOW] :
                                       cr[CR_RED]    ;
            break;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// ID_DrawCrosshair
//  [JN] Drawing routine, called via D_Display.
// -----------------------------------------------------------------------------

void ID_DrawCrosshair (void)
{
    const int xx = (ORIGWIDTH  / 2) - 3;
    const int yy = (ORIGHEIGHT / 2) - 3 - (dp_screen_size <= 10 ? 20 : 0);

    dp_translation = ID_CrosshairColor(xhair_color);
    V_DrawPatch(xx, yy, ID_CrosshairShape());
    dp_translation = NULL;
}

// =============================================================================
//
//                             Demo enhancements
//
// =============================================================================

// -----------------------------------------------------------------------------
// ID_DemoTimer
//  [crispy] Demo Timer widget
// -----------------------------------------------------------------------------

void ID_DemoTimer (const int time)
{
    const int hours = time / (3600 * TICRATE);
    const int mins = time / (60 * TICRATE) % 60;
    const float secs = (float)(time % (60 * TICRATE)) / TICRATE;
    char n[16];
    int x = 237;

    if (hours)
    {
        M_snprintf(n, sizeof(n), "%02i:%02i:%05.02f", hours, mins, secs);
    }
    else
    {
        M_snprintf(n, sizeof(n), "%02i:%05.02f", mins, secs);
        x += 20;
    }

    MN_DrTextA(n, x + WIDESCREENDELTA, 10, cr[CR_LIGHTGRAY]);
}

// -----------------------------------------------------------------------------
// ID_DemoBar
//  [crispy] print a bar indicating demo progress at the bottom of the screen
// -----------------------------------------------------------------------------

void ID_DemoBar (void)
{
    static boolean colors_set = false;
    static int black = 0;
    static int white = 0;
    const int i = SCREENWIDTH * defdemotics / deftotaldemotics;

    // [JN] Don't rely on palette indexes,
    // try to find nearest colors instead.
    if (!colors_set)
    {
#ifndef CRISPY_TRUECOLOR
        black = I_GetPaletteIndex(0, 0, 0);
        white = I_GetPaletteIndex(255, 255, 255);
#else
        black = I_MapRGB(0, 0, 0);
        white = I_MapRGB(255, 255, 255);
#endif
        colors_set = true;
    }

#ifndef CRISPY_TRUECOLOR
    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, black); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, white); // [crispy] white
#else
    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, black); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, white); // [crispy] white
#endif
}

// =============================================================================
//
//                           Level Select one variable
//
// =============================================================================

int level_select[] = {
    2,    //  0 - Skill level
    1,    //  1 - Episode
    1,    //  2 - Map
    100,  //  3 - Health
    0,    //  4 - Armor
    1,    //  5 - Armor type
    0,    //  6 - Gauntlets
    0,    //  7 - Ethereal Crossbow
    0,    //  8 - Dragon Claw
    0,    //  9 - Hellstaff
    0,    // 10 - Phoenix Rod
    0,    // 11 - Firemace
    0,    // 12 - Bag of Holding
    50,   // 13 - Wand crystals
    0,    // 14 - Ethereal arrows
    0,    // 15 - Claw orbs
    0,    // 16 - Hellstaff runes
    0,    // 17 - Flame orbs
    0,    // 18 - Mace spheres
    0,    // 19 - Yellow key
    0,    // 20 - Green key
    0,    // 21 - Blue key
    0,    // 22 - Fast monsters
    0,    // 23 - Respawning monsters
    0,    // 24 - Quartz flask
    0,    // 25 - Mystic urn
    0,    // 26 - Time bomb
    0,    // 27 - Tome of power
    0,    // 28 - Ring of invincibility
    0,    // 29 - Morph ovum
    0,    // 30 - Chaos device
    0,    // 31 - Shadowsphere
    0,    // 32 - Wings of wrath
    0,    // 33 - Torch
};

// =============================================================================
//
//                              Spectator Mode
//
// =============================================================================

// Camera position and orientation.
fixed_t CRL_camera_x, CRL_camera_y, CRL_camera_z;
angle_t CRL_camera_ang;

// [JN] An "old" position and orientation used for interpolation.
fixed_t CRL_camera_oldx, CRL_camera_oldy, CRL_camera_oldz;
angle_t CRL_camera_oldang;

// -----------------------------------------------------------------------------
// CRL_GetCameraPos
//  Returns the camera position.
// -----------------------------------------------------------------------------

void CRL_GetCameraPos (fixed_t *x, fixed_t *y, fixed_t *z, angle_t *a)
{
    *x = CRL_camera_x;
    *y = CRL_camera_y;
    *z = CRL_camera_z;
    *a = CRL_camera_ang;
}

// -----------------------------------------------------------------------------
// CRL_ReportPosition
//  Reports the position of the camera.
//  @param x The x position.
//  @param y The y position.
//  @param z The z position.
//  @param angle The angle used.
// -----------------------------------------------------------------------------

void CRL_ReportPosition (fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
	CRL_camera_oldx = x;
	CRL_camera_oldy = y;
	CRL_camera_oldz = z;
	CRL_camera_oldang = angle;
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCamera
//  @param fwm Forward movement.
//  @param swm Sideways movement.
//  @param at Angle turning.
// -----------------------------------------------------------------------------

void CRL_ImpulseCamera (fixed_t fwm, fixed_t swm, angle_t at)
{
    // Rotate camera first
    CRL_camera_ang += at << FRACBITS;

    // Forward movement
    at = CRL_camera_ang >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(fwm * 32768, finecosine[at]); 
    CRL_camera_y += FixedMul(fwm * 32768, finesine[at]);

    // Sideways movement
    at = (CRL_camera_ang - ANG90) >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(swm * 32768, finecosine[at]); 
    CRL_camera_y += FixedMul(swm * 32768, finesine[at]);
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCameraVert
//  [JN] Impulses the camera up/down.
//  @param direction: true = up, false = down.
//  @param intensity: 32 of 64 map unit, depending on player run mode.
// -----------------------------------------------------------------------------

void CRL_ImpulseCameraVert (boolean direction, fixed_t intensity)
{
    if (direction)
    {
        CRL_camera_z += FRACUNIT*intensity;
    }
    else
    {
        CRL_camera_z -= FRACUNIT*intensity;
    }
}
