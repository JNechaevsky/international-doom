//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Polina "Aura" N.
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

#include "deh_str.h"
#include "doomdef.h"
#include "i_swap.h"
#include "i_system.h"
#include "m_misc.h"
#include "p_local.h"
#include "v_trans.h"
#include "v_video.h"
#include "z_zone.h"


//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

typedef PACKED_STRUCT (
{
    short originx;
    short originy;
    short patch;
    short stepdir;
    short colormap;
}) mappatch_t;

//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//

typedef PACKED_STRUCT (
{
    char  name[8];
    int   masked;
    short width;
    short height;
    int   obsolete;
    short patchcount;
    mappatch_t patches[1];
}) maptexture_t;

// A single patch from a texture definition,
//  basically a rectangular area within
//  the texture rectangle.

typedef struct
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    short originx;
    short originy;
    int	  patch;
} texpatch_t;

// A maptexturedef_t describes a rectangular texture,
//  which is composed of one or more mappatch_t structures
//  that arrange graphic patches.

typedef struct texture_s texture_t;

struct texture_s
{
    // Keep name for switch changing, etc.
    char  name[8];
    short width;
    short height;

    // Index in textures list
    int index;

    // Next in hash table chain
    texture_t *next;
    
    // All the patches[patchcount]
    //  are drawn back to front into the cached texture.
    short patchcount;
    texpatch_t patches[1];
};

int firstflat;
int lastflat;
int numflats;

int firstspritelump;
int lastspritelump;
int numspritelumps;

int numtextures;
texture_t **textures;
static texture_t **textures_hashtable;

static int *texturewidthmask;
static int *texturewidth;             // [crispy] texture width for wrapping column getter function
fixed_t    *textureheight;            // [crispy] texture height for Tutti-Frutti fix
static int *texturecompositesize;
static short    **texturecolumnlump;
static unsigned **texturecolumnofs;   // [crispy] column offsets for composited translucent mid-textures on 2S walls
static unsigned **texturecolumnofs2;  // [crispy] column offsets for composited opaque textures
static byte **texturecomposite;       // [crispy] composited translucent mid-textures on 2S walls
static byte **texturecomposite2;      // [crispy] composited opaque textures
const  byte **texturebrightmap;       // [crispy] brightmaps

// for global animation
int *flattranslation;
int *texturetranslation;

// needed for pre rendering
fixed_t *spritewidth;
fixed_t *spriteoffset;
fixed_t *spritetopoffset;

lighttable_t *colormaps;


//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//


// -----------------------------------------------------------------------------
// R_DrawColumnInCache
//  [PN] Clips and draws a patch column into the cache with branch-reduced
//  clipping and restrict hints for faster execution. Based on Killough’s
//  rewrite that fixed the Medusa bug.
// -----------------------------------------------------------------------------

static void R_DrawColumnInCache
(
    const column_t *restrict patch,
    byte *restrict           cache,
    int                      originy,
    int                      cacheheight,
    byte *restrict           marks
)
{
    // Aliasing hints + const where applicable
    const column_t *restrict col   = (const column_t *)patch;
    byte           *restrict dst   = cache;
    byte           *restrict mark  = marks;

    // Hot locals, declared near use
    int top = -1;

    // Single-pass, clamp with start/end to reduce branches
    while (col->topdelta != 0xff)
    {
        const int topdelta = col->topdelta;

        // [crispy] support for DeePsea tall patches
        if (topdelta <= top)
        {
            top += topdelta;
        }
        else
        {
            top = topdelta;
        }

        const int count = col->length;
        const int start = originy + top;
        const int end   = start + count;

        // Clip once; compute copy window [dst_start, dst_end)
        const int dst_start = start < 0 ? 0 : start;
        const int dst_end   = end   > cacheheight ? cacheheight : end;
        const int n         = dst_end - dst_start;

        if (n > 0)
        {
            // Source pointer adjusted by the amount we clipped at the top
            const byte *restrict src = (const byte *)col + 3 + (dst_start - start);

            // memcpy + memset; aliasing is restricted for better codegen
            memcpy(dst  + dst_start, src,  (size_t)n);
            memset(mark + dst_start, 0xFF, (size_t)n);
        }

        // Advance to next post (length bytes + 4 header bytes)
        col = (const column_t *)((const byte *)col + col->length + 4);
    }
}

// -----------------------------------------------------------------------------
// R_GenerateComposite
//  [PN] Builds composite columns for a texture and reconstructs true posts from
//  transparency marks with fewer branches and restrict-qualified pointers.
//  Based on Killough’s rewrite that fixed the Medusa bug.
// -----------------------------------------------------------------------------

