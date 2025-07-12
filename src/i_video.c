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
//	DOOM graphics stuff for SDL.
//


#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_opengl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "txt_main.h"

#include "id_vars.h"


static int vid_startup_delay = 35;
static int vid_resize_delay = 35;

int SCREENWIDTH, SCREENHEIGHT, SCREENHEIGHT_4_3;
int SCREENAREA; // [JN] SCREENWIDTH * SCREENHEIGHT
int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
int WIDESCREENDELTA; // [crispy] horizontal widescreen offset

void (*post_rendering_hook) (void); // [crispy]

// These are (1) the window (or the full screen) that our game is rendered to
// and (2) the renderer that scales the texture (see below) into this window.

static SDL_Window *screen;
static SDL_Renderer *renderer;

// Window title

static const char *window_title = "";

// [JN] Defines window title composition:
// 1 - only game name will appear.
// 0 - game name, port name and version will appear.

static int vid_window_title_short = 1;

// These are (1) the 320x200x8 paletted buffer that we draw to (i.e. the one
// that holds I_VideoBuffer), (2) the 320x200x32 RGBA intermediate buffer that
// we blit the former buffer to, (3) the intermediate 320x200 texture that we
// load the RGBA buffer to and that we render into another texture (4) which
// is upscaled by an integer factor UPSCALE using "nearest" scaling and which
// in turn is finally rendered to screen using "linear" scaling.

SDL_Surface *argbbuffer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Texture *texture_upscaled = NULL;


// palette

static SDL_Texture *curpane = NULL;
static SDL_Texture *redpane = NULL;
static SDL_Texture *yelpane = NULL;
static SDL_Texture *grnpane = NULL;
// Hexen exclusive color panes
static SDL_Texture *grnspane = NULL;
static SDL_Texture *bluepane = NULL;
static SDL_Texture *graypane = NULL;
static SDL_Texture *orngpane = NULL;
static int pane_alpha;
static boolean palette_to_set;
// [JN] Smooth palette.
int    red_pane_alpha, yel_pane_alpha, grn_pane_alpha;
int    gray_pane_alpha, orng_pane_alpha;

// display has been set up?

static boolean initialized = false;

// disable mouse?

static boolean nomouse = false;
int usemouse = 1;

// [JN/PN] Mouse coordinates for menu control.

// Used by in-game menu
int menu_mouse_x, menu_mouse_y;
// Used by SDL cursor for position saving and resoring
static int menu_mouse_x_sdl, menu_mouse_y_sdl;
boolean menu_mouse_allow, menu_mouse_allow_click;

// SDL video driver name

char *vid_video_driver = "";

// [JN] Allow to choose render driver to use.
// https://wiki.libsdl.org/SDL2/SDL_HINT_RENDER_DRIVER

#ifdef _WIN32
// On Windows, set Direct3D 11 by default for better performance.
char *vid_screen_scaler_api = "direct3d11";
#else
// On other OSes let SDL decide what is better for compatibility.
char *vid_screen_scaler_api = "";
#endif

// [JN] Window X and Y position to save and restore.

int vid_window_position_y = 0;
int vid_window_position_x = 0;

// SDL display number on which to run.

int vid_video_display = 0;

// Screen width and height, from configuration file.

int vid_window_width = 640;
int vid_window_height = 480;

// vid_fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.

int vid_fullscreen_width = 0, vid_fullscreen_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

static int vid_max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

int vid_fullscreen = true;

// [JN] Exclusive full screen mode.

int vid_fullscreen_exclusive = 0;

// Aspect ratio correction mode

int vid_aspect_ratio_correct = true;
static int actualheight;

// Force integer scales for resolution-independent rendering

int vid_integer_scaling = false;

// VGA Porch palette change emulation

int vid_vga_porch_flash = false;

// Force software rendering, for systems which lack effective hardware
// acceleration

int vid_force_software_renderer = false;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

static int mouse_grab = true;
static boolean nograbmouse_override = false;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible = true;

// If true, we display dots at the bottom of the screen to 
// indicate FPS.

static boolean display_fps_dots;

// [JN] Externalized FPS value used for FPS counter.

int id_fps_value;

// [JN] Moved to upper level to prevent following while demo warp:
// - prevent force frame rate uncapping after demo warp
// - disk icon drawing
// - palette changing
int demowarp;

// If this is true, the screen is rendered but not blitted to the
// video buffer.

static boolean noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.

static grabmouse_callback_t mouse_grab_callback = NULL;

// Does the window currently have focus?

boolean window_focused = true;

// [JN] Does the sound volume needs to be updated?
// Used for "snd_mute_inactive" feature.

boolean volume_needs_update = false;

// Window resize state.

static boolean need_resize = false;
static unsigned int last_resize_time;

// Joystick/gamepad hysteresis
unsigned int joywait = 0;

// Icon RGB data and dimensions
static const unsigned int *icon_data;
static int icon_w;
static int icon_h;

// [JN] Used for realtime resizing of ENDOOM screen.
boolean endoom_screen_active = false;

void *I_GetSDLWindow(void)
{
    return screen;
}

void *I_GetSDLRenderer(void)
{
    return renderer;
}

static boolean MouseShouldBeGrabbed(void)
{
    // never grab the mouse when in screensaver mode
   
    if (screensaver_mode)
        return false;

    // if the window doesn't have focus, never grab it

    if (!window_focused)
        return false;

    // Don't grab the mouse if mouse input is disabled

    if (!usemouse || nomouse)
        return false;

    // if we specify not to grab the mouse, never grab

    if (nograbmouse_override || !mouse_grab)
        return false;

    // Invoke the mouse_grab callback function to determine whether
    // the mouse should be grabbed

    if (mouse_grab_callback != NULL)
    {
        return mouse_grab_callback();
    }
    else
    {
        return true;
    }
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
    mouse_grab_callback = func;
}

// Set the variable controlling FPS dots.

void I_DisplayFPSDots(boolean dots_on)
{
    display_fps_dots = dots_on;
}

