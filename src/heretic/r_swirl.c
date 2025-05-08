//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2000, 2005-2014 Simon Howard
// Copyright(C) 2019 Fabian Greffrath
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2025 Julia Nechaevskaya
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
#include "doomdef.h"


// swirl factors
#define SWIRLFACTOR  (FINEANGLES / 64)  // [PN] Defines the number of waves per 64 units.
#define SWIRLFACTOR2 (FINEANGLES / 32)  // [PN] Defines the number of waves per 32 units.

#define SEQUENCE 256                    // [PN] Number of frames in the distortion sequence.
#define FLATSIZE (64 * 64)              // [PN] Flat texture size (64x64 pixels).

#define AMP 2                           // [PN] Amplitude for the primary distortion.
#define AMP2 2                          // [PN] Amplitude for the secondary distortion.
#define SPEED 32                        // [PN] Speed of the wave distortion.

// Classic SMMU swirling effect
static int *offsets = NULL;             // [PN] Array to store offsets for all frames.
static int *offset;                     // [PN] Current pointer to offsets for a specific frame.
static int *offset_frames[SEQUENCE];    // [PN] Array of pointers to frame offsets.

// Warping effect 1
static int *offsets1 = NULL;
static int *offset1;
static int *offset_frames1[SEQUENCE];

// Warping effect 2
static int *offsets2 = NULL;
static int *offset2;
static int *offset_frames2[SEQUENCE];

// Warping effect 3
static int *offsets3 = NULL;
static int *offset3;
static int *offset_frames3[SEQUENCE];

void R_InitDistortedFlats (void)
{
	if (!offsets)
	{
		// Classic SMMU swirling effect.
		// ANIMATED: 65536, internal: -1
		offsets = I_Realloc(offsets, SEQUENCE * FLATSIZE * sizeof(*offsets));
		offset = offsets;

		for (int i = 0; i < SEQUENCE; i++)
		{
			offset_frames[i] = offsets + i * FLATSIZE;

			const int speed5 = i * SPEED * 5;
			const int speed4 = i * SPEED * 4;

			for (int x = 0; x < 64; x++)
			{
				for (int y = 0; y < 64; y++)
				{
                    // [PN] Calculate X distortion.
                    const int sin1 = (y * SWIRLFACTOR + speed5 + 900) & FINEMASK;
                    const int sin2 = (x * SWIRLFACTOR2 + speed4 + 300) & FINEMASK;

                    int x1 = x + 128 + ((finesine[sin1] * AMP) >> FRACBITS)
                                     + ((finesine[sin2] * AMP2) >> FRACBITS);
                    x1 &= 63;

                    // [PN] Calculate Y distortion (swapped x and y).                    
                    const int sin3 = (x * SWIRLFACTOR + speed5 + 900) & FINEMASK;
                    const int sin4 = (y * SWIRLFACTOR2 + speed4 + 300) & FINEMASK;

                    int y1 = y + 128 + ((finesine[sin3] * AMP) >> FRACBITS)
                                     + ((finesine[sin4] * AMP2) >> FRACBITS);
                    y1 &= 63;

					offset_frames[i][(y << 6) + x] = (y1 << 6) + x1;
				}
			}
		}

        // [PN] Warping effect 1, more uniform. Used for slime and blood.
        // ANIMATED: 65537, internal: -2.
        offsets1 = I_Realloc(offsets1, SEQUENCE * FLATSIZE * sizeof(*offsets1));
        offset1 = offsets1;

        for (int i = 0; i < SEQUENCE; i++)
        {
            offset_frames1[i] = offsets1 + i * FLATSIZE;

            const int phase_scale = i * (SPEED * FINEANGLES) / SEQUENCE / 8 /* Slow down factor */;

            for (int y = 0; y < 64; y++)
            {
                for (int x = 0; x < 64; x++)
                {
                    const int u = (x + ((finesine[(y * SWIRLFACTOR + phase_scale) & FINEMASK] * 3) >> FRACBITS)) & 63;
                    const int v = (y + ((finesine[(x * SWIRLFACTOR2 + (phase_scale >> 1)) & FINEMASK] * 3) >> FRACBITS)) & 63;

                    offset_frames1[i][(y << 6) + x] = (v << 6) + u;
                }
            }
        }

        // [JN] Warping effect 2, less uniform. Used for lava.
        // ANIMATED: 65538, internal: -3.
        offsets2 = I_Realloc(offsets2, SEQUENCE * FLATSIZE * sizeof(*offsets2));
        offset2 = offsets2;

        for (int i = 0; i < SEQUENCE; i++)
        {
            offset_frames2[i] = offsets2 + i * FLATSIZE;

            const int speed = i >> 1;

            for (int y = 0; y < 64; y++)
            {
                for (int x = 0; x < 64; x++)
                {
                    const int warp_x = (x + ((finesine[(((y + speed) & 63) << 6) + x] * 8) >> FRACBITS)) & 63;
                    const int warp_y = (y + ((finesine[(((x + speed) & 63) << 6) + y] * 8) >> FRACBITS)) & 63;

                    offset_frames2[i][(y << 6) + x] = (warp_y << 6) + warp_x;
                }
            }
        }

        // [JN] Warping effect 3, weak diagonal effect. Used for sludge.
        // ANIMATED: 65539, internal: -4.
        offsets3 = I_Realloc(offsets3, SEQUENCE * FLATSIZE * sizeof(*offsets3));
        offset3 = offsets3;

        for (int i = 0; i < SEQUENCE; i++)
        {
            offset_frames3[i] = offsets3 + i * FLATSIZE;

            const int speed_term = i * (SPEED << 1);

            for (int x = 0; x < 64; x++)
            {
                for (int y = 0; y < 64; y++)
                {
                    const int wave = ((x + y) * SWIRLFACTOR + speed_term) & FINEMASK;
                    const int dx = (finesine[wave] * AMP) >> FRACBITS;
                    const int dy = (finecosine[wave] * AMP2) >> FRACBITS;
 
                    const int x1 = (x + dx) & 63;
                    const int y1 = (y + dy) & 63;
 
                    offset_frames3[i][(y << 6) + x] = (y1 << 6) + x1;
                }
            }
        }
	}
}

