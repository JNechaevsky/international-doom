//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath, Paul Haeberli
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


#include <math.h>
#include "m_fixed.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"


// -----------------------------------------------------------------------------
// [crispy] here used to be static color translation tables based on
// the ones found in Boom and MBF. Nowadays these are recalculated
// by means of actual color space conversions in r_data:R_InitColormaps().
// -----------------------------------------------------------------------------

static byte cr_menu_bright5[256];
static byte cr_menu_bright4[256];
static byte cr_menu_bright3[256];
static byte cr_menu_bright2[256];
static byte cr_menu_bright1[256];
static byte cr_menu_dark1[256];
static byte cr_menu_dark2[256];
static byte cr_menu_dark3[256];
static byte cr_menu_dark4[256];

static byte cr_red[256];
static byte cr_red_bright[256];

static byte cr_darkred[256];

static byte cr_green[256];
static byte cr_green_bright[256];

static byte cr_green_hx[256];
static byte cr_green_hx_bright[256];

static byte cr_darkgreen[256];
static byte cr_darkgreen_bright[256];

static byte cr_olive[256];
static byte cr_olive_bright[256];

static byte cr_blue2[256];
static byte cr_blue2_bright[256];

static byte cr_yellow[256];
static byte cr_yellow_bright[256];

static byte cr_orange[256];
static byte cr_orange_bright[256];

// [JN] Slightly different orange for Heretic.
static byte cr_orange_hr[256];
static byte cr_orange_hr_bright[256];

static byte cr_white[256];

static byte cr_gray[256];
static byte cr_gray_bright[256];

static byte cr_lightgray[256];
static byte cr_lightgray_bright[256];
static byte cr_lightgray_dark[256];

static byte cr_brown[256];
static byte cr_flame[256];
static byte cr_purple[256];

static const byte cr_red2blue[256] =
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
     16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
     32,33,34,35,36,37,38,39,40,41,42,43,207,207,46,207,
     48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
     64,65,66,207,68,69,70,71,72,73,74,75,76,77,78,79,
     80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
     96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
     112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
     200,200,201,201,202,202,203,203,204,204,205,205,206,206,207,207,
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};

static const byte cr_red2green[256] =
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
     16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
     32,33,34,35,36,37,38,39,40,41,42,43,127,127,46,127,
     48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
     64,65,66,127,68,69,70,71,72,73,74,75,76,77,78,79,
     80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
     96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
     112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
     114,115,116,117,118,119,120,121,122,123,124,125,126,126,127,127,
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};

byte *cr[] =
{
    (byte *) &cr_menu_bright5,
    (byte *) &cr_menu_bright4,
    (byte *) &cr_menu_bright3,
    (byte *) &cr_menu_bright2,
    (byte *) &cr_menu_bright1,
    (byte *) &cr_menu_dark1,
    (byte *) &cr_menu_dark2,
    (byte *) &cr_menu_dark3,
    (byte *) &cr_menu_dark4,

    (byte *) &cr_red,
    (byte *) &cr_red_bright,

    (byte *) &cr_darkred,

    (byte *) &cr_green,
    (byte *) &cr_green_bright,

    (byte *) &cr_green_hx,
    (byte *) &cr_green_hx_bright,

    (byte *) &cr_darkgreen,
    (byte *) &cr_darkgreen_bright,

    (byte *) &cr_olive,
    (byte *) &cr_olive_bright,

    (byte *) &cr_blue2,
    (byte *) &cr_blue2_bright,

    (byte *) &cr_yellow,
    (byte *) &cr_yellow_bright,

    (byte *) &cr_orange,
    (byte *) &cr_orange_bright,

    (byte *) &cr_orange_hr,
    (byte *) &cr_orange_hr_bright,

    (byte *) &cr_white,

    (byte *) &cr_gray,
    (byte *) &cr_gray_bright,

    (byte *) &cr_lightgray,
    (byte *) &cr_lightgray_bright,
    (byte *) &cr_lightgray_dark,

    (byte *) &cr_brown,
    (byte *) &cr_flame,
    (byte *) &cr_purple,

    (byte *) &cr_red2blue,
    (byte *) &cr_red2green,
};

char **crstr = 0;

/*
Date: Sun, 26 Oct 2014 10:36:12 -0700
From: paul haeberli <paulhaeberli@yahoo.com>
Subject: Re: colors and color conversions
To: Fabian Greffrath <fabian@greffrath.com>

Yes, this seems exactly like the solution I was looking for. I just
couldn't find code to do the HSV->RGB conversion. Speaking of the code,
would you allow me to use this code in my software? The Doom source code
is licensed under the GNU GPL, so this code yould have to be under a
compatible license.

    Yes. I'm happy to contribute this code to your project.  GNU GPL or anything
    compatible sounds fine.

Regarding the conversions, the procedure you sent me will leave grays
(r=g=b) untouched, no matter what I set as HUE, right? Is it possible,
then, to also use this routine to convert colors *to* gray?

    You can convert any color to an equivalent grey by setting the saturation
    to 0.0


    - Paul Haeberli
*/

