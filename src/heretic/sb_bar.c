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

// SB_bar.c

#include "doomdef.h"
#include "deh_str.h"
#include "i_video.h"
#include "i_swap.h"
#include "i_timer.h"
#include "m_controls.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "am_map.h"
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"


// Types

typedef struct Cheat_s
{
    void (*func) (player_t * player, struct Cheat_s * cheat);
    cheatseq_t *seq;
} Cheat_t;

// Private Functions

static void DrawSoundInfo(void);
static void ShadeLine(int x, int y, int height, int shade);
static void ShadeChain(void);
static void DrINumber(signed int val, int x, int y);
static void DrBNumber(signed int val, int x, int y);
static void DrawCommonBar(void);
static void DrawMainBar(void);
static void DrawInventoryBar(void);
static void DrawFullScreenStuff(void);
static boolean HandleCheats(byte key);

// Public Data

boolean DebugSound;             // debug flag for displaying sound info

boolean inventory;
int curpos;
int inv_ptr;
int ArtifactFlash;

// [crispy] for widescreen status bar background
pixel_t *st_backing_screen;

// Private Data

static int HealthMarker;
static int ChainWiggle;
static player_t *CPlayer;
int playpalette;
int sb_palette = 0;  // [JN] Externalazied variable of current palette index.

patch_t *PatchLTFACE;
patch_t *PatchRTFACE;
patch_t *PatchBARBACK;
patch_t *PatchCHAIN;
patch_t *PatchSTATBAR;
patch_t *PatchLIFEGEM[MAXPLAYERS];  // [JN] Make array for hot swapping in coop spy.
//patch_t *PatchEMPWEAP;
//patch_t *PatchLIL4BOX;
patch_t *PatchLTFCTOP;
patch_t *PatchRTFCTOP;
//patch_t *PatchARMORBOX;
//patch_t *PatchARTIBOX;
patch_t *PatchSELECTBOX;
//patch_t *PatchKILLSPIC;
//patch_t *PatchMANAPIC;
//patch_t *PatchPOWERICN;
patch_t *PatchINVLFGEM1;
patch_t *PatchINVLFGEM2;
patch_t *PatchINVRTGEM1;
patch_t *PatchINVRTGEM2;
patch_t *PatchINumbers[10];
patch_t *PatchNEGATIVE;
patch_t *PatchSmNumbers[10];
patch_t *PatchBLACKSQ;
patch_t *PatchINVBAR;
patch_t *PatchARMCLEAR;
patch_t *PatchCHAINBACK;
//byte *ShadeTables;
int FontBNumBase;
int spinbooklump;
int spinflylump;

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
static void CheatGodFunc (player_t *player, Cheat_t *cheat);

// Get all weapons and ammo
static cheatseq_t CheatWeaponsDoomSeq = CHEAT("idfa", 0);
static cheatseq_t CheatWeaponsHticSeq = CHEAT("rambo", 0);
static cheatseq_t CheatWeaponsHexnSeq = CHEAT("nra", 0);
static void CheatWeaponsFunc (player_t *player, Cheat_t *cheat);

// Get all weapons and keys
static cheatseq_t CheatWpnsKeysDoomSeq = CHEAT("idkfa", 0);
static void CheatWeapKeysFunc (player_t *player, Cheat_t *cheat);

// Get Gauntlets of the Necromancer
static cheatseq_t CheatIDCHOPPERSSeq = CHEAT("idchoppers", 0);
static void CheatChoppersFunc (player_t *player, Cheat_t *cheat);

// Get all keys
static cheatseq_t CheatKeysDoomSeq = CHEAT("idka", 0);
static cheatseq_t CheatKeysHticSeq = CHEAT("skel", 0);
static cheatseq_t CheatKeysHexnSeq = CHEAT("locksmith", 0);
static void CheatKeysFunc (player_t *player, Cheat_t *cheat);

// Toggle no clipping mode
static cheatseq_t CheatNoClipDoomSeq = CHEAT("idclip", 0);
static cheatseq_t CheatNoClipDmSwSeq = CHEAT("idspispopd", 0);
static cheatseq_t CheatNoClipHticSeq = CHEAT("kitty", 0);
static cheatseq_t CheatNoClipHexnSeq = CHEAT("casper", 0);
static void CheatNoClipFunc (player_t *player, Cheat_t *cheat);

// Warp to new level
static cheatseq_t CheatWarpDoomSeq = CHEAT("idclev", 2);
static cheatseq_t CheatWarpHticSeq = CHEAT("engage", 2);
static cheatseq_t CheatWarpHexnSeq = CHEAT("visit", 2);
static void CheatWarpFunc (player_t *player, Cheat_t *cheat);

// Artifacts
static cheatseq_t CheatArtifact1Seq = CHEAT("gimme", 0);  // Get an artifact 1st stage (ask for type)
static cheatseq_t CheatArtifact2Seq = CHEAT("gimme", 1);  // Get an artifact 2nd stage (ask for count)
static cheatseq_t CheatArtifact3Seq = CHEAT("gimme", 2);  // Get an artifact final stage
static cheatseq_t CheatIDBEHOLD1Seq = CHEAT("idbehold", 0);
static cheatseq_t CheatIDBEHOLD2Seq = CHEAT("idbehold", 1);
static cheatseq_t CheatIDBEHOLD3Seq = CHEAT("idbehold", 2);
static void CheatArtifact1Func (player_t *player, Cheat_t *cheat);
static void CheatArtifact2Func (player_t *player, Cheat_t *cheat);
static void CheatArtifact3Func (player_t *player, Cheat_t *cheat);

// Get all artifacts
static cheatseq_t CheatArtifactsHexnSeq = CHEAT("indiana", 0);
static void CheatArtifactAllFunc (player_t *player, Cheat_t *cheat);

// Toggle tome of power
static cheatseq_t CheatPowerSeq = CHEAT("shazam", 0);
static void CheatPowerFunc (player_t *player, Cheat_t *cheat);

// Get full health
static cheatseq_t CheatHealthHticSeq = CHEAT("ponce", 0);
static cheatseq_t CheatHealthHexnSeq = CHEAT("clubmed", 0);
static void CheatHealthFunc (player_t *player, Cheat_t *cheat);

// Turn to a chicken
static cheatseq_t CheatChickenHticSeq = CHEAT("cockadoodledoo", 0);
static cheatseq_t CheatChickenHexnSeq = CHEAT("deliverance", 0);
static void CheatChickenFunc (player_t *player, Cheat_t *cheat);

// Kill all monsters
static cheatseq_t CheatMassacreDoomBSeq = CHEAT("tntem", 0);
static cheatseq_t CheatMassacreDoomMSeq = CHEAT("killem", 0);
static cheatseq_t CheatMassacreHticSeq = CHEAT("massacre", 0);
static cheatseq_t CheatMassacreHexnSeq = CHEAT("butcher", 0);
static void CheatMassacreFunc (player_t *player, Cheat_t *cheat);
static void CheatTNTEMFunc (player_t *player, Cheat_t *cheat);
static void CheatKILLEMFunc (player_t *player, Cheat_t *cheat);

