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

// P_spec.h

/*
===============================================================================

							P_SPEC

===============================================================================
*/

//
//      Animating textures and planes
//
typedef struct
{
    boolean istexture;
    int picnum;
    int basepic;
    int numpics;
    int speed;
} anim_t;

//
//      source animation definition
//
typedef PACKED_STRUCT (
{
    signed char istexture;      // if false, it's a flat
    char endname[9];
    char startname[9];
    int speed;
}) animdef_t;

#define	MAXANIMS		32

extern int *TerrainTypes;

//
//      Animating line specials
//
#define	MAXLINEANIMS		64*256
extern short numlinespecials;
extern line_t *linespeciallist[MAXLINEANIMS];

//      Define values for map objects
#define	MO_TELEPORTMAN		14

// at game start
void P_InitPicAnims(void);
void P_InitTerrainTypes(void);
void P_InitLava(void);

// at map load
void P_SpawnSpecials(void);
void P_InitAmbientSound(void);
void P_AddAmbientSfx(int sequence);

// every tic
void P_UpdateSpecials(void);
void P_AmbientSound(void);

// when needed
boolean P_UseSpecialLine(mobj_t * thing, line_t * line);
void P_ShootSpecialLine(const mobj_t * thing, line_t * line);
void P_CrossSpecialLine(int linenum, int side, mobj_t * thing);

void P_PlayerInSpecialSector(player_t * player);

int twoSided(int sector, int line);
sector_t *getSector(int currentSector, int line, int side);
side_t *getSide(int currentSector, int line, int side);
fixed_t P_FindLowestFloorSurrounding(sector_t * sec);
fixed_t P_FindHighestFloorSurrounding(sector_t * sec);
fixed_t P_FindNextHighestFloor(sector_t * sec, int currentheight);
fixed_t P_FindLowestCeilingSurrounding(sector_t * sec);
fixed_t P_FindHighestCeilingSurrounding(sector_t * sec);
int P_FindSectorFromLineTag(const line_t * line, int start);
int P_FindMinSurroundingLight(sector_t * sector, int max);
sector_t *getNextSector(line_t * line, const sector_t * sec);

//
//      SPECIAL
//
int EV_DoDonut(const line_t * line);

/*
===============================================================================

							P_LIGHTS

===============================================================================
*/
typedef struct
{
    thinker_t thinker;
    sector_t *sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
} lightflash_t;

typedef struct
{
    thinker_t thinker;
    sector_t *sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
} strobe_t;

typedef struct
{
    thinker_t thinker;
    sector_t *sector;
    int minlight;
    int maxlight;
    int direction;
} glow_t;

#define GLOWSPEED		8
#define	STROBEBRIGHT	5
#define	FASTDARK		15
#define	SLOWDARK		35

void T_LightFlash(thinker_t *thinker);
void P_SpawnLightFlash(sector_t * sector);
void T_StrobeFlash(thinker_t *thinker);
void P_SpawnStrobeFlash(sector_t * sector, int fastOrSlow, int inSync);
void EV_StartLightStrobing(const line_t * line);
void EV_TurnTagLightsOff(const line_t * line);
void EV_LightTurnOn(const line_t * line, int bright);
void T_Glow(thinker_t *thinker);
void P_SpawnGlowingLight(sector_t * sector);

/*
===============================================================================

							P_SWITCH

===============================================================================
*/
typedef PACKED_STRUCT (
{
    char name1[9];
    char name2[9];
    short episode;
}) switchlist_t;

typedef enum
{
    top,
    middle,
    bottom
} bwhere_e;

typedef struct
{
    line_t *line;
    bwhere_e where;
    int btexture;
    int btimer;
    void *soundorg;
} button_t;

#define	MAXSWITCHES	50      // max # of wall switches in a level
#define	MAXBUTTONS	16      // 4 players, 4 buttons each at once, max.
#define BUTTONTIME	35      // 1 second

extern button_t *buttonlist; // [crispy] remove MAXBUTTONS limit
extern int       maxbuttons; // [crispy] remove MAXBUTTONS limit

void P_ChangeSwitchTexture(line_t * line, int useAgain);
void P_InitSwitchList(void);

/*
===============================================================================

							P_PLATS

===============================================================================
*/
typedef enum
{
    up,
    down,
    waiting,
    in_stasis
} plat_e;

typedef enum
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange
   ,// [JN] H+H Specials:
    hh_24640,
} plattype_e;

