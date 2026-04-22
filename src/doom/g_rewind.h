//
// Copyright(C) 2026 Polina "Aura" N.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//

#pragma once

#include "doomtype.h"

void G_Rewind(void);
void G_SaveAutoKeyframe(void);
void G_LoadAutoKeyframe(void);
void G_ResetRewind(boolean force);
boolean G_RewindIsRestoring(void);
