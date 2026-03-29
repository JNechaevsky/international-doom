//
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

#include "v_png.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "i_swap.h"
#include "m_array.h"
#include "v_trans.h"
#include "w_wad.h"
#include "z_zone.h"

#include "spng.h"

#define NO_COLOR_KEY (-1)
#define PNG_MEM_LIMIT (1024 * 1024 * 64)

typedef struct
{
    byte row_off;
    byte *pixels;
} vpost_t;

typedef struct
{
    vpost_t *posts;
} vcolumn_t;

typedef struct
{
    int value;
    int pixel_count;
} color_slot_t;

typedef struct
{
    color_slot_t red_slots[8];
    color_slot_t green_slots[8];
    color_slot_t blue_slots[4];
    byte palette[768];
} uniform_quantizer_t;

typedef struct
{
    spng_ctx *ctx;
    byte *image;
    byte *translate;
    size_t image_size;
    int width;
    int height;
    int color_key;
} png_decode_t;

#define PUTBYTE(r, v) \
    do { *(r) = (byte) (v); ++(r); } while (0)

#define PUTSHORT(r, v)                                     \
    do {                                                   \
        *(r) = (byte) (((unsigned int) (v) >> 0) & 0xff); \
        ++(r);                                             \
        *(r) = (byte) (((unsigned int) (v) >> 8) & 0xff); \
        ++(r);                                             \
    } while (0)

#define PUTLONG(r, v)                                      \
    do {                                                   \
        *(r) = (byte) (((unsigned int) (v) >> 0) & 0xff); \
        ++(r);                                             \
        *(r) = (byte) (((unsigned int) (v) >> 8) & 0xff); \
        ++(r);                                             \
        *(r) = (byte) (((unsigned int) (v) >> 16) & 0xff);\
        ++(r);                                             \
        *(r) = (byte) (((unsigned int) (v) >> 24) & 0xff);\
        ++(r);                                             \
    } while (0)

static inline short V_ClampShort(int value)
{
    if (value < SHRT_MIN)
    {
        return SHRT_MIN;
    }

    if (value > SHRT_MAX)
    {
        return SHRT_MAX;
    }

    return (short) value;
}

static inline int V_ReadBE32Signed(const byte *src)
{
    const uint32_t raw = ((uint32_t) src[0] << 24)
                       | ((uint32_t) src[1] << 16)
                       | ((uint32_t) src[2] << 8)
                       | (uint32_t) src[3];
    return (int) (int32_t) raw;
}

static void AddValue(color_slot_t *slot, int component)
{
    slot->value += component;
    slot->pixel_count++;
}

static int GetAverage(color_slot_t *slot)
{
    if (slot->pixel_count > 0)
    {
        return slot->value / slot->pixel_count;
    }

    return 0;
}

static void AddColor(uniform_quantizer_t *q, int r, int g, int b)
{
    AddValue(&q->red_slots[r >> 5], r);
    AddValue(&q->green_slots[g >> 5], g);
    AddValue(&q->blue_slots[b >> 6], b);
}

static void BuildQuantizedPalette(uniform_quantizer_t *q)
{
    byte *dst = q->palette;

    for (size_t rs = 0; rs < ARRAY_LEN(q->red_slots); ++rs)
    {
        for (size_t gs = 0; gs < ARRAY_LEN(q->green_slots); ++gs)
        {
            for (size_t bs = 0; bs < ARRAY_LEN(q->blue_slots); ++bs)
            {
                *dst++ = (byte) GetAverage(&q->red_slots[rs]);
                *dst++ = (byte) GetAverage(&q->green_slots[gs]);
                *dst++ = (byte) GetAverage(&q->blue_slots[bs]);
            }
        }
    }
}

static inline int QuantizedPaletteIndex(int r, int g, int b)
{
    return ((r >> 5) << 5) + ((g >> 5) << 2) + (b >> 6);
}

static boolean InitPNGDecoder(png_decode_t *png, const void *buffer, int size)
{
    int ret;

    png->ctx = spng_ctx_new(0);

    if (png->ctx == NULL)
    {
        return false;
    }

    ret = spng_set_crc_action(png->ctx, SPNG_CRC_USE, SPNG_CRC_USE);
    if (ret)
    {
        return false;
    }

    ret = spng_set_chunk_limits(png->ctx, PNG_MEM_LIMIT, PNG_MEM_LIMIT);
    if (ret)
    {
        return false;
    }

    ret = spng_set_png_buffer(png->ctx, buffer, (size_t) size);
    return ret == 0;
}

