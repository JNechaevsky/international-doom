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
// DESCRIPTION:
//    Configuration file interface.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <locale.h>

#include "SDL_filesystem.h"

#include "config.h"

#include "doomtype.h"
#include "doomkeys.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"

#include "z_zone.h"

//
// DEFAULTS
//

// Location where all configuration data is stored - 
// default.cfg, savegames, etc.

const char *configdir;

static char *autoload_path = "";

// Default filenames for configuration files.

static const char *default_main_config;

// [JN] "savegames_path" config file variable.

char *SavePathConfig = "";

// [JN] Location where screenshots are saved.

char *screenshotdir;

// [JN] "screenshots_path" config file variable.

char *ShotPathConfig = "";

typedef enum 
{
    DEFAULT_INT,
    DEFAULT_INT_HEX,
    DEFAULT_STRING,
    DEFAULT_FLOAT,
    DEFAULT_KEY,
} default_type_t;

typedef struct
{
    // Name of the variable
    const char *name;

    // Pointer to the location in memory of the variable
    union {
        int *i;
        char **s;
        float *f;
    } location;

    // Type of the variable
    default_type_t type;

    // If this is a key value, the original integer scancode we read from
    // the config file before translating it to the internal key value.
    // If zero, we didn't read this value from a config file.
    int untranslated;

    // The value we translated the scancode into when we read the 
    // config file on startup.  If the variable value is different from
    // this, it has been changed and needs to be converted; otherwise,
    // use the 'untranslated' value.
    int original_translated;

    // If true, this config variable has been bound to a variable
    // and is being used.
    boolean bound;
} default_t;

typedef struct
{
    default_t *defaults;
    int numdefaults;
    const char *filename;
} default_collection_t;

#define CONFIG_VARIABLE_GENERIC(name, type) \
    { #name, {NULL}, type, 0, 0, false }

#define CONFIG_VARIABLE_KEY(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_KEY)
#define CONFIG_VARIABLE_INT(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_INT)
#define CONFIG_VARIABLE_INT_HEX(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_INT_HEX)
#define CONFIG_VARIABLE_FLOAT(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_FLOAT)
#define CONFIG_VARIABLE_STRING(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_STRING)

//! @begin_config_file default

static default_t	doom_defaults_list[] =
{
    //
    // Autoload
    //

    CONFIG_VARIABLE_STRING(autoload_path),
    CONFIG_VARIABLE_STRING(music_pack_path),
    CONFIG_VARIABLE_STRING(savegames_path),
    CONFIG_VARIABLE_STRING(screenshots_path),

    //
    // Render
    //

    CONFIG_VARIABLE_INT(vid_startup_delay),
    CONFIG_VARIABLE_INT(vid_resize_delay),
    CONFIG_VARIABLE_STRING(vid_video_driver),
    CONFIG_VARIABLE_STRING(vid_screen_scale_api),
    CONFIG_VARIABLE_INT(vid_fullscreen),
    CONFIG_VARIABLE_INT(vid_window_position_x),
    CONFIG_VARIABLE_INT(vid_window_position_y),
    CONFIG_VARIABLE_INT(vid_video_display),
    CONFIG_VARIABLE_INT(vid_aspect_ratio_correct),
    CONFIG_VARIABLE_INT(vid_integer_scaling),
    CONFIG_VARIABLE_INT(vid_vga_porch_flash),
    CONFIG_VARIABLE_INT(vid_window_width),
    CONFIG_VARIABLE_INT(vid_window_height),
    CONFIG_VARIABLE_INT(vid_fullscreen_width),
    CONFIG_VARIABLE_INT(vid_fullscreen_height),
    CONFIG_VARIABLE_INT(vid_window_title_short),
    CONFIG_VARIABLE_INT(vid_force_software_renderer),
    CONFIG_VARIABLE_INT(vid_max_scaling_buffer_pixels),

    // Video options
    CONFIG_VARIABLE_INT(vid_truecolor),
    CONFIG_VARIABLE_INT(vid_resolution),
    CONFIG_VARIABLE_INT(vid_widescreen),
    CONFIG_VARIABLE_INT(vid_uncapped_fps),
    CONFIG_VARIABLE_INT(vid_fpslimit),
    CONFIG_VARIABLE_INT(vid_vsync),
    CONFIG_VARIABLE_INT(vid_showfps),
    CONFIG_VARIABLE_INT(vid_smooth_scaling),
    CONFIG_VARIABLE_INT(vid_screenwipe),
    CONFIG_VARIABLE_INT(vid_diskicon),
    CONFIG_VARIABLE_INT(vid_endoom),
    CONFIG_VARIABLE_INT(vid_graphical_startup),    
    CONFIG_VARIABLE_INT(vid_banners),
    CONFIG_VARIABLE_INT(vid_exit_screen),

    // Display options
    CONFIG_VARIABLE_INT(vid_gamma),
    CONFIG_VARIABLE_INT(vid_fov),
    CONFIG_VARIABLE_INT(vid_saturation),
    CONFIG_VARIABLE_FLOAT(vid_r_intensity),
    CONFIG_VARIABLE_FLOAT(vid_g_intensity),
    CONFIG_VARIABLE_FLOAT(vid_b_intensity),
    CONFIG_VARIABLE_INT(dp_screen_size),
    CONFIG_VARIABLE_INT(dp_detail_level),
    CONFIG_VARIABLE_INT(dp_menu_shading),
    CONFIG_VARIABLE_INT(dp_level_brightness),

    // Messages
    CONFIG_VARIABLE_INT(msg_show),
    CONFIG_VARIABLE_INT(msg_alignment),
    CONFIG_VARIABLE_INT(msg_text_shadows),
    CONFIG_VARIABLE_INT(msg_local_time),

    //
    // Sound and Music
    //

    CONFIG_VARIABLE_INT(sfx_volume),
    CONFIG_VARIABLE_INT(music_volume),
    CONFIG_VARIABLE_INT(snd_sfxdevice),
    CONFIG_VARIABLE_INT(snd_musicdevice),
    CONFIG_VARIABLE_STRING(snd_musiccmd),
    CONFIG_VARIABLE_STRING(snd_dmxoption),
    CONFIG_VARIABLE_INT_HEX(opl_io_port),
    CONFIG_VARIABLE_INT(snd_monosfx),
    CONFIG_VARIABLE_INT(snd_pitchshift),
    CONFIG_VARIABLE_INT(snd_channels),
    CONFIG_VARIABLE_INT(snd_mute_inactive),
#ifdef HAVE_FLUIDSYNTH
    CONFIG_VARIABLE_INT(fsynth_chorus_active),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_depth),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_level),
    CONFIG_VARIABLE_INT(fsynth_chorus_nr),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_speed),
    CONFIG_VARIABLE_STRING(fsynth_midibankselect),
    CONFIG_VARIABLE_INT(fsynth_polyphony),
    CONFIG_VARIABLE_INT(fsynth_reverb_active),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_damp),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_level),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_roomsize),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_width),
    CONFIG_VARIABLE_FLOAT(fsynth_gain),
    CONFIG_VARIABLE_STRING(fsynth_sf_path),
