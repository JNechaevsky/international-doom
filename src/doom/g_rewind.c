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

#include <stdlib.h>

#include "ct_chat.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_rewind.h"
#include "i_system.h"
#include "i_timer.h"
#include "mn_menu.h"
#include "p_local.h"

#include "id_vars.h"

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
    return TICRATE * BETWEEN(1, 600, crl_rewind_interval);
}

static int RewindDepth(void)
{
    return BETWEEN(10, 600, crl_rewind_depth);
}

static int RewindTimeout(void)
{
    return BETWEEN(0, 25, crl_rewind_timeout);
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

    if (keyframe == NULL)
    {
        return NULL;
    }

    P_OpenMemorySaveGame();
    P_WriteSaveGameHeader("REWIND");
    P_ArchiveGameplaySettings();
    P_ArchivePlayers();
    P_ArchiveWorld();
    P_ArchiveThinkers();
    P_ArchiveSpecials();
    P_WriteSaveGameEOF();
    P_ArchiveTotalTimes();
    P_ArchiveOldSpecials();
    P_ArchiveAutomap();

    if (!P_CloseMemorySaveGame(&keyframe->data, &keyframe->size))
    {
        FreeKeyframe(keyframe);
        return NULL;
    }

    keyframe->tic = gametic;
    return keyframe;
}

static boolean LoadKeyframe(const keyframe_t *keyframe)
{
    int savedleveltime;

    P_OpenMemoryLoadGame(keyframe->data, keyframe->size);

    if (!P_ReadSaveGameHeader())
    {
        P_CloseMemoryLoadGame();
        return false;
    }

    P_UnArchiveGameplaySettings();
    savedleveltime = leveltime;

    rewind_restoring = true;
    G_InitNew(gameskill, gameepisode, gamemap);
    rewind_restoring = false;
    wipegamestate = gamestate;

    leveltime = savedleveltime;

    P_UnArchivePlayers();
    P_UnArchiveWorld();
    P_UnArchiveThinkers();
    P_UnArchiveSpecials();

    if (!P_ReadSaveGameEOF())
    {
        P_CloseMemoryLoadGame();
        I_Error("Bad rewind key frame");
    }

    P_UnArchiveTotalTimes();
    P_UnArchiveOldSpecials();
    P_UnArchiveAutomap();
    P_RestoreTargets();
    P_CloseMemoryLoadGame();

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
    if (!crl_rewind_auto || netgame || demoplayback || demorecording || !RewindAllowedGamestate())
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
     || netgame || demoplayback || demorecording || menuactive || paused)
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
        CT_SetMessage(&players[consoleplayer], "SLOW KEY FRAMING: REWIND DISABLED", false, NULL);
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
        CT_SetMessage(&players[consoleplayer], "NO REWIND KEY FRAMES", false, NULL);
        return;
    }

    elem = queue_top;

    while (elem != NULL && gametic - elem->tic < interval_tics)
    {
        elem = elem->next;
    }

    if (elem == NULL)
    {
        CT_SetMessage(&players[consoleplayer], "NO EARLIER KEY FRAME", false, NULL);
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
        CT_SetMessage(&players[consoleplayer], "RESTORED KEY FRAME", false, NULL);
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
