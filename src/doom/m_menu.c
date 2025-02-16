//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "deh_main.h"
#include "gusconf.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_controls.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_local.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_controls.h"
#include "s_sound.h"
#include "doomstat.h"
#include "sounds.h"
#include "m_menu.h"
#include "p_local.h"
#include "ct_chat.h"
#include "v_diskicon.h"
#include "v_trans.h"
#include "am_map.h"
#include "st_bar.h"

#include "id_vars.h"
#include "id_func.h"


#define SKULLXOFF		-32
#define LINEHEIGHT       16


boolean menuactive;

// -1 = no quicksave slot picked!
static int quickSaveSlot;

 // 1 = message to be printed
static int messageToPrint;
// ...and here is the message string!
static const char *messageString;
// [JN] Certain messages needs background filling.
static boolean messageFillBG;

// message x & y
static int messageLastMenuActive;

// timed message = no input from user
static boolean messageNeedsInput;

static void (*messageRoutine)(int response);

static char *gammalvls[MAXGAMMA][2] =
{
    { GAMMALVL_N050,  "-0.50" },
    { GAMMALVL_N055,  "-0.55" },
    { GAMMALVL_N060,  "-0.60" },
    { GAMMALVL_N065,  "-0.65" },
    { GAMMALVL_N070,  "-0.70" },
    { GAMMALVL_N075,  "-0.75" },
    { GAMMALVL_N080,  "-0.80" },
    { GAMMALVL_N085,  "-0.85" },
    { GAMMALVL_N090,  "-0.90" },
    { GAMMALVL_N095,  "-0.95" },
    { GAMMALVL_OFF,   "OFF"   },
    { GAMMALVL_010,   "0.1"   },
    { GAMMALVL_020,   "0.2"   },
    { GAMMALVL_030,   "0.3"   },
    { GAMMALVL_040,   "0.4"   },
    { GAMMALVL_050,   "0.5"   },
    { GAMMALVL_060,   "0.6"   },
    { GAMMALVL_070,   "0.7"   },
    { GAMMALVL_080,   "0.8"   },
    { GAMMALVL_090,   "0.9"   },
    { GAMMALVL_100,   "1.0"   },
    { GAMMALVL_110,   "1.1"   },
    { GAMMALVL_120,   "1.2"   },
    { GAMMALVL_130,   "1.3"   },
    { GAMMALVL_140,   "1.4"   },
    { GAMMALVL_150,   "1.5"   },
    { GAMMALVL_160,   "1.6"   },
    { GAMMALVL_170,   "1.7"   },
    { GAMMALVL_180,   "1.8"   },
    { GAMMALVL_190,   "1.9"   },
    { GAMMALVL_200,   "2.0"   },
    { GAMMALVL_220,   "2.2"   },
    { GAMMALVL_240,   "2.4"   },
    { GAMMALVL_260,   "2.6"   },
    { GAMMALVL_280,   "2.8"   },
    { GAMMALVL_300,   "3.0"   },
    { GAMMALVL_320,   "3.2"   },
    { GAMMALVL_340,   "3.4"   },
    { GAMMALVL_360,   "3.6"   },
    { GAMMALVL_380,   "3.8"   },
    { GAMMALVL_400,   "4.0"   },
};

// we are going to be entering a savegame string
static int saveStringEnter;              
static int saveSlot;	   // which slot to save in
static int saveCharIndex;  // which char we're editing
static boolean joypadSave = false; // was the save action initiated by joypad?

// old save description before edit
static char saveOldString[SAVESTRINGSIZE];  

// [FG] support up to 8 pages of savegames
int savepage = 0;
static const int savepage_max = 7;

static char savegamestrings[10][SAVESTRINGSIZE];
static char endstring[160];


//
// MENU TYPEDEFS
//

typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;

    // [JN] Menu item timer for glowing effect.
    short   tics;
    
    char	name[32];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			
} menuitem_t;

typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)(void);	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
    boolean		smallFont;	// [JN] Menu is using small font
    boolean		ScrollAR;	// [JN] Menu can be scrolled by arrow keys
    boolean		ScrollPG;	// [JN] Menu can be scrolled by PGUP/PGDN keys
} menu_t;

// [JN] For currentMenu->menuitems[itemOn].statuses:
#define STS_SKIP -1
#define STS_NONE  0
#define STS_SWTC  1
#define STS_MUL1  2
#define STS_MUL2  3
#define STS_SLDR  4

// [JN] Macro definitions for first two items of menuitem_t.
// Trailing zero initializes "tics" field.
#define M_SKIP    STS_SKIP,0  // Skippable, cursor can't get here.
#define M_SWTC    STS_SWTC,0  // On/off type or entering function.
#define M_MUL1    STS_MUL1,0  // Multichoice function: increase by wheel up, decrease by wheel down
#define M_MUL2    STS_MUL2,0  // Multichoice function: decrease by wheel up, increase by wheel down
#define M_SLDR    STS_SLDR,0  // Slider line.

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

static short itemOn;            // menu item skull is on
static short skullAnimCounter;  // skull animation counter
static short whichSkull;        // which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
static char *skullName[2] = {"M_SKULL1","M_SKULL2"};

// current menudef
static menu_t *currentMenu;

// =============================================================================
// PROTOTYPES
// =============================================================================

static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_Choose_ID_Main (int choice);
static menu_t ID_Def_Main;

static void M_ChangeMessages(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
static void M_ChangeDetail(int choice);
static void M_SizeDisplay(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(int x,int y);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x,int y,int thermWidth,int thermDot,int itemPos);
static int  M_StringHeight(const char *string);
static void M_StartMessage(const char *string, void (*routine)(int), boolean input);
static void M_ClearMenus (void);

static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max);

// =============================================================================
// DOOM MENU
// =============================================================================

enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

static menuitem_t MainMenu[]=
{
    { M_SWTC, "M_NGAME",   M_NewGame,        'n' },
    { M_SWTC, "M_OPTION",  M_Choose_ID_Main, 'o' },
    { M_SWTC, "M_LOADG",   M_LoadGame,       'l' },
    { M_SWTC, "M_SAVEG",   M_SaveGame,       's' },
    // Another hickup with Special edition.
    { M_SWTC, "M_RDTHIS",  M_ReadThis,       'r' },
    { M_SWTC, "M_QUITG",   M_QuitDOOM,       'q' }
};

static menu_t MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,64,
    0,
    false, false, false,
};

//
// EPISODE SELECT
//

enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep5, // [crispy] Sigil
    ep6, // [crispy] Sigil II
    ep_end
} episodes_e;

static menuitem_t EpisodeMenu[]=
{
    { M_SWTC, "M_EPI1",  M_Episode,  'k' },
    { M_SWTC, "M_EPI2",  M_Episode,  't' },
    { M_SWTC, "M_EPI3",  M_Episode,  'i' },
    { M_SWTC, "M_EPI4",  M_Episode,  't' },
    { M_SWTC, "M_EPI5",  M_Episode,  's' }, // [crispy] Sigil
    { M_SWTC, "M_EPI6",  M_Episode,  's' }, // [crispy] Sigil II
};

// [crispy] have Sigil II but not Sigil
static menuitem_t EpisodeMenuSII[]=
{
    { M_SWTC, "M_EPI1", M_Episode, 'k' },
    { M_SWTC, "M_EPI2", M_Episode, 't' },
    { M_SWTC, "M_EPI3", M_Episode, 'i' },
    { M_SWTC, "M_EPI4", M_Episode, 't' }, 
    { M_SWTC, "M_EPI6", M_Episode, 's' },  // [crispy] Sigil II
};

static menu_t EpiDef =
{
    ep_end,         // # of menu items
    &MainDef,       // previous menu
    EpisodeMenu,    // menuitem_t ->
    M_DrawEpisode,  // drawing routine ->
    48,63,          // x,y
    ep1,            // lastOn
    false, false, false,
};

//
// NEW GAME
//

enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

static menuitem_t NewGameMenu[]=
{
    { M_SWTC, "M_JKILL",  M_ChooseSkill,  'i' },
    { M_SWTC, "M_ROUGH",  M_ChooseSkill,  'h' },
    { M_SWTC, "M_HURT",   M_ChooseSkill,  'h' },
    { M_SWTC, "M_ULTRA",  M_ChooseSkill,  'u' },
    { M_SWTC, "M_NMARE",  M_ChooseSkill,  'n' }
};

static menu_t NewDef =
{
    newg_end,       // # of menu items
    &EpiDef,        // previous menu
    NewGameMenu,    // menuitem_t ->
    M_DrawNewGame,  // drawing routine ->
    48,63,          // x,y
    hurtme,         // lastOn
    false, false, false,
};

//
// Read This! MENU 1 & 2
//

enum
{
    rdthsempty1,
    read1_end
} read_e;

static menuitem_t ReadMenu1[] =
{
    { M_SWTC, "",  M_ReadThis2,  0 }
};

static menu_t ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0,
    false, false, false,
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

static menuitem_t ReadMenu2[]=
{
    { M_SWTC, "",  M_FinishReadThis,  0 }
};

static menu_t ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0,
    false, false, false,
};

//
// SOUND VOLUME MENU
//

enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

static menuitem_t SoundMenu[]=
{
    { M_SLDR, "M_SFXVOL",  M_SfxVol,    's'  },
    { M_SKIP, "",          0,           '\0' },
    { M_SLDR, "M_MUSVOL",  M_MusicVol,  'm'  },
    { M_SKIP, "",          0,           '\0' }
};

static menu_t SoundDef =
{
    sound_end,
    &ID_Def_Main,
    SoundMenu,
    M_DrawSound,
    80,64,
    0,
    false, false, false,
};

//
// LOAD GAME MENU
//

enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load7,
    load8,
    load_end
} load_e;

static menuitem_t LoadMenu[]=
{
    { M_SWTC, "",  M_LoadSelect,  '1' },
    { M_SWTC, "",  M_LoadSelect,  '2' },
    { M_SWTC, "",  M_LoadSelect,  '3' },
    { M_SWTC, "",  M_LoadSelect,  '4' },
    { M_SWTC, "",  M_LoadSelect,  '5' },
    { M_SWTC, "",  M_LoadSelect,  '6' },
    { M_SWTC, "",  M_LoadSelect,  '7' },
    { M_SWTC, "",  M_LoadSelect,  '8' }
};

static menu_t LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    67,27,
    0,
    false, true, true,
};

//
// SAVE GAME MENU
//

static menuitem_t SaveMenu[]=
{
    { M_SWTC, "",  M_SaveSelect,  '1' },
    { M_SWTC, "",  M_SaveSelect,  '2' },
    { M_SWTC, "",  M_SaveSelect,  '3' },
    { M_SWTC, "",  M_SaveSelect,  '4' },
    { M_SWTC, "",  M_SaveSelect,  '5' },
    { M_SWTC, "",  M_SaveSelect,  '6' },
    { M_SWTC, "",  M_SaveSelect,  '7' },
    { M_SWTC, "",  M_SaveSelect,  '8' }
};

static menu_t SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    67,27,
    0,
    false, true, true,
};

// =============================================================================
// [JN] Custom ID menu
// =============================================================================

#define ID_MENU_TOPOFFSET         (18)
#define ID_MENU_LEFTOFFSET        (48)
#define ID_MENU_LEFTOFFSET_SML    (93)
#define ID_MENU_LEFTOFFSET_MID    (64)
#define ID_MENU_LEFTOFFSET_BIG    (32)
#define ID_MENU_LEFTOFFSET_LEVEL  (74)

#define ID_MENU_LINEHEIGHT_SMALL  (9)
#define ID_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return ORIGWIDTH - currentMenu->x - M_StringWidth(text);
}

static player_t *player;

static void M_Draw_ID_Main (void);

static void M_Choose_ID_Video (int choice);
static void M_Draw_ID_Video (void);
static void M_ID_TrueColor (int choice);
static void M_ID_RenderingRes (int choice);
static void M_ID_Widescreen (int choice);
static void M_ID_ExclusiveFS (int choice);

static void M_ID_UncappedFPS (int choice);
static void M_ID_LimitFPS (int choice);
static void M_ID_VSync (int choice);
static void M_ID_ShowFPS (int choice);
static void M_ID_PixelScaling (int choice);
static void M_ID_ScreenWipe (int choice);
static void M_ID_DiskIcon (int choice);
static void M_ID_ShowENDOOM (int choice);

static void M_Choose_ID_Display (int choice);
static void M_Draw_ID_Display (void);
static void M_ID_FOV (int choice);
static void M_ID_MenuShading (int choice);
static void M_ID_LevelBrightness (int choice);
static void M_ID_Gamma (int choice);
static void M_ID_Saturation (int choice);
static void M_ID_Contrast (int choice);
static void M_ID_R_Intensity (int choice);
static void M_ID_G_Intensity (int choice);
static void M_ID_B_Intensity (int choice);
static void M_ID_MessagesAlignment (int choice);
static void M_ID_TextShadows (int choice);
static void M_ID_LocalTime (int choice);

static void M_Choose_ID_Sound (int choice);
static void M_Draw_ID_Sound (void);
static void M_ID_SFXSystem (int choice);
static void M_ID_MusicSystem (int choice);
static void M_ID_SFXMode (int choice);
static void M_ID_SFXChannels (int choice);
static void M_ID_MuteInactive (int choice);
static void M_ID_PitchShift (int choice);
static void M_ID_RemasterOST (int choice);

static void M_Choose_ID_Controls (int choice);
static void M_Draw_ID_Controls (void);
static void M_ID_Controls_Sensivity (int choice);
static void M_ID_Controls_Acceleration (int choice);
static void M_ID_Controls_Threshold (int choice);
static void M_ID_Controls_MLook (int choice);
static void M_ID_Controls_NoVert (int choice);
static void M_ID_Controls_DblClck (int choice);
static void M_ID_Controls_InvertY (int choice);

static void M_Draw_ID_Keybinds_1 (void);
static void M_Bind_MoveForward (int choice);
static void M_Bind_MoveBackward (int choice);
static void M_Bind_TurnLeft (int choice);
static void M_Bind_TurnRight (int choice);
static void M_Bind_StrafeLeft (int choice);
static void M_Bind_StrafeRight (int choice);
static void M_Bind_SpeedOn (int choice);
static void M_Bind_StrafeOn (int choice);
static void M_Bind_180Turn (int choice);
static void M_Bind_FireAttack (int choice);
static void M_Bind_Use (int choice);

static void M_Draw_ID_Keybinds_2 (void);
static void M_Bind_AlwaysRun (int choice);
static void M_Bind_MouseLook (int choice);
static void M_Bind_NoVert (int choice);
static void M_Bind_RestartLevel (int choice);
static void M_Bind_NextLevel (int choice);
static void M_Bind_FastForward (int choice);
static void M_Bind_FlipLevels (int choice);
static void M_Bind_ExtendedHUD (int choice);
static void M_Bind_SpectatorMode (int choice);
static void M_Bind_FreezeMode (int choice);
static void M_Bind_NotargetMode (int choice);
static void M_Bind_BuddhaMode (int choice);

static void M_Draw_ID_Keybinds_3 (void);
static void M_Bind_Weapon1 (int choice);
static void M_Bind_Weapon2 (int choice);
static void M_Bind_Weapon3 (int choice);
static void M_Bind_Weapon4 (int choice);
static void M_Bind_Weapon5 (int choice);
static void M_Bind_Weapon6 (int choice);
static void M_Bind_Weapon7 (int choice);
static void M_Bind_Weapon8 (int choice);
static void M_Bind_PrevWeapon (int choice);
static void M_Bind_NextWeapon (int choice);

static void M_Draw_ID_Keybinds_4 (void);
static void M_Bind_ToggleMap (int choice);
static void M_Bind_ZoomIn (int choice);
static void M_Bind_ZoomOut (int choice);
static void M_Bind_MaxZoom (int choice);
static void M_Bind_FollowMode (int choice);
static void M_Bind_RotateMode (int choice);
static void M_Bind_OverlayMode (int choice);
static void M_Bind_ToggleGrid (int choice);
static void M_Bind_AddMark (int choice);
static void M_Bind_ClearMarks (int choice);

static void M_Draw_ID_Keybinds_5 (void);
static void M_Bind_HelpScreen (int choice);
static void M_Bind_SaveGame (int choice);
static void M_Bind_LoadGame (int choice);
static void M_Bind_SoundVolume (int choice);
static void M_Bind_ToggleDetail (int choice);
static void M_Bind_QuickSave (int choice);
static void M_Bind_EndGame (int choice);
static void M_Bind_ToggleMessages (int choice);
static void M_Bind_QuickLoad (int choice);
static void M_Bind_QuitGame (int choice);
static void M_Bind_ToggleGamma (int choice);
static void M_Bind_MultiplayerSpy (int choice);

static void M_Draw_ID_Keybinds_6 (void);
static void M_Bind_Pause (int choice);
static void M_Bind_SaveScreenshot (int choice);
static void M_Bind_LastMessage (int choice);
static void M_Bind_FinishDemo (int choice);
static void M_Bind_SendMessage (int choice);
static void M_Bind_ToPlayer1 (int choice);
static void M_Bind_ToPlayer2 (int choice);
static void M_Bind_ToPlayer3 (int choice);
static void M_Bind_ToPlayer4 (int choice);
static void M_Bind_Reset (int choice);

static void M_Choose_ID_MouseBinds (int choice);
static void M_Draw_ID_MouseBinds (void);
static void M_Bind_M_FireAttack (int choice);
static void M_Bind_M_MoveForward (int choice);
static void M_Bind_M_MoveBackward (int choice);
static void M_Bind_M_Use (int choice);
static void M_Bind_M_SpeedOn (int choice);
static void M_Bind_M_StrafeOn (int choice);
static void M_Bind_M_StrafeLeft (int choice);
static void M_Bind_M_StrafeRight (int choice);
static void M_Bind_M_PrevWeapon (int choice);
static void M_Bind_M_NextWeapon (int choice);
static void M_Bind_M_Reset (int choice);

static void M_Choose_ID_Widgets (int choice);
static void M_Draw_ID_Widgets (void);
static void M_ID_Widget_Colors (int choice);
static void M_ID_Widget_Placement (int choice);
static void M_ID_Widget_Alignment (int choice);
static void M_ID_Widget_KIS (int choice);
static void M_ID_Widget_KIS_Format (int choice);
static void M_ID_Widget_Time (int choice);
static void M_ID_Widget_TotalTime (int choice);
static void M_ID_Widget_LevelName (int choice);
static void M_ID_Widget_Coords (int choice);
static void M_ID_Widget_Render (int choice);
static void M_ID_Widget_Health (int choice);

static void M_Choose_ID_Automap (int choice);
static void M_Draw_ID_Automap (void);
static void M_ID_Automap_Colors (int choice);
static void M_ID_Automap_Smooth (int choice);
static void M_ID_Automap_Thick (int choice);
static void M_ID_Automap_Square (int choice);
static void M_ID_Automap_Secrets (int choice);
static void M_ID_Automap_Rotate (int choice);
static void M_ID_Automap_Overlay (int choice);
static void M_ID_Automap_Shading (int choice);

static void M_Draw_ID_Gameplay_1 (void);
static void M_ID_Brightmaps (int choice);
static void M_ID_Translucency (int choice);
static void M_ID_FakeContrast (int choice);
static void M_ID_SmoothLighting (int choice);
static void M_ID_SmoothPalette (int choice);
static void M_ID_ImprovedFuzz (int choice);
static void M_ID_ColoredBlood (int choice);
static void M_ID_SwirlingLiquids (int choice);
static void M_ID_InvulSky (int choice);
static void M_ID_LinearSky (int choice);
static void M_ID_FlipCorpses (int choice);
static void M_ID_Crosshair (int choice);
static void M_ID_CrosshairColor (int choice);

static void M_Draw_ID_Gameplay_2 (void);
static void M_ID_ColoredSTBar (int choice);
static void M_ID_NegativeHealth (int choice);
static void M_ID_BlinkingKeys (int choice);
static void M_ID_ZAxisSfx (int choice);
static void M_ID_FullSounds (int choice);
static void M_ID_SndOfCrushedCorpse (int choice);
static void M_ID_Torque (int choice);
static void M_ID_SSGTearMonsters (int choice);
static void M_ID_TossDrop (int choice);
static void M_ID_FloatingPowerups (int choice);
static void M_ID_WeaponAlignment (int choice);
static void M_ID_Breathing (int choice);

static void M_Draw_ID_Gameplay_3 (void);
static void M_ID_DefaulSkill (int choice);
static void M_ID_PistolStart (int choice);
static void M_ID_RevealedSecrets (int choice);
static void M_ID_FlipLevels (int choice);
static void M_ID_OnDeathAction (int choice);
static void M_ID_DemoTimer (int choice);
static void M_ID_TimerDirection (int choice);
static void M_ID_ProgressBar (int choice);
static void M_ID_InternalDemos (int choice);
static void M_ID_BlockmapFix (int choice);
static void M_ID_VerticalAiming (int choice);

static void M_ScrollGameplay (int choice);
static void M_DrawGameplayFooter (char *pagenum);

static void M_Choose_ID_Misc (int choice);
static void M_Draw_ID_Misc (void);
static void M_ID_Misc_A11yInvul (int choice);
static void M_ID_Misc_A11yPalFlash (int choice);
static void M_ID_Misc_A11yMoveBob (int choice);
static void M_ID_Misc_A11yWeaponBob (int choice);
static void M_ID_Misc_A11yColorblind (int choice);
static void M_ID_Misc_AutoloadWAD (int choice);
static void M_ID_Misc_AutoloadDEH (int choice);
static void M_ID_Misc_Hightlight (int choice);
static void M_ID_Misc_MenuEscKey (int choice);

static void M_Choose_ID_Level (int choice);
static void M_Draw_ID_Level_1 (void);
static void M_ID_LevelSkill (int choice);
static void M_ID_LevelEpisode (int choice);
static void M_ID_LevelMap (int choice);
static void M_ID_LevelHealth (int choice);
static void M_ID_LevelArmor (int choice);
static void M_ID_LevelArmorType (int choice);
static void M_ID_LevelChainsaw (int choice);
static void M_ID_LevelShotgun (int choice);
static void M_ID_LevelSSG (int choice);
static void M_ID_LevelChaingun (int choice);
static void M_ID_LevelRLauncher (int choice);
static void M_ID_LevelPlasmagun (int choice);
static void M_ID_LevelBFG9000 (int choice);

static void M_Draw_ID_Level_2 (void);
static void M_ID_LevelBackpack (int choice);
static void M_ID_LevelBullets (int choice);
static void M_ID_LevelShells (int choice);
static void M_ID_LevelRockets (int choice);
static void M_ID_LevelCells (int choice);
static void M_ID_LevelBlueKeycard (int choice);
static void M_ID_LevelYellowKeycard (int choice);
static void M_ID_LevelRedKeycard (int choice);
static void M_ID_LevelBlueSkull (int choice);
static void M_ID_LevelYellowSkull (int choice);
static void M_ID_LevelRedSkull (int choice);
static void M_ID_LevelFastMonsters (int choice);
static void M_ID_LevelRespMonsters (int choice);

static void M_ScrollLevel (int choice);

static void M_Choose_ID_Reset (int choice);

// Keyboard binding prototypes
static boolean KbdIsBinding;
static int     keyToBind;

static char   *M_NameBind (int itemSetOn, int key);
static void    M_StartBind (int keynum);
static void    M_CheckBind (int key);
static void    M_DoBind (int keynum, int key);
static void    M_ClearBind (int itemOn);
static byte   *M_ColorizeBind (int itemSetOn, int key);
static void    M_ResetBinds (void);
static void    M_DrawBindKey (int itemNum, int yPos, int key);
static void    M_DrawBindFooter (char *pagenum, boolean drawPages);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static char   *M_NameMouseBind (int itemSetOn, int btn);
static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int itemOn);
static byte   *M_ColorizeMouseBind (int itemSetOn, int btn);
static void    M_DrawBindButton (int itemNum, int yPos, int btn);
static void    M_ResetMouseBinds (void);

// Forward declarations for scrolling and remembering last pages.
static menu_t ID_Def_Keybinds_1;
static menu_t ID_Def_Keybinds_2;
static menu_t ID_Def_Keybinds_3;
static menu_t ID_Def_Keybinds_4;
static menu_t ID_Def_Keybinds_5;
static menu_t ID_Def_Keybinds_6;
static menu_t ID_Def_Gameplay_1;
static menu_t ID_Def_Gameplay_2;
static menu_t ID_Def_Gameplay_3;
static menu_t ID_Def_Level_1;
static menu_t ID_Def_Level_2;

// Remember last keybindings page.
static int Keybinds_Cur;

static menu_t *KeybindsMenus[] =
{
    &ID_Def_Keybinds_1,
    &ID_Def_Keybinds_2,
    &ID_Def_Keybinds_3,
    &ID_Def_Keybinds_4,
    &ID_Def_Keybinds_5,
    &ID_Def_Keybinds_6,
};

static void M_Choose_ID_Keybinds (int choice)
{
    M_SetupNextMenu(KeybindsMenus[Keybinds_Cur]);
}

// Remember last gameplay page.
static int Gameplay_Cur;

static menu_t *GameplayMenus[] =
{
    &ID_Def_Gameplay_1,
    &ID_Def_Gameplay_2,
    &ID_Def_Gameplay_3,
};

static void M_Choose_ID_Gameplay (int choice)
{
    M_SetupNextMenu(GameplayMenus[Gameplay_Cur]);
}

// Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // Remember cursor position.
    currentMenu->lastOn = itemOn;

    // Save/Load menu:
    if (currentMenu == &LoadDef || currentMenu == &SaveDef)
    {
        if (direction)
        {
            if (savepage < savepage_max)
            {
                savepage++;
                S_StartSound(NULL, sfx_pstop);
            }
        }
        else
        {
            if (savepage > 0)
            {
                savepage--;
                S_StartSound(NULL, sfx_pstop);
            }
        }
        quickSaveSlot = -1;
        M_ReadSaveStrings();
        return;
    }

    // Keyboard bindings:
    else if (currentMenu == &ID_Def_Keybinds_1) M_SetupNextMenu(direction ? &ID_Def_Keybinds_2 : &ID_Def_Keybinds_6);
    else if (currentMenu == &ID_Def_Keybinds_2) M_SetupNextMenu(direction ? &ID_Def_Keybinds_3 : &ID_Def_Keybinds_1);
    else if (currentMenu == &ID_Def_Keybinds_3) M_SetupNextMenu(direction ? &ID_Def_Keybinds_4 : &ID_Def_Keybinds_2);
    else if (currentMenu == &ID_Def_Keybinds_4) M_SetupNextMenu(direction ? &ID_Def_Keybinds_5 : &ID_Def_Keybinds_3);
    else if (currentMenu == &ID_Def_Keybinds_5) M_SetupNextMenu(direction ? &ID_Def_Keybinds_6 : &ID_Def_Keybinds_4);
    else if (currentMenu == &ID_Def_Keybinds_6) M_SetupNextMenu(direction ? &ID_Def_Keybinds_1 : &ID_Def_Keybinds_5);

    // Gameplay features:
    else if (currentMenu == &ID_Def_Gameplay_1) M_SetupNextMenu(direction ? &ID_Def_Gameplay_2 : &ID_Def_Gameplay_3);
    else if (currentMenu == &ID_Def_Gameplay_2) M_SetupNextMenu(direction ? &ID_Def_Gameplay_3 : &ID_Def_Gameplay_1);
    else if (currentMenu == &ID_Def_Gameplay_3) M_SetupNextMenu(direction ? &ID_Def_Gameplay_1 : &ID_Def_Gameplay_2);

    // Level select:
    else if (currentMenu == &ID_Def_Level_1) M_SetupNextMenu(&ID_Def_Level_2);
    else if (currentMenu == &ID_Def_Level_2) M_SetupNextMenu(&ID_Def_Level_1);

    // Play sound.
    S_StartSound(NULL, sfx_pstop);
}

// -----------------------------------------------------------------------------

// [JN] Delay before shading.
static int shade_wait;

// [JN] Shade background while in active menu.
static void M_ShadeBackground (void)
{
    if (dp_menu_shading)
    {
        pixel_t *dest = I_VideoBuffer;
        const int shade = dp_menu_shading;
        const int scr = SCREENWIDTH * SCREENHEIGHT;
        
        for (int i = 0; i < scr; i++)
        {
#ifndef CRISPY_TRUECOLOR
            *dest = colormaps[((dp_menu_shading + 3) * 2) * 256 + I_VideoBuffer[y]];
#else
            *dest = I_BlendDark(*dest, I_ShadeFactor[shade]);
#endif
            ++dest;
        }
    }
}

static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("FLOOR4_8", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
}

static byte *M_Small_Line_Glow (int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT5] :
        tics == 4 ? cr[CR_MENU_BRIGHT4] :
        tics == 3 ? cr[CR_MENU_BRIGHT3] :
        tics == 2 ? cr[CR_MENU_BRIGHT2] :
        tics == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}

static byte *M_Big_Line_Glow (int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT3] :
        tics >= 3 ? cr[CR_MENU_BRIGHT2] :
        tics >= 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}

static void M_Reset_Line_Glow (void)
{
    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        currentMenu->menuitems[i].tics = 0;
    }
}

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_YELLOW     4
#define GLOW_ORANGE     5
#define GLOW_LIGHTGRAY  6
#define GLOW_BLUE       7
#define GLOW_OLIVE      8
#define GLOW_DARKGREEN  9
#define GLOW_GRAY       10

#define ITEMONTICS      currentMenu->menuitems[itemOn].tics
#define ITEMSETONTICS   currentMenu->menuitems[itemSetOn].tics

static byte *M_Item_Glow (int itemSetOn, int color)
{
    if (!menu_highlight)
    {
        return
            color == GLOW_RED ? cr[CR_RED] :
            color == GLOW_DARKRED ? cr[CR_DARKRED] :
            color == GLOW_GREEN ? cr[CR_GREEN] :
            color == GLOW_YELLOW ? cr[CR_YELLOW] :
            color == GLOW_ORANGE ? cr[CR_ORANGE] :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY] :
            color == GLOW_BLUE ? cr[CR_BLUE2] :
            color == GLOW_OLIVE ? cr[CR_OLIVE] :
            color == GLOW_DARKGREEN ? cr[CR_DARKGREEN] :
            color == GLOW_GRAY ? cr[CR_GRAY] :
                     NULL; // color == GLOW_UNCOLORED
    }

    if (itemOn == itemSetOn)
    {
        return
            color == GLOW_RED ||
            color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]       :
            color == GLOW_GREEN     ? cr[CR_GREEN_BRIGHT5]     :
            color == GLOW_YELLOW    ? cr[CR_YELLOW_BRIGHT5]    :
            color == GLOW_ORANGE    ? cr[CR_ORANGE_BRIGHT5]    :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY_BRIGHT5] :
            color == GLOW_BLUE      ? cr[CR_BLUE2_BRIGHT5]     :
            color == GLOW_OLIVE     ? cr[CR_OLIVE_BRIGHT5]     :
            color == GLOW_DARKGREEN ? cr[CR_DARKGREEN_BRIGHT5] :
            color == GLOW_GRAY      ? cr[CR_GRAY_BRIGHT5]      :
                                      cr[CR_MENU_BRIGHT5]      ; // GLOW_UNCOLORED
    }
    else
    {
        if (color == GLOW_UNCOLORED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_MENU_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_MENU_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_MENU_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_MENU_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
        }
        if (color == GLOW_RED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        if (color == GLOW_DARKRED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_DARK1] :
                ITEMSETONTICS == 4 ? cr[CR_RED_DARK2] :
                ITEMSETONTICS == 3 ? cr[CR_RED_DARK3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_DARK4] :
                ITEMSETONTICS == 1 ? cr[CR_RED_DARK5] : cr[CR_DARKRED];
        }
        if (color == GLOW_GREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
        if (color == GLOW_YELLOW)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_YELLOW_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_YELLOW_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_YELLOW_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_YELLOW_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_YELLOW_BRIGHT1] : cr[CR_YELLOW];
        }
        if (color == GLOW_ORANGE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_ORANGE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_ORANGE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_ORANGE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_ORANGE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_ORANGE_BRIGHT1] : cr[CR_ORANGE];
        }
        if (color == GLOW_LIGHTGRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_LIGHTGRAY_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_LIGHTGRAY_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_LIGHTGRAY_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_LIGHTGRAY_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_LIGHTGRAY_BRIGHT1] : cr[CR_LIGHTGRAY];
        }
        if (color == GLOW_BLUE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_BLUE2_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_BLUE2_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_BLUE2_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_BLUE2_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_BLUE2_BRIGHT1] : cr[CR_BLUE2];
        }
        if (color == GLOW_OLIVE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_OLIVE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_OLIVE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_OLIVE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_OLIVE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_OLIVE_BRIGHT1] : cr[CR_OLIVE];
        }
        if (color == GLOW_DARKGREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_DARKGREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_DARKGREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_DARKGREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_DARKGREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_DARKGREEN_BRIGHT1] : cr[CR_DARKGREEN];
        }
        if (color == GLOW_GRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GRAY_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GRAY_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GRAY_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GRAY_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GRAY_BRIGHT1] : cr[CR_GRAY];
        }
    }
    return NULL;
}

static byte *M_Cursor_Glow (int tics)
{
    if (!menu_highlight)
    return whichSkull ? NULL : cr[CR_MENU_DARK4];

    return
        tics ==  8 || tics ==  7 ? cr[CR_MENU_BRIGHT4] :
        tics ==  6 || tics ==  5 ? cr[CR_MENU_BRIGHT3] :
        tics ==  4 || tics ==  3 ? cr[CR_MENU_BRIGHT2] :
        tics ==  2 || tics ==  1 ? cr[CR_MENU_BRIGHT1] :
        tics == -1 || tics == -2 ? cr[CR_MENU_DARK1]   :
        tics == -3 || tics == -4 ? cr[CR_MENU_DARK2]   :
        tics == -5 || tics == -6 ? cr[CR_MENU_DARK3]   :
        tics == -7 || tics == -8 ? cr[CR_MENU_DARK4]   : NULL;
}

enum
{
    saveload_border,
    saveload_slider,
    saveload_text,
    saveload_cursor,
};

static byte *M_SaveLoad_Glow (int itemSetOn, int tics, int type)
{
    if (!menu_highlight)
    return NULL;

    switch (type)
    {
        case saveload_border:
        case saveload_slider:
            return itemSetOn ? cr[CR_MENU_BRIGHT2] : NULL;

        case saveload_text:
            return itemSetOn ? cr[CR_MENU_BRIGHT5] : M_Small_Line_Glow(tics);

        case saveload_cursor:
            return cr[CR_MENU_BRIGHT5];
    }

    return NULL;
}

static int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    // [PN] Adjust the slider value based on direction and handle min/max limits
    val += (direction == -1) ?  0 :     // [JN] Routine "-1" just reintializes value.
           (direction ==  0) ? -1 : 1;  // Otherwise, move either left "0" or right "1".

    if (val < min)
        val = capped ? min : max;
    else
    if (val > max)
        val = capped ? max : min;

    return val;
}

static float M_FLOAT_Slider (float val, float min, float max, float step,
                             int direction, boolean capped)
{
    // [PN] Adjust value based on direction
    val += (direction == -1) ? 0 :            // [JN] Routine "-1" just reintializes value.
           (direction ==  0) ? -step : step;  // Otherwise, move either left "0" or right "1".

    // [PN] Handle min/max limits
    if (val < min)
        val = capped ? min : max;
    else
    if (val > max)
        val = capped ? max : min;

    // [PN/JN] Do a float correction to get x.xxx000 values
    val = roundf(val * 1000.0f) / 1000.0f;

    return val;
}

static int DefSkillColor (const int skill)
{
    return
        skill == 0 ? GLOW_OLIVE     :
        skill == 1 ? GLOW_DARKGREEN :
        skill == 2 ? GLOW_GREEN     :
        skill == 3 ? GLOW_YELLOW    :
        skill == 4 ? GLOW_ORANGE    :
                     GLOW_RED;
}

static char *const DefSkillName[5] = 
{
    "IMTYTD" ,
    "HNTR"   ,
    "HMP"    ,
    "UV"     ,
    "NM"     
};


// -----------------------------------------------------------------------------
// Main ID Menu
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Main[]=
{
    { M_SWTC, "VIDEO OPTIONS",     M_Choose_ID_Video,    'v' },
    { M_SWTC, "DISPLAY OPTIONS",   M_Choose_ID_Display,  'd' },
    { M_SWTC, "SOUND OPTIONS",     M_Choose_ID_Sound,    's' },
    { M_SWTC, "CONTROL SETTINGS",  M_Choose_ID_Controls, 'c' },
    { M_SWTC, "WIDGETS SETTINGS",  M_Choose_ID_Widgets,  'w' },
    { M_SWTC, "AUTOMAP SETTINGS",  M_Choose_ID_Automap,  'a' },
    { M_SWTC, "GAMEPLAY FEATURES", M_Choose_ID_Gameplay, 'g' },
    { M_SWTC, "MISC FEATURES",     M_Choose_ID_Misc,     'm' },
    { M_SWTC, "LEVEL SELECT",      M_Choose_ID_Level,    'l' },
    { M_SWTC, "END GAME",          M_EndGame,            'e' },
    { M_SWTC, "RESET SETTINGS",    M_Choose_ID_Reset,    'r' },
};

