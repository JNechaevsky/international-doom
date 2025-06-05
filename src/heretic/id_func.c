//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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

// [JN] Enum for widget strings and values.
enum
{
    widget_kis_str,
    widget_kills,
    widget_items,
    widget_secret,
    widget_plyr1,
    widget_plyr2,
    widget_plyr3,
    widget_plyr4,
    widget_time_str,
    widget_time_val,
    widget_render_str,
    widget_render_val,
    widget_coords_str,
    widget_coords_val,
} widgetcolor_t;

static byte *ID_WidgetColor (const int i)
{
    if (!widget_scheme)
    {
        return NULL;
    }

    static byte *player_colors[4];
    static int   plyr_indices[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};

    player_colors[0] = cr[CR_GREEN];
    player_colors[1] = cr[CR_YELLOW];
    player_colors[2] = cr[CR_RED];
    player_colors[3] = cr[CR_BLUE2];

    switch (widget_scheme)
    {
        case 1: // Inter
            switch (i)
            {
                case widget_kis_str:
                case widget_time_str:
                case widget_render_str:
                case widget_coords_str:
                    return cr[CR_GRAY];
                
                case widget_kills:
                    return
                        IDWidget.totalkills == 0 ? cr[CR_GREEN] :
                        IDWidget.kills == 0 ? cr[CR_RED] :
                        IDWidget.kills < IDWidget.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN];
                case widget_items:
                    return
                        IDWidget.totalitems == 0 ? cr[CR_GREEN] :
                        IDWidget.items == 0 ? cr[CR_RED] :
                        IDWidget.items < IDWidget.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN];
                case widget_secret:
                    return
                        IDWidget.totalsecrets == 0 ? cr[CR_GREEN] :
                        IDWidget.secrets == 0 ? cr[CR_RED] :
                        IDWidget.secrets < IDWidget.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN];

                case widget_time_val:
                    return cr[CR_LIGHTGRAY];

                case widget_render_val:
                case widget_coords_val:
                    return cr[CR_GREEN];

                default:
                    for (int j = 0; j < 4; j++)
                    {
                        if (i == plyr_indices[j])
                        {
                            return player_colors[j];
                        }
                    }
            }
        break;

        case 2: // Crispy
            switch (i)
            {
                case widget_kis_str:
                case widget_time_str:
                    return cr[CR_RED];
                case widget_render_str:
                    return cr[CR_YELLOW];
                case widget_coords_str:
                    return cr[CR_GREEN];

                default:
                    for (int j = 0; j < 4; j++)
                    {
                        if (i == plyr_indices[j])
                        {
                            return player_colors[j];
                        }
                    }
                    return cr[CR_WHITE];
            }
        break;

        case 3: // Woof
            switch (i)
            {
                case widget_kis_str:
                case widget_time_str:
                    return cr[CR_RED];

                case widget_kills:
                    return
                        IDWidget.totalkills == 0 ? cr[CR_BLUE2] :
                        IDWidget.kills < IDWidget.totalkills ? cr[CR_WHITE] : cr[CR_BLUE2];
                
                case widget_items:
                    return
                        IDWidget.totalitems == 0 ? cr[CR_BLUE2] :
                        IDWidget.items < IDWidget.totalitems ? cr[CR_WHITE] : cr[CR_BLUE2];
                
                case widget_secret:
                    return
                        IDWidget.totalsecrets == 0 ? cr[CR_BLUE2] :
                        IDWidget.secrets < IDWidget.totalsecrets ? cr[CR_WHITE] : cr[CR_BLUE2];

                case widget_render_str:
                case widget_render_val:
                case widget_coords_str:
                    return cr[CR_GREEN];

                default:
                    for (int j = 0; j < 4; j++)
                    {
                        if (i == plyr_indices[j])
                        {
                            return player_colors[j];
                        }
                    }
                    return cr[CR_WHITE];
            }
        break;

        case 4: // DSDA
            switch (i)
            {
                case widget_kis_str:
                    return cr[CR_RED];

                case widget_kills:
                    return
                        IDWidget.totalkills == 0 ? cr[CR_BLUE2] :
                        IDWidget.kills < IDWidget.totalkills ? cr[CR_YELLOW] : cr[CR_BLUE2];
                
                case widget_items:
                    return
                        IDWidget.totalitems == 0 ? cr[CR_BLUE2] :
                        IDWidget.items < IDWidget.totalitems ? cr[CR_YELLOW] : cr[CR_BLUE2];
                
                case widget_secret:
                    return
                        IDWidget.totalsecrets == 0 ? cr[CR_BLUE2] :
                        IDWidget.secrets < IDWidget.totalsecrets ? cr[CR_YELLOW] : cr[CR_BLUE2];

                case widget_time_str:
                case widget_render_str:
                    return cr[CR_WHITE];

                case widget_render_val:
                    return cr[CR_YELLOW];

                case widget_time_val:
                case widget_coords_str:
                case widget_coords_val:
                    return cr[CR_GREEN];

                default:
                    for (int j = 0; j < 4; j++)
                    {
                        if (i == plyr_indices[j])
                        {
                            return player_colors[j];
                        }
                    }
                    return cr[CR_WHITE];
            }
        break;
    }
    return NULL;
}

