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
#include <string.h>

#include "h2def.h"
#include "ct_chat.h"
#include "g_rewind.h"
#include "i_system.h"
#include "i_timer.h"
#include "p_local.h"
#include "s_sound.h"

#include "id_vars.h"

typedef enum
{
    KEYFRAME_FULL,
    KEYFRAME_DELTA
} keyframe_kind_t;

typedef struct keyframe_s
{
    keyframe_kind_t kind;
    byte *data;
    size_t size;
    int tic;
    int delta_tics;
    struct keyframe_s *next;
    struct keyframe_s *prev;
} keyframe_t;

static keyframe_t *queue_top;
static keyframe_t *queue_tail;
static int queue_count;
static boolean disable_rewind;
static boolean rewind_restoring;
static int rewind_frames_since_full;
static int rewind_save_cooldown_tics;

static ticcmd_t *rewind_cmd_history;
static int rewind_cmd_history_size;
static int rewind_cmd_history_count;
static int rewind_cmd_history_head;

// [PN] Store one full keyframe each N autosaves, intermediate keyframes are deltas.
#define REWIND_FULL_STRIDE 4

static boolean RewindQueueIsEmpty(void)
{
    return queue_top == NULL;
}

static void StopActiveSounds(void)
{
    S_StopAllSound();
    SN_StopAllSequences();
}

static boolean RewindAllowedGamestate(void)
{
    return gamestate == GS_LEVEL
        || gamestate == GS_INTERMISSION
        || gamestate == GS_FINALE;
}

static int RewindIntervalTics(void)
{
    return TICRATE * BETWEEN(1, 600, rewind_interval);
}

static int RewindDepth(void)
{
    return BETWEEN(10, 600, rewind_depth);
}

static int RewindTimeout(void)
{
    return BETWEEN(0, 25, rewind_timeout);
}

static void ClearCommandHistory(void)
{
    rewind_cmd_history_count = 0;
    rewind_cmd_history_head = 0;
}

static boolean EnsureCommandHistory(const int interval_tics)
{
    if (interval_tics <= 0)
    {
        return false;
    }

    if (rewind_cmd_history_size == interval_tics)
    {
        return true;
    }

    free(rewind_cmd_history);
    rewind_cmd_history = calloc(interval_tics, sizeof(*rewind_cmd_history));

    if (rewind_cmd_history == NULL)
    {
        rewind_cmd_history_size = 0;
        ClearCommandHistory();
        return false;
    }

    rewind_cmd_history_size = interval_tics;
    ClearCommandHistory();
    return true;
}

static void RecordCommand(const ticcmd_t *cmd)
{
    if (rewind_cmd_history == NULL || rewind_cmd_history_size <= 0)
    {
        return;
    }

    rewind_cmd_history[rewind_cmd_history_head] = *cmd;
    rewind_cmd_history_head = (rewind_cmd_history_head + 1) % rewind_cmd_history_size;

    if (rewind_cmd_history_count < rewind_cmd_history_size)
    {
        ++rewind_cmd_history_count;
    }
}

static void CopyRecentCommands(ticcmd_t *dest, const int count)
{
    const int tail = rewind_cmd_history_head - count;

    if (tail >= 0)
    {
        // [PN] Data is contiguous in memory.
        memcpy(dest, &rewind_cmd_history[tail], count * sizeof(ticcmd_t));
    }
    else
    {
        // [PN] Buffer wrap-around.
        const int first_chunk_size = -tail;
        const int second_chunk_size = rewind_cmd_history_head;

        memcpy(dest,
               &rewind_cmd_history[rewind_cmd_history_size + tail],
               first_chunk_size * sizeof(ticcmd_t));
        memcpy(dest + first_chunk_size,
               rewind_cmd_history,
               second_chunk_size * sizeof(ticcmd_t));
    }
}

static void FreeKeyframe(keyframe_t *keyframe)
{
    if (keyframe == NULL)
    {
        return;
    }

    if (keyframe->kind == KEYFRAME_FULL)
    {
        free(keyframe->data);
    }

    free(keyframe);
}

