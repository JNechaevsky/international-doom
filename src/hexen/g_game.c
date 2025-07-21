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


#include <string.h>
#include <time.h>   // [JN] Local time.
#include "m_random.h"
#include "h2def.h"
#include "s_sound.h"
#include "doomkeys.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_video.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "v_video.h"
#include "am_map.h"
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"

#define AM_STARTKEY	9

#define MLOOKUNIT 8 // [crispy] for mouselook
#define MLOOKUNITLOWRES 16 // [crispy] for mouselook when recording

// External functions


// Functions

static void G_ReadDemoTiccmd(ticcmd_t * cmd);
static void G_WriteDemoTiccmd(ticcmd_t * cmd);

void G_DoReborn(int playernum);

void G_DoLoadLevel(void);
void G_DoInitNew(void);
void G_DoTeleportNewMap(void);
void G_DoCompleted(void);
void G_DoVictory(void);
void G_DoWorldDone(void);
void G_DoSaveGame(void);
void G_DoSingleReborn(void);

void H2_PageTicker(void);
void H2_AdvanceDemo(void);


gameaction_t gameaction;
gamestate_t gamestate;
skill_t gameskill;
//boolean         respawnmonsters;
int gameepisode;
int gamemap;
int prevmap;

boolean paused;
boolean sendpause;              // send a pause event next tic
boolean sendsave;               // send a save event next tic
boolean usergame;               // ok to save / end game

boolean timingdemo;             // if true, exit with report on completion
boolean nodrawers = false; // [crispy] for the demowarp feature
int starttime;                  // for comparative timing purposes      

boolean viewactive;

boolean deathmatch;             // only if started as net death
boolean netgame;                // only true if packets are broadcast
boolean playeringame[MAXPLAYERS];
player_t players[MAXPLAYERS];
pclass_t PlayerClass[MAXPLAYERS];

// Position indicator for cooperative net-play reborn
int RebornPosition;

int consoleplayer;              // player taking events and displaying
int displayplayer;              // view being displayed
int levelstarttic;              // gametic at level start

char *demoname;
static const char *orig_demoname = NULL; // [crispy] the name originally chosen for the demo, i.e. without "-00000"
boolean demorecording;
boolean longtics;               // specify high resolution turning in demos
boolean lowres_turn;
boolean shortticfix;            // calculate lowres turning like doom
boolean demoplayback;
boolean demoextend;
boolean netdemo;
boolean solonet;                // [JN] Boolean for Auto SR50 check
byte *demobuffer, *demo_p, *demoend;
boolean singledemo;             // quit after playing a demo from cmdline

boolean precache = true;        // if true, load all graphics at start

// TODO: Hexen uses 16-bit shorts for consistancy?
byte consistancy[MAXPLAYERS][BACKUPTICS];

int LeaveMap;
static int LeavePosition;

//#define MAXPLMOVE       0x32 // Old Heretic Max move

fixed_t MaxPlayerMove[NUMCLASSES] = { 0x3C, 0x32, 0x2D, 0x31 };
fixed_t forwardmove[NUMCLASSES][2] = {
    {0x1D, 0x3C},
    {0x19, 0x32},
    {0x16, 0x2E},
    {0x18, 0x31}
};

fixed_t sidemove_original[NUMCLASSES][2] = {
    {0x1B, 0x3B},
    {0x18, 0x28},
    {0x15, 0x25},
    {0x17, 0x27}
};
fixed_t (*sidemove)[2] = sidemove_original;

fixed_t angleturn[3] = { 640, 1280, 320 };      // + slow turn

static int *weapon_keys[] =
{
    &key_weapon1,
    &key_weapon2,
    &key_weapon3,
    &key_weapon4,
};

static int next_weapon = 0;

#define SLOWTURNTICS    6

boolean gamekeydown[NUMKEYS];
int turnheld;                   // for accelerative turning
int lookheld;


boolean mousearray[MAX_MOUSE_BUTTONS + 1];
boolean *mousebuttons = &mousearray[1];
        // allow [-1]
int mousex, mousey;             // mouse values are used once
int dclicktime, dclickstate, dclicks;
int dclicktime2, dclickstate2, dclicks2;

// [crispy] for rounding error
typedef struct carry_s
{
    double angle;
    double pitch;
    double side;
    double vert;
} carry_t;

static carry_t prevcarry;
static carry_t carry;

#define MAX_JOY_BUTTONS 20

int joyxmove, joyymove;         // joystick values are repeated
int joystrafemove;
int joylook;
boolean joyarray[MAX_JOY_BUTTONS + 1];
boolean *joybuttons = &joyarray[1];     // allow [-1]

// [JN] Determinates speed of camera Z-axis movement in spectator mode.
static int crl_camzspeed;

int savegameslot;
char savedescription[32];

static ticcmd_t basecmd; // [crispy]

static int inventoryTics;

// haleyjd: removed externdriver crap

static skill_t TempSkill;
static int TempEpisode;
static int TempMap;

boolean testcontrols = false;
int testcontrols_mousespeed;

boolean usearti = true;

boolean speedkeydown (void)
{
    return (key_speed < NUMKEYS && gamekeydown[key_speed]) ||
           (mousebspeed < MAX_MOUSE_BUTTONS && mousebuttons[mousebspeed]) ||
           (joybspeed < MAX_JOY_BUTTONS && joybuttons[joybspeed]);
}

// [crispy] for carrying rounding error
static int CarryError(double value, const double *prevcarry, double *carry)
{
    const double desired = value + *prevcarry;
    const int actual = lround(desired);
    *carry = desired - actual;

    return actual;
}

static short CarryAngle(double angle)
{
    if (lowres_turn && fabs(angle + prevcarry.angle) < 128)
    {
        carry.angle = angle + prevcarry.angle;
        return 0;
    }
    else
    {
        return CarryError(angle, &prevcarry.angle, &carry.angle);
    }
}

static short CarryPitch(double pitch)
{
    return CarryError(pitch, &prevcarry.pitch, &carry.pitch);
}

static int CarryMouseVert(double vert)
{
    return CarryError(vert, &prevcarry.vert, &carry.vert);
}

static int CarryMouseSide(double side)
{
    const double desired = side + prevcarry.side;
    const int actual = lround(side * 0.5) * 2; // Even values only.
    carry.side = desired - actual;
    return actual;
}

static double CalcMouseAngle(int mousex)
{
    if (!mouseSensitivity)
        return 0.0;

    return (I_AccelerateMouse(mousex) * (mouseSensitivity + 5) * 8 / 10);
}

static double CalcMouseVert(int mousey)
{
    if (!mouse_sensitivity_y)
        return 0.0;

    return (I_AccelerateMouseY(mousey) * (mouse_sensitivity_y + 5) * 2 / 10);
}

//=============================================================================

// -----------------------------------------------------------------------------
// G_SetSideMove
//  [JN] Enable automatic automatic straferunning 50 (SR50).
// -----------------------------------------------------------------------------

void G_SetSideMove (void)
{
    if ((singleplayer || (netgame && solonet)) && compat_auto_sr50)
    {
        sidemove = forwardmove;
    }
    else
    {
        sidemove = sidemove_original;
    }
}

/*
====================
=
= G_BuildTiccmd
=
= Builds a ticcmd from all of the available inputs or reads it from the
= demo buffer.
= If recording a demo, write it out
====================
*/

