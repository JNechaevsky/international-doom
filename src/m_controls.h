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


#pragma once


//
// Keyboard controls
//

// Movement

extern int key_up;
extern int key_down;
extern int key_right;
extern int key_left;
extern int key_strafeleft;
extern int key_straferight;
extern int key_speed;
extern int key_strafe;
extern int key_180turn;

// Action

extern int key_fire;
extern int key_use;

// Advanced movement

extern int key_autorun;
extern int key_mouse_look; // [crispy]
extern int key_novert;

// Special keys

extern int key_nextlevel;   // [crispy]
extern int key_reloadlevel; // [crispy]
extern int key_demospeed;   // [crispy]
extern int key_flip_levels; // [crispy]

// Heretic

extern int key_flyup;
extern int key_flydown;
extern int key_flycenter;
extern int key_lookup;
extern int key_lookdown;
extern int key_lookcenter;
extern int key_invleft;
extern int key_invright;
extern int key_useartifact;

// Hexen

extern int key_jump;

// RestlessRodent -- CRL (Special modes)

extern int key_spectator;
extern int key_freeze;
extern int key_notarget;
extern int key_buddha;

// Weapons

extern int key_weapon1;
extern int key_weapon2;
extern int key_weapon3;
extern int key_weapon4;
extern int key_weapon5;
extern int key_weapon6;
extern int key_weapon7;
extern int key_weapon8;
extern int key_prevweapon;
extern int key_nextweapon;

// Inventory

extern int key_arti_quartz;
extern int key_arti_urn;
extern int key_arti_bomb;
extern int key_arti_tome;
extern int key_arti_ring;
extern int key_arti_chaosdevice;
extern int key_arti_shadowsphere;
extern int key_arti_wings;
extern int key_arti_torch;
extern int key_arti_morph;

extern int key_arti_all;
extern int key_arti_health;
extern int key_arti_poisonbag;
extern int key_arti_blastradius;
extern int key_arti_teleport;
extern int key_arti_teleportother;
extern int key_arti_egg;
extern int key_arti_invulnerability;
// Extra artifacts
extern int key_arti_servant;
extern int key_arti_bracers;
extern int key_arti_boots;
extern int key_arti_krater;
extern int key_arti_incant;

// Automap

extern int key_map_toggle;
extern int key_map_zoomin;
extern int key_map_zoomout;
extern int key_map_maxzoom;
extern int key_map_follow;
extern int key_map_rotate;
extern int key_map_overlay;
extern int key_map_grid;
extern int key_map_mark;
extern int key_map_clearmark;
extern int key_map_north;
extern int key_map_south;
extern int key_map_east;
extern int key_map_west;

// Function keys

extern int key_menu_help;
extern int key_menu_save;
extern int key_menu_load;
extern int key_menu_volume;
extern int key_menu_detail;
extern int key_menu_qsave;
extern int key_menu_endgame;
extern int key_menu_messages;
extern int key_menu_qload;
extern int key_menu_quit;
extern int key_menu_gamma;
extern int key_spy;

// Shortcut keys

extern int key_pause;
extern int key_menu_screenshot;
extern int key_message_refresh;
extern int key_message_refresh_hr;
extern int key_demo_quit;

// Multiplayer

extern int key_multi_msg;
extern int key_multi_msgplayer[8];

// Special menu keys, not available for rebinding

extern int key_menu_activate;
extern int key_menu_up;
extern int key_menu_down;
extern int key_menu_left;
extern int key_menu_right;
extern int key_menu_back;
extern int key_menu_forward;
extern int key_menu_confirm;
extern int key_menu_abort;
extern int key_menu_incscreen;
extern int key_menu_decscreen;
extern int key_menu_del; // [crispy]

//
// Mouse controls
//

extern int mousebfire;
extern int mousebforward;
extern int mousebstrafe;
extern int mousebbackward;
extern int mousebuse;
extern int mousebstrafeleft;
extern int mousebstraferight;
extern int mousebprevweapon;
extern int mousebnextweapon;

extern int mousebinvleft;
extern int mousebinvright;
extern int mousebuseartifact;

extern int mousebjump;

// Control whether if a mouse button is double clicked,
// it acts like "use" has been pressed.

extern int mouse_dclick_use;

//
// Joystick controls
//

extern int joybfire;
extern int joybstrafe;
extern int joybuse;
extern int joybspeed;
extern int joybstrafeleft;
extern int joybstraferight;
extern int joybprevweapon;
extern int joybnextweapon;
extern int joybmenu;
extern int joybautomap;
extern int joybjump;

//
// Allow artifacts to be used when the run key is held down.
//

extern int ctrl_noartiskip;


extern void M_BindControls (void);
extern void M_BindHereticControls (void);
extern void M_BindHexenControls (void);
extern void M_BindChatControls (unsigned int num_players);
