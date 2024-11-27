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


// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "i_swap.h"
#include "i_timer.h"
#include "am_map.h"
#include "ct_chat.h"

#include "id_func.h"


// TYPES -------------------------------------------------------------------

typedef struct Cheat_s
{
    void (*func) (player_t * player, struct Cheat_s * cheat);
    cheatseq_t *seq;
} Cheat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrawSoundInfo(void);
static void DrINumber(signed int val, int x, int y);
static void DrRedINumber(signed int val, int x, int y);
static void DrBNumber(signed int val, int x, int y);
static void DrawCommonBar(void);
static void DrawMainBar(void);
static void DrawInventoryBar(void);
static void DrawKeyBar(void);
static void DrawWeaponPieces(void);
static void DrawFullScreenStuff(void);
static void DrawAnimatedIcons(void);
static boolean HandleCheats(byte key);
static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean * eat);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

boolean DebugSound;             // Debug flag for displaying sound info
boolean inventory;
int curpos;
int inv_ptr;
int ArtifactFlash;

// [crispy] for widescreen status bar background
pixel_t *st_backing_screen;

// [JN] Externalazied variable of current palette index.
int SB_palette = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int HealthMarker;
//static int ChainWiggle;
static player_t *CPlayer;
static int SpinFlylump;
static int SpinMinotaurLump;
static int SpinSpeedLump;
static int SpinDefenseLump;

static int FontBNumBase;
static int PlayPalette;

static patch_t *PatchH2BAR;
static patch_t *PatchH2TOP;
static patch_t *PatchLFEDGE;
static patch_t *PatchRTEDGE;
static patch_t *PatchARMCLEAR;
static patch_t *PatchARTICLEAR;
static patch_t *PatchMANACLEAR;
static patch_t *PatchKILLS;
static patch_t *PatchMANAVIAL1;
static patch_t *PatchMANAVIAL2;
static patch_t *PatchMANAVIALDIM1;
static patch_t *PatchMANAVIALDIM2;
static patch_t *PatchMANADIM1;
static patch_t *PatchMANADIM2;
static patch_t *PatchMANABRIGHT1;
static patch_t *PatchMANABRIGHT2;
static patch_t *PatchCHAIN;
static patch_t *PatchSTATBAR;
static patch_t *PatchKEYBAR;
static patch_t *PatchLIFEGEM;
static patch_t *PatchSELECTBOX;
static patch_t *PatchINumbers[10];
static patch_t *PatchNEGATIVE;
static patch_t *PatchSmNumbers[10];
static patch_t *PatchINVBAR;
static patch_t *PatchWEAPONSLOT;
static patch_t *PatchWEAPONFULL;
static patch_t *PatchPIECE1;
static patch_t *PatchPIECE2;
static patch_t *PatchPIECE3;
static patch_t *PatchINVLFGEM1;
static patch_t *PatchINVLFGEM2;
static patch_t *PatchINVRTGEM1;
static patch_t *PatchINVRTGEM2;

// -----------------------------------------------------------------------------
//
// CHEAT CODES
//
// -----------------------------------------------------------------------------

// [JN] CRL - prevent other than typing actions in G_Responder
// while cheat tics are ticking.
static cheatseq_t CheatWaitSeq = CHEAT("id", 0);
static void CheatWaitFunc (player_t *player, Cheat_t *cheat);

// Toggle god mode
static cheatseq_t CheatGodDoomSeq = CHEAT("iddqd", 0);
static cheatseq_t CheatGodHticSeq = CHEAT("quicken", 0);
static cheatseq_t CheatGodHexnSeq = CHEAT("satan", 0);
static cheatseq_t CheatGodHxSwSeq = CHEAT("bgokey", 0);
static void CheatGodFunc(player_t *player, Cheat_t * cheat);

// Toggle no clipping mode
static cheatseq_t CheatNoClipDoomSeq = CHEAT("idclip", 0);
static cheatseq_t CheatNoClipDmSwSeq = CHEAT("idspispopd", 0);
static cheatseq_t CheatNoClipHticSeq = CHEAT("kitty", 0);
static cheatseq_t CheatNoClipHexnSeq = CHEAT("casper", 0);
static cheatseq_t CheatNoClipHxSwSeq = CHEAT("rjohnson", 0);
static void CheatNoClipFunc (player_t *player, Cheat_t *cheat);

// Get all weapons and mana
static cheatseq_t CheatWeaponsDoomSeq = CHEAT("idfa", 0);
static cheatseq_t CheatWeaponsHticSeq = CHEAT("rambo", 0);
static cheatseq_t CheatWeaponsHexnSeq = CHEAT("nra", 0);
static cheatseq_t CheatWeaponsHxSwSeq = CHEAT("crhinehart", 0);
static void CheatWeaponsFunc (player_t *player, Cheat_t *cheat);

// Get all weapons and mana and keys
static cheatseq_t CheatWpnsKeysDoomSeq = CHEAT("idkfa", 0);
static void CheatWpnsKeysFunc (player_t *player, Cheat_t *cheat);

// Get full health
static cheatseq_t CheatHealthHticSeq = CHEAT("ponce", 0);
static cheatseq_t CheatHealthHexnSeq = CHEAT("clubmed", 0);
static cheatseq_t CheatHealthHxSwSeq = CHEAT("sgurno", 0);
static void CheatHealthFunc (player_t *player, Cheat_t *cheat);

// Get all keys
static cheatseq_t CheatKeysDoomSeq = CHEAT("idka", 0);
static cheatseq_t CheatKeysHticSeq = CHEAT("skel", 0);
static cheatseq_t CheatKeysHexnSeq = CHEAT("locksmith", 0);
static cheatseq_t CheatKeysHxSwSeq = CHEAT("mraymondjudy", 0);
static void CheatKeysFunc (player_t *player, Cheat_t *cheat);

// Get all artifacts
static cheatseq_t CheatArtifactsHexnSeq = CHEAT("indiana", 0);
static cheatseq_t CheatArtifactsHxSwSeq = CHEAT("braffel", 0);
static void CheatArtifactAllFunc (player_t *player, Cheat_t *cheat);

// Get all puzzle pieces
static cheatseq_t CheatPuzzleHexnSeq = CHEAT("sherlock", 0);
static cheatseq_t CheatPuzzleHxSwSeq = CHEAT("tmoore", 0);
static void CheatPuzzleFunc (player_t *player, Cheat_t *cheat);

// Warp to new level
static cheatseq_t CheatWarpDoomSeq = CHEAT("idclev", 2);
static cheatseq_t CheatWarpHticSeq = CHEAT("engage", 2);
static cheatseq_t CheatWarpHexnSeq = CHEAT("visit", 2);
static cheatseq_t CheatWarpHxSwSeq = CHEAT("bpelletier", 2);
static void CheatWarpFunc (player_t *player, Cheat_t *cheat);

// Become a pig
static cheatseq_t CheatPigHticSeq = CHEAT("cockadoodledoo", 0);
static cheatseq_t CheatPigHexnSeq = CHEAT("deliverance", 0);
static cheatseq_t CheatPigHxSwSeq = CHEAT("ebiessman", 0);
static void CheatPigFunc (player_t *player, Cheat_t *cheat);

// Kill all monsters
static cheatseq_t CheatMassacreDoomBSeq = CHEAT("tntem", 0);
static cheatseq_t CheatMassacreDoomMSeq = CHEAT("killem", 0);
static cheatseq_t CheatMassacreHticSeq = CHEAT("massacre", 0);
static cheatseq_t CheatMassacreHexnSeq = CHEAT("butcher", 0);
static cheatseq_t CheatMassacreHxSwSeq = CHEAT("cstika", 0);
static void CheatMassacreFunc (player_t *player, Cheat_t *cheat);

// New class
static cheatseq_t CheatClass1HexnSeq = CHEAT("shadowcaster", 0);
static cheatseq_t CheatClass2HexnSeq = CHEAT("shadowcaster", 1);
static cheatseq_t CheatClass1HxSwSeq = CHEAT("plipo", 0);
static cheatseq_t CheatClass2HxSwSeq = CHEAT("plipo", 1);
static void CheatClassFunc1(player_t * player, Cheat_t * cheat);
static void CheatClassFunc2(player_t * player, Cheat_t * cheat);

// Restart level from scratch
static cheatseq_t CheatInitSeq = CHEAT("init", 0);
static void CheatInitFunc (player_t *player, Cheat_t *cheat);

// Show Hexen (not source port) version
static cheatseq_t CheatVersionDoomSeq = CHEAT("version", 0);
static cheatseq_t CheatVersionHexnSeq = CHEAT("mrjones", 0);
static cheatseq_t CheatVersionHxSwSeq = CHEAT("pmacarther", 0);
static void CheatVersionFunc (player_t *player, Cheat_t *cheat);

// Show player coords and map info
static cheatseq_t CheatDebugDoomSeq = CHEAT("idmypos", 0);
static cheatseq_t CheatDebugHexnSeq = CHEAT("where", 0);
static cheatseq_t CheatDebugHxSwSeq = CHEAT("jsumwalt", 0);
static void CheatDebugFunc (player_t *player, Cheat_t *cheat);

// Execute defined ACS map scripts
static cheatseq_t CheatScriptSeqHexn1 = CHEAT("puke", 0);
static cheatseq_t CheatScriptSeqHexn2 = CHEAT("puke", 1);
static cheatseq_t CheatScriptSeqHexn3 = CHEAT("puke", 2);
static cheatseq_t CheatScriptSeqHxSw1 = CHEAT("mwagabaza", 0);
static cheatseq_t CheatScriptSeqHxSw2 = CHEAT("mwagabaza", 1);
static cheatseq_t CheatScriptSeqHxSw3 = CHEAT("mwagabaza", 2);
static void CheatScriptFunc1 (player_t *player, Cheat_t *cheat);
static void CheatScriptFunc2 (player_t *player, Cheat_t *cheat);
static void CheatScriptFunc3 (player_t *player, Cheat_t *cheat);

// Reveal all map
static cheatseq_t CheatRevealDoomSeq = CHEAT("iddt", 0);
static cheatseq_t CheatRevealHticSeq = CHEAT("ravmap", 0);
static cheatseq_t CheatRevealHexnSeq = CHEAT("mapsco", 0);
static cheatseq_t CheatRevealHxSwSeq = CHEAT("reveal", 0);
static void CheatRevealFunc (player_t *player, Cheat_t *cheat);

