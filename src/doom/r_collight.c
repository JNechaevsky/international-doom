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

#include "doomstat.h"
#include "i_system.h"
#include "p_local.h"
#include "r_collight.h"
#include "w_wad.h"

typedef struct
{
    uint32_t rgb;
    lighttable_t *lut;
    lighttable_t **rows;
    int row_count;
} collight_bank_t;

static collight_bank_t *collight_banks;
static int collight_num_banks;
static int collight_cap_banks;

// -----------------------------------------------------------------------------
// CL_EnsureDefaultBank
// [PN] Bank 0 is the neutral (white) bank and always maps to base colormaps[].
// -----------------------------------------------------------------------------

static void CL_EnsureDefaultBank(void)
{
    if (collight_num_banks > 0)
    {
        return;
    }

    if (collight_cap_banks < 1)
    {
        collight_cap_banks = 1;
        collight_banks = I_Realloc(collight_banks, (size_t)collight_cap_banks * sizeof(*collight_banks));
    }

    memset(&collight_banks[0], 0, sizeof(collight_banks[0]));
    collight_banks[0].rgb = 0xFFFFFFu;
    collight_num_banks = 1;
}

// -----------------------------------------------------------------------------
// CL_FreeBankData
// [PN] Frees one colored LUT bank (not the bank slot itself).
// -----------------------------------------------------------------------------

static void CL_FreeBankData(collight_bank_t *bank)
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
// CL_ResetSectorAssignments
// [PN] Clears per-sector bank indices for the currently loaded level.
// -----------------------------------------------------------------------------

static void CL_ResetSectorAssignments(void)
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
// CL_FindOrAddBank
// [PN] Deduplicates RGB colors and returns a compact bank index.
// -----------------------------------------------------------------------------

static int CL_FindOrAddBank(const uint32_t rgb)
{
    const uint32_t norm = rgb & 0x00FFFFFFu;

    if (norm == 0x00FFFFFFu)
    {
        return 0;
    }

    for (int i = 1; i < collight_num_banks; ++i)
    {
        if (collight_banks[i].rgb == norm)
        {
            return i;
        }
    }

    if (collight_num_banks >= collight_cap_banks)
    {
        const int new_cap = collight_cap_banks > 0 ? collight_cap_banks * 2 : 8;
        collight_banks = I_Realloc(collight_banks, (size_t)new_cap * sizeof(*collight_banks));
        memset(collight_banks + collight_cap_banks, 0, (size_t)(new_cap - collight_cap_banks) * sizeof(*collight_banks));
        collight_cap_banks = new_cap;
    }

    const int bank_index = collight_num_banks++;
    collight_banks[bank_index].rgb = norm;
    collight_banks[bank_index].lut = NULL;
    collight_banks[bank_index].rows = NULL;
    collight_banks[bank_index].row_count = 0;

    return bank_index;
}

// -----------------------------------------------------------------------------
// CL_TrimLeft / CL_TrimRight
// [PN] In-place trimming helpers for simple text-based LUT parsing.
// -----------------------------------------------------------------------------

static char *CL_TrimLeft(char *text)
{
    // [PN] Some editors store UTF-8 BOM in plain text lumps.
    // Strip it once so first token matching (MISSION/MAP) is stable.
    if ((unsigned char)text[0] == 0xEF
     && (unsigned char)text[1] == 0xBB
     && (unsigned char)text[2] == 0xBF)
    {
        text += 3;
    }

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }

    return text;
}

static void CL_TrimRight(char *text)
{
    size_t len = strlen(text);

    while (len > 0 && isspace((unsigned char)text[len - 1]))
    {
        text[--len] = '\0';
    }
}

// -----------------------------------------------------------------------------
// CL_ParseHexRGB
// [PN] Parses "RRGGBB", "#RRGGBB", or "0xRRGGBB" into a 24-bit RGB value.
// -----------------------------------------------------------------------------

static boolean CL_ParseHexRGB(const char *text, uint32_t *rgb)
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
// CL_MapMatches
// [PN] Matches a map token against current map name; "*" acts as wildcard.
// -----------------------------------------------------------------------------

static boolean CL_MapMatches(const char *token, const char *map_name)
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
// CL_MissionMatches
// [PN] Matches mission token against current IWAD mission.
//      Supported tokens: DOOM, DOOM2, TNT, PLUTONIA (also PLUT).
// -----------------------------------------------------------------------------

static boolean CL_MissionMatches(const char *token)
{
    const GameMission_t mission = logical_gamemission;

    if (token == NULL)
    {
        return false;
    }

    if (token[0] == '*' && token[1] == '\0')
    {
        return true;
    }

    if (!strncasecmp(token, "DOOM2", 5) && token[5] == '\0')
    {
        return mission == doom2 || mission == doom2f;
    }

    if (!strncasecmp(token, "DOOM", 4) && token[4] == '\0')
    {
        return mission == doom;
    }

    if (!strncasecmp(token, "TNT", 3) && token[3] == '\0')
    {
        return mission == pack_tnt;
    }

    if ((!strncasecmp(token, "PLUTONIA", 8) && token[8] == '\0')
     || (!strncasecmp(token, "PLUT", 4) && token[4] == '\0'))
    {
        return mission == pack_plut;
    }

    return false;
}

// -----------------------------------------------------------------------------
// CL_IsMissionToken
// [PN] Recognizes supported mission tags in token[0], independent from
//      currently running IWAD.
// -----------------------------------------------------------------------------

