//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2024 Julia Nechaevskaya
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

// R_data.c

#include <stdlib.h> // [crispy] calloc()

#include "doomdef.h"
#include "deh_str.h"

#include "i_swap.h"
#include "i_system.h"
#include "m_misc.h"
#include "r_local.h"
#include "p_local.h"
#include "v_trans.h"

#include "id_vars.h"


typedef struct
{
    int originx;                // block origin (allways UL), which has allready
    int originy;                // accounted  for the patch's internal origin
    int patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches

typedef struct texture_s texture_t;

struct texture_s
{
    char name[8];               // for switch changing, etc
    short width;
    short height;
    int index;                  // Index in textures list
    texture_t *next;            // Next in hash table chain
    short patchcount;
    texpatch_t patches[1];      // [patchcount] drawn back to front
    //  into the cached texture
};



int firstflat, lastflat, numflats;
int firstpatch, lastpatch, numpatches;
int firstspritelump, lastspritelump, numspritelumps;

int numtextures;
texture_t **textures;
texture_t **textures_hashtable;
int *texturewidthmask;
fixed_t *textureheight;         // needed for texture pegging
int *texturecompositesize;
short **texturecolumnlump;
unsigned **texturecolumnofs; // [crispy] was short
byte **texturecomposite;
const byte **texturebrightmap;  // [crispy] brightmaps

int *flattranslation;           // for global animation
int *texturetranslation;        // for global animation

fixed_t *spritewidth;           // needed for pre rendering
fixed_t *spriteoffset;
fixed_t *spritetopoffset;

lighttable_t *colormaps;
lighttable_t *pal_color; // [crispy] array holding palette colors for true color mode


/*
==============================================================================

						MAPTEXTURE_T CACHING

when a texture is first needed, it counts the number of composite columns
required in the texture and allocates space for a column directory and any
new columns.  The directory will simply point inside other patches if there
is only one patch in a given column, but any columns with multiple patches
will have new column_ts generated.

==============================================================================
*/

// [crispy] replace R_DrawColumnInCache(), R_GenerateComposite() and
// R_GenerateLookup() with Lee Killough's implementations found in MBF to fix
// Medusa bug taken from mbfsrc/R_DATA.C:136-425
//
// R_DrawColumnInCache
// Clip and draw a column from a patch into a cached post.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

void R_DrawColumnInCache(column_t * patch, byte * cache, int originy,
                         int cacheheight, byte * marks)
{
    int count, position;
    byte *source;
    int top = -1;

    while (patch->topdelta != 0xff)
    {
        // [crispy] support for DeePsea tall patches
        if (patch->topdelta <= top)
        {
            top += patch->topdelta;
        }
        else
        {
            top = patch->topdelta;
        }
        source = (byte *)patch + 3;
        count = patch->length;
        position = originy + top;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
        {
            memcpy(cache + position, source, count);

            // killough 4/9/98: remember which cells in column have been drawn,
            // so that column can later be converted into a series of posts, to
            // fix the Medusa bug.

            memset(marks + position, 0xff, count);
        }

        patch = (column_t *)((byte *)patch + patch->length + 4);
    }
}


//
// R_GenerateComposite
// Using the texture definition, the composite texture is created from the
// patches, and each column is cached.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug

void R_GenerateComposite(int texnum)
{
    byte* block;
    texture_t *texture;
    texpatch_t *patch;
    patch_t *realpatch;
    int x, x1, x2;
    int i;
    column_t *patchcol;
    short *collump;
    unsigned* colofs; // killough 4/9/98: make 32-bit
    byte *marks; // killough 4/9/98: transparency marks
    byte *source; // killough 4/9/98: temporary column

    texture = textures[texnum];

    block = Z_Malloc(texturecompositesize[texnum], PU_STATIC,
                     &texturecomposite[texnum]);

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];

    // killough 4/9/98: marks to identify transparent regions in merged textures
    marks = calloc(texture->width, texture->height);

    // [crispy] initialize composite background to palette index 0 (usually black)
    memset(block, 0, texturecompositesize[texnum]);

//
// composite the columns together
//
    for (i = 0, patch = texture->patches; i < texture->patchcount;
         i++, patch++)
    {
        realpatch = W_CacheLumpNum(patch->patch, PU_CACHE);
        x1 = patch->originx;
        x2 = x1 + SHORT(realpatch->width);

        if (x1 < 0)
            x = 0;
        else
            x = x1;
        if (x2 > texture->width)
            x2 = texture->width;

        for (; x < x2; x++)
        {
            if (collump[x] >= 0)
                continue;       // column does not have multiple patches

            patchcol = (column_t *) ((byte *) realpatch +
                                     LONG(realpatch->columnofs[x - x1]));
            R_DrawColumnInCache(patchcol, block + colofs[x],
                                patch->originy,
                                texture->height,
                                marks + x * texture->height);
        }

    }

    // killough 4/9/98: Next, convert multipatched columns into true columns,
    // to fix Medusa bug while still allowing for transparent regions.

    source = I_Realloc(NULL, texture->height); // temporary column
    for (i = 0; i < texture->width; i++)
    {
        if (collump[i] == -1) // process only multipatched columns
        {
            column_t *col =
                (column_t *) (block + colofs[i] - 3); // cached column
            const byte *mark = marks + i * texture->height;
            int j = 0;
            // [crispy] absolute topdelta for first 254 pixels, then relative
            int abstop, reltop = 0;
            boolean relative = false;

            // save column in temporary so we can shuffle it around
            memcpy(source, (byte *) col + 3, texture->height);

            for (;;) // reconstruct the column by scanning transparency marks
            {
                unsigned len; // killough 12/98

                while (j < texture->height && reltop < 254 &&
                       !mark[j]) // skip transparent cells
                    j++, reltop++;

                if (j >= texture->height) // if at end of column
                {
                    col->topdelta = -1; // end-of-column marker
                    break;
                }

                // [crispy] absolute topdelta for first 254 pixels, then relative
                col->topdelta =
                    relative ? reltop : j; // starting offset of post

                // [crispy] once we pass the 254 boundary, topdelta becomes relative
                if ((abstop = j) >= 254)
                {
                    relative = true;
                    reltop = 0;
                }

                // killough 12/98: Use 32-bit len counter, to support tall 1s
                // multipatched textures

                for (len = 0; j < texture->height && reltop < 254 && mark[j];
                     j++, reltop++)
                    len++; // count opaque cells

                col->length = len; // killough 12/98: intentionally truncate length

                // copy opaque cells from the temporary back into the column
                memcpy((byte *) col + 3, source + abstop, len);
                col = (column_t *) ((byte *) col + len + 4); // next post
            }
        }
    }

    free(source); // free temporary column
    free(marks); // free transparency marks

    // Now that the texture has been built in column cache, it is purgable
    // from zone memory.
    Z_ChangeTag(block, PU_CACHE);
}


