//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2023 Julia Nechaevskaya
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
//	Play functions, animation, global header.
//


#pragma once


#include "r_local.h"


#define MAXHEALTH       (100)
#define VIEWHEIGHT      (FRACUNIT*41)
#define FLOATSPEED      (FRACUNIT*4)

// follow a player exlusively for 3 seconds
#define	BASETHRESHOLD   (100)

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS   (128)
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

// player radius for movement checking
#define PLAYERRADIUS    (FRACUNIT*16)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       (FRACUNIT*32)

#define GRAVITY         (FRACUNIT)
#define MAXMOVE         (FRACUNIT*30)

#define USERANGE        (FRACUNIT*64)
#define MELEERANGE      (FRACUNIT*64)
#define MISSILERANGE    (FRACUNIT*32*64)

// [JN] Weapon positioning. Also used for centering while firing.
#define WEAPONTOP       (32*FRACUNIT)
#define WEAPONBOTTOM    (128*FRACUNIT)

// -----------------------------------------------------------------------------
// P_CEILNG
// -----------------------------------------------------------------------------

#define CEILSPEED   (FRACUNIT)
#define CEILWAIT    (150)
#define MAXCEILINGS (30)

typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise
} ceiling_e;

typedef struct
{
    thinker_t  thinker;
    ceiling_e  type;
    sector_t  *sector;
    fixed_t    bottomheight;
    fixed_t    topheight;
    fixed_t    speed;
    boolean    crush;
    // 1 = up, 0 = waiting, -1 = down
    int        direction;
    // ID
    int        tag;                   
    int        olddirection;
} ceiling_t;

extern int  EV_CeilingCrushStop(line_t *line);
extern int  EV_DoCeiling (line_t *line, ceiling_e type);
extern void P_ActivateInStasisCeiling (line_t *line);
extern void P_AddActiveCeiling (ceiling_t *c);
extern void P_RemoveActiveCeiling (ceiling_t *c);
extern void T_MoveCeiling (ceiling_t *ceiling);

extern ceiling_t *activeceilings[MAXCEILINGS];

// -----------------------------------------------------------------------------
// P_DOORS
// -----------------------------------------------------------------------------

#define VDOORSPEED  (FRACUNIT*2)
#define VDOORWAIT   (150)

typedef enum
{
    vld_normal,
    vld_close30ThenOpen,
    vld_close,
    vld_open,
    vld_raiseIn5Mins,
    vld_blazeRaise,
    vld_blazeOpen,
    vld_blazeClose
} vldoor_e;

typedef struct
{
    thinker_t  thinker;
    vldoor_e   type;
    sector_t  *sector;
    fixed_t    topheight;
    fixed_t    speed;
    // 1 = up, 0 = waiting at top, -1 = down
    int        direction;
    // tics to wait at the top
    int        topwait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int        topcountdown;
} vldoor_t;

extern int  EV_DoDoor (line_t *line, vldoor_e type);
extern int  EV_DoLockedDoor (line_t *line, vldoor_e type, mobj_t *thing);
extern void EV_VerticalDoor (line_t *line, mobj_t *thing);
extern void P_SpawnDoorCloseIn30 (sector_t *sec);
extern void P_SpawnDoorRaiseIn5Mins (sector_t *sec, int secnum);
extern void T_VerticalDoor (vldoor_t *door);

// -----------------------------------------------------------------------------
// P_ENEMY
// -----------------------------------------------------------------------------