void G_BuildTiccmd(ticcmd_t *cmd, int maketic)
{
    int i;
    boolean strafe, bstrafe;
    int speed, tspeed, lspeed;
    int angle = 0; // [crispy]
    short mousex_angleturn; // [crispy]
    int forward, side;
    int look, arti;
    int fly_height;
    int pClass;
    ticcmd_t spect;

    // haleyjd: removed externdriver crap

    pClass = players[consoleplayer].class;

    if (!crl_spectating)
    {
        // [crispy] For fast polling.
        G_PrepTiccmd();
        memcpy(cmd, &basecmd, sizeof(*cmd));
        memset(&basecmd, 0, sizeof(ticcmd_t));
    }
    else
    {
        // [JN] CRL - can't interpolate spectator.
        memset(cmd, 0, sizeof(ticcmd_t));
        // [JN] CRL - reset basecmd.angleturn for exact
        // position of jumping to the camera position.
        basecmd.angleturn = 0;
    }

//      cmd->consistancy =
//              consistancy[consoleplayer][(maketic*ticdup)%BACKUPTICS];

    cmd->consistancy = consistancy[consoleplayer][maketic % BACKUPTICS];

    // [JN] Deny all player control events while active menu 
    // in multiplayer to eliminate movement and camera rotation.
    if (netgame && (MenuActive || askforquit))
    return;

 	// RestlessRodent -- If spectating then the player loses all input
 	memmove(&spect, cmd, sizeof(spect));
 	// [JN] Allow saving and pausing while spectating.
 	if (crl_spectating && !sendsave && !sendpause)
 		cmd = &spect;

//printf ("cons: %i\n",cmd->consistancy);

    strafe = gamekeydown[key_strafe]
          || mousebuttons[mousebstrafe]
          || joybuttons[joybstrafe];

    // Allow joybspeed hack.

    speed = (key_speed >= NUMKEYS
        || joybspeed >= MAX_JOY_BUTTONS);
    speed ^= speedkeydown();
    crl_camzspeed = speed;

    // haleyjd: removed externdriver crap
    
    forward = side = look = arti = fly_height = 0;

//
// use two stage accelerative turning on the keyboard and joystick
//
    if (joyxmove < 0 || joyxmove > 0
        || gamekeydown[key_right] || gamekeydown[key_left])
        turnheld += ticdup;
    else
        turnheld = 0;
    if (turnheld < SLOWTURNTICS)
        tspeed = 2;             // slow turn
    else
        tspeed = speed;

    if (gamekeydown[key_lookdown] || gamekeydown[key_lookup])
    {
        lookheld += ticdup;
    }
    else
    {
        lookheld = 0;
    }
    if (lookheld < SLOWTURNTICS)
    {
        lspeed = 1;             // 3;
    }
    else
    {
        lspeed = 2;             // 5;
    }

    // [crispy] toggle "always run"
    if (gamekeydown[key_autorun])
    {
        static int joybspeed_old = 2;

        if (joybspeed >= MAX_JOY_BUTTONS)
        {
            joybspeed = joybspeed_old;
        }
        else
        {
            joybspeed_old = joybspeed;
            joybspeed = MAX_JOY_BUTTONS;
        }

        CT_SetMessage(&players[consoleplayer], joybspeed >= MAX_JOY_BUTTONS ?
                      ID_AUTORUN_ON : ID_AUTORUN_OFF, false, NULL);
        S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
        gamekeydown[key_autorun] = false;
    }

    // [JN] Toggle mouse look.
    if (gamekeydown[key_mouse_look])
    {
        mouse_look ^= 1;
        if (!mouse_look)
        {
            look = TOCENTER;
        }
        CT_SetMessage(&players[consoleplayer], mouse_look ?
                      ID_MLOOK_ON : ID_MLOOK_OFF, false, NULL);
        S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
        gamekeydown[key_mouse_look] = false;
    }

    // [JN] Toggle vertical mouse movement.
    if (gamekeydown[key_novert])
    {
        mouse_novert ^= 1;
        CT_SetMessage(&players[consoleplayer], mouse_novert ?
                      ID_NOVERT_ON : ID_NOVERT_OFF, false, NULL);
        S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
        gamekeydown[key_novert] = false;
    }

    // [crispy] add quick 180° reverse
    if (gamekeydown[key_180turn])
    {
        angle += ANG180 >> FRACBITS;
        gamekeydown[key_180turn] = false;
    }
//
// let movement keys cancel each other out
//
    if (strafe)
    {
        if (!cmd->angleturn)
        {
            if (gamekeydown[key_right])
            {
                side += sidemove[pClass][speed];
            }
            if (gamekeydown[key_left])
            {
                side -= sidemove[pClass][speed];
            }
            if (use_analog && joyxmove)
            {
                joyxmove = joyxmove * joystick_move_sensitivity / 10;
                joyxmove = (joyxmove > FRACUNIT) ? FRACUNIT : joyxmove;
                joyxmove = (joyxmove < -FRACUNIT) ? -FRACUNIT : joyxmove;
                side += FixedMul(sidemove[pClass][speed], joyxmove);
            }
            else if (joystick_move_sensitivity)
            {
                if (joyxmove > 0)
                {
                    side += sidemove[pClass][speed];
                }
                if (joyxmove < 0)
                {
                    side -= sidemove[pClass][speed];
                }
            }
        }
    }
    else
    {
        if (gamekeydown[key_right])
            angle -= angleturn[tspeed];
        if (gamekeydown[key_left])
            angle += angleturn[tspeed];
        if (use_analog && joyxmove)
        {
            // Cubic response curve allows for finer control when stick
            // deflection is small.
            joyxmove = FixedMul(FixedMul(joyxmove, joyxmove), joyxmove);
            joyxmove = joyxmove * joystick_turn_sensitivity / 10;
            angle -= FixedMul(angleturn[1], joyxmove);
        }
        else if (joystick_turn_sensitivity)
        {
            if (joyxmove > 0)
                angle -= angleturn[tspeed];
            if (joyxmove < 0)
                angle += angleturn[tspeed];
        }
    }

    if (gamekeydown[key_up])
    {
        forward += forwardmove[pClass][speed];
    }
    if (gamekeydown[key_down])
    {
        forward -= forwardmove[pClass][speed];
    }
    if (use_analog && joyymove)
    {
        joyymove = joyymove * joystick_move_sensitivity / 10;
        joyymove = (joyymove > FRACUNIT) ? FRACUNIT : joyymove;
        joyymove = (joyymove < -FRACUNIT) ? FRACUNIT : joyymove;
        forward -= FixedMul(forwardmove[pClass][speed], joyymove);
    }
    else if (joystick_move_sensitivity)
    {
        if (joyymove < 0)
        {
            forward += forwardmove[pClass][speed];
        }
        if (joyymove > 0)
        {
            forward -= forwardmove[pClass][speed];
        }
    }
    if (gamekeydown[key_straferight] || mousebuttons[mousebstraferight]
     || joybuttons[joybstraferight])
    {
        side += sidemove[pClass][speed];
    }
    if (gamekeydown[key_strafeleft] || mousebuttons[mousebstrafeleft]
     || joybuttons[joybstrafeleft])
    {
        side -= sidemove[pClass][speed];
    }

    if (use_analog && joystrafemove)
    {
        joystrafemove = joystrafemove * joystick_move_sensitivity / 10;
        joystrafemove = (joystrafemove > FRACUNIT) ? FRACUNIT : joystrafemove;
        joystrafemove = (joystrafemove < -FRACUNIT) ? -FRACUNIT : joystrafemove;
        side += FixedMul(sidemove[pClass][speed], joystrafemove);
    }
    else if (joystick_move_sensitivity)
    {
        if (joystrafemove < 0)
            side -= sidemove[pClass][speed];
        if (joystrafemove > 0)
            side += sidemove[pClass][speed];
    }

    // Look up/down/center keys
    if (gamekeydown[key_lookup])
    {
        look = lspeed;
    }
    if (gamekeydown[key_lookdown])
    {
        look = -lspeed;
    }
    if (use_analog && joylook)
    {
        joylook = joylook * joystick_look_sensitivity / 10;
        joylook = (joylook > FRACUNIT) ? FRACUNIT : joylook;
        joylook = (joylook < -FRACUNIT) ? -FRACUNIT : joylook;
        look = -FixedMul(2, joylook);
    }
    else if (joystick_look_sensitivity)
    {
        if (joylook < 0)
        {
            look = lspeed;
        }

        if (joylook > 0)
        {
            look = -lspeed;
        }
    }
    // haleyjd: removed externdriver crap
    if (gamekeydown[key_lookcenter])
    {
        look = TOCENTER;
    }

    // haleyjd: removed externdriver crap

    // Fly up/down/drop keys
    if (gamekeydown[key_flyup])
    {
        fly_height = 5;          // note that the actual fly_height will be twice this
    }
    if (gamekeydown[key_flydown])
    {
        fly_height = -5;
    }
    if (gamekeydown[key_flycenter])
    {
        fly_height = TOCENTER;
        // haleyjd: removed externdriver crap
        look = TOCENTER;
    }
    // Use artifact key
    if (gamekeydown[key_useartifact] || mousebuttons[mousebuseartifact])
    {
        if (gamekeydown[key_speed] && artiskip)
        {
            if (players[consoleplayer].inventory[inv_ptr].type != arti_none)
            {                   // Skip an artifact
                gamekeydown[key_useartifact] = false;
                mousebuttons[mousebuseartifact] = false;
                P_PlayerNextArtifact(&players[consoleplayer]);
            }
        }
        else
        {
            if (inventory)
            {
                players[consoleplayer].readyArtifact =
                    players[consoleplayer].inventory[inv_ptr].type;
                inventory = false;
                cmd->arti = 0;
                usearti = false;
            }
            else if (usearti)
            {
                cmd->arti |=
                    players[consoleplayer].inventory[inv_ptr].
                    type & AFLAG_MASK;
                usearti = false;
            }
        }
    }
    if (gamekeydown[key_jump] || mousebuttons[mousebjump])
    {
        cmd->arti |= AFLAG_JUMP;
    }
    if (mn_SuicideConsole)
    {
        cmd->arti |= AFLAG_SUICIDE;
        mn_SuicideConsole = false;
    }

    // Artifact hot keys
    if (gamekeydown[key_arti_all] && !cmd->arti)
    {
        gamekeydown[key_arti_all] = false;     // Use one of each artifact
        cmd->arti = NUMARTIFACTS;
    }
    else if (gamekeydown[key_arti_health] && !cmd->arti
             && (players[consoleplayer].mo->health < MAXHEALTH))
    {
        gamekeydown[key_arti_health] = false;
        cmd->arti = arti_health;
    }
    else if (gamekeydown[key_arti_poisonbag] && !cmd->arti)
    {
        gamekeydown[key_arti_poisonbag] = false;
        cmd->arti = arti_poisonbag;
    }
    else if (gamekeydown[key_arti_blastradius] && !cmd->arti)
    {
        gamekeydown[key_arti_blastradius] = false;
        cmd->arti = arti_blastradius;
    }
    else if (gamekeydown[key_arti_teleport] && !cmd->arti)
    {
        gamekeydown[key_arti_teleport] = false;
        cmd->arti = arti_teleport;
    }
    else if (gamekeydown[key_arti_teleportother] && !cmd->arti)
    {
        gamekeydown[key_arti_teleportother] = false;
        cmd->arti = arti_teleportother;
    }
    else if (gamekeydown[key_arti_egg] && !cmd->arti)
    {
        gamekeydown[key_arti_egg] = false;
        cmd->arti = arti_egg;
    }
    else if (gamekeydown[key_arti_invulnerability] && !cmd->arti
             && !players[consoleplayer].powers[pw_invulnerability])
    {
        gamekeydown[key_arti_invulnerability] = false;
        cmd->arti = arti_invulnerability;
    }
    // [JN] Add dedicated binds for all artifacts.
    else if (gamekeydown[key_arti_urn] && !cmd->arti
             && (players[consoleplayer].mo->health < MAXHEALTH))
    {
        gamekeydown[key_arti_urn] = false;
        cmd->arti = arti_superhealth;
    }
    else if (gamekeydown[key_arti_wings] && !cmd->arti)
    {
        gamekeydown[key_arti_wings] = false;
        cmd->arti = arti_fly;
    }
    else if (gamekeydown[key_arti_servant] && !cmd->arti)
    {
        gamekeydown[key_arti_servant] = false;
        cmd->arti = arti_summon;
    }
    else if (gamekeydown[key_arti_bracers] && !cmd->arti)
    {
        gamekeydown[key_arti_bracers] = false;
        cmd->arti = arti_boostarmor;
    }
    else if (gamekeydown[key_arti_boots] && !cmd->arti)
    {
        gamekeydown[key_arti_boots] = false;
        cmd->arti = arti_speed;
    }
    else if (gamekeydown[key_arti_torch] && !cmd->arti)
    {
        gamekeydown[key_arti_torch] = false;
        cmd->arti = arti_torch;
    }
    else if (gamekeydown[key_arti_krater] && !cmd->arti)
    {
        gamekeydown[key_arti_krater] = false;
        cmd->arti = arti_boostmana;
    }
    else if (gamekeydown[key_arti_incant] && !cmd->arti)
    {
        gamekeydown[key_arti_incant] = false;
        cmd->arti = arti_healingradius;
    }
    else if (gamekeydown[key_arti_incant] && !cmd->arti)
    {
        gamekeydown[key_arti_incant] = false;
        cmd->arti = arti_healingradius;
    }

//
// buttons
//
    cmd->chatchar = CT_dequeueChatChar();

    if (gamekeydown[key_fire] || mousebuttons[mousebfire]
        || joybuttons[joybfire])
        cmd->buttons |= BT_ATTACK;

    if (gamekeydown[key_use] || joybuttons[joybuse] || mousebuttons[mousebuse])
    {
        cmd->buttons |= BT_USE;
        dclicks = 0;            // clear double clicks if hit use button
    }

    // Weapon cycling. Switch to previous or next weapon.
    // (Disabled when player is a pig).
    if (gamestate == GS_LEVEL
     && players[consoleplayer].morphTics == 0 && next_weapon != 0)
    {
        int start_i;

        if (players[consoleplayer].pendingweapon == WP_NOCHANGE)
        {
            i = players[consoleplayer].readyweapon;
        }
        else
        {
            i = players[consoleplayer].pendingweapon;
        }

        // Don't loop forever.
        start_i = i;
        do {
            i = (i + next_weapon + NUMWEAPONS) % NUMWEAPONS;
        } while (i != start_i && !players[consoleplayer].weaponowned[i]);

        cmd->buttons |= BT_CHANGE;
        cmd->buttons |= i << BT_WEAPONSHIFT;
    }
    else
    {
        for (i=0; i<arrlen(weapon_keys); ++i)
        {
            int key = *weapon_keys[i];

            if (gamekeydown[key])
            {
                cmd->buttons |= BT_CHANGE; 
                cmd->buttons |= i<<BT_WEAPONSHIFT; 
                break; 
            }
        }
    }

    next_weapon = 0;

//
// mouse
//
    if (mousebuttons[mousebforward])
    {
        forward += forwardmove[pClass][speed];
    }
    if (mousebuttons[mousebbackward])
    {
        forward -= forwardmove[pClass][speed];
    }

    // Double click to use can be disabled

    if (mouse_dclick_use)
    {
        //
        // forward double click
        //
        if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1)
        {
            dclickstate = mousebuttons[mousebforward];
            if (dclickstate)
                dclicks++;
            if (dclicks == 2)
            {
                cmd->buttons |= BT_USE;
                dclicks = 0;
            }
            else
                dclicktime = 0;
        }
        else
        {
            dclicktime += ticdup;
            if (dclicktime > 20)
            {
                dclicks = 0;
                dclickstate = 0;
            }
        }

        //
        // strafe double click
        //
        bstrafe = mousebuttons[mousebstrafe] || joybuttons[joybstrafe];
        if (bstrafe != dclickstate2 && dclicktime2 > 1)
        {
            dclickstate2 = bstrafe;
            if (dclickstate2)
                dclicks2++;
            if (dclicks2 == 2)
            {
                cmd->buttons |= BT_USE;
                dclicks2 = 0;
            }
            else
                dclicktime2 = 0;
        }
        else
        {
            dclicktime2 += ticdup;
            if (dclicktime2 > 20)
            {
                dclicks2 = 0;
                dclickstate2 = 0;
            }
        }
    }

    if (strafe)
    {
        side += mousex * 2;
    }
    else
    {
        if (!crl_spectating)
        cmd->angleturn += CarryMouseSide(mousex);
        else
        angle -= mousex*0x8;
    }

    mousex_angleturn = cmd->angleturn;

    if (mousex_angleturn == 0)
    {
        testcontrols_mousespeed = 0;
    }

    if (angle)
    {
        if (!crl_spectating)
        {
        cmd->angleturn = CarryAngle(cmd->angleturn + angle);
        localview.ticangleturn = gp_flip_levels ?
            (mousex_angleturn - cmd->angleturn) :
            (cmd->angleturn - mousex_angleturn);
        }
        else
        {
        const short old_angleturn = cmd->angleturn;
        cmd->angleturn = CarryAngle(localview.rawangle + angle);
        localview.ticangleturn = gp_flip_levels ?
            (old_angleturn - cmd->angleturn) :
            (cmd->angleturn - old_angleturn);
        }
    }

    if (cmd->lookdir)
    {
        if (demorecording || lowres_turn)
        {
            // [Dasperal] Skip mouse look if it is TOCENTER cmd
            if (look != TOCENTER)
            {
                // [crispy] Map mouse movement to look variable when recording
                look += cmd->lookdir / MLOOKUNITLOWRES;

                // [crispy] Limit to max speed of keyboard look up/down
                if (look > 2)
                    look = 2;
                else if (look < -2)
                    look = -2;
            }
            cmd->lookdir = 0;
        }
        else
        {
            // [Dasperal] Allow precise vertical look with near 0 mouse movement
            if (cmd->lookdir > 0)
                cmd->lookdir = (cmd->lookdir + MLOOKUNIT - 1) / MLOOKUNIT;
            else
                cmd->lookdir = (cmd->lookdir - MLOOKUNIT + 1) / MLOOKUNIT;
        }
    }
    else if (!mouse_novert)
    {
        forward += CarryMouseVert(mousey);
    }

    mousex = mousey = 0;

    if (forward > MaxPlayerMove[pClass])
    {
        forward = MaxPlayerMove[pClass];
    }
    else if (forward < -MaxPlayerMove[pClass])
    {
        forward = -MaxPlayerMove[pClass];
    }
    if (side > MaxPlayerMove[pClass])
    {
        side = MaxPlayerMove[pClass];
    }
    else if (side < -MaxPlayerMove[pClass])
    {
        side = -MaxPlayerMove[pClass];
    }
    if (players[consoleplayer].powers[pw_speed]
        && !players[consoleplayer].morphTics)
    {                           // Adjust for a player with a speed artifact
        forward = (3 * forward) >> 1;
        side = (3 * side) >> 1;
    }
    cmd->forwardmove += forward;
    cmd->sidemove += side;

    // [crispy]
    localview.angle = 0;
    localview.rawangle = 0.0;
    prevcarry = carry;

    if (players[consoleplayer].playerstate == PST_LIVE)
    {
        if (look < 0)
        {
            look += 16;
        }
        cmd->lookfly = look;
    }
    if (fly_height < 0)
    {
        fly_height += 16;
    }
    cmd->lookfly |= fly_height << 4;

