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
// R_local.h

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "i_video.h"
#include "v_patch.h"

#define PL_SKYFLAT (0x80000000)
#define	ANGLETOSKYSHIFT		22      // sky map is 256*128*4 maps

#define	BASEYCENTER			100

#define	PI					3.141592657

#define	CENTERY				(SCREENHEIGHT/2)

#define	MINZ			(FRACUNIT*4)
#define	MAXZ			(FRACUNIT*8192)

//
// lighting constants
//
// [crispy] parameterized for smooth diminishing lighting
extern int LIGHTLEVELS;
extern int LIGHTSEGSHIFT;
extern int LIGHTBRIGHT;
extern int MAXLIGHTSCALE;
extern int LIGHTSCALESHIFT;
extern int MAXLIGHTZ;
extern int LIGHTZSHIFT;
extern int BMAPSHIFTINDEX;
// [crispy] parameterized for smooth diminishing lighting
extern int NUMCOLORMAPS;      // number of diminishing
#define	INVERSECOLORMAP		32

#define LOOKDIRMIN 110 // [crispy] -110, actually
#define LOOKDIRMAX 90
#define LOOKDIRS (LOOKDIRMIN + 1 + LOOKDIRMAX) // [crispy] lookdir range: -110..0..90
/*
==============================================================================

					INTERNAL MAP TYPES

==============================================================================
*/

//================ used by play and refresh

typedef struct
{
    fixed_t x, y;

// [crispy] remove slime trails
// vertex coordinates *only* used in rendering that have been
// moved towards the linedef associated with their seg by projecting them
// using the law of cosines in p_setup.c:P_RemoveSlimeTrails();
    fixed_t	r_x;
    fixed_t	r_y;
    boolean	moved;
} vertex_t;

struct line_s;

typedef struct
{
    fixed_t floorheight, ceilingheight;

    short floorpic, ceilingpic;
    short lightlevel;
    short special, tag;

    int soundtraversed;         // 0 = untraversed, 1,2 = sndlines -1
    mobj_t *soundtarget;        // thing that made a sound (or null)

    int blockbox[4];            // mapblock bounding box for height changes
    degenmobj_t soundorg;       // for any sounds played by the sector

    int validcount;             // if == validcount, already checked
    mobj_t *thinglist;          // list of mobjs in sector
    void *specialdata;          // thinker_t for reversable actions
    int linecount;
    struct line_s **lines;      // [linecount] size

    // [crispy] add support for MBF sky transfers
    int sky;

    // [crispy] WiggleFix: [kb] for R_FixWiggle()
    int cachedheight;
    int scaleindex;

    // [AM] Previous position of floor and ceiling before
    //      think.  Used to interpolate between positions.
    fixed_t	oldfloorheight;
    fixed_t	oldceilingheight;

    // [AM] Gametic when the old positions were recorded.
    //      Has a dual purpose; it prevents movement thinkers
    //      from storing old positions twice in a tic, and
    //      prevents the renderer from attempting to interpolate
    //      if old values were not updated recently.
    int         oldgametic;

    // [AM] Interpolated floor and ceiling height.
    //      Calculated once per tic and used inside
    //      the renderer.
    fixed_t	interpfloorheight;
    fixed_t	interpceilingheight;

    // [crispy] revealed secrets
    short	oldspecial;
} sector_t;

typedef struct
{
    fixed_t textureoffset;      // add this to the calculated texture col
    fixed_t rowoffset;          // add this to the calculated texture top
    short toptexture, bottomtexture, midtexture;
    sector_t *sector;

    // [crispy] smooth texture scrolling
    fixed_t	basetextureoffset;
} side_t;

typedef enum
{ ST_HORIZONTAL, ST_VERTICAL, ST_POSITIVE, ST_NEGATIVE } slopetype_t;

