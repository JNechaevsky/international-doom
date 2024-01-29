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

#include <stdio.h>

#include "doomtype.h"
#include "doomkeys.h"
#include "m_config.h"
#include "m_misc.h"


//
// Keyboard controls
//

// Movement

int key_up          = 'w';
int key_down        = 's'; 
int key_right       = KEY_RIGHTARROW;
int key_left        = KEY_LEFTARROW;
int key_strafeleft  = 'a';
int key_straferight = 'd';
int key_speed       = KEY_RSHIFT; 
int key_strafe      = KEY_RALT;
int key_180turn     = 0; // [crispy]

// Action

int key_fire = KEY_RCTRL;
int key_use  = ' ';

// Advanced movement

int key_autorun    = KEY_CAPSLOCK; // [crispy]
int key_mouse_look = 0;            // [crispy]

// Special keys

int key_reloadlevel = 0; // [crispy]
int key_nextlevel   = 0; // [crispy]
int key_demospeed   = 0; // [crispy]
int key_flip_levels = 0; // [crispy]

// Heretic keyboard controls
 
int key_flyup = KEY_PGUP;
int key_flydown = KEY_INS;
int key_flycenter = KEY_HOME;

int key_lookup = KEY_PGDN;
int key_lookdown = KEY_DEL;
int key_lookcenter = KEY_END;

int key_invleft = '[';
int key_invright = ']';
int key_useartifact = KEY_ENTER;

int key_arti_quartz = 0;
int key_arti_urn = 0;
int key_arti_bomb = 0;
int key_arti_tome = 127;
int key_arti_ring = 0;
int key_arti_chaosdevice = 0;
int key_arti_shadowsphere = 0;
int key_arti_wings = 0;
int key_arti_torch = 0;
int key_arti_morph = 0;

// Hexen keyboard controls

int key_jump = '/';

int key_arti_all             = KEY_BACKSPACE;
int key_arti_health          = '\\';
int key_arti_poisonbag       = '0';
int key_arti_blastradius     = '9';
int key_arti_teleport        = '8';
int key_arti_teleportother   = '7';
int key_arti_egg             = '6';
int key_arti_invulnerability = '5';

// Special modes

int key_spectator = 0;
int key_freeze    = 0;
int key_notarget  = 0;
int key_buddha    = 0;

// Weapons

int key_weapon1    = '1';
int key_weapon2    = '2';
int key_weapon3    = '3';
int key_weapon4    = '4';
int key_weapon5    = '5';
int key_weapon6    = '6';
int key_weapon7    = '7';
int key_weapon8    = '8';
int key_prevweapon = 0;
int key_nextweapon = 0;

// Automap

int key_map_toggle    = KEY_TAB;
int key_map_zoomin    = '=';
int key_map_zoomout   = '-';
int key_map_maxzoom   = '0';
int key_map_follow    = 'f';
int key_map_rotate    = 'r';
int key_map_overlay   = 'o';
int key_map_grid      = 'g';
int key_map_mark      = 'm';
int key_map_clearmark = 'c';
int key_map_north     = KEY_UPARROW;
int key_map_south     = KEY_DOWNARROW;
int key_map_east      = KEY_RIGHTARROW;
int key_map_west      = KEY_LEFTARROW;

// Function keys

int key_menu_help     = KEY_F1;
int key_menu_save     = KEY_F2;
int key_menu_load     = KEY_F3;
int key_menu_volume   = KEY_F4;
int key_menu_detail   = KEY_F5;
int key_menu_qsave    = KEY_F6;
int key_menu_endgame  = KEY_F7;
int key_menu_messages = KEY_F8;
int key_menu_qload    = KEY_F9;
int key_menu_quit     = KEY_F10;
int key_menu_gamma    = KEY_F11;
int key_spy           = KEY_F12;

// Shortcut keys

int key_pause           = KEY_PAUSE;
int key_menu_screenshot = KEY_PRTSCR;
int key_message_refresh = KEY_ENTER;
// [JN] Heretic using ENTER for afrtifacts activation.
int key_message_refresh_hr = 0;
int key_demo_quit       = 'q';

