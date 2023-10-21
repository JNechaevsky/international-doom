//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	System specific interface stuff.
//


#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"
#include "m_fixed.h"


// Screen width and height.

#define ORIGWIDTH  320 // [crispy]
#define ORIGHEIGHT 200 // [crispy]

// [JN] Increase more to support quad rendering resolution.
#define MAXWIDTH  (ORIGWIDTH << 3)  // [crispy] 
#define MAXHEIGHT (ORIGHEIGHT << 2) // [crispy] 

extern int SCREENWIDTH;
extern int SCREENHEIGHT;
extern int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
extern int WIDESCREENDELTA; // [crispy] horizontal widescreen offset
extern void (*post_rendering_hook) (void); // [crispy]
void I_GetScreenDimensions (void); // [crispy] re-calculate WIDESCREENDELTA
extern void I_ToggleVsync (void);

enum
{
    RATIO_ORIG,
    RATIO_MATCH_SCREEN,
    RATIO_16_10,
    RATIO_16_9,
    RATIO_21_9,
    NUM_RATIOS
};

enum
{
	REINIT_FRAMEBUFFERS = 1,
	REINIT_RENDERER = 2,
	REINIT_TEXTURES = 4,
	REINIT_ASPECTRATIO = 8,
};

// Screen height used when vid_aspect_ratio_correct=true.

#define ORIGHEIGHT_4_3 240 // [crispy]
#define MAXHEIGHT_4_3 (ORIGHEIGHT_4_3 << 1) // [crispy]

extern int SCREENHEIGHT_4_3;

void *I_GetSDLWindow(void);
void *I_GetSDLRenderer(void);

typedef boolean (*grabmouse_callback_t)(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void);
void I_ReInitGraphics (int reinit);

void I_GraphicsCheckCommandLine(void);

void I_ShutdownGraphics(void);

void I_RenderReadPixels (byte **data, int *w, int *h);

// Takes full 8 bit values.
#ifndef CRISPY_TRUECOLOR
void I_SetPalette (byte* palette);
int I_GetPaletteIndex(int r, int g, int b);
#else
void I_SetPalette (int palette);
extern const pixel_t I_MapRGB (const uint8_t r, const uint8_t g, const uint8_t b);
extern const int I_ShadeFactor[];
extern const float I_SaturationPercent[];
#endif
void I_FinishUpdate (void);
void I_FinishDemoWarpUpdate (void);

void I_ReadScreen (pixel_t* scr);

void I_BeginRead (void);

void I_SetWindowTitle(char *title);

void I_CheckIsScreensaver(void);
void I_SetGrabMouseCallback(grabmouse_callback_t func);

void I_DisplayFPSDots(boolean dots_on);
void I_BindVideoVariables(void);

void I_InitWindowTitle(void);
void I_RegisterWindowIcon(const unsigned int *icon, int width, int height);
void I_InitWindowIcon(void);

// Called before processing any tics in a frame (just after displaying a frame).
// Time consuming syncronous operations are performed here (joystick reading).

void I_StartFrame (void);

// Called before processing each tic in a frame.
// Quick syncronous operations are performed here.

void I_StartTic (void);


extern char *vid_video_driver;
extern boolean screenvisible;

extern int vanilla_keyboard_mapping;
extern boolean screensaver_mode;
extern pixel_t *I_VideoBuffer;

extern int screen_width;
extern int screen_height;
extern int vid_fullscreen;
extern int vid_aspect_ratio_correct;
extern int vid_smooth_scaling;
extern int vid_integer_scaling;
extern int vid_vga_porch_flash;
extern int vid_force_software_renderer;
extern int id_fps_value;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
extern fixed_t fractionaltic;

extern char *window_position;
void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
extern unsigned int joywait;

extern int usemouse;

extern boolean endoom_screen_active;
extern boolean volume_needs_update;
extern boolean window_focused;

#endif
