//
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//     GUS emulation code.
//
//     Actually emulating a GUS is far too much work; fortunately
//     GUS "emulation" already exists in the form of Timidity, which
//     supports GUS patch files. This code therefore converts Doom's
//     DMXGUS lump into an equivalent Timidity configuration file.
//
//     [JN] Reverted DMX's "GUS instrument mappings bug" emulation:
//     https://github.com/chocolate-doom/chocolate-doom/pull/705/files


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAX_INSTRUMENTS 256

typedef struct
{
    char *patch_names[MAX_INSTRUMENTS];
    // int used[MAX_INSTRUMENTS];
    int mapping[MAX_INSTRUMENTS];
    // unsigned int count;
} gus_config_t;

char *gus_patch_path = "";
int gus_ram_kb = 1024;

static unsigned int MappingIndex(void)
{
    unsigned int result = gus_ram_kb / 256;

    if (result < 1)
    {
        return 1;
    }
    else if (result > 4)
    {
        return 4;
    }
    else
    {
        return result;
    }
}

static int SplitLine(char *line, char **fields, unsigned int max_fields)
{
    unsigned int num_fields;
    char *p;

    fields[0] = line;
    num_fields = 1;

    for (p = line; *p != '\0'; ++p)
    {
        if (*p == ',')
        {
            *p = '\0';

            // Skip spaces following the comma.
            do
            {
                ++p;
            } while (*p != '\0' && isspace(*p));

            fields[num_fields] = p;
            ++num_fields;
            --p;

            if (num_fields >= max_fields)
            {
                break;
            }
        }
        else if (*p == '#')
        {
            *p = '\0';
            break;
        }
    }

    // Strip off trailing whitespace from the end of the line.
    p = fields[num_fields - 1] + strlen(fields[num_fields - 1]);
    while (p > fields[num_fields - 1] && isspace(*(p - 1)))
    {
        --p;
        *p = '\0';
    }

    return num_fields;
}

static void ParseLine(gus_config_t *config, char *line)
{
    char *fields[6];
    // unsigned int i;
    unsigned int num_fields;
    unsigned int instr_id, mapped_id;

    num_fields = SplitLine(line, fields, 6);

    if (num_fields < 6)
    {
        return;
    }

    instr_id = atoi(fields[0]);

    // Skip non GM percussions.
    /*
    if ((instr_id >= 128 && instr_id < 128 + 35) || instr_id > 128 + 81)
    {
        return;
    }
    */

    mapped_id = atoi(fields[MappingIndex()]);

    free(config->patch_names[instr_id]);
    config->patch_names[instr_id] = M_StringDuplicate(fields[5]);
    config->mapping[instr_id] = mapped_id;

    /*
    for (i = 0; i < config->count; i++)
    {
        if (config->used[i] == mapped_id)
        {
            break;
        }
    }
    
    if (i == config->count)
    {
        // DMX uses wrong patch name (we should use name of 'mapped_id'
        // instrument, but DMX uses name of 'instr_id' instead).
        free(config->patch_names[i]);
        config->patch_names[i] = M_StringDuplicate(fields[5]);
        config->used[i] = mapped_id;
        config->count++;
    }
    config->mapping[instr_id] = i;
    */
}

static void ParseDMXConfig(char *dmxconf, gus_config_t *config)
{
    char *p, *newline;
    unsigned int i;

    memset(config, 0, sizeof(gus_config_t));

    for (i = 0; i < MAX_INSTRUMENTS; ++i)
    {
        config->mapping[i] = -1;
        // config->used[i] = -1;
    }

    // config->count = 0;

    p = dmxconf;

    for (;;)
    {
        newline = strchr(p, '\n');

        if (newline != NULL)
        {
            *newline = '\0';
        }

        ParseLine(config, p);

        if (newline == NULL)
        {
            break;
        }
        else
        {
            p = newline + 1;
        }
    }
}

static void FreeDMXConfig(gus_config_t *config)
{
    unsigned int i;

    for (i = 0; i < MAX_INSTRUMENTS; ++i)
    {
        free(config->patch_names[i]);
    }
}

static char *ReadDMXConfig(void)
{
    int lumpnum;
    unsigned int len;
    char *data;

    // TODO: This should be chosen based on gamemode == commercial:

    lumpnum = W_CheckNumForName("DMXGUS");

    if (lumpnum < 0)
    {
        lumpnum = W_GetNumForName("DMXGUSC");
    }

    len = W_LumpLength(lumpnum);
    data = Z_Malloc(len + 1, PU_STATIC, NULL);
    W_ReadLump(lumpnum, data);

    data[len] = '\0';
    return data;
}

static boolean WriteTimidityConfig(const char *path, const gus_config_t *config)
{
    FILE *fstream;
    unsigned int i;

    fstream = M_fopen(path, "w");

    if (fstream == NULL)
    {
        return false;
    }

    fprintf(fstream, "# Autogenerated Timidity config.\n\n");

    fprintf(fstream, "dir \"%s\"\n", gus_patch_path);

    fprintf(fstream, "\nbank 0\n\n");

    for (i = 0; i < 128; ++i)
    {
        if (config->mapping[i] >= 0 && config->mapping[i] < MAX_INSTRUMENTS
         && config->patch_names[config->mapping[i]] != NULL)
        {
            fprintf(fstream, "%u %s\n",
                    i, config->patch_names[config->mapping[i]]);
        }
    }

    fprintf(fstream, "\ndrumset 0\n\n");
    
    // for (i = 128 + 35; i <= 128 + 81; ++i)
    for (i = 128 + 25; i < MAX_INSTRUMENTS; ++i)
    {
        if (config->mapping[i] >= 0 && config->mapping[i] < MAX_INSTRUMENTS
         && config->patch_names[config->mapping[i]] != NULL)
        {
            fprintf(fstream, "%u %s\n",
                    i - 128, config->patch_names[config->mapping[i]]);
        }
    }

    fprintf(fstream, "\n");

    fclose(fstream);

    return true;
}

boolean GUS_WriteConfig(char *path)
{
    boolean result;
    char *dmxconf;
    gus_config_t config;

    if (!strcmp(gus_patch_path, ""))
    {
        printf("You haven't configured gus_patch_path.\n");
        printf("gus_patch_path needs to point to the location of "
               "your GUS patch set.\n"
               "To get a copy of the \"standard\" GUS patches, "
               "download a copy of dgguspat.zip.\n");

        return false;
    }

    dmxconf = ReadDMXConfig();
    ParseDMXConfig(dmxconf, &config);

    result = WriteTimidityConfig(path, &config);

    FreeDMXConfig(&config);
    Z_Free(dmxconf);

    return result;
}

