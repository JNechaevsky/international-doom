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
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//


#include <stdio.h>
#include <ctype.h>  // isdigit

#include "i_swap.h" // [crispy] SHORT()
#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_random.h"
#include "w_wad.h"
#include "d_main.h"
#include "deh_main.h"
#include "deh_misc.h"
#include "g_game.h"
#include "p_local.h"
#include "m_menu.h"
#include "s_sound.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_englsh.h"
#include "v_trans.h"
#include "ct_chat.h"
#include "st_bar.h"
#include "am_map.h"

#include "id_vars.h"
#include "id_func.h"


// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13


// Number of status faces.
#define ST_NUMPAINFACES     5
#define ST_NUMSTRAIGHTFACES 3
#define ST_NUMTURNFACES     2
#define ST_NUMSPECIALFACES  3
#define ST_MUCHPAIN         20

#define ST_FACESTRIDE       (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)
#define ST_NUMEXTRAFACES    2
#define ST_NUMFACES         (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET       (ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET       (ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET   (ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET    (ST_EVILGRINOFFSET + 1)
#define ST_GODFACE          (ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE         (ST_GODFACE+1)

#define ST_EVILGRINCOUNT        (TICRATE*2)
#define ST_STRAIGHTFACECOUNT    (TICRATE/2)
#define ST_TURNCOUNT            (TICRATE)
#define ST_OUCHCOUNT            (TICRATE)
#define ST_RAMPAGEDELAY         (TICRATE*2)


// graphics are drawn to a backing screen and blitted to the real screen
static pixel_t *st_backing_screen;

// main player in game
static player_t *plyr; 

// lump number for PLAYPAL
static int lu_palette;
int st_palette = 0;

// used for evil grin
static int oldweaponsowned[NUMWEAPONS]; 

static patch_t *sbar;                // main bar background
static patch_t *sbarr;               // main bar right, for doom 1.0
static patch_t *tallnum[10];         // 0-9, tall numbers
static patch_t *tallpercent;         // tall % sign
static patch_t *tallminus;           // [JN] "minus" symbol
static patch_t *shortnum_y[10];      // 0-9, short, yellow numbers
static patch_t *shortnum_g[10];      // 0-9, short, gray numbers
static patch_t *keys[NUMCARDS];      // 3 key-cards, 3 skulls
static patch_t *faces[ST_NUMFACES];  // face status patches
static patch_t *faceback[MAXPLAYERS];// [JN] killough 3/7/98: make array
static patch_t *armsbg;              // ARMS background

// [crispy] blinking key or skull in the status bar
int st_keyorskull[3];

static int st_fragscount; // number of frags so far in deathmatch
static int st_oldhealth = -1;  // used to use appopriately pained face
static int st_facecount = 0;  // count until face changes
static int st_faceindex = 1;  // current face index, used by w_faces
static int st_randomnumber; // a random number per tick
static int faceindex; // [crispy] fix status bar face hysteresis

// [JN] Condition to redraw status bar background. 
boolean st_fullupdate = true;

// [JN] Arrays for holding buffered background of status bar elements.
static int ammo_bg[5], hlth_bg[5], frgs_bg[5], face_bg[5], armr_bg[5];
static int keys_bg[5], amoc_bg[5], amom_bg[5], disk_bg[5];

cheatseq_t cheat_wait = CHEAT("id", 0);
cheatseq_t cheat_mus = CHEAT("idmus", 2);
cheatseq_t cheat_god = CHEAT("iddqd", 0);
cheatseq_t cheat_ammo = CHEAT("idkfa", 0);
cheatseq_t cheat_ammonokey = CHEAT("idfa", 0);
cheatseq_t cheat_keys = CHEAT("idka", 0);
cheatseq_t cheat_noclip = CHEAT("idspispopd", 0);
cheatseq_t cheat_commercial_noclip = CHEAT("idclip", 0);
cheatseq_t cheat_choppers = CHEAT("idchoppers", 0);
cheatseq_t cheat_clev = CHEAT("idclev", 2);
cheatseq_t cheat_mypos = CHEAT("idmypos", 0);
// [JN] IDDT cheat, moved from am_map.c
cheatseq_t cheat_amap = CHEAT("iddt", 0);

// [crispy] new cheats
static cheatseq_t cheat_massacre1 = CHEAT("tntem", 0);
static cheatseq_t cheat_massacre2 = CHEAT("killem", 0);
static cheatseq_t cheat_freeze = CHEAT("freeze", 0);
static cheatseq_t cheat_notarget = CHEAT("notarget", 0);
static cheatseq_t cheat_buddha = CHEAT("buddha", 0);

cheatseq_t cheat_powerup[7] =
{
    CHEAT("idbeholdv", 0),
    CHEAT("idbeholds", 0),
    CHEAT("idbeholdi", 0),
    CHEAT("idbeholdr", 0),
    CHEAT("idbeholda", 0),
    CHEAT("idbeholdl", 0),
    CHEAT("idbehold", 0),
};

// -----------------------------------------------------------------------------
// cht_CheckCheatSP
// [crispy] restrict cheat usage
// -----------------------------------------------------------------------------
static const inline int cht_CheckCheatSP (cheatseq_t *cht, char key)
{
    if (!cht_CheckCheat(cht, key))
    {
        return false;
    }
    else
    if (!singleplayer)
    {
        // plyr->message = "Cheater!";
        return false;
    }
    return true;
}

// [JN] Versions prior 1.4 does not have STTMINUS patch.
// If case of using such versions, drawing negative health is not possible.
static boolean no_sttminus = false;

// -----------------------------------------------------------------------------
// WeaponAvailable
// [crispy] only give available weapons
// -----------------------------------------------------------------------------
static boolean WeaponAvailable (int w)
{
    if (w < 0 || w >= NUMWEAPONS)
    {
        return false;
    }

    if (w == wp_supershotgun && !havessg)
    {
        return false;
    }

    if ((w == wp_bfg || w == wp_plasma) && gamemode == shareware)
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// GiveBackpack
// [crispy] give or take backpack
// -----------------------------------------------------------------------------

static void GiveBackpack (boolean give)
{
    int i;
    
    if (give && !plyr->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            plyr->maxammo[i] *= 2;
        }
        plyr->backpack = true;
    }
    else
    if (!give && plyr->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            plyr->maxammo[i] /= 2;
        }
        plyr->backpack = false;
    }
}

// -----------------------------------------------------------------------------
// [crispy] adapted from boom202s/M_CHEAT.C:467-498
// -----------------------------------------------------------------------------