byte *R_SwirlingFlat (int flatnum)
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
			distortedflat[i] = normalflat[offset[i]];

		W_ReleaseLumpNum(flatnum);
		swirlflat = flatnum;
	}

	return distortedflat;
}

byte *R_WarpingFlat1 (int flatnum)
{
    static int swirltic = -1;
    static int swirlflat = -1;
    static byte distortedflat[FLATSIZE];

    if (swirltic != leveltime)
    {
        offset1 = offset_frames1[leveltime & (SEQUENCE - 1)];
        swirltic = leveltime;
        swirlflat = -1;
    }

    if (swirlflat != flatnum)
    {
        const char *normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

        // [PN] Loop through each pixel and apply the distortion.
        for (int i = 0; i < FLATSIZE; i++)
            distortedflat[i] = normalflat[offset1[i]];

        W_ReleaseLumpNum(flatnum);
        swirlflat = flatnum;
    }

    return distortedflat;
}

byte *R_WarpingFlat2 (int flatnum)
{
    static int swirltic = -1;
    static int swirlflat = -1;
    static byte distortedflat[FLATSIZE];

    if (swirltic != leveltime)
    {
        offset2 = offset_frames2[leveltime & (SEQUENCE - 1)];
        swirltic = leveltime;
        swirlflat = -1;
    }

    if (swirlflat != flatnum)
    {
        const char *normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

        // [PN] Loop through each pixel and apply the distortion.
        for (int i = 0; i < FLATSIZE; i++)
            distortedflat[i] = normalflat[offset2[i]];

        W_ReleaseLumpNum(flatnum);
        swirlflat = flatnum;
    }

    return distortedflat;
}

byte *R_WarpingFlat3 (int flatnum)
{
    static int swirltic = -1;
    static int swirlflat = -1;
    static byte distortedflat[FLATSIZE];

    if (swirltic != leveltime)
    {
        offset3 = offset_frames3[leveltime & (SEQUENCE - 1)];
        swirltic = leveltime;
        swirlflat = -1;
    }

    if (swirlflat != flatnum)
    {
        const char *normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

        // [PN] Loop through each pixel and apply the distortion.
        for (int i = 0; i < FLATSIZE; i++)
            distortedflat[i] = normalflat[offset3[i]];

        W_ReleaseLumpNum(flatnum);
        swirlflat = flatnum;
    }

    return distortedflat;
}