#define CTOLERANCE      (0.0001)

typedef struct vect {
    float x;
    float y;
    float z;
} vect;

static void hsv_to_rgb (vect *hsv, vect *rgb)
{
    float h, s, v;

    h = hsv->x;
    s = hsv->y;
    v = hsv->z;
    h *= 360.0;
    if (s < CTOLERANCE)
    {
        rgb->x = v;
        rgb->y = v;
        rgb->z = v;
    }
    else
    {
        int i;
        float f, p, q, t;

        if (h >= 360.0)
        {
            h -= 360.0;
        }
        h /= 60.0;
        i = floor(h);
        f = h - i;
        p = v*(1.0-s);
        q = v*(1.0-(s*f));
        t = v*(1.0-(s*(1.0-f)));
        switch (i)
        {
            case 0:
                rgb->x = v;
                rgb->y = t;
                rgb->z = p;
            break;
            case 1:
                rgb->x = q;
                rgb->y = v;
                rgb->z = p;
            break;
            case 2:
                rgb->x = p;
                rgb->y = v;
                rgb->z = t;
            break;
            case 3:
                rgb->x = p;
                rgb->y = q;
                rgb->z = v;
            break;
            case 4:
                rgb->x = t;
                rgb->y = p;
                rgb->z = v;
            break;
            case 5:
                rgb->x = v;
                rgb->y = p;
                rgb->z = q;
            break;
        }
    }
}

static void rgb_to_hsv(vect *rgb, vect *hsv)
{
    float h, s, v;
    float cmax, cmin;
    float r, g, b;

    r = rgb->x;
    g = rgb->y;
    b = rgb->z;
    /* find the cmax and cmin of r g b */
    cmax = r;
    cmin = r;
    cmax = (g > cmax ? g : cmax);
    cmin = (g < cmin ? g : cmin);
    cmax = (b > cmax ? b : cmax);
    cmin = (b < cmin ? b : cmin);
    v = cmax;           /* value */
    
    if (cmax > CTOLERANCE)
    {
        s = (cmax - cmin) / cmax;
    }
    else
    {
        s = 0.0;
        h = 0.0;
    }
    if (s < CTOLERANCE)
    {
        h = 0.0;
    }
    else
    {
        float cdelta;
        float rc, gc, bc;

        cdelta = cmax-cmin;
        rc = (cmax - r) / cdelta;
        gc = (cmax - g) / cdelta;
        bc = (cmax - b) / cdelta;
        if (r == cmax)
        {
            h = bc - gc;
        }
        else if (g == cmax)
        {
            h = 2.0 + rc - bc;
        }
        else
        {
            h = 4.0 + gc - rc;
        }
        h = h * 60.0;
        if (h < 0.0)
        {
            h += 360.0;
        }
    }
    hsv->x = h / 360.0;
    hsv->y = s;
    hsv->z = v;
}

 
// [crispy] copied over from i_video.c
// [PN] Refactored for performance
int V_GetPaletteIndex(byte *palette, int r, int g, int b)
{
    int best = 0;
    int best_diff = INT_MAX;

    for (int i = 0; i < 256; i++, palette += 3)
    {
        // [PN] Explicit cast to int to ensure correct calculation
        const int dr = (int)r - (int)palette[0];
        const int dg = (int)g - (int)palette[1];
        const int db = (int)b - (int)palette[2];

        // [PN] Compute the sum of squared differences (Euclidean distance in RGB space)
        const int diff = dr * dr + dg * dg + db * db;

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;

            if (diff == 0)
                break; // [PN] Early exit for exact match
        }
    }

    return best;
}

