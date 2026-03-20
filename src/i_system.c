//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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
//



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "SDL.h"

#include "config.h"

#include "deh_str.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_timer.h"
#include "i_video.h"

#include "i_system.h"

#include "w_wad.h"
#include "z_zone.h"

#define DEFAULT_RAM 16*2 /* MiB [crispy] */
#define MIN_RAM     4*4  /* MiB [crispy] */
#define I_ERROR_MESSAGE_BUFSIZE 4096


typedef struct atexit_listentry_s atexit_listentry_t;

struct atexit_listentry_s
{
    atexit_func_t func;
    boolean run_on_error;
    atexit_listentry_t *next;
};

static atexit_listentry_t *exit_funcs = NULL;

void I_AtExit(atexit_func_t func, boolean run_on_error)
{
    atexit_listentry_t *entry;

    entry = malloc(sizeof(*entry));

    entry->func = func;
    entry->run_on_error = run_on_error;
    entry->next = exit_funcs;
    exit_funcs = entry;
}

// Zone memory auto-allocation function that allocates the zone size
// by trying progressively smaller zone sizes until one is found that
// works.

static byte *AutoAllocMemory(int *size, int default_ram, int min_ram)
{
    byte *zonemem;

    // Allocate the zone memory.  This loop tries progressively smaller
    // zone sizes until a size is found that can be allocated.
    // If we used the -mb command line parameter, only the parameter
    // provided is accepted.

    zonemem = NULL;

    while (zonemem == NULL)
    {
        // We need a reasonable minimum amount of RAM to start.

        if (default_ram < min_ram)
        {
            I_Error("Unable to allocate %i MiB of RAM for zone", default_ram);
        }

        // Try to allocate the zone memory.

        *size = default_ram * 1024 * 1024;

        zonemem = malloc(*size);

        // Failed to allocate?  Reduce zone size until we reach a size
        // that is acceptable.

        if (zonemem == NULL)
        {
            default_ram -= 1;
        }
    }

    return zonemem;
}

byte *I_ZoneBase (int *size)
{
    byte *zonemem;
    int min_ram, default_ram;
    int p;
    static int i = 1;

    //!
    // @category obscure
    // @arg <mb>
    //
    // Specify the heap size, in MiB (default 16).
    //

    p = M_CheckParmWithArgs("-mb", 1);

    if (p > 0)
    {
        default_ram = atoi(myargv[p+1]);
        min_ram = default_ram;
    }
    else
    {
        default_ram = DEFAULT_RAM;
        min_ram = MIN_RAM;
    }

    // [crispy] do not allocate new zones ad infinitum
    if (i > 16)
    {
        min_ram = default_ram + 1;
    }

    zonemem = AutoAllocMemory(size, default_ram * i, min_ram * i);

    // [crispy] if called again, allocate another zone twice as big
    i *= 2;

    printf("  zone memory: %p, %x MiB allocated for zone.\n", 
           (void*)zonemem, *size >> 20); // [crispy] human-understandable zone heap size

    return zonemem;
}

void I_PrintBanner(const char *msg)
{
    int i;
    int spaces = 35 - (strlen(msg) / 2);

    for (i=0; i<spaces; ++i)
        putchar(' ');

    puts(msg);
}

void I_PrintDivider(void)
{
    int i;

    for (i=0; i<75; ++i)
    {
        putchar('=');
    }

    putchar('\n');
}

void I_PrintStartupBanner(const char *gamedescription)
{
    I_PrintDivider();
    I_PrintBanner(gamedescription);
    I_PrintDivider();
    
    printf(
    " " PACKAGE_NAME "-Doom is free software, covered by the GNU General Public License.\n"
    " There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
    " FOR A PARTICULAR PURPOSE. You are welcome to change and distribute\n"
    " copies under certain conditions. See the source for more information.\n");

    I_PrintDivider();
}

//
// I_Init
//
/*
void I_Init (void)
{
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
}
void I_BindVariables(void)
{
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();
}
*/

//
// I_Quit
//

void I_Quit (void)
{
    atexit_listentry_t *entry;

    // Run through all exit functions
 
    entry = exit_funcs; 

    while (entry != NULL)
    {
        entry->func();
        entry = entry->next;
    }

    SDL_Quit();

    exit(0);
}



//
// I_Error
//

static boolean already_quitting = false;

