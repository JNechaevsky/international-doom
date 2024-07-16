//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	Refresh (R_*) module, global header.
//	All the rendering/drawing stuff is here.
//


#pragma once

#include "tables.h"
#include "d_player.h"
#include "doomdef.h"
#include "i_video.h"
#include "v_patch.h"


// Silhouette, needed for clipping Segs (mainly) and sprites representing things.
#define SIL_NONE    0
#define SIL_BOTTOM  1
#define SIL_TOP     2
#define SIL_BOTH    3


// =============================================================================
// INTERNAL MAP TYPES
//  used by play and refresh
// =============================================================================

// Your plain vanilla vertex.
// Note: transformed values not buffered locally, 
//  like some DOOM-alikes ("wt", "WebView") did.

typedef struct
{
    fixed_t	x;
    fixed_t	y;
    // [crispy] remove slime trails
    // vertex coordinates *only* used in rendering that have been
    // moved towards the linedef associated with their seg by projecting them
    // using the law of cosines in p_setup.c:P_RemoveSlimeTrails();
    fixed_t	r_x;
    fixed_t	r_y;
    boolean	moved;
} vertex_t;

// Forward of LineDefs, for Sectors.
struct line_s;

// Each sector has a degenmobj_t in its center for sound origin purposes.
// I suppose this does not handle sound from moving objects (doppler),
// because position is prolly just buffered, not updated.

typedef struct
{
    thinker_t   thinker;  // not used for anything
    fixed_t     x;
    fixed_t     y;
    fixed_t     z;
} degenmobj_t;

// The SECTORS record, at runtime.
// Stores things/mobjs.

typedef	struct
{
    fixed_t floorheight;
    fixed_t ceilingheight;

    short   floorpic;
    short   ceilingpic;
    short   lightlevel;
    short   special;
    short   tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int     soundtraversed;

    // thing that made a sound (or null)
    mobj_t *soundtarget;

    // mapblock bounding box for height changes
    int     blockbox[4];

    // origin for any sounds played by the sector
    degenmobj_t soundorg;

    // if == validcount, already checked
    int     validcount;

    // list of mobjs in sector
    mobj_t *thinglist;

    // thinker_t for reversable actions
    void   *specialdata;

    int     linecount;
    struct line_s **lines;  // linecount size

    // [crispy] WiggleFix: [kb] for R_FixWiggle()
    int		cachedheight;
    int		scaleindex;

    // [crispy] add support for MBF sky tranfers
    int		sky;

    // [AM] Previous position of floor and ceiling before
    //      think.  Used to interpolate between positions.
    fixed_t	oldfloorheight;
    fixed_t	oldceilingheight;

    // [AM] Gametic when the old positions were recorded.
    //      Has a dual purpose; it prevents movement thinkers
    //      from storing old positions twice in a tic, and
    //      prevents the renderer from attempting to interpolate
    //      if old values were not updated recently.
    int     oldgametic;

    // [AM] Interpolated floor and ceiling height.
    //      Calculated once per tic and used inside
    //      the renderer.
    fixed_t	interpfloorheight;
    fixed_t	interpceilingheight;

    // [crispy] revealed secrets
    short	oldspecial;
} sector_t;

//
// The SideDef.
//

typedef struct
{
    // add this to the calculated texture column
    fixed_t	textureoffset;

    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture indices.
    // We do not maintain names here. 
    short toptexture;
    short bottomtexture;
    short midtexture;

    // Sector the SideDef is facing.
    sector_t *sector;

    // [crispy] smooth texture scrolling
    fixed_t	basetextureoffset;
} side_t;

//
// Move clipping aid for LineDefs.
//

typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t *v1;
    vertex_t *v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t dx;
    fixed_t dy;

    // Animation related.
    unsigned short flags;
    short special;
    short tag;

    // Visual appearance: SideDefs.
    // sidenum[1] will be -1 (NO_INDEX) if one sided
    unsigned short sidenum[2];		

    // Neat. Another bounding box, for the extent of the LineDef.
    fixed_t bbox[4];

    // To aid move clipping.
    slopetype_t slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t *frontsector;
    sector_t *backsector;

    // if == validcount, already checked
    int validcount;

    // thinker_t for reversable actions
    void *specialdata;		

    // [JN] Improved column clipping.
    int r_validcount;   // cph: if == gametic, r_flags already done
    enum {              // cph:
    RF_TOP_TILE  = 1,   // Upper texture needs tiling
    RF_MID_TILE  = 2,   // Mid texture needs tiling
    RF_BOT_TILE  = 4,   // Lower texture needs tiling
    RF_IGNORE    = 8,   // Renderer can skip this line
    RF_CLOSED    = 16,  // Line blocks view
    } r_flags;

    // [crispy] calculate sound origin of line to be its midpoint
    degenmobj_t	soundorg;
} line_t;

//
// A SubSector.
// References a Sector. Basically, this is a list of LineSegs, indicating 
// the visible walls that define (all or some) sides of a convex BSP leaf.
//

typedef struct subsector_s
{
    sector_t *sector;
    int       numlines;
    int       firstline;
} subsector_t;

//
// The LineSeg.
//

typedef struct
{
    vertex_t *v1;
    vertex_t *v2;
    fixed_t	  offset;
    angle_t	  angle;
    side_t   *sidedef;
    line_t   *linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one sided lines
    sector_t *frontsector;
    sector_t *backsector;

    uint32_t  length;   // [crispy] fix long wall wobble
    angle_t   r_angle;  // [crispy] re-calculated angle used for rendering
    int       fakecontrast;
} seg_t;

//
// BSP node.
//

typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;

    // Bounding box for each child.
    fixed_t	bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    int children[2];
} node_t;

//
// OTHER TYPES
//

// This could be wider for >8 bit display. Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT. From darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef pixel_t lighttable_t;

//
// ?
//

typedef struct drawseg_s
{
    seg_t *curline;
    int    x1;
    int    x2;

    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int silhouette;

    // do not clip sprites above this
    fixed_t bsilheight;

    // do not clip sprites below this
    fixed_t tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    int *sprtopclip;        // [JN] 32-bit integer math
    int *sprbottomclip;     // [JN] 32-bit integer math
    int *maskedtexturecol;  // [JN] 32-bit integer math
} drawseg_t;

// A vissprite_t is a thing that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.

typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s *prev;
    struct vissprite_s *next;

    int         x1;
    int         x2;

    // for line side calculation
    fixed_t     gx;
    fixed_t     gy;		

    // global bottom / top for silhouette clipping
    fixed_t     gz;
    fixed_t     gzt;

    // horizontal position of x1
    fixed_t     startfrac;
    
    fixed_t     scale;
    
    // negative if flipped
    fixed_t     xiscale;	

    fixed_t     texturemid;
    int         patch;

    // for color translation and shadow draw, maxbright frames as well
    // [crispy] brightmaps for select sprites
    lighttable_t *colormap[2];
    const byte   *brightmap;
   
    int         mobjflags;
    // [crispy] color translation table for blood colored by monster class
    byte         *translation;
#ifndef CRISPY_TRUECOLOR
    byte         *blendfunc;
#else
    const pixel_t	(*blendfunc)(const pixel_t fg, const pixel_t bg);
#endif
} vissprite_t;

//	
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//

typedef struct
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    int	rotate; // [crispy] we use a value of 2 for 16 sprite rotations

    // Lump to use for view angles 0-7.
    short	lump[16]; // [crispy] support 16 sprite rotations

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte	flip[16]; // [crispy] support 16 sprite rotations

} spriteframe_t;

//
// A sprite definition: a number of animation frames.
//

typedef struct
{
    int            numframes;
    spriteframe_t *spriteframes;
} spritedef_t;

//
// Now what is a visplane, anyway?
// 

