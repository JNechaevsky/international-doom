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
#include "h2def.h"


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
static int *offset_frames[SEQUENCE];    // [PN] Array of pointers to frame offsets.


void R_InitDistortedFlats (void)
{
	if (!offsets)
	{
		offsets = I_Realloc(offsets, SEQUENCE * FLATSIZE * sizeof(*offsets));
		offset = offsets;

		for (int i = 0; i < SEQUENCE; i++)
		{
			offset_frames[i] = offsets + i * FLATSIZE;

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
                    // [PN] Calculate X distortion.
                    const int sin1 = (y * SWIRLFACTOR + i * SPEED * 5 + 900) & FINEMASK;
                    const int sin2 = (x * SWIRLFACTOR2 + i * SPEED * 4 + 300) & FINEMASK;

                    int x1 = x + 128 + ((finesine[sin1] * AMP) >> FRACBITS)
                                     + ((finesine[sin2] * AMP2) >> FRACBITS);
                    x1 &= 63;

                    // [PN] Calculate Y distortion (swapped x and y).                    
                    const int sin3 = (x * SWIRLFACTOR + i * SPEED * 5 + 900) & FINEMASK;
                    const int sin4 = (y * SWIRLFACTOR2 + i * SPEED * 4 + 300) & FINEMASK;

                    int y1 = y + 128 + ((finesine[sin3] * AMP) >> FRACBITS)
                                     + ((finesine[sin4] * AMP2) >> FRACBITS);
                    y1 &= 63;

					offset_frames[i][(y << 6) + x] = (y1 << 6) + x1;
				}
			}
		}
	}
}

byte *R_DistortedFlat(int flatnum)
{
	static int swirltic = -1;
	static int swirlflat = -1;
	static byte distortedflat[FLATSIZE];

	if (swirltic != leveltime)
	{
		offset = offset_frames[leveltime & (SEQUENCE - 1)];

		swirltic = leveltime;
		swirlflat = -1;
	}

	if (swirlflat != flatnum)
	{
		const char *normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

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