#ifdef _WIN32
#define I_ERROR_DIALOG_CLASS_NAME L"InterErrorDialogWindow"
#define I_ERROR_DIALOG_BTN_OLDPROC "InterErrorDialogBtnOldProc"
#define I_ERROR_DIALOG_BTN_HOVER "InterErrorDialogBtnHover"
#define IDC_I_ERROR_DIALOG_OK 6002
#define I_ERROR_DIALOG_ICON_SIZE 34
#define I_ERROR_DIALOG_BUTTON_W 88
#define I_ERROR_DIALOG_BUTTON_H 28
#define I_ERROR_DIALOG_MARGIN 14
#define I_ERROR_DIALOG_GAP 10
#define I_ERROR_DIALOG_TEXT_MAX_W 360
#define I_ERROR_DIALOG_MIN_W 340
#define I_ERROR_DIALOG_MAX_W 520
#define I_ERROR_DIALOG_MIN_H 150

#define I_ERROR_COLOR_WINDOW_BG     RGB(43, 43, 43)
#define I_ERROR_COLOR_BUTTON_BG     RGB(58, 58, 58)
#define I_ERROR_COLOR_BUTTON_HOVER  RGB(78, 78, 78)
#define I_ERROR_COLOR_BUTTON_BORDER RGB(96, 96, 96)
#define I_ERROR_COLOR_TEXT          RGB(235, 235, 235)
#define I_ERROR_ICON_RESOURCE_INFO  MAKEINTRESOURCEW(32516)
#define I_ERROR_ICON_RESOURCE_ERROR MAKEINTRESOURCEW(32513)

#ifndef SIID_INFO
#define SIID_INFO 79
#endif

#ifndef SIID_ERROR
#define SIID_ERROR 80
#endif

#ifndef SHGSI_ICON
#define SHGSI_ICON 0x000000100
#endif

#ifndef SHGSI_LARGEICON
#define SHGSI_LARGEICON 0x000000000
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

typedef HRESULT (WINAPI *dwm_set_window_attribute_t)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI *sh_get_stock_icon_info_t)(int, UINT, void *);

typedef struct
{
    const wchar_t *message;
    const wchar_t *title;
    boolean safe;
    HWND window;
    HWND icon_control;
    HWND ok_button;
    HICON icon;
    boolean owns_icon;
    int dpi;
    HFONT font;
    RECT message_rect;
    HBRUSH window_brush;
    HBRUSH button_brush;
    HBRUSH button_hover_brush;
    HBRUSH button_border_brush;
    BOOL done;
} i_error_dialog_t;

typedef struct
{
    WNDPROC proc;
} i_error_proc_holder_t;

// -----------------------------------------------------------------------------
// GetStoredErrorDialogProc
//  [PN] Read stored original proc for error-dialog button subclass.
// -----------------------------------------------------------------------------

static WNDPROC GetStoredErrorDialogProc(HWND hwnd)
{
    i_error_proc_holder_t *holder =
        (i_error_proc_holder_t *) GetPropA(hwnd, I_ERROR_DIALOG_BTN_OLDPROC);

    return holder != NULL ? holder->proc : NULL;
}

// -----------------------------------------------------------------------------
// SetStoredErrorDialogProc
//  [PN] Store original button proc in heap holder attached as window property.
// -----------------------------------------------------------------------------

