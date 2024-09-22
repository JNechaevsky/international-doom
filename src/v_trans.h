// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_video.h,v 1.9 1998/05/06 11:12:54 jim Exp $
//
//  BOOM, a modified and improved DOOM engine
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//  Gamma correction LUT.
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#pragma once

#include "doomtype.h"


enum
{
    CR_DARK,

    CR_MENU_BRIGHT5,
    CR_MENU_BRIGHT4,
    CR_MENU_BRIGHT3,
    CR_MENU_BRIGHT2,
    CR_MENU_BRIGHT1,
    CR_MENU_DARK1,
    CR_MENU_DARK2,
    CR_MENU_DARK3,
    CR_MENU_DARK4,

    CR_RED,
    CR_RED_BRIGHT5,
    CR_RED_BRIGHT4,
    CR_RED_BRIGHT3,
    CR_RED_BRIGHT2,
    CR_RED_BRIGHT1,
    CR_RED_DARK1,
    CR_RED_DARK2,
    CR_RED_DARK3,
    CR_RED_DARK4,
    CR_RED_DARK5,

    CR_DARKRED,

    CR_GREEN,
    CR_GREEN_BRIGHT5,
    CR_GREEN_BRIGHT4,
    CR_GREEN_BRIGHT3,
    CR_GREEN_BRIGHT2,
    CR_GREEN_BRIGHT1,

    CR_GREEN_HX,
    CR_GREEN_BRIGHT5_HX,
    CR_GREEN_BRIGHT4_HX,
    CR_GREEN_BRIGHT3_HX,
    CR_GREEN_BRIGHT2_HX,
    CR_GREEN_BRIGHT1_HX,

    CR_DARKGREEN,
    CR_DARKGREEN_BRIGHT5,
    CR_DARKGREEN_BRIGHT4,
    CR_DARKGREEN_BRIGHT3,
    CR_DARKGREEN_BRIGHT2,
    CR_DARKGREEN_BRIGHT1,

    CR_OLIVE,
    CR_OLIVE_BRIGHT5,
    CR_OLIVE_BRIGHT4,
    CR_OLIVE_BRIGHT3,
    CR_OLIVE_BRIGHT2,
    CR_OLIVE_BRIGHT1,

    CR_BLUE2,
    CR_BLUE2_BRIGHT5,
    CR_BLUE2_BRIGHT4,
    CR_BLUE2_BRIGHT3,
    CR_BLUE2_BRIGHT2,
    CR_BLUE2_BRIGHT1,

    CR_YELLOW,
    CR_YELLOW_BRIGHT5,
    CR_YELLOW_BRIGHT4,
    CR_YELLOW_BRIGHT3,
    CR_YELLOW_BRIGHT2,
    CR_YELLOW_BRIGHT1,

    CR_ORANGE,
    CR_ORANGE_BRIGHT5,
    CR_ORANGE_BRIGHT4,
    CR_ORANGE_BRIGHT3,
    CR_ORANGE_BRIGHT2,
    CR_ORANGE_BRIGHT1,

    CR_ORANGE_HR,
    CR_ORANGE_HR_BRIGHT5,
    CR_ORANGE_HR_BRIGHT4,
    CR_ORANGE_HR_BRIGHT3,
    CR_ORANGE_HR_BRIGHT2,
    CR_ORANGE_HR_BRIGHT1,

    CR_WHITE,
    CR_GRAY,
    CR_GRAY_BRIGHT5,
    CR_GRAY_BRIGHT4,
    CR_GRAY_BRIGHT3,
    CR_GRAY_BRIGHT2,
    CR_GRAY_BRIGHT1,

    CR_LIGHTGRAY,
    CR_LIGHTGRAY_BRIGHT5,
    CR_LIGHTGRAY_BRIGHT4,
    CR_LIGHTGRAY_BRIGHT3,
    CR_LIGHTGRAY_BRIGHT2,
    CR_LIGHTGRAY_BRIGHT1,
    CR_LIGHTGRAY_DARK1,

    CR_BROWN,
    CR_FLAME,

    CR_RED2BLUE,
    CR_RED2GREEN,

    CRMAX,
    CR_NONE,
};

extern byte  *cr[CRMAX];
extern char **crstr;

#define cr_esc '~'

#ifndef CRISPY_TRUECOLOR
extern byte *blendfunc;
extern void V_InitTransMaps (void);
extern void V_LoadTintTable (void);
#endif

int V_GetPaletteIndex(byte *palette, int r, int g, int b);
byte V_Colorize (byte *playpal, int cr, byte source, boolean keepgray109);
