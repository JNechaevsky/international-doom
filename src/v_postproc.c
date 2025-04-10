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
// V_PProc_OverbrightGlow
// [PN] Applies dynamic exposure adjustment based on average frame brightness.
// Implements "Overbright Glow" — an inverse HDR effect that intensifies 
// already bright scenes and slightly fades darker ones.
// -----------------------------------------------------------------------------

void V_PProc_OverbrightGlow (void)
{
    // [PN] Validate input buffer and 32-bit format.
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    const int total_pixels = width * height;
    Uint32 *pixels = (Uint32*)argbbuffer->pixels;

    // -------------------------------------------------------------------------
    // Step 1: calculate average brightness (0..255)
    // -------------------------------------------------------------------------

    uint64_t total_brightness = 0;
    Uint32 *p = pixels;
    for (int i = 0; i < total_pixels; ++i)
    {
        Uint32 px = *p++;
        Uint8 r = (Uint8)(px >> 16);
        Uint8 g = (Uint8)(px >> 8);
        Uint8 b = (Uint8)(px);

        total_brightness += ((int)r * 3 + (int)g * 5 + (int)b * 2) / 10;
    }

    int avg_brightness = (int)(total_brightness / total_pixels);  // 0..255

    // -------------------------------------------------------------------------
    // Step 2: calculate target exposure (Q8.8 fixed point)
    // -------------------------------------------------------------------------

    // target_exposure = 0.25 + (avg_brightness / 255.0) * 2.5;
    // in Q8.8: 64 + avg_brightness * 640 / 255;
    int target_exp = 64 + (avg_brightness * 640) / 255;

    const int min_exp = 230;   // ~0.90 (0.9 * 256)
    const int max_exp = 1024;  //  4.00 (  4 * 256)

    if (target_exp < min_exp) target_exp = min_exp;
    if (target_exp > max_exp) target_exp = max_exp;

    // -------------------------------------------------------------------------
    // Step 3: smooth exposure transition
    // -------------------------------------------------------------------------

    static int exposure = 256;  // initial exposure level (Q8.8 == 1.0)
    const int adapt_rate = 13;  // how quickly we adapt (~0.05 in Q8.8)

    exposure += ((target_exp - exposure) * adapt_rate) >> 8;

    // -------------------------------------------------------------------------
    // Step 4: apply exposure to all pixels (r * exposure >> 8)
    // -------------------------------------------------------------------------

    p = pixels;
    for (int i = 0; i < total_pixels; ++i)
    {
        Uint32 px = *p;

        int r = (Uint8)(px >> 16);
        int g = (Uint8)(px >> 8);
        int b = (Uint8)px;

        r = (r * exposure) >> 8;
        g = (g * exposure) >> 8;
        b = (b * exposure) >> 8;

        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;

        *p++ = (0xFFU << 24) | (r << 16) | (g << 8) | b;
    }
}

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
// V_PProc_VHSLineDistortion
//  [PN] Applies a VHS-style horizontal line glitch effect.
//  Random horizontal line segments are shifted left or right across the frame,
//  mimicking analog tape distortions. This effect is subtle but expressive,
//  especially at higher resolutions or during gameplay intensity spikes.
//  It introduces short-lived pixel offsets for 2–4 scanlines at a time,
//  with randomized amplitude and location each frame.
// -----------------------------------------------------------------------------

