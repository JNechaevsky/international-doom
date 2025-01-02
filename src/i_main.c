//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	Main program, simply calls D_DoomMain high level loop.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h> // [crispy] setlocale
#include <time.h>   // [JN] srand(time(0))
#include <SDL.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "config.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"


//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
//

void D_DoomMain (void);

int main(int argc, char **argv)
{
    // save arguments

    myargc = argc;
    myargv = malloc(argc * sizeof(char *));
    assert(myargv != NULL);

    for (int i = 0; i < argc; i++)
    {
        myargv[i] = M_StringDuplicate(argv[i]);
    }

    // [crispy] Print date and time in the Load/Save Game menus in the current locale
    setlocale(LC_TIME, "");

#if defined(_WIN32)
    // [JN] Create console output window if "-console" parameter is present.
    if (M_CheckParm ("-console"))
    {
        // Allocate console.
        AllocConsole();
        SetConsoleTitle("Console");

        // Head text outputs.
        freopen("CONIN$", "r",stdin); 
        freopen("CONOUT$","w",stdout); 
        freopen("CONOUT$","w",stderr); 

        // Set a proper codepage.
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
#endif

    // [FG] compose a proper command line from loose file paths passed as arguments
    // to allow for loading WADs and DEHACKED patches by drag-and-drop
    M_AddLooseFiles();

    M_FindResponseFile();
    M_SetExeDir();

    // [JN] Use current time as seed for random generator.
    srand(time(0));

    // start doom

    D_DoomMain ();

    return 0;
}

