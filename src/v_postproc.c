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
// V_PProc_SupersampledSmoothing
//  [PN] Applies a fast full-screen smoothing effect via supersampled downscale
//  and upscale. This technique reduces pixel harshness and aliasing effects by
//  averaging screen regions into a smaller buffer and then scaling them back,
//  producing soft transitions and visually pleasing results.
//
//  Unlike traditional blurring, this method preserves structure and color
//  boundaries, creating a subtle antialiasing-like effect. The smoothing level
//  is controlled by `post_supersample`, where higher values produce stronger
//  smoothing.
// -----------------------------------------------------------------------------

static Uint32 *blur_small = NULL;
static int blur_small_w = 0, blur_small_h = 0;

void V_PProc_SupersampledSmoothing(boolean st_background_on, int st_height)
{
    // Ensure framebuffer is valid and in 32-bit mode
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int w = argbbuffer->w;
    // [JN] Exclude status bar area from smoothing if active.
    const int h = argbbuffer->h - (st_background_on ? st_height : 0);

    // Scale determines the reduction factor: 2 = half, 4 = quarter, etc.
    const int scale = post_supersample + 1;
    const int sw = w / scale;
    const int sh = h / scale;

    if (sw < 2 || sh < 2)
        return;

    const size_t size = sizeof(Uint32) * sw * sh;

    // Reallocate working buffer if size changed
    if (!blur_small || blur_small_w != sw || blur_small_h != sh)
    {
        free(blur_small);
        blur_small = malloc(size);
        if (!blur_small) return;
        blur_small_w = sw;
        blur_small_h = sh;
    }

    Uint32 *src = (Uint32 *)argbbuffer->pixels;
    Uint32 *dst = blur_small;

    // -------------------------------------------------------------------------
    // Step 1: Downscale — average pixels in blocks (scale x scale)
    // -------------------------------------------------------------------------

    for (int y = 0; y < sh; ++y)
    {
        for (int x = 0; x < sw; ++x)
        {
            int r = 0, g = 0, b = 0, count = 0;

            for (int ky = 0; ky < scale; ++ky)
            {
                const int sy = y * scale + ky;
                if (sy >= h) continue;

                for (int kx = 0; kx < scale; ++kx)
                {
                    const int sx = x * scale + kx;
                    if (sx >= w) continue;

                    Uint32 c = src[sy * w + sx];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                    ++count;
                }
            }

            if (count > 0)
            {
                r /= count;
                g /= count;
                b /= count;
            }

            dst[y * sw + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // -------------------------------------------------------------------------
    // Step 2: Upscale — expand each low-res pixel back into full-resolution
    // -------------------------------------------------------------------------

    for (int y = 0; y < h; ++y)
    {
        int sy = y / scale;
        if (sy >= sh) sy = sh - 1;

        for (int x = 0; x < w; ++x)
        {
            int sx = x / scale;
            if (sx >= sw) sx = sw - 1;

            src[y * w + x] = dst[sy * sw + sx];
        }
    }
}


// -----------------------------------------------------------------------------
// V_PProc_OverbrightGlow
// [PN] Applies dynamic exposure adjustment based on average frame brightness.
// Implements "Overbright Glow" — an inverse HDR effect that intensifies 
// already bright scenes and slightly fades darker ones.
// -----------------------------------------------------------------------------

static void V_PProc_OverbrightGlow (void)
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

static void V_PProc_AnalogRGBDrift (void)
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

static void V_PProc_VHSLineDistortion (void)
{
    // Check if argbbuffer exists and is in 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width = argbbuffer->w;
    const int height = argbbuffer->h;
    Uint32 *pixels = (Uint32*)argbbuffer->pixels;

    // Determine the number of glitch blocks and block height
    const int max_blocks = 1;
    const int block_height = (4 + rand() % 3) * vid_resolution; // Each block is 4–6 lines per resolution
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
// V_PProc_ScreenVignette
//  [PN] Applies a radial vignette effect that darkens pixels based on distance
//  from screen center. Intensity is controlled by 'post_vignette' variable,
//  mapping to predefined attenuation levels.
//  The effect subtly focuses player's vision toward center, enhancing atmosphere.
//
//  Implemented using Q8.8 fixed-point math — no floats used.
// -----------------------------------------------------------------------------

static void V_PProc_ScreenVignette (void)
{
    // Validate input buffer and 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    Uint32 *pixels   = (Uint32 *)argbbuffer->pixels;

    const int cx = width / 2;
    const int cy = height / 2;
    const int max_dist2 = cx * cx + cy * cy;

    // Attenuation levels by post_vignette (Q8.8 fixed-point)
    // 0 = OFF, higher = stronger vignette
    static const int attenuation_table[] = { 0, 80, 146, 200, 255 };
    int attenuation_max = attenuation_table[post_vignette];

    for (int y = 0; y < height; ++y)
    {
        const int dy = y - cy;
        const int dy2 = dy * dy;

        for (int x = 0; x < width; ++x)
        {
            const int dx = x - cx;
            const int dist2 = dx * dx + dy2;

            // attenuation = distance² scaled to max attenuation
            int atten = (dist2 * attenuation_max) / max_dist2;
            if (atten > attenuation_max)
                atten = attenuation_max;

            // scale = 1.0 - attenuation (in Q8.8)
            const int scale = 256 - atten;

            const int i = y * width + x;
            Uint32 px = pixels[i];

            int r = (px >> 16) & 0xFF;
            int g = (px >> 8) & 0xFF;
            int b = px & 0xFF;

            r = (r * scale) >> 8;
            g = (g * scale) >> 8;
            b = (b * scale) >> 8;

            pixels[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
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

static void V_PProc_MotionBlur (void)
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
//  [PN] Applies a radial depth-of-field blur effect directly on the framebuffer,
//  softening only peripheral areas beyond a dynamic focus threshold.
//
//  The effect simulates shallow camera focus: the center remains sharp,
//  while edges are softened via a 3x3 box blur. Pixels are blurred only if 
//  their squared distance from the screen center exceeds a threshold
//  proportional to the current resolution.
//
//  Unlike traditional approaches, this version avoids using a secondary buffer
//  and performs all processing in-place, greatly reducing memory usage.
//
//  The main blur pass unrolls the 3x3 kernel using three row pointers for
//  optimal cache coherence and CPU efficiency. Edge pixels (left and right
//  columns) are handled explicitly with boundary checks to eliminate
//  visible artifacts at screen borders.
// -----------------------------------------------------------------------------

static void V_PProc_DepthOfFieldBlur (void)
{
    // Validate input buffer and 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    if (width < 3 || height < 3)
        return;

    Uint32* pixels = (Uint32*)argbbuffer->pixels;
    const int cx = width / 2;
    const int cy = height / 2;

    // Fixed kernel parameters for 3x3 blur
    const int blur_size = 9;
    const int threshold = 150 * vid_resolution;
    const int thresholdSq = threshold * threshold;

    // Main blur pass: optimized 3x3 box blur using row pointers
    for (int y = 1; y < height - 1; ++y)
    {
        const int dy = y - cy;
        const int dy2 = dy * dy;

        Uint32* prevRow = pixels + (y - 1) * width;
        Uint32* curRow  = pixels + y * width;
        Uint32* nextRow = pixels + (y + 1) * width;

        for (int x = 1; x < width - 1; ++x)
        {
            const int dx = x - cx;
            if (dx * dx + dy2 < thresholdSq)
                continue; // Pixel is in focus – skip blur

            // Manually unrolled 3x3 accumulation for R, G, B
            const int r_sum = ((prevRow[x - 1] >> 16) & 0xFF) +
                              ((prevRow[x]     >> 16) & 0xFF) +
                              ((prevRow[x + 1] >> 16) & 0xFF) +
                              ((curRow[x - 1]  >> 16) & 0xFF) +
                              ((curRow[x]      >> 16) & 0xFF) +
                              ((curRow[x + 1]  >> 16) & 0xFF) +
                              ((nextRow[x - 1] >> 16) & 0xFF) +
                              ((nextRow[x]     >> 16) & 0xFF) +
                              ((nextRow[x + 1] >> 16) & 0xFF);

            const int g_sum = ((prevRow[x - 1] >> 8) & 0xFF) +
                              ((prevRow[x]     >> 8) & 0xFF) +
                              ((prevRow[x + 1] >> 8) & 0xFF) +
                              ((curRow[x - 1]  >> 8) & 0xFF) +
                              ((curRow[x]      >> 8) & 0xFF) +
                              ((curRow[x + 1]  >> 8) & 0xFF) +
                              ((nextRow[x - 1] >> 8) & 0xFF) +
                              ((nextRow[x]     >> 8) & 0xFF) +
                              ((nextRow[x + 1] >> 8) & 0xFF);

            const int b_sum = (prevRow[x - 1] & 0xFF) +
                              (prevRow[x]     & 0xFF) +
                              (prevRow[x + 1] & 0xFF) +
                              (curRow[x - 1]  & 0xFF) +
                              (curRow[x]      & 0xFF) +
                              (curRow[x + 1]  & 0xFF) +
                              (nextRow[x - 1] & 0xFF) +
                              (nextRow[x]     & 0xFF) +
                              (nextRow[x + 1] & 0xFF);

            // Apply averaged color back to pixel
            curRow[x] = (0xFF << 24) |
                        ((r_sum / blur_size) << 16) |
                        ((g_sum / blur_size) << 8) |
                         (b_sum / blur_size);
        }
    }

    // Border correction: handle left (x=0) and right (x=width-1) columns
    for (int y = 1; y < height - 1; ++y)
    {
        for (int x = 0; x < width; x += (width - 1))
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy < thresholdSq)
                continue;

            int r_sum = 0, g_sum = 0, b_sum = 0, count = 0;

            // Safe 3x3 sampling with boundary checks
            for (int ky = -1; ky <= 1; ++ky)
            {
                const int ny = y + ky;
                if (ny < 0 || ny >= height)
                    continue;

                Uint32* row = pixels + ny * width;
                for (int kx = -1; kx <= 1; ++kx)
                {
                    const int nx = x + kx;
                    if (nx < 0 || nx >= width)
                        continue;

                    Uint32 c = row[nx];
                    r_sum += (c >> 16) & 0xFF;
                    g_sum += (c >> 8)  & 0xFF;
                    b_sum += c & 0xFF;
                    ++count;
                }
            }

            if (count > 0)
            {
                pixels[y * width + x] = (0xFF << 24) |
                                        ((r_sum / count) << 16) |
                                        ((g_sum / count) << 8) |
                                         (b_sum / count);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// V_PProc_Display and V_PProc_PlayerView
//  [JN] Dilute functions that triggers general purpose post-processing effects 
//  (D_Display) and for player view only (R_RenderPlayerView).
// -----------------------------------------------------------------------------

boolean pproc_display_effects;
boolean pproc_plyrview_effects;

boolean V_PProc_EffectsActive (void)
{
    return (pproc_display_effects || pproc_plyrview_effects);
}

void V_PProc_Display (void)
{
    pproc_display_effects =
        post_overglow || post_rgbdrift || post_vhsdist;
    
    // Overbright Glow
    if (post_overglow)
        V_PProc_OverbrightGlow();

    // Analog RGB Drift
    if (post_rgbdrift)
        V_PProc_AnalogRGBDrift();

    // VHS Line Distortion
    if (post_vhsdist)
        V_PProc_VHSLineDistortion();
}

void V_PProc_PlayerView (void)
{
    pproc_plyrview_effects =
        post_motionblur || post_dofblur || post_vignette;

    // Motion Blur
    if (post_motionblur)
        V_PProc_MotionBlur();

    // Depth of Field Blur
    if (post_dofblur)
        V_PProc_DepthOfFieldBlur();

    // Screen Vignette
    if (post_vignette)
        V_PProc_ScreenVignette();
}