typedef struct
{
    thinker_t thinker;
    sector_t *sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int wait;
    int count;
    plat_e status;
    plat_e oldstatus;
    boolean crush;
    int tag;
    plattype_e type;
} plat_t;

#define	PLATWAIT	3
#define	PLATSPEED	FRACUNIT
#define	MAXPLATS	30*256

extern plat_t *activeplats[MAXPLATS];

void T_PlatRaise(thinker_t *thinker);
int EV_DoPlat(line_t * line, plattype_e type, int amount);
void P_AddActivePlat(plat_t * plat);
void EV_StopPlat(const line_t *line);

/*
===============================================================================

							P_DOORS

===============================================================================
*/
typedef enum
{
    vld_normal,
    vld_close30ThenOpen,
    vld_close,
    vld_open,
    vld_raiseIn5Mins
} vldoor_e;

typedef struct
{
    thinker_t thinker;
    vldoor_e type;
    sector_t *sector;
    fixed_t topheight;
    fixed_t speed;
    int direction;              // 1 = up, 0 = waiting at top, -1 = down
    int topwait;                // tics to wait at the top
    // (keep in case a door going down is reset)
    int topcountdown;           // when it reaches 0, start going down
} vldoor_t;

#define	VDOORSPEED	FRACUNIT*2
#define	VDOORWAIT		150

void EV_VerticalDoor(line_t * line, mobj_t * thing);
int EV_DoDoor(const line_t * line, vldoor_e type, fixed_t speed);
void T_VerticalDoor(thinker_t *thinker);
void P_SpawnDoorCloseIn30(sector_t * sec);
void P_SpawnDoorRaiseIn5Mins(sector_t * sec, int secnum);

/*
===============================================================================

							P_CEILNG

===============================================================================
*/
typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise
} ceiling_e;

typedef struct
{
    thinker_t thinker;
    ceiling_e type;
    sector_t *sector;
    fixed_t bottomheight, topheight;
    fixed_t speed;
    boolean crush;
    int direction;              // 1 = up, 0 = waiting, -1 = down
    int tag;                    // ID
    int olddirection;
} ceiling_t;

#define	CEILSPEED		FRACUNIT
#define	CEILWAIT		150
#define MAXCEILINGS		30

extern ceiling_t *activeceilings[MAXCEILINGS];

int EV_DoCeiling(const line_t * line, ceiling_e type);
void T_MoveCeiling(thinker_t *thinker);
void P_AddActiveCeiling(ceiling_t * c);
extern void P_RemoveActiveCeiling (const ceiling_t *const c);
int EV_CeilingCrushStop(const line_t *const line);
extern void P_ActivateInStasisCeiling (const line_t *const line);

/*
===============================================================================

							P_FLOOR

===============================================================================
*/
typedef enum
{
    lowerFloor,                 // lower floor to highest surrounding floor
    lowerFloorToLowest,         // lower floor to lowest surrounding floor
    turboLower,                 // lower floor to highest surrounding floor VERY FAST
    raiseFloor,                 // raise floor to lowest surrounding CEILING
    raiseFloorToNearest,        // raise floor to next highest surrounding floor
    raiseToTexture,             // raise floor to shortest height texture around it
    lowerAndChange,             // lower floor to lowest surrounding floor and change
    // floorpic
    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,
    donutRaise,
    raiseBuildStep              // One step of a staircase
   ,// [JN] H+H Specials:
    hh_159,
    hh_161,
    hh_24720,
    hh_24722,
    hh_24832,
    hh_28064,
} floor_e;

typedef struct
{
    thinker_t thinker;
    floor_e type;
    boolean crush;
    sector_t *sector;
    int direction;
    int newspecial;
    short texture;
    fixed_t floordestheight;
    fixed_t speed;
} floormove_t;

#define	FLOORSPEED	FRACUNIT

typedef enum
{
    ok,
    crushed,
    pastdest
} result_e;

result_e T_MovePlane(sector_t * sector, fixed_t speed,
                     fixed_t dest, boolean crush, int floorOrCeiling,
                     int direction);

int EV_BuildStairs(const line_t * line, fixed_t stepDelta);
int EV_DoFloor(line_t * line, floor_e floortype);
void T_MoveFloor(thinker_t *thinker);

/*
===============================================================================

							P_TELEPT

===============================================================================
*/

boolean P_Teleport(mobj_t * thing, fixed_t x, fixed_t y, angle_t angle);
boolean EV_Teleport(const line_t *line, int side, mobj_t * thing);
