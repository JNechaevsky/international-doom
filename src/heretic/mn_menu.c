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

// MN_menu.c

#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "deh_str.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "gusconf.h"
#include "i_input.h"
#include "i_system.h"
#include "i_swap.h"
#include "i_timer.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "am_map.h"
#include "ct_chat.h"
#include "sb_bar.h"

#include "id_vars.h"
#include "id_func.h"

// Macros

#define LEFT_DIR 0
#define RIGHT_DIR 1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN     22
#define ASCII_CURSOR '['

// Types

typedef enum
{
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC1,    // Multichoice function: increase by wheel up, decrease by wheel down
    ITT_LRFUNC2,    // Multichoice function: decrease by wheel up, increase by wheel down
    ITT_SETMENU,
    ITT_SLDR,       // Slider line.
    ITT_INERT
} ItemType_t;

typedef enum
{
    MENU_MAIN,
    MENU_EPISODE,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_FILES,
    MENU_LOAD,
    MENU_SAVE,
    MENU_ID_MAIN,
    MENU_ID_VIDEO1,
    MENU_ID_VIDEO2,
    MENU_ID_DISPLAY,
    MENU_ID_SOUND,
    MENU_ID_CONTROLS,
    MENU_ID_KBDBINDS1,
    MENU_ID_KBDBINDS2,
    MENU_ID_KBDBINDS3,
    MENU_ID_KBDBINDS4,
    MENU_ID_KBDBINDS5,
    MENU_ID_KBDBINDS6,
    MENU_ID_KBDBINDS7,
    MENU_ID_KBDBINDS8,
    MENU_ID_MOUSEBINDS,
    MENU_ID_WIDGETS,
    MENU_ID_AUTOMAP,
    MENU_ID_GAMEPLAY1,
    MENU_ID_GAMEPLAY2,
    MENU_ID_GAMEPLAY3,
    MENU_ID_GAMEPLAY4,
    MENU_ID_MISC,
    MENU_ID_LEVEL1,
    MENU_ID_LEVEL2,
    MENU_ID_LEVEL3,
    MENU_NONE
} MenuType_t;

typedef struct
{
    ItemType_t type;
    char *text;
    void (*func) (int option);
    int option;
    MenuType_t menu;
    short tics;  // [JN] Menu item timer for glowing effect.
} MenuItem_t;

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

// [JN] Font enum's used by FontType in Menu_t below.
// NoFont is used only in Save/Load menu for allowing to 
// choose slot by pressing number key.
enum {
    NoFont,
    SmallFont,
    BigFont
} FontType_t;

typedef struct
{
    int x;
    int y;
    void (*drawFunc) (void);
    int itemCount;
    MenuItem_t *items;
    int oldItPos;
    int FontType;       // [JN] 0 = no font, 1 = small font, 2 = big font
    boolean ScrollAR;   // [JN] Menu can be scrolled by arrow keys
    boolean ScrollPG;   // [JN] Menu can be scrolled by PGUP/PGDN keys
    MenuType_t prevMenu;
} Menu_t;

// Private Functions

static void InitFonts(void);
static void SetMenu(MenuType_t menu);
static boolean SCNetCheck(int option);
static void SCNetCheck2(int option);
static void SCQuitGame(int option);
static void SCEpisode(int option);
static void SCSkill(int option);
static void SCMouseSensi(int option);
static void SCMouseSensi_y(int option);
static void SCSfxVolume(int option);
static void SCMusicVolume(int option);
static void SCChangeDetail(int option);
static void SCScreenSize(int option);
static void SCLoadGame(int option);
static void SCSaveCheck(int option);
static void SCSaveGame(int option);
static void SCEndGame(int option);
static void SCInfo(int option);
static void DrawMainMenu(void);
static void DrawEpisodeMenu(void);
static void DrawSkillMenu(void);
static void DrawOptionsMenu(void);
static void DrawOptions2Menu(void);
static void DrawFileSlots(Menu_t * menu);
static void DrawFilesMenu(void);
static void MN_DrawInfo(void);
static void DrawLoadMenu(void);
static void DrawSaveMenu(void);
static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing, int itemPos);
void MN_LoadSlotText(void);

static void M_ID_MenuMouseControl (void);
static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max);

// Public Data

boolean MenuActive;
int InfoType;

// Private Data

static int FontABaseLump;
static int FontBBaseLump;
static int SkullBaseLump;
static Menu_t *CurrentMenu;
static int CurrentItPos;    // -1 = no selection
static int MenuEpisode;
static int MenuTime;

boolean askforquit;
static int typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static char SlotText[SAVES_PER_PAGE][SLOTTEXTLEN + 2];
static char oldSlotText[SLOTTEXTLEN + 2];
static int SlotStatus[SAVES_PER_PAGE];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

// [JN] Show custom titles while performing quick save/load.
static boolean quicksaveTitle = false;
static boolean quickloadTitle = false;

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

static MenuItem_t MainItems[] = {
    {ITT_SETMENU, "NEW GAME", SCNetCheck2, 1, MENU_EPISODE},
    {ITT_SETMENU, "OPTIONS", NULL, 0, MENU_ID_MAIN},
    {ITT_SETMENU, "GAME FILES", NULL, 0, MENU_FILES},
    {ITT_EFUNC, "INFO", SCInfo, 0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME", SCQuitGame, 0, MENU_NONE}
};

static Menu_t MainMenu = {
    110, 56,
    DrawMainMenu,
    5, MainItems,
    0,
    BigFont, false, false,
    MENU_NONE
};

static MenuItem_t EpisodeItems[] = {
    {ITT_EFUNC, "CITY OF THE DAMNED", SCEpisode, 1, MENU_NONE},
    {ITT_EFUNC, "HELL'S MAW", SCEpisode, 2, MENU_NONE},
    {ITT_EFUNC, "THE DOME OF D'SPARIL", SCEpisode, 3, MENU_NONE},
    {ITT_EFUNC, "THE OSSUARY", SCEpisode, 4, MENU_NONE},
    {ITT_EFUNC, "THE STAGNANT DEMESNE", SCEpisode, 5, MENU_NONE}
};

static Menu_t EpisodeMenu = {
    80, 50,
    DrawEpisodeMenu,
    3, EpisodeItems,
    0,
    BigFont, false, false,
    MENU_MAIN
};

static MenuItem_t FilesItems[] = {
    { ITT_SETMENU, "LOAD GAME", SCNetCheck2,  2, MENU_LOAD },
    { ITT_EFUNC, "SAVE GAME", SCSaveCheck, 0, MENU_SAVE },
};

static Menu_t FilesMenu = {
    110, 60,
    DrawFilesMenu,
    2, FilesItems,
    0,
    BigFont, false, false,
    MENU_MAIN
};

// [JN] Allow to chose slot by pressing number key.
// This behavior is same to Doom.
static MenuItem_t LoadItems[] = {
    {ITT_EFUNC, "1", SCLoadGame, 0, MENU_NONE},
    {ITT_EFUNC, "2", SCLoadGame, 1, MENU_NONE},
    {ITT_EFUNC, "3", SCLoadGame, 2, MENU_NONE},
    {ITT_EFUNC, "4", SCLoadGame, 3, MENU_NONE},
    {ITT_EFUNC, "5", SCLoadGame, 4, MENU_NONE},
    {ITT_EFUNC, "6", SCLoadGame, 5, MENU_NONE}
};

static Menu_t LoadMenu = {
    70, 18,
    DrawLoadMenu,
    SAVES_PER_PAGE, LoadItems,
    0,
    NoFont, true, true,
    MENU_FILES
};

// [JN] Allow to chose slot by pressing number key.
// This behavior is same to Doom.
static MenuItem_t SaveItems[] = {
    {ITT_EFUNC, "1", SCSaveGame, 0, MENU_NONE},
    {ITT_EFUNC, "2", SCSaveGame, 1, MENU_NONE},
    {ITT_EFUNC, "3", SCSaveGame, 2, MENU_NONE},
    {ITT_EFUNC, "4", SCSaveGame, 3, MENU_NONE},
    {ITT_EFUNC, "5", SCSaveGame, 4, MENU_NONE},
    {ITT_EFUNC, "6", SCSaveGame, 5, MENU_NONE}
};

static Menu_t SaveMenu = {
    70, 18,
    DrawSaveMenu,
    SAVES_PER_PAGE, SaveItems,
    0,
    NoFont, true, true,
    MENU_FILES
};

static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, "THOU NEEDETH A WET-NURSE", SCSkill, sk_baby, MENU_NONE},
    {ITT_EFUNC, "YELLOWBELLIES-R-US", SCSkill, sk_easy, MENU_NONE},
    {ITT_EFUNC, "BRINGEST THEM ONETH", SCSkill, sk_medium, MENU_NONE},
    {ITT_EFUNC, "THOU ART A SMITE-MEISTER", SCSkill, sk_hard, MENU_NONE},
    {ITT_EFUNC, "BLACK PLAGUE POSSESSES THEE",
     SCSkill, sk_nightmare, MENU_NONE}
};

static Menu_t SkillMenu = {
    38, 30,
    DrawSkillMenu,
    5, SkillItems,
    2,
    BigFont, false, false,
    MENU_EPISODE
};

static MenuItem_t OptionsItems[] = {
    {ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE},
    {ITT_LRFUNC1, "MOUSE SENSITIVITY", SCMouseSensi, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_SETMENU, "MORE...", NULL, 0, MENU_OPTIONS2}
};

static Menu_t OptionsMenu = {
    88, 30,
    DrawOptionsMenu,
    4, OptionsItems,
    0,
    BigFont, false, false,
    MENU_ID_MAIN
};

static MenuItem_t Options2Items[] = {
    { ITT_SLDR,  "SFX VOLUME",   SCSfxVolume,   0, MENU_NONE },
    { ITT_EMPTY, NULL,           NULL,          0, MENU_NONE },
    { ITT_SLDR,  "MUSIC VOLUME", SCMusicVolume, 0, MENU_NONE },
    { ITT_EMPTY, NULL,           NULL,          0, MENU_NONE },
    { ITT_SLDR,  "SCREEN SIZE",  SCScreenSize,  0, MENU_NONE },
    { ITT_EMPTY, NULL,           NULL,          0, MENU_NONE },
};

static Menu_t Options2Menu = {
    72, 20,
    DrawOptions2Menu,
    6, Options2Items,
    0,
    BigFont, false, false,
    MENU_ID_MAIN
};

// =============================================================================
// [JN] ID custom menu
// =============================================================================

#define ID_MENU_TOPOFFSET         (20)
#define ID_MENU_LEFTOFFSET        (48)
#define ID_MENU_LEFTOFFSET_SML    (90)
#define ID_MENU_LEFTOFFSET_BIG    (32)
#define ID_MENU_LEFTOFFSET_LEVEL  (74)
#define ID_MENU_CTRLSOFFSET       (44)

#define ID_MENU_LINEHEIGHT_SMALL  (10)
#define ID_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return ORIGWIDTH - CurrentMenu->x - MN_TextAWidth(text);
}

static player_t *player;

static void M_Draw_ID_Main (void);

static void M_Draw_ID_Video_1 (void);
static void M_ID_TrueColor (int choice);
static void M_ID_RenderingRes (int choice);
static void M_ID_Widescreen (int choice);
static void M_ID_ExclusiveFS (int choice);
static void M_ID_UncappedFPS (int choice);
static void M_ID_LimitFPS (int choice);
static void M_ID_VSync (int choice);
static void M_ID_ShowFPS (int choice);
static void M_ID_PixelScaling (int choice);

static void M_Draw_ID_Video_2 (void);
static void M_ID_GfxStartup (int choice);
static void M_ID_ScreenWipe (int choice);
static void M_ID_EndText (int choice);
static void M_ID_SuperSmoothing (int choice);
static void M_ID_OverbrightGlow (int choice);
static void M_ID_SoftBloom (int choice);
static void M_ID_AnalogRGBDrift (int choice);
static void M_ID_VHSLineDistortion (int choice);
static void M_ID_ScreenVignette (int choice);
static void M_ID_FilmGrain (int choice);
static void M_ID_MotionBlur (int choice);
static void M_ID_DepthOfFieldBlur (int choice);

static void M_ScrollVideo (int choice);

static void M_Draw_ID_Display (void);
static void M_ID_FOV (int choice);
static void M_ID_MenuShading (int choice);
static void M_ID_LevelBrightness (int choice);
static void M_ID_Gamma (int choice);
static void M_ID_Contrast (int choice);
static void M_ID_Saturation (int choice);
static void M_ID_R_Intensity (int choice);
static void M_ID_G_Intensity (int choice);
static void M_ID_B_Intensity (int choice);
static void M_ID_Messages (int choice);
static void M_ID_TextShadows (int choice);
static void M_ID_LocalTime (int choice);

static void M_Draw_ID_Sound (void);
static void M_ID_MusicSystem (int option);
static void M_ID_SFXMode (int option);
static void M_ID_PitchShift (int option);
static void M_ID_SFXChannels (int option);
static void M_ID_MuteInactive (int option);

static void M_Draw_ID_Controls (void);
static void M_ID_Controls_Acceleration (int option);
static void M_ID_Controls_Threshold (int option);
static void M_ID_Controls_MLook (int option);
static void M_ID_Controls_NoVert (int option);
static void M_ID_Controls_InvertY (int option);
static void M_ID_Controls_NoArtiSkip (int option);

static void M_Draw_ID_Keybinds_1 (void);
static void M_Bind_MoveForward (int option);
static void M_Bind_MoveBackward (int option);
static void M_Bind_TurnLeft (int option);
static void M_Bind_TurnRight (int option);
static void M_Bind_StrafeLeft (int option);
static void M_Bind_StrafeRight (int option);
static void M_Bind_StrafeOn (int option);
static void M_Bind_SpeedOn (int option);
static void M_Bind_180Turn (int option);
static void M_Bind_FireAttack (int option);
static void M_Bind_Use (int option);

static void M_Draw_ID_Keybinds_2 (void);
static void M_Bind_LookUp (int option);
static void M_Bind_LookDown (int option);
static void M_Bind_LookCenter (int option);
static void M_Bind_FlyUp (int option);
static void M_Bind_FlyDown (int option);
static void M_Bind_FlyCenter (int option);
static void M_Bind_InvLeft (int option);
static void M_Bind_InvRight (int option);
static void M_Bind_UseArti (int option);

static void M_Draw_ID_Keybinds_3 (void);
static void M_Bind_AlwaysRun (int option);
static void M_Bind_MouseLook (int option);
static void M_Bind_NoVert (int option);
static void M_Bind_RestartLevel (int option);
static void M_Bind_NextLevel (int option);
static void M_Bind_FastForward (int option);
static void M_Bind_FlipLevels (int option);
static void M_Bind_ExtendedHUD (int choice);
static void M_Bind_SpectatorMode (int option);
static void M_Bind_FreezeMode (int option);
static void M_Bind_NotargetMode (int option);
static void M_Bind_BuddhaMode (int option);

static void M_Draw_ID_Keybinds_4 (void);
static void M_Bind_Weapon1 (int option);
static void M_Bind_Weapon2 (int option);
static void M_Bind_Weapon3 (int option);
static void M_Bind_Weapon4 (int option);
static void M_Bind_Weapon5 (int option);
static void M_Bind_Weapon6 (int option);
static void M_Bind_Weapon7 (int option);
static void M_Bind_Weapon8 (int option);
static void M_Bind_PrevWeapon (int option);
static void M_Bind_NextWeapon (int option);

static void M_Draw_ID_Keybinds_5 (void);
static void M_Bind_Quartz (int option);
static void M_Bind_Urn (int option);
static void M_Bind_Bomb (int option);
static void M_Bind_Tome (int option);
static void M_Bind_Ring (int option);
static void M_Bind_Chaosdevice (int option);
static void M_Bind_Shadowsphere (int option);
static void M_Bind_Wings (int option);
static void M_Bind_Torch (int option);
static void M_Bind_Morph (int option);

static void M_Draw_ID_Keybinds_6 (void);
static void M_Bind_ToggleMap (int option);
static void M_Bind_ZoomIn (int option);
static void M_Bind_ZoomOut (int option);
static void M_Bind_MaxZoom (int option);
static void M_Bind_FollowMode (int option);
static void M_Bind_RotateMode (int option);
static void M_Bind_OverlayMode (int option);
static void M_Bind_ToggleGrid (int option);
static void M_Bind_AddMark (int option);
static void M_Bind_ClearMarks (int option);

static void M_Draw_ID_Keybinds_7 (void);
static void M_Bind_HelpScreen (int option);
static void M_Bind_SaveGame (int option);
static void M_Bind_LoadGame (int option);
static void M_Bind_SoundVolume (int option);
static void M_Bind_ToggleDetail (int option);
static void M_Bind_QuickSave (int option);
static void M_Bind_EndGame (int option);
static void M_Bind_ToggleMessages (int option);
static void M_Bind_QuickLoad (int option);
static void M_Bind_QuitGame (int option);
static void M_Bind_ToggleGamma (int option);
static void M_Bind_MultiplayerSpy (int option);

static void M_Draw_ID_Keybinds_8 (void);
static void M_Bind_Pause (int option);
static void M_Bind_SaveScreenshot (int option);
static void M_Bind_LastMessage (int option);
static void M_Bind_FinishDemo (int option);
static void M_Bind_SendMessage (int option);
static void M_Bind_ToPlayer1 (int option);
static void M_Bind_ToPlayer2 (int option);
static void M_Bind_ToPlayer3 (int option);
static void M_Bind_ToPlayer4 (int option);
static void M_Bind_Reset (int option);

static void M_Draw_ID_MouseBinds (void);
static void M_Bind_M_FireAttack (int option);
static void M_Bind_M_MoveForward (int option);
static void M_Bind_M_MoveBackward (int option);
static void M_Bind_M_Use (int option);
static void M_Bind_M_SpeedOn (int option);
static void M_Bind_M_StrafeOn (int option);
static void M_Bind_M_StrafeLeft (int option);
static void M_Bind_M_StrafeRight (int option);
static void M_Bind_M_PrevWeapon (int option);
static void M_Bind_M_NextWeapon (int option);
static void M_Bind_M_InventoryLeft (int option);
static void M_Bind_M_InventoryRight (int option);
static void M_Bind_M_UseArtifact (int option);

static void M_Bind_M_Reset (int option);

static void M_Draw_ID_Widgets (void);
static void M_ID_Widget_Colors (int choice);
static void M_ID_Widget_Placement (int choice);
static void M_ID_Widget_Alignment (int choice);
static void M_ID_Widget_KIS (int choice);
static void M_ID_Widget_KIS_Format (int choice);
static void M_ID_Widget_KIS_Items (int choice);
static void M_ID_Widget_Coords (int choice);
static void M_ID_Widget_Speed (int choice);
static void M_ID_Widget_Render (int choice);
static void M_ID_Widget_Time (int choice);
static void M_ID_Widget_TotalTime (int choice);
static void M_ID_Widget_LevelName (int choice);
static void M_ID_Widget_Health (int choice);

static void M_Draw_ID_Automap (void);
static void M_ID_Automap_Smooth (int choice);
static void M_ID_Automap_Thick (int choice);
static void M_ID_Automap_Square (int choice);
static void M_ID_Automap_TexturedBg (int choice);
static void M_ID_Automap_ScrollBg (int choice);
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
static void M_ID_SwirlingLiquids (int choice);
static void M_ID_InvulSky (int choice);
static void M_ID_LinearSky (int choice);
static void M_ID_FlipCorpses (int choice);
static void M_ID_Crosshair (int choice);
static void M_ID_CrosshairColor (int choice);

static void M_Draw_ID_Gameplay_2 (void);
static void M_ID_ColoredSBar (int choice);
static void M_ID_AmmoWidget (int choice);
static void M_ID_AmmoWidgetTranslucent (int choice);
static void M_ID_AmmoWidgetColors (int choice);
static void M_ID_ZAxisSfx (int choice);
static void M_ID_Torque (int choice);
static void M_ID_WeaponAlignment (int choice);
static void M_ID_Breathing (int choice);

static void M_Draw_ID_Gameplay_3 (void);
static void M_ID_DefaulSkill (int choice);
static void M_ID_RevealedSecrets (int choice);
static void M_ID_FlipLevels (int choice);
static void M_ID_OnDeathAction (int choice);
static void M_ID_DemoTimer (int choice);
static void M_ID_TimerDirection (int choice);
static void M_ID_ProgressBar (int choice);
static void M_ID_InternalDemos (int choice);

static void M_Draw_ID_Gameplay_4 (void);
static void M_ID_PistolStart (int choice);
static void M_ID_BlockmapFix (int choice);
static void M_ID_AutomaticSR50 (int choice);
static void M_ID_NoLandCenter (int choice);

static void M_ScrollGameplay (int choice);

static void M_Draw_ID_Misc (void);
static void M_ID_Misc_A11yInvul (int choice);
static void M_ID_Misc_A11yPalFlash (int choice);
static void M_ID_Misc_A11yMoveBob (int choice);
static void M_ID_Misc_A11yWeaponBob (int choice);
static void M_ID_Misc_A11yColorblind (int choice);
static void M_ID_Misc_AutoloadWAD (int choice);
static void M_ID_Misc_AutoloadHHE (int choice);
static void M_ID_Misc_Hightlight (int choice);
static void M_ID_Misc_MenuEscKey (int choice);

static void M_Draw_ID_Level_1 (void);
static void M_ID_LevelSkill (int choice);
static void M_ID_LevelEpisode (int choice);
static void M_ID_LevelMap (int choice);
static void M_ID_LevelHealth (int choice);
static void M_ID_LevelArmor (int choice);
static void M_ID_LevelArmorType (int choice);
static void M_ID_LevelGauntlets (int choice);
static void M_ID_LevelCrossbow (int choice);
static void M_ID_LevelDragonClaw (int choice);
static void M_ID_LevelHellStaff (int choice);
static void M_ID_LevelPhoenixRod (int choice);
static void M_ID_LevelFireMace (int choice);

static void M_Draw_ID_Level_2 (void);
static void M_ID_LevelBag (int choice);
static void M_ID_LevelAmmo_0 (int choice);
static void M_ID_LevelAmmo_1 (int choice);
static void M_ID_LevelAmmo_2 (int choice);
static void M_ID_LevelAmmo_3 (int choice);
static void M_ID_LevelAmmo_4 (int choice);
static void M_ID_LevelAmmo_5 (int choice);
static void M_ID_LevelKey_0 (int choice);
static void M_ID_LevelKey_1 (int choice);
static void M_ID_LevelKey_2 (int choice);
static void M_ID_LevelFast (int choice);
static void M_ID_LevelRespawn (int choice);

static void M_Draw_ID_Level_3 (void);
static void M_ID_LevelArti_0 (int choice);
static void M_ID_LevelArti_1 (int choice);
static void M_ID_LevelArti_2 (int choice);
static void M_ID_LevelArti_3 (int choice);
static void M_ID_LevelArti_4 (int choice);
static void M_ID_LevelArti_5 (int choice);
static void M_ID_LevelArti_6 (int choice);
static void M_ID_LevelArti_7 (int choice);
static void M_ID_LevelArti_8 (int choice);
static void M_ID_LevelArti_9 (int choice);

static void M_ScrollLevel (int choice);

static void M_ID_SettingReset (int choice);
static void M_ID_ApplyReset (void);

// Keyboard binding prototypes
static boolean KbdIsBinding;
static int     keyToBind;

static char   *M_NameBind (int itemSetOn, int key);
static void    M_StartBind (int keynum);
static void    M_CheckBind (int key);
static void    M_DoBind (int keynum, int key);
static void    M_ClearBind (int itemOn);
static void    M_ResetBinds (void);
static void    M_DrawBindKey (int itemNum, int yPos, int keyBind);
static void    M_DrawBindFooter (char *pagenum, boolean drawPages);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static char   *M_NameMouseBind (int CurrentItPosOn, int btn);
static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int itemOn);
static void    M_DrawBindButton (int itemNum, int yPos, int btnBind);
static void    M_ResetMouseBinds (void);

// Forward declarations for scrolling and remembering last pages.
static Menu_t ID_Def_Video_1;
static Menu_t ID_Def_Video_2;
static Menu_t ID_Def_Keybinds_1;
static Menu_t ID_Def_Keybinds_2;
static Menu_t ID_Def_Keybinds_3;
static Menu_t ID_Def_Keybinds_4;
static Menu_t ID_Def_Keybinds_5;
static Menu_t ID_Def_Keybinds_6;
static Menu_t ID_Def_Keybinds_7;
static Menu_t ID_Def_Keybinds_8;
static Menu_t ID_Def_Gameplay_1;
static Menu_t ID_Def_Gameplay_2;
static Menu_t ID_Def_Gameplay_3;
static Menu_t ID_Def_Gameplay_4;
static Menu_t ID_Def_Level_1;
static Menu_t ID_Def_Level_2;
static Menu_t ID_Def_Level_3;

// Remember last keybindings page.
static int Keybinds_Cur;

static void M_Choose_ID_Keybinds (int choice)
{
    SetMenu(Keybinds_Cur);
}

// Remember last gameplay page.
static int Gameplay_Cur;

static void M_Choose_ID_Gameplay (int choice)
{
    SetMenu(Gameplay_Cur);
}

// [JN/PN] Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // "sfx_switch" sound will be played only if menu will be changed.
    int nextMenu = 0;

    // Save/Load menu:
    if (CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
    {
        if (savepage > 0 && !direction)
        {
            savepage--;
            S_StartSound(NULL, sfx_switch);
        }
        else
        if (savepage < SAVEPAGE_MAX && direction)
        {
            savepage++;
            S_StartSound(NULL, sfx_switch);
        }
        quicksave = -1;
        MN_LoadSlotText();
        return;
    }

    // Video options:
    else if (CurrentMenu == &ID_Def_Video_1) nextMenu = (MENU_ID_VIDEO2);
    else if (CurrentMenu == &ID_Def_Video_2) nextMenu = (MENU_ID_VIDEO1);

    // Keyboard bindings:
    else if (CurrentMenu == &ID_Def_Keybinds_1) nextMenu = (direction ? MENU_ID_KBDBINDS2 : MENU_ID_KBDBINDS8);
    else if (CurrentMenu == &ID_Def_Keybinds_2) nextMenu = (direction ? MENU_ID_KBDBINDS3 : MENU_ID_KBDBINDS1);
    else if (CurrentMenu == &ID_Def_Keybinds_3) nextMenu = (direction ? MENU_ID_KBDBINDS4 : MENU_ID_KBDBINDS2);
    else if (CurrentMenu == &ID_Def_Keybinds_4) nextMenu = (direction ? MENU_ID_KBDBINDS5 : MENU_ID_KBDBINDS3);
    else if (CurrentMenu == &ID_Def_Keybinds_5) nextMenu = (direction ? MENU_ID_KBDBINDS6 : MENU_ID_KBDBINDS4);
    else if (CurrentMenu == &ID_Def_Keybinds_6) nextMenu = (direction ? MENU_ID_KBDBINDS7 : MENU_ID_KBDBINDS5);
    else if (CurrentMenu == &ID_Def_Keybinds_7) nextMenu = (direction ? MENU_ID_KBDBINDS8 : MENU_ID_KBDBINDS6);
    else if (CurrentMenu == &ID_Def_Keybinds_8) nextMenu = (direction ? MENU_ID_KBDBINDS1 : MENU_ID_KBDBINDS7);

    // Gameplay features:
    else if (CurrentMenu == &ID_Def_Gameplay_1) nextMenu = (direction ? MENU_ID_GAMEPLAY2 : MENU_ID_GAMEPLAY3);
    else if (CurrentMenu == &ID_Def_Gameplay_2) nextMenu = (direction ? MENU_ID_GAMEPLAY3 : MENU_ID_GAMEPLAY1);
    else if (CurrentMenu == &ID_Def_Gameplay_3) nextMenu = (direction ? MENU_ID_GAMEPLAY4 : MENU_ID_GAMEPLAY2);
    else if (CurrentMenu == &ID_Def_Gameplay_4) nextMenu = (direction ? MENU_ID_GAMEPLAY1 : MENU_ID_GAMEPLAY3);

    // Level select:
    else if (CurrentMenu == &ID_Def_Level_1) nextMenu = (direction ? MENU_ID_LEVEL2 : MENU_ID_LEVEL3);
    else if (CurrentMenu == &ID_Def_Level_2) nextMenu = (direction ? MENU_ID_LEVEL3 : MENU_ID_LEVEL1);
    else if (CurrentMenu == &ID_Def_Level_3) nextMenu = (direction ? MENU_ID_LEVEL1 : MENU_ID_LEVEL2);

    // If a new menu was set up, play the navigation sound.
    if (nextMenu)
    {
        SetMenu(nextMenu);
        S_StartSound(NULL, sfx_switch);
    }
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
        const int scr = SCREENAREA;
        
        for (int i = 0; i < scr; i++)
        {
            *dest = I_BlendDark(*dest, I_ShadeFactor[shade]);
            ++dest;
        }
    }
}

