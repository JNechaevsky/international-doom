//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2023 Julia Nechaevskaya
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
//	Disk load indicator.
//


#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "v_diskicon.h"

#include "id_vars.h"


// Only display the disk icon if more then this much bytes have been read
// during the previous tic.
static const int diskicon_threshold = 20*1024;

static int loading_disk_xoffs = 0;
static int loading_disk_yoffs = 0;

// Number of bytes read since the last call to V_DrawDiskIcon().
static size_t recent_bytes_read = 0;

// [JN] Which icon to use, diskette or cdrom.
char *disk_lump_name;


void V_EnableLoadingDisk (void)
{
    disk_lump_name = M_CheckParm("-cdrom") > 0 ? "STCDROM" : "STDISK";

    loading_disk_xoffs = ORIGWIDTH - LOADING_DISK_W + WIDESCREENDELTA;
    loading_disk_yoffs = vid_diskicon == 2 ? 0 :        // Top
                         ORIGHEIGHT - LOADING_DISK_H;  // Bottom
}

void V_BeginRead (const size_t nbytes)
{
    recent_bytes_read += nbytes;
}

void V_DrawDiskIcon (void)
{
    // [JN] Don't draw disk icon while demo warp.
    if (demowarp)
    {
        return;
    }

    if (recent_bytes_read > diskicon_threshold)
    {
        V_DrawPatch(loading_disk_xoffs, loading_disk_yoffs, 
                    W_CacheLumpName(disk_lump_name, PU_CACHE));
    }

    recent_bytes_read = 0;
}
