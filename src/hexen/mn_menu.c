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


// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include "h2def.h"
#include "doomkeys.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "am_map.h"
#include "ct_chat.h"

#include "id_vars.h"


// MACROS ------------------------------------------------------------------

#define LEFT_DIR 0
#define RIGHT_DIR 1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN	22
#define ASCII_CURSOR '['

// TYPES -------------------------------------------------------------------

typedef enum
{
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC,
    ITT_SETMENU,
    ITT_INERT
} ItemType_t;

typedef enum
{
    MENU_MAIN,
    MENU_CLASS,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_FILES,
    MENU_LOAD,
    MENU_SAVE,
    MENU_ID_MAIN,
    MENU_ID_VIDEO,
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
    MENU_ID_GAMEPLAY1,
    MENU_ID_GAMEPLAY2,
    MENU_NONE
} MenuType_t;

typedef struct
{
    ItemType_t type;
    const char *text;
    void (*func) (int option);
    int option;
    MenuType_t menu;
    short tics;  // [JN] Menu item timer for glowing effect.
} MenuItem_t;

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

typedef struct
{
    int x;
    int y;
    void (*drawFunc) (void);
    int itemCount;
    MenuItem_t *items;
    int oldItPos;
    boolean smallFont;  // [JN] Menu is using small font
    boolean ScrollAR;   // [JN] Menu can be scrolled by arrow keys
    boolean ScrollPG;   // [JN] Menu can be scrolled by PGUP/PGDN keys
    MenuType_t prevMenu;
} Menu_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void InitFonts(void);
static void SetMenu(MenuType_t menu);
static void SCQuitGame(int option);
static void SCClass(int option);
static void SCSkill(int option);
static void SCMouseSensi(int option);
static void SCSfxVolume(int option);
static void SCMusicVolume(int option);
static void SCScreenSize(int option);
static boolean SCNetCheck(int option);
static void SCNetCheck2(int option);
static void SCLoadGame(int option);
static void SCSaveGame(int option);
static void SCMessages(int option);
static void SCEndGame(int option);
static void SCInfo(int option);
static void DrawMainMenu(void);
static void DrawClassMenu(void);
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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------


// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean MenuActive;
int InfoType;
boolean mn_SuicideConsole;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int FontABaseLump;
static int FontAYellowBaseLump;
static int FontBBaseLump;
static int MauloBaseLump;
static Menu_t *CurrentMenu;
static int CurrentItPos;
static int MenuPClass;
static int MenuTime;

boolean askforquit;
static int typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static boolean joypadsave;
static char SlotText[6][SLOTTEXTLEN + 2];
static char oldSlotText[SLOTTEXTLEN + 2];
static int SlotStatus[6];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

// [JN] Show custom titles while performing quick save/load.
static boolean quicksaveTitle = false;
static boolean quickloadTitle = false;

static char *gammalvls[16][32] =
{
    { GAMMALVL05,   "0.50" },
    { GAMMALVL055,  "0.55" },
    { GAMMALVL06,   "0.60" },
    { GAMMALVL065,  "0.65" },
    { GAMMALVL07,   "0.70" },
    { GAMMALVL075,  "0.75" },
    { GAMMALVL08,   "0.80" },
    { GAMMALVL085,  "0.85" },
    { GAMMALVL09,   "0.90" },
    { GAMMALVL095,  "0.95" },
    { GAMMALVL0,    "OFF"  },
    { GAMMALVL1,    "1"    },
    { GAMMALVL2,    "2"    },
    { GAMMALVL3,    "3"    },
    { GAMMALVL4,    "4"    },
    { NULL,         NULL   },
};

static MenuItem_t MainItems[] = {
    {ITT_SETMENU, "NEW GAME", SCNetCheck2, 1, MENU_CLASS},
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
    false, false, false,
    MENU_NONE
};

static MenuItem_t ClassItems[] = {
    {ITT_EFUNC, "FIGHTER", SCClass, 0, MENU_NONE},
    {ITT_EFUNC, "CLERIC", SCClass, 1, MENU_NONE},
    {ITT_EFUNC, "MAGE", SCClass, 2, MENU_NONE},
    {ITT_EFUNC, "RANDOM", SCClass, 4, MENU_NONE},
};

static Menu_t ClassMenu = {
    66, 60,
    DrawClassMenu,
    4, ClassItems,
    0,
    false, false, false,
    MENU_MAIN
};

static MenuItem_t FilesItems[] = {
    {ITT_SETMENU, "LOAD GAME", SCNetCheck2, 2, MENU_LOAD},
    {ITT_SETMENU, "SAVE GAME", NULL, 0, MENU_SAVE}
};

static Menu_t FilesMenu = {
    110, 60,
    DrawFilesMenu,
    2, FilesItems,
    0,
    false, false, false,
    MENU_MAIN
};

static MenuItem_t LoadItems[] = {
    {ITT_EFUNC, NULL, SCLoadGame, 0, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 1, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 2, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 3, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 4, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 5, MENU_NONE}
};

static Menu_t LoadMenu = {
    70, 26,
    DrawLoadMenu,
    SAVES_PER_PAGE, LoadItems,
    0,
    false, true, true,
    MENU_FILES
};

static MenuItem_t SaveItems[] = {
    {ITT_EFUNC, NULL, SCSaveGame, 0, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 1, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 2, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 3, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 4, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 5, MENU_NONE}
};

static Menu_t SaveMenu = {
    70, 26,
    DrawSaveMenu,
    SAVES_PER_PAGE, SaveItems,
    0,
    false, true, true,
    MENU_FILES
};

static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, NULL, SCSkill, sk_baby, MENU_NONE},
    {ITT_EFUNC, NULL, SCSkill, sk_easy, MENU_NONE},
    {ITT_EFUNC, NULL, SCSkill, sk_medium, MENU_NONE},
    {ITT_EFUNC, NULL, SCSkill, sk_hard, MENU_NONE},
    {ITT_EFUNC, NULL, SCSkill, sk_nightmare, MENU_NONE}
};

static Menu_t SkillMenu = {
    120, 44,
    DrawSkillMenu,
    5, SkillItems,
    2,
    false, false, false,
    MENU_CLASS
};

static MenuItem_t OptionsItems[] = {
    {ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE},
    {ITT_EFUNC, "MESSAGES : ", SCMessages, 0, MENU_NONE},
    {ITT_LRFUNC, "MOUSE SENSITIVITY", SCMouseSensi, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_SETMENU, "MORE...", NULL, 0, MENU_OPTIONS2}
};

static Menu_t OptionsMenu = {
    88, 30,
    DrawOptionsMenu,
    5, OptionsItems,
    0,
    false, false, false,
    MENU_MAIN
};

static MenuItem_t Options2Items[] = {
    { ITT_LRFUNC, "SFX VOLUME",   SCSfxVolume,   0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
    { ITT_LRFUNC, "MUSIC VOLUME", SCMusicVolume, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
    { ITT_LRFUNC, "SCREEN SIZE",  SCScreenSize,  0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
};

static Menu_t Options2Menu = {
    72, 20,
    DrawOptions2Menu,
    6, Options2Items,
    0,
    false, false, false,
    MENU_ID_MAIN
};

// =============================================================================
// [JN] ID custom menu
// =============================================================================

#define ID_MENU_TOPOFFSET         (20)
#define ID_MENU_LEFTOFFSET        (48)
#define ID_MENU_LEFTOFFSET_SML    (90)
#define ID_MENU_LEFTOFFSET_BIG    (38)

#define ID_MENU_LINEHEIGHT_SMALL  (10)
#define ID_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return ORIGWIDTH - CurrentMenu->x - MN_TextAWidth(text);
}

static void M_Draw_ID_Main (void);

static void M_Draw_ID_Video (void);
static void M_ID_TrueColor (int choice);
static void M_ID_RenderingRes (int choice);
static void M_ID_Widescreen (int choice);
static void M_ID_UncappedFPS (int choice);
static void M_ID_LimitFPS (int choice);
static void M_ID_VSync (int choice);
static void M_ID_ShowFPS (int choice);
static void M_ID_PixelScaling (int choice);
static void M_ID_GfxStartup (int choice);

static void M_Draw_ID_Display (void);
static void M_ID_Gamma (int choice);
static void M_ID_FOV (int choice);
static void M_ID_MenuShading (int choice);
static void M_ID_LevelBrightness (int choice);
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
static void M_Bind_Jump (int option);
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
static void M_Bind_RestartLevel (int option);
static void M_Bind_NextLevel (int option);
static void M_Bind_FastForward (int option);
static void M_Bind_FlipLevels (int option);
static void M_Bind_SpectatorMode (int option);
static void M_Bind_FreezeMode (int option);
static void M_Bind_NotargetMode (int option);
static void M_Bind_BuddhaMode (int option);

static void M_Draw_ID_Keybinds_4 (void);
static void M_Bind_Weapon1 (int option);
static void M_Bind_Weapon2 (int option);
static void M_Bind_Weapon3 (int option);
static void M_Bind_Weapon4 (int option);
static void M_Bind_Quartz (int option);
static void M_Bind_Urn (int option);
static void M_Bind_Flechette (int option);
static void M_Bind_Disk (int option);
static void M_Bind_Icon (int option);
static void M_Bind_PrevWeapon (int option);
static void M_Bind_NextWeapon (int option);

static void M_Draw_ID_Keybinds_5 (void);
static void M_Bind_Porkalator (int option);
static void M_Bind_Chaos (int option);
static void M_Bind_Banishment (int option);
static void M_Bind_Wings (int option);
static void M_Bind_Servant (int option);
static void M_Bind_Bracers (int option);
static void M_Bind_Boots (int option);
static void M_Bind_Torch (int option);
static void M_Bind_Krater (int option);
static void M_Bind_Incant (int option);
static void M_Bind_AllArti (int option);


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
static void M_Bind_Suicide (int option);
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
static void M_Bind_Reset (int option);
/*
static void M_Bind_ToPlayer1 (int option);
static void M_Bind_ToPlayer2 (int option);
static void M_Bind_ToPlayer3 (int option);
static void M_Bind_ToPlayer4 (int option);
*/

static void M_Draw_ID_MouseBinds (void);
static void M_Bind_M_FireAttack (int option);
static void M_Bind_M_MoveForward (int option);
static void M_Bind_M_StrafeOn (int option);
static void M_Bind_M_MoveBackward (int option);
static void M_Bind_M_Use (int option);
static void M_Bind_M_StrafeLeft (int option);
static void M_Bind_M_StrafeRight (int option);
static void M_Bind_M_PrevWeapon (int option);
static void M_Bind_M_NextWeapon (int option);
static void M_Bind_M_InventoryLeft (int option);
static void M_Bind_M_InventoryRight (int option);
static void M_Bind_M_UseArtifact (int option);

static void M_Bind_M_Reset (int option);

static void M_Draw_ID_Widgets (void);
static void M_ID_Widget_Location (int option);
static void M_ID_Widget_Kills (int option);
static void M_ID_Widget_LevelName (int option);
static void M_ID_Widget_Coords (int option);
static void M_ID_Widget_Render (int option);
static void M_ID_Widget_Health (int option);
static void M_ID_Automap_Rotate (int option);
static void M_ID_Automap_Overlay (int option);
static void M_ID_Automap_Shading (int option);

static void M_Draw_ID_Gameplay_1 (void);
static void M_ID_Brightmaps (int choice);
static void M_ID_Translucency (int choice);
static void M_ID_SmoothLighting (int choice);
static void M_ID_SwirlingLiquids (int choice);
static void M_ID_LinearSky (int choice);
static void M_ID_FlipCorpses (int choice);
static void M_ID_Crosshair (int choice);
static void M_ID_CrosshairColor (int choice);
static void M_ID_ColoredSBar (int choice);
static void M_ID_WeaponWidget (int choice);

static void M_Draw_ID_Gameplay_2 (void);
static void M_ID_Torque (int choice);
static void M_ID_Breathing (int choice);
static void M_ID_DefaultClass (int choice);
static void M_ID_DefaultSkill (int choice);
static void M_ID_FlipLevels (int choice);
static void M_ID_DemoTimer (int choice);
static void M_ID_TimerDirection (int choice);
static void M_ID_ProgressBar (int choice);
static void M_ID_InternalDemos (int choice);

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
static byte   *M_ColorizeBind (int itemSetOn, int key);
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
static byte   *M_ColorizeMouseBind (int CurrentItPosOn, int btn);
static void    M_DrawBindButton (int itemNum, int yPos, int btnBind);
static void    M_ResetMouseBinds (void);

// Forward declarations for scrolling and remembering last pages.
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

// Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // Save/Load menu:
    if (CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
    {
        if (savepage > 0 && !direction)
        {
            savepage--;
            S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
        }
        else
        if (savepage < SAVEPAGE_MAX && direction)
        {
            savepage++;
            S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
        }
        quicksave = -1;
        MN_LoadSlotText();
        return;
    }

    // Keyboard bindings:
    else if (CurrentMenu == &ID_Def_Keybinds_1) SetMenu(direction ? MENU_ID_KBDBINDS2 : MENU_ID_KBDBINDS8);
    else if (CurrentMenu == &ID_Def_Keybinds_2) SetMenu(direction ? MENU_ID_KBDBINDS3 : MENU_ID_KBDBINDS1);
    else if (CurrentMenu == &ID_Def_Keybinds_3) SetMenu(direction ? MENU_ID_KBDBINDS4 : MENU_ID_KBDBINDS2);
    else if (CurrentMenu == &ID_Def_Keybinds_4) SetMenu(direction ? MENU_ID_KBDBINDS5 : MENU_ID_KBDBINDS3);
    else if (CurrentMenu == &ID_Def_Keybinds_5) SetMenu(direction ? MENU_ID_KBDBINDS6 : MENU_ID_KBDBINDS4);
    else if (CurrentMenu == &ID_Def_Keybinds_6) SetMenu(direction ? MENU_ID_KBDBINDS7 : MENU_ID_KBDBINDS5);
    else if (CurrentMenu == &ID_Def_Keybinds_7) SetMenu(direction ? MENU_ID_KBDBINDS8 : MENU_ID_KBDBINDS6);
    else if (CurrentMenu == &ID_Def_Keybinds_8) SetMenu(direction ? MENU_ID_KBDBINDS1 : MENU_ID_KBDBINDS7);

    // Gameplay features:
    else if (CurrentMenu == &ID_Def_Gameplay_1) SetMenu(MENU_ID_GAMEPLAY2);
    else if (CurrentMenu == &ID_Def_Gameplay_2) SetMenu(MENU_ID_GAMEPLAY1);

    // Play sound.
    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
}

// -----------------------------------------------------------------------------

// [JN] Delay before shading.
static int shade_wait;

// [JN] Shade background while in CRL menu.
static void M_ShadeBackground (void)
{
    if (dp_menu_shading)
    {
        for (int y = 0; y < SCREENWIDTH * SCREENHEIGHT; y++)
        {
#ifndef CRISPY_TRUECOLOR
            I_VideoBuffer[y] = colormaps[((dp_menu_shading + 3) * 2) * 256 + I_VideoBuffer[y]];
#else
            I_VideoBuffer[y] = I_BlendDark(I_VideoBuffer[y], I_ShadeFactor[dp_menu_shading]);
#endif
        }
    }
}

static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("F_032", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
}

static byte *M_Small_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT2] :
        tics == 4 ? cr[CR_MENU_BRIGHT1] :
        tics == 3 ? NULL :
        tics == 2 ? cr[CR_MENU_DARK1]   :
                    cr[CR_MENU_DARK2]   ;
}

