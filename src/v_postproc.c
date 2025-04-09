//
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2025 Julia Nechaevskaya
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
//  Video post processing effect for software renderer.
//

#include "v_postproc.h"


// -----------------------------------------------------------------------------
// V_PProc_AnalogRGBDrift
//  [PN] Applies analog-style RGB drift effect by offsetting red and blue 
//  channels horizontally in opposite directions.
//  Creates a chromatic aberration/glitchy visual by shifting color channels 
//  on CPU after the frame is rendered.
// -----------------------------------------------------------------------------

void V_PProc_AnalogRGBDrift(void)
{
    if (!argbbuffer)
        return;

    const int width = SCREENWIDTH;
    const int height = SCREENHEIGHT;

    // Static temp buffer to store the original frame for sampling
    static pixel_t* chromabuf = NULL;
    static size_t chromabuf_size = 0;

    // Resize buffer if needed (e.g., after resolution change)
    const size_t needed_size = width * height * sizeof(pixel_t);
    if (chromabuf_size != needed_size)
    {
        free(chromabuf);
        chromabuf = malloc(needed_size);
        if (!chromabuf) return;
        chromabuf_size = needed_size;
    }

    // Copy the current frame to buffer for safe reading while modifying original
    pixel_t* src = (pixel_t*)argbbuffer->pixels;
    memcpy(chromabuf, src, needed_size);

            const int dx = vid_resolution + post_rgbdrift; // fixed shift, or make dynamic based on strength

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Compute safe offsets for red (left) and blue (right) components
            const int xr = (x - dx >= 0) ? x - dx : 0;
            const int xb = (x + dx < width) ? x + dx : width - 1;

            // Sample pixels: red shifted left, blue shifted right, green unchanged
            pixel_t orig = chromabuf[y * width + x];
            pixel_t red  = chromabuf[y * width + xr];
            pixel_t blue = chromabuf[y * width + xb];

            // Extract RGB components
            Uint8 r = (red >> 16) & 0xff;
            Uint8 g = (orig >> 8) & 0xff;
            Uint8 b = blue & 0xff;

            // Compose new pixel with shifted R/B and original G; keep alpha
            src[y * width + x] = (r << 16) | (g << 8) | b | 0xff000000;
        }
    }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

static Uint32 *dof_blur_buffer = NULL;
static int dof_buffer_w = 0;
static int dof_buffer_h = 0;

void InitDepthOfFieldBuffer(void)
{
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    int w = argbbuffer->w;
    int h = argbbuffer->h;

    if (dof_blur_buffer && (w != dof_buffer_w || h != dof_buffer_h))
    {
        free(dof_blur_buffer);
        dof_blur_buffer = NULL;
    }

    if (!dof_blur_buffer)
    {
        dof_blur_buffer = malloc(sizeof(Uint32) * w * h);
        dof_buffer_w = w;
        dof_buffer_h = h;
    }
}

void ApplyDepthOfFieldBlur(void)
{
    InitDepthOfFieldBuffer();

    if (!argbbuffer || !dof_blur_buffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    if (width < 3 || height < 3)
        return;

    const int stride = argbbuffer->w;
    Uint32 *pixels = (Uint32 *)argbbuffer->pixels;
    memcpy(dof_blur_buffer, pixels, sizeof(Uint32) * width * height);

    const int cx = width / 2;
    const int cy = height / 2;

    // Адаптивный радиус DOF
    const int base_threshold = 150;
    const int blur_threshold = base_threshold * vid_resolution;
    const int thresholdSq = blur_threshold * blur_threshold;

    // Адаптивный радиус маски блюра (1 = 3x3, 2 = 5x5, 3 = 7x7)
    const int blur_radius = (vid_resolution <= 2) ? 1 : (vid_resolution <= 4) ? 2 : 3;
    const int blur_size = (2 * blur_radius + 1) * (2 * blur_radius + 1);

    for (int y = blur_radius; y < height - blur_radius; ++y)
    {
        for (int x = blur_radius; x < width - blur_radius; ++x)
        {
            int dx = x - cx;
            int dy = y - cy;
            if ((dx * dx + dy * dy) <= thresholdSq)
                continue;

            int r = 0, g = 0, b = 0;

            for (int ky = -blur_radius; ky <= blur_radius; ++ky)
            {
                for (int kx = -blur_radius; kx <= blur_radius; ++kx)
                {
                    Uint32 c = dof_blur_buffer[(y + ky) * stride + (x + kx)];

                    b +=  c        & 0xFF;
                    g += (c >> 8)  & 0xFF;
                    r += (c >> 16) & 0xFF;
                }
            }

            r /= blur_size;
            g /= blur_size;
            b /= blur_size;

            Uint32 a = pixels[y * stride + x] & 0xFF000000;
            pixels[y * stride + x] = a | (r << 16) | (g << 8) | b;
        }
    }
}
