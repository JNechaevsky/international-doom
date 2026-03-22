//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2013-2017 Brad Harding
// Copyright(C) 2017 Fabian Greffrath
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
// DESCRIPTION:
//	Brightmap textures and flats lookup routine.
//


#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "doomstat.h"
#include "m_array.h"
#include "m_misc.h"
#include "r_local.h"
#include "w_wad.h"
#include "z_zone.h"

#include "id_vars.h"


// -----------------------------------------------------------------------------
// [crispy] brightmap data
// -----------------------------------------------------------------------------

static const byte nobrightmap[256] = {0};

static const byte fullbright[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static const byte notgray[256] =
{
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static const byte notgrayorbrown[256] =
{
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static const byte notgrayorbrown2[256] =
{
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static const byte bluegreenbrownred[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte bluegreenbrown[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte blueandorange[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte redonly[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte redonly2[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte redandgreen[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte greenonly1[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte greenonly2[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte greenonly3[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte yellowonly[256] =
{
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
};

static const byte blueandgreen[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte brighttan[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0,
    1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
    0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const byte candle[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
};

static const byte light_sources[256] =
{
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};

const byte *dc_brightmap = nobrightmap;

// -----------------------------------------------------------------------------
// [crispy] brightmaps for textures
// -----------------------------------------------------------------------------

enum
{
    DOOM1AND2,
    DOOM1ONLY,
    DOOM2ONLY,
};

typedef enum
{
    BMTOK_EOF = 0,
    BMTOK_IDENT,
    BMTOK_NUMBER,
    BMTOK_COMMA,
    BMTOK_DASH,
    BMTOK_PIPE,
    BMTOK_INVALID
} bmap_token_type_t;

typedef struct
{
    bmap_token_type_t type;
    char text[64];
    int number;
} bmap_token_t;

typedef struct
{
    const char *cur;
    const char *end;
    boolean has_unget;
    bmap_token_t unget;
} bmap_parser_t;

typedef struct
{
    char name[64];
    byte colormask[256];
} parsed_brightmap_t;

typedef struct
{
    char texture[9];
    int brightmap;
} parsed_texture_bmap_t;

typedef struct
{
    int sprite;
    int brightmap;
} parsed_sprite_bmap_t;

typedef struct
{
    int flat;
    int brightmap;
} parsed_flat_bmap_t;

typedef struct
{
    int state;
    int brightmap;
} parsed_state_bmap_t;

static parsed_brightmap_t *parsed_brightmaps;
static parsed_texture_bmap_t *parsed_texture_bmaps;
static parsed_sprite_bmap_t *parsed_sprite_bmaps;
static parsed_flat_bmap_t *parsed_flat_bmaps;
static parsed_state_bmap_t *parsed_state_bmaps;

// [PN] Converts a name to uppercase and truncates it to the destination size.
static void BM_NormalizeName(char *dest, const size_t size, const char *src)
{
    size_t i = 0;

    if (size == 0)
    {
        return;
    }

    while (i + 1 < size && src[i] != '\0')
    {
        dest[i] = toupper((unsigned char) src[i]);
        i++;
    }

    dest[i] = '\0';
}

// [PN] Prints a warning when a BRGHTMPS entry references a missing brightmap.
static void BM_ReportMissingBrightmap(const char *name)
{
    fprintf(stderr, "R_ParseBrightmaps: brightmap '%s' not found\n", name);
}

// [PN] Checks if a BRGHTMPS game filter excludes the current game mission.
static boolean BM_IsGameMismatch(const int game)
{
    return (gamemission == doom && game == DOOM2ONLY)
        || (gamemission != doom && game == DOOM1ONLY);
}

// [PN] Clears all parsed BRGHTMPS override arrays.
static void BM_ClearParsedBrightmaps(void)
{
    array_free(parsed_brightmaps);
    array_free(parsed_texture_bmaps);
    array_free(parsed_sprite_bmaps);
    array_free(parsed_flat_bmaps);
    array_free(parsed_state_bmaps);
}

// [PN] Finds a parsed BRGHTMPS brightmap definition by name.
static int BM_FindParsedBrightmap(const char *name)
{
    char upper_name[64];

    BM_NormalizeName(upper_name, sizeof(upper_name), name);

    for (int i = array_size(parsed_brightmaps) - 1; i >= 0; --i)
    {
        if (!strcasecmp(parsed_brightmaps[i].name, upper_name))
        {
            return i;
        }
    }

    return -1;
}

// [PN] Adds or replaces a named BRGHTMPS brightmap colormask.
static void BM_SetParsedBrightmap(const parsed_brightmap_t *brightmap)
{
    for (int i = array_size(parsed_brightmaps) - 1; i >= 0; --i)
    {
        if (!strcasecmp(parsed_brightmaps[i].name, brightmap->name))
        {
            memcpy(parsed_brightmaps[i].colormask,
                   brightmap->colormask,
                   sizeof(parsed_brightmaps[i].colormask));
            return;
        }
    }

    array_push(parsed_brightmaps, *brightmap);
}

// [PN] Adds or replaces a texture-to-brightmap override.
static void BM_SetParsedTexture(const char texture[9], const int brightmap)
{
    for (int i = array_size(parsed_texture_bmaps) - 1; i >= 0; --i)
    {
        if (!strncasecmp(parsed_texture_bmaps[i].texture, texture, 8))
        {
            parsed_texture_bmaps[i].brightmap = brightmap;
            return;
        }
    }

    parsed_texture_bmap_t entry;

    M_StringCopy(entry.texture, texture, sizeof(entry.texture));
    entry.brightmap = brightmap;
    array_push(parsed_texture_bmaps, entry);
}

// [PN] Adds or replaces a sprite-to-brightmap override.
static void BM_SetParsedSprite(const int sprite, const int brightmap)
{
    for (int i = array_size(parsed_sprite_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_sprite_bmaps[i].sprite == sprite)
        {
            parsed_sprite_bmaps[i].brightmap = brightmap;
            return;
        }
    }

    parsed_sprite_bmap_t entry;

    entry.sprite = sprite;
    entry.brightmap = brightmap;
    array_push(parsed_sprite_bmaps, entry);
}

// [PN] Adds or replaces a flat-to-brightmap override.
static void BM_SetParsedFlat(const int flat, const int brightmap)
{
    for (int i = array_size(parsed_flat_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_flat_bmaps[i].flat == flat)
        {
            parsed_flat_bmaps[i].brightmap = brightmap;
            return;
        }
    }

    parsed_flat_bmap_t entry;

    entry.flat = flat;
    entry.brightmap = brightmap;
    array_push(parsed_flat_bmaps, entry);
}

// [PN] Adds or replaces a state-to-brightmap override.
static void BM_SetParsedState(const int state, const int brightmap)
{
    for (int i = array_size(parsed_state_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_state_bmaps[i].state == state)
        {
            parsed_state_bmaps[i].brightmap = brightmap;
            return;
        }
    }

    parsed_state_bmap_t entry;

    entry.state = state;
    entry.brightmap = brightmap;
    array_push(parsed_state_bmaps, entry);
}

// [PN] Returns an override brightmap index for a texture name, if present.
static int BM_FindTextureBrightmap(const char *texname)
{
    for (int i = array_size(parsed_texture_bmaps) - 1; i >= 0; --i)
    {
        if (!strncasecmp(parsed_texture_bmaps[i].texture, texname, 8))
        {
            return parsed_texture_bmaps[i].brightmap;
        }
    }

    return -1;
}

// [PN] Returns an override brightmap index for a sprite, if present.
static int BM_FindSpriteBrightmap(const int sprite)
{
    for (int i = array_size(parsed_sprite_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_sprite_bmaps[i].sprite == sprite)
        {
            return parsed_sprite_bmaps[i].brightmap;
        }
    }

    return -1;
}

// [PN] Returns an override brightmap index for a flat, if present.
static int BM_FindFlatBrightmap(const int flat)
{
    for (int i = array_size(parsed_flat_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_flat_bmaps[i].flat == flat)
        {
            return parsed_flat_bmaps[i].brightmap;
        }
    }

    return -1;
}

// [PN] Returns an override brightmap index for a state, if present.
static int BM_FindStateBrightmap(const int state)
{
    for (int i = array_size(parsed_state_bmaps) - 1; i >= 0; --i)
    {
        if (parsed_state_bmaps[i].state == state)
        {
            return parsed_state_bmaps[i].brightmap;
        }
    }

    return -1;
}

// [PN] Skips whitespace and line/block comments in BRGHTMPS text.
static void BM_SkipSpaceAndComments(bmap_parser_t *parser)
{
    while (parser->cur < parser->end)
    {
        const unsigned char c = (unsigned char) *parser->cur;

        if (isspace(c))
        {
            parser->cur++;
            continue;
        }

        if (c == ';')
        {
            while (parser->cur < parser->end && *parser->cur != '\n')
            {
                parser->cur++;
            }
            continue;
        }

        if (c == '/' && parser->cur + 1 < parser->end)
        {
            if (parser->cur[1] == '/')
            {
                parser->cur += 2;

                while (parser->cur < parser->end && *parser->cur != '\n')
                {
                    parser->cur++;
                }
                continue;
            }

            if (parser->cur[1] == '*')
            {
                parser->cur += 2;

                while (parser->cur + 1 < parser->end
                    && !(parser->cur[0] == '*' && parser->cur[1] == '/'))
                {
                    parser->cur++;
                }

                if (parser->cur + 1 < parser->end)
                {
                    parser->cur += 2;
                }
                else
                {
                    parser->cur = parser->end;
                }
                continue;
            }
        }

        break;
    }
}

// [PN] Pushes one token back so it can be read again by the parser.
static void BM_UngetToken(bmap_parser_t *parser, const bmap_token_t *token)
{
    parser->unget = *token;
    parser->has_unget = true;
}

// [PN] Reads the next BRGHTMPS token from the parser stream.
static void BM_GetToken(bmap_parser_t *parser, bmap_token_t *token)
{
    memset(token, 0, sizeof(*token));

    if (parser->has_unget)
    {
        *token = parser->unget;
        parser->has_unget = false;
        return;
    }

    BM_SkipSpaceAndComments(parser);

    if (parser->cur >= parser->end)
    {
        token->type = BMTOK_EOF;
        return;
    }

    switch (*parser->cur)
    {
        case ',':
            token->type = BMTOK_COMMA;
            parser->cur++;
            return;

        case '-':
            token->type = BMTOK_DASH;
            parser->cur++;
            return;

        case '|':
            token->type = BMTOK_PIPE;
            parser->cur++;
            return;
    }

    if (*parser->cur == '"')
    {
        size_t len = 0;

        parser->cur++;

        while (parser->cur < parser->end && *parser->cur != '"')
        {
            if (len + 1 < sizeof(token->text))
            {
                token->text[len++] = *parser->cur;
            }
            parser->cur++;
        }

        if (parser->cur < parser->end && *parser->cur == '"')
        {
            parser->cur++;
        }

        token->text[len] = '\0';
        token->type = BMTOK_IDENT;
        return;
    }

    if (isdigit((unsigned char) *parser->cur))
    {
        int value = 0;

        while (parser->cur < parser->end
            && isdigit((unsigned char) *parser->cur))
        {
            value = value * 10 + (*parser->cur - '0');
            parser->cur++;
        }

        token->type = BMTOK_NUMBER;
        token->number = value;
        return;
    }

    {
        size_t len = 0;

        while (parser->cur < parser->end)
        {
            const char c = *parser->cur;

            if (isspace((unsigned char) c)
             || c == ','
             || c == '-'
             || c == '|'
             || c == '"'
             || c == ';')
            {
                break;
            }

            if (c == '/' && parser->cur + 1 < parser->end
             && (parser->cur[1] == '/' || parser->cur[1] == '*'))
            {
                break;
            }

            if (len + 1 < sizeof(token->text))
            {
                token->text[len++] = c;
            }

            parser->cur++;
        }

        if (len > 0)
        {
            token->text[len] = '\0';
            token->type = BMTOK_IDENT;
        }
        else
        {
            token->type = BMTOK_INVALID;
            parser->cur++;
        }
    }
}

// [PN] Reads an identifier token into a name buffer.
static boolean BM_GetNameToken(bmap_parser_t *parser, char *name, const size_t size)
{
    bmap_token_t token;

    BM_GetToken(parser, &token);

    if (token.type != BMTOK_IDENT)
    {
        return false;
    }

    M_StringCopy(name, token.text, size);
    return true;
}

// [PN] Reads an integer token value from the parser.
static boolean BM_GetNumberToken(bmap_parser_t *parser, int *value)
{
    bmap_token_t token;

    BM_GetToken(parser, &token);

    if (token.type != BMTOK_NUMBER)
    {
        return false;
    }

    *value = token.number;
    return true;
}

// [PN] Parses a color list/ranges into a 256-byte brightmap mask.
static boolean BM_ReadColormask(bmap_parser_t *parser, byte *colormask)
{
    bmap_token_t token;
    int color1;

    memset(colormask, 0, 256);

    if (!BM_GetNumberToken(parser, &color1))
    {
        return false;
    }

    while (1)
    {
        int color2 = color1;
        int range_from, range_to;

        BM_GetToken(parser, &token);

        if (token.type == BMTOK_DASH)
        {
            if (!BM_GetNumberToken(parser, &color2))
            {
                return false;
            }

            BM_GetToken(parser, &token);
        }

        range_from = color1;
        range_to = color2;

        if (range_from > range_to)
        {
            const int temp = range_from;
            range_from = range_to;
            range_to = temp;
        }

        if (range_from < 0)
        {
            range_from = 0;
        }

        if (range_to > 255)
        {
            range_to = 255;
        }

        for (int i = range_from; i <= range_to; ++i)
        {
            colormask[i] = 1;
        }

        if (token.type != BMTOK_COMMA)
        {
            BM_UngetToken(parser, &token);
            break;
        }

        if (!BM_GetNumberToken(parser, &color1))
        {
            return false;
        }
    }

    return true;
}

// [PN] Parses a brightmap reference with optional game filter.
static int BM_ParseProperty(bmap_parser_t *parser)
{
    bmap_token_t token;
    char brightmap_name[64];
    int game = DOOM1AND2;

    if (!BM_GetNameToken(parser, brightmap_name, sizeof(brightmap_name)))
    {
        return -1;
    }

    const int brightmap = BM_FindParsedBrightmap(brightmap_name);

    if (brightmap < 0)
    {
        BM_ReportMissingBrightmap(brightmap_name);
        return -1;
    }

    BM_GetToken(parser, &token);

    if (token.type != BMTOK_IDENT)
    {
        BM_UngetToken(parser, &token);
    }
    else if (!strcasecmp(token.text, "DOOM") || !strcasecmp(token.text, "DOOM1"))
    {
        game = DOOM1ONLY;

        BM_GetToken(parser, &token);

        if (token.type == BMTOK_PIPE)
        {
            BM_GetToken(parser, &token);

            if (token.type == BMTOK_IDENT && !strcasecmp(token.text, "DOOM2"))
            {
                game = DOOM1AND2;
            }
            else
            {
                BM_UngetToken(parser, &token);
            }
        }
        else
        {
            BM_UngetToken(parser, &token);
        }
    }
    else if (!strcasecmp(token.text, "DOOM2"))
    {
        game = DOOM2ONLY;
    }
    else
    {
        BM_UngetToken(parser, &token);
    }

    return BM_IsGameMismatch(game) ? -1 : brightmap;
}

// [PN] Parses one BRGHTMPS lump and stores overrides in local arrays.
static void BM_ParseBrightmapsLump(const int lumpnum)
{
    bmap_parser_t parser;
    bmap_token_t token;

    parser.cur = W_CacheLumpNum(lumpnum, PU_CACHE);
    parser.end = parser.cur + W_LumpLength(lumpnum);
    parser.has_unget = false;

    while (1)
    {
        BM_GetToken(&parser, &token);

        if (token.type == BMTOK_EOF)
        {
            break;
        }

        if (token.type != BMTOK_IDENT)
        {
            continue;
        }

        if (!strcasecmp(token.text, "BRIGHTMAP"))
        {
            parsed_brightmap_t brightmap;

            if (!BM_GetNameToken(&parser, brightmap.name, sizeof(brightmap.name)))
            {
                continue;
            }

            BM_NormalizeName(brightmap.name, sizeof(brightmap.name), brightmap.name);

            if (!BM_ReadColormask(&parser, brightmap.colormask))
            {
                continue;
            }

            BM_SetParsedBrightmap(&brightmap);
        }
        else if (!strcasecmp(token.text, "TEXTURE"))
        {
            char texture_name[64];
            char texture_name8[9];

            if (!BM_GetNameToken(&parser, texture_name, sizeof(texture_name)))
            {
                continue;
            }

            BM_NormalizeName(texture_name8, sizeof(texture_name8), texture_name);

            const int brightmap = BM_ParseProperty(&parser);

            if (brightmap >= 0)
            {
                BM_SetParsedTexture(texture_name8, brightmap);
            }
        }
        else if (!strcasecmp(token.text, "SPRITE"))
        {
            char sprite_name[64];

            if (!BM_GetNameToken(&parser, sprite_name, sizeof(sprite_name)))
            {
                continue;
            }

            BM_NormalizeName(sprite_name, sizeof(sprite_name), sprite_name);

            const int brightmap = BM_ParseProperty(&parser);

            if (brightmap >= 0)
            {
                for (int i = 0; i < NUMSPRITES; ++i)
                {
                    if (!strcasecmp(sprite_name, sprnames[i]))
                    {
                        BM_SetParsedSprite(i, brightmap);
                        break;
                    }
                }
            }
        }
        else if (!strcasecmp(token.text, "FLAT"))
        {
            char flat_name[64];
            char flat_name8[9];

            if (!BM_GetNameToken(&parser, flat_name, sizeof(flat_name)))
            {
                continue;
            }

            BM_NormalizeName(flat_name8, sizeof(flat_name8), flat_name);
            const int brightmap = BM_ParseProperty(&parser);

            if (brightmap >= 0 && numflats > 0)
            {
                const int flat_lump_end = firstflat + numflats - 1;
                const int lump = W_CheckNumForNameFromTo(flat_name8, flat_lump_end, firstflat);

                if (lump >= 0)
                {
                    BM_SetParsedFlat(lump - firstflat, brightmap);
                }
            }
        }
        else if (!strcasecmp(token.text, "STATE"))
        {
            int state;
            char brightmap_name[64];

            if (!BM_GetNumberToken(&parser, &state))
            {
                continue;
            }

            if (!BM_GetNameToken(&parser, brightmap_name, sizeof(brightmap_name)))
            {
                continue;
            }

            const int brightmap = BM_FindParsedBrightmap(brightmap_name);

            if (brightmap < 0)
            {
                BM_ReportMissingBrightmap(brightmap_name);
                continue;
            }

            if (state >= 0 && state < NUMSTATES)
            {
                BM_SetParsedState(state, brightmap);
            }
        }
    }
}

// [PN] Parses all BRGHTMPS lumps in load order, with later lumps overriding earlier ones.
static void BM_ParseAllBrightmapsLumps(void)
{
    BM_ClearParsedBrightmaps();

    for (unsigned int i = 0; i < numlumps; ++i)
    {
        if (!strncasecmp(lumpinfo[i]->name, "BRGHTMPS", 8))
        {
            BM_ParseBrightmapsLump((int)i);
        }
    }
}

typedef struct
{
    const char *const texture;
    const int game;
    const byte *colormask;
} fullbright_t;

static const fullbright_t fullbright_walls[] = {
    // [crispy] common textures
    {"COMP2",    DOOM1AND2, blueandgreen},
    {"COMPSTA1", DOOM1AND2, notgray},
    {"COMPSTA2", DOOM1AND2, notgray},
    {"COMPUTE1", DOOM1AND2, bluegreenbrownred},
    {"COMPUTE2", DOOM1AND2, bluegreenbrown},
    {"COMPUTE3", DOOM1AND2, blueandorange},
    {"EXITSIGN", DOOM1AND2, notgray},
    {"EXITSTON", DOOM1AND2, redonly},
    {"METAL3",   DOOM2ONLY, redonly},
    {"PLANET1",  DOOM1AND2, notgray},
    {"SILVER2",  DOOM1AND2, notgray},
    {"SILVER3",  DOOM1AND2, notgrayorbrown2},
    {"SLADSKUL", DOOM1AND2, redonly},
    {"SW1BRCOM", DOOM1AND2, redonly},
    {"SW1BRIK",  DOOM1AND2, redonly},
    {"SW1BRN1",  DOOM2ONLY, redonly},
    {"SW1COMM",  DOOM1AND2, redonly},
    {"SW1DIRT",  DOOM1AND2, redonly},
    {"SW1MET2",  DOOM1AND2, redonly},
    {"SW1STARG", DOOM2ONLY, redonly},
    {"SW1STON1", DOOM1AND2, redonly},
    {"SW1STON2", DOOM2ONLY, redonly},
    {"SW1STONE", DOOM1AND2, redonly},
    {"SW1STRTN", DOOM1AND2, redonly},
    {"SW2BLUE",  DOOM1AND2, redonly},
    {"SW2BRCOM", DOOM1AND2, greenonly2},
    {"SW2BRIK",  DOOM1AND2, greenonly1},
    {"SW2BRN1",  DOOM1AND2, greenonly2},
    {"SW2BRN2",  DOOM1AND2, greenonly1},
    {"SW2BRNGN", DOOM1AND2, greenonly3},
    {"SW2COMM",  DOOM1AND2, greenonly1},
    {"SW2COMP",  DOOM1AND2, redonly},
    {"SW2DIRT",  DOOM1AND2, greenonly2},
    {"SW2EXIT",  DOOM1AND2, notgray},
    {"SW2GRAY",  DOOM1AND2, notgray},
    {"SW2GRAY1", DOOM1AND2, notgray},
    {"SW2GSTON", DOOM1AND2, redonly},
    // [JN] Special case: fewer colors lit.
    {"SW2HOT",   DOOM1AND2, redonly2},
    {"SW2MARB",  DOOM2ONLY, redonly},
    {"SW2MET2",  DOOM1AND2, greenonly1},
    {"SW2METAL", DOOM1AND2, greenonly3},
    {"SW2MOD1",  DOOM1AND2, greenonly1},
    {"SW2PANEL", DOOM1AND2, redonly},
    {"SW2ROCK",  DOOM1AND2, redonly},
    {"SW2SLAD",  DOOM1AND2, redonly},
    {"SW2STARG", DOOM2ONLY, greenonly2},
    {"SW2STON1", DOOM1AND2, greenonly3},
    // [crispy] beware!
    {"SW2STON2", DOOM1ONLY, redonly},
    {"SW2STON2", DOOM2ONLY, greenonly2},
    {"SW2STON6", DOOM1AND2, redonly},
    // [JN] beware!
    {"SW2STONE", DOOM1ONLY, greenonly1},
    {"SW2STONE", DOOM2ONLY, greenonly2},
    {"SW2STRTN", DOOM1AND2, greenonly1},
    {"SW2TEK",   DOOM1AND2, greenonly1},
    {"SW2VINE",  DOOM1AND2, greenonly1},
    {"SW2WOOD",  DOOM1AND2, redonly},
    {"SW2ZIM",   DOOM1AND2, redonly},
    {"WOOD4",    DOOM1AND2, redonly},
    {"WOODGARG", DOOM1AND2, redonly},
    {"WOODSKUL", DOOM1AND2, redonly},
//  {"ZELDOOR",  DOOM1AND2, redonly},
    {"LITEBLU1", DOOM1AND2, notgray},
    {"LITEBLU2", DOOM1AND2, notgray},
    {"SPCDOOR3", DOOM2ONLY, greenonly1},
    {"PIPEWAL1", DOOM2ONLY, greenonly1},
    {"TEKLITE2", DOOM2ONLY, greenonly1},
    {"TEKBRON2", DOOM2ONLY, yellowonly},
    {"TEKWALL2", DOOM1ONLY, redonly},
    {"TEKWALL5", DOOM1ONLY, redonly},
    // [JN] Green in Doom 2, red in Plutonia.
    {"SW2SKULL", DOOM2ONLY, redandgreen},
    {"SW2SATYR", DOOM1AND2, brighttan},
    {"SW2LION",  DOOM1AND2, brighttan},
    {"SW2GARG",  DOOM1AND2, brighttan},
    // [JN] SIGIL I
    {"SIGIL",    DOOM1ONLY, redonly},
};

static const fullbright_t fullbright_finaldoom[] = {
    // [crispy] Final Doom textures
    // TNT - Evilution exclusive
    {"PNK4EXIT", DOOM2ONLY, redonly},
    {"SLAD2",    DOOM2ONLY, notgrayorbrown},
    {"SLAD3",    DOOM2ONLY, notgrayorbrown},
    {"SLAD4",    DOOM2ONLY, notgrayorbrown},
    {"SLAD5",    DOOM2ONLY, notgrayorbrown},
    {"SLAD6",    DOOM2ONLY, notgrayorbrown},
    {"SLAD7",    DOOM2ONLY, notgrayorbrown},
    {"SLAD8",    DOOM2ONLY, notgrayorbrown},
    {"SLAD9",    DOOM2ONLY, notgrayorbrown},
    {"SLAD10",   DOOM2ONLY, notgrayorbrown},
    {"SLAD11",   DOOM2ONLY, notgrayorbrown},
    {"SLADRIP1", DOOM2ONLY, notgrayorbrown},
    {"SLADRIP3", DOOM2ONLY, notgrayorbrown},
    {"M_TEC",    DOOM2ONLY, greenonly2},
    {"LITERED2", DOOM2ONLY, redonly},
    {"BTNTMETL", DOOM2ONLY, notgrayorbrown},
    {"BTNTSLVR", DOOM2ONLY, notgrayorbrown},
    {"LITEYEL2", DOOM2ONLY, yellowonly},
    {"LITEYEL3", DOOM2ONLY, yellowonly},
    {"YELMETAL", DOOM2ONLY, yellowonly},
    // Plutonia exclusive
//  {"SW2SKULL", DOOM2ONLY, redonly},
    
};

const byte *R_BrightmapForTexName (const char *texname)
{
    int i;
    int brightmap = BM_FindTextureBrightmap(texname);

    if (brightmap >= 0)
    {
        return parsed_brightmaps[brightmap].colormask;
    }

    for (i = 0; (size_t)i < arrlen(fullbright_walls); i++)
    {
        const fullbright_t *const fullbright_tex = &fullbright_walls[i];

        if ((gamemission == doom && fullbright_tex->game == DOOM2ONLY)
        ||  (gamemission != doom && fullbright_tex->game == DOOM1ONLY))
        {
            continue;
        }

        if (!strncasecmp(fullbright_tex->texture, texname, 8))
        {
            return fullbright_tex->colormask;
        }
    }

    // Final Doom: Plutonia has no exclusive brightmaps
    if (gamemission == pack_tnt /* || gamemission == pack_plut */ )
    {
        for (i = 0; (size_t)i < arrlen(fullbright_finaldoom); i++)
        {
            const fullbright_t *const fullbright_tex = &fullbright_finaldoom[i];

            if (!strncasecmp(fullbright_tex->texture, texname, 8))
            {
                return fullbright_tex->colormask;
            }
        }
    }

    return nobrightmap;
}

// -----------------------------------------------------------------------------
// [crispy] brightmaps for sprites
// -----------------------------------------------------------------------------

const byte *R_BrightmapForSprite (const int type)
{
    if (vis_brightmaps)
    {
        const int brightmap = BM_FindSpriteBrightmap(type);

        if (brightmap >= 0)
        {
            return parsed_brightmaps[brightmap].colormask;
        }

        switch (type)
        {
            // Armor Bonus
            case SPR_BON2:
            {
                return greenonly1;
            }
            // Cell Charge
            case SPR_CELL:
            {
                return greenonly2;
            }
            // Barrel
            case SPR_BAR1:
            {
                return greenonly3;
            }
            // Cell Charge Pack
            case SPR_CELP:
            // Floor Lamp
            case SPR_COLU:
            // Burning Barrel
            case SPR_FCAN:
            {
                return yellowonly;
            }
            // BFG9000
            case SPR_BFUG:
            // Plasmagun
            case SPR_PLAS:
            {
                return redonly;
            }

            // Candlestick
            case SPR_CAND:
            // Candelabra
            case SPR_CBRA:
            {
                return candle;
            }
            // Tall Blue Torch
            case SPR_TBLU:
            // Tall Green Torch
            case SPR_TGRN:
            // Tall Red Torch
            case SPR_TRED:
            // Short Blue Torch
            case SPR_SMBT:
            // Short Green Torch
            case SPR_SMGT:
            // Short Red Torch
            case SPR_SMRT:
            // Tall Technocolumn
            case SPR_TLMP:
            // Short Technocolumn
            case SPR_TLP2:
            {
                return light_sources;
            }
            // Evil Eye
            case SPR_CEYE:
            {
                return greenonly1;
            }
            // Floating Skull Rock
            case SPR_FSKU:
            // Pile of Skulls and Candles
            case SPR_POL3:
            {
                return fullbright;
            }
        }
    }
    else
    {
        switch (type)
        {
            case SPR_FCAN:
            case SPR_CAND:
            case SPR_CEYE:
            case SPR_FSKU:
            case SPR_CBRA:
            case SPR_COLU:
            case SPR_TLMP:
            case SPR_TLP2:
            case SPR_TBLU:
            case SPR_TGRN:
            case SPR_TRED:
            case SPR_SMBT:
            case SPR_SMGT:
            case SPR_SMRT:
            {
                return fullbright;
            }
        }
    }

    return nobrightmap;
}

// -----------------------------------------------------------------------------
// [crispy] brightmaps for flats
// -----------------------------------------------------------------------------

static int bmapflatnum[3];

const byte *R_BrightmapForFlatNum (const int num)
{
    if (vis_brightmaps)
    {
        const int brightmap = BM_FindFlatBrightmap(num);

        if (brightmap >= 0)
        {
            return parsed_brightmaps[brightmap].colormask;
        }

        if (num == bmapflatnum[0]
        ||  num == bmapflatnum[1]
        ||  num == bmapflatnum[2])
        {
            return notgrayorbrown;
        }
    }

    return nobrightmap;
}

// -----------------------------------------------------------------------------
// [crispy] brightmaps for states
// -----------------------------------------------------------------------------

const byte *R_BrightmapForState (const int state)
{
    if (vis_brightmaps)
    {
        const int brightmap = BM_FindStateBrightmap(state);

        if (brightmap >= 0)
        {
            return parsed_brightmaps[brightmap].colormask;
        }

        switch (state)
        {
            case S_BFG1:
            case S_BFG2:
            case S_BFG3:
            case S_BFG4:
            {
                return redonly;
            }
        }
	}

    return nobrightmap;
}

// -----------------------------------------------------------------------------
// [crispy] initialize brightmaps
// -----------------------------------------------------------------------------

void R_InitBrightmaps (void)
{
    // [PN] Parse BRGHTMPS overrides first (if present), keep hardcoded maps as fallback.
    BM_ParseAllBrightmapsLumps();

    // [crispy] only three select brightmapped flats
    bmapflatnum[0] = R_FlatNumForName("CONS1_1");
    bmapflatnum[1] = R_FlatNumForName("CONS1_5");
    bmapflatnum[2] = R_FlatNumForName("CONS1_7");
}