//
// R_GenerateLookup
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

void R_GenerateLookup(int texnum)
{
    texture_t *texture;
    byte *patchcount;           // [texture->width]
    byte *postcount; // killough 4/9/98: keep count of posts in addition to patches.
    texpatch_t *patch;
    patch_t *realpatch;
    int x, x1, x2;
    int i;
    short *collump;
    unsigned *colofs; // killough 4/9/98: make 32-bit
    int csize = 0; // killough 10/98
    int err = 0; // killough 10/98

    texture = textures[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = 0;

    texturecompositesize[texnum] = 0;
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];

//
// count the number of columns that are covered by more than one patch
// fill in the lump / offset, so columns with only a single patch are
// all done
//
    patchcount = (byte *) Z_Malloc(texture->width, PU_STATIC, &patchcount);
    postcount = (byte *) Z_Malloc(texture->width, PU_STATIC, &postcount);
    memset(patchcount, 0, texture->width);
    memset(postcount, 0, texture->width);

    for (i = 0, patch = texture->patches; i < texture->patchcount;
         i++, patch++)
    {
        realpatch = W_CacheLumpNum(patch->patch, PU_CACHE);
        x1 = patch->originx;
        x2 = x1 + SHORT(realpatch->width);
        if (x1 < 0)
            x = 0;
        else
            x = x1;
        if (x2 > texture->width)
            x2 = texture->width;
        for (; x < x2; x++)
        {
            patchcount[x]++;
            collump[x] = patch->patch;
            colofs[x] = LONG(realpatch->columnofs[x - x1]) + 3;
        }
    }

    // killough 4/9/98: keep a count of the number of posts in column,
    // to fix Medusa bug while allowing for transparent multipatches.
    //
    // killough 12/98:
    // Post counts are only necessary if column is multipatched,
    // so skip counting posts if column comes from a single patch.
    // This allows arbitrarily tall textures for 1s walls.
    //
    // If texture is >= 256 tall, assume it's 1s, and hence it has
    // only one post per column. This avoids crashes while allowing
    // for arbitrarily tall multipatched 1s textures.

    if (texture->patchcount > 1 && texture->height < 256)
    {
        // killough 12/98: Warn about a common column construction bug
        unsigned limit = texture->height * 3 + 3; // absolute column size limit

        for (i = texture->patchcount, patch = texture->patches; --i >= 0;)
        {
            int pat = patch->patch;
            const patch_t *realpatch = W_CacheLumpNum(pat, PU_CACHE);
            int x, x1 = patch++->originx, x2 = x1 + SHORT(realpatch->width);
            const int *cofs = realpatch->columnofs - x1;

            if (x2 > texture->width)
                x2 = texture->width;
            if (x1 < 0)
                x1 = 0;

            for (x = x1; x < x2; x++)
            {
                if (patchcount[x] > 1) // Only multipatched columns
                {
                    const column_t *col =
                        (const column_t *) ((const byte *) realpatch +
                                            LONG(cofs[x]));
                    const byte *base = (const byte *) col;

                    // count posts
                    for (; col->topdelta != 0xff; postcount[x]++)
                    {
                        if ((unsigned) ((const byte *) col - base) <= limit)
                            col = (const column_t *) ((const byte *) col +
                                                      col->length + 4);
                        else
                            break;
                    }
                }
            }
        }
    }

    for (x = 0; x < texture->width; x++)
    {
        if (!patchcount[x] && !err++) // killough 10/98: non-verbose output
        {
            // [crispy] fix absurd texture name in error message
            printf("R_GenerateLookup: column without a patch (%.8s)\n",
                texture->name);
            // [crispy] do not return yet
            /*
            return;
            */
        }
        // I_Error ("R_GenerateLookup: column without a patch");

        // [crispy] treat patch-less columns the same as multi-patched
        if (patchcount[x] > 1 || !patchcount[x])
        {
            // killough 1/25/98, 4/9/98:
            //
            // Fix Medusa bug, by adding room for column header
            // and trailer bytes for each post in merged column.
            // For now, just allocate conservatively 4 bytes
            // per post per patch per column, since we don't
            // yet know how many posts the merged column will
            // require, and it's bounded above by this limit.
            collump[x] = -1; // use the cached block
            colofs[x] = csize + 3; // three header bytes in a column
            // killough 12/98: add room for one extra post
            csize += 4 * postcount[x] + 5; // 1 stop byte plus 4 bytes per post
        }

            // [crispy] remove limit
            /*
            if (texturecompositesize[texnum] > 0x10000 - texture->height)
                I_Error("R_GenerateLookup: texture %i is >64k", texnum);
            */
        csize += texture->height; // height bytes of texture data
    }

    texturecompositesize[texnum] += csize;

    Z_Free(patchcount);
    Z_Free(postcount);
}


