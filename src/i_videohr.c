//
// Copyright(C) 2005-2014 Simon Howard
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
//     SDL emulation of VGA 640x480x4 planar video mode,
//     for Hexen startup loading screen.
//

#include "SDL.h"
#include "string.h"

#include "doomtype.h"
#include "i_timer.h"
#include "i_video.h"
#include "i_videohr.h"

// Palette fade-in takes two seconds

#define FADE_TIME 2000

#define HR_SCREENWIDTH 640
#define HR_SCREENHEIGHT 480

static SDL_Window *hr_screen = NULL;
static SDL_Surface *hr_surface = NULL;
static const char *window_title = "";
static boolean use_main_window = false;

// -----------------------------------------------------------------------------
// I_PresentHRToMainWindow
//  [PN] Copy the temporary 640x480 startup surface into the main game buffer.
//  Converts 16-color indexed pixels to the active render format and keeps
//  the original 4:3 startup aspect ratio with black bars when needed.
// -----------------------------------------------------------------------------

static void I_PresentHRToMainWindow(void)
{
    const SDL_Color *colors;
    const pixel_t black = I_MapRGB(0, 0, 0);
    int target_w;
    int target_h;
    int offset_x;
    int offset_y;
    int x, y;

    if (!use_main_window || hr_surface == NULL || I_VideoBuffer == NULL
     || argbbuffer == NULL || hr_surface->format == NULL
     || hr_surface->format->palette == NULL)
    {
        return;
    }

    colors = hr_surface->format->palette->colors;

    // Keep the original 4:3 startup aspect ratio, adding bars if needed.
    target_w = SCREENHEIGHT * HR_SCREENWIDTH / HR_SCREENHEIGHT;
    target_h = SCREENHEIGHT;

    if (target_w > SCREENWIDTH)
    {
        target_w = SCREENWIDTH;
        target_h = SCREENWIDTH * HR_SCREENHEIGHT / HR_SCREENWIDTH;
    }

    offset_x = (SCREENWIDTH - target_w) / 2;
    offset_y = (SCREENHEIGHT - target_h) / 2;

    for (y = 0; y < SCREENAREA; ++y)
    {
        I_VideoBuffer[y] = black;
    }

    for (y = 0; y < target_h; ++y)
    {
        const int src_y = y * HR_SCREENHEIGHT / target_h;
        const byte *src_row =
            ((const byte *) hr_surface->pixels) + src_y * hr_surface->pitch;
        pixel_t *dst_row =
            I_VideoBuffer + (y + offset_y) * SCREENWIDTH + offset_x;

        for (x = 0; x < target_w; ++x)
        {
            const int src_x = x * HR_SCREENWIDTH / target_w;
            const SDL_Color *c = &colors[src_row[src_x]];
            dst_row[x] = I_MapRGB(c->r, c->g, c->b);
        }
    }

    I_FinishUpdate();
}

boolean I_SetVideoModeHR(void)
{
    int x, y;

    if (hr_surface != NULL)
    {
        I_UnsetVideoModeHR();
    }

    hr_screen = (SDL_Window *) I_GetSDLWindow();
    use_main_window = (hr_screen != NULL);

    if (!use_main_window)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            return false;
        }

        // [JN] Use different window centering function.
        CenterWindow(&x, &y, HR_SCREENWIDTH, HR_SCREENHEIGHT);

        // Create screen surface at the native desktop pixel depth (bpp=0),
        // as we cannot trust true 8-bit to reliably work nowadays.
        hr_screen = SDL_CreateWindow(window_title, x, y,
            HR_SCREENWIDTH, HR_SCREENHEIGHT,
            0);

        if (hr_screen == NULL)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }
    }
    else if (window_title != NULL && *window_title != '\0')
    {
        SDL_SetWindowTitle(hr_screen, window_title);
    }

    // We do all actual drawing into an intermediate surface.
    hr_surface = SDL_CreateRGBSurface(0, HR_SCREENWIDTH, HR_SCREENHEIGHT,
                                      8, 0, 0, 0, 0);

    if (hr_surface == NULL)
    {
        if (!use_main_window)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }

        hr_screen = NULL;
        use_main_window = false;
        return false;
    }

    return true;
}

void I_SetWindowTitleHR(const char *title)
{
    window_title = title;

    if (hr_screen != NULL)
    {
        SDL_SetWindowTitle(hr_screen, window_title);
    }
}

void I_UnsetVideoModeHR(void)
{
    if (hr_surface != NULL)
    {
        SDL_FreeSurface(hr_surface);
        hr_surface = NULL;
    }

    if (!use_main_window && hr_screen != NULL)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    hr_screen = NULL;
    use_main_window = false;
}