static const int ST_cheat_massacre (boolean explode)
{
    int killcount = 0;
    thinker_t *th;

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function.acp1 == (actionf_p1)P_MobjThinker)
        {
            mobj_t *mo = (mobj_t *)th;
            const int amount = explode ? 10000 : mo->health;

            if (mo->flags & MF_COUNTKILL || mo->type == MT_SKULL)
            {
                if (mo->health > 0)
                {
                    P_DamageMobj(mo, NULL, NULL, amount);
                    killcount++;
                }
                if (mo->type == MT_PAIN)
                {
                    A_PainDie(mo);
                    P_SetMobjState(mo, S_PAIN_DIE6);
                }
            }
        }
    }

    // [crispy] disable brain spitters
    // numbraintargets = -1;

    return killcount;
}

// -----------------------------------------------------------------------------
// ST_Responder
// Respond to keyboard input events, intercept cheats.
// -----------------------------------------------------------------------------

boolean ST_Responder (event_t *ev)
{
    int i;

    // if a user keypress...
    if (ev->type == ev_keydown)
    {
        // [JN] Allow cheats in Nightmare, as it was done in old released
        // version of Chocorenderlimits, but still disallow in netgame.
        if (!netgame /*&& gameskill != sk_nightmare*/)
        {
            // [JN] If user types "id", activate timer to prevent
            // other than typing actions in G_Responder.
            if (cht_CheckCheat(&cheat_wait, ev->data2))
            {
                plyr->cheatTics = TICRATE * 2;
            }

            // 'dqd' cheat for toggleable god mode
            if (cht_CheckCheatSP(&cheat_god, ev->data2))
            {
                // [crispy] dead players are first respawned at the current position
                mapthing_t mt = {0};

                if (plyr->playerstate == PST_DEAD)
                {
                    angle_t an;

                    mt.x = plyr->mo->x >> FRACBITS;
                    mt.y = plyr->mo->y >> FRACBITS;
                    mt.angle = (plyr->mo->angle + ANG45/2)*(uint64_t)45/ANG45;
                    mt.type = consoleplayer + 1;
                    P_SpawnPlayer(&mt);

                    // [crispy] spawn a teleport fog
                    an = plyr->mo->angle >> ANGLETOFINESHIFT;
                    P_SpawnMobj(plyr->mo->x+20*finecosine[an], plyr->mo->y+20*finesine[an], plyr->mo->z, MT_TFOG);
                    S_StartSound(NULL, sfx_telept);

                    // [crispy] fix reviving as "zombie" if god mode was already enabled
                    if (plyr->mo)
                    {
                        plyr->mo->health = deh_god_mode_health;
                    }
                    plyr->health = deh_god_mode_health;
                }

                plyr->cheats ^= CF_GODMODE;
                if (plyr->cheats & CF_GODMODE)
                {
                    if (plyr->mo)
                    {
                        plyr->mo->health = 100;
                    }
                    plyr->health = deh_god_mode_health;
                    CT_SetMessage(plyr, DEH_String(STSTR_DQDON), false, NULL);
                }
                else 
                {
                    CT_SetMessage(plyr, DEH_String(STSTR_DQDOFF), false, NULL);
                }
                plyr->cheatTics = 1;
            }
            // 'fa' cheat for killer fucking arsenal
            else if (cht_CheckCheatSP(&cheat_ammonokey, ev->data2))
            {
                plyr->armorpoints = deh_idfa_armor;
                plyr->armortype = deh_idfa_armor_class;

                // [crispy] give backpack
                GiveBackpack(true);

                for (i=0;i<NUMWEAPONS;i++)
                {
                    if (WeaponAvailable(i)) // [crispy] only give available weapons
                    {
                        plyr->weaponowned[i] = true;
                    }
                }
                for (i=0;i<NUMAMMO;i++)
                {
                    plyr->ammo[i] = plyr->maxammo[i];
                }

                plyr->cheatTics = 1;
                CT_SetMessage(plyr, DEH_String(STSTR_FAADDED), false, NULL);
            }
            // 'kfa' cheat for key full ammo
            else if (cht_CheckCheatSP(&cheat_ammo, ev->data2))
            {
                plyr->armorpoints = deh_idkfa_armor;
                plyr->armortype = deh_idkfa_armor_class;

                // [crispy] give backpack
                GiveBackpack(true);

                for (i = 0 ; i < NUMWEAPONS ; i++)
                {
                    if (WeaponAvailable(i)) // [crispy] only give available weapons
                    {
                        plyr->weaponowned[i] = true;
                    }
                }
                for (i = 0 ; i < NUMAMMO ; i++)
                {
                    plyr->ammo[i] = plyr->maxammo[i];
                }
                for (i = 0 ; i < NUMCARDS ; i++)
                {
                    plyr->cards[i] = true;
                }

                plyr->cheatTics = 1;
                CT_SetMessage(plyr, DEH_String(STSTR_KFAADDED), false, NULL);
            }
            // [JN] 'ka' for keys only
            else if (cht_CheckCheatSP(&cheat_keys, ev->data2))
            {
                for (i = 0; i < NUMCARDS ; i++)
                {
                    plyr->cards[i] = true;
                }

                plyr->cheatTics = 1;
                CT_SetMessage(plyr, DEH_String(STSTR_KAADDED), false, NULL);
            }
            // 'mus' cheat for changing music
            else if (cht_CheckCheat(&cheat_mus, ev->data2))
            {
                char buf[3];
                int  musnum;

                CT_SetMessage(plyr, DEH_String(STSTR_MUS), false, NULL);
                cht_GetParam(&cheat_mus, buf);

                // Note: The original v1.9 had a bug that tried to play back
                // the Doom II music regardless of gamemode.  This was fixed
                // in the Ultimate Doom executable so that it would work for
                // the Doom 1 music as well.
                // [JN] Fixed: using a proper IDMUS selection for shareware 
                // and registered game versions.
                if (gamemode == commercial /*|| gameversion < exe_ultimate*/)
                {
                    musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;

                    // [crispy] prevent crash with IDMUS00
                    if (musnum < mus_runnin || musnum >= NUMMUSIC)
                    {
                        CT_SetMessage(plyr, DEH_String(STSTR_NOMUS), false, NULL);
                    }
                    else
                    {
                        S_ChangeMusic(musnum, 1);
                        // [JN] jff 3/17/98 remember idmus number for restore
                        idmusnum = musnum;
                    }
                }
                else
                {
                    musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');

                    // [crispy] prevent crash with IDMUS0x or IDMUSx0
                    if (musnum < mus_e1m1 || musnum >= mus_runnin
                    // [crispy] support dedicated music tracks for the 4th episode
                    || S_music[musnum].lumpnum == -1)
                    {
                        CT_SetMessage(plyr, DEH_String(STSTR_NOMUS), false, NULL);
                    }
                    else
                    {
                        S_ChangeMusic(musnum, 1);
                        // [JN] jff 3/17/98 remember idmus number for restore
                        idmusnum = musnum;
                    }
                }

                plyr->cheatTics = 1;
            }
            // Noclip cheat.
            // For Doom 1, use the idspipsopd cheat; for all others, use idclip            
            // [crispy] allow both idspispopd and idclip cheats in all gamemissions
            else 
            if (cht_CheckCheatSP(&cheat_noclip, ev->data2)
            || (cht_CheckCheatSP(&cheat_commercial_noclip, ev->data2)))
            {	
                plyr->cheats ^= CF_NOCLIP;

                if (plyr->cheats & CF_NOCLIP)
                {
                    plyr->mo->flags |= MF_NOCLIP;
                    CT_SetMessage(plyr, DEH_String(STSTR_NCON), false, NULL);
                }
                else
                {
                    plyr->mo->flags &= ~MF_NOCLIP;
                    CT_SetMessage(plyr, DEH_String(STSTR_NCOFF), false, NULL);
                }

                plyr->cheatTics = 1;
            }

            // 'behold?' power-up cheats
            for (i = 0 ; i < 6 ; i++)
            {
                if (cht_CheckCheatSP(&cheat_powerup[i], ev->data2))
                {
                    if (!plyr->powers[i])
                    {
                        P_GivePower( plyr, i);
                    }
                    else if (i!=pw_strength)
                    {
                        plyr->powers[i] = 1;
                    }
                    else
                    {
                        plyr->powers[i] = 0;
                    }

                    plyr->cheatTics = 1;
                    CT_SetMessage(plyr, DEH_String(STSTR_BEHOLDX), false, NULL);
                }
            }
            // 'behold' power-up menu
            if (cht_CheckCheatSP(&cheat_powerup[6], ev->data2))
            {
                CT_SetMessage(plyr, DEH_String(STSTR_BEHOLD), false, NULL);
            }
            // 'choppers' invulnerability & chainsaw
            else if (cht_CheckCheatSP(&cheat_choppers, ev->data2))
            {
                plyr->weaponowned[wp_chainsaw] = true;
                plyr->powers[pw_invulnerability] = true;
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, DEH_String(STSTR_CHOPPERS), false, NULL);
            }
            // 'mypos' for player position
            else if (cht_CheckCheat(&cheat_mypos, ev->data2))
            {
                static char buf[52];

                M_snprintf(buf, sizeof(buf), "ang=0x%x;x,y=(0x%x,0x%x)",
                           players[displayplayer].mo->angle,
                           players[displayplayer].mo->x,
                           players[displayplayer].mo->y);
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, buf, false, NULL);
            }
            // [JN] IDDT cheating, moved from am_map.c
            else if ((!deathmatch || gameversion <= exe_doom_1_8)
            && cht_CheckCheat(&cheat_amap, ev->data2))
            {
                iddt_cheating = (iddt_cheating + 1) % 3;
                plyr->cheatTics = 1;
            }
            // [crispy] implement Boom's "tntem" cheat
            else if (cht_CheckCheatSP(&cheat_massacre1, ev->data2))
            {
                static char buf[52];
                const int killcount = ST_cheat_massacre(true);

                M_snprintf(buf, sizeof(buf), "Monsters killed: %d", killcount);
                
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, buf, false, NULL);
            }
            // [JN] implement MBF's "killem" cheat, which kills, not explodes enemies.
            else if (cht_CheckCheatSP(&cheat_massacre2, ev->data2))
            {
                static char buf[52];
                const int killcount = ST_cheat_massacre(false);

                M_snprintf(buf, sizeof(buf), "Monsters killed: %d", killcount);
                
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, buf, false, NULL);
            }
            // [JN] CRL - Freeze mode.
            else if (cht_CheckCheatSP(&cheat_freeze, ev->data2))
            {
                crl_freeze ^= 1;
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, crl_freeze ?
                              ID_FREEZE_ON : ID_FREEZE_OFF, false, NULL);
            }
            // [JN] Implement Woof's "notarget" cheat.
            else if (cht_CheckCheatSP(&cheat_notarget, ev->data2))
            {
                plyr->cheats ^= CF_NOTARGET;
                P_ForgetPlayer(plyr);
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, plyr->cheats & CF_NOTARGET ?
                               ID_NOTARGET_ON : ID_NOTARGET_OFF, false, NULL);
            }
            // [JN] Implement Woof's "buddha" cheat.
            else if (cht_CheckCheatSP(&cheat_buddha, ev->data2))
            {
                plyr->cheats ^= CF_BUDDHA;
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, plyr->cheats & CF_BUDDHA ?
                               ID_BUDDHA_ON : ID_BUDDHA_OFF, false, NULL);
            }
        }

        // 'clev' change-level cheat
        if (!netgame && cht_CheckCheat(&cheat_clev, ev->data2))
        {
            char  buf[3];
            int   epsd;
            int   map;

            cht_GetParam(&cheat_clev, buf);

            if (gamemode == commercial)
            {
                epsd = 0;
                map = (buf[0] - '0')*10 + buf[1] - '0';
            }
            else
            {
                epsd = buf[0] - '0';
                map = buf[1] - '0';

                // Chex.exe always warps to episode 1.

                if (gameversion == exe_chex)
                {
                    if (epsd > 1)
                    {
                        epsd = 1;
                    }
                    if (map > 5)
                    {
                        map = 5;
                    }
                }
            }

            // Catch invalid maps.
            if (gamemode != commercial)
            {
                if (epsd < 1)
                {
                    return false;
                }
                if (epsd > 4)
                {
                    // [crispy] Sigil
                    if (!(sigil && epsd == 5) &&
                        !(sigil2 && epsd == 6))
                    return false;
                }
                if (epsd == 4 && gameversion < exe_ultimate)
                {
                    return false;
                }
                if (map < 1)
                {
                    return false;
                }
                if (map > 9)
                {
                    return false;
                }
            }
            else
            {
                if (map < 1)
                {
                    return false;
                }
                if (map > 40)
                {
                    return false;
                }
            }

            // So be it.
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

                return true;
            }
            else
            {
                plyr->cheatTics = 1;
                CT_SetMessage(plyr, DEH_String(STSTR_CLEV), false, NULL);
                G_DeferedInitNew(gameskill, epsd, map);
            }
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// ST_calcPainOffset
// -----------------------------------------------------------------------------