static menu_t ID_Def_Main =
{
    11,
    &MainDef,
    ID_Menu_Main,
    M_Draw_ID_Main,
    ID_MENU_LEFTOFFSET_SML, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Main (int choice)
{
    M_SetupNextMenu (&ID_Def_Main);
}

static void M_Draw_ID_Main (void)
{
    M_WriteTextCentered(9, "OPTIONS", cr[CR_YELLOW]);
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Video[]=
{
    { M_MUL2, "TRUECOLOR RENDERING",  M_ID_TrueColor,    't' },
    { M_MUL1, "RENDERING RESOLUTION", M_ID_RenderingRes, 'r' },
    { M_MUL1, "WIDESCREEN MODE",      M_ID_Widescreen,   'w' },
    { M_MUL2, "EXCLUSIVE FULLSCREEN", M_ID_ExclusiveFS,  'e' },
    { M_MUL2, "UNCAPPED FRAMERATE",   M_ID_UncappedFPS,  'u' },
    { M_MUL1, "FRAMERATE LIMIT",      M_ID_LimitFPS,     'f' },
    { M_MUL2, "ENABLE VSYNC",         M_ID_VSync,        'e' },
    { M_MUL2, "SHOW FPS COUNTER",     M_ID_ShowFPS,      's' },
    { M_MUL2, "PIXEL SCALING",        M_ID_PixelScaling, 'p' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "SCREEN WIPE EFFECT",   M_ID_ScreenWipe,   's' },
    { M_MUL2, "SHOW DISK ICON",       M_ID_DiskIcon,     's' },
    { M_MUL2, "SHOW ENDOOM SCREEN",   M_ID_ShowENDOOM,   's' },
};

static menu_t ID_Def_Video =
{
    13,
    &ID_Def_Main,
    ID_Menu_Video,
    M_Draw_ID_Video,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Video (int choice)
{
    M_SetupNextMenu (&ID_Def_Video);
}

static void M_Draw_ID_Video (void)
{
    char str[32];

    M_WriteTextCentered(9, "VIDEO OPTIONS", cr[CR_YELLOW]);

#ifndef CRISPY_TRUECOLOR
    sprintf(str, "N/A");
    M_WriteText (M_ItemRightAlign(str), 18, str, 
                 M_Item_Glow(0, GLOW_DARKRED));
#else
    sprintf(str, vid_truecolor ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 18, str, 
                 M_Item_Glow(0, vid_truecolor ? GLOW_GREEN : GLOW_DARKRED));
#endif

    // Rendering resolution
    sprintf(str, vid_resolution == 1 ? "1X (200P)"  :
                 vid_resolution == 2 ? "2X (400P)"  :
                 vid_resolution == 3 ? "3X (600P)"  :
                 vid_resolution == 4 ? "4X (800P)"  :
                 vid_resolution == 5 ? "5X (1000P)" :
                 vid_resolution == 6 ? "6X (1200P)" :
                                       "CUSTOM");
    M_WriteText (M_ItemRightAlign(str), 27, str, 
                 M_Item_Glow(1, vid_resolution == 1 ? GLOW_DARKRED :
                                vid_resolution == 2 ||
                                vid_resolution == 3 ? GLOW_GREEN :
                                vid_resolution == 4 ||
                                vid_resolution == 5 ? GLOW_YELLOW :
                                                      GLOW_ORANGE));

    // Widescreen rendering
    sprintf(str, vid_widescreen == 1 ? "MATCH SCREEN" :
                 vid_widescreen == 2 ? "16:10" :
                 vid_widescreen == 3 ? "16:9" :
                 vid_widescreen == 4 ? "21:9" :
                 vid_widescreen == 5 ? "32:9" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 36, str, 
                 M_Item_Glow(2, vid_widescreen ? GLOW_GREEN : GLOW_DARKRED));

    // Exclusive fullscreen
    sprintf(str, vid_fullscreen_exclusive ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 45, str, 
                 M_Item_Glow(3, vid_fullscreen_exclusive ? GLOW_GREEN : GLOW_DARKRED));

    // Uncapped framerate
    sprintf(str, vid_uncapped_fps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 54, str, 
                 M_Item_Glow(4, vid_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !vid_uncapped_fps ? "35" :
                 vid_fpslimit ? "%d" : "NONE", vid_fpslimit);
    M_WriteText (M_ItemRightAlign(str), 63, str, 
                 !vid_uncapped_fps ? cr[CR_DARKRED] :
                 M_Item_Glow(5, vid_fpslimit == 0 ? GLOW_RED :
                                vid_fpslimit >= 500 ? GLOW_YELLOW : GLOW_GREEN));

    // Enable vsync
    sprintf(str, vid_vsync ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str, 
                 M_Item_Glow(6, vid_vsync ? GLOW_GREEN : GLOW_DARKRED));

    // Show FPS counter
    sprintf(str, vid_showfps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 81, str, 
                 M_Item_Glow(7, vid_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Pixel scaling
    sprintf(str, vid_smooth_scaling ? "SMOOTH" : "SHARP");
    M_WriteText (M_ItemRightAlign(str), 90, str, 
                 M_Item_Glow(8, vid_smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(99, "MISCELLANEOUS", cr[CR_YELLOW]);

    // Screen wipe effect
    sprintf(str, vid_screenwipe == 1 ? "ORIGINAL" :
                 vid_screenwipe == 2 ? "FAST" :
                 vid_screenwipe == 3 ? "CROSSFADE" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, vid_screenwipe == 1 ? GLOW_DARKRED : GLOW_GREEN));

    // Show disk icon
    sprintf(str, vid_diskicon == 1 ? "BOTTOM" :
                 vid_diskicon == 2 ? "TOP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 117, str, 
                 M_Item_Glow(11, vid_diskicon == 1 ? GLOW_DARKRED : GLOW_GREEN));

    // Show ENDOOM screen
    sprintf(str, vid_endoom == 1 ? "ALWAYS" :
                 vid_endoom == 2 ? "PWAD ONLY" : "NEVER");
    M_WriteText (M_ItemRightAlign(str), 126, str, 
                 M_Item_Glow(12, vid_endoom == 1 ? GLOW_DARKRED : GLOW_GREEN));

    // [JN] Print current resolution. Shamelessly taken from Nugget Doom!
    if (itemOn == 1 || itemOn == 2)
    {
        char  width[8];
        char  height[8];
        const char *resolution;

        M_snprintf(width, 8, "%d", (ORIGWIDTH + (WIDESCREENDELTA*2)) * vid_resolution);
        M_snprintf(height, 8, "%d", (ORIGHEIGHT * vid_resolution));
        resolution = M_StringJoin("CURRENT RESOLUTION: ", width, "x", height, NULL);

        M_WriteTextCentered(135, resolution, cr[CR_LIGHTGRAY_DARK1]);
    }
}

#ifdef CRISPY_TRUECOLOR
static void M_ID_TrueColorHook (void)
{
    vid_truecolor ^= 1;

    // [crispy] re-calculate amount of colormaps and light tables
    R_InitColormaps();
    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
    // [crispy] re-calculate fake contrast
    P_SegLengths(true);
}
#endif

static void M_ID_TrueColor (int choice)
{
#ifdef CRISPY_TRUECOLOR
    post_rendering_hook = M_ID_TrueColorHook;
#endif
}

static void M_ID_RenderingResHook (void)
{
    // [crispy] re-initialize framebuffers, textures and renderer
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    // [crispy] re-calculate framebuffer coordinates
    R_ExecuteSetViewSize();
    // [crispy] re-draw bezel
    R_FillBackScreen();
    // [crispy] re-calculate disk icon coordinates
    V_EnableLoadingDisk();
    // [JN] re-calculate status bar elements background buffers
    ST_InitElementsBackground();
    // [crispy] re-calculate automap coordinates
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
    // [JN] re-initialize mouse cursor position
    I_ReInitCursorPosition();
}

static void M_ID_RenderingRes (int choice)
{
    vid_resolution = M_INT_Slider(vid_resolution, 1, MAXHIRES, choice, false);
    post_rendering_hook = M_ID_RenderingResHook;
}

static void M_ID_WidescreenHook (void)
{
    // [crispy] re-initialize framebuffers, textures and renderer
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    // [crispy] re-calculate framebuffer coordinates
    R_ExecuteSetViewSize();
    // [crispy] re-draw bezel
    R_FillBackScreen();
    // [crispy] re-calculate disk icon coordinates
    V_EnableLoadingDisk();
    // [JN] re-calculate status bar elements background buffers
    ST_InitElementsBackground();
    // [crispy] re-calculate automap coordinates
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
}

static void M_ID_Widescreen (int choice)
{
    vid_widescreen = M_INT_Slider(vid_widescreen, 0, 5, choice, false);
    post_rendering_hook = M_ID_WidescreenHook;
}

static void M_ID_ExclusiveFSHook (void)
{
    vid_fullscreen_exclusive ^= 1;

    // [JN] Force to update, if running in fullscreen mode.
    if (vid_fullscreen)
    {
        I_UpdateExclusiveFullScreen();
    }
}

static void M_ID_ExclusiveFS (int choice)
{
    post_rendering_hook = M_ID_ExclusiveFSHook;
}

static void M_ID_UncappedFPS (int choice)
{
    vid_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
}

static void M_ID_LimitFPS (int choice)
{
    if (!vid_uncapped_fps)
    {
        return;  // Do not allow change value in capped framerate.
    }

    // [PN] Adjust vid_fpslimit based on choice and speedkeydown
    if (choice == 0 && vid_fpslimit)
    {
        vid_fpslimit -= speedkeydown() ? 10 : 1;

        if (vid_fpslimit < TICRATE)
            vid_fpslimit = 0;
    }
    else if (choice == 1 && vid_fpslimit < 501)
    {
        vid_fpslimit += speedkeydown() ? 10 : 1;

        if (vid_fpslimit < TICRATE)
            vid_fpslimit = TICRATE;
        if (vid_fpslimit > 500)
            vid_fpslimit = 500;
    }
}

static void M_ID_VSyncHook (void)
{
    I_ToggleVsync();
}

static void M_ID_VSync (int choice)
{
    vid_vsync ^= 1;
    post_rendering_hook = M_ID_VSyncHook;    
}

static void M_ID_ShowFPS (int choice)
{
    vid_showfps ^= 1;
}

static void M_ID_PixelScaling (int choice)
{
    vid_smooth_scaling ^= 1;

    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
}

static void M_ID_ScreenWipe (int choice)
{
    vid_screenwipe = M_INT_Slider(vid_screenwipe, 0, 3, choice, false);
}

static void M_ID_DiskIcon (int choice)
{
    vid_diskicon = M_INT_Slider(vid_diskicon, 0, 2, choice, false);
    V_EnableLoadingDisk();
}

static void M_ID_ShowENDOOM (int choice)
{
    vid_endoom = M_INT_Slider(vid_endoom, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Display[]=
{
    { M_MUL1, "FIELD OF VIEW",           M_ID_FOV,               'f' },
    { M_MUL1, "MENU BACKGROUND SHADING", M_ID_MenuShading,       'm' },
    { M_MUL1, "EXTRA LEVEL BRIGHTNESS",  M_ID_LevelBrightness,   'e' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL1, "GAMMA-CORRECTION",        M_ID_Gamma,             'g' },
    { M_MUL1, "SATURATION",              M_ID_Saturation,        's' },
    { M_MUL1, "CONTRAST",                M_ID_Contrast,          'c' },
    { M_MUL1, "RED INTENSITY",           M_ID_R_Intensity,       'r' },
    { M_MUL1, "GREEN INTENSITY",         M_ID_G_Intensity,       'g' },
    { M_MUL1, "BLUE INTENSITY",          M_ID_B_Intensity,       'b' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "MESSAGES ENABLED",        M_ChangeMessages,       'm' },
    { M_MUL2, "MESSAGES ALIGNMENT",      M_ID_MessagesAlignment, 'm' },
    { M_MUL2, "TEXT CASTS SHADOWS",      M_ID_TextShadows,       't' },
    { M_MUL2, "LOCAL TIME",              M_ID_LocalTime,         'l' },
};

static menu_t ID_Def_Display =
{
    15,
    &ID_Def_Main,
    ID_Menu_Display,
    M_Draw_ID_Display,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Display (int choice)
{
    M_SetupNextMenu (&ID_Def_Display);
}

static void M_Draw_ID_Display (void)
{
    char str[32];

    M_WriteTextCentered(9, "DISPLAY OPTIONS", cr[CR_YELLOW]);

    // Field of View
    sprintf(str, "%d", vid_fov);
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, vid_fov == 135 || vid_fov == 45 ? GLOW_YELLOW :
                                vid_fov == 90 ? GLOW_DARKRED : GLOW_GREEN));

    // Background shading
    sprintf(str, dp_menu_shading ? "%d" : "OFF", dp_menu_shading);
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, dp_menu_shading == 12 ? GLOW_YELLOW :
                                dp_menu_shading  >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, dp_level_brightness ? "%d" : "OFF", dp_level_brightness);
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, dp_level_brightness == 8 ? GLOW_YELLOW :
                                dp_level_brightness >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    M_WriteTextCentered(45, "COLOR SETTINGS", cr[CR_YELLOW]);

    // Gamma-correction
    sprintf(str, "%s", gammalvls[vid_gamma][1]);
    M_WriteText (M_ItemRightAlign(str), 54, str,
                    M_Item_Glow(4, GLOW_UNCOLORED));

    // Saturation
    M_snprintf(str, 6, "%d%%", vid_saturation);
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, GLOW_LIGHTGRAY));

    // Contrast
    M_snprintf(str, 6, "%3f", vid_contrast);
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, GLOW_YELLOW));

    // RED intensity
    M_snprintf(str, 6, "%3f", vid_r_intensity);
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, GLOW_RED));

    // GREEN intensity
    M_snprintf(str, 6, "%3f", vid_g_intensity);
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, GLOW_GREEN));

    // BLUE intensity
    M_snprintf(str, 6, "%3f", vid_b_intensity);
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, GLOW_BLUE));

    M_WriteTextCentered(108, "MESSAGES SETTINGS", cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, msg_show ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 117, str,
                 M_Item_Glow(11, msg_show ? GLOW_DARKRED : GLOW_GREEN));

    // Messages alignment
    sprintf(str, msg_alignment == 1 ? "STATUS BAR" :
                 msg_alignment == 2 ? "CENTERED" : "LEFT");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, msg_alignment ? GLOW_GREEN : GLOW_DARKRED));

    // Text casts shadows
    sprintf(str, msg_text_shadows ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 135, str, 
                 M_Item_Glow(13, msg_text_shadows ? GLOW_GREEN : GLOW_DARKRED));

    // Local time
    sprintf(str, msg_local_time == 1 ? "12-HOUR FORMAT" :
                 msg_local_time == 2 ? "24-HOUR FORMAT" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 144, str, 
                 M_Item_Glow(14, msg_local_time ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_ID_FOV (int choice)
{
    vid_fov = M_INT_Slider(vid_fov, 45, 135, choice, true);

    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
}

static void M_ID_MenuShading (int choice)
{
    dp_menu_shading = M_INT_Slider(dp_menu_shading, 0, 12, choice, true);
}

static void M_ID_LevelBrightness (int choice)
{
    dp_level_brightness = M_INT_Slider(dp_level_brightness, 0, 8, choice, true);
}

static void M_ID_Gamma (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_gamma = M_INT_Slider(vid_gamma, 0, MAXGAMMA-1, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    I_SetPalette (st_palette);
    R_InitColormaps();
    R_FillBackScreen();
    st_fullupdate = true;
#endif
}

static void M_ID_SaturationHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_Saturation (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_saturation = M_INT_Slider(vid_saturation, 0, 100, choice, true);
    post_rendering_hook = M_ID_SaturationHook;
}

static void M_ID_ContrastHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_Contrast (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_contrast = M_FLOAT_Slider(vid_contrast, 0.100000f, 2.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_ContrastHook;
}

static void M_ID_R_IntensityHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_R_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_r_intensity = M_FLOAT_Slider(vid_r_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_R_IntensityHook;
}

static void M_ID_G_IntensityHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_G_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_g_intensity = M_FLOAT_Slider(vid_g_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_G_IntensityHook;
}

static void M_ID_B_IntensityHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_B_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_b_intensity = M_FLOAT_Slider(vid_b_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_B_IntensityHook;
}

static void M_ID_MessagesAlignment (int choice)
{
    msg_alignment = M_INT_Slider(msg_alignment, 0, 2, choice, false);
}

static void M_ID_TextShadows (int choice)
{
    msg_text_shadows ^= 1;
}

static void M_ID_LocalTime (int choice)
{
    msg_local_time = M_INT_Slider(msg_local_time, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Sound options
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Sound[]=
{
    { M_SLDR, "SFX VOLUME",           M_SfxVol,          's' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_SLDR, "MUSIC VOLUME",         M_MusicVol,        'm' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "SFX PLAYBACK",         M_ID_SFXSystem,    's' },
    { M_MUL2, "MUSIC PLAYBACK",       M_ID_MusicSystem,  'm' },
    { M_MUL1, "SOUND EFFECTS MODE",   M_ID_SFXMode,      's' },
    { M_MUL2, "PITCH-SHIFTED SOUNDS", M_ID_PitchShift,   'p' },
    { M_MUL1, "NUMBER OF SFX TO MIX", M_ID_SFXChannels,  'n' },
    { M_MUL2, "MUTE INACTIVE WINDOW", M_ID_MuteInactive, 'm' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "",                     M_ID_RemasterOST,  'p' }, // PREFFERED SOUNDTRACK
};

static menu_t ID_Def_Sound =
{
    15,
    &ID_Def_Main,
    ID_Menu_Sound,
    M_Draw_ID_Sound,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Sound (int choice)
{
    M_SetupNextMenu (&ID_Def_Sound);
}

static void M_Draw_ID_Sound (void)
{
    char str[16];

    M_WriteTextCentered(9, "VOLUME", cr[CR_YELLOW]);

    M_DrawThermo(46, 27, 16, sfxVolume, 0);
    M_ID_HandleSliderMouseControl(52, 27, 132, &sfxVolume, false, 0, 15);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (192, 30, str, M_Item_Glow(0, GLOW_UNCOLORED));

    M_DrawThermo(46, 54, 16, musicVolume, 3);
    M_ID_HandleSliderMouseControl(52, 54, 132, &musicVolume, false, 0, 15);
    sprintf(str,"%d", musicVolume);
    M_WriteText (192, 57, str, M_Item_Glow(3, GLOW_UNCOLORED));

    M_WriteTextCentered(72, "SOUND SYSTEM", cr[CR_YELLOW]);

    // SFX playback
    sprintf(str, snd_sfxdevice == 0 ? "DISABLED"    :
                 snd_sfxdevice == 1 ? "PC SPEAKER"  :
                 snd_sfxdevice == 3 ? "DIGITAL SFX" :
                                      "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, snd_sfxdevice ? GLOW_GREEN : GLOW_RED));

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ? "GUS (EMULATED)" :
                 snd_musicdevice == 8 ? "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                        "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, snd_musicdevice ? GLOW_GREEN : GLOW_RED));

    // Sound effects mode
    sprintf(str, snd_monosfx ? "MONO" : "STEREO");
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, snd_monosfx ? GLOW_RED : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, snd_pitchshift ? GLOW_GREEN : GLOW_RED));

    // Number of SFX to mix
    sprintf(str, "%i", snd_channels);
    M_WriteText (M_ItemRightAlign(str), 117, str,
                 M_Item_Glow(11, snd_channels == 8 ? GLOW_DARKRED :
                                 snd_channels == 1 || snd_channels == 16 ? GLOW_YELLOW : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_mute_inactive ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, snd_mute_inactive ? GLOW_GREEN : GLOW_RED));

    M_WriteTextCentered(135, "REMASTERED MUSIC", cr[CR_YELLOW]);

    M_WriteText (ID_MENU_LEFTOFFSET, 144, "PREFERRED SOUNDTRACK",
                 M_Item_Glow(14, remaster_ost ? GLOW_UNCOLORED : GLOW_DARKRED));
    // Remastered music
    sprintf(str, !remaster_ost ? "OFF" :
                 snd_remaster_ost == 1 ? "REMIX" :
                 snd_remaster_ost == 2 ? "ORIGINAL" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 144, str,
                 M_Item_Glow(14, !remaster_ost ? GLOW_DARKRED :
                                 snd_remaster_ost == 1 ? GLOW_GREEN :
                                 snd_remaster_ost == 2 ? GLOW_YELLOW : GLOW_RED));

    // Inform if FSYNTH/GUS paths anen't set.
    if (itemOn == 8)
    {
        if (snd_musicdevice == 5 && strcmp(gus_patch_path, "") == 0)
        {
            M_WriteTextCentered(153, "\"GUS_PATCH_PATH\" VARIABLE IS NOT SET", cr[CR_GRAY]);
        }
#ifdef HAVE_FLUIDSYNTH
        if (snd_musicdevice == 11 && strcmp(fsynth_sf_path, "") == 0)
        {
            M_WriteTextCentered(153, "\"FSYNTH_SF_PATH\" VARIABLE IS NOT SET", cr[CR_GRAY]);
        }
#endif // HAVE_FLUIDSYNTH
    }
    // Inform if remaster soundtrack is not loaded
    if (itemOn == 14 && !remaster_ost)
    {
        M_WriteTextCentered(153, "EXTRAS.WAD FILE NOT LOADED", cr[CR_GRAY]);
    }
}

static void M_ID_SFXSystem (int choice)
{
    // [PN] Adjust snd_sfxdevice based on choice
    if (choice == 0)
    {
        snd_sfxdevice = (snd_sfxdevice == 0) ? 1 :
                        (snd_sfxdevice == 1) ? 3 : 0;
    }
    else if (choice == 1)
    {
        snd_sfxdevice = (snd_sfxdevice == 0) ? 3 :
                        (snd_sfxdevice == 3) ? 1 : 0;
    }

    // Shut down current music
    S_StopMusic();

    // Free all sound channels/usefulness
    S_ChangeSFXSystem();

    // Shut down sound/music system
    I_ShutdownSound();

    // Start sound/music system
    I_InitSound(doom);

    // Re-generate SFX cache
    I_PrecacheSounds(S_sfx, NUMSFX);

    // Reinitialize sound volume
    S_SetSfxVolume(sfxVolume * 8);

    // Reinitialize music volume
    S_SetMusicVolume(musicVolume * 8);

    // Restart current music
    S_ChangeMusic(current_mus_num, usergame);
}

static void M_ID_MusicSystem (int choice)
{
    switch (choice)
    {
        case 0:
            if (snd_musicdevice == 0)
            {
                snd_musicdevice = 5;    // Set to SDL Mixer
            }
            else if (snd_musicdevice == 5)
#ifdef HAVE_FLUIDSYNTH
            {
                snd_musicdevice = 11;    // Set to FluidSynth
            }
            else if (snd_musicdevice == 11)
#endif // HAVE_FLUIDSYNTH
            {
                snd_musicdevice = 8;    // Set to Native MIDI
            }
            else if (snd_musicdevice == 8)
            {
                snd_musicdevice = 3;    // Set to OPL3
                snd_dmxoption = "-opl3";
            }
            else if (snd_musicdevice == 3  && !strcmp(snd_dmxoption, "-opl3"))
            {
                snd_musicdevice = 3;    // Set to OPL2
                snd_dmxoption = "";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, ""))
            {
                snd_musicdevice = 0;    // Disable
            }
            break;
        case 1:
            if (snd_musicdevice == 0)
            {
                snd_musicdevice  = 3;   // Set to OPL2
                snd_dmxoption = "";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, ""))
            {
                snd_musicdevice  = 3;   // Set to OPL3
                snd_dmxoption = "-opl3";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3"))
            {
                snd_musicdevice  = 8;   // Set to Native MIDI
            }
            else if (snd_musicdevice == 8)
#ifdef HAVE_FLUIDSYNTH
            {
                snd_musicdevice  = 11;   // Set to FluidSynth
            }
            else if (snd_musicdevice == 11)
#endif // HAVE_FLUIDSYNTH
            {
                snd_musicdevice  = 5;   // Set to SDL Mixer
            }
            else if (snd_musicdevice == 5)
            {
                snd_musicdevice  = 0;   // Disable
            }
            break;
        default:
            {
                break;
            }
    }

    // Shut down current music
    S_StopMusic();

    // Shut down music system
    S_Shutdown();
    
    // Start music system
    I_InitSound(doom);

    // Reinitialize music volume
    S_SetMusicVolume(musicVolume * 8);

    // Restart current music
    S_ChangeMusic(current_mus_num, usergame);
}

static void M_ID_SFXMode (int choice)
{
    snd_monosfx ^= 1;

    // Update stereo separation
    S_UpdateStereoSeparation();
}

static void M_ID_PitchShift (int choice)
{
    snd_pitchshift ^= 1;
}

static void M_ID_SFXChannels (int choice)
{
    snd_channels = M_INT_Slider(snd_channels, 1, 16, choice, true);
}

static void M_ID_MuteInactive (int choice)
{
    snd_mute_inactive ^= 1;
}

static void M_ID_RemasterOST (int choice)
{
    if (!remaster_ost)
        return;

    snd_remaster_ost = M_INT_Slider(snd_remaster_ost, 0, 2, choice, false);

    // [JN] Forcefully stop current music.
    S_StopMusic();
    // [JN] Reinitialize music lump numbers and restart current music.
    S_Start();
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Controls[]=
{
    { M_SWTC, "KEYBOARD BINDINGS",            M_Choose_ID_Keybinds,       'k' },
    { M_SWTC, "MOUSE BINDINGS",               M_Choose_ID_MouseBinds,     'm' },
    { M_SKIP, "", 0, '\0' },
    { M_SLDR, "SENSIVITY",                    M_ID_Controls_Sensivity,    's' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_SLDR, "ACCELERATION",                 M_ID_Controls_Acceleration, 'a' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_SLDR, "ACCELERATION THRESHOLD",       M_ID_Controls_Threshold,    'a' },
    { M_SKIP, "", 0, '\0' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "MOUSE LOOK",                   M_ID_Controls_MLook,        'm' },
    { M_MUL2, "VERTICAL MOUSE MOVEMENT",      M_ID_Controls_NoVert,       'v' },
    { M_MUL2, "INVERT VERTICAL AXIS",         M_ID_Controls_InvertY,      'v' },
    { M_MUL2, "DOUBLE CLICK ACTS AS \"USE\"", M_ID_Controls_DblClck,      'd' },
};

static menu_t ID_Def_Controls =
{
    16,
    &ID_Def_Main,
    ID_Menu_Controls,
    M_Draw_ID_Controls,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Controls (int choice)
{
    M_SetupNextMenu (&ID_Def_Controls);
}

static void M_Draw_ID_Controls (void)
{
    char str[32];

    M_WriteTextCentered(9, "BINDINGS", cr[CR_YELLOW]);
    
    M_WriteTextCentered(36, "MOUSE CONFIGURATION", cr[CR_YELLOW]);

    M_DrawThermo(46, 54, 15, mouseSensitivity, 3);
    M_ID_HandleSliderMouseControl(52, 54, 124, &mouseSensitivity, false, 0, 14);
    sprintf(str,"%d", mouseSensitivity);
    M_WriteText (184, 57, str, M_Item_Glow(3, mouseSensitivity == 255 ? GLOW_YELLOW :
                                              mouseSensitivity > 14 ? GLOW_GREEN : GLOW_UNCOLORED));

    M_DrawThermo(46, 81, 8, (mouse_acceleration * 1.8f) - 2, 6);
    M_ID_HandleSliderMouseControl(52, 81, 100, &mouse_acceleration, true, 0, 9);
    sprintf(str,"%.1f", mouse_acceleration);
    M_WriteText (128, 84, str, M_Item_Glow(6, GLOW_UNCOLORED));

    M_DrawThermo(46, 108, 15, mouse_threshold / 2.2f, 9);
    M_ID_HandleSliderMouseControl(52, 108, 124, &mouse_threshold, false, 0, 32);
    sprintf(str,"%d", mouse_threshold);
    M_WriteText (184, 111, str, M_Item_Glow(9, GLOW_UNCOLORED));

    // Mouse look
    sprintf(str, mouse_look ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, mouse_look ? GLOW_GREEN : GLOW_RED));

    // Vertical mouse movement
    sprintf(str, mouse_novert ? "OFF" : "ON");
    M_WriteText (M_ItemRightAlign(str), 135, str,
                 M_Item_Glow(13, mouse_novert ? GLOW_RED : GLOW_GREEN));

    // Invert vertical axis
    sprintf(str, mouse_y_invert ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 144, str,
                 M_Item_Glow(14, mouse_y_invert ? GLOW_GREEN : GLOW_RED));

    // Double click acts as "use"
    sprintf(str, mouse_dclick_use ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 153, str,
                 M_Item_Glow(15, mouse_dclick_use ? GLOW_GREEN : GLOW_RED));

}

static void M_ID_Controls_Sensivity (int choice)
{
    // [crispy] extended range
    mouseSensitivity = M_INT_Slider(mouseSensitivity, 0, 255, choice, true);
}

static void M_ID_Controls_Acceleration (int choice)
{
    mouse_acceleration = M_FLOAT_Slider(mouse_acceleration, 1.000000f, 5.000000f, 0.100000f, choice, true);
}

static void M_ID_Controls_Threshold (int choice)
{
    mouse_threshold = M_INT_Slider(mouse_threshold, 0, 32, choice, true);
}

static void M_ID_Controls_MLook (int choice)
{
    mouse_look ^= 1;
    if (!mouse_look)
    {
        players[consoleplayer].lookdir = 0;
    }
    R_InitSkyMap();
}

static void M_ID_Controls_NoVert (int choice)
{
    mouse_novert ^= 1;
}

static void M_ID_Controls_DblClck (int choice)
{
    mouse_dclick_use ^= 1;
}

static void M_ID_Controls_InvertY (int choice)
{
    mouse_y_invert ^= 1;
}

// -----------------------------------------------------------------------------
// Keybinds 1
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_1[]=
{
    { M_SWTC, "MOVE FORWARD",    M_Bind_MoveForward,  'm' },
    { M_SWTC, "MOVE BACKWARD",   M_Bind_MoveBackward, 'm' },
    { M_SWTC, "TURN LEFT",       M_Bind_TurnLeft,     't' },
    { M_SWTC, "TURN RIGHT",      M_Bind_TurnRight,    't' },
    { M_SWTC, "STRAFE LEFT",     M_Bind_StrafeLeft,   's' },
    { M_SWTC, "STRAFE RIGHT",    M_Bind_StrafeRight,  's' },
    { M_SWTC, "SPEED ON",        M_Bind_SpeedOn,      's' },
    { M_SWTC, "STRAFE ON",       M_Bind_StrafeOn,     's' },
    { M_SWTC, "180 DEGREE TURN", M_Bind_180Turn,      '1' },
    { M_SKIP, "", 0, '\0'},
    { M_SWTC, "FIRE/ATTACK",     M_Bind_FireAttack,   'f' },
    { M_SWTC, "USE",             M_Bind_Use,          'u' },
};

static menu_t ID_Def_Keybinds_1 =
{
    12,
    &ID_Def_Controls,
    ID_Menu_Keybinds_1,
    M_Draw_ID_Keybinds_1,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_MoveForward (int choice)
{
    M_StartBind(100);  // key_up
}

static void M_Bind_MoveBackward (int choice)
{
    M_StartBind(101);  // key_down
}

static void M_Bind_TurnLeft (int choice)
{
    M_StartBind(102);  // key_left
}

static void M_Bind_TurnRight (int choice)
{
    M_StartBind(103);  // key_right
}

static void M_Bind_StrafeLeft (int choice)
{
    M_StartBind(104);  // key_strafeleft
}

static void M_Bind_StrafeRight (int choice)
{
    M_StartBind(105);  // key_straferight
}

static void M_Bind_SpeedOn (int choice)
{
    M_StartBind(106);  // key_speed
}

static void M_Bind_StrafeOn (int choice)
{
    M_StartBind(107);  // key_strafe
}

static void M_Bind_180Turn (int choice)
{
    M_StartBind(108);  // key_180turn
}

static void M_Bind_FireAttack (int choice)
{
    M_StartBind(109);  // key_fire
}

static void M_Bind_Use (int choice)
{
    M_StartBind(110);  // key_use
}

static void M_Draw_ID_Keybinds_1 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 0;

    M_FillBackground();

    M_WriteTextCentered(9, "MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_up);
    M_DrawBindKey(1, 27, key_down);
    M_DrawBindKey(2, 36, key_left);
    M_DrawBindKey(3, 45, key_right);
    M_DrawBindKey(4, 54, key_strafeleft);
    M_DrawBindKey(5, 63, key_straferight);
    M_DrawBindKey(6, 72, key_speed);
    M_DrawBindKey(7, 81, key_strafe);
    M_DrawBindKey(8, 90, key_180turn);

    M_WriteTextCentered(99, "ACTION", cr[CR_YELLOW]);

    M_DrawBindKey(10, 108, key_fire);
    M_DrawBindKey(11, 117, key_use);

    M_DrawBindFooter("1", true);
}

// -----------------------------------------------------------------------------
// Keybinds 2
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_2[]=
{
    { M_SWTC, "ALWAYS RUN",              M_Bind_AlwaysRun,      'a' },
    { M_SWTC, "MOUSE LOOK",              M_Bind_MouseLook,      'm' },
    { M_SWTC, "VERTICAL MOUSE MOVEMENT", M_Bind_NoVert,         'v' },
    { M_SKIP, "", 0, '\0'},
    { M_SWTC, "RESTART LEVEL/DEMO",      M_Bind_RestartLevel,   'g' },
    { M_SWTC, "GO TO NEXT LEVEL",        M_Bind_NextLevel,      'd' },
    { M_SWTC, "DEMO FAST-FORWARD",       M_Bind_FastForward,    'd' },
    { M_SWTC, "FLIP LEVEL HORIZONTALLY", M_Bind_FlipLevels,     'f' },
    { M_SWTC, "TOGGLE EXTENDED HUD",     M_Bind_ExtendedHUD,    't' },
    { M_SKIP, "", 0, '\0'},
    { M_SWTC, "SPECTATOR MODE",           M_Bind_SpectatorMode, 's' },
    { M_SWTC, "FREEZE MODE",              M_Bind_FreezeMode,    'f' },
    { M_SWTC, "NOTARGET MODE",            M_Bind_NotargetMode,  'n' },
    { M_SWTC, "BUDDHA MODE",              M_Bind_BuddhaMode,    'b' },
};

static menu_t ID_Def_Keybinds_2 =
{
    14,
    &ID_Def_Controls,
    ID_Menu_Keybinds_2,
    M_Draw_ID_Keybinds_2,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_AlwaysRun (int choice)
{
    M_StartBind(200);  // key_autorun
}

static void M_Bind_MouseLook (int choice)
{
    M_StartBind(201);  // key_mouse_look
}

static void M_Bind_NoVert (int choice)
{
    M_StartBind(202);  // key_novert
}

static void M_Bind_RestartLevel (int choice)
{
    M_StartBind(203);  // key_reloadlevel
}

static void M_Bind_NextLevel (int choice)
{
    M_StartBind(204);  // key_nextlevel
}

static void M_Bind_FastForward (int choice)
{
    M_StartBind(205);  // key_demospeed
}

static void M_Bind_FlipLevels (int choice)
{
    M_StartBind(206);  // key_flip_levels
}

static void M_Bind_ExtendedHUD (int choice)
{
    M_StartBind(207);  // key_widget_enable
}

static void M_Bind_SpectatorMode (int choice)
{
    M_StartBind(208);  // key_spectator
}

static void M_Bind_FreezeMode (int choice)
{
    M_StartBind(209);  // key_freeze
}

static void M_Bind_NotargetMode (int choice)
{
    M_StartBind(210);  // key_notarget
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(211);  // key_buddha
}

static void M_Draw_ID_Keybinds_2 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 1;

    M_FillBackground();

    M_WriteTextCentered(9, "ADVANCED MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_autorun);
    M_DrawBindKey(1, 27, key_mouse_look);
    M_DrawBindKey(2, 36, key_novert);

    M_WriteTextCentered(45, "SPECIAL KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(4, 54, key_reloadlevel);
    M_DrawBindKey(5, 63, key_nextlevel);
    M_DrawBindKey(6, 72, key_demospeed);
    M_DrawBindKey(7, 81, key_flip_levels);
    M_DrawBindKey(8, 90, key_widget_enable);

    M_WriteTextCentered(99, "SPECIAL MODES", cr[CR_YELLOW]);

    M_DrawBindKey(10, 108, key_spectator);
    M_DrawBindKey(11, 117, key_freeze);
    M_DrawBindKey(12, 126, key_notarget);
    M_DrawBindKey(13, 135, key_buddha);

    M_DrawBindFooter("2", true);
}

// -----------------------------------------------------------------------------
// Keybinds 3
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_3[]=
{
    { M_SWTC, "WEAPON 1",        M_Bind_Weapon1,    'w' },
    { M_SWTC, "WEAPON 2",        M_Bind_Weapon2,    'w' },
    { M_SWTC, "WEAPON 3",        M_Bind_Weapon3,    'w' },
    { M_SWTC, "WEAPON 4",        M_Bind_Weapon4,    'w' },
    { M_SWTC, "WEAPON 5",        M_Bind_Weapon5,    'w' },
    { M_SWTC, "WEAPON 6",        M_Bind_Weapon6,    'w' },
    { M_SWTC, "WEAPON 7",        M_Bind_Weapon7,    'w' },
    { M_SWTC, "WEAPON 8",        M_Bind_Weapon8,    'w' },
    { M_SWTC, "PREVIOUS WEAPON", M_Bind_PrevWeapon, 'p' },
    { M_SWTC, "NEXT WEAPON",     M_Bind_NextWeapon, 'n' },
};

static menu_t ID_Def_Keybinds_3 =
{
    10,
    &ID_Def_Controls,
    ID_Menu_Keybinds_3,
    M_Draw_ID_Keybinds_3,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_Weapon1 (int choice)
{
    M_StartBind(300);  // key_weapon1
}

static void M_Bind_Weapon2 (int choice)
{
    M_StartBind(301);  // key_weapon2
}

static void M_Bind_Weapon3 (int choice)
{
    M_StartBind(302);  // key_weapon3
}

static void M_Bind_Weapon4 (int choice)
{
    M_StartBind(303);  // key_weapon4
}

static void M_Bind_Weapon5 (int choice)
{
    M_StartBind(304);  // key_weapon5
}

static void M_Bind_Weapon6 (int choice)
{
    M_StartBind(305);  // key_weapon6
}

static void M_Bind_Weapon7 (int choice)
{
    M_StartBind(306);  // key_weapon7
}

static void M_Bind_Weapon8 (int choice)
{
    M_StartBind(307);  // key_weapon8
}

static void M_Bind_PrevWeapon (int choice)
{
    M_StartBind(308);  // key_prevweapon
}

static void M_Bind_NextWeapon (int choice)
{
    M_StartBind(309);  // key_nextweapon
}

static void M_Draw_ID_Keybinds_3 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 2;

    M_FillBackground();

    M_WriteTextCentered(9, "WEAPONS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_weapon1);
    M_DrawBindKey(1, 27, key_weapon2);
    M_DrawBindKey(2, 36, key_weapon3);
    M_DrawBindKey(3, 45, key_weapon4);
    M_DrawBindKey(4, 54, key_weapon5);
    M_DrawBindKey(5, 63, key_weapon6);
    M_DrawBindKey(6, 72, key_weapon7);
    M_DrawBindKey(7, 81, key_weapon8);
    M_DrawBindKey(8, 90, key_prevweapon);
    M_DrawBindKey(9, 99, key_nextweapon);

    M_DrawBindFooter("3", true);
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_4[]=
{
    { M_SWTC, "TOGGLE MAP",       M_Bind_ToggleMap,   't' },
    { M_SWTC, "ZOOM IN",          M_Bind_ZoomIn,      'z' },
    { M_SWTC, "ZOOM OUT",         M_Bind_ZoomOut,     'z' },
    { M_SWTC, "MAXIMUM ZOOM OUT", M_Bind_MaxZoom,     'm' },
    { M_SWTC, "FOLLOW MODE",      M_Bind_FollowMode,  'f' },
    { M_SWTC, "ROTATE MODE",      M_Bind_RotateMode,  'r' },
    { M_SWTC, "OVERLAY MODE",     M_Bind_OverlayMode, 'o' },
    { M_SWTC, "TOGGLE GRID",      M_Bind_ToggleGrid,  't' },
    { M_SWTC, "MARK LOCATION",    M_Bind_AddMark,     'm' },
    { M_SWTC, "CLEAR ALL MARKS",  M_Bind_ClearMarks,  'c' },
};

static menu_t ID_Def_Keybinds_4 =
{
    10,
    &ID_Def_Controls,
    ID_Menu_Keybinds_4,
    M_Draw_ID_Keybinds_4,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_ToggleMap (int choice)
{
    M_StartBind(400);  // key_map_toggle
}

static void M_Bind_ZoomIn (int choice)
{
    M_StartBind(401);  // key_map_zoomin
}

static void M_Bind_ZoomOut (int choice)
{
    M_StartBind(402);  // key_map_zoomout
}

static void M_Bind_MaxZoom (int choice)
{
    M_StartBind(403);  // key_map_maxzoom
}

static void M_Bind_FollowMode (int choice)
{
    M_StartBind(404);  // key_map_follow
}

static void M_Bind_RotateMode (int choice)
{
    M_StartBind(405);  // key_map_rotate
}

static void M_Bind_OverlayMode (int choice)
{
    M_StartBind(406);  // key_map_overlay
}

static void M_Bind_ToggleGrid (int choice)
{
    M_StartBind(407);  // key_map_grid
}

static void M_Bind_AddMark (int choice)
{
    M_StartBind(408);  // key_map_mark
}

static void M_Bind_ClearMarks (int choice)
{
    M_StartBind(409);  // key_map_clearmark
}

static void M_Draw_ID_Keybinds_4 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 3;

    M_FillBackground();

    M_WriteTextCentered(9, "AUTOMAP", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_map_toggle);
    M_DrawBindKey(1, 27, key_map_zoomin);
    M_DrawBindKey(2, 36, key_map_zoomout);
    M_DrawBindKey(3, 45, key_map_maxzoom);
    M_DrawBindKey(4, 54, key_map_follow);
    M_DrawBindKey(5, 63, key_map_rotate);
    M_DrawBindKey(6, 72, key_map_overlay);
    M_DrawBindKey(7, 81, key_map_grid);
    M_DrawBindKey(8, 90, key_map_mark);
    M_DrawBindKey(9, 99, key_map_clearmark);

    M_DrawBindFooter("4", true);
}

// -----------------------------------------------------------------------------
// Keybinds 5
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_5[]=
{
    { M_SWTC, "HELP SCREEN",     M_Bind_HelpScreen,     'h' },
    { M_SWTC, "SAVE GAME",       M_Bind_SaveGame,       's' },
    { M_SWTC, "LOAD GAME",       M_Bind_LoadGame,       'l' },
    { M_SWTC, "SOUND VOLUME",    M_Bind_SoundVolume,    's' },
    { M_SWTC, "TOGGLE DETAIL",   M_Bind_ToggleDetail,   't' },
    { M_SWTC, "QUICK SAVE",      M_Bind_QuickSave,      'q' },
    { M_SWTC, "END GAME",        M_Bind_EndGame,        'e' },
    { M_SWTC, "TOGGLE MESSAGES", M_Bind_ToggleMessages, 't' },
    { M_SWTC, "QUICK LOAD",      M_Bind_QuickLoad,      'q' },
    { M_SWTC, "QUIT GAME",       M_Bind_QuitGame,       'q' },
    { M_SWTC, "TOGGLE GAMMA",    M_Bind_ToggleGamma,    't' },
    { M_SWTC, "MULTIPLAYER SPY", M_Bind_MultiplayerSpy, 'm' },
};

static menu_t ID_Def_Keybinds_5 =
{
    12,
    &ID_Def_Controls,
    ID_Menu_Keybinds_5,
    M_Draw_ID_Keybinds_5,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_HelpScreen (int choice)
{
    M_StartBind(500);  // key_menu_help
}

static void M_Bind_SaveGame (int choice)
{
    M_StartBind(501);  // key_menu_save
}

static void M_Bind_LoadGame (int choice)
{
    M_StartBind(502);  // key_menu_load
}

static void M_Bind_SoundVolume (int choice)
{
    M_StartBind(503);  // key_menu_volume
}

static void M_Bind_ToggleDetail (int choice)
{
    M_StartBind(504);  // key_menu_detail
}

static void M_Bind_QuickSave (int choice)
{
    M_StartBind(505);  // key_menu_qsave
}

static void M_Bind_EndGame (int choice)
{
    M_StartBind(506);  // key_menu_endgame
}

static void M_Bind_ToggleMessages (int choice)
{
    M_StartBind(507);  // key_menu_messages
}

static void M_Bind_QuickLoad (int choice)
{
    M_StartBind(508);  // key_menu_qload
}

static void M_Bind_QuitGame (int choice)
{
    M_StartBind(509);  // key_menu_quit
}

static void M_Bind_ToggleGamma (int choice)
{
    M_StartBind(510);  // key_menu_gamma
}

static void M_Bind_MultiplayerSpy (int choice)
{
    M_StartBind(511);  // key_spy
}
static void M_Draw_ID_Keybinds_5 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 4;

    M_FillBackground();

    M_WriteTextCentered(9, "FUNCTION KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_menu_help);
    M_DrawBindKey(1, 27, key_menu_save);
    M_DrawBindKey(2, 36, key_menu_load);
    M_DrawBindKey(3, 45, key_menu_volume);
    M_DrawBindKey(4, 54, key_menu_detail);
    M_DrawBindKey(5, 63, key_menu_qsave);
    M_DrawBindKey(6, 72, key_menu_endgame);
    M_DrawBindKey(7, 81, key_menu_messages);
    M_DrawBindKey(8, 90, key_menu_qload);
    M_DrawBindKey(9, 99, key_menu_quit);
    M_DrawBindKey(10, 108, key_menu_gamma);
    M_DrawBindKey(11, 117, key_spy);

    M_DrawBindFooter("5", true);
}

// -----------------------------------------------------------------------------
// Keybinds 6
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Keybinds_6[]=
{
    { M_SWTC, "PAUSE GAME",                M_Bind_Pause,          'p' },
    { M_SWTC, "SAVE A SCREENSHOT",         M_Bind_SaveScreenshot, 's' },
    { M_SWTC, "DISPLAY LAST MESSAGE",      M_Bind_LastMessage,    'd' },
    { M_SWTC, "FINISH DEMO RECORDING",     M_Bind_FinishDemo,     'f' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "SEND MESSAGE",              M_Bind_SendMessage,    's' },
    { M_SWTC, "- TO PLAYER 1",             M_Bind_ToPlayer1,      '1' },
    { M_SWTC, "- TO PLAYER 2",             M_Bind_ToPlayer2,      '2' },
    { M_SWTC, "- TO PLAYER 3",             M_Bind_ToPlayer3,      '3' },
    { M_SWTC, "- TO PLAYER 4",             M_Bind_ToPlayer4,      '4' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,          'r' },
};

static menu_t ID_Def_Keybinds_6 =
{
    12,
    &ID_Def_Controls,
    ID_Menu_Keybinds_6,
    M_Draw_ID_Keybinds_6,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_Bind_Pause (int choice)
{
    M_StartBind(600);  // key_pause
}

static void M_Bind_SaveScreenshot (int choice)
{
    M_StartBind(601);  // key_menu_screenshot
}

static void M_Bind_LastMessage (int choice)
{
    M_StartBind(602);  // key_message_refresh
}

static void M_Bind_FinishDemo (int choice)
{
    M_StartBind(603);  // key_demo_quit
}

static void M_Bind_SendMessage (int choice)
{
    M_StartBind(604);  // key_multi_msg
}

static void M_Bind_ToPlayer1 (int choice)
{
    M_StartBind(605);  // key_multi_msgplayer1
}

static void M_Bind_ToPlayer2 (int choice)
{
    M_StartBind(606);  // key_multi_msgplayer2
}

static void M_Bind_ToPlayer3 (int choice)
{
    M_StartBind(607);  // key_multi_msgplayer3
}

static void M_Bind_ToPlayer4 (int choice)
{
    M_StartBind(608);  // key_multi_msgplayer4
}

static void M_Bind_ResetResponse (int key)
{
    if (key != key_menu_confirm)
    {
        return;
    }
    
    M_ResetBinds();
}

static void M_Bind_Reset (int choice)
{
    const char *resetwarning =
	    M_StringJoin("RESET KEYBOARD BINDINGS TO DEFAULT VALUES?",
                     "\n\n", PRESSYN, NULL);

    messageFillBG = true;
    M_StartMessage(resetwarning, M_Bind_ResetResponse, true);
}

static void M_Draw_ID_Keybinds_6 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 5;

    M_FillBackground();

    M_WriteTextCentered(9, "SHORTCUT KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 18, key_pause);
    M_DrawBindKey(1, 27, key_menu_screenshot);
    M_DrawBindKey(2, 36, key_message_refresh);
    M_DrawBindKey(3, 45, key_demo_quit);

    M_WriteTextCentered(54, "MULTIPLAYER", cr[CR_YELLOW]);

    M_DrawBindKey(5, 63, key_multi_msg);
    M_DrawBindKey(6, 72, key_multi_msgplayer[0]);
    M_DrawBindKey(7, 81, key_multi_msgplayer[1]);
    M_DrawBindKey(8, 90, key_multi_msgplayer[2]);
    M_DrawBindKey(9, 99, key_multi_msgplayer[3]);

    M_WriteTextCentered(108, "RESET", cr[CR_YELLOW]);

    M_DrawBindFooter("6", true);
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_MouseBinds[]=
{
    { M_SWTC, "FIRE/ATTACK",               M_Bind_M_FireAttack,   'f' },
    { M_SWTC, "MOVE FORWARD",              M_Bind_M_MoveForward,  'm' },
    { M_SWTC, "MOVE BACKWARD",             M_Bind_M_MoveBackward, 'm' },
    { M_SWTC, "USE",                       M_Bind_M_Use,          'u' },
    { M_SWTC, "SPEED ON",                  M_Bind_M_SpeedOn,      's' },
    { M_SWTC, "STRAFE ON",                 M_Bind_M_StrafeOn,     's' },
    { M_SWTC, "STRAFE LEFT",               M_Bind_M_StrafeLeft,   's' },
    { M_SWTC, "STRAFE RIGHT",              M_Bind_M_StrafeRight,  's' },
    { M_SWTC, "PREV WEAPON",               M_Bind_M_PrevWeapon,   'p' },
    { M_SWTC, "NEXT WEAPON",               M_Bind_M_NextWeapon,   'n' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "RESET BINDINGS TO DEFAULT", M_Bind_M_Reset,        'r' },
};

static menu_t ID_Def_MouseBinds =
{
    12,
    &ID_Def_Controls,
    ID_Menu_MouseBinds,
    M_Draw_ID_MouseBinds,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_MouseBinds (int choice)
{
    M_SetupNextMenu (&ID_Def_MouseBinds);
}

static void M_Bind_M_FireAttack (int choice)
{
    M_StartMouseBind(1000);  // mousebfire
}

static void M_Bind_M_MoveForward (int choice)
{
    M_StartMouseBind(1001);  // mousebforward
}

static void M_Bind_M_MoveBackward (int choice)
{
    M_StartMouseBind(1002);  // mousebbackward
}

static void M_Bind_M_Use (int choice)
{
    M_StartMouseBind(1003);  // mousebuse
}

static void M_Bind_M_SpeedOn (int choice)
{
    M_StartMouseBind(1004);  // mousebspeed
}

static void M_Bind_M_StrafeOn (int choice)
{
    M_StartMouseBind(1005);  // mousebstrafe
}

static void M_Bind_M_StrafeLeft (int choice)
{
    M_StartMouseBind(1006);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int choice)
{
    M_StartMouseBind(1007);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int choice)
{
    M_StartMouseBind(1008);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int choice)
{
    M_StartMouseBind(1009);  // mousebnextweapon
}

static void M_Bind_M_ResetResponse (int key)
{
    if (key != key_menu_confirm)
    {
        return;
    }

    M_ResetMouseBinds();
}

static void M_Bind_M_Reset (int choice)
{
    const char *resetwarning =
	    M_StringJoin("RESET MOUSE BINDINGS TO DEFAULT VALUES?",
                     "\n\n", PRESSYN, NULL);

    messageFillBG = true;
    M_StartMessage(resetwarning, M_Bind_M_ResetResponse, true);
}

static void M_Draw_ID_MouseBinds (void)
{
    st_fullupdate = true;

    M_FillBackground();

    M_WriteTextCentered(9, "MOUSE BINDINGS", cr[CR_YELLOW]);

    M_DrawBindButton(0, 18, mousebfire);
    M_DrawBindButton(1, 27, mousebforward);
    M_DrawBindButton(2, 36, mousebbackward);
    M_DrawBindButton(3, 45, mousebuse);
    M_DrawBindButton(4, 54, mousebspeed);
    M_DrawBindButton(5, 63, mousebstrafe);
    M_DrawBindButton(6, 72, mousebstrafeleft);
    M_DrawBindButton(7, 81, mousebstraferight);
    M_DrawBindButton(8, 90, mousebprevweapon);
    M_DrawBindButton(9, 99, mousebnextweapon);

    M_WriteTextCentered(108, "RESET", cr[CR_YELLOW]);

    M_DrawBindFooter(NULL, false);
}

// -----------------------------------------------------------------------------
// Widget settings
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Widgets[]=
{
    { M_MUL2, "COLOR SCHEME",          M_ID_Widget_Colors,    'c' },
    { M_MUL2, "PLACEMENT",             M_ID_Widget_Placement, 'p' },
    { M_MUL2, "ALIGNMENT",             M_ID_Widget_Alignment, 'a' },
    { M_MUL2, "KIS STATS/FRAGS",       M_ID_Widget_KIS,       'k' },
    { M_MUL2, "- STATS FORMAT",        M_ID_Widget_KIS_Format,'s' },
    { M_MUL2, "LEVEL/DM TIMER",        M_ID_Widget_Time,      'l' },
    { M_MUL2, "TOTAL TIME",            M_ID_Widget_TotalTime, 't' },
    { M_MUL2, "LEVEL NAME",            M_ID_Widget_LevelName, 'l' },
    { M_MUL2, "PLAYER COORDS",         M_ID_Widget_Coords,    'p' },
    { M_MUL2, "RENDER COUNTERS",       M_ID_Widget_Render,    'r' },
    { M_MUL2, "TARGET'S HEALTH",       M_ID_Widget_Health,    't' },
};

static menu_t ID_Def_Widgets =
{
    11,
    &ID_Def_Main,
    ID_Menu_Widgets,
    M_Draw_ID_Widgets,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Widgets (int choice)
{
    M_SetupNextMenu (&ID_Def_Widgets);
}

static void M_Draw_ID_Widgets (void)
{
    char str[32];

    M_WriteTextCentered(9, "WIDGETS", cr[CR_YELLOW]);

    // Color scheme
    sprintf(str, widget_scheme == 1 ? "INTER"  :
                 widget_scheme == 2 ? "CRISPY" :
                 widget_scheme == 3 ? "WOOF"   :
                 widget_scheme == 4 ? "DSDA"   : "OFF");
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, widget_scheme ? GLOW_GREEN : GLOW_DARKRED));

    // Placement
    sprintf(str, widget_location ? "TOP" : "BOTTOM");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, GLOW_GREEN));

    // Alignment
    sprintf(str, widget_alignment == 1 ? "STATUS BAR" :
                 widget_alignment == 2 ? "AUTO" : "LEFT");
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, widget_alignment ? GLOW_GREEN : GLOW_DARKRED));

    // K/I/S stats
    sprintf(str, widget_kis == 1 ? "ALWAYS"  :
                 widget_kis == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, widget_kis ? GLOW_GREEN : GLOW_DARKRED));

    // Stats format
    sprintf(str, widget_kis_format == 1 ? "REMAINING" :
                 widget_kis_format == 2 ? "PERCENT" : "RATIO");
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, widget_kis_format ? GLOW_GREEN : GLOW_DARKRED));

    // Level time
    sprintf(str, widget_time == 1 ? "ALWAYS"  :
                 widget_time == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, widget_time ? GLOW_GREEN : GLOW_DARKRED));

    // Total time
    sprintf(str, widget_totaltime == 1 ? "ALWAYS"  :
                 widget_totaltime == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, widget_totaltime ? GLOW_GREEN : GLOW_DARKRED));

    // Level name
    sprintf(str, widget_levelname ? "ALWAYS" : "AUTOMAP");
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, widget_levelname ? GLOW_GREEN : GLOW_DARKRED));

    // Player coords
    sprintf(str, widget_coords == 1 ? "ALWAYS"  :
                 widget_coords == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, widget_coords ? GLOW_GREEN : GLOW_DARKRED));

    // Rendering counters
    sprintf(str, widget_render ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, widget_render ? GLOW_GREEN : GLOW_DARKRED));

    // Target's health
    sprintf(str, widget_health == 1 ? "TOP" :
                 widget_health == 2 ? "TOP+NAME" :
                 widget_health == 3 ? "BOTTOM" :
                 widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, widget_health ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_ID_Widget_Colors (int choice)
{
    widget_scheme = M_INT_Slider(widget_scheme, 0, 4, choice, false);
}

static void M_ID_Widget_Placement (int choice)
{
    widget_location ^= 1;
}

static void M_ID_Widget_Alignment (int choice)
{
    widget_alignment = M_INT_Slider(widget_alignment, 0, 2, choice, false);
}

static void M_ID_Widget_KIS (int choice)
{
    widget_kis = M_INT_Slider(widget_kis, 0, 2, choice, false);
}

static void M_ID_Widget_KIS_Format (int choice)
{
    widget_kis_format = M_INT_Slider(widget_kis_format, 0, 2, choice, false);
}

static void M_ID_Widget_Time (int choice)
{
    widget_time = M_INT_Slider(widget_time, 0, 2, choice, false);
}

static void M_ID_Widget_TotalTime (int choice)
{
    widget_totaltime = M_INT_Slider(widget_totaltime, 0, 2, choice, false);
}

static void M_ID_Widget_LevelName (int choice)
{
    widget_levelname ^= 1;
}

static void M_ID_Widget_Coords (int choice)
{
    widget_coords = M_INT_Slider(widget_coords, 0, 2, choice, false);
}

static void M_ID_Widget_Render (int choice)
{
    widget_render ^= 1;
}

static void M_ID_Widget_Health (int choice)
{
    widget_health = M_INT_Slider(widget_health, 0, 4, choice, false);
}

static void M_ID_Automap_Colors (int choice)
{
    automap_scheme = M_INT_Slider(automap_scheme, 0, 3, choice, false);
}

// -----------------------------------------------------------------------------
// Automap settings
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Automap[]=
{
    { M_MUL2, "COLOR SCHEME",          M_ID_Automap_Colors,   'c' },
    { M_MUL2, "LINE SMOOTHING",        M_ID_Automap_Smooth,   'l' },
    { M_MUL1, "LINE THICKNESS",        M_ID_Automap_Thick,    'l' },
    { M_MUL2, "SQUARE ASPECT RATIO",   M_ID_Automap_Square,   's' },
    { M_MUL2, "MARK SECRET SECTORS",   M_ID_Automap_Secrets,  'm' },
    { M_MUL2, "ROTATE MODE",           M_ID_Automap_Rotate,   'r' },
    { M_MUL2, "OVERLAY MODE",          M_ID_Automap_Overlay,  'o' },
    { M_MUL1, "OVERLAY SHADING LEVEL", M_ID_Automap_Shading,  'o' },
};

static menu_t ID_Def_Automap =
{
    8,
    &ID_Def_Main,
    ID_Menu_Automap,
    M_Draw_ID_Automap,
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Automap (int choice)
{
    M_SetupNextMenu (&ID_Def_Automap);
}

static void M_Draw_ID_Automap (void)
{
    char str[32];
    const char *thickness[] = {
        "DEFAULT","2X","3X","4X","5X","6X","AUTO"
    };

    M_WriteTextCentered(9, "AUTOMAP", cr[CR_YELLOW]);

    // Color scheme
    sprintf(str, automap_scheme == 1 ? "BOOM" :
                 automap_scheme == 2 ? "REMASTER" :
                 automap_scheme == 3 ? "JAGUAR" :
                                       "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, automap_scheme ? GLOW_GREEN : GLOW_DARKRED));

    // Line smoothing
    sprintf(str, automap_smooth ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, automap_smooth ? GLOW_GREEN : GLOW_DARKRED));

    // Line thickness
    sprintf(str, "%s", thickness[automap_thick]);
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, automap_thick ? GLOW_GREEN : GLOW_DARKRED));

    // Square aspect ratio
    sprintf(str, automap_square ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, automap_square ? GLOW_GREEN : GLOW_DARKRED));

    // Mark secret sectors
    sprintf(str, automap_secrets == 1 ? "REVEALED" :
                 automap_secrets == 2 ? "ALWAYS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, automap_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Rotate mode
    sprintf(str, automap_rotate ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, automap_rotate ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay mode
    sprintf(str, automap_overlay ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, automap_overlay ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay shading level
    sprintf(str,"%d", automap_shading);
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, !automap_overlay ? GLOW_DARKRED :
                                 automap_shading ==  0 ? GLOW_RED :
                                 automap_shading == 12 ? GLOW_YELLOW : GLOW_GREEN));
}