// [JN/PN] Enum for widget type values.
enum
{
    widget_kis_kills,
    widget_kis_items,
    widget_kis_secrets,
} widget_kis_count_t;

// [PN] Function for safe division to prevent division by zero.
// Returns the percentage or 0 if the total is zero.
static int safe_percent (int value, int total)
{
    return (total == 0) ? 0 : (value * 100) / total;
}

// [PN/JN] Main function to format KIS counts based on format and widget type.
static void ID_WidgetKISCount (char *buffer, size_t buffer_size, const int i)
{
    int value = 0, total = 0;
    
    // [PN] Set values for kills, items, or secrets based on widget type
    switch (i)
    {
        case widget_kis_kills:
            value = IDWidget.kills;
            total = IDWidget.totalkills;
            break;
        
        case widget_kis_items:
            value = IDWidget.items;
            total = IDWidget.totalitems;
            break;
        
        case widget_kis_secrets:
            value = IDWidget.secrets;
            total = IDWidget.totalsecrets;
            break;
        
        default:
            // [PN] Default case for unsupported widget type
            snprintf(buffer, buffer_size, "N/A");
            return;
    }

    // [PN] Format based on widget_kis_format
    switch (widget_kis_format)
    {
        case 1: // Remaining
            snprintf(buffer, buffer_size, "%d ", total - value);
            break;

        case 2: // Percent
            snprintf(buffer, buffer_size, "%d%% ", safe_percent(value, total));
            break;

        default: // Ratio
            snprintf(buffer, buffer_size, "%d/%d ", value, total);
            break;
    }
}