static void FreePNGDecoder(png_decode_t *png)
{
    if (png->ctx != NULL)
    {
        spng_ctx_free(png->ctx);
    }

    free(png->image);
    free(png->translate);
}

static boolean ConvertRGBToIndexed(png_decode_t *png, const byte *src, size_t size,
                                   const byte *playpal)
{
    const size_t pixel_count = size / 3;
    byte *indexed = malloc(pixel_count);
    uniform_quantizer_t q = {0};
    byte map[256];

    if (indexed == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < pixel_count; ++i)
    {
        const int r = src[3 * i + 0];
        const int g = src[3 * i + 1];
        const int b = src[3 * i + 2];

        AddColor(&q, r, g, b);
    }

    BuildQuantizedPalette(&q);

    for (int i = 0; i < 256; ++i)
    {
        map[i] = (byte) V_GetPaletteIndex(playpal,
                                          q.palette[3 * i + 0],
                                          q.palette[3 * i + 1],
                                          q.palette[3 * i + 2]);
    }

    for (size_t i = 0; i < pixel_count; ++i)
    {
        const int r = src[3 * i + 0];
        const int g = src[3 * i + 1];
        const int b = src[3 * i + 2];
        indexed[i] = map[QuantizedPaletteIndex(r, g, b)];
    }

    free(png->image);
    png->image = indexed;
    png->image_size = pixel_count;
    return true;
}

static boolean ConvertRGBAToIndexed(png_decode_t *png, const byte *src, size_t size,
                                    const byte *playpal)
{
    const size_t pixel_count = size / 4;
    byte *indexed = malloc(pixel_count);
    uniform_quantizer_t q = {0};
    byte map[256];
    byte used_colors[256] = {0};
    boolean has_alpha = false;
    int color_key = NO_COLOR_KEY;

    if (indexed == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < pixel_count; ++i)
    {
        const int r = src[4 * i + 0];
        const int g = src[4 * i + 1];
        const int b = src[4 * i + 2];
        const int a = src[4 * i + 3];

        if (a < 255)
        {
            has_alpha = true;
            continue;
        }

        AddColor(&q, r, g, b);
    }

    BuildQuantizedPalette(&q);

    for (int i = 0; i < 256; ++i)
    {
        const byte mapped = (byte) V_GetPaletteIndex(playpal,
                                                     q.palette[3 * i + 0],
                                                     q.palette[3 * i + 1],
                                                     q.palette[3 * i + 2]);
        map[i] = mapped;
        used_colors[mapped] = 1;
    }

    if (has_alpha)
    {
        for (int i = 0; i < 256; ++i)
        {
            if (used_colors[i] == 0)
            {
                color_key = i;
                break;
            }
        }

        png->color_key = color_key;
    }

    for (size_t i = 0; i < pixel_count; ++i)
    {
        const int r = src[4 * i + 0];
        const int g = src[4 * i + 1];
        const int b = src[4 * i + 2];
        const int a = src[4 * i + 3];

        if (a < 255)
        {
            indexed[i] = (byte) color_key;
        }
        else
        {
            indexed[i] = map[QuantizedPaletteIndex(r, g, b)];
        }
    }

    free(png->image);
    png->image = indexed;
    png->image_size = pixel_count;
    return true;
}