//
// special buttons
//
    if (sendpause)
    {
        sendpause = false;
        cmd->buttons = BT_SPECIAL | BTS_PAUSE;
    }

    if (sendsave)
    {
        sendsave = false;
        cmd->buttons =
            BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
    }

    if (gp_flip_levels)
    {
        mousex_angleturn = -mousex_angleturn;
        cmd->angleturn = -cmd->angleturn;
        cmd->sidemove = -cmd->sidemove;
    }

    if (lowres_turn)
    {
        if (shortticfix)
        {
            signed short desired_angleturn;

            desired_angleturn = cmd->angleturn;

            // round angleturn to the nearest 256 unit boundary
            // for recording demos with single byte values for turn

            cmd->angleturn = (desired_angleturn + 128) & 0xff00;

            if (angle)
            {
                localview.ticangleturn = cmd->angleturn - mousex_angleturn;
            }

            // Carry forward the error from the reduced resolution to the
            // next tic, so that successive small movements can accumulate.

            prevcarry.angle += gp_flip_levels ?
                                cmd->angleturn - desired_angleturn :
                                desired_angleturn - cmd->angleturn;
        }
        else
        {
            // truncate angleturn to the nearest 256 boundary
            // for recording demos with single byte values for turn
            cmd->angleturn &= 0xff00;
        }
    }

    // RestlessRodent -- If spectating, send the movement commands instead
    if (crl_spectating && !MenuActive)
    	CRL_ImpulseCamera(cmd->forwardmove, cmd->sidemove, cmd->angleturn); 
}


/*
==============
=
= G_DoLoadLevel
=
==============
*/

void G_DoLoadLevel(void)
{
    int i;

    levelstarttic = gametic;    // for time calculation 

    if (wipegamestate == GS_LEVEL)
    {
        wipegamestate = -1;             // force a wipe
    }

    gamestate = GS_LEVEL;
    for (i = 0; i < maxplayers; i++)
    {
        if (playeringame[i] && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
        memset(players[i].frags, 0, sizeof(players[i].frags));
    }

    SN_StopAllSequences();
    P_SetupLevel(gameepisode, gamemap, 0, gameskill);
    // [JN] Do not reset chosen player view across levels in multiplayer
    // demo playback. However, it must be reset when starting a new game.
    if (usergame)
    {
        displayplayer = consoleplayer;      // view the guy you are playing
    }
    gameaction = ga_nothing;
    Z_CheckHeap();

//
// clear cmd building stuff
// 

    memset(gamekeydown, 0, sizeof(gamekeydown));
    joyxmove = joyymove = joystrafemove = joylook = 0;
    mousex = mousey = 0;
    memset(&localview, 0, sizeof(localview)); // [crispy]
    memset(&carry, 0, sizeof(carry)); // [crispy]
    memset(&prevcarry, 0, sizeof(prevcarry)); // [crispy]
    memset(&basecmd, 0, sizeof(basecmd)); // [crispy]
    sendpause = sendsave = paused = false;
    // [PN] Resume music playback after loading a savegame,
    // in case the game was previously paused. This prevents
    // the current track from staying silent even though
    // S_StartSong() skips restarting it.
    S_ResumeSound();
    memset(mousearray, 0, sizeof(mousearray));
    memset(joyarray, 0, sizeof(joyarray));

    if (testcontrols)
    {
        CT_SetMessage(&players[consoleplayer], "PRESS ESCAPE TO QUIT.", false, NULL);
    }
}

static void SetJoyButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_JOY_BUTTONS; ++i)
    {
        int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!joybuttons[i] && button_on)
        {
            // Weapon cycling:

            if (i == joybprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == joybnextweapon)
            {
                next_weapon = 1;
            }
        }

        joybuttons[i] = button_on;
    }
}