static void M_ID_Automap_Smooth (int choice)
{
    automap_smooth ^= 1;
    AM_SetdrawFline();
}

static void M_ID_Automap_Thick (int choice)
{
    automap_thick = M_INT_Slider(automap_thick, 0, 6, choice, false);
}

static void M_ID_Automap_Square (int choice)
{
    automap_square ^= 1;
}

static void M_ID_Automap_Rotate (int choice)
{
    automap_rotate ^= 1;
}

static void M_ID_Automap_Overlay (int choice)
{
    automap_overlay ^= 1;
}

static void M_ID_Automap_Shading (int choice)
{
    automap_shading = M_INT_Slider(automap_shading, 0, 12, choice, true);
}

static void M_ID_Automap_Secrets (int choice)
{
    automap_secrets = M_INT_Slider(automap_secrets, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Gameplay features 1
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Gameplay_1[]=
{
    { M_MUL1, "BRIGHTMAPS",                  M_ID_Brightmaps,        'b' },
    { M_MUL2, "TRANSLUCENCY",                M_ID_Translucency,      't' },
    { M_MUL1, "FAKE CONTRAST",               M_ID_FakeContrast,      'f' },
    { M_MUL1, "DIMINISHED LIGHTING",         M_ID_SmoothLighting,    'd' },
    { M_MUL1, "PALETTE FADING EFFECT",       M_ID_SmoothPalette,     'p' },
    { M_MUL2, "FUZZ EFFECT",                 M_ID_ImprovedFuzz,      'f' },
    { M_MUL1, "COLORED BLOOD AND CORPSES",   M_ID_ColoredBlood,      'c' },
    { M_MUL1, "LIQUIDS ANIMATION",           M_ID_SwirlingLiquids,   'l' },
    { M_MUL1, "INVULNERABILITY AFFECTS SKY", M_ID_InvulSky,          'i' },
    { M_MUL1, "SKY DRAWING MODE",            M_ID_LinearSky,         's' },
    { M_MUL1, "RANDOMLY MIRRORED CORPSES",   M_ID_FlipCorpses,       'r' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "SHAPE",                       M_ID_Crosshair,         's' },
    { M_MUL2, "INDICATION",                  M_ID_CrosshairColor,    'i' },
    { M_MUL2, "", /* < SCROLL PAGES >*/      M_ScrollGameplay,       's' },
};

static menu_t ID_Def_Gameplay_1 =
{
    15,
    &ID_Def_Main,
    ID_Menu_Gameplay_1,
    M_Draw_ID_Gameplay_1,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, true,
};

static void M_Draw_ID_Gameplay_1 (void)
{
    char str[32];
    Gameplay_Cur = 0;

    M_WriteTextCentered(9, "VISUAL", cr[CR_YELLOW]);

    // Brightmaps
    sprintf(str, vis_brightmaps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, vis_brightmaps ? GLOW_GREEN : GLOW_DARKRED));

    // Translucency
    sprintf(str, vis_translucency == 1 ? "ADDITIVE" :
                 vis_translucency == 2 ? "BLENDING" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, vis_translucency ? GLOW_GREEN : GLOW_DARKRED));

    // Fake contrast
    sprintf(str, vis_fake_contrast ? "ORIGINAL" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, vis_fake_contrast ? GLOW_DARKRED : GLOW_GREEN));

    // Diminished lighting
    sprintf(str, vis_smooth_light ? "SMOOTH" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, vis_smooth_light ? GLOW_GREEN : GLOW_DARKRED));

    // Palette fading effect
#ifndef CRISPY_TRUECOLOR
    sprintf(str, "N/A");
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, GLOW_DARKRED));
#else
    sprintf(str, vis_smooth_palette ? "SMOOTH" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, vis_smooth_palette ? GLOW_GREEN : GLOW_DARKRED));
#endif

    // Fuzz effect
    sprintf(str, vis_improved_fuzz == 1 ? "IMPROVED" :
                 vis_improved_fuzz == 2 ? "TRANSLUCENT" :
                 vis_improved_fuzz == 3 ? "GRAYSCALE" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, vis_improved_fuzz ? GLOW_GREEN : GLOW_DARKRED));

    // Colored blood and corpses
    sprintf(str, vis_colored_blood ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, vis_colored_blood ? GLOW_GREEN : GLOW_DARKRED));

    // Liquids animation
    sprintf(str, vis_swirling_liquids ? "SWIRLING" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, vis_swirling_liquids ? GLOW_GREEN : GLOW_DARKRED));

    // Invulnerability affects sky
    sprintf(str, vis_invul_sky ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, vis_invul_sky ? GLOW_GREEN : GLOW_DARKRED));

    // Sky drawing mode
    sprintf(str, vis_linear_sky ? "LINEAR" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, vis_linear_sky ? GLOW_GREEN : GLOW_DARKRED));

    // Randomly mirrored corpses
    sprintf(str, vis_flip_corpses ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, vis_flip_corpses ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(117, "CROSSHAIR", cr[CR_YELLOW]);

    // Crosshair shape
    sprintf(str, xhair_draw == 1 ? "CROSS 1" :
                 xhair_draw == 2 ? "CROSS 2" :
                 xhair_draw == 3 ? "X" :
                 xhair_draw == 4 ? "CIRCLE" :
                 xhair_draw == 5 ? "ANGLE" :
                 xhair_draw == 6 ? "TRIANGLE" :
                 xhair_draw == 7 ? "DOT" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, xhair_draw ? GLOW_GREEN : GLOW_DARKRED));

    // Crosshair indication
    sprintf(str, xhair_color == 1 ? "HEALTH" :
                 xhair_color == 2 ? "TARGET HIGHLIGHT" :
                 xhair_color == 3 ? "TARGET HIGHLIGHT+HEALTH" : "STATIC");
    M_WriteText (M_ItemRightAlign(str), 135, str,
                 M_Item_Glow(13, xhair_color ? GLOW_GREEN : GLOW_DARKRED));

    // Footer
    M_DrawGameplayFooter("1");
}

static void M_ID_Brightmaps (int choice)
{
    vis_brightmaps ^= 1;
}

static void M_ID_Translucency (int choice)
{
    vis_translucency = M_INT_Slider(vis_translucency, 0, 2, choice, false);
}

static void M_ID_FakeContrast (int choice)
{
    vis_fake_contrast ^= 1;
    // [crispy] re-calculate fake contrast
    P_SegLengths(true);
}

static void M_ID_SmoothLightingHook (void)
{
    vis_smooth_light ^= 1;

#ifdef CRISPY_TRUECOLOR
    // [crispy] re-calculate amount of colormaps and light tables
    R_InitColormaps();
#endif
    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
    // [crispy] re-calculate fake contrast
    P_SegLengths(true);
}

static void M_ID_SmoothLighting (int choice)
{
    post_rendering_hook = M_ID_SmoothLightingHook;
}