static boolean DecodePNGImage(png_decode_t *png)
{
    struct spng_ihdr ihdr = {0};
    size_t image_size = 0;
    int fmt;
    int ret;
    byte *playpal;

    ret = spng_get_ihdr(png->ctx, &ihdr);
    if (ret)
    {
        return false;
    }

    png->width = (int) ihdr.width;
    png->height = (int) ihdr.height;

    if (png->width <= 0 || png->height <= 0)
    {
        return false;
    }

    switch (ihdr.color_type)
    {
        case SPNG_COLOR_TYPE_INDEXED:
            fmt = SPNG_FMT_PNG;
            break;

        case SPNG_COLOR_TYPE_GRAYSCALE:
        case SPNG_COLOR_TYPE_TRUECOLOR:
            fmt = SPNG_FMT_RGB8;
            break;

        default:
            fmt = SPNG_FMT_RGBA8;
            break;
    }

    if (fmt == SPNG_FMT_PNG)
    {
        struct spng_trns trns = {0};
        ret = spng_get_trns(png->ctx, &trns);

        if (ret != 0 && ret != SPNG_ECHUNKAVAIL)
        {
            return false;
        }

        if (ret == 0)
        {
            for (uint32_t i = 0; i < trns.n_type3_entries; ++i)
            {
                if (trns.type3_alpha[i] < 255)
                {
                    png->color_key = (int) i;
                    break;
                }
            }
        }
    }

    ret = spng_decoded_image_size(png->ctx, fmt, &image_size);
    if (ret)
    {
        return false;
    }

    png->image = malloc(image_size);
    if (png->image == NULL)
    {
        return false;
    }

    ret = spng_decode_image(png->ctx, png->image, image_size, fmt, 0);
    if (ret)
    {
        return false;
    }

    png->image_size = image_size;
    playpal = W_CacheLumpName("PLAYPAL", PU_CACHE);

    if (fmt == SPNG_FMT_RGB8)
    {
        const boolean ok = ConvertRGBToIndexed(png, png->image, image_size, playpal);
        W_ReleaseLumpName("PLAYPAL");
        return ok;
    }

    if (fmt == SPNG_FMT_RGBA8)
    {
        const boolean ok = ConvertRGBAToIndexed(png, png->image, image_size, playpal);
        W_ReleaseLumpName("PLAYPAL");
        return ok;
    }

    {
        struct spng_plte plte = {0};
        boolean need_translation = false;
        byte *translate;

        ret = spng_get_plte(png->ctx, &plte);
        if (ret)
        {
            W_ReleaseLumpName("PLAYPAL");
            return false;
        }

        translate = malloc(plte.n_entries);
        if (translate == NULL)
        {
            W_ReleaseLumpName("PLAYPAL");
            return false;
        }

        for (uint32_t i = 0; i < plte.n_entries; ++i)
        {
            struct spng_plte_entry *e = &plte.entries[i];
            const byte mapped = (byte) V_GetPaletteIndex(playpal,
                                                         e->red, e->green, e->blue);
            translate[i] = mapped;

            if ((byte) i != mapped)
            {
                need_translation = true;
            }
        }

        if (need_translation)
        {
            png->translate = translate;
        }
        else
        {
            free(translate);
        }
    }

    W_ReleaseLumpName("PLAYPAL");
    return true;
}

static void ApplyTranslationToPatch(patch_t *patch, const byte *translate)
{
    const int width = SHORT(patch->width);

    for (int x = 0; x < width; ++x)
    {
        byte *post = (byte *) patch + LONG(patch->columnofs[x]);

        while (*post != 0xff)
        {
            int count = post[1];
            byte *src = post + 3;

            while (count-- > 0)
            {
                *src = translate[*src];
                ++src;
            }

            post = src + 1;
        }
    }
}