#endif // HAVE_FLUIDSYNTH
    CONFIG_VARIABLE_STRING(timidity_cfg_path),
    CONFIG_VARIABLE_STRING(gus_patch_path),
    CONFIG_VARIABLE_INT(gus_ram_kb),
#ifdef _WIN32
    CONFIG_VARIABLE_STRING(winmm_midi_device),
    CONFIG_VARIABLE_INT(winmm_complevel),
    CONFIG_VARIABLE_INT(winmm_reset_type),
    CONFIG_VARIABLE_INT(winmm_reset_delay),
#endif

    CONFIG_VARIABLE_INT(use_libsamplerate),
    CONFIG_VARIABLE_FLOAT(libsamplerate_scale),
    CONFIG_VARIABLE_INT(snd_samplerate),
    CONFIG_VARIABLE_INT(snd_cachesize),
    CONFIG_VARIABLE_INT(snd_maxslicetime_ms),

    //
    // Keyboard controls
    //

    // Movement
    CONFIG_VARIABLE_KEY(key_up),
    CONFIG_VARIABLE_KEY(key_down),
    CONFIG_VARIABLE_KEY(key_left),
    CONFIG_VARIABLE_KEY(key_right),
    CONFIG_VARIABLE_KEY(key_strafeleft),
    CONFIG_VARIABLE_KEY(key_straferight),
    CONFIG_VARIABLE_KEY(key_speed),
    CONFIG_VARIABLE_KEY(key_strafe),
    CONFIG_VARIABLE_KEY(key_180turn),

    // Action
    CONFIG_VARIABLE_KEY(key_fire),
    CONFIG_VARIABLE_KEY(key_use),

    // Heretic: View
    CONFIG_VARIABLE_KEY(key_lookup),
    CONFIG_VARIABLE_KEY(key_lookdown),
    CONFIG_VARIABLE_KEY(key_lookcenter),

    // Heretic: Fly
    CONFIG_VARIABLE_KEY(key_flyup),
    CONFIG_VARIABLE_KEY(key_flydown),
    CONFIG_VARIABLE_KEY(key_flycenter),

    // Heretic: Inventory
    CONFIG_VARIABLE_KEY(key_invleft),
    CONFIG_VARIABLE_KEY(key_invright),
    CONFIG_VARIABLE_KEY(key_useartifact),

    // Hexen: Jump
    CONFIG_VARIABLE_KEY(key_jump),

    // Strife:
    CONFIG_VARIABLE_INT(show_talk),
    CONFIG_VARIABLE_INT(voice_volume),
    CONFIG_VARIABLE_KEY(key_invUse),
    CONFIG_VARIABLE_KEY(key_invDrop),
    CONFIG_VARIABLE_KEY(key_invLeft),
    CONFIG_VARIABLE_KEY(key_invRight),
    CONFIG_VARIABLE_KEY(key_lookUp),
    CONFIG_VARIABLE_KEY(key_lookDown),    
    CONFIG_VARIABLE_KEY(key_useHealth),
    CONFIG_VARIABLE_KEY(key_invquery),
    CONFIG_VARIABLE_KEY(key_mission),
    CONFIG_VARIABLE_KEY(key_invPop),
    CONFIG_VARIABLE_KEY(key_invKey),
    CONFIG_VARIABLE_KEY(key_invHome),
    CONFIG_VARIABLE_KEY(key_invEnd),

    // Advanced movement
    CONFIG_VARIABLE_KEY(key_autorun),
    CONFIG_VARIABLE_KEY(key_mouse_look),
    CONFIG_VARIABLE_KEY(key_novert),

    // Special keys
    CONFIG_VARIABLE_KEY(key_reloadlevel),
    CONFIG_VARIABLE_KEY(key_nextlevel),
    CONFIG_VARIABLE_KEY(key_demospeed),
    CONFIG_VARIABLE_KEY(key_flip_levels),
    CONFIG_VARIABLE_KEY(key_widget_enable),

    // Heretic: Artifacts
    CONFIG_VARIABLE_KEY(key_arti_quartz),
    CONFIG_VARIABLE_KEY(key_arti_urn),
    CONFIG_VARIABLE_KEY(key_arti_bomb),
    CONFIG_VARIABLE_KEY(key_arti_tome),
    CONFIG_VARIABLE_KEY(key_arti_ring),
    CONFIG_VARIABLE_KEY(key_arti_chaosdevice),
    CONFIG_VARIABLE_KEY(key_arti_shadowsphere),
    CONFIG_VARIABLE_KEY(key_arti_wings),
    CONFIG_VARIABLE_KEY(key_arti_torch),
    CONFIG_VARIABLE_KEY(key_arti_morph),

    // Hexen: Artifacts
    CONFIG_VARIABLE_KEY(key_arti_all),
    CONFIG_VARIABLE_KEY(key_arti_health),
    CONFIG_VARIABLE_KEY(key_arti_poisonbag),
    CONFIG_VARIABLE_KEY(key_arti_blastradius),
    CONFIG_VARIABLE_KEY(key_arti_teleport),
    CONFIG_VARIABLE_KEY(key_arti_teleportother),
    CONFIG_VARIABLE_KEY(key_arti_egg),
    CONFIG_VARIABLE_KEY(key_arti_invulnerability),
    // Hexen: Artifacts (extra)
    CONFIG_VARIABLE_KEY(key_arti_servant),
    CONFIG_VARIABLE_KEY(key_arti_bracers),
    CONFIG_VARIABLE_KEY(key_arti_boots),
    CONFIG_VARIABLE_KEY(key_arti_krater),
    CONFIG_VARIABLE_KEY(key_arti_incant),

    // Game modes
    CONFIG_VARIABLE_KEY(key_spectator),  // RestlessRodent -- CRL
    CONFIG_VARIABLE_KEY(key_freeze),
    CONFIG_VARIABLE_KEY(key_notarget),
    CONFIG_VARIABLE_KEY(key_buddha),

    // Weapons
    CONFIG_VARIABLE_KEY(key_weapon1),
    CONFIG_VARIABLE_KEY(key_weapon2),
    CONFIG_VARIABLE_KEY(key_weapon3),
    CONFIG_VARIABLE_KEY(key_weapon4),
    CONFIG_VARIABLE_KEY(key_weapon5),
    CONFIG_VARIABLE_KEY(key_weapon6),
    CONFIG_VARIABLE_KEY(key_weapon7),
    CONFIG_VARIABLE_KEY(key_weapon8),
    CONFIG_VARIABLE_KEY(key_prevweapon),
    CONFIG_VARIABLE_KEY(key_nextweapon),

    // Automap
    CONFIG_VARIABLE_KEY(key_map_toggle),
    CONFIG_VARIABLE_KEY(key_map_zoomin),
    CONFIG_VARIABLE_KEY(key_map_zoomout),
    CONFIG_VARIABLE_KEY(key_map_maxzoom),
    CONFIG_VARIABLE_KEY(key_map_follow),
    CONFIG_VARIABLE_KEY(key_map_rotate),
    CONFIG_VARIABLE_KEY(key_map_overlay),
    CONFIG_VARIABLE_KEY(key_map_grid),
    CONFIG_VARIABLE_KEY(key_map_mark),
    CONFIG_VARIABLE_KEY(key_map_clearmark),
    CONFIG_VARIABLE_KEY(key_map_north),
    CONFIG_VARIABLE_KEY(key_map_south),
    CONFIG_VARIABLE_KEY(key_map_east),
    CONFIG_VARIABLE_KEY(key_map_west),

    // Function keys
    CONFIG_VARIABLE_KEY(key_menu_help),
    CONFIG_VARIABLE_KEY(key_menu_save),
    CONFIG_VARIABLE_KEY(key_menu_load),
    CONFIG_VARIABLE_KEY(key_menu_volume),
    CONFIG_VARIABLE_KEY(key_menu_detail),
    CONFIG_VARIABLE_KEY(key_menu_qsave),
    CONFIG_VARIABLE_KEY(key_menu_endgame),
    CONFIG_VARIABLE_KEY(key_menu_messages),
    CONFIG_VARIABLE_KEY(key_menu_qload),
    CONFIG_VARIABLE_KEY(key_menu_quit),
    CONFIG_VARIABLE_KEY(key_menu_gamma),
    CONFIG_VARIABLE_KEY(key_spy),

    // Shortcut keys
    CONFIG_VARIABLE_KEY(key_pause),
    CONFIG_VARIABLE_KEY(key_menu_screenshot),
    CONFIG_VARIABLE_KEY(key_message_refresh),
    CONFIG_VARIABLE_KEY(key_demo_quit),

    // Multiplayer
    CONFIG_VARIABLE_KEY(key_multi_msg),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer1),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer2),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer3),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer4),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer5),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer6),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer7),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer8),
    CONFIG_VARIABLE_STRING(player_name),
    CONFIG_VARIABLE_STRING(chatmacro0),
    CONFIG_VARIABLE_STRING(chatmacro1),
    CONFIG_VARIABLE_STRING(chatmacro2),
    CONFIG_VARIABLE_STRING(chatmacro3),
    CONFIG_VARIABLE_STRING(chatmacro4),
    CONFIG_VARIABLE_STRING(chatmacro5),
    CONFIG_VARIABLE_STRING(chatmacro6),
    CONFIG_VARIABLE_STRING(chatmacro7),
    CONFIG_VARIABLE_STRING(chatmacro8),
    CONFIG_VARIABLE_STRING(chatmacro9),

    // Special menu keys, not available for rebinding
    CONFIG_VARIABLE_KEY(key_menu_activate),
    CONFIG_VARIABLE_KEY(key_menu_up),
    CONFIG_VARIABLE_KEY(key_menu_down),
    CONFIG_VARIABLE_KEY(key_menu_left),
    CONFIG_VARIABLE_KEY(key_menu_right),
    CONFIG_VARIABLE_KEY(key_menu_back),
    CONFIG_VARIABLE_KEY(key_menu_forward),
    CONFIG_VARIABLE_KEY(key_menu_confirm),
    CONFIG_VARIABLE_KEY(key_menu_abort),
    CONFIG_VARIABLE_KEY(key_menu_incscreen),
    CONFIG_VARIABLE_KEY(key_menu_decscreen),
    CONFIG_VARIABLE_KEY(key_menu_del),
    CONFIG_VARIABLE_INT(vanilla_keyboard_mapping),

    //
    // Mouse controls
    //

    CONFIG_VARIABLE_INT(mouse_enable),
    CONFIG_VARIABLE_INT(mouse_grab),
    CONFIG_VARIABLE_INT(mouse_novert),
    CONFIG_VARIABLE_INT(mouse_y_invert),
    CONFIG_VARIABLE_INT(mouse_dclick_use),
    CONFIG_VARIABLE_FLOAT(mouse_acceleration),
    CONFIG_VARIABLE_INT(mouse_threshold),
    CONFIG_VARIABLE_INT(mouse_sensitivity),
    CONFIG_VARIABLE_INT(mouse_look),
    CONFIG_VARIABLE_INT(mouseb_fire),
    CONFIG_VARIABLE_INT(mouseb_forward),
    CONFIG_VARIABLE_INT(mouseb_speed),
    CONFIG_VARIABLE_INT(mouseb_strafe),
    CONFIG_VARIABLE_INT(mouseb_use),
    CONFIG_VARIABLE_INT(mouseb_strafeleft),
    CONFIG_VARIABLE_INT(mouseb_straferight),
    CONFIG_VARIABLE_INT(mouseb_backward),
    CONFIG_VARIABLE_INT(mouseb_prevweapon),
    CONFIG_VARIABLE_INT(mouseb_nextweapon),
    CONFIG_VARIABLE_INT(mouseb_jump),

    // Heretic: Inventory
    CONFIG_VARIABLE_INT(mouseb_invleft),
    CONFIG_VARIABLE_INT(mouseb_invright),
    CONFIG_VARIABLE_INT(mouseb_useartifact),

    // Heretic: permanent "noartiskip" mode
    CONFIG_VARIABLE_INT(ctrl_noartiskip),

    //
    // Joystick controls
    //

    CONFIG_VARIABLE_INT(use_joystick),
    CONFIG_VARIABLE_INT(use_gamepad),
    CONFIG_VARIABLE_INT(gamepad_type),
    CONFIG_VARIABLE_STRING(joystick_guid),
    CONFIG_VARIABLE_INT(joystick_index),
    CONFIG_VARIABLE_INT(use_analog),
    CONFIG_VARIABLE_INT(joystick_x_axis),
    CONFIG_VARIABLE_INT(joystick_x_invert),
    CONFIG_VARIABLE_INT(joystick_turn_sensitivity),
    CONFIG_VARIABLE_INT(joystick_y_axis),
    CONFIG_VARIABLE_INT(joystick_y_invert),
    CONFIG_VARIABLE_INT(joystick_strafe_axis),
    CONFIG_VARIABLE_INT(joystick_strafe_invert),
    CONFIG_VARIABLE_INT(joystick_move_sensitivity),
    CONFIG_VARIABLE_INT(joystick_look_axis),
    CONFIG_VARIABLE_INT(joystick_look_invert),
    CONFIG_VARIABLE_INT(joystick_look_sensitivity),
    CONFIG_VARIABLE_INT(joystick_x_dead_zone),
    CONFIG_VARIABLE_INT(joystick_y_dead_zone),
    CONFIG_VARIABLE_INT(joystick_strafe_dead_zone),
    CONFIG_VARIABLE_INT(joystick_look_dead_zone),    
    CONFIG_VARIABLE_INT(joystick_physical_button0),
    CONFIG_VARIABLE_INT(joystick_physical_button1),
    CONFIG_VARIABLE_INT(joystick_physical_button2),
    CONFIG_VARIABLE_INT(joystick_physical_button3),
    CONFIG_VARIABLE_INT(joystick_physical_button4),
    CONFIG_VARIABLE_INT(joystick_physical_button5),
    CONFIG_VARIABLE_INT(joystick_physical_button6),
    CONFIG_VARIABLE_INT(joystick_physical_button7),
    CONFIG_VARIABLE_INT(joystick_physical_button8),
    CONFIG_VARIABLE_INT(joystick_physical_button9),
    CONFIG_VARIABLE_INT(joystick_physical_button10),
    CONFIG_VARIABLE_INT(joyb_fire),
    CONFIG_VARIABLE_INT(joyb_strafe),
    CONFIG_VARIABLE_INT(joyb_use),
    CONFIG_VARIABLE_INT(joyb_speed),
    CONFIG_VARIABLE_INT(joyb_jump),
    CONFIG_VARIABLE_INT(joyb_strafeleft),
    CONFIG_VARIABLE_INT(joyb_straferight),
    CONFIG_VARIABLE_INT(joyb_prevweapon),
    CONFIG_VARIABLE_INT(joyb_nextweapon),
    CONFIG_VARIABLE_INT(joyb_menu_activate),
    CONFIG_VARIABLE_INT(joyb_toggle_automap),

    // Widgets
    CONFIG_VARIABLE_INT(widget_enable),
    CONFIG_VARIABLE_INT(widget_location),
    CONFIG_VARIABLE_INT(widget_kis),
    CONFIG_VARIABLE_INT(widget_time),
    CONFIG_VARIABLE_INT(widget_totaltime),
    CONFIG_VARIABLE_INT(widget_levelname),
    CONFIG_VARIABLE_INT(widget_coords),
    CONFIG_VARIABLE_INT(widget_render),
    CONFIG_VARIABLE_INT(widget_health),

    // Automap
    CONFIG_VARIABLE_INT(automap_scheme),
    CONFIG_VARIABLE_INT(automap_smooth),
    CONFIG_VARIABLE_INT(automap_square),
    CONFIG_VARIABLE_INT(automap_secrets),
    CONFIG_VARIABLE_INT(automap_rotate),
    CONFIG_VARIABLE_INT(automap_overlay),
    CONFIG_VARIABLE_INT(automap_shading),

    //
    // Gameplay Features
    //

    // Visual
    CONFIG_VARIABLE_INT(vis_brightmaps),
    CONFIG_VARIABLE_INT(vis_translucency),
    CONFIG_VARIABLE_INT(vis_fake_contrast),
    CONFIG_VARIABLE_INT(vis_smooth_light),
    CONFIG_VARIABLE_INT(vis_smooth_palette),
    CONFIG_VARIABLE_INT(vis_improved_fuzz),
    CONFIG_VARIABLE_INT(vis_colored_blood),
    CONFIG_VARIABLE_INT(vis_swirling_liquids),
    CONFIG_VARIABLE_INT(vis_invul_sky),
    CONFIG_VARIABLE_INT(vis_linear_sky),
    CONFIG_VARIABLE_INT(vis_flip_corpses),

    // Crosshair
    CONFIG_VARIABLE_INT(xhair_draw),
    CONFIG_VARIABLE_INT(xhair_color),

    // Status Bar
    CONFIG_VARIABLE_INT(st_colored_stbar),
    CONFIG_VARIABLE_INT(st_negative_health),
    CONFIG_VARIABLE_INT(st_blinking_keys),
    CONFIG_VARIABLE_INT(st_ammo_widget),
    CONFIG_VARIABLE_INT(st_ammo_widget_translucent),
    CONFIG_VARIABLE_INT(st_ammo_widget_colors),
    CONFIG_VARIABLE_INT(st_weapon_widget),
    CONFIG_VARIABLE_INT(st_armor_icon),

    // Audible
    CONFIG_VARIABLE_INT(aud_z_axis_sfx),
    CONFIG_VARIABLE_INT(aud_full_sounds),
    CONFIG_VARIABLE_INT(aud_crushed_corpse),
    CONFIG_VARIABLE_INT(aud_exit_sounds),

    // Physical
    CONFIG_VARIABLE_INT(phys_torque),
    CONFIG_VARIABLE_INT(phys_ssg_tear_monsters),
    CONFIG_VARIABLE_INT(phys_toss_drop),
    CONFIG_VARIABLE_INT(phys_floating_powerups),
    CONFIG_VARIABLE_INT(phys_weapon_alignment),
    CONFIG_VARIABLE_INT(phys_breathing),

    // Gameplay
    CONFIG_VARIABLE_INT(gp_default_class),
    CONFIG_VARIABLE_INT(gp_default_skill),
    CONFIG_VARIABLE_INT(gp_revealed_secrets),
    CONFIG_VARIABLE_INT(gp_flip_levels),
    CONFIG_VARIABLE_INT(gp_death_use_action),

    // Demos
    CONFIG_VARIABLE_INT(demo_timer),
    CONFIG_VARIABLE_INT(demo_timerdir),
    CONFIG_VARIABLE_INT(demo_bar),
    CONFIG_VARIABLE_INT(demo_internal),

    // Compatibility-breaking
    CONFIG_VARIABLE_INT(compat_pistol_start),
    CONFIG_VARIABLE_INT(compat_blockmap_fix),
    CONFIG_VARIABLE_INT(compat_vertical_aiming),
};

