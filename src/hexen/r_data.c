//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2025 Julia Nechaevskaya
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


#include <stdlib.h> // [crispy] calloc()

#include "i_system.h"
#include "i_swap.h"
#include "m_misc.h"
#include "z_zone.h"

#include "h2def.h"

#include "v_trans.h"
#include "v_video.h"
#include "p_local.h"


//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 
typedef struct
{
    int originx;                // block origin (allways UL), which has allready
    int originy;                // accounted  for the patch's internal origin
    int patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct
{
    char name[8];               // for switch changing, etc
    short width;
    short height;
    short patchcount;
    texpatch_t patches[1];      // [patchcount] drawn back to front
    //  into the cached texture
} texture_t;



int firstflat, lastflat, numflats;
int firstpatch, lastpatch, numpatches;
int firstspritelump, lastspritelump, numspritelumps;

int numtextures;
texture_t **textures;

int*			texturewidthmask;
int*			texturewidth; // [crispy] texture width for wrapping column getter function
// needed for texture pegging
fixed_t*		textureheight; // [crispy] texture height for Tutti-Frutti fix
int*			texturecompositesize;
short**			texturecolumnlump;
unsigned**		texturecolumnofs; // [crispy] column offsets for composited translucent mid-textures on 2S walls
unsigned**		texturecolumnofs2; // [crispy] column offsets for composited opaque textures
byte**			texturecomposite; // [crispy] composited translucent mid-textures on 2S walls
byte**			texturecomposite2; // [crispy] composited opaque textures
const byte**	texturebrightmap; // [crispy] brightmaps

int *flattranslation;           // for global animation
int *texturetranslation;        // for global animation

fixed_t *spritewidth;           // needed for pre rendering
fixed_t *spriteoffset;
fixed_t *spritetopoffset;

lighttable_t *colormaps;


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

static void R_DrawColumnInCache(column_t * patch, byte * cache, int originy,
                         int cacheheight, byte * marks)
{
    int		count;
    int		position;
    const byte*	source;
    int		top = -1;

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
	    memcpy (cache + position, source, count);

	    // killough 4/9/98: remember which cells in column have been drawn,
	    // so that column can later be converted into a series of posts, to
	    // fix the Medusa bug.

	    memset (marks + position, 0xff, count);
	}
		
	patch = (column_t *)(  (byte *)patch + patch->length + 4); 
    }
}


//
// R_GenerateComposite
// Using the texture definition, the composite texture is created from the
// patches, and each column is cached.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug

static void R_GenerateComposite(int texnum)
{
    byte*		block, *block2;
    texture_t*		texture;
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x_1;
    int			x_2;
    int			i;
    column_t*		patchcol;
    const short*		collump;
    unsigned*		colofs, *colofs2; // killough 4/9/98: make 32-bit
    byte*		marks; // killough 4/9/98: transparency marks
    byte*		source; // killough 4/9/98: temporary column
	
    texture = textures[texnum];

    block = Z_Malloc (texturecompositesize[texnum],
		      PU_STATIC, 
		      &texturecomposite[texnum]);	
    // [crispy] memory block for opaque textures
    block2 = Z_Malloc (texture->width * texture->height,
		      PU_STATIC,
		      &texturecomposite2[texnum]);

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    colofs2 = texturecolumnofs2[texnum];
    
    // killough 4/9/98: marks to identify transparent regions in merged textures
    marks = calloc(texture->width, texture->height);

    // [crispy] initialize composite background to palette index 0 (usually black)
    memset(block, 0, texturecompositesize[texnum]);

    // Composite the columns together.
    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	x_1 = patch->originx;
	x_2 = x_1 + SHORT(realpatch->width);

	if (x_1<0)
	    x = 0;
	else
	    x = x_1;
	
	if (x_2 > texture->width)
	    x_2 = texture->width;

	for ( ; x<x_2 ; x++)
	{
	    // Column does not have multiple patches?
	    // [crispy] generate composites for single-patched columns as well
	    /*
	    if (collump[x] >= 0)
		continue;
	    */
	    
	    patchcol = (column_t *)((byte *)realpatch
				    + LONG(realpatch->columnofs[x-x_1]));
	    R_DrawColumnInCache (patchcol,
				 block + colofs[x],
				 // [crispy] single-patched columns are normally not composited
				 // but directly read from the patch lump ignoring their originy
				 collump[x] >= 0 ? 0 : patch->originy,
				 texture->height,
				 marks + x * texture->height);
	}
						
    }

    // killough 4/9/98: Next, convert multipatched columns into true columns,
    // to fix Medusa bug while still allowing for transparent regions.

    source = I_Realloc(NULL, texture->height); // temporary column
    for (i = 0; i < texture->width; i++)
    {
	// [crispy] generate composites for all columns
//	if (collump[i] == -1) // process only multipatched columns
	{
	    column_t *col = (column_t *)(block + colofs[i] - 3); // cached column
	    const byte *mark = marks + i * texture->height;
	    int j = 0;
	    // [crispy] absolut topdelta for first 254 pixels, then relative
	    int abstop, reltop = 0;
	    boolean relative = false;

	    // save column in temporary so we can shuffle it around
	    memcpy(source, (byte *) col + 3, texture->height);
	    // [crispy] copy composited columns to opaque texture
	    memcpy(block2 + colofs2[i], source, texture->height);

	    for ( ; ; ) // reconstruct the column by scanning transparency marks
	    {
		unsigned len; // killough 12/98

		while (j < texture->height && reltop < 254 && !mark[j]) // skip transparent cells
		    j++, reltop++;

		if (j >= texture->height) // if at end of column
		{
		    col->topdelta = -1; // end-of-column marker
		    break;
		}

		// [crispy] absolut topdelta for first 254 pixels, then relative
		col->topdelta = relative ? reltop : j; // starting offset of post

		// [crispy] once we pass the 254 boundary, topdelta becomes relative
		if ((abstop = j) >= 254)
		{
			relative = true;
			reltop = 0;
		}

		// killough 12/98:
		// Use 32-bit len counter, to support tall 1s multipatched textures

		for (len = 0; j < texture->height && reltop < 254 && mark[j]; j++, reltop++)
		    len++; // count opaque cells

		col->length = len; // killough 12/98: intentionally truncate length

		// copy opaque cells from the temporary back into the column
		memcpy((byte *) col + 3, source + abstop, len);
		col = (column_t *)((byte *) col + len + 4); // next post
	    }
	}
    }

    free(source); // free temporary column
    free(marks); // free transparency marks

    // Now that the texture has been built in column cache,
    //  it is purgable from zone memory.
    Z_ChangeTag (block, PU_CACHE);
    Z_ChangeTag (block2, PU_CACHE);
}