// If an InventoryMove*() function is called when the inventory is not active,
// it will instead activate the inventory without attempting to change the
// selected item. This action is indicated by a return value of false.
// Otherwise, it attempts to change items and will return a value of true.

static boolean InventoryMoveLeft(void)
{
    inventoryTics = 5 * TICRATE;

    // [JN] Do not pop-up while active menu or demo playback.
    if (MenuActive || demoplayback)
    {
        return false;
    }

    if (!inventory)
    {
        inventory = true;
        return false;
    }
    inv_ptr--;
    if (inv_ptr < 0)
    {
        inv_ptr = 0;
    }
    else
    {
        curpos--;
        if (curpos < 0)
        {
            curpos = 0;
        }
    }
    return true;
}

static boolean InventoryMoveRight(void)
{
    const player_t *plr;

    plr = &players[consoleplayer];
    inventoryTics = 5 * TICRATE;

    // [JN] Do not pop-up while active menu or demo playback.
    if (MenuActive || demoplayback)
    {
        return false;
    }

    if (!inventory)
    {
        inventory = true;
        return false;
    }
    inv_ptr++;
    if (inv_ptr >= plr->inventorySlotNum)
    {
        inv_ptr--;
        if (inv_ptr < 0)
            inv_ptr = 0;
    }
    else
    {
        curpos++;
        if (curpos > CURPOS_MAX)
        {
            curpos = CURPOS_MAX;
        }
    }
    return true;
}

static void SetMouseButtons(unsigned int buttons_mask)
{
    int i;
    player_t *plr;

    plr = &players[consoleplayer];

    for (i=0; i<MAX_MOUSE_BUTTONS; ++i)
    {
        unsigned int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!mousebuttons[i] && button_on)
        {
            // [JN] CRL - move spectator camera up/down.
            if (crl_spectating && !MenuActive)
            {
                if (i == 4)  // Hardcoded mouse wheel down
                {
                    CRL_ImpulseCameraVert(false, crl_camzspeed ? 64 : 32); 
                }
                else
                if (i == 3)  // Hardcoded Mouse wheel down
                {
                    CRL_ImpulseCameraVert(true, crl_camzspeed ? 64 : 32);
                }
            }
            else
            {
            if (i == mousebprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == mousebnextweapon)
            {
                next_weapon = 1;
            }
            else if (i == mousebuse)
            {
                // [PN] Mouse wheel "use" workaround: some mouse buttons (e.g. wheel click)
                // generate only a single tick event. We simulate a short BT_USE press here.
                basecmd.buttons |= BT_USE;
            }
            else if (i == mousebinvleft)
            {
                InventoryMoveLeft();
            }
            else if (i == mousebinvright)
            {
                InventoryMoveRight();
            }
            else if (i == mousebuseartifact)
            {
                if (!inventory)
                {
                    plr->readyArtifact = plr->inventory[inv_ptr].type;
                }
                usearti = true;
            }
            }
        }

        mousebuttons[i] = button_on;
    }
}

/*
===============================================================================
=
= G_Responder 
=
= get info needed to make ticcmd_ts for the players
=
===============================================================================
*/

boolean G_Responder(event_t * ev)
{
    player_t *plr;

    plr = &players[consoleplayer];

    // [crispy] demo pause (from prboom-plus)
    if (gameaction == ga_nothing && 
        (demoplayback || gamestate == GS_INTERMISSION))
    {
        if (ev->type == ev_keydown && ev->data1 == key_pause)
        {
            if (paused ^= 2)
                S_PauseSound();
            else
                S_ResumeSound();
            return true;
        }
    }

    // [crispy] demo fast-forward
    if (ev->type == ev_keydown && ev->data1 == key_demospeed
    && (demoplayback || gamestate == GS_DEMOSCREEN))
    {
        singletics = !singletics;
        return (true);
    }

    if (ev->type == ev_keyup && ev->data1 == key_useartifact)
    {                           // flag to denote that it's okay to use an artifact
        if (!inventory)
        {
            plr->readyArtifact = plr->inventory[inv_ptr].type;
        }
        usearti = true;
    }

    // Check for spy mode player cycle
    if (gamestate == GS_LEVEL && ev->type == ev_keydown
        && ev->data1 == key_spy && !deathmatch)
    {                           // Cycle the display player
        do
        {
            displayplayer++;
            if (displayplayer == maxplayers)
            {
                displayplayer = 0;
            }
            // [JN] Re-set appropriate assembled weapon widget graphics.
            SB_SetClassData();
        }
        while (!playeringame[displayplayer]
               && displayplayer != consoleplayer);
        // [JN] Refresh status bar.
        SB_ForceRedraw();
        // [JN] Update sound values for appropriate player.
        S_UpdateSounds(players[displayplayer].mo);
        // [JN] Re-init automap variables for correct player arrow angle.
        if (automapactive)
        AM_initVariables();
        return (true);
    }

    // [JN] CRL - same to Doom behavior:
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && !askforquit
    && (demoplayback || gamestate == GS_DEMOSCREEN)) 
    {
        if (ev->type == ev_keydown
        || (ev->type == ev_mouse && ev->data1 &&
           !ev->data2 && !ev->data3)  // [JN] Do not consider mouse movement as pressing.
        || (ev->type == ev_joystick && ev->data1))
        {
            MN_ActivateMenu (); 
            return (true); 
        } 
        return (false); 
    } 

    if (CT_Responder(ev))
    {                           // Chat ate the event
        return (true);
    }
    if (gamestate == GS_LEVEL)
    {
        if (SB_Responder(ev))
        {                       // Status bar ate the event
            return (true);
        }
        if (AM_Responder(ev))
        {                       // Automap ate the event
            return (true);
        }

        if (players[consoleplayer].cheatTics)
        {
            // [JN] Reset cheatTics if user have opened menu or moved/pressed mouse buttons.
            if (MenuActive || ev->type == ev_mouse)
            {
                players[consoleplayer].cheatTics = 0;
            }
            // [JN] Prevent other keys while cheatTics is running after typing "ID".
            if (players[consoleplayer].cheatTics > 0)
            {
                return true;
            }
        }
    }

    if (ev->type == ev_mouse)
    {
        testcontrols_mousespeed = abs(ev->data2);
    }

    if (ev->type == ev_keydown && ev->data1 == key_prevweapon)
    {
        next_weapon = -1;
    }
    else if (ev->type == ev_keydown && ev->data1 == key_nextweapon)
    {
        next_weapon = 1;
    }

    switch (ev->type)
    {
        case ev_keydown:
            if (ev->data1 == key_invleft)
            {
                if (InventoryMoveLeft())
                {
                    return (true);
                }
                break;
            }
            if (ev->data1 == key_invright)
            {
                if (InventoryMoveRight())
                {
                    return (true);
                }
                break;
            }
            if (ev->data1 == key_pause && !MenuActive)
            {
                sendpause = true;
                return (true);
            }
            if (ev->data1 < NUMKEYS)
            {
                gamekeydown[ev->data1] = true;
            }
            // [JN] Flip level horizontally.
            if (ev->data1 == key_flip_levels)
            {
                gp_flip_levels ^= 1;
                // Redraw game screen
                R_ExecuteSetViewSize();
                // Audible feedback
                S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
            }
            // [JN] CRL - Toggle extended HUD.
            if (ev->data1 == key_widget_enable)
            {
                widget_enable ^= 1;
                CT_SetMessage(&players[consoleplayer], widget_enable ?
                              ID_EXTHUD_ON : ID_EXTHUD_OFF, false, NULL);
                // Redraw status bar to possibly clean up 
                // remainings of demo progress bar.
                SB_state = -1;
            }
            // [JN] CRL - Toggle spectator mode.
            if (ev->data1 == key_spectator)
            {
                crl_spectating ^= 1;
                CT_SetMessage(&players[consoleplayer], crl_spectating ?
                             ID_SPECTATOR_ON : ID_SPECTATOR_OFF, false, NULL);
            }  
            // [JN] CRL - Toggle freeze mode.
            if (ev->data1 == key_freeze)
            {
                // Allow freeze only in single player game, otherwise desyncs may occur.
                if (demorecording)
                {
                    CT_SetMessage(&players[consoleplayer], ID_FREEZE_NA_R , false, NULL);
                    return true;
                }            
                if (demoplayback)
                {
                    CT_SetMessage(&players[consoleplayer], ID_FREEZE_NA_P , false, NULL);
                    return true;
                }   
                if (netgame)
                {
                    CT_SetMessage(&players[consoleplayer], ID_FREEZE_NA_N , false, NULL);
                    return true;
                }   
                crl_freeze ^= 1;

                CT_SetMessage(&players[consoleplayer], crl_freeze ?
                             ID_FREEZE_ON : ID_FREEZE_OFF, false, NULL);
            }
            // [JN] CRL - Toggle notarget mode.
            if (ev->data1 == key_notarget)
            {
                player_t *player = &players[consoleplayer];

                // Allow notarget only in single player game, otherwise desyncs may occur.
                if (demorecording)
                {
                    CT_SetMessage(&players[consoleplayer], ID_NOTARGET_NA_R, false, NULL);
                    return true;
                }
                if (demoplayback)
                {
                    CT_SetMessage(&players[consoleplayer], ID_NOTARGET_NA_P, false, NULL);
                    return true;
                }
                if (netgame)
                {
                    CT_SetMessage(&players[consoleplayer], ID_NOTARGET_NA_N, false, NULL);
                    return true;
                }   

                player->cheats ^= CF_NOTARGET;
                P_ForgetPlayer(player);

                CT_SetMessage(player, player->cheats & CF_NOTARGET ?
                            ID_NOTARGET_ON : ID_NOTARGET_OFF, false, NULL);
            }
            // [JN] Woof - Toggle Buddha mode.
            if (ev->data1 == key_buddha)
            {
                player_t *player = &players[consoleplayer];

                // Allow notarget only in single player game, otherwise desyncs may occur.
                if (demorecording)
                {
                    CT_SetMessage(&players[consoleplayer], ID_BUDDHA_NA_R, false, NULL);
                    return true;
                }
                if (demoplayback)
                {
                    CT_SetMessage(&players[consoleplayer], ID_BUDDHA_NA_P, false, NULL);
                    return true;
                }
                if (netgame)
                {
                    CT_SetMessage(&players[consoleplayer], ID_BUDDHA_NA_N, false, NULL);
                    return true;
                }
                player->cheats ^= CF_BUDDHA;
                CT_SetMessage(player, player->cheats & CF_BUDDHA ?
                            ID_BUDDHA_ON : ID_BUDDHA_OFF, false, NULL);
            }
            return (true);      // eat key down events

        case ev_keyup:
            if (ev->data1 < NUMKEYS)
            {
                gamekeydown[ev->data1] = false;
            }
            return (false);     // always let key up events filter down

        case ev_mouse:
            // [JN] Do not treat mouse movement as a button press.
            // Prevents the player from firing a weapon when holding LMB
            // and moving the mouse after loading the game.
            if (!ev->data2 && !ev->data3)
            SetMouseButtons(ev->data1);
            mousex += ev->data2;
            mousey += ev->data3;
            return (true);      // eat events

        case ev_joystick:
            SetJoyButtons(ev->data1);
            joyxmove = ev->data2;
            joyymove = ev->data3;
            joystrafemove = ev->data4;
            joylook = ev->data5;
            return (true);      // eat events

        default:
            break;
    }
    return (false);
}

