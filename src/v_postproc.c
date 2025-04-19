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
// V_PProc_DepthOfFieldBlur
//  [PN] Applies a radial depth-of-field blur based on distance from screen
//  center. The blur radius dynamically scales with `vid_resolution` to ensure
//  visibility across resolutions. Central pixels remain sharp, while peripheral
//  areas are softened with a box blur of size 3x3 to 7x7.
// -----------------------------------------------------------------------------

static void V_PProc_DepthOfFieldBlur (void)
{
    // Validate input buffer and 32-bit pixel format
    if (!argbbuffer || argbbuffer->format->BytesPerPixel != 4)
        return;

    const int width  = argbbuffer->w;
    const int height = argbbuffer->h;
    if (width < 7 || height < 7)
        return;

    Uint32 *restrict pixels = (Uint32*)argbbuffer->pixels;
    const int cx = width / 2;
    const int cy = height / 2;
    const int stride = width;  // Precomputed row stride

    // Adaptive blur radius based on resolution
    const int radius = (vid_resolution <= 2) ? 1 :
                       (vid_resolution <= 4) ? 2 : 3;
    const int diameter = 2 * radius + 1;
    const int kernel_size = diameter * diameter;

    // Threshold for blur application
    const int threshold = 150 * vid_resolution;
    const int thresholdSq = threshold * threshold;

    // Main blur pass
    for (int y = radius; y < height - radius; ++y)
    {
        const int dy = y - cy;
        const int dy2 = dy * dy;
        Uint32 *restrict row_center = pixels + y * stride;  // Central row

        for (int x = radius; x < width - radius; ++x)
        {
            const int dx = x - cx;
            if (dx * dx + dy2 < thresholdSq)
                continue; // Pixel is in focus – skip

            int r_sum = 0, g_sum = 0, b_sum = 0;

            // Precomputed pointers to rows
            for (int ky = -radius; ky <= radius; ++ky)
            {
                const Uint32 *restrict row = pixels + (y + ky) * stride;
                const int x_start = x - radius;
                
                // Unrolled inner loop for radii 1-3
                if (radius == 1)
                {
                    Uint32 c0 = row[x_start];
                    Uint32 c1 = row[x_start + 1];
                    Uint32 c2 = row[x_start + 2];
                    
                    r_sum += ((c0 >> 16) & 0xFF) + ((c1 >> 16) & 0xFF) + ((c2 >> 16) & 0xFF);
                    g_sum += ((c0 >> 8) & 0xFF) + ((c1 >> 8) & 0xFF) + ((c2 >> 8) & 0xFF);
                    b_sum += (c0 & 0xFF) + (c1 & 0xFF) + (c2 & 0xFF);
                }
                else if (radius == 2)
                {
                    // Similarly for radius 2 (5x5)
                    for (int kx = -2; kx <= 2; ++kx)
                    {
                        Uint32 c = row[x + kx];
                        r_sum += (c >> 16) & 0xFF;
                        g_sum += (c >> 8) & 0xFF;
                        b_sum += c & 0xFF;
                    }
                }
                else // radius == 3
                {
                    // Similarly for radius 3 (7x7)
                    for (int kx = -3; kx <= 3; ++kx)
                    {
                        Uint32 c = row[x + kx];
                        r_sum += (c >> 16) & 0xFF;
                        g_sum += (c >> 8) & 0xFF;
                        b_sum += c & 0xFF;
                    }
                }
            }

            // Fast division using precomputed kernel_size
            const int r_avg = r_sum / kernel_size;
            const int g_avg = g_sum / kernel_size;
            const int b_avg = b_sum / kernel_size;

            row_center[x] = (0xFF << 24) | (r_avg << 16) | (g_avg << 8) | b_avg;
        }
    }

    // Optimized border processing
    for (int y = 1; y < height - 1; ++y)
    {
        const int dy = y - cy;
        const int dy2 = dy * dy;
        Uint32 *restrict row = pixels + y * stride;

        // Processing left and right borders
        for (int x = 0; x < width; x += (width - 1))
        {
            const int dx = x - cx;
            if (dx * dx + dy2 < thresholdSq)
                continue;

            int r_sum = 0, g_sum = 0, b_sum = 0;
            int count = 0;

            // Explicit handling of 3x3 kernel with boundary checks
            for (int ky = (y == 0) ? 0 : -1; ky <= (y == height - 1) ? 0 : 1; ++ky)
            {
                const Uint32 *restrict sample_row = pixels + (y + ky) * stride;
                const int x_start = (x == 0) ? 0 : -1;
                const int x_end = (x == width - 1) ? 0 : 1;
                
                for (int kx = x_start; kx <= x_end; ++kx)
                {
                    Uint32 c = sample_row[x + kx];
                    r_sum += (c >> 16) & 0xFF;
                    g_sum += (c >> 8) & 0xFF;
                    b_sum += c & 0xFF;
                    ++count;
                }
            }

            if (count > 0)
            {
                row[x] = (0xFF << 24) | 
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