static void M_ID_SmoothPalette (int choice)
{
#ifndef CRISPY_TRUECOLOR
    return;
#else
    vis_smooth_palette ^= 1;
#endif
}

static void M_ID_ImprovedFuzz (int choice)
{
    vis_improved_fuzz = M_INT_Slider(vis_improved_fuzz, 0, 3, choice, false);
}

static void M_ID_ColoredBlood (int choice)
{
    vis_colored_blood ^= 1;
}

static void M_ID_SwirlingLiquids (int choice)
{
    vis_swirling_liquids ^= 1;
    // [JN] Re-init animation sequences.
    P_InitPicAnims();
}

static void M_ID_InvulSky (int choice)
{
    vis_invul_sky ^= 1;
}

static void M_ID_LinearSky (int choice)
{
    vis_linear_sky ^= 1;
}

static void M_ID_FlipCorpses (int choice)
{
    vis_flip_corpses ^= 1;
}

static void M_ID_Crosshair (int choice)
{
    xhair_draw = M_INT_Slider(xhair_draw, 0, 7, choice, false);
}

static void M_ID_CrosshairColor (int choice)
{
    xhair_color = M_INT_Slider(xhair_color, 0, 3, choice, false);
}

// -----------------------------------------------------------------------------
// Gameplay features 2
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Gameplay_2[]=
{
    { M_MUL1, "COLORED ELEMENTS",              M_ID_ColoredSTBar,      'c' },
    { M_MUL1, "SHOW NEGATIVE HEALTH",          M_ID_NegativeHealth,    's' },
    { M_MUL1, "BLINK MISSING KEYS",            M_ID_BlinkingKeys,      'b' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL1, "SFX ATTENUATION AXISES",        M_ID_ZAxisSfx,          's' },
    { M_MUL1, "PLAY SOUNDS IN FULL LENGTH",    M_ID_FullSounds,        'p' },
    { M_MUL1, "SOUND OF CRUSHED CORPSE",       M_ID_SndOfCrushedCorpse,'s' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL1, "CORPSES SLIDING FROM LEDGES",   M_ID_Torque,            'c' },
    { M_MUL1, "POINT-BLANK SSG TEAR MONSTERS", M_ID_SSGTearMonsters,   'p' },
    { M_MUL1, "ITEMS ARE TOSSED WHEN DROPPED", M_ID_TossDrop,          'i' },
    { M_MUL2, "FLOATING POWERUPS AMPLITUDE",   M_ID_FloatingPowerups,  'f' },
    { M_MUL2, "WEAPON ATTACK ALIGNMENT",       M_ID_WeaponAlignment,   'w' },
    { M_MUL1, "IMITATE PLAYER'S BREATHING",    M_ID_Breathing,         'i' },
    { M_MUL2, "", /* < SCROLL PAGES >*/        M_ScrollGameplay,       's' },
};

static menu_t ID_Def_Gameplay_2 =
{
    15,
    &ID_Def_Main,
    ID_Menu_Gameplay_2,
    M_Draw_ID_Gameplay_2,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, true,
};

static void M_Draw_ID_Gameplay_2 (void)
{
    char str[32];
    Gameplay_Cur = 1;

    M_WriteTextCentered(9, "STATUS BAR", cr[CR_YELLOW]);

    // Colored elements
    sprintf(str, st_colored_stbar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, st_colored_stbar ? GLOW_GREEN : GLOW_DARKRED));

    // Show negative health
    sprintf(str, st_negative_health ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, st_negative_health ? GLOW_GREEN : GLOW_DARKRED));

    // Blink missing keys
    sprintf(str, st_blinking_keys ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, st_blinking_keys ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(45, "AUDIBLE", cr[CR_YELLOW]);

    // Sfx attenuation axises
    sprintf(str, aud_z_axis_sfx ? "X/Y/Z" : "X/Y");
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, aud_z_axis_sfx ? GLOW_GREEN : GLOW_DARKRED));

    // Play sounds in full length
    sprintf(str, aud_full_sounds ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, aud_full_sounds ? GLOW_GREEN : GLOW_DARKRED));

    // Sound of crushed corpse
    sprintf(str, aud_crushed_corpse ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, aud_crushed_corpse ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(81, "PHYSICAL", cr[CR_YELLOW]);

    // Corpses sliding from ledges
    sprintf(str, phys_torque ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, phys_torque ? GLOW_GREEN : GLOW_DARKRED));

    // Point-blank SSG tear monsters
    sprintf(str, phys_ssg_tear_monsters ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, phys_ssg_tear_monsters ? GLOW_GREEN : GLOW_DARKRED));

    // Items are tossed when dropped
    sprintf(str, phys_toss_drop ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, phys_toss_drop ? GLOW_GREEN : GLOW_DARKRED));

    // Floating powerups amplitude
    sprintf(str, phys_floating_powerups == 1 ? "LOW" :
                 phys_floating_powerups == 2 ? "MIDDLE" :
                 phys_floating_powerups == 3 ? "HIGH" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 117, str,
                 M_Item_Glow(11, phys_floating_powerups ? GLOW_GREEN : GLOW_DARKRED));

    // Weapon attack alignment
    sprintf(str, phys_weapon_alignment == 1 ? "BOBBING" :
                 phys_weapon_alignment == 2 ? "CENTERED" : "ORIGINAL");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, phys_weapon_alignment ? GLOW_GREEN : GLOW_DARKRED));

    // Imitate player's breathing
    sprintf(str, phys_breathing ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 135, str,
                 M_Item_Glow(13, phys_breathing ? GLOW_GREEN : GLOW_DARKRED));

    // Footer
    M_DrawGameplayFooter("2");
}

static void M_ID_ColoredSTBar (int choice)
{
    st_colored_stbar ^= 1;
}

static void M_ID_NegativeHealth (int choice)
{
    st_negative_health ^= 1;
}

static void M_ID_BlinkingKeys (int choice)
{
    st_blinking_keys ^= 1;
}

static void M_ID_ZAxisSfx (int choice)
{
    aud_z_axis_sfx ^= 1;
}

static void M_ID_FullSounds (int choice)
{
    aud_full_sounds ^= 1;

    // [crispy] weapon sound sources
    for (int i = 0 ; i < MAXPLAYERS ; i++)
    {
        if (playeringame[i])
        {
            players[i].so = Crispy_PlayerSO(i);
        }
    }
}

static void M_ID_SndOfCrushedCorpse (int choice)
{
    aud_crushed_corpse ^= 1;
}

static void M_ID_Torque (int choice)
{
    phys_torque ^= 1;
}

static void M_ID_SSGTearMonsters(int choice)
{
    phys_ssg_tear_monsters ^= 1;
}

static void M_ID_TossDrop(int choice)
{
    phys_toss_drop ^= 1;
}

static void M_ID_FloatingPowerups(int choice)
{
    phys_floating_powerups = M_INT_Slider(phys_floating_powerups, 0, 3, choice, false);
}

static void M_ID_WeaponAlignment (int choice)
{
    phys_weapon_alignment = M_INT_Slider(phys_weapon_alignment, 0, 2, choice, false);
    pspr_interp = false;
}

static void M_ID_Breathing (int choice)
{
    phys_breathing ^= 1;
}

// -----------------------------------------------------------------------------
// Gameplay features 3
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Gameplay_3[]=
{
    { M_MUL2, "DEFAULT SKILL LEVEL",      M_ID_DefaulSkill,       'd' },
    { M_MUL2, "REPORT REVEALED SECRETS",  M_ID_RevealedSecrets,   'r' },
    { M_MUL1, "FLIP LEVELS HORIZONTALLY", M_ID_FlipLevels,        'f' },
    { M_MUL2, "ON DEATH ACTION",          M_ID_OnDeathAction,     'o' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "SHOW DEMO TIMER",          M_ID_DemoTimer,         's' },
    { M_MUL1, "TIMER DIRECTION",          M_ID_TimerDirection,    't' },
    { M_MUL1, "SHOW PROGRESS BAR",        M_ID_ProgressBar,       's' },
    { M_MUL1, "PLAY INTERNAL DEMOS",      M_ID_InternalDemos,     'p' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL1, "PISTOL START GAME MODE",   M_ID_PistolStart,       'p' },
    { M_MUL1, "IMPROVED HIT DETECTION",   M_ID_BlockmapFix,       'i' },
    { M_MUL2, "VERTICAL AIMING",          M_ID_VerticalAiming,    'v' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "", /* < SCROLL PAGES >*/   M_ScrollGameplay,       's' },
};

static menu_t ID_Def_Gameplay_3 =
{
    15,
    &ID_Def_Main,
    ID_Menu_Gameplay_3,
    M_Draw_ID_Gameplay_3,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, true,
};

static void M_Draw_ID_Gameplay_3 (void)
{
    char str[32];
    Gameplay_Cur = 2;

    M_WriteTextCentered(9, "GAMEPLAY", cr[CR_YELLOW]);

    // Default skill level
    DEH_snprintf(str, sizeof(str), "%s", DefSkillName[gp_default_skill]);
    M_WriteText (M_ItemRightAlign(str), 18, str, 
                 M_Item_Glow(0, DefSkillColor(gp_default_skill)));

    // Report revealed secrets
    sprintf(str, gp_revealed_secrets == 1 ? "TOP" :
                 gp_revealed_secrets == 2 ? "CENTERED" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, gp_revealed_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Flip levels horizontally
    sprintf(str, gp_flip_levels ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, gp_flip_levels ? GLOW_GREEN : GLOW_DARKRED));

    // On death action
    sprintf(str, gp_death_use_action == 1 ? "LAST SAVE" :
                 gp_death_use_action == 2 ? "NOTHING" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, gp_death_use_action ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(54, "DEMOS", cr[CR_YELLOW]);

    // Demo timer
    sprintf(str, demo_timer == 1 ? "PLAYBACK" : 
                 demo_timer == 2 ? "RECORDING" : 
                 demo_timer == 3 ? "ALWAYS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Timer direction
    sprintf(str, demo_timerdir ? "BACKWARD" : "FORWARD");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Progress bar
    sprintf(str, demo_bar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, demo_bar ? GLOW_GREEN : GLOW_DARKRED));

    // Play internal demos
    sprintf(str, demo_internal ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, demo_internal ? GLOW_DARKRED : GLOW_GREEN));

    M_WriteTextCentered(99, "COMPATIBILITY-BREAKING", cr[CR_YELLOW]);

    // Pistol start game mode
    sprintf(str, compat_pistol_start ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str,
                 M_Item_Glow(10, compat_pistol_start ? GLOW_GREEN : GLOW_DARKRED));

    // Improved hit detection
    sprintf(str, compat_blockmap_fix ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 117, str,
                 M_Item_Glow(11, compat_blockmap_fix ? GLOW_GREEN : GLOW_DARKRED));

    // Vertical aiming
    sprintf(str, compat_vertical_aiming == 1 ? "DIRECT" :
                 compat_vertical_aiming == 2 ? "BOTH" : "AUTOAIM");
    M_WriteText (M_ItemRightAlign(str), 126, str,
                 M_Item_Glow(12, compat_vertical_aiming ? GLOW_GREEN : GLOW_DARKRED));

    // Footer
    M_DrawGameplayFooter("3");
}

static void M_ID_DefaulSkill (int choice)
{
    gp_default_skill = M_INT_Slider(gp_default_skill, 0, 4, choice, false);
    // [JN] Set new skill in skill level menu.
    NewDef.lastOn = gp_default_skill;
}

static void M_ID_PistolStart (int choice)
{
    compat_pistol_start ^= 1;
}

static void M_ID_RevealedSecrets (int choice)
{
    gp_revealed_secrets = M_INT_Slider(gp_revealed_secrets, 0, 2, choice, false);
}

static void M_ID_FlipLevels (int choice)
{
    gp_flip_levels ^= 1;

    // Redraw game screen
    R_ExecuteSetViewSize();

    // Update stereo separation
    S_UpdateStereoSeparation();
}

static void M_ID_OnDeathAction (int choice)
{
    gp_death_use_action = M_INT_Slider(gp_death_use_action, 0, 2, choice, false);
}

static void M_ID_DemoTimer (int choice)
{
    demo_timer = M_INT_Slider(demo_timer, 0, 3, choice, false);
}

static void M_ID_TimerDirection (int choice)
{
    demo_timerdir ^= 1;
}

static void M_ID_ProgressBar (int choice)
{
    demo_bar ^= 1;

    // [JN] Redraw status bar to possibly 
    // clean up remainings of progress bar.
    st_fullupdate = true;
}

static void M_ID_InternalDemos (int choice)
{
    demo_internal ^= 1;
}

static void M_ID_BlockmapFix (int choice)
{
    compat_blockmap_fix ^= 1;
}

static void M_ID_VerticalAiming (int choice)
{
    compat_vertical_aiming = M_INT_Slider(compat_vertical_aiming, 0, 2, choice, false);
}

static void M_ScrollGameplay (int choice)
{
    if (choice) // Scroll right
    {
             if (currentMenu == &ID_Def_Gameplay_1) { M_SetupNextMenu(&ID_Def_Gameplay_2); }
        else if (currentMenu == &ID_Def_Gameplay_2) { M_SetupNextMenu(&ID_Def_Gameplay_3); }
        else if (currentMenu == &ID_Def_Gameplay_3) { M_SetupNextMenu(&ID_Def_Gameplay_1); }
    }
    else
    {           // Scroll left
             if (currentMenu == &ID_Def_Gameplay_1) { M_SetupNextMenu(&ID_Def_Gameplay_3); }
        else if (currentMenu == &ID_Def_Gameplay_2) { M_SetupNextMenu(&ID_Def_Gameplay_1); }
        else if (currentMenu == &ID_Def_Gameplay_3) { M_SetupNextMenu(&ID_Def_Gameplay_2); }
    }
    itemOn = 14;
}

static void M_DrawGameplayFooter (char *pagenum)
{
    char str[32];
    
    M_WriteText(ID_MENU_LEFTOFFSET_BIG, 144, "< SCROLL PAGES >",
                M_Item_Glow(14, GLOW_LIGHTGRAY));

    M_snprintf(str, 32, "%s", M_StringJoin("PAGE ", pagenum, "/3", NULL));
    M_WriteText(M_ItemRightAlign(str), 144, str,
                M_Item_Glow(14, GLOW_GRAY));
}

// -----------------------------------------------------------------------------
// Miscellaneous features
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Misc[]=
{
    { M_MUL1, "INVULNERABILITY EFFECT",     M_ID_Misc_A11yInvul,      'i' },
    { M_MUL2, "PALETTE FLASH EFFECTS",      M_ID_Misc_A11yPalFlash,   'p' },
    { M_MUL1, "MOVEMENT BOBBING",           M_ID_Misc_A11yMoveBob,    'm' },
    { M_MUL1, "WEAPON BOBBING",             M_ID_Misc_A11yWeaponBob,  'w' },
    { M_MUL2, "COLORBLIND FILTER",          M_ID_Misc_A11yColorblind, 'c' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "AUTOLOAD WAD FILES",         M_ID_Misc_AutoloadWAD,    'a' },
    { M_MUL2, "AUTOLOAD DEH FILES",         M_ID_Misc_AutoloadDEH,    'a' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "HIGHLIGHTING EFFECT",        M_ID_Misc_Hightlight,     'h' },
    { M_MUL1, "ESC KEY BEHAVIOUR",          M_ID_Misc_MenuEscKey,     'e' },
};

static menu_t ID_Def_Misc =
{
    11,
    &ID_Def_Main,
    ID_Menu_Misc,
    M_Draw_ID_Misc,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Misc (int choice)
{
    M_SetupNextMenu (&ID_Def_Misc);
}

static void M_Draw_ID_Misc (void)
{
    char str[32];
    const char *bobpercent[] = {
        "OFF","5%","10%","15%","20%","25%","30%","35%","40%","45%","50%",
        "55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"
    };
    const char *colorblind_name[] = {
        "NONE","PROTANOPIA","PROTANOMALY","DEUTERANOPIA","DEUTERANOMALY",
        "TRITANOPIA","TRITANOMALY","ACHROMATOPSIA","ACHROMATOMALY"
    };

    M_WriteTextCentered(9, "ACCESSIBILITY", cr[CR_YELLOW]);

    // Invulnerability effect
    sprintf(str, a11y_invul ? "GRAYSCALE" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 18, str,
                 M_Item_Glow(0, a11y_invul ? GLOW_GREEN : GLOW_DARKRED));

    // Palette flash effects
    sprintf(str, a11y_pal_flash == 1 ? "HALVED" :
                 a11y_pal_flash == 2 ? "QUARTERED" :
                 a11y_pal_flash == 3 ? "OFF" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, a11y_pal_flash == 1 ? GLOW_YELLOW :
                                a11y_pal_flash == 2 ? GLOW_ORANGE : 
                                a11y_pal_flash == 3 ? GLOW_RED : GLOW_DARKRED));

    // Movement bobbing
    sprintf(str, "%s", bobpercent[a11y_move_bob]);
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, a11y_move_bob == 20 ? GLOW_DARKRED :
                                a11y_move_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Weapon bobbing
    sprintf(str, "%s", bobpercent[a11y_weapon_bob]);
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, a11y_weapon_bob == 20 ? GLOW_DARKRED :
                                a11y_weapon_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Colorblind filter
    sprintf(str, "%s", colorblind_name[a11y_colorblind]);
    M_WriteText (M_ItemRightAlign(str), 54, str,
                 M_Item_Glow(4, a11y_colorblind ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(63, "AUTOLOAD", cr[CR_YELLOW]);

    // Autoload WAD files
    sprintf(str, autoload_wad == 1 ? "IWAD ONLY" :
                 autoload_wad == 2 ? "IWAD AND PWAD" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, autoload_wad == 1 ? GLOW_YELLOW :
                                autoload_wad == 2 ? GLOW_GREEN : GLOW_DARKRED));

    // Autoload DEH patches
    sprintf(str, autoload_deh == 1 ? "IWAD ONLY" :
                 autoload_deh == 2 ? "IWAD AND PWAD" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, autoload_deh == 1 ? GLOW_YELLOW :
                                autoload_deh == 2 ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(90, "MENU SETTINGS", cr[CR_YELLOW]);

    // Highlighting effect
    sprintf(str, menu_highlight == 1 ? "ANIMATED" :
                 menu_highlight == 2 ? "STATIC" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 99, str,
                 M_Item_Glow(9, menu_highlight == 1 ? GLOW_GREEN :
                                menu_highlight == 2 ? GLOW_YELLOW : GLOW_DARKRED));

    // ESC key behaviour
    sprintf(str, menu_esc_key ? "GO BACK" : "CLOSE MENU" );
    M_WriteText (M_ItemRightAlign(str), 107, str,
                 M_Item_Glow(10, menu_esc_key ? GLOW_GREEN : GLOW_DARKRED));

    // [PN] Added explanations for colorblind filters
    if (itemOn == 4)
    {
        const char *colorblind_hint[] = {
            "","RED-BLIND","RED-WEAK","GREEN-BLIND","GREEN-WEAK",
            "BLUE-BLIND","BLUE-WEAK","MONOCHROMACY","BLUE CONE MONOCHROMACY"
        };

        M_WriteTextCentered(135, colorblind_hint[a11y_colorblind], cr[CR_LIGHTGRAY_DARK1]);
    }
    // [PN] Added explanations for autoload variables
    if (itemOn == 6 || itemOn == 7)
    {
        const char *off = "AUTOLOAD IS DISABLED";
        const char *first_line = "AUTOLOAD AND FOLDER CREATION";
        const char *second_line1 = "ONLY ALLOWED FOR IWAD FILES";
        const char *second_line2 = "ALLOWED FOR BOTH IWAD AND PWAD FILES";
        const int   autoload_option = (itemOn == 6) ? autoload_wad : autoload_deh;

        switch (autoload_option)
        {
            case 1:
                M_WriteTextCentered(135, first_line, cr[CR_LIGHTGRAY_DARK1]);
                M_WriteTextCentered(144, second_line1, cr[CR_LIGHTGRAY_DARK1]);
                break;

            case 2:
                M_WriteTextCentered(135, first_line, cr[CR_LIGHTGRAY_DARK1]);
                M_WriteTextCentered(144, second_line2, cr[CR_LIGHTGRAY_DARK1]);
                break;

            default:
                M_WriteTextCentered(135, off, cr[CR_LIGHTGRAY_DARK1]);
                break;            
        }
    }
}

static void M_ID_Misc_A11yInvul (int choice)
{
    a11y_invul ^= 1;
    // [JN] Recalculate colormaps to apply the appropriate invulnerability effect.
    R_InitColormaps();
}

static void M_ID_Misc_A11yPalFlash (int choice)
{
    a11y_pal_flash = M_INT_Slider(a11y_pal_flash, 0, 3, choice, false);
    I_SetPalette (st_palette);
}

static void M_ID_Misc_A11yMoveBob (int choice)
{
    a11y_move_bob = M_INT_Slider(a11y_move_bob, 0, 20, choice, true);
}

static void M_ID_Misc_A11yWeaponBob (int choice)
{
    a11y_weapon_bob = M_INT_Slider(a11y_weapon_bob, 0, 20, choice, true);
}

static void M_ID_Misc_A11yColorblindHook (void)
{
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    AM_Init();
    st_fullupdate = true;
    I_SetColorPanes(false);
    I_SetPalette(st_palette);
#endif
}

static void M_ID_Misc_A11yColorblind (int choice)
{
    a11y_colorblind = M_INT_Slider(a11y_colorblind, 0, 8, choice, false);
    post_rendering_hook = M_ID_Misc_A11yColorblindHook;
}

static void M_ID_Misc_AutoloadWAD (int choice)
{
    autoload_wad = M_INT_Slider(autoload_wad, 0, 2, choice, false);
}

static void M_ID_Misc_AutoloadDEH (int choice)
{
    autoload_deh = M_INT_Slider(autoload_deh, 0, 2, choice, false);
}

static void M_ID_Misc_Hightlight (int choice)
{
    menu_highlight = M_INT_Slider(menu_highlight, 0, 2, choice, false);
}

static void M_ID_Misc_MenuEscKey (int choice)
{
    menu_esc_key ^= 1;
}

// -----------------------------------------------------------------------------
// Level select 1
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Level_1[]=
{
    { M_MUL2, "SKILL LEVEL",        M_ID_LevelSkill,     's' },
    { M_MUL1, "EPISODE",            M_ID_LevelEpisode,   'e' },
    { M_MUL1, "MAP",                M_ID_LevelMap,       'm' },
    { M_SKIP, "", 0, '\0' },  // PLAYER
    { M_MUL1, "HEALTH",             M_ID_LevelHealth,    'h' },
    { M_MUL1, "ARMOR",              M_ID_LevelArmor,     'a' },
    { M_MUL1, "ARMOR TYPE",         M_ID_LevelArmorType, 'a' },
    { M_SKIP, "", 0, '\0' },  // WEAPONS
    { M_MUL1, "CHAINSAW",           M_ID_LevelChainsaw,  'c' },
    { M_MUL1, "SHOTGUN",            M_ID_LevelShotgun,   's' },
    { M_MUL1, "SUPER SHOTGUN",      M_ID_LevelSSG,       's' },
    { M_MUL1, "CHAINGUN",           M_ID_LevelChaingun,  'c' },
    { M_MUL1, "ROCKET LAUNCHER",    M_ID_LevelRLauncher, 'r' },
    { M_MUL1, "PLASMA RIFLE",       M_ID_LevelPlasmagun, 'p' },
    { M_MUL1, "BFG 9000",           M_ID_LevelBFG9000,   'b' },
    { M_SKIP, "", 0, '\0' },  // WEAPONS
    { M_MUL1, "", /* NEXT PAGE > */ M_ScrollLevel,       'n' },
    { M_SWTC, "", /* START GAME  */ G_DoSelectiveGame,   's' }
};


static menu_t ID_Def_Level_1 =
{
    18,
    &ID_Def_Main,
    ID_Menu_Level_1,
    M_Draw_ID_Level_1,
    ID_MENU_LEFTOFFSET_LEVEL, 25,
    0,
    true, false, true,
};

static void M_Choose_ID_Level (int choice)
{
    M_SetupNextMenu(&ID_Def_Level_1);
}

static void M_Draw_ID_Level_1 (void)
{
    char str[32];

    st_fullupdate = true;

    M_FillBackground();
    
    M_WriteTextCentered(16, "LEVEL SELECT", cr[CR_YELLOW]);

    // Skill level
    sprintf(str, "%s", DefSkillName[level_select[0]]);
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(0, DefSkillColor(level_select[0])));

    // Episode
    sprintf(str, gamemode == shareware ? "1" :
                 gamemode == commercial ? "N/A" : "%d",
                 level_select[1]);
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(1, gamemode == shareware || gamemode == commercial ?
                             GLOW_DARKRED : GLOW_RED));

    // Map
    sprintf(str, "%d", level_select[2]);
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(2, GLOW_RED));

    M_WriteTextCentered(52, "PLAYER", cr[CR_YELLOW]);

    // Health
    sprintf(str, "%d", level_select[3]);
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(4, level_select[3] >  100 ? GLOW_BLUE   :
                                level_select[3] >=  67 ? GLOW_GREEN  : 
                                level_select[3] >=  34 ? GLOW_YELLOW : 
                                                         GLOW_RED));

    // Armor
    sprintf(str, "%d", level_select[4]);
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(5, level_select[4] == 0 ? GLOW_RED :
                                level_select[5] == 1 ? GLOW_GREEN : GLOW_BLUE));

    // Armor type
    sprintf(str, "%d", level_select[5]);
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(6, level_select[5] == 1 ? GLOW_GREEN : GLOW_BLUE));

    M_WriteTextCentered(88, "WEAPONS", cr[CR_YELLOW]);

    // Chainsaw
    sprintf(str, level_select[6] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(8, level_select[6] ? GLOW_GREEN : GLOW_RED));

    // Shotgun
    sprintf(str, level_select[7] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(9, level_select[7] ? GLOW_GREEN : GLOW_RED));

    // Super Shotgun
    sprintf(str, gamemode != commercial ? "N/A" : 
                 level_select[8] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(10, gamemode != commercial ? GLOW_DARKRED :
                                 level_select[8] ? GLOW_GREEN : GLOW_RED));

    // Chaingun
    sprintf(str, level_select[9] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(11, level_select[9] ? GLOW_GREEN : GLOW_RED));

    // Rocket Launcher
    sprintf(str, level_select[10] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 133, str,
                 M_Item_Glow(12, level_select[10] ? GLOW_GREEN : GLOW_RED));

    // Plasma Rifle
    sprintf(str, gamemode == shareware ? "N/A" :
                 level_select[11] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 142, str,
                 M_Item_Glow(13, gamemode == shareware ? GLOW_DARKRED :
                                 level_select[11] ? GLOW_GREEN : GLOW_RED));

    // BFG 9000
    sprintf(str, gamemode == shareware ? "N/A" :
                 level_select[12] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 151, str,
                 M_Item_Glow(14, gamemode == shareware ? GLOW_DARKRED :
                                 level_select[12] ? GLOW_GREEN : GLOW_RED));

    M_WriteText (ID_MENU_LEFTOFFSET_LEVEL, 169, "NEXT PAGE >",
                 M_Item_Glow(16, GLOW_LIGHTGRAY));
    M_WriteText (ID_MENU_LEFTOFFSET_LEVEL, 178, "START GAME",
                 M_Item_Glow(17, GLOW_LIGHTGRAY));
}

