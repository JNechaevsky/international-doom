//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL_stdinc.h"

#include "doomtype.h"
#include "d_iwad.h"
#include "i_system.h"
#include "m_misc.h"
#include "m_argv.h"  // haleyjd 20110212: warning fix

int		myargc;
char**		myargv;




//
// M_CheckParm
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
//

int M_CheckParmWithArgs(const char *check, int num_args)
{
    int i;

    // Check if myargv[i] has been set to NULL in LoadResponseFile(),
    // which may call I_Error(), which in turn calls M_ParmExists("-nogui").

    for (i = 1; i < myargc - num_args && myargv[i]; i++)
    {
	if (!strcasecmp(check, myargv[i]))
	    return i;
    }

    return 0;
}

//
// M_ParmExists
//
// Returns true if the given parameter exists in the program's command
// line arguments, false if not.
//

boolean M_ParmExists(const char *check)
{
    return M_CheckParm(check) != 0;
}

int M_CheckParm(const char *check)
{
    return M_CheckParmWithArgs(check, 0);
}

#define MAXARGVS        100

static void LoadResponseFile(int argv_index, const char *filename)
{
    FILE *handle;
    int size;
    char *infile;
    char *file;
    char **newargv;
    int newargc;
    int i, k;

    // Read the response file into memory
    handle = M_fopen(filename, "rb");

    if (handle == NULL)
    {
        printf ("\nNo such response file!");
        exit(1);
    }

    printf("Found response file %s!\n", filename);

    size = M_FileLength(handle);

    // Read in the entire file
    // Allocate one byte extra - this is in case there is an argument
    // at the end of the response file, in which case a '\0' will be
    // needed.

    file = malloc(size + 1);

    i = 0;

    while (i < size)
    {
        k = fread(file + i, 1, size - i, handle);

        if (k < 0)
        {
            I_Error("Failed to read full contents of '%s'", filename);
        }

        i += k;
    }

    fclose(handle);

    // Create new arguments list array

    newargv = malloc(sizeof(char *) * MAXARGVS);
    newargc = 0;
    memset(newargv, 0, sizeof(char *) * MAXARGVS);

    // Copy all the arguments in the list up to the response file

    if (argv_index >= MAXARGVS)
    {
        I_Error("Too many arguments up to the response file!");
    }

    for (i=0; i<argv_index; ++i)
    {
        newargv[i] = myargv[i];
        myargv[i] = NULL;
        ++newargc;
    }

    infile = file;
    k = 0;

    while(k < size)
    {
        // Skip past space characters to the next argument

        while(k < size && isspace(infile[k]))
        {
            ++k;
        }

        if (k >= size)
        {
            break;
        }

        // If the next argument is enclosed in quote marks, treat
        // the contents as a single argument.  This allows long filenames
        // to be specified.

        if (infile[k] == '\"')
        {
            const char *argstart;
            // Skip the first character(")
            ++k;

            argstart = &infile[k];

            // Read all characters between quotes

            while (k < size && infile[k] != '\"' && infile[k] != '\n')
            {
                ++k;
            }

            if (k >= size || infile[k] == '\n')
            {
                I_Error("Quotes unclosed in response file '%s'",
                        filename);
            }

            // Cut off the string at the closing quote

            infile[k] = '\0';
            ++k;

            if (newargc >= MAXARGVS)
            {
                I_Error("Too many arguments in the response file!");
            }

            newargv[newargc++] = M_StringDuplicate(argstart);
        }
        else
        {
            const char *argstart;
            // Read in the next argument until a space is reached

            argstart = &infile[k];

            while(k < size && !isspace(infile[k]))
            {
                ++k;
            }

            // Cut off the end of the argument at the first space

            infile[k] = '\0';
            ++k;

            if (newargc >= MAXARGVS)
            {
                I_Error("Too many arguments in the response file!");
            }

            newargv[newargc++] = M_StringDuplicate(argstart);
        }
    }

    // Add arguments following the response file argument

    if (newargc + myargc - (argv_index + 1) >= MAXARGVS)
    {
        I_Error("Too many arguments following the response file!");
    }

    for (i=argv_index + 1; i<myargc; ++i)
    {
        newargv[newargc] = myargv[i];
        myargv[i] = NULL;
        ++newargc;
    }

    // Free any old strings in myargv which were not moved to newargv
    for (i = 0; i < myargc; ++i)
    {
        if (myargv[i] != NULL)
        {
            free(myargv[i]);
            myargv[i] = NULL;
        }
    }

    free(myargv);
    myargv = newargv;
    myargc = newargc;

    free(file);

#if 0
    // Disabled - Vanilla Doom does not do this.
    // Display arguments

    printf("%d command-line args:\n", myargc);

    for (k=1; k<myargc; k++)
    {
        printf("'%s'\n", myargv[k]);
    }
#endif
}