typedef struct visplane_s
{
    struct visplane_s *next; // [JN] Next visplane in hash chain -- killough
    fixed_t height;
    int     picnum;
    int     lightlevel;
    int     minx;
    int     maxx;

    // leave pads for [minx-1]/[maxx+1]
    unsigned short pad1;
    unsigned short top[MAXWIDTH];
    unsigned short pad2;
    unsigned short pad3;
    unsigned short bottom[MAXWIDTH];
    unsigned short pad4;
} visplane_t;

//
// Refresh internal data structures, for rendering.
//

// needed for texture pegging
extern fixed_t *textureheight;

// needed for pre rendering (fracs)
extern fixed_t *spritewidth;
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;

extern lighttable_t *colormaps;
extern lighttable_t *pal_color; // [crispy] array holding palette colors for true color mode

extern int viewwidth;
extern int scaledviewwidth;
extern int viewheight;

extern int firstflat;

// for global animation
extern int *flattranslation;	
extern int *texturetranslation;	

// Sprite....
extern int  firstspritelump;
extern int  lastspritelump;
extern int  numspritelumps;

//
// Lookup tables for map data.
//

extern int numsprites;
extern int numvertexes;
extern int numsegs;
extern int numsectors;
extern int numsubsectors;
extern int numnodes;
extern int numlines;
extern int numsides;

extern spritedef_t *sprites;
extern vertex_t    *vertexes;
extern seg_t       *segs;
extern sector_t    *sectors;
extern subsector_t *subsectors;
extern node_t      *nodes;
extern line_t      *lines;
extern side_t      *sides;

// [crispy]
typedef struct localview_s
{
    angle_t oldticangle;
    angle_t ticangle;
    short ticangleturn;
    double rawangle;
    angle_t angle;
} localview_t;

//
// POV data.
//

extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;

extern angle_t   viewangle;
extern player_t *viewplayer;
extern localview_t localview; // [crispy]

extern int      viewangletox[FINEANGLES/2];
extern angle_t  xtoviewangle[MAXWIDTH+1];
extern angle_t  linearskyangle[MAXWIDTH+1];
extern fixed_t  rw_distance;
extern angle_t  rw_normalangle;
extern angle_t  rw_angle1;
extern angle_t  clipangle;

extern visplane_t *floorplane;
extern visplane_t *ceilingplane;

// -----------------------------------------------------------------------------
// R_BMAPS
// -----------------------------------------------------------------------------

#define MAXDIMINDEX 47

extern void R_InitBrightmaps (void);

extern const byte  *R_BrightmapForTexName (const char *texname);
extern const byte  *R_BrightmapForSprite (const int type);
extern const byte  *R_BrightmapForFlatNum (const int num);
extern const byte  *R_BrightmapForState (const int state);
extern const byte **texturebrightmap;


// -----------------------------------------------------------------------------
// R_BSP
// -----------------------------------------------------------------------------

typedef void (*drawfunc_t) (int start, int stop);

extern void R_ClearClipSegs (void);
extern void R_ClearDrawSegs (void);
extern void R_RenderBSPNode (int bspnum);

extern seg_t    *curline;
extern side_t   *sidedef;
extern line_t   *linedef;
extern sector_t *frontsector;
extern sector_t *backsector;

extern byte solidcol[MAXWIDTH];

extern drawseg_t *drawsegs;
extern drawseg_t *ds_p;
extern unsigned   maxdrawsegs;

// -----------------------------------------------------------------------------
// R_DATA
// -----------------------------------------------------------------------------

#define LOOKDIRMIN	110 // [crispy] -110, actually
#define LOOKDIRMAX	90
#define LOOKDIRS	(LOOKDIRMIN+1+LOOKDIRMAX) // [crispy] lookdir range: -110..0..90

extern byte *R_GetColumn (int tex, int col);
extern byte *R_GetColumnMod (int tex, int col);
extern byte *R_GetColumnMod2 (int tex, int col);
extern int   R_CheckTextureNumForName (const char *name);
extern int   R_FlatNumForName (const char *name);
extern int   R_TextureNumForName (const char *name);
extern void  R_InitColormaps (void);
extern void  R_InitData (void);
extern void  R_PrecacheLevel (void);

