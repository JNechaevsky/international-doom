//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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


#include "d_mode.h"  // [JN] M_BindIntVariable


#pragma once


// -----------------------------------------------------------------------------
// [JN] ID-specific config variables.
// -----------------------------------------------------------------------------

// System and video
extern int vid_truecolor;
extern int vid_resolution;
extern int vid_widescreen;

extern int vid_diskicon;
extern int vid_endoom;
extern int vid_graphical_startup;
extern int vid_banners;

extern int vid_uncapped_fps;
extern int vid_fpslimit;
extern int vid_vsync;
extern int vid_showfps;
extern int vid_gamma;
extern int vid_fov;
extern int vid_saturation;
extern float vid_contrast;
extern float vid_r_intensity;
extern float vid_g_intensity;
extern float vid_b_intensity;
extern int dp_screen_size;
extern int vid_screenwipe;
extern int vid_screenwipe_hr;
extern int msg_text_shadows;

extern int post_supersample;
extern int post_overglow;
extern int post_bloom;
extern int post_rgbdrift;
extern int post_vhsdist;
extern int post_vignette;
extern int post_filmgrain;
extern int post_motionblur;
extern int post_dofblur;

// Display
extern int dp_detail_level;
extern int dp_menu_shading;
extern int dp_level_brightness;

// Messages
extern int msg_show;
extern int msg_alignment;
extern int msg_local_time;

// Game modes
extern int crl_spectating;
extern int crl_freeze;

// Widgets
extern int widget_enable;
extern int widget_scheme;
extern int widget_location;
extern int widget_alignment;
extern int widget_coords;
extern int widget_speed;
extern int widget_render;
extern int widget_kis;
extern int widget_kis_format;
extern int widget_kis_items;
extern int widget_time;
extern int widget_totaltime;
extern int widget_levelname;
extern int widget_health;

// Sound
extern int snd_monosfx;
extern int snd_channels;
extern int snd_mute_inactive;
extern int snd_remaster_ost;

// Automap
extern int automap_scheme;
extern int automap_smooth;
extern int automap_smooth_hr;
extern int automap_thick;
extern int automap_square;
extern int automap_textured_bg;
extern int automap_scroll_bg;
extern int automap_secrets;
extern int automap_rotate;
extern int automap_overlay;
extern int automap_shading;
extern int automap_mouse_pan;

// Gameplay features
extern int vis_brightmaps;
extern int vis_translucency;
extern int vis_fake_contrast;
extern int vis_smooth_light;
extern int vis_smooth_palette;
extern int vis_improved_fuzz;
extern int vis_colored_blood;
extern int vis_swirling_liquids;
extern int vis_invul_sky;
extern int vis_linear_sky;
extern int vis_flip_corpses;

extern int xhair_draw;
extern int xhair_color;

extern int st_fullscreen_layout;
extern int st_colored_stbar;
extern int st_negative_health;
extern int st_blinking_keys;
extern int st_ammo_widget;  // Heretic only
extern int st_ammo_widget_translucent;  // Heretic only
extern int st_ammo_widget_colors;  // Heretic only
extern int st_weapon_widget;  // Hexen only
extern int st_armor_icon;  // Hexen only
extern int st_armor_value;  // Hexen only

extern int aud_z_axis_sfx;
extern int aud_full_sounds;
extern int aud_crushed_corpse;
extern int aud_exit_sounds;

extern int phys_torque;
extern int phys_ssg_tear_monsters;
extern int phys_toss_drop;
extern int phys_floating_powerups;
extern int phys_weapon_alignment;

extern int gp_default_class;
extern int gp_default_skill;
extern int gp_revealed_secrets;
extern char *gp_revealed_secrets_msg;
extern int phys_breathing;
extern int gp_flip_levels;
extern int gp_death_use_action;

// Demos
extern int demo_timer;
extern int demo_timerdir;
extern int demo_bar;
extern int demo_internal;

// Compatibility-breaking
extern int compat_pistol_start;
extern int compat_blockmap_fix;
extern int compat_vertical_aiming;
extern int compat_auto_sr50;
extern int compat_intercepts;

// Miscellaneous
extern int a11y_invul;
extern int a11y_pal_flash;
extern int a11y_move_bob;
extern int a11y_weapon_bob;
extern int a11y_quake_intensity;
extern int a11y_colorblind;
extern int autoload_wad;
extern int autoload_deh;
extern int autoload_hhe;
extern int autoload_seh;
extern int menu_highlight;
extern int menu_esc_key;
extern int menu_cap_fps;

// Mouse look
extern int mouse_look;

extern void ID_BindVariables (GameMission_t mission);