static void SetShowCursor(boolean show)
{
    if (!screensaver_mode)
    {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(!show);
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        SetShowCursor(true);

        SDL_FreeSurface(argbbuffer);
        SDL_DestroyTexture(texture_upscaled);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(screen);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        initialized = false;
    }
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

// Adjust vid_window_width / vid_window_height variables to be an an aspect
// ratio consistent with the vid_aspect_ratio_correct variable.
static void AdjustWindowSize(void)
{
    if (vid_aspect_ratio_correct || vid_integer_scaling)
    {
        static int old_v_w, old_v_h;

        if (old_v_w > 0 && old_v_h > 0)
        {
          int rendered_height;

          // rendered height does not necessarily match window height
          if (vid_window_height * old_v_w > vid_window_width * old_v_h)
            rendered_height = (vid_window_width * old_v_h + old_v_w - 1) / old_v_w;
          else
            rendered_height = vid_window_height;

          vid_window_width = rendered_height * SCREENWIDTH / actualheight;
        }

        old_v_w = SCREENWIDTH;
        old_v_h = actualheight;
#if 0
        if (vid_window_width * actualheight <= vid_window_height * SCREENWIDTH)
        {
            // We round up vid_window_height if the ratio is not exact; this leaves
            // the result stable.
            vid_window_height = (vid_window_width * actualheight + SCREENWIDTH - 1) / SCREENWIDTH;
        }
        else
        {
            vid_window_width = vid_window_height * SCREENWIDTH / actualheight;
        }
#endif
    }
}

static void HandleWindowEvent(SDL_WindowEvent *event)
{
    int i;
    int flags = 0;

    switch (event->event)
    {
#if 0 // SDL2-TODO
        case SDL_ACTIVEEVENT:
            // need to update our focus state
            UpdateFocus();
            break;
#endif
        case SDL_WINDOWEVENT_EXPOSED:
            palette_to_set = true;
            break;

        case SDL_WINDOWEVENT_RESIZED:
            need_resize = true;
            last_resize_time = SDL_GetTicks();
            break;

        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            // [JN] Focus gained in exclusive fullscreen mode.
            // Set SDL_WINDOW_FULLSCREEN flag to the window.
            if (vid_fullscreen_exclusive && vid_fullscreen)
            {
                flags |= SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(screen, flags);
            }
            volume_needs_update = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            // [JN] Focus lost in exclusive fullscreen mode.
            // Clear SDL_WINDOW_FULLSCREEN flag from the window.
            if (vid_fullscreen_exclusive && vid_fullscreen)
            {
                flags &= ~SDL_WINDOW_FULLSCREEN;
                SDL_SetWindowFullscreen(screen, flags);
            }
            volume_needs_update = true;
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on and
        // update the vid_video_display config variable.

        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                vid_video_display = i;
            }
            // [JN] Get X and Y coordinates after moving a window.
            // But do not get in vid_fullscreen mode, since x and y becomes 0,
            // which will cause position reset to "centered" in SetVideoMode.
            if (!vid_fullscreen)
            {
                SDL_GetWindowPosition(screen, &vid_window_position_x, &vid_window_position_y);
            }
            break;

        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// HandleWindowResize
// [JN] Updates window contents (SDL texture) on fly while resizing.
// SDL_WINDOWEVENT_RESIZED from above is still needed to get rid of 
// black borders after window size has been changed.
// -----------------------------------------------------------------------------

static int HandleWindowResize (void* data, SDL_Event *event) 
{
    if (event->type == SDL_WINDOWEVENT 
    &&  event->window.event == SDL_WINDOWEVENT_RESIZED)
    {
        // Redraw window contents:
        if (endoom_screen_active)
        {
            TXT_UpdateScreen();
        }
        else
        {
            I_FinishUpdate();
        }
        
    }
    return 0;
}

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN || 
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    vid_fullscreen = !vid_fullscreen;

    if (vid_fullscreen)
    {
        flags |= vid_fullscreen_exclusive ? SDL_WINDOW_FULLSCREEN :
                                            SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (vid_fullscreen_exclusive)
    {
        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(vid_video_display, &mode);
        SDL_SetWindowSize(screen, mode.w, mode.h);
    }

    SDL_SetWindowFullscreen(screen, flags);
    // [JN] Hack to fix missing window icon after starting in vid_fullscreen mode.
    I_InitWindowIcon();

    if (!vid_fullscreen)
    {
        AdjustWindowSize();
        SDL_SetWindowSize(screen, vid_window_width, vid_window_height);
    }
}

void I_UpdateExclusiveFullScreen(void)
{
    unsigned int flags = 0;

    flags |= vid_fullscreen_exclusive ? SDL_WINDOW_FULLSCREEN :
                                        SDL_WINDOW_FULLSCREEN_DESKTOP;

    if (vid_fullscreen_exclusive)
    {
        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(vid_video_display, &mode);
        SDL_SetWindowSize(screen, mode.w, mode.h);
    }

    SDL_SetWindowFullscreen(screen, flags);
}

void I_GetEvent(void)
{
    SDL_Event sdlevent;

    SDL_PumpEvents();

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
		I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEMOTION:
                if (menu_mouse_allow && window_focused)
                {
                    // [PN] Get mouse coordinates for menu control
                    menu_mouse_x = sdlevent.motion.x;
                    menu_mouse_y = (int)(sdlevent.motion.y / 1.2); // Aspect ratio correction
                    // [JN] Get mouse coordinates for SDL control
                    SDL_GetMouseState(&menu_mouse_x_sdl, &menu_mouse_y_sdl);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (usemouse && !nomouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
                if (screensaver_mode)
                {
                    I_Quit();
                }
                else
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
    }
}

// [JN] Reinitialize mouse cursor position on changing rendering resoluton
void I_ReInitCursorPosition (void)
{
    SDL_Event sdlevent;

    SDL_PollEvent(&sdlevent);
    // [PN] Get mouse coordinates for menu control
    menu_mouse_x = sdlevent.motion.x;
    menu_mouse_y = (int)(sdlevent.motion.y / 1.2); // Aspect ratio correction
    // [JN] Get mouse coordinates for SDL control
    SDL_GetMouseState(&menu_mouse_x_sdl, &menu_mouse_y_sdl);
}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();

    if (usemouse && !nomouse && window_focused)
    {
        I_ReadMouse();
    }

    if (joywait < I_GetTime())
    {
        I_UpdateJoystick();
    }
}

void I_StartDisplay(void) // [crispy]
{
    // [AM] Figure out how far into the current tic we're in as a fixed_t.
    fractionaltic = I_GetFracRealTime();

    SDL_PumpEvents();

    if (usemouse && !nomouse && window_focused)
    {
        I_ReadMouseUncapped();
    }
}

static void UpdateGrab(void)
{
    static boolean currently_grabbed = false;
    boolean grab;

    grab = MouseShouldBeGrabbed();

    if (screensaver_mode)
    {
        // Hide the cursor in screensaver mode

        SetShowCursor(false);
    }
    else if (grab && !currently_grabbed)
    {
        SetShowCursor(false);
    }
    else if (!grab && currently_grabbed)
    {
        SetShowCursor(true);

        // [JN] Restore cursor position.
        SDL_WarpMouseInWindow(screen, menu_mouse_x_sdl, menu_mouse_y_sdl);
        
        SDL_GetRelativeMouseState(NULL, NULL);
    }

    currently_grabbed = grab;
}

static void LimitTextureSize(int *w_upscale, int *h_upscale)
{
    SDL_RendererInfo rinfo;
    int orig_w, orig_h;

    orig_w = *w_upscale;
    orig_h = *h_upscale;

    // Query renderer and limit to maximum texture dimensions of hardware:
    if (SDL_GetRendererInfo(renderer, &rinfo) != 0)
    {
        I_Error("CreateUpscaledTexture: SDL_GetRendererInfo() call failed: %s",
                SDL_GetError());
    }

    while (*w_upscale * SCREENWIDTH > rinfo.max_texture_width)
    {
        --*w_upscale;
    }
    while (*h_upscale * SCREENHEIGHT > rinfo.max_texture_height)
    {
        --*h_upscale;
    }

    if ((*w_upscale < 1 && rinfo.max_texture_width > 0) ||
        (*h_upscale < 1 && rinfo.max_texture_height > 0))
    {
        I_Error("CreateUpscaledTexture: Can't create a texture big enough for "
                "the whole screen! Maximum texture size %dx%d",
                rinfo.max_texture_width, rinfo.max_texture_height);
    }

    // We limit the amount of texture memory used for the intermediate buffer,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures, so the user can use this to reduce the maximum texture
    // size if desired.

    if (vid_max_scaling_buffer_pixels < SCREENAREA)
    {
        I_Error("CreateUpscaledTexture: vid_max_scaling_buffer_pixels too small "
                "to create a texture buffer: %d < %d",
                vid_max_scaling_buffer_pixels, SCREENAREA);
    }

    while (*w_upscale * *h_upscale * SCREENAREA
           > vid_max_scaling_buffer_pixels)
    {
        if (*w_upscale > *h_upscale)
        {
            --*w_upscale;
        }
        else
        {
            --*h_upscale;
        }
    }

    if (*w_upscale != orig_w || *h_upscale != orig_h)
    {
        printf("CreateUpscaledTexture: Limited texture size to %dx%d "
               "(max %d pixels, max texture size %dx%d)\n",
               *w_upscale * SCREENWIDTH, *h_upscale * SCREENHEIGHT,
               vid_max_scaling_buffer_pixels,
               rinfo.max_texture_width, rinfo.max_texture_height);
    }
}

static void CreateUpscaledTexture(boolean force)
{
    int w, h;
    int h_upscale, w_upscale;
    static int h_upscale_old, w_upscale_old;

    SDL_Texture *new_texture, *old_texture;

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0)
    {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.

    if (w * actualheight < h * SCREENWIDTH)
    {
        // Tall window.

        h = w * actualheight / SCREENWIDTH;
    }
    else
    {
        // Wide window.

        w = h * SCREENWIDTH / actualheight;
    }

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.

    w_upscale = (w + SCREENWIDTH - 1) / SCREENWIDTH;
    h_upscale = (h + SCREENHEIGHT - 1) / SCREENHEIGHT;

    // Minimum texture dimensions of 320x200.

    if (w_upscale < 1)
    {
        w_upscale = 1;
    }
    if (h_upscale < 1)
    {
        h_upscale = 1;
    }

    LimitTextureSize(&w_upscale, &h_upscale);

    // Create a new texture only if the upscale factors have actually changed.

    if (h_upscale == h_upscale_old && w_upscale == w_upscale_old && !force)
    {
        return;
    }

    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    // Set the scaling quality for rendering the upscaled texture to "linear",
    // which looks much softer and smoother than "nearest" but does a better
    // job at downscaling from the upscaled texture to screen.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    new_texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_TARGET,
                                w_upscale*SCREENWIDTH,
                                h_upscale*SCREENHEIGHT);

    old_texture = texture_upscaled;
    texture_upscaled = new_texture;

    if (old_texture != NULL)
    {
        SDL_DestroyTexture(old_texture);
    }
}

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
fixed_t fractionaltic;

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    if (!initialized)
        return;

    if (noblit)
        return;

    if (need_resize)
    {
        if (SDL_GetTicks() > last_resize_time + vid_resize_delay)
        {
            int flags;
            // When the window is resized (we're not in vid_fullscreen mode),
            // save the new window size.
            flags = SDL_GetWindowFlags(screen);
            if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
            {
                SDL_GetWindowSize(screen, &vid_window_width, &vid_window_height);

                // Adjust the window by resizing again so that the window
                // is the right aspect ratio.
                AdjustWindowSize();
                SDL_SetWindowSize(screen, vid_window_width, vid_window_height);
            }
            CreateUpscaledTexture(false);
            need_resize = false;
            palette_to_set = true;
        }
        else
        {
            return;
        }
    }

    UpdateGrab();

#if 0 // SDL2-TODO
    // Don't update the screen if the window isn't visible.
    // Not doing this breaks under Windows when we alt-tab away 
    // while vid_fullscreen.

    if (!(SDL_GetAppState() & SDL_APPACTIVE))
        return;
#endif

	// [crispy] [AM] Real FPS counter
	if (vid_showfps)
	{
		static int lastmili;
		static int fpscount;

		fpscount++;

		const uint32_t i = SDL_GetTicks();
		const int mili = i - lastmili;

		// Update FPS counter every 1/4th of second
		if (mili >= 250)
		{
			id_fps_value = (fpscount * 1000) / mili;
			fpscount = 0;
			lastmili = i;
		}
	}

    // Draw disk icon before blit, if necessary.
    if (vid_diskicon && diskicon_enabled)
    V_DrawDiskIcon();

    SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);

    // Make sure the pillarboxes are kept clear each frame.

    SDL_RenderClear(renderer);

    if (vid_smooth_scaling && !vid_force_software_renderer)
    {
    // Render this intermediate texture into the upscaled texture
    // using "nearest" integer scaling.

    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    // Finally, render this upscaled texture to screen using linear scaling.

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);
    }
    else
    {
	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
    }

    if (curpane)
    {
	SDL_SetTextureAlphaMod(curpane, pane_alpha);
	SDL_RenderCopy(renderer, curpane, NULL, NULL);
    }

    // Draw!

    SDL_RenderPresent(renderer);

    if (vid_uncapped_fps && !singletics)
    {
        // Limit framerate
        if (vid_fpslimit >= TICRATE)
        {
            uint64_t target_time = 1000000ull / vid_fpslimit;
            static uint64_t start_time;

            while (1)
            {
                uint64_t current_time = I_GetTimeUS();
                uint64_t elapsed_time = current_time - start_time;
                uint64_t remaining_time = 0;

                if (elapsed_time >= target_time)
                {
                    start_time = current_time;
                    break;
                }

                remaining_time = target_time - elapsed_time;

                if (remaining_time > 1000)
                {
                    I_Sleep((remaining_time - 1000) / 1000);
                }
            }
        }
    }
}