// [crispy] For fast polling.
void G_FastResponder (void)
{
    if (newfastmouse)
    {
        mousex += fastmouse.data2;
        mousey += fastmouse.data3;

        newfastmouse = false;
    }
}

// [crispy]
void G_PrepTiccmd (void)
{
    const boolean strafe = gamekeydown[key_strafe] ||
        mousebuttons[mousebstrafe] || joybuttons[joybstrafe];

    // [JN] Deny camera rotation/looking while active menu in multiplayer.
    if (netgame && (MenuActive || askforquit))
    {
        mousex = 0;
        mousey = 0;
        return;
    }

    if (mousex && !strafe)
    {
        localview.rawangle -= CalcMouseAngle(mousex);
        basecmd.angleturn = CarryAngle(localview.rawangle);
        localview.angle = gp_flip_levels ?
            -(basecmd.angleturn << 16) : (basecmd.angleturn << 16);
        mousex = 0;
    }

    if (mousey && mouse_look && !crl_spectating)
    {
        const double vert = CalcMouseVert(mousey);
        basecmd.lookdir += mouse_y_invert ?
                            CarryPitch(-vert): CarryPitch(vert);
        mousey = 0;
    }
}


//==========================================================================
//
// G_Ticker
//
//==========================================================================

void G_Ticker(void)
{
    int i, buf;
    ticcmd_t *cmd = NULL;

//
// do player reborns if needed
//
    for (i = 0; i < maxplayers; i++)
        if (playeringame[i] && players[i].playerstate == PST_REBORN)
            G_DoReborn(i);

//
// do things to change the game state
//
    while (gameaction != ga_nothing)
    {
        switch (gameaction)
        {
            case ga_loadlevel:
                G_DoLoadLevel();
                break;
            case ga_initnew:
                G_DoInitNew();
                break;
            case ga_newgame:
                G_DoNewGame();
                break;
            case ga_loadgame:
                // [JN] Make optional (save/load/travel)
                if (vid_banners == 1)
                {
                    Draw_LoadIcon();
                }
                G_DoLoadGame();
                break;
            case ga_savegame:
                // [JN] Make optional (save/load/travel)
                if (vid_banners == 1)
                {
                    Draw_SaveIcon();
                }
                G_DoSaveGame();
                break;
            case ga_singlereborn:
                G_DoSingleReborn();
                break;
            case ga_playdemo:
                G_DoPlayDemo();
                break;
            case ga_screenshot:
                V_ScreenShot("HEXEN%02i.%s");
                gameaction = ga_nothing;
                break;
            case ga_leavemap:
                // [JN] Make optional (save/load/travel or travel only)
                if (vid_banners)
                {
                    Draw_TeleportIcon();
                }
                G_DoTeleportNewMap();
                break;
            case ga_completed:
                G_DoCompleted();
                break;
            case ga_worlddone:
                G_DoWorldDone();
                break;
            case ga_victory:
                F_StartFinale();
                break;
            default:
                break;
        }
    }


    // [crispy] demo sync of revenant tracers and RNG (from prboom-plus)
    if (paused & 2 || (!demoplayback && MenuActive && !netgame))
    {
        // [JN] Means: no-op! Stop tics from running while demo is paused.
    }
    else
    {
//
// get commands, check consistancy, and build new consistancy check
//
    //buf = gametic%BACKUPTICS;
    buf = (gametic / ticdup) % BACKUPTICS;

    for (i = 0; i < maxplayers; i++)
        if (playeringame[i])
        {
            cmd = &players[i].cmd;

            memcpy(cmd, &netcmds[i], sizeof(ticcmd_t));

            if (demoplayback)
                G_ReadDemoTiccmd(cmd);
            if (demorecording)
                G_WriteDemoTiccmd(cmd);

            if (netgame && !netdemo && !(gametic % ticdup))
            {
                if (gametic > BACKUPTICS
                    && consistancy[i][buf] != cmd->consistancy)
                {
                    I_Error("consistency failure (%i should be %i)",
                            cmd->consistancy, consistancy[i][buf]);
                }
                if (players[i].mo)
                    consistancy[i][buf] = players[i].mo->x;
                else
                    consistancy[i][buf] = rndindex;
            }
        }

    // [crispy] increase demo tics counter
    if (demoplayback || demorecording)
    {
        defdemotics++;
    }

//
// check for special buttons
//
    for (i = 0; i < maxplayers; i++)
        if (playeringame[i])
        {
            if (players[i].cmd.buttons & BT_SPECIAL)
            {
                switch (players[i].cmd.buttons & BT_SPECIALMASK)
                {
                    case BTS_PAUSE:
                        paused ^= 1;
                        if (paused)
                        {
                            S_PauseSound();
                        }
                        else
                        {
                            S_ResumeSound();
                        }
                        break;

                    case BTS_SAVEGAME:
                        if (!savedescription[0])
                        {
                            if (netgame)
                            {
                                M_StringCopy(savedescription, "NET GAME",
                                             sizeof(savedescription));
                            }
                            else
                            {
                                M_StringCopy(savedescription, "SAVE GAME",
                                             sizeof(savedescription));
                            }
                        }
                        savegameslot =
                            (players[i].cmd.
                             buttons & BTS_SAVEMASK) >> BTS_SAVESHIFT;
                        gameaction = ga_savegame;
                        break;
                }
            }
        }
    }
    // turn inventory off after a certain amount of time
    if (inventory && !(--inventoryTics))
    {
        players[consoleplayer].readyArtifact =
            players[consoleplayer].inventory[inv_ptr].type;
        inventory = false;
    }

    oldleveltime = realleveltime; // [crispy] Track if game is running

    // [crispy] no pause at intermission screen during demo playback 
    // to avoid desyncs (from prboom-plus)
    if ((paused & 2 || (!demoplayback && MenuActive && !netgame)) 
        && gamestate == GS_INTERMISSION)
    {
    return;
    }

//
// do main actions
//
//
// do main actions
//
    switch (gamestate)
    {
        case GS_LEVEL:
            P_Ticker();
            SB_Ticker();
            AM_Ticker();
            // [JN] Not really needed in single player game.
            if (netgame)
            {
                CT_Ticker();
            }
            // [JN] Target's health widget.
            if (widget_health || (xhair_draw && xhair_color > 1))
            {
                player_t *player = &players[displayplayer];
                // Do an overflow-safe trace to gather target's health.
                P_AimLineAttack(player->mo, player->mo->angle, MISSILERANGE, true);
            }
            break;
        case GS_INTERMISSION:
            IN_Ticker();
            break;
        case GS_FINALE:
            F_Ticker();
            break;
        case GS_DEMOSCREEN:
            H2_PageTicker();
            break;
    }

    // [JN] Reduce message tics independently from framerate and game states.
    // Tics can't go negative.
    MSG_Ticker();

    //
    // [JN] Query time for time-related widgets:
    //

    // Total time
    if (widget_totaltime)
    {
        const int worldTimer = players[consoleplayer].worldTimer;
        const int hours = worldTimer / (3600 * TICRATE);
        const int mins = worldTimer / (60 * TICRATE) % 60;
        const int secs = (worldTimer % (60 * TICRATE)) / TICRATE;

        M_snprintf(ID_Total_Time, sizeof(ID_Total_Time), "%02i:%02i:%02i", hours, mins, secs);
    }
    // Local time
    if (msg_local_time)
    {
        time_t t = time(NULL);
        const struct tm *const tm = localtime(&t);

        if (msg_local_time == 1)
        {
            // 12-hour (HH:MM AM/PM)
            // [PN] Always show AM/PM independently of the system locale
            snprintf(ID_Local_Time, sizeof(ID_Local_Time), "%d:%02d %s",
                    (tm->tm_hour % 12 == 0) ? 12 : tm->tm_hour % 12, tm->tm_min,
                    (tm->tm_hour >= 12) ? "PM" : "AM");
        }
        else
        {
            // 24-hour (HH:MM)
            snprintf(ID_Local_Time, sizeof(ID_Local_Time), "%d:%02d", tm->tm_hour, tm->tm_min);
        }
    }
}


/*
==============================================================================

						PLAYER STRUCTURE FUNCTIONS

also see P_SpawnPlayer in P_Things
==============================================================================
*/

