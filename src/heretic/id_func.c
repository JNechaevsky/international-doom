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
        if (widget_kis == 1 || (widget_kis == 2 && automapactive))
        {
            if (!deathmatch)
            {
                const int yy = widget_kis_items ? 0 : 10;  // shift secret up if items hidden

                // Kills:
                MN_DrTextA("K:", left_align, 10, ID_WidgetColor(widget_kis_str));
                char buf1[16];
                ID_WidgetKISCount(buf1, sizeof(buf1), widgets_kis_kills);
                MN_DrTextA(buf1, left_align + 16, 10, ID_WidgetColor(widget_kills));

                // Items:
                if (widget_kis_items)
                {
                    MN_DrTextA("I:", left_align, 20, ID_WidgetColor(widget_kis_str));
                    char buf2[16];
                    ID_WidgetKISCount(buf2, sizeof(buf2), widgets_kis_items);
                    MN_DrTextA(buf2, left_align + 16, 20, ID_WidgetColor(widget_items));
                }

                // Secret:
                MN_DrTextA("S:", left_align, 30 - yy, ID_WidgetColor(widget_kis_str));
                char buf3[16];
                ID_WidgetKISCount(buf3, sizeof(buf3), widgets_kis_secrets);
                MN_DrTextA(buf3, left_align + 16, 30 - yy, ID_WidgetColor(widget_secret));
            }
            else
            {
                const char *const labels[] = {"G:", "Y:", "R:", "B:"};
                const int y_pos[] = {10, 20, 30, 40};
                const int *const frags[] = {&IDWidget.frags_g, &IDWidget.frags_y,
                                            &IDWidget.frags_r, &IDWidget.frags_b};
                const int colors[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};

                for (int i = 0; i < 4; i++)
                {
                    if (!playeringame[i]) continue;
                    MN_DrTextA(labels[i], left_align, y_pos[i],
                               ID_WidgetColor(colors[i]));
                    char buf[16];
                    sprintf(buf, "%d", *frags[i]);
                    MN_DrTextA(buf, left_align + 16, y_pos[i], ID_WidgetColor(colors[i]));
                }
            }
        }

        // Level / DeathMatch timer and Total time. Time gathered in G_Ticker.
        struct { int cond; const char *label; const char *value; int y_label; int y_val; } times[] = {
            {widget_time,      "TIME",  ID_Level_Time,  40, 50},
            {widget_totaltime, "TOTAL", ID_Total_Time,  60, 70}
        };
        for (int i = 0; i < 2; i++)
        {
            const int c = times[i].cond;
            if (c == 1 || c == 3 || ((c == 2 || c == 4) && automapactive))
            {
                MN_DrTextA(times[i].label, left_align, times[i].y_label, ID_WidgetColor(widget_time_str));
                MN_DrTextA(times[i].value, left_align, times[i].y_val, ID_WidgetColor(widget_time_val));
            }
        }

        // Player coords
        if (widget_coords == 1 || (widget_coords == 2 && automapactive))
        {
            struct { const char *label; int y; int offset; int *value; } coords[] = {
                {"X:", 80, 16, &IDWidget.x},
                {"Y:", 90, 16, &IDWidget.y},
                {"ANG:", 100, 32, &IDWidget.ang}
            };
            for (int i = 0; i < 3; i++)
            {
                MN_DrTextA(coords[i].label, left_align, coords[i].y, ID_WidgetColor(widget_coords_str));
                char buf[32];
                sprintf(buf, "%d", *coords[i].value);
                MN_DrTextA(buf, left_align + coords[i].offset, coords[i].y, ID_WidgetColor(widget_coords_val));
            }
        }

        // Render counters
        if (widget_render)
        {
            struct { const char *label; int y; int value; } counters[] = {
                {"SPR:", 110, IDRender.numsprites},
                {"SEG:", 120, IDRender.numsegs},
                {"OPN:", 130, IDRender.numopenings},
                {"PLN:", 140, IDRender.numplanes}
            };
            for (int i = 0; i < 4; i++)
            {
                MN_DrTextA(counters[i].label, left_align, counters[i].y, ID_WidgetColor(widget_render_str));
                char buf[32];
                M_snprintf(buf, sizeof(buf), "%d", counters[i].value);
                MN_DrTextA(buf, left_align + 32, counters[i].y, ID_WidgetColor(widget_render_val));
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
            const int yy1 = widget_coords ? 0 : 45;
            struct { const char *label; int y; int value; } counters[] = {
                {"SPR:", 26 + yy1, IDRender.numsprites},
                {"SEG:", 36 + yy1, IDRender.numsegs},
                {"OPN:", 46 + yy1, IDRender.numopenings},
                {"PLN:", 56 + yy1, IDRender.numplanes}
            };
            for (int i = 0; i < 4; i++)
            {
                MN_DrTextA(counters[i].label, left_align, counters[i].y, ID_WidgetColor(widget_render_str));
                char buf[32];
                M_snprintf(buf, sizeof(buf), "%d", counters[i].value);
                MN_DrTextA(buf, left_align + 32, counters[i].y, ID_WidgetColor(widget_render_val));
            }
        }

        // Player coords
        if (widget_coords == 1 || (widget_coords == 2 && automapactive))
        {
            struct { const char *label; int y; int offset; int *value; } coords[] = {
                {"X:", 76, 16, &IDWidget.x},
                {"Y:", 86, 16, &IDWidget.y},
                {"ANG:", 96, 32, &IDWidget.ang}
            };
            for (int i = 0; i < 3; i++)
            {
                MN_DrTextA(coords[i].label, left_align, coords[i].y, ID_WidgetColor(widget_coords_str));
                char buf[32];
                sprintf(buf, "%d", *coords[i].value);
                MN_DrTextA(buf, left_align + coords[i].offset, coords[i].y, ID_WidgetColor(widget_coords_val));
            }
        }

        if (automapactive)
        {
            yy -= 10;
        }

        // K/I/S stats
        if (widget_kis == 1 || (widget_kis == 2 && automapactive))
        {
            if (!deathmatch)
            {
                const char *const labels[] = {"K ", " I ", " S "};
                const int active[] = {1, widget_kis_items, 1};
                const int params[] = {widgets_kis_kills, widgets_kis_items, widgets_kis_secrets};
                const int colors[] = {widget_kills, widget_items, widget_secret};
                int x = left_align;

                for (int i = 0; i < 3; i++)
                {
                    if (!active[i]) continue;
                    MN_DrTextA(labels[i], x, 146 + yy, ID_WidgetColor(widget_kis_str));
                    x += MN_TextAWidth(labels[i]);

                    char buf[16];
                    ID_WidgetKISCount(buf, sizeof(buf), params[i]);
                    MN_DrTextA(buf, x, 146 + yy, ID_WidgetColor(colors[i]));
                    x += MN_TextAWidth(buf);
                }
            }
            else
            {
                const char *const labels[] = {"G ", "Y ", "R ", "B "};
                const int *const frags[] = {&IDWidget.frags_g, &IDWidget.frags_y,
                                            &IDWidget.frags_r, &IDWidget.frags_b};
                const int colors[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};
                int x = left_align;

                for (int i = 0; i < 4; i++)
                {
                    if (!playeringame[i]) continue;
                    MN_DrTextA(labels[i], x, 146 + yy, ID_WidgetColor(colors[i]));
                    x += MN_TextAWidth(labels[i]);

                    char buf[16];
                    sprintf(buf, "%d ", *frags[i]);
                    MN_DrTextA(buf, x, 146 + yy, ID_WidgetColor(colors[i]));
                    x += MN_TextAWidth(buf);
                }
            }
        }

        if (widget_kis)
        {
            yy -= 10;
        }

        // Total time and Level / DeathMatch timer. Time gathered in G_Ticker.
        struct { int cond; const char *label; const char *value; } times[] = {
            {widget_totaltime, "TOTAL ", ID_Total_Time},
            {widget_time,      "TIME ",  ID_Level_Time}
        };
        for (int i = 0; i < 2; i++)
        {
            const int c = times[i].cond;
            if (c == 1 || c == 3 || ((c == 2 || c == 4) && automapactive))
            {
                MN_DrTextA(times[i].label, left_align, 146 + yy, ID_WidgetColor(widget_time_str));
                MN_DrTextA(times[i].value, left_align + MN_TextAWidth(times[i].label), 
                           146 + yy, ID_WidgetColor(widget_time_val));
            }
            if (c) yy -= 10;
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

    // Move the widget slightly further down when using a fullscreen status bar.
    const int yy = (dp_screen_size > 10 && (!automapactive || automap_overlay)) ? 13 : 0;

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
            MN_DrTextACentered(str, 125 + yy, color);
            break;
        case 4:  // Bottom + name
            MN_DrTextACentered(player->targetsname, 125 + yy, color);
            MN_DrTextACentered(str, 115 + yy, color);
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

    // Calculating speed only every game tic (not every frame)
    // is not possible while D_Display is called after S_UpdateSounds in D_DoomLoop.
    // if (oldgametic < gametic)
    {
        const double dx = (double)(player->mo->x - player->mo->oldx) / FRACUNIT;
        const double dy = (double)(player->mo->y - player->mo->oldy) / FRACUNIT;
        const double dz = (double)(player->mo->z - player->mo->oldz) / FRACUNIT;
        speed = sqrt(dx * dx + dy * dy + dz * dz) * TICRATE;
    }

    M_snprintf(str, sizeof(str), "SPD:");
    M_snprintf(val, sizeof(val), " %.0f", speed);

    const int x_val = (ORIGWIDTH / 2);
    const int x_str = x_val - MN_TextAWidth(str);

    // Move the widget slightly further down when using a fullscreen status bar.
    const int yy = (dp_screen_size > 10 && (!automapactive || automap_overlay)) ? 13 : 0;

    MN_DrTextA(str, x_str, 136 + yy, ID_WidgetColor(widget_speed_str));
    MN_DrTextA(val, x_val, 136 + yy, ID_WidgetColor(widget_speed_val));
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
    0,    // 34 - Co-op spawn
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