typedef struct line_s
{
    vertex_t *v1, *v2;
    fixed_t dx, dy;             // v2 - v1 for side checking
    unsigned short flags; // [crispy] extended nodes
    short special, tag;
    // sidenum[1] will be NO_INDEX if one sided
    unsigned short sidenum[2]; // [crispy] extended nodes
    fixed_t bbox[4];
    slopetype_t slopetype;      // to aid move clipping
    sector_t *frontsector, *backsector;
    int validcount;             // if == validcount, already checked
    void *specialdata;          // thinker_t for reversable actions

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


typedef struct subsector_s
{
    sector_t *sector;
    int numlines; // [crispy] extended nodes
    int firstline; // [crispy] extended nodes
} subsector_t;

typedef struct
{
    vertex_t *v1, *v2;
    fixed_t offset;
    angle_t angle;
    side_t *sidedef;
    line_t *linedef;
    sector_t *frontsector;
    sector_t *backsector;       // NULL for one sided lines

    uint32_t length; // [crispy] fix long wall wobble
    angle_t r_angle; // [crispy] recalculated angle used for rendering
    int fakecontrast;
} seg_t;

typedef struct
{
    fixed_t x, y, dx, dy;       // partition line
    fixed_t bbox[2][4];         // bounding box for each child
    // if NF_SUBSECTOR its a subsector
    int children[2]; // [crispy] extended nodes
} node_t;


/*
==============================================================================

						OTHER TYPES

==============================================================================
*/

typedef pixel_t lighttable_t;      // this could be wider for >8 bit display

extern size_t  maxopenings;         // [JN] 32-bit integer maths
extern int    *lastopening;
extern int    *openings;

typedef struct visplane_s
{
    struct visplane_s *next; // [JN] Next visplane in hash chain -- killough

    fixed_t height;
    int picnum;
    int lightlevel;
    int special;
    int minx, maxx;
    unsigned short pad1;
    unsigned short top[MAXWIDTH];
    unsigned short pad2;
    unsigned short pad3;
    unsigned short bottom[MAXWIDTH];
    unsigned short pad4;
} visplane_t;

typedef struct drawseg_s
{
    seg_t *curline;
    int x1, x2;
    fixed_t scale1, scale2, scalestep;
    int silhouette;             // 0=none, 1=bottom, 2=top, 3=both
    fixed_t bsilheight;         // don't clip sprites above this
    fixed_t tsilheight;         // don't clip sprites below this
// pointers to lists for sprite clipping
    int *sprtopclip;            // [crispy] 32-bit integer math
    int *sprbottomclip;         // [crispy] 32-bit integer math
    int *maskedtexturecol;      // [crispy] 32-bit integer math
} drawseg_t;

#define	SIL_NONE	0
#define	SIL_BOTTOM	1
#define SIL_TOP		2
#define	SIL_BOTH	3

#define	MAXDRAWSEGS		256

// A vissprite_t is a thing that will be drawn during a refresh
typedef struct vissprite_s
{
    struct vissprite_s *prev, *next;
    int x1, x2;
    fixed_t gx, gy;             // for line side calculation
    fixed_t gz, gzt;            // global bottom / top for silhouette clipping
    fixed_t startfrac;          // horizontal position of x1
    fixed_t scale;
    fixed_t xiscale;            // negative if flipped
    fixed_t texturemid;
    int patch;
    // for color translation and shadow draw, maxbright frames as well
    // [crispy] brightmaps for select sprites
    lighttable_t *colormap[2];
    const byte   *brightmap;
    int mobjflags;              // for color translation and shadow draw
    boolean psprite;            // true if psprite
    fixed_t footclip;           // foot clipping

    // [JN] Indicate if vissprite's frame is bright for choosing 
    // blending option of colfunc:
    // - tlcolfunc for overlay (unlit) blending.
    // - tladdcolfunc for additive (lit) blending.
    int brightframe;
} vissprite_t;


extern visplane_t *floorplane, *ceilingplane;

// Sprites are patches with a special naming convention so they can be 
// recognized by R_InitSprites.  The sprite and frame specified by a 
// thing_t is range checked at run time.
// a sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn.  Horizontal flipping 
// is used to save space. Some sprites will only have one picture used
// for all views.  

typedef struct
{
    boolean rotate;             // if false use 0 for any position
    short lump[8];              // lump to use for view angles 0-7
    byte flip[8];               // flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct
{
    int numframes;
    spriteframe_t *spriteframes;
} spritedef_t;

extern spritedef_t *sprites;
extern int numsprites;

//=============================================================================

extern int numvertexes;
extern vertex_t *vertexes;

extern int numsegs;
extern seg_t *segs;

extern int numsectors;
extern sector_t *sectors;

extern int numsubsectors;
extern subsector_t *subsectors;

extern int numnodes;
extern node_t *nodes;

extern int numlines;
extern line_t *lines;

extern int numsides;
extern side_t *sides;


// [crispy]
typedef struct localview_s
{
    angle_t oldticangle;
    angle_t ticangle;
    short ticangleturn;
    double rawangle;
    angle_t angle;
} localview_t;

extern fixed_t viewx, viewy, viewz;
extern angle_t viewangle;
extern player_t *viewplayer;
extern localview_t localview; // [crispy]


extern angle_t clipangle;

extern int viewangletox[FINEANGLES / 2];
extern angle_t xtoviewangle[MAXWIDTH + 1];
extern angle_t  linearskyangle[MAXWIDTH+1];

extern fixed_t rw_distance;
extern angle_t rw_normalangle;

//
// R_main.c
//
extern int viewwidth, viewheight, viewwindowx, viewwindowy;
extern int scaledviewwidth;
extern int centerx, centery;
extern int flyheight;
extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern fixed_t projection;

// [JN] FOV from DOOM Retro, Woof! and Nugget Doom
extern  float fovdiff;
extern  int   max_project_slope;

extern int validcount;

// [crispy] lookup table for horizontal screen coordinates
extern int  flipscreenwidth[MAXWIDTH];
extern int *flipviewwidth;

extern lighttable_t***	scalelight;
extern lighttable_t**	scalelightfixed;
extern lighttable_t***	zlight;

extern int extralight;
extern lighttable_t *fixedcolormap;

extern fixed_t viewcos, viewsin;

extern int detailshift;         // 0 = high, 1 = low

extern void (*colfunc) (void);
extern void (*basecolfunc) (void);
extern void (*tlcolfunc) (void);
extern void (*tladdcolfunc) (void);
extern void (*transtlcolfunc) (void);
extern void (*extratlcolfunc) (void);
extern void (*spanfunc) (void);

// [crispy] smooth texture scrolling
extern void R_InterpolateTextureOffsets (void);

int R_PointOnSide(fixed_t x, fixed_t y, const node_t *node);
int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line);
angle_t R_PointToAngle(fixed_t x, fixed_t y);
angle_t R_PointToAngleCrispy(fixed_t x, fixed_t y);
angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t R_PointToDist(fixed_t x, fixed_t y);
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y);
void R_AddPointToBox(int x, int y, fixed_t * box);
void R_ExecuteSetViewSize(void);
void R_InitLightTables(void);

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

