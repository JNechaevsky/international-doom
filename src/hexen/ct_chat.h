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
//
// Chat mode stuff
//

#pragma once


#define CT_KEY_GREEN	'g'
#define CT_KEY_YELLOW	'y'
#define CT_KEY_RED		'r'
#define CT_KEY_BLUE		'b'
#define CT_KEY_ALL		't'

extern char *chat_macros[10];

extern void CT_SetMessage (player_t *player, const char *message, boolean ultmsg, byte *table);
extern void CT_SetYellowMessage (player_t *player, const char *message, boolean ultmsg);
extern void CT_ClearMessage (player_t *player);
extern void MSG_Ticker (void);
