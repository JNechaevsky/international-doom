//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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

#include "doomdef.h"


#define CT_KEY_GREEN    'g'
#define CT_KEY_YELLOW   'y'
#define CT_KEY_RED      'r'
#define CT_KEY_BLUE     'b'
#define CT_KEY_ALL      't'


extern void CT_Drawer (void);
extern void CT_Init (void);
extern void CT_SetMessage (player_t *player, const char *message, boolean ultmsg, byte *table);
extern void CT_SetMessageCentered (player_t *player, const char *message);
extern void CT_Ticker (void);
extern void MSG_Ticker (void);

extern boolean chatmodeon;
extern boolean ultimatemsg;
extern boolean CT_Responder (event_t *ev);

extern char *chat_macros[10];
extern char  CT_dequeueChatChar(void);
extern const char *lastmessage;

extern int   showMessages;
