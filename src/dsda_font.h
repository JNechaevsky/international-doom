//
// Copyright(C) 2023 by Ryan Krafnick
// Copyright(C) 2018-2026 Julia Nechaevskaya
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
// DESCRIPTION:
//  DSDA Font, embedded into CRL executable.
//

#include "doomtype.h"


extern const byte *const dsda_font_lut[];

extern void DSDA_FontInit(void);
extern void DSDA_DrawText(int x, int y, const char *const text, byte *const table);
extern void DSDA_DrawTextCentered(int y, const char *const text, byte *const table);
extern int  DSDA_StringWidth(const char *const string);
