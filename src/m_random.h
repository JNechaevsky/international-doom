//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2024-2025 Polina "Aura" N.
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

#pragma once


#include "d_mode.h"  // GameMission_t


// Most damage defined using HITDICE. Used in Heretic and Hexen.
#define HITDICE(a) ((1+(P_Random()&7))*a)

extern int  ID_Random (void);
extern int  ID_RealRandom (void);
extern int  ID_SubRandom (void);
extern int  M_Random (void);
extern int  P_Random (void);
extern int  P_SubRandom (void);
extern void M_ClearRandom (void);
extern void M_InitRandom (GameMission_t mission);

extern int  m_rndindex;