//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENAREA*sizeof(*scr));
}


//
// I_SetPalette
//

void I_SetPalette (int palette)
{
    // [PN] Alpha values for palette flash effects (cases 13-27).
    // Each row corresponds to a palette case, with 4 intensity levels:
    // [Full intensity, Half intensity, Quarter intensity, Minimal visibility/Off].
    static const int alpha_values[15][4] = {
        {  31,  16,   8,   4 }, // case 13
        {  51,  26,  12,  10 }, // case 14
        {  76,  38,  19,  11 }, // case 15
        { 102,  51,  26,  12 }, // case 16
        { 127,  64,  32,  13 }, // case 17
        { 153,  77,  38,  14 }, // case 18
        { 178,  89,  45,  15 }, // case 19
        { 204, 102,  51,  16 }, // case 20
        { 128,  64,  32,  16 }, // case 21
        { 127,  64,  32,   0 }, // case 22
        { 106,  53,  27,   0 }, // case 23
        {  52,  26,  13,   0 }, // case 24
        { 127,  64,  32,   0 }, // case 25
        {  96,  48,  24,   0 }, // case 26
        {  72,  36,  18,   0 }  // case 27
    };

    // [JN] Don't change palette while demo warp.
    if (demowarp)
    {
        return;
    }
    
    switch (palette)
    {
	case 0:
	    curpane = NULL;
	    break;
	case 1:
	    if (vis_smooth_palette)
	    {
	    curpane = redpane;
	    pane_alpha = red_pane_alpha;
	    break;
	    }
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	    curpane = redpane;
	    pane_alpha = 0xff * palette / 9;
	    break;
	case 9:
	    if (vis_smooth_palette)
	    {
	    curpane = yelpane;
	    pane_alpha = yel_pane_alpha;
	    break;
	    }
	case 10:
	case 11:
	case 12:
	    curpane = yelpane;
	    pane_alpha = 0xff * (palette - 8) / 8;
	    break;
	// [PN] A11Y - Palette flash effects for following cases:
	case 13:
	    curpane = grnpane;
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    break;
	// Hexen exclusive color panes and palette indexes
	// https://doomwiki.org/wiki/PLAYPAL#Hexen
	case 14:  // STARTPOISONPALS + 1 (13 is shared with other games)
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	    curpane = grnspane;
	    if (vis_smooth_palette)
	    {
	    pane_alpha = grn_pane_alpha;
	    }
	    else
	    {
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    }
	    break;
	case 21:  // STARTICEPAL
	    curpane = bluepane;
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    break;
	case 22:  // STARTHOLYPAL
	    curpane = graypane;
	    if (vis_smooth_palette)
	    {
	    pane_alpha = gray_pane_alpha;
	    }
	    else
	    {
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    }
	    break;
	case 23:
	case 24:
	    curpane = graypane;
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    break;
	case 25:  // STARTSCOURGEPAL
	case 26:
	case 27:
	    curpane = orngpane;
	    if (vis_smooth_palette)
	    {
	    pane_alpha = orng_pane_alpha;
	    }
	    else
	    {
	    pane_alpha = alpha_values[palette - 13][a11y_pal_flash];
	    }
	    break;
	default:
	    I_Error("Unknown palette: %d!\n", palette);
	    break;
    }
}

