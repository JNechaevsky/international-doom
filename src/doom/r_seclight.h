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

#ifndef DOOM_R_SECLIGHT_H
#define DOOM_R_SECLIGHT_H

#include "r_local.h"

// -----------------------------------------------------------------------------
// R_SecLight_ResetLevel
// [PN] Clears per-level sector color-lighting state and frees cached LUT banks.
// -----------------------------------------------------------------------------

void R_SecLight_ResetLevel(void);

// -----------------------------------------------------------------------------
// R_SecLight_LoadMapLUT
// [PN] Parses SECLIGHT lumps and assigns color-lighting banks to map sectors.
// -----------------------------------------------------------------------------

void R_SecLight_LoadMapLUT(const char *map_name);

// -----------------------------------------------------------------------------
// R_SecLight_RebuildBanks
// [PN] Rebuilds cached colored colormap LUT banks from current base colormaps[].
// -----------------------------------------------------------------------------

void R_SecLight_RebuildBanks(void);

// -----------------------------------------------------------------------------
// R_SecLight_Apply
// [PN] Maps a base colormap row pointer to a sector color-light bank row.
// -----------------------------------------------------------------------------

lighttable_t *R_SecLight_Apply(int bank_index, const lighttable_t *base_colormap);

#endif