static void M_ID_LevelSkill (int choice)
{
    level_select[0] = M_INT_Slider(level_select[0], 0, 4, choice, false);
}

static void M_ID_LevelEpisode (int choice)
{
    if (gamemode == shareware || gamemode == commercial)
    {
        return;
    }

    level_select[1] = M_INT_Slider(level_select[1], 1,
                                   sigil ? 5 :
                                   gamemode == retail ? 4 : 3, choice, false);
}

static void M_ID_LevelMap (int choice)
{
    level_select[2] = M_INT_Slider(level_select[2], 1,
                                   gamemode == commercial ? 32 : 9, choice, false);
}

static void M_ID_LevelHealth (int choice)
{
    level_select[3] = M_INT_Slider(level_select[3], 1, 200, choice, false);
}

static void M_ID_LevelArmor (int choice)
{
    level_select[4] = M_INT_Slider(level_select[4], 0, 200, choice, false);
}

static void M_ID_LevelArmorType (int choice)
{
    level_select[5] = M_INT_Slider(level_select[5], 1, 2, choice, false);
}

static void M_ID_LevelChainsaw (int choice)
{
    level_select[6] ^= 1;
}

static void M_ID_LevelShotgun (int choice)
{
    level_select[7] ^= 1;
}

static void M_ID_LevelSSG (int choice)
{
    if (gamemode != commercial)
    {
        return;
    }
    level_select[8] ^= 1;
}

static void M_ID_LevelChaingun (int choice)
{
    level_select[9] ^= 1;
}

static void M_ID_LevelRLauncher (int choice)
{
    level_select[10] ^= 1;
}

static void M_ID_LevelPlasmagun (int choice)
{
    level_select[11] ^= 1;
}

static void M_ID_LevelBFG9000 (int choice)
{
    level_select[12] ^= 1;
}

// -----------------------------------------------------------------------------
// Level select 2
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Level_2[]=
{
    { M_MUL1, "BACKPACK",            M_ID_LevelBackpack,      'b' },
    { M_MUL1, "BULLETS",             M_ID_LevelBullets,       'b' },
    { M_MUL1, "SHELLS",              M_ID_LevelShells,        's' },
    { M_MUL1, "ROCKETS",             M_ID_LevelRockets,       'r' },
    { M_MUL1, "CELLS",               M_ID_LevelCells,         'c' },
    { M_SKIP, "", 0, '\0' },  // KEYS
    { M_MUL1, "BLUE KEYCARD",        M_ID_LevelBlueKeycard,   'b' },
    { M_MUL1, "YELLOW KEYCARD",      M_ID_LevelYellowKeycard, 'y' },
    { M_MUL1, "RED KEYCARD",         M_ID_LevelRedKeycard,    'r' },
    { M_MUL1, "BLUE SKULL KEY",      M_ID_LevelBlueSkull,     'b' },
    { M_MUL1, "YELLOW SKULL KEY",    M_ID_LevelYellowSkull,   'y' },
    { M_MUL1, "RED SKULL KEY",       M_ID_LevelRedSkull,      'r' },
    { M_SKIP, "", 0, '\0' },  // EXTRA
    { M_MUL1, "FAST MONSTERS",       M_ID_LevelFastMonsters,  'f' },
    { M_MUL1, "RESPAWNING MONSTERS", M_ID_LevelRespMonsters,  'r' },
    { M_SKIP, "", 0, '\0' },  // WEAPONS
    { M_MUL1, "", /* < PREV PAGE */  M_ScrollLevel,           'p' },
    { M_SWTC, "", /* START GAME  */  G_DoSelectiveGame,       's' }
};


static menu_t ID_Def_Level_2 =
{
    18,
    &ID_Def_Main,
    ID_Menu_Level_2,
    M_Draw_ID_Level_2,
    ID_MENU_LEFTOFFSET_LEVEL, 25,
    0,
    true, false, true,
};

static void M_Draw_ID_Level_2 (void)
{
    char str[32];

    st_fullupdate = true;

    M_FillBackground();
    
    M_WriteTextCentered(16, "AMMO", cr[CR_YELLOW]);

    // Backpack
    sprintf(str, level_select[13] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(0, level_select[13] ? GLOW_GREEN : GLOW_RED));

    // Bullets
    sprintf(str, "%d", level_select[14]);
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(1, level_select[14]  < 200 / 4 ? GLOW_RED    :
                                level_select[14]  < 200 / 2 ? GLOW_YELLOW :
                                level_select[14] <= 200     ? GLOW_GREEN  : GLOW_BLUE));

    // Shells
    sprintf(str, "%d", level_select[15]);
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(2, level_select[15]  < 50 / 4 ? GLOW_RED    :
                                level_select[15]  < 50 / 2 ? GLOW_YELLOW :
                                level_select[15] <= 50     ? GLOW_GREEN  : GLOW_BLUE));

    // Rockets
    sprintf(str, "%d", level_select[16]);
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(3, level_select[16]  < 50 / 4 ? GLOW_RED    :
                                level_select[16]  < 50 / 2 ? GLOW_YELLOW :
                                level_select[16] <= 50     ? GLOW_GREEN  : GLOW_BLUE));

    // Cells
    sprintf(str, "%d", level_select[17]);
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(4, level_select[17]  < 300 / 4 ? GLOW_RED    :
                                level_select[17]  < 300 / 2 ? GLOW_YELLOW :
                                level_select[17] <= 300     ? GLOW_GREEN  : GLOW_BLUE));

    M_WriteTextCentered(70, "KEYS", cr[CR_YELLOW]);

    // Blue keycard
    sprintf(str, level_select[18] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(6, level_select[18] ? GLOW_GREEN : GLOW_RED));

    // Yellow keycard
    sprintf(str, level_select[19] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(7, level_select[19] ? GLOW_GREEN : GLOW_RED));

    // Red keycard
    sprintf(str, level_select[20] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(8, level_select[20] ? GLOW_GREEN : GLOW_RED));

    // Blue skull key
    sprintf(str, level_select[21] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(9, level_select[21] ? GLOW_GREEN : GLOW_RED));

    // Yellow skull key
    sprintf(str, level_select[22] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(10, level_select[22] ? GLOW_GREEN : GLOW_RED));

    // Red skull key
    sprintf(str, level_select[23] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(11, level_select[23] ? GLOW_GREEN : GLOW_RED));

    M_WriteTextCentered(133, "EXTRA", cr[CR_YELLOW]);

    // Fast monsters
    sprintf(str, level_select[24] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 142, str,
                 M_Item_Glow(13, level_select[24] ? GLOW_GREEN : GLOW_RED));

    // Respawning monsters
    sprintf(str, level_select[25] ? "YES" : "NO");
    M_WriteText (M_ItemRightAlign(str), 151, str,
                 M_Item_Glow(14, level_select[25] ? GLOW_GREEN : GLOW_RED));

    M_WriteText (ID_MENU_LEFTOFFSET_LEVEL, 169, "< PREV PAGE",
                 M_Item_Glow(16, GLOW_LIGHTGRAY));
    M_WriteText (ID_MENU_LEFTOFFSET_LEVEL, 178, "START GAME",
                 M_Item_Glow(17, GLOW_LIGHTGRAY));
}

static void M_ID_LevelBackpack (int choice)
{
    level_select[13] ^= 1;

    // Possibly reduce ammo if no backpack is given.
    if (level_select[13] == 0)
    {
        if (level_select[14] > 200) level_select[14] = 200;
        if (level_select[15] > 50) level_select[15] = 50;
        if (level_select[16] > 50) level_select[16] = 50;
        if (level_select[17] > 300) level_select[17] = 300;
    }

}

static void M_ID_LevelBullets (int choice)
{
    level_select[14] = M_INT_Slider(level_select[14], 0,
                                    level_select[13] ? 400 : 200, choice, false);
}

static void M_ID_LevelShells (int choice)
{
    level_select[15] = M_INT_Slider(level_select[15], 0,
                                    level_select[13] ? 100 : 50, choice, false);
}

static void M_ID_LevelRockets (int choice)
{
    level_select[16] = M_INT_Slider(level_select[16], 0,
                                    level_select[13] ? 100 : 50, choice, false);
}

static void M_ID_LevelCells (int choice)
{
    level_select[17] = M_INT_Slider(level_select[17], 0,
                                    level_select[13] ? 600 : 300, choice, false);
}

static void M_ID_LevelBlueKeycard (int choice)
{
    level_select[18] ^= 1;
}

static void M_ID_LevelYellowKeycard (int choice)
{
    level_select[19] ^= 1;
}

static void M_ID_LevelRedKeycard (int choice)
{
    level_select[20] ^= 1;
}

static void M_ID_LevelBlueSkull (int choice)
{
    level_select[21] ^= 1;
}

static void M_ID_LevelYellowSkull (int choice)
{
    level_select[22] ^= 1;
}

static void M_ID_LevelRedSkull (int choice)
{
    level_select[23] ^= 1;
}

static void M_ID_LevelFastMonsters (int choice)
{
    level_select[24] ^= 1;
}

static void M_ID_LevelRespMonsters (int choice)
{
    level_select[25] ^= 1;
}

static void M_ScrollLevel (int choice)
{
         if (currentMenu == &ID_Def_Level_1) { ID_Def_Level_2.lastOn = 16; M_SetupNextMenu(&ID_Def_Level_2); }
    else if (currentMenu == &ID_Def_Level_2) { ID_Def_Level_1.lastOn = 16; M_SetupNextMenu(&ID_Def_Level_1); }
}

// -----------------------------------------------------------------------------
// Reset settings
// -----------------------------------------------------------------------------

static void M_ID_ApplyResetHook (void)
{
    //
    // Video options
    //

#ifdef CRISPY_TRUECOLOR
    vid_truecolor = 0;
#endif
    vid_resolution = 2;
    vid_widescreen = 0;
    vid_fullscreen_exclusive = 0;
    vid_uncapped_fps = 0;
    vid_fpslimit = 0;
    vid_vsync = 1;
    vid_showfps = 0;
    vid_smooth_scaling = 0;
    // Miscellaneous
    vid_screenwipe = 1;
    vid_diskicon = 1;
    vid_endoom = 0;

    //
    // Display options
    //

    dp_screen_size = 10;
    dp_detail_level = 0;
    vid_gamma = 10;
    vid_fov = 90;
    dp_menu_shading = 0;
    dp_level_brightness = 0;
    // Color settings
    vid_saturation = 100;
    vid_contrast = 1.000000;
    vid_r_intensity = 1.000000;
    vid_g_intensity = 1.000000;
    vid_b_intensity = 1.000000;
    // Messages settings
    msg_show = 1;
    msg_alignment = 0;
    msg_text_shadows = 0;
    msg_local_time = 0;

    //
    // Sound options
    //

    sfxVolume = 8;
    musicVolume = 8;
    snd_sfxdevice = 3;
    snd_musicdevice = 3;
    snd_dmxoption = "-opl3";
    snd_monosfx = 0;
    snd_pitchshift = 0;
    snd_channels = 8;
    snd_mute_inactive = 0;
    snd_remaster_ost = 1;

    //
    // Widgets and automap
    //

    widget_scheme = 1;
    widget_location = 0;
    widget_alignment = 0;
    widget_kis = 0;
    widget_kis_format = 0;
    widget_time = 0;
    widget_totaltime = 0;
    widget_levelname = 0;
    widget_coords = 0;
    widget_render = 0;
    widget_health = 0;
    // Automap
    automap_scheme = 0;
    automap_smooth = 0;
    automap_thick = 0;
    automap_square = 0;
    automap_secrets = 0;
    automap_rotate = 0;
    automap_overlay = 0;
    automap_shading = 0;

    //
    // Gameplay features
    //

    // Visual
    vis_brightmaps = 0;
    vis_translucency = 0;
    vis_fake_contrast = 1;
    vis_smooth_light = 0;
    vis_smooth_palette = 0;
    vis_improved_fuzz = 0;
    vis_colored_blood = 0;
    vis_swirling_liquids = 0;
    vis_invul_sky = 0;
    vis_linear_sky = 0;
    vis_flip_corpses = 0;

    // Crosshair
    xhair_draw = 0;
    xhair_color = 0;

    // Status bar
    st_colored_stbar = 0;
    st_negative_health = 0;
    st_blinking_keys = 0;

    // Audible
    aud_z_axis_sfx = 0;
    aud_full_sounds = 0;
    aud_crushed_corpse = 0;
    aud_exit_sounds = 0;

    // Physical
    phys_torque = 0;
    phys_ssg_tear_monsters = 0;
    phys_toss_drop = 0;
    phys_floating_powerups = 0;
    phys_weapon_alignment = 0;
    phys_breathing = 0;

    // Gameplay
    gp_default_skill = 2;
    gp_revealed_secrets = 0;
    gp_flip_levels = 0;
    gp_death_use_action = 0;

    // Demos
    demo_timer = 0;
    demo_timerdir = 0;
    demo_bar = 0;
    demo_internal = 1;

    // Compatibility-breaking
    compat_pistol_start = 0;
    compat_blockmap_fix = 0;
    compat_vertical_aiming = 0;

    // Restart graphical systems
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    R_InitColormaps();
    R_InitLightTables();
    R_SetViewSize(dp_screen_size, dp_detail_level);
    R_ExecuteSetViewSize();
    P_SegLengths(true);
    P_InitPicAnims();
    I_ToggleVsync();
#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    I_SetPalette(st_palette);
#endif
    R_FillBackScreen();
    V_EnableLoadingDisk();
    ST_InitElementsBackground();
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
    if (vid_fullscreen)
    {
        I_UpdateExclusiveFullScreen();
    }

    // Restart audio systems
    S_StopMusic();
    S_ChangeSFXSystem();
    I_ShutdownSound();
    I_InitSound(doom);
    I_PrecacheSounds(S_sfx, NUMSFX);
    S_SetSfxVolume(sfxVolume * 8);
    S_SetMusicVolume(musicVolume * 8);
    S_ChangeMusic(current_mus_num, usergame);
    S_UpdateStereoSeparation();

    // [crispy] weapon sound sources
    for (int i = 0 ; i < MAXPLAYERS ; i++)
    {
        if (playeringame[i])
        {
            players[i].so = Crispy_PlayerSO(i);
        }
    }    
}

static void M_ID_ApplyReset (int key)
{
    if (key != key_menu_confirm)
    {
        return;
    }

    post_rendering_hook = M_ID_ApplyResetHook;
}

static void M_Choose_ID_Reset (int choice)
{
	M_StartMessage(DEH_String(ID_RESET), M_ID_ApplyReset, true);
}


// =============================================================================


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
    FILE   *handle;
    int     i;
    char    name[256];

    for (i = 0;i < load_end;i++)
    {
        int retval;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = M_fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }
}


// [FG] support up to 8 pages of savegames
static void M_DrawSaveLoadBottomLine (void)
{
    char pagestr[16];

    if (savepage > 0)
    {
        M_WriteText(LoadDef.x, 151, "< PGUP", cr[CR_MENU_DARK2]);
    }
    if (savepage < savepage_max)
    {
        M_WriteText(LoadDef.x+(SAVESTRINGSIZE-6)*8, 151, "PGDN >", cr[CR_MENU_DARK2]);
    }

    M_snprintf(pagestr, sizeof(pagestr), "PAGE %d/%d", savepage + 1, savepage_max + 1);
    
    M_WriteTextCentered(151, pagestr, cr[CR_MENU_DARK1]);

    // [JN] Print "modified" (or created initially) time of savegame file.
    if (LoadMenu[itemOn].status)
    {
        struct stat filestat;
        char filedate[32];

        if (M_stat(P_SaveGameFile(itemOn), &filestat) == 0)
        {
// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
        strftime(filedate, sizeof(filedate), "%x %X", localtime(&filestat.st_mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        M_WriteTextCentered(160, filedate, cr[CR_MENU_DARK1]);
        }
    }
}


//
// M_LoadGame & Cie.
//
static void M_DrawLoad(void)
{
    int             i;
	
    V_DrawShadowedPatchOptional(72, 7, 0, W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE));

    for (i = 0;i < load_end; i++)
    {
	// [JN] Highlight selected item (itemOn == i) or apply fading effect.
	dp_translation = M_SaveLoad_Glow(itemOn == i, 0, saveload_border);
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], 
	            M_SaveLoad_Glow(itemOn == i, currentMenu->menuitems[i].tics, saveload_text));
    }

    M_DrawSaveLoadBottomLine();
}



//
// Draw border for the savegame description
//
static void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
	
    V_DrawShadowedPatchOptional(x - 8, y, 0, W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE));
	
    for (i = 0;i < 24;i++)
    {
	V_DrawShadowedPatchOptional(x, y, 0, W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
	x += 8;
    }

    V_DrawShadowedPatchOptional(x, y, 0, W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE));
}



//
// User wants to load this game
//
static void M_LoadSelect(int choice)
{
    char    name[256];
	
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));

    G_LoadGame (name);
    M_ClearMenus ();

    // [crispy] allow quickload before quicksave
    if (quickSaveSlot == -2)
	quickSaveSlot = choice;
}

//
// Selected from DOOM menu
//
static void M_LoadGame (int choice)
{
    // [crispy] allow loading game while multiplayer demo playback
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(LOADNET),NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
static void M_DrawSave(void)
{
    int             i;
	
    V_DrawShadowedPatchOptional(72, 7, 0, W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	// [JN] Highlight selected item (itemOn == i) or apply fading effect.
	dp_translation = M_SaveLoad_Glow(itemOn == i, 0, saveload_border);
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i],
	            M_SaveLoad_Glow(itemOn == i, currentMenu->menuitems[i].tics, saveload_text));
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	// [JN] Highlight "_" cursor, line is always active while typing.
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_", M_SaveLoad_Glow(0, 0, saveload_cursor));
	// [JN] Forcefully hide the mouse cursor while typing.
	menu_mouse_allow = false;
    }

    M_DrawSaveLoadBottomLine();
}

//
// M_Responder calls this when user is finished
//
static void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // [JN] Generate save name. ExMx for Doom 1, MAPxx for Doom 2.
    if (gamemode == commercial)
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE - 1,
                   "MAP%02d", gamemap);
    }
    else
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE - 1,
                   "E%dM%d", gameepisode, gamemap);
    }
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

// [crispy] override savegame name if it already starts with a map identifier
static boolean StartsWithMapIdentifier (char *str)
{
    M_ForceUppercase(str);

    if (strlen(str) >= 4 &&
        str[0] == 'E' && isdigit(str[1]) &&
        str[2] == 'M' && isdigit(str[3]))
    {
        return true;
    }

    if (strlen(str) >= 5 &&
        str[0] == 'M' && str[1] == 'A' && str[2] == 'P' &&
        isdigit(str[3]) && isdigit(str[4]))
    {
        return true;
    }

    return false;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // [crispy] load the last game you saved
    LoadDef.lastOn = choice;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING) ||
        // [crispy] override savegame name if it already starts with a map identifier
        StartsWithMapIdentifier(savegamestrings[choice]))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave || true) // [crispy] always prefill empty savegame slot names
        {
            SetDefaultSaveName(choice);
        }
    }
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
static void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
static void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }

	// [JN] Do not show confirmation dialog,
	// do a quick save immediately.
	M_DoSave(quickSaveSlot);
}



//
// M_QuickLoad
//
static void M_QuickLoad(void)
{
    // [crispy] allow quickloading game while multiplayer demo playback
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(QLOADNET),NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	// [crispy] allow quickload before quicksave
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&LoadDef);
	quickSaveSlot = -2;
	return;
    }

	// [JN] Do not show confirmation dialog,
	// do a quick load immediately.
	M_LoadSelect(quickSaveSlot);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
static void M_DrawReadThis1(void)
{
    st_fullupdate = true;

    V_DrawPatchFullScreen(W_CacheLumpName(DEH_String("HELP2"), PU_CACHE), false);
}



//
// Read This Menus - optional second page.
//
static void M_DrawReadThis2(void)
{
    st_fullupdate = true;

    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

    V_DrawPatchFullScreen(W_CacheLumpName(DEH_String("HELP1"), PU_CACHE), false);
}

static void M_DrawReadThisCommercial(void)
{
    st_fullupdate = true;

    V_DrawPatchFullScreen(W_CacheLumpName(DEH_String("HELP"), PU_CACHE), false);
}


//
// Change Sfx & Music volumes
//
static void M_DrawSound(void)
{
    char str[8];

    V_DrawShadowedPatchOptional(60, 38, 0, W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE));

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (sfx_vol + 1), 16, sfxVolume, 0);
    M_ID_HandleSliderMouseControl(86, 80, 132, &sfxVolume, false, 0, 15);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (226, 83, str, M_Item_Glow(0, sfxVolume ? GLOW_UNCOLORED : GLOW_DARKRED));

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (music_vol + 1), 16, musicVolume, 2);
    M_ID_HandleSliderMouseControl(86, 112, 132, &musicVolume, false, 0, 15);
    sprintf(str,"%d", musicVolume);
    M_WriteText (226, 115, str, M_Item_Glow(2, musicVolume ? GLOW_UNCOLORED : GLOW_DARKRED));
}

static void M_SfxVol (int choice)
{
    sfxVolume = M_INT_Slider(sfxVolume, 0, 15, choice, true);
    S_SetSfxVolume(sfxVolume * 8);
}

static void M_MusicVol (int choice)
{
    musicVolume = M_INT_Slider(musicVolume, 0, 15, choice, true);
    S_SetMusicVolume(musicVolume * 8);
}




//
// M_DrawMainMenu
//
static void M_DrawMainMenu(void)
{
    V_DrawPatch(94, 2, W_CacheLumpName(DEH_String("M_DOOM"), PU_CACHE));
}




//
// M_NewGame
//
static void M_DrawNewGame(void)
{
    V_DrawShadowedPatchOptional(96, 14, 0, W_CacheLumpName(DEH_String("M_NEWG"), PU_CACHE));
    V_DrawShadowedPatchOptional(54, 38, 0, W_CacheLumpName(DEH_String("M_SKILL"), PU_CACHE));
}

static void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(NEWGAME),NULL,false);
	return;
    }
	
    // Chex Quest disabled the episode select screen, as did Doom II.

    if (gamemode == commercial || gameversion == exe_chex)
	M_SetupNextMenu(&NewDef);
    else
	M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
static int epi;

static void M_DrawEpisode(void)
{
    V_DrawShadowedPatchOptional(54, 38, 0, W_CacheLumpName(DEH_String("M_EPISOD"), PU_CACHE));
}

static void M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
	return;
		
    G_DeferedInitNew((skill_t)nightmare,epi+1,1);
    M_ClearMenus ();
}

static void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
	return;
    }
	
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

static void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(DEH_String(SWSTRING),NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    epi = choice;
    // [crispy] have Sigil II loaded but not Sigil
    if (epi == 4 && sigil2 && !sigil)
        epi = 5;
    M_SetupNextMenu(&NewDef);
}


//
//      Toggle messages on/off
//
static void M_ChangeMessages(int choice)
{
    msg_show ^= 1;
	
	CT_SetMessage(&players[consoleplayer],
                   DEH_String(msg_show ? MSGON : MSGOFF), true, NULL);
}


//
// M_EndGame
//
static void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageCenteredTics = 1;
    players[consoleplayer].messageCentered = NULL;
    st_palette = 0;
    D_StartTitle ();
}

static void M_EndGame(int choice)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(DEH_String(NETEND),NULL,false);
	return;
    }
	
    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
static void M_ReadThis(int choice)
{
    M_SetupNextMenu(&ReadDef1);
}

static void M_ReadThis2(int choice)
{
    M_SetupNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int choice)
{
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//

static void M_QuitResponse(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
    {
        return;
    }
    else
    {
        // [PN] Define quit sounds arrays locally
        static const int quitsounds[8] = {
            sfx_pldeth, sfx_dmpain, sfx_popain, sfx_slop,
            sfx_telept, sfx_posit1, sfx_posit3, sfx_sgtatk
        };
        static const int quitsounds2[8] = {
            sfx_vilact, sfx_getpow, sfx_boscub, sfx_slop,
            sfx_skeswg, sfx_kntdth, sfx_bspact, sfx_sgtatk
        };

        // [JN] Play exit sound optionally.
        if (!netgame && aud_exit_sounds)
        {
            const int *sound_array = (gamemode == commercial) ? quitsounds2 : quitsounds;
            S_StartSound(NULL, sound_array[(gametic >> 2) & 7]);
            I_WaitVBL(105);
        }

        I_Quit ();
    }
}

static const char *M_SelectEndMessage(void)
{
    // [PN] Choose the message array based on the game version
    const char **endmsg = (logical_gamemission == doom) ?
                           doom1_endmsg : doom2_endmsg;

    return endmsg[gametic % NUM_QUITMESSAGES];
}

static void M_QuitDOOM(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY,
                 DEH_String(M_SelectEndMessage()));

    M_StartMessage(endstring,M_QuitResponse,true);
}

static void M_ChangeDetail(int choice)
{
    dp_detail_level ^= 1;

    R_SetViewSize (dp_screen_size, dp_detail_level);
    CT_SetMessage(&players[consoleplayer],
                   DEH_String(dp_detail_level ? DETAILLO : DETAILHI), false, NULL);
}