static void R_GenerateComposite (int texnum)
{
    texture_t *const texture = textures[texnum];
    const int width  = texture->width;
    const int height = texture->height;

    byte *const block = Z_Malloc(texturecompositesize[texnum], PU_STATIC, &texturecomposite[texnum]);
    byte *const block2 = Z_Malloc((size_t)width * (size_t)height, PU_STATIC, &texturecomposite2[texnum]);

    short    *restrict collump = texturecolumnlump[texnum];
    unsigned *restrict colofs  = texturecolumnofs [texnum];
    unsigned *restrict colofs2 = texturecolumnofs2[texnum];

    // marks[y + x*height] == 0xFF for opaque pixels of column x
    byte *const marks = (byte *)calloc((size_t)width, (size_t)height);

    // [crispy] initialize composite background to palette index 0 (usually black)
    memset(block, 0, (size_t)texturecompositesize[texnum]);

    // 1) Composite all contributing patches into 'block' and stamp marks[]
    for (int i = 0; i < texture->patchcount; ++i)
    {
        const texpatch_t *const patch = &texture->patches[i];
        const patch_t *const realpatch = W_CacheLumpNum(patch->patch, PU_CACHE);

        int x1 = patch->originx;
        int x2 = x1 + SHORT(realpatch->width);

        if (x1 < 0)        x1 = 0;
        if (x2 > width)    x2 = width;
        if (x1 >= x2)      continue;

        // Column offsets base, indexed by absolute x
        const int *const cofs_base = realpatch->columnofs - x1;
        const byte *const rp_base  = (const byte *)realpatch;

        for (int x = x1; x < x2; ++x)
        {
            const unsigned ofs = (unsigned)LONG(cofs_base[x]);
            const column_t *patchcol = (const column_t *)(rp_base + ofs);

            // [crispy] single-patched columns are normally not composited,
            // but we do composite all columns; single-patched use originy=0.
            const int originy = (collump[x] >= 0) ? 0 : patch->originy;

            R_DrawColumnInCache(patchcol,
                                block + colofs[x],
                                originy,
                                height,
                                marks + (size_t)x * (size_t)height);
        }
    }

    // 2) Convert composited columns into true posts (Medusa-safe) and
    //    copy an opaque linear copy into block2 for fast sampling.
    byte *const source = (byte *)I_Realloc(NULL, (size_t)height); // temporary column

    for (int i = 0; i < width; ++i)
    {
        column_t *col = (column_t *)(block + colofs[i] - 3); // cached column header
        const byte *restrict mark = marks + (size_t)i * (size_t)height;

        // Save the fully composited payload for shuffling
        memcpy(source, (const byte *)col + 3, (size_t)height);

        // [crispy] copy composited columns to opaque texture
        memcpy(block2 + colofs2[i], source, (size_t)height);

        // Reconstruct posts by scanning transparency marks
        int j = 0;
        int abstop, reltop = 0;
        boolean relative = false;

        for (;;)
        {
            // Skip transparent cells; reltop caps relative topdelta at < 254
            while (j < height && reltop < 254 && !mark[j])
            {
                ++j;
                ++reltop;
            }

            if (j >= height)
            {
                col->topdelta = -1; // end-of-column marker (0xFF)
                break;
            }

            // First 0..253 pixels: absolute topdelta; after that: relative
            col->topdelta = relative ? reltop : j;
            abstop = j;

            // Once we pass 254 boundary, switch to relative topdelta
            if (abstop >= 254)
            {
                relative = true;
                reltop = 0;
            }

            // Count opaque run (bounded by height and reltop < 254)
            unsigned len = 0;
            while (j < height && reltop < 254 && mark[j])
            {
                ++j;
                ++reltop;
                ++len;
            }

            col->length = len; // (Killough 12/98: 32-bit len in builder, truncates to byte in struct)
            memcpy((byte *)col + 3, source + abstop, len);

            // Next post
            col = (column_t *)((byte *)col + len + 4);
        }
    }

    free(source);
    free(marks);

    // Purgable from zone memory now that caches are built
    Z_ChangeTag(block,  PU_CACHE);
    Z_ChangeTag(block2, PU_CACHE);
}

// -----------------------------------------------------------------------------
// R_GenerateLookup
//  [PN] Rewritten for fewer passes and better aliasing hints.
//  Keeps Medusa bug protections and Crispy semantics intact.
// -----------------------------------------------------------------------------