static default_collection_t doom_defaults =
{
    doom_defaults_list,
    arrlen(doom_defaults_list),
    NULL,
};

// Search a collection for a variable

static default_t *SearchCollection(default_collection_t *collection, const char *name)
{
    int i;

    for (i=0; i<collection->numdefaults; ++i) 
    {
        if (!strcmp(name, collection->defaults[i].name))
        {
            return &collection->defaults[i];
        }
    }

    return NULL;
}

// Mapping from DOS keyboard scan code to internal key code (as defined
// in doomkey.h). I think I (fraggle) reused this from somewhere else
// but I can't find where. Anyway, notes:
//  * KEY_PAUSE is wrong - it's in the KEY_NUMLOCK spot. This shouldn't
//    matter in terms of Vanilla compatibility because neither of
//    those were valid for key bindings.
//  * There is no proper scan code for PrintScreen (on DOS machines it
//    sends an interrupt). So I added a fake scan code of 126 for it.
//    The presence of this is important so we can bind PrintScreen as
//    a screenshot key.
static const int scantokey[128] =
{
    0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
    '7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, 9,
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
    'o',    'p',    '[',    ']',    13,		KEY_RCTRL, 'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
    '\'',   '`',    KEY_RSHIFT,'\\',   'z',    'x',    'c',    'v',
    'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT,KEYP_MULTIPLY,
    KEY_RALT,  ' ',  KEY_CAPSLOCK,KEY_F1,  KEY_F2,   KEY_F3,   KEY_F4,   KEY_F5,
    KEY_F6,   KEY_F7,   KEY_F8,   KEY_F9,   KEY_F10,  /*KEY_NUMLOCK?*/KEY_PAUSE,KEY_SCRLCK,KEY_HOME,
    KEY_UPARROW,KEY_PGUP,KEY_MINUS,KEY_LEFTARROW,KEYP_5,KEY_RIGHTARROW,KEYP_PLUS,KEY_END,
    KEY_DOWNARROW,KEY_PGDN,KEY_INS,KEY_DEL,0,   0,      0,      KEY_F11,
    KEY_F12,  0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      KEY_PRTSCR, 0
};


