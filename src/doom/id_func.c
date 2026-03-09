//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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


#include <stdio.h>

#include "v_trans.h"
#include "v_video.h"
#include "doomstat.h"
#include "mn_menu.h"
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
    int value = 0, total = 0;
    
    // [PN] Set values for kills, items, or secrets based on widget type
    switch (i)
    {
        case widgets_kis_kills:
            value = IDWidget.kills;
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
        {
            const int total_value = total - value;
            snprintf(buffer, buffer_size, "%d", total_value);
            break;
        }

        case 2: // Percent
        {
            snprintf(buffer, buffer_size, "%d%%", safe_percent(value, total));
            break;
        }

        case 3: // Count
        {
            snprintf(buffer, buffer_size, "%d", value);
            break;
        }

        default: // Ratio
        {
            snprintf(buffer, buffer_size, "%d/%d", value, total);
            break;
        }
    }
}

// [JN/PN] Format time string for the level/total time widget.
void ID_FormatWidgetTime (char *buf, size_t bufsize, int ticks, int mode)
{
    const int hours   = ticks / (3600 * TICRATE);
    const int mins    = (ticks / (60 * TICRATE)) % 60;
    const int sec     = (ticks / TICRATE) % 60;
    const int csec    = (((ticks % TICRATE) * 100 + TICRATE) >> 1) / TICRATE;
    const int show_cs = (mode == 3 || mode == 4);

    if (hours)
    {
        const char *const fmt = show_cs ? "%02i:%02i:%02i.%02i" : "%02i:%02i:%02i";
        M_snprintf(buf, bufsize, fmt, hours, mins, sec, show_cs ? csec : 0);
    }
    else
    {
        const char *const fmt = show_cs ? "%02i:%02i.%02i" : "%02i:%02i";
        M_snprintf(buf, bufsize, fmt, mins, sec, show_cs ? csec : 0);
    }
}