//
// Find a Response File
//

void M_FindResponseFile(void)
{
    int i;

    for (i = 1; i < myargc; i++)
    {
        if (myargv[i][0] == '@')
        {
            LoadResponseFile(i, myargv[i] + 1);
        }
    }

    for (;;)
    {
        //!
        // @arg <filename>
        //
        // Load extra command line arguments from the given response file.
        // Arguments read from the file will be inserted into the command
        // line replacing this argument. A response file can also be loaded
        // using the abbreviated syntax '@filename.rsp'.
        //
        i = M_CheckParmWithArgs("-response", 1);
        if (i <= 0)
        {
            break;
        }
        // Replace the -response argument so that the next time through
        // the loop we'll ignore it. Since some parameters stop reading when
        // an argument beginning with a '-' is encountered, we keep something
        // that starts with a '-'.
        free(myargv[i]);
        myargv[i] = M_StringDuplicate("-_");
        LoadResponseFile(i + 1, myargv[i + 1]);
    }
}

// [FG] compose a proper command line from loose file paths passed as arguments
// to allow for loading WADs and DEHACKED patches by drag-and-drop

enum
{
    FILETYPE_UNKNOWN = 0x0,
    FILETYPE_IWAD =    0x2,
    FILETYPE_PWAD =    0x4,
    FILETYPE_DEH =     0x8,
    FILETYPE_DEMO =    0x10,
    FILETYPE_CONFIG =  0x20,
};

static boolean FileIsDemoLump(const char *filename)
{
    FILE *handle;
    int count, ver;
    byte buf[32], *p = buf;

    handle = M_fopen(filename, "rb");

    if (handle == NULL)
    {
        return false;
    }

    count = fread(buf, 1, sizeof(buf), handle);
    fclose(handle);

    if (count != sizeof(buf))
    {
        return false;
    }

    ver = *p++;

    if (ver >= 0 && ver <= 4) // v1.0/v1.1/v1.2
    {
        p--;
    }
    else
    {
        switch (ver)
        {
            case 104: // v1.4
            case 105: // v1.5
            case 106: // v1.6/v1.666
            case 107: // v1.7/v1.7a
            case 108: // v1.8
            case 109: // v1.9
            case 111: // v1.91 hack
                break;
            default:
                return false;
                break;
        }
    }

    if (*p++ > 5) // skill
    {
        return false;
    }
    if (*p++ > 9) // episode
    {
        return false;
    }
    if (*p++ > 99) // map
    {
        return false;
    }

    return true;
}

static int GuessFileType(const char *name)
{
    int ret = FILETYPE_UNKNOWN;
    const char *base;
    char *lower;
    static boolean iwad_found = false, demo_found = false;

    base = M_BaseName(name);
    lower = M_StringDuplicate(base);
    M_ForceLowercase(lower);

    // only ever add one argument to the -iwad parameter

    if (iwad_found == false && D_IsIWADName(lower))
    {
        ret = FILETYPE_IWAD;
        iwad_found = true;
    }
    else if (M_StringEndsWith(lower, ".wad"))
    {
        ret = FILETYPE_PWAD;
    }
    else if (M_StringEndsWith(lower, ".lmp"))
    {
        // only ever add one argument to the -playdemo parameter

        if (demo_found == false && FileIsDemoLump(name))
        {
            ret = FILETYPE_DEMO;
            demo_found = true;
        }
        else
        {
            ret = FILETYPE_PWAD;
        }
    }
    else if (M_StringEndsWith(lower, ".deh") ||
             M_StringEndsWith(lower, ".bex") || // [crispy] *.bex
             M_StringEndsWith(lower, ".hhe") ||
             M_StringEndsWith(lower, ".seh"))
    {
        ret = FILETYPE_DEH;
    }
    else if (M_StringEndsWith(lower, ".ini")) // [JN] *.ini config files
    {
        ret = FILETYPE_CONFIG;
    }

    free(lower);

    return ret;
}