static const int ST_calcPainOffset (void)
{
    int        health;
    static int lastcalc;
    static int oldhealth = -1;

    health = plyr->health > 100 ? 100 : plyr->health;

    if (health != oldhealth)
    {
        lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
        oldhealth = health;
    }

    return lastcalc;
}

// -----------------------------------------------------------------------------
// ST_updateFaceWidget
// This is a not-very-pretty routine which handles the face states 
// and their timing. // the precedence of expressions is:
//  dead > evil grin > turned head > straight ahead
// -----------------------------------------------------------------------------

static void ST_updateFaceWidget (void)
{
    int         i;
    static int  lastattackdown = -1;
    static int  priority = 0;
    angle_t     badguyangle;
    angle_t     diffang;
    boolean     doevilgrin;

    // [crispy] fix status bar face hysteresis
    int painoffset = ST_calcPainOffset();
    // [crispy] no evil grin or rampage face in god mode
    const boolean invul = (plyr->cheats & CF_GODMODE) || plyr->powers[pw_invulnerability];

    if (priority < 11)
    {
        // dead
        if (!plyr->health)
        {
            priority = 10;
            painoffset = 0;
            faceindex = ST_DEADFACE;
            st_facecount = 1;
        }
    }

    if (priority < 10)
    {
        // [JN] invulnerability (moved up here)
        if (invul)
        {
            priority = 9;
            painoffset = 0;
            faceindex = ST_GODFACE;
            st_facecount = 1;
        }
    }

    if (priority < 9)
    {
        if (plyr->bonuscount)
        {
            // picking up bonus
            doevilgrin = false;

            for (i = 0 ; i < NUMWEAPONS ; i++)
            {
                if (oldweaponsowned[i] != plyr->weaponowned[i])
                {
                    doevilgrin = true;
                    oldweaponsowned[i] = plyr->weaponowned[i];
                }
            }
            // [crispy] no evil grin in god mode
            if (doevilgrin && !invul)
            {
                // evil grin if just picked up weapon
                priority = 8;
                st_facecount = ST_EVILGRINCOUNT;
                faceindex = ST_EVILGRINOFFSET;
            }
        }
    }

    if (priority < 8)
    {
        if (plyr->damagecount && plyr->attacker && plyr->attacker != plyr->mo)
        {
            // being attacked
            priority = 7;

            // [crispy] show "Ouch Face" as intended
            if (st_oldhealth - plyr->health > ST_MUCHPAIN)
            {
                // [crispy] raise "Ouch Face" priority
                priority = 8;
                st_facecount = ST_TURNCOUNT;
                faceindex = ST_OUCHOFFSET;
            }
            else
            {
                badguyangle = R_PointToAngle2(plyr->mo->x,
                                              plyr->mo->y,
                                              plyr->attacker->x,
                                              plyr->attacker->y);

                if (badguyangle > plyr->mo->angle)
                {
                    // whether right or left
                    diffang = badguyangle - plyr->mo->angle;
                    i = diffang > ANG180; 
                }
                else
                {
                    // whether left or right
                    diffang = plyr->mo->angle - badguyangle;
                    i = diffang <= ANG180; 
                } // confusing, aint it?

                st_facecount = ST_TURNCOUNT;

                if (diffang < ANG45)
                {
                    // head-on    
                    faceindex = ST_RAMPAGEOFFSET;
                }
                else if (i ^ gp_flip_levels) // [Nugget] Flip levels
                {
                    // turn face right
                    faceindex = ST_TURNOFFSET;
                }
                else
                {
                    // turn face left
                    faceindex = ST_TURNOFFSET+1;
                }
            }
        }
    }

    if (priority < 7)
    {
        // getting hurt because of your own damn stupidity
        if (plyr->damagecount)
        {
            // [crispy] show "Ouch Face" as intended
            if (st_oldhealth - plyr->health > ST_MUCHPAIN)
            {
                priority = 7;
                st_facecount = ST_TURNCOUNT;
                faceindex = ST_OUCHOFFSET;
            }
            else
            {
                priority = 6;
                st_facecount = ST_TURNCOUNT;
                faceindex = ST_RAMPAGEOFFSET;
            }
        }
    }

    if (priority < 6)
    {
        // rapid firing
        if (plyr->attackdown)
        {
            if (lastattackdown==-1)
            {
                lastattackdown = ST_RAMPAGEDELAY;
            }
            // [crispy] no rampage face in god mode
            else if (!--lastattackdown && !invul)
            {
                priority = 5;
                faceindex = ST_RAMPAGEOFFSET;
                st_facecount = 1;
                lastattackdown = 1;
            }
        }
        else
        {
            lastattackdown = -1;
        }
    }

    // [JN] Processed above to fix status bar face hysteresis
    // while getting hurt/ouch face and going into invulnerability state.
    /*
    if (priority < 5)
    {
        // invulnerability
        if (invul)
        {
            priority = 4;

            painoffset = 0;
            faceindex = ST_GODFACE;
            st_facecount = 1;
        }
    }
    */

    // look left or look right if the facecount has timed out
    if (!st_facecount)
    {
        faceindex = st_randomnumber % 3;
        st_facecount = ST_STRAIGHTFACECOUNT;
        priority = 0;
    }

    st_facecount--;

    // [crispy] fix status bar face hysteresis
    st_faceindex = painoffset + faceindex;
}

