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

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "p_local.h"
#include "r_seclight.h"
#include "w_wad.h"

typedef struct
{
    uint32_t rgb;
    lighttable_t *lut;
    lighttable_t **rows;
    int row_count;
} seclight_bank_t;

static seclight_bank_t *seclight_banks;
static int seclight_num_banks;
static int seclight_cap_banks;

// -----------------------------------------------------------------------------
// SL_EnsureDefaultBank
// [PN] Bank 0 is the neutral (white) bank and always maps to base colormaps[].
// -----------------------------------------------------------------------------

static void SL_EnsureDefaultBank(void)
{
    if (seclight_num_banks > 0)
    {
        return;
    }

    if (seclight_cap_banks < 1)
    {
        seclight_cap_banks = 1;
        seclight_banks = I_Realloc(seclight_banks, (size_t)seclight_cap_banks * sizeof(*seclight_banks));
    }

    memset(&seclight_banks[0], 0, sizeof(seclight_banks[0]));
    seclight_banks[0].rgb = 0xFFFFFFu;
    seclight_num_banks = 1;
}

// -----------------------------------------------------------------------------
// SL_FreeBankData
// [PN] Frees one colored LUT bank (not the bank slot itself).
// -----------------------------------------------------------------------------

static void SL_FreeBankData(seclight_bank_t *bank)
{
    if (bank == NULL)
    {
        return;
    }

    free(bank->lut);
    free(bank->rows);
    bank->lut = NULL;
    bank->rows = NULL;
    bank->row_count = 0;
}

// -----------------------------------------------------------------------------
// SL_ResetSectorAssignments
// [PN] Clears per-sector bank indices for the currently loaded level.
// -----------------------------------------------------------------------------

static void SL_ResetSectorAssignments(void)
{
    if (sectors == NULL || numsectors <= 0)
    {
        return;
    }

    for (int i = 0; i < numsectors; ++i)
    {
        sectors[i].lightbank = 0;
    }
}

// -----------------------------------------------------------------------------
// SL_FindOrAddBank
// [PN] Deduplicates RGB colors and returns a compact bank index.
// -----------------------------------------------------------------------------

static int SL_FindOrAddBank(const uint32_t rgb)
{
    const uint32_t norm = rgb & 0x00FFFFFFu;

    if (norm == 0x00FFFFFFu)
    {
        return 0;
    }

    for (int i = 1; i < seclight_num_banks; ++i)
    {
        if (seclight_banks[i].rgb == norm)
        {
            return i;
        }
    }

    if (seclight_num_banks >= seclight_cap_banks)
    {
        const int new_cap = seclight_cap_banks > 0 ? seclight_cap_banks * 2 : 8;
        seclight_banks = I_Realloc(seclight_banks, (size_t)new_cap * sizeof(*seclight_banks));
        memset(seclight_banks + seclight_cap_banks, 0, (size_t)(new_cap - seclight_cap_banks) * sizeof(*seclight_banks));
        seclight_cap_banks = new_cap;
    }

    const int bank_index = seclight_num_banks++;
    seclight_banks[bank_index].rgb = norm;
    seclight_banks[bank_index].lut = NULL;
    seclight_banks[bank_index].rows = NULL;
    seclight_banks[bank_index].row_count = 0;

    return bank_index;
}

// -----------------------------------------------------------------------------
// SL_TrimLeft / SL_TrimRight
// [PN] In-place trimming helpers for simple text-based LUT parsing.
// -----------------------------------------------------------------------------

static char *SL_TrimLeft(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }

    return text;
}

static void SL_TrimRight(char *text)
{
    size_t len = strlen(text);

    while (len > 0 && isspace((unsigned char)text[len - 1]))
    {
        text[--len] = '\0';
    }
}

// -----------------------------------------------------------------------------
// SL_ParseHexRGB
// [PN] Parses "RRGGBB", "#RRGGBB", or "0xRRGGBB" into a 24-bit RGB value.
// -----------------------------------------------------------------------------

static boolean SL_ParseHexRGB(const char *text, uint32_t *rgb)
{
    const char *ptr = text;
    char *endptr;
    unsigned long value;

    if (ptr[0] == '#')
    {
        ++ptr;
    }
    else if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'))
    {
        ptr += 2;
    }

    if (strlen(ptr) != 6)
    {
        return false;
    }

    value = strtoul(ptr, &endptr, 16);

    if (*endptr != '\0' || value > 0xFFFFFFul)
    {
        return false;
    }

    *rgb = (uint32_t)value;
    return true;
}