// 
// Set the window title
//

void I_SetWindowTitle(const char *title)
{
    window_title = title;
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

void I_InitWindowTitle(void)
{
    char *buf;

    // [JN] Leave only game name in window title.
    buf = M_StringJoin(window_title, vid_window_title_short ?
                       NULL : " - ", PACKAGE_FULLNAME, NULL);
    SDL_SetWindowTitle(screen, buf);
    free(buf);
}

void I_RegisterWindowIcon(const unsigned int *icon, int width, int height)
{
    icon_data = icon;
    icon_w = width;
    icon_h = height;
}

// Set the application icon

void I_InitWindowIcon(void)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) icon_data, icon_w, icon_h,
                                       32, icon_w * 4,
                                       0xffu << 24, 0xffu << 16,
                                       0xffu << 8, 0xffu << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

// Set video size to a particular scale factor (1x, 2x, 3x, etc.)

static void SetScaleFactor(int factor)
{
    int height;

    // Pick 320x200 or 320x240, depending on aspect ratio correct
    if (vid_aspect_ratio_correct)
    {
        height = SCREENHEIGHT_4_3;
    }
    else
    {
        height = SCREENHEIGHT;
    }

    vid_window_width = factor * SCREENWIDTH;
    vid_window_height = factor * height;
    vid_fullscreen = false;
}