static boolean SetStoredErrorDialogProc(HWND hwnd, WNDPROC proc)
{
    i_error_proc_holder_t *holder;

    if (hwnd == NULL || proc == NULL)
    {
        return false;
    }

    holder = (i_error_proc_holder_t *) malloc(sizeof(*holder));
    if (holder == NULL)
    {
        return false;
    }

    holder->proc = proc;

    if (!SetPropA(hwnd, I_ERROR_DIALOG_BTN_OLDPROC, (HANDLE) holder))
    {
        free(holder);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// ClearStoredErrorDialogProc
//  [PN] Remove and free stored proc holder for error-dialog button subclass.
// -----------------------------------------------------------------------------

static void ClearStoredErrorDialogProc(HWND hwnd)
{
    i_error_proc_holder_t *holder =
        (i_error_proc_holder_t *) RemovePropA(hwnd, I_ERROR_DIALOG_BTN_OLDPROC);

    if (holder != NULL)
    {
        free(holder);
    }
}

// -----------------------------------------------------------------------------
// GetErrorDialogDPI
//  [PN] Get effective DPI for error dialog (window or system fallback).
// -----------------------------------------------------------------------------

static int GetErrorDialogDPI(HWND hwnd)
{
    UINT dpi = 96;
    HMODULE user32 = LoadLibraryA("user32.dll");

    if (user32 != NULL)
    {
        typedef UINT (WINAPI *get_dpi_for_window_t)(HWND);
        const get_dpi_for_window_t get_dpi_for_window =
            (get_dpi_for_window_t) GetProcAddress(user32, "GetDpiForWindow");

        if (get_dpi_for_window != NULL && hwnd != NULL)
        {
            const UINT window_dpi = get_dpi_for_window(hwnd);
            if (window_dpi > 0)
                dpi = window_dpi;
        }

        FreeLibrary(user32);
    }

    if (dpi == 96)
    {
        HDC dc = GetDC(hwnd);

        if (dc != NULL)
        {
            const int log_dpi = GetDeviceCaps(dc, LOGPIXELSX);
            if (log_dpi > 0)
                dpi = (UINT) log_dpi;

            ReleaseDC(hwnd, dc);
        }
    }

    return (int) dpi;
}

// -----------------------------------------------------------------------------
// ScaleErrorDialogByDPI
//  [PN] Scale pixel value from 96 DPI baseline for error dialog layout.
// -----------------------------------------------------------------------------

static int ScaleErrorDialogByDPI(int value, int dpi)
{
    if (dpi <= 0)
        dpi = 96;

    return MulDiv(value, dpi, 96);
}

// -----------------------------------------------------------------------------
// MeasureErrorDialogText
//  [PN] Measure wrapped message text to size error dialog close to MessageBox.
// -----------------------------------------------------------------------------

static void MeasureErrorDialogText(const wchar_t *message, HFONT font, int max_width,
                                   int *out_w, int *out_h)
{
    HDC dc;
    int text_w = max_width / 2;
    int text_h = 24;

    if (message == NULL || *message == L'\0')
    {
        if (out_w != NULL)
            *out_w = text_w;
        if (out_h != NULL)
            *out_h = text_h;
        return;
    }

    dc = GetDC(NULL);

    if (dc != NULL)
    {
        HFONT old_font = NULL;
        RECT rc;

        if (font != NULL)
            old_font = (HFONT) SelectObject(dc, font);

        SetRect(&rc, 0, 0, max_width, 0);

        if (DrawTextW(dc, message, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX) > 0)
        {
            text_w = rc.right - rc.left;
            text_h = rc.bottom - rc.top;
        }

        if (old_font != NULL)
            SelectObject(dc, old_font);

        ReleaseDC(NULL, dc);
    }

    if (text_w < 80)
        text_w = 80;
    if (text_h < 20)
        text_h = 20;

    if (out_w != NULL)
        *out_w = text_w;
    if (out_h != NULL)
        *out_h = text_h;
}

// -----------------------------------------------------------------------------
// TryEnableErrorDarkTitleBar
//  [PN] Try enabling dark title bar for error dialog on supported Windows.
// -----------------------------------------------------------------------------

static void TryEnableErrorDarkTitleBar(HWND hwnd)
{
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
    if (dwmapi == NULL)
        return;

    const dwm_set_window_attribute_t set_window_attribute =
        (dwm_set_window_attribute_t) GetProcAddress(dwmapi, "DwmSetWindowAttribute");

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
// LoadErrorDialogIcon
//  [PN] Load info/error icon, scaled for dialog DPI.
// -----------------------------------------------------------------------------

static HICON LoadErrorDialogIcon(boolean safe, boolean *owns_icon, int icon_size)
{
    const PCWSTR icon_resource = safe ? I_ERROR_ICON_RESOURCE_INFO : I_ERROR_ICON_RESOURCE_ERROR;
    HMODULE shell32 = LoadLibraryA("shell32.dll");

    if (owns_icon != NULL)
        *owns_icon = false;

    if (shell32 != NULL)
    {
        typedef struct
        {
            DWORD cbSize;
            HICON hIcon;
            int iSysImageIndex;
            int iIcon;
            WCHAR szPath[MAX_PATH];
        } i_error_shstockiconinfo_t;

        const sh_get_stock_icon_info_t get_stock_icon_info =
            (sh_get_stock_icon_info_t) GetProcAddress(shell32, "SHGetStockIconInfo");

        if (get_stock_icon_info != NULL)
        {
            i_error_shstockiconinfo_t info;
            memset(&info, 0, sizeof(info));
            info.cbSize = sizeof(info);

            if (SUCCEEDED(get_stock_icon_info(safe ? SIID_INFO : SIID_ERROR, SHGSI_ICON | SHGSI_LARGEICON, &info))
             && info.hIcon != NULL)
            {
                HICON scaled_icon = (HICON) CopyImage(info.hIcon, IMAGE_ICON, icon_size, icon_size, 0);

                if (scaled_icon != NULL)
                {
                    DestroyIcon(info.hIcon);
                    info.hIcon = scaled_icon;
                }

                if (owns_icon != NULL)
                    *owns_icon = true;

                FreeLibrary(shell32);
                return info.hIcon;
            }
        }

        FreeLibrary(shell32);
    }

    {
        HICON scaled_icon = (HICON) LoadImageW(NULL, icon_resource, IMAGE_ICON, icon_size, icon_size, 0);
        if (scaled_icon != NULL)
        {
            if (owns_icon != NULL)
                *owns_icon = true;
            return scaled_icon;
        }
    }

    return LoadIconW(NULL, icon_resource);
}

// -----------------------------------------------------------------------------
// CreateErrorDialogFont
//  [PN] Create system message font for error dialog controls.
// -----------------------------------------------------------------------------

static HFONT CreateErrorDialogFont(boolean *owns_font)
{
    NONCLIENTMETRICSW metrics;

    if (owns_font != NULL)
        *owns_font = false;

    memset(&metrics, 0, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);

    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        HFONT font = CreateFontIndirectW(&metrics.lfMessageFont);
        if (font != NULL)
        {
            if (owns_font != NULL)
                *owns_font = true;
            return font;
        }
    }

    return (HFONT) GetStockObject(DEFAULT_GUI_FONT);
}

// -----------------------------------------------------------------------------
// LayoutErrorDialog
//  [PN] Reposition controls in dark error dialog for current client size.
// -----------------------------------------------------------------------------

static void LayoutErrorDialog(i_error_dialog_t *dialog)
{
    RECT rc;

    if (dialog == NULL || dialog->window == NULL)
        return;

    const int margin = ScaleErrorDialogByDPI(I_ERROR_DIALOG_MARGIN, dialog->dpi);
    const int gap = ScaleErrorDialogByDPI(I_ERROR_DIALOG_GAP, dialog->dpi);
    const int icon_size = ScaleErrorDialogByDPI(I_ERROR_DIALOG_ICON_SIZE, dialog->dpi);
    const int button_w = ScaleErrorDialogByDPI(I_ERROR_DIALOG_BUTTON_W, dialog->dpi);
    const int button_h = ScaleErrorDialogByDPI(I_ERROR_DIALOG_BUTTON_H, dialog->dpi);

    GetClientRect(dialog->window, &rc);

    const int message_left = margin + icon_size + gap;
    const int message_top = margin;
    const int button_x = rc.right - margin - button_w;
    const int button_y = rc.bottom - margin - button_h;
    int message_w = rc.right - message_left - margin;
    int message_h = button_y - gap - message_top;

    if (message_w < 80)
        message_w = 80;
    if (message_h < 24)
        message_h = 24;

    if (dialog->icon_control != NULL)
        MoveWindow(dialog->icon_control, margin, message_top, icon_size, icon_size, TRUE);

    if (dialog->ok_button != NULL)
        MoveWindow(dialog->ok_button, button_x, button_y, button_w, button_h, TRUE);

    dialog->message_rect.left = message_left;
    dialog->message_rect.top = message_top;
    dialog->message_rect.right = message_left + message_w;
    dialog->message_rect.bottom = message_top + message_h;
}

// -----------------------------------------------------------------------------
// DrawErrorDialogButton
//  [PN] Draw owner-drawn OK button with launcher-like dark palette.
// -----------------------------------------------------------------------------

static void DrawErrorDialogButton(const DRAWITEMSTRUCT *dis,
                                  const i_error_dialog_t *dialog)
{
    RECT rc = dis->rcItem;
    static const wchar_t caption[] = L"OK";
    HBRUSH bg_brush = dialog->button_brush;

    if (GetPropA(dis->hwndItem, I_ERROR_DIALOG_BTN_HOVER) != NULL
     && dialog->button_hover_brush != NULL)
        bg_brush = dialog->button_hover_brush;

    FillRect(dis->hDC, &rc, bg_brush);
    FrameRect(dis->hDC, &rc, dialog->button_border_brush);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, I_ERROR_COLOR_TEXT);
    DrawTextW(dis->hDC, caption, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// -----------------------------------------------------------------------------
// ErrorDialogButtonHoverProc
//  [PN] Track hover state for owner-drawn OK button.
// -----------------------------------------------------------------------------

static LRESULT CALLBACK ErrorDialogButtonHoverProc(HWND hwnd, UINT msg,
                                                   WPARAM wparam, LPARAM lparam)
{
    const WNDPROC old_proc = GetStoredErrorDialogProc(hwnd);

    if (old_proc == NULL)
    {
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_MOUSEMOVE && GetPropA(hwnd, I_ERROR_DIALOG_BTN_HOVER) == NULL)
    {
        TRACKMOUSEEVENT track;
        memset(&track, 0, sizeof(track));
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);

        SetPropA(hwnd, I_ERROR_DIALOG_BTN_HOVER, (HANDLE) (INT_PTR) 1);
        InvalidateRect(hwnd, NULL, FALSE);
    }
    else if (msg == WM_MOUSELEAVE && GetPropA(hwnd, I_ERROR_DIALOG_BTN_HOVER) != NULL)
    {
        RemovePropA(hwnd, I_ERROR_DIALOG_BTN_HOVER);
        InvalidateRect(hwnd, NULL, FALSE);
    }
    else if (msg == WM_NCDESTROY)
    {
        RemovePropA(hwnd, I_ERROR_DIALOG_BTN_HOVER);
        ClearStoredErrorDialogProc(hwnd);
    }

    return CallWindowProcW(old_proc, hwnd, msg, wparam, lparam);
}

// -----------------------------------------------------------------------------
// InstallErrorDialogButtonHover
//  [PN] Install hover tracking subclass for OK button.
// -----------------------------------------------------------------------------

static void InstallErrorDialogButtonHover(HWND hwnd)
{
    if (hwnd == NULL)
        return;

    const LONG_PTR old_proc = GetWindowLongPtr(hwnd, GWLP_WNDPROC);

    if (old_proc != 0 && old_proc != (LONG_PTR) ErrorDialogButtonHoverProc)
    {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) ErrorDialogButtonHoverProc);

        if (!SetStoredErrorDialogProc(hwnd, (WNDPROC) old_proc))
        {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, old_proc);
        }
    }
}

