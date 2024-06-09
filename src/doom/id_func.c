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

#include "v_trans.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_menu.h"
#include "m_misc.h"
#include "p_local.h"

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
                M_WriteText(0 - WIDESCREENDELTA, 9, "K:", cr[CR_GRAY]);
                if (IDWidget.extrakills)
                {
                    sprintf(str1, "%d+%d/%d ", IDWidget.kills, IDWidget.extrakills, IDWidget.totalkills);
                }
                else
                {
                    sprintf(str1, "%d/%d ", IDWidget.kills, IDWidget.totalkills);
                }
                M_WriteText(0 - WIDESCREENDELTA + 16, 9, str1, ID_WidgetColor(widget_kills));

                // Items:
                M_WriteText(0 - WIDESCREENDELTA, 18, "I:", cr[CR_GRAY]);
                sprintf(str2, "%d/%d ", IDWidget.items, IDWidget.totalitems);
                M_WriteText(0 - WIDESCREENDELTA + 16, 18, str2, ID_WidgetColor(widget_items));

                // Secret:
                M_WriteText(0 - WIDESCREENDELTA, 27, "S:", cr[CR_GRAY]);
                sprintf(str3, "%d/%d ", IDWidget.secrets, IDWidget.totalsecrets);
                M_WriteText(0 - WIDESCREENDELTA + 16, 27, str3, ID_WidgetColor(widget_secret));
            }
            else
            {
                char str1[16] = {0};  // Green
                char str2[16] = {0};  // Indigo
                char str3[16] = {0};  // Brown
                char str4[16] = {0};  // Red

                // Green
                if (playeringame[0])
                {
                    M_WriteText(0 - WIDESCREENDELTA, 9, "G:", cr[CR_GREEN]);
                    sprintf(str1, "%d", IDWidget.frags_g);
                    M_WriteText(0 - WIDESCREENDELTA + 16, 9, str1, cr[CR_GREEN]);
                }
                // Indigo
                if (playeringame[1])
                {
                    M_WriteText(0 - WIDESCREENDELTA, 18, "I:", cr[CR_GRAY]);
                    sprintf(str2, "%d", IDWidget.frags_i);
                    M_WriteText(0 - WIDESCREENDELTA + 16, 18, str2, cr[CR_GRAY]);
                }
                // Brown
                if (playeringame[2])
                {
                    M_WriteText(0 - WIDESCREENDELTA, 27, "B:", cr[CR_BROWN]);
                    sprintf(str3, "%d", IDWidget.frags_b);
                    M_WriteText(0 - WIDESCREENDELTA + 16, 27, str3, cr[CR_BROWN]);
                }
                // Red
                if (playeringame[3])
                {
                    M_WriteText(0 - WIDESCREENDELTA, 36, "B:", cr[CR_RED]);
                    sprintf(str4, "%d", IDWidget.frags_r);
                    M_WriteText(0 - WIDESCREENDELTA + 16, 36, str4, cr[CR_RED]);
                }
            }
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            M_WriteText(0 - WIDESCREENDELTA, 45, "TIME", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 54, ID_Level_Time, cr[CR_LIGHTGRAY]);
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            M_WriteText(0 - WIDESCREENDELTA, 63, "TOTAL", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 72, ID_Total_Time, cr[CR_LIGHTGRAY]);
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            M_WriteText(0 - WIDESCREENDELTA, 90, "X:", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 99, "Y:", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 108, "ANG:", cr[CR_GRAY]);

            sprintf(str, "%d", IDWidget.x);
            M_WriteText(0 - WIDESCREENDELTA + 16, 90, str, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.y);
            M_WriteText(0 - WIDESCREENDELTA + 16, 99, str, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.ang);
            M_WriteText(0 - WIDESCREENDELTA + 32, 108, str, cr[CR_GREEN]);
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];

            // Sprites
            M_WriteText(0 - WIDESCREENDELTA, 124, "SPR:", cr[CR_GRAY]);
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            M_WriteText(32 - WIDESCREENDELTA, 124, spr, cr[CR_GREEN]);

            // Segments (256 max)
            M_WriteText(0 - WIDESCREENDELTA, 133, "SEG:", cr[CR_GRAY]);
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            M_WriteText(32 - WIDESCREENDELTA, 133, seg, cr[CR_GREEN]);

            // Openings
            M_WriteText(0 - WIDESCREENDELTA, 142, "OPN:", cr[CR_GRAY]);
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            M_WriteText(32 - WIDESCREENDELTA, 142, opn, cr[CR_GREEN]);

            // Planes
            M_WriteText(0 - WIDESCREENDELTA, 151, "PLN:", cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            M_WriteText(32 - WIDESCREENDELTA, 151, vis, cr[CR_GREEN]);
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
            yy -= 9;
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];
            const int yy1 = widget_coords ? 0 : 34;

            // Sprites
            M_WriteText(0 - WIDESCREENDELTA, 54 + yy1, "SPR:", cr[CR_GRAY]);
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            M_WriteText(32 - WIDESCREENDELTA, 54 + yy1, spr, cr[CR_GREEN]);

            // Segments (256 max)
            M_WriteText(0 - WIDESCREENDELTA, 63 + yy1, "SEG:", cr[CR_GRAY]);
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            M_WriteText(32 - WIDESCREENDELTA, 63 + yy1, seg, cr[CR_GREEN]);

            // Openings
            M_WriteText(0 - WIDESCREENDELTA, 72 + yy1, "OPN:", cr[CR_GRAY]);
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            M_WriteText(32 - WIDESCREENDELTA, 72 + yy1, opn, cr[CR_GREEN]);

            // Planes
            M_WriteText(0 - WIDESCREENDELTA, 81 + yy1, "PLN:", cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            M_WriteText(32 - WIDESCREENDELTA, 81 + yy1, vis, cr[CR_GREEN]);
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            M_WriteText(0 - WIDESCREENDELTA, 97, "X:", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 106, "Y:", cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA, 115, "ANG:", cr[CR_GRAY]);

            sprintf(str, "%d", IDWidget.x);
            M_WriteText(16 - WIDESCREENDELTA, 97, str, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.y);
            M_WriteText(16 - WIDESCREENDELTA, 106, str, cr[CR_GREEN]);
            sprintf(str, "%d", IDWidget.ang);
            M_WriteText(32 - WIDESCREENDELTA, 115, str, cr[CR_GREEN]);
        }

        if (automapactive)
        {
            yy -= 9;
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
                M_WriteText(0 - WIDESCREENDELTA, 160 + yy, str1, cr[CR_GRAY]);
                
                if (IDWidget.extrakills)
                {
                    sprintf(str2, "%d+%d/%d ", IDWidget.kills, IDWidget.extrakills, IDWidget.totalkills);
                }
                else
                {
                    sprintf(str2, "%d/%d ", IDWidget.kills, IDWidget.totalkills);
                }
                M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(str1), 160 + yy, str2, ID_WidgetColor(widget_kills));
        
                // Items:
                sprintf(str3, "I ");
                M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(str1) + M_StringWidth(str2), 160 + yy, str3, cr[CR_GRAY]);
                
                sprintf(str4, "%d/%d ", IDWidget.items, IDWidget.totalitems);
                M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3), 160 + yy, str4, ID_WidgetColor(widget_items));
        
                // Secret:
                sprintf(str5, "S ");
                M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4), 160 + yy, str5, cr[CR_GRAY]);
        
                sprintf(str6, "%d/%d ", IDWidget.secrets, IDWidget.totalsecrets);
                M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(str1) +
                            M_StringWidth(str2) + 
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5), 160 + yy, str6, ID_WidgetColor(widget_secret));
            }
            else
            {
                char str1[8] = {0}, str2[16] = {0};  // Green
                char str3[8] = {0}, str4[16] = {0};  // Indigo
                char str5[8] = {0}, str6[16] = {0};  // Brown
                char str7[8] = {0}, str8[16] = {0};  // Red

                // Green
                if (playeringame[0])
                {
                    sprintf(str1, "G ");
                    M_WriteText(0 - WIDESCREENDELTA, 160 + yy, str1, cr[CR_GREEN]);

                    sprintf(str2, "%d ", IDWidget.frags_g);
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1), 160 + yy, str2, cr[CR_GREEN]);
                }
                // Indigo
                if (playeringame[1])
                {
                    sprintf(str3, "I ");
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2),
                                160 + yy, str3, cr[CR_GRAY]);

                    sprintf(str4, "%d ", IDWidget.frags_i);
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3),
                                160 + yy, str4, cr[CR_GRAY]);
                }
                // Brown
                if (playeringame[2])
                {
                    sprintf(str5, "B ");
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4),
                                160 + yy, str5, cr[CR_BROWN]);

                    sprintf(str6, "%d ", IDWidget.frags_b);
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5),
                                160 + yy, str6, cr[CR_BROWN]);
                }
                // Red
                if (playeringame[3])
                {
                    sprintf(str7, "R ");
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5) +
                                M_StringWidth(str6),
                                160 + yy, str7, cr[CR_RED]);

                    sprintf(str8, "%d ", IDWidget.frags_r);
                    M_WriteText(0 - WIDESCREENDELTA +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5) +
                                M_StringWidth(str6) +
                                M_StringWidth(str7),
                                160 + yy, str8, cr[CR_RED]);
                }
            }
        }

        if (widget_kis)
        {
            yy -= 9;
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            char stra[8];

            sprintf(stra, "TOTAL ");
            M_WriteText(0 - WIDESCREENDELTA, 160 + yy, stra, cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(stra), 160 + yy, ID_Total_Time, cr[CR_LIGHTGRAY]);
        }

        if (widget_totaltime)
        {
            yy -= 9;
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            char stra[8];

            sprintf(stra, "TIME ");
            M_WriteText(0 - WIDESCREENDELTA, 160 + yy, stra, cr[CR_GRAY]);
            M_WriteText(0 - WIDESCREENDELTA + M_StringWidth(stra), 160 + yy, ID_Level_Time, cr[CR_LIGHTGRAY]);
        }
    }
}

