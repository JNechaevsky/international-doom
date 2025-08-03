//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	System specific interface stuff.
//


#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "SDL.h"
#include "doomtype.h"
#include "i_truecolor.h"
#include "m_fixed.h"
#include "v_postproc.h"


// Screen width and height.

#define ORIGWIDTH  320 // [crispy]
#define ORIGHEIGHT 200 // [crispy]

// [JN] Maximum available rendering resolution.
#define MAXHIRES 6

// [JN] Allocate enough to support higher rendering resolutions and 32:9 ratio.
#define MAXWIDTH  5136 // [crispy] [JN] 856 * 6
#define MAXHEIGHT 1200 // [crispy] [JN] 200 * 6

extern int SCREENWIDTH;
extern int SCREENHEIGHT;
extern int SCREENAREA; // [JN] SCREENWIDTH * SCREENHEIGHT
extern int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
extern int WIDESCREENDELTA; // [crispy] horizontal widescreen offset
extern void (*post_rendering_hook) (void); // [crispy]
void I_GetScreenDimensions (void); // [crispy] re-calculate WIDESCREENDELTA
extern void I_ToggleVsync (void);
extern void I_SetColorPanes (boolean recreate_argbbuffer); // [PN] (Re-)initialize color panes
extern void I_UpdateExclusiveFullScreen(void);

enum
{
    RATIO_ORIG,
    RATIO_MATCH_SCREEN,
    RATIO_16_10,
    RATIO_16_9,
    RATIO_21_9,
    RATIO_32_9,
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
#define MAXHEIGHT_4_3 1440 // [crispy] [JN] 240 * 6

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
void I_SetPalette (int palette);

// [PN] We define a macro that manually assembles a 32-bit pixel from separate R/G/B values.
// It applies the format's shift and loss adjustments, then merges all components (and alpha mask) with bitwise OR.
// This avoids the overhead of the SDL_MapRGB() function call.
//
// Original human-readable mapping function from Crispy Doom:
/*
#ifdef CRISPY_TRUECOLOR
const pixel_t I_MapRGB (const uint8_t r, const uint8_t g, const uint8_t b)
{
	return SDL_MapRGB(argbbuffer->format, r, g, b);
}
#endif
*/

extern SDL_Surface *argbbuffer;

#define I_MapRGB(r, g, b) ( \
    (pixel_t)( \
        (((uint32_t)(r) >> argbbuffer->format->Rloss) << argbbuffer->format->Rshift) | \
        (((uint32_t)(g) >> argbbuffer->format->Gloss) << argbbuffer->format->Gshift) | \
        (((uint32_t)(b) >> argbbuffer->format->Bloss) << argbbuffer->format->Bshift) | \
        (argbbuffer->format->Amask) \
    ) \
)
void I_FinishUpdate (void);

void I_ReadScreen (pixel_t* scr);

void I_BeginRead (void);

void I_SetWindowTitle(const char *title);

void I_CheckIsScreensaver(void);
void I_SetGrabMouseCallback(grabmouse_callback_t func);
void CenterWindow(int *x, int *y, int w, int h);

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

void I_UpdateFracTic (void); // [crispy]
void I_StartDisplay (void); // [crispy]


extern char *vid_video_driver;
extern boolean screenvisible;

extern int vanilla_keyboard_mapping;
extern boolean screensaver_mode;
extern pixel_t *I_VideoBuffer;

extern int screen_width;
extern int screen_height;
extern int vid_fullscreen;
extern int vid_fullscreen_exclusive;
extern int vid_aspect_ratio_correct;
extern int vid_smooth_scaling;
extern int vid_integer_scaling;
extern int vid_vga_porch_flash;
extern int vid_force_software_renderer;
extern int id_fps_value;
extern int demowarp;

// [JN] Smooth palette.
extern int red_pane_alpha, yel_pane_alpha, grn_pane_alpha;
extern int gray_pane_alpha, orng_pane_alpha;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
extern fixed_t fractionaltic;

extern char *window_position;
void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
extern unsigned int joywait;

extern int usemouse;

extern int menu_mouse_x;
extern int menu_mouse_y;
extern boolean menu_mouse_allow, menu_mouse_allow_click;
extern void I_ReInitCursorPosition (void);

extern boolean endoom_screen_active;
extern boolean volume_needs_update;
extern boolean window_focused;

#endif