// -----------------------------------------------------------------------------
// ST_doPaletteStuff
// -----------------------------------------------------------------------------

void ST_doPaletteStuff (void)
{
    int palette;
#ifndef CRISPY_TRUECOLOR
    byte*	pal;
#endif
    int bzc;
    int cnt = plyr->damagecount;

    if (plyr->powers[pw_strength])
    {
        // slowly fade the berzerk out
        bzc = 12 - (plyr->powers[pw_strength]>>6);

        if (bzc > cnt)
        {
            cnt = bzc;
        }
    }

    if (cnt)
    {
        palette = (cnt+7)>>3;

        if (palette >= NUMREDPALS)
        {
            palette = NUMREDPALS-1;
        }

        palette += STARTREDPALS;
    }
    else if (plyr->bonuscount)
    {
        palette = (plyr->bonuscount+7)>>3;

        // [JN] Fix missing first bonus palette index
        // by sudstracting -1 from STARTBONUSPALS, not NUMBONUSPALS.
        if (palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS;
        }

        palette += STARTBONUSPALS-1;
    }
    else if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
    {
        palette = RADIATIONPAL;
    }
    else
    {
        palette = 0;
    }

    // In Chex Quest, the player never sees red.  Instead, the
    // radiation suit palette is used to tint the screen green,
    // as though the player is being covered in goo by an
    // attacking flemoid.

    if (gameversion == exe_chex
    &&  palette >= STARTREDPALS && palette < STARTREDPALS + NUMREDPALS)
    {
        palette = RADIATIONPAL;
    }

    if (palette != st_palette)
    {
        st_palette = palette;
#ifndef CRISPY_TRUECOLOR
	pal = (byte *) W_CacheLumpNum (lu_palette, PU_CACHE)+palette*768;
	I_SetPalette (pal);
#else
	I_SetPalette (palette);
#endif
    }
}

// -----------------------------------------------------------------------------
// ST_doSmoothPaletteStuff
// [JN] Smooth palette handling.
// -----------------------------------------------------------------------------