// -----------------------------------------------------------------------------
// ID_LeftWidgets.
//  [JN/PN] Draw all the widgets and counters.
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
                const int yy = widget_kis_items ? 0 : 9;  // shift secret up if items hidden

                // Kills:
                M_WriteText(left_align, 9, "K:", ID_WidgetColor(widget_kis_str));
                char buf1[16];
                ID_WidgetKISCount(buf1, sizeof(buf1), widgets_kis_kills);
                M_WriteText(left_align + 16, 9, buf1, ID_WidgetColor(widget_kills));

                // Items:
                if (widget_kis_items)
                {
                    M_WriteText(left_align, 18, "I:", ID_WidgetColor(widget_kis_str));
                    char buf2[16];
                    ID_WidgetKISCount(buf2, sizeof(buf2), widgets_kis_items);
                    M_WriteText(left_align + 16, 18, buf2, ID_WidgetColor(widget_items));
                }

                // Secret:
                M_WriteText(left_align, 27 - yy, "S:", ID_WidgetColor(widget_kis_str));
                char buf3[16];
                ID_WidgetKISCount(buf3, sizeof(buf3), widgets_kis_secrets);
                M_WriteText(left_align + 16, 27 - yy, buf3, ID_WidgetColor(widget_secret));
            }
            else
            {
                // Deathmatch frags for up to 4 players
                const char *const labels[] = {"G:", "I:", "B:", "R:"};
                const int y_pos[] = {9, 18, 27, 36};
                const int *const frags[] = {&IDWidget.frags_g, &IDWidget.frags_i,
                                            &IDWidget.frags_b, &IDWidget.frags_r};
                const int colors[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};

                for (int i = 0; i < 4; i++)
                {
                    if (!playeringame[i]) continue;
                    M_WriteText(left_align, y_pos[i], labels[i],
                                ID_WidgetColor(colors[i]));
                    char buf[16];
                    sprintf(buf, "%d", *frags[i]);
                    M_WriteText(left_align + 16, y_pos[i], buf, ID_WidgetColor(colors[i]));
                }
            }
        }

        // Level / DeathMatch timer and Total time. Time gathered in G_Ticker.
        struct { int cond; const char *label; const char *value; int y_label; int y_val; } times[] = {
            {widget_time,      "TIME",  ID_Level_Time,  45, 54},
            {widget_totaltime, "TOTAL", ID_Total_Time,  63, 72}
        };
        for (int i = 0; i < 2; i++)
        {
            const int c = times[i].cond;
            if (c == 1 || c == 3 || ((c == 2 || c == 4) && automapactive))
            {
                M_WriteText(left_align, times[i].y_label, times[i].label, ID_WidgetColor(widget_time_str));
                M_WriteText(left_align, times[i].y_val, times[i].value, ID_WidgetColor(widget_time_val));
            }
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            struct { const char *label; int y; int offset; int *value; } coords[] = {
                {"X:", 90, 16, &IDWidget.x},
                {"Y:", 99, 16, &IDWidget.y},
                {"ANG:", 108, 32, &IDWidget.ang}
            };
            for (int i = 0; i < 3; i++)
            {
                M_WriteText(left_align, coords[i].y, coords[i].label,
                            ID_WidgetColor(widget_coords_str));
                char buf[32];
                sprintf(buf, "%d", *coords[i].value);
                M_WriteText(left_align + coords[i].offset, coords[i].y, buf, ID_WidgetColor(widget_coords_val));
            }
        }

        // Render counters
        if (widget_render)
        {
            struct { const char *label; int y; int value; } counters[] = {
                {"SPR:", 124, IDRender.numsprites},
                {"SEG:", 133, IDRender.numsegs},
                {"OPN:", 142, IDRender.numopenings},
                {"PLN:", 151, IDRender.numplanes}
            };
            for (int i = 0; i < 4; i++)
            {
                M_WriteText(left_align, counters[i].y, counters[i].label,
                            ID_WidgetColor(widget_render_str));
                char buf[32];
                M_snprintf(buf, sizeof(buf), "%d", counters[i].value);
                M_WriteText(left_align + 32, counters[i].y, buf, ID_WidgetColor(widget_render_val));
            }
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
            const int base_y = 54 + (widget_coords ? 0 : 34);
            struct { const char *label; int value; } counters[] = {
                {"SPR:", IDRender.numsprites},
                {"SEG:", IDRender.numsegs},
                {"OPN:", IDRender.numopenings},
                {"PLN:", IDRender.numplanes}
            };

            for (int i = 0; i < 4; i++)
            {
                const int y = base_y + 9 * i;
                M_WriteText(left_align, y, counters[i].label, ID_WidgetColor(widget_render_str));
                char buf[32];
                M_snprintf(buf, sizeof(buf), "%d", counters[i].value);
                M_WriteText(left_align + 32, y, buf, ID_WidgetColor(widget_render_val));
            }
        }

        // Player coords
        if (widget_coords == 1
        || (widget_coords == 2 && automapactive))
        {
            struct { const char *label; int y; int val_x_offset; int *value; } coords[] = {
                {"X:", 97, 16, &IDWidget.x},
                {"Y:", 106, 16, &IDWidget.y},
                {"ANG:", 115, 32, &IDWidget.ang}
            };
            for (int i = 0; i < 3; i++)
            {
                M_WriteText(left_align, coords[i].y, coords[i].label, ID_WidgetColor(widget_coords_str));
                char buf[32];
                sprintf(buf, "%d", *coords[i].value);
                M_WriteText(left_align + coords[i].val_x_offset, coords[i].y, buf, ID_WidgetColor(widget_coords_val));
            }
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
                const char *labels[] = {"K ", " I ", " S "};
                const int active[] = {1, widget_kis_items, 1};  // kills, items (optional), secrets
                const int params[] = {widgets_kis_kills, widgets_kis_items, widgets_kis_secrets};
                const int colors[] = {widget_kills, widget_items, widget_secret};
                int x = left_align;
            
                for (int i = 0; i < 3; i++)
                {
                    if (!active[i]) continue;
                    M_WriteText(x, 160 + yy, labels[i], ID_WidgetColor(widget_kis_str));
                    x += M_StringWidth(labels[i]);
            
                    char buf[16];
                    ID_WidgetKISCount(buf, sizeof(buf), params[i]);
                    M_WriteText(x, 160 + yy, buf, ID_WidgetColor(colors[i]));
                    x += M_StringWidth(buf);
                }
            }
            else
            {
                const char *labels[] = {"G ", "I ", "B ", "R "};
                const int *frags[] = {&IDWidget.frags_g, &IDWidget.frags_i, &IDWidget.frags_b, &IDWidget.frags_r};
                const int colors[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};
                int x = left_align;
            
                for (int i = 0; i < 4; i++)
                {
                    if (!playeringame[i]) continue;
                    M_WriteText(x, 160 + yy, labels[i], ID_WidgetColor(colors[i]));
                    x += M_StringWidth(labels[i]);
            
                    char buf[16];
                    sprintf(buf, "%d ", *frags[i]);
                    M_WriteText(x, 160 + yy, buf, ID_WidgetColor(colors[i]));
                    x += M_StringWidth(buf);
                }
            }
        }

        if (widget_kis)
        {
            yy -= 9;
        }

        // Level / DeathMatch and Total time. Time gathered in G_Ticker.
        struct { int cond; const char *label; const char *value; } times[] = {
            {widget_totaltime, "TOTAL ", ID_Total_Time},
            {widget_time,      "TIME ",  ID_Level_Time}
        };
        for (int i = 0; i < 2; i++)
        {
            const int c = times[i].cond;
            if (c == 1 || c == 3 || ((c == 2 || c == 4) && automapactive))
            {
                M_WriteText(left_align, 160 + yy, times[i].label, ID_WidgetColor(widget_time_str));
                M_WriteText(left_align + M_StringWidth(times[i].label), 160 + yy, 
                            times[i].value, ID_WidgetColor(widget_time_val));
            }
            if (c) yy -= 9;
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
    0,    // 26 - Co-op spawn
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

// [PN] Spectator mouse look in MLOOKUNIT units.
fixed_t CRL_camera_lookdir;
fixed_t CRL_camera_oldlookdir;

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
// [PN/JN] Spectator mouse look.
//  Clamp to the same range as player lookdir.
// -----------------------------------------------------------------------------

#define CLAMPLOOKDIR(x) BETWEEN(-LOOKDIRMIN * MLOOKUNIT, LOOKDIRMAX * MLOOKUNIT, (x))

void CRL_ReportLookdir (fixed_t lookdir)
{
    CRL_camera_oldlookdir = CRL_camera_lookdir;
    CRL_camera_lookdir = CLAMPLOOKDIR(lookdir);
}

void CRL_LimitLookdir (fixed_t delta)
{
    CRL_camera_lookdir = CLAMPLOOKDIR(CRL_camera_lookdir + delta);
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
