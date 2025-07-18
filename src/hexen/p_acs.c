//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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


// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "m_misc.h"
#include "m_random.h"
#include "s_sound.h"
#include "i_swap.h"
#include "i_system.h"
#include "p_local.h"
#include "ct_chat.h"

// MACROS ------------------------------------------------------------------

#define MAX_SCRIPT_ARGS 3
#define SCRIPT_CONTINUE 0
#define SCRIPT_STOP 1
#define SCRIPT_TERMINATE 2
#define OPEN_SCRIPTS_BASE 1000
#define PRINT_BUFFER_SIZE 256
#define GAME_SINGLE_PLAYER 0
#define GAME_NET_COOPERATIVE 1
#define GAME_NET_DEATHMATCH 2
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2

// TYPES -------------------------------------------------------------------

typedef PACKED_STRUCT (
{
    int marker;
    int infoOffset;
    int code;
}) acsHeader_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void StartOpenACS(int number, int infoIndex, int offset);
static void ScriptFinished(int number);
static boolean TagBusy(int tag);
static boolean AddToACSStore(int map, int number, const byte *args);
static int GetACSIndex(int number);
static void Push(int value);
static int Pop(void);
static int Top(void);
static void Drop(void);

static int CmdNOP(void);
static int CmdTerminate(void);
static int CmdSuspend(void);
static int CmdPushNumber(void);
static int CmdLSpec1(void);
static int CmdLSpec2(void);
static int CmdLSpec3(void);
static int CmdLSpec4(void);
static int CmdLSpec5(void);
static int CmdLSpec1Direct(void);
static int CmdLSpec2Direct(void);
static int CmdLSpec3Direct(void);
static int CmdLSpec4Direct(void);
static int CmdLSpec5Direct(void);
static int CmdAdd(void);
static int CmdSubtract(void);
static int CmdMultiply(void);
static int CmdDivide(void);
static int CmdModulus(void);
static int CmdEQ(void);
static int CmdNE(void);
static int CmdLT(void);
static int CmdGT(void);
static int CmdLE(void);
static int CmdGE(void);
static int CmdAssignScriptVar(void);
static int CmdAssignMapVar(void);
static int CmdAssignWorldVar(void);
static int CmdPushScriptVar(void);
static int CmdPushMapVar(void);
static int CmdPushWorldVar(void);
static int CmdAddScriptVar(void);
static int CmdAddMapVar(void);
static int CmdAddWorldVar(void);
static int CmdSubScriptVar(void);
static int CmdSubMapVar(void);
static int CmdSubWorldVar(void);
static int CmdMulScriptVar(void);
static int CmdMulMapVar(void);
static int CmdMulWorldVar(void);
static int CmdDivScriptVar(void);
static int CmdDivMapVar(void);
static int CmdDivWorldVar(void);
static int CmdModScriptVar(void);
static int CmdModMapVar(void);
static int CmdModWorldVar(void);
static int CmdIncScriptVar(void);
static int CmdIncMapVar(void);
static int CmdIncWorldVar(void);
static int CmdDecScriptVar(void);
static int CmdDecMapVar(void);
static int CmdDecWorldVar(void);
static int CmdGoto(void);
static int CmdIfGoto(void);
static int CmdDrop(void);
static int CmdDelay(void);
static int CmdDelayDirect(void);
static int CmdRandom(void);
static int CmdRandomDirect(void);
static int CmdThingCount(void);
static int CmdThingCountDirect(void);
static int CmdTagWait(void);
static int CmdTagWaitDirect(void);
static int CmdPolyWait(void);
static int CmdPolyWaitDirect(void);
static int CmdChangeFloor(void);
static int CmdChangeFloorDirect(void);
static int CmdChangeCeiling(void);
static int CmdChangeCeilingDirect(void);
static int CmdRestart(void);
static int CmdAndLogical(void);
static int CmdOrLogical(void);
static int CmdAndBitwise(void);
static int CmdOrBitwise(void);
static int CmdEorBitwise(void);
static int CmdNegateLogical(void);
static int CmdLShift(void);
static int CmdRShift(void);
static int CmdUnaryMinus(void);
static int CmdIfNotGoto(void);
static int CmdLineSide(void);
static int CmdScriptWait(void);
static int CmdScriptWaitDirect(void);
static int CmdClearLineSpecial(void);
static int CmdCaseGoto(void);
static int CmdBeginPrint(void);
static int CmdEndPrint(void);
static int CmdPrintString(void);
static int CmdPrintNumber(void);
static int CmdPrintCharacter(void);
static int CmdPlayerCount(void);
static int CmdGameType(void);
static int CmdGameSkill(void);
static int CmdTimer(void);
static int CmdSectorSound(void);
static int CmdAmbientSound(void);
static int CmdSoundSequence(void);
static int CmdSetLineTexture(void);
static int CmdSetLineBlocking(void);
static int CmdSetLineSpecial(void);
static int CmdThingSound(void);
static int CmdEndPrintBold(void);

static void ThingCount(int type, int tid);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int ACScriptCount;
byte *ActionCodeBase;
static int ActionCodeSize;
acsInfo_t *ACSInfo;
int MapVars[MAX_ACS_MAP_VARS];
int WorldVars[MAX_ACS_WORLD_VARS];
acsstore_t ACSStore[MAX_ACS_STORE + 1]; // +1 for termination marker

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char EvalContext[64];
static acs_t *ACScript;
static unsigned int PCodeOffset;
static byte SpecArgs[8];
static int ACStringCount;
static char **ACStrings;
static char PrintBuffer[PRINT_BUFFER_SIZE];
static acs_t *NewScript;