void I_GraphicsCheckCommandLine(void)
{
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    noblit = M_CheckParm ("-noblit");

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    nograbmouse_override = M_ParmExists("-nograbmouse");

    // default to fullscreen mode, allow override with command line
    // nofullscreen because we love prboom

    //!
    // @category video 
    //
    // Run in a window.
    //

    if (M_CheckParm("-window") || M_CheckParm("-nofullscreen"))
    {
        vid_fullscreen = false;
    }

    //!
    // @category video 
    //
    // Run in fullscreen mode.
    //

    if (M_CheckParm("-fullscreen"))
    {
        vid_fullscreen = true;
    }

    //!
    // @category video 
    //
    // Disable the mouse.
    //

    nomouse = M_CheckParm("-nomouse") > 0;

    //!
    // @category video
    // @arg <x>
    //
    // Specify the screen width, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-width", 1);

    if (i > 0)
    {
        vid_window_width = atoi(myargv[i + 1]);
        vid_window_height = vid_window_width * 2;
        AdjustWindowSize();
        vid_fullscreen = false;
    }

    //!
    // @category video
    // @arg <y>
    //
    // Specify the screen height, in pixels. Implies -window.
    //

    i = M_CheckParmWithArgs("-height", 1);

    if (i > 0)
    {
        vid_window_height = atoi(myargv[i + 1]);
        vid_window_width = vid_window_height * 2;
        AdjustWindowSize();
        vid_fullscreen = false;
    }

    //!
    // @category video
    // @arg <WxY>
    //
    // Specify the dimensions of the window. Implies -window.
    //

    i = M_CheckParmWithArgs("-geometry", 1);

    if (i > 0)
    {
        int w, h, s;

        s = sscanf(myargv[i + 1], "%ix%i", &w, &h);
        if (s == 2)
        {
            vid_window_width = w;
            vid_window_height = h;
            vid_fullscreen = false;
        }
    }

    //!
    // @category video
    // @arg <x>
    //
    // Specify the display number on which to show the screen.
    //

    i = M_CheckParmWithArgs("-display", 1);

    if (i > 0)
    {
        int display = atoi(myargv[i + 1]);
        if (display >= 0)
        {
            vid_video_display = display;
        }
    }

    //!
    // @category video
    //
    // Don't scale up the screen. Implies -window.
    //

    if (M_CheckParm("-1")) 
    {
        SetScaleFactor(1);
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size. Implies -window.
    //

    if (M_CheckParm("-2")) 
    {
        SetScaleFactor(2);
    }

    //!
    // @category video
    //
    // Double up the screen to 3x its normal size. Implies -window.
    //

    if (M_CheckParm("-3")) 
    {
        SetScaleFactor(3);
    }
}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
    char *env;

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        screensaver_mode = true;
    }
}

static void SetSDLVideoDriver(void)
{
    // Allow a default value for the SDL video driver to be specified
    // in the configuration file.

    if (strcmp(vid_video_driver, "") != 0)
    {
        char *env_string;

        env_string = M_StringJoin("SDL_VIDEODRIVER=", vid_video_driver, NULL);
        putenv(env_string);
        free(env_string);
    }
}