//==========================================================================
//
// G_PlayerExitMap
//
// Called when the player leaves a map.
//
//==========================================================================

void G_PlayerExitMap(int playerNumber)
{
    int i;
    player_t *player;
    int flightPower;

    player = &players[playerNumber];

//      if(deathmatch)
//      {
//              // Strip all but one of each type of artifact
//              for(i = 0; i < player->inventorySlotNum; i++)
//              {
//                      player->inventory[i].count = 1;
//              }
//              player->artifactCount = player->inventorySlotNum;
//      }
//      else

    // Strip all current powers (retain flight)
    flightPower = player->powers[pw_flight];
    memset(player->powers, 0, sizeof(player->powers));
    player->powers[pw_flight] = flightPower;

    if (deathmatch)
    {
        player->powers[pw_flight] = 0;
    }
    else
    {
        if (P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap))
        {                       // Entering new cluster
            // Strip all keys
            player->keys = 0;

            // Strip flight artifact
            for (i = 0; i < 25; i++)
            {
                player->powers[pw_flight] = 0;
                P_PlayerUseArtifact(player, arti_fly);
            }
            player->powers[pw_flight] = 0;
        }
    }

    if (player->morphTics)
    {
        player->readyweapon = player->mo->special1.i;     // Restore weapon
        player->morphTics = 0;
    }
    player->messageTics = 0;
    player->targetsheathTics = 0;
    player->lookdir = 0;
    player->mo->flags &= ~MF_SHADOW;    // Remove invisibility
    player->extralight = 0;     // Remove weapon flashes
    player->fixedcolormap = 0;  // Remove torch
    player->damagecount = 0;    // No palette changes
    player->bonuscount = 0;
    player->poisoncount = 0;
    // [JN] Reset smooth palette fading for Wraithwerge and Bloodscourge.
    player->graycount = player->orngcount = 0;
    if (player == &players[consoleplayer])
    {
        SB_state = -1;          // refresh the status bar
        viewangleoffset = 0;
    }
    // [JN] Return controls to the player.
    crl_spectating = 0;
}

//==========================================================================
//
// G_PlayerReborn
//
// Called after a player dies.  Almost everything is cleared and
// initialized.
//
//==========================================================================

