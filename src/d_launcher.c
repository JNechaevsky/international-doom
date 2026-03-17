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

#include "d_launcher.h"
#include "d_iwad.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"

#if defined(_WIN32) && !defined(_WIN32_WCE)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

#define IWAD_LAUNCHER_CLASS_NAME "InterIwadLauncherWindow"
#define IWAD_LAUNCHER_PROMPT     "Select IWAD file to run:"
#define IDC_IWAD_LAUNCHER_LIST   2001
#define IDC_IWAD_LAUNCHER_EDIT   2002
#define IDC_IWAD_LAUNCHER_PLAY   2003
#define IDC_IWAD_LAUNCHER_EXIT   2004

typedef struct
{
    HWND window;
    HWND prompt_label;
    HWND iwad_list;
    HWND params_label;
    HWND params_edit;
    const iwad_t **iwads;
    int dpi;
    int selected_iwad;
    char *additional_params;
    boolean play_pressed;
    boolean done;
} iwad_launcher_t;

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
    int label_h = ScaleByDPI(20, launcher->dpi);
    int edit_h = ScaleByDPI(18, launcher->dpi);
    int ideal_btn_w = ScaleByDPI(120, launcher->dpi);
    int btn_h = ScaleByDPI(30, launcher->dpi);

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

    MoveWindow(launcher->prompt_label, x, y, width, label_h, TRUE);
    y += label_h + gap;

    int bottom_reserved = margin + btn_h + gap + edit_h + gap + label_h + gap;
    int list_available = height - y - bottom_reserved;
    if (list_available < ScaleByDPI(40, launcher->dpi))
    {
        list_available = ScaleByDPI(40, launcher->dpi);
    }
    int list_h = list_available;

    MoveWindow(launcher->iwad_list, x, y, width, list_h, TRUE);
    y += list_h + gap;

    MoveWindow(launcher->params_label, x, y, width, label_h, TRUE);
    y += label_h + gap;

    MoveWindow(launcher->params_edit, x, y, width, edit_h, TRUE);

    int btn_y = height - margin - btn_h;

    MoveWindow(GetDlgItem(launcher->window, IDC_IWAD_LAUNCHER_PLAY),
               x, btn_y, btn_w, btn_h, TRUE);
    MoveWindow(GetDlgItem(launcher->window, IDC_IWAD_LAUNCHER_EXIT),
               x + width - btn_w, btn_y, btn_w, btn_h, TRUE);
}

static char *BuildIWADDisplayName(const iwad_t *iwad)
{
    const char *iwad_name = iwad->name;

    if (M_StringEndsWith(iwad_name, ".wad"))
    {
        char *short_name = M_StringDuplicate(iwad_name);
        short_name[strlen(short_name) - 4] = '\0';
        char *label = M_StringJoin(iwad->description, " (", short_name, ")", NULL);
        free(short_name);
        return label;
    }

    return M_StringJoin(iwad->description, " (", iwad_name, ")", NULL);
}