// Multiplayer

int key_multi_msg = 't';
int key_multi_msgplayer[8];

// Special menu keys, not available for rebinding

int key_menu_activate  = KEY_ESCAPE;
int key_menu_up        = KEY_UPARROW;
int key_menu_down      = KEY_DOWNARROW;
int key_menu_left      = KEY_LEFTARROW;
int key_menu_right     = KEY_RIGHTARROW;
int key_menu_back      = KEY_BACKSPACE;
int key_menu_forward   = KEY_ENTER;
int key_menu_confirm   = 'y';
int key_menu_abort     = 'n';
int key_menu_incscreen = KEY_EQUALS;
int key_menu_decscreen = KEY_MINUS;
int key_menu_del       = KEY_DEL; // [crispy]

//
// Mouse controls
//

int mousebfire        = 0;
int mousebforward     = 2;
int mousebstrafe      = 1;
int mousebbackward    = -1;
int mousebuse         = -1;
int mousebstrafeleft  = -1;
int mousebstraferight = -1;
int mousebprevweapon  = 4;
int mousebnextweapon  = 3;
int mousebjump        = -1;

// Heretic: Inventory
int mousebinvleft = -1;
int mousebinvright = -1;
int mousebuseartifact = -1;

// Control whether if a mouse button is double clicked,
// it acts like "use" has been pressed.

int mouse_dclick_use = 1;

//
// Joystick controls
//

int joybfire = 0;
int joybstrafe = 1;
int joybuse = 3;
int joybspeed = 29;
int joybstrafeleft = -1;
int joybstraferight = -1;
int joybprevweapon = -1;
int joybnextweapon = -1;
int joybmenu = -1;
int joybautomap = -1;
int joybjump = -1;

//
// Allow artifacts to be used when the run key is held down.
//

int ctrl_noartiskip = 0;


// 
// Bind all of the common controls used by Doom and all other games.
//