extern int   *texturecompositesize;
extern byte **texturecomposite;

extern int    numflats;

// -----------------------------------------------------------------------------
// R_DRAW
// -----------------------------------------------------------------------------

extern void R_DrawColumn (void);
extern void R_DrawColumnLow (void);
extern void R_DrawFuzzColumn (void);
extern void R_DrawFuzzColumnLow (void);
extern void R_DrawSpan (void);
extern void R_DrawSpanLow (void);
extern void R_DrawTLColumn (void);
extern void R_DrawTLColumnLow (void);
extern void R_DrawTLFuzzColumn (void);
extern void R_DrawTLFuzzColumnLow (void);
extern void R_DrawTranslatedColumn (void);
extern void R_DrawTranslatedColumnLow (void);
extern void R_DrawTransTLFuzzColumn (void);
extern void R_DrawTransTLFuzzColumnLow (void);

extern void R_DrawViewBorder (void);
extern void R_FillBackScreen (void);
extern void R_InitBuffer (int width, int height);
extern void R_InitTranslationTables (void);
extern void R_SetFuzzPosDraw (void);
extern void R_SetFuzzPosTic (void);
extern void R_VideoErase (unsigned ofs, int count);

extern byte *dc_source;
extern byte *ds_source;		
extern byte *translationtables;
extern byte *dc_translation;

extern int dc_x;
extern int dc_yl;
extern int dc_yh;
extern int ds_y;
extern int ds_x1;
extern int ds_x2;

extern fixed_t dc_iscale;
extern fixed_t dc_texturemid;
extern int     dc_texheight;
extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;

extern lighttable_t *dc_colormap[2];
extern lighttable_t *ds_colormap[2];

extern const byte *dc_brightmap;
extern const byte *ds_brightmap;

// -----------------------------------------------------------------------------
// R_MAIN
// -----------------------------------------------------------------------------

extern void    R_ExecuteSetViewSize (void);
extern void    R_Init (void);
extern void    R_InitLightTables (void);
extern void    R_InitSkyMap (void);
extern void    R_RenderPlayerView (player_t *player);
extern void    R_SetViewSize (int blocks, int detail);

// Utility functions.
extern angle_t R_PointToAngle (fixed_t x, fixed_t y);
extern angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
extern angle_t R_PointToAngleCrispy (fixed_t x, fixed_t y);
extern fixed_t R_PointToDist (fixed_t x, fixed_t y);
extern fixed_t R_ScaleFromGlobalAngle (angle_t visangle);
extern int     R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line);
extern int     R_PointOnSide (fixed_t x, fixed_t y, const node_t *node);
extern subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
extern void    R_AddPointToBox (int x, int y, fixed_t *box);

inline static fixed_t LerpFixed(fixed_t oldvalue, fixed_t newvalue)
{
    return (oldvalue + FixedMul(newvalue - oldvalue, fractionaltic));
}

inline static int LerpInt(int oldvalue, int newvalue)
{
    return (oldvalue + (int)((newvalue - oldvalue) * FIXED2DOUBLE(fractionaltic)));
}

// [AM] Interpolate between two angles.
inline static angle_t LerpAngle(angle_t oangle, angle_t nangle)
{
    if (nangle == oangle)
        return nangle;
    else if (nangle > oangle)
    {
        if (nangle - oangle < ANG270)
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
    }
    else // nangle < oangle
    {
        if (oangle - nangle < ANG270)
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
    }
}

// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
extern void (*colfunc) (void);
extern void (*basecolfunc) (void);
extern void (*fuzzcolfunc) (void);
extern void (*transcolfunc) (void);
extern void (*tlcolfunc) (void);
extern void (*tlfuzzcolfunc) (void);
extern void (*transtlfuzzcolfunc) (void);
extern void (*spanfunc) (void);