// [JN] IDMUS
static cheatseq_t CheatIDMUSSeq = CHEAT("idmus", 2);
static void CheatIDMUSFunc (player_t *player, Cheat_t *cheat);

// Reveal all map
static cheatseq_t CheatRevealDoomSeq = CHEAT("iddt", 0);
static cheatseq_t CheatRevealHticSeq = CHEAT("ravmap", 0);
static cheatseq_t CheatRevealHexnSeq = CHEAT("mapsco", 0);
static void CheatAMapFunc (player_t *player, Cheat_t *cheat);

// [JN] IDMYPOS coords
static cheatseq_t CheatDebugDoomSeq = CHEAT("idmypos", 0);
static cheatseq_t CheatDebugHexnSeq = CHEAT("where", 0);
static void CheatIDMYPOSFunc (player_t *player, Cheat_t *cheat);

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
    // Get all weapons and ammo
    { CheatWeaponsFunc,     &CheatWeaponsDoomSeq   },
    { CheatWeaponsFunc,     &CheatWeaponsHticSeq   },
    { CheatWeaponsFunc,     &CheatWeaponsHexnSeq   },
    // Get all weapons and keys
    { CheatWeapKeysFunc,    &CheatWpnsKeysDoomSeq  },
    // Get Gauntlets of the Necromancer
    { CheatChoppersFunc,    &CheatIDCHOPPERSSeq    },
    // Get all keys
    { CheatKeysFunc,        &CheatKeysDoomSeq      },
    { CheatKeysFunc,        &CheatKeysHticSeq      },
    { CheatKeysFunc,        &CheatKeysHexnSeq      },
    // Toggle no clipping mode
    { CheatNoClipFunc,      &CheatNoClipDoomSeq    },
    { CheatNoClipFunc,      &CheatNoClipDmSwSeq    },
    { CheatNoClipFunc,      &CheatNoClipHticSeq    },
    { CheatNoClipFunc,      &CheatNoClipHexnSeq    },
    // Warp to new level
    { CheatWarpFunc,        &CheatWarpDoomSeq      },
    { CheatWarpFunc,        &CheatWarpHticSeq      },
    { CheatWarpFunc,        &CheatWarpHexnSeq      },
    // Artifacts
    { CheatArtifact1Func, &CheatArtifact1Seq       },
    { CheatArtifact2Func, &CheatArtifact2Seq       },
    { CheatArtifact3Func, &CheatArtifact3Seq       },
    { CheatArtifact1Func, &CheatIDBEHOLD1Seq       },
    { CheatArtifact2Func, &CheatIDBEHOLD2Seq       },
    { CheatArtifact3Func, &CheatIDBEHOLD3Seq       },
    // Get all artifacts
    { CheatArtifactAllFunc, &CheatArtifactsHexnSeq },
    // Toggle tome of power
    { CheatPowerFunc,     &CheatPowerSeq           },
    // Get full health
    { CheatHealthFunc,    &CheatHealthHticSeq      },
    { CheatHealthFunc,    &CheatHealthHexnSeq      },
    // Turn to a chicken
    { CheatChickenFunc,   &CheatChickenHticSeq     },
    { CheatChickenFunc,   &CheatChickenHexnSeq     },    
    // Kill all monsters
    { CheatTNTEMFunc,     &CheatMassacreDoomBSeq   },
    { CheatKILLEMFunc,    &CheatMassacreDoomMSeq   },
    { CheatMassacreFunc,  &CheatMassacreHticSeq    },
    { CheatMassacreFunc,  &CheatMassacreHexnSeq    },
    // [JN] IDMUS
    { CheatIDMUSFunc,     &CheatIDMUSSeq           },
    // Reveal all map
    { CheatAMapFunc,      &CheatRevealDoomSeq      },
    { CheatAMapFunc,      &CheatRevealHticSeq      },
    { CheatAMapFunc,      &CheatRevealHexnSeq      },
    // [JN] IDMYPOS coords
    { CheatIDMYPOSFunc,   &CheatDebugDoomSeq       },
    { CheatIDMYPOSFunc,   &CheatDebugHexnSeq       },
    // [JN] ID-specific modes
    { CheatFREEZEFunc,    &CheatFREEZESeq          },
    { CheatNOTARGETFunc,  &CheatNOTARGETSeq        },
    { CheatBUDDHAFunc,    &CheatBUDDHASeq          },
    {NULL,                NULL                     }
};

//---------------------------------------------------------------------------
//
// PROC SB_Init
//
//---------------------------------------------------------------------------