static void R_GenerateLookup (int texnum)
{
    texture_t *const texture = textures[texnum];
    const int width  = texture->width;
    const int height = texture->height;

    // Composited buffers not yet built.
    texturecomposite [texnum] = 0;
    texturecomposite2[texnum] = 0;
    texturecompositesize[texnum] = 0;

    short    *restrict collump  = texturecolumnlump[texnum];
    unsigned *restrict colofs   = texturecolumnofs [texnum];
    unsigned *restrict colofs2  = texturecolumnofs2[texnum];

    // Use byte for patchcount (as before) and 16-bit for postcount to avoid overflow.
    unsigned char  *restrict patchcount = (unsigned char *) Z_Malloc((size_t)width, PU_STATIC, NULL);
    unsigned short *restrict postcount  = (unsigned short*) Z_Malloc((size_t)width * sizeof(unsigned short), PU_STATIC, NULL);

    memset(patchcount, 0, (size_t)width);
    memset(postcount,  0, (size_t)width * sizeof(unsigned short));

    int err = 0;

    // Single pass over all patches:
    // - accumulate patchcount[x]
    // - prime collump[x] / colofs[x] (last writer wins, vanilla behavior)
    // - accumulate total number of posts per column (for Medusa-safe csize)
    for (int i = 0; i < texture->patchcount; ++i)
    {
        const texpatch_t *const p = &texture->patches[i];
        const int pat = p->patch;
        const patch_t *const realpatch = W_CacheLumpNum(pat, PU_CACHE);

        int x1 = p->originx;
        int x2 = x1 + SHORT(realpatch->width);

        if (x1 < 0)
        {
            x1 = 0;
        }
        if (x2 > width)
        {
            x2 = width;
        }
        // Column-ofs table base; allow direct index by screen x
        const int *const cofs_base = realpatch->columnofs - p->originx;

        // Absolute column size guard (Killough 12/98).
        const unsigned limit = (unsigned)height * 3u + 3u;

        for (int x = x1; x < x2; ++x)
        {
            // Bump number of patches touching this column.
            ++patchcount[x];

            // Keep last lump/offset (vanilla search order).
            collump[x] = (short)pat;
            colofs [x] = (unsigned)LONG(cofs_base[x]) + 3u;

            // [PN] Count posts now (previously a second pass):
            // Still Medusa-safe: walk posts until 0xff, bounded by 'limit'.
            const column_t *col = (const column_t *)((const byte *)realpatch + LONG(cofs_base[x]));
            const byte *const base = (const byte *)col;

            while (col->topdelta != 0xff)
            {
                ++postcount[x];

                // Guard against malformed columns.
                const unsigned delta = (unsigned)((const byte *)col - base);
                if (delta > limit)
                {
                    break;
                }
                col = (const column_t *)((const byte *)col + col->length + 4);
            }
        }
    }

    // Build per-column directory and total composite size.
    // Keep Crispy behavior: we generate composites for ALL columns.
    int csize = 0;

    for (int x = 0; x < width; ++x)
    {
        if (!patchcount[x] && !err++)
        {
            // Non-fatal, consistent with modified Crispy behavior.
            printf("R_GenerateLookup: column without a patch (%.8s)\n", texture->name); // [PN] warn once
        }

        // [PN] Use cached block for multi-patched or patch-less columns.
        // Single-patch columns keep collump[x] != -1 to preserve originY=0 path during composite.
        if (patchcount[x] > 1 || !patchcount[x])
        {
            collump[x] = -1;
        }

        // Column header starts at csize+3 (3 bytes header).
        colofs[x] = (unsigned)csize + 3u;

        // Medusa-safe: 4 bytes per post + trailing stop byte.
        // postcount[x] is total across all contributing patches.
        csize += 4 * (int)postcount[x] + 5;

        // Add room for the column payload (height bytes).
        csize += height;

        // Opaque composite (linear storage by columns * height).
        colofs2[x] = (unsigned)(x * height);
    }

    texturecompositesize[texnum] = csize;

    Z_Free(patchcount);
    Z_Free(postcount);
}

// -----------------------------------------------------------------------------
// R_GetColumn
//  [crispy] wrapping column getter function for any non-power-of-two textures
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
//  [crispy] wrapping column getter function for composited translucent mid-textures on 2S walls
// -----------------------------------------------------------------------------