/*
================
=
= R_GetColumn
=
================
*/

byte *R_GetColumn(int tex, int col)
{
    int lump, ofs;

    col &= texturewidthmask[tex];
    lump = texturecolumnlump[tex][col];
    ofs = texturecolumnofs[tex][col];
    if (lump > 0)
        return (byte *) W_CacheLumpNum(lump, PU_CACHE) + ofs;
    if (!texturecomposite[tex])
        R_GenerateComposite(tex);
    return texturecomposite[tex] + ofs;
}

/*
================
=
= GenerateTextureHashTable
=
================
*/

static void GenerateTextureHashTable(void)
{
    texture_t **rover;
    int i;
    int key;

    textures_hashtable 
            = Z_Malloc(sizeof(texture_t *) * numtextures, PU_STATIC, 0);

    memset(textures_hashtable, 0, sizeof(texture_t *) * numtextures);

    // Add all textures to hash table

    for (i=0; i<numtextures; ++i)
    {
        // Store index

        textures[i]->index = i;

        // Vanilla Doom does a linear search of the texures array
        // and stops at the first entry it finds.  If there are two
        // entries with the same name, the first one in the array
        // wins. The new entry must therefore be added at the end
        // of the hash chain, so that earlier entries win.

        key = W_LumpNameHash(textures[i]->name) % numtextures;

        rover = &textures_hashtable[key];

        while (*rover != NULL)
        {
            rover = &(*rover)->next;
        }

        // Hook into hash table

        textures[i]->next = NULL;
        *rover = textures[i];
    }
}