// Check the display bounds of the display referred to by 'vid_video_display' and
// set x and y to a location that places the window in the center of that
// display.
void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(vid_video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", vid_video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}


// [PN] Apply intensity correction to RGB channels
#define ADJUST_INTENSITY(r, g, b, r_intensity, g_intensity, b_intensity) \
    { (r) = (byte)((r) * (r_intensity)); \
      (g) = (byte)((g) * (g_intensity)); \
      (b) = (byte)((b) * (b_intensity)); }

// [PN] Apply saturation correction to RGB channels
#define ADJUST_SATURATION(r, g, b, a_hi, a_lo) \
    { const float one_minus_a_hi = 1.0f - (a_hi); \
      const byte new_r = (byte)(one_minus_a_hi * (r) + (a_lo) * ((g) + (b))); \
      const byte new_g = (byte)(one_minus_a_hi * (g) + (a_lo) * ((r) + (b))); \
      const byte new_b = (byte)(one_minus_a_hi * (b) + (a_lo) * ((r) + (g))); \
      (r) = new_r; \
      (g) = new_g; \
      (b) = new_b; }

// [PN] Apply contrast adjustment to RGB channels
#define ADJUST_CONTRAST(r, g, b, contrast) \
    { const float contrast_adjustment = 128 * (1.0f - (contrast)); \
      (r) = (byte)BETWEEN(0, 255, (int)((contrast) * (r) + contrast_adjustment)); \
      (g) = (byte)BETWEEN(0, 255, (int)((contrast) * (g) + contrast_adjustment)); \
      (b) = (byte)BETWEEN(0, 255, (int)((contrast) * (b) + contrast_adjustment)); }

// [PN] Apply colorblind filter to RGB channels
#define ADJUST_COLORBLIND(r, g, b, matrix) \
    { const byte orig_r = (r); \
      const byte orig_g = (g); \
      const byte orig_b = (b); \
      (r) = (byte)BETWEEN(0, 255, (int)((matrix)[0][0] * orig_r + (matrix)[0][1] * orig_g + (matrix)[0][2] * orig_b)); \
      (g) = (byte)BETWEEN(0, 255, (int)((matrix)[1][0] * orig_r + (matrix)[1][1] * orig_g + (matrix)[1][2] * orig_b)); \
      (b) = (byte)BETWEEN(0, 255, (int)((matrix)[2][0] * orig_r + (matrix)[2][1] * orig_g + (matrix)[2][2] * orig_b)); }


void I_SetColorPanes (boolean recreate_argbbuffer)
{
    // [PN] Define a structure to hold texture names and their colors
    typedef struct {
        SDL_Texture **texture;  // Pointer to the texture variable
        uint8_t r, g, b;        // RGB values
    } pane_t;

    // [PN] Array of pane definitions
    static const pane_t panes[] = {
        { &redpane,  0xff, 0x0,  0x0  },  // red
        { &yelpane,  0xd7, 0xba, 0x45 },  // yellow
        { &grnpane,  0x0,  0xff, 0x0  },  // green
        { &grnspane, 0x2c, 0x5c, 0x24 },  // green (Hexen: poison)
        { &bluepane, 0x0,  0x0,  0xe0 },  // blue (Hexen: ice)
        { &graypane, 0x82, 0x82, 0x82 },  // gray (Hexen: Wraithverge)
        { &orngpane, 0x96, 0x6e, 0x0  }   // orange (Hexen: Arc of Death)
    };

    // [PN] Precompute saturation values
    const float a_hi = vid_saturation < 100 ? I_SaturationPercent[vid_saturation] : 0;
    const float a_lo = vid_saturation < 100 ? (a_hi / 2) : 0;

    // [PN] Safely free and allocate the surface to avoid memory leaks.
    // [JN] This is have to be done just once, at startup. No need to 
    // destroy and recreate argbbuffer every time color adjustment happens.
    if (recreate_argbbuffer)
    {
        if (argbbuffer != NULL)
        {
            SDL_FreeSurface(argbbuffer);
            argbbuffer = NULL;
        }

        argbbuffer = SDL_CreateRGBSurfaceWithFormat(
                     0, SCREENWIDTH, SCREENHEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
    }

    // [PN] Adjust and create textures from the defined colors
    for (int i = 0; i < sizeof(panes) / sizeof(panes[0]); i++)
    {
        // [PN] Extract original RGB values
        byte r = panes[i].r;
        byte g = panes[i].g;
        byte b = panes[i].b;

        // [PN] Apply intensity, saturation and contrast adjustments
        ADJUST_INTENSITY(r, g, b, vid_r_intensity, vid_g_intensity, vid_b_intensity);
        ADJUST_SATURATION(r, g, b, a_hi, a_lo);
        ADJUST_CONTRAST(r, g, b, vid_contrast);
        ADJUST_COLORBLIND(r, g, b, colorblind_matrix[a11y_colorblind]);

        // [PN] Fill rectangle with modified color
        SDL_FillRect(argbbuffer, NULL, I_MapRGB(r, g, b));

        // [PN] Create texture from surface
        *(panes[i].texture) = SDL_CreateTextureFromSurface(renderer, argbbuffer);

        // [PN] Set blend mode
        SDL_SetTextureBlendMode(*(panes[i].texture), SDL_BLENDMODE_BLEND);
    }
}

static void SetVideoMode(void)
{
    int w, h;
    int x = 0, y = 0;
    int window_flags = 0, renderer_flags = 0;
    SDL_DisplayMode mode;

    w = vid_window_width;
    h = vid_window_height;

    // In windowed mode, the window can be resized while the game is
    // running.
    window_flags = SDL_WINDOW_RESIZABLE;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    // [JN] Choose render driver to use.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, vid_screen_scaler_api);

    // [JN] Ensure 'mode.w/h' is initialized before use.
    if (SDL_GetCurrentDisplayMode(vid_video_display, &mode) != 0)
    {
        I_Error("Could not get display mode for video display #%d: %s",
        vid_video_display, SDL_GetError());
    }

    if (vid_fullscreen)
    {
        if (!vid_fullscreen_exclusive)
        {
            // This window_flags means "Never change the screen resolution!
            // Instead, draw to the entire screen by scaling the texture
            // appropriately".
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
            // [JN] Use native resolution for exclusive fullscreen mode. 
            // Width (w) and height (h) are set from SDL.
            w = mode.w;
            h = mode.h;
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }

    // in vid_fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run vid_fullscreen on.
    if (vid_fullscreen)
    {
        CenterWindow(&x, &y, w, h);
    }

    // [JN] If window X and Y coords was not set,
    // place game window in the center of the screen.
    if (vid_window_position_x == 0 || vid_window_position_y == 0)
    {
        vid_window_position_x = x/2 + w/2;
        vid_window_position_y = y/2 + h/2;
    }

    // Create window and renderer contexts. We set the window title
    // later anyway and leave the window position "undefined". If
    // "window_flags" contains the vid_fullscreen flag (see above), then
    // w and h are ignored.

    if (screen == NULL)
    {
        screen = SDL_CreateWindow(NULL, vid_window_position_x, vid_window_position_y,
                                  w, h, window_flags);

        if (screen == NULL)
        {
            I_Error("Error creating window for video startup: %s",
            SDL_GetError());
        }

        // [JN] Always enable resolution-independent minimal window size
        // and do not increase minimal window size with higher resolutions.
        SDL_SetWindowMinimumSize(screen, SCREENWIDTH / vid_resolution,
                                         actualheight / vid_resolution);

        I_InitWindowTitle();
        I_InitWindowIcon();
    }

    // The SDL_RENDERER_TARGETTEXTURE flag is required to render the
    // intermediate texture into the upscaled texture.
    renderer_flags = SDL_RENDERER_TARGETTEXTURE;
	
    // Turn on vsync if we aren't in a -timedemo
    if ((!singletics && mode.refresh_rate > 0) || demowarp)
    {
        if (vid_vsync) // [crispy] uncapped vsync
        {
            renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
        }
    }

	// Force software mode
    if (vid_force_software_renderer)
    {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;
        vid_vsync = false;
    }

    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
        // all associated textures get destroyed
        texture = NULL;
        texture_upscaled = NULL;
    }

    renderer = SDL_CreateRenderer(screen, -1, renderer_flags);

    // If we could not find a matching render driver,
    // try again without hardware acceleration.

    if (renderer == NULL && !vid_force_software_renderer)
    {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(screen, -1, renderer_flags);

        // If this helped, save the setting for later.
        if (renderer != NULL)
        {
            vid_force_software_renderer = 1;
        }
    }

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }

    // Important: Set the "logical size" of the rendering context. At the same
    // time this also defines the aspect ratio that is preserved while scaling
    // and stretching the texture into the window.

    if (vid_aspect_ratio_correct || vid_integer_scaling)
    {
        SDL_RenderSetLogicalSize(renderer,
                                 SCREENWIDTH,
                                 actualheight);
    }

    // Force integer scales for resolution-independent rendering.

    SDL_RenderSetIntegerScale(renderer, vid_integer_scaling);

    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);


    // Format of argbbuffer must match the screen pixel format because we
    // import the surface data into the texture.

    if (argbbuffer != NULL)
    {
        SDL_FreeSurface(argbbuffer);
        argbbuffer = NULL;
    }

    if (argbbuffer == NULL)
    {
	    // [PN] Allocate argbbuffer and initialize color panes
	    I_SetColorPanes(true);
    }

    if (texture != NULL)
    {
        SDL_DestroyTexture(texture);
    }

    // Set the scaling quality for rendering the intermediate texture into
    // the upscaled texture to "nearest", which is gritty and pixelated and
    // resembles software scaling pretty well.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Create the intermediate texture that the RGBA surface gets loaded into.
    // The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
    // is going to change frequently.

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                SCREENWIDTH, SCREENHEIGHT);

    // [JN] Workaround for SDL 2.0.14+ alt-tab bug
