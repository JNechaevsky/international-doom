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

// G_game.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   // [JN] Local time.
#include "doomdef.h"
#include "doomkeys.h"
#include "deh_str.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_timer.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_video.h"

#include "deh_main.h" // [crispy] for demo footer
#include "memio.h"

#include "am_map.h"
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"


// Macros

#define AM_STARTKEY     9

#define MLOOKUNIT 8 // [crispy] for mouselook
#define MLOOKUNITLOWRES 16 // [crispy] for mouselook when recording

// Functions

void G_ReadDemoTiccmd(ticcmd_t * cmd);
void G_WriteDemoTiccmd(ticcmd_t * cmd);
void G_PlayerReborn(int player);

void G_DoReborn(int playernum);

void G_DoLoadLevel(void);
void G_DoNewGame(void);
void G_DoCompleted(void);
void G_DoVictory(void);
void G_DoWorldDone(void);
void G_DoSaveGame(void);

void D_PageTicker(void);
void D_AdvanceDemo(void);

struct
{
    int type;   // mobjtype_t
    int speed[2];
} MonsterMissileInfo[] = {
    { MT_IMPBALL, { 10, 20 } },
    { MT_MUMMYFX1, { 9, 18 } },
    { MT_KNIGHTAXE, { 9, 18 } },
    { MT_REDAXE, { 9, 18 } },
    { MT_BEASTBALL, { 12, 20 } },
    { MT_WIZFX1, { 18, 24 } },
    { MT_SNAKEPRO_A, { 14, 20 } },
    { MT_SNAKEPRO_B, { 14, 20 } },
    { MT_HEADFX1, { 13, 20 } },
    { MT_HEADFX3, { 10, 18 } },
    { MT_MNTRFX1, { 20, 26 } },
    { MT_MNTRFX2, { 14, 20 } },
    { MT_SRCRFX1, { 20, 28 } },
    { MT_SOR2FX1, { 20, 28 } },
    { -1, { -1, -1 } }                 // Terminator
};

gameaction_t gameaction;
gamestate_t gamestate;
skill_t gameskill;
boolean respawnmonsters;
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

boolean deathmatch;             // only if started as net death
boolean netgame;                // only true if packets are broadcast
boolean playeringame[MAXPLAYERS];
player_t players[MAXPLAYERS];

int consoleplayer;              // player taking events and displaying
int displayplayer;              // view being displayed
int levelstarttic;              // gametic at level start
int totalkills, totalitems, totalsecret;        // for intermission
int totaltimes, totalleveltimes; // [crispy] CPhipps - total time for all completed levels

boolean finalintermission; // [crispy] track intermission at end of episode

char *demoname;
static const char *orig_demoname = NULL; // [crispy] the name originally chosen for the demo, i.e. without "-00000"
boolean demorecording;
boolean longtics;               // specify high resolution turning in demos
boolean lowres_turn;
boolean shortticfix;            // calculate lowres turning like doom
boolean demoplayback;
boolean netdemo;
boolean demoextend;
byte *demobuffer, *demo_p, *demoend;
boolean singledemo;             // quit after playing a demo from cmdline

boolean precache = true;        // if true, load all graphics at start

// TODO: Heretic uses 16-bit shorts for consistency?
byte consistancy[MAXPLAYERS][BACKUPTICS];
char *savegamedir;
char  savename[256];

boolean testcontrols = false;
int testcontrols_mousespeed;


//
// controls (have defaults)
//



#define MAXPLMOVE       0x32

fixed_t forwardmove[2] = { 0x19, 0x32 };
fixed_t sidemove[2] = { 0x18, 0x28 };
fixed_t angleturn[3] = { 640, 1280, 320 };      // + slow turn

static int *weapon_keys[] =
{
    &key_weapon1,
    &key_weapon2,
    &key_weapon3,
    &key_weapon4,
    &key_weapon5,
    &key_weapon6,
    &key_weapon7
};

// Set to -1 or +1 to switch to the previous or next weapon.

static int next_weapon = 0;

// Used for prev/next weapon keys.

static const struct
{
    weapontype_t weapon;
    weapontype_t weapon_num;
} weapon_order_table[] = {
    { wp_staff,       wp_staff },
    { wp_gauntlets,   wp_staff },
    { wp_goldwand,    wp_goldwand },
    { wp_crossbow,    wp_crossbow },
    { wp_blaster,     wp_blaster },
    { wp_skullrod,    wp_skullrod },
    { wp_phoenixrod,  wp_phoenixrod },
    { wp_mace,        wp_mace },
    { wp_beak,        wp_beak },
};

#define SLOWTURNTICS    6

#define NUMKEYS 256
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
boolean joyarray[MAX_JOY_BUTTONS + 1];
boolean *joybuttons = &joyarray[1];     // allow [-1]

// [JN] Determinates speed of camera Z-axis movement in spectator mode.
static int crl_camzspeed;

int savegameslot;
char savedescription[32];

static ticcmd_t basecmd; // [crispy]

static int inventoryTics;

// haleyjd: removed WATCOMC

//=============================================================================
// Not used - ripped out for Heretic
/*
int G_CmdChecksum(ticcmd_t *cmd)
{
	int     i;
	int sum;

	sum = 0;
	for(i = 0; i < sizeof(*cmd)/4-1; i++)
	{
		sum += ((int *)cmd)[i];
	}
	return(sum);
}
*/

static boolean WeaponSelectable(weapontype_t weapon)
{
    if (weapon == wp_beak)
    {
        return false;
    }

    return players[consoleplayer].weaponowned[weapon];
}

static int G_NextWeapon(int direction)
{
    weapontype_t weapon;
    int start_i, i;

    // Find index in the table.

    if (players[consoleplayer].pendingweapon == wp_nochange)
    {
        weapon = players[consoleplayer].readyweapon;
    }
    else
    {
        weapon = players[consoleplayer].pendingweapon;
    }

    for (i=0; i<arrlen(weapon_order_table); ++i)
    {
        if (weapon_order_table[i].weapon == weapon)
        {
            break;
        }
    }

    // Switch weapon. Don't loop forever.
    start_i = i;
    do
    {
        i += direction;
        i = (i + arrlen(weapon_order_table)) % arrlen(weapon_order_table);
    } while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

    return weapon_order_table[i].weapon_num;
}