// -----------------------------------------------------------------------------
// R_BMAPS
// -----------------------------------------------------------------------------

#define MAXDIMINDEX 47

extern void R_InitBrightmaps (void);

extern const byte  *R_BrightmapForTexName (const char *texname);
extern const byte  *R_BrightmapForSprite (const int state);
extern const byte  *R_BrightmapForFlatNum (const int num);
extern const byte  *R_BrightmapForState (const int state);
extern const byte **texturebrightmap;

//
// R_bsp.c
//
extern seg_t *curline;
extern side_t *sidedef;
extern line_t *linedef;
extern sector_t *frontsector, *backsector;

extern byte solidcol[MAXWIDTH];

extern drawseg_t *drawsegs;
extern drawseg_t *ds_p;
extern unsigned   maxdrawsegs;

extern lighttable_t **hscalelight, **vscalelight, **dscalelight;

typedef void (*drawfunc_t) (int start, int stop);
void R_ClearClipSegs(void);

void R_ClearDrawSegs(void);
void R_InitSkyMap(void);
void R_RenderBSPNode(int bspnum);

//
// R_segs.c
//
extern angle_t rw_angle1;           // angle to line origin
extern lighttable_t **walllights;


void R_RenderMaskedSegRange(drawseg_t * ds, int x1, int x2);
extern void R_StoreWallRange (int start, int stop);


//
// R_plane.c
//
typedef void (*planefunction_t) (int top, int bottom);
extern planefunction_t floorfunc, ceilingfunc;

extern int skyflatnum;

extern int floorclip[MAXWIDTH];   // [crispy] 32-bit integer math
extern int ceilingclip[MAXWIDTH]; // [crispy] 32-bit integer math

extern fixed_t *yslope;
extern fixed_t yslopes[LOOKDIRS][MAXHEIGHT]; // [crispy]
extern fixed_t distscale[MAXWIDTH];

extern fixed_t swirlCoord_x;
extern fixed_t swirlCoord_y;

void R_ClearPlanes(void);
void R_DrawPlanes(void);

visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                        int special);
visplane_t *R_CheckPlane(visplane_t * pl, int start, int stop);
extern visplane_t *R_DupPlane (const visplane_t *pl, int start, int stop);


//
// R_debug.m
//
extern int drawbsp;

//
// R_data.c
//
extern fixed_t *textureheight;  // needed for texture pegging
extern fixed_t *spritewidth;    // needed for pre rendering (fracs)
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;
extern lighttable_t *colormaps;
extern int firstflat;
extern int numflats;

extern int *flattranslation;    // for global animation
extern int *texturetranslation; // for global animation

extern int firstspritelump, lastspritelump, numspritelumps;

extern int   *texturecompositesize;
extern byte **texturecomposite;

byte *R_GetColumn(int tex, int col);
byte *R_GetColumnMod (int tex, int col);
void R_InitData(void);
void R_PrecacheLevel(void);
extern void R_InitColormaps (void);
extern void R_SetUnderwaterPalette (const byte *const palette);
extern int R_GetPatchHeight(int texture_num, int patch_index);


//
// R_things.c
//

// constant arrays used for psprite clipping and initializing clipping
extern int negonearray[MAXWIDTH];       // [crispy] 32-bit integer math
extern int screenheightarray[MAXWIDTH]; // [crispy] 32-bit integer math

// vars for R_DrawMaskedColumn
extern int *mfloorclip;   // [crispy] 32-bit integer math
extern int *mceilingclip; // [crispy] 32-bit integer math
extern fixed_t spryscale;
extern int64_t sprtopscreen; // [crispy] WiggleFix
extern fixed_t sprbotscreen;

extern fixed_t pspritescale, pspriteiscale;

extern void R_DrawMaskedColumn (const column_t *column, signed int baseclip);


extern void R_AddSprites(const sector_t *const sec);
void R_AddPSprites(void);
void R_DrawSprites(void);
void R_InitSprites(const char **namelist);
void R_ClearSprites(void);
void R_DrawMasked(void);

//=============================================================================
//
// R_draw.c
//
//=============================================================================

extern lighttable_t *dc_colormap[2];
extern int dc_x;
extern int dc_yl;
extern int dc_yh;
extern fixed_t dc_iscale;
extern fixed_t dc_texturemid;
extern int dc_texheight;
extern byte *dc_source;         // first pixel in a column
extern pixel_t *ylookup[MAXHEIGHT];
extern int columnofs[MAXWIDTH];

void R_DrawColumn(void);
void R_DrawColumnLow(void);
void R_DrawTLColumn(void);
void R_DrawTLColumnLow(void);
void R_DrawTLAddColumn (void);
void R_DrawTLAddColumnLow (void);
void R_DrawTranslatedColumn(void);
void R_DrawTranslatedColumnLow(void);
void R_DrawTranslatedTLColumn(void);
void R_DrawTranslatedTLColumnLow(void);
void R_DrawExtraTLColumn(void);
void R_DrawExtraTLColumnLow(void);
extern void R_DrawTLColumn_8 (void);
extern void R_DrawTLColumnLow_8 (void);
extern void R_DrawTLAddColumn_8 (void);
extern void R_DrawTLAddColumnLow_8 (void);
extern void R_DrawTranslatedTLColumn_8 (void);
extern void R_DrawTranslatedTLColumnLow_8 (void);
extern void R_DrawExtraTLColumn_8 (void);
extern void R_DrawExtraTLColumnLow_8 (void);

extern int ds_y;
extern int ds_x1;
extern int ds_x2;
extern lighttable_t *ds_colormap[2];
extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;
extern byte *ds_source;         // start of a 64*64 tile image

extern byte *translationtables;
extern byte *dc_translation;

extern const byte *dc_brightmap;
extern const byte *ds_brightmap;

void R_DrawSpan(void);
void R_DrawSpanLow(void);

void R_InitBuffer(int width, int height);
void R_InitTranslationTables(void);

// -----------------------------------------------------------------------------
// R_SWIRL
// -----------------------------------------------------------------------------

extern void  R_InitDistortedFlats (void);
extern byte *R_SwirlingFlat (int flatnum);
extern byte *R_WarpingFlat1 (int flatnum);
extern byte *R_WarpingFlat2 (int flatnum);
extern byte *R_WarpingFlat3 (int flatnum);

#endif // __R_LOCAL__