byte V_Colorize (byte *playpal, int cr, byte source, boolean keepgray109)
{
    vect rgb, hsv;

    // [crispy] preserve gray drop shadow in IWAD status bar numbers
    if (cr == CR_NONE || (keepgray109 && source == 109))
    {
        return source;
    }

    rgb.x = playpal[3 * source + 0] / 255.;
    rgb.y = playpal[3 * source + 1] / 255.;
    rgb.z = playpal[3 * source + 2] / 255.;

    rgb_to_hsv(&rgb, &hsv);

    // [JN] Menu glowing effects.
    if (cr == CR_MENU_BRIGHT5)
    {
        hsv.z *= 1.5f;
    }
    else if (cr == CR_MENU_BRIGHT4)
    {
        hsv.z *= 1.4f;
    }
    else if (cr == CR_MENU_BRIGHT3)
    {
        hsv.z *= 1.3f;
    }
    else if (cr == CR_MENU_BRIGHT2)
    {
        hsv.z *= 1.2f;
    }
    else if (cr == CR_MENU_BRIGHT1)
    {
        hsv.z *= 1.1f;
    }
    else if (cr == CR_MENU_DARK1)
    {
        hsv.z *= 0.9f;
    }
    else if (cr == CR_MENU_DARK2)
    {
        hsv.z *= 0.8f;
    }
    else if (cr == CR_MENU_DARK3)
    {
        hsv.z *= 0.7f;
    }    
    else if (cr == CR_MENU_DARK4)
    {
        hsv.z *= 0.6f;
    }
    else
    {
        // [crispy] hack colors to full saturation
        hsv.y = 1.0f;

        if (cr == CR_RED)
        {
            hsv.x = 0.f;
        }
        else if (cr == CR_RED_BRIGHT)
        {
            hsv.x = 0.f;
            hsv.z *= 1.5f;
        }

        else if (cr == CR_DARKRED)
        {
            hsv.x = 0.f;
            hsv.z *= 0.666f;
        }

        else if (cr == CR_GREEN)
        {
            hsv.x = (144.f * hsv.z + 140.f * (1.f - hsv.z))/360.f;
        }
        else if (cr == CR_GREEN_BRIGHT)
        {
            hsv.x = 0.3f;
            hsv.z *= 1.5f;
        }

        // [JN] Slightly different for Hexen...
        else if (cr == CR_GREEN_HX)
        {
            hsv.x = 0.425f;
            hsv.y = 0.75f;
            hsv.z *= 0.8f;
        }
        else if (cr == CR_GREEN_HX_BRIGHT)
        {
            hsv.x = 0.425f;
            hsv.y = 0.75f;
            hsv.z *= 1.15f;
        }

        else if (cr == CR_DARKGREEN)
        {
            hsv.x = 0.3f;
            hsv.z *= 0.666f;
        }
        else if (cr == CR_DARKGREEN_BRIGHT)
        {
            hsv.x = 0.3f;
            hsv.z *= 0.888f;
        }

        else if (cr == CR_OLIVE)
        {
            hsv.x = 0.25f;
            hsv.y = 0.5f;
            hsv.z *= 0.5f;
        }
        else if (cr == CR_OLIVE_BRIGHT)
        {
            hsv.x = 0.25f;
            hsv.y = 0.5f;
            hsv.z *= 0.65f;
        }

        else if (cr == CR_BLUE2)
        {
            hsv.x = 0.65f;
            hsv.z *= 1.2f;
        }
        else if (cr == CR_BLUE2_BRIGHT)
        {
            hsv.x = 0.65f;
            hsv.z *= 1.55f;
        }

        else if (cr == CR_YELLOW)
        {
            hsv.x = (7.0f + 53.f * hsv.z)/360.f;
            hsv.y = 1.0f - 0.4f * hsv.z;
            hsv.z = 0.2f + 0.8f * hsv.z;
        }
        else if (cr == CR_YELLOW_BRIGHT)
        {
            hsv.x = (7.0f + 53.f * hsv.z)/360.f;
            hsv.y = 1.0f - 0.4f * hsv.z;
            hsv.z = 0.2f + 1.05f * hsv.z;
        }

        else if (cr == CR_ORANGE)
        {
            hsv.x = 0.075f;
            hsv.z *= 1.15f;
        }
        else if (cr == CR_ORANGE_BRIGHT)
        {
            hsv.x = 0.075f;
            hsv.z *= 1.40f;
        }

        else if (cr == CR_ORANGE_HR)
        {
            hsv.x = 0.0777f;
            hsv.z *= 1.15f;
        }
        else if (cr == CR_ORANGE_HR_BRIGHT)
        {
            hsv.x = 0.0777f;
            hsv.z *= 1.40f;
        }

        else if (cr == CR_WHITE)
        {
            hsv.y = 0.f;
        }
        else if (cr == CR_GRAY)
        {
            hsv.y = 0.f;
            hsv.z *= 0.5f;
        }
        else if (cr == CR_GRAY_BRIGHT)
        {
            hsv.y = 0.f;
            hsv.z *= 0.75f;
        }

        else if (cr == CR_LIGHTGRAY)
        {
            hsv.y = 0.f;
            hsv.z *= 0.80f;
        }
        else if (cr == CR_LIGHTGRAY_BRIGHT)
        {
            hsv.y = 0.f;
            hsv.z *= 1.05f;
        }
        else if (cr == CR_LIGHTGRAY_DARK)
        {
            hsv.y = 0.f;
            hsv.z *= 0.6f;
        }
        else if (cr == CR_BROWN)
        {
            hsv.x = 0.1f;
            hsv.y = 0.75f;
            hsv.z *= 0.65f;
        }
        else if (cr == CR_FLAME)
        {
            hsv.x = 0.125f;
            hsv.z *= 1.75f;
        }
        else if (cr == CR_PURPLE)
        {
            hsv.x = 0.85f;
        }
    }

    hsv_to_rgb(&hsv, &rgb);

    rgb.x *= 255.f;
    rgb.y *= 255.f;
    rgb.z *= 255.f;

    return V_GetPaletteIndex(playpal, (int) rgb.x, (int) rgb.y, (int) rgb.z);
}