void V_PProc_VHSLineDistortion (void)
{
    // Check if argbbuffer exists and is in 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width = argbbuffer->w;
    const int height = argbbuffer->h;
    Uint32 *pixels = (Uint32*)argbbuffer->pixels;

    // Determine the number of glitch blocks and block height
    const int max_blocks = 1;
    const int block_height = 2 + rand() % 3; // Each glitch block is 2–4 lines high
    const int glitch_intensity = (vid_resolution > 3 ? 4 : 2) + rand() % 3;
    const int stride = width;

    for (int i = 0; i < glitch_intensity && i < max_blocks; ++i)
    {
        const int y_start = rand() % (height - block_height);
        // Horizontal shift in the range [-5 .. +5]
        const int shift_val = (rand() % 11) - 5;
        // Skip when shift is zero (no change)
        if (shift_val == 0)
            continue;

        for (int y = y_start; y < y_start + block_height; ++y)
        {
            Uint32* row = pixels + y * stride;
            if (shift_val > 0)
            {
                // For positive shifts, move current row to the right.
                // memmove handles overlapping memory areas efficiently.
                memmove(row + shift_val, row, (width - shift_val) * sizeof(Uint32));
                // Fill the gap on the left with opaque black
                for (int x = 0; x < shift_val; ++x)
                    row[x] = 0xFF000000;
            }
            else // shift_val < 0
            {
                const int shift = -shift_val;
                // For negative shifts, move current row to the left
                memmove(row, row + shift, (width - shift) * sizeof(Uint32));
                // Fill the gap on the right with opaque black
                for (int x = width - shift; x < width; ++x)
                    row[x] = 0xFF000000;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// V_PProc_MotionBlur
//  [PN] Applies a motion blur effect by blending the current frame with a 
//  previously stored frame. This creates a perceptual smearing effect that 
//  enhances the feeling of motion, especially at lower framerates.
//
//  The blending strength is dynamically controlled by the post_motionblur 
//  variable, which adjusts the RGB weighting between the current 
//  and previous frame (e.g., 9:1 = very subtle, 9:1 = strong trail).
//
//  At uncapped framerate, the function uses a minimal ring buffer for frame 
//  history; at capped framerate (35 FPS), it switches to a simpler single-buffer 
//  mode for efficiency.
//
//  The function responds to resolution changes and preserves internal state 
//  across frames to maintain smooth and stable rendering effects.
// -----------------------------------------------------------------------------


#define MAX_BLUR_LAG 1

void V_PProc_MotionBlur(void)
{
    // Validate input buffer and 32-bit pixel format.
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    Uint32 *pixels   = (Uint32*)argbbuffer->pixels;
    const size_t size = width * height * sizeof(Uint32);
    const int total_pixels = width * height;

    // Declare static buffers for previous frame and ring buffer.
    static Uint32 *prev_frame = NULL;
    static size_t  prev_size = 0;
    static Uint32 *frame_ring[MAX_BLUR_LAG + 1] = {0};
    static int ring_index = 0;
    static size_t ring_size = 0;

    // [JN] Modulate effect intensity
    int blend_curr = 0, blend_prev = 0; // initialize
    switch (post_motionblur)
    {
        case 1: blend_curr = 8; blend_prev = 2; break; // Soft
        case 2: blend_curr = 7; blend_prev = 3; break; // Light
        case 3: blend_curr = 6; blend_prev = 4; break; // Medium
        case 4: blend_curr = 5; blend_prev = 5; break; // Heavy
        case 5: blend_curr = 1; blend_prev = 9; break; // Ghost
    }

    // Allocate or reallocate memory for the buffers if frame size changed.
    if (vid_uncapped_fps)
    {
        if (ring_size != size)
        {
            for (int i = 0; i <= MAX_BLUR_LAG; ++i)
            {
                free(frame_ring[i]);
                frame_ring[i] = malloc(size);
                if (frame_ring[i])
                    memset(frame_ring[i], 0, size);
            }
            ring_size = size;
        }
    }
    else
    {
        if (prev_size != size)
        {
            free(prev_frame);
            prev_frame = malloc(size);
            if (prev_frame)
                memset(prev_frame, 0, size);
            prev_size = size;
        }
    }

    // Motion blur blending.
    if (vid_uncapped_fps)
    {
        // Use ring buffer: retrieve previous frame at an index based on delay.
        const int prev_index = (ring_index + MAX_BLUR_LAG) % (MAX_BLUR_LAG + 1);
        Uint32 *ring_prev = frame_ring[prev_index];
        if (!ring_prev)
            return;

        // Blending loop using pointer arithmetic.
        for (int i = 0; i < total_pixels; ++i)
        {
            Uint32 curr = pixels[i], old = ring_prev[i];
            int r = (((curr >> 16) & 0xFF) * blend_curr + ((old >> 16) & 0xFF) * blend_prev) / 10;
            int g = (((curr >> 8)  & 0xFF) * blend_curr + ((old >> 8)  & 0xFF) * blend_prev) / 10;
            int b = (((curr)       & 0xFF) * blend_curr + ((old)       & 0xFF) * blend_prev) / 10;
            pixels[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
        // Advance ring buffer index and save current frame.
        ring_index = (ring_index + 1) % (MAX_BLUR_LAG + 1);
        memcpy(frame_ring[ring_index], pixels, size);
    }
    else
    {
        // Single-buffer mode: blend current frame with prev_frame.
        if (!prev_frame)
            return;
        for (int i = 0; i < total_pixels; ++i)
        {
            Uint32 curr = pixels[i], old = prev_frame[i];
            int r = (((curr >> 16) & 0xFF) * blend_curr + ((old >> 16) & 0xFF) * blend_prev) / 10;
            int g = (((curr >> 8)  & 0xFF) * blend_curr + ((old >> 8)  & 0xFF) * blend_prev) / 10;
            int b = (((curr)       & 0xFF) * blend_curr + ((old)       & 0xFF) * blend_prev) / 10;
            pixels[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
        memcpy(prev_frame, pixels, size);
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