// -----------------------------------------------------------------------------
// [crispy] holding down the "Run" key may trigger special behavior,
// e.g. quick exit, clean screenshots, resurrection from savegames
// -----------------------------------------------------------------------------

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
    if (!mouseSensitivity)
        return 0.0;

    return (I_AccelerateMouseY(mousey) * (mouseSensitivity + 5) * 2 / 10);
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
    int flyheight;
    ticcmd_t spect;

    // haleyjd: removed externdriver crap

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
    //cmd->consistancy =
    //      consistancy[consoleplayer][(maketic*ticdup)%BACKUPTICS];
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

    strafe = gamekeydown[key_strafe] || mousebuttons[mousebstrafe]
        || joybuttons[joybstrafe];

    // [crispy] when "always run" is active,
    // pressing the "run" key will result in walking
    speed = (key_speed >= NUMKEYS || joybspeed >= MAX_JOY_BUTTONS);
    speed ^= speedkeydown();
    crl_camzspeed = speed;

    // haleyjd: removed externdriver crap
    
    forward = side = look = arti = flyheight = 0;

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

    // [crispy] add quick 180° reverse
    if (gamekeydown[key_180turn])
    {
        angle += ANG180 >> FRACBITS;
        gamekeydown[key_180turn] = false;
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
        S_StartSound(NULL, sfx_switch);
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
        S_StartSound(NULL, sfx_switch);
        gamekeydown[key_mouse_look] = false;
    }

    // [JN] Toggle vertical mouse movement.
    if (gamekeydown[key_novert])
    {
        mouse_novert ^= 1;
        CT_SetMessage(&players[consoleplayer], mouse_novert ?
                      ID_NOVERT_ON : ID_NOVERT_OFF, false, NULL);
        S_StartSound(NULL, sfx_switch);
        gamekeydown[key_novert] = false;
    }

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
        lspeed = 1;
    }
    else
    {
        lspeed = 2;
    }

