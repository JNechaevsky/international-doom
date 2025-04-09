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

void V_PProc_AnalogRGBDrift (void)
{
    if (!argbbuffer)
        return;

    const int width = SCREENWIDTH;
    const int height = SCREENHEIGHT;

    // Static buffer to store the original frame for safe sampling during modification
    static pixel_t* chromabuf = NULL;
    static size_t chromabuf_size = 0;

    // Reallocate buffer if resolution has changed
    const size_t needed_size = width * height * sizeof(pixel_t);
    if (chromabuf_size != needed_size)
    {
        free(chromabuf);
        chromabuf = malloc(needed_size);
        if (!chromabuf)
            return;
        chromabuf_size = needed_size;
    }

    // Copy the current frame into the temporary buffer
    pixel_t *const src = (pixel_t*)argbbuffer->pixels;
    memcpy(chromabuf, src, needed_size);

    // [JN] Calculate RGB drift offset depending on resolution.
    const int dx = post_rgbdrift + (vid_resolution > 2 ? (vid_resolution - 2) : 0);

    // Precompute shifted column indices for red (left) and blue (right) channels
    static int *x_src_r = NULL;
    static int *x_src_b = NULL;
    static int allocated_width = 0;
    static int last_dx = -1;
    if (allocated_width != width || last_dx != dx)
    {
        free(x_src_r);
        free(x_src_b);
        x_src_r = malloc(sizeof(int) * width);
        x_src_b = malloc(sizeof(int) * width);
        if (!x_src_r || !x_src_b)
        {
            free(x_src_r);
            free(x_src_b);
            x_src_r = x_src_b = NULL;
            return;
        }
        allocated_width = width;
        last_dx = dx;
        for (int x = 0; x < width; ++x)
        {
            x_src_r[x] = (x - dx >= 0) ? x - dx : 0;
            x_src_b[x] = (x + dx < width) ? x + dx : width - 1;
        }
    }

    // Use row pointers to avoid repeated multiplications in pixel indexing
    for (int y = 0; y < height; ++y)
    {
        pixel_t *const srcRow = src + y * width;
        const pixel_t *const chromaRow = chromabuf + y * width;
        for (int x = 0; x < width; ++x)
        {
            // Fetch original pixel and shifted red/blue samples
            pixel_t orig = chromaRow[x];
            pixel_t red  = chromaRow[x_src_r[x]];
            pixel_t blue = chromaRow[x_src_b[x]];

            // Extract RGB components from the appropriate pixels
            Uint8 r = (red >> 16) & 0xff;
            Uint8 g = (orig >> 8) & 0xff;
            Uint8 b = blue & 0xff;

            // Compose the final pixel with altered red/blue and original green; alpha is fixed
            srcRow[x] = (r << 16) | (g << 8) | b | 0xff000000;
        }
    }
}

// -----------------------------------------------------------------------------
// V_PProc_DepthOfFieldBlur
//  [PN] Applies a radial depth-of-field blur effect based on distance from
//  screen center. Pixels near the center remain sharp, while those farther
//  away are blurred using a variable-size box blur kernel.
//  The effect simulates camera-like focus, with adaptive radius and strength
//  depending on current resolution settings. Handles edges by clamping kernel.
//
//  Uses a hybrid approach: fast kernel pass for safe central
//  area, and boundary-aware blur for edges to ensure full-frame coverage.
// -----------------------------------------------------------------------------

static Uint32 *dof_blur_buffer = NULL;
static int dof_buffer_w = 0;
static int dof_buffer_h = 0;

static void InitDepthOfFieldBuffer (void)
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

void V_PProc_DepthOfFieldBlur (void)
{
    InitDepthOfFieldBuffer();

    if (!argbbuffer || !dof_blur_buffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    if (width < 3 || height < 3)
        return;

    const int stride = width;
    Uint32* pixels = (Uint32*)argbbuffer->pixels;
    memcpy(dof_blur_buffer, pixels, sizeof(Uint32) * width * height);

    const int cx = width / 2;
    const int cy = height / 2;

    const int base_threshold = 150;
    const int blur_threshold = base_threshold * vid_resolution;
    const int thresholdSq = blur_threshold * blur_threshold;

    const int blur_radius = (vid_resolution <= 2) ? 1 : (vid_resolution <= 4) ? 2 : 3;
    const int blur_size = (2 * blur_radius + 1) * (2 * blur_radius + 1);

    const int x_start = blur_radius;
    const int x_end   = width - blur_radius;
    const int y_start = blur_radius;
    const int y_end   = height - blur_radius;

    // [PN] Fast path: process safe central zone without boundary checks
    for (int y = y_start; y < y_end; ++y)
    {
        int dy = y - cy;
        int dy2 = dy * dy;
        Uint32* destRow = pixels + y * stride;

        for (int x = x_start; x < x_end; ++x)
        {
            int dx = x - cx;
            if (dx * dx + dy2 <= thresholdSq)
                continue;

            int r_sum = 0, g_sum = 0, b_sum = 0;

            for (int ky = -blur_radius; ky <= blur_radius; ++ky)
            {
                Uint32* kernelRow = dof_blur_buffer + (y + ky) * stride;
                for (int kx = -blur_radius; kx <= blur_radius; ++kx)
                {
                    Uint32 c = kernelRow[x + kx];
                    b_sum += c & 0xFF;
                    g_sum += (c >> 8) & 0xFF;
                    r_sum += (c >> 16) & 0xFF;
                }
            }

            int r_avg = r_sum / blur_size;
            int g_avg = g_sum / blur_size;
            int b_avg = b_sum / blur_size;

            Uint32 a = destRow[x] & 0xFF000000;
            destRow[x] = a | (r_avg << 16) | (g_avg << 8) | b_avg;
        }
    }

    // [PN] Slow path: process edges with boundary checks
    for (int y = 0; y < height; ++y)
    {
        int dy = y - cy;
        int dy2 = dy * dy;
        Uint32* destRow = pixels + y * stride;

        for (int x = 0; x < width; ++x)
        {
            // [PN] Skip already-processed central area
            if (x >= x_start && x < x_end && y >= y_start && y < y_end)
                continue;

            int dx = x - cx;
            if (dx * dx + dy2 <= thresholdSq)
                continue;

            int r_sum = 0, g_sum = 0, b_sum = 0;
            int count = 0;

            for (int ky = -blur_radius; ky <= blur_radius; ++ky)
            {
                int ny = y + ky;
                if (ny < 0 || ny >= height)
                    continue;

                Uint32* kernelRow = dof_blur_buffer + ny * stride;
                for (int kx = -blur_radius; kx <= blur_radius; ++kx)
                {
                    int nx = x + kx;
                    if (nx < 0 || nx >= width)
                        continue;

                    Uint32 c = kernelRow[nx];
                    b_sum += c & 0xFF;
                    g_sum += (c >> 8) & 0xFF;
                    r_sum += (c >> 16) & 0xFF;
                    ++count;
                }
            }

            if (count == 0)
                continue;

            int r_avg = r_sum / count;
            int g_avg = g_sum / count;
            int b_avg = b_sum / count;

            Uint32 a = destRow[x] & 0xFF000000;
            destRow[x] = a | (r_avg << 16) | (g_avg << 8) | b_avg;
        }
    }
}