// [JN] Freeze mode
static cheatseq_t CheatFREEZESeq = CHEAT("freeze", 0);
static void CheatFREEZEFunc (player_t *player, Cheat_t *cheat);

// [JN] No target mode
static cheatseq_t CheatNOTARGETSeq = CHEAT("notarget", 0);
static void CheatNOTARGETFunc (player_t *player, Cheat_t *cheat);

// [JN] Buddha mode
static cheatseq_t CheatBUDDHASeq = CHEAT("buddha", 0);
static void CheatBUDDHAFunc (player_t *player, Cheat_t *cheat);


static Cheat_t Cheats[] = {
    { CheatWaitFunc,        &CheatWaitSeq          },
    // Toggle god mode
    { CheatGodFunc,         &CheatGodDoomSeq       },
    { CheatGodFunc,         &CheatGodHticSeq       },
    { CheatGodFunc,         &CheatGodHexnSeq       },
    { CheatGodFunc,         &CheatGodHxSwSeq       },
    // Toggle no clipping mode
    { CheatNoClipFunc,      &CheatNoClipDoomSeq    },
    { CheatNoClipFunc,      &CheatNoClipDmSwSeq    },
    { CheatNoClipFunc,      &CheatNoClipHticSeq    },
    { CheatNoClipFunc,      &CheatNoClipHexnSeq    },
    { CheatNoClipFunc,      &CheatNoClipHxSwSeq    },
    // Get all weapons and mana
    { CheatWeaponsFunc,     &CheatWeaponsDoomSeq   },
    { CheatWeaponsFunc,     &CheatWeaponsHticSeq   },
    { CheatWeaponsFunc,     &CheatWeaponsHexnSeq   },
    { CheatWeaponsFunc,     &CheatWeaponsHxSwSeq   },
    // Get all weapons and mana and keys
    { CheatWpnsKeysFunc,    &CheatWpnsKeysDoomSeq  },
    // Get full health
    { CheatHealthFunc,      &CheatHealthHticSeq    },
    { CheatHealthFunc,      &CheatHealthHexnSeq    },
    { CheatHealthFunc,      &CheatHealthHxSwSeq    },
    // Get all keys
    { CheatKeysFunc,        &CheatKeysDoomSeq      },
    { CheatKeysFunc,        &CheatKeysHticSeq      },
    { CheatKeysFunc,        &CheatKeysHexnSeq      },
    { CheatKeysFunc,        &CheatKeysHxSwSeq      },
    // Get all artifacts
    { CheatArtifactAllFunc, &CheatArtifactsHexnSeq },
    { CheatArtifactAllFunc, &CheatArtifactsHxSwSeq },
    // Get all puzzle pieces
    { CheatPuzzleFunc,      &CheatPuzzleHexnSeq    },
    { CheatPuzzleFunc,      &CheatPuzzleHxSwSeq    },
    // Warp to new level
    { CheatWarpFunc,        &CheatWarpDoomSeq      },
    { CheatWarpFunc,        &CheatWarpHticSeq      },
    { CheatWarpFunc,        &CheatWarpHexnSeq      },
    { CheatWarpFunc,        &CheatWarpHxSwSeq      },
    // Become a pig
    { CheatPigFunc,         &CheatPigHticSeq       },
    { CheatPigFunc,         &CheatPigHexnSeq       },
    { CheatPigFunc,         &CheatPigHxSwSeq       },
    // Kill all monsters
    { CheatMassacreFunc,    &CheatMassacreDoomBSeq },
    { CheatMassacreFunc,    &CheatMassacreDoomMSeq },
    { CheatMassacreFunc,    &CheatMassacreHticSeq  },
    { CheatMassacreFunc,    &CheatMassacreHexnSeq  },
    { CheatMassacreFunc,    &CheatMassacreHxSwSeq  },
    // New class
    { CheatClassFunc1,      &CheatClass1HexnSeq    },
    { CheatClassFunc2,      &CheatClass2HexnSeq    },
    { CheatClassFunc1,      &CheatClass1HxSwSeq    },
    { CheatClassFunc2,      &CheatClass2HxSwSeq    },
    // Restart level from scratch
    { CheatInitFunc,        &CheatInitSeq          },
    // Show Hexen (not source port) version
    { CheatVersionFunc,     &CheatVersionDoomSeq   },
    { CheatVersionFunc,     &CheatVersionHexnSeq   },
    { CheatVersionFunc,     &CheatVersionHxSwSeq   },
    // Show player coords and map info
    { CheatDebugFunc,       &CheatDebugDoomSeq     },
    { CheatDebugFunc,       &CheatDebugHexnSeq     },
    { CheatDebugFunc,       &CheatDebugHxSwSeq     },
    // Execute defined ACS map scripts
    { CheatScriptFunc1,     &CheatScriptSeqHexn1   },
    { CheatScriptFunc2,     &CheatScriptSeqHexn2   },
    { CheatScriptFunc3,     &CheatScriptSeqHexn3   },
    { CheatScriptFunc1,     &CheatScriptSeqHxSw1   },
    { CheatScriptFunc2,     &CheatScriptSeqHxSw2   },
    { CheatScriptFunc3,     &CheatScriptSeqHxSw3   },
    // Reveal all map
    { CheatRevealFunc,      &CheatRevealDoomSeq    },
    { CheatRevealFunc,      &CheatRevealHticSeq    },
    { CheatRevealFunc,      &CheatRevealHexnSeq    },
    { CheatRevealFunc,      &CheatRevealHxSwSeq    },
    // [JN] ID-specific modes
    { CheatFREEZEFunc,     &CheatFREEZESeq         },
    { CheatNOTARGETFunc,   &CheatNOTARGETSeq       },
    { CheatBUDDHAFunc,     &CheatBUDDHASeq         },
};

#define SET_CHEAT(cheat, seq) \
    { memcpy(cheat.sequence, seq, sizeof(seq)); \
      cheat.sequence_len = sizeof(seq) - 1; }

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SB_Init
//
//==========================================================================

void SB_Init(void)
{
    int i;
    int startLump;

    PatchH2BAR = W_CacheLumpName("H2BAR", PU_STATIC);
    PatchH2TOP = W_CacheLumpName("H2TOP", PU_STATIC);
    PatchINVBAR = W_CacheLumpName("INVBAR", PU_STATIC);
    PatchLFEDGE = W_CacheLumpName("LFEDGE", PU_STATIC);
    PatchRTEDGE = W_CacheLumpName("RTEDGE", PU_STATIC);
    PatchSTATBAR = W_CacheLumpName("STATBAR", PU_STATIC);
    PatchKEYBAR = W_CacheLumpName("KEYBAR", PU_STATIC);
    PatchSELECTBOX = W_CacheLumpName("SELECTBOX", PU_STATIC);
    PatchARTICLEAR = W_CacheLumpName("ARTICLS", PU_STATIC);
    PatchARMCLEAR = W_CacheLumpName("ARMCLS", PU_STATIC);
    PatchMANACLEAR = W_CacheLumpName("MANACLS", PU_STATIC);
    PatchMANAVIAL1 = W_CacheLumpName("MANAVL1", PU_STATIC);
    PatchMANAVIAL2 = W_CacheLumpName("MANAVL2", PU_STATIC);
    PatchMANAVIALDIM1 = W_CacheLumpName("MANAVL1D", PU_STATIC);
    PatchMANAVIALDIM2 = W_CacheLumpName("MANAVL2D", PU_STATIC);
    PatchMANADIM1 = W_CacheLumpName("MANADIM1", PU_STATIC);
    PatchMANADIM2 = W_CacheLumpName("MANADIM2", PU_STATIC);
    PatchMANABRIGHT1 = W_CacheLumpName("MANABRT1", PU_STATIC);
    PatchMANABRIGHT2 = W_CacheLumpName("MANABRT2", PU_STATIC);
    PatchINVLFGEM1 = W_CacheLumpName("invgeml1", PU_STATIC);
    PatchINVLFGEM2 = W_CacheLumpName("invgeml2", PU_STATIC);
    PatchINVRTGEM1 = W_CacheLumpName("invgemr1", PU_STATIC);
    PatchINVRTGEM2 = W_CacheLumpName("invgemr2", PU_STATIC);

//      PatchCHAINBACK = W_CacheLumpName("CHAINBACK", PU_STATIC);
    startLump = W_GetNumForName("IN0");
    for (i = 0; i < 10; i++)
    {
        PatchINumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PatchNEGATIVE = W_CacheLumpName("NEGNUM", PU_STATIC);
    FontBNumBase = W_GetNumForName("FONTB16");
    startLump = W_GetNumForName("SMALLIN0");
    for (i = 0; i < 10; i++)
    {
        PatchSmNumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PlayPalette = W_GetNumForName("PLAYPAL");
    SpinFlylump = W_GetNumForName("SPFLY0");
    SpinMinotaurLump = W_GetNumForName("SPMINO0");
    SpinSpeedLump = W_GetNumForName("SPBOOT0");
    SpinDefenseLump = W_GetNumForName("SPSHLD0");

    st_backing_screen = (pixel_t *) Z_Malloc(MAXWIDTH * (ORIGSBARHEIGHT * MAXHIRES) * sizeof(*st_backing_screen), PU_STATIC, 0);
    if (deathmatch)
    {
        PatchKILLS = W_CacheLumpName("KILLS", PU_STATIC);
    }
    SB_SetClassData();
}

//==========================================================================
//
// SB_SetClassData
//
//==========================================================================

void SB_SetClassData(void)
{
    int class;

    class = PlayerClass[displayplayer]; // original player class (not pig)
    PatchWEAPONSLOT = W_CacheLumpNum(W_GetNumForName("wpslot0")
                                     + class, PU_STATIC);
    PatchWEAPONFULL = W_CacheLumpNum(W_GetNumForName("wpfull0")
                                     + class, PU_STATIC);
    PatchPIECE1 = W_CacheLumpNum(W_GetNumForName("wpiecef1")
                                 + class, PU_STATIC);
    PatchPIECE2 = W_CacheLumpNum(W_GetNumForName("wpiecef2")
                                 + class, PU_STATIC);
    PatchPIECE3 = W_CacheLumpNum(W_GetNumForName("wpiecef3")
                                 + class, PU_STATIC);
    PatchCHAIN = W_CacheLumpNum(W_GetNumForName("chain") + class, PU_STATIC);
    if (!netgame)
    {                           // single player game uses red life gem (the second gem)
        PatchLIFEGEM = W_CacheLumpNum(W_GetNumForName("lifegem")
                                      + maxplayers * class + 1, PU_STATIC);
    }
    else
    {
        PatchLIFEGEM = W_CacheLumpNum(W_GetNumForName("lifegem")
                                      + maxplayers * class + displayplayer,
                                      PU_STATIC);
    }
    SB_state = -1;
}

//==========================================================================
//
// SB_Ticker
//
//==========================================================================

void SB_Ticker(void)
{
    int delta;
    int curHealth;

    curHealth = players[displayplayer].mo->health;
    if (curHealth < 0)
    {
        curHealth = 0;
    }
    if (curHealth < HealthMarker)
    {
        delta = (HealthMarker - curHealth) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 6)
        {
            delta = 6;
        }
        HealthMarker -= delta;
    }
    else if (curHealth > HealthMarker)
    {
        delta = (curHealth - HealthMarker) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 6)
        {
            delta = 6;
        }
        HealthMarker += delta;
    }

    // [JN] Update IDWidget data.
    CPlayer = &players[displayplayer];
    IDWidget.kills = CPlayer->killcount;
    IDWidget.x = CPlayer->mo->x >> FRACBITS;
    IDWidget.y = CPlayer->mo->y >> FRACBITS;
    IDWidget.ang = CPlayer->mo->angle / ANG1;

#ifndef CRISPY_TRUECOLOR
    SB_PaletteFlash(false);
#else
    if (vis_smooth_palette)
    {
        SB_SmoothPaletteFlash(false);
    }
    else
    {
        SB_PaletteFlash(false);
    }
#endif
}