void M_BindControls (void)
{
    //
    // Keyboard controls
    //

    // Movement

    M_BindIntVariable("key_up",                 &key_up);
    M_BindIntVariable("key_down",               &key_down);
    M_BindIntVariable("key_right",              &key_right);
    M_BindIntVariable("key_left",               &key_left);
    M_BindIntVariable("key_strafeleft",         &key_strafeleft);
    M_BindIntVariable("key_straferight",        &key_straferight);
    M_BindIntVariable("key_speed",              &key_speed);
    M_BindIntVariable("key_strafe",             &key_strafe);
    M_BindIntVariable("key_180turn",            &key_180turn);

    // Action

    M_BindIntVariable("key_fire",               &key_fire);
    M_BindIntVariable("key_use",                &key_use);

    // Advanced movement

    M_BindIntVariable("key_autorun",         &key_autorun); // [crispy]
    M_BindIntVariable("key_mouse_look",      &key_mouse_look);

    // Special keys

    M_BindIntVariable("key_reloadlevel",     &key_reloadlevel); // [crispy]
    M_BindIntVariable("key_nextlevel",       &key_nextlevel);   // [crispy]
    M_BindIntVariable("key_demospeed",       &key_demospeed);   // [crispy]
    M_BindIntVariable("key_flip_levels",     &key_flip_levels); // [crispy]

    // RestlessRodent -- CRL (Special modes)

    M_BindIntVariable("key_spectator",       &key_spectator);
    M_BindIntVariable("key_freeze",          &key_freeze);
    M_BindIntVariable("key_notarget",        &key_notarget);
    M_BindIntVariable("key_buddha",          &key_buddha);

    // Weapons

    M_BindIntVariable("key_weapon1",            &key_weapon1);
    M_BindIntVariable("key_weapon2",            &key_weapon2);
    M_BindIntVariable("key_weapon3",            &key_weapon3);
    M_BindIntVariable("key_weapon4",            &key_weapon4);
    M_BindIntVariable("key_weapon5",            &key_weapon5);
    M_BindIntVariable("key_weapon6",            &key_weapon6);
    M_BindIntVariable("key_weapon7",            &key_weapon7);
    M_BindIntVariable("key_weapon8",            &key_weapon8);
    M_BindIntVariable("key_prevweapon",         &key_prevweapon);
    M_BindIntVariable("key_nextweapon",         &key_nextweapon);

    // Automap

    M_BindIntVariable("key_map_toggle",         &key_map_toggle);
    M_BindIntVariable("key_map_zoomin",         &key_map_zoomin);
    M_BindIntVariable("key_map_zoomout",        &key_map_zoomout);
    M_BindIntVariable("key_map_maxzoom",        &key_map_maxzoom);
    M_BindIntVariable("key_map_follow",         &key_map_follow);
    M_BindIntVariable("key_map_rotate",         &key_map_rotate);
    M_BindIntVariable("key_map_overlay",        &key_map_overlay);
    M_BindIntVariable("key_map_grid",           &key_map_grid);
    M_BindIntVariable("key_map_mark",           &key_map_mark);
    M_BindIntVariable("key_map_clearmark",      &key_map_clearmark);
    M_BindIntVariable("key_map_north",          &key_map_north);
    M_BindIntVariable("key_map_south",          &key_map_south);
    M_BindIntVariable("key_map_east",           &key_map_east);
    M_BindIntVariable("key_map_west",           &key_map_west);

    // Function keys

    M_BindIntVariable("key_menu_help",          &key_menu_help);
    M_BindIntVariable("key_menu_save",          &key_menu_save);
    M_BindIntVariable("key_menu_load",          &key_menu_load);
    M_BindIntVariable("key_menu_volume",        &key_menu_volume);
    M_BindIntVariable("key_menu_detail",        &key_menu_detail);
    M_BindIntVariable("key_menu_qsave",         &key_menu_qsave);
    M_BindIntVariable("key_menu_endgame",       &key_menu_endgame);
    M_BindIntVariable("key_menu_messages",      &key_menu_messages);
    M_BindIntVariable("key_menu_qload",         &key_menu_qload);
    M_BindIntVariable("key_menu_quit",          &key_menu_quit);
    M_BindIntVariable("key_menu_gamma",         &key_menu_gamma);
    M_BindIntVariable("key_spy",                &key_spy);

    // Shortcut keys

    M_BindIntVariable("key_pause",              &key_pause);
    M_BindIntVariable("key_menu_screenshot",    &key_menu_screenshot);
    M_BindIntVariable("key_demo_quit",          &key_demo_quit);

    // Special menu keys, not available for rebinding

    M_BindIntVariable("key_menu_activate",      &key_menu_activate);
    M_BindIntVariable("key_menu_up",            &key_menu_up);
    M_BindIntVariable("key_menu_down",          &key_menu_down);
    M_BindIntVariable("key_menu_left",          &key_menu_left);
    M_BindIntVariable("key_menu_right",         &key_menu_right);
    M_BindIntVariable("key_menu_back",          &key_menu_back);
    M_BindIntVariable("key_menu_forward",       &key_menu_forward);
    M_BindIntVariable("key_menu_confirm",       &key_menu_confirm);
    M_BindIntVariable("key_menu_abort",         &key_menu_abort);
    M_BindIntVariable("key_menu_incscreen",     &key_menu_incscreen);
    M_BindIntVariable("key_menu_decscreen",     &key_menu_decscreen);

    //
    // Mouse controls
    //

    M_BindIntVariable("mouseb_fire",            &mousebfire);
    M_BindIntVariable("mouseb_forward",         &mousebforward);
    M_BindIntVariable("mouseb_strafe",          &mousebstrafe);
    M_BindIntVariable("mouseb_backward",        &mousebbackward);
    M_BindIntVariable("mouseb_use",             &mousebuse);
    M_BindIntVariable("mouseb_strafeleft",      &mousebstrafeleft);
    M_BindIntVariable("mouseb_straferight",     &mousebstraferight);
    M_BindIntVariable("mouseb_prevweapon",      &mousebprevweapon);
    M_BindIntVariable("mouseb_nextweapon",      &mousebnextweapon);

    M_BindIntVariable("mouse_dclick_use",             &mouse_dclick_use);

    //
    // Joystick controls
    //

    M_BindIntVariable("joyb_fire",              &joybfire);
    M_BindIntVariable("joyb_strafe",            &joybstrafe);
    M_BindIntVariable("joyb_use",               &joybuse);
    M_BindIntVariable("joyb_speed",             &joybspeed);
    M_BindIntVariable("joyb_strafeleft",        &joybstrafeleft);
    M_BindIntVariable("joyb_straferight",       &joybstraferight);
    M_BindIntVariable("joyb_prevweapon",        &joybprevweapon);
    M_BindIntVariable("joyb_nextweapon",        &joybnextweapon);
    M_BindIntVariable("joyb_menu_activate",     &joybmenu);
    M_BindIntVariable("joyb_toggle_automap",    &joybautomap);
}