extern void A_BabyMetal (mobj_t *mo);
extern void A_BossDeath (mobj_t *mo);
extern void A_BrainAwake (mobj_t *mo);
extern void A_BrainDie (mobj_t *mo);
extern void A_BrainExplode (mobj_t *mo);
extern void A_BrainPain (mobj_t *mo);
extern void A_BrainScream (mobj_t *mo);
extern void A_BrainSpit (mobj_t *mo);
extern void A_BruisAttack (mobj_t *actor);
extern void A_BspiAttack (mobj_t *actor);
extern void A_Chase (mobj_t *actor);
extern void A_CPosAttack (mobj_t *actor);
extern void A_CPosRefire (mobj_t *actor);
extern void A_CyberAttack (mobj_t *actor);
extern void A_FaceTarget (mobj_t *actor);
extern void A_Fall (mobj_t *actor);
extern void A_FatAttack1 (mobj_t *actor);
extern void A_FatAttack2 (mobj_t *actor);
extern void A_FatAttack3 (mobj_t *actor);
extern void A_FatRaise (mobj_t *actor);
extern void A_Fire (mobj_t *actor);
extern void A_FireCrackle (mobj_t *actor);
extern void A_HeadAttack (mobj_t *actor);
extern void A_Hoof (mobj_t *mo);
extern void A_KeenDie (mobj_t *mo);
extern void A_Look (mobj_t *actor);
extern void A_Metal (mobj_t *mo);
extern void A_Pain (mobj_t *actor);
extern void A_PainAttack (mobj_t *actor);
extern void A_PainDie (mobj_t *actor);
extern void A_PosAttack (mobj_t *actor);
extern void A_SargAttack (mobj_t *actor);
extern void A_Scream (mobj_t *actor);
extern void A_SkelFist (mobj_t *actor);
extern void A_SkelMissile (mobj_t *actor);
extern void A_SkelWhoosh (mobj_t *actor);
extern void A_SkullAttack (mobj_t *actor);
extern void A_SpawnFly (mobj_t *mo);
extern void A_SpawnSound (mobj_t *mo);
extern void A_SpidRefire (mobj_t *actor);
extern void A_SPosAttack (mobj_t *actor);
extern void A_StartFire (mobj_t *actor);
extern void A_Tracer (mobj_t *actor);
extern void A_TroopAttack (mobj_t *actor);
extern void A_VileAttack (mobj_t *actor);
extern void A_VileChase (mobj_t *actor);
extern void A_VileStart (mobj_t *actor);
extern void A_VileTarget (mobj_t *actor);
extern void A_XScream (mobj_t *actor);
extern void P_NoiseAlert (mobj_t *target, mobj_t *emmiter);

extern boolean P_CheckMeleeRange (mobj_t *actor);

// -----------------------------------------------------------------------------
// P_FLOOR
// -----------------------------------------------------------------------------

#define FLOORSPEED (FRACUNIT)

typedef enum
{
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,       
    donutRaise,
    raiseFloor512
} floor_e;

typedef enum
{
    build8,  // slowly build by 8
    turbo16  // quickly build by 16
} stair_e;

typedef struct
{
    thinker_t thinker;
    floor_e   type;
    boolean   crush;
    sector_t *sector;
    int       direction;
    int       newspecial;
    short     texture;
    fixed_t   floordestheight;
    fixed_t   speed;
} floormove_t;

typedef enum
{
    ok,
    crushed,
    pastdest
} result_e;

extern int  EV_BuildStairs (line_t *line, stair_e type);
extern int  EV_DoFloor (line_t *line, floor_e floortype);
extern void T_MoveFloor (floormove_t *floor);

extern result_e T_MovePlane (sector_t *sector, fixed_t speed, fixed_t dest,
                             boolean crush, int floorOrCeiling, int direction);

// -----------------------------------------------------------------------------
// P_INTER
// -----------------------------------------------------------------------------

extern boolean P_GivePower(player_t*, int);
extern void    P_TouchSpecialThing (mobj_t *special, mobj_t *toucher);
extern void    P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage);

extern int maxammo[NUMAMMO];
extern int clipammo[NUMAMMO];

// -----------------------------------------------------------------------------
// P_LIGHTS
// -----------------------------------------------------------------------------

#define GLOWSPEED       8
#define STROBEBRIGHT    5
#define FASTDARK        15
#define SLOWDARK        35

typedef struct
{
    thinker_t  thinker;
    sector_t  *sector;
    int        count;
    int        maxlight;
    int        minlight;
} fireflicker_t;

typedef struct
{
    thinker_t  thinker;
    sector_t  *sector;
    int        count;
    int        maxlight;
    int        minlight;
    int        maxtime;
    int        mintime;
} lightflash_t;

typedef struct
{
    thinker_t  thinker;
    sector_t  *sector;
    int        count;
    int        minlight;
    int        maxlight;
    int        darktime;
    int        brighttime;
} strobe_t;

typedef struct
{
    thinker_t  thinker;
    sector_t  *sector;
    int        minlight;
    int        maxlight;
    int        direction;
} glow_t;