static byte *M_Big_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT3] :
        tics >= 3 ? cr[CR_MENU_BRIGHT2] :
        tics >= 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_YELLOW     4
#define GLOW_ORANGE     5
#define GLOW_LIGHTGRAY  6
#define GLOW_DARKGRAY   7
#define GLOW_BLUE       8

#define ITEMONTICS      CurrentMenu->items[CurrentItPos].tics
#define ITEMSETONTICS   CurrentMenu->items[CurrentItPosOn].tics

static byte *M_Item_Glow (const int CurrentItPosOn, const int color)
{
    if (CurrentItPos == CurrentItPosOn)
    {
        return
            color == GLOW_RED ||
            color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]       :
            color == GLOW_GREEN     ? cr[CR_GREEN_BRIGHT5_HX]  :
            color == GLOW_YELLOW    ? cr[CR_YELLOW_BRIGHT5]    :
            color == GLOW_ORANGE    ? cr[CR_ORANGE_HR_BRIGHT5] :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY_BRIGHT5] :
            color == GLOW_DARKGRAY  ? cr[CR_MENU_DARK1]        :
            color == GLOW_BLUE      ? cr[CR_BLUE2_BRIGHT5]     :
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
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5_HX] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4_HX] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3_HX] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2_HX] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1_HX] : cr[CR_GREEN_HX];
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
                ITEMSETONTICS == 5 ? cr[CR_ORANGE_HR_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_ORANGE_HR_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_ORANGE_HR_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_ORANGE_HR_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_ORANGE_HR_BRIGHT1] : cr[CR_ORANGE_HR];
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
        if (color == GLOW_DARKGRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_MENU_DARK1] :
                ITEMSETONTICS == 4 ? cr[CR_MENU_DARK2] :
                ITEMSETONTICS == 3 ? cr[CR_MENU_DARK3] :
                ITEMSETONTICS == 2 ? cr[CR_MENU_DARK4] :
                ITEMSETONTICS == 1 ? cr[CR_MENU_DARK4] : cr[CR_MENU_DARK4];
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
    }
    return NULL;
}

static byte *M_Cursor_Glow (const int tics)
{
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

static const int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    switch (direction)
    {
        case 0:
        val--;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val++;
        if (val > max)
            val = capped ? max : min;
        break;
    }
    return val;
}

static const float M_FLOAT_Slider (float val, float min, float max, float step,
                                   int direction, boolean capped)
{
    char buf[9];

    switch (direction)
    {
        case 0:
        val -= step;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val += step;
        if (val > max)
            val = capped ? max : min;
        break;
    }

    // [JN] Do a float correction to always get x.xxx000 values:
    sprintf (buf, "%f", val);
    val = (float)atof(buf);
    return val;
}

static byte *DefSkillColor (const int skill)
{
    return
        skill == 0 ? cr[CR_OLIVE]     :
        skill == 1 ? cr[CR_DARKGREEN] :
        skill == 2 ? cr[CR_GREEN_HX]  :
        skill == 3 ? cr[CR_YELLOW]    :
        skill == 4 ? cr[CR_RED]       :
                     NULL;
}

static char *const DefClassName[3] = 
{
    "FIGHTER" ,
    "CLERIC"  ,
    "MAGE"    ,
};

static char *const DefSkillName[5] = 
{
    "EASIEST" ,
    "EASY"    ,
    "NORMAL"  ,
    "HARD"    ,
    "HARDEST"
};

// -----------------------------------------------------------------------------
// Main ID Menu
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Main[] = {
    { ITT_SETMENU, "VIDEO OPTIONS",       NULL,                 0, MENU_ID_VIDEO     },
    { ITT_SETMENU, "DISPLAY OPTIONS",     NULL,                 0, MENU_ID_DISPLAY   },
    { ITT_SETMENU, "SOUND OPTIONS",       NULL,                 0, MENU_ID_SOUND     },
    { ITT_SETMENU, "CONTROL SETTINGS",    NULL,                 0, MENU_ID_CONTROLS  },
    { ITT_SETMENU, "WIDGETS AND AUTOMAP", NULL,                 0, MENU_ID_WIDGETS   },
    { ITT_EFUNC,   "GAMEPLAY FEATURES",   M_Choose_ID_Gameplay, 0, MENU_NONE         },
    { ITT_EFUNC,   "END GAME",            SCEndGame,            0, MENU_NONE         },
    { ITT_EFUNC,   "RESET SETTINGS",      M_ID_SettingReset,    0, MENU_NONE         },
};

static Menu_t ID_Def_Main = {
    ID_MENU_LEFTOFFSET_SML, ID_MENU_TOPOFFSET,
    M_Draw_ID_Main,
    8, ID_Menu_Main,
    0,
    true, false, false,
    MENU_MAIN
};

static void M_Draw_ID_Main (void)
{
    MN_DrTextACentered("OPTIONS", 10, cr[CR_YELLOW]);
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Video[] = {
    { ITT_LRFUNC, "TRUECOLOR RENDERING",  M_ID_TrueColor,    0, MENU_NONE },
    { ITT_LRFUNC, "RENDERING RESOLUTION", M_ID_RenderingRes, 0, MENU_NONE },
    { ITT_LRFUNC, "WIDESCREEN MODE",      M_ID_Widescreen,   0, MENU_NONE },
    { ITT_LRFUNC, "UNCAPPED FRAMERATE",   M_ID_UncappedFPS,  0, MENU_NONE },
    { ITT_LRFUNC, "FRAMERATE LIMIT",      M_ID_LimitFPS,     0, MENU_NONE },
    { ITT_LRFUNC, "ENABLE VSYNC",         M_ID_VSync,        0, MENU_NONE },
    { ITT_LRFUNC, "SHOW FPS COUNTER",     M_ID_ShowFPS,      0, MENU_NONE },
    { ITT_LRFUNC, "PIXEL SCALING",        M_ID_PixelScaling, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,              0, MENU_NONE },
    { ITT_LRFUNC, "GRAPHICAL STARTUP",    M_ID_GfxStartup,   0, MENU_NONE },
};

static Menu_t ID_Def_Video = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Video,
    10, ID_Menu_Video,
    0,
    true, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Video (void)
{
    char str[32];

    MN_DrTextACentered("VIDEO OPTIONS", 10, cr[CR_YELLOW]);

    // Truecolor Rendering
#ifndef CRISPY_TRUECOLOR
    sprintf(str, "N/A");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, GLOW_DARKRED));
#else
    sprintf(str, vid_truecolor ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
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
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, vid_resolution == 1 ? GLOW_DARKRED :
                              vid_resolution == 2 ||
                              vid_resolution == 3 ? GLOW_GREEN :
                              vid_resolution == 4 ||
                              vid_resolution == 5 ? GLOW_YELLOW :
                                                    GLOW_ORANGE));

    // Widescreen mode
    sprintf(str, vid_widescreen == 1 ? "MATCH SCREEN" :
                 vid_widescreen == 2 ? "16:10" :
                 vid_widescreen == 3 ? "16:9" :
                 vid_widescreen == 4 ? "21:9" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, vid_widescreen ? GLOW_GREEN : GLOW_DARKRED));

    // Uncapped framerate
    sprintf(str, vid_uncapped_fps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, vid_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !vid_uncapped_fps ? "35" :
                 vid_fpslimit ? "%d" : "NONE", vid_fpslimit);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, !vid_uncapped_fps ? GLOW_DARKRED :
                               vid_fpslimit == 0 ? GLOW_RED :
                               vid_fpslimit >= 500 ? GLOW_YELLOW : GLOW_GREEN));

    // Enable vsync
    sprintf(str, vid_vsync ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, vid_vsync ? GLOW_GREEN : GLOW_DARKRED));

    // Show FPS counter
    sprintf(str, vid_showfps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, vid_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Pixel scaling
    sprintf(str, vid_smooth_scaling ? "SMOOTH" : "SHARP");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, vid_smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("MISCELLANEOUS", 100, cr[CR_YELLOW]);

    // Graphical startup
    sprintf(str, vid_graphical_startup == 1 ? "FAST" :
                 vid_graphical_startup == 2 ? "SLOW" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, vid_graphical_startup == 1 ? GLOW_GREEN :
                              vid_graphical_startup == 2 ? GLOW_YELLOW : GLOW_RED));

    // [JN] Print current resolution. Shamelessly taken from Nugget Doom!
    if (CurrentItPos == 1 || CurrentItPos == 2)
    {
        char  width[8];
        char  height[8];
        const char *resolution;

        M_snprintf(width, 8, "%d", (ORIGWIDTH + (WIDESCREENDELTA*2)) * vid_resolution);
        M_snprintf(height, 8, "%d", (vid_aspect_ratio_correct == 1 ? ORIGHEIGHT_4_3 : ORIGHEIGHT) * vid_resolution);
        resolution = M_StringJoin("CURRENT RESOLUTION: ", width, "X", height, NULL);

        MN_DrTextACentered(resolution, 130, cr[CR_LIGHTGRAY_DARK1]);
    }
}

#ifdef CRISPY_TRUECOLOR
static void M_ID_TrueColorHook (void)
{
    I_SetPalette (SB_palette);
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
}
#endif

