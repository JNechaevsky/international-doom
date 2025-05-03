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
//  Video post processing effects for software renderer.
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

void V_PProc_SupersampledSmoothing (boolean st_background_on, int st_height)
{
    // [PN] Ensure framebuffer is valid and in 32-bit mode
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int w = argbbuffer->w;
    // [JN] Exclude status bar area from smoothing if active.
    const int h = argbbuffer->h - (st_background_on ? st_height : 0);
    Uint32 *restrict pixels = (Uint32*)argbbuffer->pixels;
    const int block = post_supersample + 1;

    // [PN] Iterate through the image in blocks
    for (int by = 0; by < h; by += block)
    {
        for (int bx = 0; bx < w; bx += block)
        {
            int r = 0, g = 0, b = 0, count = 0;

            // [PN] Loop through each pixel in the block to calculate average color
            for (int y = by; y < by + block && y < h; ++y)
            {
                for (int x = bx; x < bx + block && x < w; ++x)
                {
                    const Uint32 c = pixels[y * w + x];
                    r += (c >> 16) & 0xFF;
                    g += (c >> 8) & 0xFF;
                    b += c & 0xFF;
                    ++count;
                }
            }

            // [PN] Avoid division by zero; if no pixels, skip processing
            if (count == 0)
                continue;

            // [PN] Calculate the average color for the block
            const Uint32 avg = (0xFF << 24) | ((r / count) << 16) | ((g / count) << 8) | (b / count);

            // [PN] Apply the averaged color back to all pixels in the block
            for (int y = by; y < by + block && y < h; ++y)
                for (int x = bx; x < bx + block && x < w; ++x)
                    pixels[y * w + x] = avg;
        }
    }
}

// -----------------------------------------------------------------------------
// V_PProc_OverbrightGlow
//  [PN] Applies a soft, color-tinted glow to the frame based on the dominant
//  hue of bright areas. Glow intensity and color adapt over time using
//  exponential smoothing.
// -----------------------------------------------------------------------------

static void V_PProc_OverbrightGlow (void)
{
    // [PN] Validate input buffer and 32-bit format.
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int w = argbbuffer->w;
    const int h = argbbuffer->h;
    Uint32 *restrict pixels = (Uint32 *)argbbuffer->pixels;

    static int glow_r = 0, glow_g = 0, glow_b = 0;

    int bright_count = 0;
    int bright_r = 0, bright_g = 0, bright_b = 0;

    const int rate = 13; // how quickly we adapt (~0.05 in Q8.8)

    // [PN] Single loop to calculate brightness and apply glow simultaneously
    for (Uint32 *restrict p = pixels, *end = pixels + (w * h); p < end; ++p)
    {
        Uint32 c = *p;
        int r = (c >> 16) & 0xFF;
        int g = (c >> 8) & 0xFF;
        int b = c & 0xFF;

        // [PN] Accumulate values for bright pixel count and average color
        bright_r += r;
        bright_g += g;
        bright_b += b;
        bright_count++;

        // [PN] Apply glow effect dynamically during the same loop
        int glow_r_adj = ((256 + glow_r) * r) >> 8;
        int glow_g_adj = ((256 + glow_g) * g) >> 8;
        int glow_b_adj = ((256 + glow_b) * b) >> 8;

        // [PN] Branchless clamping (ensuring values don't exceed 255)
        glow_r_adj = glow_r_adj > 255 ? 255 : glow_r_adj;
        glow_g_adj = glow_g_adj > 255 ? 255 : glow_g_adj;
        glow_b_adj = glow_b_adj > 255 ? 255 : glow_b_adj;

        *p = (0xFF << 24) | (glow_r_adj << 16) | (glow_g_adj << 8) | glow_b_adj;
    }

    // [PN] Adapt exposure based on the average brightness of bright pixels
    if (bright_count > 0)
    {
        glow_r = ((glow_r * (rate - 1)) + (bright_r / bright_count)) / rate;
        glow_g = ((glow_g * (rate - 1)) + (bright_g / bright_count)) / rate;
        glow_b = ((glow_b * (rate - 1)) + (bright_b / bright_count)) / rate;
    }
}