//==========================================================================
//
// DrINumber
//
// Draws a three digit number.
//
//==========================================================================

static void DrINumber(signed int val, int x, int y)
{
    patch_t *patch;
    int oldval;

    oldval = val;
    if (val < 0)
    {
        val = -val;
        if (val > 99)
        {
            val = 99;
        }
        if (val > 9)
        {
            patch = PatchINumbers[val / 10];
            V_DrawPatch(x + 8, y, patch);
            V_DrawPatch(x, y, PatchNEGATIVE);
        }
        else
        {
            V_DrawPatch(x + 8, y, PatchNEGATIVE);
        }
        val = val % 10;
        patch = PatchINumbers[val];
        V_DrawPatch(x + 16, y, patch);
        return;
    }
    if (val > 99)
    {
        patch = PatchINumbers[val / 100];
        V_DrawPatch(x, y, patch);
    }
    val = val % 100;
    if (val > 9 || oldval > 99)
    {
        patch = PatchINumbers[val / 10];
        V_DrawPatch(x + 8, y, patch);
    }
    val = val % 10;
    patch = PatchINumbers[val];
    V_DrawPatch(x + 16, y, patch);
}

//==========================================================================
//
// DrRedINumber
//
// Draws a three digit number using the red font
//
//==========================================================================

static void DrRedINumber(signed int val, int x, int y)
{
    patch_t *patch;
    int oldval;

    oldval = val;
    if (val < 0)
    {
        val = 0;
    }
    if (val > 99)
    {
        patch =
            W_CacheLumpNum(W_GetNumForName("inred0") + val / 100, PU_CACHE);
        V_DrawPatch(x, y, patch);
    }
    val = val % 100;
    if (val > 9 || oldval > 99)
    {
        patch =
            W_CacheLumpNum(W_GetNumForName("inred0") + val / 10, PU_CACHE);
        V_DrawPatch(x + 8, y, patch);
    }
    val = val % 10;
    patch = W_CacheLumpNum(W_GetNumForName("inred0") + val, PU_CACHE);
    V_DrawPatch(x + 16, y, patch);
}

//==========================================================================
//
// DrBNumber
//
// Draws a three digit number using FontB
//
//==========================================================================

static void DrBNumber(signed int val, int x, int y)
{
    patch_t *patch;
    int xpos;
    int oldval;

    oldval = val;
    xpos = x;
    if (val < 0)
    {
        val = 0;
    }
    if (val > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 100, PU_CACHE);
        V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 100;
    xpos += 12;
    if (val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 10, PU_CACHE);
        V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(FontBNumBase + val, PU_CACHE);
    V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);
}

//==========================================================================
//
// DrSmallNumber
//
// Draws a small two digit number.
//
//==========================================================================

static void DrSmallNumber(int val, int x, int y)
{
    patch_t *patch;

    if (val <= 0)
    {
        return;
    }
    if (val > 999)
    {
        val %= 1000;
    }
    if (val > 99)
    {
        patch = PatchSmNumbers[val / 100];
        V_DrawPatch(x, y, patch);
        patch = PatchSmNumbers[(val % 100) / 10];
        V_DrawPatch(x + 4, y, patch);
    }
    else if (val > 9)
    {
        patch = PatchSmNumbers[val / 10];
        V_DrawPatch(x + 4, y, patch);
    }
    val %= 10;
    patch = PatchSmNumbers[val];
    V_DrawPatch(x + 8, y, patch);
}

/*
//==========================================================================
//
// ShadeLine
//
//==========================================================================

static void ShadeLine(int x, int y, int height, int shade)
{
	byte *dest;
	byte *shades;

	shades = colormaps+9*256+shade*2*256;
	dest = I_VideoBuffer+y*SCREENWIDTH+x;
	while(height--)
	{
		*(dest) = *(shades+*dest);
		dest += SCREENWIDTH;
	}
}

//==========================================================================
//
// ShadeChain
//
//==========================================================================

static void ShadeChain(void)
{
	int i;

	for(i = 0; i < 16; i++)
	{
		ShadeLine(277+i, 190, 10, i/2);
		ShadeLine(19+i, 190, 10, 7-(i/2));
	}
}
*/

//==========================================================================
//
// DrawSoundInfo
//
// Displays sound debugging information.
//
//==========================================================================

static void DrawSoundInfo(void)
{
    int i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char text[32];
    int x;
    int y;
    int xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

    if (leveltime & 16)
    {
        MN_DrTextA("*** SOUND DEBUG INFO ***", xPos[0], 20, NULL);
    }
    S_GetChannelInfo(&s);
    if (s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA("NAME", xPos[x++], 30, NULL);
    MN_DrTextA("MO.T", xPos[x++], 30, NULL);
    MN_DrTextA("MO.X", xPos[x++], 30, NULL);
    MN_DrTextA("MO.Y", xPos[x++], 30, NULL);
    MN_DrTextA("ID", xPos[x++], 30, NULL);
    MN_DrTextA("PRI", xPos[x++], 30, NULL);
    MN_DrTextA("DIST", xPos[x++], 30, NULL);
    for (i = 0; i < s.channelCount; i++)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if (c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA("------", xPos[0], y, NULL);
            continue;
        }
        M_snprintf(text, sizeof(text), "%s", c->name);
        M_ForceUppercase(text);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", (int) c->id);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->priority);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->distance);
        MN_DrTextA(text, xPos[x++], y, NULL);
    }
}

//==========================================================================
//
// SB_Drawer
//
//==========================================================================