void SB_Init(void)
{
    int i;
    int startLump;

    PatchLTFACE = W_CacheLumpName(DEH_String("LTFACE"), PU_STATIC);
    PatchRTFACE = W_CacheLumpName(DEH_String("RTFACE"), PU_STATIC);
    PatchBARBACK = W_CacheLumpName(DEH_String("BARBACK"), PU_STATIC);
    PatchINVBAR = W_CacheLumpName(DEH_String("INVBAR"), PU_STATIC);
    PatchCHAIN = W_CacheLumpName(DEH_String("CHAIN"), PU_STATIC);
    if (deathmatch)
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("STATBAR"), PU_STATIC);
    }
    else
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("LIFEBAR"), PU_STATIC);
    }
    // [JN] Better support for spy mode by loading
    // all health gems and using displayplayer to choose them:
    startLump = W_GetNumForName(DEH_String("LIFEGEM0"));
    for (i = 0; i < MAXPLAYERS; i++)
    {
        PatchLIFEGEM[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PatchLTFCTOP = W_CacheLumpName(DEH_String("LTFCTOP"), PU_STATIC);
    PatchRTFCTOP = W_CacheLumpName(DEH_String("RTFCTOP"), PU_STATIC);
    PatchSELECTBOX = W_CacheLumpName(DEH_String("SELECTBOX"), PU_STATIC);
    PatchINVLFGEM1 = W_CacheLumpName(DEH_String("INVGEML1"), PU_STATIC);
    PatchINVLFGEM2 = W_CacheLumpName(DEH_String("INVGEML2"), PU_STATIC);
    PatchINVRTGEM1 = W_CacheLumpName(DEH_String("INVGEMR1"), PU_STATIC);
    PatchINVRTGEM2 = W_CacheLumpName(DEH_String("INVGEMR2"), PU_STATIC);
    PatchBLACKSQ = W_CacheLumpName(DEH_String("BLACKSQ"), PU_STATIC);
    PatchARMCLEAR = W_CacheLumpName(DEH_String("ARMCLEAR"), PU_STATIC);
    PatchCHAINBACK = W_CacheLumpName(DEH_String("CHAINBACK"), PU_STATIC);
    startLump = W_GetNumForName(DEH_String("IN0"));
    for (i = 0; i < 10; i++)
    {
        PatchINumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PatchNEGATIVE = W_CacheLumpName(DEH_String("NEGNUM"), PU_STATIC);
    FontBNumBase = W_GetNumForName(DEH_String("FONTB16"));
    startLump = W_GetNumForName(DEH_String("SMALLIN0"));
    for (i = 0; i < 10; i++)
    {
        PatchSmNumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    playpalette = W_GetNumForName(DEH_String("PLAYPAL"));
    spinbooklump = W_GetNumForName(DEH_String("SPINBK0"));
    spinflylump = W_GetNumForName(DEH_String("SPFLY0"));

    st_backing_screen = (pixel_t *) Z_Malloc(MAXWIDTH * (42 * MAXHIRES) * sizeof(*st_backing_screen), PU_STATIC, 0);
}

//---------------------------------------------------------------------------
//
// PROC SB_Ticker
//
//---------------------------------------------------------------------------

void SB_Ticker(void)
{
    int delta;
    int curHealth;

    if (leveltime & 1)
    {
        ChainWiggle = P_Random() & 1;
    }
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
        else if (delta > 8)
        {
            delta = 8;
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
        else if (delta > 8)
        {
            delta = 8;
        }
        HealthMarker += delta;
    }

    // [JN] Update IDWidget data.
    CPlayer = &players[displayplayer];
    IDWidget.kills = CPlayer->killcount;
    IDWidget.totalkills = totalkills;
    IDWidget.items = CPlayer->itemcount;
    IDWidget.totalitems = totalitems;
    IDWidget.secrets = CPlayer->secretcount;
    IDWidget.totalsecrets = totalsecret;

    IDWidget.x = CPlayer->mo->x >> FRACBITS;
    IDWidget.y = CPlayer->mo->y >> FRACBITS;
    IDWidget.ang = CPlayer->mo->angle / ANG1;

    if (deathmatch)
    {
        static int totalFrags[MAXPLAYERS];

        for (int i = 0 ; i < MAXPLAYERS ; i++)
        {
            totalFrags[i] = 0;

            if (playeringame[i])
            {
                for (int j = 0 ; j < MAXPLAYERS ; j++)
                {
                    totalFrags[i] += players[i].frags[j];
                }
            }
            IDWidget.frags_g = totalFrags[0];
            IDWidget.frags_y = totalFrags[1];
            IDWidget.frags_r = totalFrags[2];
            IDWidget.frags_b = totalFrags[3];
        }
    }

    // [JN] Do red-/gold-shifts from damage/items.
    if (vis_smooth_palette)
    {
        SB_SmoothPaletteFlash();
    }
    else
    {
        SB_PaletteFlash();
    }
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

static void DrINumber(signed int val, int x, int y)
{
    patch_t *patch;
    int oldval;

    oldval = val;
    if (val < 0)
    {
        if (val < -9)
        {
            V_DrawPatch(x + 1, y + 1, W_CacheLumpName(DEH_String("LAME"), PU_CACHE));
        }
        else
        {
            val = -val;
            V_DrawPatch(x + 18, y, PatchINumbers[val]);
            V_DrawPatch(x + 9, y, PatchNEGATIVE);
        }
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
        V_DrawPatch(x + 9, y, patch);
    }
    val = val % 10;
    patch = PatchINumbers[val];
    V_DrawPatch(x + 18, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws a three digit number using FontB
//
//---------------------------------------------------------------------------

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

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small two digit number.
//
//---------------------------------------------------------------------------

static void DrSmallNumber(int val, int x, int y)
{
    patch_t *patch;

    if (val == 1)
    {
        return;
    }
    if (val > 9)
    {
        patch = PatchSmNumbers[val / 10];
        V_DrawPatch(x, y, patch);
    }
    val = val % 10;
    patch = PatchSmNumbers[val];
    V_DrawPatch(x + 4, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC ShadeLine
//
//---------------------------------------------------------------------------

static void ShadeLine(int x, int y, int height, int shade)
{
    pixel_t *dest;

    x *= vid_resolution;
    y *= vid_resolution;
    height *= vid_resolution;

    shade = (int)BETWEEN(0, 255, 0xFF - (((9 + shade * 2) << 8) / 32) * vid_contrast); // [crispy] shade to darkest 32nd COLORMAP row
    dest = I_VideoBuffer + y * SCREENWIDTH + x + (WIDESCREENDELTA * vid_resolution);
    while (height--)
    {
        // [JN] Need to draw extra lines for higher resolutions.
        if (vid_resolution > 1)
        {
            for (int i = 0 ; i < vid_resolution - 1 ; i++)
            {
                *(dest + i + 1) = I_BlendDark(*dest, shade);
            }
        }

        *(dest) = I_BlendDark(*dest, shade);

        dest += SCREENWIDTH;
    }
}

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

static void ShadeChain(void)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        ShadeLine(277 + i, 190, 10, i / 2);
        ShadeLine(19 + i, 190, 10, 7 - (i / 2));
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawSoundInfo
//
// Displays sound debugging information.
//
//---------------------------------------------------------------------------

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
        MN_DrTextA(DEH_String("*** SOUND DEBUG INFO ***"), xPos[0], 20, NULL);
    }
    S_GetChannelInfo(&s);
    if (s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA(DEH_String("NAME"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.T"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.X"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.Y"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("ID"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("PRI"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("DIST"), xPos[x++], 30, NULL);
    for (i = 0; i < s.channelCount; i++)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if (c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA(DEH_String("------"), xPos[0], y, NULL);
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
        M_snprintf(text, sizeof(text), "%d", c->id);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->priority);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->distance);
        MN_DrTextA(text, xPos[x++], y, NULL);
    }
}

//---------------------------------------------------------------------------
//
// PROC SB_Drawer
//
//---------------------------------------------------------------------------

char patcharti[][10] = {
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIINVS"},               // invisibility
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIPWBK"},               // tomeofpower
    {"ARTITRCH"},               // torch
    {"ARTIFBMB"},               // firebomb
    {"ARTIEGGC"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIATLP"}                // teleport
};

char ammopic[][10] = {
    {"INAMGLD"},
    {"INAMBOW"},
    {"INAMBST"},
    {"INAMRAM"},
    {"INAMPNX"},
    {"INAMLOB"}
};

int SB_state = -1;
static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldammo = -1;
static int oldarmor = -1;
static int oldweapon = -1;
static int oldhealth = -1;
static int oldlife = -1;
static int oldkeys = -1;

int playerkeys = 0;


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
        const char *name = (gamemode == shareware) ?
                           DEH_String("FLOOR04") :
                           DEH_String("FLAT513");

        src = W_CacheLumpName(name, PU_CACHE);
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
    V_CopyRect(0, 0, st_backing_screen, SCREENWIDTH, SBARHEIGHT, 0, 158 * vid_resolution);
}

enum
{
    ammowidgetcolor_ammo,
    ammowidgetcolor_weapon
} ammowidgetcolor_t;


// -----------------------------------------------------------------------------
// SB_AmmoWidgetColor
// [plums] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

static byte *SB_AmmoWidgetColor (int i, weapontype_t weapon)
{
    switch (i)
    {
        case ammowidgetcolor_ammo:
        {

            const int ammo = CPlayer->ammo[wpnlev1info[weapon].ammo];
            const int fullammo = CPlayer->maxammo[wpnlev1info[weapon].ammo];

            if (st_ammo_widget_colors != 1 && st_ammo_widget_colors != 2)
            {
                return cr[CR_GRAY];
            }

            if (ammo < fullammo/4)
                return cr[CR_RED];
            else if (ammo < fullammo/2)
                return cr[CR_YELLOW];
            else
                return cr[CR_GREEN];
        }
        case ammowidgetcolor_weapon:
        {
            // always color the weapon letter if ammo widget coloring is OFF
            if ((st_ammo_widget_colors != 1 && st_ammo_widget_colors != 3) ||
                CPlayer->weaponowned[weapon] == true)
            {
                switch (weapon)
                {
                    case wp_goldwand:   return cr[CR_YELLOW];
                    case wp_crossbow:   return cr[CR_GREEN];
                    case wp_blaster:    return cr[CR_BLUE2];
                    case wp_skullrod:   return cr[CR_RED];
                    case wp_phoenixrod: return cr[CR_ORANGE];
                    case wp_mace:       return cr[CR_LIGHTGRAY];
                    default:            return cr[CR_GRAY];
                }
            }
            else
            {
                return cr[CR_GRAY];
            }
        }
    }

    return NULL;
}

void SB_Drawer(void)
{
    int frame;
    static boolean hitCenterFrame;

    // Sound info debug stuff
    if (DebugSound == true)
    {
        DrawSoundInfo();
    }
    CPlayer = &players[displayplayer];
    if (viewheight == SCREENHEIGHT && (!automapactive || automap_overlay))
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
            if (SHORT(PatchBARBACK->width) > ORIGWIDTH &&
                    SHORT(PatchBARBACK->leftoffset) == 0)
            {
                V_DrawPatch((ORIGWIDTH - SHORT(PatchBARBACK->width)) / 2, 158,
                        PatchBARBACK);
            }
            else
            {
                V_DrawPatch(0, 158, PatchBARBACK);
            }

            if (players[displayplayer].cheats & CF_GODMODE)
            {
                V_DrawPatch(16, 167,
                            W_CacheLumpName(DEH_String("GOD1"), PU_CACHE));
                V_DrawPatch(287, 167,
                            W_CacheLumpName(DEH_String("GOD2"), PU_CACHE));
            }
            oldhealth = -1;
        }
        DrawCommonBar();
        if (!inventory)
        {
            if (SB_state != 0)
            {
                // Main interface
                V_DrawPatch(34, 160, PatchSTATBAR);
                oldarti = 0;
                oldammo = -1;
                oldarmor = -1;
                oldweapon = -1;
                oldfrags = -9999;       //can't use -1, 'cuz of negative frags
                oldlife = -1;
                oldkeys = -1;
            }
            DrawMainBar();
            SB_state = 0;
        }
        else
        {
            if (SB_state != 1)
            {
                V_DrawPatch(34, 160, PatchINVBAR);
            }
            DrawInventoryBar();
            SB_state = 1;
        }
    }

    // Flight icons
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
                    V_DrawPatch(spinfly_x, 17,
                                W_CacheLumpNum(spinflylump + 15,
                                                PU_CACHE));
                }
                else
                {
                    V_DrawPatch(spinfly_x, 17,
                                W_CacheLumpNum(spinflylump + frame,
                                                PU_CACHE));
                    hitCenterFrame = false;
                }
            }
            else
            {
                if (!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(spinfly_x, 17,
                                W_CacheLumpNum(spinflylump + frame,
                                                PU_CACHE));
                    hitCenterFrame = false;
                }
                else
                {
                    V_DrawPatch(spinfly_x, 17,
                                W_CacheLumpNum(spinflylump + 15,
                                                PU_CACHE));
                    hitCenterFrame = true;
                }
            }
        }
    }

    if (CPlayer->powers[pw_weaponlevel2] && !CPlayer->chickenTics)
    {
        int spinbook_x = 300 + WIDESCREENDELTA; // [crispy]

        // [JN] Shift tome icon left if fps counter,
        // local time or demo timer is active.
        if (vid_showfps
        ||  msg_local_time
        || (demoplayback && (demo_timer == 1 || demo_timer == 3))
        || (demorecording && (demo_timer == 2 || demo_timer == 3)))
        {
            spinbook_x -= 70;
        }

        if (CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_weaponlevel2] & 16))
        {
            frame = (leveltime / 3) & 15;
            V_DrawPatch(spinbook_x, 17,
                        W_CacheLumpNum(spinbooklump + frame, PU_CACHE));
        }
    }

    // [JN] Ammo widget.
    if (st_ammo_widget)
    {
        char str[8];
        // [JN] Shift widgets based on the "Widgets alignment" setting.
        const int xx = (widget_alignment ==  0) ? WIDESCREENDELTA :     // left
                       (widget_alignment ==  1) ? 0 :                   // status bar
                       (dp_screen_size   >= 12) ? WIDESCREENDELTA : 0;  // auto
        // [JN] Move widgets slightly down when using a fullscreen status bar.
        const int yy = dp_screen_size > 10
                        && (!automapactive || automap_overlay) ? 13 : 0;

        // Brief
        if (st_ammo_widget == 1)
        {
            dp_translucent = (st_ammo_widget_translucent);

            MN_DrTextA("W", 282 + xx,  96 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_goldwand));
            MN_DrTextA("E", 282 + xx, 106 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_crossbow));
            MN_DrTextA("D", 282 + xx, 116 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_blaster));
            MN_DrTextA("H", 282 + xx, 126 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_skullrod));
            MN_DrTextA("P", 282 + xx, 136 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_phoenixrod));
            MN_DrTextA("M", 282 + xx, 146 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_mace));

            // Elven Wand
            sprintf(str, "%d",  CPlayer->ammo[am_goldwand]);
            MN_DrTextA(str, 293 + xx, 96 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_goldwand));

            // Ethereal Crossbow
            sprintf(str, "%d",  CPlayer->ammo[am_crossbow]);
            MN_DrTextA(str, 293 + xx, 106 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_crossbow));

            // Dragon Claw
            sprintf(str, "%d",  CPlayer->ammo[am_blaster]);
            MN_DrTextA(str, 293 + xx, 116 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_blaster));

            // Hellstaff
            sprintf(str, "%d",  CPlayer->ammo[am_skullrod]);
            MN_DrTextA(str, 293 + xx, 126 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_skullrod));

            // Phoenix Rod
            sprintf(str, "%d",  CPlayer->ammo[am_phoenixrod]);
            MN_DrTextA(str, 293 + xx, 136 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_phoenixrod));

            // Firemace
            sprintf(str, "%d",  CPlayer->ammo[am_mace]);
            MN_DrTextA(str, 293 + xx, 146 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_mace));

            dp_translucent = false;
        }
        // Full
        else
        {
            dp_translucent = (st_ammo_widget_translucent);

            MN_DrTextA("W", 251 + xx,  96 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_goldwand));
            MN_DrTextA("E", 251 + xx, 106 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_crossbow));
            MN_DrTextA("D", 251 + xx, 116 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_blaster));
            MN_DrTextA("H", 251 + xx, 126 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_skullrod));
            MN_DrTextA("P", 251 + xx, 136 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_phoenixrod));
            MN_DrTextA("M", 251 + xx, 146 + yy, SB_AmmoWidgetColor(ammowidgetcolor_weapon, wp_mace));

            // Elven Wand
            sprintf(str, "%d/",  CPlayer->ammo[am_goldwand]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 96 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_goldwand));
            sprintf(str, "%d",  CPlayer->maxammo[am_goldwand]);
            MN_DrTextA(str, 293 + xx, 96 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_goldwand));

            // Ethereal Crossbow
            sprintf(str, "%d/",  CPlayer->ammo[am_crossbow]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 106 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_crossbow));
            sprintf(str, "%d",  CPlayer->maxammo[am_crossbow]);
            MN_DrTextA(str, 293 + xx, 106 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_crossbow));

            // Dragon Claw
            sprintf(str, "%d/",  CPlayer->ammo[am_blaster]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 116 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_blaster));
            sprintf(str, "%d",  CPlayer->maxammo[am_blaster]);
            MN_DrTextA(str, 293 + xx, 116 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_blaster));

            // Hellstaff
            sprintf(str, "%d/",  CPlayer->ammo[am_skullrod]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 126 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_skullrod));
            sprintf(str, "%d",  CPlayer->maxammo[am_skullrod]);
            MN_DrTextA(str, 293 + xx, 126 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_skullrod));

            // Phoenix Rod
            sprintf(str, "%d/",  CPlayer->ammo[am_phoenixrod]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 136 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_phoenixrod));
            sprintf(str, "%d",  CPlayer->maxammo[am_phoenixrod]);
            MN_DrTextA(str, 293 + xx, 136 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_phoenixrod));

            // Firemace
            sprintf(str, "%d/",  CPlayer->ammo[am_mace]);
            MN_DrTextA(str, 293 + xx - MN_TextAWidth(str), 146 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_mace));
            sprintf(str, "%d",  CPlayer->maxammo[am_mace]);
            MN_DrTextA(str, 293 + xx, 146 + yy, SB_AmmoWidgetColor(ammowidgetcolor_ammo, wp_mace));

            dp_translucent = false;
        }
    }