// -----------------------------------------------------------------------------
// ID_RightWidgets.
//  [JN] Draw actual frames per second value.
//  Some M_StringWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void ID_RightWidgets (void)
{
    int yy = 9;

    // [JN] If demo timer is active and running,
    // shift FPS and time widgets one line down.
    if ((demoplayback && (demo_timer == 1 || demo_timer == 3))
    ||  (demorecording && (demo_timer == 2 || demo_timer == 3)))
    {
        yy += 9;
    }

    // [JN] FPS counter
    if (vid_showfps)
    {
        char fps[8];
        char fps_str[4];

        sprintf(fps, "%d", id_fps_value);
        sprintf(fps_str, "FPS");

        M_WriteText(ORIGWIDTH + WIDESCREENDELTA - 11 - M_StringWidth(fps) 
                              - M_StringWidth(fps_str), yy, fps, cr[CR_LIGHTGRAY_DARK1]);

        M_WriteText(ORIGWIDTH + WIDESCREENDELTA - 7 - M_StringWidth(fps_str), yy, "FPS", cr[CR_LIGHTGRAY_DARK1]);

        yy += 9;
    }

    // [JN] Local time. Time gathered in G_Ticker.
    if (msg_local_time)
    {
        M_WriteText(ORIGWIDTH + WIDESCREENDELTA - 7
                              - M_StringWidth(ID_Local_Time), yy, ID_Local_Time, cr[CR_GRAY]);
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
    char  str[16];
    const player_t *player = &players[displayplayer];

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);

    if (widget_health == 1)  // Top
    {
        M_WriteTextCentered(18, str, ID_HealthColor(player->targetsheath,
                                                    player->targetsmaxheath));
    }
    else
    if (widget_health == 2)  // Top + name
    {
        M_WriteTextCentered(9, player->targetsname, ID_HealthColor(player->targetsheath,
                                                                   player->targetsmaxheath));
        M_WriteTextCentered(18, str, ID_HealthColor(player->targetsheath,
                                                    player->targetsmaxheath));
    }
    else
    if (widget_health == 3)  // Bottom
    {
        M_WriteTextCentered(152, str, ID_HealthColor(player->targetsheath,
                                                     player->targetsmaxheath));
    }
    else
    if (widget_health == 4)  // Bottom + name
    {
        M_WriteTextCentered(144, player->targetsname, ID_HealthColor(player->targetsheath,
                                                                     player->targetsmaxheath));
        M_WriteTextCentered(152, str, ID_HealthColor(player->targetsheath,
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
    0x03, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0x03, 0x01, 0x53, 0x53, 0x53, 0xFF, 
    0xFF, 0x00, 0x02, 0x5A, 0x5A, 0x53, 0x53, 0x05, 0x02, 0x53, 0x53, 0x5A, 
    0x5A, 0xFF, 0xFF, 0x03, 0x01, 0x53, 0x53, 0x53, 0xFF, 0x03, 0x01, 0x5A, 
    0x5A, 0x5A, 0xFF
};

static const byte xhair_cross2[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 
    0x34, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0x03, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0x02, 0x03, 0x5A, 0x5A, 
    0x53, 0x5A, 0x5A, 0xFF, 0x03, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0xFF, 0xFF
};

static const byte xhair_x[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0x3C, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 
    0xFF, 0x01, 0x01, 0x5A, 0x5A, 0x5A, 0x05, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 
    0x02, 0x01, 0x53, 0x53, 0x53, 0x04, 0x01, 0x53, 0x53, 0x53, 0xFF, 0xFF, 
    0x02, 0x01, 0x53, 0x53, 0x53, 0x04, 0x01, 0x53, 0x53, 0x53, 0xFF, 0x01, 
    0x01, 0x5A, 0x5A, 0x5A, 0x05, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0xFF
};

static const byte xhair_circle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 
    0x43, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 
    0xFF, 0x02, 0x03, 0x5A, 0x5A, 0x53, 0x5A, 0x5A, 0xFF, 0x01, 0x01, 0x5A, 
    0x5A, 0x5A, 0x05, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0x01, 0x01, 0x53, 0x53, 
    0x53, 0x05, 0x01, 0x53, 0x53, 0x53, 0xFF, 0x01, 0x01, 0x5A, 0x5A, 0x5A, 
    0x05, 0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0x02, 0x03, 0x5A, 0x5A, 0x53, 0x5A, 
    0x5A, 0xFF, 0xFF
};

static const byte xhair_angle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 
    0x2F, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0x03, 0x03, 0x53, 0x53, 0x5A, 0x60, 0x60, 0xFF, 0x03, 
    0x01, 0x5A, 0x5A, 0x5A, 0xFF, 0x03, 0x01, 0x60, 0x60, 0x60, 0xFF, 0xFF
};

static const byte xhair_triangle[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 
    0x3D, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x4A, 0x00, 0x00, 0x00, 
    0xFF, 0x05, 0x01, 0x60, 0x60, 0x60, 0xFF, 0x04, 0x02, 0x5A, 0x5A, 0x5A, 
    0x5A, 0xFF, 0x03, 0x01, 0x53, 0x53, 0x53, 0x05, 0x01, 0x53, 0x53, 0x53, 
    0xFF, 0x04, 0x02, 0x5A, 0x5A, 0x5A, 0x5A, 0xFF, 0x05, 0x01, 0x60, 0x60, 
    0x60, 0xFF, 0xFF
};

static const byte xhair_dot[] =
{
    0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 
    0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 
    0x2D, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0x03, 0x01, 0x53, 0x53, 0x53, 0xFF, 0xFF, 0xFF, 0xFF
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
    const int yy = (ORIGHEIGHT / 2) - 3 - (dp_screen_size <= 10 ? 16 : 0);

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

    M_WriteText(x + WIDESCREENDELTA, 9, n, cr[CR_LIGHTGRAY]);
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
    0,    //  6 - Chainsaw
    0,    //  7 - Shotgun
    0,    //  8 - Super Shotgun
    0,    //  9 - Chaingun
    0,    // 10 - Rocket Launcher
    0,    // 11 - Plasma Rifle
    0,    // 12 - BFG 9000
    0,    // 13 - Backpack
    50,   // 14 - Bullets
    0,    // 15 - Shells
    0,    // 16 - Rockets
    0,    // 17 - Cells
    0,    // 18 - Blue keycard
    0,    // 19 - Yellow keycard
    0,    // 20 - Red keycard
    0,    // 21 - Blue skull key
    0,    // 22 - Yellow skull key
    0,    // 23 - Red skull key
    0,    // 24 - Fast monsters
    0,    // 25 - Respawning monsters
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
