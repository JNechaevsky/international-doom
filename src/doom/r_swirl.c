//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2000, 2005-2014 Simon Howard
// Copyright(C) 2019 Fabian Greffrath
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
//	[crispy] add support for SMMU swirling flats
//

// [crispy] adapted from smmu/r_ripple.c, by Simon Howard

#include "tables.h"
#include "i_system.h"
#include "w_wad.h"
#include "z_zone.h"
#include "doomstat.h"


// swirl factors
#define SWIRLFACTOR  (FINEANGLES / 64)  // [PN] Defines the number of waves per 64 units.
#define SWIRLFACTOR2 (FINEANGLES / 32)  // [PN] Defines the number of waves per 32 units.

#define SEQUENCE 256                    // [PN] Number of frames in the distortion sequence.
#define FLATSIZE (64 * 64)              // [PN] Flat texture size (64x64 pixels).

#define AMP 2                           // [PN] Amplitude for the primary distortion.
#define AMP2 2                          // [PN] Amplitude for the secondary distortion.
#define SPEED 32                        // [PN] Speed of the wave distortion.

static int *offsets = NULL;             // [PN] Array to store offsets for all frames.
static int *offset;                     // [PN] Current pointer to offsets for a specific frame.


// [PN] Helper function to calculate the offset based on sine wave values.
static inline int calculate_offset (int x, int y, int i, int factor, int factor2, int amp, int amp2, int speed) 
{
    const int sinvalue = (y * factor + i * speed * 5 + 900) & FINEMASK;
    const int sinvalue2 = (x * factor2 + i * speed * 4 + 300) & FINEMASK;
    const int result = x + 128 + ((finesine[sinvalue] * amp) >> FRACBITS) 
                     + ((finesine[sinvalue2] * amp2) >> FRACBITS);

    // [PN] Ensuring the result wraps around within the 64x64 grid.
    return result & 63;
}

void R_InitDistortedFlats (void)
{
	if (!offsets)
	{
		offsets = I_Realloc(offsets, SEQUENCE * FLATSIZE * sizeof(*offsets));
		offset = offsets;

		for (int i = 0; i < SEQUENCE; i++)
		{
			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
                    // [PN] Calculate X distortion.
                    const int x1 = calculate_offset(x, y, i, SWIRLFACTOR, SWIRLFACTOR2, AMP, AMP2, SPEED);
                    // [PN] Calculate Y distortion (swapped x and y).                    
                    const int y1 = calculate_offset(y, x, i, SWIRLFACTOR, SWIRLFACTOR2, AMP, AMP2, SPEED);

					offset[(y << 6) + x] = (y1 << 6) + x1;
				}
			}

			offset += FLATSIZE;
		}
	}
}

byte *R_DistortedFlat(int flatnum)
{
	static int swirltic = -1;
	static int swirlflat = -1;
	static char distortedflat[FLATSIZE];

	if (swirltic != leveltime)
	{
		offset = offsets + ((leveltime & (SEQUENCE - 1)) * FLATSIZE);

		swirltic = leveltime;
		swirlflat = -1;
	}

	if (swirlflat != flatnum)
	{
		char *normalflat;

		normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

        // [PN] Loop through each pixel and apply the distortion.
		for (int i = 0; i < FLATSIZE; i++)
		{
			distortedflat[i] = normalflat[offset[i]];
		}

		W_ReleaseLumpNum(flatnum);

		swirlflat = flatnum;
	}

	return distortedflat;
}