/*
		if(CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
			|| (CPlayer->powers[pw_weaponlevel2]&8))
		{
			V_DrawPatch(291, 0, W_CacheLumpName("ARTIPWBK", PU_CACHE));
		}
		else
		{
			BorderTopRefresh = true;
		}
	}
*/
}

// -----------------------------------------------------------------------------
// SB_NumberColor
// [crispy] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

enum
{
    hudcolor_ammo,
    hudcolor_health,
    hudcolor_frags,
    hudcolor_armor
} hudcolor_t;

static byte *SB_NumberColor (int i)
{
    if (!st_colored_stbar)
    {
        return NULL;
    }

    switch (i)
    {
        case hudcolor_ammo:
        {
            if (wpnlev1info[CPlayer->readyweapon].ammo == am_noammo)
            {
                return NULL;
            }
            else
            {
                const int ammo = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
                const int fullammo = CPlayer->maxammo[wpnlev1info[CPlayer->readyweapon].ammo];

                if (ammo < fullammo/4)
                    return cr[CR_RED];
                else if (ammo < fullammo/2)
                    return cr[CR_YELLOW];
                else
                    return cr[CR_GREEN];
            }
            break;
        }
        case hudcolor_health:
        {
            const int health = CPlayer->health;

            // [crispy] Invulnerability powerup and God Mode cheat turn Health values into flame gold
            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                return cr[CR_FLAME];
            else if (health >= 67)
                return cr[CR_GREEN];
            else if (health >= 34)
                return cr[CR_YELLOW];
            else
                return cr[CR_RED];
            break;
        }
        case hudcolor_frags:
        {
            const int frags = CPlayer->frags[displayplayer];

            if (frags < 0)
                return cr[CR_RED];
            else if (frags == 0)
                return cr[CR_YELLOW];
            else
                return cr[CR_GREEN];
            break;
        }
        case hudcolor_armor:
        {
	    // [crispy] Invulnerability powerup and God Mode cheat turn Armor values into flame gold
	    if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                return cr[CR_FLAME];
	    // [crispy] color by armor type
	    else if (CPlayer->armortype >= 2)
                return cr[CR_YELLOW];
	    else if (CPlayer->armortype == 1)
                return cr[CR_LIGHTGRAY];
	    else if (CPlayer->armortype == 0)
                return cr[CR_RED];
            break;
        }
    }

    return NULL;
}