static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("FLOOR16", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
}

static int M_Line_Alpha (int tics)
{
    const int alpha_val[] = { 0, 50, 100, 150, 200, 255 };
    return alpha_val[tics];
}

#define LINE_ALPHA(i)   M_Line_Alpha(CurrentMenu->items[i].tics)

static void M_Reset_Line_Glow (void)
{
    for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
    {
        CurrentMenu->items[i].tics = 0;
    }

    // [JN] If menu is controlled by mouse, reset "last on" position
    // so this item won't blink upon reentering to the current menu.
    if (menu_mouse_allow)
    {
        CurrentMenu->oldItPos = -1;
    }
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

static void M_DrawScrollPages (int x, int y, int itemOnGlow, const char *pagenum)
{
    char str[32];

    MN_DrTextAGlow("SCROLL PAGES", x, y,
                        cr[CR_MENU_DARK4],
                            cr[CR_MENU_DARK1],
                                LINE_ALPHA(itemOnGlow));

    M_snprintf(str, 32, "%s", M_StringJoin("PAGE ", pagenum, NULL));
    MN_DrTextAGlow(str, M_ItemRightAlign(str), y,
                        cr[CR_MENU_DARK4],
                            cr[CR_MENU_DARK1],
                                LINE_ALPHA(itemOnGlow));
}

// -----------------------------------------------------------------------------
// Main ID Menu
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Main[] = {
    { ITT_SETMENU, "VIDEO OPTIONS",     NULL,                 0, MENU_ID_VIDEO1    },
    { ITT_SETMENU, "DISPLAY OPTIONS",   NULL,                 0, MENU_ID_DISPLAY   },
    { ITT_SETMENU, "SOUND OPTIONS",     NULL,                 0, MENU_ID_SOUND     },
    { ITT_SETMENU, "CONTROL SETTINGS",  NULL,                 0, MENU_ID_CONTROLS  },
    { ITT_SETMENU, "WIDGETS SETTINGS",  NULL,                 0, MENU_ID_WIDGETS   },
    { ITT_SETMENU, "AUTOMAP SETTINGS",  NULL,                 0, MENU_ID_AUTOMAP   },
    { ITT_EFUNC,   "GAMEPLAY FEATURES", M_Choose_ID_Gameplay, 0, MENU_NONE         },
    { ITT_SETMENU, "MISC FEATURES",     NULL,                 0, MENU_ID_MISC      },
    { ITT_SETMENU, "LEVEL SELECT",      NULL,                 0, MENU_ID_LEVEL1    },
    { ITT_EFUNC,   "END GAME",          SCEndGame,            0, MENU_NONE         },
    { ITT_EFUNC,   "RESET SETTINGS",    M_ID_SettingReset,    0, MENU_NONE         },
};

static Menu_t ID_Def_Main = {
    ID_MENU_LEFTOFFSET_SML, ID_MENU_TOPOFFSET,
    M_Draw_ID_Main,
    11, ID_Menu_Main,
    0,
    SmallFont, false, false,
    MENU_MAIN
};

static void M_Draw_ID_Main (void)
{
    MN_DrTextACentered("OPTIONS", 10, cr[CR_YELLOW]);
}

// -----------------------------------------------------------------------------
// Video options 1
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Video_1[] = {
    { ITT_LRFUNC2, "TRUECOLOR RENDERING",  M_ID_TrueColor,    0, MENU_NONE },
    { ITT_LRFUNC1, "RENDERING RESOLUTION", M_ID_RenderingRes, 0, MENU_NONE },
    { ITT_LRFUNC1, "WIDESCREEN MODE",      M_ID_Widescreen,   0, MENU_NONE },
    { ITT_LRFUNC2, "EXCLUSIVE FULLSCREEN", M_ID_ExclusiveFS,  0, MENU_NONE },
    { ITT_LRFUNC1, "UNCAPPED FRAMERATE",   M_ID_UncappedFPS,  0, MENU_NONE },
    { ITT_LRFUNC1, "FRAMERATE LIMIT",      M_ID_LimitFPS,     0, MENU_NONE },
    { ITT_LRFUNC2, "ENABLE VSYNC",         M_ID_VSync,        0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW FPS COUNTER",     M_ID_ShowFPS,      0, MENU_NONE },
    { ITT_LRFUNC2, "PIXEL SCALING",        M_ID_PixelScaling, 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,              0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,              0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,              0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,              0, MENU_NONE },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */ M_ScrollVideo,     0, MENU_NONE },
};

static Menu_t ID_Def_Video_1 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Video_1,
    14, ID_Menu_Video_1,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Video_1 (void)
{
    char str[32];

    MN_DrTextACentered("VIDEO OPTIONS", 10, cr[CR_YELLOW]);

    // Truecolor Rendering
    sprintf(str, vid_truecolor ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        vid_truecolor ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            vid_truecolor ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(0));

    // Rendering resolution
    sprintf(str, vid_resolution == 1 ? "1X (200P)"  :
                 vid_resolution == 2 ? "2X (400P)"  :
                 vid_resolution == 3 ? "3X (600P)"  :
                 vid_resolution == 4 ? "4X (800P)"  :
                 vid_resolution == 5 ? "5X (1000P)" :
                 vid_resolution == 6 ? "6X (1200P)" :
                                       "CUSTOM");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        vid_resolution == 1 ? cr[CR_DARKRED] :
                        vid_resolution == 2 ||
                        vid_resolution == 3 ? cr[CR_GREEN] :
                        vid_resolution == 4 ||
                        vid_resolution == 5 ? cr[CR_YELLOW] : cr[CR_ORANGE_HR],
                            vid_resolution == 1 ? cr[CR_RED_BRIGHT] :
                            vid_resolution == 2 ||
                            vid_resolution == 3 ? cr[CR_GREEN_BRIGHT] :
                            vid_resolution == 4 ||
                            vid_resolution == 5 ? cr[CR_YELLOW_BRIGHT] : cr[CR_ORANGE_HR_BRIGHT],
                                LINE_ALPHA(1));

    // Widescreen mode
    sprintf(str, vid_widescreen == 1 ? "MATCH SCREEN" :
                 vid_widescreen == 2 ? "16:10" :
                 vid_widescreen == 3 ? "16:9" :
                 vid_widescreen == 4 ? "21:9" :
                 vid_widescreen == 5 ? "32:9" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        vid_widescreen ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            vid_widescreen ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(2));

    // Exclusive fullscreen
    sprintf(str, vid_fullscreen_exclusive ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        vid_fullscreen_exclusive ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vid_fullscreen_exclusive ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(3));

    // Uncapped framerate
    sprintf(str, vid_uncapped_fps ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        vid_uncapped_fps ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            vid_uncapped_fps ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(4));


    // Framerate limit
    sprintf(str, !vid_uncapped_fps ? "35" :
                 vid_fpslimit ? "%d" : "NONE", vid_fpslimit);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        !vid_uncapped_fps ? cr[CR_DARKRED] :
                        vid_fpslimit == 0 ? cr[CR_RED] :
                        vid_fpslimit >= 500 ? cr[CR_YELLOW] : cr[CR_GREEN],
                            !vid_uncapped_fps ? cr[CR_RED_BRIGHT] :
                            vid_fpslimit == 0 ? cr[CR_RED_BRIGHT] :
                            vid_fpslimit >= 500 ? cr[CR_YELLOW_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(5));

    // Enable vsync
    sprintf(str, vid_vsync ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                    vid_vsync ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vid_vsync ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Show FPS counter
    sprintf(str, vid_showfps ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                    vid_showfps ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vid_showfps ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Pixel scaling
    sprintf(str, vid_smooth_scaling ? "SMOOTH" : "SHARP");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        vid_smooth_scaling ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vid_smooth_scaling ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // [JN] Print current resolution. Shamelessly taken from Nugget Doom!
    if (CurrentItPos == 1 || CurrentItPos == 2)
    {
        char  width[8];
        char  height[8];
        const char *resolution;

        M_snprintf(width, 8, "%d", (ORIGWIDTH + (WIDESCREENDELTA*2)) * vid_resolution);
        M_snprintf(height, 8, "%d", (ORIGHEIGHT * vid_resolution));
        resolution = M_StringJoin("CURRENT RESOLUTION: ", width, "X", height, NULL);

        MN_DrTextACentered(resolution, 125, cr[CR_LIGHTGRAY_DARK]);
    }

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET, 150, 13, "1/2");
}

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

static void M_ID_TrueColor (int option)
{
    post_rendering_hook = M_ID_TrueColorHook;
}

static void M_ID_RenderingResHook (void)
{
    // [crispy] re-initialize framebuffers, textures and renderer
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    // [crispy] re-calculate framebuffer coordinates
    R_ExecuteSetViewSize();
    // [crispy] re-draw bezel
    R_FillBackScreen();
    // [JN] re-calculate sky texture scaling
    R_InitSkyMap();
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
    // [JN] re-calculate sky texture scaling
    R_InitSkyMap();
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

static void M_ID_VSync (int choice)
{
    vid_vsync ^= 1;
    I_ToggleVsync();
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

static void M_ID_GfxStartup (int choice)
{
    vid_graphical_startup = M_INT_Slider(vid_graphical_startup, 0, 2, choice, false);
}

static void M_ID_ScreenWipe (int choice)
{
    vid_screenwipe_hr ^= 1;
}

static void M_ID_EndText (int option)
{
    vid_endoom = M_INT_Slider(vid_endoom, 0, 2, option, false);
}

// -----------------------------------------------------------------------------
// Video options 2
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Video_2[] = {
    { ITT_LRFUNC2, "GRAPHICAL STARTUP",      M_ID_GfxStartup,        0, MENU_NONE },
    { ITT_LRFUNC2, "SCREEN WIPE EFFECT",     M_ID_ScreenWipe,        0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW ENDTEXT SCREEN",    M_ID_EndText,           0, MENU_NONE },
    { ITT_EMPTY,   NULL,                     NULL,                   0, MENU_NONE },
    { ITT_LRFUNC1, "SUPERSAMPLED SMOOTHING", M_ID_SuperSmoothing,    0, MENU_NONE },
    { ITT_LRFUNC2, "OVERBRIGHT GLOW",        M_ID_OverbrightGlow,    0, MENU_NONE },
    { ITT_LRFUNC2, "SOFT BLOOM",             M_ID_SoftBloom,         0, MENU_NONE },
    { ITT_LRFUNC1, "ANALOG RGB DRIFT",       M_ID_AnalogRGBDrift,    0, MENU_NONE },
    { ITT_LRFUNC2, "VHS LINE DISTORTION",    M_ID_VHSLineDistortion, 0, MENU_NONE },
    { ITT_LRFUNC1, "SCREEN VIGNETTE",        M_ID_ScreenVignette,    0, MENU_NONE },
    { ITT_LRFUNC1, "FILM GRAIN",             M_ID_FilmGrain,         0, MENU_NONE },
    { ITT_LRFUNC1, "MOTION BLUR",            M_ID_MotionBlur,        0, MENU_NONE },
    { ITT_LRFUNC2, "DEPTH OF FIELD BLUR",    M_ID_DepthOfFieldBlur,  0, MENU_NONE },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */   M_ScrollVideo,          0, MENU_NONE },
};

static Menu_t ID_Def_Video_2 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Video_2,
    14, ID_Menu_Video_2,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Video_2 (void)
{
    char str[32];
    const char *sample_factors[] = { "NONE", "1x", "2x", "3x", "4x", "5x", "6x" };

    MN_DrTextACentered("MISCELLANEOUS", 10, cr[CR_YELLOW]);

    // Graphical startup
    sprintf(str, vid_graphical_startup == 1 ? "FAST" :
                 vid_graphical_startup == 2 ? "SLOW" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        vid_graphical_startup == 1 ? cr[CR_GREEN] :
                        vid_graphical_startup == 2 ? cr[CR_YELLOW] : cr[CR_DARKRED],
                            vid_graphical_startup == 1 ? cr[CR_GREEN_BRIGHT] :
                            vid_graphical_startup == 2 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],                        
                                LINE_ALPHA(0));

    // Screen wipe effect
    sprintf(str, vid_screenwipe_hr ? "CROSSFADE" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        vid_screenwipe_hr ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vid_screenwipe_hr ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Show ENDTEXT screen
    sprintf(str, vid_endoom == 1 ? "ALWAYS" :
                 vid_endoom == 2 ? "PWAD ONLY" : "NEVER");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        vid_endoom == 1 ? cr[CR_GREEN] :
                        vid_endoom == 2 ? cr[CR_YELLOW] : cr[CR_DARKRED],
                            vid_endoom == 1 ? cr[CR_GREEN_BRIGHT] : 
                            vid_endoom == 2 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    MN_DrTextACentered("POST-PROCESSING EFFECTS", 50, cr[CR_YELLOW]);

    // Supersampled smoothing
    sprintf(str, "%s", sample_factors[post_supersample]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        post_supersample ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_supersample ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Overbright glow
    sprintf(str, post_overglow ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        post_overglow ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_overglow ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Soft bloom
    sprintf(str, post_bloom ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        post_bloom ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_bloom ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Analog RGB drift
    sprintf(str, post_rgbdrift == 1 ? "SUBTLE" :
                 post_rgbdrift == 2 ? "STRONG" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        post_rgbdrift ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_rgbdrift ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // VHS line distortion
    sprintf(str, post_vhsdist ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        post_vhsdist ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_vhsdist ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Screen vignette
    sprintf(str, post_vignette == 1 ? "SUBTLE" :
                 post_vignette == 2 ? "SOFT"   : 
                 post_vignette == 3 ? "STRONG" : 
                 post_vignette == 4 ? "DARK"   : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        post_vignette ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_vignette ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Film grain
    sprintf(str, post_filmgrain == 1 ? "SOFT"      :
                 post_filmgrain == 2 ? "LIGHT"     : 
                 post_filmgrain == 3 ? "MEDIUM"    : 
                 post_filmgrain == 4 ? "HEAVY"     : 
                 post_filmgrain == 5 ? "NIGHTMARE" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        post_filmgrain ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_filmgrain ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    // Motion blur
    sprintf(str, post_motionblur == 1 ? "SOFT"   :
                 post_motionblur == 2 ? "LIGHT"  : 
                 post_motionblur == 3 ? "MEDIUM" : 
                 post_motionblur == 4 ? "HEAVY"  : 
                 post_motionblur == 5 ? "GHOST"  : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        post_motionblur ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_motionblur ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(11));

    // Depth if field blur
    sprintf(str, post_dofblur ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        post_dofblur ? cr[CR_GREEN] : cr[CR_DARKRED],
                            post_dofblur ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(12));

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET, 150, 13, "2/2");
}

static void M_ID_SuperSmoothing (int choice)
{
    post_supersample = M_INT_Slider(post_supersample, 0, 6, choice, false);
}

static void M_ID_OverbrightGlow (int choice)
{
    post_overglow ^= 1;
}

static void M_ID_SoftBloom (int choice)
{
    post_bloom ^= 1;
}

static void M_ID_AnalogRGBDrift (int choice)
{
    post_rgbdrift = M_INT_Slider(post_rgbdrift, 0, 2, choice, false);
}

static void M_ID_VHSLineDistortion (int choice)
{
    post_vhsdist ^= 1;
}

static void M_ID_ScreenVignette (int choice)
{
    post_vignette = M_INT_Slider(post_vignette, 0, 4, choice, false);
}

static void M_ID_FilmGrain (int choice)
{
    post_filmgrain = M_INT_Slider(post_filmgrain, 0, 5, choice, false);
}

static void M_ID_MotionBlur (int choice)
{
    post_motionblur = M_INT_Slider(post_motionblur, 0, 5, choice, false);
}

static void M_ID_DepthOfFieldBlur (int choice)
{
    post_dofblur ^= 1;
}

static void M_ScrollVideo (int choice)
{
    SetMenu(CurrentMenu == &ID_Def_Video_1 ? MENU_ID_VIDEO2 : MENU_ID_VIDEO1);
    CurrentItPos = 13;
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Display[] = {
    { ITT_LRFUNC1, "FIELD OF VIEW",           M_ID_FOV,             0, MENU_NONE },
    { ITT_LRFUNC1, "MENU BACKGROUND SHADING", M_ID_MenuShading,     0, MENU_NONE },
    { ITT_LRFUNC1, "EXTRA LEVEL BRIGHTNESS",  M_ID_LevelBrightness, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                 0, MENU_NONE },
    { ITT_LRFUNC1, "GAMMA-CORRECTION",        M_ID_Gamma,           0, MENU_NONE },
    { ITT_LRFUNC1, "SATURATION",              M_ID_Saturation,      0, MENU_NONE },
    { ITT_LRFUNC1, "CONTRAST",                M_ID_Contrast,        0, MENU_NONE },
    { ITT_LRFUNC1, "RED INTENSITY",           M_ID_R_Intensity,     0, MENU_NONE },
    { ITT_LRFUNC1, "GREEN INTENSITY",         M_ID_G_Intensity,     0, MENU_NONE },
    { ITT_LRFUNC1, "BLUE INTENSITY",          M_ID_B_Intensity,     0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "MESSAGES ENABLED",        M_ID_Messages,        0, MENU_NONE },
    { ITT_LRFUNC2, "TEXT CASTS SHADOWS",      M_ID_TextShadows,     0, MENU_NONE },
    { ITT_LRFUNC2, "LOCAL TIME",              M_ID_LocalTime,       0, MENU_NONE },
};

static Menu_t ID_Def_Display = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Display,
    14, ID_Menu_Display,
    0,
    SmallFont, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Display (void)
{
    char str[32];

    MN_DrTextACentered("DISPLAY OPTIONS", 10, cr[CR_YELLOW]);

    // Field of View
    sprintf(str, "%d", vid_fov);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        vid_fov == 135 || vid_fov == 70 ? cr[CR_YELLOW] :
                        vid_fov == 90 ? cr[CR_DARKRED] : cr[CR_GREEN],
                            vid_fov == 135 || vid_fov == 70 ? cr[CR_YELLOW_BRIGHT] :
                            vid_fov == 90 ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(0));

    // Background shading
    sprintf(str, dp_menu_shading ? "%d" : "OFF", dp_menu_shading);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        dp_menu_shading == 12 ? cr[CR_YELLOW] :
                        dp_menu_shading  > 0  ? cr[CR_GREEN] : cr[CR_DARKRED],
                            dp_menu_shading == 12 ? cr[CR_YELLOW_BRIGHT] :
                            dp_menu_shading  > 0  ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Extra level brightness
    sprintf(str, dp_level_brightness ? "%d" : "OFF", dp_level_brightness);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        dp_level_brightness == 8 ? cr[CR_YELLOW] :
                        dp_level_brightness  > 0  ? cr[CR_GREEN] : cr[CR_DARKRED],
                            dp_level_brightness == 8 ? cr[CR_YELLOW_BRIGHT] :
                            dp_level_brightness  > 0  ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    MN_DrTextACentered("COLOR SETTINGS", 50, cr[CR_YELLOW]);

    // Gamma-correction num
    sprintf(str, "%s", gammalvls[vid_gamma][1]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        cr[CR_LIGHTGRAY],
                            cr[CR_MENU_BRIGHT5],
                                LINE_ALPHA(4));

    // Saturation
    M_snprintf(str, 6, "%d%%", vid_saturation);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(5));

    // Contrast
    M_snprintf(str, 6, "%3f", vid_contrast);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        cr[CR_YELLOW],
                            cr[CR_YELLOW_BRIGHT],
                                LINE_ALPHA(6));

    // RED intensity
    M_snprintf(str, 6, "%3f", vid_r_intensity);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        cr[CR_RED],
                            cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // GREEN intensity
    M_snprintf(str, 6, "%3f", vid_g_intensity);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        cr[CR_GREEN],
                            cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(8));

    // BLUE intensity
    M_snprintf(str, 6, "%3f", vid_b_intensity);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        cr[CR_BLUE2],
                            cr[CR_BLUE2_BRIGHT],
                                LINE_ALPHA(9));

    MN_DrTextACentered("MESSAGES SETTINGS", 120, cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, msg_show ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        msg_show ? cr[CR_GREEN] : cr[CR_DARKRED],
                            msg_show ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(11));

    // Text casts shadows
    sprintf(str, msg_text_shadows ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        msg_text_shadows ? cr[CR_GREEN] : cr[CR_DARKRED],
                            msg_text_shadows ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(12));

    // Local time
    sprintf(str, msg_local_time == 1 ? "12-HOUR FORMAT" :
                 msg_local_time == 2 ? "24-HOUR FORMAT" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 150,
                        msg_local_time ? cr[CR_GREEN] : cr[CR_DARKRED],
                            msg_local_time ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(13));
}

static void M_ID_FOV (int choice)
{
    vid_fov = M_INT_Slider(vid_fov, 70, 135, choice, true);

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

    I_SetPalette(sb_palette);
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
}

static void M_ID_SaturationHook (void)
{
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
}

static void M_ID_Saturation (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_saturation = M_INT_Slider(vid_saturation, 0, 100, choice, true);
    post_rendering_hook = M_ID_SaturationHook;
}

static void M_ID_ContrastHook (void)
{
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
}

static void M_ID_Contrast (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_contrast = M_FLOAT_Slider(vid_contrast, 0.100000f, 2.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_ContrastHook;
}

static void M_ID_R_IntensityHook (void)
{
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
}

static void M_ID_R_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_r_intensity = M_FLOAT_Slider(vid_r_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_R_IntensityHook;
}

static void M_ID_G_IntensityHook (void)
{
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
}

static void M_ID_G_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_g_intensity = M_FLOAT_Slider(vid_g_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_G_IntensityHook;
}

static void M_ID_B_IntensityHook (void)
{
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
}

static void M_ID_B_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_b_intensity = M_FLOAT_Slider(vid_b_intensity, 0, 1.000000f, 0.025000f, choice, true);
    post_rendering_hook = M_ID_B_IntensityHook;
}

static void M_ID_Messages (int choice)
{
    msg_show ^= 1;
    CT_SetMessage(&players[consoleplayer],
                 DEH_String(msg_show ? "MESSAGES ON" : "MESSAGES OFF"), true, NULL);
    S_StartSound(NULL, sfx_switch);
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

static MenuItem_t ID_Menu_Sound[] = {
    { ITT_SLDR,   "SFX VOLUME",           SCSfxVolume,         MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_SLDR,   "MUSIC VOLUME",         SCMusicVolume,       MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_LRFUNC2, "MUSIC PLAYBACK",       M_ID_MusicSystem, 0, MENU_NONE },
    { ITT_LRFUNC2, "SOUND EFFECTS MODE",   M_ID_SFXMode,     0, MENU_NONE },
    { ITT_LRFUNC2, "PITCH-SHIFTED SOUNDS", M_ID_PitchShift,  0, MENU_NONE },
    { ITT_LRFUNC1, "NUMBER OF SFX TO MIX", M_ID_SFXChannels, 0, MENU_NONE },
    { ITT_LRFUNC2, "MUTE INACTIVE WINDOW", M_ID_MuteInactive,0, MENU_NONE },
};

static Menu_t ID_Def_Sound = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Sound,
    12, ID_Menu_Sound,
    0,
    SmallFont, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Sound (void)
{
    char str[32];

    MN_DrTextACentered("SOUND OPTIONS", 10, cr[CR_YELLOW]);

    DrawSlider(&ID_Def_Sound, 1, 16, snd_MaxVolume, false, 0);
    M_ID_HandleSliderMouseControl(70, 30, 134, &snd_MaxVolume, false, 0, 15);
    sprintf(str,"%d", snd_MaxVolume);
    MN_DrTextAGlow(str, 228, 35, cr[CR_MENU_DARK2], cr[CR_MENU_BRIGHT2], LINE_ALPHA(0));

    DrawSlider(&ID_Def_Sound, 4, 16, snd_MusicVolume, false, 3);
    M_ID_HandleSliderMouseControl(70, 60, 134, &snd_MusicVolume, false, 0, 15);
    sprintf(str,"%d", snd_MusicVolume);
    MN_DrTextAGlow(str, 228, 65, cr[CR_MENU_DARK2], cr[CR_MENU_BRIGHT2], LINE_ALPHA(3));

    MN_DrTextACentered("SOUND SYSTEM", 80, cr[CR_YELLOW]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ?  "GUS (EMULATED)" :
                 snd_musicdevice == 8 ?  "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                         "UNKNOWN");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        snd_musicdevice ? cr[CR_GREEN] : cr[CR_RED],
                            snd_musicdevice ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Sound effects mode
    sprintf(str, snd_monosfx ? "MONO" : "STEREO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        snd_monosfx ? cr[CR_RED] : cr[CR_GREEN],
                            snd_monosfx ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(8));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        snd_pitchshift ? cr[CR_GREEN] : cr[CR_RED],
                            snd_pitchshift ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Number of SFX to mix
    sprintf(str, "%i", snd_channels);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        snd_channels == 8 ? cr[CR_DARKRED] :
                        snd_channels  < 3 || snd_channels == 16 ? cr[CR_YELLOW] : cr[CR_GREEN],
                            snd_channels == 8 ? cr[CR_RED_BRIGHT] :
                            snd_channels  < 3 || snd_channels == 16 ? cr[CR_YELLOW_BRIGHT] : cr[CR_GREEN_BRIGHT],                        
                                LINE_ALPHA(10));

    // Mute inactive window
    sprintf(str, snd_mute_inactive ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        snd_mute_inactive ? cr[CR_GREEN] : cr[CR_RED],
                            snd_mute_inactive ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(11));

    // Inform that music system is not hot-swappable. :(
    if (CurrentItPos == 7)
    {
        if (snd_musicdevice == 5 && strcmp(gus_patch_path, "") == 0)
        {
            MN_DrTextACentered("\"GUS[PATCH[PATH\" VARIABLE IS NOT SET", 140, cr[CR_GRAY]);
        }
#ifdef HAVE_FLUIDSYNTH
        if (snd_musicdevice == 11 && strcmp(fsynth_sf_path, "") == 0)
        {
            MN_DrTextACentered("\"FSYNTH[SF[PATH\" VARIABLE IS NOT SET", 140, cr[CR_GRAY]);
        }
#endif // HAVE_FLUIDSYNTH
    }
}

static void M_ID_MusicSystem (int option)
{
    switch (option)
    {
        case 0:
            if (snd_musicdevice == 0)
            {
                snd_musicdevice = 5;    // Set to GUS
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
                snd_musicdevice  = 5;   // Set to GUS
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

    // [PN] Hot-swap music system
    S_ShutDown();
    I_InitSound(heretic);
    I_InitMusic();
    S_SetMusicVolume();

    // [JN] Enforce music replay while changing music system.
    mus_force_replay = true;

    if (mus_song != -1)
    {
        S_StartSong(mus_song, true);
    }
    else if (idmusnum != -1)
    {
        S_StartSong(idmusnum, true);
    }
    else
    {
        S_StartSong((gameepisode - 1) * 9 + gamemap - 1, true);
    }

    mus_force_replay = false;
}

static void M_ID_SFXMode (int option)
{
    snd_monosfx ^= 1;
}

static void M_ID_PitchShift (int option)
{
    snd_pitchshift ^= 1;
}

static void M_ID_SFXChannels (int option)
{
    // [JN] Note: cap minimum channels to 2, not 1.
    // Only one channel produces a strange effect, 
    // as if there were no channels at all.
    snd_channels = M_INT_Slider(snd_channels, 2, 16, option, true);
}

static void M_ID_MuteInactive (int option)
{
    snd_mute_inactive ^= 1;
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Controls[] = {
    { ITT_EFUNC,   "KEYBOARD BINDINGS",       M_Choose_ID_Keybinds,       0, MENU_NONE          },
    { ITT_SETMENU, "MOUSE BINDINGS",          NULL,                       0, MENU_ID_MOUSEBINDS },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_SLDR,    "HORIZONTAL SENSITIVITY",  SCMouseSensi,               0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_SLDR,    "VERTICAL SENSITIVITY",    SCMouseSensi_y,             0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC1, "ACCELERATION",            M_ID_Controls_Acceleration, 0, MENU_NONE          },
    { ITT_LRFUNC1, "ACCELERATION THRESHOLD",  M_ID_Controls_Threshold,    0, MENU_NONE          },
    { ITT_LRFUNC2,  "MOUSE LOOK",              M_ID_Controls_MLook,        0, MENU_NONE          },
    { ITT_LRFUNC2,  "VERTICAL MOUSE MOVEMENT", M_ID_Controls_NoVert,       0, MENU_NONE          },
    { ITT_LRFUNC2,  "INVERT VERTICAL AXIS",    M_ID_Controls_InvertY,      0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC2,  "PERMANENT \"NOARTISKIP\" MODE", M_ID_Controls_NoArtiSkip, 0, MENU_NONE      },
};

static Menu_t ID_Def_Controls = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Controls,
    16, ID_Menu_Controls,
    0,
    SmallFont, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Controls (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("BINDINGS", 10, cr[CR_YELLOW]);

    MN_DrTextACentered("MOUSE CONFIGURATION", 40, cr[CR_YELLOW]);

    DrawSlider(&ID_Def_Controls, 4, 16, mouseSensitivity, false, 3);
    M_ID_HandleSliderMouseControl(66, 60, 132, &mouseSensitivity, false, 0, 15);
    sprintf(str,"%d", mouseSensitivity);
    MN_DrTextAGlow(str, 227, 65,
                        mouseSensitivity == 255 ? cr[CR_YELLOW] :
                        mouseSensitivity  >  14 ? cr[CR_GREEN] : cr[CR_MENU_DARK2],
                            mouseSensitivity == 255 ? cr[CR_YELLOW_BRIGHT] :
                            mouseSensitivity  >  14 ? cr[CR_GREEN_BRIGHT] : cr[CR_MENU_BRIGHT2],
                                LINE_ALPHA(3));

    DrawSlider(&ID_Def_Controls, 7, 16, mouse_sensitivity_y, false, 6);
    M_ID_HandleSliderMouseControl(66, 90, 132, &mouse_sensitivity_y, false, 0, 15);
    sprintf(str,"%d", mouse_sensitivity_y);
    MN_DrTextAGlow(str, 227, 95,
                        mouse_sensitivity_y == 255 ? cr[CR_YELLOW] :
                        mouse_sensitivity_y  >  14 ? cr[CR_GREEN] : cr[CR_MENU_DARK2],
                            mouse_sensitivity_y == 255 ? cr[CR_YELLOW_BRIGHT] :
                            mouse_sensitivity_y  >  14 ? cr[CR_GREEN_BRIGHT] : cr[CR_MENU_BRIGHT2],
                                LINE_ALPHA(6));

    // Acceleration
    sprintf(str,"%.1f", mouse_acceleration);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        mouse_acceleration == 2.0f ? cr[CR_MENU_DARK2] :
                        mouse_acceleration == 1.0f ? cr[CR_DARKRED] :
                        mouse_acceleration  < 2.0f ? cr[CR_YELLOW] : cr[CR_GREEN],
                            mouse_acceleration == 2.0f ? cr[CR_MENU_BRIGHT2] :
                            mouse_acceleration == 1.0f ? cr[CR_RED_BRIGHT] :
                            mouse_acceleration  < 2.0f ? cr[CR_YELLOW_BRIGHT] : cr[CR_GREEN_BRIGHT],                        
                                LINE_ALPHA(9));

    // Acceleration threshold
    sprintf(str,"%d", mouse_threshold);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        mouse_threshold == 10 ? cr[CR_MENU_DARK2] :
                        mouse_threshold ==  0 ? cr[CR_DARKRED] :
                        mouse_threshold  < 10 ? cr[CR_YELLOW] : cr[CR_GREEN],
                            mouse_threshold == 10 ? cr[CR_MENU_BRIGHT2] :
                            mouse_threshold ==  0 ? cr[CR_RED_BRIGHT] :
                            mouse_threshold  < 10 ? cr[CR_YELLOW_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(10));

    // Mouse look
    sprintf(str, mouse_look ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        mouse_look ? cr[CR_GREEN] : cr[CR_RED],
                            mouse_look ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(11));

    // Vertical mouse movement
    sprintf(str, mouse_novert ? "OFF" : "ON");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        mouse_novert ? cr[CR_RED] : cr[CR_GREEN],
                            mouse_novert ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(12));

    // Invert vertical axis
    sprintf(str, mouse_y_invert ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 150,
                        mouse_y_invert ? cr[CR_GREEN] : cr[CR_RED],
                            mouse_y_invert ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(13));

    MN_DrTextACentered("MISCELLANEOUS", 160, cr[CR_YELLOW]);

    // Permanent "noartiskip" mode
    sprintf(str, ctrl_noartiskip ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 170,
                        ctrl_noartiskip ? cr[CR_GREEN] : cr[CR_RED],
                            ctrl_noartiskip ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(15));
}

static void M_ID_Controls_Acceleration (int option)
{
    mouse_acceleration = M_FLOAT_Slider(mouse_acceleration, 1.000000f, 5.000000f, 0.100000f, option, true);
}

static void M_ID_Controls_Threshold (int option)
{
    mouse_threshold = M_INT_Slider(mouse_threshold, 0, 32, option, true);
}

static void M_ID_Controls_MLook (int option)
{
    mouse_look ^= 1;
    if (!mouse_look)
    {
        players[consoleplayer].centering = true;
    }
}

static void M_ID_Controls_NoVert (int option)
{
    mouse_novert ^= 1;
}

static void M_ID_Controls_InvertY (int choice)
{
    mouse_y_invert ^= 1;
}

static void M_ID_Controls_NoArtiSkip (int choice)
{
    ctrl_noartiskip ^= 1;
}

// -----------------------------------------------------------------------------
// Keybinds 1
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_1[] = {
    { ITT_EFUNC, "MOVE FORWARD",    M_Bind_MoveForward,  0, MENU_NONE },
    { ITT_EFUNC, "MOVE BACKWARD",   M_Bind_MoveBackward, 0, MENU_NONE },
    { ITT_EFUNC, "TURN LEFT",       M_Bind_TurnLeft,     0, MENU_NONE },
    { ITT_EFUNC, "TURN RIGHT",      M_Bind_TurnRight,    0, MENU_NONE },
    { ITT_EFUNC, "STRAFE LEFT",     M_Bind_StrafeLeft,   0, MENU_NONE },
    { ITT_EFUNC, "STRAFE RIGHT",    M_Bind_StrafeRight,  0, MENU_NONE },
    { ITT_EFUNC, "STRAFE ON",       M_Bind_StrafeOn,     0, MENU_NONE },
    { ITT_EFUNC, "SPEED ON",        M_Bind_SpeedOn,      0, MENU_NONE },
    { ITT_EFUNC, "180 DEGREE TURN", M_Bind_180Turn,      0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,                0, MENU_NONE },
    { ITT_EFUNC, "FIRE/ATTACK",     M_Bind_FireAttack,   0, MENU_NONE },
    { ITT_EFUNC, "USE",             M_Bind_Use,          0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_1 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_1,
    12, ID_Menu_Keybinds_1,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_1 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS1;

    M_FillBackground();

    MN_DrTextACentered("MOVEMENT", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_up);
    M_DrawBindKey(1, 30, key_down);
    M_DrawBindKey(2, 40, key_left);
    M_DrawBindKey(3, 50, key_right);
    M_DrawBindKey(4, 60, key_strafeleft);
    M_DrawBindKey(5, 70, key_straferight);
    M_DrawBindKey(6, 80, key_strafe);
    M_DrawBindKey(7, 90, key_speed);
    M_DrawBindKey(8, 100, key_180turn);

    MN_DrTextACentered("ACTION", 110, cr[CR_YELLOW]);

    M_DrawBindKey(10, 120, key_fire);
    M_DrawBindKey(11, 130, key_use);

    M_DrawBindFooter("1", true);
}

static void M_Bind_MoveForward (int option)
{
    M_StartBind(100);  // key_up
}

static void M_Bind_MoveBackward (int option)
{
    M_StartBind(101);  // key_down
}

static void M_Bind_TurnLeft (int option)
{
    M_StartBind(102);  // key_left
}

static void M_Bind_TurnRight (int option)
{
    M_StartBind(103);  // key_right
}

static void M_Bind_StrafeLeft (int option)
{
    M_StartBind(104);  // key_strafeleft
}

static void M_Bind_StrafeRight (int option)
{
    M_StartBind(105);  // key_straferight
}

static void M_Bind_StrafeOn (int option)
{
    M_StartBind(106);  // key_strafe
}

static void M_Bind_SpeedOn (int option)
{
    M_StartBind(107);  // key_speed
}

static void M_Bind_180Turn (int choice)
{
    M_StartBind(108);  // key_180turn
}

static void M_Bind_FireAttack (int option)
{
    M_StartBind(109);  // key_fire
}

static void M_Bind_Use (int option)
{
    M_StartBind(110);  // key_use
}

// -----------------------------------------------------------------------------
// Keybinds 2
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_2[] = {
    { ITT_EFUNC, "LOOK UP",         M_Bind_LookUp,     0, MENU_NONE },
    { ITT_EFUNC, "LOOK DOWN",       M_Bind_LookDown,   0, MENU_NONE },
    { ITT_EFUNC, "CENTER VIEW",     M_Bind_LookCenter, 0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,              0, MENU_NONE },
    { ITT_EFUNC, "FLY UP",          M_Bind_FlyUp,      0, MENU_NONE },
    { ITT_EFUNC, "FLY DOWN",        M_Bind_FlyDown,    0, MENU_NONE },
    { ITT_EFUNC, "STOP FLYING",     M_Bind_FlyCenter,  0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,              0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY LEFT",  M_Bind_InvLeft,    0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY RIGHT", M_Bind_InvRight,   0, MENU_NONE },
    { ITT_EFUNC, "USE ARTIFACT",    M_Bind_UseArti,    0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_2 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_2,
    11, ID_Menu_Keybinds_2,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_2 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS2;

    M_FillBackground();

    MN_DrTextACentered("VIEW", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_lookup);
    M_DrawBindKey(1, 30, key_lookdown);
    M_DrawBindKey(2, 40, key_lookcenter);

    MN_DrTextACentered("FLYING", 50, cr[CR_YELLOW]);

    M_DrawBindKey(4, 60, key_flyup);
    M_DrawBindKey(5, 70, key_flydown);
    M_DrawBindKey(6, 80, key_flycenter);

    MN_DrTextACentered("INVENTORY", 90, cr[CR_YELLOW]);

    M_DrawBindKey(8, 100, key_invleft);
    M_DrawBindKey(9, 110, key_invright);
    M_DrawBindKey(10, 120, key_useartifact);

    M_DrawBindFooter("2", true);
}

static void M_Bind_LookUp (int option)
{
    M_StartBind(200);  // key_lookup
}

static void M_Bind_LookDown (int option)
{
    M_StartBind(201);  // key_lookdown
}

static void M_Bind_LookCenter (int option)
{
    M_StartBind(202);  // key_lookcenter
}

static void M_Bind_FlyUp (int option)
{
    M_StartBind(203);  // key_flyup
}

static void M_Bind_FlyDown (int option)
{
    M_StartBind(204);  // key_flydown
}

static void M_Bind_FlyCenter (int option)
{
    M_StartBind(205);  // key_flycenter
}

static void M_Bind_InvLeft (int option)
{
    M_StartBind(206);  // key_invleft
}

static void M_Bind_InvRight (int option)
{
    M_StartBind(207);  // key_invright
}

static void M_Bind_UseArti (int option)
{
    M_StartBind(208);  // key_useartifact
}

// -----------------------------------------------------------------------------
// Keybinds 3
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_3[] = {
    { ITT_EFUNC, "ALWAYS RUN",              M_Bind_AlwaysRun,     0, MENU_NONE },
    { ITT_EFUNC, "MOUSE LOOK",              M_Bind_MouseLook,     0, MENU_NONE },
    { ITT_EFUNC, "VERTICAL MOUSE MOVEMENT", M_Bind_NoVert,        0, MENU_NONE },
    { ITT_EMPTY, NULL,                      NULL,                 0, MENU_NONE },
    { ITT_EFUNC, "RESTART LEVEL/DEMO",      M_Bind_RestartLevel,  0, MENU_NONE },
    { ITT_EFUNC, "GO TO NEXT LEVEL",        M_Bind_NextLevel,     0, MENU_NONE },
    { ITT_EFUNC, "DEMO FAST-FORWARD",       M_Bind_FastForward,   0, MENU_NONE },
    { ITT_EFUNC, "FLIP LEVEL HORIZONTALLY", M_Bind_FlipLevels,    0, MENU_NONE },
    { ITT_EFUNC, "TOGGLE EXTENDED HUD",     M_Bind_ExtendedHUD,   0, MENU_NONE },
    { ITT_EMPTY, NULL,                      NULL,                 0, MENU_NONE },
    { ITT_EFUNC, "SPECTATOR MODE",          M_Bind_SpectatorMode, 0, MENU_NONE },
    { ITT_EFUNC, "FREEZE MODE",             M_Bind_FreezeMode,    0, MENU_NONE },
    { ITT_EFUNC, "NOTARGET MODE",           M_Bind_NotargetMode,  0, MENU_NONE },
    { ITT_EFUNC, "BUDDHA MODE",             M_Bind_BuddhaMode,    0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_3 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_3,
    14, ID_Menu_Keybinds_3,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_3 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS3;

    M_FillBackground();

    MN_DrTextACentered("ADVANCED MOVEMENT", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_autorun);
    M_DrawBindKey(1, 30, key_mouse_look);
    M_DrawBindKey(2, 40, key_novert);

    MN_DrTextACentered("SPECIAL KEYS", 50, cr[CR_YELLOW]);

    M_DrawBindKey(4, 60, key_reloadlevel);
    M_DrawBindKey(5, 70, key_nextlevel);
    M_DrawBindKey(6, 80, key_demospeed);
    M_DrawBindKey(7, 90, key_flip_levels);
    M_DrawBindKey(8, 100, key_widget_enable);

    MN_DrTextACentered("SPECIAL MODES", 110, cr[CR_YELLOW]);

    M_DrawBindKey(10, 120, key_spectator);
    M_DrawBindKey(11, 130, key_freeze);
    M_DrawBindKey(12, 140, key_notarget);
    M_DrawBindKey(13, 150, key_buddha);

    M_DrawBindFooter("3", true);
}

static void M_Bind_AlwaysRun (int option)
{
    M_StartBind(300);  // key_autorun
}

static void M_Bind_MouseLook (int option)
{
    M_StartBind(301);  // key_mouse_look
}

static void M_Bind_NoVert (int option)
{
    M_StartBind(302);  // key_novert
}

static void M_Bind_RestartLevel (int option)
{
    M_StartBind(303);  // key_reloadlevel
}

static void M_Bind_NextLevel (int option)
{
    M_StartBind(304);  // key_nextlevel
}

static void M_Bind_FastForward (int option)
{
    M_StartBind(305);  // key_demospeed
}

static void M_Bind_FlipLevels (int choice)
{
    M_StartBind(306);  // key_flip_levels
}

static void M_Bind_ExtendedHUD (int choice)
{
    M_StartBind(307);  // key_widget_enable
}

static void M_Bind_SpectatorMode (int option)
{
    M_StartBind(308);  // key_spectator
}

static void M_Bind_FreezeMode (int option)
{
    M_StartBind(309);  // key_freeze
}

static void M_Bind_NotargetMode (int option)
{
    M_StartBind(310);  // key_notarget
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(311);  // key_buddha
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_4[] = {
    { ITT_EFUNC, "WEAPON 1",        M_Bind_Weapon1,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 2",        M_Bind_Weapon2,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 3",        M_Bind_Weapon3,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 4",        M_Bind_Weapon4,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 5",        M_Bind_Weapon5,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 6",        M_Bind_Weapon6,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 7",        M_Bind_Weapon7,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 8",        M_Bind_Weapon8,    0, MENU_NONE },
    { ITT_EFUNC, "PREVIOUS WEAPON", M_Bind_PrevWeapon, 0, MENU_NONE },
    { ITT_EFUNC, "NEXT WEAPON",     M_Bind_NextWeapon, 0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_4 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_4,
    10, ID_Menu_Keybinds_4,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_4 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS4;

    M_FillBackground();

    MN_DrTextACentered("WEAPONS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_weapon1);
    M_DrawBindKey(1, 30, key_weapon2);
    M_DrawBindKey(2, 40, key_weapon3);
    M_DrawBindKey(3, 50, key_weapon4);
    M_DrawBindKey(4, 60, key_weapon5);
    M_DrawBindKey(5, 70, key_weapon6);
    M_DrawBindKey(6, 80, key_weapon7);
    M_DrawBindKey(7, 90, key_weapon8);
    M_DrawBindKey(8, 100, key_prevweapon);
    M_DrawBindKey(9, 110, key_nextweapon);

    M_DrawBindFooter("4", true);
}

static void M_Bind_Weapon1 (int option)
{
    M_StartBind(400);  // key_weapon1
}

static void M_Bind_Weapon2 (int option)
{
    M_StartBind(401);  // key_weapon2
}

static void M_Bind_Weapon3 (int option)
{
    M_StartBind(402);  // key_weapon3
}

static void M_Bind_Weapon4 (int option)
{
    M_StartBind(403);  // key_weapon4
}

static void M_Bind_Weapon5 (int option)
{
    M_StartBind(404);  // key_weapon5
}

static void M_Bind_Weapon6 (int option)
{
    M_StartBind(405);  // key_weapon6
}

static void M_Bind_Weapon7 (int option)
{
    M_StartBind(406);  // key_weapon7
}

static void M_Bind_Weapon8 (int option)
{
    M_StartBind(407);  // key_weapon8
}

static void M_Bind_PrevWeapon (int option)
{
    M_StartBind(408);  // key_prevweapon
}

static void M_Bind_NextWeapon (int option)
{
    M_StartBind(409);  // key_nextweapon
}

// -----------------------------------------------------------------------------
// Keybinds 5
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_5[] = {
    { ITT_EFUNC, "QUARTZ FLASK",          M_Bind_Quartz,       0, MENU_NONE },
    { ITT_EFUNC, "MYSTIC URN",            M_Bind_Urn,          0, MENU_NONE },
    { ITT_EFUNC, "TIMEBOMB",              M_Bind_Bomb,         0, MENU_NONE },
    { ITT_EFUNC, "TOME OF POWER",         M_Bind_Tome,         0, MENU_NONE },
    { ITT_EFUNC, "RING OF INVINCIBILITY", M_Bind_Ring,         0, MENU_NONE },
    { ITT_EFUNC, "CHAOS DEVICE",          M_Bind_Chaosdevice,  0, MENU_NONE },
    { ITT_EFUNC, "SHADOWSPHERE",          M_Bind_Shadowsphere, 0, MENU_NONE },
    { ITT_EFUNC, "WINGS OF WRATH",        M_Bind_Wings,        0, MENU_NONE },
    { ITT_EFUNC, "TORCH",                 M_Bind_Torch,        0, MENU_NONE },
    { ITT_EFUNC, "MORPH OVUM",            M_Bind_Morph,        0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_5 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_5,
    10, ID_Menu_Keybinds_5,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_5 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS5;

    M_FillBackground();

    MN_DrTextACentered("ARTIFACTS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_arti_quartz);
    M_DrawBindKey(1, 30, key_arti_urn);
    M_DrawBindKey(2, 40, key_arti_bomb);
    M_DrawBindKey(3, 50, key_arti_tome);
    M_DrawBindKey(4, 60, key_arti_ring);
    M_DrawBindKey(5, 70, key_arti_chaosdevice);
    M_DrawBindKey(6, 80, key_arti_shadowsphere);
    M_DrawBindKey(7, 90, key_arti_wings);
    M_DrawBindKey(8, 100, key_arti_torch);
    M_DrawBindKey(9, 110, key_arti_morph);

    M_DrawBindFooter("5", true);
}

static void M_Bind_Quartz (int option)
{
    M_StartBind(500);  // key_arti_quartz
}

static void M_Bind_Urn (int option)
{
    M_StartBind(501);  // key_arti_urn
}

static void M_Bind_Bomb (int option)
{
    M_StartBind(502);  // key_arti_bomb
}

static void M_Bind_Tome (int option)
{
    M_StartBind(503);  // key_arti_tome
}

static void M_Bind_Ring (int option)
{
    M_StartBind(504);  // key_arti_ring
}

static void M_Bind_Chaosdevice (int option)
{
    M_StartBind(505);  // key_arti_chaosdevice
}

static void M_Bind_Shadowsphere (int option)
{
    M_StartBind(506);  // key_arti_shadowsphere
}

static void M_Bind_Wings (int option)
{
    M_StartBind(507);  // key_arti_wings
}

static void M_Bind_Torch (int option)
{
    M_StartBind(508);  // key_arti_torch
}

static void M_Bind_Morph (int option)
{
    M_StartBind(509);  // key_arti_morph
}

// -----------------------------------------------------------------------------
// Keybinds 6
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_6[] = {
    { ITT_EFUNC, "TOGGLE MAP",       M_Bind_ToggleMap,   0, MENU_NONE },
    { ITT_EFUNC, "ZOOM IN",          M_Bind_ZoomIn,      0, MENU_NONE },
    { ITT_EFUNC, "ZOOM OUT",         M_Bind_ZoomOut,     0, MENU_NONE },
    { ITT_EFUNC, "MAXIMUM ZOOM OUT", M_Bind_MaxZoom,     0, MENU_NONE },
    { ITT_EFUNC, "FOLLOW MODE",      M_Bind_FollowMode,  0, MENU_NONE },
    { ITT_EFUNC, "ROTATE MODE",      M_Bind_RotateMode,  0, MENU_NONE },
    { ITT_EFUNC, "OVERLAY MODE",     M_Bind_OverlayMode, 0, MENU_NONE },
    { ITT_EFUNC, "TOGGLE GRID",      M_Bind_ToggleGrid,  0, MENU_NONE },
    { ITT_EFUNC, "MARK LOCATION",    M_Bind_AddMark,     0, MENU_NONE },
    { ITT_EFUNC, "CLEAR ALL MARKS",  M_Bind_ClearMarks,  0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_6 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_6,
    10, ID_Menu_Keybinds_6,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_6 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS6;

    M_FillBackground();

    MN_DrTextACentered("AUTOMAP", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_map_toggle);
    M_DrawBindKey(1, 30, key_map_zoomin);
    M_DrawBindKey(2, 40, key_map_zoomout);
    M_DrawBindKey(3, 50, key_map_maxzoom);
    M_DrawBindKey(4, 60, key_map_follow);
    M_DrawBindKey(5, 70, key_map_rotate);
    M_DrawBindKey(6, 80, key_map_overlay);
    M_DrawBindKey(7, 90, key_map_grid);
    M_DrawBindKey(8, 100, key_map_mark);
    M_DrawBindKey(9, 110, key_map_clearmark);

    M_DrawBindFooter("6", true);
}

static void M_Bind_ToggleMap (int option)
{
    M_StartBind(600);  // key_map_toggle
}

static void M_Bind_ZoomIn (int option)
{
    M_StartBind(601);  // key_map_zoomin
}

static void M_Bind_ZoomOut (int option)
{
    M_StartBind(602);  // key_map_zoomout
}

static void M_Bind_MaxZoom (int option)
{
    M_StartBind(603);  // key_map_maxzoom
}

static void M_Bind_FollowMode (int option)
{
    M_StartBind(604);  // key_map_follow
}

static void M_Bind_RotateMode (int option)
{
    M_StartBind(605);  // key_map_rotate
}

static void M_Bind_OverlayMode (int option)
{
    M_StartBind(606);  // key_map_overlay
}

static void M_Bind_ToggleGrid (int option)
{
    M_StartBind(607);  // key_map_grid
}

static void M_Bind_AddMark (int option)
{
    M_StartBind(608);  // key_map_mark
}

static void M_Bind_ClearMarks (int option)
{
    M_StartBind(609);  // key_map_clearmark
}

// -----------------------------------------------------------------------------
// Keybinds 7
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_7[] = {
    {ITT_EFUNC, "HELP SCREEN",     M_Bind_HelpScreen,     0, MENU_NONE},
    {ITT_EFUNC, "SAVE GAME",       M_Bind_SaveGame,       0, MENU_NONE},
    {ITT_EFUNC, "LOAD GAME",       M_Bind_LoadGame,       0, MENU_NONE},
    {ITT_EFUNC, "SOUND VOLUME",    M_Bind_SoundVolume,    0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE DETAIL",   M_Bind_ToggleDetail,   0, MENU_NONE},
    {ITT_EFUNC, "QUICK SAVE",      M_Bind_QuickSave,      0, MENU_NONE},
    {ITT_EFUNC, "END GAME",        M_Bind_EndGame,        0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE MESSAGES", M_Bind_ToggleMessages, 0, MENU_NONE},
    {ITT_EFUNC, "QUICK LOAD",      M_Bind_QuickLoad,      0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME",       M_Bind_QuitGame,       0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE GAMMA",    M_Bind_ToggleGamma,    0, MENU_NONE},
    {ITT_EFUNC, "MULTIPLAYER SPY", M_Bind_MultiplayerSpy, 0, MENU_NONE}
};

static Menu_t ID_Def_Keybinds_7 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_7,
    12, ID_Menu_Keybinds_7,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_7 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS7;

    M_FillBackground();

    MN_DrTextACentered("FUNCTION KEYS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_menu_help);
    M_DrawBindKey(1, 30, key_menu_save);
    M_DrawBindKey(2, 40, key_menu_load);
    M_DrawBindKey(3, 50, key_menu_volume);
    M_DrawBindKey(4, 60, key_menu_detail);
    M_DrawBindKey(5, 70, key_menu_qsave);
    M_DrawBindKey(6, 80, key_menu_endgame);
    M_DrawBindKey(7, 90, key_menu_messages);
    M_DrawBindKey(8, 100, key_menu_qload);
    M_DrawBindKey(9, 110, key_menu_quit);
    M_DrawBindKey(10, 120, key_menu_gamma);
    M_DrawBindKey(11, 130, key_spy);

    M_DrawBindFooter("7", true);
}

static void M_Bind_HelpScreen (int option)
{
    M_StartBind(700);  // key_menu_help
}

static void M_Bind_SaveGame (int option)
{
    M_StartBind(701);  // key_menu_save
}

static void M_Bind_LoadGame (int option)
{
    M_StartBind(702);  // key_menu_load
}

static void M_Bind_SoundVolume (int option)
{
    M_StartBind(703);  // key_menu_volume
}

static void M_Bind_ToggleDetail (int option)
{
    M_StartBind(704);  // key_menu_detail
}

static void M_Bind_QuickSave (int option)
{
    M_StartBind(705);  // key_menu_qsave
}

static void M_Bind_EndGame (int option)
{
    M_StartBind(706);  // key_menu_endgame
}

static void M_Bind_ToggleMessages (int option)
{
    M_StartBind(707);  // key_menu_messages
}

static void M_Bind_QuickLoad (int option)
{
    M_StartBind(708);  // key_menu_qload
}

static void M_Bind_QuitGame (int option)
{
    M_StartBind(709);  // key_menu_quit
}

static void M_Bind_ToggleGamma (int option)
{
    M_StartBind(710);  // key_menu_gamma
}

static void M_Bind_MultiplayerSpy (int option)
{
    M_StartBind(711);  // key_spy
}

// -----------------------------------------------------------------------------
// Keybinds 8
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_8[] = {
    {ITT_EFUNC, "PAUSE GAME",            M_Bind_Pause,          0, MENU_NONE},
    {ITT_EFUNC, "SAVE A SCREENSHOT",     M_Bind_SaveScreenshot, 0, MENU_NONE},
    {ITT_EFUNC, "DISPLAY LAST MESSAGE",  M_Bind_LastMessage,    0, MENU_NONE},
    {ITT_EFUNC, "FINISH DEMO RECORDING", M_Bind_FinishDemo,     0, MENU_NONE},
    {ITT_EMPTY, NULL,                    NULL,                  0, MENU_NONE},
    {ITT_EFUNC, "SEND MESSAGE",          M_Bind_SendMessage,    0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 1",         M_Bind_ToPlayer1,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 2",         M_Bind_ToPlayer2,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 3",         M_Bind_ToPlayer3,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 4",         M_Bind_ToPlayer4,      0, MENU_NONE},
    {ITT_EMPTY, NULL,                    NULL,                  0, MENU_NONE},
    {ITT_EFUNC, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,      0, MENU_NONE},
};

static Menu_t ID_Def_Keybinds_8 = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_8,
    12, ID_Menu_Keybinds_8,
    0,
    SmallFont, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_8 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS8;

    M_FillBackground();

    MN_DrTextACentered("SHORTCUT KEYS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_pause);
    M_DrawBindKey(1, 30, key_menu_screenshot);
    M_DrawBindKey(2, 40, key_message_refresh_hr);
    M_DrawBindKey(3, 50, key_demo_quit);

    MN_DrTextACentered("MULTIPLAYER", 60, cr[CR_YELLOW]);

    M_DrawBindKey(5, 70, key_multi_msg);
    M_DrawBindKey(6, 80, key_multi_msgplayer[0]);
    M_DrawBindKey(7, 90, key_multi_msgplayer[1]);
    M_DrawBindKey(8, 100, key_multi_msgplayer[2]);
    M_DrawBindKey(9, 110, key_multi_msgplayer[3]);

    MN_DrTextACentered("RESET", 120, cr[CR_YELLOW]);

    M_DrawBindFooter("8", true);
}

static void M_Bind_Pause (int option)
{
    M_StartBind(800);  // key_pause
}

static void M_Bind_SaveScreenshot (int option)
{
    M_StartBind(801);  // key_menu_screenshot
}

static void M_Bind_LastMessage (int choice)
{
    M_StartBind(802);  // key_message_refresh_hr
}

static void M_Bind_FinishDemo (int option)
{
    M_StartBind(803);  // key_demo_quit
}

static void M_Bind_SendMessage (int option)
{
    M_StartBind(804);  // key_multi_msg
}

static void M_Bind_ToPlayer1 (int option)
{
    M_StartBind(805);  // key_multi_msgplayer[0]
}

static void M_Bind_ToPlayer2 (int option)
{
    M_StartBind(806);  // key_multi_msgplayer[1]
}

static void M_Bind_ToPlayer3 (int option)
{
    M_StartBind(807);  // key_multi_msgplayer[2]
}

static void M_Bind_ToPlayer4 (int option)
{
    M_StartBind(808);  // key_multi_msgplayer[3]
}

static void M_Bind_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 6;      // [JN] keybinds reset
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_MouseBinds[] = {
    { ITT_EFUNC, "FIRE/ATTACK",               M_Bind_M_FireAttack,     0, MENU_NONE },
    { ITT_EFUNC, "MOVE FORWARD",              M_Bind_M_MoveForward,    0, MENU_NONE },
    { ITT_EFUNC, "MOVE BACKWARD",             M_Bind_M_MoveBackward,   0, MENU_NONE },
    { ITT_EFUNC, "USE",                       M_Bind_M_Use,            0, MENU_NONE },
    { ITT_EFUNC, "SPEED ON",                  M_Bind_M_SpeedOn,        0, MENU_NONE },
    { ITT_EFUNC, "STRAFE ON",                 M_Bind_M_StrafeOn,       0, MENU_NONE },
    { ITT_EFUNC, "STRAFE LEFT",               M_Bind_M_StrafeLeft,     0, MENU_NONE },
    { ITT_EFUNC, "STRAFE RIGHT",              M_Bind_M_StrafeRight,    0, MENU_NONE },
    { ITT_EFUNC, "PREV WEAPON",               M_Bind_M_PrevWeapon,     0, MENU_NONE },
    { ITT_EFUNC, "NEXT WEAPON",               M_Bind_M_NextWeapon,     0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY LEFT",            M_Bind_M_InventoryLeft,  0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY RIGHT",           M_Bind_M_InventoryRight, 0, MENU_NONE },
    { ITT_EFUNC, "USE ARTIFACT",              M_Bind_M_UseArtifact,    0, MENU_NONE },
    { ITT_EMPTY, NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EFUNC, "RESET BINDINGS TO DEFAULT", M_Bind_M_Reset,          0, MENU_NONE },
};

static Menu_t ID_Def_MouseBinds = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_MouseBinds,
    15, ID_Menu_MouseBinds,
    0,
    SmallFont, false, false,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_MouseBinds (void)
{
    M_FillBackground();

    MN_DrTextACentered("MOUSE BINDINGS", 10, cr[CR_YELLOW]);

    M_DrawBindButton(0, 20, mousebfire);
    M_DrawBindButton(1, 30, mousebforward);
    M_DrawBindButton(2, 40, mousebbackward);
    M_DrawBindButton(3, 50, mousebuse);
    M_DrawBindButton(4, 60, mousebspeed);
    M_DrawBindButton(5, 70, mousebstrafe);
    M_DrawBindButton(6, 80, mousebstrafeleft);
    M_DrawBindButton(7, 90, mousebstraferight);
    M_DrawBindButton(8, 100, mousebprevweapon);
    M_DrawBindButton(9, 110, mousebnextweapon);
    M_DrawBindButton(10, 120, mousebinvleft);
    M_DrawBindButton(11, 130, mousebinvright);
    M_DrawBindButton(12, 140, mousebuseartifact);

    MN_DrTextACentered("RESET", 150, cr[CR_YELLOW]);

    M_DrawBindFooter(NULL, false);
}

static void M_Bind_M_FireAttack (int option)
{
    M_StartMouseBind(1000);  // mousebfire
}

static void M_Bind_M_MoveForward (int option)
{
    M_StartMouseBind(1001);  // mousebforward
}

static void M_Bind_M_MoveBackward (int option)
{
    M_StartMouseBind(1002);  // mousebbackward
}

static void M_Bind_M_Use (int option)
{
    M_StartMouseBind(1003);  // mousebuse
}

static void M_Bind_M_SpeedOn (int option)
{
    M_StartMouseBind(1004);  // mousebspeed
}

static void M_Bind_M_StrafeOn (int option)
{
    M_StartMouseBind(1005);  // mousebstrafe
}

static void M_Bind_M_StrafeLeft (int option)
{
    M_StartMouseBind(1006);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int option)
{
    M_StartMouseBind(1007);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int option)
{
    M_StartMouseBind(1008);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int option)
{
    M_StartMouseBind(1009);  // mousebnextweapon
}

static void M_Bind_M_InventoryLeft (int option)
{
    M_StartMouseBind(1010);  // mousebinvleft
}

static void M_Bind_M_InventoryRight (int option)
{
    M_StartMouseBind(1011);  // mousebinvright
}

static void M_Bind_M_UseArtifact (int option)
{
    M_StartMouseBind(1012);  // mousebuseartifact
}

static void M_Bind_M_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 7;      // [JN] mouse binds reset
}

// -----------------------------------------------------------------------------
// Widget settings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Widgets[] = {
    { ITT_LRFUNC2, "COLOR SCHEME",     M_ID_Widget_Colors,     0, MENU_NONE },
    { ITT_LRFUNC2, "PLACEMENT",        M_ID_Widget_Placement,  0, MENU_NONE },
    { ITT_LRFUNC2, "ALIGNMENT",        M_ID_Widget_Alignment,  0, MENU_NONE },
    { ITT_LRFUNC2, "KIS STATS",        M_ID_Widget_KIS,        0, MENU_NONE },
    { ITT_LRFUNC2, "- STATS FORMAT",   M_ID_Widget_KIS_Format, 0, MENU_NONE },
    { ITT_LRFUNC2, "- SHOW ITEMS",     M_ID_Widget_KIS_Items,  0, MENU_NONE },
    { ITT_LRFUNC2, "LEVEL/DM TIMER",   M_ID_Widget_Time,       0, MENU_NONE },
    { ITT_LRFUNC2, "TOTAL TIME",       M_ID_Widget_TotalTime,  0, MENU_NONE },
    { ITT_LRFUNC2, "LEVEL NAME",       M_ID_Widget_LevelName,  0, MENU_NONE },
    { ITT_LRFUNC2, "PLAYER COORDS",    M_ID_Widget_Coords,     0, MENU_NONE },
    { ITT_LRFUNC2, "PLAYER SPEED",     M_ID_Widget_Speed,      0, MENU_NONE },
    { ITT_LRFUNC2, "RENDER COUNTERS",  M_ID_Widget_Render,     0, MENU_NONE },
    { ITT_LRFUNC2, "TARGET'S HEALTH",  M_ID_Widget_Health,     0, MENU_NONE },
};

static Menu_t ID_Def_Widgets = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Widgets,
    13, ID_Menu_Widgets,
    0,
    SmallFont, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Widgets (void)
{
    char str[32];

    MN_DrTextACentered("WIDGETS", 10, cr[CR_YELLOW]);

    // Color scheme
    sprintf(str, widget_scheme == 1 ? "INTER"  :
                 widget_scheme == 2 ? "CRISPY" :
                 widget_scheme == 3 ? "WOOF"   :
                 widget_scheme == 4 ? "DSDA"   : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        widget_scheme ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            widget_scheme ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(0));

    // Placement
    sprintf(str, widget_location ? "TOP" : "BOTTOM");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        cr[CR_GREEN],
                            cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(1));

    // Alignment
    sprintf(str, widget_alignment == 1 ? "STATUS BAR" :
                 widget_alignment == 2 ? "AUTO" : "LEFT");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        widget_alignment ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            widget_alignment ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(2));

    // K/I/S stats
    sprintf(str, widget_kis == 1 ? "ALWAYS"  :
                 widget_kis == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        widget_kis ? cr[CR_GREEN] : cr[CR_DARKRED], 
                            widget_kis ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT], 
                                LINE_ALPHA(3));

    // Stats format
    sprintf(str, widget_kis_format == 1 ? "REMAINING" :
                 widget_kis_format == 2 ? "PERCENT" : "RATIO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        widget_kis_format ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_kis_format ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Show items
    sprintf(str, widget_kis_items ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        widget_kis_items ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_kis_items ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Level/DM timer
    sprintf(str, widget_time == 1 ? "ALWAYS"  :
                 widget_time == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        widget_time ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_time ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Total time
    sprintf(str, widget_totaltime == 1 ? "ALWAYS"  :
                 widget_totaltime == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        widget_totaltime ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_totaltime ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Level name
    sprintf(str, widget_levelname ? "ALWAYS" : "AUTOMAP");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        widget_levelname ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_levelname ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Player coords
    sprintf(str, widget_coords == 1 ? "ALWAYS"  :
                 widget_coords == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        widget_coords ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_coords ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Player speed
    sprintf(str, widget_speed ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        widget_speed ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_speed ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    // Render counters
    sprintf(str, widget_render ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        widget_render ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_render ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(11));

    // Target's health
    sprintf(str, widget_health == 1 ? "TOP" :
                 widget_health == 2 ? "TOP+NAME" :
                 widget_health == 3 ? "BOTTOM" :
                 widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        widget_health ? cr[CR_GREEN] : cr[CR_DARKRED],
                            widget_health ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(12));
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

static void M_ID_Widget_KIS_Items (int choice)
{
    widget_kis_items ^= 1;
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

static void M_ID_Widget_Speed (int choice)
{
    widget_speed ^= 1;
}

static void M_ID_Widget_Render (int choice)
{
    widget_render ^= 1;
}

static void M_ID_Widget_Health (int choice)
{
    widget_health = M_INT_Slider(widget_health, 0, 4, choice, false);
}

// -----------------------------------------------------------------------------
// Automap settings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Automap[] = {
    { ITT_LRFUNC2, "LINE SMOOTHING",        M_ID_Automap_Smooth,   0, MENU_NONE },
    { ITT_LRFUNC1, "LINE THICKNESS",        M_ID_Automap_Thick,    0, MENU_NONE },
    { ITT_LRFUNC2, "SQUARE ASPECT RATIO",   M_ID_Automap_Square,   0, MENU_NONE },
    { ITT_LRFUNC2, "BACKGROUND STYLE",      M_ID_Automap_TexturedBg, 0, MENU_NONE },
    { ITT_LRFUNC2, "SCROLL BACKGROUND"  ,   M_ID_Automap_ScrollBg, 0, MENU_NONE },
    { ITT_LRFUNC2, "MARK SECRET SECTORS",   M_ID_Automap_Secrets,  0, MENU_NONE },
    { ITT_LRFUNC2, "ROTATE MODE",           M_ID_Automap_Rotate,   0, MENU_NONE },
    { ITT_LRFUNC2, "OVERLAY MODE",          M_ID_Automap_Overlay,  0, MENU_NONE },
    { ITT_LRFUNC1, "OVERLAY SHADING LEVEL", M_ID_Automap_Shading,  0, MENU_NONE },
};

static Menu_t ID_Def_Automap = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Automap,
    9, ID_Menu_Automap,
    0,
    SmallFont, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Automap (void)
{
    char str[32];
    const char *thickness[] = {
        "DEFAULT","2X","3X","4X","5X","6X","AUTO"
    };

    MN_DrTextACentered("AUTOMAP", 10, cr[CR_YELLOW]);

    // Line smoothing
    sprintf(str, automap_smooth_hr ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        automap_smooth_hr ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_smooth_hr ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Line thickness
    sprintf(str, "%s", thickness[automap_thick]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        automap_thick ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_thick ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Square aspect ratio
    sprintf(str, automap_square ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        automap_square ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_square ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // Background style
    sprintf(str, automap_textured_bg ? "TEXTURED" : "BLACK");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        !automap_textured_bg ? cr[CR_GREEN] : cr[CR_DARKRED],
                            !automap_textured_bg ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    // Scroll background
    sprintf(str, automap_scroll_bg ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        automap_scroll_bg && automap_textured_bg ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_scroll_bg && automap_textured_bg ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Mark secret sectors
    sprintf(str, automap_secrets == 1 ? "REVEALED" :
                 automap_secrets == 2 ? "ALWAYS" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        automap_secrets ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_secrets ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Rotate mode
    sprintf(str, automap_rotate ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        automap_rotate ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_rotate ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Overlay mode
    sprintf(str, automap_overlay ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        automap_overlay ? cr[CR_GREEN] : cr[CR_DARKRED],
                            automap_overlay ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Overlay shading level
    sprintf(str,"%d", automap_shading);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        !automap_overlay ? cr[CR_DARKRED] :
                         automap_shading ==  0 ? cr[CR_RED] :
                         automap_shading == 12 ? cr[CR_YELLOW] : cr[CR_GREEN],
                            !automap_overlay ? cr[CR_RED_BRIGHT] :
                             automap_shading ==  0 ? cr[CR_RED_BRIGHT] :
                             automap_shading == 12 ? cr[CR_YELLOW_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(8));
}

static void M_ID_Automap_Smooth (int choice)
{
    automap_smooth_hr ^= 1;
}

static void M_ID_Automap_Thick (int choice)
{
    automap_thick = M_INT_Slider(automap_thick, 0, 6, choice, false);
}

static void M_ID_Automap_Square (int choice)
{
    automap_square ^= 1;
}

static void M_ID_Automap_TexturedBg (int choice)
{
    automap_textured_bg ^= 1;
    // [JN] Reinitialize pointer to antialiased tables for line drawing.
    AM_initOverlayMode();
}

static void M_ID_Automap_ScrollBg (int choice)
{
    automap_scroll_bg ^= 1;
}

static void M_ID_Automap_Secrets (int choice)
{
    automap_secrets = M_INT_Slider(automap_secrets, 0, 2, choice, false);
}

static void M_ID_Automap_Rotate (int choice)
{
    automap_rotate ^= 1;
}

static void M_ID_Automap_Overlay (int choice)
{
    automap_overlay ^= 1;
    // [JN] Reinitialize pointer to antialiased tables for line drawing.
    AM_initOverlayMode();
}

static void M_ID_Automap_Shading (int choice)
{
    automap_shading = M_INT_Slider(automap_shading, 0, 12, choice, true);
}

// -----------------------------------------------------------------------------
// Gameplay features 1
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Gameplay_1[] = {
    { ITT_LRFUNC1, "BRIGHTMAPS",                  M_ID_Brightmaps,      0, MENU_NONE },
    { ITT_LRFUNC2, "EXTRA TRANSLUCENCY",          M_ID_Translucency,    0, MENU_NONE },
    { ITT_LRFUNC1, "FAKE CONTRAST",               M_ID_FakeContrast,    0, MENU_NONE },
    { ITT_LRFUNC1, "DIMINISHED LIGHTING",         M_ID_SmoothLighting,  0, MENU_NONE },
    { ITT_LRFUNC1, "PALETTE FADING EFFECT",       M_ID_SmoothPalette,   0, MENU_NONE },
    { ITT_LRFUNC1, "LIQUIDS ANIMATION",           M_ID_SwirlingLiquids, 0, MENU_NONE },
    { ITT_LRFUNC1, "INVULNERABILITY AFFECTS SKY", M_ID_InvulSky,        0, MENU_NONE },
    { ITT_LRFUNC1, "SKY DRAWING MODE",            M_ID_LinearSky,       0, MENU_NONE },
    { ITT_LRFUNC1, "RANDOMLY MIRRORED CORPSES",   M_ID_FlipCorpses,     0, MENU_NONE },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "SHAPE",                       M_ID_Crosshair,       0, MENU_NONE },
    { ITT_LRFUNC2, "INDICATION",                  M_ID_CrosshairColor,  0, MENU_NONE },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */        M_ScrollGameplay,     0, MENU_NONE },
};

static Menu_t ID_Def_Gameplay_1 = {
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_1,
    14, ID_Menu_Gameplay_1,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_1 (void)
{
    char str[32];
    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY1;

    MN_DrTextACentered("VISUAL", 10, cr[CR_YELLOW]);

    // Brightmaps
    sprintf(str, vis_brightmaps == 1 ? "STANDARD" :
                 vis_brightmaps == 2 ? "FULL" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        vis_brightmaps ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_brightmaps ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Translucency
    sprintf(str, vis_translucency == 1 ? "ADDITIVE" :
                 vis_translucency == 2 ? "BLENDING" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        vis_translucency ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_translucency ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Fake contrast
    sprintf(str, vis_fake_contrast ? "ORIGINAL" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        vis_fake_contrast ? cr[CR_DARKRED] : cr[CR_GREEN],
                            vis_fake_contrast ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(2));

    // Diminished lighting
    sprintf(str, vis_smooth_light ? "SMOOTH" : "ORIGINAL");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        vis_smooth_light ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_smooth_light ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    // Palette fading effect
    sprintf(str, vis_smooth_palette ? "SMOOTH" : "ORIGINAL");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        vis_smooth_palette ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_smooth_palette ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Liquids animation
    sprintf(str, vis_swirling_liquids ? "IMPROVED" : "ORIGINAL");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        vis_swirling_liquids ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_swirling_liquids ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Invulnerability affects sky
    sprintf(str, vis_invul_sky ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        vis_invul_sky ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_invul_sky ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Sky drawing mode
    sprintf(str, vis_linear_sky ? "LINEAR" : "ORIGINAL");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        vis_linear_sky ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_linear_sky ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Randomly mirrored corpses
    sprintf(str, vis_flip_corpses ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        vis_flip_corpses ? cr[CR_GREEN] : cr[CR_DARKRED],
                            vis_flip_corpses ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    MN_DrTextACentered("CROSSHAIR", 110, cr[CR_YELLOW]);

    // Crosshair shape
    sprintf(str, xhair_draw == 1 ? "CROSS 1" :
                 xhair_draw == 2 ? "CROSS 2" :
                 xhair_draw == 3 ? "X" :
                 xhair_draw == 4 ? "CIRCLE" :
                 xhair_draw == 5 ? "ANGLE" :
                 xhair_draw == 6 ? "TRIANGLE" :
                 xhair_draw == 7 ? "DOT" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        xhair_draw ? cr[CR_GREEN] : cr[CR_DARKRED],
                            xhair_draw ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    // Crosshair indication
    sprintf(str, xhair_color == 1 ? "HEALTH" :
                 xhair_color == 2 ? "TARGET HIGHLIGHT" :
                 xhair_color == 3 ? "TARGET HIGHLIGHT+HEALTH" : "STATIC");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        xhair_color ? cr[CR_GREEN] : cr[CR_DARKRED],
                            xhair_color ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(11));

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET_BIG, 140, 12, "1/4");
}

static void M_ID_Brightmaps (int choice)
{
    vis_brightmaps = M_INT_Slider(vis_brightmaps, 0, 2, choice, false);
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

    // [crispy] re-calculate amount of colormaps and light tables
    R_InitColormaps();
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
    vis_smooth_palette ^= 1;
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

static MenuItem_t ID_Menu_Gameplay_2[] = {
    { ITT_LRFUNC1, "COLORED ELEMENTS",            M_ID_ColoredSBar,     0, MENU_NONE       },
    { ITT_LRFUNC2, "SHOW AMMO WIDGET",            M_ID_AmmoWidget,      0, MENU_NONE       },
    { ITT_LRFUNC1, "AMMO WIDGET TRANSLUCENCY",    M_ID_AmmoWidgetTranslucent, 0, MENU_NONE },
    { ITT_LRFUNC2, "AMMO WIDGET COLORING",        M_ID_AmmoWidgetColors,0, MENU_NONE       },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE       },
    { ITT_LRFUNC1, "SFX ATTENUATION AXISES",      M_ID_ZAxisSfx,        0, MENU_NONE       },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE       },
    { ITT_LRFUNC1, "CORPSES SLIDING FROM LEDGES", M_ID_Torque,          0, MENU_NONE       },
    { ITT_LRFUNC2, "WEAPON ATTACK ALIGNMENT",     M_ID_WeaponAlignment, 0, MENU_NONE       },
    { ITT_LRFUNC1, "IMITATE PLAYER'S BREATHING",  M_ID_Breathing,       0, MENU_NONE       },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE       },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE       },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */        M_ScrollGameplay,     0, MENU_NONE       },
};

static Menu_t ID_Def_Gameplay_2 = {
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_2,
    13, ID_Menu_Gameplay_2,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_2 (void)
{
    char str[32];
    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY2;

    MN_DrTextACentered("STATUS BAR", 10, cr[CR_YELLOW]);

    // Colored elements
    sprintf(str, st_colored_stbar ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        st_colored_stbar ? cr[CR_GREEN] : cr[CR_DARKRED],
                            st_colored_stbar ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Ammo widget
    sprintf(str, st_ammo_widget == 1 ? "BRIEF" :
                 st_ammo_widget == 2 ? "FULL" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        st_ammo_widget ? cr[CR_GREEN] : cr[CR_DARKRED],
                            st_ammo_widget ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Ammo widget translucency
    sprintf(str, st_ammo_widget_translucent ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        !st_ammo_widget ? cr[CR_DARKRED] :
                        st_ammo_widget_translucent ? cr[CR_GREEN] : cr[CR_DARKRED],
                            !st_ammo_widget ? cr[CR_RED_BRIGHT] :
                            st_ammo_widget_translucent ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // Ammo widget colors
    sprintf(str, st_ammo_widget_colors == 1 ? "AMMO+WEAPONS" :
                 st_ammo_widget_colors == 2 ? "AMMO ONLY" :
                 st_ammo_widget_colors == 3 ? "WEAPONS ONLY" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        !st_ammo_widget ? cr[CR_DARKRED] :
                        st_ammo_widget_colors ? cr[CR_GREEN] : cr[CR_DARKRED],
                            !st_ammo_widget ? cr[CR_RED_BRIGHT] :
                            st_ammo_widget_colors ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                            LINE_ALPHA(3));

    MN_DrTextACentered("AUDIBLE", 60, cr[CR_YELLOW]);

    // Sfx attenuation axises
    sprintf(str, aud_z_axis_sfx ? "X/Y/Z" : "X/Y");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        aud_z_axis_sfx ? cr[CR_GREEN] : cr[CR_DARKRED],
                            aud_z_axis_sfx ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    MN_DrTextACentered("PHYSICAL", 80, cr[CR_YELLOW]);

    // Corpses sliding from ledges
    sprintf(str, phys_torque ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        phys_torque ? cr[CR_GREEN] : cr[CR_DARKRED],
                            phys_torque ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Weapon attack alignment
    sprintf(str, phys_weapon_alignment == 1 ? "BOBBING" :
                 phys_weapon_alignment == 2 ? "CENTERED" : "ORIGINAL");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        phys_weapon_alignment ? cr[CR_GREEN] : cr[CR_DARKRED],
                            phys_weapon_alignment ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Imitate player's breathing
    sprintf(str, phys_breathing ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        phys_breathing ? cr[CR_GREEN] : cr[CR_DARKRED],
                            phys_breathing ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET_BIG, 140, 12, "2/4");
}

static void M_ID_ColoredSBar (int choice)
{
    st_colored_stbar ^= 1;
}

static void M_ID_AmmoWidget (int choice)
{
    st_ammo_widget = M_INT_Slider(st_ammo_widget, 0, 2, choice, false);
}

static void M_ID_AmmoWidgetTranslucent (int choice)
{
    st_ammo_widget_translucent = M_INT_Slider(st_ammo_widget_translucent, 0, 1, choice, false);
}

static void M_ID_AmmoWidgetColors (int choice)
{
    st_ammo_widget_colors = M_INT_Slider(st_ammo_widget_colors, 0, 3, choice, false);
}

static void M_ID_ZAxisSfx (int choice)
{
    aud_z_axis_sfx ^= 1;
}

static void M_ID_Torque (int choice)
{
    phys_torque ^= 1;
}

static void M_ID_WeaponAlignment (int choice)
{
    phys_weapon_alignment = M_INT_Slider(phys_weapon_alignment, 0, 2, choice, false);
}

static void M_ID_Breathing (int choice)
{
    phys_breathing ^= 1;
}

// -----------------------------------------------------------------------------
// Gameplay features 3
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Gameplay_3[] = {
    { ITT_LRFUNC2, "DEFAULT SKILL LEVEL",          M_ID_DefaulSkill,     0, MENU_NONE },
    { ITT_LRFUNC2, "REPORT REVEALED SECRETS",      M_ID_RevealedSecrets, 0, MENU_NONE },
    { ITT_LRFUNC1, "FLIP LEVELS HORIZONTALLY",     M_ID_FlipLevels,      0, MENU_NONE },
    { ITT_LRFUNC2, "ON DEATH ACTION",              M_ID_OnDeathAction,   0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW DEMO TIMER",              M_ID_DemoTimer,       0, MENU_NONE },
    { ITT_LRFUNC1, "TIMER DIRECTION",              M_ID_TimerDirection,  0, MENU_NONE },
    { ITT_LRFUNC1, "SHOW PROGRESS BAR",            M_ID_ProgressBar,     0, MENU_NONE },
    { ITT_LRFUNC1, "PLAY INTERNAL DEMOS",          M_ID_InternalDemos,   0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */         M_ScrollGameplay,     0, MENU_NONE },
};

static Menu_t ID_Def_Gameplay_3 = {
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_3,
    13, ID_Menu_Gameplay_3,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_3 (void)
{
    char str[32];
    const char *const DefSkillName[5] = { "WET-NURSE", "YELLOWBELLIES", "BRINGEST", "SMITE-MEISTER", "BLACK PLAGUE" };

    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY3;

    MN_DrTextACentered("GAMEPLAY", 10, cr[CR_YELLOW]);

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[gp_default_skill]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        gp_default_skill == 0 ? cr[CR_OLIVE] :
                        gp_default_skill == 1 ? cr[CR_DARKGREEN] :
                        gp_default_skill == 2 ? cr[CR_GREEN] :
                        gp_default_skill == 3 ? cr[CR_YELLOW] :
                        gp_default_skill == 4 ? cr[CR_ORANGE_HR] : cr[CR_RED],
                            gp_default_skill == 0 ? cr[CR_OLIVE_BRIGHT] :
                            gp_default_skill == 1 ? cr[CR_DARKGREEN_BRIGHT] :
                            gp_default_skill == 2 ? cr[CR_GREEN_BRIGHT] :
                            gp_default_skill == 3 ? cr[CR_YELLOW_BRIGHT] :
                            gp_default_skill == 4 ? cr[CR_ORANGE_HR_BRIGHT] : cr[CR_RED_BRIGHT],    
                                LINE_ALPHA(0));

    // Report revealed secrets
    sprintf(str, gp_revealed_secrets == 1 ? "TOP" :
                 gp_revealed_secrets == 2 ? "CENTERED" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        gp_revealed_secrets ? cr[CR_GREEN] : cr[CR_DARKRED],
                            gp_revealed_secrets ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Flip levels horizontally
    sprintf(str, gp_flip_levels ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        gp_flip_levels ? cr[CR_GREEN] : cr[CR_DARKRED],
                            gp_flip_levels ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // On death action
    sprintf(str, gp_death_use_action == 1 ? "LAST SAVE" :
                 gp_death_use_action == 2 ? "NOTHING" : "DEFAULT");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        gp_death_use_action ? cr[CR_GREEN] : cr[CR_DARKRED],
                            gp_death_use_action ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    MN_DrTextACentered("DEMOS", 60, cr[CR_YELLOW]);

    // Show Demo timer
    sprintf(str, demo_timer == 1 ? "PLAYBACK" : 
                 demo_timer == 2 ? "RECORDING" : 
                 demo_timer == 3 ? "ALWAYS" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        demo_timer ? cr[CR_GREEN] : cr[CR_DARKRED],
                            demo_timer ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Timer direction
    sprintf(str, demo_timerdir ? "BACKWARD" : "FORWARD");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        demo_timer ? cr[CR_GREEN] : cr[CR_DARKRED],
                            demo_timer ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Show progress bar
    sprintf(str, demo_bar ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        demo_bar ? cr[CR_GREEN] : cr[CR_DARKRED],
                            demo_bar ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Play internal demos
    sprintf(str, demo_internal ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        demo_internal ? cr[CR_GREEN] : cr[CR_DARKRED],
                            demo_internal ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET_BIG, 140, 12, "3/4");
}

static void M_ID_DefaulSkill (int choice)
{
    gp_default_skill = M_INT_Slider(gp_default_skill, 0, 4, choice, false);
    SkillMenu.oldItPos = gp_default_skill;
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
}

static void M_ID_OnDeathAction (int choice)
{
    gp_death_use_action = M_INT_Slider(gp_death_use_action, 0, 2, choice, false);
}

static void M_ID_DemoTimer (int choice)
{
    demo_timer = M_INT_Slider(demo_timer, 0, 3, choice, false);
}

static void M_ID_ProgressBar (int choice)
{
    demo_bar ^= 1;
}

static void M_ID_InternalDemos (int choice)
{
    demo_internal ^= 1;
}

static void M_ID_TimerDirection (int choice)
{
    demo_timerdir ^= 1;
}

// -----------------------------------------------------------------------------
// Gameplay features 4
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Gameplay_4[] = {
    { ITT_LRFUNC1, "WAND START GAME MODE",         M_ID_PistolStart,     0, MENU_NONE },
    { ITT_LRFUNC1, "IMPROVED HIT DETECTION",       M_ID_BlockmapFix,     0, MENU_NONE },
    { ITT_LRFUNC1, "AUTOMATIC STRAFE 50",          M_ID_AutomaticSR50,   0, MENU_NONE },
    { ITT_LRFUNC1, "LANDING DOESN\'T CENTER VIEW", M_ID_NoLandCenter,    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                           NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "", /* SCROLLS PAGES */         M_ScrollGameplay,     0, MENU_NONE },
};

static Menu_t ID_Def_Gameplay_4 = {
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_4,
    13, ID_Menu_Gameplay_4,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_4 (void)
{
    char str[32];

    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY4;

    MN_DrTextACentered("COMPATIBILITY-BREAKING", 10, cr[CR_YELLOW]);

    // Wand start game mode
    sprintf(str, compat_pistol_start ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        compat_pistol_start ? cr[CR_GREEN] : cr[CR_DARKRED],
                            compat_pistol_start ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Improved hit detection
    sprintf(str, compat_blockmap_fix ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        compat_blockmap_fix ? cr[CR_GREEN] : cr[CR_DARKRED],
                            compat_blockmap_fix ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Automatic strafe 50
    sprintf(str, compat_auto_sr50 ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        compat_auto_sr50 ? cr[CR_GREEN] : cr[CR_DARKRED],
                            compat_auto_sr50 ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // Landing doesn't center view
    sprintf(str, compat_no_land_centering ? "ON" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        compat_no_land_centering ? cr[CR_GREEN] : cr[CR_DARKRED],
                            compat_no_land_centering ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    // < Scroll pages >
    M_DrawScrollPages(ID_MENU_LEFTOFFSET_BIG, 140, 12, "4/4");
}

static void M_ID_PistolStart (int choice)
{
    compat_pistol_start ^= 1;
}

static void M_ID_BlockmapFix (int choice)
{
    compat_blockmap_fix ^= 1;
}

static void M_ID_NoLandCenter (int choice)
{
    compat_no_land_centering ^= 1;
}

static void M_ID_AutomaticSR50 (int choice)
{
    compat_auto_sr50 ^= 1;
    G_SetSideMove();
}

static void M_ScrollGameplay (int choice)
{
    if (choice) // Scroll right
    {
             if (CurrentMenu == &ID_Def_Gameplay_1) { SetMenu(MENU_ID_GAMEPLAY2); }
        else if (CurrentMenu == &ID_Def_Gameplay_2) { SetMenu(MENU_ID_GAMEPLAY3); }
        else if (CurrentMenu == &ID_Def_Gameplay_3) { SetMenu(MENU_ID_GAMEPLAY4); }
        else if (CurrentMenu == &ID_Def_Gameplay_4) { SetMenu(MENU_ID_GAMEPLAY1); }
    }
    else
    {           // Scroll left
             if (CurrentMenu == &ID_Def_Gameplay_1) { SetMenu(MENU_ID_GAMEPLAY4); }
        else if (CurrentMenu == &ID_Def_Gameplay_2) { SetMenu(MENU_ID_GAMEPLAY1); }
        else if (CurrentMenu == &ID_Def_Gameplay_3) { SetMenu(MENU_ID_GAMEPLAY2); }
        else if (CurrentMenu == &ID_Def_Gameplay_4) { SetMenu(MENU_ID_GAMEPLAY3); }
        
    }
    CurrentItPos = 12;
}

// -----------------------------------------------------------------------------
// Miscellaneous settings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Misc[] = {
    { ITT_LRFUNC1, "INVULNERABILITY EFFECT", M_ID_Misc_A11yInvul,      0, MENU_NONE },
    { ITT_LRFUNC2, "PALETTE FLASH EFFECTS",  M_ID_Misc_A11yPalFlash,   0, MENU_NONE },
    { ITT_LRFUNC1, "MOVEMENT BOBBING",       M_ID_Misc_A11yMoveBob,    0, MENU_NONE },
    { ITT_LRFUNC1, "WEAPON BOBBING",         M_ID_Misc_A11yWeaponBob,  0, MENU_NONE },
    { ITT_LRFUNC2, "COLORBLIND FILTER",      M_ID_Misc_A11yColorblind, 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                     NULL,                     0, MENU_NONE },
    { ITT_LRFUNC2, "AUTOLOAD WAD FILES",     M_ID_Misc_AutoloadWAD,    0, MENU_NONE },
    { ITT_LRFUNC2, "AUTOLOAD HHE FILES",     M_ID_Misc_AutoloadHHE,    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                     NULL,                     0, MENU_NONE },
    { ITT_LRFUNC2, "HIGHLIGHTING EFFECT",    M_ID_Misc_Hightlight,     0, MENU_NONE },
    { ITT_LRFUNC1, "ESC KEY BEHAVIOUR",      M_ID_Misc_MenuEscKey,     0, MENU_NONE },
};

static Menu_t ID_Def_Misc = {
    ID_MENU_CTRLSOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Misc,
    11, ID_Menu_Misc,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

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

    MN_DrTextACentered("ACCESSIBILITY", 10, cr[CR_YELLOW]);

    // Invulnerability effect
    sprintf(str, a11y_invul ? "GRAYSCALE" : "DEFAULT");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        a11y_invul ? cr[CR_GREEN] : cr[CR_DARKRED],
                            a11y_invul ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Palette flash effects
    sprintf(str, a11y_pal_flash == 1 ? "HALVED" :
                 a11y_pal_flash == 2 ? "QUARTERED" :
                 a11y_pal_flash == 3 ? "OFF" : "DEFAULT");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        a11y_pal_flash == 1 ? cr[CR_YELLOW] :
                        a11y_pal_flash == 2 ? cr[CR_ORANGE_HR] :
                        a11y_pal_flash == 2 ? cr[CR_RED] : cr[CR_DARKRED],
                            a11y_pal_flash == 1 ? cr[CR_YELLOW_BRIGHT] :
                            a11y_pal_flash == 2 ? cr[CR_ORANGE_HR_BRIGHT] :
                            a11y_pal_flash == 2 ? cr[CR_RED_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Movement bobbing
    sprintf(str, "%s", bobpercent[a11y_move_bob]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        a11y_move_bob == 20 ? cr[CR_DARKRED] :
                        a11y_move_bob ==  0 ? cr[CR_RED] : cr[CR_YELLOW],
                            a11y_move_bob == 20 ? cr[CR_RED_BRIGHT] :
                            a11y_move_bob ==  0 ? cr[CR_RED_BRIGHT] : cr[CR_YELLOW_BRIGHT],
                                LINE_ALPHA(2));

    // Weapon bobbing
    sprintf(str, "%s", bobpercent[a11y_weapon_bob]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        a11y_weapon_bob == 20 ? cr[CR_DARKRED] :
                        a11y_weapon_bob ==  0 ? cr[CR_RED] : cr[CR_YELLOW],
                            a11y_weapon_bob == 20 ? cr[CR_RED_BRIGHT] :
                            a11y_weapon_bob ==  0 ? cr[CR_RED_BRIGHT] : cr[CR_YELLOW_BRIGHT],
                                LINE_ALPHA(3));

    // Colorblind filter
    sprintf(str, "%s", colorblind_name[a11y_colorblind]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        a11y_colorblind ? cr[CR_GREEN] : cr[CR_DARKRED],
                            a11y_colorblind ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    MN_DrTextACentered("AUTOLOAD", 70, cr[CR_YELLOW]);

    // Autoload WAD files
    sprintf(str, autoload_wad == 1 ? "IWAD ONLY" :
                 autoload_wad == 2 ? "IWAD AND PWAD" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        autoload_wad == 1 ? cr[CR_YELLOW] :
                        autoload_wad == 2 ? cr[CR_GREEN] : cr[CR_DARKRED],
                            autoload_wad == 1 ? cr[CR_YELLOW_BRIGHT] :
                            autoload_wad == 2 ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Autoload DEH patches
    sprintf(str, autoload_hhe == 1 ? "IWAD ONLY" :
                 autoload_hhe == 2 ? "IWAD AND PWAD" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        autoload_deh == 1 ? cr[CR_YELLOW] :
                        autoload_deh == 2 ? cr[CR_GREEN] : cr[CR_DARKRED],
                            autoload_deh == 1 ? cr[CR_YELLOW_BRIGHT] :
                            autoload_deh == 2 ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    MN_DrTextACentered("MENU SETTINGS", 100, cr[CR_YELLOW]);

    // Highlighting effect
    sprintf(str, menu_highlight == 1 ? "ANIMATED" :
                 menu_highlight == 2 ? "STATIC" : "OFF");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        menu_highlight == 1 ? cr[CR_GREEN] :
                        menu_highlight == 2 ? cr[CR_YELLOW] : cr[CR_DARKRED],
                            menu_highlight == 1 ? cr[CR_GREEN_BRIGHT] :
                            menu_highlight == 2 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // ESC key behaviour
    sprintf(str, menu_esc_key ? "GO BACK" : "CLOSE MENU");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        menu_esc_key ? cr[CR_GREEN] : cr[CR_DARKRED],
                            menu_esc_key ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    // [PN] Added explanations for colorblind filters
    if (CurrentItPos == 4)
    {
        const char *colorblind_hint[] = {
            "","RED-BLIND","RED-WEAK","GREEN-BLIND","GREEN-WEAK",
            "BLUE-BLIND","BLUE-WEAK","MONOCHROMACY","BLUE CONE MONOCHROMACY"
        };

        MN_DrTextACentered(colorblind_hint[a11y_colorblind], 130, cr[CR_LIGHTGRAY_DARK]);
    }

    // [PN] Added explanations for autoload variables
    if (CurrentItPos == 6 || CurrentItPos == 7)
    {
        const char *off = "AUTOLOAD IS DISABLED";
        const char *first_line = "AUTOLOAD AND FOLDER CREATION";
        const char *second_line1 = "ONLY ALLOWED FOR IWAD FILES";
        const char *second_line2 = "ALLOWED FOR BOTH IWAD AND PWAD FILES";
        const int   autoload_option = (CurrentItPos == 6) ? autoload_wad : autoload_hhe;

        switch (autoload_option)
        {
            case 1:
                MN_DrTextACentered(first_line, 130, cr[CR_LIGHTGRAY_DARK]);
                MN_DrTextACentered(second_line1, 140, cr[CR_LIGHTGRAY_DARK]);
                break;

            case 2:
                MN_DrTextACentered(first_line, 130, cr[CR_LIGHTGRAY_DARK]);
                MN_DrTextACentered(second_line2, 140, cr[CR_LIGHTGRAY_DARK]);
                break;

            default:
                MN_DrTextACentered(off, 130, cr[CR_LIGHTGRAY_DARK]);
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
    I_SetPalette (sb_palette);
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
    R_InitColormaps();
    R_FillBackScreen();
    SB_ForceRedraw();
    I_SetColorPanes(false);
    I_SetPalette(sb_palette);
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

static void M_ID_Misc_AutoloadHHE (int choice)
{
    autoload_hhe = M_INT_Slider(autoload_hhe, 0, 2, choice, false);
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

static MenuItem_t ID_Menu_Level_1[] = {
    { ITT_LRFUNC2, "SKILL LEVEL",       M_ID_LevelSkill,      0, MENU_NONE },
    { ITT_LRFUNC1, "EPISODE",           M_ID_LevelEpisode,    0, MENU_NONE },
    { ITT_LRFUNC1, "MAP",               M_ID_LevelMap,        0, MENU_NONE },
    { ITT_EMPTY,   NULL,                NULL,                 0, MENU_NONE },
    { ITT_LRFUNC1, "HEALTH",            M_ID_LevelHealth,     0, MENU_NONE },
    { ITT_LRFUNC1, "ARMOR",             M_ID_LevelArmor,      0, MENU_NONE },
    { ITT_LRFUNC1, "ARMOR TYPE",        M_ID_LevelArmorType,  0, MENU_NONE },
    { ITT_EMPTY,   NULL,                NULL,                 0, MENU_NONE },
    { ITT_LRFUNC1, "GAUNTLETS",         M_ID_LevelGauntlets,  0, MENU_NONE },
    { ITT_LRFUNC1, "ETHEREAL CROSSBOW", M_ID_LevelCrossbow,   0, MENU_NONE },
    { ITT_LRFUNC1, "DRAGON CLAW",       M_ID_LevelDragonClaw, 0, MENU_NONE },
    { ITT_LRFUNC1, "HELLSTAFF",         M_ID_LevelHellStaff,  0, MENU_NONE },
    { ITT_LRFUNC1, "PHOENIX ROD",       M_ID_LevelPhoenixRod, 0, MENU_NONE },
    { ITT_LRFUNC1, "FIREMACE",          M_ID_LevelFireMace,   0, MENU_NONE },
    { ITT_EMPTY,   NULL,                NULL,                 0, MENU_NONE },
    { ITT_LRFUNC2, "NEXT PAGE",         M_ScrollLevel,        0, MENU_NONE },
    { ITT_EFUNC,   "START GAME",        G_DoSelectiveGame,    0, MENU_NONE },
};

static Menu_t ID_Def_Level_1 = {
    ID_MENU_LEFTOFFSET_LEVEL, ID_MENU_TOPOFFSET,
    M_Draw_ID_Level_1,
    17, ID_Menu_Level_1,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Level_1 (void)
{
    char str[32];
    const char *const DefSkillName[5] = { "WET-NURSE", "YELLOWBELLIES", "BRINGEST", "SMITE-MEISTER", "BLACK PLAGUE" };

    M_FillBackground();

    MN_DrTextACentered("LEVEL SELECT", 10, cr[CR_YELLOW]);

    // Skill level
    sprintf(str, "%s", DefSkillName[level_select[0]]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        level_select[0] == 0 ? cr[CR_OLIVE] :
                        level_select[0] == 1 ? cr[CR_DARKGREEN] :
                        level_select[0] == 2 ? cr[CR_GREEN] :
                        level_select[0] == 3 ? cr[CR_YELLOW] :
                        level_select[0] == 4 ? cr[CR_ORANGE_HR] : cr[CR_RED],
                            level_select[0] == 0 ? cr[CR_OLIVE_BRIGHT] :
                            level_select[0] == 1 ? cr[CR_DARKGREEN_BRIGHT] :
                            level_select[0] == 2 ? cr[CR_GREEN_BRIGHT] :
                            level_select[0] == 3 ? cr[CR_YELLOW_BRIGHT] :
                            level_select[0] == 4 ? cr[CR_ORANGE_HR_BRIGHT] : cr[CR_RED_BRIGHT],    
                                LINE_ALPHA(0));

    // Episode
    sprintf(str, gamemode == shareware ? "1" : "%d", level_select[1]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        gamemode == shareware ? cr[CR_DARKRED] : cr[CR_LIGHTGRAY],
                            gamemode == shareware ? cr[CR_RED_BRIGHT] : cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(1));

    // Map
    sprintf(str, "%d", level_select[2]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(2));

    MN_DrTextACentered("PLAYER", 50, cr[CR_YELLOW]);

    // Health
    sprintf(str, "%d", level_select[3]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        level_select[3] >=  67 ? cr[CR_GREEN] :
                        level_select[3] >=  34 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[3] >=  67 ? cr[CR_GREEN_BRIGHT] :
                            level_select[3] >=  34 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Armor
    sprintf(str, "%d", level_select[4]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        level_select[4] == 0 ? cr[CR_RED] :
                        level_select[5] == 1 ? cr[CR_LIGHTGRAY] : cr[CR_YELLOW],
                            level_select[4] == 0 ? cr[CR_RED_BRIGHT] :
                            level_select[5] == 1 ? cr[CR_LIGHTGRAY_BRIGHT] : cr[CR_YELLOW_BRIGHT],
                                LINE_ALPHA(5));

    // Armor type
    sprintf(str, "%d", level_select[5]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        level_select[5] == 1 ? cr[CR_LIGHTGRAY] : cr[CR_YELLOW],
                            level_select[5] == 1 ? cr[CR_LIGHTGRAY_BRIGHT] : cr[CR_YELLOW_BRIGHT],
                                LINE_ALPHA(6));

    MN_DrTextACentered("WEAPONS", 90, cr[CR_YELLOW]);

    // Gauntlets
    sprintf(str, level_select[6] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        level_select[6] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[6] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Ethereal Crossbow
    sprintf(str, level_select[7] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        level_select[7] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[7] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Dragon Claw
    sprintf(str, level_select[8] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        level_select[8] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[8] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    // Hellstaff
    sprintf(str, gamemode == shareware ? "N/A" :
                 level_select[9] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 130,
                        gamemode == shareware ? cr[CR_DARKRED] :
                        level_select[9] ? cr[CR_GREEN] : cr[CR_RED],
                            gamemode == shareware ? cr[CR_RED_BRIGHT] :
                            level_select[9] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],                        
                                LINE_ALPHA(11));

    // Phoenix Rod
    sprintf(str, gamemode == shareware ? "N/A" :
                 level_select[10] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        gamemode == shareware ? cr[CR_DARKRED] :
                        level_select[10] ? cr[CR_GREEN] : cr[CR_RED],
                            gamemode == shareware ? cr[CR_RED_BRIGHT] :
                            level_select[10] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],                        
                                LINE_ALPHA(12));

    // Firemace
    sprintf(str, gamemode == shareware ? "N/A" :
                 level_select[11] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 150,
                        gamemode == shareware ? cr[CR_DARKRED] :
                        level_select[11] ? cr[CR_GREEN] : cr[CR_RED],
                            gamemode == shareware ? cr[CR_RED_BRIGHT] :
                            level_select[11] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],                        
                                LINE_ALPHA(13));

    // Footer
    sprintf(str, "PAGE 1/3");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 170,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(15));
}

static void M_ID_LevelSkill (int choice)
{
    level_select[0] = M_INT_Slider(level_select[0], 0, 4, choice, false);
}

static void M_ID_LevelEpisode (int choice)
{
    if (gamemode == shareware)
    {
        return;
    }

    level_select[1] = M_INT_Slider(level_select[1], 1,
                                   gamemode == retail ? 5 : 3, choice, false);
}

static void M_ID_LevelMap (int choice)
{
    level_select[2] = M_INT_Slider(level_select[2], 1, 9, choice, false);
}

static void M_ID_LevelHealth (int choice)
{
    level_select[3] = M_INT_Slider(level_select[3], 1, 100, choice, false);
}

static void M_ID_LevelArmor (int choice)
{
    level_select[4] = M_INT_Slider(level_select[4], 0, 
                                   level_select[5] == 1 ? 100 : 200, choice, false);
}

static void M_ID_LevelArmorType (int choice)
{
    level_select[5] = M_INT_Slider(level_select[5], 1, 2, choice, false);

    // [JN] Silver Shield armor can't go above 100.
    if (level_select[5] == 1 && level_select[4] > 100)
    {
        level_select[4] = 100;
    }
}

static void M_ID_LevelGauntlets (int choice)
{
    level_select[6] ^= 1;
}

static void M_ID_LevelCrossbow (int choice)
{
    level_select[7] ^= 1;
}

static void M_ID_LevelDragonClaw (int choice)
{
    level_select[8] ^= 1;
}

static void M_ID_LevelHellStaff (int choice)
{
    if (gamemode == shareware)
    {
        return;
    }

    level_select[9] ^= 1;
}

static void M_ID_LevelPhoenixRod (int choice)
{
    if (gamemode == shareware)
    {
        return;
    }

    level_select[10] ^= 1;
}

static void M_ID_LevelFireMace (int choice)
{
    if (gamemode == shareware)
    {
        return;
    }
    
    level_select[11] ^= 1;
}

// -----------------------------------------------------------------------------
// Level select 2
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Level_2[] = {
    { ITT_LRFUNC1, "BAG OF HOLDING",  M_ID_LevelBag,     0, MENU_NONE },
    { ITT_LRFUNC1, "WAND CRYSTALS",   M_ID_LevelAmmo_0,  0, MENU_NONE },
    { ITT_LRFUNC1, "ETHEREAL ARROWS", M_ID_LevelAmmo_1,  0, MENU_NONE },
    { ITT_LRFUNC1, "CLAW ORBS",       M_ID_LevelAmmo_2,  0, MENU_NONE },
    { ITT_LRFUNC1, "HELLSTAFF RUNES", M_ID_LevelAmmo_3,  0, MENU_NONE },
    { ITT_LRFUNC1, "FLAME ORBS",      M_ID_LevelAmmo_4,  0, MENU_NONE },
    { ITT_LRFUNC1, "MACE SPHERES",    M_ID_LevelAmmo_5,  0, MENU_NONE },
    { ITT_EMPTY,   NULL,              NULL,              0, MENU_NONE },
    { ITT_LRFUNC1, "YELLOW KEY",      M_ID_LevelKey_0,   0, MENU_NONE },
    { ITT_LRFUNC1, "GREEN KEY",       M_ID_LevelKey_1,   0, MENU_NONE },
    { ITT_LRFUNC1, "BLUE KEY",        M_ID_LevelKey_2,   0, MENU_NONE },
    { ITT_EMPTY,   NULL,              NULL,              0, MENU_NONE },
    { ITT_LRFUNC1, "FAST",            M_ID_LevelFast,    0, MENU_NONE },
    { ITT_LRFUNC1, "RESPAWNING",      M_ID_LevelRespawn, 0, MENU_NONE },
    { ITT_EMPTY,   NULL,              NULL,              0, MENU_NONE },
    { ITT_LRFUNC2, "LAST PAGE",       M_ScrollLevel,     0, MENU_NONE },
    { ITT_EFUNC,   "START GAME",      G_DoSelectiveGame, 0, MENU_NONE },
};

static Menu_t ID_Def_Level_2 = {
    ID_MENU_LEFTOFFSET_LEVEL, ID_MENU_TOPOFFSET,
    M_Draw_ID_Level_2,
    17, ID_Menu_Level_2,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Level_2 (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("AMMO", 10, cr[CR_YELLOW]);

    // Bag of Holding
    sprintf(str, level_select[12] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        level_select[12] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[12] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Wand crystals
    sprintf(str, "%d", level_select[13]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        level_select[13] >= 50 ? cr[CR_GREEN] :
                        level_select[13] >= 25 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[13] >= 50 ? cr[CR_GREEN_BRIGHT] :
                            level_select[13] >= 25 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Ethereal arrows
    sprintf(str, "%d", level_select[14]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        level_select[14] >= 25 ? cr[CR_GREEN] :
                        level_select[14] >= 12 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[14] >= 25 ? cr[CR_GREEN_BRIGHT] :
                            level_select[14] >= 12 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // Claw orbs
    sprintf(str, "%d", level_select[15]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        level_select[15] >= 100 ? cr[CR_GREEN] :
                        level_select[15] >=  50 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[15] >= 100 ? cr[CR_GREEN_BRIGHT] :
                            level_select[15] >=  50 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    // Hellstaff runes
    sprintf(str, "%d", level_select[16]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        level_select[16] >= 100 ? cr[CR_GREEN] :
                        level_select[16] >=  50 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[16] >= 100 ? cr[CR_GREEN_BRIGHT] :
                            level_select[16] >=  50 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Flame orbs
    sprintf(str, "%d", level_select[17]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        level_select[17] >= 10 ? cr[CR_GREEN] :
                        level_select[17] >=  5 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[17] >= 10 ? cr[CR_GREEN_BRIGHT] :
                            level_select[17] >=  5 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Mace spheres
    sprintf(str, "%d", level_select[18]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        level_select[18] >= 75 ? cr[CR_GREEN] :
                        level_select[18] >= 35 ? cr[CR_YELLOW] : cr[CR_RED],
                            level_select[18] >= 75 ? cr[CR_GREEN_BRIGHT] :
                            level_select[18] >= 35 ? cr[CR_YELLOW_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    MN_DrTextACentered("KEYS", 90, cr[CR_YELLOW]);

    // Yellow key
    sprintf(str, level_select[19] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        level_select[19] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[19] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Green key
    sprintf(str, level_select[20] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        level_select[20] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[20] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Blue key
    sprintf(str, level_select[21] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 120,
                        level_select[21] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[21] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(10));

    MN_DrTextACentered("MONSTERS", 130, cr[CR_YELLOW]);

    // Fast monsters
    sprintf(str, level_select[22] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 140,
                        level_select[22] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[22] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(12));

    // Respawning monsters
    sprintf(str, level_select[23] ? "YES" : "NO");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 150,
                        level_select[23] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[23] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(13));

    // Footer
    sprintf(str, "PAGE 2/3");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 170,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(15));
}

static void M_ID_LevelBag (int choice)
{
    level_select[12] ^= 1;

    if (!level_select[12])
    {
        if (level_select[13] > 100) // Wand crystals
            level_select[13] = 100;
        if (level_select[14] > 50)  // Ethereal arrows
            level_select[14] = 50;
        if (level_select[15] > 200) // Claw orbs
            level_select[15] = 200;
        if (level_select[16] > 200) // Hellstaff runes
            level_select[16] = 200;
        if (level_select[17] > 20)  // Flame orbs
            level_select[17] = 20;
        if (level_select[18] > 150) // Mace spheres
            level_select[18] = 150;
    }
}

static void M_ID_LevelAmmo_0 (int choice)
{
    level_select[13] = M_INT_Slider(level_select[13], 0, level_select[12] ? 200 : 100, choice, false);
}

static void M_ID_LevelAmmo_1 (int choice)
{
    level_select[14] = M_INT_Slider(level_select[14], 0, level_select[12] ? 100 : 50, choice, false);
}

static void M_ID_LevelAmmo_2 (int choice)
{
    level_select[15] = M_INT_Slider(level_select[15], 0, level_select[12] ? 400 : 200, choice, false);
}

static void M_ID_LevelAmmo_3 (int choice)
{
    level_select[16] = M_INT_Slider(level_select[16], 0, level_select[12] ? 400 : 200, choice, false);
}

static void M_ID_LevelAmmo_4 (int choice)
{
    level_select[17] = M_INT_Slider(level_select[17], 0, level_select[12] ? 40 : 20, choice, false);
}

static void M_ID_LevelAmmo_5 (int choice)
{
    level_select[18] = M_INT_Slider(level_select[18], 0, level_select[12] ? 300 : 150, choice, false);
}

static void M_ID_LevelKey_0 (int choice)
{
    level_select[19] ^= 1;
}

static void M_ID_LevelKey_1 (int choice)
{
    level_select[20] ^= 1;
}

static void M_ID_LevelKey_2 (int choice)
{
    level_select[21] ^= 1;
}

static void M_ID_LevelFast (int choice)
{
    level_select[22] ^= 1;
}

static void M_ID_LevelRespawn (int choice)
{
    level_select[23] ^= 1;
}

// -----------------------------------------------------------------------------
// Level select 3
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Level_3[] = {
    { ITT_LRFUNC1, "QUARTZ FLASK",          M_ID_LevelArti_0,  0, MENU_NONE },
    { ITT_LRFUNC1, "MYSTIC URN",            M_ID_LevelArti_1,  0, MENU_NONE },
    { ITT_LRFUNC1, "TIME BOMB",             M_ID_LevelArti_2,  0, MENU_NONE },
    { ITT_LRFUNC1, "TOME OF POWER",         M_ID_LevelArti_3,  0, MENU_NONE },
    { ITT_LRFUNC1, "RING OF INVINCIBILITY", M_ID_LevelArti_4,  0, MENU_NONE },
    { ITT_LRFUNC1, "MORPH OVUM",            M_ID_LevelArti_5,  0, MENU_NONE },
    { ITT_LRFUNC1, "CHAOS DEVICE",          M_ID_LevelArti_6,  0, MENU_NONE },
    { ITT_LRFUNC1, "SHADOWSPHERE",          M_ID_LevelArti_7,  0, MENU_NONE },
    { ITT_LRFUNC1, "WINGS OF WRATH",        M_ID_LevelArti_8,  0, MENU_NONE },
    { ITT_LRFUNC1, "TORCH",                 M_ID_LevelArti_9,  0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,              0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,              0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,              0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,              0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,              0, MENU_NONE },
    { ITT_LRFUNC2, "FIRST PAGE",            M_ScrollLevel,     0, MENU_NONE },
    { ITT_EFUNC,  "START GAME",            G_DoSelectiveGame, 0, MENU_NONE },
};

static Menu_t ID_Def_Level_3 = {
    ID_MENU_LEFTOFFSET_LEVEL, ID_MENU_TOPOFFSET,
    M_Draw_ID_Level_3,
    17, ID_Menu_Level_3,
    0,
    SmallFont, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Level_3 (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("ARTIFACTS", 10, cr[CR_YELLOW]);

    // Quartz flask
    sprintf(str, "%d", level_select[24]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 20,
                        level_select[24] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[24] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(0));

    // Mystic urn
    sprintf(str, "%d", level_select[25]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 30,
                        level_select[25] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[25] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(1));

    // Time bomb
    sprintf(str, "%d", level_select[26]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 40,
                        level_select[26] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[26] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(2));

    // Tome of power
    sprintf(str, "%d", level_select[27]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 50,
                        level_select[27] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[27] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(3));

    // Ring of invincibility
    sprintf(str, "%d", level_select[28]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 60,
                        level_select[28] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[28] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(4));

    // Morph ovum
    sprintf(str, "%d", level_select[29]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 70,
                        level_select[29] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[29] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(5));

    // Chaos device
    sprintf(str, "%d", level_select[30]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 80,
                        level_select[30] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[30] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(6));

    // Shadowsphere
    sprintf(str, "%d", level_select[31]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 90,
                        level_select[31] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[31] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(7));

    // Wings of wrath
    sprintf(str, "%d", level_select[32]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 100,
                        level_select[32] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[32] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(8));

    // Torch
    sprintf(str, "%d", level_select[33]);
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 110,
                        level_select[33] ? cr[CR_GREEN] : cr[CR_RED],
                            level_select[33] ? cr[CR_GREEN_BRIGHT] : cr[CR_RED_BRIGHT],
                                LINE_ALPHA(9));

    // Footer
    sprintf(str, "PAGE 3/3");
    MN_DrTextAGlow(str, M_ItemRightAlign(str), 170,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(15));
}

static void M_ID_LevelArti_0 (int choice)
{
    level_select[24] = M_INT_Slider(level_select[24], 0, 16, choice, false);
}

static void M_ID_LevelArti_1 (int choice)
{
    level_select[25] = M_INT_Slider(level_select[25], 0, 16, choice, false);
}

static void M_ID_LevelArti_2 (int choice)
{
    level_select[26] = M_INT_Slider(level_select[26], 0, 16, choice, false);
}

static void M_ID_LevelArti_3 (int choice)
{
    level_select[27] = M_INT_Slider(level_select[27], 0, 16, choice, false);
}

static void M_ID_LevelArti_4 (int choice)
{
    level_select[28] = M_INT_Slider(level_select[28], 0, 16, choice, false);
}

static void M_ID_LevelArti_5 (int choice)
{
    level_select[29] = M_INT_Slider(level_select[29], 0, 16, choice, false);
}

static void M_ID_LevelArti_6 (int choice)
{
    level_select[30] = M_INT_Slider(level_select[30], 0, 16, choice, false);
}

static void M_ID_LevelArti_7 (int choice)
{
    level_select[31] = M_INT_Slider(level_select[31], 0, 16, choice, false);
}

static void M_ID_LevelArti_8 (int choice)
{
    level_select[32] = M_INT_Slider(level_select[32], 0, 16, choice, false);
}

static void M_ID_LevelArti_9 (int choice)
{
    level_select[33] = M_INT_Slider(level_select[33], 0, 16, choice, false);
}

static void M_ScrollLevel (int choice)
{
    if (choice) // Scroll right
    {
             if (CurrentMenu == &ID_Def_Level_1) { SetMenu(MENU_ID_LEVEL2); }
        else if (CurrentMenu == &ID_Def_Level_2) { SetMenu(MENU_ID_LEVEL3); }
        else if (CurrentMenu == &ID_Def_Level_3) { SetMenu(MENU_ID_LEVEL1); }

    }
    else
    {           // Scroll left
             if (CurrentMenu == &ID_Def_Level_1) { SetMenu(MENU_ID_LEVEL3); }
        else if (CurrentMenu == &ID_Def_Level_2) { SetMenu(MENU_ID_LEVEL1); }
        else if (CurrentMenu == &ID_Def_Level_3) { SetMenu(MENU_ID_LEVEL2); }
        
    }
    CurrentItPos = 15;
}


// -----------------------------------------------------------------------------
// Reset settings
// -----------------------------------------------------------------------------

static void M_ID_ApplyResetHook (void)
{
    //
    // Video options
    //

    vid_truecolor = 0;
    vid_resolution = 2;
    vid_widescreen = 0;
    vid_fullscreen_exclusive = 0;
    vid_uncapped_fps = 0;
    vid_fpslimit = 0;
    vid_vsync = 1;
    vid_showfps = 0;
    vid_smooth_scaling = 0;
    // Miscellaneous
    vid_graphical_startup = 0;
    vid_screenwipe_hr = 0;
    vid_endoom = 0;
    // Post-processing effects
    post_supersample = 0;
    post_overglow = 0;
    post_bloom = 0;
    post_rgbdrift = 0;
    post_vhsdist = 0;
    post_vignette = 0;
    post_filmgrain = 0;
    post_motionblur = 0;
    post_dofblur = 0;

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
    // Messages Settings
    msg_show = 1;
    msg_text_shadows = 0;
    msg_local_time = 0;

    //
    // Sound options
    //

    snd_MaxVolume = 10;
    snd_MusicVolume = 10;
    snd_monosfx = 0;
    snd_pitchshift = 1;
    snd_channels = 8;
    snd_mute_inactive = 0;

    //
    // Widgets and automap
    //

    widget_scheme = 1;
    widget_location = 0;
    widget_alignment = 0;
    widget_kis = 0;
    widget_kis_format = 0;
    widget_kis_items = 1;
    widget_time = 0;
    widget_totaltime = 0;
    widget_levelname = 0;
    widget_coords = 0;
    widget_speed = 0;
    widget_render = 0;
    widget_health = 0;
    // Automap
    automap_smooth_hr = 1;
    automap_thick = 0;
    automap_square = 0;
    automap_textured_bg = 1;
    automap_scroll_bg = 1;
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
    vis_swirling_liquids = 0;
    vis_invul_sky = 0;
    vis_linear_sky = 0;
    vis_flip_corpses = 0;

    // Crosshair
    xhair_draw = 0;
    xhair_color = 0;

    // Status bar
    st_colored_stbar = 0;
    st_ammo_widget = 0;
    st_ammo_widget_translucent = 0;
    st_ammo_widget_colors = 0;

    // Audible
    aud_z_axis_sfx = 0;

    // Physical
    phys_torque = 0;
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
    compat_auto_sr50 = 0;
    G_SetSideMove();
    compat_no_land_centering = 0;

    // Restart graphical systems
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    R_InitColormaps();
    R_InitLightTables();
    R_InitSkyMap();
    R_SetViewSize(dp_screen_size, dp_detail_level);
    R_ExecuteSetViewSize();
    P_SegLengths(true);
    P_InitPicAnims();
    I_ToggleVsync();
    I_SetPalette(sb_palette);
    R_FillBackScreen();
    AM_initOverlayMode();
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
    if (vid_fullscreen)
    {
        I_UpdateExclusiveFullScreen();
    }

    // Restart audio systems (sort of...)
    S_SetMaxVolume();
    S_SetMusicVolume();
}

static void M_ID_ApplyReset (void)
{
    post_rendering_hook = M_ID_ApplyResetHook;
}

static void M_ID_SettingReset (int choice)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 8;      // [JN] Settings reset

    if (!netgame && !demoplayback)
    {
        paused = true;
    }
}

static Menu_t *Menus[] = {
    &MainMenu,
    &EpisodeMenu,
    &SkillMenu,
    &OptionsMenu,
    &Options2Menu,
    &FilesMenu,
    &LoadMenu,
    &SaveMenu,
    // [JN] ID menu items
    &ID_Def_Main,
    &ID_Def_Video_1,
    &ID_Def_Video_2,
    &ID_Def_Display,
    &ID_Def_Sound,
    &ID_Def_Controls,
    &ID_Def_Keybinds_1,
    &ID_Def_Keybinds_2,
    &ID_Def_Keybinds_3,
    &ID_Def_Keybinds_4,
    &ID_Def_Keybinds_5,
    &ID_Def_Keybinds_6,
    &ID_Def_Keybinds_7,
    &ID_Def_Keybinds_8,
    &ID_Def_MouseBinds,
    &ID_Def_Widgets,
    &ID_Def_Automap,
    &ID_Def_Gameplay_1,
    &ID_Def_Gameplay_2,
    &ID_Def_Gameplay_3,
    &ID_Def_Gameplay_4,
    &ID_Def_Misc,
    &ID_Def_Level_1,
    &ID_Def_Level_2,
    &ID_Def_Level_3,
};

//---------------------------------------------------------------------------
//
// PROC MN_Init
//
//---------------------------------------------------------------------------

void MN_Init(void)
{
    InitFonts();
    // [JN] Initialize to prevent crashing on pressing "back".
    CurrentMenu = &MainMenu;
    MenuActive = false;
    SkullBaseLump = W_GetNumForName(DEH_String("M_SKL00"));

    // [JN] CRL - player is always local, "console" player.
    player = &players[consoleplayer];

    if (gamemode == retail)
    {                           // Add episodes 4 and 5 to the menu
        EpisodeMenu.itemCount = 5;
        EpisodeMenu.y -= ITEM_HEIGHT;
    }

    // [crispy] apply default difficulty
    SkillMenu.oldItPos = gp_default_skill;
    
    // [JN] Apply default first page of Keybinds and Gameplay menus.
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS1;
    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY1;
}

//---------------------------------------------------------------------------
//
// PROC InitFonts
//
//---------------------------------------------------------------------------

static void InitFonts(void)
{
    FontABaseLump = W_GetNumForName(DEH_String("FONTA_S")) + 1;
    FontBBaseLump = W_GetNumForName(DEH_String("FONTB_S")) + 1;
}

// [crispy] Check if printable character is existing in FONTA/FONTB sets
// and do a replacement or case correction if needed.

enum {
    big_font, small_font
} fontsize_t;

static const char MN_CheckValidChar (char ascii_index, int have_cursor)
{
    if ((ascii_index > 'Z' + have_cursor && ascii_index < 'a') || ascii_index > 'z')
    {
        // Replace "\]^_`" and "{|}~" with spaces,
        // allow "[" (cursor symbol) only in small fonts.
        return ' ';
    }
    else if (ascii_index >= 'a' && ascii_index <= 'z')
    {
        // Force lowercase "a...z" characters to uppercase "A...Z".
        return ascii_index + 'A' - 'a';
    }
    else
    {
        // Valid char, do not modify it's ASCII index.
        return ascii_index;
    }
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextA
//
// Draw text using font A.
//
//---------------------------------------------------------------------------

void MN_DrTextA (const char *text, int x, int y, byte *table)
{
    char c;
    patch_t *p;

    dp_translation = table;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

        if (c < 33)
        {
            x += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchOptional(x, y, 1, p);
            x += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextAWidth
//
// Returns the pixel width of a string using font A.
//
//---------------------------------------------------------------------------

int MN_TextAWidth(const char *text)
{
    char c;
    int width;
    patch_t *p;

    width = 0;
    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

        if (c < 33)
        {
            width += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            width += SHORT(p->width) - 1;
        }
    }
    return (width);
}

void MN_DrTextACentered (const char *text, int y, byte *table)
{
    char c;
    int cx;
    patch_t *p;

    cx = 160 - MN_TextAWidth(text) / 2;
    
    dp_translation = table;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

        if (c < 33)
        {
            cx += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchOptional(cx, y, 1, p);
            cx += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextB
//
// Draw text using font B.
//
//---------------------------------------------------------------------------

void MN_DrTextB(const char *text, int x, int y, byte *table)
{
    char c;
    patch_t *p;

    dp_translation = table;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, big_font); // [crispy] check for valid characters

        if (c < 33)
        {
            x += 8;
        }
        else
        {
            p = W_CacheLumpNum(FontBBaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchOptional(x, y, 1, p);
            x += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextBWidth
//
// Returns the pixel width of a string using font B.
//
//---------------------------------------------------------------------------

int MN_TextBWidth(const char *text)
{
    char c;
    int width;
    patch_t *p;

    width = 0;
    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, big_font); // [crispy] check for valid characters

        if (c < 33)
        {
            width += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontBBaseLump + c - 33, PU_CACHE);
            width += SHORT(p->width) - 1;
        }
    }
    return (width);
}

// -----------------------------------------------------------------------------
// MN_DrTextAGlow
// [JN] Write a centered string using the hu_font.
// -----------------------------------------------------------------------------

void MN_DrTextAGlow (const char *text, int x, int y, byte *table1, byte *table2, int alpha)
{
    char c;
    patch_t *p;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

        if (c < 33)
        {
            x += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);

            // Base patch
            dp_translation = table1;
            V_DrawShadowedPatchOptional(x, y, 1, p);

            // Glowing overlay
            dp_translation = table2;
            if (alpha == 255)
                V_DrawPatch(x, y, p);
            else if (alpha > 0)
                V_DrawFadePatch(x, y, p, alpha);

            x += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// MN_DrTextBGlow
// [JN] 
// -----------------------------------------------------------------------------

void MN_DrTextBGlow (const char *text, int x, int y, byte *table1, byte *table2, int alpha)
{
    char c;
    patch_t *p;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, big_font); // [crispy] check for valid characters

        if (c < 33)
        {
            x += 8;
        }
        else
        {
            p = W_CacheLumpNum(FontBBaseLump + c - 33, PU_CACHE);

            // Base patch
            dp_translation = table1;
            V_DrawShadowedPatchOptional(x, y, 1, p);

            // Glowing overlay
            dp_translation = table2;
            if (alpha == 255)
                V_DrawPatch(x, y, p);
            else if (alpha > 0)
                V_DrawFadePatch(x, y, p, alpha);

            x += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// PROC MN_Ticker
//
//---------------------------------------------------------------------------

void MN_Ticker(void)
{
    if (MenuActive == false)
    {
        return;
    }
    MenuTime++;

    // [JN] Don't go any farther with effects while active info screens.
    if (InfoType)
    {
        return;
    }

    // [JN] Cursor glowing animation:
    cursor_tics += cursor_direction ? -1 : 1;
    if (cursor_tics == 0 || cursor_tics == 15)
    {
        cursor_direction = !cursor_direction;
    }

    // [JN] Menu item fading effect:
    // Keep menu item bright or decrease tics for fading effect.
    for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
    {
        if (menu_highlight == 1)
        {
            CurrentMenu->items[i].tics = (CurrentItPos == i) ? 5 :
                (CurrentMenu->items[i].tics > 0 ? CurrentMenu->items[i].tics - 1 : 0);
        }
        else
        if (menu_highlight == 2)
        {
            CurrentMenu->items[i].tics =
                (CurrentItPos == i) ? 5 : 0;
        }
        else
        {
            CurrentMenu->items[i].tics = 0;
        }
    }
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
        const int line_height = (CurrentMenu->FontType == SmallFont) ?
                                 ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;

        // [JN] Reset current menu item, it will be set in a cycle below.
        CurrentItPos = -1;

        // [PN] Check if the cursor is hovering over a menu item
        for (int i = 0; i < CurrentMenu->itemCount; i++)
        {
            // [JN] Slider takes three lines.
            const int line_item = CurrentMenu->items[i].type == ITT_SLDR ? 3 : 1;

            if (menu_mouse_x >= (CurrentMenu->x + WIDESCREENDELTA) * vid_resolution
            &&  menu_mouse_x <= (ORIGWIDTH + WIDESCREENDELTA - CurrentMenu->x) * vid_resolution
            &&  menu_mouse_y >= (CurrentMenu->y + i * line_height) * vid_resolution
            &&  menu_mouse_y <= (CurrentMenu->y + (i + line_item) * line_height) * vid_resolution
            &&  CurrentMenu->items[i].type != ITT_EMPTY)
            {
                // [PN] Highlight the current menu item
                CurrentItPos = i;
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
    const int adj_height = ITEM_HEIGHT * vid_resolution;

    // [PN] Check cursor position and item status
    if (menu_mouse_x < adj_x || menu_mouse_x > adj_x + adj_width
    ||  menu_mouse_y < adj_y || menu_mouse_y > adj_y + adj_height
    ||  CurrentMenu->items[CurrentItPos].type != ITT_SLDR)
        return;

    // [PN] Calculate and update slider value
    const float normalized = (float)(menu_mouse_x - adj_x + 5) / adj_width;
    const float newValue = min + normalized * (max - min);
    if (is_float)
        *((float *)value) = newValue;
    else
        *((int *)value) = (int)newValue;

    // [JN/PN] Call related routine and reset mouse click allowance
    CurrentMenu->items[CurrentItPos].func(-1);
    menu_mouse_allow_click = false;

    // Play sound
    S_StartSound(NULL, sfx_keyup);
}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

static const char *QuitEndMsg[] = {
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED",
    "DO YOU WANT TO DELETE THE GAME NAMED",        // [crispy] typeofask 5 (delete a savegame)
    "RESET KEYBOARD BINDINGS TO DEFAULT VALUES?",  // [JN] typeofask 6 (reset keyboard binds)
    "RESET MOUSE BINDINGS TO DEFAULT VALUES?",     // [JN] typeofask 7 (reset mouse binds)
    "",                                            // [JN] typeofask 8 (setting reset), full text in drawer below
};

void MN_Drawer(void)
{
    int i;
    int x;
    int y;
    MenuItem_t *item;
    const char *message;
    const char *selName;

    if (MenuActive || typeofask)
    {
        // Temporary unshade while changing certain settings.
        if (shade_wait < I_GetTime())
        {
            M_ShadeBackground();
        }
        // Always redraw status bar background.
        SB_ForceRedraw();
    }

    if (MenuActive == false)
    {
        // [JN] Disallow cursor if menu is not active.
        menu_mouse_allow = false;

        if (askforquit)
        {
            message = DEH_String(QuitEndMsg[typeofask - 1]);
            // [JN] Allow cursor while active type of asking.
            menu_mouse_allow = true;

            // [JN] Keep backgound filling while asking for 
            // reset and inform about Y or N pressing.
            if (typeofask == 6 || typeofask == 7)
            {
                M_FillBackground();
                MN_DrTextACentered("PRESS Y OR N.", 100, NULL);
            }

            MN_DrTextA(message, 160 - MN_TextAWidth(message) / 2, 80, NULL);
            if (typeofask == 3)
            {
                MN_DrTextA(SlotText[quicksave - 1], 160 -
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
            }
            if (typeofask == 4)
            {
                MN_DrTextA(SlotText[quickload - 1], 160 -
                           MN_TextAWidth(SlotText[quickload - 1]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[quickload - 1]) / 2, 90, NULL);
            }
            if (typeofask == 5)
            {
                MN_DrTextA(SlotText[CurrentItPos], 160 -
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
            }
            if (typeofask == 8)
            {
                MN_DrTextACentered("GRAPHICAL, AUDIO AND GAMEPLAY SETTINGS", 70, NULL);
                MN_DrTextACentered("WILL BE RESET TO DEFAULT VALUES.", 80, NULL);
                MN_DrTextACentered("ARE YOU SURE WANT TO CONTINUE?", 100, NULL);
                MN_DrTextACentered("PRESS Y OR N.", 120, NULL);
            }
        }
        return;
    }
    else
    {
        if (InfoType)
        {
            MN_DrawInfo();
            return;
        }
        if (CurrentMenu->drawFunc != NULL)
        {
            CurrentMenu->drawFunc();
        }
        x = CurrentMenu->x;
        y = CurrentMenu->y;
        item = CurrentMenu->items;
        for (i = 0; i < CurrentMenu->itemCount; i++)
        {
            if (item->type != ITT_EMPTY && item->text)
            {
                if (CurrentMenu->FontType == SmallFont)
                {
                    MN_DrTextAGlow(DEH_String(item->text), x, y, cr[CR_MENU_DARK2], cr[CR_MENU_BRIGHT2], LINE_ALPHA(i));
                }
                else
                if (CurrentMenu->FontType == BigFont)
                {
                    MN_DrTextBGlow(DEH_String(item->text), x, y, NULL, cr[CR_MENU_BRIGHT4], LINE_ALPHA(i));
                }
                // [JN] Else, don't draw file slot names (1, 2, 3, ...) in Save/Load menus.
            }
            y += CurrentMenu->FontType == SmallFont ? ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;
            item++;
        }
        
        if (CurrentItPos != -1)
        {
            if (CurrentMenu->FontType == SmallFont)
            {
                y = CurrentMenu->y + (CurrentItPos * ID_MENU_LINEHEIGHT_SMALL);
                MN_DrTextAGlow("*", x - ID_MENU_CURSOR_OFFSET, y, cr[CR_MENU_DARK4], cr[CR_MENU_BRIGHT5],
                                    menu_highlight == 1 ? (cursor_tics * 17)           :  // Animated
                                    menu_highlight == 0 ? (MenuTime & 16 ?  50 : 150)  :  // Off
                                                          (MenuTime & 16 ? 100 : 200)) ;  // Static
            }
            else
            {
                y = CurrentMenu->y + (CurrentItPos * ITEM_HEIGHT) + SELECTOR_YOFFSET;
                selName = DEH_String(MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2");
                V_DrawShadowedPatchOptional(x + SELECTOR_XOFFSET, y, 1, W_CacheLumpName(selName, PU_CACHE));
            }
        }
    }

    // [JN] Always refresh statbar while menu is active.
    SB_ForceRedraw();

    // [JN] Call the menu control routine for mouse input.
    M_ID_MenuMouseControl();
}

//---------------------------------------------------------------------------
//
// PROC DrawMainMenu
//
//---------------------------------------------------------------------------

static void DrawMainMenu(void)
{
    int frame;

    frame = (MenuTime / 3) % 18;
    V_DrawPatch(88, 0, W_CacheLumpName(DEH_String("M_HTIC"), PU_CACHE));
    V_DrawPatch(40, 10, W_CacheLumpNum(SkullBaseLump + (17 - frame),
                                       PU_CACHE));
    V_DrawPatch(232, 10, W_CacheLumpNum(SkullBaseLump + frame, PU_CACHE));
}

//---------------------------------------------------------------------------
//
// PROC DrawEpisodeMenu
//
//---------------------------------------------------------------------------

static void DrawEpisodeMenu(void)
{
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawSkillMenu(void)
{
}

//---------------------------------------------------------------------------
//
// PROC DrawFilesMenu
//
//---------------------------------------------------------------------------

static void DrawFilesMenu(void)
{
// clear out the quicksave/quickload stuff
    quicksave = 0;
    quickload = 0;
}

// [crispy] support additional pages of savegames
static void DrawSaveLoadBottomLine(const Menu_t *menu)
{
    char pagestr[16];
    static short width;
    const int y = menu->y + ITEM_HEIGHT * SAVES_PER_PAGE;

    if (!width)
    {
        const patch_t *const p = W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE);
        width = SHORT(p->width);
    }
    if (savepage > 0)
        MN_DrTextA("PGUP", menu->x + 1, y, cr[CR_MENU_DARK4]);
    if (savepage < SAVEPAGE_MAX)
        MN_DrTextA("PGDN", menu->x + width - MN_TextAWidth("PGDN"), y, cr[CR_MENU_DARK4]);

    M_snprintf(pagestr, sizeof(pagestr), "PAGE %d/%d", savepage + 1, SAVEPAGE_MAX + 1);
    MN_DrTextA(pagestr, ORIGWIDTH / 2 - MN_TextAWidth(pagestr) / 2, y, cr[CR_MENU_DARK4]);

    // [JN] Print "modified" (or created initially) time of savegame file.
    if (SlotStatus[CurrentItPos] && !FileMenuKeySteal)
    {
        struct stat filestat;
        char filedate[32];

        if (M_stat(SV_Filename(CurrentItPos), &filestat) == 0)
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
        MN_DrTextACentered(filedate, y + 10, cr[CR_MENU_DARK4]);
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

static void DrawLoadMenu(void)
{
    const char *title;

    title = DEH_String(quickloadTitle ? "QUICK LOAD GAME" : "LOAD GAME");

    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&LoadMenu);
    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 1, NULL);
    DrawSaveLoadBottomLine(&LoadMenu);
}

//---------------------------------------------------------------------------
//
// PROC DrawSaveMenu
//
//---------------------------------------------------------------------------

static void DrawSaveMenu(void)
{
    const char *title;

    title = DEH_String(quicksaveTitle ? "QUICK SAVE GAME" : "SAVE GAME");

    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&SaveMenu);
    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 1, NULL);
    DrawSaveLoadBottomLine(&SaveMenu);
}

//===========================================================================
//
// MN_LoadSlotText
//
//              Loads in the text message for each slot
//===========================================================================

void MN_LoadSlotText(void)
{
    FILE *fp;
    int i;
    char *filename;

    for (i = 0; i < SAVES_PER_PAGE; i++)
    {
        int retval;
        filename = SV_Filename(i);
        fp = M_fopen(filename, "rb+");
	free(filename);

        if (!fp)
        {
            SlotText[i][0] = 0; // empty the string
            SlotStatus[i] = 0;
            continue;
        }
        retval = fread(&SlotText[i], 1, SLOTTEXTLEN, fp);
        fclose(fp);
        SlotStatus[i] = retval == SLOTTEXTLEN;
    }
    slottextloaded = true;
}

//---------------------------------------------------------------------------
//
// PROC DrawFileSlots
//
//---------------------------------------------------------------------------

static void DrawFileSlots(Menu_t * menu)
{
    int i;
    int x;
    int y;

    x = menu->x;
    y = menu->y;
    for (i = 0; i < SAVES_PER_PAGE; i++)
    {
        // [JN] Highlight selected item (CurrentItPos == i) or apply fading effect.
        dp_translation = (CurrentItPos == i && menu_highlight) ? cr[CR_MENU_BRIGHT2] : NULL;
        V_DrawShadowedPatchOptional(x, y, 1, W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE));
        dp_translation = false;

        if (SlotStatus[i])
        {
            MN_DrTextAGlow(SlotText[i], x + 5, y + 5,
                                cr[CR_MENU_DARK2],
                                    (CurrentItPos == i) ? cr[CR_MENU_BRIGHT2] : cr[CR_MENU_DARK2],
                                        LINE_ALPHA(i));
        }
        y += ITEM_HEIGHT;
    }

    // [JN] Forcefully hide the mouse cursor while typing.
    if (FileMenuKeySteal)
    {
        menu_mouse_allow = false;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawOptionsMenu
//
//---------------------------------------------------------------------------

static void DrawOptionsMenu(void)
{
    if (msg_show)
    {
        MN_DrTextB(DEH_String("ON"), 196, 50, NULL);
    }
    else
    {
        MN_DrTextB(DEH_String("OFF"), 196, 50, NULL);
    }
    DrawSlider(&OptionsMenu, 3, 10, mouseSensitivity, true, 0);
}

//---------------------------------------------------------------------------
//
// PROC DrawOptions2Menu
//
//---------------------------------------------------------------------------

static void DrawOptions2Menu(void)
{
    char str[32];

    // SFX Volume
    sprintf(str, "%d", snd_MaxVolume);
    DrawSlider(&Options2Menu, 1, 16, snd_MaxVolume, true, 0);
    M_ID_HandleSliderMouseControl(94, 40, 132, &snd_MaxVolume, false, 0, 15);
    MN_DrTextAGlow(str, 252, 45,
                        snd_MaxVolume ? cr[CR_LIGHTGRAY] : cr[CR_MENU_DARK4],
                        snd_MaxVolume ? cr[CR_LIGHTGRAY_BRIGHT] : cr[CR_MENU_DARK1],
                        LINE_ALPHA(0));

    // Music Volume
    sprintf(str, "%d", snd_MusicVolume);
    DrawSlider(&Options2Menu, 3, 16, snd_MusicVolume, true, 2);
    M_ID_HandleSliderMouseControl(94, 80, 132, &snd_MusicVolume, false, 0, 15);
    MN_DrTextAGlow(str, 252, 85,
                        snd_MusicVolume ? cr[CR_LIGHTGRAY] : cr[CR_MENU_DARK4],
                        snd_MaxVolume ? cr[CR_LIGHTGRAY_BRIGHT] : cr[CR_MENU_DARK1],
                        LINE_ALPHA(2));

    // Screen Size
    sprintf(str, "%d", dp_screen_size);
    DrawSlider(&Options2Menu, 5,  11, dp_screen_size - 3, true, 4);
    M_ID_HandleSliderMouseControl(94, 120, 92, &dp_screen_size, false, 3, 13);
    MN_DrTextAGlow(str, 212, 125,
                        cr[CR_LIGHTGRAY],
                            cr[CR_LIGHTGRAY_BRIGHT],
                                LINE_ALPHA(4));
}

//---------------------------------------------------------------------------
//
// PROC SCNetCheck
//
//---------------------------------------------------------------------------

static boolean SCNetCheck(int option)
{
    if (!netgame)
    {                           // okay to go into the menu
        return true;
    }
    switch (option)
    {
        case 1:
            CT_SetMessage(&players[consoleplayer],
                         "YOU CAN'T START A NEW GAME IN NETPLAY!", true, NULL);
            break;
        case 2:
            CT_SetMessage(&players[consoleplayer],
                         "YOU CAN'T LOAD A GAME IN NETPLAY!", true, NULL);
            break;
        case 3:                // end game
            CT_SetMessage(&players[consoleplayer],
                         "YOU CAN'T END A GAME IN NETPLAY!", true, NULL);
            break;
    }
    MenuActive = false;
    return false;
}

static void SCNetCheck2(int option)
{
    SCNetCheck(option);
}

//---------------------------------------------------------------------------
//
// PROC SCQuitGame
//
//---------------------------------------------------------------------------

static void SCQuitGame(int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 1;              //quit game
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
}

//---------------------------------------------------------------------------
//
// PROC SCEndGame
//
//---------------------------------------------------------------------------

static void SCEndGame(int option)
{
    if (demoplayback || netgame)
    {
        return;
    }
    if (SCNetCheck(3))
    {
        MenuActive = false;
        askforquit = true;
        typeofask = 2;              //endgame
        if (!netgame && !demoplayback)
        {
            paused = true;
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static void SCLoadGame(int option)
{
    char *filename;

    if (!SlotStatus[option])
    {                           // slot's empty...don't try and load
        return;
    }

    filename = SV_Filename(option);
    G_LoadGame(filename);
    free(filename);

    MN_DeactivateMenu();
    if (quickload == -1)
    {
        quickload = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
}

static void SCDeleteGame(int option)
{
    char *filename;

    if (!SlotStatus[option])
    {
        return;
    }

    filename = SV_Filename(option);
    remove(filename);
    free(filename);

    CurrentMenu->oldItPos = CurrentItPos;  // [JN] Do not reset cursor position.
    MN_LoadSlotText();
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

// [crispy] override savegame name if it already starts with a map identifier
static boolean StartsWithMapIdentifier (char *str)
{
    if (strlen(str) >= 4 &&
        toupper(str[0]) == 'E' && isdigit(str[1]) &&
        toupper(str[2]) == 'M' && isdigit(str[3]))
    {
        return true;
    }

    return false;
}

// [JN] Check if Save Game menu should be accessable.
static void SCSaveCheck(int option)
{
    if (!usergame)
    {
        CT_SetMessage(&players[consoleplayer],
                     "YOU CAN'T SAVE IF YOU AREN'T PLAYING", true, NULL);
    }
    else
    {
        SetMenu(MENU_SAVE);
    }
}

static void SCSaveGame(int option)
{
    char *ptr;

    if (!FileMenuKeySteal)
    {
        int x, y;

        FileMenuKeySteal = true;
        // We need to activate the text input interface to type the save
        // game name:
        x = SaveMenu.x + 1;
        y = SaveMenu.y + 1 + option * ITEM_HEIGHT;
        I_StartTextInput(x, y, x + 190, y + ITEM_HEIGHT - 2);

        M_StringCopy(oldSlotText, SlotText[option], sizeof(oldSlotText));
        ptr = SlotText[option];
        // [crispy] generate a default save slot name when the user saves to an empty slot
        if (!oldSlotText[0] || StartsWithMapIdentifier(oldSlotText))
          M_snprintf(ptr, sizeof(oldSlotText), "E%dM%d", gameepisode, gamemap);
        while (*ptr)
        {
            ptr++;
        }
        *ptr = '[';
        *(ptr + 1) = 0;
        SlotStatus[option]++;
        currentSlot = option;
        slotptr = ptr - SlotText[option];
        return;
    }
    else
    {
        G_SaveGame(option, SlotText[option]);
        FileMenuKeySteal = false;
        I_StopTextInput();
        MN_DeactivateMenu();
    }
    if (quicksave == -1)
    {
        quicksave = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
}

//---------------------------------------------------------------------------
//
// PROC SCEpisode
//
//---------------------------------------------------------------------------

static void SCEpisode(int option)
{
    if (gamemode == shareware && option > 1)
    {
        CT_SetMessage(&players[consoleplayer],
                     "ONLY AVAILABLE IN THE REGISTERED VERSION", true, NULL);
    }
    else
    {
        MenuEpisode = option;
        SetMenu(MENU_SKILL);
    }
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static void SCSkill(int option)
{
    G_DeferedInitNew(option, MenuEpisode, 1);
    MN_DeactivateMenu();
}

//---------------------------------------------------------------------------
//
// PROC SCMouseSensi
//
//---------------------------------------------------------------------------

static void SCMouseSensi(int option)
{
    // [crispy] extended range
    mouseSensitivity = M_INT_Slider(mouseSensitivity, 0, 255, option, true);
}

static void SCMouseSensi_y(int option)
{
    // [crispy] extended range
    mouse_sensitivity_y = M_INT_Slider(mouse_sensitivity_y, 0, 255, option, true);
}

//---------------------------------------------------------------------------
//
// PROC SCSfxVolume
//
//---------------------------------------------------------------------------

static void SCSfxVolume(int option)
{
    snd_MaxVolume = M_INT_Slider(snd_MaxVolume, 0, 15, option, true);
    // [JN] Always do a full recalc of the sound curve imideatelly.
    // Needed for proper volume update of ambient sounds
    // while active menu and in demo playback mode.
    S_SetMaxVolume();
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static void SCMusicVolume(int option)
{
    snd_MusicVolume = M_INT_Slider(snd_MusicVolume, 0, 15, option, true);
    S_SetMusicVolume();
}

static void SCChangeDetail (int option)
{
    dp_detail_level ^= 1;
    R_SetViewSize (dp_screen_size, dp_detail_level);
    CT_SetMessage(&players[consoleplayer], DEH_String(!dp_detail_level ? 
                                           DETAILHI : DETAILLO), false, NULL);
}

//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static void SCScreenSize (int option)
{
    // [PN] Simplified logic for adjusting screen size
    if (option == LEFT_DIR && dp_screen_size > 3)
    {
        dp_screen_size--;
        // [JN] Skip wide status bar in non-wide screen mode.
        if (!vid_widescreen && dp_screen_size == 11)
        {
            dp_screen_size = 10;
        }
    }
    else
    if (option == RIGHT_DIR && dp_screen_size < 13)
    {
        dp_screen_size++;
        // [JN] Skip wide status bar in non-wide screen mode.
        if (!vid_widescreen && dp_screen_size == 11)
        {
            dp_screen_size = 12;
        }
    }

    // [PN] Update view size after adjustment
    R_SetViewSize(dp_screen_size, dp_detail_level);
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static void SCInfo(int option)
{
    InfoType = 1;
    S_StartSound(NULL, sfx_dorcls);
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
}

//---------------------------------------------------------------------------
// [crispy] reload current level / go to next level
// adapted from prboom-plus/src/e6y.c:369-449
//---------------------------------------------------------------------------

static int G_ReloadLevel (void)
{
    int result = false;

    if (gamestate == GS_LEVEL)
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

static int G_GotoNextLevel(void)
{
    static byte heretic_next[6][9] = {
    {12, 13, 14, 15, 16, 19, 18, 21, 17},
    {22, 23, 24, 29, 26, 27, 28, 31, 25},
    {32, 33, 34, 39, 36, 37, 38, 41, 35},
    {42, 43, 44, 49, 46, 47, 48, 51, 45},
    {52, 53, 59, 55, 56, 57, 58, 61, 54},
    {62, 63, 11, 11, 11, 11, 11, 11, 11}, // E6M4-E6M9 shouldn't be accessible
    };

    int changed = false;

    if (gamemode == shareware)
        heretic_next[0][7] = 11;

    if (gamemode == registered)
        heretic_next[2][7] = 11;

    if (gamestate == GS_LEVEL)
    {
        int epsd, map;

        epsd = heretic_next[gameepisode-1][gamemap-1] / 10;
        map = heretic_next[gameepisode-1][gamemap-1] % 10;

        G_DeferedInitNew(gameskill, epsd, map);
        changed = true;
    }

    return changed;
}

static void MN_ReturnToMenu (void)
{
	Menu_t *cur = CurrentMenu;
	MN_ActivateMenu();
	CurrentMenu = cur;
	CurrentItPos = CurrentMenu->oldItPos;
}

//---------------------------------------------------------------------------
//
// FUNC MN_Responder
//
//---------------------------------------------------------------------------

static boolean MN_ID_TypeOfAsk (void)
{
    switch (typeofask)
    {
        case 1:
            G_CheckDemoStatus();
            I_Quit();
            break;

        case 2:
            players[consoleplayer].messageTics = 0;
            //set the msg to be cleared
            players[consoleplayer].message = NULL;
            paused = false;
            I_SetPalette(0);
            D_StartTitle();     // go to intro/demo mode.
            break;

        case 3:
            CT_SetMessage(&players[consoleplayer],
                         "QUICKSAVING....", false, NULL);
            FileMenuKeySteal = true;
            SCSaveGame(quicksave - 1);
            break;

        case 4:
            CT_SetMessage(&players[consoleplayer],
                         "QUICKLOADING....", false, NULL);
            SCLoadGame(quickload - 1);
            break;

        case 5:
            SCDeleteGame(CurrentItPos);
            MN_ReturnToMenu();
            break;

        case 6: // [JN] Reset keybinds.
            M_ResetBinds();
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            MN_ReturnToMenu();
            break;

        case 7: // [JN] Reset mouse binds.
            M_ResetMouseBinds();
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            MN_ReturnToMenu();
            break;

        case 8: // [JN] Setting reset.
            M_ID_ApplyReset();
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            MN_ReturnToMenu();
        default:
            break;
    }

    askforquit = false;
    typeofask = 0;
    return true;
}

boolean MN_Responder(event_t * event)
{
    int charTyped;
    int key;
    int i;
    MenuItem_t *item;
    char *textBuffer;
    static int mousewait = 0;
    static int mousey = 0;
    static int lasty = 0;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (event->type == ev_quit
         || (event->type == ev_keydown
          && (event->data1 == key_menu_activate
           || event->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (event->type == ev_quit)
    {
        // First click on close = bring up quit confirm message.
        // Second click = confirm quit.

        if (!MenuActive && askforquit && typeofask == 1)
        {
            G_CheckDemoStatus();
            I_Quit();
        }
        else
        {
            SCQuitGame(0);
            S_StartSound(NULL, sfx_chat);
        }
        return true;
    }

    // key is the key pressed, ch is the actual character typed
  
    charTyped = 0;
    key = -1;

    // Allow the menu to be activated from a joystick button if a button
    // is bound for joybmenu.
    if (event->type == ev_joystick)
    {
        if (joybmenu >= 0 && (event->data1 & (1 << joybmenu)) != 0)
        {
            MN_ActivateMenu();
            return true;
        }
    }
    else
    {
        // [JN] Shows the mouse cursor when moved.
        if (event->data2 || event->data3)
        {
        menu_mouse_allow = true;
        menu_mouse_allow_click = false;
        }

        // [JN] Allow menu control by mouse.
        if (event->type == ev_mouse && mousewait < I_GetTime())
        {
            // [crispy] mouse_novert disables controlling the menus with the mouse
            // [JN] Not needed, as menu is fully controllable by mouse wheel and buttons.
            /*
            if (!mouse_novert)
            {
                mousey += event->data3;
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

            // [JN] Handle mouse bindings before going any farther.
            // Catch only button pressing events, i.e. event->data1.
            if (MouseIsBinding && event->data1 && !event->data2 && !event->data3)
            {
                M_CheckMouseBind(SDL_mouseButton);
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 5;
                return true;
            }

            if (event->data1 & 1)
            {
                if (MenuActive && CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // [JN] Allow repetitive on sliders to move it while mouse movement.
                    menu_mouse_allow_click = true;      
                }
                else
                if (!event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
                {
                if (!MenuActive && !usergame)
                {
                    // [JN] Open the main menu if the game is not active.
                    MN_ActivateMenu();
                }
                else
                {
                key = key_menu_forward;
                mousewait = I_GetTime() + 1;
                }
                }

                if (typeofask
                && !event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
                {
                    MN_ID_TypeOfAsk();
                }
            }

            if (event->data1 & 2
            && !event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
            {
                if (!MenuActive && !usergame)
                {
                    // [JN] Open the main menu if the game is not active.
                    MN_ActivateMenu();
                }
                else
                if (FileMenuKeySteal)
                {
                    key = KEY_ESCAPE;
                    FileMenuKeySteal = false;
                }
                else
                {
                    key = key_menu_back;
                }

                // [JN] Properly return to active menu.
                if (askforquit)
                {
                    askforquit = false;
                    typeofask = 0;
                    MenuActive = true;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                }
                mousewait = I_GetTime() + 1;
            }

            // [JN] Scrolls through menu item values or navigates between pages.
            if (event->data1 & (1 << 4) && MenuActive)  // Wheel down
            {
                if (CurrentItPos == -1
                || (CurrentMenu->ScrollAR && !FileMenuKeySteal && !KbdIsBinding))
                {
                    M_ScrollPages(1);
                }
                else
                if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2
                ||  CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // Scroll menu item backward normally, or forward for ITT_LRFUNC2
                    CurrentMenu->items[CurrentItPos].func(CurrentMenu->items[CurrentItPos].type != ITT_LRFUNC2 ? LEFT_DIR : RIGHT_DIR);
                    S_StartSound(NULL, sfx_keyup);
                }
                mousewait = I_GetTime();
            }
            else
            if (event->data1 & (1 << 3) && MenuActive)  // Wheel up
            {
                if (CurrentItPos == -1
                || (CurrentMenu->ScrollAR && !FileMenuKeySteal && !KbdIsBinding))
                {
                    M_ScrollPages(0);
                }
                else
                if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2
                ||  CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // Scroll menu item forward normally, or backward for ITT_LRFUNC2
                    CurrentMenu->items[CurrentItPos].func(CurrentMenu->items[CurrentItPos].type != ITT_LRFUNC2 ? RIGHT_DIR : LEFT_DIR);
                    S_StartSound(NULL, sfx_keyup);
                }
                mousewait = I_GetTime();
            }
        }
        else
        {
            if (event->type == ev_keydown)
            {
                key = event->data1;
                charTyped = event->data2;
                // [JN] Hide mouse cursor by pressing a key.
                menu_mouse_allow = false;
            }
        }
    }

    if (key == -1)
    {
        return false;
    }

    if (InfoType)
    {
        if (gamemode == shareware)
        {
            InfoType = (InfoType + 1) % 5;
        }
        else
        {
            InfoType = (InfoType + 1) % 4;
        }
        if (key == KEY_ESCAPE)
        {
            InfoType = 0;
        }
        if (!InfoType)
        {
            paused = false;
            MN_DeactivateMenu();
            SB_state = -1;      //refresh the statbar
        }
        S_StartSound(NULL, sfx_dorcls);
        return (true);          //make the info screen eat the keypress
    }

    // [JN] Handle keyboard bindings:
    if (KbdIsBinding)
    {
        if (event->type == ev_mouse)
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
        if (event->type != ev_mouse)
        {
            btnToBind = 0;
            MouseIsBinding = false;
            return false;
        }
    }


    if ((ravpic && key == KEY_F1) ||
        (key != 0 && key == key_menu_screenshot))
    {
        G_ScreenShot();
        // [JN] Audible feedback.
        S_StartSound(NULL, sfx_itemup);
        return (true);
    }

    if (askforquit)
    {
        if (key == key_menu_confirm
        // [JN] Allow to confirm quit (1) and end game (2) by pressing Enter.
        || (key == key_menu_forward && (typeofask == 1 || typeofask == 2))
        // [JN] Confirm by left mouse button.
        || (event->type == ev_mouse && event->data1 & 1)
        // [JN] Allow to exclusevely confirm quit game by pressing F10 again.
        || (key == key_menu_quit && typeofask == 1))
        {
            MN_ID_TypeOfAsk();
        }
        else
        if (key == key_menu_abort || key == KEY_ESCAPE
        || (event->type == ev_mouse && event->data1 & 2))  // [JN] Cancel by right mouse button.
        {
            // [JN] Do not close reset menus after canceling.
            if (typeofask == 6 || typeofask == 7 || typeofask == 8)
            {
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                MenuActive = true;
                askforquit = false;
                typeofask = 0;
                return true;
            }
            else
            {
                players[consoleplayer].messageTics = 1;  //set the msg to be cleared
                askforquit = false;
                typeofask = 0;
                paused = false;
                return true;
            }
        }

        return false;           // don't let the keys filter thru
    }

    if (!MenuActive && !chatmodeon)
    {
        if (key == key_menu_decscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(LEFT_DIR);
            S_StartSound(NULL, sfx_keyup);
            return (true);
        }
        else if (key == key_menu_incscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(RIGHT_DIR);
            S_StartSound(NULL, sfx_keyup);
            return (true);
        }
        else if (key == key_menu_help)           // F1
        {
            SCInfo(0);      // start up info screens
            MenuActive = true;
            return (true);
        }
        else if (key == key_menu_save)           // F2 (save game)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                MenuActive = true;
                FileMenuKeySteal = false;
                MenuTime = 0;
                CurrentMenu = &SaveMenu;
                CurrentItPos = CurrentMenu->oldItPos;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                S_StartSound(NULL, sfx_dorcls);
                slottextloaded = false;     //reload the slot text, when needed
                quicksaveTitle = false;  // [JN] "Save game" title.
            }
            return true;
        }
        else if (key == key_menu_load)           // F3 (load game)
        {
            if (SCNetCheck(2))
            {
                MenuActive = true;
                FileMenuKeySteal = false;
                MenuTime = 0;
                CurrentMenu = &LoadMenu;
                CurrentItPos = CurrentMenu->oldItPos;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                S_StartSound(NULL, sfx_dorcls);
                slottextloaded = false;     //reload the slot text, when needed
                quickloadTitle = false;  // [JN] "Load game" title.
            }
            return true;
        }
        else if (key == key_menu_volume)         // F4 (volume)
        {
            MenuActive = true;
            FileMenuKeySteal = false;
            MenuTime = 0;
            CurrentMenu = &Options2Menu;
            CurrentItPos = CurrentMenu->oldItPos;
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            S_StartSound(NULL, sfx_dorcls);
            slottextloaded = false; //reload the slot text, when needed
            return true;
        }
        else if (key == key_menu_detail)          // F5 (detail)
        {
            SCChangeDetail(0);
            S_StartSound(NULL, sfx_switch);
            return true;
        }
        else if (key == key_menu_qsave)           // F6 (quicksave)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                if (!quicksave || quicksave == -1)
                {
                    MenuActive = true;
                    FileMenuKeySteal = false;
                    MenuTime = 0;
                    CurrentMenu = &SaveMenu;
                    CurrentItPos = CurrentMenu->oldItPos;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    S_StartSound(NULL, sfx_dorcls);
                    slottextloaded = false; //reload the slot text, when needed
                    quicksave = -1;
                    // [JN] "Quick save game" title instead of message.
                    quicksaveTitle = true;
                }
                else
                {
                    // [JN] Do not ask for quick save confirmation.
                    S_StartSound(NULL, sfx_dorcls);
                    FileMenuKeySteal = true;
                    SCSaveGame(quicksave - 1);
                }
            }
            return true;
        }
        else if (key == key_menu_endgame)         // F7 (end game)
        {
            if (SCNetCheck(3))
            {
                if (gamestate == GS_LEVEL && !demoplayback)
                {
                    S_StartSound(NULL, sfx_chat);
                    SCEndGame(0);
                }
            }
            return true;
        }
        else if (key == key_menu_messages)        // F8 (toggle messages)
        {
            M_ID_Messages(0);
            return true;
        }
        else if (key == key_menu_qload)           // F9 (quickload)
        {
            if (SCNetCheck(2))
            {
                if (!quickload || quickload == -1)
                {
                    MenuActive = true;
                    FileMenuKeySteal = false;
                    MenuTime = 0;
                    CurrentMenu = &LoadMenu;
                    CurrentItPos = CurrentMenu->oldItPos;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    S_StartSound(NULL, sfx_dorcls);
                    slottextloaded = false;     //reload the slot text, when needed
                    quickload = -1;
                    // [JN] "Quick load game" title instead of message.
                    quickloadTitle = true;
                }
                else
                {
                    // [JN] Do not ask for quick load confirmation.
                    SCLoadGame(quickload - 1);
                }
            }
            return true;
        }
        else if (key == key_menu_quit)            // F10 (quit)
        {
            // [JN] Allow to invoke quit in any game state.
            //if (gamestate == GS_LEVEL)
            {
                SCQuitGame(0);
                S_StartSound(NULL, sfx_chat);
            }
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
                // [JN] TODO - trying to go to next level while paused state
                // is restarting current level with demo desync. But why?
                if (paused)
                {
                    return true;
                }
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
    if (key == key_menu_gamma)           // F11 (gamma correction)
    {
        vid_gamma = M_INT_Slider(vid_gamma, 0, MAXGAMMA-1, 1 /*right*/, false);
        CT_SetMessage(&players[consoleplayer], gammalvls[vid_gamma][0], false, NULL);
        I_SetPalette(0);
        R_InitColormaps();
        R_FillBackScreen();
        SB_state = -1;
        return true;
    }

    if (!MenuActive)
    {
        // [JN] Open Heretic/CRL menu only by pressing it's keys to allow 
        // certain CRL features to be toggled. This behavior is same to Doom.
        if (key == key_menu_activate)
        {
            MN_ActivateMenu();

            return (true);
        }
        return (false);
    }

    if (!FileMenuKeySteal)
    {
        item = &CurrentMenu->items[CurrentItPos];

        if (key == key_menu_down)            // Next menu item
        {
            do
            {
                if (CurrentItPos + 1 > CurrentMenu->itemCount - 1)
                {
                    CurrentItPos = 0;
                }
                else
                {
                    CurrentItPos++;
                }
            }
            while (CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
            S_StartSound(NULL, sfx_switch);
            return (true);
        }
        else if (key == key_menu_up)         // Previous menu item
        {
            // [JN] Current menu item was hidden while mouse controls,
            // so move cursor to last one menu item by pressing "up" key.
            if (CurrentItPos == -1)
            {
                CurrentItPos = CurrentMenu->itemCount;
            }

            do
            {
                if (CurrentItPos == 0)
                {
                    CurrentItPos = CurrentMenu->itemCount - 1;
                }
                else
                {
                    CurrentItPos--;
                }
            }
            while (CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
            S_StartSound(NULL, sfx_switch);
            return (true);
        }
        else if (key == key_menu_activate)     // Toggle menu
        {
            // [JN] If ESC key behaviour is set to "go back":
            if (menu_esc_key)
            {
                if (CurrentMenu == &MainMenu || CurrentMenu == &Options2Menu
                ||  CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
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
            MN_DeactivateMenu();
            }
            return (true);
        }
        else if (key == key_menu_back)         // Go back to previous menu
        {
            id_prev_menu:
            if (CurrentMenu->prevMenu == MENU_NONE)
            {
                MN_DeactivateMenu();
            }
            else
            {
                S_StartSound(NULL, sfx_switch);
                SetMenu(CurrentMenu->prevMenu);
            }
            return (true);
        }
        else if (key == key_menu_left)       // Slider left
        {
            if ((item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2 || item->type == ITT_SLDR) && item->func != NULL)
            {
                item->func(LEFT_DIR);
                S_StartSound(NULL, sfx_keyup);
            }
            // [JN] Go to previous-left menu by pressing Left Arrow.
            if (CurrentMenu->ScrollAR || CurrentItPos == -1)
            {
                M_ScrollPages(false);
            }
            return (true);
        }
        else if (key == key_menu_right)      // Slider right
        {
            if ((item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2 || item->type == ITT_SLDR) && item->func != NULL)
            {
                item->func(RIGHT_DIR);
                S_StartSound(NULL, sfx_keyup);
            }
            // [JN] Go to next-right menu by pressing Right Arrow.
            if (CurrentMenu->ScrollAR || CurrentItPos == -1)
            {
                M_ScrollPages(true);
            }
            return (true);
        }
        // [JN] Go to previous-left menu by pressing Page Up key.
        else if (key == KEY_PGUP)
        {
            if (CurrentMenu->ScrollPG)
            {
                M_ScrollPages(false);
            }
            return (true);
        }
        // [JN] Go to next-right menu by pressing Page Down key.
        else if (key == KEY_PGDN)
        {
            if (CurrentMenu->ScrollPG)
            {
                M_ScrollPages(true);
            }
            return (true);
        }
        else if (key == key_menu_forward && CurrentItPos != -1)    // Activate item (enter)
        {
            if (item->type == ITT_SETMENU)
            {
                if (item->func != NULL)
                {
                    item->func(item->option);
                }
                SetMenu(item->menu);
            }
            else if (item->func != NULL)
            {
                CurrentMenu->oldItPos = CurrentItPos;
                if (item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2)
                {
                    item->func(RIGHT_DIR);
                }
                else if (item->type == ITT_EFUNC)
                {
                    item->func(item->option);
                }
            }
            S_StartSound(NULL, sfx_dorcls);
            return (true);
        }
        // [crispy] delete a savegame
        else if (key == key_menu_del)
        {
            if (CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
            {
                if (SlotStatus[CurrentItPos])
                {
                    MenuActive = false;
                    askforquit = true;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    typeofask = 5;
                    S_StartSound(NULL, sfx_chat);
                }
            }
            // [JN] ...or clear key bind.
            else
            if (CurrentMenu == &ID_Def_Keybinds_1 || CurrentMenu == &ID_Def_Keybinds_2
            ||  CurrentMenu == &ID_Def_Keybinds_3 || CurrentMenu == &ID_Def_Keybinds_4
            ||  CurrentMenu == &ID_Def_Keybinds_5 || CurrentMenu == &ID_Def_Keybinds_6
            ||  CurrentMenu == &ID_Def_Keybinds_7 || CurrentMenu == &ID_Def_Keybinds_8)
            {
                M_ClearBind(CurrentItPos);
            }
            // [JN] ...or clear mouse bind.
            else if (CurrentMenu == &ID_Def_MouseBinds)
            {
                M_ClearMouseBind(CurrentItPos);
            }
            return (true);
        }
        // Jump to menu item based on first letter:
        // [JN] Allow multiple jumps over menu items with
        // same first letters. This behavior is same to Doom.
        // [PN] Combined loops using a cyclic index to traverse
        // the array twice, avoiding code duplication.
        else if (charTyped != 0)
        {
            for (i = CurrentItPos + 1; i < CurrentMenu->itemCount + CurrentItPos + 1; i++)
            {
                const int index = i % CurrentMenu->itemCount;

                if (CurrentMenu->items[index].text)
                {
                    if (toupper(charTyped) == toupper(DEH_String(CurrentMenu->items[index].text)[0]))
                    {
                        CurrentItPos = index;
                        S_StartSound(NULL, sfx_switch);
                        return (true);
                    }
                }
            }
        }

        return (false);
    }
    else
    {
        // Editing file names
        // When typing a savegame name, we use the fully shifted and
        // translated input value from event->data3.
        charTyped = event->data3;

        textBuffer = &SlotText[currentSlot][slotptr];
        if (key == KEY_BACKSPACE)
        {
            if (slotptr)
            {
                *textBuffer-- = 0;
                *textBuffer = ASCII_CURSOR;
                slotptr--;
            }
            return (true);
        }
        if (key == KEY_ESCAPE)
        {
            memset(SlotText[currentSlot], 0, SLOTTEXTLEN + 2);
            M_StringCopy(SlotText[currentSlot], oldSlotText,
                         sizeof(SlotText[currentSlot]));
            SlotStatus[currentSlot]--;
            MN_DeactivateMenu();
            return (true);
        }
        if (key == KEY_ENTER)
        {
            SlotText[currentSlot][slotptr] = 0; // clear the cursor
            item = &CurrentMenu->items[CurrentItPos];
            CurrentMenu->oldItPos = CurrentItPos;
            if (item->type == ITT_EFUNC)
            {
                item->func(item->option);
                if (item->menu != MENU_NONE)
                {
                    SetMenu(item->menu);
                }
            }
            return (true);
        }
        if (slotptr < SLOTTEXTLEN && key != KEY_BACKSPACE)
        {
            if (isalpha(charTyped))
            {
                *textBuffer++ = toupper(charTyped);
                *textBuffer = ASCII_CURSOR;
                slotptr++;
                return (true);
            }
            if (isdigit(charTyped) || charTyped == ' '
              || charTyped == ',' || charTyped == '.' || charTyped == '-'
              || charTyped == '!')
            {
                *textBuffer++ = charTyped;
                *textBuffer = ASCII_CURSOR;
                slotptr++;
                return (true);
            }
        }
        return (true);
    }
    return (false);
}

//---------------------------------------------------------------------------
//
// PROC MN_ActivateMenu
//
//---------------------------------------------------------------------------

void MN_ActivateMenu(void)
{
    if (MenuActive)
    {
        return;
    }
    if (paused)
    {
        S_ResumeSound();
    }
    MenuActive = true;
    FileMenuKeySteal = false;
    MenuTime = 0;
    CurrentMenu = &MainMenu;
    M_Reset_Line_Glow();
    CurrentItPos = CurrentMenu->oldItPos;
    // [JN] Update mouse cursor position.
    M_ID_MenuMouseControl();
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    S_StartSound(NULL, sfx_dorcls);
    slottextloaded = false;     //reload the slot text, when needed
    // [JN] Disallow menu items highlighting initially to prevent
    // cursor jumping. It will be allowed by mouse movement.
    menu_mouse_allow = false;
}

//---------------------------------------------------------------------------
//
// PROC MN_DeactivateMenu
//
//---------------------------------------------------------------------------

void MN_DeactivateMenu(void)
{
    if (CurrentMenu != NULL)
    {
        M_Reset_Line_Glow();
        CurrentMenu->oldItPos = CurrentItPos;
    }
    MenuActive = false;
    if (FileMenuKeySteal)
    {
        I_StopTextInput();
    }
    if (!netgame)
    {
        paused = false;
    }
    // [JN] Do not play closing menu sound on quick save/loading actions.
    // Quick save playing it separatelly, quick load doesn't need it at all.
    if (!quicksave && !quickload)
    {
        S_StartSound(NULL, sfx_dorcls);
    }
    // [JN] Hide cursor on closing menu.
    menu_mouse_allow = false;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawInfo
//
//---------------------------------------------------------------------------

void MN_DrawInfo(void)
{
    lumpindex_t lumpindex; // [crispy]

    I_SetPalette(0);

    // [crispy] Refactor to allow for use of V_DrawFullscreenRawOrPatch

    switch (InfoType)
    {
        case 1:
            lumpindex = W_GetNumForName("HELP1");
            break;

        case 2:
            lumpindex = W_GetNumForName("HELP2");
            break;

        case 3:
            lumpindex = W_GetNumForName("CREDIT");
            break;

        default:
            lumpindex = W_GetNumForName("TITLE");
            break;
    }

    V_DrawFullscreenRawOrPatch(lumpindex);

    // [JN] Always refresh statbar while drawing full screen graphics.
    SB_ForceRedraw();

//      V_DrawPatch(0, 0, W_CacheLumpNum(W_GetNumForName("TITLE")+InfoType,
//              PU_CACHE));
}


//---------------------------------------------------------------------------
//
// PROC SetMenu
//
//---------------------------------------------------------------------------

static void SetMenu(MenuType_t menu)
{
    CurrentMenu->oldItPos = CurrentItPos;
    CurrentMenu = Menus[menu];
    M_Reset_Line_Glow();
    CurrentItPos = CurrentMenu->oldItPos;
    // [JN] Update mouse cursor position.
    M_ID_MenuMouseControl();
}

//---------------------------------------------------------------------------
//
// PROC DrawSlider
//
//---------------------------------------------------------------------------

static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing, int itemPos)
{
    int x;
    int y;
    int x2;
    int count;

    x = menu->x + 24;
    y = menu->y + 2 + (item * (bigspacing ? ITEM_HEIGHT : ID_MENU_LINEHEIGHT_SMALL));

    // [JN] Highlight active slider and gem.
    dp_translation = (itemPos == CurrentItPos && menu_highlight) ? cr[CR_MENU_BRIGHT2] : NULL;

    V_DrawShadowedPatchOptional(x - 32, y, 1, W_CacheLumpName(DEH_String("M_SLDLT"), PU_CACHE));
    for (x2 = x, count = width; count--; x2 += 8)
    {
        V_DrawShadowedPatchOptional(x2, y, 1, W_CacheLumpName(DEH_String(count & 1 ? "M_SLDMD1" : "M_SLDMD2"), PU_CACHE));
    }
    V_DrawShadowedPatchOptional(x2, y, 1, W_CacheLumpName(DEH_String("M_SLDRT"), PU_CACHE));

    // [JN] Prevent gem go out of slider bounds.
    if (slot > width - 1)
    {
        slot = width - 1;
    }

    V_DrawPatch(x + 4 + slot * 8, y + 7,
                W_CacheLumpName(DEH_String("M_SLDKB"), PU_CACHE));

    dp_translation = NULL;
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
} key_names[] = KEY_NAMES_ARRAY_RAVEN;

static char *M_NameBind (int CurrentItPosOn, int key)
{
    if (CurrentItPos == CurrentItPosOn && KbdIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        for (int i = 0; i < arrlen(key_names); ++i)
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
    if (key_strafe == key)           key_strafe           = 0;
    if (key_speed == key)            key_speed            = 0;
    if (key_180turn == key)          key_180turn          = 0;
    if (key_fire == key)             key_fire             = 0;
    if (key_use == key)              key_use              = 0;

    // Page 2
    if (key_lookup == key)           key_lookup           = 0;
    if (key_lookdown == key)         key_lookdown         = 0;
    if (key_lookcenter == key)       key_lookcenter       = 0;
    if (key_flyup == key)            key_flyup            = 0;
    if (key_flydown == key)          key_flydown          = 0;
    if (key_flycenter == key)        key_flycenter        = 0;
    if (key_invleft == key)          key_invleft          = 0;
    if (key_invright == key)         key_invright         = 0;
    if (key_useartifact == key)      key_useartifact      = 0;

    // Page 3
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

    // Page 4
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

    // Page 5
    if (key_arti_quartz == key)      key_arti_quartz      = 0;
    if (key_arti_urn == key)         key_arti_urn         = 0;
    if (key_arti_bomb == key)        key_arti_bomb        = 0;
    if (key_arti_tome == key)        key_arti_tome        = 0;
    if (key_arti_ring == key)        key_arti_ring        = 0;
    if (key_arti_chaosdevice == key) key_arti_chaosdevice = 0;
    if (key_arti_shadowsphere == key) key_arti_shadowsphere = 0;
    if (key_arti_wings == key)       key_arti_wings       = 0;
    if (key_arti_torch == key)       key_arti_torch       = 0;
    if (key_arti_morph == key)       key_arti_morph       = 0;

    // Page 6
    if (key_map_toggle == key)       key_map_toggle       = 0;
    // Do not override Automap binds in other pages.
    if (CurrentMenu == &ID_Def_Keybinds_6)
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

    // Page 7
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

    // Page 8
    if (key_pause == key)              key_pause              = 0;
    if (key_menu_screenshot == key)    key_menu_screenshot    = 0;
    if (key_message_refresh_hr == key) key_message_refresh_hr = 0;
    if (key_demo_quit == key)          key_demo_quit          = 0;
    if (key_multi_msg == key)          key_multi_msg          = 0;
    // Do not override Send To binds in other pages.
    if (CurrentMenu == &ID_Def_Keybinds_8)
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
        case 106:  key_strafe = key;            break;
        case 107:  key_speed = key;             break;
        case 108:  key_180turn = key;           break;
        case 109:  key_fire = key;              break;
        case 110:  key_use = key;               break;

        // Page 2
        case 200:  key_lookup = key;            break;
        case 201:  key_lookdown = key;          break;
        case 202:  key_lookcenter = key;        break;
        case 203:  key_flyup = key;             break;
        case 204:  key_flydown = key;           break;
        case 205:  key_flycenter = key;         break;
        case 206:  key_invleft = key;           break;
        case 207:  key_invright = key;          break;
        case 208:  key_useartifact = key;       break;

        // Page 3
        case 300:  key_autorun = key;           break;
        case 301:  key_mouse_look = key;        break;
        case 302:  key_novert = key;            break;
        case 303:  key_reloadlevel = key;       break;
        case 304:  key_nextlevel = key;         break;
        case 305:  key_demospeed = key;         break;
        case 306:  key_flip_levels = key;       break;
        case 307:  key_widget_enable = key;     break;
        case 308:  key_spectator = key;         break;
        case 309:  key_freeze = key;            break;
        case 310:  key_notarget = key;          break;
        case 311:  key_buddha = key;            break;

        // Page 4
        case 400:  key_weapon1 = key;           break;
        case 401:  key_weapon2 = key;           break;
        case 402:  key_weapon3 = key;           break;
        case 403:  key_weapon4 = key;           break;
        case 404:  key_weapon5 = key;           break;
        case 405:  key_weapon6 = key;           break;
        case 406:  key_weapon7 = key;           break;
        case 407:  key_weapon8 = key;           break;
        case 408:  key_prevweapon = key;        break;
        case 409:  key_nextweapon = key;        break;

        // Page 5
        case 500:  key_arti_quartz = key;       break;
        case 501:  key_arti_urn = key;          break;
        case 502:  key_arti_bomb = key;         break;
        case 503:  key_arti_tome = key;         break;
        case 504:  key_arti_ring = key;         break;
        case 505:  key_arti_chaosdevice = key;  break;
        case 506:  key_arti_shadowsphere = key; break;
        case 507:  key_arti_wings = key;        break;
        case 508:  key_arti_torch = key;        break;
        case 509:  key_arti_morph = key;        break;

        // Page 6
        if (CurrentMenu == &ID_Def_Keybinds_6)
        {
        case 600:  key_map_toggle = key;        break;
        case 601:  key_map_zoomin = key;        break;
        case 602:  key_map_zoomout = key;       break;
        case 603:  key_map_maxzoom = key;       break;
        case 604:  key_map_follow = key;        break;
        case 605:  key_map_rotate = key;        break;
        case 606:  key_map_overlay = key;       break;
        case 607:  key_map_grid = key;          break;
        case 608:  key_map_mark = key;          break;
        case 609:  key_map_clearmark = key;     break;
        }

        // Page 7
        case 700:  key_menu_help = key;         break;
        case 701:  key_menu_save = key;         break;
        case 702:  key_menu_load = key;         break;
        case 703:  key_menu_volume = key;       break;
        case 704:  key_menu_detail = key;       break;
        case 705:  key_menu_qsave = key;        break;
        case 706:  key_menu_endgame = key;      break;
        case 707:  key_menu_messages = key;     break;
        case 708:  key_menu_qload = key;        break;
        case 709:  key_menu_quit = key;         break;
        case 710:  key_menu_gamma = key;        break;
        case 711:  key_spy = key;               break;

        // Page 8
        case 800:  key_pause = key;              break;
        case 801:  key_menu_screenshot = key;    break;
        case 802:  key_message_refresh_hr = key; break;
        case 803:  key_demo_quit = key;          break;
        case 804:  key_multi_msg = key;          break;
        if (CurrentMenu == &ID_Def_Keybinds_8)
        {
        case 805:  key_multi_msgplayer[0] = key; break;
        case 806:  key_multi_msgplayer[1] = key; break;
        case 807:  key_multi_msgplayer[2] = key; break;
        case 808:  key_multi_msgplayer[3] = key; break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ClearBind
//  [JN] Clear key bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------

static void M_ClearBind (int CurrentItPos)
{
    if (CurrentMenu == &ID_Def_Keybinds_1)
    {
        switch (CurrentItPos)
        {
            case 0:   key_up = 0;               break;
            case 1:   key_down = 0;             break;
            case 2:   key_left = 0;             break;
            case 3:   key_right = 0;            break;
            case 4:   key_strafeleft = 0;       break;
            case 5:   key_straferight = 0;      break;
            case 6:   key_strafe = 0;           break;
            case 7:   key_speed = 0;            break;
            case 8:   key_180turn = 0;          break;
            // Action title
            case 10:  key_fire = 0;             break;
            case 11:  key_use = 0;              break;
        }
    }
    if (CurrentMenu == &ID_Def_Keybinds_2)
    {
        switch (CurrentItPos)
        {
            case 0:   key_lookup = 0;           break;
            case 1:   key_lookdown = 0;         break;
            case 2:   key_lookcenter = 0;       break;
            // Flying title
            case 4:   key_flyup = 0;            break;
            case 5:   key_flydown = 0;          break;
            case 6:   key_flycenter = 0;        break;
            // Inventory title
            case 8:   key_invleft = 0;          break;
            case 9:   key_invright = 0;         break;
            case 10:  key_useartifact = 0;      break;
        }
    }
    if (CurrentMenu == &ID_Def_Keybinds_3)
    {
        switch (CurrentItPos)
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
    if (CurrentMenu == &ID_Def_Keybinds_4)
    {
        switch (CurrentItPos)
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
    if (CurrentMenu == &ID_Def_Keybinds_5)
    {
        switch (CurrentItPos)
        {
            case 0:   key_arti_quartz = 0;      break;
            case 1:   key_arti_urn = 0;         break;
            case 2:   key_arti_bomb = 0;        break;
            case 3:   key_arti_tome = 0;        break;
            case 4:   key_arti_ring = 0;        break;
            case 5:   key_arti_chaosdevice = 0; break;
            case 6:   key_arti_shadowsphere = 0; break;
            case 7:   key_arti_wings = 0;       break;
            case 8:   key_arti_torch = 0;       break;
            case 9:   key_arti_morph = 0;       break;
        }
    }
    if (CurrentMenu == &ID_Def_Keybinds_6)
    {
        switch (CurrentItPos)
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
    if (CurrentMenu == &ID_Def_Keybinds_7)
    {
        switch (CurrentItPos)
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
    if (CurrentMenu == &ID_Def_Keybinds_8)
    {
        switch (CurrentItPos)
        {
            case 0:   key_pause = 0;              break;
            case 1:   key_menu_screenshot = 0;    break;
            case 2:   key_message_refresh_hr = 0; break;
            case 3:   key_demo_quit = 0;          break;
            // Multiplayer title
            case 5:   key_multi_msg = 0;          break;
            case 6:   key_multi_msgplayer[0] = 0; break;
            case 7:   key_multi_msgplayer[1] = 0; break;
            case 8:   key_multi_msgplayer[2] = 0; break;
            case 9:   key_multi_msgplayer[3] = 0; break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all keyboard bindings to default.
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
    key_strafe = KEY_RALT;
    key_speed = KEY_RSHIFT;
    key_180turn = 0;
    key_fire = KEY_RCTRL;
    key_use = ' ';

    // Page 2
    key_lookup = KEY_PGDN;
    key_lookdown = KEY_DEL;
    key_lookcenter = KEY_END;
    key_flyup = KEY_PGUP;
    key_flydown = KEY_INS;
    key_flycenter = KEY_HOME;
    key_invleft = '[';
    key_invright = ']';
    key_useartifact = KEY_ENTER;

    // Page 3
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

    // Page 4
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

    // Page 5
    key_arti_quartz = 0;
    key_arti_urn = 0;
    key_arti_bomb = 0;
    key_arti_tome = 127; // backspace
    key_arti_ring = 0;
    key_arti_chaosdevice = 0;
    key_arti_shadowsphere = 0;
    key_arti_wings = 0;
    key_arti_torch = 0;
    key_arti_morph = 0;

    // Page 6
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

    // Page 7
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

    // Page 8
    key_pause = KEY_PAUSE;
    key_menu_screenshot = KEY_PRTSCR;
    key_message_refresh_hr = 0;
    key_demo_quit = 'q';
    key_multi_msg = 't';
    key_multi_msgplayer[0] = 'g';
    key_multi_msgplayer[1] = 'i';
    key_multi_msgplayer[2] = 'b';
    key_multi_msgplayer[3] = 'r';
}

// -----------------------------------------------------------------------------
// M_DrawBindKey
//  [JN] Do keyboard bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindKey (int itemNum, int yPos, int keyBind)
{
    MN_DrTextAGlow(M_NameBind(itemNum, keyBind), M_ItemRightAlign(M_NameBind(itemNum, keyBind)), yPos,
                        CurrentItPos == itemNum && KbdIsBinding ? cr[CR_YELLOW] :
                        keyBind == 0 ? cr[CR_RED] : cr[CR_GREEN],
                            CurrentItPos == itemNum && KbdIsBinding ? cr[CR_YELLOW_BRIGHT] :
                            keyBind == 0 ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(itemNum));
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
        MN_DrTextACentered(string, 170, cr[CR_GRAY]);
        MN_DrTextA("PGUP", ID_MENU_LEFTOFFSET, 180, cr[CR_GRAY]);
        MN_DrTextACentered(M_StringJoin("PAGE ", pagenum, "/8", NULL), 180, cr[CR_GRAY]);
        MN_DrTextA("PGDN", M_ItemRightAlign("PGDN"), 180, cr[CR_GRAY]);
    }
    else
    {
        MN_DrTextACentered(string, 180, cr[CR_GRAY]);
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

static char *M_NameMouseBind (int CurrentItPosOn, int btn)
{
    if (CurrentItPos == CurrentItPosOn && MouseIsBinding)
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
    if (mousebinvleft == btn)     mousebinvleft     = -1;
    if (mousebinvright == btn)    mousebinvright    = -1;
    if (mousebuseartifact == btn) mousebuseartifact = -1;
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
        case 1010:  mousebinvleft = btn;      break;
        case 1011:  mousebinvright = btn;     break;
        case 1012:  mousebuseartifact = btn;  break;
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
        case 0:   mousebfire = -1;         break;
        case 1:   mousebforward = -1;      break;
        case 2:   mousebbackward = -1;     break;
        case 3:   mousebuse = -1;          break;
        case 4:   mousebspeed = -1;        break;
        case 5:   mousebstrafe = -1;       break;
        case 6:   mousebstrafeleft = -1;   break;
        case 7:   mousebstraferight = -1;  break;
        case 8:   mousebprevweapon = -1;   break;
        case 9:   mousebnextweapon = -1;   break;
        case 10:  mousebinvleft = -1;      break;
        case 11:  mousebinvright = -1;     break;
        case 12:  mousebuseartifact = -1;  break;
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindButton
//  [JN] Do mouse button bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindButton (int itemNum, int yPos, int btnBind)
{
    MN_DrTextAGlow(M_NameMouseBind(itemNum, btnBind), M_ItemRightAlign(M_NameMouseBind(itemNum, btnBind)), yPos,
                        CurrentItPos == itemNum && MouseIsBinding ? cr[CR_YELLOW] :
                        btnBind == - 1 ? cr[CR_RED] : cr[CR_GREEN],
                            CurrentItPos == itemNum && MouseIsBinding ? cr[CR_YELLOW_BRIGHT] :
                            btnBind == - 1 ? cr[CR_RED_BRIGHT] : cr[CR_GREEN_BRIGHT],
                                LINE_ALPHA(itemNum));
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all mouse binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    mousebfire = 0;
    mousebforward = 2;
    mousebspeed = -1;
    mousebstrafe = 1;
    mousebbackward = -1;
    mousebuse = -1;
    mousebstrafeleft = -1;
    mousebstraferight = -1;
    mousebprevweapon = 4;
    mousebnextweapon = 3;
    mousebinvleft = -1;
    mousebinvright = -1;
    mousebuseartifact = -1;
}