static void SaveDefaultCollection(default_collection_t *collection)
{
    default_t *defaults;
    int i, v;
    FILE *f;
	
    f = M_fopen(collection->filename, "w");
    if (!f)
	return; // can't write the file, but don't complain

    defaults = collection->defaults;
		
    for (i=0 ; i<collection->numdefaults ; i++)
    {
        int chars_written;

        // Ignore unbound variables

        if (!defaults[i].bound)
        {
            continue;
        }

        // Print the name and line up all values at 30 characters

        chars_written = fprintf(f, "%s ", defaults[i].name);

        for (; chars_written < 30; ++chars_written)
            fprintf(f, " ");

        // Print the value

        switch (defaults[i].type) 
        {
            case DEFAULT_KEY:

                // use the untranslated version if we can, to reduce
                // the possibility of screwing up the user's config
                // file
                
                v = *defaults[i].location.i;

                if (v == KEY_RSHIFT)
                {
                    // Special case: for shift, force scan code for
                    // right shift, as this is what Vanilla uses.
                    // This overrides the change check below, to fix
                    // configuration files made by old versions that
                    // mistakenly used the scan code for left shift.

                    v = 54;
                }
                else if (defaults[i].untranslated
                      && v == defaults[i].original_translated)
                {
                    // Has not been changed since the last time we
                    // read the config file.

                    v = defaults[i].untranslated;
                }
                else
                {
                    // search for a reverse mapping back to a scancode
                    // in the scantokey table

                    int s;

                    for (s=0; s<128; ++s)
                    {
                        if (scantokey[s] == v)
                        {
                            v = s;
                            break;
                        }
                    }
                }

	        fprintf(f, "%i", v);
                break;

            case DEFAULT_INT:
	        fprintf(f, "%i", *defaults[i].location.i);
                break;

            case DEFAULT_INT_HEX:
	        fprintf(f, "0x%x", *defaults[i].location.i);
                break;

            case DEFAULT_FLOAT:
                fprintf(f, "%f", *defaults[i].location.f);
                break;

            case DEFAULT_STRING:
	        fprintf(f,"\"%s\"", *defaults[i].location.s);
                break;
        }

        fprintf(f, "\n");
    }

    fclose (f);
}

