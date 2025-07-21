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
    CR_RED_BRIGHT,

    CR_DARKRED,

    CR_GREEN,
    CR_GREEN_BRIGHT,

    CR_GREEN_HX,
    CR_GREEN_HX_BRIGHT,

    CR_DARKGREEN,
    CR_DARKGREEN_BRIGHT,

    CR_OLIVE,
    CR_OLIVE_BRIGHT,

    CR_BLUE2,
    CR_BLUE2_BRIGHT,

    CR_YELLOW,
    CR_YELLOW_BRIGHT,

    CR_ORANGE,
    CR_ORANGE_BRIGHT,

    CR_ORANGE_HR,
    CR_ORANGE_HR_BRIGHT,

    CR_WHITE,

    CR_GRAY,
    CR_GRAY_BRIGHT,

    CR_LIGHTGRAY,
    CR_LIGHTGRAY_BRIGHT,
    CR_LIGHTGRAY_DARK,

    CR_BROWN,
    CR_FLAME,
    CR_PURPLE,

    CR_RED2BLUE,
    CR_RED2GREEN,

    CRMAX,
    CR_NONE,
};

extern byte  *cr[CRMAX];
extern char **crstr;

#define cr_esc '~'

int V_GetPaletteIndex(const byte *palette, int r, int g, int b);
byte V_Colorize (byte *playpal, int cr, byte source, boolean keepgray109);