char patcharti[][10] = {
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIHRAD"},               // healing radius
    {"ARTISUMN"},               // summon maulator
    {"ARTITRCH"},               // torch
    {"ARTIPORK"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIBLST"},               // blast radius
    {"ARTIPSBG"},               // poison bag
    {"ARTITELO"},               // teleport other
    {"ARTISPED"},               // speed
    {"ARTIBMAN"},               // boost mana
    {"ARTIBRAC"},               // boost armor
    {"ARTIATLP"},               // teleport
    {"ARTISKLL"},               // arti_puzzskull
    {"ARTIBGEM"},               // arti_puzzgembig
    {"ARTIGEMR"},               // arti_puzzgemred
    {"ARTIGEMG"},               // arti_puzzgemgreen1
    {"ARTIGMG2"},               // arti_puzzgemgreen2
    {"ARTIGEMB"},               // arti_puzzgemblue1
    {"ARTIGMB2"},               // arti_puzzgemblue2
    {"ARTIBOK1"},               // arti_puzzbook1
    {"ARTIBOK2"},               // arti_puzzbook2
    {"ARTISKL2"},               // arti_puzzskull2
    {"ARTIFWEP"},               // arti_puzzfweapon
    {"ARTICWEP"},               // arti_puzzcweapon
    {"ARTIMWEP"},               // arti_puzzmweapon
    {"ARTIGEAR"},               // arti_puzzgear1
    {"ARTIGER2"},               // arti_puzzgear2
    {"ARTIGER3"},               // arti_puzzgear3
    {"ARTIGER4"},               // arti_puzzgear4
};

int SB_state = -1;
static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldmana1 = -1;
static int oldmana2 = -1;
static int oldarmor = -1;
static int oldhealth = -1;
static int oldlife = -1;
static int oldpieces = -1;
static int oldweapon = -1;
static int oldkeys = -1;


// [crispy] Needed to support widescreen status bar.
void SB_ForceRedraw(void)
{
    SB_state = -1;
}

// [crispy] Create background texture which appears at each side of the status
// bar in widescreen rendering modes. The chosen textures match those which
// surround the non-fullscreen game window.
static void RefreshBackground(void)
{
    V_UseBuffer(st_backing_screen);

    if ((SCREENWIDTH / vid_resolution) != ORIGWIDTH)
    {
        byte *src;
        pixel_t *dest;

        src = W_CacheLumpName("F_022", PU_CACHE);
        dest = st_backing_screen;

        V_FillFlat(SCREENHEIGHT - SBARHEIGHT, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);

        // [crispy] preserve bezel bottom edge
        if (scaledviewwidth == SCREENWIDTH)
        {
            int x;
            patch_t *const patch = W_CacheLumpName("bordb", PU_CACHE);

            for (x = 0; x < WIDESCREENDELTA; x += 16)
            {
                V_DrawPatch(x - WIDESCREENDELTA, 0, patch);
                V_DrawPatch(ORIGWIDTH + WIDESCREENDELTA - x - 16, 0, patch);
            }
        }
    }

    V_RestoreBuffer();
    V_CopyRect(0, 0, st_backing_screen, SCREENWIDTH,
                   SBARHEIGHT, 0, (ORIGHEIGHT - ORIGSBARHEIGHT) * vid_resolution);
}

extern int right_widget_w; // [crispy]

void SB_Drawer(void)
{
    // Sound info debug stuff
    if (DebugSound == true)
    {
        DrawSoundInfo();
    }
    CPlayer = &players[displayplayer];
    if (viewheight == SCREENHEIGHT
        && !(automapactive && !automap_overlay))
    {
        DrawFullScreenStuff();
        SB_state = -1;
    }
    else
    {
        if (SB_state == -1)
        {
            RefreshBackground(); // [crispy] for widescreen

            // [crispy] support wide status bars with 0 offset
            if (SHORT(PatchH2BAR->width) > ORIGWIDTH &&
                    SHORT(PatchH2BAR->leftoffset) == 0)
            {
                V_DrawPatch((ORIGWIDTH - SHORT(PatchH2BAR->width)) / 2, 134,
                        PatchH2BAR);
            }
            else
            {
                V_DrawPatch(0, 134, PatchH2BAR);
            }

            oldhealth = -1;
        }
        DrawCommonBar();
        if (!inventory)
        {
            if (SB_state != 0)
            {
                // Main interface
                if (!(automapactive && !automap_overlay))
                {
                    V_DrawPatch(38, 162, PatchSTATBAR);
                }
                else
                {
                    V_DrawPatch(38, 162, PatchKEYBAR);
                }
                oldarti = 0;
                oldmana1 = -1;
                oldmana2 = -1;
                oldarmor = -1;
                oldpieces = -1;
                oldfrags = -9999;       //can't use -1, 'cuz of negative frags
                oldlife = -1;
                oldweapon = -1;
                oldkeys = -1;
            }
            if (!(automapactive && !automap_overlay))
            {
                DrawMainBar();
            }
            else
            {
                DrawKeyBar();
            }
            SB_state = 0;
        }
        else
        {
            DrawInventoryBar();
            SB_state = 1;
        }
    }
    DrawAnimatedIcons();
}

//==========================================================================
//
// DrawAnimatedIcons
//
//==========================================================================

static void DrawAnimatedIcons(void)
{
    int frame;
    static boolean hitCenterFrame;

    // Wings of wrath
    if (CPlayer->powers[pw_flight])
    {
        int spinfly_x = 20 - WIDESCREENDELTA; // [crispy]

        // [JN] Shift wings icon right if widgets
        // are placed on top and if KIS stats are on.
        if (widget_location == 1 && widget_kis == 1)
        {
            spinfly_x += 70;
        }

        if (CPlayer->powers[pw_flight] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_flight] & 16))
        {
            frame = (leveltime / 3) & 15;
            if (CPlayer->mo->flags2 & MF2_FLY)
            {
                if (hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(spinfly_x, 19,
                                W_CacheLumpNum(SpinFlylump + 15,
                                                PU_CACHE));
                }
                else
                {
                    V_DrawPatch(spinfly_x, 19,
                                W_CacheLumpNum(SpinFlylump + frame,
                                                PU_CACHE));
                    hitCenterFrame = false;
                }
            }
            else
            {
                if (!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(spinfly_x, 19,
                                W_CacheLumpNum(SpinFlylump + frame,
                                                PU_CACHE));
                    hitCenterFrame = false;
                }
                else
                {
                    V_DrawPatch(spinfly_x, 19,
                                W_CacheLumpNum(SpinFlylump + 15,
                                                PU_CACHE));
                    hitCenterFrame = true;
                }
            }
        }
    }

    // Speed Boots
    if (CPlayer->powers[pw_speed])
    {
        int spinspeed_x = 60 - WIDESCREENDELTA; // [crispy]

        // [JN] Shift boots icon right if widgets
        // are placed on top and if KIS stats are on.
        if (widget_location == 1 && widget_kis == 1)
        {
            spinspeed_x += 70;
        }

        if (CPlayer->powers[pw_speed] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_speed] & 16))
        {
            frame = (leveltime / 3) & 15;
            V_DrawPatch(spinspeed_x, 19,
                        W_CacheLumpNum(SpinSpeedLump + frame,
                                        PU_CACHE));
        }
    }

    // Defensive power
    if (CPlayer->powers[pw_invulnerability])
    {
        int spindefense_x = 260 + WIDESCREENDELTA; // [crispy]
        spindefense_x -= right_widget_w; // [crispy]

        // [JN] Shift chess icon left if fps counter,
        // local time or demo timer is active.
        if (vid_showfps
        ||  msg_local_time
        || (demoplayback && (demo_timer == 1 || demo_timer == 3))
        || (demorecording && (demo_timer == 2 || demo_timer == 3)))
        {
            spindefense_x -= 70;
        }

        if (CPlayer->powers[pw_invulnerability] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_invulnerability] & 16))
        {
            frame = (leveltime / 3) & 15;
            V_DrawPatch(spindefense_x, 19,
                        W_CacheLumpNum(SpinDefenseLump + frame,
                                        PU_CACHE));
        }
    }

    // Minotaur Active
    if (CPlayer->powers[pw_minotaur])
    {
        int spinminotaur_x = 300 + WIDESCREENDELTA; // [crispy]
        spinminotaur_x -= right_widget_w; // [crispy]

        // [JN] Shift minotaur icon left if fps counter,
        // local time or demo timer is active.
        if (vid_showfps
        ||  msg_local_time
        || (demoplayback && (demo_timer == 1 || demo_timer == 3))
        || (demorecording && (demo_timer == 2 || demo_timer == 3)))
        {
            spinminotaur_x -= 70;
        }

        if (CPlayer->powers[pw_minotaur] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_minotaur] & 16))
        {
            frame = (leveltime / 3) & 15;
            V_DrawPatch(spinminotaur_x, 19,
                        W_CacheLumpNum(SpinMinotaurLump + frame,
                                        PU_CACHE));
        }
    }
}

// -----------------------------------------------------------------------------
// SB_NumberColor
// [crispy] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

enum
{
    hudcolor_health,
    hudcolor_armor,
    hudcolor_mana_blue,
    hudcolor_mana_green,
    hudcolor_frags,
} hudcolor_t;

static byte *SB_NumberColor (int i)
{
    const int armor = AutoArmorSave[CPlayer->class]
                    + CPlayer->armorpoints[ARMOR_ARMOR]
                    + CPlayer->armorpoints[ARMOR_SHIELD]
                    + CPlayer->armorpoints[ARMOR_HELMET]
                    + CPlayer->armorpoints[ARMOR_AMULET];

    if (!st_colored_stbar)
    {
        return NULL;
    }

    switch (i)
    {
        case hudcolor_health:
        {
            const int health = CPlayer->health;

            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                return cr[CR_LIGHTGRAY];
            else if (health >= 67)
                return cr[CR_GREEN_HX];
            else if (health >= 34)
                return cr[CR_YELLOW];
            else
                return cr[CR_RED];
            break;
        }

        // [JN] Well... Hexen armor system is a bit mind blowing,
        // so let's just use some hard-coded values here.
        case hudcolor_armor:
        {
            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
            {
                return cr[CR_LIGHTGRAY];
            }
            else
            if ((FixedDiv(armor, 5 * FRACUNIT) >> FRACBITS)
            >=  (CPlayer->class == 0 ? 8 :  // Fighted
                 CPlayer->class == 1 ? 7 :  // Cleric
                                       6))  // Mage
            {
                return cr[CR_GREEN_HX];
            }
            else 
            if ((FixedDiv(armor, 5 * FRACUNIT) >> FRACBITS)
            >   (CPlayer->class == 0 ? 3 :  // Fighted
                 CPlayer->class == 1 ? 2 :  // Cleric
                                       1))  // Mage
            {
                return cr[CR_YELLOW];
            }
            else
            {
                return cr[CR_RED];
            }
            break;
        }
        
        case hudcolor_mana_blue:
        {
            if (CPlayer->mana[0] >= MAX_MANA / 2)
            {
                return cr[CR_GREEN_HX];
            }
            else
            if (CPlayer->mana[0] >= MAX_MANA / 4)
            {
                return cr[CR_YELLOW];
            }
            else
            {
                return cr[CR_RED];
            }
        }
        break;

        case hudcolor_mana_green:
        {
            if (CPlayer->mana[1] >= MAX_MANA / 2)
            {
                return cr[CR_GREEN_HX];
            }
            else
            if (CPlayer->mana[1] >= MAX_MANA / 4)
            {
                return cr[CR_YELLOW];
            }
            else
            {
                return cr[CR_RED];
            }
        }
        break;

        case hudcolor_frags:
        {
            const int frags = CPlayer->frags[displayplayer];

            if (frags < 0)
                return cr[CR_RED];
            else if (frags == 0)
                return cr[CR_YELLOW];
            else
                return cr[CR_GREEN];
        }
        break;
    }

    return NULL;
}

//==========================================================================
//
// SB_PaletteFlash
//
// Sets the new palette based upon the current values of
// consoleplayer->damagecount and consoleplayer->bonuscount.
//
//==========================================================================