/*
==================
=
= R_InitTextures
=
= Initializes the texture list with the textures from the world map
=
==================
*/

void R_InitTextures(void)
{
    maptexture_t *mtexture;
    texture_t *texture;
    mappatch_t *mpatch;
    texpatch_t *patch;
    int i, j;
    int *maptex, *maptex2, *maptex1;
    char name[9], *names, *name_p;
    int *patchlookup;
    int nummappatches;
    int offset, maxoff, maxoff2;
    int numtextures1, numtextures2;
    int *directory;
    const char *texture1, *texture2, *pnames;

    texture1 = DEH_String("TEXTURE1");
    texture2 = DEH_String("TEXTURE2");
    pnames = DEH_String("PNAMES");

//
// load the patch names from pnames.lmp
//
    names = W_CacheLumpName(pnames, PU_STATIC);
    nummappatches = LONG(*((int *) names));
    name_p = names + 4;
    patchlookup = Z_Malloc(nummappatches * sizeof(*patchlookup), PU_STATIC, NULL);
    for (i = 0; i < nummappatches; i++)
    {
        M_StringCopy(name, name_p + i * 8, sizeof(name));
        patchlookup[i] = W_CheckNumForName(name);
    }
    W_ReleaseLumpName(pnames);

//
// load the map texture definitions from textures.lmp
//
    maptex = maptex1 = W_CacheLumpName(texture1, PU_STATIC);
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength(W_GetNumForName(texture1));
    directory = maptex + 1;

    if (W_CheckNumForName(texture2) != -1)
    {
        maptex2 = W_CacheLumpName(texture2, PU_STATIC);
        numtextures2 = LONG(*maptex2);
        maxoff2 = W_LumpLength(W_GetNumForName(texture2));
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        maxoff2 = 0;
    }
    numtextures = numtextures1 + numtextures2;

    //
    //      Init the startup thermometer at this point...
    //
    {
        int start, end;
        int spramount;
        start = W_GetNumForName(DEH_String("S_START"));
        end = W_GetNumForName(DEH_String("S_END"));
        spramount = end - start + 1;
        InitThermo(spramount + numtextures + 6);
    }

    textures = Z_Malloc(numtextures * sizeof(texture_t *), PU_STATIC, 0);
    texturecolumnlump = Z_Malloc(numtextures * sizeof(short *), PU_STATIC, 0);
    texturecolumnofs = Z_Malloc(numtextures * sizeof(*texturecolumnofs), PU_STATIC, 0);
    texturecomposite = Z_Malloc(numtextures * sizeof(byte *), PU_STATIC, 0);
    texturecompositesize = Z_Malloc(numtextures * sizeof(int), PU_STATIC, 0);
    texturewidthmask = Z_Malloc(numtextures * sizeof(int), PU_STATIC, 0);
    textureheight = Z_Malloc(numtextures * sizeof(fixed_t), PU_STATIC, 0);
    texturebrightmap = Z_Malloc(numtextures * sizeof(*texturebrightmap), PU_STATIC, 0);

    for (i = 0; i < numtextures; i++, directory++)
    {
#ifdef __NEXT__
        if (!(i & 63))
            printf(".");
#else
        IncThermo();
#endif
        if (i == numtextures1)
        {                       // start looking in second texture file
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex + 1;
        }

        offset = LONG(*directory);
        if (offset > maxoff)
            I_Error("R_InitTextures: bad texture directory");
        mtexture = (maptexture_t *) ((byte *) maptex + offset);
        texture = textures[i] = Z_Malloc(sizeof(texture_t)
                                         +
                                         sizeof(texpatch_t) *
                                         (SHORT(mtexture->patchcount) - 1),
                                         PU_STATIC, 0);
        texture->width = SHORT(mtexture->width);
        texture->height = SHORT(mtexture->height);
        texture->patchcount = SHORT(mtexture->patchcount);
        memcpy(texture->name, mtexture->name, sizeof(texture->name));
        mpatch = &mtexture->patches[0];
        patch = &texture->patches[0];
        // [crispy] initialize brightmaps
        texturebrightmap[i] = R_BrightmapForTexName(texture->name);
        for (j = 0; j < texture->patchcount; j++, mpatch++, patch++)
        {
            patch->originx = SHORT(mpatch->originx);
            patch->originy = SHORT(mpatch->originy);
            patch->patch = patchlookup[SHORT(mpatch->patch)];
            if (patch->patch == -1)
                I_Error("R_InitTextures: Missing patch in texture %s",
                        texture->name);
        }
        texturecolumnlump[i] = Z_Malloc(texture->width * sizeof(**texturecolumnlump),
                                        PU_STATIC, 0);
        texturecolumnofs[i] = Z_Malloc(texture->width * sizeof(**texturecolumnofs),
                                       PU_STATIC, 0);
        j = 1;
        while (j * 2 <= texture->width)
            j <<= 1;
        texturewidthmask[i] = j - 1;
        textureheight[i] = texture->height << FRACBITS;
    }

    Z_Free(patchlookup);

    W_ReleaseLumpName(texture1);
    if (maptex2)
    {
        W_ReleaseLumpName(texture2);
    }

//
// precalculate whatever possible
//              
    for (i = 0; i < numtextures; i++)
    {
        R_GenerateLookup(i);
        CheckAbortStartup();
    }

//
// translation table for global animation
//
    texturetranslation = Z_Malloc((numtextures + 1) * sizeof(int), PU_STATIC, 0);
    for (i = 0; i < numtextures; i++)
        texturetranslation[i] = i;

    GenerateTextureHashTable();
}