static void M_SizeDisplay (int choice)
{
    // [PN] Simplified logic for adjusting screen size
    if (choice == 0 && dp_screen_size > 3)
    {
        dp_screen_size--;
        // [JN] Skip wide status bar in non-wide screen mode
        if (!vid_widescreen)
        {
            if (dp_screen_size == 15)
            {
                dp_screen_size -= 3;
            }
            else
            if (dp_screen_size == 13 || dp_screen_size == 12)
            {
                dp_screen_size -= 2;
            }
        }
    }
    else
    if (choice == 1 && dp_screen_size < 15)
    {
        dp_screen_size++;
        // [JN] Skip wide status bar in non-wide screen mode
        if (!vid_widescreen && dp_screen_size == 13)
        {
            dp_screen_size += 2;
        }
    }

    // [PN] Update view size after adjustment
    R_SetViewSize(dp_screen_size, dp_detail_level);
}




//
//      Menu Functions
//
static void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot,
  int	itemPos )
{
    int		xx;
    int		i;

    // [JN] Highlight active slider and gem.
    dp_translation = M_SaveLoad_Glow(itemPos == itemOn, 0, saveload_slider);

    xx = x;
    V_DrawShadowedPatchOptional(xx, y, 0, W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawShadowedPatchOptional(xx, y, 0, W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE));
	xx += 8;
    }
    V_DrawShadowedPatchOptional(xx, y, 0, W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE));

    // [crispy] do not crash anymore if value exceeds thermometer range
    if (thermDot >= thermWidth)
    {
        thermDot = thermWidth - 1;
    }

    V_DrawPatch((x + 8) + thermDot * 8, y, W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE));
    dp_translation = NULL;
}

static void
M_StartMessage
( const char*	string,
  void		(*routine)(int),
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}

// -----------------------------------------------------------------------------
// M_StringWidth
// [PN] Calculate the total width of a string based on the hu_font characters.
// Adds spacing for characters not found in the font or outside the valid range.
// -----------------------------------------------------------------------------

const int M_StringWidth(const char* string)
{
    int w = 0;
    int c;
    const char* ch = string;

    while (*ch)
    {
        c = toupper(*ch++) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            // Add default spacing for invalid characters
            w += 4;
        }
        else
        {
            // Add character width
            w += SHORT(hu_font[c]->width);
        }
    }

    return w;
}

// -----------------------------------------------------------------------------
// M_StringHeight
// [PN] Calculate the total height of a string based on the number of lines
// (accounts for newline characters) and the height of the hu_font characters.
// -----------------------------------------------------------------------------

static int M_StringHeight (const char *string)
{
    const int height = SHORT(hu_font[0]->height);  // Base height of a single line
    int lines = 1;  // At least one line by default

    // Count the number of newline characters
    for (const char *ch = string; *ch != '\0'; ++ch)
    {
        if (*ch == '\n')
            ++lines;
    }

    return height * lines;
}

// -----------------------------------------------------------------------------
// M_WriteText
// Write a string using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteText (int x, int y, const char *string, byte *table)
{
    const char*	ch;
    int w, c, cx, cy;

    ch = string;
    cx = x;
    cy = y;

    dp_translation = table;

    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        V_DrawShadowedPatchOptional(cx, cy, 0, hu_font[c]);
        cx+=w;
    }

    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// M_WriteTextCentered
// [JN] Write a centered string using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteTextCentered (const int y, const char *string, byte *table)
{
    const char *ch;
    const int width = M_StringWidth(string);
    int w, c, cx, cy;

    ch = string;
    cx = ORIGWIDTH/2-width/2;
    cy = y;

    dp_translation = table;

    // find width
    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = c - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            continue;
        }

        w = SHORT (hu_font[c]->width);
    }

    // draw it
    cx = ORIGWIDTH/2-width/2;
    ch = string;

    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx+w > ORIGWIDTH)
        {
            break;
        }
        
        V_DrawShadowedPatchOptional(cx, cy, 0, hu_font[c]);
        cx += w;
    }
    
    dp_translation = NULL;
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.

static boolean IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

// -----------------------------------------------------------------------------
// G_ReloadLevel
// [crispy] reload current level / go to next level
// adapted from prboom-plus/src/e6y.c:369-449
// -----------------------------------------------------------------------------

static int G_ReloadLevel (void)
{
    int result = false;

    if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION)
    {
        // [crispy] restart demos from the map they were started
        if (demorecording)
        {
            gamemap = startmap;
        }
        G_DeferedInitNew(gameskill, gameepisode, gamemap);
        result = true;
    }

    return result;
}

static int G_GotoNextLevel (void)
{
    static byte doom_next[6][9] = {
    {12, 13, 19, 15, 16, 17, 18, 21, 14},
    {22, 23, 24, 25, 29, 27, 28, 31, 26},
    {32, 33, 34, 35, 36, 39, 38, 41, 37},
    {42, 49, 44, 45, 46, 47, 48, 51, 43},
    {52, 53, 54, 55, 56, 59, 58, 11, 57},
    {62, 63, 69, 65, 66, 67, 68, 11, 64},
    };

    static byte doom2_next[33] = {
     2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 31, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 1,
    32, 16, 3
    };

    int changed = false;

    if (gamemode == commercial)
    {
        if (W_CheckNumForName("map31") < 0)
        {
            doom2_next[14] = 16;
        }

        if (gamemission == pack_hacx)
        {
            doom2_next[30] = 16;
            doom2_next[20] = 1;
        }

    }
    else
    {
        if (gamemode == shareware)
        {
            doom_next[0][7] = 11;
        }

        if (gamemode == registered)
        {
            doom_next[2][7] = 11;
        }
        
        if (!sigil && !sigil2)
        {
            doom_next[3][7] = 11;
        }
        else if (!sigil && sigil2)
        {
            doom_next[3][7] = 61;
        }
        else if (sigil && !sigil2)
        {
            doom_next[4][7] = 11;
        }

        if (gameversion == exe_chex)
        {
            doom_next[0][2] = 14;
            doom_next[0][4] = 11;
        }
    }

    if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION)
    {
        int epsd, map;

        if (gamemode == commercial)
        {
            epsd = gameepisode;
            map = doom2_next[gamemap-1];
        }
        else
        {
            const int level = doom_next[gameepisode - 1][gamemap - 1];

            epsd = level / 10;
            map = level % 10;
        }

        G_DeferedInitNew(gameskill, epsd, map);
        changed = true;
    }

    return changed;
}


//
// CONTROL PANEL
//


//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             key;
    int             i;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    // [JN] Disable menu left/right controls by mouse movement.
    /*
    static  int     mousex = 0;
    static  int     lastx = 0;
    */

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (ev->type == ev_quit
        || (ev->type == ev_keydown
        && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit

        if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
        {
            M_QuitResponse(key_menu_confirm);
        }
        else
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitDOOM(0);
        }

        return true;
    }

    // key is the key pressed, ch is the actual character typed

    ch = 0;
    key = -1;

    if (ev->type == ev_joystick)
    {
        // Simulate key presses from joystick events to interact with the menu.

        if (ev->data3 < 0)
        {
            key = key_menu_up;
            joywait = I_GetTime() + 5;
        }
        else if (ev->data3 > 0)
        {
            key = key_menu_down;
            joywait = I_GetTime() + 5;
        }

        if (ev->data2 < 0)
        {
            key = key_menu_left;
            joywait = I_GetTime() + 2;
        }
        else if (ev->data2 > 0)
        {
            key = key_menu_right;
            joywait = I_GetTime() + 2;
        }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

        if (JOY_BUTTON_PRESSED(joybfire))
        {
            // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_confirm;
            }
            // Simulate pressing "Enter" when we are supplying a save slot name
            else if (saveStringEnter)
            {
                key = KEY_ENTER;
            }
            else
            {
                // if selecting a save slot via joypad, set a flag
                if (currentMenu == &SaveDef)
                {
                    joypadSave = true;
                }
                key = key_menu_forward;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybuse))
        {
            // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_abort;
            }
            // If user was entering a save name, back out
            else if (saveStringEnter)
            {
                key = KEY_ESCAPE;
            }
            else
            {
                key = key_menu_back;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybmenu))
        {
            key = key_menu_activate;
            joywait = I_GetTime() + 5;
        }
    }
    else
    {
        const int item_status = currentMenu->menuitems[itemOn].status;

        // [JN] Shows the mouse cursor when moved.
        if (ev->data2 || ev->data3)
        {
            menu_mouse_allow = true;
            menu_mouse_allow_click = false;
        }

        if (ev->type == ev_mouse && mousewait < I_GetTime())
        {
            // [crispy] mouse_novert disables controlling the menus with the mouse
            // [JN] Not needed, as menu is fully controllable by mouse wheel and buttons.
            /*
            if (!mouse_novert)
            {
                mousey += ev->data3;
            }
            */
            if (mousey < lasty - 30)
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 5;
                mousey = lasty -= 30;
            }
            else if (mousey > lasty + 30)
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 5;
                mousey = lasty += 30;
            }

            // [JN] Disable menu left/right controls by mouse movement.
            /*
            mousex += ev->data2;
            if (mousex < lastx-30)
            {
                key = key_menu_left;
                mousewait = I_GetTime() + 5;
                mousex = lastx -= 30;
            }
            else if (mousex > lastx+30)
            {
                key = key_menu_right;
                mousewait = I_GetTime() + 5;
                mousex = lastx += 30;
            }
            */

            // [JN] Handle mouse bindings before going any farther.
            // Catch only button pressing events, i.e. ev->data1.
            if (MouseIsBinding && ev->data1)
            {
                M_CheckMouseBind(SDL_mouseButton);
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 5;
                return true;
            }

            if (ev->data1 & 1)
            {
                if (menuactive && item_status == STS_SLDR)
                {
                    // [JN] Allow repetitive on sliders to move it while mouse movement.
                    menu_mouse_allow_click = true;
                }
                else
                if (!ev->data2 && !ev->data3)   // [JN] Do not consider movement as pressing.
                {
                    if (!menuactive && !usergame)
                    {
                        M_StartControlPanel();  // [JN] Open the main menu if the game is not active.
                    }
                    else
                    if (messageToPrint && messageNeedsInput)
                    {
                        key = key_menu_confirm; // [JN] Confirm by left mouse button.
                    }
                    else
                    {
                        key = key_menu_forward;
                    }
                    mousewait = I_GetTime() + 1;
                }
            }

            if (ev->data1 & 2
            && !ev->data2 && !ev->data3)    // [JN] Do not consider movement as pressing.
            {
                if (!menuactive && !usergame)
                {
                    M_StartControlPanel();  // [JN] Open the main menu if the game is not active.
                }
                else
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_abort;   // [JN] Cancel by right mouse button.
                }
                else
                if (saveStringEnter)
                {
                    key = key_menu_abort;
                    saveStringEnter = 0;
                    M_ReadSaveStrings();    // [JN] Reread save strings after cancelation.
                }
                else
                {
                    key = key_menu_back;
                }
                mousewait = I_GetTime() + 1;
            }

            // [JN] Scrolls through menu item values or navigates between pages.
            if (ev->data1 & (1 << 4) && menuactive)  // Wheel down
            {
                if (item_status > 1)
                {
                    // Scroll menu item backward normally, or forward for STS_MUL2
                    currentMenu->menuitems[itemOn].routine(item_status != STS_MUL2 ? 0 : 1);
                    S_StartSound(NULL, sfx_stnmov);
                }
                else
                if (currentMenu->ScrollAR && !saveStringEnter && !KbdIsBinding)
                {
                    M_ScrollPages(1);
                }
                mousewait = I_GetTime();
            }
            else
            if (ev->data1 & (1 << 3) && menuactive)  // Wheel up
            {
                if (item_status > 1)
                {
                    // Scroll menu item forward normally, or backward for STS_MUL2
                    currentMenu->menuitems[itemOn].routine(item_status != STS_MUL2 ? 1 : 0);
                    S_StartSound(NULL, sfx_stnmov);
                }
                else
                if (currentMenu->ScrollAR && !saveStringEnter && !KbdIsBinding)
                {
                    M_ScrollPages(0);
                }
                mousewait = I_GetTime();
            }
        }
        else
        {
            if (ev->type == ev_keydown)
            {
                key = ev->data1;
                ch = ev->data2;
                // [JN] Hide mouse cursor by pressing a key.
                menu_mouse_allow = false;
            }
        }
    }

    if (key == -1)
        return false;

    // Save Game string input
    if (saveStringEnter)
    {
        switch(key)
        {
            case KEY_BACKSPACE:
                if (saveCharIndex > 0)
                {
                    saveCharIndex--;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;

            case KEY_ESCAPE:
                saveStringEnter = 0;
                I_StopTextInput();
                M_StringCopy(savegamestrings[saveSlot], saveOldString, SAVESTRINGSIZE);
                break;

            case KEY_ENTER:
                saveStringEnter = 0;
                I_StopTextInput();
                if (savegamestrings[saveSlot][0])
                    M_DoSave(saveSlot);
                break;

            default:
                // Savegame name entry. This is complicated.
                // Vanilla has a bug where the shift key is ignored when entering
                // a savegame name. If vanilla_keyboard_mapping is on, we want
                // to emulate this bug by using ev->data1. But if it's turned off,
                // it implies the user doesn't care about Vanilla emulation:
                // instead, use ev->data3 which gives the fully-translated and
                // modified key input.

                if (ev->type != ev_keydown)
                {
                    break;
                }
                if (vanilla_keyboard_mapping)
                {
                    ch = ev->data1;
                }
                else
                {
                    ch = ev->data3;
                }

                ch = toupper(ch);

                if (ch != ' '
                && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE))
                {
                    break;
                }

                if (ch >= 32 && ch <= 127
                && saveCharIndex < SAVESTRINGSIZE - 1
                && M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE - 2) * 8)
                {
                    savegamestrings[saveSlot][saveCharIndex++] = ch;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (messageNeedsInput)
        {
            // [JN] Allow to exclusevely confirm quit game by pressing F10 again.
            if (key == key_menu_quit && messageRoutine == M_QuitResponse)
            {
                I_Quit ();
                return true;
            }

            if (key != ' ' && key != KEY_ESCAPE
            &&  key != key_menu_confirm && key != key_menu_abort
            // [JN] Allow to confirm nightmare, end game and quit by pressing Enter.
            &&  key != key_menu_forward)
            {
                return false;
            }
        }

        menuactive = messageLastMenuActive;
        messageFillBG = false;
        messageToPrint = 0;
        if (messageRoutine)
            messageRoutine(key);

        // [JN] Do not close Save/Load menu after deleting a savegame.
        if (currentMenu != &SaveDef && currentMenu != &LoadDef
        // [JN] Do not close Options menu after pressing "N" in End Game.
        &&  currentMenu != &ID_Def_Main
        // [JN] Do not close bindings menu after keyboard/mouse binds reset.
        &&  currentMenu != &ID_Def_Keybinds_6 && currentMenu != &ID_Def_MouseBinds)
            menuactive = false;
        S_StartSound(NULL, sfx_swtchx);
        return true;
    }

    // [JN] Handle keyboard bindings:
    if (KbdIsBinding)
    {
        if (ev->type == ev_mouse)
        {
            // Reject mouse buttons, but keep binding active.
            return false;
        }

        if (key == KEY_ESCAPE)
        {
            // Pressing ESC will cancel binding and leave key unchanged.
            keyToBind = 0;
            KbdIsBinding = false;
            return false;
        }
        else
        {
            M_CheckBind(key);
            M_DoBind(keyToBind, key);
            keyToBind = 0;
            KbdIsBinding = false;
            return true;
        }
    }

    // [JN] Disallow keyboard pressing and stop binding
    // while mouse binding is active.
    if (MouseIsBinding)
    {
        if (ev->type != ev_mouse)
        {
            btnToBind = 0;
            MouseIsBinding = false;
            return false;
        }
    }

    if ((devparm && key == key_menu_help)
    || (key != 0 && key == key_menu_screenshot))
    {
        S_StartSound(NULL, sfx_tink);       // [JN] Add audible feedback
        G_ScreenShot ();
        return true;
    }

    // F-Keys
    if (!menuactive && !chatmodeon)
    {
        if (key == key_menu_decscreen)      // Screen size down
        {
            if (automapactive)
                return false;
            M_SizeDisplay(0);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_incscreen) // Screen size up
        {
            if (automapactive)
                return false;
            M_SizeDisplay(1);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_help)     // Help key
        {
            M_StartControlPanel ();

            if (gameversion >= exe_ultimate)
              currentMenu = &ReadDef2;
            else
              currentMenu = &ReadDef1;

            itemOn = 0;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_save)     // Save
        {
            M_StartControlPanel();
            S_StartSound(NULL, sfx_swtchn);
            M_SaveGame(0);
            return true;
        }
        else if (key == key_menu_load)     // Load
        {
            M_StartControlPanel();
            S_StartSound(NULL, sfx_swtchn);
            M_LoadGame(0);
            return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
            M_StartControlPanel();
            currentMenu = &SoundDef;
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_detail)   // Detail toggle
        {
            M_ChangeDetail(0);
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qsave)    // Quicksave
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuickSave();
            return true;
        }
        else if (key == key_menu_endgame)  // End game
        {
            S_StartSound(NULL, sfx_swtchn);
            M_EndGame(0);
            return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
            M_ChangeMessages(0);
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qload)    // Quickload
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuickLoad();
            return true;
        }
        else if (key == key_menu_quit)     // Quit DOOM
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitDOOM(0);
            return true;
        }
        // [crispy] those two can be considered as shortcuts for the IDCLEV cheat
        // and should be treated as such, i.e. add "if (!netgame)"
        // [JN] Hovewer, allow while multiplayer demos.
        else if ((!netgame || netdemo) && key != 0 && key == key_reloadlevel)
        {
            if (demoplayback)
            {
                if (demowarp)
                {
                    // [JN] Enable screen render back before replaying.
                    nodrawers = false;
                    singletics = false;
                }
                // [JN] Replay demo lump or file.
                G_DoPlayDemo();
                return true;
            }
            else
            if (G_ReloadLevel())
            return true;
        }
        else if ((!netgame || netdemo) && key != 0 && key == key_nextlevel)
        {
            if (demoplayback)
            {
                // [JN] Go to next level.
                demo_gotonextlvl = true;
                G_DemoGoToNextLevel(true);
                return true;
            }
            else
            if (G_GotoNextLevel())
            return true;
        }
    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma)    // gamma toggle
    {
        vid_gamma = M_INT_Slider(vid_gamma, 0, MAXGAMMA-1, 1 /*right*/, false);
        CT_SetMessage(&players[consoleplayer], DEH_String(gammalvls[vid_gamma][0]), false, NULL);
#ifndef CRISPY_TRUECOLOR
        I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
        {
            I_SetPalette(st_palette);
            R_InitColormaps();
            R_FillBackScreen();
            st_fullupdate = true;
        }
#endif
        return true;
    }

    // Pop-up menu?
    if (!menuactive)
    {
        if (key == key_menu_activate)
        {
            M_StartControlPanel();
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        return false;
    }

    //
    // Keys usable within menu
    //

    if (key == key_menu_down)
    {
        // [PN] Move down to the next available item
        do
        {
            itemOn = (itemOn + 1) % currentMenu->numitems;
        } while (currentMenu->menuitems[itemOn].status == STS_SKIP);

        S_StartSound(NULL, sfx_pstop);
        return true;
    }
    else if (key == key_menu_up)
    {
        // [PN] Move back up to the previous available item
        do
        {
            itemOn = (itemOn == 0) ? currentMenu->numitems - 1 : itemOn - 1;
        } while (currentMenu->menuitems[itemOn].status == STS_SKIP);

        S_StartSound(NULL, sfx_pstop);
        return true;
    }
    else if (key == key_menu_left)
    {
        // [JN] Go to previous-left menu by pressing Left Arrow.
        if (currentMenu->ScrollAR)
        {
            M_ScrollPages(false);
        }

        // Slide slider left
        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status > STS_SWTC)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(0);
        }
        return true;
    }
    else if (key == key_menu_right)
    {
        // [JN] Go to next-right menu by pressing Right Arrow.
        if (currentMenu->ScrollAR)
        {
            M_ScrollPages(true);
        }

        // Slide slider right
        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status > STS_SWTC)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(1);
        }
        return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item
        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;

            if (currentMenu->menuitems[itemOn].status == STS_MUL1
            ||  currentMenu->menuitems[itemOn].status == STS_MUL2)
            {
                currentMenu->menuitems[itemOn].routine(1); // right arrow
                S_StartSound(NULL, sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                S_StartSound(NULL, sfx_pistol);
            }
        }
        return true;
    }
    else if (key == key_menu_activate)
    {
        // [JN] If ESC key behaviour is set to "go back":
        if (menu_esc_key)
        {
            if (currentMenu == &MainDef || currentMenu == &SoundDef
            ||  currentMenu == &LoadDef || currentMenu == &SaveDef)
            {
                goto id_close_menu;  // [JN] Close menu imideatelly.
            }
            else
            {
                goto id_prev_menu;   // [JN] Go to previous menu.
            }
        }
        else
        {
        id_close_menu:
        // Deactivate menu
        currentMenu->lastOn = itemOn;
        M_ClearMenus();
        S_StartSound(NULL, sfx_swtchx);
        }
        return true;
    }
    else if (key == key_menu_back)
    {
        id_prev_menu:
        // Go back to previous menu
        currentMenu->lastOn = itemOn;

        if (currentMenu->prevMenu)
        {
            currentMenu = currentMenu->prevMenu;
            M_Reset_Line_Glow();
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
        }
        // [JN] Close menu if pressed "back" in Doom main or ID main menu.
        else if (currentMenu == &MainDef || currentMenu == &ID_Def_Main)
        {
            S_StartSound(NULL, sfx_swtchx);
            M_ClearMenus();
        }
        return true;
    }
    else if (key == key_menu_del)
    {
        // [crispy] delete a savegame
        if (currentMenu == &LoadDef || currentMenu == &SaveDef)
        {
            if (LoadMenu[itemOn].status)
            {
                currentMenu->lastOn = itemOn;
                M_ConfirmDeleteGame();
                return true;
            }
            else
            {
                return true;
            }
        }
        // [JN] ...or clear key bind.
        else
        if (currentMenu == &ID_Def_Keybinds_1 || currentMenu == &ID_Def_Keybinds_2
        ||  currentMenu == &ID_Def_Keybinds_3 || currentMenu == &ID_Def_Keybinds_4
        ||  currentMenu == &ID_Def_Keybinds_5 || currentMenu == &ID_Def_Keybinds_6)
        {
            M_ClearBind(itemOn);
            return true;
        }
        // [JN] ...or clear mouse bind.
        else
        if (currentMenu == &ID_Def_MouseBinds)
        {
            M_ClearMouseBind(itemOn);
            return true;
        }
    }
    else if (key == KEY_PGUP)
    {
        // [JN] Go to previous-left menu by pressing Page Up key.
        if (currentMenu->ScrollPG)
        {
            M_ScrollPages(false);
        }
    }
    else if (key == KEY_PGDN)
    {
        // [JN] Go to next-right menu by pressing Page Down key.
        if (currentMenu->ScrollPG)
        {
            M_ScrollPages(true);
        }
    }
    // Jump to menu item based on first letter:
    else if (ch != 0 || IsNullKey(key))
    {
        // [PN] Combined loops using a cyclic index to traverse the array twice,
        // avoiding code duplication.
        for (i = itemOn + 1; i < currentMenu->numitems + itemOn + 1; i++)
        {
            const int index = i % currentMenu->numitems;

            if (currentMenu->menuitems[index].alphaKey == ch)
            {
                itemOn = index;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;   // JDC
    // [JN] Disallow menu items highlighting initially to prevent
    // cursor jumping. It will be allowed by mouse movement.
    menu_mouse_allow = false;
}

static void M_ID_MenuMouseControl (void)
{
    if (!menu_mouse_allow || KbdIsBinding || MouseIsBinding)
    {
        // [JN] Skip hovering if the cursor is disabled/hidden or a binding is active.
        return;
    }
    else
    {
        // [JN] Which line height should be used?
        const int line_height = currentMenu->smallFont ? ID_MENU_LINEHEIGHT_SMALL : LINEHEIGHT;

        // [PN] Check if the cursor is hovering over a menu item
        for (int i = 0; i < currentMenu->numitems; i++)
        {
            // [JN] Slider takes three lines.
            const int line_item = currentMenu->menuitems[i].status == STS_SLDR ? 3 : 1;
            
            if (menu_mouse_x >= (currentMenu->x + WIDESCREENDELTA) * vid_resolution
            &&  menu_mouse_x <= (ORIGWIDTH + WIDESCREENDELTA - currentMenu->x) * vid_resolution
            &&  menu_mouse_y >= (currentMenu->y + i * line_height) * vid_resolution
            &&  menu_mouse_y <= (currentMenu->y + (i + line_item) * line_height) * vid_resolution
            &&  currentMenu->menuitems[i].status != -1)
            {
                // [PN] Highlight the current menu item
                itemOn = i;
            }
        }
    }
}

static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max)
{
    if (!menu_mouse_allow_click)
        return;
    
    // [JN/PN] Adjust slider boundaries
    const int adj_x = (x + WIDESCREENDELTA) * vid_resolution;
    const int adj_y = y * vid_resolution;
    const int adj_width = width * vid_resolution;
    const int adj_height = LINEHEIGHT * vid_resolution;

    // [PN] Check cursor position and item status
    if (menu_mouse_x < adj_x || menu_mouse_x > adj_x + adj_width
    ||  menu_mouse_y < adj_y || menu_mouse_y > adj_y + adj_height
    ||  currentMenu->menuitems[itemOn].status != STS_SLDR)
        return;

    // [PN] Calculate and update slider value
    const float normalized = (float)(menu_mouse_x - adj_x + 5) / adj_width;
    const float newValue = min + normalized * (max - min);
    if (is_float)
        *((float *)value) = newValue;
    else
        *((int *)value) = (int)newValue;

    // [JN/PN] Call related routine and reset mouse click allowance
    currentMenu->menuitems[itemOn].routine(-1);
    menu_mouse_allow_click = false;

    // Play sound
    S_StartSound(NULL, sfx_stnmov);
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    unsigned int	i;
    unsigned int	max;
    char		string[80];
    const char *name;
    int			start;

    // [JN] Shade background while active menu.
    if (menuactive)
    {
        // Temporary unshade while changing certain settings.
        if (shade_wait < I_GetTime())
        {
            M_ShadeBackground();
        }
        // Always redraw status bar background.
        st_fullupdate = true;
    }

    // [JN] Certain messages (binds reset) needs background filling.
    if (messageFillBG)
    {
        M_FillBackground();
    }

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = ORIGHEIGHT/2 - M_StringHeight(messageString) / 2;
	while (messageString[start] != '\0')
	{
	    int foundnewline = 0;

            for (i = 0; i < strlen(messageString + start); i++)
            {
                if (messageString[start + i] == '\n')
                {
                    M_StringCopy(string, messageString + start,
                                 sizeof(string));
                    if (i < sizeof(string))
                    {
                        string[i] = '\0';
                    }

                    foundnewline = 1;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, messageString + start, sizeof(string));
                start += strlen(string);
            }

	    x = ORIGWIDTH/2 - M_StringWidth(string) / 2;
	    M_WriteText(x, y, string, NULL);
	    y += SHORT(hu_font[0]->height);
	}

	return;
    }

    if (!menuactive)
    {
	menu_mouse_allow = false;  // [JN] Disallow cursor if menu is not active.
	return;
    }

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    if (currentMenu->smallFont)
    {
        // [JN] Draw glowing * symbol.
        M_WriteText(x - ID_MENU_CURSOR_OFFSET, y + itemOn * ID_MENU_LINEHEIGHT_SMALL, "*",
                    M_Cursor_Glow(cursor_tics));

        for (i = 0 ; i < max ; i++)
        {
            M_WriteText (x, y, currentMenu->menuitems[i].name,
                         M_Small_Line_Glow(currentMenu->menuitems[i].tics));
            y += ID_MENU_LINEHEIGHT_SMALL;
        }
    }
    else
    {
        // DRAW SKULL
        V_DrawShadowedPatchOptional(x + SKULLXOFF, y - 5 + itemOn * LINEHEIGHT, 0,
                                    W_CacheLumpName(DEH_String(skullName[whichSkull]), PU_CACHE));

        for (i = 0 ; i < max ; i++)
        {
            name = DEH_String(currentMenu->menuitems[i].name);

            if (name[0])
            {
                dp_translation = M_Big_Line_Glow(currentMenu->menuitems[i].tics);
                V_DrawShadowedPatchOptional(x, y, 0, W_CacheLumpName(name, PU_CACHE));
                dp_translation = NULL;
            }
            y += LINEHEIGHT;
        }
    }

    // [JN] Call the menu control routine for mouse input.
    M_ID_MenuMouseControl();
}