// Parses integer values in the configuration file

static int ParseIntParameter(const char *strparm)
{
    int parm;

    if (strparm[0] == '0' && strparm[1] == 'x')
        sscanf(strparm+2, "%x", (unsigned int *) &parm);
    else
        sscanf(strparm, "%i", &parm);

    return parm;
}

static void SetVariable(default_t *def, const char *value)
{
    int intparm;

    // parameter found

    switch (def->type)
    {
        case DEFAULT_STRING:
            *def->location.s = M_StringDuplicate(value);
            break;

        case DEFAULT_INT:
        case DEFAULT_INT_HEX:
            *def->location.i = ParseIntParameter(value);
            break;

        case DEFAULT_KEY:

            // translate scancodes read from config
            // file (save the old value in untranslated)

            intparm = ParseIntParameter(value);
            def->untranslated = intparm;
            if (intparm >= 0 && intparm < 128)
            {
                intparm = scantokey[intparm];
            }
            else
            {
                intparm = 0;
            }

            def->original_translated = intparm;
            *def->location.i = intparm;
            break;

        case DEFAULT_FLOAT:
        {
            // Different locales use different decimal separators.
            // However, the choice of the current locale isn't always
            // under our own control. If the atof() function fails to
            // parse the string representing the floating point number
            // using the current locale's decimal separator, it will
            // return 0, resulting in silent sound effects. To
            // mitigate this, we replace the first non-digit,
            // non-minus character in the string with the current
            // locale's decimal separator before passing it to atof().
            struct lconv *lc = localeconv();
            char dec, *str;
            int i = 0;

            dec = lc->decimal_point[0];
            str = M_StringDuplicate(value);

            // Skip sign indicators.
            if (str[i] == '-' || str[i] == '+')
            {
                i++;
            }

            for ( ; str[i] != '\0'; i++)
            {
                if (!isdigit(str[i]))
                {
                    str[i] = dec;
                    break;
                }
            }

            *def->location.f = (float) atof(str);
            free(str);
        }
            break;
    }
}

