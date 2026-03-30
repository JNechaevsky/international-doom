//
// Copyright(C) 2026 Polina "Aura" N.
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

#include "r_local.h"

extern void R_ColLight_ResetLevel(void);
extern void R_ColLight_LoadMapLUT(const char *map_name);
extern void R_ColLight_RebuildBanks(void);
extern lighttable_t *R_ColLight_Apply(int bank_index, const lighttable_t *base_colormap);