static int (*PCodeCmds[]) (void) =
{
        CmdNOP,
        CmdTerminate,
        CmdSuspend,
        CmdPushNumber,
        CmdLSpec1,
        CmdLSpec2,
        CmdLSpec3,
        CmdLSpec4,
        CmdLSpec5,
        CmdLSpec1Direct,
        CmdLSpec2Direct,
        CmdLSpec3Direct,
        CmdLSpec4Direct,
        CmdLSpec5Direct,
        CmdAdd,
        CmdSubtract,
        CmdMultiply,
        CmdDivide,
        CmdModulus,
        CmdEQ,
        CmdNE,
        CmdLT,
        CmdGT,
        CmdLE,
        CmdGE,
        CmdAssignScriptVar,
        CmdAssignMapVar,
        CmdAssignWorldVar,
        CmdPushScriptVar,
        CmdPushMapVar,
        CmdPushWorldVar,
        CmdAddScriptVar,
        CmdAddMapVar,
        CmdAddWorldVar,
        CmdSubScriptVar,
        CmdSubMapVar,
        CmdSubWorldVar,
        CmdMulScriptVar,
        CmdMulMapVar,
        CmdMulWorldVar,
        CmdDivScriptVar,
        CmdDivMapVar,
        CmdDivWorldVar,
        CmdModScriptVar,
        CmdModMapVar,
        CmdModWorldVar,
        CmdIncScriptVar,
        CmdIncMapVar,
        CmdIncWorldVar,
        CmdDecScriptVar,
        CmdDecMapVar,
        CmdDecWorldVar,
        CmdGoto,
        CmdIfGoto,
        CmdDrop,
        CmdDelay,
        CmdDelayDirect,
        CmdRandom,
        CmdRandomDirect,
        CmdThingCount,
        CmdThingCountDirect,
        CmdTagWait,
        CmdTagWaitDirect,
        CmdPolyWait,
        CmdPolyWaitDirect,
        CmdChangeFloor,
        CmdChangeFloorDirect,
        CmdChangeCeiling,
        CmdChangeCeilingDirect,
        CmdRestart,
        CmdAndLogical,
        CmdOrLogical,
        CmdAndBitwise,
        CmdOrBitwise,
        CmdEorBitwise,
        CmdNegateLogical,
        CmdLShift,
        CmdRShift,
        CmdUnaryMinus,
        CmdIfNotGoto,
        CmdLineSide,
        CmdScriptWait,
        CmdScriptWaitDirect,
        CmdClearLineSpecial,
        CmdCaseGoto,
        CmdBeginPrint,
        CmdEndPrint,
        CmdPrintString,
        CmdPrintNumber,
        CmdPrintCharacter,
        CmdPlayerCount,
        CmdGameType,
        CmdGameSkill,
        CmdTimer,
        CmdSectorSound,
        CmdAmbientSound,
        CmdSoundSequence,
        CmdSetLineTexture,
        CmdSetLineBlocking,
        CmdSetLineSpecial,
        CmdThingSound,
        CmdEndPrintBold,
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ACSAssert
//
// Check that the given condition evaluates to true. If it does not, exit
// with an I_Error() printing the given message.
//
//==========================================================================

static void ACSAssert(int condition, const char *fmt, ...)
{
    char buf[128];
    va_list args;

    if (condition)
    {
        return;
    }

    va_start(args, fmt);
    M_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    I_Error("ACS assertion failure: in %s: %s", EvalContext, buf);
}

//==========================================================================
//
// ReadCodeInt
//
// Read a 32-bit value from the loaded ACS lump at the location pointed to
// by PCodeOffset, advancing PCodeOffset to the next value in the process.
//
//==========================================================================

static int ReadCodeInt(void)
{
    int result;
    int *ptr;

    ACSAssert(PCodeOffset + 3 < ActionCodeSize,
              "unexpectedly reached end of ACS lump");

    ptr = (int *) (ActionCodeBase + PCodeOffset);
    result = LONG(*ptr);
    PCodeOffset += 4;

    return result;
}

//==========================================================================
//
// ReadScriptVar
//
// Read a script variable index as an immediate value, validating the
// result is a valid script variable number.
//
//==========================================================================

static int ReadScriptVar(void)
{
    int var = ReadCodeInt();
    ACSAssert(var >= 0, "negative script variable: %d < 0", var);
    ACSAssert(var < MAX_ACS_SCRIPT_VARS,
              "invalid script variable: %d >= %d", var, MAX_ACS_SCRIPT_VARS);
    return var;
}

//==========================================================================
//
// ReadMapVar
//
// Read a map variable index as an immediate value, validating the
// result is a valid map variable number.
//
//==========================================================================

static int ReadMapVar(void)
{
    int var = ReadCodeInt();
    ACSAssert(var >= 0, "negative map variable: %d < 0", var);
    ACSAssert(var < MAX_ACS_MAP_VARS,
              "invalid map variable: %d >= %d", var, MAX_ACS_MAP_VARS);
    return var;
}

//==========================================================================
//
// ReadWorldVar
//
// Read a world variable index as an immediate value, validating the
// result is a valid world variable number.
//
//==========================================================================

static int ReadWorldVar(void)
{
    int var = ReadCodeInt();
    ACSAssert(var >= 0, "negative world variable: %d < 0", var);
    ACSAssert(var < MAX_ACS_WORLD_VARS,
              "invalid world variable: %d >= %d", var, MAX_ACS_WORLD_VARS);
    return var;
}

//==========================================================================
//
// StringLookup
//
// Look up the given string in the strings table by index, validating that
// it is a valid string index.
//
//==========================================================================

static char *StringLookup(int string_index)
{
    ACSAssert(string_index >= 0,
              "negative string index: %d < 0", string_index);
    ACSAssert(string_index < ACStringCount,
              "invalid string index: %d >= %d", string_index, ACStringCount);
    return ACStrings[string_index];
}

//==========================================================================
//
// ReadOffset
//
// Read a lump offset value, validating that it is an offset within the
// range of the lump.
//
//==========================================================================

static int ReadOffset(void)
{
    int offset = ReadCodeInt();
    ACSAssert(offset >= 0, "negative lump offset %d", offset);
    ACSAssert(offset < ActionCodeSize, "invalid lump offset: %d >= %d",
              offset, ActionCodeSize);
    return offset;
}

//==========================================================================
//
// P_LoadACScripts
//
//==========================================================================

void P_LoadACScripts(int lump)
{
    int i, offset;
    acsHeader_t *header;
    acsInfo_t *info;

    ActionCodeBase = W_CacheLumpNum(lump, PU_LEVEL);
    ActionCodeSize = W_LumpLength(lump);

    M_snprintf(EvalContext, sizeof(EvalContext),
               "header parsing of lump #%d", lump);

    header = (acsHeader_t *) ActionCodeBase;
    PCodeOffset = LONG(header->infoOffset);

    ACScriptCount = ReadCodeInt();

    if (ACScriptCount == 0)
    {                           // Empty behavior lump
        return;
    }

    ACSInfo = Z_Malloc(ACScriptCount * sizeof(acsInfo_t), PU_LEVEL, 0);
    memset(ACSInfo, 0, ACScriptCount * sizeof(acsInfo_t));
    for (i = 0, info = ACSInfo; i < ACScriptCount; i++, info++)
    {
        info->number = ReadCodeInt();
        info->offset = ReadOffset();
        info->argCount = ReadCodeInt();

        if (info->argCount > MAX_SCRIPT_ARGS)
        {
            fprintf(stderr, "Warning: ACS script #%i has %i arguments, more "
                            "than the maximum of %i. Enforcing limit.\n"
                            "If you are seeing this message, please report "
                            "the name of the WAD where you saw it.\n",
                            i, info->argCount, MAX_SCRIPT_ARGS);
            info->argCount = MAX_SCRIPT_ARGS;
        }

        if (info->number >= OPEN_SCRIPTS_BASE)
        {                       // Auto-activate
            info->number -= OPEN_SCRIPTS_BASE;
            StartOpenACS(info->number, i, info->offset);
            info->state = ASTE_RUNNING;
        }
        else
        {
            info->state = ASTE_INACTIVE;
        }
    }

    ACStringCount = ReadCodeInt();
    ACSAssert(ACStringCount >= 0, "negative string count %d", ACStringCount);
    ACStrings = Z_Malloc(ACStringCount * sizeof(char *), PU_LEVEL, NULL);

    for (i=0; i<ACStringCount; ++i)
    {
        offset = ReadOffset();
        ACStrings[i] = (char *) ActionCodeBase + offset;
        ACSAssert(memchr(ACStrings[i], '\0', ActionCodeSize - offset) != NULL,
                  "string %d missing terminating NUL", i);
    }

    memset(MapVars, 0, sizeof(MapVars));
}

//==========================================================================
//
// StartOpenACS
//
//==========================================================================

static void StartOpenACS(int number, int infoIndex, int offset)
{
    acs_t *script;

    script = Z_Malloc(sizeof(acs_t), PU_LEVSPEC, 0);
    memset(script, 0, sizeof(acs_t));
    script->number = number;

    // World objects are allotted 1 second for initialization
    script->delayCount = 35;

    script->infoIndex = infoIndex;
    script->ip = offset;
    script->thinker.function = T_InterpretACS;
    P_AddThinker(&script->thinker);
}

//==========================================================================
//
// P_CheckACSStore
//
// Scans the ACS store and executes all scripts belonging to the current
// map.
//
//==========================================================================

void P_CheckACSStore(void)
{
    acsstore_t *store;

    for (store = ACSStore; store->map != 0; store++)
    {
        if (store->map == gamemap)
        {
            P_StartACS(store->script, 0, store->args, NULL, NULL, 0);
            if (NewScript)
            {
                NewScript->delayCount = 35;
            }
            store->map = -1;
        }
    }
}

//==========================================================================
//
// P_StartACS
//
// Start an ACS script. The 'args' array should be at least MAX_SCRIPT_ARGS
// elements in length.
//
//==========================================================================

static char ErrorMsg[128];

boolean P_StartACS(int number, int map, const byte *args, mobj_t * activator,
                   line_t * line, int side)
{
    int i;
    acs_t *script;
    int infoIndex;
    aste_t *statePtr;

    NewScript = NULL;
    if (map && map != gamemap)
    {                           // Add to the script store
        return AddToACSStore(map, number, args);
    }
    infoIndex = GetACSIndex(number);
    if (infoIndex == -1)
    {                           // Script not found
        //I_Error("P_StartACS: Unknown script number %d", number);
        M_snprintf(ErrorMsg, sizeof(ErrorMsg),
                   // [JN] Fix vanilla bug:
                   // Replace "_" with "[" because of Raven font system.
                   "P[STARTACS ERROR: UNKNOWN SCRIPT %d", number);
        CT_SetMessage(&players[consoleplayer], ErrorMsg, true, NULL);
    }
    statePtr = &ACSInfo[infoIndex].state;
    if (*statePtr == ASTE_SUSPENDED)
    {                           // Resume a suspended script
        *statePtr = ASTE_RUNNING;
        return true;
    }
    if (*statePtr != ASTE_INACTIVE)
    {                           // Script is already executing
        return false;
    }
    script = Z_Malloc(sizeof(acs_t), PU_LEVSPEC, 0);
    memset(script, 0, sizeof(acs_t));
    script->number = number;
    script->infoIndex = infoIndex;
    script->activator = activator;
    script->line = line;
    script->side = side;
    script->ip = ACSInfo[infoIndex].offset;
    script->thinker.function = T_InterpretACS;
    for (i = 0; i < MAX_SCRIPT_ARGS && i < ACSInfo[infoIndex].argCount; i++)
    {
        script->vars[i] = args[i];
    }
    *statePtr = ASTE_RUNNING;
    P_AddThinker(&script->thinker);
    NewScript = script;
    return true;
}

//==========================================================================
//
// AddToACSStore
//
//==========================================================================

static boolean AddToACSStore(int map, int number, const byte *args)
{
    int i;
    int index;

    index = -1;
    for (i = 0; ACSStore[i].map != 0; i++)
    {
        if (ACSStore[i].script == number && ACSStore[i].map == map)
        {                       // Don't allow duplicates
            return false;
        }
        if (index == -1 && ACSStore[i].map == -1)
        {                       // Remember first empty slot
            index = i;
        }
    }
    if (index == -1)
    {                           // Append required
        if (i == MAX_ACS_STORE)
        {
            I_Error("AddToACSStore: MAX_ACS_STORE (%d) exceeded.",
                    MAX_ACS_STORE);
        }
        index = i;
        ACSStore[index + 1].map = 0;
    }
    ACSStore[index].map = map;
    ACSStore[index].script = number;
    memcpy(ACSStore[index].args, args, MAX_SCRIPT_ARGS);
    return true;
}

//==========================================================================
//
// P_StartLockedACS
//
//==========================================================================


boolean P_StartLockedACS(line_t * line, const byte * args, mobj_t * mo, int side)
{
    int i;
    int lock;
    byte newArgs[5];
    char LockedBuffer[80];

    lock = args[4];
    if (!mo->player)
    {
        return false;
    }
    if (lock)
    {
        if (!(mo->player->keys & (1 << (lock - 1))))
        {
            M_snprintf(LockedBuffer, sizeof(LockedBuffer),
                       "YOU NEED THE %s\n", TextKeyMessages[lock - 1]);
            CT_SetMessage(mo->player, LockedBuffer, true, NULL);
            S_StartSound(mo, SFX_DOOR_LOCKED);
            return false;
        }
    }
    for (i = 0; i < 4; i++)
    {
        newArgs[i] = args[i];
    }
    newArgs[4] = 0;
    return P_StartACS(newArgs[0], newArgs[1], &newArgs[2], mo, line, side);
}

//==========================================================================
//
// P_TerminateACS
//
//==========================================================================

boolean P_TerminateACS(int number, int map)
{
    int infoIndex;

    infoIndex = GetACSIndex(number);
    if (infoIndex == -1)
    {                           // Script not found
        return false;
    }
    if (ACSInfo[infoIndex].state == ASTE_INACTIVE
        || ACSInfo[infoIndex].state == ASTE_TERMINATING)
    {                           // States that disallow termination
        return false;
    }
    ACSInfo[infoIndex].state = ASTE_TERMINATING;
    return true;
}

//==========================================================================
//
// P_SuspendACS
//
//==========================================================================

boolean P_SuspendACS(int number, int map)
{
    int infoIndex;

    infoIndex = GetACSIndex(number);
    if (infoIndex == -1)
    {                           // Script not found
        return false;
    }
    if (ACSInfo[infoIndex].state == ASTE_INACTIVE
        || ACSInfo[infoIndex].state == ASTE_SUSPENDED
        || ACSInfo[infoIndex].state == ASTE_TERMINATING)
    {                           // States that disallow suspension
        return false;
    }
    ACSInfo[infoIndex].state = ASTE_SUSPENDED;
    return true;
}

//==========================================================================
//
// P_Init
//
//==========================================================================

void P_ACSInitNewGame(void)
{
    memset(WorldVars, 0, sizeof(WorldVars));
    memset(ACSStore, 0, sizeof(ACSStore));
}

//==========================================================================
//
// T_InterpretACS
//
//==========================================================================

void T_InterpretACS(thinker_t *thinker)
{
    acs_t *script = (acs_t *) thinker;
    int cmd;
    int action;

    if (ACSInfo[script->infoIndex].state == ASTE_TERMINATING)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        P_RemoveThinker(&ACScript->thinker);
        return;
    }
    if (ACSInfo[script->infoIndex].state != ASTE_RUNNING)
    {
        return;
    }
    if (script->delayCount)
    {
        script->delayCount--;
        return;
    }
    ACScript = script;
    PCodeOffset = ACScript->ip;

    do
    {
        M_snprintf(EvalContext, sizeof(EvalContext), "script %d @0x%x",
                   ACSInfo[script->infoIndex].number, PCodeOffset);
        cmd = ReadCodeInt();
        M_snprintf(EvalContext, sizeof(EvalContext), "script %d @0x%x, cmd=%d",
                   ACSInfo[script->infoIndex].number, PCodeOffset, cmd);
        ACSAssert(cmd >= 0, "negative ACS instruction %d", cmd);
        ACSAssert(cmd < arrlen(PCodeCmds),
                  "invalid ACS instruction %d (maybe this WAD is designed "
                  "for an advanced source port and is not vanilla "
                  "compatible)", cmd);
        action = PCodeCmds[cmd]();
    } while (action == SCRIPT_CONTINUE);

    ACScript->ip = PCodeOffset;

    if (action == SCRIPT_TERMINATE)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        P_RemoveThinker(&ACScript->thinker);
    }
}

