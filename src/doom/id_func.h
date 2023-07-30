//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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


#pragma once


#define singleplayer (!demorecording && !demoplayback && !netgame)


//
// Data types
//

// Render counters data.
typedef struct ID_Data_s
{
    int numsprites;     // [JN] Number of sprites.
    int numsegs;        // [JN] Number of wall segments.
    int numplanes;      // [JN] Number of visplanes.
    int numopenings;    // [JN] Number of openings.
} ID_Render_t;

extern ID_Render_t IDRender;

// Widgets data. 
typedef struct ID_Widget_s
{
    int x;    // Player X coord
    int y;    // Player Y coord
    int ang;  // Player angle

    int time; // Time spent on the level.

    int kills;         // Current kill count
    int extrakills;    // Current extra kill count
    int totalkills;    // Total enemy count on the level
    int items;         // Current items count
    int totalitems;    // Total item count on the level
    int secrets;       // Current secrets count
    int totalsecrets;  // Total secrets on the level

    int frags_g;       // Frags counter of green player
    int frags_i;       // Frags counter of indigo player
    int frags_b;       // Frags counter of brown player
    int frags_r;       // Frags counter of red player
} ID_Widget_t;

extern ID_Widget_t IDWidget;

extern char ID_Level_Time[64];
extern char ID_Total_Time[64];
extern char ID_Local_Time[64];

//
// Render Counters and Widgets
//

extern void ID_LeftWidgets (void);
extern void ID_RightWidgets (void);
extern void ID_DrawTargetsHealth (void);

//
// Crosshair
//

extern void ID_DrawCrosshair (void);

//
// [crispy] demo progress bar and timer widget
//

extern void ID_DemoTimer (const int time);
extern void ID_DemoBar (void);
extern int  defdemotics, deftotaldemotics;
extern int  demowarp;

//
// Level Select one variable
//

extern int level_select[];

//
// Spectator Mode
//

extern fixed_t CRL_camera_x, CRL_camera_y, CRL_camera_z;
extern fixed_t CRL_camera_oldx, CRL_camera_oldy, CRL_camera_oldz;
extern angle_t CRL_camera_ang;
extern angle_t CRL_camera_oldang;

extern void CRL_GetCameraPos (fixed_t *x, fixed_t *y, fixed_t *z, angle_t *a);
extern void CRL_ReportPosition (fixed_t x, fixed_t y, fixed_t z, angle_t angle);
extern void CRL_ImpulseCamera (fixed_t fwm, fixed_t swm, angle_t at);
extern void CRL_ImpulseCameraVert (boolean direction, fixed_t intensity);