//
// M_ClearMenus
//
static void M_ClearMenus (void)
{
    menuactive = 0;
    menu_mouse_allow = false;  // [JN] Hide cursor on closing menu.
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
        whichSkull ^= 1;
        skullAnimCounter = 8;
    }

    // [JN] Cursor glowing animation:
    cursor_tics += cursor_direction ? -1 : 1;
    if (cursor_tics == 8 || cursor_tics == -8)
    {
        cursor_direction = !cursor_direction;
    }

    // [JN] Menu item fading effect:
    // Keep menu item bright or decrease tics for fading effect.
    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        if (menu_highlight == 1)
        {
            currentMenu->menuitems[i].tics =
                (itemOn == i) ? 5 : currentMenu->menuitems[i].tics - 1;
        }
        else
        if (menu_highlight == 2)
        {
            currentMenu->menuitems[i].tics =
                (itemOn == i) ? 5 : 0;
        }
        else
        {
            currentMenu->menuitems[i].tics = 0;
        }
    }
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // [JN] Player is always local, "console" player.
    player = &players[consoleplayer];

    // [JN] Set cursor position in skill menu to default skill level.
    NewDef.lastOn = level_select[0] = gp_default_skill;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    // The same hacks were used in the original Doom EXEs.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
    }

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

    // [crispy] Sigil
    if (!sigil && !sigil2)
    {
        EpiDef.numitems = 4;
    }
    else if (sigil != sigil2)
    {
        EpiDef.numitems = 5;
        if (sigil2)
        {
            EpiDef.menuitems = EpisodeMenuSII;
        }
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate)
    {
        EpiDef.numitems--;
    }
    // chex.exe shows only one episode.
    else if (gameversion == exe_chex)
    {
        EpiDef.numitems = 1;
    }

    // [JN] Older verions of Doom Shareware does not have M_NMARE patch,
    // so Nightmare is not available (at least in game menu).
    if (W_CheckNumForName(DEH_String("M_NMARE")) < 0)
    {
        NewDef.numitems--;
    }
}

// [crispy] delete a savegame
static char *savegwarning;
static void M_ConfirmDeleteGameResponse (int key)
{
	free(savegwarning);

	if (key == key_menu_confirm)
	{
		char name[256];

		M_StringCopy(name, P_SaveGameFile(itemOn), sizeof(name));
		remove(name);

		if (itemOn == quickSaveSlot)
			quickSaveSlot = -1;

		M_ReadSaveStrings();
	}
}

void M_ConfirmDeleteGame (void)
{
	savegwarning =
	M_StringJoin("delete savegame\n\n", "\"", savegamestrings[itemOn], "\"", " ?\n\n",
	             PRESSYN, NULL);

	M_StartMessage(savegwarning, M_ConfirmDeleteGameResponse, true);
	messageToPrint = 2;
	S_StartSound(NULL,sfx_swtchn);
}


// =============================================================================
//
//                        [JN] Keyboard binding routines.
//                    Drawing, coloring, checking and binding.
//
// =============================================================================


// -----------------------------------------------------------------------------
// M_NameBind
//  [JN] Convert Doom key number into printable string.
// -----------------------------------------------------------------------------

static struct {
    int key;
    char *name;
} key_names[] = KEY_NAMES_ARRAY;

static char *M_NameBind (int itemSetOn, int key)
{
    if (itemOn == itemSetOn && KbdIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        for (int i = 0; (size_t)i < arrlen(key_names); ++i)
        {
            if (key_names[i].key == key)
            {
                return key_names[i].name;
            }
        }
    }
    return "---";  // Means empty
}

// -----------------------------------------------------------------------------
// M_StartBind
//  [JN] Indicate that key binding is started (KbdIsBinding), and
//  pass internal number (keyToBind) for binding a new key.
// -----------------------------------------------------------------------------

static void M_StartBind (int keynum)
{
    KbdIsBinding = true;
    keyToBind = keynum;
}

// -----------------------------------------------------------------------------
// M_CheckBind
//  [JN] Check if pressed key is already binded, clear previous bind if found.
// -----------------------------------------------------------------------------

static void M_CheckBind (int key)
{
    // Page 1
    if (key_up == key)               key_up               = 0;
    if (key_down == key)             key_down             = 0;
    if (key_left == key)             key_left             = 0;
    if (key_right == key)            key_right            = 0;
    if (key_strafeleft == key)       key_strafeleft       = 0;
    if (key_straferight == key)      key_straferight      = 0;
    if (key_speed == key)            key_speed            = 0;
    if (key_strafe == key)           key_strafe           = 0;
    if (key_180turn == key)          key_180turn          = 0;
    if (key_fire == key)             key_fire             = 0;
    if (key_use == key)              key_use              = 0;
    // Page 2
    if (key_autorun == key)          key_autorun          = 0;
    if (key_mouse_look == key)       key_mouse_look       = 0;
    if (key_novert == key)           key_novert           = 0;
    if (key_reloadlevel == key)      key_reloadlevel      = 0;
    if (key_nextlevel == key)        key_nextlevel        = 0;
    if (key_demospeed == key)        key_demospeed        = 0;
    if (key_flip_levels == key)      key_flip_levels      = 0;
    if (key_widget_enable == key)    key_widget_enable    = 0;
    if (key_spectator == key)        key_spectator        = 0;
    if (key_freeze == key)           key_freeze           = 0;
    if (key_notarget == key)         key_notarget         = 0;
    if (key_buddha == key)           key_buddha           = 0;
    // Page 3
    if (key_weapon1 == key)          key_weapon1          = 0;
    if (key_weapon2 == key)          key_weapon2          = 0;
    if (key_weapon3 == key)          key_weapon3          = 0;
    if (key_weapon4 == key)          key_weapon4          = 0;
    if (key_weapon5 == key)          key_weapon5          = 0;
    if (key_weapon6 == key)          key_weapon6          = 0;
    if (key_weapon7 == key)          key_weapon7          = 0;
    if (key_weapon8 == key)          key_weapon8          = 0;
    if (key_prevweapon == key)       key_prevweapon       = 0;
    if (key_nextweapon == key)       key_nextweapon       = 0;
    // Page 4
    if (key_map_toggle == key)       key_map_toggle       = 0;
    // Do not override Automap binds in other pages.
    if (currentMenu == &ID_Def_Keybinds_4)
    {
        if (key_map_zoomin == key)     key_map_zoomin     = 0;
        if (key_map_zoomout == key)    key_map_zoomout    = 0;
        if (key_map_maxzoom == key)    key_map_maxzoom    = 0;
        if (key_map_follow == key)     key_map_follow     = 0;
        if (key_map_rotate == key)     key_map_rotate     = 0;
        if (key_map_overlay == key)    key_map_overlay    = 0;
        if (key_map_grid == key)       key_map_grid       = 0;
        if (key_map_mark == key)       key_map_mark       = 0;
        if (key_map_clearmark == key)  key_map_clearmark  = 0;
    }
    // Page 5
    if (key_menu_help == key)        key_menu_help        = 0;
    if (key_menu_save == key)        key_menu_save        = 0;
    if (key_menu_load == key)        key_menu_load        = 0;
    if (key_menu_volume == key)      key_menu_volume      = 0;
    if (key_menu_detail == key)      key_menu_detail      = 0;
    if (key_menu_qsave == key)       key_menu_qsave       = 0;
    if (key_menu_endgame == key)     key_menu_endgame     = 0;
    if (key_menu_messages == key)    key_menu_messages    = 0;
    if (key_menu_qload == key)       key_menu_qload       = 0;
    if (key_menu_quit == key)        key_menu_quit        = 0;
    if (key_menu_gamma == key)       key_menu_gamma       = 0;
    if (key_spy == key)              key_spy              = 0;
    // Page 6
    if (key_pause == key)            key_pause            = 0;
    if (key_menu_screenshot == key)  key_menu_screenshot  = 0;
    if (key_message_refresh == key)  key_message_refresh  = 0;
    if (key_demo_quit == key)        key_demo_quit        = 0;
    if (key_multi_msg == key)        key_multi_msg        = 0;
    // Do not override Send To binds in other pages.
    if (currentMenu == &ID_Def_Keybinds_6)
    {
        if (key_multi_msgplayer[0] == key) key_multi_msgplayer[0] = 0;
        if (key_multi_msgplayer[1] == key) key_multi_msgplayer[1] = 0;
        if (key_multi_msgplayer[2] == key) key_multi_msgplayer[2] = 0;
        if (key_multi_msgplayer[3] == key) key_multi_msgplayer[3] = 0;
    }
}

// -----------------------------------------------------------------------------
// M_DoBind
//  [JN] By catching internal bind number (keynum), do actual binding
//  of pressed key (key) to real keybind.
// -----------------------------------------------------------------------------

static void M_DoBind (int keynum, int key)
{
    switch (keynum)
    {
        // Page 1
        case 100:  key_up = key;                break;
        case 101:  key_down = key;              break;
        case 102:  key_left = key;              break;
        case 103:  key_right = key;             break;
        case 104:  key_strafeleft = key;        break;
        case 105:  key_straferight = key;       break;
        case 106:  key_speed = key;             break;
        case 107:  key_strafe = key;            break;
        case 108:  key_180turn = key;           break;
        case 109:  key_fire = key;              break;
        case 110:  key_use = key;               break;
        // Page 2  
        case 200:  key_autorun = key;           break;
        case 201:  key_mouse_look = key;        break;
        case 202:  key_novert = key;            break;
        case 203:  key_reloadlevel = key;       break;
        case 204:  key_nextlevel = key;         break;
        case 205:  key_demospeed = key;         break;
        case 206:  key_flip_levels = key;       break;
        case 207:  key_widget_enable = key;     break;
        case 208:  key_spectator = key;         break;
        case 209:  key_freeze = key;            break;
        case 210:  key_notarget = key;          break;
        case 211:  key_buddha = key;            break;
        // Page 3  
        case 300:  key_weapon1 = key;           break;
        case 301:  key_weapon2 = key;           break;
        case 302:  key_weapon3 = key;           break;
        case 303:  key_weapon4 = key;           break;
        case 304:  key_weapon5 = key;           break;
        case 305:  key_weapon6 = key;           break;
        case 306:  key_weapon7 = key;           break;
        case 307:  key_weapon8 = key;           break;
        case 308:  key_prevweapon = key;        break;
        case 309:  key_nextweapon = key;        break;
        // Page 4  
        case 400:  key_map_toggle = key;        break;
        case 401:  key_map_zoomin = key;        break;
        case 402:  key_map_zoomout = key;       break;
        case 403:  key_map_maxzoom = key;       break;
        case 404:  key_map_follow = key;        break;
        case 405:  key_map_rotate = key;        break;
        case 406:  key_map_overlay = key;       break;
        case 407:  key_map_grid = key;          break;
        case 408:  key_map_mark = key;          break;
        case 409:  key_map_clearmark = key;     break;
        // Page 5  
        case 500:  key_menu_help = key;         break;
        case 501:  key_menu_save = key;         break;
        case 502:  key_menu_load = key;         break;
        case 503:  key_menu_volume = key;       break;
        case 504:  key_menu_detail = key;       break;
        case 505:  key_menu_qsave = key;        break;
        case 506:  key_menu_endgame = key;      break;
        case 507:  key_menu_messages = key;     break;
        case 508:  key_menu_qload = key;        break;
        case 509:  key_menu_quit = key;         break;
        case 510:  key_menu_gamma = key;        break;
        case 511:  key_spy = key;               break;
        // Page 6  
        case 600:  key_pause = key;             break;
        case 601:  key_menu_screenshot = key;   break;
        case 602:  key_message_refresh = key;   break;
        case 603:  key_demo_quit = key;         break;
        case 604:  key_multi_msg = key;         break;
        case 605:  key_multi_msgplayer[0] = key;  break;
        case 606:  key_multi_msgplayer[1] = key;  break;
        case 607:  key_multi_msgplayer[2] = key;  break;
        case 608:  key_multi_msgplayer[3] = key;  break;
    }
}

// -----------------------------------------------------------------------------
// M_ClearBind
//  [JN] Clear key bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------

static void M_ClearBind (int itemOn)
{
    if (currentMenu == &ID_Def_Keybinds_1)
    {
        switch (itemOn)
        {
            case 0:   key_up = 0;               break;
            case 1:   key_down = 0;             break;
            case 2:   key_left = 0;             break;
            case 3:   key_right = 0;            break;
            case 4:   key_strafeleft = 0;       break;
            case 5:   key_straferight = 0;      break;
            case 6:   key_speed = 0;            break;
            case 7:   key_strafe = 0;           break;
            case 8:   key_180turn = 0;          break;
            // Action title
            case 10:  key_fire = 0;             break;
            case 11:  key_use = 0;              break;
        }
    }
    if (currentMenu == &ID_Def_Keybinds_2)
    {
        switch (itemOn)
        {
            case 0:   key_autorun = 0;          break;
            case 1:   key_mouse_look = 0;       break;
            case 2:   key_novert = 0;           break;
            // Special keys title
            case 4:   key_reloadlevel = 0;      break;
            case 5:   key_nextlevel = 0;        break;
            case 6:   key_demospeed = 0;        break;
            case 7:   key_flip_levels = 0;      break;
            case 8:   key_widget_enable = 0;    break;
            // Special modes title
            case 10:  key_spectator = 0;        break;
            case 11:  key_freeze = 0;           break;
            case 12:  key_notarget = 0;         break;
            case 13:  key_buddha = 0;           break;
        }
    }
    if (currentMenu == &ID_Def_Keybinds_3)
    {
        switch (itemOn)
        {
            case 0:   key_weapon1 = 0;          break;
            case 1:   key_weapon2 = 0;          break;
            case 2:   key_weapon3 = 0;          break;
            case 3:   key_weapon4 = 0;          break;
            case 4:   key_weapon5 = 0;          break;
            case 5:   key_weapon6 = 0;          break;
            case 6:   key_weapon7 = 0;          break;
            case 7:   key_weapon8 = 0;          break;
            case 8:   key_prevweapon = 0;       break;
            case 9:   key_nextweapon = 0;       break;
        }
    }
    if (currentMenu == &ID_Def_Keybinds_4)
    {
        switch (itemOn)
        {
            case 0:   key_map_toggle = 0;       break;
            case 1:   key_map_zoomin = 0;       break;
            case 2:   key_map_zoomout = 0;      break;
            case 3:   key_map_maxzoom = 0;      break;
            case 4:   key_map_follow = 0;       break;
            case 5:   key_map_rotate = 0;       break;
            case 6:   key_map_overlay = 0;      break;
            case 7:   key_map_grid = 0;         break;
            case 8:   key_map_mark = 0;         break;
            case 9:   key_map_clearmark = 0;    break;
        }
    }
    if (currentMenu == &ID_Def_Keybinds_5)
    {
        switch (itemOn)
        {
            case 0:   key_menu_help = 0;        break;
            case 1:   key_menu_save = 0;        break;
            case 2:   key_menu_load = 0;        break;
            case 3:   key_menu_volume = 0;      break;
            case 4:   key_menu_detail = 0;      break;
            case 5:   key_menu_qsave = 0;       break;
            case 6:   key_menu_endgame = 0;     break;
            case 7:   key_menu_messages = 0;    break;
            case 8:   key_menu_qload = 0;       break;
            case 9:   key_menu_quit = 0;        break;
            case 10:  key_menu_gamma = 0;       break;
            case 11:  key_spy = 0;              break;
        }
    }
    if (currentMenu == &ID_Def_Keybinds_6)
    {
        switch (itemOn)
        {
            case 0:   key_pause = 0;            break;
            case 1:   key_menu_screenshot = 0;  break;
            case 2:   key_message_refresh = 0;  break;
            case 3:   key_demo_quit = 0;        break;
            // Multiplayer title
            case 5:   key_multi_msg = 0;        break;
            case 6:   key_multi_msgplayer[0] = 0;  break;
            case 7:   key_multi_msgplayer[1] = 0;  break;
            case 8:   key_multi_msgplayer[2] = 0;  break;
            case 9:   key_multi_msgplayer[3] = 0;  break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all keyboard binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetBinds (void)
{
    // Page 1
    key_up = 'w';
    key_down = 's';
    key_left = KEY_LEFTARROW;
    key_right = KEY_RIGHTARROW;
    key_strafeleft = 'a';
    key_straferight = 'd';
    key_speed = KEY_RSHIFT;
    key_strafe = KEY_RALT;
    key_180turn = 0;
    key_fire = KEY_RCTRL;
    key_use = ' ';
    // Page 2
    key_autorun = KEY_CAPSLOCK;
    key_mouse_look = 0;
    key_novert = 0;
    key_reloadlevel = 0;
    key_nextlevel = 0;
    key_demospeed = 0;
    key_flip_levels = 0;
    key_widget_enable = 0;
    key_spectator = 0;
    key_freeze = 0;
    key_notarget = 0;
    key_buddha = 0;
    // Page 3
    key_weapon1 = '1';
    key_weapon2 = '2';
    key_weapon3 = '3';
    key_weapon4 = '4';
    key_weapon5 = '5';
    key_weapon6 = '6';
    key_weapon7 = '7';
    key_weapon8 = '8';
    key_prevweapon = 0;
    key_nextweapon = 0;
    // Page 4
    key_map_toggle = KEY_TAB;
    key_map_zoomin = '=';
    key_map_zoomout = '-';
    key_map_maxzoom = '0';
    key_map_follow = 'f';
    key_map_rotate = 'r';
    key_map_overlay = 'o';
    key_map_grid = 'g';
    key_map_mark = 'm';
    key_map_clearmark = 'c';
    // Page 5
    key_menu_help = KEY_F1;
    key_menu_save = KEY_F2;
    key_menu_load = KEY_F3;
    key_menu_volume = KEY_F4;
    key_menu_detail = KEY_F5;
    key_menu_qsave = KEY_F6;
    key_menu_endgame = KEY_F7;
    key_menu_messages = KEY_F8;
    key_menu_qload = KEY_F9;
    key_menu_quit = KEY_F10;
    key_menu_gamma = KEY_F11;
    key_spy = KEY_F12;
    // Page 6
    key_pause = KEY_PAUSE;
    key_menu_screenshot = KEY_PRTSCR;
    key_message_refresh = KEY_ENTER;
    key_demo_quit = 'q';
    key_multi_msg = 't';
    key_multi_msgplayer[0] = 'g';
    key_multi_msgplayer[1] = 'i';
    key_multi_msgplayer[2] = 'b';
    key_multi_msgplayer[3] = 'r';
}

// -----------------------------------------------------------------------------
// M_ColorizeBind
//  [JN] Do key bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeBind (int itemSetOn, int key)
{
    if (itemOn == itemSetOn && KbdIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        if (key == 0)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        else
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindKey
//  [JN] Do keyboard bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindKey (int itemNum, int yPos, int key)
{
    M_WriteText(M_ItemRightAlign(M_NameBind(itemNum, key)),
                yPos,
                M_NameBind(itemNum, key),
                M_ColorizeBind(itemNum, key));
}

// -----------------------------------------------------------------------------
// M_DrawBindFooter
//  [JN] Draw footer in key binding pages with numeration.
// -----------------------------------------------------------------------------

static void M_DrawBindFooter (char *pagenum, boolean drawPages)
{
    const char *string = "PRESS ENTER TO BIND, DEL TO CLEAR";

    if (drawPages)
    {
        M_WriteTextCentered(171, string, cr[CR_MENU_DARK1]);
        M_WriteText(ID_MENU_LEFTOFFSET, 180, "< PGUP", cr[CR_MENU_DARK3]);
        M_WriteTextCentered(180, M_StringJoin("PAGE ", pagenum, "/6", NULL), cr[CR_MENU_DARK2]);
        M_WriteText(M_ItemRightAlign("PGDN >"), 180, "PGDN >", cr[CR_MENU_DARK3]);
    }
    else
    {
        M_WriteTextCentered(180, string, cr[CR_MENU_DARK1]);
    }
}


// =============================================================================
//
//                          [JN] Mouse binding routines.
//                    Drawing, coloring, checking and binding.
//
// =============================================================================


// -----------------------------------------------------------------------------
// M_NameBind
//  [JN] Draw mouse button number as printable string.
// -----------------------------------------------------------------------------


static char *M_NameMouseBind (int itemSetOn, int btn)
{
    if (itemOn == itemSetOn && MouseIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        char  num[8]; 
        char *other_button;

        M_snprintf(num, 8, "%d", btn + 1);
        other_button = M_StringJoin("BUTTON #", num, NULL);

        switch (btn)
        {
            case -1:  return  "---";            break;  // Means empty
            case  0:  return  "LEFT BUTTON";    break;
            case  1:  return  "RIGHT BUTTON";   break;
            case  2:  return  "MIDDLE BUTTON";  break;
            case  3:  return  "WHEEL UP";       break;
            case  4:  return  "WHEEL DOWN";     break;
            default:  return  other_button;     break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_StartMouseBind
//  [JN] Indicate that mouse button binding is started (MouseIsBinding), and
//  pass internal number (btnToBind) for binding a new button.
// -----------------------------------------------------------------------------

static void M_StartMouseBind (int btn)
{
    MouseIsBinding = true;
    btnToBind = btn;
}

// -----------------------------------------------------------------------------
// M_CheckMouseBind
//  [JN] Check if pressed button is already binded, clear previous bind if found.
// -----------------------------------------------------------------------------

static void M_CheckMouseBind (int btn)
{
    if (mousebfire == btn)        mousebfire        = -1;
    if (mousebforward == btn)     mousebforward     = -1;
    if (mousebbackward == btn)    mousebbackward    = -1;
    if (mousebuse == btn)         mousebuse         = -1;
    if (mousebspeed == btn)       mousebspeed       = -1;
    if (mousebstrafe == btn)      mousebstrafe      = -1;
    if (mousebstrafeleft == btn)  mousebstrafeleft  = -1;
    if (mousebstraferight == btn) mousebstraferight = -1;
    if (mousebprevweapon == btn)  mousebprevweapon  = -1;
    if (mousebnextweapon == btn)  mousebnextweapon  = -1;
}

// -----------------------------------------------------------------------------
// M_DoMouseBind
//  [JN] By catching internal bind number (btnnum), do actual binding
//  of pressed button (btn) to real mouse bind.
// -----------------------------------------------------------------------------

static void M_DoMouseBind (int btnnum, int btn)
{
    switch (btnnum)
    {
        case 1000:  mousebfire = btn;         break;
        case 1001:  mousebforward = btn;      break;
        case 1002:  mousebbackward = btn;     break;
        case 1003:  mousebuse = btn;          break;
        case 1004:  mousebspeed = btn;        break;
        case 1005:  mousebstrafe = btn;       break;
        case 1006:  mousebstrafeleft = btn;   break;
        case 1007:  mousebstraferight = btn;  break;
        case 1008:  mousebprevweapon = btn;   break;
        case 1009:  mousebnextweapon = btn;   break;
        default:                              break;
    }
}

// -----------------------------------------------------------------------------
// M_ClearMouseBind
//  [JN] Clear mouse bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------


static void M_ClearMouseBind (int itemOn)
{
    switch (itemOn)
    {
        case 0:  mousebfire = -1;         break;
        case 1:  mousebforward = -1;      break;
        case 2:  mousebbackward = -1;     break;
        case 3:  mousebuse = -1;          break;
        case 4:  mousebspeed = -1;        break;
        case 5:  mousebstrafe = -1;       break;
        case 6:  mousebstrafeleft = -1;   break;
        case 7:  mousebstraferight = -1;  break;
        case 8:  mousebprevweapon = -1;   break;
        case 9:  mousebnextweapon = -1;   break;
    }
}

// -----------------------------------------------------------------------------
// M_ColorizeMouseBind
//  [JN] Do mouse bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeMouseBind (int itemSetOn, int btn)
{
    if (itemOn == itemSetOn && MouseIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        if (btn == -1)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        else
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindButton
//  [JN] Do mouse button bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindButton (int itemNum, int yPos, int btn)
{
    M_WriteText(M_ItemRightAlign(M_NameMouseBind(itemNum, btn)),
                yPos,
                M_NameMouseBind(itemNum, btn),
                M_ColorizeMouseBind(itemNum, btn));
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all mouse binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    mousebfire = 0;
    mousebforward = 2;
    mousebbackward = -1;
    mousebuse = -1;
    mousebspeed = -1;
    mousebstrafe = 1;
    mousebstrafeleft = -1;
    mousebstraferight = -1;
    mousebprevweapon = 4;
    mousebnextweapon = 3;
}