#if defined(_WIN32)
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
#endif

    // Initially create the upscaled texture for rendering to screen

    CreateUpscaledTexture(true);

    // [JN] Set the initial position of the mouse cursor.
    {
        int screen_w, screen_h;

        SDL_GetWindowSize(screen, &screen_w, &screen_h);
        menu_mouse_x_sdl = (int)(screen_w / 1.3);
        menu_mouse_y_sdl = (int)(screen_h / 1.3);
    }
}

// [crispy] re-calculate SCREENWIDTH, SCREENHEIGHT, NONWIDEWIDTH and WIDESCREENDELTA
void I_GetScreenDimensions (void)
{
	SDL_DisplayMode mode;
	int w = 16, h = 9;
	int ah;

	SCREENWIDTH = ORIGWIDTH * vid_resolution;
	SCREENHEIGHT = ORIGHEIGHT * vid_resolution;

	NONWIDEWIDTH = SCREENWIDTH;

	ah = (vid_aspect_ratio_correct == 1) ? (6 * SCREENHEIGHT / 5) : SCREENHEIGHT;

	if (SDL_GetCurrentDisplayMode(vid_video_display, &mode) == 0)
	{
		// [crispy] sanity check: really widescreen display?
		if (mode.w * ah >= mode.h * SCREENWIDTH)
		{
			w = mode.w;
			h = mode.h;
		}
	}

	// [crispy] widescreen rendering makes no sense without aspect ratio correction
	if (vid_widescreen && vid_aspect_ratio_correct == 1)
	{
		switch(vid_widescreen)
		{
			case RATIO_16_10:
				w = 16;
				h = 10;
				break;
			case RATIO_16_9:
				w = 16;
				h = 9;
				break;
			case RATIO_21_9:
				w = 21;
				h = 9;
				break;
			case RATIO_32_9:
				w = 32;
				h = 9;
				break;
			default:
				break;
		}

		SCREENWIDTH = w * ah / h;
		// [crispy] make sure SCREENWIDTH is an integer multiple of 4 ...
		if (vid_resolution > 1)
		{
			// [Nugget] Since we have uneven resolution multipliers, mask it twice
			SCREENWIDTH = (((SCREENWIDTH * vid_resolution) & (int)~3) / vid_resolution + 3) & (int)~3;
		}
		else
		{
			SCREENWIDTH = (SCREENWIDTH + 3) & (int)~3;
		}

		// [crispy] ... but never exceeds MAXWIDTH (array size!)
		SCREENWIDTH = MIN(SCREENWIDTH, MAXWIDTH);
	}

	SCREENAREA = SCREENWIDTH * SCREENHEIGHT;
	WIDESCREENDELTA = ((SCREENWIDTH - NONWIDEWIDTH) / vid_resolution) / 2;
}

// [crispy] calls native SDL vsync toggle
void I_ToggleVsync (void)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_RenderSetVSync(renderer, vid_vsync);
#else
    I_ReInitGraphics(REINIT_RENDERER | REINIT_TEXTURES | REINIT_ASPECTRATIO);
#endif
}

void I_InitGraphics(void)
{
    SDL_Event dummy;
    char *env;

    // Pass through the XSCREENSAVER_WINDOW environment variable to 
    // SDL_WINDOWID, to embed the SDL window into the Xscreensaver
    // window.

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        char winenv[30];
        unsigned int winid;

        sscanf(env, "0x%x", &winid);
        M_snprintf(winenv, sizeof(winenv), "SDL_WINDOWID=%u", winid);

        putenv(winenv);
    }

    SetSDLVideoDriver();

    // [JN] Set an event watcher for window resize to allow
    // update window contents on fly.
    SDL_AddEventWatch(HandleWindowResize, screen);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    // When in screensaver mode, run full screen and auto detect
    // screen dimensions (don't change video mode)
    if (screensaver_mode)
    {
        vid_fullscreen = true;
    }

    // [crispy] run-time variable high-resolution rendering
    I_GetScreenDimensions();


    // [crispy] (re-)initialize resolution-agnostic patch drawing
    V_Init();

    if (vid_aspect_ratio_correct == 1)
    {
        actualheight = 6 * SCREENHEIGHT / 5;
    }
    else
    {
        actualheight = SCREENHEIGHT;
    }

    // Create the game window; this may switch graphic modes depending
    // on configuration.
    AdjustWindowSize();
    SetVideoMode();


    // SDL2-TODO UpdateFocus();
    UpdateGrab();

    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.

    if (vid_fullscreen && !screensaver_mode)
    {
        SDL_Delay(vid_startup_delay);
    }

    // The actual 320x200 canvas that we draw to. This is the pixel buffer of
    // the 8-bit paletted screen buffer that gets blit on an intermediate
    // 32-bit RGBA screen buffer that gets loaded into a texture that gets
    // finally rendered into our window or full screen in I_FinishUpdate().

    I_VideoBuffer = argbbuffer->pixels;
    V_RestoreBuffer();

    // Clear the screen to black.

    memset(I_VideoBuffer, 0, SCREENAREA * sizeof(*I_VideoBuffer));

    // clear out any events waiting at the start and center the mouse
  
    while (SDL_PollEvent(&dummy));

    initialized = true;
}

// [crispy] re-initialize only the parts of the rendering stack that are really necessary