static void M_ID_TrueColor (int option)
{
#ifdef CRISPY_TRUECOLOR
    vid_truecolor ^= 1;
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
    // [JN] re-calculate sky texture scaling
    R_InitSkyMap();
    // [crispy] re-calculate automap coordinates
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
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
    vid_widescreen = M_INT_Slider(vid_widescreen, 0, 4, choice, false);
    post_rendering_hook = M_ID_WidescreenHook;
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
    
    switch (choice)
    {
        case 0:
            if (vid_fpslimit)
                vid_fpslimit--;

            if (vid_fpslimit < TICRATE)
                vid_fpslimit = 0;

            break;
        case 1:
            if (vid_fpslimit < 500)
                vid_fpslimit++;

            if (vid_fpslimit < TICRATE)
                vid_fpslimit = TICRATE;

        default:
            break;
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

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Display[] = {
    { ITT_LRFUNC, "GAMMA-CORRECTION",        M_ID_Gamma,           0, MENU_NONE },
    { ITT_LRFUNC, "FIELD OF VIEW",           M_ID_FOV,             0, MENU_NONE },
    { ITT_LRFUNC, "MENU BACKGROUND SHADING", M_ID_MenuShading,     0, MENU_NONE },
    { ITT_LRFUNC, "EXTRA LEVEL BRIGHTNESS",  M_ID_LevelBrightness, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                 0, MENU_NONE },
    { ITT_LRFUNC, "SATURATION",              M_ID_Saturation,      0, MENU_NONE },
    { ITT_LRFUNC, "RED INTENSITY",           M_ID_R_Intensity,     0, MENU_NONE },
    { ITT_LRFUNC, "GREEN INTENSITY",         M_ID_G_Intensity,     0, MENU_NONE },
    { ITT_LRFUNC, "BLUE INTENSITY",          M_ID_B_Intensity,     0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                 0, MENU_NONE },
    { ITT_LRFUNC, "MESSAGES ENABLED",        M_ID_Messages,        0, MENU_NONE },
    { ITT_LRFUNC, "TEXT CASTS SHADOWS",      M_ID_TextShadows,     0, MENU_NONE },
    { ITT_LRFUNC, "LOCAL TIME",              M_ID_LocalTime,       0, MENU_NONE },
};

static Menu_t ID_Def_Display = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Display,
    13, ID_Menu_Display,
    0,
    true, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Display (void)
{
    char str[32];

    MN_DrTextACentered("DISPLAY OPTIONS", 10, cr[CR_YELLOW]);

    // Gamma-correction num
    MN_DrTextA(gammalvls[vid_gamma][1], M_ItemRightAlign(gammalvls[vid_gamma][1]), 20,
               M_Item_Glow(0, GLOW_LIGHTGRAY));

    // Field of View
    sprintf(str, "%d", vid_fov);
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, vid_fov == 135 || vid_fov == 70 ? GLOW_YELLOW :
                              vid_fov == 90 ? GLOW_DARKRED : GLOW_GREEN));

    // Background shading
    sprintf(str, dp_menu_shading ? "%d" : "OFF", dp_menu_shading);
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, dp_menu_shading == 12 ? GLOW_YELLOW :
                              dp_menu_shading  >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, dp_level_brightness ? "%d" : "OFF", dp_level_brightness);
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, dp_level_brightness == 8 ? GLOW_YELLOW :
                              dp_level_brightness >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    MN_DrTextACentered("COLOR SETTINGS", 60, cr[CR_YELLOW]);

    // Saturation
    M_snprintf(str, 6, "%d%%", vid_saturation);
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, GLOW_LIGHTGRAY));

    // RED intensity
    M_snprintf(str, 6, "%3f", vid_r_intensity);
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, GLOW_RED));

    // GREEN intensity
    M_snprintf(str, 6, "%3f", vid_g_intensity);
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, GLOW_GREEN));

    // BLUE intensity
    M_snprintf(str, 6, "%3f", vid_b_intensity);
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, GLOW_BLUE));

    MN_DrTextACentered("MESSAGES SETTINGS", 110, cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, msg_show ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, msg_show ? GLOW_DARKRED : GLOW_GREEN));

    // Text casts shadows
    sprintf(str, msg_text_shadows ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, msg_text_shadows ? GLOW_GREEN : GLOW_DARKRED));

    // Local time
    sprintf(str, msg_local_time == 1 ? "12-HOUR FORMAT" :
                 msg_local_time == 2 ? "24-HOUR FORMAT" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 140,
               M_Item_Glow(12, msg_local_time ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_ID_Gamma (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_gamma = M_INT_Slider(vid_gamma, 0, 14, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
#else
    I_SetPalette(SB_palette);
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    SB_ForceRedraw();
#endif
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

static void M_ID_Saturation (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_saturation = M_INT_Slider(vid_saturation, 0, 100, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE) + SB_palette * 768);
#else
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    SB_ForceRedraw();
#endif
}

static void M_ID_R_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_r_intensity = M_FLOAT_Slider(vid_r_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE) + SB_palette * 768);
#else
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    SB_ForceRedraw();
#endif
}

static void M_ID_G_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_g_intensity = M_FLOAT_Slider(vid_g_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE) + SB_palette * 768);
#else
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    SB_ForceRedraw();
#endif
}

static void M_ID_B_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_b_intensity = M_FLOAT_Slider(vid_b_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE) + SB_palette * 768);
#else
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    SB_ForceRedraw();
#endif
}

static void M_ID_Messages (int choice)
{
    msg_show ^= 1;
    CT_SetMessage(&players[consoleplayer],
                  msg_show ? "MESSAGES ON" : "MESSAGES OFF", true, NULL);
    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
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
    { ITT_LRFUNC, "SFX VOLUME",           SCSfxVolume,         MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_LRFUNC, "MUSIC VOLUME",         SCMusicVolume,       MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,             0, MENU_NONE },
    { ITT_LRFUNC, "MUSIC PLAYBACK",       M_ID_MusicSystem, 0, MENU_NONE },
    { ITT_LRFUNC, "SOUNDS EFFECTS MODE",  M_ID_SFXMode,     0, MENU_NONE },
    { ITT_LRFUNC, "PITCH-SHIFTED SOUNDS", M_ID_PitchShift,  0, MENU_NONE },
    { ITT_LRFUNC, "NUMBER OF SFX TO MIX", M_ID_SFXChannels, 0, MENU_NONE },
    { ITT_LRFUNC, "MUTE INACTIVE WINDOW", M_ID_MuteInactive,0, MENU_NONE },
};

static Menu_t ID_Def_Sound = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Sound,
    12, ID_Menu_Sound,
    0,
    true, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Sound (void)
{
    char str[32];

    MN_DrTextACentered("SOUND OPTIONS", 10, cr[CR_YELLOW]);

    DrawSlider(&ID_Def_Sound, 1, 16, snd_MaxVolume, false, 0);
    sprintf(str,"%d", snd_MaxVolume);
    MN_DrTextA(str, 228, 35, M_Item_Glow(0, GLOW_LIGHTGRAY));

    DrawSlider(&ID_Def_Sound, 4, 16, snd_MusicVolume, false, 3);
    sprintf(str,"%d", snd_MusicVolume);
    MN_DrTextA(str, 228, 65, M_Item_Glow(3, GLOW_LIGHTGRAY));

    MN_DrTextACentered("SOUND SYSTEM", 80, cr[CR_YELLOW]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ?  "GUS (EMULATED)" :
                 snd_musicdevice == 8 ?  "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                         "UNKNOWN");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, snd_musicdevice ? GLOW_GREEN : GLOW_RED));

    // Sound effects mode
    sprintf(str, snd_monosfx ? "MONO" : "STEREO");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, snd_monosfx ? GLOW_RED : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, snd_pitchshift ? GLOW_GREEN : GLOW_RED));

    // Number of SFX to mix
    sprintf(str, "%i", snd_channels);
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, snd_channels == 8 ? GLOW_DARKRED :
                               snd_channels  < 3 ? GLOW_RED : GLOW_YELLOW));

    // Mute inactive window
    sprintf(str, snd_mute_inactive ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, snd_mute_inactive ? GLOW_GREEN : GLOW_RED));

    // Inform that music system is not hot-swappable. :(
    if (CurrentItPos == 7)
    {
        MN_DrTextACentered("CHANGE WILL REQUIRE RESTART OF THE PROGRAM", 142, cr[CR_GRAY]);
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
    { ITT_LRFUNC,  "SENSIVITY",               SCMouseSensi,               0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC,  "ACCELERATION",            M_ID_Controls_Acceleration, 0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC,  "ACCELERATION THRESHOLD",  M_ID_Controls_Threshold,    0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC,  "MOUSE LOOK",              M_ID_Controls_MLook,        0, MENU_NONE          },
    { ITT_LRFUNC,  "VERTICAL MOUSE MOVEMENT", M_ID_Controls_NoVert,       0, MENU_NONE          },
    { ITT_LRFUNC,  "INVERT VERTICAL AXIS",    M_ID_Controls_InvertY,      0, MENU_NONE          },
    { ITT_EMPTY,   NULL,                      NULL,                       0, MENU_NONE          },
    { ITT_LRFUNC,  "PERMANENT \"NOARTISKIP\" MODE", M_ID_Controls_NoArtiSkip, 0, MENU_NONE      },
};