/*
================
=
= R_InitFlats
=
=================
*/

void R_InitFlats(void)
{
    int i;

    firstflat = W_GetNumForName(DEH_String("F_START")) + 1;
    lastflat = W_GetNumForName(DEH_String("F_END")) - 1;
    numflats = lastflat - firstflat + 1;

// translation table for global animation
    flattranslation = Z_Malloc((numflats + 1) * sizeof(int), PU_STATIC, 0);
    for (i = 0; i < numflats; i++)
        flattranslation[i] = i;
}


/*
================
=
= R_InitSpriteLumps
=
= Finds the width and hoffset of all sprites in the wad, so the sprite doesn't
= need to be cached just for the header during rendering
=================
*/

void R_InitSpriteLumps(void)
{
    int i;
    patch_t *patch;

    firstspritelump = W_GetNumForName(DEH_String("S_START")) + 1;
    lastspritelump = W_GetNumForName(DEH_String("S_END")) - 1;
    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = Z_Malloc(numspritelumps * sizeof(fixed_t), PU_STATIC, 0);
    spriteoffset = Z_Malloc(numspritelumps * sizeof(fixed_t), PU_STATIC, 0);
    spritetopoffset = Z_Malloc(numspritelumps * sizeof(fixed_t), PU_STATIC, 0);

    for (i = 0; i < numspritelumps; i++)
    {
#ifdef __NEXT__
        if (!(i & 63))
            printf(".");
#else
        IncThermo();
#endif
        patch = W_CacheLumpNum(firstspritelump + i, PU_CACHE);
        spritewidth[i] = SHORT(patch->width) << FRACBITS;
        spriteoffset[i] = SHORT(patch->leftoffset) << FRACBITS;
        spritetopoffset[i] = SHORT(patch->topoffset) << FRACBITS;
    }
}


/*
================
=
= R_InitColormaps
=
=================
*/