// sets the new palette based upon current values of player->damagecount
// and player->bonuscount
void SB_PaletteFlash(void)
{
    int palette;

    CPlayer = &players[displayplayer];

    if (CPlayer->damagecount)
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
    else
    {
        palette = 0;
    }
    if (palette != sb_palette)
    {
        sb_palette = palette;
        I_SetPalette(palette);
    }
}

// -----------------------------------------------------------------------------
// SB_SmoothPaletteFlash
// [JN/PN] Smooth palette handling.
// Handles smooth palette transitions for a better visual effect.
// -----------------------------------------------------------------------------

void SB_SmoothPaletteFlash (void)
{
    int palette = 0;
    // [JN] A11Y - Palette flash effects.
    // [PN] Maximum alpha values for palette flash effects (damage and bonus).
    // Each row represents an effect type, with 4 intensity levels:
    // [Full intensity, Half intensity, Quarter intensity, Minimal visibility/Off].
    static const int max_alpha[2][4] = {
        { 226, 113, 56, 0 }, // Damage (red)
        { 127,  64, 32, 0 }  // Bonus (yellow)
    };

    CPlayer = &players[displayplayer];

    if (CPlayer->damagecount)
    {
        palette = 1;
        red_pane_alpha = MIN(CPlayer->damagecount * PAINADD, max_alpha[0][a11y_pal_flash]);
    }
    else if (CPlayer->bonuscount)
    {
        palette = 9;
        yel_pane_alpha = MIN(CPlayer->bonuscount * BONUSADD, max_alpha[1][a11y_pal_flash]);
    }

    if (palette != sb_palette || CPlayer->damagecount || CPlayer->bonuscount)
    {
        sb_palette = palette;
        I_SetPalette(palette);
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawCommonBar
//
//---------------------------------------------------------------------------

void DrawCommonBar(void)
{
    int chainY;
    int healthPos;

    V_DrawPatch(0, 148, PatchLTFCTOP);
    V_DrawPatch(290, 148, PatchRTFCTOP);

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
        healthPos = (healthPos * 256) / 100;
        // [JN] Do not refer to CPlayer as map object (mo->) here,
        // otherwise chain will keep wiggling while SB_state = -1.
        chainY =
            (HealthMarker == CPlayer->/*mo->*/health) ? 191 : 191 + ChainWiggle;
        V_DrawPatch(0, 190, PatchCHAINBACK);
        V_DrawPatch(2 + (healthPos % 17), chainY, PatchCHAIN);
        // [JN] Make health gem change with displayplayer.
        // Single player game uses red life gem.
        V_DrawPatch(17 + healthPos, chainY,
                    !netgame ? PatchLIFEGEM[2] : PatchLIFEGEM[displayplayer]);
        V_DrawPatch(0, 190, PatchLTFACE);
        V_DrawPatch(276, 190, PatchRTFACE);
        ShadeChain();
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

void DrawMainBar(void)
{
    int i;
    int temp;

    // Ready artifact
    if (ArtifactFlash)
    {
        V_DrawPatch(180, 161, PatchBLACKSQ);

        temp = W_GetNumForName(DEH_String("useartia")) + ArtifactFlash - 1;

        V_DrawPatch(182, 161, W_CacheLumpNum(temp, PU_CACHE));
        ArtifactFlash--;
        oldarti = -1;           // so that the correct artifact fills in after the flash
    }
    else if (oldarti != CPlayer->readyArtifact
             || oldartiCount != CPlayer->inventory[inv_ptr].count)
    {
        V_DrawPatch(180, 161, PatchBLACKSQ);
        if (CPlayer->readyArtifact > 0)
        {
            V_DrawPatch(179, 160,
                        W_CacheLumpName(DEH_String(patcharti[CPlayer->readyArtifact]),
                                        PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 201, 182);
        }
        oldarti = CPlayer->readyArtifact;
        oldartiCount = CPlayer->inventory[inv_ptr].count;
    }

    // Frags
    if (deathmatch)
    {
        temp = 0;
        for (i = 0; i < MAXPLAYERS; i++)
        {
            temp += CPlayer->frags[i];
        }
        if (temp != oldfrags)
        {
            V_DrawPatch(57, 171, PatchARMCLEAR);
            dp_translation = SB_NumberColor(hudcolor_frags);
            DrINumber(temp, 61, 170);
            dp_translation = NULL;
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
            V_DrawPatch(57, 171, PatchARMCLEAR);
            dp_translation = SB_NumberColor(hudcolor_health);
            DrINumber(temp, 61, 170);
            dp_translation = NULL;
        }
    }

    // Keys
    if (oldkeys != playerkeys)
    {
        if (CPlayer->keys[key_yellow])
        {
            V_DrawPatch(153, 164, W_CacheLumpName(DEH_String("ykeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_green])
        {
            V_DrawPatch(153, 172, W_CacheLumpName(DEH_String("gkeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_blue])
        {
            V_DrawPatch(153, 180, W_CacheLumpName(DEH_String("bkeyicon"), PU_CACHE));
        }
        oldkeys = playerkeys;
    }
    // Ammo
    temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
    if (oldammo != temp || oldweapon != CPlayer->readyweapon)
    {
        V_DrawPatch(108, 161, PatchBLACKSQ);
        if (temp && CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
        {
            dp_translation = SB_NumberColor(hudcolor_ammo);
            DrINumber(temp, 109, 162);
            dp_translation = NULL;
            V_DrawPatch(111, 172,
                        W_CacheLumpName(DEH_String(ammopic[CPlayer->readyweapon - 1]),
                                        PU_CACHE));
        }
        oldammo = temp;
        oldweapon = CPlayer->readyweapon;
    }

    // Armor
    // [JN] Need to perform update for colored status bar.
    // TODO - not very optimal, ideally to update once after invul. runs out.
    if (oldarmor != CPlayer->armorpoints || st_colored_stbar)
    {
        V_DrawPatch(224, 171, PatchARMCLEAR);
        dp_translation = SB_NumberColor(hudcolor_armor);
        DrINumber(CPlayer->armorpoints, 228, 170);
        dp_translation = NULL;
        oldarmor = CPlayer->armorpoints;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

void DrawInventoryBar(void)
{
    const char *patch;
    int i;
    int x;

    x = inv_ptr - curpos;
    V_DrawPatch(34, 160, PatchINVBAR);
    for (i = 0; i < 7; i++)
    {
        //V_DrawPatch(50+i*31, 160, W_CacheLumpName("ARTIBOX", PU_CACHE));
        if (CPlayer->inventorySlotNum > x + i
            && CPlayer->inventory[x + i].type != arti_none)
        {
            patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);

            V_DrawPatch(50 + i * 31, 160, W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31, 182);
        }
    }
    V_DrawPatch(50 + curpos * 31, 189, PatchSELECTBOX);
    if (x != 0)
    {
        V_DrawPatch(38, 159, !(leveltime & 4) ? PatchINVLFGEM1 :
                    PatchINVLFGEM2);
    }
    if (CPlayer->inventorySlotNum - x > 7)
    {
        V_DrawPatch(269, 159, !(leveltime & 4) ?
                    PatchINVRTGEM1 : PatchINVRTGEM2);
    }
}

// -----------------------------------------------------------------------------
// DrawFullScreenStuff
// [JN] Upgraded to draw extra elements.
// -----------------------------------------------------------------------------

static void DrawFullScreenStuff (void)
{
    const char *patch;
    const int wide_x = dp_screen_size == 12 ? WIDESCREENDELTA : 0;
    int i;

    // Health.
    dp_translation = SB_NumberColor(hudcolor_health);
    DrBNumber(CPlayer->health, -1 - wide_x, 175);
    dp_translation = NULL;
    // Draw health vial.
    V_DrawShadowedPatchNoOffsets(39 - wide_x, 176, W_CacheLumpName(DEH_String("PTN1A0"), PU_CACHE));

    if (!inventory)
    {
        // Armor.
        if (CPlayer->armorpoints > 0)
        {
            dp_translation = SB_NumberColor(hudcolor_armor);
            DrBNumber(CPlayer->armorpoints, 51 - wide_x, 175);
            dp_translation = NULL;

            // [JN] Draw an appropriate picture of a shield.
            // Slightly different placements needed for better placement.
            if (CPlayer->armortype == 1)
            {
                V_DrawShadowedPatchNoOffsets(91 - wide_x, 174, W_CacheLumpName(DEH_String("SHLDA0"), PU_CACHE));
            }
            else
            {
                V_DrawShadowedPatchNoOffsets(91 - wide_x, 172, W_CacheLumpName(DEH_String("SHD2A0"), PU_CACHE));
            }
        }

        // Frags.
        if (deathmatch)
        {
            int temp = 0;

            for (i = 0 ; i < MAXPLAYERS ; i++)
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
            // [JN] Move a little to the right until player has one of 
            // the keys to avoid drawing too much empty space.
            const int xx = CPlayer->keys[key_yellow]
                        || CPlayer->keys[key_green]
                        || CPlayer->keys[key_blue] ? 0 : 16;

            patch = DEH_String(patcharti[CPlayer->readyArtifact]);

            V_DrawAltTLPatch(211 + xx + wide_x, 170, W_CacheLumpName(DEH_String("ARTIBOX"), PU_CACHE));
            V_DrawShadowedPatch(211 + xx + wide_x, 170, W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 232 + xx + wide_x, 192);
        }

        // Keys.
        {
            if (CPlayer->keys[key_yellow])
            {
                V_DrawShadowedPatch(247 + wide_x, 173, W_CacheLumpName(DEH_String("YKEYICON"), PU_CACHE));
            }
            if (CPlayer->keys[key_green])
            {
                V_DrawShadowedPatch(247 + wide_x, 181, W_CacheLumpName(DEH_String("GKEYICON"), PU_CACHE));
            }
            if (CPlayer->keys[key_blue])
            {
                V_DrawShadowedPatch(247 + wide_x, 189, W_CacheLumpName(DEH_String("BKEYICON"), PU_CACHE));
            }
        }
    }
    else
    {
        int x = inv_ptr - curpos;

        for (i = 0 ; i < 7 ; i++)
        {
            V_DrawAltTLPatch(47 + i * 31, 169, W_CacheLumpName(DEH_String("ARTIBOX"), PU_CACHE));

            if (CPlayer->inventorySlotNum > x + i && CPlayer->inventory[x + i].type != arti_none)
            {
                patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);
                V_DrawPatch(47 + i * 31, 169, W_CacheLumpName(patch, PU_CACHE));
                DrSmallNumber(CPlayer->inventory[x + i].count, 66 + i * 31, 191);
            }
        }

        V_DrawPatch(47 + curpos * 31, 198, PatchSELECTBOX);

        if (x != 0)
        {
            V_DrawPatch(35, 168, !(leveltime & 4) ? PatchINVLFGEM1 : PatchINVLFGEM2);
        }
        if (CPlayer->inventorySlotNum - x > 7)
        {
            V_DrawPatch(266, 168, !(leveltime & 4) ? PatchINVRTGEM1 : PatchINVRTGEM2);
        }
    }

    // [JN] Draw amount of current weapon ammo. Don't draw for staff and gauntlets.
    if (CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
    {
        dp_translation = SB_NumberColor(hudcolor_ammo);
        DrBNumber(CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo], 262 + wide_x, 175);
        dp_translation = NULL;

        // Draw appropriate ammo picture.
        V_DrawShadowedPatch(297 + wide_x, 177, W_CacheLumpName(DEH_String(ammopic[CPlayer->readyweapon - 1]), PU_CACHE));
    }
}

//--------------------------------------------------------------------------
//
// FUNC SB_Responder
//
//--------------------------------------------------------------------------

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

//--------------------------------------------------------------------------
//
// FUNC HandleCheats
//
// Returns true if the caller should eat the key.
//
//--------------------------------------------------------------------------

static boolean HandleCheats(byte key)
{
    int i;
    boolean eat;

    /* [crispy] check for nightmare/netgame per cheat, to allow "harmless" cheats
    ** [JN] Allow in nightmare and for dead player (can be resurrected).
    if (netgame || gameskill == sk_nightmare)
    {                           // Can't cheat in a net-game, or in nightmare mode
        return (false);
    }
    */
    eat = false;
    for (i = 0; Cheats[i].func != NULL; i++)
    {
        if (cht_CheckCheat(Cheats[i].seq, key))
        {
            Cheats[i].func(&players[consoleplayer], &Cheats[i]);
            // [JN] Do not play sound after typing just "ID".
            if (Cheats[i].func != CheatWaitFunc)
            {
                S_StartSound(NULL, sfx_dorcls);
            }
        }
    }
    return (eat);
}

//------------------------------------------------------------------------------
//
// CHEAT FUNCTIONS
//
//------------------------------------------------------------------------------

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
        S_StartSound(NULL, sfx_telept);

        // [crispy] fix reviving as "zombie" if god mode was already enabled
        if (player->mo)
        {
            player->mo->health = MAXHEALTH;
        }
        player->health = MAXHEALTH;
        player->lookdir = 0;
    }

    player->cheats ^= CF_GODMODE;
    CT_SetMessage(player, DEH_String(player->cheats & CF_GODMODE ?
                  TXT_CHEATGODON : TXT_CHEATGODOFF), false, NULL);
    SB_state = -1;
    player->cheatTics = 1;
}

static void CheatWeaponsFunc (player_t *player, Cheat_t *cheat)
{
    int i;

    FULL_CHEAT_CHECK;
    player->armorpoints = 200;
    player->armortype = 2;
    if (!player->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for (i = 0; i < NUMWEAPONS - 1; i++)
    {
        player->weaponowned[i] = true;
    }
    if (gamemode == shareware)
    {
        player->weaponowned[wp_skullrod] = false;
        player->weaponowned[wp_phoenixrod] = false;
        player->weaponowned[wp_mace] = false;
    }
    for (i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATWEAPONS), false, NULL);
    player->cheatTics = 1;
}

static void CheatWeapKeysFunc (player_t *player, Cheat_t *cheat)
{
    int i;

    FULL_CHEAT_CHECK;
    player->armorpoints = 200;
    player->armortype = 2;
    if (!player->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for (i = 0; i < NUMWEAPONS - 1; i++)
    {
        player->weaponowned[i] = true;
    }
    if (gamemode == shareware)
    {
        player->weaponowned[wp_skullrod] = false;
        player->weaponowned[wp_phoenixrod] = false;
        player->weaponowned[wp_mace] = false;
    }
    for (i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    player->keys[key_yellow] = true;
    player->keys[key_green] = true;
    player->keys[key_blue] = true;
    playerkeys = 7;             // Key refresh flags
    CT_SetMessage(player, DEH_String(TXT_CHEATWEAPKEYS), false, NULL);
    player->cheatTics = 1;
}

static void CheatChoppersFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    player->weaponowned[wp_gauntlets] = true;
    player->powers[pw_invulnerability] = true;
    CT_SetMessage(player, DEH_String(TXT_CHOPPERS), false, NULL);
}

static void CheatKeysFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    player->keys[key_yellow] = true;
    player->keys[key_green] = true;
    player->keys[key_blue] = true;
    playerkeys = 7;             // Key refresh flags
    CT_SetMessage(player, DEH_String(TXT_CHEATKEYS), false, NULL);
    player->cheatTics = 1;
}

static void CheatNoClipFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_NOCLIP;
    CT_SetMessage(player, DEH_String(player->cheats & CF_NOCLIP ?
                  TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), false, NULL);
    player->cheatTics = 1;
}

static void CheatWarpFunc (player_t *player, Cheat_t *cheat)
{
    char args[2];
    int episode;
    int map;

    // [JN] Safe to use IDCLEV/ENGAGE while demo playback.
    SAFE_CHEAT_CHECK;

    cht_GetParam(cheat->seq, args);

    episode = args[0] - '0';
    map = args[1] - '0';
    if (D_ValidEpisodeMap(heretic, gamemode, episode, map))
    {
        // [crisp] allow IDCLEV during demo playback and warp to the requested map
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
            G_DeferedInitNew(gameskill, episode, map);
            player->cheatTics = 1;
        }
    }
}

static void CheatArtifact1Func (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS1), false, NULL);
}

static void CheatArtifact2Func (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS2), false, NULL);
}

static void CheatArtifact3Func (player_t *player, Cheat_t *cheat)
{
    char args[2];
    int i;
    int j;
    int type;
    int count;

    FULL_CHEAT_CHECK;

    cht_GetParam(cheat->seq, args);
    type = args[0] - 'a' + 1;
    count = args[1] - '0';
    if (type == 26 && count == 0)
    {                           // All artifacts
        for (i = arti_none + 1; i < NUMARTIFACTS; i++)
        {
            if (gamemode == shareware 
             && (i == arti_superhealth || i == arti_teleport))
            {
                continue;
            }
            for (j = 0; j < 16; j++)
            {
                P_GiveArtifact(player, i, NULL);
            }
        }
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS3), false, NULL);
    }
    else if (type > arti_none && type < NUMARTIFACTS
             && count > 0 && count < 10)
    {
        if (gamemode == shareware
        && (type == arti_superhealth || type == arti_teleport))
        {
            CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTSFAIL), false, NULL);
            return;
        }
        for (i = 0; i < count; i++)
        {
            P_GiveArtifact(player, type, NULL);
        }
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS3), false, NULL);
    }
    else
    {                           // Bad input
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTSFAIL), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatArtifactAllFunc (player_t *player, Cheat_t *cheat)
{
    int i;
    int j;

    FULL_CHEAT_CHECK;

    for (i = arti_none + 1; i < NUMARTIFACTS; i++)
    {
        for (j = 0; j < 16; j++)
        {
            P_GiveArtifact(player, i, NULL);
        }
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS4), false, NULL);
    player->cheatTics = 1;
}