static void LoadDefaultCollection(default_collection_t *collection)
{
    FILE *f;
    default_t *def;
    char defname[80];
    char strparm[100];

    // read the file in, overriding any set defaults
    f = M_fopen(collection->filename, "r");

    if (f == NULL)
    {
        // File not opened, but don't complain. 
        // It's probably just the first time they ran the game.

        return;
    }

    while (!feof(f))
    {
        if (fscanf(f, "%79s %99[^\n]\n", defname, strparm) != 2)
        {
            // This line doesn't match

            continue;
        }

        // Find the setting in the list

        def = SearchCollection(collection, defname);

        if (def == NULL || !def->bound)
        {
            // Unknown variable?  Unbound variables are also treated
            // as unknown.

            continue;
        }

        // Strip off trailing non-printable characters (\r characters
        // from DOS text files)

        while (strlen(strparm) > 0 && !isprint(strparm[strlen(strparm)-1]))
        {
            strparm[strlen(strparm)-1] = '\0';
        }

        // Surrounded by quotes? If so, remove them.
        if (strlen(strparm) >= 2
         && strparm[0] == '"' && strparm[strlen(strparm) - 1] == '"')
        {
            strparm[strlen(strparm) - 1] = '\0';
            memmove(strparm, strparm + 1, sizeof(strparm) - 1);
        }

        SetVariable(def, strparm);
    }

    fclose (f);
}

// Set the default filenames to use for configuration files.

void M_SetConfigFilenames(const char *main_config)
{
    default_main_config = main_config;
}

//
// M_SaveDefaults
//

void M_SaveDefaults (void)
{
    SaveDefaultCollection(&doom_defaults);
}

//
// Save defaults to alternate filenames
//