//
// let movement keys cancel each other out
//
    if (strafe)
    {
        if (!cmd->angleturn)
        {
            if (gamekeydown[key_right])
                side += sidemove[speed];
            if (gamekeydown[key_left])
                side -= sidemove[speed];
            if (use_analog && joyxmove)
            {
                joyxmove = joyxmove * joystick_move_sensitivity / 10;
                joyxmove = (joyxmove > FRACUNIT) ? FRACUNIT : joyxmove;
                joyxmove = (joyxmove < -FRACUNIT) ? -FRACUNIT : joyxmove;
                side += FixedMul(sidemove[speed], joyxmove);
            }
            else if (joystick_move_sensitivity)
            {
                if (joyxmove > 0)
                    side += sidemove[speed];
                if (joyxmove < 0)
                    side -= sidemove[speed];
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
        forward += forwardmove[speed];
    if (gamekeydown[key_down])
        forward -= forwardmove[speed];
    if (use_analog && joyymove)
    {
        joyymove = joyymove * joystick_move_sensitivity / 10;
        joyymove = (joyymove > FRACUNIT) ? FRACUNIT : joyymove;
        joyymove = (joyymove < -FRACUNIT) ? FRACUNIT : joyymove;
        forward -= FixedMul(forwardmove[speed], joyymove);
    }
    else if (joystick_move_sensitivity)
    {
        if (joyymove < 0)
            forward += forwardmove[speed];
        if (joyymove > 0)
            forward -= forwardmove[speed];
    }
    if (gamekeydown[key_straferight] || mousebuttons[mousebstraferight]
     || joybuttons[joybstraferight])
        side += sidemove[speed];
    if (gamekeydown[key_strafeleft] || mousebuttons[mousebstrafeleft]
     || joybuttons[joybstrafeleft])
        side -= sidemove[speed];

    if (use_analog && joystrafemove)
    {
        joystrafemove = joystrafemove * joystick_move_sensitivity / 10;
        joystrafemove = (joystrafemove > FRACUNIT) ? FRACUNIT : joystrafemove;
        joystrafemove = (joystrafemove < -FRACUNIT) ? -FRACUNIT : joystrafemove;
        side += FixedMul(sidemove[speed], joystrafemove);
    }
    else if (joystick_move_sensitivity)
    {
        if (joystrafemove < 0)
            side -= sidemove[speed];
        if (joystrafemove > 0)
            side += sidemove[speed];
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
    // TODO
    /*
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
    */
    // haleyjd: removed externdriver crap
    if (gamekeydown[key_lookcenter])
    {
        look = TOCENTER;
    }

    // haleyjd: removed externdriver crap
    
    // Fly up/down/drop keys
    if (gamekeydown[key_flyup])
    {
        flyheight = 5;          // note that the actual flyheight will be twice this
    }
    if (gamekeydown[key_flydown])
    {
        flyheight = -5;
    }
    if (gamekeydown[key_flycenter])
    {
        flyheight = TOCENTER;
        // haleyjd: removed externdriver crap
        look = TOCENTER;
    }

    // Use artifact key
    if (gamekeydown[key_useartifact] || mousebuttons[mousebuseartifact])
    {
        if (gamekeydown[key_speed] && !ctrl_noartiskip)
        {
            if (players[consoleplayer].inventory[inv_ptr].type != arti_none)
            {
                gamekeydown[key_useartifact] = false;
                mousebuttons[mousebuseartifact] = false;
                cmd->arti = 0xff;       // skip artifact code
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
                cmd->arti = players[consoleplayer].inventory[inv_ptr].type;
                usearti = false;
            }
        }
    }
    if (gamekeydown[key_arti_tome] && !cmd->arti
        && !players[consoleplayer].powers[pw_weaponlevel2])
    {
        gamekeydown[key_arti_tome] = false;
        cmd->arti = arti_tomeofpower;
    }
    else if (gamekeydown[key_arti_quartz] && !cmd->arti
        && (players[consoleplayer].mo->health < MAXHEALTH))
    {
        gamekeydown[key_arti_quartz] = false;
        cmd->arti = arti_health;
    }
    else if (gamekeydown[key_arti_urn] && !cmd->arti)
    {
        gamekeydown[key_arti_urn] = false;
        cmd->arti = arti_superhealth;
    }
    else if (gamekeydown[key_arti_bomb] && !cmd->arti)
    {
        gamekeydown[key_arti_bomb] = false;
        cmd->arti = arti_firebomb;
    }
    else if (gamekeydown[key_arti_ring] && !cmd->arti)
    {
        gamekeydown[key_arti_ring] = false;
        cmd->arti = arti_invulnerability;
    }
    else if (gamekeydown[key_arti_chaosdevice] && !cmd->arti)
    {
        gamekeydown[key_arti_chaosdevice] = false;
        cmd->arti = arti_teleport;
    }
    else if (gamekeydown[key_arti_shadowsphere] && !cmd->arti)
    {
        gamekeydown[key_arti_shadowsphere] = false;
        cmd->arti = arti_invisibility;
    }
    else if (gamekeydown[key_arti_wings] && !cmd->arti)
    {
        gamekeydown[key_arti_wings] = false;
        cmd->arti = arti_fly;
    }
    else if (gamekeydown[key_arti_torch] && !cmd->arti)
    {
        gamekeydown[key_arti_torch] = false;
        cmd->arti = arti_torch;
    }
    else if (gamekeydown[key_arti_morph] && !cmd->arti)
    {
        gamekeydown[key_arti_morph] = false;
        cmd->arti = arti_egg;
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

    // If the previous or next weapon button is pressed, the
    // next_weapon variable is set to change weapons when
    // we generate a ticcmd.  Choose a new weapon.
    // (Can't weapon cycle when the player is a chicken)

    if (gamestate == GS_LEVEL
     && players[consoleplayer].chickenTics == 0 && next_weapon != 0)
    {
        i = G_NextWeapon(next_weapon);
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

    // [JN] Show last message.
    if (gamekeydown[key_message_refresh_hr])
    {
        players[consoleplayer].messageTics = MESSAGETICS;
    }

//
// mouse
//
    if (mousebuttons[mousebforward])
    {
        forward += forwardmove[speed];
    }

    if (mousebuttons[mousebbackward])
    {
	forward -= forwardmove[speed];
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

    // No mouse movement in previous frame?

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

    if (forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if (forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if (side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if (side < -MAXPLMOVE)
        side = -MAXPLMOVE;

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
    if (flyheight < 0)
    {
        flyheight += 16;
    }
    cmd->lookfly |= flyheight << 4;

//
// special buttons
//
    if (sendpause)
    {
        sendpause = false;
        // [crispy] ignore un-pausing in menus during demo recording
        if (!(MenuActive && demorecording && paused) && gameaction != ga_loadgame)
        {
        cmd->buttons = BT_SPECIAL | BTS_PAUSE;
        }
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
    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i] && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
        memset(players[i].frags, 0, sizeof(players[i].frags));
    }

    // [JN] CRL - wand start game mode.
    if (compat_pistol_start)
    {
        if (singleplayer)
        {
            G_PlayerReborn(0);
        }
        else if ((demoplayback || netdemo) /*&& !singledemo*/)
        {
            // no-op - silently ignore pistolstart when playing demo from the
            // demo reel
            // [JN] Do not rely on "singledemo" here to allow to play multiple
            // levels demo with Wand start mode enabled.
        }
        else
        {
            const char message[] = "Wand start game mode is not supported"
                                   " for demos and network play. Please disable"
                                   " Wand start mode in Gameplay Features"
                                   " menu.";
            if (!demo_p) demorecording = false;
            I_Error(message);
        }
    }

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
    joyxmove = joyymove = joystrafemove = 0;
    mousex = mousey = 0;
    memset(&localview, 0, sizeof(localview)); // [crispy]
    memset(&carry, 0, sizeof(carry)); // [crispy]
    memset(&prevcarry, 0, sizeof(prevcarry)); // [crispy]
    memset(&basecmd, 0, sizeof(basecmd)); // [crispy]
    sendpause = sendsave = paused = false;
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
    player_t *plr;

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
            if (displayplayer == MAXPLAYERS)
            {
                displayplayer = 0;
            }
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

    if (gamestate == GS_LEVEL)
    {
        if (CT_Responder(ev))
        {                       // Chat ate the event
            return (true);
        }
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
                S_StartSound(NULL, sfx_switch);
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
                pspr_interp = false;
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

/*
===============================================================================
=
= G_Ticker
=
===============================================================================
*/

void G_Ticker(void)
{
    int i, buf;
    ticcmd_t *cmd = NULL;

//
// do player reborns if needed
//
    for (i = 0; i < MAXPLAYERS; i++)
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
            case ga_newgame:
                G_DoNewGame();
                break;
            case ga_loadgame:
                G_DoLoadGame();
                break;
            case ga_savegame:
                G_DoSaveGame();
                break;
            case ga_playdemo:
                G_DoPlayDemo();
                break;
            case ga_screenshot:
                V_ScreenShot("HTIC%02i.%s");
                gameaction = ga_nothing;
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

    for (i = 0; i < MAXPLAYERS; i++)
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
    for (i = 0; i < MAXPLAYERS; i++)
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
                        // [crispy] never override savegames by demo playback
                        if (demoplayback)
                        break;
                        if (!savedescription[0])
                        {
                            if (netgame)
                            {
                                M_StringCopy(savedescription,
                                             DEH_String("NET GAME"),
                                             sizeof(savedescription));
                            }
                            else
                            {
                                M_StringCopy(savedescription,
                                             DEH_String("SAVE GAME"),
                                             sizeof(savedescription));
                            }
                        }
                        savegameslot =
                            (players[i].cmd.
                             buttons & BTS_SAVEMASK) >> BTS_SAVESHIFT;
                        gameaction = ga_savegame;
                        // [crispy] un-pause immediately after saving
                        // (impossible to send save and pause specials within the same tic)
                        if (demorecording && paused)
                        sendpause = true;
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

    oldleveltime = realleveltime;

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
            if (widget_health  || (xhair_draw && xhair_color > 1))
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
            D_PageTicker();
            break;
    }

    // [JN] Reduce message tics independently from framerate and game states.
    // Tics can't go negative.
    MSG_Ticker();

    //
    // [JN] Query time for time-related widgets:
    //

    // Level / DeathMatch timer
    if (widget_time)
    {
        const int time = (deathmatch && TimerGame ? TimerGame : leveltime) / TICRATE;
    
        M_snprintf(ID_Level_Time, sizeof(ID_Level_Time),
                   "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
    }
    // Total time
    if (widget_totaltime)
    {
        const int totaltime = (totalleveltimes / TICRATE) + (leveltime / TICRATE);

        M_snprintf(ID_Total_Time, sizeof(ID_Total_Time),
                   "%02d:%02d:%02d", totaltime/3600, (totaltime%3600)/60, totaltime%60);
    }
    // Local time
    if (msg_local_time)
    {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

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

/*
====================
=
= G_InitPlayer
=
= Called at the start
= Called by the game initialization functions
====================
*/

void G_InitPlayer(int player)
{
    // clear everything else to defaults
    G_PlayerReborn(player);
}


/*
====================
=
= G_PlayerFinishLevel
=
= Can when a player completes a level
====================
*/

void G_PlayerFinishLevel(int player)
{
    player_t *p;
    int i;

/*      // BIG HACK
	inv_ptr = 0;
	curpos = 0;
*/
    // END HACK
    p = &players[player];
    for (i = 0; i < p->inventorySlotNum; i++)
    {
        p->inventory[i].count = 1;
    }
    p->artifactCount = p->inventorySlotNum;

    if (!deathmatch)
    {
        for (i = 0; i < 16; i++)
        {
            P_PlayerUseArtifact(p, arti_fly);
        }
    }
    memset(p->powers, 0, sizeof(p->powers));
    memset(p->keys, 0, sizeof(p->keys));
    playerkeys = 0;
//      memset(p->inventory, 0, sizeof(p->inventory));
    if (p->chickenTics)
    {
        p->readyweapon = p->mo->special1.i;       // Restore weapon
        p->chickenTics = 0;
    }
    p->messageTics = 0;
    p->messageCenteredTics = 0;
    p->targetsheathTics = 0;
    p->lookdir = p->oldlookdir = 0;
    p->mo->flags &= ~MF_SHADOW; // Remove invisibility
    p->extralight = 0;          // Remove weapon flashes
    p->fixedcolormap = 0;       // Remove torch
    p->damagecount = 0;         // No palette changes
    p->bonuscount = 0;
    p->rain1 = NULL;
    p->rain2 = NULL;
    if (p == &players[consoleplayer])
    {
        SB_state = -1;          // refresh the status bar
    }
    // [JN] Return controls to the player.
    crl_spectating = 0;
}

/*
====================
=
= G_PlayerReborn
=
= Called after a player dies
= almost everything is cleared and initialized
====================
*/

void G_PlayerReborn(int player)
{
    player_t *p;
    int i;
    int frags[MAXPLAYERS];
    int killcount, itemcount, secretcount;
    boolean secret;

    secret = false;
    memcpy(frags, players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;

    p = &players[player];
    if (p->didsecret)
    {
        secret = true;
    }
    memset(p, 0, sizeof(*p));

    memcpy(players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;

    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;
    p->readyweapon = p->pendingweapon = wp_goldwand;
    p->weaponowned[wp_staff] = true;
    p->weaponowned[wp_goldwand] = true;
    p->messageTics = 0;
    p->messageCenteredTics = 0;
    p->targetsheathTics = 0;
    p->lookdir = 0;
    p->ammo[am_goldwand] = 50;
    for (i = 0; i < NUMAMMO; i++)
    {
        p->maxammo[i] = maxammo[i];
    }
    if (gamemap == 9 || secret)
    {
        p->didsecret = true;
    }
    if (p == &players[consoleplayer])
    {
        SB_state = -1;          // refresh the status bar
        inv_ptr = 0;            // reset the inventory pointer
        curpos = 0;
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

boolean G_CheckSpot(int playernum, mapthing_t * mthing)
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
        S_StartSound(mo, sfx_telept);   // don't start sound on first frame

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
    if (selections < 4)
        I_Error("Only %i deathmatch spots, 4 required", selections);

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
    P_SpawnPlayer(&playerstarts[playernum]);
}

// [crispy] clear the "savename" variable,
// i.e. restart level from scratch upon resurrection
void G_ClearSavename (void)
{
    M_StringCopy(savename, "", sizeof(savename));
}

/*
====================
=
= G_DoReborn
=
====================
*/

void G_DoReborn(int playernum)
{
    int i;

    // quit demo unless -demoextend
    if (!demoextend && G_CheckDemoStatus())
        return;
    if (!netgame)
        gameaction = ga_loadlevel;      // reload the level from scratch
    else
    {                           // respawn at the start
        players[playernum].mo->player = NULL;   // dissasociate the corpse

        // spawn at random spot if in death match
        if (deathmatch)
        {
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

        if (G_CheckSpot(playernum, &playerstarts[playernum]))
        {
            P_SpawnPlayer(&playerstarts[playernum]);
            return;
        }
        // try to spawn at one of the other players spots
        for (i = 0; i < MAXPLAYERS; i++)
            if (G_CheckSpot(playernum, &playerstarts[i]))
            {
                playerstarts[i].type = playernum + 1;   // fake as other player
                P_SpawnPlayer(&playerstarts[i]);
                playerstarts[i].type = i + 1;   // restore
                return;
            }
        // he's going to be inside something.  Too bad.
        P_SpawnPlayer(&playerstarts[playernum]);
    }
}


void G_ScreenShot(void)
{
    gameaction = ga_screenshot;
}


/*
====================
=
= G_DoCompleted
=
====================
*/

boolean secretexit;

void G_ExitLevel(void)
{
    secretexit = false;
    gameaction = ga_completed;
}

void G_SecretExitLevel(void)
{
    secretexit = true;
    gameaction = ga_completed;
}

// [crispy] format time for level statistics
#define TIMESTRSIZE 16
static void G_FormatLevelStatTime(char *str, int tics)
{
    int exitHours, exitMinutes;
    float exitTime, exitSeconds;

    exitTime = (float) tics / 35;
    exitHours = exitTime / 3600;
    exitTime -= exitHours * 3600;
    exitMinutes = exitTime / 60;
    exitTime -= exitMinutes * 60;
    exitSeconds = exitTime;

    if (exitHours)
    {
        M_snprintf(str, TIMESTRSIZE, "%d:%02d:%05.2f",
                    exitHours, exitMinutes, exitSeconds);
    }
    else
    {
        M_snprintf(str, TIMESTRSIZE, "%01d:%05.2f", exitMinutes, exitSeconds);
    }
}

// [crispy] Write level statistics upon exit
static void G_WriteLevelStat(void)
{
    FILE *fstream;

    int i, playerKills = 0, playerItems = 0, playerSecrets = 0;

    char levelString[8];
    char levelTimeString[TIMESTRSIZE];
    char totalTimeString[TIMESTRSIZE];
    char *decimal;

    static boolean firsttime = true;

    if (firsttime)
    {
        firsttime = false;
        fstream = M_fopen("levelstat.txt", "w");
    }
    else
    {
        fstream = M_fopen("levelstat.txt", "a");
    }

    if (fstream == NULL)
    {
        fprintf(stderr, "G_WriteLevelStat: Unable to open levelstat.txt for writing!\n");
        return;
    }

    M_snprintf(levelString, sizeof(levelString), "E%dM%d",
                gameepisode, gamemap);

    G_FormatLevelStatTime(levelTimeString, leveltime);
    G_FormatLevelStatTime(totalTimeString, totalleveltimes + leveltime);

    // Total time ignores centiseconds
    decimal = strchr(totalTimeString, '.');
    if (decimal != NULL)
    {
        *decimal = '\0';
    }

    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            playerKills += players[i].killcount;
            playerItems += players[i].itemcount;
            playerSecrets += players[i].secretcount;
        }
    }

    fprintf(fstream, "%s%s - %s (%s)  K: %d/%d  I: %d/%d  S: %d/%d\n",
            levelString, (secretexit ? "s" : ""),
            levelTimeString, totalTimeString, playerKills, totalkills, 
            playerItems, totalitems, playerSecrets, totalsecret);

    fclose(fstream);
}

void G_DoCompleted(void)
{
    int i;
    static int afterSecret[5] = { 7, 5, 5, 5, 4 };

    // [crispy] Write level statistics upon exit
    if (M_ParmExists("-levelstat"))
    {
        G_WriteLevelStat();
    }

    gameaction = ga_nothing;

    // quit demo unless -demoextend
    if (!demoextend && G_CheckDemoStatus())
    {
        return;
    }
    for (i = 0; i < MAXPLAYERS; i++)
    {
        if (playeringame[i])
        {
            G_PlayerFinishLevel(i);
        }
    }
    prevmap = gamemap;
    if (secretexit == true)
    {
        gamemap = 9;
    }
    else if (gamemap == 9)
    {                           // Finished secret level
        gamemap = afterSecret[gameepisode - 1];
        // [JN] Force set "didsecret" flag, so cross mark
        // will not dissapear on interlude screen.
        for (i = 0 ; i < MAXPLAYERS ; i++)
        {
            players[i].didsecret = true;
        }
    }
    else if (gamemap == 8)
    {
        // [crispy] track intermission at end of episode
        finalintermission = true;
    }
    else
    {
        gamemap++;
    }

    // [crispy] CPhipps - total time for all completed levels
    // cph - modified so that only whole seconds are added to the totalleveltimes
    // value; so our total is compatible with the "naive" total of just adding
    // the times in seconds shown for each level. Also means our total time
    // will agree with Compet-n.
    totaltimes = (totalleveltimes += (leveltime - leveltime % TICRATE));

    gamestate = GS_INTERMISSION;
    IN_Start();
}

//============================================================================
//
// G_WorldDone
//
//============================================================================

void G_WorldDone(void)
{
    gameaction = ga_worlddone;

    // [crispy] track intermission at end of episode
    if (finalintermission)
    {
        gameaction = ga_victory;
    }
}

//============================================================================
//
// G_DoWorldDone
//
//============================================================================

void G_DoWorldDone(void)
{
    // [JN] jff 3/17/98 allow new level's music to be loaded
    idmusnum = -1;
    gamestate = GS_LEVEL;
    G_DoLoadLevel();
    gameaction = ga_nothing;
    // [JN] jff 4/12/98 clear any marks on the automap
    AM_ClearMarks();
}

//---------------------------------------------------------------------------
//
// PROC G_LoadGame
//
// Can be called by the startup code or the menu task.
//
//---------------------------------------------------------------------------

void G_LoadGame(char *name)
{
    M_StringCopy(savename, name, sizeof(savename));
    gameaction = ga_loadgame;
}

//---------------------------------------------------------------------------
//
// PROC G_DoLoadGame
//
// Called by G_Ticker based on gameaction.
//
//---------------------------------------------------------------------------

#define VERSIONSIZE 16

void G_DoLoadGame(void)
{
    int i;
    int a, b, c;
    int d, e, f;
    char savestr[SAVESTRINGSIZE];
    char vcheck[VERSIONSIZE], readversion[VERSIONSIZE];
    const player_t *p; // [crispy]

    p = &players[consoleplayer]; // [crispy]

    gameaction = ga_nothing;

    SV_OpenRead(savename);

    // [JN] Do not erase save data,
    // it will be needed for "On death action" feature.
    /*
    free(savename);
    savename = NULL;
    */

    // Skip the description field
    SV_Read(savestr, SAVESTRINGSIZE);

    memset(vcheck, 0, sizeof(vcheck));
    DEH_snprintf(vcheck, VERSIONSIZE, "version %i", HERETIC_VERSION);
    SV_Read(readversion, VERSIONSIZE);

    if (strncmp(readversion, vcheck, VERSIONSIZE) != 0)
    {                           // Bad version
        return;
    }
    gameskill = SV_ReadByte();
    gameepisode = SV_ReadByte();
    gamemap = SV_ReadByte();
    // [JN] Read chosen by IDMUS music from saved game.
    idmusnum = SV_ReadByte();
    // [JN] jff 3/18/98 account for unsigned byte
    if (idmusnum == 255)
    {
        idmusnum = -1;
    }
    for (i = 0; i < MAXPLAYERS; i++)
    {
        playeringame[i] = SV_ReadByte();
    }
    // Load a base level
    G_InitNew(gameskill, gameepisode, gamemap);

    // Create leveltime
    a = SV_ReadByte();
    b = SV_ReadByte();
    c = SV_ReadByte();
    leveltime = (a << 16) + (b << 8) + c;

    // [JN] Create total level time
    d = SV_ReadByte();
    e = SV_ReadByte();
    f = SV_ReadByte();
    totalleveltimes = (d << 16) + (e << 8) + f;

    // De-archive all the modifications
    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveSpecials();
    P_RestoreTargets();
    // [JN] Restore automap marks.
    P_UnArchiveAutomap();
    // [plums] restore old sector specials.
    P_UnArchiveOldSpecials();

    // [crispy] point to active artifact after load
    for (i = 0; i < p->inventorySlotNum; i++)
    {
        if (p->inventory[i].type == p->readyArtifact)
        {
            curpos = inv_ptr = i;
            curpos = (curpos > CURPOS_MAX) ? CURPOS_MAX : curpos;
            break;
        }
    }

    if (SV_ReadByte() != SAVE_GAME_TERMINATOR)
    {                           // Missing savegame termination marker
        I_Error("Bad savegame");
    }

    // Draw the pattern into the back screen
    R_FillBackScreen();

    // [crispy] if the player is dead in this savegame,
    // do not consider it for reload
    if (players[consoleplayer].health <= 0)
	G_ClearSavename();

    // [JN] If "On death action" is set to "last save",
    // then prevent holded "use" button to work for next few tics.
    // This fixes imidiate pressing on wall upon reloading
    // a save game, if "use" button is kept pressed.
    if (singleplayer && gp_death_use_action == 1)
	players[consoleplayer].usedown = true;
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

skill_t d_skill;
int d_episode;
int d_map;

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    G_ClearSavename();
    gameaction = ga_newgame;

    // [crispy] if a new game is started during demo recording, start a new demo
    if (demorecording)
    {
	G_CheckDemoStatus();
	Z_Free(demoname);
	G_RecordDemo(skill, 1, episode, map, orig_demoname);
    }
}

void G_DoNewGame(void)
{
    // [JN] Andrey Budko: allow new level's music to be loaded
    idmusnum = -1;
    G_InitNew(d_skill, d_episode, d_map);
    gameaction = ga_nothing;
}

void G_InitNew(skill_t skill, int episode, int map)
{
    int i;
    int speed;
    static const char *skyLumpNames[5] = {
        "SKY1", "SKY2", "SKY3", "SKY1", "SKY3"
    };

    if (paused)
    {
        paused = false;
        S_ResumeSound();
    }
    if (skill < sk_baby)
        skill = sk_baby;
    if (skill > sk_nightmare)
        skill = sk_nightmare;
    if (episode < 1)
        episode = 1;
    // Up to 9 episodes for testing
    if (episode > 9)
        episode = 9;
    if (map < 1)
        map = 1;
    if (map > 9)
        map = 9;
    M_ClearRandom();
    if (respawnparm)
    {
        respawnmonsters = true;
    }
    else
    {
        respawnmonsters = false;
    }
    // Set monster missile speeds
    speed = skill == sk_nightmare;
    for (i = 0; MonsterMissileInfo[i].type != -1; i++)
    {
        mobjinfo[MonsterMissileInfo[i].type].speed
            = MonsterMissileInfo[i].speed[speed] << FRACBITS;
    }
    // Force players to be initialized upon first level load
    for (i = 0; i < MAXPLAYERS; i++)
    {
        players[i].playerstate = PST_REBORN;
        players[i].didsecret = false;
    }
    // Set up a bunch of globals
    usergame = true;            // will be set false if a demo
    paused = false;
    demorecording = false;
    demoplayback = false;
    netdemo = false;
    // [JN] Reset automap scale. Fixes:
    // https://doomwiki.org/wiki/Automap_scale_preserved_after_warps_in_Heretic_and_Hexen
    automapactive = false; 
    // [JN] jff 4/16/98 force marks on automap cleared every new level start
    AM_ClearMarks();
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    // [crispy] CPhipps - total time for all completed levels
    totalleveltimes = 0;

    // [crispy] track intermission at end of episode
    finalintermission = false;

    defdemotics = 0;

    // Set the sky map
    if (episode > 5)
    {
        skytexture = R_TextureNumForName(DEH_String("SKY1"));
    }
    else
    {
        skytexture = R_TextureNumForName(DEH_String(skyLumpNames[episode - 1]));
    }

//
// give one null ticcmd_t
//
#if 0
    gametic = 0;
    maketic = 1;
    for (i = 0; i < MAXPLAYERS; i++)
        nettics[i] = 1;         // one null event for this gametic
    memset(localcmds, 0, sizeof(localcmds));
    memset(netcmds, 0, sizeof(netcmds));
#endif
    G_DoLoadLevel();
}

// -----------------------------------------------------------------------------
// G_DoSelectiveGame
//  [JN] Start new game with given parameters in "Level select" menu.
// -----------------------------------------------------------------------------

void G_DoSelectiveGame (int choice)
{
    int i;
    int speed;
    player_t *plr = &players[consoleplayer];
    // [JN] Andrey Budko: allow new level's music to be loaded
    idmusnum = -1;
    demoplayback = false;
    netdemo = false;
    netgame = false;
    deathmatch = false;
    // [crispy] reset game speed after demo fast-forward
    singletics = false;
    // [JN] jff 4/16/98 force marks on automap cleared every new level start
    AM_ClearMarks();
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    consoleplayer = 0;
    gameaction = ga_nothing;
    automapactive = false;

    // Close "Level select" menu
    MenuActive = false;

    G_InitNew (level_select[0],
               gamemode == shareware ? 1 : level_select[1],
               level_select[2]); 

    // Fast monsters
    speed = level_select[22] ? 1 : 0;
    for (i = 0; MonsterMissileInfo[i].type != -1; i++)
    {
        mobjinfo[MonsterMissileInfo[i].type].speed
               = MonsterMissileInfo[i].speed[speed] << FRACBITS;
    }

    // Respawning monsters
    respawnmonsters = level_select[23];

    // Health
    plr->health = level_select[3];
    plr->mo->health = level_select[3];

    // Armor
    plr->armorpoints = level_select[4];
    // Armor type. Set to 0 if no armor given.
    plr->armortype = level_select[5] == 0 ? 0 : level_select[5];

    // Weapons
    plr->weaponowned[wp_gauntlets]  = level_select[6];
    plr->weaponowned[wp_crossbow]   = level_select[7];
    plr->weaponowned[wp_blaster]    = level_select[8];
    plr->weaponowned[wp_skullrod]   = gamemode == shareware ? 0 : level_select[9];
    plr->weaponowned[wp_phoenixrod] = gamemode == shareware ? 0 : level_select[10];
    plr->weaponowned[wp_mace]       = gamemode == shareware ? 0 : level_select[11];

    // Bag of Holding
    plr->backpack = level_select[12];
    if (level_select[12])
    {
        for (i = 0 ; i < NUMAMMO ; i++)
        {
            plr->maxammo[i] *= 2;
        }
    }

    // Ammo
    plr->ammo[0] = level_select[13]; // Wand crystals
    plr->ammo[1] = level_select[14]; // Ethereal arrows
    plr->ammo[2] = level_select[15]; // Claw orbs
    plr->ammo[3] = level_select[16]; // Hellstaff runes
    plr->ammo[4] = level_select[17]; // Flame orbs
    plr->ammo[5] = level_select[18]; // Mace spheres

    // Keys
    plr->keys[0] = level_select[19]; // Yellow key
    plr->keys[1] = level_select[20]; // Green key
    plr->keys[2] = level_select[21]; // Blue key

    // Artifacts
    for (i = 0 ; i < level_select[24] ; i++) P_GiveArtifact(plr, arti_health, NULL);
    for (i = 0 ; i < level_select[25] ; i++) P_GiveArtifact(plr, arti_superhealth, NULL);
    for (i = 0 ; i < level_select[26] ; i++) P_GiveArtifact(plr, arti_firebomb, NULL);
    for (i = 0 ; i < level_select[27] ; i++) P_GiveArtifact(plr, arti_tomeofpower, NULL);
    for (i = 0 ; i < level_select[28] ; i++) P_GiveArtifact(plr, arti_invulnerability, NULL);
    for (i = 0 ; i < level_select[29] ; i++) P_GiveArtifact(plr, arti_egg, NULL);
    for (i = 0 ; i < level_select[30] ; i++) P_GiveArtifact(plr, arti_teleport, NULL);
    for (i = 0 ; i < level_select[31] ; i++) P_GiveArtifact(plr, arti_invisibility, NULL);
    for (i = 0 ; i < level_select[32] ; i++) P_GiveArtifact(plr, arti_fly, NULL);
    for (i = 0 ; i < level_select[33] ; i++) P_GiveArtifact(plr, arti_torch, NULL);
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

void G_ReadDemoTiccmd(ticcmd_t * cmd)
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

void G_WriteDemoTiccmd(ticcmd_t * cmd)
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
    // This aligns with vvHeretic demo usage:
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

    for (i = 1; i < MAXPLAYERS; i++)
        *demo_p++ = playeringame[i];

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

    // vvHeretic allows extra options to be stored in the upper bits of
    // the player 1 present byte. However, this is a non-vanilla extension.
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

    for (i = 0; i < MAXPLAYERS; i++)
        playeringame[i] = (*demo_p++) != 0;

    if (playeringame[1] || M_CheckParm("-solo-net") > 0
                        || M_CheckParm("-netdemo") > 0)
    {
    	netgame = true;
    }

    precache = false;           // don't spend a lot of time in loadlevel
    G_InitNew(skill, episode, map);
    precache = true;
    usergame = false;
    demoplayback = true;

    if (netgame == true)
    {
      netdemo = true;
    }

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
    skill_t skill;
    int episode, map, i;
    int lumpnum, lumplength; // [crispy]

    demobuffer = demo_p = W_CacheLumpName(name, PU_STATIC);

    // [crispy] ignore empty demo lumps
    lumpnum = W_GetNumForName(name);
    lumplength = W_LumpLength(lumpnum);
    if (lumplength < 0xd)
    {
        demoplayback = true;
        G_CheckDemoStatus();
        return;
    }

    skill = *demo_p++;
    episode = *demo_p++;
    map = *demo_p++;

    // Read special parameter bits: see G_RecordDemo() for details.
    longtics = (*demo_p & DEMOHEADER_LONGTICS) != 0;

    // don't overwrite arguments from the command line
    respawnparm |= (*demo_p & DEMOHEADER_RESPAWN) != 0;
    nomonsters  |= (*demo_p & DEMOHEADER_NOMONSTERS) != 0;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        playeringame[i] = (*demo_p++) != 0;
    }

    if (playeringame[1] || M_CheckParm("-solo-net") > 0
                        || M_CheckParm("-netdemo") > 0)
    {
      netgame = true;
    }

    G_InitNew(skill, episode, map);
    starttime = I_GetTime();

    // [JN] Disable rendering the screen entirely.
    nodrawers = M_CheckParm ("-nodraw");

    usergame = false;
    demoplayback = true;
    timingdemo = true;
    singletics = true;

    if (netgame == true)
    {
      netdemo = true;
    }

    // [crispy] demo progress bar
    G_DemoProgressBar(lumplength);
}

#define DEMO_FOOTER_SEPARATOR "\n"
#define NUM_DEMO_FOOTER_LUMPS 4

static size_t WriteCmdLineLump(MEMFILE *stream)
{
    int i;
    long pos;
    char *tmp, **filenames;

    filenames = W_GetWADFileNames();

    pos = mem_ftell(stream);

    tmp = M_StringJoin("-iwad \"", M_BaseName(filenames[0]), "\"", NULL);
    mem_fputs(tmp, stream);
    free(tmp);

    if (filenames[1])
    {
        mem_fputs(" -file", stream);

        for (i = 1; filenames[i]; i++)
        {
            tmp = M_StringJoin(" \"", M_BaseName(filenames[i]), "\"", NULL);
            mem_fputs(tmp, stream);
            free(tmp);
        }
    }

    filenames = DEH_GetFileNames();

    if (filenames)
    {
        mem_fputs(" -deh", stream);

        for (i = 0; filenames[i]; i++)
        {
            tmp = M_StringJoin(" \"", M_BaseName(filenames[i]), "\"", NULL);
            mem_fputs(tmp, stream);
            free(tmp);
        }
    }

    mem_fputs(" -complevel 0", stream);

    if (M_CheckParm("-solo-net"))
    {
        mem_fputs(" -solo-net", stream);
    }

    return mem_ftell(stream) - pos;
}

static void WriteFileInfo(const char *name, size_t size, MEMFILE *stream)
{
    filelump_t fileinfo = { 0 };
    static long filepos = sizeof(wadinfo_t);

    fileinfo.filepos = LONG(filepos);
    fileinfo.size = LONG(size);

    if (name)
    {
        size_t len = strnlen(name, 8);
        if (len < 8)
        {
            len++;
        }
        memcpy(fileinfo.name, name, len);
    }

    mem_fwrite(&fileinfo, 1, sizeof(fileinfo), stream);

    filepos += size;
}

static void G_AddDemoFooter(void)
{
    byte *data;
    size_t size;

    MEMFILE *stream = mem_fopen_write();

    wadinfo_t header = { "PWAD" };
    header.numlumps = LONG(NUM_DEMO_FOOTER_LUMPS);
    mem_fwrite(&header, 1, sizeof(header), stream);

    mem_fputs(PACKAGE_FULLNAME_HERETIC, stream);  // [JN] Use full port name.
    mem_fputs(DEMO_FOOTER_SEPARATOR, stream);
    size = WriteCmdLineLump(stream);
    mem_fputs(DEMO_FOOTER_SEPARATOR, stream);

    header.infotableofs = LONG(mem_ftell(stream));
    mem_fseek(stream, 0, MEM_SEEK_SET);
    mem_fwrite(&header, 1, sizeof(header), stream);
    mem_fseek(stream, 0, MEM_SEEK_END);

    WriteFileInfo("PORTNAME", strlen(PACKAGE_STRING), stream);
    WriteFileInfo(NULL, strlen(DEMO_FOOTER_SEPARATOR), stream);
    WriteFileInfo("CMDLINE", size, stream);
    WriteFileInfo(NULL, strlen(DEMO_FOOTER_SEPARATOR), stream);

    mem_get_buf(stream, (void **)&data, &size);

    while (demo_p > demoend - size)
    {
        IncreaseDemoBuffer();
    }

    memcpy(demo_p, data, size);
    demo_p += size;

    mem_fclose(stream);
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
        D_AdvanceDemo();
        return true;
    }

    if (demorecording)
    {
        *demo_p++ = DEMOMARKER;
        G_AddDemoFooter();
        M_WriteFile(demoname, demobuffer, demo_p - demobuffer);
        Z_Free(demobuffer);
        demorecording = false;
        // [crispy] if a new game is started during demo recording, start a new demo
        if (gameaction != ga_newgame)
        {
        I_Error("Demo %s recorded", demoname);
        }
        else
        {
            fprintf(stderr, "Demo %s recorded\n", demoname);
        }
    }

    return false;
}

/**************************************************************************/
/**************************************************************************/

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
    int i;
    char *filename;
    char verString[VERSIONSIZE];
    char *description;

    filename = SV_Filename(savegameslot);

    description = savedescription;

    SV_Open(filename);
    SV_Write(description, SAVESTRINGSIZE);
    memset(verString, 0, sizeof(verString));
    DEH_snprintf(verString, VERSIONSIZE, "version %i", HERETIC_VERSION);
    SV_Write(verString, VERSIONSIZE);
    SV_WriteByte(gameskill);
    SV_WriteByte(gameepisode);
    SV_WriteByte(gamemap);
    // [JN] Write chosen by IDMUS music into saved game.
    SV_WriteByte(idmusnum);
    for (i = 0; i < MAXPLAYERS; i++)
    {
        SV_WriteByte(playeringame[i]);
    }
    SV_WriteByte(leveltime >> 16);
    SV_WriteByte(leveltime >> 8);
    SV_WriteByte(leveltime);
    // [JN] Write total level times
    SV_WriteByte(totalleveltimes >> 16);
    SV_WriteByte(totalleveltimes >> 8);
    SV_WriteByte(totalleveltimes);
    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveSpecials();
    // [JN] Archive automap marks.
    P_ArchiveAutomap();
    // [plums] write old sector specials (for revealed secrets) at the end
    // to keep save compatibility with previous versions.
    P_ArchiveOldSpecials();
    SV_Close(filename);

    gameaction = ga_nothing;
    savedescription[0] = 0;
    M_StringCopy(savename, filename, sizeof(savename));
    CT_SetMessage(&players[consoleplayer], DEH_String(TXT_GAMESAVED), true, NULL);

    free(filename);

    // Draw the pattern into the back screen
    R_FillBackScreen();
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