extern void EV_LightTurnOn (line_t *line, int bright);
extern void EV_StartLightStrobing (line_t *line);
extern void EV_TurnTagLightsOff (line_t *line);
extern void P_SpawnFireFlicker (sector_t *sector);
extern void P_SpawnGlowingLight (sector_t *sector);
extern void P_SpawnLightFlash (sector_t *sector);
extern void P_SpawnStrobeFlash (sector_t *sector, int fastOrSlow, int inSync);
extern void T_FireFlicker (fireflicker_t *flick);
extern void T_Glow (glow_t *g);
extern void T_LightFlash (lightflash_t *flash);
extern void T_StrobeFlash (strobe_t *flash);

// -----------------------------------------------------------------------------
// P_MAP
// -----------------------------------------------------------------------------

// fraggle: I have increased the size of this buffer.  In the original Doom,
// overrunning past this limit caused other bits of memory to be overwritten,
// affecting demo playback.  However, in doing so, the limit was still
// exceeded.  So we have to support more than 8 specials.
//
// We keep the original limit, to detect what variables in memory were
// overwritten (see SpechitOverrun())

#define MAXSPECIALCROSS_ORIGINAL    8
#define MAXSPECIALCROSS             20

extern boolean P_ChangeSector (sector_t *sector, boolean crunch);
extern boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y);
extern boolean P_CheckSight (mobj_t *t1, mobj_t *t2);
extern boolean P_TeleportMove (mobj_t *thing, fixed_t x, fixed_t y);
extern boolean P_TryMove (mobj_t *thing, fixed_t x, fixed_t y);
extern boolean PIT_ChangeSector (mobj_t *thing);
extern boolean PIT_RadiusAttack (mobj_t *thing);
extern boolean PTR_NoWayAudible (line_t *line);
extern fixed_t P_AimLineAttack (mobj_t *t1, angle_t angle, fixed_t distance, boolean safe);
extern void    P_ApplyTorque(mobj_t *mo);
extern void    P_LineAttack (mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);
extern void    P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage);
extern void    P_SlideMove (mobj_t *mo);
extern void    P_UseLines (player_t *player);

// If "floatok" true, move would be ok. If within "tmfloorz - tmceilingz".
extern boolean floatok;
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;

extern line_t  *ceilingline;
extern line_t **spechit;
extern int      numspechit;

extern mobj_t  *linetarget;  // who got hit (or NULL)
extern boolean  safe_intercept;

// -----------------------------------------------------------------------------
// P_MAPUTL
// -----------------------------------------------------------------------------

// Extended MAXINTERCEPTS, to allow for intercepts overrun emulation.
#define MAXINTERCEPTS_ORIGINAL 128
#define MAXINTERCEPTS          (MAXINTERCEPTS_ORIGINAL + 61)
// [JN] CRL - a number of triggered intercepts which is causing All-Ghosts effect.
#define MAXINTERCEPTS_ALLGHOSTS 147

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

typedef struct
{
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;
} divline_t;

typedef struct
{
    fixed_t	frac;   // along trace line
    boolean	isaline;
    union
    {
        mobj_t *thing;
        line_t *line;
    } d;
} intercept_t;

typedef boolean (*traverser_t) (intercept_t *in);

extern boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) );
extern boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) );
extern boolean P_PathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                               int flags, boolean (*trav) (intercept_t*));
extern fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
extern fixed_t P_InterceptVector (divline_t* v2, divline_t* v1);
extern int     P_BoxOnLineSide (fixed_t* tmbox, line_t* ld);
extern int     P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t* line);
extern int     P_PointOnLineSide (fixed_t x, fixed_t y, line_t* line);
extern void    P_LineOpening (line_t* linedef);
extern void    P_MakeDivline (line_t* li, divline_t* dl);
extern void    P_SetThingPosition (mobj_t *thing);
extern void    P_UnsetThingPosition (mobj_t *thing);

extern fixed_t opentop;
extern fixed_t openbottom;
extern fixed_t openrange;
extern fixed_t lowfloor;

extern divline_t trace;

// -----------------------------------------------------------------------------
// P_MOBJ
// -----------------------------------------------------------------------------

#define ONFLOORZ    INT_MIN
#define ONCEILINGZ  INT_MAX

// Time interval for item respawning.
#define ITEMQUESIZE 128