// -----------------------------------------------------------------------------
// ID_LeftWidgets.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void ID_LeftWidgets (void)
{
    int left_align;

    if (dp_screen_size > 10 && (!automapactive || automap_overlay))
    {
        left_align = (widget_alignment ==  0) ? -WIDESCREENDELTA :     // left
                     (widget_alignment ==  1) ? 20 :                   // status bar
                     (dp_screen_size   >= 12) ? -WIDESCREENDELTA : 0;  // auto
    }
    else
    {
        left_align = (widget_alignment ==  0) ? -WIDESCREENDELTA : 20;
    }

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
                MN_DrTextA("K:", left_align, 10, ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str1, sizeof(str1), widget_kis_kills);
                MN_DrTextA(str1, left_align + 16, 10, ID_WidgetColor(widget_kills));

                // Items:
                MN_DrTextA("I:", left_align, 20, ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str2, sizeof(str2), widget_kis_items);
                MN_DrTextA(str2, left_align + 16, 20, ID_WidgetColor(widget_items));

                // Secret:
                MN_DrTextA("S:", left_align, 30, ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str3, sizeof(str3), widget_kis_secrets);
                MN_DrTextA(str3, left_align + 16, 30, ID_WidgetColor(widget_secret));
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
                    MN_DrTextA("G:", left_align, 10, ID_WidgetColor(widget_plyr1));
                    sprintf(str1, "%d", IDWidget.frags_g);
                    MN_DrTextA(str1, left_align + 16, 10, ID_WidgetColor(widget_plyr1));
                }
                // Yellow
                if (playeringame[1])
                {
                    MN_DrTextA("Y:", left_align, 20, ID_WidgetColor(widget_plyr2));
                    sprintf(str2, "%d", IDWidget.frags_y);
                    MN_DrTextA(str2, left_align + 16, 20, ID_WidgetColor(widget_plyr2));
                }
                // Red
                if (playeringame[2])
                {
                    MN_DrTextA("R:", left_align, 30, ID_WidgetColor(widget_plyr3));
                    sprintf(str3, "%d", IDWidget.frags_r);
                    MN_DrTextA(str3, left_align + 16, 30, ID_WidgetColor(widget_plyr3));
                }
                // Blue
                if (playeringame[3])
                {
                    MN_DrTextA("B:", left_align, 40, ID_WidgetColor(widget_plyr3));
                    sprintf(str4, "%d", IDWidget.frags_b);
                    MN_DrTextA(str4, left_align + 16, 40, ID_WidgetColor(widget_plyr3));
                }
            }
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            MN_DrTextA("TIME", left_align, 40, ID_WidgetColor(widget_time_str));
            MN_DrTextA(ID_Level_Time, left_align, 50, ID_WidgetColor(widget_time_val));
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            MN_DrTextA("TOTAL", left_align, 60, ID_WidgetColor(widget_time_str));
            MN_DrTextA(ID_Total_Time, left_align, 70, ID_WidgetColor(widget_time_val));
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];
            
            MN_DrTextA("X:", left_align, 80, ID_WidgetColor(widget_coords_str));
            MN_DrTextA("Y:", left_align, 90, ID_WidgetColor(widget_coords_str));
            MN_DrTextA("ANG:", left_align, 100, ID_WidgetColor(widget_coords_str));

            sprintf(str, "%d", IDWidget.x);
            MN_DrTextA(str, left_align + 16, 80, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.y);
            MN_DrTextA(str, left_align + 16, 90, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.ang);
            MN_DrTextA(str, left_align + 32, 100, ID_WidgetColor(widget_coords_val));
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];

            // Sprites
            MN_DrTextA("SPR:", left_align, 110, ID_WidgetColor(widget_render_str));
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            MN_DrTextA(spr, 32 + left_align, 110, ID_WidgetColor(widget_render_val));

            // Segments
            MN_DrTextA("SEG:", left_align, 120, ID_WidgetColor(widget_render_str));
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            MN_DrTextA(seg, 32 + left_align, 120, ID_WidgetColor(widget_render_val));

            // Openings
            MN_DrTextA("OPN:", left_align, 130, ID_WidgetColor(widget_render_str));
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            MN_DrTextA(opn, 32 + left_align, 130, ID_WidgetColor(widget_render_val));

            // Planes
            MN_DrTextA("PLN:", left_align, 140, ID_WidgetColor(widget_render_str));
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            MN_DrTextA(vis, 32 + left_align, 140, ID_WidgetColor(widget_render_val));
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
            MN_DrTextA("SPR:", left_align, 26 + yy1, ID_WidgetColor(widget_render_str));
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            MN_DrTextA(spr, 32 + left_align, 26 + yy1, ID_WidgetColor(widget_render_val));

            // Segments
            MN_DrTextA("SEG:", left_align, 36 + yy1, ID_WidgetColor(widget_render_str));
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            MN_DrTextA(seg, 32 + left_align, 36 + yy1, ID_WidgetColor(widget_render_val));

            // Openings
            MN_DrTextA("OPN:", left_align, 46 + yy1, ID_WidgetColor(widget_render_str));
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            MN_DrTextA(opn, 32 + left_align, 46 + yy1, ID_WidgetColor(widget_render_val));

            // Planes
            MN_DrTextA("PLN:", left_align, 56 + yy1, ID_WidgetColor(widget_render_str));
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            MN_DrTextA(vis, 32 + left_align, 56 + yy1, ID_WidgetColor(widget_render_val));
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            MN_DrTextA("X:", left_align, 76, ID_WidgetColor(widget_coords_str));
            MN_DrTextA("Y:", left_align, 86, ID_WidgetColor(widget_coords_str));
            MN_DrTextA("ANG:", left_align, 96, ID_WidgetColor(widget_coords_str));

            sprintf(str, "%d", IDWidget.x);
            MN_DrTextA(str, 16 + left_align, 76, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.y);
            MN_DrTextA(str, 16 + left_align, 86, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.ang);
            MN_DrTextA(str, 32 + left_align, 96, ID_WidgetColor(widget_coords_val));
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
                MN_DrTextA(str1, left_align, 146 + yy, ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str2, sizeof(str2), widget_kis_kills);
                MN_DrTextA(str2, left_align + MN_TextAWidth(str1), 146 + yy, ID_WidgetColor(widget_kills));

                // Items:
                sprintf(str3, "I ");
                MN_DrTextA(str3, left_align + MN_TextAWidth(str1) + 
                                     MN_TextAWidth(str2), 146 + yy, ID_WidgetColor(widget_kis_str));

                ID_WidgetKISCount(str4, sizeof(str4), widget_kis_items);
                MN_DrTextA(str4, left_align    +
                           MN_TextAWidth(str1) +
                           MN_TextAWidth(str2) +
                           MN_TextAWidth(str3), 146 + yy, ID_WidgetColor(widget_items));

                // Secret:
                sprintf(str5, "S ");
                MN_DrTextA(str5, left_align    +
                           MN_TextAWidth(str1) +
                           MN_TextAWidth(str2) +
                           MN_TextAWidth(str3) +
                           MN_TextAWidth(str4), 146 + yy, ID_WidgetColor(widget_kis_str));

                ID_WidgetKISCount(str6, sizeof(str6), widget_kis_secrets);
                MN_DrTextA(str6, left_align    +
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
                    MN_DrTextA(str1, left_align, 146 + yy, ID_WidgetColor(widget_plyr1));

                    sprintf(str2, "%d ", IDWidget.frags_g);
                    MN_DrTextA(str2, left_align +
                                     MN_TextAWidth(str1), 146 + yy, ID_WidgetColor(widget_plyr2));
                }
                // Yellow
                if (playeringame[1])
                {
                    sprintf(str3, "Y ");
                    MN_DrTextA(str3, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2),
                                     146 + yy, ID_WidgetColor(widget_plyr2));

                    sprintf(str4, "%d ", IDWidget.frags_y);
                    MN_DrTextA(str4, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3),
                                     146 + yy, ID_WidgetColor(widget_plyr2));
                }
                // Red
                if (playeringame[2])
                {
                    sprintf(str5, "R ");
                    MN_DrTextA(str5, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4),
                                     146 + yy, ID_WidgetColor(widget_plyr3));

                    sprintf(str6, "%d ", IDWidget.frags_r);
                    MN_DrTextA(str6, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4) +
                                     MN_TextAWidth(str5),
                                     146 + yy, ID_WidgetColor(widget_plyr3));
                }
                // Blue
                if (playeringame[3])
                {
                    sprintf(str7, "B ");
                    MN_DrTextA(str7, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4) +
                                     MN_TextAWidth(str5) +
                                     MN_TextAWidth(str6),
                                     146 + yy, ID_WidgetColor(widget_plyr4));

                    sprintf(str8, "%d ", IDWidget.frags_b);
                    MN_DrTextA(str8, left_align          +
                                     MN_TextAWidth(str1) +
                                     MN_TextAWidth(str2) +
                                     MN_TextAWidth(str3) +
                                     MN_TextAWidth(str4) +
                                     MN_TextAWidth(str5) +
                                     MN_TextAWidth(str6) +
                                     MN_TextAWidth(str7),
                                     146 + yy, ID_WidgetColor(widget_plyr4));
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
            MN_DrTextA(stra, left_align, 146 + yy, ID_WidgetColor(widget_time_str));
            MN_DrTextA(ID_Total_Time, left_align + MN_TextAWidth(stra), 146 + yy, ID_WidgetColor(widget_time_val));
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
            MN_DrTextA(stra, left_align, 146 + yy, ID_WidgetColor(widget_time_str));
            MN_DrTextA(ID_Level_Time, left_align + MN_TextAWidth(stra), 146 + yy, ID_WidgetColor(widget_time_val));
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
    int yy = 10;

    // [JN] If demo timer is active and running,
    // shift FPS and time widgets one line down.
    if ((demoplayback && (demo_timer == 1 || demo_timer == 3))
    ||  (demorecording && (demo_timer == 2 || demo_timer == 3)))
    {
        yy += 10;
    }

    // [JN/PN] FPS counter
    if (vid_showfps)
    {
        int  fps_x_pos;
        char fps[8];

        sprintf(fps, "%d", id_fps_value);
        fps_x_pos = ORIGWIDTH + WIDESCREENDELTA - 11 
                  - MN_TextAWidth(fps) - MN_TextAWidth("FPS");

        MN_DrTextA(fps, fps_x_pos, yy, cr[CR_LIGHTGRAY_DARK]);
        MN_DrTextA("FPS", fps_x_pos + MN_TextAWidth(fps) + 4, yy, cr[CR_LIGHTGRAY_DARK]); // [PN] 4 for spacing

        yy += 10;
    }

    // [JN] Local time. Time gathered in G_Ticker.
    if (msg_local_time)
    {
        MN_DrTextA(ID_Local_Time, ORIGWIDTH + WIDESCREENDELTA - 7
                              - MN_TextAWidth(ID_Local_Time), yy, cr[CR_GRAY]);
    }
}