void M_SaveDefaultsAlternate(const char *main)
{
    const char *orig_main;

    // Temporarily change the filenames

    orig_main = doom_defaults.filename;

    doom_defaults.filename = main;

    M_SaveDefaults();

    // Restore normal filenames

    doom_defaults.filename = orig_main;
}

//
// M_LoadDefaults
//

void M_LoadDefaults (void)
{
    int i;

    // This variable is a special snowflake for no good reason.
    M_BindStringVariable("autoload_path", &autoload_path);

    // check for a custom default file

    //!
    // @arg <file>
    // @vanilla
    //
    // Load main configuration from the specified file, instead of the
    // default.
    //

    i = M_CheckParmWithArgs("-config", 1);

    if (i)
    {
	doom_defaults.filename = myargv[i+1];
	printf ("	default file: %s\n",doom_defaults.filename);
    }
    else
    {
        doom_defaults.filename
            = M_StringJoin(configdir, default_main_config, NULL);
    }

    printf("  saving config in %s\n", doom_defaults.filename);

    LoadDefaultCollection(&doom_defaults);
}

// Get a configuration file variable by its name

static default_t *GetDefaultForName(const char *name)
{
    default_t *result;

    // Try the main list and the extras

    result = SearchCollection(&doom_defaults, name);

    // Not found? Internal error.

    if (result == NULL)
    {
        I_Error("Unknown configuration variable: '%s'", name);
    }

    return result;
}

//
// Bind a variable to a given configuration file variable, by name.
//

void M_BindIntVariable(const char *name, int *location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_INT
        || variable->type == DEFAULT_INT_HEX
        || variable->type == DEFAULT_KEY);

    variable->location.i = location;
    variable->bound = true;
}

void M_BindFloatVariable(const char *name, float *location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_FLOAT);

    variable->location.f = location;
    variable->bound = true;
}

void M_BindStringVariable(const char *name, char **location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_STRING);

    variable->location.s = location;
    variable->bound = true;
}

// Set the value of a particular variable; an API function for other
// parts of the program to assign values to config variables by name.

boolean M_SetVariable(const char *name, const char *value)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound)
    {
        return false;
    }

    SetVariable(variable, value);

    return true;
}

// Get the value of a variable.

int M_GetIntVariable(const char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || (variable->type != DEFAULT_INT && variable->type != DEFAULT_INT_HEX))
    {
        return 0;
    }

    return *variable->location.i;
}

const char *M_GetStringVariable(const char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || variable->type != DEFAULT_STRING)
    {
        return NULL;
    }

    return *variable->location.s;
}

float M_GetFloatVariable(const char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || variable->type != DEFAULT_FLOAT)
    {
        return 0;
    }

    return *variable->location.f;
}

// Get the path to the default configuration dir to use, if NULL
// is passed to M_SetConfigDir.

static char *GetDefaultConfigDir(void)
{
#if !defined(_WIN32) || defined(_WIN32_WCE)

    // Configuration settings are stored in an OS-appropriate path
    // determined by SDL.  On typical Unix systems, this might be
    // ~/.local/share/chocolate-doom.  On Windows, we behave like
    // Vanilla Doom and save in the current directory.

    char *result;
    char *copy;

    result = SDL_GetPrefPath("", PACKAGE_TARNAME);
    if (result != NULL)
    {
        copy = M_StringDuplicate(result);
        SDL_free(result);
        return copy;
    }
#endif /* #ifndef _WIN32 */
    return M_StringDuplicate(exedir);
}

// 
// SetConfigDir:
//
// Sets the location of the configuration directory, where configuration
// files are stored - default.cfg, chocolate-doom.cfg, savegames, etc.
//

void M_SetConfigDir(const char *dir)
{
    // Use the directory that was passed, or find the default.

    if (dir != NULL)
    {
        configdir = dir;
    }
    else
    {
        configdir = GetDefaultConfigDir();
    }

    if (strcmp(configdir, exedir) != 0)
    {
        printf("Using %s for configuration and saves\n", configdir);
    }

    // Make the directory if it doesn't already exist:

    M_MakeDirectory(configdir);
}

#define MUSIC_PACK_README \
"Extract music packs into this directory in .flac or .ogg format;\n"   \
"they will be automatically loaded based on filename to replace the\n" \
"in-game music with high quality versions.\n\n" \
"For more information check here:\n\n" \
"  <https://www.chocolate-doom.org/wiki/index.php/Digital_music_packs>\n\n"

// Set the value of music_pack_path if it is currently empty, and create
// the directory if necessary.
void M_SetMusicPackDir(void)
{
    const char *current_path;
    char *prefdir, *music_pack_path, *readme_path;

    current_path = M_GetStringVariable("music_pack_path");

    if (current_path != NULL && strlen(current_path) > 0)
    {
        return;
    }

#ifdef _WIN32
    // [JN] On Windows, create "music-packs" directory in program folder.
    prefdir = M_StringDuplicate(exedir);
#else
    // [JN] On other OSes use system home folder.    
    prefdir = SDL_GetPrefPath("", PACKAGE_TARNAME);
#endif
    if (prefdir == NULL)
    {
        printf("M_SetMusicPackDir: SDL_GetPrefPath failed, music pack directory not set\n");
        return;
    }
    music_pack_path = M_StringJoin(prefdir, "music-packs", NULL);

    M_MakeDirectory(prefdir);
    M_MakeDirectory(music_pack_path);
    M_SetVariable("music_pack_path", music_pack_path);

    // We write a README file with some basic instructions on how to use
    // the directory.
    readme_path = M_StringJoin(music_pack_path, DIR_SEPARATOR_S,
                               "README.txt", NULL);
    M_WriteFile(readme_path, MUSIC_PACK_README, strlen(MUSIC_PACK_README));

    free(readme_path);
    free(music_pack_path);
#ifdef _WIN32
    // [JN] Note: prefdir not gathered via SDL_GetPrefPath,
    // so just use system "free" function, not SDL_free.
    free(prefdir);
#else
    SDL_free(prefdir);
#endif
}