static void CheatPowerFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    if (player->powers[pw_weaponlevel2])
    {
        player->powers[pw_weaponlevel2] = 0;
        CT_SetMessage(player, DEH_String(TXT_CHEATPOWEROFF), false, NULL);
    }
    else
    {
        P_UseArtifact(player, arti_tomeofpower);
        CT_SetMessage(player, DEH_String(TXT_CHEATPOWERON), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatHealthFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    if (player->chickenTics)
    {
        player->health = player->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        player->health = player->mo->health = MAXHEALTH;
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATHEALTH), false, NULL);
    player->cheatTics = 1;
}

static void CheatChickenFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;

    if (player->chickenTics)
    {
        if (P_UndoPlayerChicken(player))
        {
            CT_SetMessage(player, DEH_String(TXT_CHEATCHICKENOFF), false, NULL);
        }
    }
    else if (P_ChickenMorphPlayer(player))
    {
        CT_SetMessage(player, DEH_String(TXT_CHEATCHICKENON), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatMassacreFunc (player_t *player, Cheat_t *cheat)
{
    FULL_CHEAT_CHECK;
    P_Massacre();
    CT_SetMessage(player, DEH_String(TXT_CHEATMASSACRE), false, NULL);
    player->cheatTics = 1;
}

static int SB_Cheat_Massacre (boolean explode)
{
    int killcount = 0;
    int amount;
    mobj_t *mo;
    thinker_t *think;

    for (think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        if (think->function != P_MobjThinker)
        {                       // Not a mobj thinker
            continue;
        }
        mo = (mobj_t *) think;
        amount = explode ? 10000 : mo->health;
        if ((mo->flags & MF_COUNTKILL) && (mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, amount);
            killcount++;
        }
    }
    return killcount;
}

static void CheatTNTEMFunc (player_t *player, Cheat_t *cheat)
{
    static char buf[52];
    int killcount;
    
    FULL_CHEAT_CHECK;
    
    killcount = SB_Cheat_Massacre(true);

    M_snprintf(buf, sizeof(buf), "MONSTERS KILLED: %d", killcount);

    CT_SetMessage(player, buf, false, NULL);
    player->cheatTics = 1;
}

static void CheatKILLEMFunc (player_t *player, Cheat_t *cheat)
{
    static char buf[52];
    int killcount;

    FULL_CHEAT_CHECK;
    
    killcount = SB_Cheat_Massacre(false);

    M_snprintf(buf, sizeof(buf), "MONSTERS KILLED: %d", killcount);

    CT_SetMessage(player, buf, false, NULL);
    player->cheatTics = 1;
}

static void CheatIDMUSFunc (player_t *player, Cheat_t *cheat)
{
    char buf[3];
    int  musnum;

    // [JN] Harmless cheat, always allow.

    // [JN] Prevent impossible selection.
    const int maxnum = gamemode == retail     ? 47 :  // 5 episodes
                       gamemode == registered ? 26 :  // 3 episodes
                                                 8 ;  // 1 episode (shareware)

    cht_GetParam(cheat->seq, buf);
    musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');

    if (((buf[0]-'1')*9 + buf[1]-'1') > maxnum)
    {
        CT_SetMessage(player, DEH_String(TXT_NOMUS), false, NULL);
    }
    else
    {
        S_StartSong(musnum, true);
        // [JN] jff 3/17/98 remember idmus number for restore
        idmusnum = musnum;
        CT_SetMessage(player, DEH_String(TXT_MUS), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatAMapFunc (player_t *player, Cheat_t *cheat)
{
    // [JN] Harmless cheat, always allow.
    ravmap_cheating = (ravmap_cheating + 1) % 3;
    player->cheatTics = 1;
}

static void CheatIDMYPOSFunc (player_t *player, Cheat_t *cheat)
{
    static char buf[52];

    // [JN] Harmless cheat, always allow.
    M_snprintf(buf, sizeof(buf), "ANG=0X%X;X,Y=(0X%X,0X%X)",
               players[displayplayer].mo->angle,
               players[displayplayer].mo->x,
               players[displayplayer].mo->y);
    CT_SetMessage(player, buf, false, NULL);
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