// -----------------------------------------------------------------------------
// I_ErrorDialogWndProc
//  [PN] Handle dark Windows error dialog events, drawing and layout.
// -----------------------------------------------------------------------------

static LRESULT CALLBACK I_ErrorDialogWndProc(HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam)
{
    i_error_dialog_t *dialog = (i_error_dialog_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_NCCREATE:
        {
            const CREATESTRUCTW *create = (CREATESTRUCTW *) lparam;
            dialog = (i_error_dialog_t *) create->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) dialog);
            break;
        }

        case WM_CREATE:
            if (dialog == NULL)
                break;

            dialog->window = hwnd;
            dialog->dpi = GetErrorDialogDPI(hwnd);
            dialog->icon_control = CreateWindowExW(0, L"STATIC", NULL,
                                                   WS_CHILD | WS_VISIBLE | SS_ICON,
                                                   0, 0, 0, 0,
                                                   hwnd, NULL, GetModuleHandle(NULL), NULL);
            dialog->ok_button = CreateWindowExW(0, L"BUTTON", L"OK",
                                                WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                              | BS_OWNERDRAW | BS_PUSHBUTTON,
                                                0, 0, 0, 0,
                                                hwnd,
                                                (HMENU) (INT_PTR) IDC_I_ERROR_DIALOG_OK,
                                                GetModuleHandle(NULL), NULL);

            if (dialog->icon_control != NULL)
            {
                dialog->icon = LoadErrorDialogIcon(dialog->safe, &dialog->owns_icon,
                                                   ScaleErrorDialogByDPI(I_ERROR_DIALOG_ICON_SIZE,
                                                                         dialog->dpi));
                SendMessageW(dialog->icon_control, STM_SETICON, (WPARAM) dialog->icon, 0);
            }

            if (dialog->font != NULL)
            {
                if (dialog->ok_button != NULL)
                    SendMessageW(dialog->ok_button, WM_SETFONT, (WPARAM) dialog->font, TRUE);
            }

            InstallErrorDialogButtonHover(dialog->ok_button);
            LayoutErrorDialog(dialog);
            break;

        case WM_SIZE:
            if (dialog != NULL)
                LayoutErrorDialog(dialog);
            return 0;

        case WM_DPICHANGED:
            if (dialog != NULL)
            {
                const RECT *new_rect = (RECT *) lparam;
                dialog->dpi = HIWORD(wparam);

                if (new_rect != NULL)
                {
                    SetWindowPos(hwnd, NULL, new_rect->left, new_rect->top,
                                 new_rect->right - new_rect->left,
                                 new_rect->bottom - new_rect->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }

                if (dialog->icon_control != NULL)
                {
                    const boolean old_owns_icon = dialog->owns_icon;
                    const HICON old_icon = dialog->icon;
                    boolean new_owns_icon = false;
                    const HICON new_icon =
                        LoadErrorDialogIcon(dialog->safe, &new_owns_icon,
                                            ScaleErrorDialogByDPI(I_ERROR_DIALOG_ICON_SIZE, dialog->dpi));

                    if (new_icon != NULL)
                    {
                        if (old_owns_icon && old_icon != NULL)
                        {
                            DestroyIcon(old_icon);
                        }

                        dialog->icon = new_icon;
                        dialog->owns_icon = new_owns_icon;
                        SendMessageW(dialog->icon_control, STM_SETICON, (WPARAM) dialog->icon, 0);
                    }
                }

                LayoutErrorDialog(dialog);
            }
            return 0;

        case WM_ERASEBKGND:
            if (dialog != NULL && dialog->window_brush != NULL)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                FillRect((HDC) wparam, &rc, dialog->window_brush);
                return 1;
            }
            break;

        case WM_PAINT:
            if (dialog != NULL)
            {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(hwnd, &ps);

                if (dc != NULL)
                {
                    HFONT old_font = NULL;
                    RECT text_rc = dialog->message_rect;

                    if (dialog->font != NULL)
                    {
                        old_font = (HFONT) SelectObject(dc, dialog->font);
                    }

                    SetBkMode(dc, TRANSPARENT);
                    SetTextColor(dc, I_ERROR_COLOR_TEXT);
                    DrawTextW(dc, dialog->message, -1, &text_rc, DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

                    if (old_font != NULL)
                    {
                        SelectObject(dc, old_font);
                    }
                }

                EndPaint(hwnd, &ps);
                return 0;
            }
            break;

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = (HDC) wparam;

            if (dialog != NULL && dialog->window_brush != NULL)
            {
                SetTextColor(dc, I_ERROR_COLOR_TEXT);
                SetBkColor(dc, I_ERROR_COLOR_WINDOW_BG);
                return (LRESULT) dialog->window_brush;
            }
            break;
        }

        case WM_DRAWITEM:
            if (dialog != NULL)
            {
                const DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *) lparam;
                if (dis->CtlID == IDC_I_ERROR_DIALOG_OK)
                {
                    DrawErrorDialogButton(dis, dialog);
                    return TRUE;
                }
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wparam) == IDC_I_ERROR_DIALOG_OK
             && HIWORD(wparam) == BN_CLICKED)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (dialog != NULL)
                dialog->done = TRUE;
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// -----------------------------------------------------------------------------
// ShowDarkErrorDialogW
//  [PN] Show dark-themed Windows error dialog matching launcher visuals.
// -----------------------------------------------------------------------------

