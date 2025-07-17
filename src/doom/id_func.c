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
    widget_speed_str,
    widget_speed_val,
} widgetcolor_t;

static byte *ID_WidgetColor (const int i)
{
    if (!widget_scheme)
    {
        return NULL;
    }

    static byte *player_colors[4];
    static const int plyr_indices[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};

    player_colors[0] = cr[CR_GREEN];
    player_colors[1] = cr[CR_GRAY];
    player_colors[2] = cr[CR_BROWN];
    player_colors[3] = cr[CR_RED];

    switch (widget_scheme)
    {
        case 1: // Inter
            switch (i)
            {
                case widget_kis_str:
                case widget_time_str:
                case widget_render_str:
                case widget_coords_str:
                case widget_speed_str:
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
                case widget_speed_val:
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
                case widget_speed_str:
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
                case widget_speed_str:
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
                case widget_speed_val:
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
    widgets_kis_kills,
    widgets_kis_items,
    widgets_kis_secrets,
} widgets_kis_count_t;

// [PN] Function for safe division to prevent division by zero.
// Returns the percentage or 0 if the total is zero.
static int safe_percent (int value, int total)
{
    return (total == 0) ? 0 : (value * 100) / total;
}

// [PN/JN] Main function to format KIS counts based on format and widget type.
static void ID_WidgetKISCount (char *buffer, size_t buffer_size, const int i)
{
    int value = 0, extra_value = 0, total = 0;
    
    // [PN] Set values for kills, items, or secrets based on widget type
    switch (i)
    {
        case widgets_kis_kills:
            value = IDWidget.kills;
            extra_value = IDWidget.extrakills;
            total = IDWidget.totalkills;
            break;
        
        case widgets_kis_items:
            value = IDWidget.items;
            total = IDWidget.totalitems;
            break;
        
        case widgets_kis_secrets:
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
            snprintf(buffer, buffer_size, "%d", total - value);
            break;

        case 2: // Percent
            snprintf(buffer, buffer_size, "%d%%", 
                     safe_percent(value, total) + safe_percent(extra_value, total));
            break;

        default: // Ratio
            if (extra_value > 0)
            {
                snprintf(buffer, buffer_size, "%d+%d/%d", value, extra_value, total);
            }
            else
            {
                snprintf(buffer, buffer_size, "%d/%d", value, total);
            }
            break;
    }
}

// -----------------------------------------------------------------------------
// ID_LeftWidgets.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void ID_LeftWidgets (void)
{
    const int left_align = (widget_alignment == 0) ? -WIDESCREENDELTA :      // left
                           (widget_alignment == 1) ? 0                :      // status bar
                           (dp_screen_size   > 12  ? -WIDESCREENDELTA : 0);  // auto

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
                int yy = 0;
                char str1[16];  // kills
                char str2[16];  // items
                char str3[16];  // secret

                // Kills:
                M_WriteText(left_align, 9, "K:", ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str1, sizeof(str1), widgets_kis_kills);
                M_WriteText(left_align + 16, 9, str1, ID_WidgetColor(widget_kills));

                // Items:
                if (widget_kis_items)
                {
                M_WriteText(left_align, 18, "I:", ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str2, sizeof(str2), widgets_kis_items);
                M_WriteText(left_align + 16, 18, str2, ID_WidgetColor(widget_items));
                }
                else
                {
                str2[0] = '\0';
                yy = 9;
                }

                // Secret:
                M_WriteText(left_align, 27 - yy, "S:", ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str3, sizeof(str3), widgets_kis_secrets);
                M_WriteText(left_align + 16, 27 - yy, str3, ID_WidgetColor(widget_secret));
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
                    M_WriteText(left_align, 9, "G:", ID_WidgetColor(widget_plyr1));
                    sprintf(str1, "%d", IDWidget.frags_g);
                    M_WriteText(left_align + 16, 9, str1, ID_WidgetColor(widget_plyr1));
                }
                // Indigo
                if (playeringame[1])
                {
                    M_WriteText(left_align, 18, "I:", ID_WidgetColor(widget_plyr2));
                    sprintf(str2, "%d", IDWidget.frags_i);
                    M_WriteText(left_align + 16, 18, str2, ID_WidgetColor(widget_plyr2));
                }
                // Brown
                if (playeringame[2])
                {
                    M_WriteText(left_align, 27, "B:", ID_WidgetColor(widget_plyr3));
                    sprintf(str3, "%d", IDWidget.frags_b);
                    M_WriteText(left_align + 16, 27, str3, ID_WidgetColor(widget_plyr3));
                }
                // Red
                if (playeringame[3])
                {
                    M_WriteText(left_align, 36, "B:", ID_WidgetColor(widget_plyr4));
                    sprintf(str4, "%d", IDWidget.frags_r);
                    M_WriteText(left_align + 16, 36, str4, ID_WidgetColor(widget_plyr4));
                }
            }
        }

        // Level / DeathMatch timer. Time gathered in G_Ticker.
        if (widget_time == 1
        || (widget_time == 2 && automapactive))
        {
            M_WriteText(left_align, 45, "TIME", ID_WidgetColor(widget_time_str));
            M_WriteText(left_align, 54, ID_Level_Time, ID_WidgetColor(widget_time_val));
        }

        // Total time. Time gathered in G_Ticker.
        if (widget_totaltime == 1
        || (widget_totaltime == 2 && automapactive))
        {
            M_WriteText(left_align, 63, "TOTAL", ID_WidgetColor(widget_time_str));
            M_WriteText(left_align, 72, ID_Total_Time, ID_WidgetColor(widget_time_val));
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            M_WriteText(left_align, 90, "X:", ID_WidgetColor(widget_coords_str));
            M_WriteText(left_align, 99, "Y:", ID_WidgetColor(widget_coords_str));
            M_WriteText(left_align, 108, "ANG:", ID_WidgetColor(widget_coords_str));

            sprintf(str, "%d", IDWidget.x);
            M_WriteText(left_align + 16, 90, str, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.y);
            M_WriteText(left_align + 16, 99, str, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.ang);
            M_WriteText(left_align + 32, 108, str, ID_WidgetColor(widget_coords_val));
        }

        // Render counters
        if (widget_render)
        {
            char spr[32];
            char seg[32];
            char opn[64];
            char vis[32];

            // Sprites
            M_WriteText(left_align, 124, "SPR:", ID_WidgetColor(widget_render_str));
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            M_WriteText(32 + left_align, 124, spr, ID_WidgetColor(widget_render_val));

            // Segments (256 max)
            M_WriteText(left_align, 133, "SEG:", ID_WidgetColor(widget_render_str));
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            M_WriteText(32 + left_align, 133, seg, ID_WidgetColor(widget_render_val));

            // Openings
            M_WriteText(left_align, 142, "OPN:", ID_WidgetColor(widget_render_str));
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            M_WriteText(32 + left_align, 142, opn, ID_WidgetColor(widget_render_val));

            // Planes
            M_WriteText(left_align, 151, "PLN:", ID_WidgetColor(widget_render_str));
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            M_WriteText(32 + left_align, 151, vis, ID_WidgetColor(widget_render_val));
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
            M_WriteText(left_align, 54 + yy1, "SPR:", ID_WidgetColor(widget_render_str));
            M_snprintf(spr, 16, "%d", IDRender.numsprites);
            M_WriteText(32 + left_align, 54 + yy1, spr, ID_WidgetColor(widget_render_val));

            // Segments (256 max)
            M_WriteText(left_align, 63 + yy1, "SEG:", ID_WidgetColor(widget_render_str));
            M_snprintf(seg, 16, "%d", IDRender.numsegs);
            M_WriteText(32 + left_align, 63 + yy1, seg, ID_WidgetColor(widget_render_val));

            // Openings
            M_WriteText(left_align, 72 + yy1, "OPN:", ID_WidgetColor(widget_render_str));
            M_snprintf(opn, 16, "%d", IDRender.numopenings);
            M_WriteText(32 + left_align, 72 + yy1, opn, ID_WidgetColor(widget_render_val));

            // Planes
            M_WriteText(left_align, 81 + yy1, "PLN:", ID_WidgetColor(widget_render_str));
            M_snprintf(vis, 32, "%d", IDRender.numplanes);
            M_WriteText(32 + left_align, 81 + yy1, vis, ID_WidgetColor(widget_render_val));
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            char str[128];

            M_WriteText(left_align, 97, "X:", ID_WidgetColor(widget_coords_str));
            M_WriteText(left_align, 106, "Y:", ID_WidgetColor(widget_coords_str));
            M_WriteText(left_align, 115, "ANG:", ID_WidgetColor(widget_coords_str));

            sprintf(str, "%d", IDWidget.x);
            M_WriteText(16 + left_align, 97, str, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.y);
            M_WriteText(16 + left_align, 106, str, ID_WidgetColor(widget_coords_val));
            sprintf(str, "%d", IDWidget.ang);
            M_WriteText(32 + left_align, 115, str, ID_WidgetColor(widget_coords_val));
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
                M_WriteText(left_align, 160 + yy, str1, ID_WidgetColor(widget_kis_str));
                ID_WidgetKISCount(str2, sizeof(str2), widgets_kis_kills);
                M_WriteText(left_align + M_StringWidth(str1), 160 + yy, str2, ID_WidgetColor(widget_kills));
        
                // Items:
                if (widget_kis_items)
                {
                sprintf(str3, " I ");
                M_WriteText(left_align + M_StringWidth(str1) +
                            M_StringWidth(str2), 160 + yy, str3, ID_WidgetColor(widget_kis_str));
                
                ID_WidgetKISCount(str4, sizeof(str4), widgets_kis_items);
                M_WriteText(left_align + M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3), 160 + yy, str4, ID_WidgetColor(widget_items));
                }
                else
                {
                str3[0] = '\0';
                str4[0] = '\0';
                }
        
                // Secret:
                sprintf(str5, " S ");
                M_WriteText(left_align + M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4), 160 + yy, str5, ID_WidgetColor(widget_kis_str));
        
                ID_WidgetKISCount(str6, sizeof(str6), widgets_kis_secrets);
                M_WriteText(left_align + M_StringWidth(str1) +
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
                    M_WriteText(left_align, 160 + yy, str1, ID_WidgetColor(widget_plyr1));

                    sprintf(str2, "%d ", IDWidget.frags_g);
                    M_WriteText(left_align +
                                M_StringWidth(str1), 160 + yy, str2, ID_WidgetColor(widget_plyr1));
                }
                // Indigo
                if (playeringame[1])
                {
                    sprintf(str3, "I ");
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2),
                                160 + yy, str3, ID_WidgetColor(widget_plyr2));

                    sprintf(str4, "%d ", IDWidget.frags_i);
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3),
                                160 + yy, str4, ID_WidgetColor(widget_plyr2));
                }
                // Brown
                if (playeringame[2])
                {
                    sprintf(str5, "B ");
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4),
                                160 + yy, str5, ID_WidgetColor(widget_plyr3));

                    sprintf(str6, "%d ", IDWidget.frags_b);
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5),
                                160 + yy, str6, ID_WidgetColor(widget_plyr3));
                }
                // Red
                if (playeringame[3])
                {
                    sprintf(str7, "R ");
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5) +
                                M_StringWidth(str6),
                                160 + yy, str7, ID_WidgetColor(widget_plyr4));

                    sprintf(str8, "%d ", IDWidget.frags_r);
                    M_WriteText(left_align +
                                M_StringWidth(str1) +
                                M_StringWidth(str2) +
                                M_StringWidth(str3) +
                                M_StringWidth(str4) +
                                M_StringWidth(str5) +
                                M_StringWidth(str6) +
                                M_StringWidth(str7),
                                160 + yy, str8, ID_WidgetColor(widget_plyr4));
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
            M_WriteText(left_align, 160 + yy, stra, ID_WidgetColor(widget_time_str));
            M_WriteText(left_align + M_StringWidth(stra), 160 + yy, ID_Total_Time, ID_WidgetColor(widget_time_val));
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
            M_WriteText(left_align, 160 + yy, stra, ID_WidgetColor(widget_time_str));
            M_WriteText(left_align + M_StringWidth(stra), 160 + yy, ID_Level_Time, ID_WidgetColor(widget_time_val));
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

    // [JN/PN] FPS counter
    if (vid_showfps)
    {
        int  fps_x_pos;
        char fps[8];

        sprintf(fps, "%d", id_fps_value);
        fps_x_pos = ORIGWIDTH + WIDESCREENDELTA - 11 
                  - M_StringWidth(fps) - M_StringWidth("FPS");

        M_WriteText(fps_x_pos, yy, fps, cr[CR_LIGHTGRAY_DARK]);
        M_WriteText(fps_x_pos + M_StringWidth(fps) + 4, yy, "FPS", cr[CR_LIGHTGRAY_DARK]); // [PN] 4 for spacing

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

    const int yy = widget_speed ? 9 : 0;
    char  str[16];
    snprintf(str, sizeof(str), "%d/%d", player->targetsheath, player->targetsmaxheath);
    byte *color = ID_HealthColor(player->targetsheath, player->targetsmaxheath);

    switch (widget_health)
    {
        case 1:  // Top
            M_WriteTextCentered(18, str, color);
            break;
        case 2:  // Top + name
            M_WriteTextCentered(9, player->targetsname, color);
            M_WriteTextCentered(18, str, color);
            break;
        case 3:  // Bottom
            M_WriteTextCentered(152 - yy, str, color);
            break;
        case 4:  // Bottom + name
            M_WriteTextCentered(144 - yy, player->targetsname, color);
            M_WriteTextCentered(152 - yy, str, color);
            break;
    }
}