// -----------------------------------------------------------------------------
// V_PProc_BloomGlow
//  [PN] Applies an optimized bloom effect with adaptive blur size.
//  Fixed 4x4 downscale, separable blur, resolution-aware bloom spread.
// -----------------------------------------------------------------------------

static void V_PProc_BloomGlow(void)
{
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int w = argbbuffer->w;
    const int h = argbbuffer->h;
    const int sw = w >> 2; // [PN] downscale factor fixed at 4
    const int sh = h >> 2;
    const int stride = sw;
    const size_t needed_size = (size_t)(sw * sh) * sizeof(Uint32);

    static Uint32 *bloom_buf = NULL;
    static Uint32 *blur_buf = NULL;
    static size_t bloom_buf_size = 0;
    static size_t blur_buf_size = 0;

    // [PN] Reallocate bloom buffer if needed
    if (bloom_buf_size != needed_size)
    {
        free(bloom_buf);
        bloom_buf = (Uint32 *)malloc(needed_size);
        if (!bloom_buf)
            return;
        memset(bloom_buf, 0, needed_size);
        bloom_buf_size = needed_size;
    }

    // [PN] Reallocate blur buffer if needed
    if (blur_buf_size != needed_size)
    {
        free(blur_buf);
        blur_buf = (Uint32 *)malloc(needed_size);
        if (!blur_buf)
            return;
        memset(blur_buf, 0, needed_size);
        blur_buf_size = needed_size;
    }

    Uint32 *restrict src = (Uint32*)argbbuffer->pixels;
    Uint32 *restrict bloom = bloom_buf;
    Uint32 *restrict blur = blur_buf;

    // --- Threshold extraction and downsample ---
    for (int by = 0; by < sh; ++by)
    {
        const int y0 = by * 4;
        const int y_max = (y0 + 4 < h) ? 4 : h - y0;
        for (int bx = 0; bx < sw; ++bx)
        {
            const int x0 = bx * 4;
            const int x_max = (x0 + 4 < w) ? 4 : w - x0;
            int sum_r = 0, sum_g = 0, sum_b = 0, count = 0;

            for (int dy = 0; dy < y_max; ++dy)
            {
                const int y = y0 + dy;
                const int row = y * w;
                for (int dx = 0; dx < x_max; ++dx)
                {
                    const int x = x0 + dx;
                    const Uint32 c = src[row + x];
                    const int r = (c >> 16) & 0xFF;
                    const int g = (c >> 8) & 0xFF;
                    const int b = c & 0xFF;
                    const int luma = (r * 3 + g * 6 + b) >> 3; // [PN] fast luma estimation
                    if (luma > 200)
                    {
                        sum_r += r;
                        sum_g += g;
                        sum_b += b;
                        ++count;
                    }
                }
            }
            if (count)
                bloom[by * stride + bx] = (0xFF << 24)
                                        | ((sum_r / count) << 16)
                                        | ((sum_g / count) << 8)
                                        |  (sum_b / count);
            else
                bloom[by * stride + bx] = 0;
        }
    }

    // --- Separable blur pass ---

    // [PN] Determine blur radius based on resolution
    const int blur_radius = (vid_resolution >= 3) ? 2 : 1; // 1 -> 3x3, 2 -> 5x5 blur

    // [JN] Precomputed reciprocals for Q16 fixed-point.
    #define RECIP3  21845  // (1/3) * 65536
    #define RECIP5  13107  // (1/5) * 65536
    const int count = 2 * blur_radius + 1;
    const int recip = (count == 3) ? RECIP3 : RECIP5;

    // [PN] Horizontal blur
    for (int y = 0; y < sh; ++y)
    {
        const int row = y * stride;
        int r_sum = 0, g_sum = 0, b_sum = 0;

        // [JN] Initialize sum for the first window.
        for (int kx = -blur_radius; kx <= blur_radius; ++kx)
        {
            const Uint32 c = bloom[row + (blur_radius + kx)];
            r_sum += (c >> 16) & 0xFF;
            g_sum += (c >> 8) & 0xFF;
            b_sum += c & 0xFF;
        }
        blur[row + blur_radius] = (0xFF << 24)
                                | (((r_sum * recip) >> 16) << 16)
                                | (((g_sum * recip) >> 16) << 8)
                                |  ((b_sum * recip) >> 16);

        // [JN] Sliding window for the remaining x.
        for (int x = blur_radius + 1; x < sw - blur_radius; ++x)
        {
            // [JN] Subtract the left pixel, add the right pixel.
            const Uint32 c_out = bloom[row + (x - blur_radius - 1)];
            const Uint32 c_in  = bloom[row + (x + blur_radius)];
            r_sum += ((c_in >> 16) & 0xFF) - ((c_out >> 16) & 0xFF);
            g_sum += ((c_in >> 8) & 0xFF) - ((c_out >> 8) & 0xFF);
            b_sum += (c_in & 0xFF) - (c_out & 0xFF);

            blur[row + x] = (0xFF << 24)
                          | (((r_sum * recip) >> 16) << 16)
                          | (((g_sum * recip) >> 16) << 8)
                          |  ((b_sum * recip) >> 16);
        }
    }

    // [PN] Vertical blur
    for (int x = 0; x < sw; ++x)
    {
        int r_sum = 0, g_sum = 0, b_sum = 0;

        // [JN] Initialize sum for the first window along y.
        for (int ky = -blur_radius; ky <= blur_radius; ++ky)
        {
            const int krow = (blur_radius + ky) * stride;
            const Uint32 c = blur[krow + x];
            r_sum += (c >> 16) & 0xFF;
            g_sum += (c >> 8) & 0xFF;
            b_sum += c & 0xFF;
        }
        bloom[blur_radius * stride + x] = (0xFF << 24)
                                        | (((r_sum * recip) >> 16) << 16)
                                        | (((g_sum * recip) >> 16) << 8)
                                        |  ((b_sum * recip) >> 16);

        // [JN] Sliding window along y.
        for (int y = blur_radius + 1; y < sh - blur_radius; ++y)
        {
            const int row_out = (y - blur_radius - 1) * stride;
            const int row_in  = (y + blur_radius) * stride;
            const Uint32 c_out = blur[row_out + x];
            const Uint32 c_in  = blur[row_in + x];
            r_sum += ((c_in >> 16) & 0xFF) - ((c_out >> 16) & 0xFF);
            g_sum += ((c_in >> 8) & 0xFF) - ((c_out >> 8) & 0xFF);
            b_sum += (c_in & 0xFF) - (c_out & 0xFF);

            bloom[y * stride + x] = (0xFF << 24)
                                  | (((r_sum * recip) >> 16) << 16)
                                  | (((g_sum * recip) >> 16) << 8)
                                  |  ((b_sum * recip) >> 16);
        }
    }

    // --- Upscale and blend ---

    // [JN] Calculate blending boost depending on rendering resolution.
    static const int boost_factor[] = { 0, 1, 1, 2, 2, 2, 2 };
    const int boost = boost_factor[vid_resolution];

    for (int by = 0; by < sh; ++by)
    {
        const int y0 = by * 4;
        const int y_max = (y0 + 4 < h) ? 4 : h - y0;
        for (int bx = 0; bx < sw; ++bx)
        {
            const Uint32 bloom_px = bloom[by * stride + bx];
            const int r_b = (bloom_px >> 16) & 0xFF;
            const int g_b = (bloom_px >> 8) & 0xFF;
            const int b_b = bloom_px & 0xFF;

            if ((r_b | g_b | b_b) == 0)
                continue;

            const int x0 = bx * 4;
            const int x_max = (x0 + 4 < w) ? 4 : w - x0;
    
            for (int dy = 0; dy < y_max; ++dy)
            {
                const int y = y0 + dy;
                const int row = y * w;
                for (int dx = 0; dx < x_max; ++dx)
                {
                    const int x = x0 + dx;
                    Uint32 *restrict p = &src[row + x];
                    const Uint32 base = *p;
                    const int r = (base >> 16) & 0xFF;
                    const int g = (base >> 8) & 0xFF;
                    const int b = base & 0xFF;
                    int r_blend = r + ((r_b * boost) >> 2); if (r_blend > 255) r_blend = 255;
                    int g_blend = g + ((g_b * boost) >> 2); if (g_blend > 255) g_blend = 255;
                    int b_blend = b + ((b_b * boost) >> 2); if (b_blend > 255) b_blend = 255;
                    *p = (0xFF << 24) | (r_blend << 16) | (g_blend << 8) | b_blend;
                }
            }
        }
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
    // [PN] Check if the framebuffer is valid.
    if (!argbbuffer)
        return;

    const int width = SCREENWIDTH;
    const int height = SCREENHEIGHT;
    const size_t total_pixels = SCREENAREA;
    const size_t needed_size = total_pixels * sizeof(pixel_t);

    // [PN] Static buffer to store the original frame for safe sampling during modification
    static pixel_t *restrict chromabuf = NULL;
    static size_t chromabuf_size = 0;

    // [PN] Reallocate buffer if resolution has changed
    if (chromabuf_size != needed_size)
    {
        free(chromabuf);
        chromabuf = malloc(needed_size);
        if (!chromabuf)
            return;
        chromabuf_size = needed_size;
    }

    // [PN] Copy the current frame into the temporary buffer
    pixel_t *restrict const src = (pixel_t*)argbbuffer->pixels;
    memcpy(chromabuf, src, needed_size);

    // [JN] Calculate the RGB drift offset depending on resolution.
    const int dx = post_rgbdrift + ((vid_resolution > 2) ? (vid_resolution - 2) : 0);

    // [PN] Precompute shifted column indices for red (left) and blue (right) channels
    static int *restrict x_src_r = NULL;
    static int *restrict x_src_b = NULL;
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
            free(x_src_r); x_src_r = NULL;
            free(x_src_b); x_src_b = NULL;
            return;
        }

        // [PN] Precompute the column indices for the red and blue channel shifts
        for (int x = 0; x < width; ++x)
        {
            x_src_r[x] = (x - dx < 0) ? 0 : x - dx; // Red shifts left
            x_src_b[x] = (x + dx >= width) ? width - 1 : x + dx; // Blue shifts right
        }

        allocated_width = width;
        last_dx = dx;
    }

    // [PN] Loop through each row of pixels
    for (int y = 0; y < height; ++y)
    {
        pixel_t *restrict const dst = src + y * width;
        const pixel_t *restrict const row = chromabuf + y * width;

        // [PN] Process each pixel in the row
        for (int x = 0; x < width; ++x)
        {
            // [PN] Fetch original pixel and shifted red/blue samples
            const pixel_t orig = row[x];
            const pixel_t rsrc = row[x_src_r[x]]; // Shifted red
            const pixel_t bsrc = row[x_src_b[x]]; // Shifted blue

            // [PN] Extract RGB components and apply the shift
            const int r = (rsrc >> 16) & 0xFF;
            const int g = (orig >> 8) & 0xFF; // Keep original green
            const int b = bsrc & 0xFF;

            // [PN] Compose the final pixel with altered red/blue and original green
            dst[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
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
    // [PN] Check if argbbuffer exists and is in 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    // [PN] Dimensions and row stride
    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    const int stride = width;

    // [PN] Framebuffer pointer; restrict allows better compiler optimization
    Uint32 *restrict pixels = (Uint32 *restrict)argbbuffer->pixels;

    // [PN] Block height: 4–6 lines scaled by resolution
    const int block_height = (4 + rand() % 3) * vid_resolution;

    // [PN] Start line: random, but ensures block fits within frame
    const int y_start = rand() % (height > block_height ? height - block_height : 1);

    // [PN] Horizontal shift: range [-5..+5]
    const int shift_val = (rand() % 11) - 5;
    if (shift_val == 0)
        return;

    const int abs_shift = (shift_val > 0) ? shift_val : -shift_val;
    const Uint32 black_pixel = 0xFF000000;

    // [PN] Apply line distortion per row within the selected block
    for (int y = y_start; y < y_start + block_height; ++y)
    {
        Uint32 *restrict row = pixels + y * stride;

        if (shift_val > 0)
        {
            // [PN] Right shift — copy pixels rightward
            for (int x = width - 1; x >= abs_shift; --x)
                row[x] = row[x - abs_shift];

            // [PN] Fill left edge with black
            for (int x = 0; x < abs_shift; ++x)
                row[x] = black_pixel;
        }
        else
        {
            // [PN] Left shift — copy pixels leftward
            for (int x = abs_shift; x < width; ++x)
                row[x - abs_shift] = row[x];

            // [PN] Fill right edge with black
            for (int x = width - abs_shift; x < width; ++x)
                row[x] = black_pixel;
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
    // [PN] Validate input buffer and 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int  w  = argbbuffer->w;
    const int  h  = argbbuffer->h;
    Uint32 *restrict const fb = (Uint32 *)argbbuffer->pixels;

    // [PN] Geometry & pre‑computed constants
    const int cx = w >> 1;                 // centre‑x
    const int cy = h >> 1;                 // centre‑y
    const int max_dist2 = cx * cx + cy * cy;

    // [PN] 0 = off … 4 = strongest
    static const int att_tbl[] = { 0, 80, 146, 200, 255 };
    const int att_max = att_tbl[post_vignette];
    if (att_max == 0)                      // early‑out if vignette disabled
        return;

    // [PN] Main loop — per scan‑line
    for (int y = 0; y < h; ++y)
    {
        const int dy   = y - cy;
        const int dy2  = dy * dy;
        Uint32 *restrict row = fb + (size_t)y * w;

        /* Incremental x² logic:
           dist2  = (‑cx)² + dy²  at x = 0
           inc    = 2·x + 1       derivative of x²
           After each pixel:
             dist2 += inc
             inc   += 2
         */
        int x      = 0;
        int dx     = -cx;
        int dist2  = dx * dx + dy2;
        int inc    = (dx << 1) + 1;

        for (; x < w; ++x)
        {
            // [PN] Attenuation (Q8.8): 0..255
            const int atten  = (dist2 >= max_dist2)
                               ? att_max
                               : (dist2 * att_max) / max_dist2;
            const int scale  = 256 - atten;          // 1.0 – attenuation

            // [PN] Apply scale
            const Uint32 px = row[x];
            const int r = (((px >> 16) & 0xFF) * scale) >> 8;
            const int g = (((px >>  8) & 0xFF) * scale) >> 8;
            const int b = (( px        & 0xFF) * scale) >> 8;

            row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;

            // [PN] Incremental x² update
            dist2 += inc;
            inc   += 2;
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
    // [PN] Validate framebuffer & format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int  w   = argbbuffer->w;
    const int  h   = argbbuffer->h;
    Uint32 *restrict const fb = (Uint32 *)argbbuffer->pixels;
    const size_t pix_cnt = (size_t)w * h;
    const size_t buf_sz  = pix_cnt * sizeof(Uint32);

    // [PN] Q8.8 weight table (≈curr/10 * 256, prev/10 * 256) ↴
    static const uint16_t Wtbl[5][2] = {
        {205,  51}, // Soft  ( 0.8, 0.2 )
        {179,  77}, // Light ( 0.7, 0.3 )
        {154, 102}, // Med   ( 0.6, 0.4 )
        {128, 128}, // Heavy ( 0.5, 0.5 )
        { 26, 230}  // Ghost ( 0.1, 0.9 )
    };

    const uint16_t w_cur  = Wtbl[post_motionblur - 1][0];
    const uint16_t w_prev = Wtbl[post_motionblur - 1][1];

    // [PN] Buffer management
    static Uint32 *prev_frame = NULL;              // single‑buffer mode
    static size_t prev_size   = 0;

    static Uint32 *ring[MAX_BLUR_LAG + 1] = {0};   // uncapped‑FPS ring
    static size_t ring_size = 0;
    static int    ring_idx  = 0;

    const int uncapped = vid_uncapped_fps;

    if (uncapped)
    {
        if (ring_size != buf_sz)
        {
            for (int i = 0; i <= MAX_BLUR_LAG; ++i)
            {
                Uint32 *tmp = realloc(ring[i], buf_sz);
                if (!tmp)
                {
                    free(ring[i]);
                    ring[i] = NULL;
                }
                else
                {
                    ring[i] = tmp;
                    memset(tmp, 0, buf_sz);
                }
            }
            ring_size = buf_sz;
        }
    }
    else if (prev_size != buf_sz)
    {
        Uint32 *tmp = realloc(prev_frame, buf_sz);
        if (!tmp)
        {
            free(prev_frame);
            prev_frame = NULL;
        }
        else
        {
            prev_frame = tmp;
            memset(tmp, 0, buf_sz);
        }
        prev_size = buf_sz;
    }

    // [PN] Choose previous‑frame source
    Uint32 *restrict const oldF = uncapped
        ? ring[(ring_idx + MAX_BLUR_LAG) & MAX_BLUR_LAG]
        : prev_frame;

    if (!oldF) return;   // nothing to blend with yet

    // [PN] Blend loop
    for (size_t i = 0; i < pix_cnt; ++i)
    {
        const Uint32 c = fb[i];
        const Uint32 o = oldF[i];

        const int r = (((c >> 16 & 0xFF) * w_cur)  + ((o >> 16 & 0xFF) * w_prev)) >> 8;
        const int g = (((c >>  8 & 0xFF) * w_cur)  + ((o >>  8 & 0xFF) * w_prev)) >> 8;
        const int b = (((c       & 0xFF) * w_cur)  + ((o       & 0xFF) * w_prev)) >> 8;

        fb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    // [PN] Save current frame
    if (uncapped)
    {
        ring_idx = (ring_idx + 1) & MAX_BLUR_LAG;  // mod power‑of‑two
        memcpy(ring[ring_idx], fb, buf_sz);
    }
    else
    {
        memcpy(prev_frame, fb, buf_sz);
    }
}

// -----------------------------------------------------------------------------
// V_PProc_FilmGrain
//  [PN] Adds a pseudo-random film grain effect to the screen. Grain pattern is
//  updated once per game tick using a fast integer-based formula and stored in
//  a reusable 8-bit buffer.
// -----------------------------------------------------------------------------

static void V_PProc_FilmGrain (void)
{
    // [PN] Early out if buffer is invalid or pixel format isn't 32-bit RGBA
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    static uint8_t *restrict grain_noise_map = NULL; // [PN] Persistent 8-bit noise buffer
    static size_t grain_noise_map_size = 0;          // [PN] Ensures reallocation on res change
    static int last_gametic_updated = -1;            // [PN] Only recompute noise once per tic

    const int w = argbbuffer->w;
    const int h = argbbuffer->h;
    const size_t npix = (size_t)w * h;

    // [PN] Allocate or resize noise map to match resolution
    if (grain_noise_map_size != npix)
    {
        free(grain_noise_map);
        grain_noise_map = (uint8_t *)malloc(npix);
        if (!grain_noise_map)
        {
            grain_noise_map_size = 0;
            return;
        }
        grain_noise_map_size = npix;
        last_gametic_updated = -1; // [PN] Force full refresh
    }

    extern int gametic;
    if (gametic != last_gametic_updated)
    {
        const unsigned int seed = rand();      // [PN] Per-frame noise basis
        const int amp = post_filmgrain * 2;    // [PN] Noise amplitude range: [-amp..+amp]
        const int mix_seed = (rand() % 8) + 1; // [JN] Randomized mixed seed for using below.

        for (size_t i = 0; i < npix; ++i)
        {
            // [JN] Fast low-cost pixel shuffler — XORs a mixed index with seed,
            // then distorts bits via variable right-shift.
            // Produces grain-like chaos with no patterns or banding.
            // No multiplications, no rand() per pixel.
            unsigned int mix = seed ^ (i + (i >> 7) + (i << 3));
            mix ^= (mix >> mix_seed);
            
            const int offset = (int)(mix % (amp * 2 + 1)) - amp;
            grain_noise_map[i] = (uint8_t)(offset + 128);
        }

        last_gametic_updated = gametic;
    }

    Uint32 *restrict const pixels = (Uint32 *restrict)argbbuffer->pixels;
    const uint8_t *restrict const noise = grain_noise_map;

    // [PN] Apply per-pixel noise offset to RGB channels
    for (size_t i = 0; i < npix; ++i)
    {
        const int offset = (int)noise[i] - 128;

        const Uint32 px = pixels[i];
        int r = ((px >> 16) & 0xFF) + offset;
        int g = ((px >>  8) & 0xFF) + offset;
        int b = ( px        & 0xFF) + offset;

        // [PN] Clamp RGB values to 0..255
        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;

        // [PN] Store final pixel with original alpha
        pixels[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
}

// -----------------------------------------------------------------------------
// V_PProc_DepthOfFieldBlur
//  [PN] Applies a radial depth-of-field blur based on distance from screen
//  center. The blur radius dynamically scales with `vid_resolution` to ensure
//  visibility across resolutions. Central pixels remain sharp, while peripheral
//  areas are softened with a box blur of size 3x3 to 7x7.
// -----------------------------------------------------------------------------

static void V_PProc_DepthOfFieldBlur (void)
{
    // [PN] Validate input buffer and 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width       = argbbuffer->w;
    const int height      = argbbuffer->h;
    if (width < 7 || height < 7)
        return;

    Uint32 *restrict pixels = (Uint32 *restrict)argbbuffer->pixels;
    const int stride       = width;
    const int cx           = width >> 1;
    const int cy           = height >> 1;
    const int resolution   = vid_resolution;
    const int radius       = (resolution <= 2) ? 1 : (resolution <= 4) ? 2 : 3;
    const int thresh       = 150 * resolution;
    const int threshSq     = thresh * thresh;

    // [PN] Central blur pass with radius-specific unrolling
    switch (radius)
    {
        case 1:
        {
            const int ksz = 9;
            for (int y = 1; y < height - 1; ++y)
            {
                const int dy    = y - cy;
                const int dy2   = dy * dy;
                Uint32 *restrict dst  = pixels + y * stride;
                const Uint32 *restrict prev = dst - stride;
                const Uint32 *restrict next = dst + stride;
                for (int x = 1; x < width - 1; ++x)
                {
                    const int dx = x - cx;
                    if (dx * dx + dy2 >= threshSq)
                    {
                        Uint32 c0 = prev[x - 1], c1 = prev[x], c2 = prev[x + 1];
                        Uint32 c3 = dst[x - 1],               c5 = dst[x + 1];
                        Uint32 c6 = next[x - 1], c7 = next[x], c8 = next[x + 1];
                        int r_sum = ((c0 >> 16) & 0xFF) + ((c1 >> 16) & 0xFF) + ((c2 >> 16) & 0xFF)
                                  + ((c3 >> 16) & 0xFF) + ((dst[x] >> 16) & 0xFF) + ((c5 >> 16) & 0xFF)
                                  + ((c6 >> 16) & 0xFF) + ((c7 >> 16) & 0xFF) + ((c8 >> 16) & 0xFF);
                        int g_sum = ((c0 >> 8)  & 0xFF) + ((c1 >> 8)  & 0xFF) + ((c2 >> 8)  & 0xFF)
                                  + ((c3 >> 8)  & 0xFF) + ((dst[x] >> 8)  & 0xFF) + ((c5 >> 8)  & 0xFF)
                                  + ((c6 >> 8)  & 0xFF) + ((c7 >> 8)  & 0xFF) + ((c8 >> 8)  & 0xFF);
                        int b_sum = (c0 & 0xFF) + (c1 & 0xFF) + (c2 & 0xFF)
                                  + (c3 & 0xFF) + (dst[x] & 0xFF) + (c5 & 0xFF)
                                  + (c6 & 0xFF) + (c7 & 0xFF) + (c8 & 0xFF);
                        dst[x] = (0xFFu << 24) | ((r_sum / ksz) << 16) | ((g_sum / ksz) << 8) | (b_sum / ksz);
                    }
                }
            }
        }
        break;

        case 2:
        {
            const int ksz = 25;
            for (int y = 2; y < height - 2; ++y)
            {
                const int dy  = y - cy;
                const int dy2 = dy * dy;
                Uint32 *restrict dst = pixels + y * stride;
                for (int x = 2; x < width - 2; ++x)
                {
                    const int dx = x - cx;
                    if (dx * dx + dy2 < threshSq) continue;
                    int r_sum = 0, g_sum = 0, b_sum = 0;
                    const Uint32 *restrict rows[5] = {
                        dst - 2 * stride,
                        dst -     stride,
                        dst,
                        dst +     stride,
                        dst + 2 * stride
                    };
                    for (int ry = 0; ry < 5; ++ry) {
                        const Uint32 *restrict row = rows[ry];
                        r_sum += (row[x - 2] >> 16) & 0xFF;
                        g_sum += (row[x - 2] >>  8) & 0xFF;
                        b_sum +=  row[x - 2]        & 0xFF;
                        r_sum += (row[x - 1] >> 16) & 0xFF;
                        g_sum += (row[x - 1] >>  8) & 0xFF;
                        b_sum +=  row[x - 1]        & 0xFF;
                        r_sum += (row[x    ] >> 16) & 0xFF;
                        g_sum += (row[x    ] >>  8) & 0xFF;
                        b_sum +=  row[x    ]        & 0xFF;
                        r_sum += (row[x + 1] >> 16) & 0xFF;
                        g_sum += (row[x + 1] >>  8) & 0xFF;
                        b_sum +=  row[x + 1]        & 0xFF;
                        r_sum += (row[x + 2] >> 16) & 0xFF;
                        g_sum += (row[x + 2] >>  8) & 0xFF;
                        b_sum +=  row[x + 2]        & 0xFF;
                    }
                    dst[x] = (0xFFu << 24) | ((r_sum / ksz) << 16) | ((g_sum / ksz) << 8) | (b_sum / ksz);
                }
            }
        }
        break;

        default:
        {   // radius == 3
            const int ksz = 49;
            for (int y = 3; y < height - 3; ++y)
            {
                const int dy  = y - cy;
                const int dy2 = dy * dy;
                Uint32 *restrict dst = pixels + y * stride;
                for (int x = 3; x < width - 3; ++x)
                {
                    const int dx = x - cx;
                    if (dx * dx + dy2 < threshSq) continue;
                    int r_sum = 0, g_sum = 0, b_sum = 0;
                    const Uint32 *restrict rows[7] = {
                        dst - 3 * stride,
                        dst - 2 * stride,
                        dst -     stride,
                        dst,
                        dst +     stride,
                        dst + 2 * stride,
                        dst + 3 * stride
                    };
                    for (int ry = 0; ry < 7; ++ry)
                    {
                        const Uint32 *restrict row = rows[ry];
                            for (int kx = -3; kx <= 3; ++kx) {
                            Uint32 c = row[x + kx];
                            r_sum += (c >> 16) & 0xFF;
                            g_sum += (c >>  8) & 0xFF;
                            b_sum +=  c        & 0xFF;
                        }
                    }
                    dst[x] = (0xFFu << 24) | ((r_sum / ksz) << 16) | ((g_sum / ksz) << 8) | (b_sum / ksz);
                }
            }
        }
        break;
    }

    // [PN] Border blur (3x3) for left/right edges only
    for (int y = 1; y < height - 1; ++y)
    {
        const int dy  = y - cy;
        const int dy2 = dy * dy;
        Uint32 *restrict dst = pixels + y * stride;
        for (int x = 0; x < width; x += (width - 1))
        {
            const int dx = x - cx;
            if (dx * dx + dy2 < threshSq) continue;
            int r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            for (int ky = -1; ky <= 1; ++ky)
            {
                const Uint32 *restrict row = pixels + (y + ky) * stride;
                for (int kx = -1; kx <= 1; ++kx)
                {
                    int xx = x + kx;
                    if ((unsigned)xx >= (unsigned)width) continue;
                    Uint32 c = row[xx];
                    r_sum += (c >> 16) & 0xFF;
                    g_sum += (c >>  8) & 0xFF;
                    b_sum +=  c        & 0xFF;
                    ++count;
                }
            }
            if (count) dst[x] = (0xFFu << 24) | ((r_sum / count) << 16) | ((g_sum / count) << 8) | (b_sum / count);
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

void V_PProc_Display (boolean supress)
{
    pproc_display_effects =
        post_overglow || post_rgbdrift || post_vhsdist;
    
    // Overbright Glow
    if (post_overglow && !supress)
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
        post_bloom || post_filmgrain || post_motionblur || post_dofblur || post_vignette;

    // Soft bloom
    if (post_bloom)
        V_PProc_BloomGlow();

    // Film Grain
    if (post_filmgrain)
        V_PProc_FilmGrain();

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