static void FinishIWADLauncher(iwad_launcher_t *launcher, boolean play_pressed)
{
    launcher->play_pressed = play_pressed;
    launcher->done = true;

    if (play_pressed)
    {
        LRESULT sel;

        sel = SendMessageA(launcher->iwad_list, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR)
        {
            launcher->selected_iwad = (int) sel;
        }

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

            launcher->iwad_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL
                                                | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                                  0, 0, 0, 0,
                                                  hwnd,
                                                  (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_LIST,
                                                  NULL, NULL);

            if (launcher->iwads[0] != NULL)
            {
                for (int i = 0; launcher->iwads[i] != NULL; ++i)
                {
                    char *label = BuildIWADDisplayName(launcher->iwads[i]);
                    SendMessageA(launcher->iwad_list, LB_ADDSTRING, 0, (LPARAM) label);
                    free(label);
                }
                SendMessageA(launcher->iwad_list, LB_SETCURSEL, 0, 0);
                launcher->selected_iwad = 0;
            }
            else
            {
                SendMessageA(launcher->iwad_list, LB_ADDSTRING, 0,
                             (LPARAM) "No compatible IWADs were found.");
                EnableWindow(launcher->iwad_list, FALSE);
            }

            launcher->params_label = CreateWindowExA(0, "STATIC", "Additional parameters:",
                                                     WS_CHILD | WS_VISIBLE,
                                                     0, 0, 0, 0, hwnd, NULL, NULL, NULL);

            launcher->params_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP
                                                  | ES_LEFT | ES_AUTOHSCROLL,
                                                    0, 0, 0, 0,
                                                    hwnd,
                                                    (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_EDIT,
                                                    NULL, NULL);

            CreateWindowExA(0, "BUTTON", "Play",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                            0, 0, 0, 0, hwnd,
                            (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_PLAY, NULL, NULL);

            CreateWindowExA(0, "BUTTON", "Exit",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            0, 0, 0, 0, hwnd,
                            (HMENU) (INT_PTR) IDC_IWAD_LAUNCHER_EXIT, NULL, NULL);

            LayoutIWADLauncher(launcher);
            return 0;
        }

        case WM_SIZE:
            if (launcher != NULL)
            {
                LayoutIWADLauncher(launcher);
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

                case IDC_IWAD_LAUNCHER_LIST:
                    if (HIWORD(wparam) == LBN_DBLCLK)
                    {
                        FinishIWADLauncher(launcher, true);
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
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static boolean RunIWADLauncherDialog(int mask)
{
    static boolean class_registered = false;
    iwad_launcher_t launcher = { 0 };
    launcher.iwads = D_FindAllIWADs(mask);
    launcher.dpi = GetSystemDPI();
    launcher.selected_iwad = 0;
    launcher.additional_params = M_StringDuplicate("");
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
        wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
        wc.lpszClassName = IWAD_LAUNCHER_CLASS_NAME;

        if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            free(launcher.additional_params);
            free((void *) launcher.iwads);
            return true;
        }

        class_registered = true;
    }

    int client_w = ScaleByDPI(240, launcher.dpi);
    int client_h = ScaleByDPI(240, launcher.dpi);
    DWORD style = WS_OVERLAPPEDWINDOW
                & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME);
    DWORD exstyle = 0;

    RECT win_rect;
    win_rect.left = 0;
    win_rect.top = 0;
    win_rect.right = client_w;
    win_rect.bottom = client_h;
    AdjustWindowRectEx(&win_rect, style, FALSE, exstyle);
    int win_w = win_rect.right - win_rect.left;
    int win_h = win_rect.bottom - win_rect.top;

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
    int pos_x = ((work_rect.right > work_rect.left) ? work_rect.left : 0)
              + (screen_w - win_w) / 2;
    int pos_y = ((work_rect.bottom > work_rect.top) ? work_rect.top : 0)
              + (screen_h - win_h) / 2;

    HWND hwnd = CreateWindowExA(exstyle,
                                IWAD_LAUNCHER_CLASS_NAME,
                                resolved_title,
                                style,
                                pos_x, pos_y, win_w, win_h,
                                NULL, NULL, GetModuleHandle(NULL), &launcher);

    if (hwnd == NULL)
    {
        free(launcher.additional_params);
        free((void *) launcher.iwads);
        return true;
    }

    SetWindowTextA(hwnd, resolved_title);
    SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
    SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);

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

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (launcher.play_pressed)
    {
        if (launcher.iwads[launcher.selected_iwad] != NULL)
        {
            D_AppendCommandLineArgument("-iwad");
            D_AppendCommandLineArgument(launcher.iwads[launcher.selected_iwad]->name);
        }

        D_AppendAdditionalCommandLine(launcher.additional_params);
    }

    free(launcher.additional_params);
    free((void *) launcher.iwads);

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
