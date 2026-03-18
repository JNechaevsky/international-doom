//
// Copyright(C) 2026 Julia Nechaevskaya
// Copyright(C) 2026 Polina "Aura" N.
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
//     IWAD launcher dialog.
//

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "d_launcher.h"
#include "d_iwad.h"
#include "id_vars.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"

#if defined(_WIN32) && !defined(_WIN32_WCE)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

static void D_AppendCommandLineArgument(const char *arg)
{
    char **newargv = realloc(myargv, (myargc + 1) * sizeof(*newargv));
    if (newargv == NULL)
    {
        I_Error("Failed to allocate memory for command-line arguments.");
    }

    myargv = newargv;
    myargv[myargc] = M_StringDuplicate(arg);
    ++myargc;
}

static void D_AppendAdditionalCommandLine(const char *params)
{
    if (params == NULL)
    {
        return;
    }

    const char *p = params;

    while (*p != '\0')
    {
        const char *start;
        const char *end;
        char quote = '\0';
        char *arg;
        size_t len;

        while (*p != '\0' && isspace((unsigned char) *p))
        {
            ++p;
        }

        if (*p == '\0')
        {
            break;
        }

        if (*p == '"' || *p == '\'')
        {
            quote = *p;
            ++p;
            start = p;

            while (*p != '\0' && *p != quote)
            {
                ++p;
            }

            end = p;

            if (*p == quote)
            {
                ++p;
            }
        }
        else
        {
            start = p;

            while (*p != '\0' && !isspace((unsigned char) *p))
            {
                ++p;
            }

            end = p;
        }

        len = end - start;

        if (len == 0)
        {
            continue;
        }

        arg = malloc(len + 1);
        if (arg == NULL)
        {
            I_Error("Failed to allocate memory for command-line arguments.");
        }

        memcpy(arg, start, len);
        arg[len] = '\0';

        D_AppendCommandLineArgument(arg);
        free(arg);
    }
}

// -----------------------------------------------------------------------------
// LauncherArgNeedsQuotes
//  [PN] Check whether an argument needs quotes when shown in the launcher.
// -----------------------------------------------------------------------------

static boolean LauncherArgNeedsQuotes(const char *arg)
{
    const unsigned char *p = (const unsigned char *) arg;

    while (*p != '\0')
    {
        if (isspace(*p) || *p == '"')
        {
            return true;
        }

        ++p;
    }

    return false;
}

// -----------------------------------------------------------------------------
// NormalizePathForCompare
//  [PN] Normalize separators and trailing slashes for stable path comparison.
// -----------------------------------------------------------------------------

static void NormalizePathForCompare(char *path)
{
    size_t len = strlen(path);

    for (size_t i = 0; i < len; ++i)
    {
        if (path[i] == '/')
        {
            path[i] = '\\';
        }
    }

    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/'))
    {
        // Keep "C:\" roots intact.
        if (len == 3 && path[1] == ':')
        {
            break;
        }

        path[--len] = '\0';
    }
}

// -----------------------------------------------------------------------------
// IsPathInDirectory
//  [PN] Compare parent directory of a file path against a target directory.
// -----------------------------------------------------------------------------

static boolean IsPathInDirectory(const char *path, const char *dir)
{
    char *path_dir = M_DirName(path);
    char *dir_copy = M_StringDuplicate(dir);
    boolean result = false;

    if (path_dir == NULL || dir_copy == NULL)
    {
        free(path_dir);
        free(dir_copy);
        return false;
    }

    NormalizePathForCompare(path_dir);
    NormalizePathForCompare(dir_copy);
    result = !strcasecmp(path_dir, dir_copy);

    free(path_dir);
    free(dir_copy);

    return result;
}

// -----------------------------------------------------------------------------
// AppendText
//  [PN] Append text to a growable string buffer.
// -----------------------------------------------------------------------------

static void AppendText(char **buffer, size_t *length, size_t *capacity,
                       const char *text)
{
    size_t add = strlen(text);
    size_t needed = *length + add + 1;

    if (needed > *capacity)
    {
        size_t new_capacity = *capacity;

        while (needed > new_capacity)
        {
            new_capacity *= 2;
        }

        char *new_buffer = realloc(*buffer, new_capacity);
        if (new_buffer == NULL)
        {
            I_Error("Failed to allocate memory for launcher parameters.");
        }

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, text, add);
    *length += add;
    (*buffer)[*length] = '\0';
}

// -----------------------------------------------------------------------------
// BuildLauncherParamsFromLooseFiles
//  [PN] Build initial launcher parameters from loose-file drag-n-drop args.
// -----------------------------------------------------------------------------

static char *BuildLauncherParamsFromLooseFiles(void)
{
    size_t capacity = 64;
    size_t length = 0;
    char *result = malloc(capacity);
    char *exe_dir = M_DirName(myargv[0]);

    if (result == NULL)
    {
        I_Error("Failed to allocate memory for launcher parameters.");
    }

    if (exe_dir != NULL)
    {
        NormalizePathForCompare(exe_dir);
    }

    result[0] = '\0';

    for (int i = 1; i < myargc; ++i)
    {
        const char *arg = myargv[i];
        const char *display_arg = arg;

        if (!strcasecmp(arg, "-merge"))
        {
            display_arg = "-file";
        }
        else if (arg[0] != '-' && exe_dir != NULL && IsPathInDirectory(arg, exe_dir))
        {
            display_arg = M_BaseName(arg);
        }

        if (length > 0)
        {
            AppendText(&result, &length, &capacity, " ");
        }

        if (LauncherArgNeedsQuotes(display_arg))
        {
            AppendText(&result, &length, &capacity, "\"");
            AppendText(&result, &length, &capacity, display_arg);
            AppendText(&result, &length, &capacity, "\"");
        }
        else
        {
            AppendText(&result, &length, &capacity, display_arg);
        }
    }

    free(exe_dir);

    return result;
}

// -----------------------------------------------------------------------------
// ClearCommandLineExceptExecutable
//  [PN] Remove loose-file args so selected launcher options are authoritative.
// -----------------------------------------------------------------------------

static void ClearCommandLineExceptExecutable(void)
{
    for (int i = 1; i < myargc; ++i)
    {
        free(myargv[i]);
        myargv[i] = NULL;
    }

    myargc = 1;
}

#define IWAD_LAUNCHER_CLASS_NAME "InterIwadLauncherWindow"
#define IWAD_LAUNCHER_PROMPT     "Select IWAD file to run:"
#define IWAD_LAUNCHER_SETTINGS_PROMPT "Launcher settings"
#define IDC_IWAD_LAUNCHER_LIST   2001
#define IDC_IWAD_LAUNCHER_EDIT   2002
#define IDC_IWAD_LAUNCHER_PLAY   2003
#define IDC_IWAD_LAUNCHER_EXIT   2004
#define IDC_IWAD_LAUNCHER_TOGGLE 2005
#define IDC_IWAD_LAUNCHER_CLEAR  2006
#define IDC_IWAD_SETTINGS_VIDEO_LABEL      2101
#define IDC_IWAD_SETTINGS_FULLSCREEN       2102
#define IDC_IWAD_SETTINGS_SOFTWARE_RENDER  2103
#define IDC_IWAD_SETTINGS_AUTOLOAD_LABEL   2104
#define IDC_IWAD_SETTINGS_AUTOLOAD_WAD     2105
#define IDC_IWAD_SETTINGS_AUTOLOAD_DEH     2106
#define IDC_IWAD_SETTINGS_LAUNCHER_LABEL   2107
#define IDC_IWAD_SETTINGS_SHOW_STARTUP     2108
#define IWAD_LAUNCHER_OLDPROC    "InterLauncherOldProc"
#define IWAD_LAUNCHER_BTN_OLDPROC "InterLauncherBtnOldProc"
#define IWAD_LAUNCHER_BTN_HOVER   "InterLauncherBtnHover"
#define IWAD_LAUNCHER_TOGGLE_GLYPH_SETTINGS L"\xE713"
#define IWAD_LAUNCHER_TOGGLE_GLYPH_BACK     L"\xE72B"
#define IWAD_LAUNCHER_CLEAR_GLYPH           L"\xE711"
#define IWAD_LAUNCHER_TOGGLE_SETTINGS "S"
#define IWAD_LAUNCHER_TOGGLE_BACK     "<"
#define IWAD_LAUNCHER_PLAY_CAPTION    "Play"
#define IWAD_LAUNCHER_CLEAR_CAPTION   "X"
#define IWAD_WINDOW_CLIENT_W     340
#define IWAD_WINDOW_CLIENT_H     480
#define IWAD_BUTTON_IDEAL_W      120
#define IWAD_BUTTON_H            30
#define IWAD_COLOR_WINDOW_BG     RGB(43, 43, 43)
#define IWAD_COLOR_CONTROL_BG    RGB(30, 30, 30)
#define IWAD_COLOR_LIST_SEL_BG   RGB(0, 90, 165)
#define IWAD_COLOR_BUTTON_BG     RGB(58, 58, 58)
#define IWAD_COLOR_BUTTON_HOVER  RGB(78, 78, 78)
#define IWAD_COLOR_BUTTON_DOWN   RGB(98, 98, 98)
#define IWAD_COLOR_BUTTON_BORDER RGB(96, 96, 96)
#define IWAD_COLOR_TEXT          RGB(235, 235, 235)
#define IWAD_COLOR_CHECK_BG      RGB(42, 42, 42)
#define IWAD_COLOR_CHECK_HOVER   RGB(58, 58, 58)
#define IWAD_COLOR_CHECK_BORDER  RGB(122, 122, 122)
#define IWAD_COLOR_CHECK_ACCENT  RGB(0, 122, 204)
#define IWAD_COLOR_CHECK_ACCENT_HOVER RGB(20, 142, 224)