//
// Calculate the path to the directory to use to store save games.
// Creates the directory as necessary.
//

char *M_GetSaveGameDir(const char *iwadname)
{
    char *savegamedir;
    char *topdir;
    int p;

    //!
    // @arg <directory>
    //
    // Specify a path from which to load and save games. If the directory
    // does not exist then it will automatically be created.
    //

    p = M_CheckParmWithArgs("-savedir", 1);
    if (p)
    {
        savegamedir = myargv[p + 1];
        if (!M_FileExists(savegamedir))
        {
            M_MakeDirectory(savegamedir);
        }

        // add separator at end just in case
        savegamedir = M_StringJoin(savegamedir, DIR_SEPARATOR_S, NULL);

        printf("Save directory changed to %s\n", savegamedir);
    }
#ifdef _WIN32
    // In -cdrom mode, we write savegames to a specific directory
    // in addition to configs.

    else if (M_ParmExists("-cdrom"))
    {
        savegamedir = M_StringDuplicate(configdir);
    }
#endif
    // If not "doing" a configuration directory (Windows), don't "do"
    // a savegame directory, either.
    else if (!strcmp(configdir, exedir))
    {
#ifdef _WIN32
        // [JN] Check if "savegames_path" variable is existing in config file.
        const char *existing_path = M_GetStringVariable("savegames_path");

        if (existing_path != NULL && strlen(existing_path) > 0)
        {
            // Variable existing, use it's path.
            savegamedir = M_StringDuplicate(SavePathConfig);
        }
        else
        {
            // Config file variable not existing or emptry, generate a path.
            savegamedir = M_StringJoin(M_StringDuplicate(exedir), "savegames", NULL);
        }
        M_MakeDirectory(savegamedir);
#else
	savegamedir = M_StringDuplicate("");
#endif
    }
    else
    {
        // ~/.local/share/chocolate-doom/savegames

        topdir = M_StringJoin(configdir, "savegames", NULL);
        M_MakeDirectory(topdir);

        // eg. ~/.local/share/chocolate-doom/savegames/doom2.wad/

        savegamedir = M_StringJoin(topdir, DIR_SEPARATOR_S, iwadname,
                                   DIR_SEPARATOR_S, NULL);

        M_MakeDirectory(savegamedir);

        free(topdir);
    }

    // Overwrite config file variable only if following command line
    // parameters are not present:
    if (!M_ParmExists("-savedir") && !M_ParmExists("-cdrom"))
    {
        SavePathConfig = savegamedir;
        // add separator at end just in case
        savegamedir = M_StringJoin(savegamedir, DIR_SEPARATOR_S, NULL);
    }

    return savegamedir;
}

//
// Calculate the path to the directory for autoloaded WADs/DEHs.
// Creates the directory as necessary.
//
char *M_GetAutoloadDir(const char *iwadname)
{
    char *result;

    if (autoload_path == NULL || strlen(autoload_path) == 0)
    {
        char *prefdir;

#ifdef _WIN32
        // [JN] On Windows, create "autoload" directory in program folder.
        prefdir = M_StringDuplicate(exedir);
#else
        // [JN] On other OSes use system home folder.
        prefdir = SDL_GetPrefPath("", PACKAGE_TARNAME);
#endif

        if (prefdir == NULL)
        {
            printf("M_GetAutoloadDir: SDL_GetPrefPath failed\n");
            return NULL;
        }
        autoload_path = M_StringJoin(prefdir, "autoload", NULL);
#ifdef _WIN32
        // [JN] Note: prefdir not gathered via SDL_GetPrefPath,
        // so just use system "free" function, not SDL_free.
        free(prefdir);
#else
        SDL_free(prefdir);
#endif
    }

    M_MakeDirectory(autoload_path);

    result = M_StringJoin(autoload_path, DIR_SEPARATOR_S, iwadname, NULL);
    M_MakeDirectory(result);

    // TODO: Add README file

    return result;
}

void M_SetScreenshotDir (void)
{
    int p = M_CheckParmWithArgs("-shotdir", 1);

    if (p)
    {
        screenshotdir = myargv[p + 1];

        if (!M_FileExists(screenshotdir))
        {
            M_MakeDirectory(screenshotdir);
        }
        
        screenshotdir = M_StringJoin(screenshotdir, DIR_SEPARATOR_S, NULL);

        printf("Screenshot directory changed to %s\n", screenshotdir);
    }
#ifdef _WIN32
    // In -cdrom mode, we write screenshots to a specific directory
    // in addition to configs.

    else if (M_ParmExists("-cdrom"))
    {
        screenshotdir = M_StringDuplicate(configdir);
    }
#endif
    else
    {
        // [JN] Check if "screenshots_path" variable is existing in config file.
        const char *existing_path = M_GetStringVariable("screenshots_path");

        if (existing_path != NULL && strlen(existing_path) > 0)
        {
            // Variable existing, use it's path.
            screenshotdir = M_StringDuplicate(ShotPathConfig);
        }
        else
        {
#ifdef _WIN32
            // [JN] Always use "screenshots" folder in program folder.
            screenshotdir = M_StringJoin(exedir, "screenshots", NULL);
#else
            // ~/.local/share/inter-doom/screenshots
            screenshotdir = M_StringJoin(configdir, "screenshots", NULL);
#endif
        }
        M_MakeDirectory(screenshotdir);
        ShotPathConfig = screenshotdir;
        // add separator at end just in case
        screenshotdir = M_StringJoin(screenshotdir, DIR_SEPARATOR_S, NULL);
    }
}
