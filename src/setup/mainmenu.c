//
// Copyright(C) 2005-2014 Simon Howard
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
#include <stdlib.h>
#include <string.h>
#include "textscreen.h"
#include "m_misc.h"
#include "mode.h"
#include "multiplayer.h"
#include "setup_icon.c"
#include "id_vars.h"


void MainMenu(void)
{
    txt_window_t *window;

    window = TXT_NewWindow("Multiplayer");

    TXT_AddWidgets(window,
        TXT_NewButton2("Start a Network Game",
                       (TxtWidgetSignalFunc) StartMultiGame, NULL),
        TXT_NewButton2("Join a Network Game",
                       (TxtWidgetSignalFunc) JoinMultiGame, NULL),
    NULL);
}

//
// Application icon
//

static void SetIcon(void)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) setup_icon_data, setup_icon_w,
                                       setup_icon_h, 32, setup_icon_w * 4,
                                       0xffu << 24, 0xffu << 16,
                                       0xffu << 8, 0xffu << 0);

    SDL_SetWindowIcon(TXT_SDLWindow, surface);
    SDL_FreeSurface(surface);
}

static void SetWindowTitle(void)
{
    TXT_SetDesktopTitle("Multiplayer launcher");
}

// Initialize the textscreen library.

static void InitTextscreen(void)
{
    if (!TXT_Init())
    {
        fprintf(stderr, "Failed to initialize GUI\n");
        exit(-1);
    }

    // [JN] Apply CRL-styled colors.
    TXT_SetColor(TXT_COLOR_BLUE,   0,  57, 134);
    TXT_SetColor(TXT_COLOR_CYAN,   0, 137, 208);
    TXT_SetColor(TXT_COLOR_GREY, 140, 151, 168);
    TXT_SetColor(TXT_COLOR_BRIGHT_CYAN,   0, 168, 255);
    TXT_SetColor(TXT_COLOR_BRIGHT_GREEN, 84, 255, 255);

    SetIcon();
    SetWindowTitle();
}


// 
// Initialize and run the textscreen GUI.
//

static void RunGUI(void)
{
    InitTextscreen();

    TXT_GUIMainLoop();
}

static void MissionSet(void)
{
    SetWindowTitle();
    MainMenu();
}

void D_DoomMain(void)
{
    SetupMission(MissionSet);

    RunGUI();
}