static void ShowDarkErrorDialogW(const wchar_t *message, const wchar_t *title,
                                 boolean safe)
{
    WNDCLASSW wc;
    MSG msg;
    const HINSTANCE instance = GetModuleHandle(NULL);
    const DWORD window_style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD window_exstyle = WS_EX_DLGMODALFRAME;
    i_error_dialog_t dialog;
    boolean owns_font;
    int x, y;
    int window_w, window_h;

    memset(&dialog, 0, sizeof(dialog));
    memset(&wc, 0, sizeof(wc));

    dialog.message = message;
    dialog.title = title;
    dialog.safe = safe;
    dialog.dpi = 96;
    dialog.window_brush = CreateSolidBrush(I_ERROR_COLOR_WINDOW_BG);
    dialog.button_brush = CreateSolidBrush(I_ERROR_COLOR_BUTTON_BG);
    dialog.button_hover_brush = CreateSolidBrush(I_ERROR_COLOR_BUTTON_HOVER);
    dialog.button_border_brush = CreateSolidBrush(I_ERROR_COLOR_BUTTON_BORDER);
    dialog.font = CreateErrorDialogFont(&owns_font);

    wc.lpfnWndProc = I_ErrorDialogWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = I_ERROR_DIALOG_CLASS_NAME;
    RegisterClassW(&wc);

    {
        const int dpi = GetErrorDialogDPI(NULL);
        const int margin = ScaleErrorDialogByDPI(I_ERROR_DIALOG_MARGIN, dpi);
        const int gap = ScaleErrorDialogByDPI(I_ERROR_DIALOG_GAP, dpi);
        const int icon_size = ScaleErrorDialogByDPI(I_ERROR_DIALOG_ICON_SIZE, dpi);
        const int button_w = ScaleErrorDialogByDPI(I_ERROR_DIALOG_BUTTON_W, dpi);
        const int button_h = ScaleErrorDialogByDPI(I_ERROR_DIALOG_BUTTON_H, dpi);
        const int max_text_w = ScaleErrorDialogByDPI(I_ERROR_DIALOG_TEXT_MAX_W, dpi);
        const int min_w = ScaleErrorDialogByDPI(I_ERROR_DIALOG_MIN_W, dpi);
        const int max_w = ScaleErrorDialogByDPI(I_ERROR_DIALOG_MAX_W, dpi);
        const int min_h = ScaleErrorDialogByDPI(I_ERROR_DIALOG_MIN_H, dpi);
        const int max_screen_w = GetSystemMetrics(SM_CXSCREEN) - ScaleErrorDialogByDPI(20, dpi);
        const int max_screen_h = GetSystemMetrics(SM_CYSCREEN) - ScaleErrorDialogByDPI(40, dpi);
        int client_w;
        int client_h;
        int text_w, text_h;
        int row_h;

        MeasureErrorDialogText(message, dialog.font, max_text_w, &text_w, &text_h);
        row_h = text_h > icon_size ? text_h : icon_size;

        client_w = margin + icon_size + gap + text_w + margin;
        if (client_w < min_w)
            client_w = min_w;
        if (client_w < margin + button_w + margin)
            client_w = margin + button_w + margin;
        if (client_w > max_w)
            client_w = max_w;

        client_h = margin + row_h + gap + button_h + margin;
        if (client_h < min_h)
            client_h = min_h;

        {
            RECT wr = { 0, 0, client_w, client_h };
            if (AdjustWindowRectEx(&wr, window_style, FALSE, window_exstyle))
            {
                window_w = wr.right - wr.left;
                window_h = wr.bottom - wr.top;
            }
            else
            {
                window_w = client_w;
                window_h = client_h;
            }
        }

        if (window_w > max_screen_w)
            window_w = max_screen_w;
        if (window_h > max_screen_h)
            window_h = max_screen_h;
    }

    x = (GetSystemMetrics(SM_CXSCREEN) - window_w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - window_h) / 2;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    dialog.window = CreateWindowExW(window_exstyle,
                                    I_ERROR_DIALOG_CLASS_NAME,
                                    dialog.title,
                                    window_style,
                                    x, y,
                                    window_w, window_h,
                                    NULL, NULL, instance, &dialog);

    if (dialog.window == NULL)
    {
        MessageBoxW(NULL, message, title, safe ? MB_ICONASTERISK : MB_ICONSTOP);
        goto cleanup;
    }

    TryEnableErrorDarkTitleBar(dialog.window);
    ShowWindow(dialog.window, SW_SHOW);
    UpdateWindow(dialog.window);
    SetForegroundWindow(dialog.window);

    if (dialog.ok_button != NULL)
        SetFocus(dialog.ok_button);

    while (!dialog.done && GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageW(dialog.window, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (!dialog.done && dialog.window != NULL && IsWindow(dialog.window))
    {
        DestroyWindow(dialog.window);
    }

cleanup:
    if (dialog.window_brush != NULL)
        DeleteObject(dialog.window_brush);
    if (dialog.button_brush != NULL)
        DeleteObject(dialog.button_brush);
    if (dialog.button_border_brush != NULL)
        DeleteObject(dialog.button_border_brush);
    if (dialog.button_hover_brush != NULL)
        DeleteObject(dialog.button_hover_brush);
    if (owns_font && dialog.font != NULL)
        DeleteObject(dialog.font);
    if (dialog.owns_icon && dialog.icon != NULL)
        DestroyIcon(dialog.icon);
}
#endif

// [JN] Indicates if I_Error is not an error, but infomative message instead.
// If false, a stop-sign icon appears.
// If true, an icon consisting of a lowercase letter i in a circle appears.
boolean i_error_safe = false;

// [JN] Different games using different I_Error dialogue box titles.
// Just in case, set a generic title first.
char *i_error_title = "Program quit";

void I_Error (const char *error, ...)
{
    char msgbuf[I_ERROR_MESSAGE_BUFSIZE];
    va_list argptr;
    atexit_listentry_t *entry;
    boolean exit_gui_popup;

    if (already_quitting)
    {
        fprintf(stderr, "Warning: recursive call to I_Error detected.\n");
        exit(-1);
    }
    else
    {
        already_quitting = true;
    }

    // Message first.
    va_start(argptr, error);
    //fprintf(stderr, "\nError: ");
    vfprintf(stderr, error, argptr);
    fprintf(stderr, "\n\n");
    va_end(argptr);
    fflush(stderr);

    // Write a copy of the message into buffer.
    va_start(argptr, error);
    memset(msgbuf, 0, sizeof(msgbuf));
    M_vsnprintf(msgbuf, sizeof(msgbuf), error, argptr);
    va_end(argptr);

    // Shutdown. Here might be other errors.

    entry = exit_funcs;

    while (entry != NULL)
    {
        if (entry->run_on_error)
        {
            entry->func();
        }

        entry = entry->next;
    }

    //!
    // @category obscure
    //
    // If specified, don't show a GUI window for error messages when the
    // game exits with an error.
    //
    exit_gui_popup = !M_ParmExists("-nogui");

    // Pop up a GUI dialog box to show the error message, if the
    // game was not run from the console (and the user will
    // therefore be unable to otherwise see the message).
    if (exit_gui_popup)
    {
#ifdef _WIN32
        // [JN] UTF-8 retranslations of error message and window title.
        wchar_t win_error_message[I_ERROR_MESSAGE_BUFSIZE];
        wchar_t win_error_title[128];

        // [PN] On Windows use custom dark-themed dialog.
        MultiByteToWideChar(CP_UTF8, 0, msgbuf, -1, win_error_message, I_ERROR_MESSAGE_BUFSIZE);
        MultiByteToWideChar(CP_UTF8, 0, i_error_title, -1, win_error_title, 128);
        ShowDarkErrorDialogW(win_error_message, win_error_title, i_error_safe);
#else
        SDL_ShowSimpleMessageBox(i_error_safe ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_ERROR,
                                 i_error_title, msgbuf, NULL);
#endif
    }

    // abort();

    SDL_Quit();

    exit(-1);
}

//
// I_Realloc
//

void *I_Realloc(void *ptr, size_t size)
{
    void *new_ptr;

    new_ptr = realloc(ptr, size);

    if (size != 0 && new_ptr == NULL)
    {
        I_Error ("I_Realloc: failed on reallocation of %llu bytes", (long long unsigned)size);
    }

    return new_ptr;
}

//
// Read Access Violation emulation.
//
// From PrBoom+, by entryway.
//

// C:\>debug
// -d 0:0
//
// DOS 6.22:
// 0000:0000  (57 92 19 00) F4 06 70 00-(16 00)
// DOS 7.1:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// Win98:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// DOSBox under XP:
// 0000:0000  (00 00 00 F1) ?? ?? ?? 00-(07 00)

#define DOS_MEM_DUMP_SIZE 10

static const unsigned char mem_dump_dos622[DOS_MEM_DUMP_SIZE] = {
  0x57, 0x92, 0x19, 0x00, 0xF4, 0x06, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_win98[DOS_MEM_DUMP_SIZE] = {
  0x9E, 0x0F, 0xC9, 0x00, 0x65, 0x04, 0x70, 0x00, 0x16, 0x00};
static const unsigned char mem_dump_dosbox[DOS_MEM_DUMP_SIZE] = {
  0x00, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
static unsigned char mem_dump_custom[DOS_MEM_DUMP_SIZE];

static const unsigned char *dos_mem_dump = mem_dump_dos622;

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    static boolean firsttime = true;

    if (firsttime)
    {
        int p, i, val;

        firsttime = false;
        i = 0;

        //!
        // @category compat
        // @arg <version>
        //
        // Specify DOS version to emulate for NULL pointer dereference
        // emulation.  Supported versions are: dos622, dos71, dosbox.
        // The default is to emulate DOS 7.1 (Windows 98).
        //

        p = M_CheckParmWithArgs("-setmem", 1);

        if (p > 0)
        {
            if (!strcasecmp(myargv[p + 1], "dos622"))
            {
                dos_mem_dump = mem_dump_dos622;
            }
            if (!strcasecmp(myargv[p + 1], "dos71"))
            {
                dos_mem_dump = mem_dump_win98;
            }
            else if (!strcasecmp(myargv[p + 1], "dosbox"))
            {
                dos_mem_dump = mem_dump_dosbox;
            }
            else
            {
                for (i = 0; i < DOS_MEM_DUMP_SIZE; ++i)
                {
                    ++p;

                    if (p >= myargc || myargv[p][0] == '-')
                    {
                        break;
                    }

                    M_StrToInt(myargv[p], &val);
                    mem_dump_custom[i++] = (unsigned char) val;
                }

                dos_mem_dump = mem_dump_custom;
            }
        }
    }

    switch (size)
    {
    case 1:
        *((unsigned char *) value) = dos_mem_dump[offset];
        return true;
    case 2:
        *((unsigned short *) value) = dos_mem_dump[offset]
                                    | (dos_mem_dump[offset + 1] << 8);
        return true;
    case 4:
        *((unsigned int *) value) = dos_mem_dump[offset]
                                  | (dos_mem_dump[offset + 1] << 8)
                                  | (dos_mem_dump[offset + 2] << 16)
                                  | (dos_mem_dump[offset + 3] << 24);
        return true;
    }

    return false;
}