//==========================================================================
//
// P_TagFinished
//
//==========================================================================

void P_TagFinished(int tag)
{
    int i;

    if (TagBusy(tag) == true)
    {
        return;
    }
    for (i = 0; i < ACScriptCount; i++)
    {
        if (ACSInfo[i].state == ASTE_WAITINGFORTAG
            && ACSInfo[i].waitValue == tag)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

//==========================================================================
//
// P_PolyobjFinished
//
//==========================================================================

void P_PolyobjFinished(int po)
{
    int i;

    if (PO_Busy(po) == true)
    {
        return;
    }
    for (i = 0; i < ACScriptCount; i++)
    {
        if (ACSInfo[i].state == ASTE_WAITINGFORPOLY
            && ACSInfo[i].waitValue == po)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

//==========================================================================
//
// ScriptFinished
//
//==========================================================================

static void ScriptFinished(int number)
{
    int i;

    for (i = 0; i < ACScriptCount; i++)
    {
        if (ACSInfo[i].state == ASTE_WAITINGFORSCRIPT
            && ACSInfo[i].waitValue == number)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

//==========================================================================
//
// TagBusy
//
//==========================================================================

static boolean TagBusy(int tag)
{
    int sectorIndex;

    sectorIndex = -1;
    while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
    {
        if (sectors[sectorIndex].specialdata)
        {
            return true;
        }
    }
    return false;
}

//==========================================================================
//
// GetACSIndex
//
// Returns the index of a script number.  Returns -1 if the script number
// is not found.
//
//==========================================================================

static int GetACSIndex(int number)
{
    int i;

    for (i = 0; i < ACScriptCount; i++)
    {
        if (ACSInfo[i].number == number)
        {
            return i;
        }
    }
    return -1;
}

//==========================================================================
//
// CheckACSPresent
//
// Placing Korax in a PWAD without extra steps will result in a crash in
// Vanilla because the relevant ACS scripts are not initialized
//
//==========================================================================

void CheckACSPresent(int number)
{
    if (GetACSIndex(number) == -1)
    {
        I_Error("Required ACS script %d not initialized", number);
    }
}

//==========================================================================
//
// Push
//
//==========================================================================

static void Push(int value)
{
    ACSAssert(ACScript->stackPtr < ACS_STACK_DEPTH,
              "maximum stack depth exceeded: %d >= %d",
              ACScript->stackPtr, ACS_STACK_DEPTH);
    ACScript->stack[ACScript->stackPtr++] = value;
}

//==========================================================================
//
// Pop
//
//==========================================================================

static int Pop(void)
{
    ACSAssert(ACScript->stackPtr > 0, "pop of empty stack");
    return ACScript->stack[--ACScript->stackPtr];
}

//==========================================================================
//
// Top
//
//==========================================================================

static int Top(void)
{
    ACSAssert(ACScript->stackPtr > 0, "read from top of empty stack");
    return ACScript->stack[ACScript->stackPtr - 1];
}

//==========================================================================
//
// Drop
//
//==========================================================================

static void Drop(void)
{
    ACSAssert(ACScript->stackPtr > 0, "drop on empty stack");
    ACScript->stackPtr--;
}

//==========================================================================
//
// P-Code Commands
//
//==========================================================================

static int CmdNOP(void)
{
    return SCRIPT_CONTINUE;
}

static int CmdTerminate(void)
{
    return SCRIPT_TERMINATE;
}

static int CmdSuspend(void)
{
    ACSInfo[ACScript->infoIndex].state = ASTE_SUSPENDED;
    return SCRIPT_STOP;
}

static int CmdPushNumber(void)
{
    Push(ReadCodeInt());
    return SCRIPT_CONTINUE;
}

static int CmdLSpec1(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec2(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec3(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec4(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec5(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[4] = Pop();
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec1Direct(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = ReadCodeInt();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec2Direct(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = ReadCodeInt();
    SpecArgs[1] = ReadCodeInt();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec3Direct(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = ReadCodeInt();
    SpecArgs[1] = ReadCodeInt();
    SpecArgs[2] = ReadCodeInt();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec4Direct(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = ReadCodeInt();
    SpecArgs[1] = ReadCodeInt();
    SpecArgs[2] = ReadCodeInt();
    SpecArgs[3] = ReadCodeInt();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdLSpec5Direct(void)
{
    int special;

    special = ReadCodeInt();
    SpecArgs[0] = ReadCodeInt();
    SpecArgs[1] = ReadCodeInt();
    SpecArgs[2] = ReadCodeInt();
    SpecArgs[3] = ReadCodeInt();
    SpecArgs[4] = ReadCodeInt();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line,
                         ACScript->side, ACScript->activator);
    return SCRIPT_CONTINUE;
}

static int CmdAdd(void)
{
    Push(Pop() + Pop());
    return SCRIPT_CONTINUE;
}

static int CmdSubtract(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() - operand2);
    return SCRIPT_CONTINUE;
}

static int CmdMultiply(void)
{
    Push(Pop() * Pop());
    return SCRIPT_CONTINUE;
}

static int CmdDivide(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() / operand2);
    return SCRIPT_CONTINUE;
}

static int CmdModulus(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() % operand2);
    return SCRIPT_CONTINUE;
}

static int CmdEQ(void)
{
    Push(Pop() == Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNE(void)
{
    Push(Pop() != Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLT(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() < operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGT(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() > operand2);
    return SCRIPT_CONTINUE;
}

static int CmdLE(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() <= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGE(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() >= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdAssignScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignMapVar(void)
{
    MapVars[ReadMapVar()] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignWorldVar(void)
{
    WorldVars[ReadWorldVar()] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdPushScriptVar(void)
{
    Push(ACScript->vars[ReadScriptVar()]);
    return SCRIPT_CONTINUE;
}

static int CmdPushMapVar(void)
{
    Push(MapVars[ReadMapVar()]);
    return SCRIPT_CONTINUE;
}

static int CmdPushWorldVar(void)
{
    Push(WorldVars[ReadWorldVar()]);
    return SCRIPT_CONTINUE;
}

static int CmdAddScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddMapVar(void)
{
    MapVars[ReadMapVar()] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddWorldVar(void)
{
    WorldVars[ReadWorldVar()] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubMapVar(void)
{
    MapVars[ReadMapVar()] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubWorldVar(void)
{
    WorldVars[ReadWorldVar()] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulMapVar(void)
{
    MapVars[ReadMapVar()] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulWorldVar(void)
{
    WorldVars[ReadWorldVar()] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivMapVar(void)
{
    MapVars[ReadMapVar()] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivWorldVar(void)
{
    WorldVars[ReadWorldVar()] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModScriptVar(void)
{
    ACScript->vars[ReadScriptVar()] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModMapVar(void)
{
    MapVars[ReadMapVar()] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModWorldVar(void)
{
    WorldVars[ReadWorldVar()] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdIncScriptVar(void)
{
    ++ACScript->vars[ReadScriptVar()];
    return SCRIPT_CONTINUE;
}

static int CmdIncMapVar(void)
{
    ++MapVars[ReadMapVar()];
    return SCRIPT_CONTINUE;
}

static int CmdIncWorldVar(void)
{
    ++WorldVars[ReadWorldVar()];
    return SCRIPT_CONTINUE;
}

static int CmdDecScriptVar(void)
{
    --ACScript->vars[ReadScriptVar()];
    return SCRIPT_CONTINUE;
}

static int CmdDecMapVar(void)
{
    --MapVars[ReadMapVar()];
    return SCRIPT_CONTINUE;
}

static int CmdDecWorldVar(void)
{
    --WorldVars[ReadWorldVar()];
    return SCRIPT_CONTINUE;
}

static int CmdGoto(void)
{
    PCodeOffset = ReadOffset();
    return SCRIPT_CONTINUE;
}

static int CmdIfGoto(void)
{
    int offset;

    offset = ReadOffset();

    if (Pop() != 0)
    {
        PCodeOffset = offset;
    }
    return SCRIPT_CONTINUE;
}

static int CmdDrop(void)
{
    Drop();
    return SCRIPT_CONTINUE;
}

static int CmdDelay(void)
{
    ACScript->delayCount = Pop();
    return SCRIPT_STOP;
}

static int CmdDelayDirect(void)
{
    ACScript->delayCount = ReadCodeInt();
    return SCRIPT_STOP;
}

static int CmdRandom(void)
{
    int low;
    int high;

    high = Pop();
    low = Pop();
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdRandomDirect(void)
{
    int low;
    int high;

    low = ReadCodeInt();
    high = ReadCodeInt();
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdThingCount(void)
{
    int tid;

    tid = Pop();
    ThingCount(Pop(), tid);
    return SCRIPT_CONTINUE;
}

static int CmdThingCountDirect(void)
{
    int type;

    type = ReadCodeInt();
    ThingCount(type, ReadCodeInt());
    return SCRIPT_CONTINUE;
}

static void ThingCount(int type, int tid)
{
    int count;
    int searcher;
    const mobj_t *mobj;
    mobjtype_t moType;
    const thinker_t *think;

    if (!(type + tid))
    {                           // Nothing to count
        return;
    }
    moType = TranslateThingType[type];
    count = 0;
    searcher = -1;
    if (tid)
    {                           // Count TID things
        while ((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
        {
            if (type == 0)
            {                   // Just count TIDs
                count++;
            }
            else if (moType == mobj->type)
            {
                if (mobj->flags & MF_COUNTKILL && mobj->health <= 0)
                {               // Don't count dead monsters
                    continue;
                }
                count++;
            }
        }
    }
    else
    {                           // Count only types
        for (think = thinkercap.next; think != &thinkercap;
             think = think->next)
        {
            if (think->function != P_MobjThinker)
            {                   // Not a mobj thinker
                continue;
            }
            mobj = (mobj_t *) think;
            if (mobj->type != moType)
            {                   // Doesn't match
                continue;
            }
            if (mobj->flags & MF_COUNTKILL && mobj->health <= 0)
            {                   // Don't count dead monsters
                continue;
            }
            count++;
        }
    }
    Push(count);
}

static int CmdTagWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORTAG;
    return SCRIPT_STOP;
}

static int CmdTagWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = ReadCodeInt();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORTAG;
    return SCRIPT_STOP;
}

static int CmdPolyWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORPOLY;
    return SCRIPT_STOP;
}

static int CmdPolyWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = ReadCodeInt();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORPOLY;
    return SCRIPT_STOP;
}

static int CmdChangeFloor(void)
{
    int tag;
    int flat;
    int sectorIndex;

    flat = R_FlatNumForName(StringLookup(Pop()));
    tag = Pop();
    sectorIndex = -1;
    while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
    {
        sectors[sectorIndex].floorpic = flat;
    }
    return SCRIPT_CONTINUE;
}

static int CmdChangeFloorDirect(void)
{
    int tag;
    int flat;
    int sectorIndex;

    tag = ReadCodeInt();
    flat = R_FlatNumForName(StringLookup(ReadCodeInt()));
    sectorIndex = -1;
    while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
    {
        sectors[sectorIndex].floorpic = flat;
    }
    return SCRIPT_CONTINUE;
}

static int CmdChangeCeiling(void)
{
    int tag;
    int flat;
    int sectorIndex;

    flat = R_FlatNumForName(StringLookup(Pop()));
    tag = Pop();
    sectorIndex = -1;
    while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
    {
        sectors[sectorIndex].ceilingpic = flat;
    }
    return SCRIPT_CONTINUE;
}

static int CmdChangeCeilingDirect(void)
{
    int tag;
    int flat;
    int sectorIndex;

    tag = ReadCodeInt();
    flat = R_FlatNumForName(StringLookup(ReadCodeInt()));
    sectorIndex = -1;
    while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
    {
        sectors[sectorIndex].ceilingpic = flat;
    }
    return SCRIPT_CONTINUE;
}

static int CmdRestart(void)
{
    PCodeOffset = ACSInfo[ACScript->infoIndex].offset;
    return SCRIPT_CONTINUE;
}

static int CmdAndLogical(void)
{
    Push(Pop() && Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrLogical(void)
{
    Push(Pop() || Pop());
    return SCRIPT_CONTINUE;
}

static int CmdAndBitwise(void)
{
    Push(Pop() & Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrBitwise(void)
{
    Push(Pop() | Pop());
    return SCRIPT_CONTINUE;
}

static int CmdEorBitwise(void)
{
    Push(Pop() ^ Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNegateLogical(void)
{
    Push(!Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLShift(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() << operand2);
    return SCRIPT_CONTINUE;
}

static int CmdRShift(void)
{
    int operand2;

    operand2 = Pop();
    Push(Pop() >> operand2);
    return SCRIPT_CONTINUE;
}

static int CmdUnaryMinus(void)
{
    Push(-Pop());
    return SCRIPT_CONTINUE;
}

static int CmdIfNotGoto(void)
{
    int offset;

    offset = ReadOffset();

    if (Pop() == 0)
    {
        PCodeOffset = offset;
    }
    return SCRIPT_CONTINUE;
}

static int CmdLineSide(void)
{
    Push(ACScript->side);
    return SCRIPT_CONTINUE;
}

static int CmdScriptWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORSCRIPT;
    return SCRIPT_STOP;
}

static int CmdScriptWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = ReadCodeInt();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITINGFORSCRIPT;
    return SCRIPT_STOP;
}

static int CmdClearLineSpecial(void)
{
    if (ACScript->line)
    {
        ACScript->line->special = 0;
    }
    return SCRIPT_CONTINUE;
}

static int CmdCaseGoto(void)
{
    int value;
    int offset;

    value = ReadCodeInt();
    offset = ReadOffset();

    if (Top() == value)
    {
        PCodeOffset = offset;
        Drop();
    }

    return SCRIPT_CONTINUE;
}

static int CmdBeginPrint(void)
{
    *PrintBuffer = 0;
    return SCRIPT_CONTINUE;
}

static int CmdEndPrint(void)
{
    player_t *player;

    if (ACScript->activator && ACScript->activator->player)
    {
        player = ACScript->activator->player;
    }
    else
    {
        player = &players[consoleplayer];
    }
    CT_SetMessage(player, PrintBuffer, true, NULL);
    return SCRIPT_CONTINUE;
}

static int CmdEndPrintBold(void)
{
    int i;

    for (i = 0; i < maxplayers; i++)
    {
        if (playeringame[i])
        {
            CT_SetYellowMessage(&players[i], PrintBuffer, true);
        }
    }
    return SCRIPT_CONTINUE;
}

static int CmdPrintString(void)
{
    M_StringConcat(PrintBuffer, StringLookup(Pop()), sizeof(PrintBuffer));
    return SCRIPT_CONTINUE;
}

static int CmdPrintNumber(void)
{
    char tempStr[16];

    M_snprintf(tempStr, sizeof(tempStr), "%d", Pop());
    M_StringConcat(PrintBuffer, tempStr, sizeof(PrintBuffer));
    return SCRIPT_CONTINUE;
}

static int CmdPrintCharacter(void)
{
    char tempStr[2];

    tempStr[0] = Pop();
    tempStr[1] = '\0';
    M_StringConcat(PrintBuffer, tempStr, sizeof(PrintBuffer));

    return SCRIPT_CONTINUE;
}

static int CmdPlayerCount(void)
{
    int i;
    int count;

    count = 0;
    for (i = 0; i < maxplayers; i++)
    {
        count += playeringame[i];
    }
    Push(count);
    return SCRIPT_CONTINUE;
}

static int CmdGameType(void)
{
    int gametype;

    if (netgame == false)
    {
        gametype = GAME_SINGLE_PLAYER;
    }
    else if (deathmatch)
    {
        gametype = GAME_NET_DEATHMATCH;
    }
    else
    {
        gametype = GAME_NET_COOPERATIVE;
    }
    Push(gametype);
    return SCRIPT_CONTINUE;
}

static int CmdGameSkill(void)
{
    Push(gameskill);
    return SCRIPT_CONTINUE;
}

static int CmdTimer(void)
{
    Push(leveltime);
    return SCRIPT_CONTINUE;
}

static int CmdSectorSound(void)
{
    int volume;
    mobj_t *mobj;

    mobj = NULL;
    if (ACScript->line)
    {
        mobj = (mobj_t *) & ACScript->line->frontsector->soundorg;
    }
    volume = Pop();
    S_StartSoundAtVolume(mobj, S_GetSoundID(StringLookup(Pop())), volume);
    return SCRIPT_CONTINUE;
}

static int CmdThingSound(void)
{
    int tid;
    int sound;
    int volume;
    mobj_t *mobj;
    int searcher;

    volume = Pop();
    sound = S_GetSoundID(StringLookup(Pop()));
    tid = Pop();
    searcher = -1;
    while ((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        S_StartSoundAtVolume(mobj, sound, volume);
    }
    return SCRIPT_CONTINUE;
}

static int CmdAmbientSound(void)
{
    int volume;

    volume = Pop();
    S_StartSoundAtVolume(NULL, S_GetSoundID(StringLookup(Pop())), volume);
    return SCRIPT_CONTINUE;
}

static int CmdSoundSequence(void)
{
    mobj_t *mobj;

    mobj = NULL;
    if (ACScript->line)
    {
        mobj = (mobj_t *) & ACScript->line->frontsector->soundorg;
    }
    SN_StartSequenceName(mobj, StringLookup(Pop()));
    return SCRIPT_CONTINUE;
}

static int CmdSetLineTexture(void)
{
    line_t *line;
    int lineTag;
    int side;
    int position;
    int texture;
    int searcher;

    texture = R_TextureNumForName(StringLookup(Pop()));
    position = Pop();
    side = Pop();
    lineTag = Pop();
    searcher = -1;
    while ((line = P_FindLine(lineTag, &searcher)) != NULL)
    {
        if (position == TEXTURE_MIDDLE)
        {
            sides[line->sidenum[side]].midtexture = texture;
        }
        else if (position == TEXTURE_BOTTOM)
        {
            sides[line->sidenum[side]].bottomtexture = texture;
        }
        else
        {                       // TEXTURE_TOP
            sides[line->sidenum[side]].toptexture = texture;
        }
    }
    return SCRIPT_CONTINUE;
}

static int CmdSetLineBlocking(void)
{
    line_t *line;
    int lineTag;
    boolean blocking;
    int searcher;

    blocking = Pop()? ML_BLOCKING : 0;
    lineTag = Pop();
    searcher = -1;
    while ((line = P_FindLine(lineTag, &searcher)) != NULL)
    {
        line->flags = (line->flags & ~ML_BLOCKING) | blocking;
    }
    return SCRIPT_CONTINUE;
}

static int CmdSetLineSpecial(void)
{
    line_t *line;
    int lineTag;
    int special, arg1, arg2, arg3, arg4, arg5;
    int searcher;

    arg5 = Pop();
    arg4 = Pop();
    arg3 = Pop();
    arg2 = Pop();
    arg1 = Pop();
    special = Pop();
    lineTag = Pop();
    searcher = -1;
    while ((line = P_FindLine(lineTag, &searcher)) != NULL)
    {
        line->special = special;
        line->arg1 = arg1;
        line->arg2 = arg2;
        line->arg3 = arg3;
        line->arg4 = arg4;
        line->arg5 = arg5;
    }
    return SCRIPT_CONTINUE;
}
