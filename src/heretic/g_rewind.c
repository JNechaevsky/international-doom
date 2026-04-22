//
// Copyright(C) 2026 Polina "Aura" N.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "ct_chat.h"
#include "deh_main.h"
#include "g_rewind.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"

#include "id_vars.h"

#define VERSIONSIZE 16

typedef struct keyframe_s
{
    byte *data;
    size_t size;
    int tic;
    struct keyframe_s *next;
    struct keyframe_s *prev;
} keyframe_t;

static keyframe_t *queue_top;
static keyframe_t *queue_tail;
static int queue_count;
static boolean disable_rewind;
static boolean rewind_restoring;

static boolean RewindQueueIsEmpty(void)
{
    return queue_top == NULL;
}

static boolean RewindAllowedGamestate(void)
{
    return gamestate == GS_LEVEL
        || gamestate == GS_INTERMISSION
        || gamestate == GS_FINALE;
}

static int RewindIntervalTics(void)
{
    return TICRATE * BETWEEN(1, 10, crl_rewind_interval);
}

static int RewindDepth(void)
{
    return BETWEEN(10, 1000, crl_rewind_depth);
}

static int RewindTimeout(void)
{
    return BETWEEN(0, 1000, crl_rewind_timeout);
}

static void FreeKeyframe(keyframe_t *keyframe)
{
    if (keyframe == NULL)
    {
        return;
    }

    free(keyframe->data);
    free(keyframe);
}

static void PushKeyframe(keyframe_t *keyframe)
{
    while (queue_count >= RewindDepth())
    {
        keyframe_t *oldtail = queue_tail;

        queue_tail = oldtail->prev;

        if (queue_tail != NULL)
        {
            queue_tail->next = NULL;
        }
        else
        {
            queue_top = NULL;
        }

        FreeKeyframe(oldtail);
        --queue_count;
    }

    keyframe->next = queue_top;
    keyframe->prev = NULL;

    if (queue_top != NULL)
    {
        queue_top->prev = keyframe;
    }
    else
    {
        queue_tail = keyframe;
    }

    queue_top = keyframe;
    ++queue_count;
}

static keyframe_t *PopKeyframe(void)
{
    keyframe_t *keyframe;

    if (RewindQueueIsEmpty())
    {
        return NULL;
    }

    keyframe = queue_top;
    queue_top = keyframe->next;

    if (queue_top != NULL)
    {
        queue_top->prev = NULL;
    }
    else
    {
        queue_tail = NULL;
    }

    keyframe->next = NULL;
    keyframe->prev = NULL;
    --queue_count;

    return keyframe;
}

static keyframe_t *SaveKeyframe(void)
{
    keyframe_t *keyframe = calloc(1, sizeof(*keyframe));
    char description[SAVESTRINGSIZE];
    char version_text[VERSIONSIZE];
    char wadname[SAVEGAME_WADNAMESIZE];
    int i;

    if (keyframe == NULL)
    {
        return NULL;
    }

    memset(description, 0, sizeof(description));
    M_StringCopy(description, "REWIND", sizeof(description));
    SV_OpenMemoryWrite();

    memset(version_text, 0, sizeof(version_text));
    DEH_snprintf(version_text, VERSIONSIZE, "version %i", HERETIC_VERSION);
    memset(wadname, 0, sizeof(wadname));

    SV_Write(description, SAVESTRINGSIZE);
    SV_Write(version_text, VERSIONSIZE);
    SV_WriteByte(gameskill);
    SV_WriteByte(gameepisode);
    SV_WriteByte(gamemap);
    SV_WriteByte(idmusnum);

    for (i = 0; i < MAXPLAYERS; ++i)
    {
        SV_WriteByte(playeringame[i]);
    }

    for (i = 0; i < 4; ++i)
    {
        SV_WriteByte("PWAD"[i]);
    }

    SV_Write(wadname, SAVEGAME_WADNAMESIZE);
    SV_WriteByte(leveltime >> 16);
    SV_WriteByte(leveltime >> 8);
    SV_WriteByte(leveltime);
    SV_WriteByte(totalleveltimes >> 16);
    SV_WriteByte(totalleveltimes >> 8);
    SV_WriteByte(totalleveltimes);
    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveSpecials();
    P_ArchiveAutomap();
    P_ArchiveOldSpecials();
    SV_WriteByte(fastparm ? 1 : 0);
    SV_WriteByte(respawnparm ? 1 : 0);
    SV_WriteByte(coop_spawns ? 1 : 0);
    SV_WriteSaveGameEOF();

    if (!SV_CloseMemoryWrite(&keyframe->data, &keyframe->size))
    {
        FreeKeyframe(keyframe);
        return NULL;
    }

    keyframe->tic = gametic;
    return keyframe;
}