void I_ReInitGraphics (int reinit)
{
	// [crispy] re-set rendering resolution and re-create framebuffers
	if (reinit & REINIT_FRAMEBUFFERS)
	{
		I_GetScreenDimensions();


		// [crispy] re-initialize resolution-agnostic patch drawing
		V_Init();

		SDL_FreeSurface(argbbuffer);
		argbbuffer = SDL_CreateRGBSurfaceWithFormat(
			0, SCREENWIDTH, SCREENHEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);

		I_VideoBuffer = argbbuffer->pixels;
		V_RestoreBuffer();

		// [crispy] it will get re-created below with the new resolution
		SDL_DestroyTexture(texture);
	}

	// [crispy] re-create renderer
	if (reinit & REINIT_RENDERER)
	{
		SDL_RendererInfo info = {0};
		int flags;

		SDL_GetRendererInfo(renderer, &info);
		flags = info.flags;

		if (vid_vsync && !(flags & SDL_RENDERER_SOFTWARE))
		{
			flags |= SDL_RENDERER_PRESENTVSYNC;
		}
		else
		{
			flags &= ~SDL_RENDERER_PRESENTVSYNC;
		}

		SDL_DestroyRenderer(renderer);
		renderer = SDL_CreateRenderer(screen, -1, flags);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

		// [crispy] the texture gets destroyed in SDL_DestroyRenderer(), force its re-creation
		texture_upscaled = NULL;
	}

	// [crispy] re-create textures
	if (reinit & REINIT_TEXTURES)
	{
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

		texture = SDL_CreateTexture(renderer,
		                            SDL_PIXELFORMAT_ARGB8888,
		                            SDL_TEXTUREACCESS_STREAMING,
		                            SCREENWIDTH, SCREENHEIGHT);

		// [crispy] force its re-creation
		CreateUpscaledTexture(true);
	}

	// [crispy] re-set logical rendering resolution
	if (reinit & REINIT_ASPECTRATIO)
	{
		if (vid_aspect_ratio_correct == 1)
		{
			actualheight = 6 * SCREENHEIGHT / 5;
		}
		else
		{
			actualheight = SCREENHEIGHT;
		}

		if (vid_aspect_ratio_correct || vid_integer_scaling)
		{
			SDL_RenderSetLogicalSize(renderer,
			                         SCREENWIDTH,
			                         actualheight);
		}
		else
		{
			SDL_RenderSetLogicalSize(renderer, 0, 0);
		}

		#if SDL_VERSION_ATLEAST(2, 0, 5)
		SDL_RenderSetIntegerScale(renderer, vid_integer_scaling);
		#endif
	}

	// [crispy] adjust the window size and re-set the palette
	need_resize = true;
}

// [crispy] take screenshot of the rendered image

void I_RenderReadPixels (byte **data, int *w, int *h)
{
	SDL_Rect rect;
	SDL_PixelFormat *format;
	int temp;
	uint32_t png_format;
	byte *pixels;

    // [crispy] adjust cropping rectangle if necessary
    rect.x = rect.y = 0;
    SDL_GetRendererOutputSize(renderer, &rect.w, &rect.h);
    if (vid_aspect_ratio_correct)
    {
        if (rect.w * actualheight > rect.h * SCREENWIDTH)
        {
            temp = rect.w;
            rect.w = rect.h * SCREENWIDTH / actualheight;
            rect.x = (temp - rect.w) / 2;
        }
        else
        if (rect.h * SCREENWIDTH > rect.w * actualheight)
        {
            temp = rect.h;
            rect.h = rect.w * actualheight / SCREENWIDTH;
            rect.y = (temp - rect.h) / 2;
        }
    }

    // [crispy] native PNG pixel format
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    png_format = SDL_PIXELFORMAT_ABGR8888;
#else
    png_format = SDL_PIXELFORMAT_RGBA8888;
#endif
    format = SDL_AllocFormat(png_format);
    temp = rect.w * format->BytesPerPixel; // [crispy] pitch

    // [crispy] As far as I understand the issue, SDL_RenderPresent()
    // may return early, i.e. before it has actually finished rendering the
    // current texture to screen -- from where we want to capture it.
    // However, it does never return before it has finished rendering the
    // *previous* texture.
    // Thus, we add a second call to SDL_RenderPresent() here to make sure
    // that it has at least finished rendering the previous texture, which
    // already contains the scene that we actually want to capture.
    if (post_rendering_hook)
    {
        SDL_RenderCopy(renderer, vid_smooth_scaling ? texture_upscaled : texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // [crispy] allocate memory for screenshot image
    pixels = malloc(rect.h * temp);
    SDL_RenderReadPixels(renderer, &rect, format->format, pixels, temp);

    *data = pixels;
    *w = rect.w;
    *h = rect.h;

    SDL_FreeFormat(format);
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
    M_BindIntVariable("vid_startup_delay",             &vid_startup_delay);
    M_BindIntVariable("vid_resize_delay",              &vid_resize_delay);
    M_BindIntVariable("vid_fullscreen",                &vid_fullscreen);
    M_BindIntVariable("vid_fullscreen_exclusive",      &vid_fullscreen_exclusive);
    M_BindIntVariable("vid_video_display",             &vid_video_display);
    M_BindIntVariable("vid_aspect_ratio_correct",      &vid_aspect_ratio_correct);
    M_BindIntVariable("vid_integer_scaling",           &vid_integer_scaling);
    M_BindIntVariable("vid_vga_porch_flash",           &vid_vga_porch_flash);
    M_BindIntVariable("vid_fullscreen_width",          &vid_fullscreen_width);
    M_BindIntVariable("vid_fullscreen_height",         &vid_fullscreen_height);
    M_BindIntVariable("vid_window_title_short",        &vid_window_title_short);
    M_BindIntVariable("vid_force_software_renderer",   &vid_force_software_renderer);
    M_BindIntVariable("vid_max_scaling_buffer_pixels", &vid_max_scaling_buffer_pixels);
    M_BindIntVariable("vid_window_width",              &vid_window_width);
    M_BindIntVariable("vid_window_height",             &vid_window_height);
    M_BindStringVariable("vid_video_driver",           &vid_video_driver);
    M_BindStringVariable("vid_screen_scaler_api",      &vid_screen_scaler_api);
    M_BindIntVariable("vid_window_position_x",         &vid_window_position_x);
    M_BindIntVariable("vid_window_position_y",         &vid_window_position_y);
    M_BindIntVariable("mouse_enable",                  &usemouse);
    M_BindIntVariable("mouse_grab",                    &mouse_grab);
}