static Menu_t ID_Def_Controls = {
    /*ID_MENU_LEFTOFFSET*/ 44, ID_MENU_TOPOFFSET,
    M_Draw_ID_Controls,
    17, ID_Menu_Controls,
    0,
    true, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Controls (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("BINDINGS", 10, cr[CR_YELLOW]);

    MN_DrTextACentered("MOUSE CONFIGURATION", 40, cr[CR_YELLOW]);

    DrawSlider(&ID_Def_Controls, 4, 10, mouseSensitivity, false, 3);
    sprintf(str,"%d", mouseSensitivity);
    MN_DrTextA(str, 180, 65, M_Item_Glow(3, mouseSensitivity == 255 ? GLOW_YELLOW :
                                         mouseSensitivity > 9 ? GLOW_GREEN : GLOW_LIGHTGRAY));

    DrawSlider(&ID_Def_Controls, 7, 12, mouse_acceleration * 2, false, 6);
    sprintf(str,"%.1f", mouse_acceleration);
    MN_DrTextA(str, 196, 95, M_Item_Glow(6, GLOW_LIGHTGRAY));

    DrawSlider(&ID_Def_Controls, 10, 14, mouse_threshold / 2, false, 9);
    sprintf(str,"%d", mouse_threshold);
    MN_DrTextA(str, 212, 125, M_Item_Glow(9, GLOW_LIGHTGRAY));

    // Mouse look
    sprintf(str, mouse_look ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 140,
               M_Item_Glow(12, mouse_look ? GLOW_GREEN : GLOW_RED));

    // Vertical mouse movement
    sprintf(str, mouse_novert ? "OFF" : "ON");
    MN_DrTextA(str, M_ItemRightAlign(str), 150,
               M_Item_Glow(13, mouse_novert ? GLOW_RED : GLOW_GREEN));

    // Invert vertical axis
    sprintf(str, mouse_y_invert ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 160,
               M_Item_Glow(14, mouse_y_invert ? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered("MISCELLANEOUS", 170, cr[CR_YELLOW]);

    // Permanent "noartiskip" mode
    sprintf(str, ctrl_noartiskip ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 180,
               M_Item_Glow(16, ctrl_noartiskip ? GLOW_GREEN : GLOW_RED));
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
    { ITT_EFUNC, "JUMP",            M_Bind_Jump,         0, MENU_NONE },
    { ITT_EFUNC, "180 DEGREE TURN", M_Bind_180Turn,      0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,                0, MENU_NONE },
    { ITT_EFUNC, "FIRE/ATTACK",     M_Bind_FireAttack,   0, MENU_NONE },
    { ITT_EFUNC, "USE",             M_Bind_Use,          0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_1 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_1,
    13, ID_Menu_Keybinds_1,
    0,
    true, true, true,
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
    M_DrawBindKey(8, 100, key_jump);
    M_DrawBindKey(9, 110, key_180turn);

    MN_DrTextACentered("ACTION", 120, cr[CR_YELLOW]);

    M_DrawBindKey(11, 130, key_fire);
    M_DrawBindKey(12, 140, key_use);

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

static void M_Bind_Jump (int option)
{
    M_StartBind(108);  // key_jump
}

static void M_Bind_180Turn (int choice)
{
    M_StartBind(109);  // key_180turn
}

static void M_Bind_FireAttack (int option)
{
    M_StartBind(110);  // key_fire
}

static void M_Bind_Use (int option)
{
    M_StartBind(111);  // key_use
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
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_2,
    11, ID_Menu_Keybinds_2,
    0,
    true, true, true,
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
    { ITT_EMPTY, NULL,                      NULL,                 0, MENU_NONE },
    { ITT_EFUNC, "RESTART LEVEL/DEMO",      M_Bind_RestartLevel,  0, MENU_NONE },
    { ITT_EFUNC, "GO TO NEXT LEVEL",        M_Bind_NextLevel,     0, MENU_NONE },
    { ITT_EFUNC, "DEMO FAST-FORWARD",       M_Bind_FastForward,   0, MENU_NONE },
    { ITT_EFUNC, "FLIP LEVEL HORIZONTALLY", M_Bind_FlipLevels,    0, MENU_NONE },
    { ITT_EMPTY, NULL,                      NULL,                 0, MENU_NONE },
    { ITT_EFUNC, "SPECTATOR MODE",          M_Bind_SpectatorMode, 0, MENU_NONE },
    { ITT_EFUNC, "FREEZE MODE",             M_Bind_FreezeMode,    0, MENU_NONE },
    { ITT_EFUNC, "NOTARGET MODE",           M_Bind_NotargetMode,  0, MENU_NONE },
    { ITT_EFUNC, "BUDDHA MODE",             M_Bind_BuddhaMode,    0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_3 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_3,
    12, ID_Menu_Keybinds_3,
    0,
    true, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_3 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS3;

    M_FillBackground();

    MN_DrTextACentered("ADVANCED MOVEMENT", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_autorun);
    M_DrawBindKey(1, 30, key_mouse_look);

    MN_DrTextACentered("SPECIAL KEYS", 40, cr[CR_YELLOW]);

    M_DrawBindKey(3, 50, key_reloadlevel);
    M_DrawBindKey(4, 60, key_nextlevel);
    M_DrawBindKey(5, 70, key_demospeed);
    M_DrawBindKey(6, 80, key_flip_levels);

    MN_DrTextACentered("SPECIAL MODES", 90, cr[CR_YELLOW]);

    M_DrawBindKey(8, 100, key_spectator);
    M_DrawBindKey(9, 110, key_freeze);
    M_DrawBindKey(10, 120, key_notarget);
    M_DrawBindKey(11, 130, key_buddha);

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

static void M_Bind_RestartLevel (int option)
{
    M_StartBind(302);  // key_reloadlevel
}

static void M_Bind_NextLevel (int option)
{
    M_StartBind(303);  // key_nextlevel
}

static void M_Bind_FastForward (int option)
{
    M_StartBind(304);  // key_demospeed
}

static void M_Bind_FlipLevels (int choice)
{
    M_StartBind(305);  // key_flip_levels
}

static void M_Bind_SpectatorMode (int option)
{
    M_StartBind(306);  // key_spectator
}

static void M_Bind_FreezeMode (int option)
{
    M_StartBind(307);  // key_freeze
}

static void M_Bind_NotargetMode (int option)
{
    M_StartBind(308);  // key_notarget
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(309);  // key_buddha
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_4[] = {
    { ITT_EFUNC, "WEAPON 1",             M_Bind_Weapon1,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 2",             M_Bind_Weapon2,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 3",             M_Bind_Weapon3,    0, MENU_NONE },
    { ITT_EFUNC, "WEAPON 4",             M_Bind_Weapon4,    0, MENU_NONE },
    { ITT_EFUNC, "PREVIOUS WEAPON",      M_Bind_PrevWeapon, 0, MENU_NONE },
    { ITT_EFUNC, "NEXT WEAPON",          M_Bind_NextWeapon, 0, MENU_NONE },
    { ITT_EMPTY, NULL,                   NULL,              0, MENU_NONE },
    { ITT_EFUNC, "QUARTZ FLASK",         M_Bind_Quartz,     0, MENU_NONE },
    { ITT_EFUNC, "MYSTIC URN",           M_Bind_Urn,        0, MENU_NONE },
    { ITT_EFUNC, "FLECHETTE",            M_Bind_Flechette,  0, MENU_NONE },
    { ITT_EFUNC, "DISK OF REPULSTION",   M_Bind_Disk,       0, MENU_NONE },
    { ITT_EFUNC, "ICON OF THE DEFENDER", M_Bind_Icon,       0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_4 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_4,
    12, ID_Menu_Keybinds_4,
    0,
    true, true, true,
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
    M_DrawBindKey(4, 60, key_prevweapon);
    M_DrawBindKey(5, 70, key_nextweapon);

    MN_DrTextACentered("ARTIFACTS", 80, cr[CR_YELLOW]);

    M_DrawBindKey(7, 90, key_arti_health);
    M_DrawBindKey(8, 100, key_arti_urn);
    M_DrawBindKey(9, 110, key_arti_poisonbag);
    M_DrawBindKey(10, 120, key_arti_blastradius);
    M_DrawBindKey(11, 130, key_arti_invulnerability);

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

static void M_Bind_PrevWeapon (int option)
{
    M_StartBind(404);  // key_prevweapon
}

static void M_Bind_NextWeapon (int option)
{
    M_StartBind(405);  // key_nextweapon
}

static void M_Bind_Quartz (int option)
{
    M_StartBind(406);  // key_arti_health
}

static void M_Bind_Urn (int option)
{
    M_StartBind(407);  // key_arti_urn
}

static void M_Bind_Flechette (int option)
{
    M_StartBind(408);  // key_arti_poisonbag
}

static void M_Bind_Disk (int option)
{
    M_StartBind(409);  // key_arti_blastradius
}

static void M_Bind_Icon (int option)
{
    M_StartBind(410);  // key_arti_invulnerability
}

// -----------------------------------------------------------------------------
// Keybinds 5
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Keybinds_5[] = {
    { ITT_EFUNC, "PORKALATOR",          M_Bind_Porkalator, 0, MENU_NONE },
    { ITT_EFUNC, "CHAOS DEVICE",        M_Bind_Chaos,      0, MENU_NONE },
    { ITT_EFUNC, "BANISHMENT DEVICE",   M_Bind_Banishment, 0, MENU_NONE },
    { ITT_EFUNC, "WINGS OF WRATH",      M_Bind_Wings,      0, MENU_NONE },
    { ITT_EFUNC, "DARK SERVANT",        M_Bind_Servant,    0, MENU_NONE },
    { ITT_EFUNC, "DRAGONSKIN BRACERS",  M_Bind_Bracers,    0, MENU_NONE },
    { ITT_EFUNC, "BOOTS OF SPEED",      M_Bind_Boots,      0, MENU_NONE },
    { ITT_EFUNC, "TORCH",               M_Bind_Torch,      0, MENU_NONE },
    { ITT_EFUNC, "KRATER OF MIGHT",     M_Bind_Krater,     0, MENU_NONE },
    { ITT_EFUNC, "MYSTIC AMBIT INCANT", M_Bind_Incant,     0, MENU_NONE },    
    { ITT_EFUNC, "ONE OF EACH",         M_Bind_AllArti,    0, MENU_NONE },
};

static Menu_t ID_Def_Keybinds_5 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_5,
    11, ID_Menu_Keybinds_5,
    0,
    true, true, true,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_Keybinds_5 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_ID_KBDBINDS5;

    M_FillBackground();

    MN_DrTextACentered("ARTIFACTS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_arti_egg);
    M_DrawBindKey(1, 30, key_arti_teleport);
    M_DrawBindKey(2, 40, key_arti_teleportother);
    M_DrawBindKey(3, 50, key_arti_wings);
    M_DrawBindKey(4, 60, key_arti_servant);
    M_DrawBindKey(5, 70, key_arti_bracers);
    M_DrawBindKey(6, 80, key_arti_boots);
    M_DrawBindKey(7, 90, key_arti_torch);
    M_DrawBindKey(8, 100, key_arti_krater);
    M_DrawBindKey(9, 110, key_arti_incant);
    M_DrawBindKey(10, 120, key_arti_all);

    M_DrawBindFooter("5", true);
}

static void M_Bind_Porkalator (int option)
{
    M_StartBind(500);  // key_arti_egg
}

static void M_Bind_Chaos (int option)
{
    M_StartBind(501);  // key_arti_teleport
}

static void M_Bind_Banishment (int option)
{
    M_StartBind(502);  // key_arti_teleportother
}

static void M_Bind_Wings (int option)
{
    M_StartBind(503);  // key_arti_wings
}

static void M_Bind_Servant (int option)
{
    M_StartBind(504);  // key_arti_servant
}

static void M_Bind_Bracers (int option)
{
    M_StartBind(505);  // key_arti_bracers
}

static void M_Bind_Boots (int option)
{
    M_StartBind(506);  // key_arti_boots
}

static void M_Bind_Torch (int option)
{
    M_StartBind(507);  // key_arti_torch
}

static void M_Bind_Krater (int option)
{
    M_StartBind(508);  // key_arti_krater
}

static void M_Bind_Incant (int option)
{
    M_StartBind(509);  // key_arti_incant
}

static void M_Bind_AllArti (int option)
{
    M_StartBind(510);  // key_arti_all
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
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_6,
    10, ID_Menu_Keybinds_6,
    0,
    true, true, true,
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
    {ITT_EFUNC, "SUICIDE",         M_Bind_Suicide,        0, MENU_NONE},
    {ITT_EFUNC, "QUICK SAVE",      M_Bind_QuickSave,      0, MENU_NONE},
    {ITT_EFUNC, "END GAME",        M_Bind_EndGame,        0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE MESSAGES", M_Bind_ToggleMessages, 0, MENU_NONE},
    {ITT_EFUNC, "QUICK LOAD",      M_Bind_QuickLoad,      0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME",       M_Bind_QuitGame,       0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE GAMMA",    M_Bind_ToggleGamma,    0, MENU_NONE},
    {ITT_EFUNC, "MULTIPLAYER SPY", M_Bind_MultiplayerSpy, 0, MENU_NONE}
};

static Menu_t ID_Def_Keybinds_7 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_7,
    12, ID_Menu_Keybinds_7,
    0,
    true, true, true,
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

static void M_Bind_Suicide (int option)
{
    // [JN] TODO own key to detail toggling?
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
    {ITT_EMPTY, NULL,                    NULL,                  0, MENU_NONE},
    {ITT_EFUNC, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,      0, MENU_NONE},
    /*
    {ITT_EFUNC, "- TO PLAYER 1",         M_Bind_ToPlayer1,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 2",         M_Bind_ToPlayer2,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 3",         M_Bind_ToPlayer3,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 4",         M_Bind_ToPlayer4,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 5",         M_Bind_ToPlayer5,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 6",         M_Bind_ToPlayer6,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 7",         M_Bind_ToPlayer7,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 8",         M_Bind_ToPlayer8,      0, MENU_NONE},
    */
};

static Menu_t ID_Def_Keybinds_8 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Keybinds_8,
    8, ID_Menu_Keybinds_8,
    0,
    true, true, true,
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

    MN_DrTextACentered("RESET", 80, cr[CR_YELLOW]);
    
    /*
    M_DrawBindKey(6, 80, key_multi_msgplayer[0]);
    M_DrawBindKey(7, 90, key_multi_msgplayer[1]);
    M_DrawBindKey(8, 100, key_multi_msgplayer[2]);
    M_DrawBindKey(9, 110, key_multi_msgplayer[3]);
    */

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

/*
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
*/

static void M_Bind_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 7;      // [JN] keybinds reset
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_MouseBinds[] = {
    { ITT_EFUNC, "FIRE/ATTACK",               M_Bind_M_FireAttack,     0, MENU_NONE },
    { ITT_EFUNC, "MOVE FORWARD",              M_Bind_M_MoveForward,    0, MENU_NONE },
    { ITT_EFUNC, "STRAFE ON",                 M_Bind_M_StrafeOn,       0, MENU_NONE },
    { ITT_EFUNC, "MOVE BACKWARD",             M_Bind_M_MoveBackward,   0, MENU_NONE },
    { ITT_EFUNC, "USE",                       M_Bind_M_Use,            0, MENU_NONE },
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
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_MouseBinds,
    14, ID_Menu_MouseBinds,
    0,
    true, false, false,
    MENU_ID_CONTROLS
};

static void M_Draw_ID_MouseBinds (void)
{
    M_FillBackground();

    MN_DrTextACentered("MOUSE BINDINGS", 10, cr[CR_YELLOW]);

    M_DrawBindButton(0, 20, mousebfire);
    M_DrawBindButton(1, 30, mousebforward);
    M_DrawBindButton(2, 40, mousebstrafe);
    M_DrawBindButton(3, 50, mousebbackward);
    M_DrawBindButton(4, 60, mousebuse);
    M_DrawBindButton(5, 70, mousebstrafeleft);
    M_DrawBindButton(6, 80, mousebstraferight);
    M_DrawBindButton(7, 90, mousebprevweapon);
    M_DrawBindButton(8, 100, mousebnextweapon);
    M_DrawBindButton(9, 110, mousebinvleft);
    M_DrawBindButton(10, 120, mousebinvright);
    M_DrawBindButton(11, 130, mousebuseartifact);

    MN_DrTextACentered("RESET", 140, cr[CR_YELLOW]);

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

static void M_Bind_M_StrafeOn (int option)
{
    M_StartMouseBind(1002);  // mousebstrafe
}

static void M_Bind_M_MoveBackward (int option)
{
    M_StartMouseBind(1003);  // mousebbackward
}

static void M_Bind_M_Use (int option)
{
    M_StartMouseBind(1004);  // mousebuse
}

static void M_Bind_M_StrafeLeft (int option)
{
    M_StartMouseBind(1005);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int option)
{
    M_StartMouseBind(1006);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int option)
{
    M_StartMouseBind(1007);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int option)
{
    M_StartMouseBind(1008);  // mousebnextweapon
}

static void M_Bind_M_InventoryLeft (int option)
{
    M_StartMouseBind(1009);  // mousebinvleft
}

static void M_Bind_M_InventoryRight (int option)
{
    M_StartMouseBind(1010);  // mousebinvright
}

static void M_Bind_M_UseArtifact (int option)
{
    M_StartMouseBind(1011);  // mousebuseartifact
}

static void M_Bind_M_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 8;      // [JN] mouse binds reset
}

// -----------------------------------------------------------------------------
// Widgets and Automap
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Widgets[] = {
    { ITT_LRFUNC, "WIDGETS LOCATION",      M_ID_Widget_Location,   0, MENU_NONE },
    { ITT_LRFUNC, "TOTAL KILLS",           M_ID_Widget_Kills,      0, MENU_NONE },
    { ITT_LRFUNC, "LEVEL NAME",            M_ID_Widget_LevelName,  0, MENU_NONE },
    { ITT_LRFUNC, "PLAYER COORDS",         M_ID_Widget_Coords,     0, MENU_NONE },
    { ITT_LRFUNC, "RENDER COUNTERS",       M_ID_Widget_Render,     0, MENU_NONE },
    { ITT_LRFUNC, "TARGET'S HEALTH",       M_ID_Widget_Health,     0, MENU_NONE },
    { ITT_EMPTY,  NULL,                    NULL,                   0, MENU_NONE },
    { ITT_LRFUNC, "ROTATE MODE",           M_ID_Automap_Rotate,    0, MENU_NONE },
    { ITT_LRFUNC, "OVERLAY MODE",          M_ID_Automap_Overlay,   0, MENU_NONE },
    { ITT_LRFUNC, "OVERLAY SHADING LEVEL", M_ID_Automap_Shading, 0, MENU_NONE },
};

static Menu_t ID_Def_Widgets = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Widgets,
    10, ID_Menu_Widgets,
    0,
    true, false, false,
    MENU_ID_MAIN
};

static void M_Draw_ID_Widgets (void)
{
    char str[32];

    MN_DrTextACentered("WIDGETS", 10, cr[CR_YELLOW]);

    // Widgets location
    sprintf(str, widget_location ? "TOP" : "BOTTOM");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, GLOW_GREEN));

    // Total kills
    sprintf(str, widget_kis == 1 ? "ALWAYS"  :
                 widget_kis == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, widget_kis ? GLOW_GREEN : GLOW_DARKRED));

    // Level name
    sprintf(str, widget_levelname ? "ALWAYS" : "AUTOMAP");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, widget_levelname ? GLOW_GREEN : GLOW_DARKRED));

    // Player coords
    sprintf(str, widget_coords == 1 ? "ALWAYS"  :
                 widget_coords == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, widget_coords ? GLOW_GREEN : GLOW_DARKRED));

    // Render counters
    sprintf(str, widget_render ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, widget_render ? GLOW_GREEN : GLOW_DARKRED));

    // Target's health
    sprintf(str, widget_health == 1 ? "TOP" :
                 widget_health == 2 ? "TOP+NAME" :
                 widget_health == 3 ? "BOTTOM" :
                 widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, widget_health ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("AUTOMAP", 80, cr[CR_YELLOW]);

    // Rotate mode
    sprintf(str, automap_rotate ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, automap_rotate ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay mode
    sprintf(str, automap_overlay ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, automap_overlay ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay shading level
    sprintf(str,"%d", automap_shading);
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, !automap_overlay ? GLOW_DARKRED :
                               automap_shading ==  0 ? GLOW_RED :
                               automap_shading == 12 ? GLOW_YELLOW : GLOW_GREEN));
}

static void M_ID_Widget_Location (int choice)
{
    widget_location ^= 1;
}

static void M_ID_Widget_Kills (int choice)
{
    widget_kis = M_INT_Slider(widget_kis, 0, 2, choice, false);
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


// -----------------------------------------------------------------------------
// Gameplay features 1
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Gameplay_1[] = {
    { ITT_LRFUNC,  "BRIGHTMAPS",                  M_ID_Brightmaps,      0, MENU_NONE         },
    { ITT_LRFUNC,  "EXTRA TRANSLUCENCY",          M_ID_Translucency,    0, MENU_NONE         },
    { ITT_LRFUNC,  "DIMINISHED LIGHTING",         M_ID_SmoothLighting,  0, MENU_NONE         },
    { ITT_LRFUNC,  "LIQUIDS ANIMATION",           M_ID_SwirlingLiquids, 0, MENU_NONE         },
    { ITT_LRFUNC,  "SKY DRAWING MODE",            M_ID_LinearSky,       0, MENU_NONE         },
    { ITT_LRFUNC,  "RANDOMLY MIRRORED CORPSES",   M_ID_FlipCorpses,     0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE         },
    { ITT_LRFUNC,  "SHAPE",                       M_ID_Crosshair,       0, MENU_NONE         },
    { ITT_LRFUNC,  "INDICATION",                  M_ID_CrosshairColor,  0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE         },
    { ITT_LRFUNC,  "COLORED ELEMENTS",            M_ID_ColoredSBar,     0, MENU_NONE         },
    { ITT_LRFUNC,  "4TH WEAPON WIDGET",            M_ID_WeaponWidget,    0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                 0, MENU_NONE         },
    { ITT_SETMENU, "", /*NEXT PAGE >*/            NULL,                 0, MENU_ID_GAMEPLAY2 },
};

static Menu_t ID_Def_Gameplay_1 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_1,
    14, ID_Menu_Gameplay_1,
    0,
    true, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_1 (void)
{
    char str[32];
    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY1;

    MN_DrTextACentered("VISUAL", 10, cr[CR_YELLOW]);

    // Brightmaps
    sprintf(str, vis_brightmaps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, vis_brightmaps ? GLOW_GREEN : GLOW_DARKRED));

    // Translucency
    sprintf(str, vis_translucency == 1 ? "ADDITIVE" :
                 vis_translucency == 2 ? "BLENDING" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, vis_translucency ? GLOW_GREEN : GLOW_DARKRED));

    // Diminished lighting
    sprintf(str, vis_smooth_light ? "SMOOTH" : "ORIGINAL");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, vis_smooth_light ? GLOW_GREEN : GLOW_DARKRED));

    // Liquids animation
    sprintf(str, vis_swirling_liquids ? "SWIRLING" : "ORIGINAL");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, vis_swirling_liquids ? GLOW_GREEN : GLOW_DARKRED));

    // Sky drawing mode
    sprintf(str, vis_linear_sky ? "LINEAR" : "ORIGINAL");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, vis_linear_sky ? GLOW_GREEN : GLOW_DARKRED));

    // Randomly mirrored corpses
    sprintf(str, vis_flip_corpses ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, vis_flip_corpses ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("CROSSHAIR", 80, cr[CR_YELLOW]);

    // Crosshair shape
    sprintf(str, xhair_draw == 1 ? "CROSS 1" :
                 xhair_draw == 2 ? "CROSS 2" :
                 xhair_draw == 3 ? "X" :
                 xhair_draw == 4 ? "CIRCLE" :
                 xhair_draw == 5 ? "ANGLE" :
                 xhair_draw == 6 ? "TRIANGLE" :
                 xhair_draw == 7 ? "DOT" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, xhair_draw ? GLOW_GREEN : GLOW_DARKRED));

    // Crosshair indication
    sprintf(str, xhair_color == 1 ? "HEALTH" :
                 xhair_color == 2 ? "TARGET HIGHLIGHT" :
                 xhair_color == 3 ? "TGT HIGHLIGHT+HEALTH" : "STATIC");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, !xhair_draw ? GLOW_DARKRED :
                               xhair_color ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("STATUS BAR", 110, cr[CR_YELLOW]);

    // Colored elements
    sprintf(str, st_colored_stbar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, st_colored_stbar ? GLOW_GREEN : GLOW_DARKRED));

    // Fourth weapon widget
    sprintf(str, st_weapon_widget == 1 ? "SOLID" :
                 st_weapon_widget == 2 ? "TRANSLUCENT" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, st_weapon_widget ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextA("NEXT PAGE", ID_MENU_LEFTOFFSET, 150,
               M_Item_Glow(13, GLOW_DARKGRAY));

    // Footer
    sprintf(str, "PAGE 1/2");
    MN_DrTextA(str, M_ItemRightAlign(str), 150, M_Item_Glow(13, GLOW_DARKGRAY));
}