#ifdef CRISPY_TRUECOLOR
static void ST_doSmoothPaletteStuff (void)
{
    int palette;
    int red = plyr->damagecount;
    int yel = plyr->bonuscount;
    int grn = plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8;

    if (plyr->powers[pw_strength])
    {
        // [JN] Smoother berserk fading.
        int bzc = (12 << 1) - (plyr->powers[pw_strength] >> 6);
        
        // [JN] Berserk is almost faded out. Handle final fading differently:
        if (bzc > 0 && bzc < 13)
        {
            if (plyr->powers[pw_ironfeet])
            {
                // If have a radiation suit, switch to the green palette immediately.
                bzc = 0;
            }
            else
            {
                // Otherwise, perform a smooth and fast final fade.
                bzc = 780 - plyr->powers[pw_strength];
            }
        }

        if (bzc > red)
        {
            red = bzc;
        }
    }

    if (red)
    {
        palette = 1;
        red_pane_alpha = MIN(red * PAINADD, 226);   // 226 pane alpha max
    }
    else if (yel)
    {
        palette = 9;
        yel_pane_alpha = MIN(yel * BONUSADD, 127);  // 127 pane alpha max
    }
    else if (grn)
    {
        palette = RADIATIONPAL;
    }
    else
    {
        palette = 0;
    }

    // In Chex Quest, the player never sees red.  Instead, the
    // radiation suit palette is used to tint the screen green,
    // as though the player is being covered in goo by an
    // attacking flemoid.

    if (gameversion == exe_chex
    &&  palette >= STARTREDPALS && palette < STARTREDPALS + NUMREDPALS)
    {
        palette = RADIATIONPAL;
    }

    if (palette != st_palette || red || yel)
    {
        st_palette = palette;
        I_SetPalette (palette);
    }
}
#endif

// -----------------------------------------------------------------------------
// ST_UpdateFragsCounter
// [JN] Updated to int type, allowing to show frags of any player.
// -----------------------------------------------------------------------------

static const int ST_UpdateFragsCounter (const int playernum, const boolean big_values)
{
    st_fragscount = 0;

    for (int i = 0 ; i < MAXPLAYERS ; i++)
    {
        if (i != playernum)
        {
            st_fragscount += players[playernum].frags[i];
        }
        else
        {
            st_fragscount -= players[playernum].frags[i];
        }
    }
    
    // [JN] Prevent overflow, ST_DrawBigNumber can only draw three 
    // digit number, and status bar fits well only two digits number
    if (!big_values)
    {
        if (st_fragscount > 99)
            st_fragscount = 99;
        if (st_fragscount < -99)
            st_fragscount = -99;
    }

    return st_fragscount;
}

// -----------------------------------------------------------------------------
// ST_Ticker
// -----------------------------------------------------------------------------

void ST_Ticker (void)
{
    // refresh everything if this is him coming back to life
    ST_updateFaceWidget();

    st_randomnumber = M_Random();
    st_oldhealth = plyr->health;

    // [JN] Update CRL_Widgets_t data.
    IDWidget.kills = plyr->killcount;
    IDWidget.extrakills = plyr->extrakillcount;
    IDWidget.totalkills = totalkills;
    IDWidget.items = plyr->itemcount;
    IDWidget.totalitems = totalitems;
    IDWidget.secrets = plyr->secretcount;
    IDWidget.totalsecrets = totalsecret;

    IDWidget.x = plyr->mo->x >> FRACBITS;
    IDWidget.y = plyr->mo->y >> FRACBITS;
    IDWidget.ang = plyr->mo->angle / ANG1;

    if (deathmatch)
    {
        if (playeringame[0])
        {
            IDWidget.frags_g = ST_UpdateFragsCounter(0, true);
        }
        if (playeringame[1])
        {
            IDWidget.frags_i = ST_UpdateFragsCounter(1, true);
        }
        if (playeringame[2])
        {
            IDWidget.frags_b = ST_UpdateFragsCounter(2, true);
        }
        if (playeringame[3])
        {
            IDWidget.frags_r = ST_UpdateFragsCounter(3, true);
        }
    }

    // [JN] Update blinking key or skull timer.
    for (int i = 0 ; i < 3 ; i++)
    {
        if (plyr->tryopen[i])
        {
            plyr->tryopen[i]--;
        }
    }

    // Do red-/gold-shifts from damage/items
#ifndef CRISPY_TRUECOLOR
    ST_doPaletteStuff();
#else
    if (vis_smooth_palette)
    {
        ST_doSmoothPaletteStuff();
    }
    else
    {
        ST_doPaletteStuff();
    }
#endif
}

// -----------------------------------------------------------------------------
// ST_WidgetColor
// [crispy] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

enum
{
    hudcolor_ammo,
    hudcolor_health,
    hudcolor_frags,
    hudcolor_armor
} hudcolor_t;

