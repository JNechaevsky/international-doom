//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#pragma once

#include "d_event.h"
#include "d_player.h"
#include "doomdef.h"
#include "v_patch.h"


#define HU_FONTSTART    '!'	// the first font characters
#define HU_FONTEND      '_'	// the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE (HU_FONTEND - HU_FONTSTART + 1)	

// Message timeout.
#define MESSAGETICS (TICRATE*4)

extern void CT_Drawer (void);
extern void CT_Init (void);
extern void CT_SetMessage (player_t *player, const char *message, boolean ultmsg, byte *table);
extern void CT_SetMessageCentered (player_t *player, const char *message, byte *table);
extern void CT_Ticker (void);
extern void MSG_Ticker (void);

extern boolean ultimatemsg;
extern boolean chatmodeon;
extern boolean CT_Responder(event_t * ev);

extern char *chat_macros[10];
extern char *player_names[];
extern char  CT_dequeueChatChar (void);
extern const char *lastmessage;

extern int   showMessages;

extern patch_t *hu_font[HU_FONTSIZE];