void R_InitColormaps(void)
{
#ifndef CRISPY_TRUECOLOR
    int lump, length;
//
// load in the light tables
// 256 byte align tables
//
    lump = W_GetNumForName(DEH_String("COLORMAP"));
    length = W_LumpLength(lump);
    colormaps = Z_Malloc(length, PU_STATIC, 0);
    W_ReadLump(lump, colormaps);
    NUMCOLORMAPS = 32; // [crispy] smooth diminishing lighting
#else
	int c, i, j = 0;
	byte r, g, b;

	byte *const playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
	byte *const colormap = W_CacheLumpName("COLORMAP", PU_STATIC);

	// [JN] Handle RGB channels separatelly
	// to support variable saturation and color intensity.
	byte r_channel, g_channel, b_channel;

	// [JN] Saturation floats, high and low.
	// If saturation has been modified (< 100), set high and low
	// values according to saturation level. Sum of r,g,b channels
	// and floats must be 1.0 to get proper colors.
	const float a_hi = vid_saturation < 100 ? I_SaturationPercent[vid_saturation] : 0;
	const float a_lo = vid_saturation < 100 ? (a_hi / 2) : 0;

	// [crispy] Smoothest diminishing lighting.
	// Compiled in but not enabled TrueColor mode
	// can't use more than original 32 colormaps.
	if (vid_truecolor && vis_smooth_light)
	{
		NUMCOLORMAPS = 256;
	}
	else
	{
		NUMCOLORMAPS = 32;
	}

	colormaps = I_Realloc(colormaps, (NUMCOLORMAPS + 1) * 256 * sizeof(lighttable_t));

	if (vid_truecolor)
	{
		for (c = 0; c < NUMCOLORMAPS; c++)
		{
			const float scale = 1. * c / NUMCOLORMAPS;

			for (i = 0; i < 256; i++)
			{
				const byte k = colormap[i];

				// [Alaux] Component intensity applied here
				const byte pal_r = playpal[3 * k + 0] * vid_r_intensity,
				           pal_g = playpal[3 * k + 1] * vid_g_intensity,
				           pal_b = playpal[3 * k + 2] * vid_b_intensity;

				// [Alaux] Saturation applied in the following three
				r_channel = 
					(byte) ((1 - a_hi) * pal_r +
							(0 + a_lo) * pal_g +
							(0 + a_lo) * pal_b);

				g_channel = 
					(byte) ((0 + a_lo) * pal_r +
							(1 - a_hi) * pal_g +
							(0 + a_lo) * pal_b);

				b_channel = 
					(byte) ((0 + a_lo) * pal_r +
							(0 + a_lo) * pal_g +
							(1 - a_hi) * pal_b);

				r = gammatable[vid_gamma][r_channel] * (1. - scale) + gammatable[vid_gamma][0] * scale;
				g = gammatable[vid_gamma][g_channel] * (1. - scale) + gammatable[vid_gamma][0] * scale;
				b = gammatable[vid_gamma][b_channel] * (1. - scale) + gammatable[vid_gamma][0] * scale;

				colormaps[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
	}
	else
	{
		for (c = 0; c < NUMCOLORMAPS; c++)
		{
			for (i = 0; i < 256; i++)
			{
				// [Alaux] Component intensity applied here
				const byte pal_r = playpal[3 * colormap[c * 256 + i] + 0] * vid_r_intensity,
				           pal_g = playpal[3 * colormap[c * 256 + i] + 1] * vid_g_intensity,
				           pal_b = playpal[3 * colormap[c * 256 + i] + 2] * vid_b_intensity;

				// [Alaux] Saturation applied in the following three
				r_channel =
					(byte) ((1 - a_hi) * pal_r +
							(0 + a_lo) * pal_g +
							(0 + a_lo) * pal_b);

				g_channel =
					(byte) ((0 + a_lo) * pal_r +
							(1 - a_hi) * pal_g +
							(0 + a_lo) * pal_b);

				b_channel =
					(byte) ((0 + a_lo) * pal_r +
							(0 + a_lo) * pal_g +
							(1 - a_hi) * pal_b);

				r = gammatable[vid_gamma][r_channel] & ~3;
				g = gammatable[vid_gamma][g_channel] & ~3;
				b = gammatable[vid_gamma][b_channel] & ~3;

				colormaps[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
	}

	// [crispy] Invulnerability (c == COLORMAPS), generated from COLORMAP lump
	for (i = 0; i < 256; i++)
	{
		// [Alaux] Component intensity applied here
		const byte pal_r = playpal[3 * colormap[INVERSECOLORMAP * 256 + i] + 0] * vid_r_intensity,
				   pal_g = playpal[3 * colormap[INVERSECOLORMAP * 256 + i] + 1] * vid_g_intensity,
				   pal_b = playpal[3 * colormap[INVERSECOLORMAP * 256 + i] + 2] * vid_b_intensity;

		// [Alaux] Saturation applied in the following three
		r_channel =
			(byte) ((1 - a_hi) * pal_r +
					(0 + a_lo) * pal_g +
					(0 + a_lo) * pal_b);

		g_channel =
			(byte) ((0 + a_lo) * pal_r +
					(1 - a_hi) * pal_g +
					(0 + a_lo) * pal_b);

		b_channel =
			(byte) ((0 + a_lo) * pal_r +
					(0 + a_lo) * pal_g +
					(1 - a_hi) * pal_b);

		r = gammatable[vid_gamma][r_channel] & ~3;
		g = gammatable[vid_gamma][g_channel] & ~3;
		b = gammatable[vid_gamma][b_channel] & ~3;

		colormaps[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
	}

	W_ReleaseLumpName("COLORMAP");

	if (!pal_color)
	{
		pal_color = (pixel_t*) Z_Malloc(256 * sizeof(pixel_t), PU_STATIC, 0);
	}

	for (i = 0, j = 0; i < 256; i++)
	{
		// [Alaux] Component intensity applied here
		const byte pal_r = playpal[3 * i + 0] * vid_r_intensity,
		           pal_g = playpal[3 * i + 1] * vid_g_intensity,
		           pal_b = playpal[3 * i + 2] * vid_b_intensity;

		// [Alaux] Saturation applied in the following three
		r_channel = 
			(byte) ((1 - a_hi) * pal_r +
					(0 + a_lo) * pal_g +
					(0 + a_lo) * pal_b);

		g_channel = 
			(byte) ((0 + a_lo) * pal_r +
					(1 - a_hi) * pal_g +
					(0 + a_lo) * pal_b);

		b_channel = 
			(byte) ((0 + a_lo) * pal_r +
					(0 + a_lo) * pal_g +
					(1 - a_hi) * pal_b);

		r = gammatable[vid_gamma][r_channel];
		g = gammatable[vid_gamma][g_channel];
		b = gammatable[vid_gamma][b_channel];

		pal_color[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
	}

	W_ReleaseLumpName("PLAYPAL");
#endif
}


/*
================
=
= R_InitHSVColors
=
= [crispy] initialize color translation and color strings tables
=
=================
*/

static void R_InitHSVColors (void)
{
    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
    char c[3];
    int i, j;

    if (!crstr)
    {
        crstr = realloc(NULL, CRMAX * sizeof(*crstr));
    }

    // [JN] TODO - get rid of -2
    for (i = 0 ; i < CRMAX-2 ; i++)
    {
        for (j = 0; j < 256; j++)
        {
            cr[i][j] = V_Colorize(playpal, i, j, false);
        }

        M_snprintf(c, sizeof(c), "%c%c", cr_esc, '0' + i);
        crstr[i] = M_StringDuplicate(c);
    }

    W_ReleaseLumpName("PLAYPAL");
}

#ifdef CRISPY_TRUECOLOR
// [crispy] Changes palette to given one. Used exclusively in true color rendering
// for proper drawing of E2END pic in F_DrawUnderwater. Changing palette back to
// original PLAYPAL for restoring proper colors will be done in R_InitColormaps.
void R_SetUnderwaterPalette(byte *palette)
{
    int i, j = 0;
    byte r, g, b;

    for (i = 0; i < 256; i++)
    {
        r = gammatable[vid_gamma][palette[3 * i + 0]] + gammatable[vid_gamma][0];
        g = gammatable[vid_gamma][palette[3 * i + 1]] + gammatable[vid_gamma][0];
        b = gammatable[vid_gamma][palette[3 * i + 2]] + gammatable[vid_gamma][0];

        pal_color[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
    }
}
#endif


/*
================
=
= R_InitData
=
= Locates all the lumps that will be used by all views
= Must be called after W_Init
=================
*/

void R_InitData(void)
{
    // [crispy] Moved R_InitFlats() to the top, because it sets firstflat/lastflat
    // which are required by R_InitTextures() to prevent flat lumps from being
    // mistaken as patches and by R_InitBrightmaps() to set brightmaps for flats.
    // R_InitBrightmaps() comes next, because it sets R_BrightmapForTexName()
    // to initialize brightmaps depending on gameversion in R_InitTextures().
    //tprintf("\nR_InitTextures ", 0);
    R_InitFlats();
    printf (".");
    R_InitBrightmaps();
    printf (".");
    R_InitTextures();
    printf (".");
    //tprintf("R_InitFlats\n", 0);
//  R_InitFlats (); [crispy] moved ...
    //IncThermo();
    printf (".");
    //tprintf("R_InitSpriteLumps ", 0);
    R_InitSpriteLumps();
    //IncThermo();
    printf (".");
    R_InitColormaps();
    printf (".");
    R_InitHSVColors();
    printf (".");
#ifndef CRISPY_TRUECOLOR
    // [JN] Load original TINTTAB lump.
    V_LoadTintTable();
    printf (".");
    // [JN] Compose extra translucency tables.
    V_InitTransMaps();
    printf (".");
#endif
}


//=============================================================================

/*
================
=
= R_FlatNumForName
=
================
*/

int R_FlatNumForName(const char *name)
{
    int i;
    char namet[9];

    i = W_CheckNumForNameFromTo(name, lastflat, firstflat);
    if (i == -1)
    {
        namet[8] = 0;
        memcpy(namet, name, 8);
        fprintf(stderr, "R_FlatNumForName: %s not found\n", namet);
        // [crispy] since there is no "No Flat" marker,
        // render missing flats as SKY
        return skyflatnum;
    }
    return i - firstflat;
}


/*
================
=
= R_CheckTextureNumForName
=
================
*/

int R_CheckTextureNumForName(const char *name)
{
    texture_t *texture;
    int key;

    if (name[0] == '-')         // no texture marker
        return 0;

    key = W_LumpNameHash(name) % numtextures;

    texture = textures_hashtable[key]; 

    while (texture != NULL)
    {
        if (!strncasecmp (texture->name, name, 8))
        {
            return texture->index;
        }
        texture = texture->next;
    }

    return -1;
}


/*
================
=
= R_TextureNumForName
=
================
*/

int R_TextureNumForName(const char *name)
{
    int i;
    //char  namet[9];

    i = R_CheckTextureNumForName(name);
    if (i == -1)
    {
        // [crispy] make missing texture non-fatal
        printf("R_TextureNumForName: %.8s not found\n", name);
        return 0;
    }

    return i;
}


/*
=================
=
= R_PrecacheLevel
=
= Totally rewritten by Lee Killough to use less memory,
= to avoid using alloca(), and to improve performance.
=================
*/

void R_PrecacheLevel(void)
{
    int i;
    byte *hitlist;

    if (demoplayback)
    {
        return;
    }

    {
        const size_t size = numflats > numsprites ? numflats : numsprites;
        hitlist = malloc(numtextures > size ? numtextures : size);
    }

    // Precache flats.

    memset(hitlist, 0, numflats);

    for (i = numsectors ; --i >= 0 ; )
    {
        hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;
    }

    for (i = numflats ; --i >= 0 ; )
    {
        if (hitlist[i])
        {
            W_CacheLumpNum(firstflat + i, PU_CACHE);
        }
    }

    // Precache textures.

    memset(hitlist, 0, numtextures);

    for (i = numsides; --i >= 0;)
    {
        hitlist[sides[i].bottomtexture] =
        hitlist[sides[i].toptexture] =
        hitlist[sides[i].midtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.

    hitlist[skytexture] = 1;

    for (i = numtextures ; --i >= 0 ; )
    {
        if (hitlist[i])
        {
            texture_t *texture = textures[i];
            int j = texture->patchcount;

            while (--j >= 0)
            {
                W_CacheLumpNum(texture->patches[j].patch, PU_CACHE);
            }
        }
    }

    // Precache sprites.
    memset(hitlist, 0, numsprites);

    {
        thinker_t *th;

        for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
        {
            if (th->function == P_MobjThinker)
            {
                hitlist[((mobj_t *)th)->sprite] = 1;
            }
        }
    }

    for (i = numsprites; --i >= 0 ;)
    {
        if (hitlist[i])
        {
            int j = sprites[i].numframes;

            while (--j >= 0)
            {
                short *sflump = sprites[i].spriteframes[j].lump;
                int k = 7;

                do
                W_CacheLumpNum(firstspritelump + sflump[k], PU_CACHE);
                while (--k >= 0);
            }
        }
    }

    free(hitlist);
}
