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

#ifndef HEXEN_R_COLLIGHT_H
#define HEXEN_R_COLLIGHT_H

#include "r_local.h"

// -----------------------------------------------------------------------------
// R_ColLight_ResetLevel
// [PN] Clears per-level sector color-lighting state and frees cached LUT banks.
// -----------------------------------------------------------------------------

void R_ColLight_ResetLevel(void);

// -----------------------------------------------------------------------------
// R_ColLight_LoadMapLUT
// [PN] Parses COLLIGHT lumps and assigns color-lighting banks to map sectors.
// -----------------------------------------------------------------------------

void R_ColLight_LoadMapLUT(const char *map_name);

// -----------------------------------------------------------------------------
// R_ColLight_RebuildBanks
// [PN] Rebuilds cached colored colormap LUT banks from current base colormaps[].
// -----------------------------------------------------------------------------

void R_ColLight_RebuildBanks(void);

// -----------------------------------------------------------------------------
// R_ColLight_Apply
// [PN] Maps a base colormap row pointer to a sector color-light bank row.
// -----------------------------------------------------------------------------

lighttable_t *R_ColLight_Apply(int bank_index, const lighttable_t *base_colormap);

#endif

