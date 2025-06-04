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
//    System-specific keyboard/mouse input.
//


#ifndef __I_INPUT__
#define __I_INPUT__

#include "doomtype.h"

#include <SDL3/SDL.h>


#define MAX_MOUSE_BUTTONS 8

extern int mouse_sensitivity;
extern int mouse_sensitivity_y; // [crispy]
extern float mouse_acceleration;
extern int mouse_threshold;
extern float mouse_acceleration_y; // [crispy]
extern int mouse_threshold_y; // [crispy]
extern int mouse_y_invert; // [crispy]
extern int mouse_novert; // [crispy]
extern int SDL_mouseButton; // [JN] Catch mouse button number to provide into mouse binding menu.

// [crispy]
double I_AccelerateMouse(int val);
double I_AccelerateMouseY(int val);
void I_BindInputVariables(void);
void I_ReadMouse(void);
void I_ReadMouseUncapped(void); // [crispy]

// I_StartTextInput begins text input, activating the on-screen keyboard
// (if one is used). The caller indicates that any entered text will be
// displayed in the rectangle given by the provided set of coordinates.
void I_StartTextInput(int x1, int y1, int x2, int y2);

// I_StopTextInput finishes text input, deactivating the on-screen keyboard
// (if one is used).
void I_StopTextInput(void);

void I_HandleKeyboardEvent(SDL_Event *sdlevent);
void I_HandleMouseEvent(SDL_Event *sdlevent);


#endif