static void M_ID_Brightmaps (int choice)
{
    vis_brightmaps ^= 1;
}

static void M_ID_Translucency (int choice)
{
    vis_translucency = M_INT_Slider(vis_translucency, 0, 2, choice, false);
}

static void M_ID_SmoothLightingHook (void)
{
    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
    // [crispy] re-calculate fake contrast
    P_SegLengths();
}

static void M_ID_SmoothLighting (int choice)
{
    vis_smooth_light ^= 1;
    post_rendering_hook = M_ID_SmoothLightingHook;
}

static void M_ID_SwirlingLiquids (int choice)
{
    vis_swirling_liquids ^= 1;
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

static void M_ID_ColoredSBar (int choice)
{
    st_colored_stbar ^= 1;
}

static void M_ID_WeaponWidget (int choice)
{
    st_weapon_widget = M_INT_Slider(st_weapon_widget, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Gameplay features 2
// -----------------------------------------------------------------------------

static MenuItem_t ID_Menu_Gameplay_2[] = {
    { ITT_LRFUNC,  "CORPSES SLIDING FROM LEDGES", M_ID_Torque,         0, MENU_NONE         },
    { ITT_LRFUNC,  "IMITATE PLAYER'S BREATHING",  M_ID_Breathing,      0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                0, MENU_NONE         },
    { ITT_LRFUNC,  "DEFAULT PLAYER CLASS",        M_ID_DefaultClass,   0, MENU_NONE         },
    { ITT_LRFUNC,  "DEFAULT SKILL LEVEL",         M_ID_DefaultSkill,   0, MENU_NONE         },
    { ITT_LRFUNC,  "FLIP LEVELS HORIZONTALLY",    M_ID_FlipLevels,     0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                0, MENU_NONE         },
    { ITT_LRFUNC,  "SHOW DEMO TIMER",             M_ID_DemoTimer,      0, MENU_NONE         },
    { ITT_LRFUNC,  "TIMER DIRECTION",             M_ID_TimerDirection, 0, MENU_NONE         },
    { ITT_LRFUNC,  "SHOW PROGRESS BAR",           M_ID_ProgressBar,    0, MENU_NONE         },
    { ITT_LRFUNC,  "PLAY INTERNAL DEMOS",         M_ID_InternalDemos,  0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                0, MENU_NONE         },
    { ITT_EMPTY,   NULL,                          NULL,                0, MENU_NONE         },
    { ITT_SETMENU, "", /*PREVIOUS PAGE >*/        NULL,                0, MENU_ID_GAMEPLAY1 },
};

static Menu_t ID_Def_Gameplay_2 = {
    ID_MENU_LEFTOFFSET, ID_MENU_TOPOFFSET,
    M_Draw_ID_Gameplay_2,
    14, ID_Menu_Gameplay_2,
    0,
    true, false, true,
    MENU_ID_MAIN
};

static void M_Draw_ID_Gameplay_2 (void)
{
    char str[32];
    Gameplay_Cur = (MenuType_t)MENU_ID_GAMEPLAY2;

    MN_DrTextACentered("PHYSICAL", 10, cr[CR_YELLOW]);

    // Corpses sliding from ledges
    sprintf(str, phys_torque ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, phys_torque ? GLOW_GREEN : GLOW_DARKRED));

    // Imitate player's breathing
    sprintf(str, phys_breathing ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, phys_breathing ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("GAMEPLAY", 40, cr[CR_YELLOW]);

    // Default player class
    M_snprintf(str, sizeof(str), "%s", DefClassName[gp_default_class]);
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, gp_default_class == 0 ? GLOW_GREEN :
                              gp_default_class == 1 ? GLOW_BLUE : GLOW_RED));

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[gp_default_skill]);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               DefSkillColor(gp_default_skill));

    // Flip levels horizontally
    sprintf(str, gp_flip_levels ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, gp_flip_levels ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("DEMOS", 80, cr[CR_YELLOW]);

    // Show Demo timer
    sprintf(str, demo_timer == 1 ? "PLAYBACK" : 
                 demo_timer == 2 ? "RECORDING" : 
                 demo_timer == 3 ? "ALWAYS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Timer direction
    sprintf(str, demo_timerdir ? "BACKWARD" : "FORWARD");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Show progress bar
    sprintf(str, demo_bar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, demo_bar ? GLOW_GREEN : GLOW_DARKRED));

    // Play internal demos
    sprintf(str, demo_internal ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, demo_internal ? GLOW_DARKRED : GLOW_GREEN));

    MN_DrTextA("PREVIOUS PAGE", ID_MENU_LEFTOFFSET, 150,
               M_Item_Glow(13, GLOW_DARKGRAY));

    // Footer
    sprintf(str, "PAGE 2/2");
    MN_DrTextA(str, M_ItemRightAlign(str), 150, M_Item_Glow(13, GLOW_DARKGRAY));
}

static void M_ID_Torque (int choice)
{
    phys_torque ^= 1;
}

static void M_ID_Breathing (int choice)
{
    phys_breathing ^= 1;
}

static void M_ID_DefaultClass (int choice)
{
    gp_default_class = M_INT_Slider(gp_default_class, 0, 2, choice, false);
    ClassMenu.oldItPos = gp_default_class;
}

static void M_ID_DefaultSkill (int choice)
{
    gp_default_skill = M_INT_Slider(gp_default_skill, 0, 4, choice, false);
    SkillMenu.oldItPos = gp_default_skill;
}

static void M_ID_FlipLevels (int choice)
{
    gp_flip_levels ^= 1;

    // Redraw game screen
    R_ExecuteSetViewSize();
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
}

static void M_ID_InternalDemos (int choice)
{
    demo_internal ^= 1;
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
    vid_uncapped_fps = 0;
    vid_fpslimit = 60;
    vid_vsync = 1;
    vid_showfps = 0;
    vid_smooth_scaling = 0;
    // Miscellaneous
    vid_graphical_startup = 0;

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

    widget_location = 0;
    widget_kis = 0;
    widget_levelname = 0;
    widget_coords = 0;
    widget_render = 0;
    widget_health = 0;
    // Automap
    automap_rotate = 0;
    automap_overlay = 0;
    automap_shading = 0;

    //
    // Gameplay features
    //

    // Visual
    vis_brightmaps = 0;
    vis_translucency = 0;
    vis_smooth_light = 0;
    vis_swirling_liquids = 0;
    vis_linear_sky = 0;
    vis_flip_corpses = 0;

    // Crosshair
    xhair_draw = 0;
    xhair_color = 0;

    // Status bar
    st_colored_stbar = 0;
    st_weapon_widget = 0;

    // Physical
    phys_torque = 0;
    phys_breathing = 0;

    // Gameplay
    gp_default_class = 0;
    gp_default_skill = 2;
    gp_flip_levels = 0;

    // Demos
    demo_timer = 0;
    demo_timerdir = 0;
    demo_bar = 0;
    demo_internal = 1;

    // Restart graphical systems
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    R_InitLightTables();
    R_InitSkyMap();
    R_SetViewSize(dp_screen_size, dp_detail_level);
    R_ExecuteSetViewSize();
    I_ToggleVsync();
#ifndef CRISPY_TRUECOLOR
    SB_PaletteFlash(true);
#endif
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
    R_FillBackScreen();
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }

    // Restart audio systems (sort of...)
    snd_MaxVolume = 10;
    snd_MusicVolume = 10;
    S_SetSfxVolume(snd_MaxVolume);
    S_SetMusicVolume(snd_MusicVolume);
}