static boolean LoadKeyframe(const keyframe_t *keyframe)
{
    char version_check[VERSIONSIZE];
    byte wad_header[4];
    char version_text[VERSIONSIZE];
    int i;
    int a, b, c;
    int d, e, f;

    SV_OpenMemoryRead(keyframe->data, keyframe->size);

    SV_Seek(SAVESTRINGSIZE, SEEK_SET);
    SV_Read(version_text, VERSIONSIZE);

    memset(version_check, 0, sizeof(version_check));
    DEH_snprintf(version_check, sizeof(version_check), "version %i", HERETIC_VERSION);

    if (strncmp(version_text, version_check, VERSIONSIZE) != 0)
    {
        SV_Close();
        return false;
    }

    gameskill = SV_ReadByte();
    gameepisode = SV_ReadByte();
    gamemap = SV_ReadByte();
    idmusnum = SV_ReadByte();

    if (idmusnum == 255)
    {
        idmusnum = -1;
    }

    for (i = 0; i < MAXPLAYERS; ++i)
    {
        playeringame[i] = SV_ReadByte();
    }

    for (i = 0; i < 4; ++i)
    {
        wad_header[i] = SV_ReadByte();
    }

    if (memcmp(wad_header, "PWAD", 4) != 0)
    {
        SV_Close();
        return false;
    }

    {
        char save_wad[SAVEGAME_WADNAMESIZE];
        SV_Read(save_wad, SAVEGAME_WADNAMESIZE);
    }

    rewind_restoring = true;
    G_InitNew(gameskill, gameepisode, gamemap);
    rewind_restoring = false;

    a = SV_ReadByte();
    b = SV_ReadByte();
    c = SV_ReadByte();
    leveltime = (a << 16) + (b << 8) + c;

    d = SV_ReadByte();
    e = SV_ReadByte();
    f = SV_ReadByte();
    totalleveltimes = (d << 16) + (e << 8) + f;

    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveSpecials();
    P_RestoreTargets();
    P_UnArchiveAutomap();
    P_UnArchiveOldSpecials();
    fastparm = (SV_ReadByte() != 0);
    respawnparm = (SV_ReadByte() != 0);
    coop_spawns = (SV_ReadByte() != 0);

    if (SV_ReadByte() != SAVE_GAME_TERMINATOR)
    {
        SV_Close();
        I_Error("Bad rewind keyframe");
    }

    SV_Close();

    if (setsizeneeded)
    {
        R_ExecuteSetViewSize();
    }

    R_FillBackScreen();
    gamestate = GS_LEVEL;

    return true;
}

static void FreeKeyframeQueue(void)
{
    keyframe_t *current = queue_top;

    while (current != NULL)
    {
        keyframe_t *next = current->next;

        FreeKeyframe(current);
        current = next;
    }

    queue_top = NULL;
    queue_tail = NULL;
    queue_count = 0;
}

void G_Rewind(void)
{
    if (netgame || demoplayback || demorecording || !RewindAllowedGamestate())
    {
        CT_SetMessage(&players[consoleplayer], "REWIND NOT AVAILABLE", false, NULL);
        return;
    }

    gameaction = ga_rewind;
}

void G_SaveAutoKeyframe(void)
{
    const int interval_tics = RewindIntervalTics();
    const int time = I_GetTime();
    keyframe_t *keyframe;

    if (!crl_rewind_auto || disable_rewind || gamestate != GS_LEVEL
     || netgame || demoplayback || demorecording
     || MenuActive || askforquit || paused)
    {
        return;
    }

    if (!RewindQueueIsEmpty() && leveltime % interval_tics != 0)
    {
        return;
    }

    keyframe = SaveKeyframe();

    if (keyframe == NULL)
    {
        disable_rewind = true;
        CT_SetMessage(&players[consoleplayer], "REWIND DISABLED", false, NULL);
        return;
    }

    PushKeyframe(keyframe);

    if (RewindTimeout() > 0
     && (I_GetTime() - time) * 1000 / TICRATE > RewindTimeout())
    {
        disable_rewind = true;
        CT_SetMessage(&players[consoleplayer],
                      "SLOW KEYFRAMING: REWIND DISABLED", false, NULL);
    }
}

void G_LoadAutoKeyframe(void)
{
    const int interval_tics = RewindIntervalTics();
    keyframe_t *elem;
    keyframe_t *keyframe;

    gameaction = ga_nothing;

    if (RewindQueueIsEmpty())
    {
        CT_SetMessage(&players[consoleplayer], "NO REWIND KEYFRAMES", false, NULL);
        return;
    }

    elem = queue_top;

    while (elem != NULL && gametic - elem->tic < interval_tics)
    {
        elem = elem->next;
    }

    if (elem == NULL)
    {
        CT_SetMessage(&players[consoleplayer], "NO EARLIER KEYFRAME", false, NULL);
        return;
    }

    while (queue_top != elem)
    {
        keyframe_t *skipped = PopKeyframe();

        FreeKeyframe(skipped);
    }

    keyframe = PopKeyframe();

    if (LoadKeyframe(keyframe))
    {
        CT_SetMessage(&players[consoleplayer], "Restored key frame", false, NULL);
    }

    if (RewindQueueIsEmpty())
    {
        PushKeyframe(keyframe);
    }
    else
    {
        FreeKeyframe(keyframe);
    }
}

void G_ResetRewind(boolean force)
{
    disable_rewind = false;

    if (force && !rewind_restoring)
    {
        FreeKeyframeQueue();
    }
}

boolean G_RewindIsRestoring(void)
{
    return rewind_restoring;
}