byte *R_GetColumnMod (int tex, int col)
{
    while (col < 0)
        col += texturewidth[tex];

    col %= texturewidth[tex];

    if (!texturecomposite[tex])
        R_GenerateComposite(tex);

    const int ofs = texturecolumnofs[tex][col];

    return texturecomposite[tex] + ofs;
}

// -----------------------------------------------------------------------------
// GenerateTextureHashTable
// -----------------------------------------------------------------------------

static void GenerateTextureHashTable(void)
{
    textures_hashtable = Z_Malloc(sizeof(texture_t *) * numtextures, PU_STATIC, 0);
    memset(textures_hashtable, 0, sizeof(texture_t *) * numtextures);

    // Add all textures to hash table

    for (int i = 0; i < numtextures; ++i)
    {
        // Store index

        textures[i]->index = i;

        // Vanilla Doom does a linear search of the texures array
        // and stops at the first entry it finds.  If there are two
        // entries with the same name, the first one in the array
        // wins. The new entry must therefore be added at the end
        // of the hash chain, so that earlier entries win.

        const int key = W_LumpNameHash(textures[i]->name) % numtextures;
        texture_t **rover = &textures_hashtable[key];

        while (*rover != NULL)
        {
            rover = &(*rover)->next;
        }

        // Hook into hash table

        textures[i]->next = NULL;
        *rover = textures[i];
    }
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

//------------------------------------------------------------------------------
// R_InitTextures
//  Initializes the texture list with the textures from the world map.
//  [crispy] partly rewritten to merge PNAMES and TEXTURE1/2 lumps.
//------------------------------------------------------------------------------

void R_InitTextures (void)
{
    // Working pointers
    texture_t *texture;
    texpatch_t *patch;

    // Scratch
    const int *restrict maptex = NULL;
    const int *restrict directory = NULL;

    char name[9];

    int *restrict patchlookup;

    int nummappatches = 0;
    int offset;
    int maxoff = 0;

    // [crispy] local helper structs
    typedef struct
    {
        int lumpnum;
        const void *names;
        short nummappatches;
        short summappatches;
        const char *name_p;
    } pnameslump_t;

    typedef struct
    {
        int lumpnum;
        const int *maptex;
        int maxoff;
        short numtextures;
        short sumtextures;
        short pnamesoffset;
    } texturelump_t;

    pnameslump_t *restrict pnameslumps = NULL;
    texturelump_t *restrict texturelumps = NULL;
    texturelump_t *texturelump;

    int maxpnameslumps = 1; // PNAMES
    int maxtexturelumps = 2; // TEXTURE1, TEXTURE2

    int numpnameslumps = 0;
    int numtexturelumps = 0;

    // [crispy] allocate memory for the pnameslumps and texturelumps arrays
    pnameslumps  = I_Realloc(pnameslumps,  maxpnameslumps  * sizeof(*pnameslumps));
    texturelumps = I_Realloc(texturelumps, maxtexturelumps * sizeof(*texturelumps));

    // [crispy] make sure the first available TEXTURE1/2 lumps are always processed first
    texturelumps[numtexturelumps++].lumpnum = W_GetNumForName("TEXTURE1");
    {
        const int t2 = W_CheckNumForName(DEH_String("TEXTURE2"));
        if (t2 != -1)
        {
            texturelumps[numtexturelumps++].lumpnum = t2;
        }
        else
        {
            texturelumps[numtexturelumps].lumpnum = -1;
        }
    }

    // [crispy] fill the arrays with all available PNAMES lumps
    // and the remaining available TEXTURE1/2 lumps
    for (int i = numlumps - 1; i >= 0; --i)
    {
        if (!strncasecmp(lumpinfo[i]->name, DEH_String("PNAMES"), 6))
        {
            if (numpnameslumps == maxpnameslumps)
            {
                maxpnameslumps++;
                pnameslumps = I_Realloc(pnameslumps, maxpnameslumps * sizeof(*pnameslumps));
            }

            pnameslumps[numpnameslumps].lumpnum = i;
            pnameslumps[numpnameslumps].names = W_CacheLumpNum(pnameslumps[numpnameslumps].lumpnum, PU_STATIC);
            pnameslumps[numpnameslumps].nummappatches = LONG(*((const int *) pnameslumps[numpnameslumps].names));

            // [crispy] accumulated number of patches in the lookup tables
            // excluding the current one
            pnameslumps[numpnameslumps].summappatches = nummappatches;
            pnameslumps[numpnameslumps].name_p = (const char *)pnameslumps[numpnameslumps].names + 4;

            // [crispy] calculate total number of patches
            nummappatches += pnameslumps[numpnameslumps].nummappatches;
            numpnameslumps++;
        }
        else if (!strncasecmp(lumpinfo[i]->name, DEH_String("TEXTURE"), 7))
        {
            // [crispy] support only TEXTURE1/2 lumps, not TEXTURE3 etc.
            if (lumpinfo[i]->name[7] != '1' && lumpinfo[i]->name[7] != '2')
            {
                continue;
            }

            // [crispy] make sure the first available TEXTURE1/2 lumps are not processed again
            if (i == texturelumps[0].lumpnum
            ||  i == texturelumps[1].lumpnum) // [crispy] may still be -1
            {
                continue;
            }

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
    name[8] = '\0';
    patchlookup = Z_Malloc((size_t)nummappatches * sizeof(*patchlookup), PU_STATIC, NULL);

    for (int i = 0, k = 0; i < numpnameslumps; ++i)
    {
        for (int j = 0; j < pnameslumps[i].nummappatches; ++j)
        {
            int p;

            M_StringCopy(name, pnameslumps[i].name_p + j * 8, sizeof(name));
            p = W_CheckNumForName(name);
            if (!V_IsPatchLump(p))
            {
                p = -1;
            }

            // [crispy] if the name is unambiguous, use the lump we found
            patchlookup[k++] = p;
        }
    }

    // [crispy] calculate total number of textures
    numtextures = 0;
    for (int i = 0; i < numtexturelumps; ++i)
    {
        texturelumps[i].maptex    = W_CacheLumpNum(texturelumps[i].lumpnum, PU_STATIC);
        texturelumps[i].maxoff    = W_LumpLength(texturelumps[i].lumpnum);
        texturelumps[i].numtextures = LONG(*texturelumps[i].maptex);

        // [crispy] accumulated number of textures in the texture files
        // including the current one
        numtextures += texturelumps[i].numtextures;
        texturelumps[i].sumtextures = numtextures;

        // [crispy] link textures to their own WAD's patch lookup table (if any)
        texturelumps[i].pnamesoffset = 0;
        for (int j = 0; j < numpnameslumps; ++j)
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
    for (int i = 0; i < numpnameslumps; ++i)
    {
        W_ReleaseLumpNum(pnameslumps[i].lumpnum);
    }
    free(pnameslumps);

    // [crispy] pointer to (i.e. actually before) the first texture file
    texturelump = texturelumps - 1; // [crispy] gets immediately increased below

    textures             = Z_Malloc(numtextures * sizeof(*textures),             PU_STATIC, 0);
    texturecolumnlump    = Z_Malloc(numtextures * sizeof(*texturecolumnlump),    PU_STATIC, 0);
    texturecolumnofs     = Z_Malloc(numtextures * sizeof(*texturecolumnofs),     PU_STATIC, 0);
    texturecolumnofs2    = Z_Malloc(numtextures * sizeof(*texturecolumnofs2),    PU_STATIC, 0);
    texturecomposite     = Z_Malloc(numtextures * sizeof(*texturecomposite),     PU_STATIC, 0);
    texturecomposite2    = Z_Malloc(numtextures * sizeof(*texturecomposite2),    PU_STATIC, 0);
    texturecompositesize = Z_Malloc(numtextures * sizeof(*texturecompositesize), PU_STATIC, 0);
    texturewidthmask     = Z_Malloc(numtextures * sizeof(*texturewidthmask),     PU_STATIC, 0);
    texturewidth         = Z_Malloc(numtextures * sizeof(*texturewidth),         PU_STATIC, 0);
    textureheight        = Z_Malloc(numtextures * sizeof(*textureheight),        PU_STATIC, 0);
    texturebrightmap     = Z_Malloc(numtextures * sizeof(*texturebrightmap),     PU_STATIC, 0);

    for (int i = 0; i < numtextures; ++i, ++directory)
    {
        if (!(i & 63))
        {
            printf(".");
        }

        // [crispy] initialize for the first texture file lump,
        // skip through empty texture file lumps which do not contain any texture
        while (texturelump == texturelumps - 1 || i == texturelump->sumtextures)
        {
            // [crispy] start looking in next texture file
            texturelump = texturelump + 1;
            maptex      = texturelump->maptex;
            maxoff      = texturelump->maxoff;
            directory   = maptex + 1;
        }

        offset = LONG(*directory);

        if (offset > maxoff)
        {
            I_Error("R_InitTextures: bad texture directory");
        }

        const maptexture_t *mtexture = (const maptexture_t *)((const byte *)maptex + offset);

        texture = textures[i] = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t) * (SHORT(mtexture->patchcount) - 1), PU_STATIC, 0);

        memcpy(texture->name, mtexture->name, sizeof(texture->name));

        texture->width      = SHORT(mtexture->width);
        texture->height     = SHORT(mtexture->height);
        texture->patchcount = SHORT(mtexture->patchcount);

        const mappatch_t *mpatch = &mtexture->patches[0];
        patch = &texture->patches[0];

        // [crispy] initialize brightmaps
        texturebrightmap[i] = R_BrightmapForTexName(texture->name);

        for (int j = 0; j < texture->patchcount; ++j, ++mpatch, ++patch)
        {
            short p;
            patch->originx = SHORT(mpatch->originx);
            patch->originy = SHORT(mpatch->originy);

            // [crispy] apply offset for patches not in the first available patch offset table
            p = SHORT(mpatch->patch) + texturelump->pnamesoffset;

            // [crispy] catch out-of-range patches
            if (p < nummappatches)
            {
                patch->patch = patchlookup[p];
            }

            if (patch->patch == -1 || p >= nummappatches)
            {
                char texturename[9];
                texturename[8] = '\0';
                memcpy(texturename, texture->name, 8);

                // [crispy] make non-fatal
                fprintf(stderr, "R_InitTextures: Missing patch in texture %s\n", texturename);
                patch->patch = W_CheckNumForName("FONTB05"); // [crispy] dummy patch
            }
        }

        // [PN/JN] Copy texture name from the WAD definition and check if it matches
        // any of the known sky texture names (SKY1–SKY6). If it is a sky,
        // set the texture height to match the first patch's height; otherwise
        // use the height value from the WAD definition.
        if (R_IsTextureName(texture->name, "SKY1")
        ||  R_IsTextureName(texture->name, "SKY2")
        ||  R_IsTextureName(texture->name, "SKY3")
        ||  R_IsTextureName(texture->name, "SKY4")
        ||  R_IsTextureName(texture->name, "SKY5")
        ||  R_IsTextureName(texture->name, "SKY6"))
        {
            texture->height = R_GetPatchHeight(i, 0);
        }

        texturecolumnlump[i] = Z_Malloc(texture->width * sizeof(**texturecolumnlump), PU_STATIC, 0);
        texturecolumnofs[i]  = Z_Malloc(texture->width * sizeof(**texturecolumnofs),  PU_STATIC, 0);
        texturecolumnofs2[i] = Z_Malloc(texture->width * sizeof(**texturecolumnofs2), PU_STATIC, 0);

        int j = 1;
        while (j * 2 <= texture->width)
        {
            j <<= 1;
        }

        texturewidthmask[i] = j - 1;
        textureheight[i]    = texture->height << FRACBITS;

        // [crispy] texture width for wrapping column getter function
        texturewidth[i] = texture->width;
    }

    Z_Free(patchlookup);

    // [crispy] release memory allocated for texture files
    for (int i = 0; i < numtexturelumps; ++i)
    {
        W_ReleaseLumpNum(texturelumps[i].lumpnum);
    }
    free(texturelumps);

    // Precalculate whatever possible.
    for (int i = 0; i < numtextures; ++i)
    {
        R_GenerateLookup(i);
    }

    // Create translation table for global animation.
    texturetranslation = Z_Malloc((numtextures + 1) * sizeof(*texturetranslation), PU_STATIC, 0);

    for (int i = 0; i < numtextures; ++i)
    {
        texturetranslation[i] = i;
    }

    GenerateTextureHashTable();
}

// -----------------------------------------------------------------------------
// R_InitFlats
// -----------------------------------------------------------------------------

static void R_InitFlats (void)
{
    firstflat = W_GetNumForName(DEH_String("F_START")) + 1;
    lastflat  = W_GetNumForName(DEH_String("F_END")) - 1;
    numflats  = lastflat - firstflat + 1;

    // Create translation table for global animation.
    flattranslation = Z_Malloc((numflats+1) * sizeof(*flattranslation), PU_STATIC, 0);

    for (int i = 0; i < numflats; i++)
    {
        flattranslation[i] = i;
    }

    // [PN] Generate hash table for flats.
    W_HashNumForNameFromTo(firstflat, lastflat, numflats);
}

// -----------------------------------------------------------------------------
// R_InitSpriteLumps
//  Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
// -----------------------------------------------------------------------------

static void R_InitSpriteLumps (void)
{
    firstspritelump = W_GetNumForName(DEH_String("S_START")) + 1;
    lastspritelump = W_GetNumForName(DEH_String("S_END")) - 1;

    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = Z_Malloc(numspritelumps * sizeof(*spritewidth), PU_STATIC, 0);
    spriteoffset = Z_Malloc(numspritelumps * sizeof(*spriteoffset), PU_STATIC, 0);
    spritetopoffset = Z_Malloc(numspritelumps * sizeof(*spritetopoffset), PU_STATIC, 0);

    for (int i = 0; i < numspritelumps; i++)
    {
        if (!(i & 63))
            printf(".");

        patch_t *patch = W_CacheLumpNum(firstspritelump + i, PU_CACHE);
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


void R_InitColormaps (void)
{
	int c, i, j = 0;
	byte r, g, b;

	const byte *const playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
	const byte *const colormap = W_CacheLumpName("COLORMAP", PU_STATIC);

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
        // [PN] Precompute gamma-corrected base RGB for all 256 palette entries.
        // Effectively: PLAYPAL -> intensity/saturation -> gamma, once per index.
        const byte *const restrict gtab = gammatable[vid_gamma];
        const int   black_gamma = gtab[0];
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
    
            // Saturation
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
            const double kB    = (double)black_gamma * scale;
    
            lighttable_t *const restrict row = &colormaps[c * 256];
    
            for (int i = 0; i < 256; ++i)
            {
                // 1) Light fade in gamma space
                double R = (double)base_gamma[i][0] * k0 + kB;
                double G = (double)base_gamma[i][1] * k0 + kB;
                double B = (double)base_gamma[i][2] * k0 + kB;
    
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

	// [crispy] Invulnerability (c == COLORMAPS), generated from COLORMAP lump
    {
        lighttable_t *const restrict invrow = &colormaps[NUMCOLORMAPS * 256];
    
        for (int i = 0; i < 256; ++i)
        {
            // [PN] Choose source RGB (pre-gamma), then apply intensity/saturation/etc.
            byte src_r, src_g, src_b;
    
            if (a11y_invul)
            {
                // [PN] Equal-weight grayscale (independent from COLORMAP)
                const byte gray = (byte)((playpal[3 * i + 0]
                                        + playpal[3 * i + 1]
                                        + playpal[3 * i + 2]) / 3);
                src_r = src_g = src_b = gray;
            }
            else
            {
                // [PN] Always use the golden invulnerability effect
                // as defined by Heretic's own COLORMAP (row 32).
                const byte idx = colormap[32 * 256 + i];
                src_r = playpal[3 * idx + 0];
                src_g = playpal[3 * idx + 1];
                src_b = playpal[3 * idx + 2];
            }
    
            // [PN] Apply intensity and saturation via the same math as CALC_* macros
            // (manually feed "pal" with our src_* values)
            byte pal[3], channels[3];

            pal[0] = (byte)(src_r * vid_r_intensity);
            pal[1] = (byte)(src_g * vid_g_intensity);
            pal[2] = (byte)(src_b * vid_b_intensity);
    
            CALC_SATURATION(channels, pal, a_hi, a_lo);
            CALC_CONTRAST(channels, vid_contrast);
            CALC_COLORBLIND(channels, colorblind_matrix[a11y_colorblind]);
    
            // [PN] Gamma (match behavior: no 2-bit rounding in truecolor, keep it in 8-bit)
            byte r = gammatable[vid_gamma][channels[0]];
            byte g = gammatable[vid_gamma][channels[1]];
            byte b = gammatable[vid_gamma][channels[2]];
    
            if (!vid_truecolor)
            {
                r &= ~3; g &= ~3; b &= ~3;
            }
    
            invrow[i] = 0xff000000 | (r << 16) | (g << 8) | b;
        }
    }

	W_ReleaseLumpName("COLORMAP");

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
//  [crispy] initialize color translation and color strings tables
// -----------------------------------------------------------------------------

static void R_InitHSVColors (void)
{
    byte *restrict playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

    if (!crstr)
        crstr = I_Realloc(NULL, CRMAX * sizeof(*crstr));

    // [crispy] CRMAX - 2: don't override the original GREN and BLUE2 Boom tables
    for (int i = 0; i < CRMAX - 2; ++i)
    {
        for (int j = 0; j < 256; ++j)
        {
            cr[i][j] = V_Colorize(playpal, i, j, false);
        }

        char buf[3] = { cr_esc, (char)('0' + i), 0 };
        crstr[i] = M_StringDuplicate(buf);
    }

    W_ReleaseLumpName("PLAYPAL");
}

// -----------------------------------------------------------------------------
// [crispy] Changes palette to given one. Used exclusively in true color rendering
// for proper drawing of E2END pic in F_DrawUnderwater. Changing palette back to
// original PLAYPAL for restoring proper colors will be done in R_InitColormaps.
// -----------------------------------------------------------------------------

void R_SetUnderwaterPalette (const byte *const palette)
{
    int i, j = 0;
    byte r, g, b;

    // [JN] Saturation floats, high and low.
    // If saturation has been modified (< 100), set high and low
    // values according to saturation level. Sum of r,g,b channels
    // and floats must be 1.0 to get proper colors.
    const float a_hi = vid_saturation < 100 ? I_SaturationPercent[vid_saturation] : 0;
    const float a_lo = vid_saturation < 100 ? (a_hi / 2) : 0;

    for (i = 0; i < 256; i++)
    {
        // [PN] Apply intensity and saturation corrections
        byte pal[3];
        byte channels[3];

        CALC_INTENSITY(pal, palette, i);
        CALC_SATURATION(channels, pal, a_hi, a_lo);
        CALC_CONTRAST(channels, vid_contrast);

        r = gammatable[vid_gamma][channels[0]] + gammatable[vid_gamma][0];
        g = gammatable[vid_gamma][channels[1]] + gammatable[vid_gamma][0];
        b = gammatable[vid_gamma][channels[2]] + gammatable[vid_gamma][0];

        pal_color[j++] = 0xff000000 | (r << 16) | (g << 8) | b;
    }
}

// -----------------------------------------------------------------------------
// R_InitData
// Locates all the lumps that will be used by all views.
// -----------------------------------------------------------------------------

void R_InitData (void)
{
    // [crispy] Moved R_InitFlats() to the top, because it sets firstflat/lastflat
    // which are required by R_InitTextures() to prevent flat lumps from being
    // mistaken as patches and by R_InitBrightmaps() to set brightmaps for flats.
    // R_InitBrightmaps() comes next, because it sets R_BrightmapForTexName()
    // to initialize brightmaps depending on gameversion in R_InitTextures().
    R_InitFlats();
    printf(".");
    R_InitBrightmaps();
    printf(".");
    R_InitTextures();
    printf(".");
    R_InitSpriteLumps();
    printf(".");
    R_InitColormaps();
    printf(".");
    R_InitHSVColors();
    printf(".");
    // [JN] Initialize and compose translucency tables.
    I_InitTCTransMaps();
    I_InitPALTransMaps();
    printf(".");
}

// -----------------------------------------------------------------------------
// R_GetPatchHeight
// [crispy] Used to grab actual height of sky textures
// -----------------------------------------------------------------------------

int R_GetPatchHeight (int texture_num, int patch_index)
{
    texpatch_t *texpatch = &textures[texture_num]->patches[patch_index];
    patch_t *patch = W_CacheLumpNum(texpatch->patch, PU_CACHE);

    return SHORT(patch->height);
}

// -----------------------------------------------------------------------------
// R_FlatNumForName
//  Retrieval, get a flat number for a flat name.
// -----------------------------------------------------------------------------

int R_FlatNumForName (const char *name)
{
    const int i = W_CheckNumForNameFromTo(name, lastflat, firstflat);

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
//  Check whether texture is available. Filter out NoTexture indicator.
// -----------------------------------------------------------------------------

int R_CheckTextureNumForName (const char *name)
{
    // "NoTexture" marker.
    if (name[0] == '-')	
        return 0;

    const int key = W_LumpNameHash(name) % numtextures;
    const texture_t *texture = textures_hashtable[key]; 
    
    while (texture != NULL)
    {
        if (!strncasecmp(texture->name, name, 8))
            return texture->index;

        texture = texture->next;
    }

    return -1;
}

// -----------------------------------------------------------------------------
// R_TextureNumForName
//  Calls R_CheckTextureNumForName, aborts with error message.
// -----------------------------------------------------------------------------

int R_TextureNumForName (const char *name)
{
    const int i = R_CheckTextureNumForName(name);

    if (i == -1)
    {
        // [crispy] make missing texture non-fatal
        // and fix absurd texture name in error message
        fprintf(stderr, "R_TextureNumForName: %.8s not found\n", name);
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

void R_PrecacheLevel (void)
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

    hitlist[skytexture] = 1;

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