void M_BindHereticControls (void)
{
    M_BindIntVariable("key_lookup",         &key_lookup);
    M_BindIntVariable("key_lookdown",       &key_lookdown);
    M_BindIntVariable("key_lookcenter",     &key_lookcenter);

    M_BindIntVariable("key_flyup",          &key_flyup);
    M_BindIntVariable("key_flydown",        &key_flydown);
    M_BindIntVariable("key_flycenter",      &key_flycenter);

    M_BindIntVariable("key_invleft",        &key_invleft);
    M_BindIntVariable("key_invright",       &key_invright);
    M_BindIntVariable("key_useartifact",    &key_useartifact);

    M_BindIntVariable("key_arti_quartz",        &key_arti_quartz);
    M_BindIntVariable("key_arti_urn",           &key_arti_urn);
    M_BindIntVariable("key_arti_bomb",          &key_arti_bomb);
    M_BindIntVariable("key_arti_tome",          &key_arti_tome);
    M_BindIntVariable("key_arti_ring",          &key_arti_ring);
    M_BindIntVariable("key_arti_chaosdevice",   &key_arti_chaosdevice);
    M_BindIntVariable("key_arti_shadowsphere",  &key_arti_shadowsphere);
    M_BindIntVariable("key_arti_wings",         &key_arti_wings);
    M_BindIntVariable("key_arti_torch",         &key_arti_torch);
    M_BindIntVariable("key_arti_morph",         &key_arti_morph);

    M_BindIntVariable("mouseb_invleft",     &mousebinvleft);
    M_BindIntVariable("mouseb_invright",    &mousebinvright);
    M_BindIntVariable("mouseb_useartifact", &mousebuseartifact);

    M_BindIntVariable("ctrl_noartiskip",        &ctrl_noartiskip);
}

void M_BindHexenControls(void)
{
    M_BindIntVariable("key_jump",           &key_jump);
    M_BindIntVariable("mouseb_jump",        &mousebjump);
    M_BindIntVariable("joyb_jump",          &joybjump);

    M_BindIntVariable("key_arti_all",             &key_arti_all);
    M_BindIntVariable("key_arti_health",          &key_arti_health);
    M_BindIntVariable("key_arti_poisonbag",       &key_arti_poisonbag);
    M_BindIntVariable("key_arti_blastradius",     &key_arti_blastradius);
    M_BindIntVariable("key_arti_teleport",        &key_arti_teleport);
    M_BindIntVariable("key_arti_teleportother",   &key_arti_teleportother);
    M_BindIntVariable("key_arti_egg",             &key_arti_egg);
    M_BindIntVariable("key_arti_invulnerability", &key_arti_invulnerability);
}

void M_BindChatControls (unsigned int num_players)
{
    char name[32];  // haleyjd: 20 not large enough - Thank you, come again!
    unsigned int i; // haleyjd: signedness conflict

    M_BindIntVariable("key_multi_msg",     &key_multi_msg);

    for (i = 0 ; i < num_players ; ++i)
    {
        M_snprintf(name, sizeof(name), "key_multi_msgplayer%i", i + 1);
        M_BindIntVariable(name, &key_multi_msgplayer[i]);
    }
}