static void RemoveTailKeyframe(void)
{
    keyframe_t *oldtail;

    if (queue_tail == NULL)
    {
        return;
    }

    oldtail = queue_tail;
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

static void PushKeyframe(keyframe_t *keyframe)
{
    while (queue_count >= RewindDepth())
    {
        RemoveTailKeyframe();
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

    // [PN] Keep queue valid for delta restore: oldest keyframe must be FULL.
    while (queue_tail != NULL && queue_tail->kind != KEYFRAME_FULL)
    {
        RemoveTailKeyframe();
    }
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

static keyframe_t *SaveFullKeyframe(void)
{
    keyframe_t *const keyframe = calloc(1, sizeof(*keyframe));

    if (keyframe == NULL)
    {
        return NULL;
    }

    keyframe->kind = KEYFRAME_FULL;

    if (!SV_SaveRewind(&keyframe->data, &keyframe->size))
    {
        FreeKeyframe(keyframe);
        return NULL;
    }

    keyframe->tic = gametic;

    return keyframe;
}

static keyframe_t *SaveDeltaKeyframe(const int interval_tics)
{
    if (rewind_cmd_history_count < interval_tics)
    {
        return NULL;
    }

    const size_t data_size = (size_t)interval_tics * sizeof(ticcmd_t);
    const size_t total_size = sizeof(keyframe_t) + data_size;

    // [PN] Single allocation: header + data in one contiguous block.
    keyframe_t *const keyframe = calloc(1, total_size);

    if (keyframe == NULL)
    {
        return NULL;
    }

    keyframe->kind = KEYFRAME_DELTA;
    keyframe->delta_tics = interval_tics;
    keyframe->size = data_size;
    // [PN] Data buffer lives directly after the struct.
    keyframe->data = (byte *)(keyframe + 1);

    // [PN] Copy delta commands directly into the tail-allocated buffer.
    ticcmd_t *const delta_cmds = (ticcmd_t *)keyframe->data;
    CopyRecentCommands(delta_cmds, interval_tics);

    keyframe->tic = gametic;

    return keyframe;
}

static boolean LoadFullKeyframe(const keyframe_t *keyframe)
{
    const boolean old_rewind_restoring = rewind_restoring;
    boolean result;

    // [PN] Drop currently playing channels before restore to avoid sound layering.
    StopActiveSounds();

    rewind_restoring = true;
    result = SV_LoadRewind(keyframe->data, keyframe->size);
    rewind_restoring = old_rewind_restoring;

    // [PN] Ensure no stale one-shot SFX channels survive restore.
    S_StopAllSound();

    return result;
}

static boolean ReplayDeltaCommands(const ticcmd_t *cmds, const int count)
{
    int i;
    const boolean old_menuactive = MenuActive;
    const int old_paused = paused;
    const boolean old_rewind_restoring = rewind_restoring;

    if (cmds == NULL || count <= 0)
    {
        return true;
    }

    // [PN] Replay must always advance logic ticks, regardless of pause/menu flags.
    rewind_restoring = true;
    MenuActive = false;
    paused = 0;

    // [JN] Save only consoleplayer's original command
    const ticcmd_t old_console_cmd = players[consoleplayer].cmd;

    for (i = 0; i < count; ++i)
    {
        players[consoleplayer].cmd = cmds[i];
        P_Ticker();
    }

    // [JN] Restore original command
    players[consoleplayer].cmd = old_console_cmd;

    // [PN] Delta replay can start many one-shot SFX in one frame; clear stacked channels.
    S_StopAllSound();

    rewind_restoring = old_rewind_restoring;
    MenuActive = old_menuactive;
    paused = old_paused;

    return true;
}

static boolean LoadDeltaKeyframe(const keyframe_t *keyframe)
{
    const keyframe_t *base;
    const keyframe_t *it;

    base = keyframe;

    // [PN] Newest -> oldest goes through "next", find nearest older FULL.
    while (base != NULL && base->kind != KEYFRAME_FULL)
    {
        base = base->next;
    }

    if (base == NULL)
    {
        return false;
    }

    if (!LoadFullKeyframe(base))
    {
        return false;
    }

    // [PN] Replay should rebuild sound state from ticks, not stack on restored base.
    StopActiveSounds();

    // [PN] Replay forward in time: FULL base -> newer deltas up to target keyframe.
    for (it = base->prev; it != NULL; it = it->prev)
    {
        if (it->kind == KEYFRAME_DELTA)
        {
            if (!ReplayDeltaCommands((const ticcmd_t *)it->data, it->delta_tics))
            {
                return false;
            }
        }

        if (it == keyframe)
        {
            return true;
        }
    }

    return false;
}

static boolean LoadKeyframe(const keyframe_t *keyframe)
{
    if (keyframe->kind == KEYFRAME_FULL)
    {
        return LoadFullKeyframe(keyframe);
    }

    return LoadDeltaKeyframe(keyframe);
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
    if (!rewind_enable || netgame || demoplayback || demorecording || !RewindAllowedGamestate())
    {
        CT_SetMessage(&players[consoleplayer], "REWIND NOT AVAILABLE", false, NULL);
        return;
    }

    gameaction = ga_rewind;
}

void G_SaveAutoKeyframe(void)
{
    const int interval_tics = RewindIntervalTics();
    const int timeout_ms = RewindTimeout();
    keyframe_t *keyframe = NULL;
    boolean save_full;
    const uint64_t start_time = I_GetTimeUS();

    if (!rewind_enable || disable_rewind || gamestate != GS_LEVEL
     || netgame || demoplayback || demorecording
     || MenuActive || askforquit || paused || rewind_restoring)
    {
        return;
    }

    if (!EnsureCommandHistory(interval_tics))
    {
        disable_rewind = true;
        CT_SetMessage(&players[consoleplayer], "REWIND DISABLED", false, NULL);
        return;
    }

    RecordCommand(&players[consoleplayer].cmd);

    // [PN] Prevent immediate duplicate keyframe right after rewind restore.
    if (rewind_save_cooldown_tics > 0)
    {
        --rewind_save_cooldown_tics;
        return;
    }

    if (!RewindQueueIsEmpty() && leveltime % interval_tics != 0)
    {
        return;
    }

    save_full = RewindQueueIsEmpty()
             || rewind_frames_since_full >= REWIND_FULL_STRIDE - 1;

    if (save_full)
    {
        keyframe = SaveFullKeyframe();
    }
    else
    {
        keyframe = SaveDeltaKeyframe(interval_tics);
    }

    if (keyframe == NULL && !save_full)
    {
        keyframe = SaveFullKeyframe();
        save_full = true;
    }

    if (keyframe == NULL)
    {
        disable_rewind = true;
        CT_SetMessage(&players[consoleplayer], "REWIND DISABLED", false, NULL);
        return;
    }

    if (keyframe->kind == KEYFRAME_FULL)
    {
        rewind_frames_since_full = 0;
    }
    else
    {
        ++rewind_frames_since_full;
    }

    PushKeyframe(keyframe);

    // [PN] Timeout control for expensive FULL keyframes only.
    if (timeout_ms > 0 && keyframe->kind == KEYFRAME_FULL)
    {
        const uint64_t elapsed_us = I_GetTimeUS() - start_time;

        if (elapsed_us > (uint64_t)timeout_ms * 1000)
        {
            disable_rewind = true;
            CT_SetMessage(&players[consoleplayer], "SLOW KEY FRAMING: REWIND DISABLED", false, NULL);
        }
    }
}

void G_LoadAutoKeyframe(void)
{
    const int interval_tics = RewindIntervalTics();
    keyframe_t *keyframe;

    gameaction = ga_nothing;

    if (RewindQueueIsEmpty())
    {
        CT_SetMessage(&players[consoleplayer], "NO REWIND KEY FRAMES", false, NULL);
        return;
    }

    // [PN] One press = one step back in stored rewind queue.
    keyframe = queue_top;

    if (LoadKeyframe(keyframe))
    {
        // [JN] Remove the restored frame from the queue; it is no longer needed.
        PopKeyframe();
        FreeKeyframe(keyframe);

        // [PN] After restore/replay, force next autosave to be FULL.
        rewind_frames_since_full = REWIND_FULL_STRIDE - 1;
        rewind_save_cooldown_tics = interval_tics;
        ClearCommandHistory();
        CT_SetMessage(&players[consoleplayer], "RESTORED KEY FRAME", false, NULL);
    }
}

void G_ResetRewind(boolean force)
{
    disable_rewind = false;
    rewind_save_cooldown_tics = 0;

    if (force && !rewind_restoring)
    {
        FreeKeyframeQueue();
        rewind_frames_since_full = 0;
        ClearCommandHistory();
    }
}

boolean G_RewindIsRestoring(void)
{
    return rewind_restoring;
}