static byte *ST_WidgetColor (const int i)
{
    if (!st_colored_stbar)
    {
        return NULL;
    }

    switch (i)
    {
        case hudcolor_ammo:
        {
            if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
            {
                return NULL;
            }
            else
            {
                int ammo =  plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
                int fullammo = maxammo[weaponinfo[plyr->readyweapon].ammo];

                if (ammo < fullammo/4)
                    return cr[CR_RED];
                else if (ammo < fullammo/2)
                    return cr[CR_YELLOW];
                else if (ammo <= fullammo)
                    return cr[CR_GREEN];
                else
                    return cr[CR_BLUE2];
            }
            break;
        }
        case hudcolor_health:
        {
            int health = plyr->health;

            // [crispy] Invulnerability powerup and God Mode cheat turn Health values gray
            // [JN] I'm using different health values, represented by crosshair,
            // and thus a little bit different logic.
            if (plyr->cheats & CF_GODMODE || plyr->powers[pw_invulnerability])
                return cr[CR_WHITE];
            else if (health > 100)
                return cr[CR_BLUE2];
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
            int frags = st_fragscount;

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
	    // [crispy] Invulnerability powerup and God Mode cheat turn Armor values gray
	    if (plyr->cheats & CF_GODMODE || plyr->powers[pw_invulnerability])
                return cr[CR_WHITE];
	    // [crispy] color by armor type
	    else if (plyr->armortype >= 2)
                return cr[CR_BLUE2];
	    else if (plyr->armortype == 1)
                return cr[CR_GREEN];
	    else if (plyr->armortype == 0)
                return cr[CR_RED];
            break;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// ST_DrawBigNumber
// [JN] Draws a three digit big red number using STTNUM* graphics.
// -----------------------------------------------------------------------------

static void ST_DrawBigNumber (int val, const int x, const int y, byte *table)
{
    int oldval = val;
    int xpos = x;

    dp_translation = table;

    // [JN] Support for negative values.
    if (val < 0)
    {
        val = -val;
        
        if (-val <= -99)
        {
            val = 99;
        }

        // [JN] Draw minus symbol with respection of digits placement.
        // However, values below -10 requires some correction in "x" placement.
        V_DrawPatch(xpos + (val <= 9 ? 20 : 5) - 4, y, tallminus);
    }
    if (val > 999)
    {
        val = 999;
    }

    if (val > 99)
    {
        V_DrawPatch(xpos - 4, y, tallnum[val / 100]);
    }

    val = val % 100;
    xpos += 14;

    if (val > 9 || oldval > 99)
    {
        V_DrawPatch(xpos - 4, y, tallnum[val / 10]);
    }

    val = val % 10;
    xpos += 14;

    V_DrawPatch(xpos - 4, y, tallnum[val]);
    
    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// ST_DrawPercent
// [JN] Draws big red percent sign.
// -----------------------------------------------------------------------------

static void ST_DrawPercent (const int x, const int y, byte *table)
{
    dp_translation = table;
    V_DrawPatch(x, y, tallpercent);
    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// ST_DrawSmallNumberY
// [JN] Draws a three digit yellow number using STYSNUM* graphics.
// -----------------------------------------------------------------------------

static void ST_DrawSmallNumberY (int val, const int x, const int y)
{
    int oldval = val;
    int xpos = x;

    if (val < 0)
    {
        val = 0;
    }
    if (val > 999)
    {
        val = 999;
    }

    if (val > 99)
    {
        V_DrawPatch(xpos - 4, y, shortnum_y[val / 100]);
    }

    val = val % 100;
    xpos += 4;

    if (val > 9 || oldval > 99)
    {
        V_DrawPatch(xpos - 4, y, shortnum_y[val / 10]);
    }

    val = val % 10;
    xpos += 4;

    V_DrawPatch(xpos - 4, y, shortnum_y[val]);
}

// -----------------------------------------------------------------------------
// ST_DrawSmallNumberG
// [JN] Draws a one digit gray number using STGNUM* graphics.
// -----------------------------------------------------------------------------

static void ST_DrawSmallNumberG (int val, const int x, const int y)
{
    if (val < 0)
    {
        val = 0;
    }
    if (val > 9)
    {
        val = 9;
    }

    V_DrawPatch(x + 4, y, shortnum_g[val]);
}

// -----------------------------------------------------------------------------
// ST_DrawSmallNumberFunc
// [JN] Decides which small font drawing function to use: if weapon is owned,
//      draw using yellow font. Else, draw using gray font.
// -----------------------------------------------------------------------------

static void ST_DrawWeaponNumberFunc (const int val, const int x, const int y, const boolean have_it)
{
    have_it ? ST_DrawSmallNumberY(val, x, y) : ST_DrawSmallNumberG(val, x, y);
}

// -----------------------------------------------------------------------------
// ST_UpdateElementsBackground
// [JN] Use V_CopyRect to draw/update background under elements.
//      This is notably faster than re-drawing entire background.
// -----------------------------------------------------------------------------

static void ST_UpdateElementsBackground (void)
{
    // Ammo
    V_CopyRect(ammo_bg[0], ammo_bg[1], st_backing_screen,
               ammo_bg[2], ammo_bg[3],
               ammo_bg[0], ammo_bg[4]);

    // Health
    V_CopyRect(hlth_bg[0], hlth_bg[1], st_backing_screen,
               hlth_bg[2], hlth_bg[3],
               hlth_bg[0], hlth_bg[4]);

    // ARMS or frags
    V_CopyRect(frgs_bg[0], frgs_bg[1], st_backing_screen,
               frgs_bg[2], frgs_bg[3],
               frgs_bg[0], frgs_bg[4]);

    // Player face
    V_CopyRect(face_bg[0], face_bg[1], st_backing_screen,
               face_bg[2], face_bg[3],
               face_bg[0], face_bg[4]);

    // Armor
    V_CopyRect(armr_bg[0], armr_bg[1], st_backing_screen,
               armr_bg[2], armr_bg[3],
               armr_bg[0], armr_bg[4]);

    // Keys
    V_CopyRect(keys_bg[0], keys_bg[1], st_backing_screen,
               keys_bg[2], keys_bg[3],
               keys_bg[0], keys_bg[4]);

    // Ammo (current)
    V_CopyRect(amoc_bg[0], amoc_bg[1], st_backing_screen,
               amoc_bg[2], amoc_bg[3],
               amoc_bg[0], amoc_bg[4]);

    // Ammo (max)
    V_CopyRect(amom_bg[0], amom_bg[1], st_backing_screen,
               amom_bg[2], amom_bg[3],
               amom_bg[0], amom_bg[4]);

    // Disk icon
    V_CopyRect(disk_bg[0], disk_bg[1], st_backing_screen,
               disk_bg[2], disk_bg[3],
               disk_bg[0], disk_bg[4]);
}

// -----------------------------------------------------------------------------
// ST_Drawer
// [JN] Main drawing function, totally rewritten.
// -----------------------------------------------------------------------------

void ST_Drawer (boolean force)
{
    const boolean st_background_on = 
                    dp_screen_size <= 10 || (automapactive && !automap_overlay);

    if (force)
    {
    // [JN] Wide status bar.
    const int wide_x = dp_screen_size > 12 && (!automapactive || automap_overlay) ?
                       WIDESCREENDELTA : 0;

    plyr = &players[displayplayer];

    // Status bar background.
    if (st_background_on && st_fullupdate)
    {
        V_UseBuffer(st_backing_screen);

        // [crispy] this is our own local copy of R_FillBackScreen() to
        // fill the entire background of st_backing_screen with the bezel pattern,
        // so it appears to the left and right of the status bar in widescreen mode
        {
            byte *src;
            pixel_t *dest;
            const char *name = (gamemode == commercial) ? DEH_String("GRNROCK") : DEH_String("FLOOR7_2");

            src = W_CacheLumpName(name, PU_CACHE);
            dest = st_backing_screen;

            // [crispy] use unified flat filling function
            V_FillFlat(SCREENHEIGHT-(ST_HEIGHT*vid_resolution), SCREENHEIGHT, 0, SCREENWIDTH, src, dest);

            // [crispy] preserve bezel bottom edge
            if (scaledviewwidth == SCREENWIDTH)
            {
                int x;
                patch_t *patch = W_CacheLumpName(DEH_String("brdr_b"), PU_CACHE);

                for (x = 0 ; x < WIDESCREENDELTA ; x += 8)
                {
                    V_DrawPatch(x - WIDESCREENDELTA, 0, patch);
                    V_DrawPatch(ORIGWIDTH + WIDESCREENDELTA - x - 8, 0, patch);
                }
            }
        }

        // [crispy] center unity rerelease wide status bar
        if (SHORT(sbar->width) > ORIGWIDTH && SHORT(sbar->leftoffset) == 0)
        {
            V_DrawPatch((ORIGWIDTH - SHORT(sbar->width)) / 2, 0, sbar);
        }
        else
        {
            V_DrawPatch(0, 0, sbar);
        }

        // draw right side of bar if needed (Doom 1.0)
        if (sbarr)
        {
            V_DrawPatch(104 /*- ST_WIDESCREENDELTA*/, 0, sbarr);
        }

        // ARMS background
        if (!deathmatch)
        {
            V_DrawPatch(104, 0, armsbg);
        }

        if (netgame)
        {
            // Player face background
            // [JN] killough 3/7/98: make face background change with displayplayer
            V_DrawPatch(143, 0, faceback[displayplayer]);
        }

        V_RestoreBuffer();

        V_CopyRect(0, 0, st_backing_screen, SCREENWIDTH, ST_HEIGHT * vid_resolution, 0, ST_Y * vid_resolution);
    }

    st_fullupdate = false;

    if (st_background_on)
    {
        ST_UpdateElementsBackground();
    }

    // [crispy] draw berserk pack instead of no ammo if appropriate
    if (dp_screen_size > 10 && (!automapactive || automap_overlay))
    {
        if (plyr->readyweapon == wp_fist && plyr->powers[pw_strength])
        {
            static int lump = -1;
            patch_t *patch;

            if (lump == -1)
            {
                lump = W_CheckNumForName(DEH_String("PSTRA0"));

                if (lump == -1)
                {
                    lump = W_CheckNumForName(DEH_String("MEDIA0"));
                }
            }

            patch = W_CacheLumpNum(lump, PU_CACHE);

            // [crispy] (23,179) is the center of the Ammo widget
            V_DrawPatch(44 - 21 - SHORT(patch->width)/2 + SHORT(patch->leftoffset) - wide_x,
                        179 - SHORT(patch->height)/2 + SHORT(patch->topoffset),
                        patch);
        }
    }

    // Ammo amount for current weapon
    if (weaponinfo[plyr->readyweapon].ammo != am_noammo)
    {
        ST_DrawBigNumber(plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
                         6 - wide_x, 171, ST_WidgetColor(hudcolor_ammo));
    }

    // Health, negative health
    {
        const boolean neghealth = st_negative_health && plyr->health <= 0 && !no_sttminus;

        ST_DrawBigNumber(neghealth ? plyr->health_negative : plyr->health,
                         52 - wide_x, 171, ST_WidgetColor(hudcolor_health));
        ST_DrawPercent(90 - wide_x, 171, ST_WidgetColor(hudcolor_health));
    }

    // Frags of Arms
    if (deathmatch)
    {
        st_fragscount = ST_UpdateFragsCounter(displayplayer, false);
        ST_DrawBigNumber(st_fragscount, 100 - wide_x, 171, ST_WidgetColor(hudcolor_frags));
    }
    else
    {
        // Pistol
        ST_DrawWeaponNumberFunc(2, 107 - wide_x, 172, plyr->weaponowned[1]);
        // Shotgun or Super Shotgun
        ST_DrawWeaponNumberFunc(3, 119 - wide_x, 172, plyr->weaponowned[2] || plyr->weaponowned[8]);
        // Chaingun
        ST_DrawWeaponNumberFunc(4, 131 - wide_x, 172, plyr->weaponowned[3]);
        // Rocket Launcher
        ST_DrawWeaponNumberFunc(5, 107 - wide_x, 182, plyr->weaponowned[4]);
        // Plasma Gun
        ST_DrawWeaponNumberFunc(6, 119 - wide_x, 182, plyr->weaponowned[5]);
        // BFG9000
        ST_DrawWeaponNumberFunc(7, 131 - wide_x, 182, plyr->weaponowned[6]);
    }

    // Player face background
    if (dp_screen_size == 11 || dp_screen_size == 13)
    {
        V_DrawPatch(143, 169, netgame ? faceback[displayplayer] : faceback[1]);
    }
    // Player face
    if (dp_screen_size <= 11 || dp_screen_size == 13 || (automapactive && !automap_overlay))
    {
        V_DrawPatch(143, 168, faces[st_faceindex]);
    }

    // [crispy] blinking key or skull in the status bar
    for (int i = 0, y = 0 ; i < 3 ; i++, y += 10)
    {
        if (st_blinking_keys && plyr->tryopen[i])
        {
            if (!(plyr->tryopen[i] & (2 * KEYBLINKMASK - 1)))
            {
                S_StartSound(NULL, sfx_itemup);
            }
            if (plyr->tryopen[i] & KEYBLINKMASK)
            {
                V_DrawPatch(239 + wide_x, 171 + y, keys[i + st_keyorskull[i]]);
            }
        }
    }

    // Armor
    ST_DrawBigNumber(plyr->armorpoints, 183 + wide_x, 171, ST_WidgetColor(hudcolor_armor));
    ST_DrawPercent(221 + wide_x, 171, ST_WidgetColor(hudcolor_armor));

    // Keys
    if (plyr->cards[it_blueskull])
    V_DrawPatch(239 + wide_x, 171, keys[3]);
    else if (plyr->cards[it_bluecard])
    V_DrawPatch(239 + wide_x, 171, keys[0]);

    if (plyr->cards[it_yellowskull])
    V_DrawPatch(239 + wide_x, 181, keys[4]);
    else if (plyr->cards[it_yellowcard])
    V_DrawPatch(239 + wide_x, 181, keys[1]);

    if (plyr->cards[it_redskull])
    V_DrawPatch(239 + wide_x, 191, keys[5]);
    else if (plyr->cards[it_redcard])
    V_DrawPatch(239 + wide_x, 191, keys[2]);

    // Ammo (current)
    ST_DrawSmallNumberY(plyr->ammo[0], 280 + wide_x, 173);
    ST_DrawSmallNumberY(plyr->ammo[1], 280 + wide_x, 179);
    ST_DrawSmallNumberY(plyr->ammo[3], 280 + wide_x, 185);
    ST_DrawSmallNumberY(plyr->ammo[2], 280 + wide_x, 191);

    // Ammo (max)
    ST_DrawSmallNumberY(plyr->maxammo[0], 306 + wide_x, 173);
    ST_DrawSmallNumberY(plyr->maxammo[1], 306 + wide_x, 179);
    ST_DrawSmallNumberY(plyr->maxammo[3], 306 + wide_x, 185);
    ST_DrawSmallNumberY(plyr->maxammo[2], 306 + wide_x, 191);
    }
}

typedef void (*load_callback_t)(const char *lumpname, patch_t **variable); 

// Iterates through all graphics to be loaded or unloaded, along with
// the variable they use, invoking the specified callback function.

static void ST_loadUnloadGraphics(load_callback_t callback)
{

    int		i;
    int		j;
    int		facenum;
    
    char	namebuf[9];

    // Load the numbers, tall and short
    for (i=0;i<10;i++)
    {
	DEH_snprintf(namebuf, 9, "STTNUM%d", i);
        callback(namebuf, &tallnum[i]);

	DEH_snprintf(namebuf, 9, "STYSNUM%d", i);
        callback(namebuf, &shortnum_y[i]);

	DEH_snprintf(namebuf, 9, "STGNUM%d", i);
        callback(namebuf, &shortnum_g[i]);
    }

    // Load percent key.
    callback(DEH_String("STTPRCNT"), &tallpercent);

    // [JN] Load minus symbol.
    // Versions prior 1.4 does not have STTMINUS patch.
    // If case of using such versions, drawing negative health is not possible.
    if (W_CheckNumForName("STTMINUS") >= 0)
    {
        callback(DEH_String("STTMINUS"), &tallminus);
    }
    else
    {
        tallminus = NULL;
        no_sttminus = true;
    }

    // key cards
    for (i=0;i<NUMCARDS;i++)
    {
	DEH_snprintf(namebuf, 9, "STKEYS%d", i);
        callback(namebuf, &keys[i]);
    }

    // arms background
    callback(DEH_String("STARMS"), &armsbg);

    // face backgrounds for different color players
    // [JN] killough 3/7/98: add better support for spy mode by loading
    // all player face backgrounds and using displayplayer to choose them:
    for (i=0; i<MAXPLAYERS; i++)
    {
    DEH_snprintf(namebuf, 9, "STFB%d", i);
    callback(namebuf, &faceback[i]);
    }

    // status bar background bits
    //callback(DEH_String("STBAR"), &sbar);
    if (W_CheckNumForName("STBAR") >= 0)
    {
        callback(DEH_String("STBAR"), &sbar);
        sbarr = NULL;
    }
    else
    {
        callback(DEH_String("STMBARL"), &sbar);
        callback(DEH_String("STMBARR"), &sbarr);
    }

    // face states
    facenum = 0;
    for (i=0; i<ST_NUMPAINFACES; i++)
    {
	for (j=0; j<ST_NUMSTRAIGHTFACES; j++)
	{
	    DEH_snprintf(namebuf, 9, "STFST%d%d", i, j);
            callback(namebuf, &faces[facenum]);
            ++facenum;
	}
	DEH_snprintf(namebuf, 9, "STFTR%d0", i);	// turn right
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFTL%d0", i);	// turn left
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFOUCH%d", i);	// ouch!
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFEVL%d", i);	// evil grin ;)
        callback(namebuf, &faces[facenum]);
        ++facenum;
	DEH_snprintf(namebuf, 9, "STFKILL%d", i);	// pissed off
        callback(namebuf, &faces[facenum]);
        ++facenum;
    }

    callback(DEH_String("STFGOD0"), &faces[facenum]);
    ++facenum;
    callback(DEH_String("STFDEAD0"), &faces[facenum]);
    ++facenum;
}

static void ST_loadCallback(const char *lumpname, patch_t **variable)
{
    *variable = W_CacheLumpName(lumpname, PU_STATIC);
}

void ST_loadGraphics(void)
{
    ST_loadUnloadGraphics(ST_loadCallback);
}

void ST_loadData(void)
{
    lu_palette = W_GetNumForName (DEH_String("PLAYPAL"));
    ST_loadGraphics();
}

static void ST_unloadCallback(const char *lumpname, patch_t **variable)
{
    W_ReleaseLumpName(lumpname);
    *variable = NULL;
}

void ST_unloadGraphics(void)
{
    ST_loadUnloadGraphics(ST_unloadCallback);
}

void ST_unloadData(void)
{
    ST_unloadGraphics();
}

void ST_Start (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette (W_CacheLumpNum (lu_palette, PU_CACHE));
#else
    I_SetPalette (0);
#endif

    plyr = &players[displayplayer];
    faceindex = 1; // [crispy] fix status bar face hysteresis across level changes
    st_faceindex = 1;
    st_palette = -1;
    st_oldhealth = -1;

    for (int i = 0 ; i < NUMWEAPONS ; i++)
    {
        oldweaponsowned[i] = plyr->weaponowned[i];
    }
}

void ST_Init (void)
{
    ST_loadData();
    st_backing_screen = (pixel_t *) Z_Malloc(MAXWIDTH * (ST_HEIGHT * MAXHIRES)
                      * sizeof(*st_backing_screen), PU_STATIC, 0);
}

// -----------------------------------------------------------------------------
// ST_InitElementsBackground
// [JN] Preallocate rectangle sizes for status bar buffered drawing 
//      to avoid some extra multiplying calculations while drawing.
// -----------------------------------------------------------------------------

void ST_InitElementsBackground (void)
{
    // Ammo
    ammo_bg[0] = WIDESCREENDELTA * vid_resolution;
    ammo_bg[1] = 2 * vid_resolution;
    ammo_bg[2] = 45 * vid_resolution;
    ammo_bg[3] = 20 * vid_resolution;
    ammo_bg[4] = 170 * vid_resolution;

    // Health
    hlth_bg[0] = (48 + WIDESCREENDELTA) * vid_resolution;
    hlth_bg[1] = 2 * vid_resolution;
    hlth_bg[2] = 57 * vid_resolution;
    hlth_bg[3] = 20 * vid_resolution;
    hlth_bg[4] = 170 * vid_resolution;

    // ARMS or frags
    frgs_bg[0] = (105 + WIDESCREENDELTA) * vid_resolution;
    frgs_bg[1] = 2 * vid_resolution;
    frgs_bg[2] = 37 * vid_resolution;
    frgs_bg[3] = 20 * vid_resolution;
    frgs_bg[4] = 170 * vid_resolution;

    // Player face background
    face_bg[0] = (142 + WIDESCREENDELTA) * vid_resolution;
    face_bg[1] = 0;
    face_bg[2] = 37 * vid_resolution;
    face_bg[3] = 32 * vid_resolution;
    face_bg[4] = 168 * vid_resolution;

    // Armor
    armr_bg[0] = (179 + WIDESCREENDELTA) * vid_resolution;
    armr_bg[1] = 2 * vid_resolution;
    armr_bg[2] = 56 * vid_resolution;
    armr_bg[3] = 20 * vid_resolution;
    armr_bg[4] = 170 * vid_resolution;    

    // Keys
    keys_bg[0] = (236 + WIDESCREENDELTA) * vid_resolution;
    keys_bg[1] = 0;
    keys_bg[2] = 13 * vid_resolution;
    keys_bg[3] = 32 * vid_resolution;
    keys_bg[4] = 168 * vid_resolution;

    // Ammo (current)
    amoc_bg[0] = (272 + WIDESCREENDELTA) * vid_resolution;
    amoc_bg[1] = 5 * vid_resolution;
    amoc_bg[2] = 16 * vid_resolution;
    amoc_bg[3] = 24 * vid_resolution;
    amoc_bg[4] = 173 * vid_resolution;

    // Ammo (max)
    amom_bg[0] = (298 + WIDESCREENDELTA) * vid_resolution;
    amom_bg[1] = 5 * vid_resolution;
    amom_bg[2] = 16 * vid_resolution;
    amom_bg[3] = 24 * vid_resolution;
    amom_bg[4] = 173 * vid_resolution;

    // Disk icon
    disk_bg[0] = (304 + WIDESCREENDELTA * 2) * vid_resolution;
    disk_bg[1] = 17 * vid_resolution;
    disk_bg[2] = 16 * vid_resolution;
    disk_bg[3] = 16 * vid_resolution;
    disk_bg[4] = 185 * vid_resolution;
}