typedef struct
{
    char *str;
    int type, stable;
} argument_t;

static int CompareByFileType(const void *a, const void *b)
{
    const argument_t *arg_a = (const argument_t *) a;
    const argument_t *arg_b = (const argument_t *) b;

    const int ret = arg_a->type - arg_b->type;

    return ret ? ret : (arg_a->stable - arg_b->stable);
}

void M_AddLooseFiles(void)
{
    int i, types = 0;
    char **newargv;
    argument_t *arguments;

    if (myargc < 2)
    {
        return;
    }

    // allocate space for up to five additional regular parameters
    // (-iwad, -merge, -deh, -playdemo, -config)

    arguments = malloc((myargc + 5) * sizeof(*arguments));
    memset(arguments, 0, (myargc + 5) * sizeof(*arguments));

    // check the command line and make sure it does not already
    // contain any regular parameters or response files
    // but only fully-qualified LFS or UNC file paths

    for (i = 1; i < myargc; i++)
    {
        char *arg = myargv[i];
        int type;

        if (strlen(arg) < 3 ||
            arg[0] == '-' ||
            arg[0] == '@' ||
#if defined (_WIN32)
            ((!isalpha(arg[0]) || arg[1] != ':' || arg[2] != '\\') &&
            (arg[0] != '\\' || arg[1] != '\\'))
#else
            (arg[0] != '/' && arg[0] != '.')
#endif
          )
        {
            free(arguments);
            return;
        }

        type = GuessFileType(arg);
        arguments[i].str = arg;
        arguments[i].type = type;
        arguments[i].stable = i;
        types |= type;
    }

    // add space for one additional regular parameter
    // for each discovered file type in the new argument  list
    // and sort parameters right before their corresponding file paths

    if (types & FILETYPE_IWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-iwad");
        arguments[myargc].type = FILETYPE_IWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_PWAD)
    {
        arguments[myargc].str = M_StringDuplicate("-merge");
        arguments[myargc].type = FILETYPE_PWAD - 1;
        myargc++;
    }
    if (types & FILETYPE_DEH)
    {
        arguments[myargc].str = M_StringDuplicate("-deh");
        arguments[myargc].type = FILETYPE_DEH - 1;
        myargc++;
    }
    if (types & FILETYPE_DEMO)
    {
        arguments[myargc].str = M_StringDuplicate("-playdemo");
        arguments[myargc].type = FILETYPE_DEMO - 1;
        myargc++;
    }
    if (types & FILETYPE_CONFIG)
    {
        arguments[myargc].str = M_StringDuplicate("-config");
        arguments[myargc].type = FILETYPE_CONFIG - 1;
        myargc++;
    }

    newargv = malloc(myargc * sizeof(*newargv));

    // sort the argument list by file type, except for the zeroth argument
    // which is the executable invocation itself

    SDL_qsort(arguments + 1, myargc - 1, sizeof(*arguments), CompareByFileType);

    newargv[0] = myargv[0];

    for (i = 1; i < myargc; i++)
    {
        newargv[i] = arguments[i].str;
    }

    free(arguments);

    free(myargv);
    myargv = newargv;
}

// Return the name of the executable used to start the program:

const char *M_GetExecutableName(void)
{
    return M_BaseName(myargv[0]);
}

char *exedir = NULL;

void M_SetExeDir(void)
{
    char *dirname;

    dirname = M_DirName(myargv[0]);
    exedir = M_StringJoin(dirname, DIR_SEPARATOR_S, NULL);
    free(dirname);
}
