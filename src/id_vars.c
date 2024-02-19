//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
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


#include "id_vars.h"
#include "m_config.h"  // [JN] M_BindIntVariable


// Game modes
int crl_spectating = 0;  // RestlessRodent -- CRL
int crl_freeze = 0;


// -----------------------------------------------------------------------------
// [JN] ID-specific config variables.
// -----------------------------------------------------------------------------

//
// Video options
//

int vid_truecolor = 0;
int vid_resolution = 2;
int vid_widescreen = 0;
int vid_uncapped_fps = 0;
int vid_fpslimit = 60;
int vid_vsync = 1;
int vid_showfps = 0;
int vid_smooth_scaling = 0;
// Miscellaneous
int vid_screenwipe = 1;
int vid_diskicon = 1;
int vid_endoom = 0;
int vid_graphical_startup = 0;
int vid_banners = 1;

//
// Display options
//

int dp_screen_size = 10;
int dp_detail_level = 0;  // Blocky mode (0 = high, 1 = normal)
int vid_gamma = 10;
int vid_fov = 90;
int dp_menu_shading = 0;
int dp_level_brightness = 0;
// Color settings
int vid_saturation = 100;
float vid_r_intensity = 1.000000;
float vid_g_intensity = 1.000000;
float vid_b_intensity = 1.000000;
// Messages Settings
int msg_show = 1;
int msg_alignment = 0;
int msg_text_shadows = 0;
int msg_local_time = 0;

//
// Sound options
//

int snd_monosfx = 0;
int snd_channels = 8;
int snd_mute_inactive = 0;

//
// Control settings
//

int mouse_look = 0;

//
// Widgets and automap
//

int widget_location = 0;
int widget_kis = 0;
int widget_time = 0;
int widget_totaltime = 0;
int widget_levelname = 0;
int widget_coords = 0;
int widget_render = 0;
int widget_health = 0;
// Automap
int automap_scheme = 0;
int automap_smooth = 0;
int automap_secrets = 0;
int automap_rotate = 0;
int automap_overlay = 0;
int automap_shading = 0;

//
// Gameplay features
//

// Visual
int vis_brightmaps = 0;
int vis_translucency = 0;
int vis_fake_contrast = 1;
int vis_smooth_light = 0;
int vis_improved_fuzz = 0;
int vis_colored_blood = 0;
int vis_swirling_liquids = 0;
int vis_invul_sky = 0;
int vis_linear_sky = 0;
int vis_flip_corpses = 0;

// Crosshair
int xhair_draw = 0;
int xhair_color = 0;

// Status bar
int st_colored_stbar = 0;
int st_negative_health = 0;
int st_blinking_keys = 0;
int st_ammo_widget = 0;
int st_ammo_widget_translucent = 0;
int st_ammo_widget_colors = 0;
int st_weapon_widget = 0;
int st_armor_icon = 0;

// Audible
int aud_z_axis_sfx = 0;
int aud_full_sounds = 0;
int aud_exit_sounds = 0;

// Physical
int phys_torque = 0;
int phys_ssg_tear_monsters = 0;
int phys_toss_drop = 0;
int phys_floating_powerups = 0;
int phys_weapon_alignment = 0;
int phys_breathing = 0;

// Gameplay
int gp_default_class = 0;
int gp_default_skill = 2;
int gp_revealed_secrets = 0;
int gp_flip_levels = 0;
int gp_death_use_action = 0;

// Demos
int demo_timer = 0;
int demo_timerdir = 0;
int demo_bar = 0;
int demo_internal = 1;

// Compatibility-breaking
int compat_pistol_start = 0;
int compat_blockmap_fix = 0;
int compat_vertical_aiming = 0;


// -----------------------------------------------------------------------------
// [JN] ID-specific config variables binding functions.
// -----------------------------------------------------------------------------