static patch_t *LinearToTransPatch(const byte *data, int width, int height,
                                   int color_key, int tag, void **user,
                                   int *out_size)
{
    vcolumn_t *columns = NULL;
    size_t size = 0;
    byte *output;
    byte *rover;
    byte *col_offsets;

    for (int c = 0; c < width; ++c)
    {
        vcolumn_t col = {0};
        vpost_t post = {0};
        int offset = c;
        byte row_off = 0;
        boolean ispost = false;
        boolean first_254 = true;

        for (int r = 0; r < height; ++r)
        {
            if (row_off == 254)
            {
                if (ispost)
                {
                    array_push(col.posts, post);
                    post.pixels = NULL;
                    ispost = false;
                }

                first_254 = false;
                post.row_off = 254;
                array_push(col.posts, post);
                row_off = 0;
            }

            if (color_key == NO_COLOR_KEY || data[offset] != color_key)
            {
                if (!ispost)
                {
                    post.row_off = row_off;

                    if (!first_254)
                    {
                        row_off = 0;
                    }

                    ispost = true;
                }

                array_push(post.pixels, data[offset]);
            }
            else if (ispost)
            {
                array_push(col.posts, post);
                post.pixels = NULL;
                ispost = false;
            }

            offset += width;
            ++row_off;
        }

        if (ispost)
        {
            array_push(col.posts, post);
        }

        array_push(columns, col);
    }

    size += 4 * sizeof(int16_t);
    size += (size_t) array_size(columns) * sizeof(int32_t);

    for (int c = 0; c < array_size(columns); ++c)
    {
        for (int p = 0; p < array_size(columns[c].posts); ++p)
        {
            size += 2;
            size += 1;
            size += (size_t) array_size(columns[c].posts[p].pixels);
            size += 1;
        }

        size += 1;
    }

    output = Z_Malloc(size, tag, user);
    rover = output;

    PUTSHORT(rover, width);
    PUTSHORT(rover, height);
    PUTSHORT(rover, 0);
    PUTSHORT(rover, 0);

    col_offsets = rover;
    rover += (size_t) array_size(columns) * 4;

    for (int c = 0; c < array_size(columns); ++c)
    {
        PUTLONG(col_offsets, (uint32_t) (rover - output));

        for (int p = 0; p < array_size(columns[c].posts); ++p)
        {
            const int numpixels = array_size(columns[c].posts[p].pixels);
            byte lastval = numpixels ? columns[c].posts[p].pixels[0] : 0;

            PUTBYTE(rover, columns[c].posts[p].row_off);
            PUTBYTE(rover, numpixels);
            PUTBYTE(rover, lastval);

            for (int i = 0; i < numpixels; ++i)
            {
                lastval = columns[c].posts[p].pixels[i];
                PUTBYTE(rover, lastval);
            }

            PUTBYTE(rover, lastval);
            array_free(columns[c].posts[p].pixels);
        }

        PUTBYTE(rover, 0xff);
        array_free(columns[c].posts);
    }

    array_free(columns);

    if (out_size != NULL)
    {
        *out_size = (int) size;
    }

    return (patch_t *) output;
}

static void ReadOffsetsFromGrAb(spng_ctx *ctx, short *left, short *top)
{
    uint32_t n_chunks = 0;
    int ret = spng_get_unknown_chunks(ctx, NULL, &n_chunks);

    if (ret != 0 && ret != SPNG_ECHUNKAVAIL)
    {
        return;
    }

    if (n_chunks == 0)
    {
        return;
    }

    struct spng_unknown_chunk *chunks = malloc((size_t) n_chunks * sizeof(*chunks));
    if (chunks == NULL)
    {
        return;
    }

    ret = spng_get_unknown_chunks(ctx, chunks, &n_chunks);
    if (ret == 0)
    {
        for (uint32_t i = 0; i < n_chunks; ++i)
        {
            if (memcmp(chunks[i].type, "grAb", 4) == 0 && chunks[i].length == 8)
            {
                const byte *vals = chunks[i].data;
                const int left_raw = V_ReadBE32Signed(vals + 0);
                const int top_raw = V_ReadBE32Signed(vals + 4);
                *left = V_ClampShort(left_raw);
                *top = V_ClampShort(top_raw);
                break;
            }
        }
    }

    free(chunks);
}

boolean V_IsPNGImage(const void *data, int size)
{
    return size >= 8 && memcmp(data, "\211PNG\r\n\032\n", 8) == 0;
}

patch_t *V_PNGToPatch(const void *data, int size, int tag,
                      void **user, int *out_size)
{
    png_decode_t png = {0};
    patch_t *patch = NULL;
    short left = 0;
    short top = 0;

    if (out_size != NULL)
    {
        *out_size = 0;
    }

    if (!V_IsPNGImage(data, size))
    {
        return NULL;
    }

    png.color_key = NO_COLOR_KEY;

    if (!InitPNGDecoder(&png, data, size))
    {
        FreePNGDecoder(&png);
        return NULL;
    }

    spng_set_option(png.ctx, SPNG_KEEP_UNKNOWN_CHUNKS, 1);

    if (!DecodePNGImage(&png))
    {
        FreePNGDecoder(&png);
        return NULL;
    }

    ReadOffsetsFromGrAb(png.ctx, &left, &top);

    patch = LinearToTransPatch(png.image, png.width, png.height,
                               png.color_key, tag, user, out_size);

    if (patch != NULL)
    {
        patch->leftoffset = left;
        patch->topoffset = top;

        if (png.translate != NULL)
        {
            ApplyTranslationToPatch(patch, png.translate);
        }
    }

    FreePNGDecoder(&png);
    return patch;
}