typedef enum
{
    LAUNCHER_VIEW_IWAD = 0,
    LAUNCHER_VIEW_SETTINGS = 1
} launcher_view_mode_t;

typedef struct
{
    HWND window;
    HWND prompt_label;
    HWND toggle_button;
    HWND iwad_list;
    HWND params_label;
    HWND params_edit;
    HWND clear_button;
    HWND clear_tooltip;
    HWND settings_video_label;
    HWND settings_fullscreen;
    HWND settings_software_renderer;
    HWND settings_autoload_label;
    HWND settings_autoload_wad;
    HWND settings_autoload_deh;
    HWND settings_launcher_label;
    HWND settings_show_startup;
    int settings_fullscreen_checked;
    int settings_software_renderer_checked;
    int settings_autoload_wad_checked;
    int settings_autoload_deh_checked;
    int settings_show_startup_checked;
    int initial_fullscreen_checkbox;
    int initial_software_renderer_checkbox;
    int initial_autoload_wad_checkbox;
    int initial_autoload_deh_checkbox;
    int initial_show_startup_checkbox;
    iwad_search_result_t *iwads;
    int dpi;
    int selected_iwad;
    char *additional_params;
    HFONT ui_font;
    boolean owns_ui_font;
    HFONT toggle_icon_font;
    boolean owns_toggle_icon_font;
    boolean toggle_use_icon_font;
    HFONT clear_icon_font;
    boolean owns_clear_icon_font;
    int list_item_height;
    HBRUSH window_brush;
    HBRUSH control_brush;
    HBRUSH list_sel_brush;
    HBRUSH button_brush;
    HBRUSH button_hover_brush;
    HBRUSH button_down_brush;
    HBRUSH button_border_brush;
    launcher_view_mode_t view_mode;
    boolean play_pressed;
    boolean done;
} iwad_launcher_t;

typedef HRESULT (WINAPI *dwm_set_window_attribute_t)(HWND, DWORD,
                                                      LPCVOID, DWORD);
typedef HRESULT (WINAPI *set_window_theme_t)(HWND, LPCWSTR, LPCWSTR);

static int ScaleByDPI(int value, int dpi);

static void CleanupLauncherFont(iwad_launcher_t *launcher)
{
    if (launcher == NULL)
    {
        return;
    }

    if (launcher->owns_ui_font && launcher->ui_font != NULL)
    {
        DeleteObject(launcher->ui_font);
    }

    launcher->ui_font = NULL;
    launcher->owns_ui_font = false;

    if (launcher->owns_toggle_icon_font && launcher->toggle_icon_font != NULL)
    {
        DeleteObject(launcher->toggle_icon_font);
    }

    launcher->toggle_icon_font = NULL;
    launcher->owns_toggle_icon_font = false;
    launcher->toggle_use_icon_font = false;

    if (launcher->owns_clear_icon_font && launcher->clear_icon_font != NULL)
    {
        DeleteObject(launcher->clear_icon_font);
    }

    launcher->clear_icon_font = NULL;
    launcher->owns_clear_icon_font = false;
}