void ID_BindVariables (GameMission_t mission)
{
    //
    // Video options
    //

    M_BindIntVariable("vid_truecolor",                  &vid_truecolor);
    M_BindIntVariable("vid_resolution",                 &vid_resolution);
    M_BindIntVariable("vid_widescreen",                 &vid_widescreen);
    M_BindIntVariable("vid_uncapped_fps",               &vid_uncapped_fps);
    M_BindIntVariable("vid_fpslimit",                   &vid_fpslimit);
    M_BindIntVariable("vid_vsync",                      &vid_vsync);
    M_BindIntVariable("vid_showfps",                    &vid_showfps);
    M_BindIntVariable("vid_smooth_scaling",             &vid_smooth_scaling);
    // Miscellaneous
    if (mission == doom)
    {
        M_BindIntVariable("vid_screenwipe",             &vid_screenwipe);
        M_BindIntVariable("vid_diskicon",               &vid_diskicon);
    }
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("vid_endoom",                 &vid_endoom);
    }
    if (mission == hexen)
    {
        M_BindIntVariable("vid_graphical_startup",      &vid_graphical_startup);
        M_BindIntVariable("vid_banners",                &vid_banners);
    }  

    //
    // Display options
    //

    M_BindIntVariable("dp_screen_size",                 &dp_screen_size);
    M_BindIntVariable("dp_detail_level",                &dp_detail_level);
    M_BindIntVariable("vid_gamma",                      &vid_gamma);
    M_BindIntVariable("vid_fov",                        &vid_fov);
    M_BindIntVariable("dp_menu_shading",                &dp_menu_shading);
    M_BindIntVariable("dp_level_brightness",            &dp_level_brightness);
    // Color settings
    M_BindIntVariable("vid_saturation",                 &vid_saturation);
    M_BindFloatVariable("vid_r_intensity",              &vid_r_intensity);
    M_BindFloatVariable("vid_g_intensity",              &vid_g_intensity);
    M_BindFloatVariable("vid_b_intensity",              &vid_b_intensity);
    // Messages Settings
    M_BindIntVariable("msg_show",                       &msg_show);
    if (mission == doom)
    {
        M_BindIntVariable("msg_alignment",              &msg_alignment);
    }
    M_BindIntVariable("msg_text_shadows",               &msg_text_shadows);
    M_BindIntVariable("msg_local_time",                 &msg_local_time);    

    //
    // Sound options
    //

    M_BindIntVariable("snd_monosfx",                    &snd_monosfx);
    M_BindIntVariable("snd_channels",                   &snd_channels);
    M_BindIntVariable("snd_mute_inactive",              &snd_mute_inactive);

    //
    // Control settings
    //

    M_BindIntVariable("mouse_look",                     &mouse_look);

    //
    // Widgets and automap
    //

    M_BindIntVariable("widget_location",                &widget_location);
    M_BindIntVariable("widget_kis",                     &widget_kis);
    M_BindIntVariable("widget_time",                    &widget_time);
    M_BindIntVariable("widget_totaltime",               &widget_totaltime);
    M_BindIntVariable("widget_levelname",               &widget_levelname);
    M_BindIntVariable("widget_coords",                  &widget_coords);
    M_BindIntVariable("widget_render",                  &widget_render);
    M_BindIntVariable("widget_health",                  &widget_health);
    // Automap
    if (mission == doom)
    {
        M_BindIntVariable("automap_scheme",             &automap_scheme);
        M_BindIntVariable("automap_smooth",             &automap_smooth);
    }
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("automap_secrets",            &automap_secrets);
    }
    M_BindIntVariable("automap_rotate",                 &automap_rotate);
    M_BindIntVariable("automap_overlay",                &automap_overlay);
    M_BindIntVariable("automap_shading",                &automap_shading);

    //
    // Gameplay features
    //

    // Visual
    M_BindIntVariable("vis_brightmaps",                 &vis_brightmaps);
    M_BindIntVariable("vis_translucency",               &vis_translucency);
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("vis_fake_contrast",          &vis_fake_contrast);
    }
    M_BindIntVariable("vis_smooth_light",               &vis_smooth_light);
    if (mission == doom)
    {
        M_BindIntVariable("vis_improved_fuzz",          &vis_improved_fuzz);
        M_BindIntVariable("vis_colored_blood",          &vis_colored_blood);
    }
    M_BindIntVariable("vis_swirling_liquids",           &vis_swirling_liquids);
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("vis_invul_sky",              &vis_invul_sky);
    }
    M_BindIntVariable("vis_linear_sky",                 &vis_linear_sky);
    M_BindIntVariable("vis_flip_corpses",               &vis_flip_corpses);
    
    // Crosshair
    M_BindIntVariable("xhair_draw",                     &xhair_draw);
    M_BindIntVariable("xhair_color",                    &xhair_color);
    
    // Status bar
    M_BindIntVariable("st_colored_stbar",               &st_colored_stbar);
    if (mission == doom)
    {
        M_BindIntVariable("st_negative_health",         &st_negative_health);
        M_BindIntVariable("st_blinking_keys",           &st_blinking_keys);
    }
    if (mission == heretic)
    {
        M_BindIntVariable("st_ammo_widget",             &st_ammo_widget);
        M_BindIntVariable("st_ammo_widget_translucent", &st_ammo_widget_translucent);
        M_BindIntVariable("st_ammo_widget_colors",      &st_ammo_widget_colors);
    }
    if (mission == hexen)
    {
        M_BindIntVariable("st_weapon_widget",           &st_weapon_widget);
        M_BindIntVariable("st_armor_icon",              &st_armor_icon);
    }        
    
    // Audible
    M_BindIntVariable("aud_z_axis_sfx",                 &aud_z_axis_sfx);
    if (mission == doom)
    {
        M_BindIntVariable("aud_full_sounds",            &aud_full_sounds);
        M_BindIntVariable("aud_exit_sounds",            &aud_exit_sounds);
    }
    
    // Physical
    M_BindIntVariable("phys_torque",                    &phys_torque);
    if (mission == doom)
    {
        M_BindIntVariable("phys_ssg_tear_monsters",     &phys_ssg_tear_monsters);
        M_BindIntVariable("phys_toss_drop",             &phys_toss_drop);
        M_BindIntVariable("phys_floating_powerups",     &phys_floating_powerups);
    }
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("phys_weapon_alignment",      &phys_weapon_alignment);
    }
    M_BindIntVariable("phys_breathing",                 &phys_breathing);
    
    // Gameplay
    if (mission == hexen)
    {
        M_BindIntVariable("gp_default_class",           &gp_default_class);
    }
    M_BindIntVariable("gp_default_skill",               &gp_default_skill);
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("gp_revealed_secrets",        &gp_revealed_secrets);
    }
    M_BindIntVariable("gp_flip_levels",                 &gp_flip_levels);
    M_BindIntVariable("gp_death_use_action",            &gp_death_use_action);
    
    // Demos
    M_BindIntVariable("demo_timer",                     &demo_timer);
    M_BindIntVariable("demo_timerdir",                  &demo_timerdir);
    M_BindIntVariable("demo_bar",                       &demo_bar);
    M_BindIntVariable("demo_internal",                  &demo_internal);
    
    // Compatibility-breaking
    if (mission == doom || mission == heretic)
    {
        M_BindIntVariable("compat_pistol_start",        &compat_pistol_start);
    }
    M_BindIntVariable("compat_blockmap_fix",            &compat_blockmap_fix);
    if (mission == doom)
    {
        M_BindIntVariable("compat_vertical_aiming",     &compat_vertical_aiming);
    }
}