// -----------------------------------------------------------------------------
// SL_MapMatches
// [PN] Matches a map token against current map name; "*" acts as wildcard.
// -----------------------------------------------------------------------------

static boolean SL_MapMatches(const char *token, const char *map_name)
{
    size_t map_len;
    size_t token_len;

    if (token == NULL || map_name == NULL)
    {
        return false;
    }

    map_len = strlen(map_name);
    token_len = strlen(token);

    if (token[0] == '*' && token[1] == '\0')
    {
        return true;
    }

    return token_len == map_len && !strncasecmp(token, map_name, map_len);
}

// -----------------------------------------------------------------------------
// SL_ParseLine
// [PN] Reads one LUT line:
//      "MAP01 12 FF0000" / "E1M1 12 FF0000" / "12 FF0000".
// -----------------------------------------------------------------------------

static void SL_ParseLine(char *line, const char *map_name)
{
    char *token;
    char *tokens[4];
    int token_count = 0;
    char *sector_token;
    char *color_token;
    char *ptr;
    long sector_number;
    uint32_t rgb;

    if (line == NULL)
    {
        return;
    }

    for (ptr = line; *ptr != '\0'; ++ptr)
    {
        if (*ptr == '#' || *ptr == ';')
        {
            *ptr = '\0';
            break;
        }

        if (*ptr == '/' && ptr[1] == '/')
        {
            *ptr = '\0';
            break;
        }
    }

    line = SL_TrimLeft(line);
    SL_TrimRight(line);

    if (*line == '\0')
    {
        return;
    }

    ptr = line;

    while (*ptr != '\0' && token_count < 4)
    {
        while (*ptr != '\0' && (isspace((unsigned char)*ptr) || *ptr == ','))
        {
            ++ptr;
        }

        if (*ptr == '\0')
        {
            break;
        }

        token = ptr;
        tokens[token_count++] = token;

        while (*ptr != '\0' && !isspace((unsigned char)*ptr) && *ptr != ',')
        {
            ++ptr;
        }

        if (*ptr != '\0')
        {
            *ptr++ = '\0';
        }
    }

    if (token_count < 2)
    {
        return;
    }

    if (token_count == 2)
    {
        sector_token = tokens[0];
        color_token = tokens[1];
    }
    else
    {
        if (!SL_MapMatches(tokens[0], map_name))
        {
            return;
        }

        sector_token = tokens[1];
        color_token = tokens[2];
    }

    sector_number = strtol(sector_token, &ptr, 10);

    if (*ptr != '\0' || sector_number < 0 || sector_number >= numsectors)
    {
        return;
    }

    if (!SL_ParseHexRGB(color_token, &rgb))
    {
        return;
    }

    sectors[sector_number].lightbank = (unsigned short)SL_FindOrAddBank(rgb);
}

// -----------------------------------------------------------------------------
// SL_ParseLump
// [PN] Parses all lines from one SECLIGHT text lump.
// -----------------------------------------------------------------------------

static void SL_ParseLump(const int lumpnum, const char *map_name)
{
    const int len = W_LumpLength((lumpindex_t)lumpnum);
    char *text;
    char *line;

    if (len <= 0)
    {
        return;
    }

    text = malloc((size_t)len + 1u);

    if (text == NULL)
    {
        return;
    }

    W_ReadLump((lumpindex_t)lumpnum, text);
    text[len] = '\0';

    line = text;

    while (*line != '\0')
    {
        char *next = strchr(line, '\n');

        if (next != NULL)
        {
            *next = '\0';
            ++next;
        }
        else
        {
            next = line + strlen(line);
        }

        SL_ParseLine(line, map_name);
        line = next;
    }

    free(text);
}

// -----------------------------------------------------------------------------
// SL_BuildBank
// [PN] Builds one colored LUT bank by channel-multiplying base colormaps[].
// -----------------------------------------------------------------------------