void G_PlayerReborn(int player)
{
    player_t *p;
    int frags[MAXPLAYERS];
    int killcount, itemcount, secretcount;
    unsigned int worldTimer;

    memcpy(frags, players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;
    worldTimer = players[player].worldTimer;

    p = &players[player];
    memset(p, 0, sizeof(*p));

    memcpy(players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;
    players[player].worldTimer = worldTimer;
    players[player].class = PlayerClass[player];

    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;
    p->readyweapon = p->pendingweapon = WP_FIRST;
    p->weaponowned[WP_FIRST] = true;
    p->messageTics = 0;
    p->targetsheathTics = 0;
    p->lookdir = 0;
    localQuakeHappening[player] = false;
    if (p == &players[consoleplayer])
    {
        SB_state = -1;          // refresh the status bar
        inv_ptr = 0;            // reset the inventory pointer
        curpos = 0;
        viewangleoffset = 0;
    }
}

/*
====================
=
= G_CheckSpot 
=
= Returns false if the player cannot be respawned at the given mapthing_t spot 
= because something is occupying it
====================
*/

static boolean G_CheckSpot(int playernum, const mapthing_t *mthing)
{
    fixed_t x, y;
    subsector_t *ss;
    unsigned an;
    mobj_t *mo;

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    players[playernum].mo->flags2 &= ~MF2_PASSMOBJ;
    if (!P_CheckPosition(players[playernum].mo, x, y))
    {
        players[playernum].mo->flags2 |= MF2_PASSMOBJ;
        return false;
    }
    players[playernum].mo->flags2 |= MF2_PASSMOBJ;

// spawn a teleport fog
    ss = R_PointInSubsector(x, y);
    an = ((unsigned) ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

    mo = P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                     ss->sector->floorheight + TELEFOGHEIGHT, MT_TFOG);
    if (players[consoleplayer].viewz != 1)
        S_StartSound(mo, SFX_TELEPORT); // don't start sound on first frame

    return true;
}

/*
====================
=
= G_DeathMatchSpawnPlayer
=
= Spawns a player at one of the random death match spots
= called at level load and each death
====================
*/

void G_DeathMatchSpawnPlayer(int playernum)
{
    int i, j;
    int selections;

    selections = deathmatch_p - deathmatchstarts;

    // This check has been moved to p_setup.c:P_LoadThings()
    //if (selections < 8)
    //      I_Error ("Only %i deathmatch spots, 8 required", selections);

    for (j = 0; j < 20; j++)
    {
        i = P_Random() % selections;
        if (G_CheckSpot(playernum, &deathmatchstarts[i]))
        {
            deathmatchstarts[i].type = playernum + 1;
            P_SpawnPlayer(&deathmatchstarts[i]);
            return;
        }
    }

// no good spot, so the player will probably get stuck
    P_SpawnPlayer(&playerstarts[0][playernum]);
}

//==========================================================================
//
// G_DoReborn
//
//==========================================================================

void G_DoReborn(int playernum)
{
    int i;
    boolean oldWeaponowned[NUMWEAPONS];
    int oldKeys;
    int oldPieces;
    boolean foundSpot;
    int bestWeapon;

    // quit demo unless -demoextend
    if (!demoextend && G_CheckDemoStatus())
    {
        return;
    }
    if (!netgame)
    {
        if (SV_RebornSlotAvailable())
        {                       // Use the reborn code if the slot is available
            gameaction = ga_singlereborn;
        }
        else
        {                       // Start a new game if there's no reborn info
            gameaction = ga_newgame;
        }
    }
    else
    {                           // Net-game
        players[playernum].mo->player = NULL;   // Dissassociate the corpse

        if (deathmatch)
        {                       // Spawn at random spot if in death match
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

        // Cooperative net-play, retain keys and weapons
        oldKeys = players[playernum].keys;
        oldPieces = players[playernum].pieces;
        for (i = 0; i < NUMWEAPONS; i++)
        {
            oldWeaponowned[i] = players[playernum].weaponowned[i];
        }

        foundSpot = false;
        if (G_CheckSpot(playernum, &playerstarts[RebornPosition][playernum]))
        {                       // Appropriate player start spot is open
            P_SpawnPlayer(&playerstarts[RebornPosition][playernum]);
            foundSpot = true;
        }
        else
        {
            // Try to spawn at one of the other player start spots
            for (i = 0; i < maxplayers; i++)
            {
                if (G_CheckSpot(playernum, &playerstarts[RebornPosition][i]))
                {               // Found an open start spot

                    // Fake as other player
                    playerstarts[RebornPosition][i].type = playernum + 1;
                    P_SpawnPlayer(&playerstarts[RebornPosition][i]);

                    // Restore proper player type
                    playerstarts[RebornPosition][i].type = i + 1;

                    foundSpot = true;
                    break;
                }
            }
        }

        if (foundSpot == false)
        {                       // Player's going to be inside something
            P_SpawnPlayer(&playerstarts[RebornPosition][playernum]);
        }

        // Restore keys and weapons
        players[playernum].keys = oldKeys;
        players[playernum].pieces = oldPieces;
        for (bestWeapon = 0, i = 0; i < NUMWEAPONS; i++)
        {
            if (oldWeaponowned[i])
            {
                bestWeapon = i;
                players[playernum].weaponowned[i] = true;
            }
        }
        players[playernum].mana[MANA_1] = 25;
        players[playernum].mana[MANA_2] = 25;
        if (bestWeapon)
        {                       // Bring up the best weapon
            players[playernum].pendingweapon = bestWeapon;
        }
    }
}

void G_ScreenShot(void)
{
    gameaction = ga_screenshot;
}

//==========================================================================
//
// G_StartNewInit
//
//==========================================================================

void G_StartNewInit(void)
{
    SV_InitBaseSlot();
    SV_ClearRebornSlot();
    P_ACSInitNewGame();
    // Default the player start spot group to 0
    RebornPosition = 0;
}

//==========================================================================
//
// G_StartNewGame
//
//==========================================================================

void G_StartNewGame(skill_t skill)
{
    int realMap;

    G_StartNewInit();
    realMap = P_TranslateMap(1);
    if (realMap == -1)
    {
        realMap = 1;
    }
    G_InitNew(TempSkill, 1, realMap);
}

//==========================================================================
//
// G_TeleportNewMap
//
// Only called by the warp cheat code.  Works just like normal map to map
// teleporting, but doesn't do any interlude stuff.
//
//==========================================================================

void G_TeleportNewMap(int map, int position)
{
    gameaction = ga_leavemap;
    LeaveMap = map;
    LeavePosition = position;
}

//==========================================================================
//
// G_DoTeleportNewMap
//
//==========================================================================

void G_DoTeleportNewMap(void)
{
    SV_MapTeleport(LeaveMap, LeavePosition);
    gamestate = GS_LEVEL;
    gameaction = ga_nothing;
    RebornPosition = LeavePosition;
}

/*
boolean secretexit;
void G_ExitLevel (void)
{
	secretexit = false;
	gameaction = ga_completed;
}
void G_SecretExitLevel (void)
{
	secretexit = true;
	gameaction = ga_completed;
}
*/

//==========================================================================
//
// G_Completed
//
// Starts intermission routine, which is used only during hub exits,
// and DeathMatch games.
//==========================================================================

void G_Completed(int map, int position)
{
    if (gamemode == shareware && map > 4)
    {
        CT_SetMessage(&players[consoleplayer], "ACCESS DENIED -- DEMO", true, NULL);
        S_StartSound(NULL, SFX_CHAT);
        return;
    }

    gameaction = ga_completed;
    LeaveMap = map;
    LeavePosition = position;
}

void G_DoCompleted(void)
{
    int i;

    gameaction = ga_nothing;

    // quit demo unless -demoextend
    if (!demoextend && G_CheckDemoStatus())
    {
        return;
    }
    for (i = 0; i < maxplayers; i++)
    {
        if (playeringame[i])
        {
            G_PlayerExitMap(i);
        }
    }
    if (LeaveMap == -1 && LeavePosition == -1)
    {
        gameaction = ga_victory;
        return;
    }
    else
    {
        gamestate = GS_INTERMISSION;
        IN_Start();
    }

/*
	int i;
	static int afterSecret[3] = { 7, 5, 5 };

	gameaction = ga_nothing;
	if(G_CheckDemoStatus())
	{
		return;
	}
	for(i = 0; i < maxplayers; i++)
	{
		if(playeringame[i])
		{
			G_PlayerFinishLevel(i);
		}
	}
	prevmap = gamemap;
	if(secretexit == true)
	{
		gamemap = 9;
	}
	else if(gamemap == 9)
	{ // Finished secret level
		gamemap = afterSecret[gameepisode-1];
	}
	else if(gamemap == 8)
	{
		gameaction = ga_victory;
		return;
	}
	else
	{
		gamemap++;
	}
	gamestate = GS_INTERMISSION;
	IN_Start();
*/
}

//============================================================================
//
// G_WorldDone
//
//============================================================================

void G_WorldDone(void)
{
    gameaction = ga_worlddone;
}

//============================================================================
//
// G_DoWorldDone
//
//============================================================================

void G_DoWorldDone(void)
{
    gamestate = GS_LEVEL;
    G_DoLoadLevel();
    gameaction = ga_nothing;
    viewactive = true;
    // [JN] jff 4/12/98 clear any marks on the automap
    AM_ClearMarks();
}

//==========================================================================
//
// G_DoSingleReborn
//
// Called by G_Ticker based on gameaction.  Loads a game from the reborn
// save slot.
//
//==========================================================================

void G_DoSingleReborn(void)
{
    gameaction = ga_nothing;
    SV_LoadGame(SV_GetRebornSlot());
    SB_SetClassData();
}

//==========================================================================
//
// G_LoadGame
//
// Can be called by the startup code or the menu task.
//
//==========================================================================

static int GameLoadSlot;
// [JN] & [plums] "On Death Action" feature: unlike Doom and Heretic,
// Hexen mostly handles save slots via their numbers, not file names.
// "-1" means "no last save slot".
int OnDeathLoadSlot = -1;

void G_LoadGame(int slot)
{
    GameLoadSlot = slot;
    gameaction = ga_loadgame;
}

//==========================================================================
//
// G_DoLoadGame
//
// Called by G_Ticker based on gameaction.
//
//==========================================================================

void G_DoLoadGame(void)
{
    gameaction = ga_nothing;
    // [crispy] support multiple pages of saves
    SV_LoadGame(GameLoadSlot + savepage * 10);
    if (!netgame)
    {                           // Copy the base slot to the reborn slot
        SV_UpdateRebornSlot();
    }
    SB_SetClassData();

    // Draw the pattern into the back screen
    R_FillBackScreen();

    // [crispy] if the player is dead in this savegame,
    // do not consider it for reload
    if (players[consoleplayer].health <= 0)
	OnDeathLoadSlot = -1;

    // [JN] If "On death action" is set to "last save",
    // then prevent holded "use" button to work for next few tics.
    // This fixes imidiate pressing on wall upon reloading
    // a save game, if "use" button is kept pressed.
    if (singleplayer && gp_death_use_action == 1)
	players[consoleplayer].usedown = true;
}

//==========================================================================
//
// G_SaveGame
//
// Called by the menu task.  <description> is a 24 byte text string.
//
//==========================================================================

void G_SaveGame(int slot, char *description)
{
    savegameslot = slot;
    M_StringCopy(savedescription, description, sizeof(savedescription));
    sendsave = true;
    OnDeathLoadSlot = slot;
}

//==========================================================================
//
// G_DoSaveGame
//
// Called by G_Ticker based on gameaction.
//
//==========================================================================

void G_DoSaveGame(void)
{
    // [crispy] support multiple pages of saves
    SV_SaveGame(savegameslot + savepage * 10, savedescription);
    gameaction = ga_nothing;
    savedescription[0] = 0;
    CT_SetMessage(&players[consoleplayer], TXT_GAMESAVED, false, NULL);

    // Draw the pattern into the back screen
    R_FillBackScreen();
}

//==========================================================================
//
// G_DeferredNewGame
//
//==========================================================================

void G_DeferredNewGame(skill_t skill)
{
    TempSkill = skill;
    gameaction = ga_newgame;
}

//==========================================================================
//
// G_DoNewGame
//
//==========================================================================

void G_DoNewGame(void)
{
    G_StartNewGame(TempSkill);
    gameaction = ga_nothing;
}

/*
====================
=
= G_InitNew
=
= Can be called by the startup code or the menu task
= consoleplayer, displayplayer, playeringame[] should be set
====================
*/

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
    TempSkill = skill;
    TempEpisode = episode;
    TempMap = map;
    gameaction = ga_initnew;

    // [crispy] if a new game is started during demo recording, start a new demo
    if (demorecording)
    {
	G_CheckDemoStatus();
	Z_Free(demoname);
	G_RecordDemo(skill, 1, episode, map, orig_demoname);
    }
}

void G_DoInitNew(void)
{
    SV_InitBaseSlot();
    G_InitNew(TempSkill, TempEpisode, TempMap);
    gameaction = ga_nothing;
}

void G_InitNew(skill_t skill, int episode, int map)
{
    int i;

    if (paused)
    {
        paused = false;
        S_ResumeSound();
    }
    if (skill < sk_baby)
    {
        skill = sk_baby;
    }
    if (skill > sk_nightmare)
    {
        skill = sk_nightmare;
    }
    if (map < 1)
    {
        map = 1;
    }
    if (map > 99)
    {
        map = 99;
    }
    M_ClearRandom();
    // Force players to be initialized upon first level load
    for (i = 0; i < maxplayers; i++)
    {
        players[i].playerstate = PST_REBORN;
        players[i].worldTimer = 0;
    }

    // Set up a bunch of globals
    // [crispy] since demoextend is the default, we also want to check to
    // make sure we're not playing a demo
    if (!demoextend || (!demorecording && !demoplayback))
    {
        // This prevents map-loading from interrupting a demo.
        // demoextend is set back to false only if starting a new game or
        // loading a saved one from the menu, and only during playback.
        demorecording = false;
        demoplayback = false;
        netdemo = false;
        usergame = true;            // will be set false if a demo
    }
    paused = false;
    viewactive = true;
    // [JN] Reset automap scale. Fixes:
    // https://doomwiki.org/wiki/Automap_scale_preserved_after_warps_in_Heretic_and_Hexen
    automapactive = false; 
    // [JN] jff 4/16/98 force marks on automap cleared every new level start
    AM_ClearMarks();
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    // [JN] No-op! Do not reset demo tics between hubs and levels,
    // so demo timer and bar will show proper time and lenght.
    // defdemotics = 0;

    // Initialize the sky
    R_InitSky(map);

    // Give one null ticcmd_t
    //gametic = 0;
    //maketic = 1;
    //for (i=0 ; i<maxplayers ; i++)
    //      nettics[i] = 1; // one null event for this gametic
    //memset (localcmds,0,sizeof(localcmds));
    //memset (netcmds,0,sizeof(netcmds));

    G_DoLoadLevel();
}

/*
===============================================================================

							DEMO RECORDING

===============================================================================
*/

#define DEMOMARKER      0x80
#define DEMOHEADER_RESPAWN    0x20
#define DEMOHEADER_LONGTICS   0x10
#define DEMOHEADER_NOMONSTERS 0x02

// [crispy] demo progress bar and timer widget
int defdemotics = 0, deftotaldemotics;

static void G_ReadDemoTiccmd(ticcmd_t * cmd)
{
    if (*demo_p == DEMOMARKER)
    {                           // end of demo data stream
        G_CheckDemoStatus();
        return;
    }
    cmd->forwardmove = ((signed char) *demo_p++);
    cmd->sidemove = ((signed char) *demo_p++);

    // If this is a longtics demo, read back in higher resolution

    if (longtics)
    {
        cmd->angleturn = *demo_p++;
        cmd->angleturn |= (*demo_p++) << 8;
    }
    else
    {
        cmd->angleturn = ((unsigned char) *demo_p++) << 8;
    }

    cmd->buttons = (unsigned char) *demo_p++;
    cmd->lookfly = (unsigned char) *demo_p++;
    cmd->arti = (unsigned char) *demo_p++;
}

// Increase the size of the demo buffer to allow unlimited demos

static void IncreaseDemoBuffer(void)
{
    int current_length;
    byte *new_demobuffer;
    byte *new_demop;
    int new_length;

    // Find the current size

    current_length = demoend - demobuffer;

    // Generate a new buffer twice the size
    new_length = current_length * 2;

    new_demobuffer = Z_Malloc(new_length, PU_STATIC, 0);
    new_demop = new_demobuffer + (demo_p - demobuffer);

    // Copy over the old data

    memcpy(new_demobuffer, demobuffer, current_length);

    // Free the old buffer and point the demo pointers at the new buffer.

    Z_Free(demobuffer);

    demobuffer = new_demobuffer;
    demo_p = new_demop;
    demoend = demobuffer + new_length;
}

static void G_WriteDemoTiccmd(ticcmd_t * cmd)
{
    byte *demo_start;

    if (gamekeydown[key_demo_quit]) // press to end demo recording
        G_CheckDemoStatus();

    demo_start = demo_p;

    *demo_p++ = cmd->forwardmove;
    *demo_p++ = cmd->sidemove;

    // If this is a longtics demo, record in higher resolution

    if (longtics)
    {
        *demo_p++ = (cmd->angleturn & 0xff);
        *demo_p++ = (cmd->angleturn >> 8) & 0xff;
    }
    else
    {
        *demo_p++ = cmd->angleturn >> 8;
    }

    *demo_p++ = cmd->buttons;
    *demo_p++ = cmd->lookfly;
    *demo_p++ = cmd->arti;

    // reset demo pointer back
    demo_p = demo_start;

    if (demo_p > demoend - 16)
    {
        // [crispy] unconditionally disable savegame and demo limits
        /*
        if (vanilla_demo_limit)
        {
            // no more space
            G_CheckDemoStatus();
            return;
        }
        else
        */
        {
            // Vanilla demo limit disabled: unlimited
            // demo lengths!

            IncreaseDemoBuffer();
        }
    }

    G_ReadDemoTiccmd(cmd);      // make SURE it is exactly the same
}



/*
===================
=
= G_RecordDemo
=
===================
*/

void G_RecordDemo(skill_t skill, int numplayers, int episode, int map,
                  const char *name)
{
    size_t demoname_size;
    int i;
    int maxsize;

    // [crispy] demo file name suffix counter
    static unsigned int j = 0;
    FILE *fp = NULL;

    // [crispy] the name originally chosen for the demo, i.e. without "-00000"
    if (!orig_demoname)
    {
	orig_demoname = name;
    }

    //!
    // @category demo
    //
    // Record or playback a demo with high resolution turning.
    //

    longtics = D_NonVanillaRecord(M_ParmExists("-longtics"),
                                  "vvHeretic longtics demo");

    // If not recording a longtics demo, record in low res

    lowres_turn = !longtics;

    //!
    // @category demo
    //
    // Don't smooth out low resolution turning when recording a demo.
    //

    shortticfix = (!M_ParmExists("-noshortticfix"));
    //[crispy] make shortticfix the default

    G_InitNew(skill, episode, map);
    usergame = false;
    demoname_size = strlen(name) + 5 + 6; // [crispy] + 6 for "-00000"
    demoname = Z_Malloc(demoname_size, PU_STATIC, NULL);
    M_snprintf(demoname, demoname_size, "%s.lmp", name);
    maxsize = 0x20000;

    // [crispy] prevent overriding demos by adding a file name suffix
    for ( ; j <= 99999 && (fp = fopen(demoname, "rb")) != NULL; j++)
    {
	M_snprintf(demoname, demoname_size, "%s-%05d.lmp", name, j);
	fclose (fp);
    }

    //!
    // @arg <size>
    // @category demo
    // @vanilla
    //
    // Specify the demo buffer size (KiB)
    //

    i = M_CheckParmWithArgs("-maxdemo", 1);
    if (i)
        maxsize = atoi(myargv[i + 1]) * 1024;
    demobuffer = Z_Malloc(maxsize, PU_STATIC, NULL);
    demoend = demobuffer + maxsize;

    demo_p = demobuffer;
    *demo_p++ = skill;
    *demo_p++ = episode;
    *demo_p++ = map;

    // Write special parameter bits onto player one byte.
    // This aligns with vvHeretic demo usage. Hexen demo support has no
    // precedent here so consistency with another game is chosen:
    //   0x20 = -respawn
    //   0x10 = -longtics
    //   0x02 = -nomonsters

    *demo_p = 1; // assume player one exists
    if (D_NonVanillaRecord(respawnparm, "vvHeretic -respawn header flag"))
    {
        *demo_p |= DEMOHEADER_RESPAWN;
    }
    if (longtics)
    {
        *demo_p |= DEMOHEADER_LONGTICS;
    }
    if (D_NonVanillaRecord(nomonsters, "vvHeretic -nomonsters header flag"))
    {
        *demo_p |= DEMOHEADER_NOMONSTERS;
    }
    demo_p++;
    *demo_p++ = PlayerClass[0];

    for (i = 1; i < maxplayers; i++)
    {
        *demo_p++ = playeringame[i];
        *demo_p++ = PlayerClass[i];
    }

    demorecording = true;
}

/*
================================================================================
=
= G_DemoProgressBar
=
= [crispy] demo progress bar
=
================================================================================
*/

static void G_DemoProgressBar (const int lumplength)
{
    int   numplayersingame = 0;
    byte *demo_ptr = demo_p;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            numplayersingame++;
        }
    }

    deftotaldemotics = defdemotics = 0;

    while (*demo_ptr != DEMOMARKER && (demo_ptr - demobuffer) < lumplength)
    {
        // [JN] Note: Heretic using extra two pointers: lookfly and arti,
        // so unlike Doom (5 : 4) we using (7 : 6) here.
        // Thanks to Roman Fomin for pointing out.
        demo_ptr += numplayersingame * (longtics ? 7 : 6);
        deftotaldemotics++;
    }
}