// -----------------------------------------------------------------------------
// ID_HealthColor, ID_DrawTargetsHealth
//  [JN/PN] Indicates and colorizes current target's health.
// -----------------------------------------------------------------------------

static byte *ID_HealthColor (const int val1, const int val2)
{
    return
        !widget_scheme ? NULL          :
        val1 <= val2/4 ? cr[CR_RED]    :
        val1 <= val2/2 ? cr[CR_YELLOW] :
                         cr[CR_GREEN]  ;
}

void ID_DrawTargetsHealth (void)
{
    const player_t *player = &players[displayplayer];

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    char str[16];
    snprintf(str, sizeof(str), "%d/%d", player->targetsheath, player->targetsmaxheath);
    byte *color = ID_HealthColor(player->targetsheath, player->targetsmaxheath);

    switch (widget_health)
    {
        case 1:  // Top
            MN_DrTextACentered(str, 10, color);
            break;
        case 2:  // Top + name
            MN_DrTextACentered(player->targetsname, 10, color);
            MN_DrTextACentered(str, 20, color);
            break;
        case 3:  // Bottom
            MN_DrTextACentered(str, 145, color);
            break;
        case 4:  // Bottom + name
            MN_DrTextACentered(player->targetsname, 145, color);
            MN_DrTextACentered(str, 135, color);
            break;
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
    // [PN] Array of crosshair shapes with explicit type casting
    patch_t *crosshair_shapes[] = {
        NULL,                        // xhair_draw == 0 (no crosshair)
        (patch_t*) &xhair_cross1,    // xhair_draw == 1
        (patch_t*) &xhair_cross2,    // xhair_draw == 2
        (patch_t*) &xhair_x,         // xhair_draw == 3
        (patch_t*) &xhair_circle,    // xhair_draw == 4
        (patch_t*) &xhair_angle,     // xhair_draw == 5
        (patch_t*) &xhair_triangle,  // xhair_draw == 6
        (patch_t*) &xhair_dot,       // xhair_draw == 7
    };

    // [PN] Return the appropriate crosshair shape
    return crosshair_shapes[xhair_draw];
}

// -----------------------------------------------------------------------------
// ID_CrosshairColor
//  [JN/PN] Determines crosshair color depending on "xhair_color" variable.
//  Supports static, health-based, target highlight, and combined modes.
// -----------------------------------------------------------------------------

static byte *ID_CrosshairColor (int type)
{
    const player_t *player = &players[displayplayer];

    switch (type)
    {
        case 0:
            // Static/uncolored.
            return cr[CR_RED];

        case 1:
            // Health-based coloring.
            // Same logic as status bar coloring (ST_WidgetColor).
            return player->health >= 67 ? cr[CR_GREEN]  :
                   player->health >= 34 ? cr[CR_YELLOW] :
                                          cr[CR_RED];

        case 2:
            // Target highlight.
            // "linetarget" is set by intercept-safe P_AimLineAttack in G_Ticker.
            return linetarget ? cr[CR_BLUE2] : cr[CR_RED];

        case 3:
            // Target highlight+health.
            return linetarget           ? cr[CR_BLUE2]  :
                   player->health >= 67 ? cr[CR_GREEN]  :
                   player->health >= 34 ? cr[CR_YELLOW] :
                                          cr[CR_RED];
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
//  [PN/JN] Reduced update frequency to once per gametic from every frame.
// -----------------------------------------------------------------------------

void ID_DemoTimer (const int time)
{
    static char n[16];
    static int  last_update_gametic = -1;
    static int  hours = 0;

    if (last_update_gametic < gametic)
    {
        hours = time / (3600 * TICRATE);
        const int mins = time / (60 * TICRATE) % 60;
        const float secs = (float)(time % (60 * TICRATE)) / TICRATE;

        if (hours)
        {
            M_snprintf(n, sizeof(n), "%02i:%02i:%05.02f", hours, mins, secs);
        }
        else
        {
            M_snprintf(n, sizeof(n), "%02i:%05.02f", mins, secs);
        }

        last_update_gametic = gametic;
    }

    const int x = 237 + (hours > 0 ? 0 : 20);
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

    // [JN/PN] Don't rely on palette indexes,
    // try to find nearest colors instead.
    if (!colors_set)
    {
        black = I_MapRGB(0, 0, 0);
        white = I_MapRGB(255, 255, 255);
        colors_set = true;
    }

    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, black); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, white); // [crispy] white
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
    // [JN/PN] Precalculate values:
    const fixed_t fwm_val = fwm * 32768;
    const fixed_t swm_val = swm * 32768;

    // Rotate camera first
    CRL_camera_ang += at << FRACBITS;

    // Forward movement
    at = CRL_camera_ang >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(fwm_val, finecosine[at]); 
    CRL_camera_y += FixedMul(fwm_val, finesine[at]);

    // Sideways movement
    at = (CRL_camera_ang - ANG90) >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(swm_val, finecosine[at]); 
    CRL_camera_y += FixedMul(swm_val, finesine[at]);
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCameraVert
//  [JN/PN] Impulses the camera up/down.
//  @param direction: true = up, false = down.
//  @param intensity: 32 of 64 map unit, depending on player run mode.
// -----------------------------------------------------------------------------

void CRL_ImpulseCameraVert (boolean direction, fixed_t intensity)
{
    CRL_camera_z += (direction ? 1 : -1) * FRACUNIT * intensity;
}