static HFONT CreateLauncherUIFont(int dpi, boolean *owns_font)
{
    NONCLIENTMETRICSA ncm;
    HFONT font = NULL;

    memset(&ncm, 0, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);

    if (!SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    {
        // XP-compatible fallback for headers with iPaddedBorderWidth.
        ncm.cbSize = sizeof(ncm) - sizeof(int);
        SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    }

    font = CreateFontIndirectA(&ncm.lfMessageFont);
    if (font != NULL)
    {
        *owns_font = true;
        return font;
    }

    // Last resort: scaled GUI font with standard UI face.
    font = CreateFontA(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (font != NULL)
    {
        *owns_font = true;
        return font;
    }

    *owns_font = false;
    return (HFONT) GetStockObject(DEFAULT_GUI_FONT);
}

static int MeasureFontHeight(HFONT font)
{
    int height = 16;
    HDC dc = GetDC(NULL);

    if (dc != NULL)
    {
        HFONT old_font = (HFONT) SelectObject(dc, font);
        TEXTMETRICA tm;

        if (GetTextMetricsA(dc, &tm))
        {
            height = tm.tmHeight + tm.tmExternalLeading;
        }

        SelectObject(dc, old_font);
        ReleaseDC(NULL, dc);
    }

    return height;
}

// -----------------------------------------------------------------------------
// CreateLauncherToggleIconFont
//  [PN] Create the icon font used for launcher view-toggle glyphs.
// -----------------------------------------------------------------------------

static HFONT CreateLauncherToggleIconFont(int dpi, boolean *owns_font)
{
    HFONT font = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE,
                             FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");

    if (font != NULL)
    {
        *owns_font = true;
        return font;
    }

    *owns_font = false;
    return NULL;
}

// -----------------------------------------------------------------------------
// CreateLauncherClearIconFont
//  [PN] Create a smaller icon font for the command-line clear button glyph.
// -----------------------------------------------------------------------------

static HFONT CreateLauncherClearIconFont(int dpi, boolean *owns_font)
{
    HFONT font = CreateFontW(-MulDiv(8, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE,
                             FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");

    if (font != NULL)
    {
        *owns_font = true;
        return font;
    }

    *owns_font = false;
    return NULL;
}

// -----------------------------------------------------------------------------
// FontFaceEqualsW
//  [PN] Verify that created font resolves to the expected face name.
// -----------------------------------------------------------------------------

static boolean FontFaceEqualsW(HFONT font, const wchar_t *expected_face)
{
    boolean matched = false;
    HDC dc = GetDC(NULL);

    if (dc == NULL || font == NULL || expected_face == NULL)
    {
        if (dc != NULL)
        {
            ReleaseDC(NULL, dc);
        }
        return false;
    }

    HFONT old_font = (HFONT) SelectObject(dc, font);
    wchar_t face[LF_FACESIZE];

    face[0] = L'\0';

    if (GetTextFaceW(dc, LF_FACESIZE, face) > 0)
    {
        matched = _wcsicmp(face, expected_face) == 0;
    }

    SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);

    return matched;
}

static void ApplyFontToControl(HWND control, HFONT font)
{
    if (control != NULL && font != NULL)
    {
        SendMessageA(control, WM_SETFONT, (WPARAM) font, TRUE);
    }
}

// -----------------------------------------------------------------------------
// CreateButtonTooltip
//  [PN] Attach a standard tooltip with static text to a button control.
// -----------------------------------------------------------------------------

static HWND CreateButtonTooltip(HWND parent, HWND button, const char *text)
{
    if (parent == NULL || button == NULL || text == NULL || text[0] == '\0')
    {
        return NULL;
    }

    HWND tooltip = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, NULL,
                                   WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   parent, NULL, GetModuleHandle(NULL), NULL);

    if (tooltip == NULL)
    {
        return NULL;
    }

    TOOLINFOA ti;
    memset(&ti, 0, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = parent;
    ti.uId = (UINT_PTR) button;
    ti.lpszText = (LPSTR) text;

    SendMessageA(tooltip, TTM_ADDTOOL, 0, (LPARAM) &ti);

    return tooltip;
}

static void DestroyLauncherBrush(HBRUSH *brush)
{
    if (*brush != NULL)
    {
        DeleteObject(*brush);
        *brush = NULL;
    }
}

static void CleanupLauncherTheme(iwad_launcher_t *launcher)
{
    if (launcher == NULL)
    {
        return;
    }

    DestroyLauncherBrush(&launcher->window_brush);
    DestroyLauncherBrush(&launcher->control_brush);
    DestroyLauncherBrush(&launcher->list_sel_brush);
    DestroyLauncherBrush(&launcher->button_brush);
    DestroyLauncherBrush(&launcher->button_hover_brush);
    DestroyLauncherBrush(&launcher->button_down_brush);
    DestroyLauncherBrush(&launcher->button_border_brush);
}

// -----------------------------------------------------------------------------
// TryEnableDarkTitleBar
//  [PN] Try enabling dark title bar on supported Windows versions.
// -----------------------------------------------------------------------------

static void TryEnableDarkTitleBar(HWND hwnd)
{
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
    if (dwmapi == NULL)
    {
        return;
    }

    dwm_set_window_attribute_t set_window_attribute =
        (dwm_set_window_attribute_t) GetProcAddress(dwmapi,
                                                    "DwmSetWindowAttribute");

    if (set_window_attribute != NULL)
    {
        BOOL enabled = TRUE;

        // Windows 10 1809
        set_window_attribute(hwnd, 19, &enabled, sizeof(enabled));
        // Windows 10 1903+
        set_window_attribute(hwnd, 20, &enabled, sizeof(enabled));
    }

    FreeLibrary(dwmapi);
}

// -----------------------------------------------------------------------------
// TryApplyDarkControlTheme
//  [PN] Request dark control theme (not guaranteed on every Windows build).
// -----------------------------------------------------------------------------

static void TryApplyDarkControlTheme(HWND hwnd)
{
    HMODULE uxtheme = LoadLibraryA("uxtheme.dll");
    if (uxtheme == NULL)
    {
        return;
    }

    set_window_theme_t set_window_theme =
        (set_window_theme_t) GetProcAddress(uxtheme, "SetWindowTheme");

    if (set_window_theme != NULL)
    {
        HRESULT hr = set_window_theme(hwnd, L"DarkMode_Explorer", NULL);

        if (FAILED(hr))
        {
            set_window_theme(hwnd, L"Explorer", NULL);
        }
    }

    FreeLibrary(uxtheme);
}

static void DrawLauncherButton(const DRAWITEMSTRUCT *dis,
                               const iwad_launcher_t *launcher)
{
    RECT rc = dis->rcItem;
    HBRUSH bg_brush = launcher->button_brush;
    HBRUSH border_brush = launcher->button_border_brush;

    if (bg_brush == NULL)
    {
        bg_brush = (HBRUSH) GetStockObject(GRAY_BRUSH);
    }
    if (border_brush == NULL)
    {
        border_brush = (HBRUSH) GetStockObject(WHITE_BRUSH);
    }

    if (dis->itemState & ODS_SELECTED)
    {
        if (launcher->button_down_brush != NULL)
        {
            bg_brush = launcher->button_down_brush;
        }
    }
    else if (dis->itemState & ODS_DISABLED)
    {
        if (launcher->control_brush != NULL)
        {
            bg_brush = launcher->control_brush;
        }
    }
    else if (GetPropA(dis->hwndItem, IWAD_LAUNCHER_BTN_HOVER) != NULL)
    {
        if (launcher->button_hover_brush != NULL)
        {
            bg_brush = launcher->button_hover_brush;
        }
    }

    FillRect(dis->hDC, &rc, bg_brush);
    FrameRect(dis->hDC, &rc, border_brush);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, IWAD_COLOR_TEXT);

    if (dis->CtlID == IDC_IWAD_LAUNCHER_TOGGLE
     && launcher->toggle_use_icon_font
     && launcher->toggle_icon_font != NULL)
    {
        const wchar_t *glyph =
            launcher->view_mode == LAUNCHER_VIEW_SETTINGS
            ? IWAD_LAUNCHER_TOGGLE_GLYPH_BACK
            : IWAD_LAUNCHER_TOGGLE_GLYPH_SETTINGS;

        HFONT old_font = (HFONT) SelectObject(dis->hDC, launcher->toggle_icon_font);
        DrawTextW(dis->hDC, glyph, -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, old_font);
    }
    else if (dis->CtlID == IDC_IWAD_LAUNCHER_CLEAR
          && launcher->toggle_use_icon_font
          && launcher->clear_icon_font != NULL)
    {
        HFONT old_font = (HFONT) SelectObject(dis->hDC, launcher->clear_icon_font);
        DrawTextW(dis->hDC, IWAD_LAUNCHER_CLEAR_GLYPH, -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, old_font);
    }
    else
    {
        char caption[32];
        GetWindowTextA(dis->hwndItem, caption, sizeof(caption));
        DrawTextA(dis->hDC, caption, -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

}

static void DrawLauncherListItem(const DRAWITEMSTRUCT *dis,
                                 const iwad_launcher_t *launcher)
{
    RECT rc = dis->rcItem;
    HBRUSH bg_brush = launcher->control_brush;

    if (dis->itemState & ODS_SELECTED)
    {
        if (launcher->list_sel_brush != NULL)
        {
            bg_brush = launcher->list_sel_brush;
        }
    }

    if (bg_brush == NULL)
    {
        bg_brush = (HBRUSH) GetStockObject(BLACK_BRUSH);
    }

    FillRect(dis->hDC, &rc, bg_brush);

    if (dis->itemID != (UINT) -1)
    {
        LRESULT text_len = SendMessageA(dis->hwndItem, LB_GETTEXTLEN,
                                        (WPARAM) dis->itemID, 0);
        if (text_len != LB_ERR)
        {
            char *text = malloc((size_t) text_len + 1);
            if (text != NULL)
            {
                SendMessageA(dis->hwndItem, LB_GETTEXT,
                             (WPARAM) dis->itemID, (LPARAM) text);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, IWAD_COLOR_TEXT);

                RECT text_rc = rc;
                text_rc.left += ScaleByDPI(4, launcher->dpi);
                DrawTextA(dis->hDC, text, -1, &text_rc,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                free(text);
            }
        }
    }

}

// -----------------------------------------------------------------------------
// DrawLauncherSettingsCheckbox
//  [PN] Draw owner-drawn settings checkbox with launcher dark theme colors.
// -----------------------------------------------------------------------------

static void DrawLauncherSettingsCheckbox(const DRAWITEMSTRUCT *dis,
                                         const iwad_launcher_t *launcher)
{
    RECT rc = dis->rcItem;
    HBRUSH bg_brush = launcher->window_brush;

    if (bg_brush == NULL)
    {
        bg_brush = (HBRUSH) GetStockObject(BLACK_BRUSH);
    }

    FillRect(dis->hDC, &rc, bg_brush);

    int box_size = ScaleByDPI(14, launcher->dpi);
    int text_gap = ScaleByDPI(8, launcher->dpi);
    int box_left = rc.left;
    int box_top = rc.top + ((rc.bottom - rc.top) - box_size) / 2;

    RECT box_rc = { box_left, box_top, box_left + box_size, box_top + box_size };
    RECT text_rc = rc;
    text_rc.left = box_rc.right + text_gap;

    boolean checked = false;
    boolean hovered = GetPropA(dis->hwndItem, IWAD_LAUNCHER_BTN_HOVER) != NULL;

    if (dis->hwndItem == launcher->settings_fullscreen)
    {
        checked = launcher->settings_fullscreen_checked ? true : false;
    }
    else if (dis->hwndItem == launcher->settings_software_renderer)
    {
        checked = launcher->settings_software_renderer_checked ? true : false;
    }
    else if (dis->hwndItem == launcher->settings_autoload_wad)
    {
        checked = launcher->settings_autoload_wad_checked ? true : false;
    }
    else if (dis->hwndItem == launcher->settings_autoload_deh)
    {
        checked = launcher->settings_autoload_deh_checked ? true : false;
    }
    else if (dis->hwndItem == launcher->settings_show_startup)
    {
        checked = launcher->settings_show_startup_checked ? true : false;
    }

    COLORREF box_fill = IWAD_COLOR_CHECK_BG;
    if (hovered)
    {
        box_fill = IWAD_COLOR_CHECK_HOVER;
    }
    if (checked)
    {
        box_fill = hovered ? IWAD_COLOR_CHECK_ACCENT_HOVER
                           : IWAD_COLOR_CHECK_ACCENT;
    }

    HBRUSH box_brush = CreateSolidBrush(box_fill);
    HBRUSH border_brush = CreateSolidBrush(IWAD_COLOR_CHECK_BORDER);
    if (box_brush != NULL)
    {
        FillRect(dis->hDC, &box_rc, box_brush);
        DeleteObject(box_brush);
    }
    if (border_brush != NULL)
    {
        FrameRect(dis->hDC, &box_rc, border_brush);
        DeleteObject(border_brush);
    }

    if (checked)
    {
        HPEN pen = CreatePen(PS_SOLID, ScaleByDPI(2, launcher->dpi), IWAD_COLOR_TEXT);
        if (pen != NULL)
        {
            HPEN old_pen = (HPEN) SelectObject(dis->hDC, pen);
            int x1 = box_rc.left + ScaleByDPI(3, launcher->dpi);
            int y1 = box_rc.top + ScaleByDPI(7, launcher->dpi);
            int x2 = box_rc.left + ScaleByDPI(6, launcher->dpi);
            int y2 = box_rc.top + ScaleByDPI(10, launcher->dpi);
            int x3 = box_rc.left + ScaleByDPI(11, launcher->dpi);
            int y3 = box_rc.top + ScaleByDPI(4, launcher->dpi);

            MoveToEx(dis->hDC, x1, y1, NULL);
            LineTo(dis->hDC, x2, y2);
            LineTo(dis->hDC, x3, y3);

            SelectObject(dis->hDC, old_pen);
            DeleteObject(pen);
        }
    }

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, IWAD_COLOR_TEXT);

    char caption[128];
    GetWindowTextA(dis->hwndItem, caption, sizeof(caption));
    DrawTextA(dis->hDC, caption, -1, &text_rc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static LRESULT CALLBACK LauncherButtonHoverProc(HWND hwnd, UINT msg,
                                                WPARAM wparam, LPARAM lparam)
{
    WNDPROC old_proc = (WNDPROC) GetPropA(hwnd, IWAD_LAUNCHER_BTN_OLDPROC);
    if (old_proc == NULL)
    {
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_MOUSEMOVE && GetPropA(hwnd, IWAD_LAUNCHER_BTN_HOVER) == NULL)
    {
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof(tme));
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);

        SetPropA(hwnd, IWAD_LAUNCHER_BTN_HOVER, (HANDLE) (INT_PTR) 1);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (msg == WM_MOUSELEAVE && GetPropA(hwnd, IWAD_LAUNCHER_BTN_HOVER) != NULL)
    {
        RemovePropA(hwnd, IWAD_LAUNCHER_BTN_HOVER);
        InvalidateRect(hwnd, NULL, TRUE);
    }

    if (msg == WM_NCDESTROY)
    {
        RemovePropA(hwnd, IWAD_LAUNCHER_BTN_HOVER);
        RemovePropA(hwnd, IWAD_LAUNCHER_BTN_OLDPROC);
        return CallWindowProcA(old_proc, hwnd, msg, wparam, lparam);
    }

    return CallWindowProcA(old_proc, hwnd, msg, wparam, lparam);
}

static void InstallButtonHover(HWND hwnd)
{
    WNDPROC old_proc =
        (WNDPROC) SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                                   (LONG_PTR) LauncherButtonHoverProc);

    if (old_proc != NULL)
    {
        SetPropA(hwnd, IWAD_LAUNCHER_BTN_OLDPROC, (HANDLE) old_proc);
    }
}

static void DrawFixedControlBorder(HWND hwnd)
{
    HDC dc = GetWindowDC(hwnd);
    if (dc == NULL)
    {
        return;
    }

    RECT rc;
    GetWindowRect(hwnd, &rc);
    OffsetRect(&rc, -rc.left, -rc.top);

    HBRUSH border_brush = CreateSolidBrush(IWAD_COLOR_BUTTON_BORDER);
    if (border_brush != NULL)
    {
        FrameRect(dc, &rc, border_brush);
        DeleteObject(border_brush);
    }

    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK LauncherControlBorderProc(HWND hwnd, UINT msg,
                                                  WPARAM wparam, LPARAM lparam)
{
    WNDPROC old_proc = (WNDPROC) GetPropA(hwnd, IWAD_LAUNCHER_OLDPROC);
    if (old_proc == NULL)
    {
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_NCDESTROY)
    {
        RemovePropA(hwnd, IWAD_LAUNCHER_OLDPROC);
        return CallWindowProcA(old_proc, hwnd, msg, wparam, lparam);
    }

    LRESULT result = CallWindowProcA(old_proc, hwnd, msg, wparam, lparam);

    if (msg == WM_NCPAINT
     || msg == WM_PAINT
     || msg == WM_SETFOCUS
     || msg == WM_KILLFOCUS
     || msg == WM_ENABLE)
    {
        DrawFixedControlBorder(hwnd);
    }

    return result;
}

static void InstallFixedControlBorder(HWND hwnd)
{
    WNDPROC old_proc =
        (WNDPROC) SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                                   (LONG_PTR) LauncherControlBorderProc);

    if (old_proc != NULL)
    {
        SetPropA(hwnd, IWAD_LAUNCHER_OLDPROC, (HANDLE) old_proc);
        DrawFixedControlBorder(hwnd);
    }
}

static int ScaleByDPI(int value, int dpi)
{
    return MulDiv(value, dpi, 96);
}

static int GetSystemDPI(void)
{
    int dpi = 96;
    HDC dc = GetDC(NULL);

    if (dc != NULL)
    {
        dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }

    if (dpi <= 0)
    {
        dpi = 96;
    }

    return dpi;
}

static const char *DefaultWindowTitleForMask(int mask)
{
    if (mask == IWAD_MASK_DOOM)
    {
        return "International Doom";
    }
    else if (mask == IWAD_MASK_HERETIC)
    {
        return "International Heretic";
    }
    else if (mask == IWAD_MASK_HEXEN)
    {
        return "International Hexen";
    }

    return "Game";
}

// -----------------------------------------------------------------------------
// SetControlVisibility
//  [PN] Show or hide a launcher control by mode.
// -----------------------------------------------------------------------------

static void SetControlVisibility(HWND control, boolean visible)
{
    if (control != NULL)
    {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

// -----------------------------------------------------------------------------
// GetLauncherMinimumClientSize
//  [PN] Compute minimum client size that keeps controls from overlapping.
// -----------------------------------------------------------------------------

static void GetLauncherMinimumClientSize(int dpi, int *min_w, int *min_h)
{
    int margin = ScaleByDPI(16, dpi);
    int gap = ScaleByDPI(8, dpi);
    int section_gap = ScaleByDPI(10, dpi);
    int check_gap = ScaleByDPI(4, dpi);
    int label_h = ScaleByDPI(20, dpi);
    int check_h = ScaleByDPI(20, dpi);
    int edit_h = ScaleByDPI(20, dpi);
    int top_btn_w = ScaleByDPI(38, dpi);
    int top_btn_h = ScaleByDPI(24, dpi);
    int btn_h = ScaleByDPI(IWAD_BUTTON_H, dpi);
    int top_row_h = (top_btn_h > label_h) ? top_btn_h : label_h;
    int params_row_h = top_row_h;
    int min_list_h = ScaleByDPI(40, dpi);

    int settings_content_h =
          (label_h + check_gap + check_h + check_gap + check_h + section_gap)
        + (label_h + check_gap + check_h + check_gap + check_h + section_gap)
        + (label_h + check_gap + check_h);

    int min_client_h_iwad = margin + top_row_h + gap + min_list_h + gap
                          + params_row_h + gap + edit_h + gap + btn_h + margin;
    int min_client_h_settings = margin + top_row_h + gap + settings_content_h + gap
                              + btn_h + margin;

    int min_content_w_buttons = (ScaleByDPI(80, dpi) * 2) + gap;
    int min_content_w_header = top_btn_w + gap + ScaleByDPI(144, dpi);
    int min_content_w = (min_content_w_buttons > min_content_w_header)
                      ? min_content_w_buttons
                      : min_content_w_header;
    int min_client_w = margin * 2 + min_content_w;

    *min_w = min_client_w;
    *min_h = (min_client_h_iwad > min_client_h_settings)
           ? min_client_h_iwad
           : min_client_h_settings;
}

static void LayoutIWADLauncher(iwad_launcher_t *launcher)
{
    if (launcher == NULL || launcher->window == NULL)
    {
        return;
    }

    RECT rc;
    GetClientRect(launcher->window, &rc);

    int margin = ScaleByDPI(16, launcher->dpi);
    int gap = ScaleByDPI(8, launcher->dpi);
    int section_gap = ScaleByDPI(10, launcher->dpi);
    int check_gap = ScaleByDPI(4, launcher->dpi);
    int label_h = ScaleByDPI(20, launcher->dpi);
    int check_h = ScaleByDPI(20, launcher->dpi);
    int edit_h = ScaleByDPI(20, launcher->dpi);
    int top_btn_w = ScaleByDPI(38, launcher->dpi);
    int top_btn_h = ScaleByDPI(24, launcher->dpi);
    int ideal_btn_w = ScaleByDPI(IWAD_BUTTON_IDEAL_W, launcher->dpi);
    int btn_h = ScaleByDPI(IWAD_BUTTON_H, launcher->dpi);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    int x = margin;
    int y = margin;

    width = width - margin * 2;
    if (width < ScaleByDPI(50, launcher->dpi))
    {
        width = ScaleByDPI(50, launcher->dpi);
    }

    int btn_w = (width - gap) / 2;
    if (btn_w > ideal_btn_w)
    {
        btn_w = ideal_btn_w;
    }

    if (top_btn_w > width / 2)
    {
        top_btn_w = width / 2;
    }

    int prompt_w = width - top_btn_w - gap;
    if (prompt_w < ScaleByDPI(40, launcher->dpi))
    {
        prompt_w = ScaleByDPI(40, launcher->dpi);
    }

    int top_row_h = (top_btn_h > label_h) ? top_btn_h : label_h;
    int prompt_y = y + (top_row_h - label_h) / 2;
    int toggle_y = y + (top_row_h - top_btn_h) / 2;

    MoveWindow(launcher->prompt_label, x, prompt_y, prompt_w, label_h, TRUE);
    MoveWindow(launcher->toggle_button, x + width - top_btn_w, toggle_y,
               top_btn_w, top_btn_h, TRUE);
    y += top_row_h + gap;

    if (launcher->view_mode == LAUNCHER_VIEW_IWAD)
    {
        int params_row_h = (top_btn_h > label_h) ? top_btn_h : label_h;
        int bottom_reserved = margin + btn_h + gap + edit_h + gap + params_row_h + gap;
        int list_available = height - y - bottom_reserved;
        if (list_available < ScaleByDPI(40, launcher->dpi))
        {
            list_available = ScaleByDPI(40, launcher->dpi);
        }
        int list_h = list_available;

        MoveWindow(launcher->iwad_list, x, y, width, list_h, TRUE);
        y += list_h + gap;

        int params_label_w = width - top_btn_w - gap;
        if (params_label_w < ScaleByDPI(40, launcher->dpi))
        {
            params_label_w = ScaleByDPI(40, launcher->dpi);
        }

        int params_label_y = y + (params_row_h - label_h) / 2;
        int clear_btn_y = y + (params_row_h - top_btn_h) / 2;

        MoveWindow(launcher->params_label, x, params_label_y, params_label_w, label_h, TRUE);
        MoveWindow(launcher->clear_button, x + width - top_btn_w, clear_btn_y,
                   top_btn_w, top_btn_h, TRUE);
        y += params_row_h + gap;

        MoveWindow(launcher->params_edit, x, y, width, edit_h, TRUE);
    }
    else
    {
        MoveWindow(launcher->settings_video_label, x, y, width, label_h, TRUE);
        y += label_h + check_gap;

        MoveWindow(launcher->settings_fullscreen, x, y, width, check_h, TRUE);
        y += check_h + check_gap;
        MoveWindow(launcher->settings_software_renderer, x, y, width, check_h, TRUE);
        y += check_h + section_gap;

        MoveWindow(launcher->settings_autoload_label, x, y, width, label_h, TRUE);
        y += label_h + check_gap;

        MoveWindow(launcher->settings_autoload_wad, x, y, width, check_h, TRUE);
        y += check_h + check_gap;
        MoveWindow(launcher->settings_autoload_deh, x, y, width, check_h, TRUE);
        y += check_h + section_gap;

        MoveWindow(launcher->settings_launcher_label, x, y, width, label_h, TRUE);
        y += label_h + check_gap;

        MoveWindow(launcher->settings_show_startup, x, y, width, check_h, TRUE);
    }

    int btn_y = height - margin - btn_h;

    MoveWindow(GetDlgItem(launcher->window, IDC_IWAD_LAUNCHER_PLAY),
               x, btn_y, btn_w, btn_h, TRUE);
    MoveWindow(GetDlgItem(launcher->window, IDC_IWAD_LAUNCHER_EXIT),
               x + width - btn_w, btn_y, btn_w, btn_h, TRUE);
}

// -----------------------------------------------------------------------------
// ApplyLauncherView
//  [PN] Apply IWAD/settings view state and refresh visible controls.
// -----------------------------------------------------------------------------

static void ApplyLauncherView(iwad_launcher_t *launcher)
{
    if (launcher == NULL || launcher->window == NULL)
    {
        return;
    }

    boolean settings_view = launcher->view_mode == LAUNCHER_VIEW_SETTINGS;
    HWND play_btn = GetDlgItem(launcher->window, IDC_IWAD_LAUNCHER_PLAY);

    SetWindowTextA(launcher->prompt_label,
                   settings_view ? IWAD_LAUNCHER_SETTINGS_PROMPT
                                 : IWAD_LAUNCHER_PROMPT);
    if (launcher->toggle_use_icon_font)
    {
        SetWindowTextA(launcher->toggle_button, "");
    }
    else
    {
        SetWindowTextA(launcher->toggle_button,
                       settings_view ? IWAD_LAUNCHER_TOGGLE_BACK
                                     : IWAD_LAUNCHER_TOGGLE_SETTINGS);
    }
    SetWindowTextA(play_btn, IWAD_LAUNCHER_PLAY_CAPTION);

    SetControlVisibility(launcher->iwad_list, !settings_view);
    SetControlVisibility(launcher->params_label, !settings_view);
    SetControlVisibility(launcher->params_edit, !settings_view);
    SetControlVisibility(launcher->clear_button, !settings_view);
    SetControlVisibility(launcher->settings_video_label, settings_view);
    SetControlVisibility(launcher->settings_fullscreen, settings_view);
    SetControlVisibility(launcher->settings_software_renderer, settings_view);
    SetControlVisibility(launcher->settings_autoload_label, settings_view);
    SetControlVisibility(launcher->settings_autoload_wad, settings_view);
    SetControlVisibility(launcher->settings_autoload_deh, settings_view);
    SetControlVisibility(launcher->settings_launcher_label, settings_view);
    SetControlVisibility(launcher->settings_show_startup, settings_view);

    LayoutIWADLauncher(launcher);
    InvalidateRect(launcher->window, NULL, TRUE);
}

// -----------------------------------------------------------------------------
// BuildIWADDisplayName
//  [PN] Build the visible list label for one IWAD search result.
// -----------------------------------------------------------------------------

static char *BuildIWADDisplayName(const iwad_search_result_t *result)
{
    const iwad_t *iwad = result->iwad;
    const char *iwad_name = iwad->name;
    char *label;

    if (M_StringEndsWith(iwad_name, ".wad"))
    {
        char *short_name = M_StringDuplicate(iwad_name);
        short_name[strlen(short_name) - 4] = '\0';
        label = M_StringJoin(iwad->description, " (", short_name, ")", NULL);
        free(short_name);
    }
    else
    {
        label = M_StringJoin(iwad->description, " (", iwad_name, ")", NULL);
    }

    if (result->source_tag != NULL)
    {
        char *with_tag = M_StringJoin(label, " [", result->source_tag, "]", NULL);
        free(label);
        return with_tag;
    }

    return label;
}

// -----------------------------------------------------------------------------
// SaveDefaultIWADValue
//  [PN] Persist default IWAD selection as a config string value.
// -----------------------------------------------------------------------------

static void SaveDefaultIWADValue(const char *value)
{
    if (value == NULL || value[0] == '\0')
    {
        return;
    }

    if (launcher_default_iwad != NULL && !strcasecmp(launcher_default_iwad, value))
    {
        return;
    }

    if (launcher_default_iwad != NULL
     && (strchr(launcher_default_iwad, '\\') != NULL
      || strchr(launcher_default_iwad, '/') != NULL))
    {
        free(launcher_default_iwad);
    }

    launcher_default_iwad = M_StringDuplicate(value);
}

// -----------------------------------------------------------------------------
// SaveLauncherCommandLineValue
//  [PN] Persist launcher command-line edit text as a config string value.
// -----------------------------------------------------------------------------

static void SaveLauncherCommandLineValue(const char *value)
{
    if (value == NULL)
    {
        value = "";
    }

    if (launcher_command_line != NULL && !strcmp(launcher_command_line, value))
    {
        return;
    }

    if (launcher_command_line != NULL && launcher_command_line[0] != '\0')
    {
        free(launcher_command_line);
    }

    launcher_command_line = M_StringDuplicate(value);
}

// -----------------------------------------------------------------------------
// SyncLauncherSettingsToUI
//  [PN] Populate launcher settings checkboxes from config variables.
// -----------------------------------------------------------------------------

static void SyncLauncherSettingsToUI(iwad_launcher_t *launcher)
{
    if (launcher == NULL)
    {
        return;
    }

    launcher->settings_fullscreen_checked = vid_fullscreen == 1 ? 1 : 0;
    launcher->settings_software_renderer_checked =
        vid_force_software_renderer == 1 ? 1 : 0;
    launcher->settings_autoload_wad_checked = autoload_wad == 1 ? 1 : 0;
    launcher->settings_autoload_deh_checked = autoload_deh == 1 ? 1 : 0;
    launcher->settings_show_startup_checked = show_startup_launcher == 1 ? 1 : 0;

    launcher->initial_fullscreen_checkbox = launcher->settings_fullscreen_checked;
    launcher->initial_software_renderer_checkbox =
        launcher->settings_software_renderer_checked;
    launcher->initial_autoload_wad_checkbox = launcher->settings_autoload_wad_checked;
    launcher->initial_autoload_deh_checkbox = launcher->settings_autoload_deh_checked;
    launcher->initial_show_startup_checkbox = launcher->settings_show_startup_checked;
}

// -----------------------------------------------------------------------------
// ApplyLauncherSettingsFromUI
//  [PN] Apply launcher settings checkbox states back to config variables.
// -----------------------------------------------------------------------------

static void ApplyLauncherSettingsFromUI(iwad_launcher_t *launcher)
{
    if (launcher == NULL)
    {
        return;
    }

    int fullscreen_value = launcher->settings_fullscreen_checked;
    int software_renderer_value = launcher->settings_software_renderer_checked;
    int autoload_wad_value = launcher->settings_autoload_wad_checked;
    int autoload_deh_value = launcher->settings_autoload_deh_checked;
    int show_startup_value = launcher->settings_show_startup_checked;

    if (fullscreen_value != launcher->initial_fullscreen_checkbox)
    {
        vid_fullscreen = fullscreen_value;
    }

    if (software_renderer_value != launcher->initial_software_renderer_checkbox)
    {
        vid_force_software_renderer = software_renderer_value;
    }

    if (autoload_wad_value != launcher->initial_autoload_wad_checkbox)
    {
        autoload_wad = autoload_wad_value;
    }

    if (autoload_deh_value != launcher->initial_autoload_deh_checkbox)
    {
        autoload_deh = autoload_deh_value;
    }

    if (show_startup_value != launcher->initial_show_startup_checkbox)
    {
        show_startup_launcher = show_startup_value;
    }
}

// -----------------------------------------------------------------------------
// IsDoubleClickOnIWADItem
//  [PN] Confirm a double-click hit a real list item, not blank list area.
// -----------------------------------------------------------------------------

static boolean IsDoubleClickOnIWADItem(HWND listbox)
{
    DWORD msg_pos = GetMessagePos();
    POINT pt;
    LRESULT item_from_point;
    int item;
    RECT item_rect;

    if (listbox == NULL)
    {
        return false;
    }

    pt.x = (short) LOWORD(msg_pos);
    pt.y = (short) HIWORD(msg_pos);

    if (!ScreenToClient(listbox, &pt))
    {
        return false;
    }

    item_from_point = SendMessageA(listbox, LB_ITEMFROMPOINT, 0,
                                   MAKELPARAM(pt.x, pt.y));
    item = (int) LOWORD(item_from_point);

    // LB_ITEMFROMPOINT returns nearest item; verify point is inside its rect.
    if (SendMessageA(listbox, LB_GETITEMRECT, (WPARAM) item,
                     (LPARAM) &item_rect) == LB_ERR)
    {
        return false;
    }

    return PtInRect(&item_rect, pt) ? true : false;
}

static void FinishIWADLauncher(iwad_launcher_t *launcher, boolean play_pressed)
{
    launcher->play_pressed = play_pressed;
    launcher->done = true;

    ApplyLauncherSettingsFromUI(launcher);

    LRESULT sel = SendMessageA(launcher->iwad_list, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR)
    {
        launcher->selected_iwad = (int) sel;
    }

    if (launcher->params_edit != NULL)
    {
        int length = GetWindowTextLengthA(launcher->params_edit);
        free(launcher->additional_params);
        launcher->additional_params = malloc((size_t) length + 1);

        if (launcher->additional_params == NULL)
        {
            I_Error("Failed to allocate memory for launcher parameters.");
        }

        GetWindowTextA(launcher->params_edit, launcher->additional_params,
                       length + 1);
    }

    if (launcher->window != NULL)
    {
        DestroyWindow(launcher->window);
        launcher->window = NULL;
    }
}

static LRESULT CALLBACK IWADLauncherWndProc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam)
{
    iwad_launcher_t *launcher = (iwad_launcher_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_NCCREATE:
        {
            CREATESTRUCTA *cs = (CREATESTRUCTA *) lparam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
            return TRUE;
        }

        case WM_CREATE:
        {
            launcher = (iwad_launcher_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (launcher == NULL)
            {
                return -1;
            }

            launcher->window = hwnd;

            launcher->prompt_label = CreateWindowExA(0, "STATIC", IWAD_LAUNCHER_PROMPT,
                                                     WS_CHILD | WS_VISIBLE,
                                                     0, 0, 0, 0, hwnd, NULL, NULL, NULL);

            launcher->toggle_button = CreateWindowExA(0, "BUTTON",
                                                      IWAD_LAUNCHER_TOGGLE_SETTINGS,
                                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                    | BS_OWNERDRAW,
                                                      0, 0, 0, 0, hwnd,
                                                      (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_TOGGLE,
                                                      NULL, NULL);

            launcher->iwad_list = CreateWindowExA(0, "LISTBOX", "",
                                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                                | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT
                                                | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED
                                                | WS_BORDER,
                                                  0, 0, 0, 0,
                                                  hwnd,
                                                  (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_LIST,
                                                  NULL, NULL);

            TryApplyDarkControlTheme(launcher->iwad_list);

            if (launcher->iwads[0].iwad != NULL)
            {
                int default_iwad = 0;
                boolean matched = false;

                if (launcher_default_iwad != NULL && launcher_default_iwad[0] != '\0')
                {
                    for (int i = 0; launcher->iwads[i].iwad != NULL; ++i)
                    {
                        if (!strcasecmp(launcher->iwads[i].path, launcher_default_iwad))
                        {
                            default_iwad = i;
                            matched = true;
                            break;
                        }
                    }

                    // Backward compatibility with older configs that store only name.
                    if (!matched)
                    {
                        for (int i = 0; launcher->iwads[i].iwad != NULL; ++i)
                        {
                            if (!strcasecmp(launcher->iwads[i].iwad->name,
                                            launcher_default_iwad))
                            {
                                default_iwad = i;
                                break;
                            }
                        }
                    }
                }

                for (int i = 0; launcher->iwads[i].iwad != NULL; ++i)
                {
                    char *label = BuildIWADDisplayName(&launcher->iwads[i]);
                    SendMessageA(launcher->iwad_list, LB_ADDSTRING, 0, (LPARAM) label);
                    free(label);
                }

                SendMessageA(launcher->iwad_list, LB_SETCURSEL, (WPARAM) default_iwad, 0);
                launcher->selected_iwad = default_iwad;
            }
            else
            {
                SendMessageA(launcher->iwad_list, LB_ADDSTRING, 0,
                             (LPARAM) "No compatible IWADs were found.");
                EnableWindow(launcher->iwad_list, FALSE);
            }

            launcher->params_label = CreateWindowExA(0, "STATIC", "Command line parameters:",
                                                     WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                                                     0, 0, 0, 0, hwnd, NULL, NULL, NULL);

            launcher->params_edit = CreateWindowExA(0, "EDIT",
                                                    launcher->additional_params,
                                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                  | ES_LEFT | ES_AUTOHSCROLL
                                                  | WS_BORDER,
                                                    0, 0, 0, 0,
                                                    hwnd,
                                                    (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_EDIT,
                                                    NULL, NULL);

            launcher->clear_button = CreateWindowExA(0, "BUTTON",
                                                     IWAD_LAUNCHER_CLEAR_CAPTION,
                                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                   | BS_OWNERDRAW,
                                                     0, 0, 0, 0, hwnd,
                                                     (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_CLEAR,
                                                     NULL, NULL);

            launcher->settings_video_label = CreateWindowExA(0, "STATIC", "Video",
                                                             WS_CHILD | WS_VISIBLE,
                                                             0, 0, 0, 0, hwnd,
                                                             (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_VIDEO_LABEL,
                                                             NULL, NULL);

            launcher->settings_fullscreen = CreateWindowExA(0, "BUTTON",
                                                            "Fullscreen mode",
                                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                          | BS_OWNERDRAW,
                                                            0, 0, 0, 0, hwnd,
                                                            (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_FULLSCREEN,
                                                            NULL, NULL);

            launcher->settings_software_renderer = CreateWindowExA(0, "BUTTON",
                                                                   "Force compatibility mode",
                                                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                                 | BS_OWNERDRAW,
                                                                   0, 0, 0, 0, hwnd,
                                                                   (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_SOFTWARE_RENDER,
                                                                   NULL, NULL);

            launcher->settings_autoload_label = CreateWindowExA(0, "STATIC", "Autoload",
                                                                WS_CHILD | WS_VISIBLE,
                                                                0, 0, 0, 0, hwnd,
                                                                (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_AUTOLOAD_LABEL,
                                                                NULL, NULL);

            launcher->settings_autoload_wad = CreateWindowExA(0, "BUTTON",
                                                              "Autoload WAD files",
                                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                            | BS_OWNERDRAW,
                                                              0, 0, 0, 0, hwnd,
                                                              (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_AUTOLOAD_WAD,
                                                              NULL, NULL);

            launcher->settings_autoload_deh = CreateWindowExA(0, "BUTTON",
                                                              "Autoload DEH files",
                                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                            | BS_OWNERDRAW,
                                                              0, 0, 0, 0, hwnd,
                                                              (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_AUTOLOAD_DEH,
                                                              NULL, NULL);

            launcher->settings_launcher_label = CreateWindowExA(0, "STATIC", "Launcher",
                                                                WS_CHILD | WS_VISIBLE,
                                                                0, 0, 0, 0, hwnd,
                                                                (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_LAUNCHER_LABEL,
                                                                NULL, NULL);

            launcher->settings_show_startup = CreateWindowExA(0, "BUTTON",
                                                              "Show launcher at startup",
                                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                            | BS_OWNERDRAW,
                                                              0, 0, 0, 0, hwnd,
                                                              (HMENU) (INT_PTR) IDC_IWAD_SETTINGS_SHOW_STARTUP,
                                                              NULL, NULL);

            InstallFixedControlBorder(launcher->iwad_list);
            InstallFixedControlBorder(launcher->params_edit);

            HWND play_btn = CreateWindowExA(0, "BUTTON", IWAD_LAUNCHER_PLAY_CAPTION,
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                          | BS_OWNERDRAW,
                                            0, 0, 0, 0, hwnd,
                                            (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_PLAY,
                                            NULL, NULL);

            HWND exit_btn = CreateWindowExA(0, "BUTTON", "Exit",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                          | BS_OWNERDRAW,
                                            0, 0, 0, 0, hwnd,
                                            (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_EXIT,
                                            NULL, NULL);

            if (play_btn != NULL)
            {
                InstallButtonHover(play_btn);
            }

            if (launcher->toggle_button != NULL)
            {
                InstallButtonHover(launcher->toggle_button);
            }

            if (launcher->clear_button != NULL)
            {
                InstallButtonHover(launcher->clear_button);
            }

            if (launcher->settings_fullscreen != NULL)
            {
                InstallButtonHover(launcher->settings_fullscreen);
            }

            if (launcher->settings_software_renderer != NULL)
            {
                InstallButtonHover(launcher->settings_software_renderer);
            }

            if (launcher->settings_autoload_wad != NULL)
            {
                InstallButtonHover(launcher->settings_autoload_wad);
            }

            if (launcher->settings_autoload_deh != NULL)
            {
                InstallButtonHover(launcher->settings_autoload_deh);
            }

            if (launcher->settings_show_startup != NULL)
            {
                InstallButtonHover(launcher->settings_show_startup);
            }

            launcher->clear_tooltip = CreateButtonTooltip(hwnd,
                                                          launcher->clear_button,
                                                          "Clear command line parameters");

            if (exit_btn != NULL)
            {
                InstallButtonHover(exit_btn);
            }

            ApplyFontToControl(launcher->prompt_label, launcher->ui_font);
            ApplyFontToControl(launcher->toggle_button, launcher->ui_font);
            ApplyFontToControl(launcher->iwad_list, launcher->ui_font);
            ApplyFontToControl(launcher->params_label, launcher->ui_font);
            ApplyFontToControl(launcher->params_edit, launcher->ui_font);
            ApplyFontToControl(launcher->clear_button, launcher->ui_font);
            ApplyFontToControl(launcher->settings_video_label, launcher->ui_font);
            ApplyFontToControl(launcher->settings_fullscreen, launcher->ui_font);
            ApplyFontToControl(launcher->settings_software_renderer, launcher->ui_font);
            ApplyFontToControl(launcher->settings_autoload_label, launcher->ui_font);
            ApplyFontToControl(launcher->settings_autoload_wad, launcher->ui_font);
            ApplyFontToControl(launcher->settings_autoload_deh, launcher->ui_font);
            ApplyFontToControl(launcher->settings_launcher_label, launcher->ui_font);
            ApplyFontToControl(launcher->settings_show_startup, launcher->ui_font);
            ApplyFontToControl(play_btn, launcher->ui_font);
            ApplyFontToControl(exit_btn, launcher->ui_font);

            SyncLauncherSettingsToUI(launcher);
            launcher->view_mode = LAUNCHER_VIEW_IWAD;
            ApplyLauncherView(launcher);
            return 0;
        }

        case WM_ERASEBKGND:
            if (launcher != NULL && launcher->window_brush != NULL)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                FillRect((HDC) wparam, &rc, launcher->window_brush);
                return 1;
            }
            break;

        case WM_CTLCOLORSTATIC:
            if (launcher != NULL && launcher->window_brush != NULL)
            {
                HDC dc = (HDC) wparam;
                SetTextColor(dc, IWAD_COLOR_TEXT);
                SetBkColor(dc, IWAD_COLOR_WINDOW_BG);
                return (LRESULT) launcher->window_brush;
            }
            break;

        case WM_CTLCOLORBTN:
            if (launcher != NULL && launcher->window_brush != NULL)
            {
                HDC dc = (HDC) wparam;
                SetTextColor(dc, IWAD_COLOR_TEXT);
                SetBkColor(dc, IWAD_COLOR_WINDOW_BG);
                SetBkMode(dc, TRANSPARENT);
                return (LRESULT) launcher->window_brush;
            }
            break;

        case WM_MEASUREITEM:
            {
                MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *) lparam;
                if (mis != NULL && mis->CtlID == IDC_IWAD_LAUNCHER_LIST)
                {
                    int dpi = (launcher != NULL) ? launcher->dpi : GetSystemDPI();
                    int height = ScaleByDPI(18, dpi);

                    if (launcher != NULL && launcher->list_item_height > height)
                    {
                        height = launcher->list_item_height;
                    }

                    mis->itemHeight = (UINT) height;
                    return TRUE;
                }
            }
            return FALSE;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            if (launcher != NULL && launcher->control_brush != NULL)
            {
                HDC dc = (HDC) wparam;
                SetTextColor(dc, IWAD_COLOR_TEXT);
                SetBkColor(dc, IWAD_COLOR_CONTROL_BG);
                return (LRESULT) launcher->control_brush;
            }
            break;

        case WM_DRAWITEM:
            if (launcher == NULL)
            {
                return FALSE;
            }

            {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *) lparam;
                if (dis == NULL)
                {
                    return FALSE;
                }

                if (dis->CtlID == IDC_IWAD_LAUNCHER_PLAY
                 || dis->CtlID == IDC_IWAD_LAUNCHER_TOGGLE
                 || dis->CtlID == IDC_IWAD_LAUNCHER_CLEAR
                 || dis->CtlID == IDC_IWAD_LAUNCHER_EXIT)
                {
                    DrawLauncherButton(dis, launcher);
                    return TRUE;
                }

                if (dis->CtlID == IDC_IWAD_SETTINGS_FULLSCREEN
                 || dis->CtlID == IDC_IWAD_SETTINGS_SOFTWARE_RENDER
                 || dis->CtlID == IDC_IWAD_SETTINGS_AUTOLOAD_WAD
                 || dis->CtlID == IDC_IWAD_SETTINGS_AUTOLOAD_DEH
                 || dis->CtlID == IDC_IWAD_SETTINGS_SHOW_STARTUP)
                {
                    DrawLauncherSettingsCheckbox(dis, launcher);
                    return TRUE;
                }

                if (dis->CtlID == IDC_IWAD_LAUNCHER_LIST)
                {
                    DrawLauncherListItem(dis, launcher);
                    return TRUE;
                }
            }
            return FALSE;

        case WM_SIZE:
            if (launcher != NULL)
            {
                LayoutIWADLauncher(launcher);

                if (!IsIconic(hwnd))
                {
                    RECT window_rect;
                    if (GetWindowRect(hwnd, &window_rect))
                    {
                        launcher_width_x = window_rect.right - window_rect.left;
                        launcher_width_y = window_rect.bottom - window_rect.top;
                    }
                }
            }
            return 0;

        case WM_GETMINMAXINFO:
            {
                MINMAXINFO *mmi = (MINMAXINFO *) lparam;
                int dpi = launcher != NULL ? launcher->dpi : GetSystemDPI();
                int min_client_w = 0;
                int min_client_h = 0;
                RECT min_rect;
                DWORD wnd_style;
                DWORD wnd_exstyle;

                GetLauncherMinimumClientSize(dpi, &min_client_w, &min_client_h);

                min_rect.left = 0;
                min_rect.top = 0;
                min_rect.right = min_client_w;
                min_rect.bottom = min_client_h;

                wnd_style = (DWORD) GetWindowLongPtr(hwnd, GWL_STYLE);
                wnd_exstyle = (DWORD) GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                AdjustWindowRectEx(&min_rect, wnd_style, FALSE, wnd_exstyle);

                mmi->ptMinTrackSize.x = min_rect.right - min_rect.left;
                mmi->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
                return 0;
            }

        case WM_MOVE:
            if (launcher != NULL && !IsIconic(hwnd))
            {
                RECT window_rect;
                if (GetWindowRect(hwnd, &window_rect))
                {
                    launcher_position_x = window_rect.left;
                    launcher_position_y = window_rect.top;
                }
            }
            return 0;

        case WM_COMMAND:
            if (launcher == NULL)
            {
                return 0;
            }

            switch (LOWORD(wparam))
            {
                case IDC_IWAD_LAUNCHER_PLAY:
                    FinishIWADLauncher(launcher, true);
                    return 0;

                case IDC_IWAD_LAUNCHER_EXIT:
                    FinishIWADLauncher(launcher, false);
                    return 0;

                case IDC_IWAD_LAUNCHER_TOGGLE:
                    launcher->view_mode =
                        (launcher->view_mode == LAUNCHER_VIEW_IWAD)
                        ? LAUNCHER_VIEW_SETTINGS
                        : LAUNCHER_VIEW_IWAD;
                    ApplyLauncherView(launcher);
                    return 0;

                case IDC_IWAD_LAUNCHER_CLEAR:
                    if (launcher->params_edit != NULL)
                    {
                        SetWindowTextA(launcher->params_edit, "");
                    }
                    return 0;

                case IDC_IWAD_SETTINGS_FULLSCREEN:
                case IDC_IWAD_SETTINGS_SOFTWARE_RENDER:
                case IDC_IWAD_SETTINGS_AUTOLOAD_WAD:
                case IDC_IWAD_SETTINGS_AUTOLOAD_DEH:
                case IDC_IWAD_SETTINGS_SHOW_STARTUP:
                    if (HIWORD(wparam) == BN_CLICKED)
                    {
                        HWND checkbox = (HWND) lparam;
                        int *value = NULL;

                        switch (LOWORD(wparam))
                        {
                            case IDC_IWAD_SETTINGS_FULLSCREEN:
                                value = &launcher->settings_fullscreen_checked;
                                break;
                            case IDC_IWAD_SETTINGS_SOFTWARE_RENDER:
                                value = &launcher->settings_software_renderer_checked;
                                break;
                            case IDC_IWAD_SETTINGS_AUTOLOAD_WAD:
                                value = &launcher->settings_autoload_wad_checked;
                                break;
                            case IDC_IWAD_SETTINGS_AUTOLOAD_DEH:
                                value = &launcher->settings_autoload_deh_checked;
                                break;
                            case IDC_IWAD_SETTINGS_SHOW_STARTUP:
                                value = &launcher->settings_show_startup_checked;
                                break;
                        }

                        if (value != NULL)
                        {
                            *value = *value ? 0 : 1;
                        }

                        InvalidateRect(checkbox, NULL, TRUE);
                    }
                    return 0;

                case IDC_IWAD_LAUNCHER_LIST:
                    if (launcher->view_mode == LAUNCHER_VIEW_IWAD
                     && HIWORD(wparam) == LBN_DBLCLK)
                    {
                        if (IsDoubleClickOnIWADItem(launcher->iwad_list))
                        {
                            FinishIWADLauncher(launcher, true);
                        }
                    }
                    return 0;
            }
            break;

        case WM_CLOSE:
            if (launcher != NULL)
            {
                FinishIWADLauncher(launcher, false);
            }
            return 0;

        case WM_DESTROY:
            if (launcher != NULL && launcher->clear_tooltip != NULL)
            {
                DestroyWindow(launcher->clear_tooltip);
                launcher->clear_tooltip = NULL;
            }
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static boolean RunIWADLauncherDialog(int mask)
{
    static boolean class_registered = false;
    iwad_launcher_t launcher = { 0 };
    INITCOMMONCONTROLSEX icc;

    memset(&icc, 0, sizeof(icc));
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    launcher.iwads = D_FindAllIWADSearchResults(mask);
    launcher.dpi = GetSystemDPI();
    launcher.selected_iwad = 0;
    launcher.additional_params =
        M_StringDuplicate(launcher_command_line != NULL ? launcher_command_line : "");

    if (M_HasLooseFiles())
    {
        free(launcher.additional_params);
        launcher.additional_params = BuildLauncherParamsFromLooseFiles();
        ClearCommandLineExceptExecutable();
    }

    launcher.ui_font = CreateLauncherUIFont(launcher.dpi, &launcher.owns_ui_font);
    launcher.toggle_icon_font = CreateLauncherToggleIconFont(launcher.dpi,
                                            &launcher.owns_toggle_icon_font);
    launcher.clear_icon_font = CreateLauncherClearIconFont(launcher.dpi,
                                            &launcher.owns_clear_icon_font);
    launcher.toggle_use_icon_font =
        FontFaceEqualsW(launcher.toggle_icon_font, L"Segoe MDL2 Assets");
    launcher.list_item_height = MeasureFontHeight(launcher.ui_font)
                              + ScaleByDPI(4, launcher.dpi);
    launcher.window_brush = CreateSolidBrush(IWAD_COLOR_WINDOW_BG);
    launcher.control_brush = CreateSolidBrush(IWAD_COLOR_CONTROL_BG);
    launcher.list_sel_brush = CreateSolidBrush(IWAD_COLOR_LIST_SEL_BG);
    launcher.button_brush = CreateSolidBrush(IWAD_COLOR_BUTTON_BG);
    launcher.button_hover_brush = CreateSolidBrush(IWAD_COLOR_BUTTON_HOVER);
    launcher.button_down_brush = CreateSolidBrush(IWAD_COLOR_BUTTON_DOWN);
    launcher.button_border_brush = CreateSolidBrush(IWAD_COLOR_BUTTON_BORDER);
    launcher.play_pressed = false;
    launcher.done = false;

    const char *resolved_title = DefaultWindowTitleForMask(mask);
    HINSTANCE inst = GetModuleHandle(NULL);
    HICON icon = (HICON) LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON,
                                    0, 0, LR_DEFAULTSIZE);

    if (icon == NULL)
    {
        icon = LoadIcon(NULL, IDI_APPLICATION);
    }

    if (!class_registered)
    {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = IWADLauncherWndProc;
        wc.hInstance = inst;
        wc.hIcon = icon;
        wc.hIconSm = icon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = IWAD_LAUNCHER_CLASS_NAME;

        if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            CleanupLauncherFont(&launcher);
            CleanupLauncherTheme(&launcher);
            free(launcher.additional_params);
            D_FreeIWADSearchResults(launcher.iwads);
            return true;
        }

        class_registered = true;
    }

    int client_w = ScaleByDPI(IWAD_WINDOW_CLIENT_W, launcher.dpi);
    int client_h = ScaleByDPI(IWAD_WINDOW_CLIENT_H, launcher.dpi);
    DWORD style = WS_OVERLAPPEDWINDOW
                & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    DWORD exstyle = 0;

    RECT win_rect;
    win_rect.left = 0;
    win_rect.top = 0;
    win_rect.right = client_w;
    win_rect.bottom = client_h;
    AdjustWindowRectEx(&win_rect, style, FALSE, exstyle);
    int win_w = win_rect.right - win_rect.left;
    int win_h = win_rect.bottom - win_rect.top;

    {
        int min_client_w = 0;
        int min_client_h = 0;
        RECT min_rect;
        GetLauncherMinimumClientSize(launcher.dpi, &min_client_w, &min_client_h);
        min_rect.left = 0;
        min_rect.top = 0;
        min_rect.right = min_client_w;
        min_rect.bottom = min_client_h;
        AdjustWindowRectEx(&min_rect, style, FALSE, exstyle);

        int min_win_w = min_rect.right - min_rect.left;
        int min_win_h = min_rect.bottom - min_rect.top;

        if (launcher_width_x > 0)
        {
            win_w = launcher_width_x;
        }
        if (launcher_width_y > 0)
        {
            win_h = launcher_width_y;
        }

        if (win_w < min_win_w)
        {
            win_w = min_win_w;
        }
        if (win_h < min_win_h)
        {
            win_h = min_win_h;
        }
    }

    RECT work_rect;
    work_rect.left = 0;
    work_rect.top = 0;
    work_rect.right = 0;
    work_rect.bottom = 0;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work_rect, 0);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    if (work_rect.right > work_rect.left && work_rect.bottom > work_rect.top)
    {
        screen_w = work_rect.right - work_rect.left;
        screen_h = work_rect.bottom - work_rect.top;
    }

    if (win_w > screen_w - ScaleByDPI(16, launcher.dpi))
    {
        win_w = screen_w - ScaleByDPI(16, launcher.dpi);
    }
    if (win_h > screen_h - ScaleByDPI(16, launcher.dpi))
    {
        win_h = screen_h - ScaleByDPI(16, launcher.dpi);
    }
    int work_left = (work_rect.right > work_rect.left) ? work_rect.left : 0;
    int work_top = (work_rect.bottom > work_rect.top) ? work_rect.top : 0;
    int work_right = work_left + screen_w;
    int work_bottom = work_top + screen_h;
    int pos_x = work_left + (screen_w - win_w) / 2;
    int pos_y = work_top + (screen_h - win_h) / 2;

    if (launcher_position_x != 0 || launcher_position_y != 0)
    {
        pos_x = launcher_position_x;
        pos_y = launcher_position_y;

        if (pos_x < work_left)
        {
            pos_x = work_left;
        }
        if (pos_y < work_top)
        {
            pos_y = work_top;
        }
        if (pos_x + win_w > work_right)
        {
            pos_x = work_right - win_w;
        }
        if (pos_y + win_h > work_bottom)
        {
            pos_y = work_bottom - win_h;
        }
    }

    HWND hwnd = CreateWindowExA(exstyle,
                                IWAD_LAUNCHER_CLASS_NAME,
                                resolved_title,
                                style,
                                pos_x, pos_y, win_w, win_h,
                                NULL, NULL, GetModuleHandle(NULL), &launcher);

    if (hwnd == NULL)
    {
        CleanupLauncherFont(&launcher);
        CleanupLauncherTheme(&launcher);
        free(launcher.additional_params);
        D_FreeIWADSearchResults(launcher.iwads);
        return true;
    }

    SetWindowTextA(hwnd, resolved_title);
    SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
    SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);
    TryEnableDarkTitleBar(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!launcher.done)
    {
        int gm = GetMessage(&msg, NULL, 0, 0);
        if (gm <= 0)
        {
            launcher.play_pressed = false;
            break;
        }

        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
         && msg.wParam == VK_ESCAPE
         && GetAncestor(msg.hwnd, GA_ROOT) == hwnd)
        {
            FinishIWADLauncher(&launcher, false);
            continue;
        }

        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
         && msg.wParam == VK_RETURN
         && GetAncestor(msg.hwnd, GA_ROOT) == hwnd)
        {
            HWND focused = GetFocus();
            if (launcher.view_mode == LAUNCHER_VIEW_IWAD
             && (focused == launcher.iwad_list || focused == launcher.params_edit))
            {
                FinishIWADLauncher(&launcher, true);
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (launcher.iwads[launcher.selected_iwad].iwad != NULL)
    {
        SaveDefaultIWADValue(launcher.iwads[launcher.selected_iwad].path);
    }

    SaveLauncherCommandLineValue(launcher.additional_params);

    if (!launcher.play_pressed)
    {
        M_SaveDefaults();
    }

    if (launcher.play_pressed)
    {
        if (launcher.iwads[launcher.selected_iwad].iwad != NULL)
        {
            D_AppendCommandLineArgument("-iwad");
            D_AppendCommandLineArgument(launcher.iwads[launcher.selected_iwad].path);
        }

        D_AppendAdditionalCommandLine(launcher.additional_params);
    }

    CleanupLauncherFont(&launcher);
    CleanupLauncherTheme(&launcher);
    free(launcher.additional_params);
    D_FreeIWADSearchResults(launcher.iwads);

    return launcher.play_pressed;
}
#endif

boolean D_MaybeShowIWADLauncher(int mask)
{
    if (M_ParmExists("-nogui")
     || M_CheckParmWithArgs("-nogui", 1) != 0       // [JN] Whatever it is, it doesn't belong in this world
     || M_CheckParmWithArgs("-iwad", 1) != 0
//   || M_CheckParmWithArgs("-file", 1) != 0        // [JN] May be just a graphical wad...
     || M_CheckParmWithArgs("-dedicated", 1) != 0   // [JN] Netplay using own textscreen launcher
     || M_CheckParmWithArgs("-search", 1) != 0
     || M_CheckParmWithArgs("-query", 1) != 0
     || M_CheckParmWithArgs("-localsearch", 1) != 0)
    {
        return true;
    }

#if defined(_WIN32) && !defined(_WIN32_WCE)
    return RunIWADLauncherDialog(mask);
#else
    (void) mask;
    return true;
#endif
}