extern boolean P_SetMobjState (mobj_t *mobj, statenum_t state);
extern mobj_t *Crispy_PlayerSO (int p); // [crispy] weapon sound sources
extern mobj_t *P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type);
extern mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
extern mobj_t *P_SubstNullMobj (mobj_t *th);
extern void    P_MobjThinker (mobj_t *mobj);
extern void    P_RemoveMobj (mobj_t *th);
extern void    P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage, mobj_t *target);
extern void    P_SpawnMapThing (mapthing_t *mthing);
extern void    P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type);
extern void    P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
extern void    P_SpawnPuffSafe (fixed_t x, fixed_t y, fixed_t z, boolean safe);
extern void    P_RespawnSpecials (void);

extern mapthing_t itemrespawnque[ITEMQUESIZE];
extern int        itemrespawntime[ITEMQUESIZE];
extern int        iquehead;
extern int        iquetail;

// -----------------------------------------------------------------------------
// P_PLATS
// -----------------------------------------------------------------------------

#define PLATWAIT    (3)
#define PLATSPEED   (FRACUNIT)

// [JN] CRL - increase actual limit, render counter will blink
// if active plats value reaches 31 and above.
#define MAXPLATS    7681

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
    raiseToNearestAndChange,
    blazeDWUS
} plattype_e;

typedef struct
{
    thinker_t   thinker;
    sector_t   *sector;
    fixed_t     speed;
    fixed_t     low;
    fixed_t     high;
    int         wait;
    int         count;
    plat_e      status;
    plat_e      oldstatus;
    boolean     crush;
    int         tag;
    plattype_e  type;
} plat_t;

extern int  EV_DoPlat (line_t *line, plattype_e type, int amount);
extern void EV_StopPlat (line_t *line);
extern void P_ActivateInStasis (int tag);
extern void P_AddActivePlat (plat_t *plat);
extern void P_RemoveActivePlat (plat_t *plat);
extern void T_PlatRaise (plat_t *plat);

extern plat_t *activeplats[MAXPLATS];

// -----------------------------------------------------------------------------
// P_PSPR
// -----------------------------------------------------------------------------

extern void P_SetupPsprites (player_t *curplayer);
extern void P_MovePsprites (player_t *curplayer);
extern void P_DropWeapon (player_t *player);