//
// R_GenerateLookup
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

static void R_GenerateLookup(int texnum)
{
    texture_t*		texture;
    byte*		patchcount;	// patchcount[texture->width]
    byte*		postcount; // killough 4/9/98: keep count of posts in addition to patches.
    texpatch_t*		patch;	
    patch_t*		realpatch;
    int			x;
    int			x_1;
    int			x_2;
    int			i;
    short*		collump;
    unsigned*		colofs, *colofs2; // killough 4/9/98: make 32-bit
    int			csize = 0; // killough 10/98
    int			err = 0; // killough 10/98
	
    texture = textures[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = 0;
    texturecomposite2[texnum] = 0;
    
    texturecompositesize[texnum] = 0;
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];
    colofs2 = texturecolumnofs2[texnum];
    
    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.
    patchcount = (byte *) Z_Malloc(texture->width, PU_STATIC, &patchcount);
    postcount = (byte *) Z_Malloc(texture->width, PU_STATIC, &postcount);
    memset (patchcount, 0, texture->width);
    memset (postcount, 0, texture->width);

    for (i=0 , patch = texture->patches;
	 i<texture->patchcount;
	 i++, patch++)
    {
	realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
	x_1 = patch->originx;
	x_2 = x_1 + SHORT(realpatch->width);

	if (x_1 < 0)
	    x = 0;
	else
	    x = x_1;

	if (x_2 > texture->width)
	    x_2 = texture->width;
	for ( ; x<x_2 ; x++)
	{
	    patchcount[x]++;
	    collump[x] = patch->patch;
	    colofs[x] = LONG(realpatch->columnofs[x-x_1])+3;
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

    // [crispy] generate composites for all textures
//  if (texture->patchcount > 1 && texture->height < 256)
    {
	// killough 12/98: Warn about a common column construction bug
	unsigned limit = texture->height * 3 + 3; // absolute column size limit

	for (i = texture->patchcount, patch = texture->patches; --i >= 0; )
	{
	    int pat = patch->patch;
	    const patch_t *real_patch = W_CacheLumpNum(pat, PU_CACHE);
	    int xx, xx_1 = patch++->originx, xx_2 = xx_1 + SHORT(realpatch->width);
	    const int *cofs = realpatch->columnofs - xx_1;

	    if (xx_2 > texture->width)
		xx_2 = texture->width;
	    if (xx_1 < 0)
		xx_1 = 0;

	    for (xx = xx_1 ; xx < xx_2 ; xx++)
	    {
		// [crispy] generate composites for all columns
//		if (patchcount[x] > 1) // Only multipatched columns
		{
		    const column_t *col = (const column_t*)((const byte*) real_patch + LONG(cofs[xx]));
		    const byte *base = (const byte *) col;

		    // count posts
		    for ( ; col->topdelta != 0xff; postcount[xx]++)
		    {
			if ((unsigned)((const byte *) col - base) <= limit)
			    col = (const column_t *)((const byte *) col + col->length + 4);
			else
			    break;
		    }
		}
	    }
	}
    }

    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.

    for (x=0 ; x<texture->width ; x++)
    {
	if (!patchcount[x] && !err++) // killough 10/98: non-verbose output
	{
	    // [crispy] fix absurd texture name in error message
	    printf ("R_GenerateLookup: column without a patch (%.8s)\n",
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
	    // Use the cached block.
	    // [crispy] moved up here, the rest in this loop
	    // applies to single-patched textures as well
	    collump[x] = -1;	
	}
	    // killough 1/25/98, 4/9/98:
	    //
	    // Fix Medusa bug, by adding room for column header
	    // and trailer bytes for each post in merged column.
	    // For now, just allocate conservatively 4 bytes
	    // per post per patch per column, since we don't
	    // yet know how many posts the merged column will
	    // require, and it's bounded above by this limit.

	    colofs[x] = csize + 3; // three header bytes in a column
	    // killough 12/98: add room for one extra post
	    csize += 4 * postcount[x] + 5; // 1 stop byte plus 4 bytes per post
	    
	    // [crispy] remove limit
	    /*
	    if (texturecompositesize[texnum] > 0x10000-texture->height)
	    {
		I_Error ("R_GenerateLookup: texture %i is >64k",
			 texnum);
	    }
	    */
	csize += texture->height; // height bytes of texture data
	// [crispy] initialize opaque texture column offset
	colofs2[x] = x * texture->height;
    }

    texturecompositesize[texnum] = csize;

    Z_Free(patchcount);
    Z_Free(postcount);
}


// -----------------------------------------------------------------------------
// R_GetColumn
// [crispy] wrapping column getter function for any non-power-of-two textures
// -----------------------------------------------------------------------------

byte *R_GetColumn (int tex, int col)
{
    const int width = texturewidth[tex];
    const int mask = texturewidthmask[tex];

    if (mask + 1 == width)
    {
        col &= mask;
    }
    else
    {
        while (col < 0)
        {
          col += width;
        }
        col %= width;
    }

    if (!texturecomposite2[tex])
        R_GenerateComposite(tex);

    const int ofs = texturecolumnofs2[tex][col];

    return texturecomposite2[tex] + ofs;
}

// -----------------------------------------------------------------------------
// R_GetColumnMod
// [crispy] wrapping column getter function for composited translucent mid-textures on 2S walls
// -----------------------------------------------------------------------------

byte *R_GetColumnMod (int tex, int col)
{
    int ofs;

    while (col < 0)
        col += texturewidth[tex];

    col %= texturewidth[tex];
    ofs = texturecolumnofs[tex][col];

    if (!texturecomposite[tex])
        R_GenerateComposite (tex);

    return texturecomposite[tex] + ofs;
}

// -----------------------------------------------------------------------------
// R_IsTextureName
//  [PN] Compare a WAD name field (8 bytes, space-padded, not null-terminated)
//  to a standard C string, case-insensitively.
//  Trims trailing spaces and nulls from the WAD name before comparing.
// -----------------------------------------------------------------------------

static inline int R_IsTextureName (const char wadname[8], const char *s)
{
    char buf[9];
    memcpy(buf, wadname, 8);
    buf[8] = '\0';
    for (int k = 7; k >= 0 && (buf[k] == '\0' || buf[k] == ' '); --k)
        buf[k] = '\0';
    return strcasecmp(buf, s) == 0;
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

static void R_InitTextures(void)
{
    maptexture_t*	mtexture;
    texture_t*		texture;
    mappatch_t*		mpatch;
    texpatch_t*		patch;

    int			i;
    int			j;
    int			k;

    int*		maptex = NULL;
    
    char		name[9];
    
    int*		patchlookup;
    
    int			nummappatches;
    int			offset;
    int			maxoff = 0;

    int*		directory = NULL;
    
//  int			temp1;
//  int			temp2;
//  int			temp3;

    typedef struct
    {
	int lumpnum;
	void *names;
	short nummappatches;
	short summappatches;
	char *name_p;
    } pnameslump_t;

    typedef struct
    {
	int lumpnum;
	int *maptex;
	int maxoff;
	short numtextures;
	short sumtextures;
	short pnamesoffset;
    } texturelump_t;

    pnameslump_t	*pnameslumps = NULL;
    texturelump_t	*texturelumps = NULL, *texturelump;

    int			maxpnameslumps = 1; // PNAMES
    int			maxtexturelumps = 2; // TEXTURE1, TEXTURE2

    int			numpnameslumps = 0;
    int			numtexturelumps = 0;

    // [crispy] allocate memory for the pnameslumps and texturelumps arrays
    pnameslumps = I_Realloc(pnameslumps, maxpnameslumps * sizeof(*pnameslumps));
    texturelumps = I_Realloc(texturelumps, maxtexturelumps * sizeof(*texturelumps));

    // [crispy] make sure the first available TEXTURE1/2 lumps
    // are always processed first
    texturelumps[numtexturelumps++].lumpnum = W_GetNumForName("TEXTURE1");
    if ((i = W_CheckNumForName("TEXTURE2")) != -1)
	texturelumps[numtexturelumps++].lumpnum = i;
    else
	texturelumps[numtexturelumps].lumpnum = -1;

    // [crispy] fill the arrays with all available PNAMES lumps
    // and the remaining available TEXTURE1/2 lumps
    nummappatches = 0;
    for (i = numlumps - 1; i >= 0; i--)
    {
	if (!strncasecmp(lumpinfo[i]->name, "PNAMES", 6))
	{
	    if (numpnameslumps == maxpnameslumps)
	    {
		maxpnameslumps++;
		pnameslumps = I_Realloc(pnameslumps, maxpnameslumps * sizeof(*pnameslumps));
	    }

	    pnameslumps[numpnameslumps].lumpnum = i;
	    pnameslumps[numpnameslumps].names = W_CacheLumpNum(pnameslumps[numpnameslumps].lumpnum, PU_STATIC);
	    pnameslumps[numpnameslumps].nummappatches = LONG(*((int *) pnameslumps[numpnameslumps].names));

	    // [crispy] accumulated number of patches in the lookup tables
	    // excluding the current one
	    pnameslumps[numpnameslumps].summappatches = nummappatches;
	    pnameslumps[numpnameslumps].name_p = (char*)pnameslumps[numpnameslumps].names + 4;

	    // [crispy] calculate total number of patches
	    nummappatches += pnameslumps[numpnameslumps].nummappatches;
	    numpnameslumps++;
	}
	else
	if (!strncasecmp(lumpinfo[i]->name, "TEXTURE", 7))
	{
	    // [crispy] support only TEXTURE1/2 lumps, not TEXTURE3 etc.
	    if (lumpinfo[i]->name[7] != '1' &&
	        lumpinfo[i]->name[7] != '2')
		continue;

	    // [crispy] make sure the first available TEXTURE1/2 lumps
	    // are not processed again
	    if (i == texturelumps[0].lumpnum ||
	        i == texturelumps[1].lumpnum) // [crispy] may still be -1
		continue;

	    if (numtexturelumps == maxtexturelumps)
	    {
		maxtexturelumps++;
		texturelumps = I_Realloc(texturelumps, maxtexturelumps * sizeof(*texturelumps));
	    }

	    // [crispy] do not proceed any further, yet
	    // we first need a complete pnameslumps[] array and need
	    // to process texturelumps[0] (and also texturelumps[1]) as well
	    texturelumps[numtexturelumps].lumpnum = i;
	    numtexturelumps++;
	}
    }

    // [crispy] fill up the patch lookup table
    name[8] = 0;
    patchlookup = Z_Malloc(nummappatches * sizeof(*patchlookup), PU_STATIC, NULL);
    for (i = 0, k = 0; i < numpnameslumps; i++)
    {
	for (j = 0; j < pnameslumps[i].nummappatches; j++)
	{
	    int p;

	    M_StringCopy(name, pnameslumps[i].name_p + j * 8, sizeof(name));
	    p = W_CheckNumForName(name);
	    if (!V_IsPatchLump(p))
	        p = -1;
	    // [crispy] if the name is unambiguous, use the lump we found
	    patchlookup[k++] = p;
	}
    }

    // [crispy] calculate total number of textures
    numtextures = 0;
    for (i = 0; i < numtexturelumps; i++)
    {
	texturelumps[i].maptex = W_CacheLumpNum(texturelumps[i].lumpnum, PU_STATIC);
	texturelumps[i].maxoff = W_LumpLength(texturelumps[i].lumpnum);
	texturelumps[i].numtextures = LONG(*texturelumps[i].maptex);

	// [crispy] accumulated number of textures in the texture files
	// including the current one
	numtextures += texturelumps[i].numtextures;
	texturelumps[i].sumtextures = numtextures;

	// [crispy] link textures to their own WAD's patch lookup table (if any)
	texturelumps[i].pnamesoffset = 0;
	for (j = 0; j < numpnameslumps; j++)
	{
	    // [crispy] both are from the same WAD?
	    if (lumpinfo[texturelumps[i].lumpnum]->wad_file ==
	        lumpinfo[pnameslumps[j].lumpnum]->wad_file)
	    {
		texturelumps[i].pnamesoffset = pnameslumps[j].summappatches;
		break;
	    }
	}
    }

    // [crispy] release memory allocated for patch lookup tables
    for (i = 0; i < numpnameslumps; i++)
    {
	W_ReleaseLumpNum(pnameslumps[i].lumpnum);
    }
    free(pnameslumps);

    // [crispy] pointer to (i.e. actually before) the first texture file
    texturelump = texturelumps - 1; // [crispy] gets immediately increased below

    textures = Z_Malloc (numtextures * sizeof(*textures), PU_STATIC, 0);
    texturecolumnlump = Z_Malloc (numtextures * sizeof(*texturecolumnlump), PU_STATIC, 0);
    texturecolumnofs = Z_Malloc (numtextures * sizeof(*texturecolumnofs), PU_STATIC, 0);
    texturecolumnofs2 = Z_Malloc (numtextures * sizeof(*texturecolumnofs2), PU_STATIC, 0);
    texturecomposite = Z_Malloc (numtextures * sizeof(*texturecomposite), PU_STATIC, 0);
    texturecomposite2 = Z_Malloc (numtextures * sizeof(*texturecomposite2), PU_STATIC, 0);
    texturecompositesize = Z_Malloc (numtextures * sizeof(*texturecompositesize), PU_STATIC, 0);
    texturewidthmask = Z_Malloc (numtextures * sizeof(*texturewidthmask), PU_STATIC, 0);
    texturewidth = Z_Malloc (numtextures * sizeof(*texturewidth), PU_STATIC, 0);
    textureheight = Z_Malloc (numtextures * sizeof(*textureheight), PU_STATIC, 0);
    texturebrightmap = Z_Malloc (numtextures * sizeof(*texturebrightmap), PU_STATIC, 0);

    for (i=0 ; i<numtextures ; i++, directory++)
    {
	if (!(i&63))
	    printf (".");

	// [crispy] initialize for the first texture file lump,
	// skip through empty texture file lumps which do not contain any texture
	while (texturelump == texturelumps - 1 || i == texturelump->sumtextures)
	{
	    // [crispy] start looking in next texture file
	    texturelump++;
	    maptex = texturelump->maptex;
	    maxoff = texturelump->maxoff;
	    directory = maptex+1;
	}
		
	offset = LONG(*directory);

	if (offset > maxoff)
	    I_Error ("R_InitTextures: bad texture directory");
	
	mtexture = (maptexture_t *) ( (byte *)maptex + offset);

	texture = textures[i] =
	    Z_Malloc (sizeof(texture_t)
		      + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
		      PU_STATIC, 0);
	
	texture->width = SHORT(mtexture->width);
    // [PN/JN] Copy texture name from the WAD definition and check if it matches
    // any of the known sky texture names (SKY1–SKY6). If it is a sky,
    // set the texture height to match the first patch's height; otherwise
    // use the height value from the WAD definition.
    memcpy(texture->name, mtexture->name, sizeof(texture->name));
    if (R_IsTextureName(texture->name, "SKY1")
    ||  R_IsTextureName(texture->name, "SKY2")
    ||  R_IsTextureName(texture->name, "SKY3")
    ||  R_IsTextureName(texture->name, "SKY4")
    ||  R_IsTextureName(texture->name, "SKY5")
    ||  R_IsTextureName(texture->name, "SKY6"))
    {
        texture->height = R_GetPatchHeight(i, 0);
    }
    else
    {
        texture->height = SHORT(mtexture->height);
    }
    texture->patchcount = SHORT(mtexture->patchcount);
	
	memcpy (texture->name, mtexture->name, sizeof(texture->name));
	mpatch = &mtexture->patches[0];
	patch = &texture->patches[0];

	// [crispy] initialize brightmaps
	texturebrightmap[i] = R_BrightmapForTexName(texture->name);

	for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
	{
	    short p;
	    patch->originx = SHORT(mpatch->originx);
	    patch->originy = SHORT(mpatch->originy);
	    // [crispy] apply offset for patches not in the
	    // first available patch offset table
	    p = SHORT(mpatch->patch) + texturelump->pnamesoffset;
	    // [crispy] catch out-of-range patches
	    if (p < nummappatches)
		patch->patch = patchlookup[p];
	    if (patch->patch == -1 || p >= nummappatches)
	    {
		char	texturename[9];
		texturename[8] = '\0';
		memcpy (texturename, texture->name, 8);
		// [crispy] make non-fatal
		fprintf (stderr, "R_InitTextures: Missing patch in texture %s\n",
			 texturename);
		patch->patch = W_CheckNumForName("WIPCNT"); // [crispy] dummy patch
	    }
	}		
	texturecolumnlump[i] = Z_Malloc (texture->width*sizeof(**texturecolumnlump), PU_STATIC,0);
	texturecolumnofs[i] = Z_Malloc (texture->width*sizeof(**texturecolumnofs), PU_STATIC,0);
	texturecolumnofs2[i] = Z_Malloc (texture->width*sizeof(**texturecolumnofs2), PU_STATIC,0);

	j = 1;
	while (j*2 <= texture->width)
	    j<<=1;

	texturewidthmask[i] = j-1;
	textureheight[i] = texture->height<<FRACBITS;

	// [crispy] texture width for wrapping column getter function
	texturewidth[i] = texture->width;
    }

    Z_Free(patchlookup);

    // [crispy] release memory allocated for texture files
    for (i = 0; i < numtexturelumps; i++)
    {
	W_ReleaseLumpNum(texturelumps[i].lumpnum);
    }
    free(texturelumps);
    
    // Precalculate whatever possible.	

    for (i=0 ; i<numtextures ; i++)
    {
	R_GenerateLookup (i);
	if (!(i & 31))
	    ST_Progress();
    }
    
    // Create translation table for global animation.
    texturetranslation = Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC, 0);
    
    for (i=0 ; i<numtextures ; i++)
	texturetranslation[i] = i;
}


// -----------------------------------------------------------------------------
// R_InitFlats
// -----------------------------------------------------------------------------

static void R_InitFlats (void)
{
    firstflat = W_GetNumForName("F_START") + 1;
    lastflat = W_GetNumForName("F_END") - 1;
    numflats = lastflat - firstflat + 1;

    // Create translation table for global animation.
    flattranslation = Z_Malloc((numflats + 1) * sizeof(*flattranslation), PU_STATIC, 0);

    for (int i = 0 ; i < numflats ; i++)
        flattranslation[i] = i;
}

// -----------------------------------------------------------------------------
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
// -----------------------------------------------------------------------------

static void R_InitSpriteLumps (void)
{
    patch_t *patch;

    firstspritelump = W_GetNumForName("S_START") + 1;
    lastspritelump = W_GetNumForName("S_END") - 1;

    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = Z_Malloc(numspritelumps * sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = Z_Malloc(numspritelumps * sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = Z_Malloc(numspritelumps * sizeof(*spritetopoffset), PU_STATIC, 0);

    for (int i = 0 ; i < numspritelumps ; i++)
    {
        if (!(i & 127))
            ST_Progress();

        patch = W_CacheLumpNum(firstspritelump + i, PU_CACHE);
        spritewidth[i] = SHORT(patch->width) << FRACBITS;
        spriteoffset[i] = SHORT(patch->leftoffset) << FRACBITS;
        spritetopoffset[i] = SHORT(patch->topoffset) << FRACBITS;
    }
}

// -----------------------------------------------------------------------------
// R_InitColormaps
//
// [PN] Macros to optimize and standardize color calculations in the R_InitColormaps.
//
// CALC_INTENSITY calculates the RGB components from playpal based on intensity settings.
//
// CALC_SATURATION applies saturation correction using values from CALC_INTENSITY along 
// with the a_hi and a_lo coefficients.
//
// CALC_CONTRAST adjusts the contrast of the red, green, and blue channels
// based on the vid_contrast variable. A value of 1.0 preserves the original contrast,
// while values below 1.0 reduce it, and values above 1.0 enhance it. The calculation
// ensures that channel values remain within the valid range [0, 255].
//
// CALC_COLORBLIND applies a colorblindness correction to the red, green, 
// and blue channels based on the provided transformation matrix. The matrix defines
// how much of each original channel (red, green, blue) contributes to the final values.
// This process simulates how colors are perceived by individuals with various types 
// of colorblindness, such as protanopia, deuteranopia, or tritanopia.
// The calculation ensures that the resulting channel values remain within the valid 
// range [0, 255], preserving consistency and avoiding overflow or underflow.
//
// Also, thanks Alaux!
// -----------------------------------------------------------------------------

#define CALC_INTENSITY(pal, playpal, index) \
    { pal[0] = playpal[3 * (index) + 0] * vid_r_intensity; \
      pal[1] = playpal[3 * (index) + 1] * vid_g_intensity; \
      pal[2] = playpal[3 * (index) + 2] * vid_b_intensity; }

#define CALC_SATURATION(channels, pal, a_hi, a_lo) \
    { const float one_minus_a_hi = 1.0f - a_hi; \
      channels[0] = (byte)(one_minus_a_hi * pal[0] + a_lo * (pal[1] + pal[2])); \
      channels[1] = (byte)(one_minus_a_hi * pal[1] + a_lo * (pal[0] + pal[2])); \
      channels[2] = (byte)(one_minus_a_hi * pal[2] + a_lo * (pal[0] + pal[1])); }

#define CALC_CONTRAST(channels, contrast) \
    { const float contrast_adjustment = 128 * (1.0f - contrast); \
      channels[0] = (int)BETWEEN(0, 255, (int)(contrast * channels[0] + contrast_adjustment)); \
      channels[1] = (int)BETWEEN(0, 255, (int)(contrast * channels[1] + contrast_adjustment)); \
      channels[2] = (int)BETWEEN(0, 255, (int)(contrast * channels[2] + contrast_adjustment)); }

#define CALC_COLORBLIND(channels, matrix) \
    { const byte r = channels[0]; \
      const byte g = channels[1]; \
      const byte b = channels[2]; \
      channels[0] = (byte)BETWEEN(0, 255, (int)(matrix[0][0] * r + matrix[0][1] * g + matrix[0][2] * b)); \
      channels[1] = (byte)BETWEEN(0, 255, (int)(matrix[1][0] * r + matrix[1][1] * g + matrix[1][2] * b)); \
      channels[2] = (byte)BETWEEN(0, 255, (int)(matrix[2][0] * r + matrix[2][1] * g + matrix[2][2] * b)); }


// [crispy] Our own function to generate colormaps for normal and foggy levels.
void R_InitTrueColormaps(char *current_colormap)
{
	int c, i, j = 0;
	byte r, g, b;

	const byte *const playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
	const byte *const colormap = W_CacheLumpName(current_colormap, PU_STATIC);

	// [JN] Saturation floats, high and low.
	// If saturation has been modified (< 100), set high and low
	// values according to saturation level. Sum of r,g,b channels
	// and floats must be 1.0 to get proper colors.
	const float a_hi = vid_saturation < 100 ? I_SaturationPercent[vid_saturation] : 0;
	const float a_lo = vid_saturation < 100 ? (a_hi / 2) : 0;

	// [JN] Allocate colormaps[] array once with max size (256 + 1) to avoid
	// repeated reallocation during subsequent function calls.
	static boolean colormaps_allocated = false;
	if (!colormaps_allocated)
	{
		colormaps = (lighttable_t*) Z_Malloc((256 + 1) * 256 * sizeof(lighttable_t), PU_STATIC, 0);
		colormaps_allocated = true;
	}

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

    if (vid_truecolor)
    {
        // [crispy] If level is allowed to use full bright mode, fade colormaps to 
        // black (0) color. Otherwise, fade to gray (147) to emulate FOGMAP table.
        const int fade_color = LevelUseFullBright ? 0 : 147;
    
        // [PN] Precompute gamma-corrected base RGB for fade target.
        // Use neutral gray (147,147,147) directly instead of sampling PLAYPAL.
        const byte *const restrict gtab = gammatable[vid_gamma];
        const int fade_r = gtab[fade_color];
        const int fade_g = gtab[fade_color];
        const int fade_b = gtab[fade_color];
        const float one_minus_a_hi = 1.0f - a_hi;
    
        // base_gamma[i][0..2] are gamma-corrected channels for index "i_base"
        byte base_gamma[256][3];
    
        for (int i = 0; i < 256; ++i)
        {
            // Keep semantics: use COLORMAP[0] mapping as base index
            const byte k = colormap[i];
    
            // Intensity
            float pr = playpal[3 * k + 0] * vid_r_intensity;
            float pg = playpal[3 * k + 1] * vid_g_intensity;
            float pb = playpal[3 * k + 2] * vid_b_intensity;
    
            // Saturation (branch-free)
            float r = one_minus_a_hi * pr + a_lo * (pg + pb);
            float g = one_minus_a_hi * pg + a_lo * (pr + pb);
            float b = one_minus_a_hi * pb + a_lo * (pr + pg);
    
            // Gamma to bytes (reuse later for every light level)
            base_gamma[i][0] = gtab[(byte)r];
            base_gamma[i][1] = gtab[(byte)g];
            base_gamma[i][2] = gtab[(byte)b];
        }
    
        // Contrast and colorblind are applied after light interpolation
        const double cMul = (double)vid_contrast;
        const double cAdj = 128.0 * (1.0 - (double)vid_contrast);
        const double (*cbm)[3] = colorblind_matrix[a11y_colorblind];
    
        for (int c = 0; c < NUMCOLORMAPS; ++c)
        {
            const double scale = (double)c / (double)NUMCOLORMAPS;
            const double k0    = 1.0 - scale;
            const double kF    = scale;  // fade factor
    
            lighttable_t *const restrict row = &colormaps[c * 256];
    
            for (int i = 0; i < 256; ++i)
            {
                // 1) Light fade in gamma space and fade_color
                double R = (double)base_gamma[i][0] * k0 + fade_r * kF;
                double G = (double)base_gamma[i][1] * k0 + fade_g * kF;
                double B = (double)base_gamma[i][2] * k0 + fade_b * kF;
    
                // 2) Contrast (affine)
                R = cMul * R + cAdj;
                G = cMul * G + cAdj;
                B = cMul * B + cAdj;
    
                // 3) Colorblind transform (matrix is double)
                double r2 = cbm[0][0]*R + cbm[0][1]*G + cbm[0][2]*B;
                double g2 = cbm[1][0]*R + cbm[1][1]*G + cbm[1][2]*B;
                double b2 = cbm[2][0]*R + cbm[2][1]*G + cbm[2][2]*B;
    
                const byte r8 = (byte)BETWEEN(0, 255, (int)r2);
                const byte g8 = (byte)BETWEEN(0, 255, (int)g2);
                const byte b8 = (byte)BETWEEN(0, 255, (int)b2);
    
                row[i] = 0xff000000 | (r8 << 16) | (g8 << 8) | b8;
            }
        }
    }
	else
	{
		for (c = 0; c < NUMCOLORMAPS; c++)
		{
			for (i = 0; i < 256; i++)
			{
				// [PN] Apply intensity and saturation corrections
				static byte pal[3];
				static byte channels[3];

				CALC_INTENSITY(pal, playpal, colormap[c * 256 + i]);
				CALC_SATURATION(channels, pal, a_hi, a_lo);
				CALC_CONTRAST(channels, vid_contrast);
				CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);

				r = gammatable[vid_gamma][channels[0]] & ~3;
				g = gammatable[vid_gamma][channels[1]] & ~3;
				b = gammatable[vid_gamma][channels[2]] & ~3;

				colormaps[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
	}

	W_ReleaseLumpName(current_colormap);

	if (!pal_color)
	{
		pal_color = (pixel_t*) Z_Malloc(256 * sizeof(pixel_t), PU_STATIC, 0);
	}

	for (i = 0, j = 0; i < 256; i++)
	{
		// [PN] Apply intensity and saturation corrections
		static byte pal[3];
		static byte channels[3];

		CALC_INTENSITY(pal, playpal, i);
		CALC_SATURATION(channels, pal, a_hi, a_lo);
		CALC_CONTRAST(channels, vid_contrast);
		CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);

		r = gammatable[vid_gamma][channels[0]];
		g = gammatable[vid_gamma][channels[1]];
		b = gammatable[vid_gamma][channels[2]];

		pal_color[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
	}

	W_ReleaseLumpName("PLAYPAL");

	// [JN] Recalculate shadow alpha value for shadowed patches based on contrast.
	// 0xA0 (160) represents 62.75% darkening. Ensure the result stays within 0-255.
	shadow_alpha = (uint8_t)BETWEEN(0, 255 - (32 * vid_contrast), 0xA0 / vid_contrast);
}

// -----------------------------------------------------------------------------
// R_InitHSVColors
// [crispy] initialize color translation and color strings tables
// -----------------------------------------------------------------------------

static void R_InitHSVColors (void)
{
    char c[3];
    int i, j;
    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

    if (!crstr)
        crstr = I_Realloc(NULL, CRMAX * sizeof(*crstr));

    // [crispy] CRMAX - 2: don't override the original GREN and BLUE2 Boom tables
    for (i = 0; i < CRMAX - 2; i++)
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


// -----------------------------------------------------------------------------
// R_InitData
// Locates all the lumps that will be used by all views.
// -----------------------------------------------------------------------------

void R_InitData (void)
{
    R_InitFlats();
    R_InitTextures();
    R_InitSpriteLumps();
    // [crispy] Generate initial colormaps[] array from standard COLORMAP.
    R_InitTrueColormaps("COLORMAP");
    // [crispy] initialize color translation and color string tables
    R_InitHSVColors();
    // [JN] Initialize and compose translucency tables.
    I_InitTCTransMaps();
    I_InitPALTransMaps();
}

// -----------------------------------------------------------------------------
// R_GetPatchHeight
// [crispy] Used to grab actual height of sky textures
// -----------------------------------------------------------------------------

int R_GetPatchHeight(int texture_num, int patch_index)
{
    texpatch_t *texpatch = &textures[texture_num]->patches[patch_index];
    patch_t *patch = W_CacheLumpNum(texpatch->patch, PU_CACHE);

    return  SHORT(patch->height);
}

// -----------------------------------------------------------------------------
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
// -----------------------------------------------------------------------------

int R_FlatNumForName(const char *name)
{
    int i = W_CheckNumForName(name);

    if (i == -1)
    {
        // [crispy] make missing flat non-fatal
        // and fix absurd flat name in error message
        fprintf (stderr, "R_FlatNumForName: %.8s not found\n", name);
        // [crispy] since there is no "No Flat" marker,
        // render missing flats as SKY
        return skyflatnum;
    }

    return i - firstflat;
}

// -----------------------------------------------------------------------------
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
// -----------------------------------------------------------------------------

int R_CheckTextureNumForName (const char *name)
{
    // "NoTexture" marker.
    if (name[0] == '-')	
        return 0;

    for (int i = 0 ; i < numtextures ; i++)
        if (!strncasecmp(textures[i]->name, name, 8))
            return i;

    return -1;
}

// -----------------------------------------------------------------------------
// R_TextureNumForName
// Calls R_CheckTextureNumForName, aborts with error message.
// -----------------------------------------------------------------------------

int R_TextureNumForName(const char *name)
{
    int i = R_CheckTextureNumForName(name);

    if (i == -1)
    {
        // [crispy] make missing texture non-fatal
        // and fix absurd texture name in error message
        fprintf (stderr, "R_TextureNumForName: %.8s not found\n", name);
        return 0;
    }
    return i;
}

// -----------------------------------------------------------------------------
// R_PrecacheLevel
//  Preloads all relevant graphics for the level.
//
// [PN] Optimized and refactored R_PrecacheLevel:
// - Uses a single zero-initialized buffer sized for the largest resource type.
// - Replaces multiple memset/malloc with one calloc and reuses the buffer across all stages.
// - All loops are forward for better cache efficiency.
// -----------------------------------------------------------------------------

#define MAX3(a,b,c) (((a)>(b))?((a)>(c)?(a):(c)):((b)>(c)?(b):(c)))

void R_PrecacheLevel(void)
{
    if (demoplayback)
        return;

    const size_t maxsize = MAX3(numtextures, numflats, numsprites);
    byte *restrict hitlist = (byte*)calloc(maxsize, 1);

    // Precache flats
    for (int i = 0; i < numsectors; ++i)
    {
        hitlist[sectors[i].floorpic] = 1;
        hitlist[sectors[i].ceilingpic] = 1;
    }
    for (int i = 0; i < numflats; ++i)
    {
        if (hitlist[i])
        {
            W_CacheLumpNum(firstflat + i, PU_CACHE);
        }
    }

    memset(hitlist, 0, maxsize);

    // Precache textures
    for (int i = 0; i < numsides; ++i)
    {
        hitlist[sides[i].bottomtexture] = 1;
        hitlist[sides[i].toptexture] = 1;
        hitlist[sides[i].midtexture] = 1;
    }

    hitlist[Sky1Texture] = 1;
    hitlist[Sky2Texture] = 1;

    for (int i = 0; i < numtextures; ++i)
    {
        if (hitlist[i])
        {
            texture_t * const texture = textures[i];
            for (int j = 0; j < texture->patchcount; ++j)
            {
                W_CacheLumpNum(texture->patches[j].patch, PU_CACHE);
            }
        }
    }

    memset(hitlist, 0, maxsize);

    // Precache sprites
    for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function == P_MobjThinker)
        {
            hitlist[((const mobj_t*)th)->sprite] = 1;
        }
    }

    for (int i = 0; i < numsprites; ++i)
    {
        if (hitlist[i])
        {
            for (int j = 0; j < sprites[i].numframes; ++j)
            {
                const short *sflump = sprites[i].spriteframes[j].lump;
                for (int k = 0; k < 8; ++k)
                {
                    W_CacheLumpNum(firstspritelump + sflump[k], PU_CACHE);
                }
            }
        }
    }

    free(hitlist);
}