void SB_PaletteFlash(boolean forceChange)
{
    int palette;
#ifndef CRISPY_TRUECOLOR
    byte *pal;
#endif

    if (forceChange)
    {
        SB_palette = -1;
    }
    if (gamestate == GS_LEVEL)
    {
        CPlayer = &players[displayplayer];
        if (CPlayer->poisoncount)
        {
            // [JN] A11Y - Palette flash effects.
            // Note: this is handled individually per palette in I_SetPalette.
            palette = 0;
            palette = (CPlayer->poisoncount + 7) >> 3;
            if (palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if (CPlayer->damagecount)
        {
            palette = (CPlayer->damagecount + 7) >> 3;

            // [JN] A11Y - Palette flash effects.
            // For A11Y, fix missing first pain palette index for smoother effect.
            // [PN] Simplified pain palette logic and reduced redundancy.
            switch (a11y_pal_flash)
            {
                case 1:  // Halved
                    palette = MIN((palette > 4 ? 4 : palette), NUMREDPALS - 1) + STARTREDPALS - 1;
                    break;
                case 2:  // Quartered
                    palette = MIN((palette > 2 ? 2 : palette), NUMREDPALS - 1) + STARTREDPALS - 1;
                    break;
                case 3:  // Off
                    palette = 0;
                    break;
                default: // On
                    palette = MIN(palette, NUMREDPALS - 1) + STARTREDPALS;
                    break;
            }
        }
        else if (CPlayer->bonuscount)
        {
            palette = (CPlayer->bonuscount + 7) >> 3;

            // [JN] A11Y - Palette flash effects.
            // Fix missing first bonus palette index
            // by subtracting -1 from STARTBONUSPALS, not NUMBONUSPALS.
            // [PN] Simplified bonus palette logic and reduced redundancy.
            palette = MIN(palette, NUMBONUSPALS) + STARTBONUSPALS - 1;

            switch (a11y_pal_flash)
            {
                case 1:  // Halved
                    palette = MIN(palette, 10);
                    break;
                case 2:  // Quartered
                    palette = MIN(palette, 9);
                    break;
                case 3:  // Off
                    palette = 0;
                    break;
            }
        }
        else if (CPlayer->mo->flags2 & MF2_ICEDAMAGE)
        {                       // Frozen player
            palette = STARTICEPAL;
        }
        else
        {
            palette = 0;
        }
    }
    else
    {
        palette = 0;
    }
    if (palette != SB_palette)
    {
        SB_palette = palette;
#ifndef CRISPY_TRUECOLOR
        pal = (byte *) W_CacheLumpNum(PlayPalette, PU_CACHE) + palette * 768;
        I_SetPalette(pal);
#else
        I_SetPalette(palette);
#endif
    }
}

// -----------------------------------------------------------------------------
// SB_SmoothPaletteFlash
// [JN/PN] Smooth palette handling.
// Handles smooth palette transitions for a better visual effect.
// -----------------------------------------------------------------------------

#ifdef CRISPY_TRUECOLOR
void SB_SmoothPaletteFlash (boolean forceChange)
{
    int palette = 0;
    // [JN] A11Y - Palette flash effects.
    // [PN] Maximum alpha values for poison, damage, and bonus effects (in order).
    // Each row represents an effect type, with 4 intensity levels:
    // [Full intensity, Half intensity, Quarter intensity, Minimal visibility/Off].
    static const int max_alpha[3][4] = {
        { 204, 102, 51, 16 }, // Poison (green)
        { 226, 113, 56,  0 }, // Damage (red)
        { 127,  64, 32,  0 }  // Bonus (yellow)
    };

    if (forceChange)
    {
        SB_palette = -1;
    }
    if (gamestate == GS_LEVEL)
    {
        CPlayer = &players[displayplayer];
        if (CPlayer->poisoncount)
        {
            palette = 14;
            grn_pane_alpha = MIN(CPlayer->poisoncount * POISONADD, max_alpha[0][a11y_pal_flash]);
        }
        else if (CPlayer->damagecount)
        {
            palette = 1;
            red_pane_alpha = MIN(CPlayer->damagecount * PAINADD, max_alpha[1][a11y_pal_flash]);
        }
        else if (CPlayer->bonuscount)
        {
            palette = 9;
            yel_pane_alpha = MIN(CPlayer->bonuscount * BONUSADD, max_alpha[2][a11y_pal_flash]);
        }
        else if (CPlayer->graycount)
        {
            // [PN/JN] A11Y - Palette flash effects.
            // Adjust holy pal alpha based on accessibility setting.
            const int max_gray = (a11y_pal_flash == 3) ? 0 : 
                                  CPlayer->graycount >> ((a11y_pal_flash == 1) ? 1 :
                                                         (a11y_pal_flash == 2) ? 2 : 0);

            palette = 22;  // STARTHOLYPAL
            gray_pane_alpha = max_gray;  // 127 pane alpha max set in A_CHolyAttack
        }
        else if (CPlayer->orngcount)
        {
            // [PN/JN] A11Y - Palette flash effects.
            // Adjust holy pal alpha based on accessibility setting.
            const int max_orng = (a11y_pal_flash == 3) ? 0 : 
                                  CPlayer->orngcount >> ((a11y_pal_flash == 1) ? 1 :
                                                         (a11y_pal_flash == 2) ? 2 : 0);

            palette = 25;  // STARTSCOURGEPAL
            orng_pane_alpha = max_orng;  // 127 pane alpha max set in A_MStaffAttack
        }
        else if (CPlayer->mo->flags2 & MF2_ICEDAMAGE)
        {                       // Frozen player
            palette = STARTICEPAL;
        }
    }

    if (palette != SB_palette || CPlayer->poisoncount || CPlayer->damagecount || CPlayer->bonuscount
    ||  CPlayer->graycount || CPlayer->orngcount)
    {
        SB_palette = palette;
        I_SetPalette(palette);
    }
}
#endif

//==========================================================================
//
// DrawCommonBar
//
//==========================================================================

void DrawCommonBar(void)
{
    int healthPos;

    V_DrawPatch(0, 134, PatchH2TOP);

    if (oldhealth != HealthMarker)
    {
        oldhealth = HealthMarker;
        healthPos = HealthMarker;
        if (healthPos < 0)
        {
            healthPos = 0;
        }
        if (healthPos > 100)
        {
            healthPos = 100;
        }
        V_DrawPatch(28 + (((healthPos * 196) / 100) % 9), 193, PatchCHAIN);
        V_DrawPatch(7 + ((healthPos * 11) / 5), 193, PatchLIFEGEM);
        V_DrawPatch(0, 193, PatchLFEDGE);
        V_DrawPatch(277, 193, PatchRTEDGE);
//              ShadeChain();
    }
}

//==========================================================================
//
// DrawMainBar
//
//==========================================================================

void DrawMainBar(void)
{
    int i, j, k;
    int temp;
    patch_t *manaPatch1, *manaPatch2;
    patch_t *manaVialPatch1, *manaVialPatch2;

    manaPatch1 = NULL;
    manaPatch2 = NULL;
    manaVialPatch1 = NULL;
    manaVialPatch2 = NULL;

    // Ready artifact
    if (ArtifactFlash)
    {
        V_DrawPatch(144, 160, PatchARTICLEAR);
        V_DrawPatch(148, 164, W_CacheLumpNum(W_GetNumForName("useartia")
                                             + ArtifactFlash - 1, PU_CACHE));
        ArtifactFlash--;
        oldarti = -1;           // so that the correct artifact fills in after the flash
    }
    else if (oldarti != CPlayer->readyArtifact
             || oldartiCount != CPlayer->inventory[inv_ptr].count)
    {
        V_DrawPatch(144, 160, PatchARTICLEAR);
        if (CPlayer->readyArtifact > 0)
        {
            V_DrawPatch(143, 163,
                        W_CacheLumpName(patcharti[CPlayer->readyArtifact],
                                        PU_CACHE));
            if (CPlayer->inventory[inv_ptr].count > 1)
            {
                DrSmallNumber(CPlayer->inventory[inv_ptr].count, 162, 184);
            }
        }
        oldarti = CPlayer->readyArtifact;
        oldartiCount = CPlayer->inventory[inv_ptr].count;
    }

    // Frags
    if (deathmatch)
    {
        temp = 0;
        for (i = 0; i < maxplayers; i++)
        {
            temp += CPlayer->frags[i];
        }
        if (temp != oldfrags)
        {
            V_DrawPatch(38, 162, PatchKILLS);
            DrINumber(temp, 40, 176);
            oldfrags = temp;
        }
    }
    else
    {
        temp = HealthMarker;
        if (temp < 0)
        {
            temp = 0;
        }
        else if (temp > 100)
        {
            temp = 100;
        }
        // [JN] Need to perform update for colored status bar.
        // TODO - not very optimal, ideally to update once after invul. runs out.
        if (oldlife != temp || st_colored_stbar)
        {
            oldlife = temp;
            V_DrawPatch(41, 178, PatchARMCLEAR);
            if (temp >= 25)
            {
                dp_translation = SB_NumberColor(hudcolor_health);
                DrINumber(temp, 40, 176);
                dp_translation = NULL;
            }
            else
            {
                DrRedINumber(temp, 40, 176);
            }
        }
    }
    // Mana
    temp = CPlayer->mana[0];
    if (oldmana1 != temp)
    {
        V_DrawPatch(77, 178, PatchMANACLEAR);
        dp_translation = SB_NumberColor(hudcolor_mana_blue);
        DrSmallNumber(temp, 79, 181);
        dp_translation = NULL;
        manaVialPatch1 = (patch_t *) 1; // force a vial update
        if (temp == 0)
        {                       // Draw Dim Mana icon
            manaPatch1 = PatchMANADIM1;
        }
        else if (oldmana1 == 0)
        {
            manaPatch1 = PatchMANABRIGHT1;
        }
        oldmana1 = temp;
    }
    temp = CPlayer->mana[1];
    if (oldmana2 != temp)
    {
        V_DrawPatch(109, 178, PatchMANACLEAR);
        dp_translation = SB_NumberColor(hudcolor_mana_green);
        DrSmallNumber(temp, 111, 181);
        dp_translation = NULL;
        manaVialPatch1 = (patch_t *) 1; // force a vial update
        if (temp == 0)
        {                       // Draw Dim Mana icon
            manaPatch2 = PatchMANADIM2;
        }
        else if (oldmana2 == 0)
        {
            manaPatch2 = PatchMANABRIGHT2;
        }
        oldmana2 = temp;
    }
    if (oldweapon != CPlayer->readyweapon || manaPatch1 || manaPatch2
        || manaVialPatch1)
    {                           // Update mana graphics based upon mana count/weapon type
        if (CPlayer->readyweapon == WP_FIRST)
        {
            manaPatch1 = PatchMANADIM1;
            manaPatch2 = PatchMANADIM2;
            manaVialPatch1 = PatchMANAVIALDIM1;
            manaVialPatch2 = PatchMANAVIALDIM2;
        }
        else if (CPlayer->readyweapon == WP_SECOND)
        {
            if (!manaPatch1)
            {
                manaPatch1 = PatchMANABRIGHT1;
            }
            manaVialPatch1 = PatchMANAVIAL1;
            manaPatch2 = PatchMANADIM2;
            manaVialPatch2 = PatchMANAVIALDIM2;
        }
        else if (CPlayer->readyweapon == WP_THIRD)
        {
            manaPatch1 = PatchMANADIM1;
            manaVialPatch1 = PatchMANAVIALDIM1;
            if (!manaPatch2)
            {
                manaPatch2 = PatchMANABRIGHT2;
            }
            manaVialPatch2 = PatchMANAVIAL2;
        }
        else
        {
            manaVialPatch1 = PatchMANAVIAL1;
            manaVialPatch2 = PatchMANAVIAL2;
            if (!manaPatch1)
            {
                manaPatch1 = PatchMANABRIGHT1;
            }
            if (!manaPatch2)
            {
                manaPatch2 = PatchMANABRIGHT2;
            }
        }
        V_DrawPatch(77, 164, manaPatch1);
        V_DrawPatch(110, 164, manaPatch2);
        V_DrawPatch(94, 164, manaVialPatch1);
        for (i = 165; i < 187 - (22 * CPlayer->mana[0]) / MAX_MANA; i++)
        {
         for (j = 0; j < vid_resolution; j++)
          for (k = 0; k < vid_resolution; k++)
          {
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((95 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((96 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((97 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
          }
        }
        V_DrawPatch(102, 164, manaVialPatch2);
        for (i = 165; i < 187 - (22 * CPlayer->mana[1]) / MAX_MANA; i++)
        {
         for (j = 0; j < vid_resolution; j++)
          for (k = 0; k < vid_resolution; k++)
          {
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((103 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((104 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
            I_VideoBuffer[SCREENWIDTH * ((i * vid_resolution) + j)
                          + ((105 + WIDESCREENDELTA) * vid_resolution) + k] = 0;
          }
        }
        oldweapon = CPlayer->readyweapon;
    }
    // Armor
    temp = AutoArmorSave[CPlayer->class]
        + CPlayer->armorpoints[ARMOR_ARMOR] +
        CPlayer->armorpoints[ARMOR_SHIELD] +
        CPlayer->armorpoints[ARMOR_HELMET] +
        CPlayer->armorpoints[ARMOR_AMULET];
    // [JN] Need to perform update for colored status bar.
    // TODO - not very optimal, ideally to update once after invul. runs out.
    if (oldarmor != temp || st_colored_stbar)
    {
        oldarmor = temp;
        V_DrawPatch(255, 178, PatchARMCLEAR);
        dp_translation = SB_NumberColor(hudcolor_armor);
        DrINumber(FixedDiv(temp, 5 * FRACUNIT) >> FRACBITS, 250, 176);
        dp_translation = NULL;
    }
    // Weapon Pieces
    if (oldpieces != CPlayer->pieces)
    {
        DrawWeaponPieces();
        oldpieces = CPlayer->pieces;
    }
}

//==========================================================================
//
// DrawInventoryBar
//
//==========================================================================

void DrawInventoryBar(void)
{
    int i;
    int x;

    x = inv_ptr - curpos;
    V_DrawPatch(38, 162, PatchINVBAR);
    for (i = 0; i < 7; i++)
    {
        //V_DrawPatch(50+i*31, 160, W_CacheLumpName("ARTIBOX", PU_CACHE));
        if (CPlayer->inventorySlotNum > x + i
            && CPlayer->inventory[x + i].type != arti_none)
        {
            V_DrawPatch(50 + i * 31, 163,
                        W_CacheLumpName(patcharti
                                        [CPlayer->inventory[x + i].type],
                                        PU_CACHE));
            if (CPlayer->inventory[x + i].count > 1)
            {
                DrSmallNumber(CPlayer->inventory[x + i].count, 68 + i * 31,
                              185);
            }
        }
    }
    V_DrawPatch(50 + curpos * 31, 163, PatchSELECTBOX);
    if (x != 0)
    {
        V_DrawPatch(42, 163, !(leveltime & 4) ? PatchINVLFGEM1 :
                    PatchINVLFGEM2);
    }
    if (CPlayer->inventorySlotNum - x > 7)
    {
        V_DrawPatch(269, 163, !(leveltime & 4) ? PatchINVRTGEM1 :
                    PatchINVRTGEM2);
    }
}

//==========================================================================
//
// DrawKeyBar
//
//==========================================================================

void DrawKeyBar(void)
{
    int i;
    int xPosition;
    int temp;

    if (oldkeys != CPlayer->keys)
    {
        xPosition = 46;
        for (i = 0; i < NUM_KEY_TYPES && xPosition <= 126; i++)
        {
            if (CPlayer->keys & (1 << i))
            {
                V_DrawPatch(xPosition, 164,
                            W_CacheLumpNum(W_GetNumForName("keyslot1") + i,
                                           PU_CACHE));
                xPosition += 20;
            }
        }
        oldkeys = CPlayer->keys;
    }
    temp = AutoArmorSave[CPlayer->class]
        + CPlayer->armorpoints[ARMOR_ARMOR] +
        CPlayer->armorpoints[ARMOR_SHIELD] +
        CPlayer->armorpoints[ARMOR_HELMET] +
        CPlayer->armorpoints[ARMOR_AMULET];
    if (oldarmor != temp)
    {
        for (i = 0; i < NUMARMOR; i++)
        {
            if (!CPlayer->armorpoints[i])
            {
                continue;
            }
            if (CPlayer->armorpoints[i] <=
                (ArmorIncrement[CPlayer->class][i] >> 2))
            {
                V_DrawTLPatch(150 + 31 * i, 164,
                              W_CacheLumpNum(W_GetNumForName("armslot1") +
                                             i, PU_CACHE));
            }
            else if (CPlayer->armorpoints[i] <=
                     (ArmorIncrement[CPlayer->class][i] >> 1))
            {
                V_DrawAltTLPatch(150 + 31 * i, 164,
                                 W_CacheLumpNum(W_GetNumForName("armslot1")
                                                + i, PU_CACHE));
            }
            else
            {
                V_DrawPatch(150 + 31 * i, 164,
                            W_CacheLumpNum(W_GetNumForName("armslot1") + i,
                                           PU_CACHE));
            }
        }
        oldarmor = temp;
    }
}

//==========================================================================
//
// DrawWeaponPieces
//
//==========================================================================

static int PieceX[NUMCLASSES][3] = {
    {190, 225, 234},
    {190, 212, 225},
    {190, 205, 224},
    {0, 0, 0}                   // Pig is never used
};

static void DrawWeaponPieces(void)
{
    if (CPlayer->pieces == 7)
    {
        V_DrawPatch(190, 162, PatchWEAPONFULL);
        return;
    }
    V_DrawPatch(190, 162, PatchWEAPONSLOT);
    if (CPlayer->pieces & WPIECE1)
    {
        V_DrawPatch(PieceX[PlayerClass[displayplayer]][0], 162, PatchPIECE1);
    }
    if (CPlayer->pieces & WPIECE2)
    {
        V_DrawPatch(PieceX[PlayerClass[displayplayer]][1], 162, PatchPIECE2);
    }
    if (CPlayer->pieces & WPIECE3)
    {
        V_DrawPatch(PieceX[PlayerClass[displayplayer]][2], 162, PatchPIECE3);
    }
}

// [JN] Class-based icon variables.
static const int ClassArmorX[3] = {
    81, // Fighter
    83, // Cleric
    82, // Mage
};

static const int ClassArmorY[3] = {
   172, // Fighter
   170, // Cleric
   168, // Mage
};

static const char *ClassArmorIcon[3] = {
    "ARM1A0", // Mesh armor
    "ARM2A0", // Falcon Shield
    "ARM4A0", // Amulet of Warding
};

// [JN] Generic armor icon.
static const byte id_armor_icon[] =
{
    0x0D, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 
    0x4B, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x00, 
    0x83, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0xAE, 0x00, 0x00, 0x00, 
    0xC4, 0x00, 0x00, 0x00, 0xDA, 0x00, 0x00, 0x00, 0xEF, 0x00, 0x00, 0x00, 
    0x03, 0x01, 0x00, 0x00, 0x16, 0x01, 0x00, 0x00, 0x27, 0x01, 0x00, 0x00, 
    0x02, 0x0A, 0x89, 0x89, 0x8A, 0x88, 0x87, 0x86, 0x85, 0x84, 0x82, 0x81, 
    0x80, 0x80, 0xFF, 0x01, 0x0C, 0x89, 0x89, 0x03, 0x33, 0x04, 0x04, 0x04, 
    0x03, 0x02, 0x02, 0x01, 0x81, 0x7E, 0x7E, 0xFF, 0x00, 0x0E, 0x87, 0x87, 
    0x03, 0x06, 0x08, 0x09, 0x23, 0x08, 0x06, 0x04, 0x03, 0x02, 0x01, 0x80, 
    0x7E, 0x7E, 0xFF, 0x00, 0x0F, 0x89, 0x89, 0x04, 0x08, 0x09, 0x0A, 0x09, 
    0x08, 0x07, 0x06, 0x04, 0x03, 0x02, 0x01, 0x80, 0x7E, 0x7E, 0xFF, 0x00, 
    0x10, 0x8A, 0x8A, 0x22, 0x0A, 0x25, 0x0D, 0x0C, 0x24, 0x23, 0x07, 0x06, 
    0x04, 0x03, 0x02, 0x01, 0x80, 0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x89, 0x89, 
    0x06, 0x09, 0x24, 0x0B, 0x0A, 0x09, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 
    0x01, 0x00, 0x80, 0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x87, 0x87, 0x22, 0x0A, 
    0x0C, 0x25, 0x0B, 0x0A, 0x23, 0x07, 0x06, 0x05, 0x04, 0x02, 0x02, 0x01, 
    0x00, 0x81, 0x81, 0xFF, 0x00, 0x11, 0x86, 0x86, 0x06, 0x09, 0x24, 0x0B, 
    0x24, 0x09, 0x07, 0x06, 0x21, 0x04, 0x03, 0x02, 0x01, 0x00, 0x81, 0x7D, 
    0x7D, 0xFF, 0x00, 0x10, 0x85, 0x85, 0x06, 0x0A, 0x0C, 0x25, 0x0C, 0x24, 
    0x08, 0x07, 0x06, 0x04, 0x03, 0x02, 0x01, 0x81, 0x7D, 0x7D, 0xFF, 0x00, 
    0x0F, 0x84, 0x84, 0x04, 0x08, 0x0A, 0x24, 0x09, 0x08, 0x22, 0x21, 0x04, 
    0x03, 0x02, 0x01, 0x81, 0x7D, 0x7D, 0xFF, 0x00, 0x0E, 0x83, 0x83, 0x03, 
    0x06, 0x08, 0x23, 0x08, 0x08, 0x06, 0x04, 0x03, 0x02, 0x01, 0x80, 0x7D, 
    0x7D, 0xFF, 0x01, 0x0C, 0x83, 0x83, 0x03, 0x04, 0x04, 0x04, 0x04, 0x03, 
    0x02, 0x02, 0x01, 0x80, 0x7D, 0x7D, 0xFF, 0x02, 0x0A, 0x84, 0x84, 0x83, 
    0x81, 0x80, 0x80, 0x7F, 0x7F, 0x7F, 0x7F, 0x7D, 0x7D, 0xFF
};

static const int PiecesX[3] = {
    307, // Quietus
    301, // Wraithverge
    311, // Bloodscourge 
};

static const int PiecesY[3][3] = {
    {133, 157, 169}, // Quietus
    {135, 151, 167}, // Wraithverge
    {125, 143, 165}, // Bloodscourge 
};

static const char *PiecesGfx[][3] = {
    {"WFR1A0", "WFR2A0", "WFR3A0"},
    {"WCH1A0", "WCH2A0", "WCH3A0"},
    {"WMS1A0", "WMS2A0", "WMS3A0"},
};

// -----------------------------------------------------------------------------
// DrawFullScreenStuff
// [JN] Upgraded to draw extra elements.
// -----------------------------------------------------------------------------

static void DrawFullScreenStuff(void)
{
    const int wide_x = dp_screen_size == 12 ? WIDESCREENDELTA : 0;
    const int class = PlayerClass[displayplayer];
    int i;

    // Health.
    dp_translation = SB_NumberColor(hudcolor_health);
    DrBNumber(CPlayer->health, -1 - wide_x, 175);
    dp_translation = NULL;
    // Draw health vial.
    V_DrawShadowedPatchNoOffsets(39 - wide_x, 176, W_CacheLumpName("PTN1A0", PU_CACHE));

    if (!inventory)
    {
        // Armor.
        {
            const int currentArmor = AutoArmorSave[CPlayer->class]
                                   + CPlayer->armorpoints[ARMOR_ARMOR]
                                   + CPlayer->armorpoints[ARMOR_SHIELD]
                                   + CPlayer->armorpoints[ARMOR_HELMET]
                                   + CPlayer->armorpoints[ARMOR_AMULET];

            dp_translation = SB_NumberColor(hudcolor_armor);
            DrBNumber(FixedDiv(currentArmor, 5 * FRACUNIT) >> FRACBITS, 41 - wide_x, 175);
            dp_translation = NULL;
            if (st_armor_icon)
            {
                // Draw generic armor icon.
                V_DrawShadowedPatch(81 - wide_x, 176, (patch_t*)id_armor_icon);
            }
            else
            {
                // Draw class-based icon.
                V_DrawShadowedPatchNoOffsets(ClassArmorX[class] - wide_x, ClassArmorY[class],
                                    W_CacheLumpName(ClassArmorIcon[class], PU_CACHE));
            }
        }

        // Frags.
        if (deathmatch)
        {
            int temp = 0;

            for (i = 0 ; i < maxplayers ; i++)
            {
                if (playeringame[i])
                {
                    temp += CPlayer->frags[i];
                }
            }

            dp_translation = SB_NumberColor(hudcolor_frags);
            DrINumber(temp, 111 - wide_x, 178);
            dp_translation = NULL;
        }

        // Ready artifact.
        if (CPlayer->readyArtifact > 0)
        {
            V_DrawTLPatch(232 + wide_x, 170, W_CacheLumpName("ARTIBOX", PU_CACHE));
            V_DrawPatch(230 + wide_x, 169, W_CacheLumpName(patcharti[CPlayer->readyArtifact], PU_CACHE));
            if (CPlayer->inventory[inv_ptr].count > 1)
            {
                DrSmallNumber(CPlayer->inventory[inv_ptr].count, 248 + wide_x, 192);
            }
        }
    }
    else
    {
        int x = inv_ptr - curpos;

        for (i = 0 ; i < 7 ; i++)
        {
            V_DrawTLPatch(50 + i * 31, 168, W_CacheLumpName("ARTIBOX", PU_CACHE));
            if (CPlayer->inventorySlotNum > x + i && CPlayer->inventory[x + i].type != arti_none)
            {
                V_DrawPatch(49 + i * 31, 167,
                            W_CacheLumpName(patcharti
                                            [CPlayer->inventory[x + i].type],
                                            PU_CACHE));
                if (CPlayer->inventory[x + i].count > 1)
                {
                    DrSmallNumber(CPlayer->inventory[x + i].count,
                                  66 + i * 31, 188);
                }
            }
        }

        V_DrawPatch(50 + curpos * 31, 167, PatchSELECTBOX);

        if (x != 0)
        {
            V_DrawPatch(40, 167, !(leveltime & 4) ? PatchINVLFGEM1 :
                        PatchINVLFGEM2);
        }
        if (CPlayer->inventorySlotNum - x > 7)
        {
            V_DrawPatch(268, 167, !(leveltime & 4) ?
                        PatchINVRTGEM1 : PatchINVRTGEM2);
        }
    }

    // [JN] Draw amount of current mana.
    if (CPlayer->readyweapon == WP_FIRST)
    {
        V_DrawShadowedPatch(302 + wide_x, 170, PatchMANADIM1);
        V_DrawShadowedPatch(302 + wide_x, 184, PatchMANADIM2);
    }
    else if (CPlayer->readyweapon == WP_SECOND)
    {
        V_DrawShadowedPatch(302 + wide_x, 170, PatchMANABRIGHT1);
        V_DrawShadowedPatch(302 + wide_x, 184, PatchMANADIM2);
    }
    else if (CPlayer->readyweapon == WP_THIRD)
    {
        V_DrawShadowedPatch(302 + wide_x, 170, PatchMANADIM1);
        V_DrawShadowedPatch(302 + wide_x, 184, PatchMANABRIGHT2);
    }
    else
    {
        V_DrawShadowedPatch(302 + wide_x, 170, PatchMANABRIGHT1);
        V_DrawShadowedPatch(302 + wide_x, 184, PatchMANABRIGHT2);
    }

    // [JN] Draw mana points, colorize if necessary. Do not draw negative values.
    dp_translation = SB_NumberColor(hudcolor_mana_blue);
    DrINumber(CPlayer->mana[0] >= 0 ? CPlayer->mana[0] : 0, 274 + wide_x, 170);
    dp_translation = NULL;

    dp_translation = SB_NumberColor(hudcolor_mana_green);
    DrINumber(CPlayer->mana[1] >= 0 ? CPlayer->mana[1] : 0, 274 + wide_x, 184); 
    dp_translation = NULL;

    // [JN] Assembled weapon widget.
    if (st_weapon_widget)
    {
        patch_t *patch1 = W_CacheLumpNum(W_CheckNumForName(PiecesGfx[class][0]), PU_CACHE);
        patch_t *patch2 = W_CacheLumpNum(W_CheckNumForName(PiecesGfx[class][1]), PU_CACHE);
        patch_t *patch3 = W_CacheLumpNum(W_CheckNumForName(PiecesGfx[class][2]), PU_CACHE);

        dp_translucent = (st_weapon_widget == 2);

        if (CPlayer->pieces & WPIECE1)
        {
            // God-awful hack to shift BloodScrouge's upper piece
            // one pixel left for better placement.
            const int xx = (class == PCLASS_MAGE ? 1 : 0);

            V_DrawPatch(PiecesX[class] + WIDESCREENDELTA - xx, PiecesY[class][0], patch1);
        }
        if (CPlayer->pieces & WPIECE2)
        {
            V_DrawPatch(PiecesX[class] + WIDESCREENDELTA, PiecesY[class][1], patch2);
        }
        if (CPlayer->pieces & WPIECE3)
        {
            V_DrawPatch(PiecesX[class] + WIDESCREENDELTA, PiecesY[class][2], patch3);
        }

        dp_translucent = false;
    }
}


//==========================================================================
//
// Draw_TeleportIcon
//
//==========================================================================
void Draw_TeleportIcon(void)
{
    patch_t *patch;
    patch = W_CacheLumpNum(W_GetNumForName("teleicon"), PU_CACHE);
    V_DrawPatch(100, 68, patch);
    I_FinishUpdate();
}

//==========================================================================
//
// Draw_SaveIcon
//
//==========================================================================
void Draw_SaveIcon(void)
{
    patch_t *patch;
    patch = W_CacheLumpNum(W_GetNumForName("saveicon"), PU_CACHE);
    V_DrawPatch(100, 68, patch);
    I_FinishUpdate();
}

//==========================================================================
//
// Draw_LoadIcon
//
//==========================================================================
void Draw_LoadIcon(void)
{
    patch_t *patch;
    patch = W_CacheLumpNum(W_GetNumForName("loadicon"), PU_CACHE);
    V_DrawPatch(100, 68, patch);
    I_FinishUpdate();
}



//==========================================================================
//
// SB_Responder
//
//==========================================================================

boolean SB_Responder(event_t * event)
{
    if (event->type == ev_keydown)
    {
        if (HandleCheats(event->data1))
        {                       // Need to eat the key
            return (true);
        }
    }
    return (false);
}

//==========================================================================
//
// HandleCheats
//
// Returns true if the caller should eat the key.
//
//==========================================================================

static boolean HandleCheats(byte key)
{
    int i;
    boolean eat;

    /* [crispy] check for nightmare/netgame per cheat, to allow "harmless" cheats
    ** [JN] Allow in nightmare and for dead player (can be resurrected).
    if (gameskill == sk_nightmare)
    {                           // Can't cheat in nightmare mode
        return (false);
    }
    else if (netgame)
    {                           // change CD track is the only cheat available in deathmatch
        eat = false;
        return eat;
    }
    if (players[consoleplayer].health <= 0)
    {                           // Dead players can't cheat
        return (false);
    }
    */
    eat = false;
    for (i = 0; i<arrlen(Cheats); ++i)
    {
        if (CheatAddKey(&Cheats[i], key, &eat))
        {
            Cheats[i].func(&players[consoleplayer], &Cheats[i]);
            // [JN] Do not play sound after typing just "ID".
            if (Cheats[i].func != CheatWaitFunc)
            {
                S_StartSound(NULL, SFX_PLATFORM_STOP);
            }
        }
    }
    return (eat);
}

//==========================================================================
//
// CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//==========================================================================

static boolean CheatAddKey(Cheat_t * cheat, byte key, boolean * eat)
{
/*
    if (!cheat->pos)
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
    }
    if (*cheat->pos == 0)
    {
        *eat = true;
        cheat->args[cheat->currentArg++] = key;
        cheat->pos++;
    }
    else if (CheatLookup[key] == *cheat->pos)
    {
        cheat->pos++;
    }
    else
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
    }
    if (*cheat->pos == 0xff)
    {
        cheat->pos = cheat->sequence;
        cheat->currentArg = 0;
        return (true);
    }
    return (false);
    */

    *eat = cht_CheckCheat(cheat->seq, key);

    return *eat;
}

//==========================================================================
//
// CHEAT FUNCTIONS
//
//==========================================================================

#define FULL_CHEAT_CHECK if(netgame || demorecording || demoplayback){return;}
#define SAFE_CHEAT_CHECK if(netgame || demorecording){return;}

static void CheatWaitFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    // [JN] If user types "id", activate timer to prevent
    // other than typing actions in G_Responder.
    player->cheatTics = TICRATE * 2;
}

static void CheatGodFunc (player_t *player, Cheat_t *cheat)
{
    // [crispy] dead players are first respawned at the current position
    mapthing_t mt = {0};

    FULL_CHEAT_CHECK;

    if (player->playerstate == PST_DEAD)
    {
        angle_t an;

        mt.x = player->mo->x >> FRACBITS;
        mt.y = player->mo->y >> FRACBITS;
        mt.angle = (player->mo->angle + ANG45/2)*(uint64_t)45/ANG45;
        mt.type = consoleplayer + 1;
        P_SpawnPlayer(&mt);

        // [crispy] spawn a teleport fog
        an = player->mo->angle >> ANGLETOFINESHIFT;
        P_SpawnMobj(player->mo->x + 20 * finecosine[an],
                    player->mo->y + 20 * finesine[an],
                    player->mo->z + TELEFOGHEIGHT, MT_TFOG);
        S_StartSound(NULL, SFX_TELEPORT);

        // [crispy] fix reviving as "zombie" if god mode was already enabled
        if (player->mo)
        {
            player->mo->health = MAXHEALTH;
        }
        player->health = MAXHEALTH;
        player->lookdir = 0;
    }

    player->cheats ^= CF_GODMODE;
    CT_SetMessage(player, player->cheats & CF_GODMODE ?
                  TXT_CHEATGODON : TXT_CHEATGODOFF, false, NULL);
    SB_state = -1;
    player->cheatTics = 1;
}

static void CheatNoClipFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    player->cheats ^= CF_NOCLIP;
    CT_SetMessage(player, player->cheats & CF_NOCLIP ?
                  TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF, false, NULL);
    player->cheatTics = 1;
}

static void CheatWeaponsFunc (player_t *player, Cheat_t *cheat)
{
    int i;

    FULL_CHEAT_CHECK;

    for (i = 0; i < NUMARMOR; i++)
    {
        player->armorpoints[i] = ArmorIncrement[player->class][i];
    }
    for (i = 0; i < NUMWEAPONS; i++)
    {
        player->weaponowned[i] = true;
    }
    for (i = 0; i < NUMMANA; i++)
    {
        player->mana[i] = MAX_MANA;
    }
    player->pieces = 7;
    CT_SetMessage(player, TXT_CHEATWEAPONS, false, NULL);
    player->cheatTics = 1;
}

static void CheatWpnsKeysFunc (player_t *player, Cheat_t *cheat)
{
    int i;

    FULL_CHEAT_CHECK;

    for (i = 0; i < NUMARMOR; i++)
    {
        player->armorpoints[i] = ArmorIncrement[player->class][i];
    }
    for (i = 0; i < NUMWEAPONS; i++)
    {
        player->weaponowned[i] = true;
    }
    for (i = 0; i < NUMMANA; i++)
    {
        player->mana[i] = MAX_MANA;
    }
    player->pieces = 7;
    player->keys = 2047;
    CT_SetMessage(player, TXT_CHEATWPNSKEYS, false, NULL);
    player->cheatTics = 1;
}

static void CheatHealthFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    if (player->morphTics)
    {
        player->health = player->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        player->health = player->mo->health = MAXHEALTH;
    }
    CT_SetMessage(player, TXT_CHEATHEALTH, false, NULL);
    player->cheatTics = 1;
}

static void CheatKeysFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    player->keys = 2047;
    CT_SetMessage(player, TXT_CHEATKEYS, false, NULL);
    player->cheatTics = 1;
}

static void CheatArtifactAllFunc (player_t *player, Cheat_t *cheat)
{
    int i;
    int j;

    FULL_CHEAT_CHECK;

    for (i = arti_none + 1; i < arti_firstpuzzitem; i++)
    {
        for (j = 0; j < 25; j++)
        {
            P_GiveArtifact(player, i, NULL);
        }
    }
    CT_SetMessage(player, TXT_CHEATARTIFACTS3, false, NULL);
    player->cheatTics = 1;
}

static void CheatPuzzleFunc (player_t *player, Cheat_t *cheat)
{
    int i;

    FULL_CHEAT_CHECK;

    for (i = arti_firstpuzzitem; i < NUMARTIFACTS; i++)
    {
        P_GiveArtifact(player, i, NULL);
    }
    CT_SetMessage(player, TXT_CHEATARTIFACTS3, false, NULL);
    player->cheatTics = 1;
}

static void CheatWarpFunc (player_t *player, Cheat_t *cheat)
{
    int tens;
    int ones;
    int map;
    char mapName[9];
    char args[2];

    // [JN] Safe to use IDCLEV/ENGAGE/VISIT while demo playback.
    SAFE_CHEAT_CHECK;

    cht_GetParam(cheat->seq, args);

    tens = args[0] - '0';
    ones = args[1] - '0';
    if (tens < 0 || tens > 9 || ones < 0 || ones > 9)
    {                           // Bad map
        CT_SetMessage(player, TXT_CHEATBADINPUT, false, NULL);
        return;
    }
    map = P_TranslateMap((args[0] - '0') * 10 + args[1] - '0');
    if (map == -1)
    {                           // Not found
        CT_SetMessage(player, TXT_CHEATNOMAP, false, NULL);
        return;
    }
    if (map == gamemap)
    {                           // Don't try to teleport to current map
        CT_SetMessage(player, TXT_CHEATBADINPUT, false, NULL);
        return;
    }
    M_snprintf(mapName, sizeof(mapName), "MAP%02d", map);
    if (W_CheckNumForName(mapName) == -1)
    {                       // Can't find
        CT_SetMessage(player, TXT_CHEATNOMAP, false, NULL);
        return;
    }
    CT_SetMessage(player, TXT_CHEATWARP, false, NULL);
    player->cheatTics = 1;
    
    // [JN] Allow warping during demo playback and go to the requested map.
    if (demoplayback)
    {
        demowarp = map;
        nodrawers = true;
        singletics = true;

        if (map <= gamemap)
        {
            G_DoPlayDemo();
        }
    }
    else
    {
        G_TeleportNewMap(map, 0);
    }
}

static void CheatPigFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    if (player->morphTics)
    {
        P_UndoPlayerMorph(player);
    }
    else
    {
        P_MorphPlayer(player);
    }
    CT_SetMessage(player, "SQUEAL!!", false, NULL);
    player->cheatTics = 1;
}

static void CheatMassacreFunc (player_t *player, Cheat_t *cheat)
{
    int count;
    char buffer[80];

    FULL_CHEAT_CHECK;

    count = P_Massacre();
    M_snprintf(buffer, sizeof(buffer), "%d MONSTERS KILLED\n", count);
    CT_SetMessage(player, buffer, false, NULL);
    player->cheatTics = 1;
}

static void CheatClassFunc1 (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    CT_SetMessage(player, "ENTER NEW PLAYER CLASS (0 - 2)", false, NULL);
}

static void CheatClassFunc2 (player_t *player, Cheat_t *cheat)
{
    int i;
    int class;
    char args[2];

    cht_GetParam(cheat->seq, args);

    if (player->morphTics)
    {                           // don't change class if the player is morphed
        return;
    }
    class = args[0] - '0';
    if (class > 2 || class < 0)
    {
        CT_SetMessage(player, "INVALID PLAYER CLASS", false, NULL);
        return;
    }
    player->class = class;
    for (i = 0; i < NUMARMOR; i++)
    {
        player->armorpoints[i] = 0;
    }
    PlayerClass[consoleplayer] = class;
    P_PostMorphWeapon(player, WP_FIRST);
    SB_SetClassData();
    SB_state = -1;
    player->cheatTics = 1;
}

static void CheatInitFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    G_DeferedInitNew(gameskill, gameepisode, gamemap);
    CT_SetMessage(player, TXT_CHEATWARP, false, NULL);
    player->cheatTics = 1;
}

static void CheatVersionFunc (player_t *player, Cheat_t *cheat)
{
    CT_SetMessage(player, HEXEN_VERSIONTEXT, false, NULL);
    player->cheatTics = 1;
}

static void CheatDebugFunc (player_t *player, Cheat_t *cheat)
{
    char textBuffer[50];
    M_snprintf(textBuffer, sizeof(textBuffer),
               "MAP %d (%d)  X:%5d  Y:%5d  Z:%5d",
               P_GetMapWarpTrans(gamemap),
               gamemap,
               player->mo->x >> FRACBITS,
               player->mo->y >> FRACBITS, player->mo->z >> FRACBITS);
    CT_SetMessage(player, textBuffer, false, NULL);
    player->cheatTics = 1;
}

static void CheatScriptFunc1 (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    CT_SetMessage(player, "RUN WHICH SCRIPT (01-99)?", false, NULL);
}

static void CheatScriptFunc2 (player_t *player, Cheat_t *cheat)
{
    CT_SetMessage(player, "RUN WHICH SCRIPT (01-99)?", false, NULL);
}

static void CheatScriptFunc3 (player_t *player, Cheat_t *cheat)
{
    int script;
    byte script_args[3];
    int tens, ones;
    char textBuffer[40];
    char args[2];

    cht_GetParam(cheat->seq, args);

    tens = args[0] - '0';
    ones = args[1] - '0';
    script = tens * 10 + ones;
    if (script < 1)
        return;
    if (script > 99)
        return;
    script_args[0] = script_args[1] = script_args[2] = 0;

    if (P_StartACS(script, 0, script_args, player->mo, NULL, 0))
    {
        M_snprintf(textBuffer, sizeof(textBuffer),
                   "RUNNING SCRIPT %.2d", script);
        CT_SetMessage(player, textBuffer, false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatRevealFunc (player_t *player, Cheat_t *cheat)
{
    // [JN] Harmless cheat, always allow.
    mapsco_cheating = (mapsco_cheating + 1) % 3;
    player->cheatTics = 1;
}

static void CheatFREEZEFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    crl_freeze ^= 1;
    CT_SetMessage(&players[consoleplayer], crl_freeze ?
                 ID_FREEZE_ON : ID_FREEZE_OFF, false, NULL);
    player->cheatTics = 1;
}

static void CheatNOTARGETFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_NOTARGET;
    P_ForgetPlayer(player);
    CT_SetMessage(player, player->cheats & CF_NOTARGET ?
                  ID_NOTARGET_ON : ID_NOTARGET_OFF, false, NULL);
    player->cheatTics = 1;
}

static void CheatBUDDHAFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_BUDDHA;
    CT_SetMessage(player, player->cheats & CF_BUDDHA ?
                  ID_BUDDHA_ON : ID_BUDDHA_OFF, false, NULL);
    player->cheatTics = 1;
}