// POV related.
extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern fixed_t projection;
extern fixed_t viewcos;
extern fixed_t viewsin;
extern int     centerx;
extern int     centery;
extern int     validcount;
extern int     viewwindowx;
extern int     viewwindowy;

// [JN] FOV from DOOM Retro, Woof! and Nugget Doom
extern  float fovdiff;
extern  int   max_project_slope;

// [crispy] lookup table for horizontal screen coordinates
extern int     flipscreenwidth[MAXWIDTH];
extern int    *flipviewwidth;

extern boolean setsizeneeded;
extern boolean original_playpal;
extern boolean original_colormap;

// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).

// Lighting constants. Now why not 32 levels here?
// [crispy] parameterized for smooth diminishing lighting
extern int LIGHTLEVELS;
extern int LIGHTSEGSHIFT;
extern int LIGHTBRIGHT;
extern int MAXLIGHTSCALE;
extern int LIGHTSCALESHIFT;
extern int MAXLIGHTZ;
extern int LIGHTZSHIFT;

extern lighttable_t***	scalelight;
extern lighttable_t**	scalelightfixed;
extern lighttable_t***	zlight;

extern int           extralight;
extern lighttable_t *fixedcolormap;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS    32

// Blocky/low detail mode. 0 = high, 1 = low
extern int detailshift;	

// -----------------------------------------------------------------------------
// R_PLANE
// -----------------------------------------------------------------------------

#define PL_SKYFLAT (0x80000000)

extern void R_ClearPlanes (void);
extern void R_DrawPlanes (void);
extern void R_InitPlanes (void);

extern int  floorclip[MAXWIDTH];    // [JN] 32-bit integer math
extern int  ceilingclip[MAXWIDTH];  // [JN] 32-bit integer math

extern size_t  maxopenings;         // [JN] 32-bit integer maths
extern int    *lastopening;
extern int    *openings;

extern fixed_t *yslope;
extern fixed_t  yslopes[LOOKDIRS][MAXHEIGHT];
extern fixed_t distscale[MAXWIDTH];

extern fixed_t swirlCoord_x;
extern fixed_t swirlCoord_y;

extern visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel);
extern visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop);
extern visplane_t *R_DupPlane (const visplane_t *pl, int start, int stop);

// -----------------------------------------------------------------------------
// R_SEGS
// -----------------------------------------------------------------------------

extern void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);
extern void R_StoreWallRange (int start, int stop);

extern lighttable_t **walllights;

// -----------------------------------------------------------------------------
// R_SKY
// -----------------------------------------------------------------------------

// SKY, store the number for name.
#define SKYFLATNAME "F_SKY1"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT 22

// [crispy] stretch sky
#define SKYSTRETCH_HEIGHT 228

extern int skytexture;
extern int skytexturemid;

// -----------------------------------------------------------------------------
// R_SWIRL
// -----------------------------------------------------------------------------

extern void  R_InitDistortedFlats (void);
extern char *R_DistortedFlat (int flatnum);

// -----------------------------------------------------------------------------
// R_THINGS
// -----------------------------------------------------------------------------

extern void R_AddPSprites (void);
extern void R_AddSprites (sector_t *sec);
extern void R_ClearSprites (void);
extern void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);
extern void R_DrawMasked (void);
extern void R_DrawMaskedColumn (column_t *column);
extern void R_DrawSprites (void);
extern void R_InitSprites (const char **namelist);

// Constant arrays used for psprite clipping and initializing clipping.
extern int negonearray[MAXWIDTH];        // [JN] 32-bit integer math
extern int screenheightarray[MAXWIDTH];  // [JN] 32-bit integer math

// vars for R_DrawMaskedColumn
extern int *mfloorclip;    // [JN] 32-bit integer math
extern int *mceilingclip;  // [JN] 32-bit integer math

extern fixed_t spryscale;
extern int64_t sprtopscreen;

extern fixed_t pspritescale;
extern fixed_t pspriteiscale;

// [crispy] interpolate weapon bobbing
extern boolean pspr_interp;