// -----------------------------------------------------------------------------
// ID_DrawPlayerSpeed
//  [PN/JN] Draws player movement speed in map untits per second format.
//  Based on the implementation by ceski from the Woof source port.
// -----------------------------------------------------------------------------

void ID_DrawPlayerSpeed (void)
{
    static char str[8];
    static char val[16];
    static double speed = 0;
    const player_t *player = &players[displayplayer];

    // Calculate speed only every game tic, not every frame.
    if (oldgametic < gametic)
    {
        const double dx = (double)(player->mo->x - player->mo->oldx) / FRACUNIT;
        const double dy = (double)(player->mo->y - player->mo->oldy) / FRACUNIT;
        const double dz = (double)(player->mo->z - player->mo->oldz) / FRACUNIT;
        speed = sqrt(dx * dx + dy * dy + dz * dz) * TICRATE;
    }

    M_snprintf(str, sizeof(str), "SPD:");
    M_snprintf(val, sizeof(val), " %.0f", speed);

    const int x_val = (ORIGWIDTH / 2);
    const int x_str = x_val - M_StringWidth(str);

    M_WriteText(x_str, 151, str, ID_WidgetColor(widget_speed_str));
    M_WriteText(x_val, 151, val, ID_WidgetColor(widget_speed_val));
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
    // [PN] Array of crosshair shapes with explicit type casting
    patch_t *const crosshair_shapes[] = {
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