static void M_ID_ApplyReset (void)
{
    post_rendering_hook = M_ID_ApplyResetHook;
}

static void M_ID_SettingReset (int choice)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 9;      // [JN] Settings reset

    if (!netgame && !demoplayback)
    {
        paused = true;
    }

}

// CODE --------------------------------------------------------------------

static Menu_t *Menus[] = {
    &MainMenu,
    &ClassMenu,
    &SkillMenu,
    &OptionsMenu,
    &Options2Menu,
    &FilesMenu,
    &LoadMenu,
    &SaveMenu,
    // [JN] ID menu items
    &ID_Def_Main,
    &ID_Def_Video,
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
    &ID_Def_Gameplay_1,
    &ID_Def_Gameplay_2,
};

//---------------------------------------------------------------------------
//
// PROC MN_Init
//
//---------------------------------------------------------------------------

void MN_Init(void)
{
    InitFonts();
    MenuActive = false;
//      messageson = true;              // Set by defaults in .CFG
    MauloBaseLump = W_GetNumForName("FBULA0");  // ("M_SKL00");

    // [JN] Apply default player class.
    ClassMenu.oldItPos = gp_default_class;
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
    FontABaseLump = W_GetNumForName("FONTA_S") + 1;
    FontAYellowBaseLump = W_GetNumForName("FONTAY_S") + 1;
    FontBBaseLump = W_GetNumForName("FONTB_S") + 1;
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

void MN_DrTextACentered (const char *text, int y, byte *table)
{
    char c;
    int cx;
    patch_t *p;

    cx = 160 - MN_TextAWidth(text) / 2;
    
    dp_translation = table;

    while ((c = *text++) != 0)
    {
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

//==========================================================================
//
// MN_DrTextAYellow
//
//==========================================================================

void MN_DrTextAYellow(const char *text, int x, int y)
{
    char c;
    patch_t *p;

    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            x += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontAYellowBaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchOptional(x, y, 1, p);
            x += SHORT(p->width) - 1;
        }
    }
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

    // [JN] Menu glowing animation:

    if (!cursor_direction && ++cursor_tics == 8)
    {
        // Brightening
        cursor_direction = true;
    }
    else
    if (cursor_direction && --cursor_tics == -8)
    {
        // Darkening
        cursor_direction = false;
    }

    // [JN] Menu item fading effect:

    for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
    {
        if (CurrentItPos == i)
        {
            // Keep menu item bright
            CurrentMenu->items[i].tics = 5;
        }
        else
        {
            // Decrease tics for glowing effect
            CurrentMenu->items[i].tics--;
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

const char *QuitEndMsg[] = {
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED",
    "ARE YOU SURE YOU WANT TO SUICIDE?",
    "DO YOU WANT TO DELETE THE GAME NAMED",        // [crispy] typeofask 6 (delete a savegame)
    "RESET KEYBOARD BINDINGS TO DEFAULT VALUES?",  // [JN] typeofask 7 (reset keyboard binds)
    "RESET MOUSE BINDINGS TO DEFAULT VALUES?",     // [JN] typeofask 8 (reset mouse binds)
    "",                                            // [JN] typeofask 9 (setting reset), full text in drawer below
};

void MN_Drawer(void)
{
    int i;
    int x;
    int y;
    MenuItem_t *item;
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
        if (askforquit)
        {
            // [JN] Keep backgound filling while asking for 
            // reset and inform about Y or N pressing.
            if (typeofask == 7 || typeofask == 8)
            {
                M_FillBackground();
                MN_DrTextACentered("PRESS Y OR N.", 100, NULL);
            }

            MN_DrTextA(QuitEndMsg[typeofask - 1], 160 -
                       MN_TextAWidth(QuitEndMsg[typeofask - 1]) / 2, 80, NULL);
            if (typeofask == 3)
            {
                MN_DrTextA(SlotText[quicksave - 1], 160 -
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
                MN_DrTextA("?", 160 +
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
            }
            if (typeofask == 4)
            {
                MN_DrTextA(SlotText[quickload - 1], 160 -
                           MN_TextAWidth(SlotText[quickload - 1]) / 2, 90, NULL);
                MN_DrTextA("?", 160 +
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
            }
            if (typeofask == 6)
            {
                MN_DrTextA(SlotText[CurrentItPos], 160 -
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
                MN_DrTextA("?", 160 +
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
            }
            if (typeofask == 9)
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
                // [JN] Highlight selected item (CurrentItPos == i) or apply fading effect.
                if (CurrentMenu->smallFont)
                {
                    MN_DrTextA(item->text, x, y, CurrentItPos == i ?
                               cr[CR_MENU_BRIGHT2] : M_Small_Line_Glow(CurrentMenu->items[i].tics));
                }
                else
                {
                    MN_DrTextB(item->text, x, y, CurrentItPos == i ?
                               cr[CR_MENU_BRIGHT3] : M_Big_Line_Glow(CurrentMenu->items[i].tics));
                }
            }
            y += CurrentMenu->smallFont ? ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;
            item++;
        }

        if (CurrentMenu->smallFont)
        {
            y = CurrentMenu->y + (CurrentItPos * ID_MENU_LINEHEIGHT_SMALL);
            MN_DrTextA("*", x - ID_MENU_CURSOR_OFFSET, y, M_Cursor_Glow(cursor_tics));
        }
        else
        {
            y = CurrentMenu->y + (CurrentItPos * ITEM_HEIGHT) + SELECTOR_YOFFSET;
            selName = MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2";
            V_DrawShadowedPatchOptional(x + SELECTOR_XOFFSET, y, 1,
                        W_CacheLumpName(selName, PU_CACHE));
        }
    }

    // [JN] Always refresh statbar while menu is active.
    SB_ForceRedraw();
}

//---------------------------------------------------------------------------
//
// PROC DrawMainMenu
//
//---------------------------------------------------------------------------

static void DrawMainMenu(void)
{
    int frame;

    frame = (MenuTime / 5) % 7;
    V_DrawPatch(88, 0, W_CacheLumpName("M_HTIC", PU_CACHE));
// Old Gold skull positions: (40, 10) and (232, 10)
    V_DrawPatch(37, 80, W_CacheLumpNum(MauloBaseLump + (frame + 2) % 7,
                                       PU_CACHE));
    V_DrawPatch(278, 80, W_CacheLumpNum(MauloBaseLump + frame, PU_CACHE));
}

//==========================================================================
//
// DrawClassMenu
//
//==========================================================================

static void DrawClassMenu(void)
{
    pclass_t class;
    static const char *boxLumpName[3] = {
        "m_fbox",
        "m_cbox",
        "m_mbox"
    };
    static const char *walkLumpName[3] = {
        "m_fwalk1",
        "m_cwalk1",
        "m_mwalk1"
    };

    MN_DrTextB("CHOOSE CLASS:", 34, 24, NULL);
    class = (pclass_t) CurrentMenu->items[CurrentItPos].option;
    if (class < 3)
    {
        V_DrawPatch(174, 8, W_CacheLumpName(boxLumpName[class], PU_CACHE));
        V_DrawPatch(174 + 24, 8 + 12,
                    W_CacheLumpNum(W_GetNumForName(walkLumpName[class])
                                   + ((MenuTime >> 3) & 3), PU_CACHE));
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawSkillMenu(void)
{
    MN_DrTextB("CHOOSE SKILL LEVEL:", 74, 16, NULL);
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
    CT_ClearMessage(&players[consoleplayer]);
}

// [crispy] support additional pages of savegames
static void DrawSaveLoadBottomLine(const Menu_t *menu)
{
    char pagestr[16];
    static short width;
    const int y = menu->y + ITEM_HEIGHT * SAVES_PER_PAGE;

    if (!width)
    {
        const patch_t *const p = W_CacheLumpName("M_FSLOT", PU_CACHE);
        width = SHORT(p->width);
    }
    if (savepage > 0)
        MN_DrTextA("PGUP", menu->x + 1, y, cr[CR_GRAY]);
    if (savepage < SAVEPAGE_MAX)
        MN_DrTextA("PGDN", menu->x + width - MN_TextAWidth("PGDN"), y, cr[CR_GRAY]);

    M_snprintf(pagestr, sizeof(pagestr), "PAGE %d/%d", savepage + 1, SAVEPAGE_MAX + 1);
    MN_DrTextA(pagestr, ORIGWIDTH / 2 - MN_TextAWidth(pagestr) / 2, y, cr[CR_GRAY]);
}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

static void DrawLoadMenu(void)
{
    const char *title;

    title = quickloadTitle ? "QUICK LOAD GAME" : "LOAD GAME";

    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 7, NULL);
    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&LoadMenu);
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

    title = quicksaveTitle ? "QUICK SAVE GAME" : "SAVE GAME";

    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 7, NULL);
    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&SaveMenu);
    DrawSaveLoadBottomLine(&SaveMenu);
}

static boolean ReadDescriptionForSlot(int slot, char *description)
{
    FILE *fp;
    boolean found;
    char name[100];
    char versionText[HXS_VERSION_TEXT_LENGTH];

    slot += savepage * 10; // [crispy]

    M_snprintf(name, sizeof(name), "%shex%d.sav", SavePath, slot);

    fp = M_fopen(name, "rb");

    if (fp == NULL)
    {
        return false;
    }

    found = fread(description, HXS_DESCRIPTION_LENGTH, 1, fp) == 1
         && fread(versionText, HXS_VERSION_TEXT_LENGTH, 1, fp) == 1;

    found = found && strcmp(versionText, HXS_VERSION_TEXT) == 0;

    fclose(fp);

    return found;
}

//===========================================================================
//
// MN_LoadSlotText
//
// For each slot, looks for save games and reads the description field.
//
//===========================================================================

void MN_LoadSlotText(void)
{
    char description[HXS_DESCRIPTION_LENGTH];
    int slot;

    for (slot = 0; slot < 6; slot++)
    {
        if (ReadDescriptionForSlot(slot, description))
        {
            memcpy(SlotText[slot], description, SLOTTEXTLEN);
            SlotStatus[slot] = 1;
        }
        else
        {
            memset(SlotText[slot], 0, SLOTTEXTLEN);
            SlotStatus[slot] = 0;
        }
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
        V_DrawShadowedPatchOptional(x, y, 1, W_CacheLumpName("M_FSLOT", PU_CACHE));
        if (SlotStatus[i])
        {
            // [JN] Highlight selected item (CurrentItPos == i) or apply fading effect.
            MN_DrTextA(SlotText[i], x + 5, y + 5, CurrentItPos == i ?
                       cr[CR_MENU_BRIGHT2] : M_Small_Line_Glow(CurrentMenu->items[i].tics));
        }
        y += ITEM_HEIGHT;
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
        MN_DrTextB("ON", 196, 50, NULL);
    }
    else
    {
        MN_DrTextB("OFF", 196, 50, NULL);
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
    MN_DrTextA(str, 252, 45, M_Item_Glow(0, snd_MaxVolume ? GLOW_LIGHTGRAY : GLOW_DARKGRAY));

    // Music Volume
    sprintf(str, "%d", snd_MusicVolume);
    DrawSlider(&Options2Menu, 3, 16, snd_MusicVolume, true, 2);
    MN_DrTextA(str, 252, 85, M_Item_Glow(2, snd_MusicVolume ? GLOW_LIGHTGRAY : GLOW_DARKGRAY));

    // Screen Size
    sprintf(str, "%d", dp_screen_size);
    DrawSlider(&Options2Menu, 5,  11, dp_screen_size - 3, true, 4);
    MN_DrTextA(str, 212, 125, M_Item_Glow(4, GLOW_LIGHTGRAY));
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
    if (demoplayback)
    {
        return;
    }
    if (SCNetCheck(3))
    {
        MenuActive = false;
        askforquit = true;
        typeofask = 2;          //endgame
        if (!netgame && !demoplayback)
        {
            paused = true;
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC SCMessages
//
//---------------------------------------------------------------------------

static void SCMessages(int option)
{
    msg_show ^= 1;
    if (msg_show)
    {
        CT_SetMessage(&players[consoleplayer], "MESSAGES ON", true, NULL);
    }
    else
    {
        CT_SetMessage(&players[consoleplayer], "MESSAGES OFF", true, NULL);
    }
    S_StartSound(NULL, SFX_CHAT);
}

//===========================================================================
//
// SCNetCheck
//
//===========================================================================

static boolean SCNetCheck(int option)
{
    if (!netgame)
    {
        return true;
    }
    switch (option)
    {
        case 1:                // new game
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T START A NEW GAME IN NETPLAY!", true, NULL);
            break;
        case 2:                // load game
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T LOAD A GAME IN NETPLAY!", true, NULL);
            break;
        case 3:                // end game
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T END A GAME IN NETPLAY!", true, NULL);
            break;
    }
    MenuActive = false;
    S_StartSound(NULL, SFX_CHAT);
    return false;
}

//===========================================================================
//
// SCNetCheck2
//
//===========================================================================

static void SCNetCheck2(int option)
{
    SCNetCheck(option);
    return;
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static void SCLoadGame(int option)
{
    if (demoplayback)
    {
        // deactivate playback, return control to player
        demoextend = false;
    }
    if (!SlotStatus[option])
    {                           // Don't try to load from an empty slot
        return;
    }
    G_LoadGame(option);
    MN_DeactivateMenu();
    if (quickload == -1)
    {
        quickload = option + 1;
        CT_ClearMessage(&players[consoleplayer]);
    }
}

// [crispy]
static void SCDeleteGame(int option)
{
    if (!SlotStatus[option])
    {
        return;
    }

    SV_ClearSaveSlot(option);
    MN_LoadSlotText();
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // map from IWAD or PWAD?
    if (W_IsIWADLump(maplumpinfo) && strcmp(SavePath, ""))
    {
        M_snprintf(SlotText[slot], SLOTTEXTLEN,
                   "%s", maplumpinfo->name);
    }
    else
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpinfo));
        char *ext = strrchr(wadname, '.');

        if (ext != NULL)
        {
            *ext = '\0';
        }

        M_snprintf(SlotText[slot], SLOTTEXTLEN,
                   "%s (%s)", maplumpinfo->name,
                   wadname);
        free(wadname);
    }
    M_ForceUppercase(SlotText[slot]);
    joypadsave = false;
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

static const char *const class_str[NUMCLASSES] =
{
    "FIGHTER",
    "CLERIC",
    "MAGE",
    "PIG",
};

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

        // [crispy] generate a default save slot name when saving to an empty slot
        if (!strcmp(ptr, "") /* && joypadsave */ || (strlen(oldSlotText) >= 3 && !strncmp(oldSlotText, "HUB", 3)))
        {
            SetDefaultSaveName(option);
          M_snprintf(ptr, sizeof(oldSlotText), "HUB %d.%d, %s",
                     P_GetMapCluster(gamemap), gamemap,
                     class_str[PlayerClass[consoleplayer]]);
        }

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
        CT_ClearMessage(&players[consoleplayer]);
    }
}

//==========================================================================
//
// SCClass
//
//==========================================================================

static void SCClass(int option)
{
    if (netgame)
    {
        CT_SetMessage(&players[consoleplayer],
                      "YOU CAN'T START A NEW GAME FROM WITHIN A NETGAME!",
                     true, NULL);
        return;
    }
    MenuPClass = option;
    switch (MenuPClass)
    {
        case PCLASS_FIGHTER:
            SkillMenu.x = 120;
            SkillItems[0].text = "SQUIRE";
            SkillItems[1].text = "KNIGHT";
            SkillItems[2].text = "WARRIOR";
            SkillItems[3].text = "BERSERKER";
            SkillItems[4].text = "TITAN";
            break;
        case PCLASS_CLERIC:
            SkillMenu.x = 116;
            SkillItems[0].text = "ALTAR BOY";
            SkillItems[1].text = "ACOLYTE";
            SkillItems[2].text = "PRIEST";
            SkillItems[3].text = "CARDINAL";
            SkillItems[4].text = "POPE";
            break;
        case PCLASS_MAGE:
            SkillMenu.x = 112;
            SkillItems[0].text = "APPRENTICE";
            SkillItems[1].text = "ENCHANTER";
            SkillItems[2].text = "SORCERER";
            SkillItems[3].text = "WARLOCK";
            SkillItems[4].text = "ARCHIMAGE";
            break;
        case PCLASS_RANDOM:
            SkillMenu.x = 38;
            SkillItems[0].text = "THOU NEEDETH A WET-NURSE";
            SkillItems[1].text = "YELLOWBELLIES-R-US";
            SkillItems[2].text = "BRINGEST THEM ONETH";
            SkillItems[3].text = "THOU ART A SMITE-MEISTER";
            SkillItems[4].text = "BLACK PLAGUE POSSESSES THEE";
            break;
    }
    SetMenu(MENU_SKILL);
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static void SCSkill(int option)
{
    if (demoplayback)
    {
        // deactivate playback, return control to player
        demoextend = false;
    }

    if (MenuPClass < 3)
    {
        PlayerClass[consoleplayer] = MenuPClass;
    }
    else
    {
        PlayerClass[consoleplayer] = ID_RealRandom () % 3;
    }
    G_DeferredNewGame(option);
    SB_SetClassData();
    SB_state = -1;
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

//---------------------------------------------------------------------------
//
// PROC SCSfxVolume
//
//---------------------------------------------------------------------------

static void SCSfxVolume(int option)
{
    snd_MaxVolume = M_INT_Slider(snd_MaxVolume, 0, 15, option, true);
    S_SetSfxVolume(snd_MaxVolume);
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static void SCMusicVolume(int option)
{
    snd_MusicVolume = M_INT_Slider(snd_MusicVolume, 0, 15, option, true);
    S_SetMusicVolume(snd_MusicVolume);
}

//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static void SCScreenSize(int option)
{
    switch (option)
    {
        case LEFT_DIR:
        if (dp_screen_size > 3)
        {
            dp_screen_size--;
            // [JN] Skip wide status bar in non-wide screen mode.
            if (!vid_widescreen)
            {
                if (dp_screen_size == 11)
                    dp_screen_size  = 10;
            }
            R_SetViewSize(dp_screen_size, dp_detail_level);
        }
        break;

        case RIGHT_DIR:
        if (dp_screen_size < 13)
        {
            dp_screen_size++;
            // [JN] Skip wide status bar in non-wide screen mode.
            if (!vid_widescreen)
            {
                if (dp_screen_size == 11)
                    dp_screen_size  = 12;
            }
            R_SetViewSize(dp_screen_size, dp_detail_level);
        }
        break;
    }
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static void SCInfo(int option)
{
    InfoType = 1;
    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
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
            S_StartSound(NULL, SFX_CHAT);
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
        // [JN] Allow menu control by mouse.
        if (event->type == ev_mouse && mousewait < I_GetTime())
        {
            // [crispy] mouse_novert disables controlling the menus with the mouse
            if (!mouse_novert)
            {
                mousey += event->data3;
            }

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
            if (MouseIsBinding && event->data1)
            {
                M_CheckMouseBind(SDL_mouseButton);
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 15;
                return true;
            }

            if (event->data1 & 1)
            {
                key = key_menu_forward;
                mousewait = I_GetTime() + 5;
            }

            if (event->data1 & 2)
            {
                key = key_menu_back;
                mousewait = I_GetTime() + 5;
            }

            // [crispy] scroll menus with mouse wheel
            // [JN] Buttons hardcoded to wheel so we won't mix it up with inventory scrolling.
            if (/*mousebprevweapon >= 0 &&*/ event->data1 & (1 << 4/*mousebprevweapon*/))
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 1;
            }
            else
            if (/*mousebnextweapon >= 0 &&*/ event->data1 & (1 << 3/*mousebnextweapon*/))
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 1;
            }
        }
        else
        {
            if (event->type == ev_keydown)
            {
                key = event->data1;
                charTyped = event->data2;
            }
        }
    }

    if (key == -1)
    {
        return false;
    }

    if (InfoType)
    {
        /* The 4-Level Demo Version also has 3 Info pages
        if (gamemode == shareware)
        {
            InfoType = (InfoType + 1) % 5;
        }
        else
        */
        {
            InfoType = (InfoType + 1) % 4;
        }
        if (key == KEY_ESCAPE)
        {
            InfoType = 0;
        }
        if (!InfoType)
        {
            if (!netgame && !demoplayback)
            {
                paused = false;
            }
            MN_DeactivateMenu();
            SB_state = -1;      //refresh the statbar
        }
        S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
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
        S_StartSound(NULL, SFX_PICKUP_ITEM);
        return (true);
    }

    if (askforquit)
    {
        if (key == key_menu_confirm
        || (event->type == ev_mouse && event->data1 & 1))  // [JN] Confirm by left mouse button.
        {
            switch (typeofask)
            {
                case 1:
                    G_CheckDemoStatus();
                    I_Quit();
                    return false;
                case 2:
                    CT_ClearMessage(&players[consoleplayer]);
                    askforquit = false;
                    typeofask = 0;
                    paused = false;
#ifndef CRISPY_TRUECOLOR
                    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
#else
                    I_SetPalette(0);
#endif
                    H2_StartTitle();    // go to intro/demo mode.
                    return false;
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
                    mn_SuicideConsole = true;
                    break;
                case 6:
                    SCDeleteGame(CurrentItPos);
                    MN_ReturnToMenu();
                case 7: // [JN] Reset keybinds.
                    M_ResetBinds();
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    MN_ReturnToMenu();
                    break;

                case 8: // [JN] Reset mouse binds.
                    M_ResetMouseBinds();
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    MN_ReturnToMenu();
                    break;

                case 9: // [JN] Setting reset.
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
        else
        if (key == key_menu_abort || key == KEY_ESCAPE
        || (event->type == ev_mouse && event->data1 & 2))  // [JN] Cancel by right mouse button.
        {
            // [JN] Do not close reset menus after canceling.
            if (typeofask == 7 || typeofask == 8 || typeofask == 9)
            {
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                MenuActive = true;
                askforquit = false;
                typeofask = 0;
            }
            else
            {
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
            S_StartSound(NULL, SFX_PICKUP_KEY);
            return (true);
        }
        else if (key == key_menu_incscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(RIGHT_DIR);
            S_StartSound(NULL, SFX_PICKUP_KEY);
            return (true);
        }
        else if (key == key_menu_help)           // F1 (help screen)
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
                S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
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
                S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
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
            S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
            slottextloaded = false; //reload the slot text, when needed
            return true;
        }
        else if (key == key_menu_detail)         // F5 (suicide)
        {
            MenuActive = false;
            askforquit = true;
            typeofask = 5;  // suicide
            return true;
        }
        else if (key == key_menu_qsave)          // F6 (quicksave)
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
                    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
                    slottextloaded = false; //reload the slot text
                    quicksave = -1;
                    // [JN] "Quick save game" title instead of message.
                    quicksaveTitle = true;
                }
                else
                {
                    // [JN] Do not ask for quick save confirmation.
                    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
                    FileMenuKeySteal = true;
                    SCSaveGame(quicksave - 1);
                }
            }
            return true;
        }
        else if (key == key_menu_endgame)        // F7 (end game)
        {
            if (SCNetCheck(3))
            {
                if (gamestate == GS_LEVEL && !demoplayback)
                {
                    S_StartSound(NULL, SFX_CHAT);
                    SCEndGame(0);
                }
            }
            return true;
        }
        else if (key == key_menu_messages)       // F8 (toggle messages)
        {
            SCMessages(0);
            return true;
        }
        else if (key == key_menu_qload)          // F9 (quickload)
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
                    S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
                    slottextloaded = false; // reload the slot text
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
        else if (key == key_menu_quit)           // F10 (quit)
        {
            if (gamestate == GS_LEVEL || gamestate == GS_FINALE)
            {
                SCQuitGame(0);
                S_StartSound(NULL, SFX_CHAT);
            }
            return true;
        }
        else if (key == KEY_F12)                 // F12 (???)
        {
            // F12 - reload current map (devmaps mode)

            if (netgame)
            {
                return false;
            }
            if (gamekeydown[key_speed])
            {               // Monsters ON
                nomonsters = false;
            }
            if (gamekeydown[key_strafe])
            {               // Monsters OFF
                nomonsters = true;
            }
            G_DeferedInitNew(gameskill, gameepisode, gamemap);
            CT_SetMessage(&players[consoleplayer], TXT_CHEATWARP, false, NULL);
            return true;
        }
    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma)          // F11 (gamma correction)
    {
        vid_gamma = M_INT_Slider(vid_gamma, 0, 14, 1 /*right*/, false);
        CT_SetMessage(&players[consoleplayer], gammalvls[vid_gamma][0], false, NULL);
        SB_PaletteFlash(true);  // force change
#ifdef CRISPY_TRUECOLOR
        R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");
        R_FillBackScreen();
        SB_state = -1;
#endif
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

        if (key == key_menu_down)                // Next menu item
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
            S_StartSound(NULL, SFX_FIGHTER_HAMMER_HITWALL);
            return (true);
        }
        else if (key == key_menu_up)             // Previous menu item
        {
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
            S_StartSound(NULL, SFX_FIGHTER_HAMMER_HITWALL);
            return (true);
        }
        else if (key == key_menu_left)           // Slider left
        {
            if (item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(LEFT_DIR);
                S_StartSound(NULL, SFX_PICKUP_KEY);
            }
            // [JN] Go to previous-left menu by pressing Left Arrow.
            if (CurrentMenu->ScrollAR)
            {
                M_ScrollPages(false);
            }
            return (true);
        }
        else if (key == key_menu_right)          // Slider right
        {
            if (item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(RIGHT_DIR);
                S_StartSound(NULL, SFX_PICKUP_KEY);
            }
            // [JN] Go to next-right menu by pressing Right Arrow.
            if (CurrentMenu->ScrollAR)
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
        else if (key == key_menu_forward)        // Activate item (enter)
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
                if (item->type == ITT_LRFUNC)
                {
                    item->func(RIGHT_DIR);
                }
                else if (item->type == ITT_EFUNC)
                {
                    item->func(item->option);
                }
            }
            S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
            return (true);
        }
        else if (key == key_menu_activate)
        {
            MN_DeactivateMenu();
            return (true);
        }
        else if (key == key_menu_back)
        {
            S_StartSound(NULL, SFX_PICKUP_KEY);

            if (CurrentMenu->prevMenu == MENU_NONE)
            {
                MN_DeactivateMenu();
            }
            else
            {
                SetMenu(CurrentMenu->prevMenu);
            }
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
                    typeofask = 6;
                    S_StartSound(NULL, SFX_CHAT);
                }
            }
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
        else if (charTyped != 0)
        {
            for (i = 0; i < CurrentMenu->itemCount; i++)
            {
                if (CurrentMenu->items[i].text)
                {
                    if (toupper(charTyped)
                        == toupper(CurrentMenu->items[i].text[0]))
                    {
                        CurrentItPos = i;
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
                *textBuffer = 0;
                slotptr--;
                textBuffer = &SlotText[currentSlot][slotptr];
                *textBuffer = ASCII_CURSOR;
            }
            return (true);
        }
        if (key == KEY_ESCAPE)
        {
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
    CurrentItPos = CurrentMenu->oldItPos;
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    S_StartSound(NULL, SFX_PLATFORM_STOP);
    slottextloaded = false;     //reload the slot text, when needed
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
    S_StartSound(NULL, SFX_PLATFORM_STOP);
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawInfo
//
//---------------------------------------------------------------------------

void MN_DrawInfo(void)
{
    lumpindex_t lumpindex; // [crispy]

#ifndef CRISPY_TRUECOLOR
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
#else
    I_SetPalette(0);
#endif

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
    CurrentItPos = CurrentMenu->oldItPos;
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
    if (itemPos == CurrentItPos)
    {
        dp_translation = cr[CR_MENU_BRIGHT2];
    }

    V_DrawShadowedPatchOptional(x - 32, y, 1, W_CacheLumpName("M_SLDLT", PU_CACHE));
    for (x2 = x, count = width; count--; x2 += 8)
    {
        V_DrawShadowedPatchOptional(x2, y, 1, W_CacheLumpName(count & 1 ? "M_SLDMD1"
                                           : "M_SLDMD2", PU_CACHE));
    }
    V_DrawShadowedPatchOptional(x2, y, 1, W_CacheLumpName("M_SLDRT", PU_CACHE));

    // [JN] Prevent gem go out of slider bounds.
    if (slot > width - 1)
    {
        slot = width - 1;
    }

    V_DrawPatch(x + 4 + slot * 8, y + 7,
                W_CacheLumpName("M_SLDKB", PU_CACHE));

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
    if (key_jump == key)             key_jump             = 0;
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
    if (key_reloadlevel == key)      key_reloadlevel      = 0;
    if (key_nextlevel == key)        key_nextlevel        = 0;
    if (key_demospeed == key)        key_demospeed        = 0;
    if (key_flip_levels == key)      key_flip_levels      = 0;
    if (key_spectator == key)        key_spectator        = 0;
    if (key_freeze == key)           key_freeze           = 0;
    if (key_notarget == key)         key_notarget         = 0;
    if (key_buddha == key)           key_buddha           = 0;

    // Page 4
    if (key_weapon1 == key)          key_weapon1          = 0;
    if (key_weapon2 == key)          key_weapon2          = 0;
    if (key_weapon3 == key)          key_weapon3          = 0;
    if (key_weapon4 == key)          key_weapon4          = 0;
    if (key_prevweapon == key)       key_prevweapon       = 0;
    if (key_nextweapon == key)       key_nextweapon       = 0;
    if (key_arti_health == key)      key_arti_health      = 0;
    if (key_arti_urn == key)         key_arti_urn         = 0;
    if (key_arti_poisonbag == key)   key_arti_poisonbag   = 0;
    if (key_arti_blastradius == key) key_arti_blastradius = 0;
    if (key_arti_invulnerability == key) key_arti_invulnerability = 0;

    // Page 5
    if (key_arti_egg == key)           key_arti_egg           = 0;
    if (key_arti_teleport == key)      key_arti_teleport      = 0;
    if (key_arti_teleportother == key) key_arti_teleportother = 0;
    if (key_arti_wings == key)         key_arti_wings         = 0;
    if (key_arti_servant == key)       key_arti_servant       = 0;
    if (key_arti_bracers == key)       key_arti_bracers       = 0;
    if (key_arti_boots == key)         key_arti_boots         = 0;
    if (key_arti_torch == key)         key_arti_torch         = 0;
    if (key_arti_krater == key)        key_arti_krater        = 0;
    if (key_arti_incant == key)        key_arti_incant        = 0;
    if (key_arti_all == key)           key_arti_all           = 0;

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
        case 108:  key_jump = key;              break;
        case 109:  key_180turn = key;           break;
        case 110:  key_fire = key;              break;
        case 111:  key_use = key;               break;

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
        case 302:  key_reloadlevel = key;       break;
        case 303:  key_nextlevel = key;         break;
        case 304:  key_demospeed = key;         break;
        case 305:  key_flip_levels = key;       break;
        case 306:  key_spectator = key;         break;
        case 307:  key_freeze = key;            break;
        case 308:  key_notarget = key;          break;
        case 309:  key_buddha = key;            break;

        // Page 4
        case 400:  key_weapon1 = key;           break;
        case 401:  key_weapon2 = key;           break;
        case 402:  key_weapon3 = key;           break;
        case 403:  key_weapon4 = key;           break;
        case 404:  key_prevweapon = key;        break;
        case 405:  key_nextweapon = key;        break;
        case 406:  key_arti_health = key;       break;
        case 407:  key_arti_urn = key;          break;
        case 408:  key_arti_poisonbag = key;    break;
        case 409:  key_arti_blastradius = key;  break;
        case 410:  key_arti_invulnerability = key; break;

        // Page 5
        case 500:  key_arti_egg = key;          break;
        case 501:  key_arti_teleport = key;     break;
        case 502:  key_arti_teleportother = key; break;
        case 503:  key_arti_wings = key;        break;
        case 504:  key_arti_servant = key;      break;
        case 505:  key_arti_bracers = key;      break;
        case 506:  key_arti_boots = key;        break;
        case 507:  key_arti_torch = key;        break;
        case 508:  key_arti_krater = key;       break;
        case 509:  key_arti_incant = key;       break;
        case 510:  key_arti_all = key;          break;

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
            case 8:   key_jump = 0;             break;
            case 9:   key_180turn = 0;          break;
            // Action title
            case 11:  key_fire = 0;             break;
            case 12:  key_use = 0;              break;
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
            // Special keys title
            case 3:   key_reloadlevel = 0;      break;
            case 4:   key_nextlevel = 0;        break;
            case 5:   key_demospeed = 0;        break;
            case 6:   key_flip_levels = 0;      break;
            // Special modes title
            case 8:   key_spectator = 0;        break;
            case 9:   key_freeze = 0;           break;
            case 10:  key_notarget = 0;         break;
            case 11:  key_notarget = 0;         break;
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
            case 4:   key_prevweapon = 0;       break;
            case 5:   key_nextweapon = 0;       break;
            
            case 7:   key_arti_health = 0;      break;
            case 8:   key_arti_urn = 0;         break;
            case 9:   key_arti_poisonbag = 0;   break;
            case 10:  key_arti_blastradius = 0; break;
            case 11:  key_arti_invulnerability = 0; break;
        }
    }
    if (CurrentMenu == &ID_Def_Keybinds_5)
    {
        switch (CurrentItPos)
        {
            case 0:   key_arti_egg = 0;         break;
            case 1:   key_arti_teleport = 0;    break;
            case 2:   key_arti_teleportother = 0; break;
            case 3:   key_arti_wings = 0;       break;
            case 4:   key_arti_servant = 0;     break;
            case 5:   key_arti_bracers = 0;     break;
            case 6:   key_arti_boots = 0;       break;
            case 7:   key_arti_torch = 0;       break;
            case 8:   key_arti_krater = 0;      break;
            case 9:   key_arti_incant = 0;      break;
            case 10:  key_arti_all = 0;         break;
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
    key_jump = '/';
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
    key_reloadlevel = 0;
    key_nextlevel = 0;
    key_demospeed = 0;
    key_flip_levels = 0;
    key_spectator = 0;
    key_freeze = 0;
    key_notarget = 0;
    key_buddha = 0;

    // Page 4
    key_weapon1 = '1';
    key_weapon2 = '2';
    key_weapon3 = '3';
    key_weapon4 = '4';
    key_prevweapon = 0;
    key_nextweapon = 0;
    key_arti_health = '\\';
    key_arti_urn = 0;
    key_arti_poisonbag = '0';
    key_arti_blastradius = '9';
    key_arti_invulnerability = '5';

    // Page 5
    key_arti_egg = '6';
    key_arti_teleport = '8';
    key_arti_teleportother = '7';
    key_arti_wings = 0;
    key_arti_servant = 0;
    key_arti_bracers = 0;
    key_arti_boots = 0;
    key_arti_torch = 0;
    key_arti_krater = 0;
    key_arti_incant = 0;
    key_arti_all = KEY_BACKSPACE;

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
// M_ColorizeBind
//  [JN] Do key bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeBind (int CurrentItPosOn, int key)
{
    if (CurrentItPos == CurrentItPosOn && KbdIsBinding)
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
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5_HX] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4_HX] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3_HX] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2_HX] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1_HX] : cr[CR_GREEN_HX];
        }
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindKey
//  [JN] Do keyboard bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindKey (int itemNum, int yPos, int keyBind)
{
    MN_DrTextA(M_NameBind(itemNum, keyBind),
               M_ItemRightAlign(M_NameBind(itemNum, keyBind)),
               yPos,
               M_ColorizeBind(itemNum, keyBind));
}

// -----------------------------------------------------------------------------
// M_DrawBindFooter
//  [JN] Draw footer in key binding pages with numeration.
// -----------------------------------------------------------------------------

static void M_DrawBindFooter (char *pagenum, boolean drawPages)
{
    MN_DrTextACentered("PRESS ENTER TO BIND, DEL TO CLEAR", 170, cr[CR_GRAY]);
    
    if (drawPages)
    {
        MN_DrTextA("PGUP", ID_MENU_LEFTOFFSET, 180, cr[CR_GRAY]);
        MN_DrTextACentered(M_StringJoin("PAGE ", pagenum, "/8", NULL), 180, cr[CR_GRAY]);
        MN_DrTextA("PGDN", M_ItemRightAlign("PGDN"), 180, cr[CR_GRAY]);
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
        switch (btn)
        {
            case -1:  return  "---";            break;  // Means empty
            case  0:  return  "LEFT BUTTON";    break;
            case  1:  return  "RIGHT BUTTON";   break;
            case  2:  return  "MIDDLE BUTTON";  break;
            case  3:  return  "BUTTON #4";      break;
            case  4:  return  "BUTTON #5";      break;
            case  5:  return  "BUTTON #6";      break;
            case  6:  return  "BUTTON #7";      break;
            case  7:  return  "BUTTON #8";      break;
            case  8:  return  "BUTTON #9";      break;
            default:  return  "UNKNOWN";        break;
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
    if (mousebstrafe == btn)      mousebstrafe      = -1;
    if (mousebbackward == btn)    mousebbackward    = -1;
    if (mousebuse == btn)         mousebuse         = -1;
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
        case 1002:  mousebstrafe = btn;       break;
        case 1003:  mousebbackward = btn;     break;
        case 1004:  mousebuse = btn;          break;
        case 1005:  mousebstrafeleft = btn;   break;
        case 1006:  mousebstraferight = btn;  break;
        case 1007:  mousebprevweapon = btn;   break;
        case 1008:  mousebnextweapon = btn;   break;
        case 1009:  mousebinvleft = btn;      break;
        case 1010:  mousebinvright = btn;     break;
        case 1011:  mousebuseartifact = btn;  break;
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
        case 2:   mousebstrafe = -1;       break;
        case 3:   mousebbackward = -1;     break;
        case 4:   mousebuse = -1;          break;
        case 5:   mousebstrafeleft = -1;   break;
        case 6:   mousebstraferight = -1;  break;
        case 7:   mousebprevweapon = -1;   break;
        case 8:   mousebnextweapon = -1;   break;
        case 9:   mousebinvleft = -1;      break;
        case 10:  mousebinvright = -1;     break;
        case 11:  mousebuseartifact = -1;  break;
    }
}

// -----------------------------------------------------------------------------
// M_ColorizeMouseBind
//  [JN] Do mouse bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeMouseBind (int CurrentItPosOn, int btn)
{
    if (CurrentItPos == CurrentItPosOn && MouseIsBinding)
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
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5_HX] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4_HX] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3_HX] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2_HX] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1_HX] : cr[CR_GREEN_HX];
        }
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindButton
//  [JN] Do mouse button bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindButton (int itemNum, int yPos, int btnBind)
{
    MN_DrTextA(M_NameMouseBind(itemNum, btnBind),
               M_ItemRightAlign(M_NameMouseBind(itemNum, btnBind)),
               yPos,
               M_ColorizeMouseBind(itemNum, btnBind));
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all mouse binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    mousebfire = 0;
    mousebforward = 2;
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