/*
===================
=
= G_PlayDemo
=
===================
*/

static const char *defdemoname;

void G_DeferedPlayDemo(const char *name)
{
    defdemoname = name;
    gameaction = ga_playdemo;

    // [crispy] fast-forward demo up to the desired map
    if (demowarp)
    {
        nodrawers = true;
        singletics = true;
    }
}

void G_DoPlayDemo(void)
{
    skill_t skill;
    int i, lumpnum, episode, map;
    int lumplength; // [crispy]

    gameaction = ga_nothing;
    lumpnum = W_GetNumForName(defdemoname);
    demobuffer = W_CacheLumpNum(lumpnum, PU_STATIC);

    // [crispy] ignore empty demo lumps
    lumplength = W_LumpLength(lumpnum);
    if (lumplength < 0xd)
    {
        demoplayback = true;
        G_CheckDemoStatus();
        return;
    }

    demo_p = demobuffer;
    skill = *demo_p++;
    episode = *demo_p++;
    map = *demo_p++;

    // When recording we store some extra options inside the upper bits
    // of the player 1 present byte. However, this is a non-vanilla extension.
    // Note references to vvHeretic here; these are the extensions used by
    // vvHeretic, which we're just reusing for Hexen demos too. There is no
    // vvHexen.
    if (D_NonVanillaPlayback((*demo_p & DEMOHEADER_LONGTICS) != 0,
                             lumpnum, "vvHeretic longtics demo"))
    {
        longtics = true;
    }
    if (D_NonVanillaPlayback((*demo_p & DEMOHEADER_RESPAWN) != 0,
                             lumpnum, "vvHeretic -respawn header flag"))
    {
        respawnparm = true;
    }
    if (D_NonVanillaPlayback((*demo_p & DEMOHEADER_NOMONSTERS) != 0,
                             lumpnum, "vvHeretic -nomonsters header flag"))
    {
        nomonsters = true;
    }

    for (i = 0; i < maxplayers; i++)
    {
        playeringame[i] = (*demo_p++) != 0;
        PlayerClass[i] = *demo_p++;
    }

    if (playeringame[1] || M_ParmExists("-solo-net")
                        || M_ParmExists("-netdemo"))
    {
        netgame = true;
        solonet = true;
    }

    // Initialize world info, etc.
    G_StartNewInit();

    precache = false;           // don't spend a lot of time in loadlevel
    G_InitNew(skill, episode, map);
    precache = true;
    usergame = false;
    demoplayback = true;

    if (netgame)
    {
        netdemo = true;
    }

    // [JN] Set appropriate assembled weapon widget graphics.
    SB_SetClassData();

    // [crispy] demo progress bar
    G_DemoProgressBar(lumplength);
}


/*
===================
=
= G_TimeDemo
=
===================
*/

void G_TimeDemo(char *name)
{
    // [JN] Note: original "-timedemo" code causing desyncs on demo playback.
    // To simplify things, just play demo via G_DoPlayDemo functions, with
    // all necessary parameters for timing demo set.

    defdemoname = name;
    gameaction = ga_playdemo;
    timingdemo = true;
    singletics = true;
    starttime = I_GetTime();

    // [JN] Disable rendering the screen entirely, if needed.
    nodrawers = M_CheckParm ("-nodraw");

    /*
    skill_t skill;
    int episode, map, i;

    demobuffer = demo_p = W_CacheLumpName(name, PU_STATIC);
    skill = *demo_p++;
    episode = *demo_p++;
    map = *demo_p++;

    // Read special parameter bits: see G_RecordDemo() for details.
    longtics = (*demo_p & DEMOHEADER_LONGTICS) != 0;

    // don't overwrite arguments from the command line
    respawnparm |= (*demo_p & DEMOHEADER_RESPAWN) != 0;
    nomonsters  |= (*demo_p & DEMOHEADER_NOMONSTERS) != 0;

    for (i = 0; i < maxplayers; i++)
    {
        playeringame[i] = (*demo_p++) != 0;
        PlayerClass[i] = *demo_p++;
    }

    if (playeringame[1] || M_ParmExists("-solo-net")
                        || M_ParmExists("-netdemo"))
    {
        netgame = true;
    }

    G_InitNew(skill, episode, map);
    starttime = I_GetTime();

    usergame = false;
    demoplayback = true;
    timingdemo = true;
    singletics = true;

    if (netgame)
    {
        netdemo = true;
    }
    */
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

boolean G_CheckDemoStatus(void)
{
    if (timingdemo)
    {
        const int endtime = I_GetTime();
        const int realtics = endtime - starttime;
        const float fps = ((float)gametic * TICRATE) / realtics;

        // Prevent recursive calls
        timingdemo = false;
        demoplayback = false;

        i_error_safe = true;
        I_Error ("Timed %i gametics in %i realtics.\n"
                 "Average fps: %f", gametic, realtics, fps);
    }

    if (demoplayback)
    {
        if (singledemo)
            I_Quit();

        W_ReleaseLumpName(defdemoname);
        demoplayback = false;
        netdemo = false;
        netgame = false;
        H2_AdvanceDemo();
        return true;
    }

    if (demorecording)
    {
        *demo_p++ = DEMOMARKER;
        M_WriteFile(demoname, demobuffer, demo_p - demobuffer);
        Z_Free(demobuffer);
        demorecording = false;
        I_Error("Demo %s recorded", demoname);
    }

    return false;
}

//
// G_DemoGoToNextLevel
// [JN] Fast forward to next level while demo playback.
//

boolean demo_gotonextlvl;

void G_DemoGoToNextLevel (boolean start)
{
    // Disable screen rendering while fast forwarding.
    nodrawers = start;

    // Switch to fast tics running mode if not in -timedemo.
    if (!timingdemo)
    {
        singletics = start;
    }
}