extern void A_Light0 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Light1 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Light2 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_WeaponReady (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Lower (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Raise (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Punch (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_ReFire (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FirePistol (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FireShotgun (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FireShotgun2 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_CheckReload (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_OpenShotgun2 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_LoadShotgun2 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_CloseShotgun2 (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FireCGun (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_GunFlash (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FireMissile (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_Saw (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FirePlasma (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_BFGsound (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_FireBFG (mobj_t *mobj, player_t *player, pspdef_t *psp);
extern void A_BFGSpray (mobj_t *mo);
extern void A_Explode (mobj_t *thingy);
extern void A_PlayerScream (mobj_t* mo);

// -----------------------------------------------------------------------------
// P_SAVEG
// -----------------------------------------------------------------------------

#define SAVEGAME_EOF    0x1d
#define VERSIONSIZE     16

// maximum size of a savegame description
#define SAVESTRINGSIZE  24

extern boolean  P_ReadSaveGameEOF(void);
extern boolean  P_ReadSaveGameHeader(void);
extern char    *P_SaveGameFile(int slot);
extern char    *P_TempSaveGameFile(void);
extern void     P_ArchiveAutomap (void);
extern void     P_ArchivePlayers (void);
extern void     P_ArchiveSpecials (void);
extern void     P_ArchiveThinkers (void);
extern void     P_ArchiveTotalTimes (void);
extern void     P_ArchiveWorld (void);
extern void     P_RestoreTargets (void);
extern void     P_UnArchiveAutomap (void);
extern void     P_UnArchivePlayers (void);
extern void     P_UnArchiveSpecials (void);
extern void     P_UnArchiveThinkers (void);
extern void     P_UnArchiveTotalTimes (void);
extern void     P_UnArchiveWorld (void);
extern void     P_WriteSaveGameEOF(void);
extern void     P_WriteSaveGameHeader(char *description);

extern FILE    *save_stream;
extern boolean  savegame_error;

extern const uint32_t P_ThinkerToIndex (const thinker_t *thinker);

// -----------------------------------------------------------------------------
// P_SETUP
// -----------------------------------------------------------------------------

// [crispy] blinking key or skull in the status bar
#define KEYBLINKMASK 0x8
#define KEYBLINKTICS (7*KEYBLINKMASK)
extern int st_keyorskull[3];

extern void P_SegLengths (boolean contrast_only);
extern void P_SetupLevel (int episode, int map);
extern void P_Init (void);

extern byte     *rejectmatrix;  // for fast sight rejection
extern int32_t  *blockmaplump;  // offsets in blockmap are from here
extern int32_t  *blockmap;
extern int       bmapwidth;
extern int       bmapheight;    // in mapblocks
extern fixed_t   bmaporgx;
extern fixed_t   bmaporgy;      // origin of block map
extern mobj_t  **blocklinks;    // for thing chains

// -----------------------------------------------------------------------------
// P_SPEC
// -----------------------------------------------------------------------------

// Define values for map objects
#define MO_TELEPORTMAN (14)

extern boolean P_UseSpecialLine (mobj_t *thing, line_t *line, int side);
extern fixed_t P_FindHighestCeilingSurrounding (sector_t *sec);
extern fixed_t P_FindHighestFloorSurrounding (sector_t* sec);
extern fixed_t P_FindLowestCeilingSurrounding (sector_t *sec);
extern fixed_t P_FindLowestFloorSurrounding (sector_t* sec);
extern fixed_t P_FindNextHighestFloor (sector_t *sec, int currentheight);
extern int     P_FindMinSurroundingLight (sector_t *sector, int max);
extern int     P_FindSectorFromLineTag (line_t *line, int start);
extern void    P_CrossSpecialLine (int linenum, int side, mobj_t *thing);
extern void    P_InitPicAnims (void);
extern void    P_PlayerInSpecialSector (player_t *player);
extern void    P_ShootSpecialLine (mobj_t *thing, line_t *line);
extern void    P_SpawnSpecials (void);
extern void    P_UpdateSpecials (void);
extern void    R_InterpolateTextureOffsets (void);
// [crispy] more MBF code pointers
extern void    P_CrossSpecialLinePtr (line_t *line, int side, mobj_t *thing);

extern int       twoSided (int sector, int line);
extern sector_t *getSector (int currentSector, int line, int side);
extern side_t   *getSide (int currentSector, int line, int side);
extern sector_t *getNextSector (line_t *line, sector_t *sec);
extern int       EV_DoDonut (line_t *line);

// End-level timer (-TIMER option)
extern boolean levelTimer;
extern int     levelTimeCount;

// -----------------------------------------------------------------------------
// P_SWITCH
// -----------------------------------------------------------------------------

// max # of wall switches in a level
#define MAXSWITCHES     (50)

// 4 players, 4 buttons each at once, max.
// [JN] CRL - increased, warnings will appear instead of crash.
#define MAXBUTTONS      (16*64)

// 1 second, in ticks. 
#define BUTTONTIME      (TICRATE)

typedef struct
{
    char  name1[9];
    char  name2[9];
    short episode;
} switchlist_t;

typedef enum
{
    top,
    middle,
    bottom
} bwhere_e;

typedef struct
{
    line_t      *line;
    bwhere_e     where;
    int          btexture;
    int          btimer;
    degenmobj_t *soundorg;
} button_t;

extern void P_ChangeSwitchTexture (line_t *line, int useAgain);
extern void P_InitSwitchList (void);
extern void P_StartButton (line_t *line, bwhere_e w, int texture, int time);

extern button_t	*buttonlist; 
extern int       maxbuttons;

// -----------------------------------------------------------------------------
// P_TELEPT
// -----------------------------------------------------------------------------

extern int EV_Teleport (line_t *line, int side, mobj_t *thing);

// -----------------------------------------------------------------------------
// P_TICK
// -----------------------------------------------------------------------------

extern void P_InitThinkers (void);
extern void P_AddThinker (thinker_t *thinker);
extern void P_RemoveThinker (thinker_t *thinker);
extern void P_Ticker (void);

// both the head and tail of the thinker list
extern thinker_t thinkercap;

// -----------------------------------------------------------------------------
// P_USER
// -----------------------------------------------------------------------------

#define TOCENTER    -8
#define MLOOKUNIT    8
// The value `160` is used for the denominator because in Vanilla Doom
// the horizontal FOV is 90° and spans 320 px, so the height of the
// resulting triangle is 160.
// Heretic/Hexen used value `173` presumably because Raven assumed an
// equilateral triangle (i.e. 60°) for the vertical FOV which spans
// 200 px, so the height of that triangle would be 100*sqrt(3) = 173.
#define PLAYER_SLOPE(a)	((((a)->lookdir / MLOOKUNIT) << FRACBITS) / 160)

extern void P_PlayerThink (player_t *player);