static boolean CL_IsMissionToken(const char *token)
{
    if (token == NULL)
    {
        return false;
    }

    if (token[0] == '*' && token[1] == '\0')
    {
        return true;
    }

    if ((!strncasecmp(token, "DOOM2", 5) && token[5] == '\0')
     || (!strncasecmp(token, "DOOM", 4) && token[4] == '\0')
     || (!strncasecmp(token, "TNT", 3) && token[3] == '\0')
     || (!strncasecmp(token, "PLUTONIA", 8) && token[8] == '\0')
     || (!strncasecmp(token, "PLUT", 4) && token[4] == '\0'))
    {
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// CL_ParseLine
// [PN] Reads one LUT line:
//      "12 FF0000"
//      "MAP01 12 FF0000"
//      "DOOM2 MAP01 12 FF0000"
// -----------------------------------------------------------------------------

static void CL_ParseLine(char *line, const char *map_name)
{
    char *token;
    char *tokens[5];
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

    line = CL_TrimLeft(line);
    CL_TrimRight(line);

    if (*line == '\0')
    {
        return;
    }

    ptr = line;

    while (*ptr != '\0' && token_count < 5)
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
    else if (token_count == 3)
    {
        if (!CL_MapMatches(tokens[0], map_name))
        {
            return;
        }

        sector_token = tokens[1];
        color_token = tokens[2];
    }
    else if (CL_IsMissionToken(tokens[0]))
    {
        if (!CL_MissionMatches(tokens[0]) || !CL_MapMatches(tokens[1], map_name))
        {
            return;
        }

        sector_token = tokens[2];
        color_token = tokens[3];
    }
    else
    {
        // [PN] Backward-compatible tolerant path:
        // if token[0] is not a known MISSION tag, treat line as
        // "MAP SECTOR COLOR" and ignore any extra tokens.
        if (!CL_MapMatches(tokens[0], map_name))
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

    if (!CL_ParseHexRGB(color_token, &rgb))
    {
        return;
    }

    sectors[sector_number].lightbank = (unsigned short)CL_FindOrAddBank(rgb);
}

// -----------------------------------------------------------------------------
// CL_ParseLump
// [PN] Parses all lines from one COLLIGHT text lump.
// -----------------------------------------------------------------------------

static void CL_ParseLump(const int lumpnum, const char *map_name)
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

        CL_ParseLine(line, map_name);
        line = next;
    }

    free(text);
}

// -----------------------------------------------------------------------------
// CL_BuildBank
// [PN] Builds one colored LUT bank by channel-multiplying base colormaps[].
// -----------------------------------------------------------------------------

static void CL_BuildBank(collight_bank_t *bank)
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
// R_ColLight_ResetLevel
// [PN] Public reset entry for level transitions.
// -----------------------------------------------------------------------------

void R_ColLight_ResetLevel(void)
{
    for (int i = 0; i < collight_num_banks; ++i)
    {
        CL_FreeBankData(&collight_banks[i]);
    }

    free(collight_banks);
    collight_banks = NULL;
    collight_num_banks = 0;
    collight_cap_banks = 0;

    CL_EnsureDefaultBank();
}

// -----------------------------------------------------------------------------
// R_ColLight_LoadMapLUT
// [PN] Applies COLLIGHT entries for the current map and prepares bank list.
// -----------------------------------------------------------------------------

void R_ColLight_LoadMapLUT(const char *map_name)
{
    CL_EnsureDefaultBank();
    CL_ResetSectorAssignments();

    if (map_name == NULL || *map_name == '\0')
    {
        return;
    }

    for (unsigned int i = 0; i < numlumps; ++i)
    {
        if (!strncasecmp(lumpinfo[i]->name, "COLLIGHT", 8))
        {
            CL_ParseLump((int)i, map_name);
        }
    }

    R_ColLight_RebuildBanks();
}

// -----------------------------------------------------------------------------
// R_ColLight_RebuildBanks
// [PN] Rebuilds all colored banks whenever base colormaps[] are regenerated.
// -----------------------------------------------------------------------------

void R_ColLight_RebuildBanks(void)
{
    if (collight_num_banks <= 1)
    {
        return;
    }

    // [PN] Sector-light LUT banks are useful for every renderer mode:
    // they are prebuilt once from current colormaps[] and then applied as
    // pointer remaps during rendering.
    if (colormaps == NULL || NUMCOLORMAPS <= 0)
    {
        for (int i = 1; i < collight_num_banks; ++i)
        {
            CL_FreeBankData(&collight_banks[i]);
        }

        return;
    }

    for (int i = 1; i < collight_num_banks; ++i)
    {
        CL_BuildBank(&collight_banks[i]);
    }
}

// -----------------------------------------------------------------------------
// R_ColLight_Apply
// [PN] Fast row remap: base colormap pointer -> colored bank pointer.
// -----------------------------------------------------------------------------

lighttable_t *R_ColLight_Apply(const int bank_index, const lighttable_t *base_colormap)
{
    const collight_bank_t *bank;
    ptrdiff_t delta;
    int row_index;

    if (base_colormap == NULL)
    {
        return NULL;
    }

    if (bank_index <= 0 || bank_index >= collight_num_banks)
    {
        return (lighttable_t *)base_colormap;
    }

    bank = &collight_banks[bank_index];

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