static void SL_BuildBank(seclight_bank_t *bank)
{
    const int row_count = NUMCOLORMAPS;
    const size_t lut_entries = (size_t)row_count << 8;
    const byte tint_r = (byte)((bank->rgb >> 16) & 0xFFu);
    const byte tint_g = (byte)((bank->rgb >> 8) & 0xFFu);
    const byte tint_b = (byte)(bank->rgb & 0xFFu);
    byte mul_r[256];
    byte mul_g[256];
    byte mul_b[256];

    bank->lut = I_Realloc(bank->lut, lut_entries * sizeof(*bank->lut));
    bank->rows = I_Realloc(bank->rows, (size_t)row_count * sizeof(*bank->rows));
    bank->row_count = row_count;

    for (int i = 0; i < 256; ++i)
    {
        mul_r[i] = (byte)((i * tint_r + 127) / 255);
        mul_g[i] = (byte)((i * tint_g + 127) / 255);
        mul_b[i] = (byte)((i * tint_b + 127) / 255);
    }

    for (int row = 0; row < row_count; ++row)
    {
        const lighttable_t *const src = colormaps + ((size_t)row << 8);
        lighttable_t *const dst = bank->lut + ((size_t)row << 8);

        bank->rows[row] = dst;

        for (int i = 0; i < 256; ++i)
        {
            const uint32_t px = src[i];
            const byte r = mul_r[(px >> 16) & 0xFFu];
            const byte g = mul_g[(px >> 8) & 0xFFu];
            const byte b = mul_b[px & 0xFFu];

            dst[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

// -----------------------------------------------------------------------------
// R_SecLight_ResetLevel
// [PN] Public reset entry for level transitions.
// -----------------------------------------------------------------------------

void R_SecLight_ResetLevel(void)
{
    for (int i = 0; i < seclight_num_banks; ++i)
    {
        SL_FreeBankData(&seclight_banks[i]);
    }

    free(seclight_banks);
    seclight_banks = NULL;
    seclight_num_banks = 0;
    seclight_cap_banks = 0;

    SL_EnsureDefaultBank();
}

// -----------------------------------------------------------------------------
// R_SecLight_LoadMapLUT
// [PN] Applies SECLIGHT entries for the current map and prepares bank list.
// -----------------------------------------------------------------------------

void R_SecLight_LoadMapLUT(const char *map_name)
{
    SL_EnsureDefaultBank();
    SL_ResetSectorAssignments();

    if (map_name == NULL || *map_name == '\0')
    {
        return;
    }

    for (unsigned int i = 0; i < numlumps; ++i)
    {
        if (!strncasecmp(lumpinfo[i]->name, "SECLIGHT", 8))
        {
            SL_ParseLump((int)i, map_name);
        }
    }

    R_SecLight_RebuildBanks();
}

// -----------------------------------------------------------------------------
// R_SecLight_RebuildBanks
// [PN] Rebuilds all colored banks whenever base colormaps[] are regenerated.
// -----------------------------------------------------------------------------

void R_SecLight_RebuildBanks(void)
{
    if (seclight_num_banks <= 1)
    {
        return;
    }

    // [PN] Sector-light LUT banks are useful for every renderer mode:
    // they are prebuilt once from current colormaps[] and then applied as
    // pointer remaps during rendering.
    if (colormaps == NULL || NUMCOLORMAPS <= 0)
    {
        for (int i = 1; i < seclight_num_banks; ++i)
        {
            SL_FreeBankData(&seclight_banks[i]);
        }

        return;
    }

    for (int i = 1; i < seclight_num_banks; ++i)
    {
        SL_BuildBank(&seclight_banks[i]);
    }
}

// -----------------------------------------------------------------------------
// R_SecLight_Apply
// [PN] Fast row remap: base colormap pointer -> colored bank pointer.
// -----------------------------------------------------------------------------

lighttable_t *R_SecLight_Apply(const int bank_index, const lighttable_t *base_colormap)
{
    const seclight_bank_t *bank;
    ptrdiff_t delta;
    int row_index;

    if (base_colormap == NULL)
    {
        return NULL;
    }

    if (bank_index <= 0 || bank_index >= seclight_num_banks)
    {
        return (lighttable_t *)base_colormap;
    }

    bank = &seclight_banks[bank_index];

    if (bank->rows == NULL || bank->row_count <= 0)
    {
        return (lighttable_t *)base_colormap;
    }

    delta = base_colormap - colormaps;

    if (delta < 0 || (delta & 0xFF) != 0)
    {
        return (lighttable_t *)base_colormap;
    }

    row_index = (int)(delta >> 8);

    if ((unsigned int)row_index >= (unsigned int)bank->row_count)
    {
        return (lighttable_t *)base_colormap;
    }

    return bank->rows[row_index];
}