void I_ClearScreenHR(void)
{
    SDL_Rect area = { 0, 0, HR_SCREENWIDTH, HR_SCREENHEIGHT };

    SDL_FillRect(hr_surface, &area, 0);
}

void I_SlamBlockHR(int x, int y, int w, int h, const byte *src)
{
    SDL_Rect blit_rect;
    const byte *srcptrs[4];
    byte srcbits[4];
    byte *dest;
    int x1, y1;
    int i;
    int bit;

    // Set up source pointers to read from source buffer - each 4-bit 
    // pixel has its bits split into four sub-buffers

    for (i=0; i<4; ++i)
    {
        srcptrs[i] = src + (i * w * h / 8);
    }

    if (SDL_LockSurface(hr_surface) < 0)
    {
        return;
    }

    // Draw each pixel

    bit = 0;

    for (y1=y; y1<y+h; ++y1)
    {
        dest = ((byte *) hr_surface->pixels) + y1 * hr_surface->pitch + x;

        for (x1=x; x1<x+w; ++x1)
        {
            // Get the bits for this pixel
            // For each bit, find the byte containing it, shift down
            // and mask out the specific bit wanted.

            for (i=0; i<4; ++i)
            {
                srcbits[i] = (srcptrs[i][bit / 8] >> (7 - (bit % 8))) & 0x1;
            }

            // Reassemble the pixel value

            *dest = (srcbits[0] << 0) 
                  | (srcbits[1] << 1)
                  | (srcbits[2] << 2)
                  | (srcbits[3] << 3);

            // Next pixel!

            ++dest;
            ++bit;
        }
    }

    SDL_UnlockSurface(hr_surface);

    if (use_main_window)
    {
        I_PresentHRToMainWindow();
        return;
    }

    // Update the region we drew.
    blit_rect.x = x;
    blit_rect.y = y;
    blit_rect.w = w;
    blit_rect.h = h;
    SDL_BlitSurface(hr_surface, &blit_rect,
                    SDL_GetWindowSurface(hr_screen), &blit_rect);
    SDL_UpdateWindowSurfaceRects(hr_screen, &blit_rect, 1);
}

void I_SlamHR(const byte *buffer)
{
    I_SlamBlockHR(0, 0, HR_SCREENWIDTH, HR_SCREENHEIGHT, buffer);
}

void I_InitPaletteHR(void)
{
    // ...
}

static void I_SetPaletteHR(const byte *palette)
{
    SDL_Rect screen_rect = {0, 0, HR_SCREENWIDTH, HR_SCREENHEIGHT};
    SDL_Color sdlpal[16];
    int i;

    for (i=0; i<16; ++i)
    {
        sdlpal[i].r = palette[i * 3 + 0] * 4;
        sdlpal[i].g = palette[i * 3 + 1] * 4;
        sdlpal[i].b = palette[i * 3 + 2] * 4;
    }

    // After setting colors, update the screen.
    SDL_SetPaletteColors(hr_surface->format->palette, sdlpal, 0, 16);

    if (use_main_window)
    {
        I_PresentHRToMainWindow();
        return;
    }

    SDL_BlitSurface(hr_surface, &screen_rect,
                    SDL_GetWindowSurface(hr_screen), &screen_rect);
    SDL_UpdateWindowSurfaceRects(hr_screen, &screen_rect, 1);
}

void I_FadeToPaletteHR(const byte *palette)
{
    byte tmppal[16 * 3];
    int starttime;
    int elapsed;
    int i;

    starttime = I_GetTimeMS();

    for (;;)
    {
        elapsed = I_GetTimeMS() - starttime;

        if (elapsed >= FADE_TIME)
        {
            break;
        }

        // Generate the fake palette

        for (i=0; i<16 * 3; ++i) 
        {
            tmppal[i] = (palette[i] * elapsed) / FADE_TIME;
        }

        I_SetPaletteHR(tmppal);

        if (!use_main_window)
        {
            SDL_UpdateWindowSurface(hr_screen);
        }

        // Sleep a bit

        I_Sleep(10);
    }

    // Set the final palette

    I_SetPaletteHR(palette);
}

void I_BlackPaletteHR(void)
{
    byte blackpal[16 * 3];

    memset(blackpal, 0, sizeof(blackpal));

    I_SetPaletteHR(blackpal);
}

// Check if the user has hit the escape key to abort startup.
boolean I_CheckAbortHR(void)
{
    SDL_Event ev;
    boolean result = false;

    // Not initialized?
    if (hr_surface == NULL)
    {
        return false;
    }

    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
        {
            result = true;
        }
    }

    return result;
}

