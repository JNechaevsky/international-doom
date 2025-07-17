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


#include <ctype.h> // toupper

#include "doomstat.h"
#include "doomkeys.h"
#include "ct_chat.h"
#include "d_englsh.h"
#include "deh_str.h"
#include "i_input.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_video.h"
#include "z_zone.h"

#include "id_vars.h"


#define QUEUESIZE       128
#define MESSAGESIZE     128
#define MESSAGELEN      265

#define CT_PLR_GREEN    1
#define CT_PLR_INDIGO   2
#define CT_PLR_BROWN    3
#define CT_PLR_RED      4
#define CT_PLR_ALL      5
#define CT_ESCAPE       6


patch_t *hu_font[HU_FONTSIZE];

boolean chatmodeon;
boolean altdown;
boolean shiftdown;

char *chat_macros[10] =
{
    HUSTR_CHATMACRO0,
    HUSTR_CHATMACRO1,
    HUSTR_CHATMACRO2,
    HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4,
    HUSTR_CHATMACRO5,
    HUSTR_CHATMACRO6,
    HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8,
    HUSTR_CHATMACRO9
};

char *player_names[] =
{
    HUSTR_PLRGREEN,
    HUSTR_PLRINDIGO,
    HUSTR_PLRBROWN,
    HUSTR_PLRRED
};

static void CT_queueChatChar (const char ch);
static void CT_ClearChatMessage (const int player);
static void CT_AddChar(const int player, const char c);
static void CT_BackSpace(const int player);

static int  head;
static int  tail;
static int  chat_sfx;  // [JN] Different chat sounds in Doom 1 and Doom 2.
static int  msgptr[MAXPLAYERS];
static int  msglen[MAXPLAYERS];
static int  chat_dest[MAXPLAYERS];
static char chat_msg[MAXPLAYERS][MESSAGESIZE];
static char plr_lastmsg[MAXPLAYERS][MESSAGESIZE + 9];  // add in the length of the pre-string
static byte ChatQueue[QUEUESIZE];

static int ChatFontBaseLump;

boolean     ultimatemsg;
const char *lastmessage;


// -----------------------------------------------------------------------------
// CT_Init
// [JN] Load the heads-up font.
// -----------------------------------------------------------------------------

void CT_Init (void)
{
    int   j = HU_FONTSTART;
    char  buffer[9];

    head = 0;  // Initialize the queue index.
    tail = 0;
    chatmodeon = false;
    memset(ChatQueue, 0, QUEUESIZE);
    chat_sfx = gamemode == commercial ? sfx_radio : sfx_tink;

    for (int i = 0 ; i < HU_FONTSIZE ; i++)
    {
        DEH_snprintf(buffer, sizeof(buffer), "STCFN%.3d", j++);
        hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
    }

    ChatFontBaseLump = W_GetNumForName(DEH_String("STCFN033"));
}

// -----------------------------------------------------------------------------
// CT_Stop
// -----------------------------------------------------------------------------

static void CT_Stop (void)
{
    chatmodeon = false;
    I_StopTextInput();
}

// -----------------------------------------------------------------------------
// ValidChatChar
// [JN] These keys are allowed by Vanilla Doom.
// [PN] Simplified and optimized for readability and maintainability.
// -----------------------------------------------------------------------------

static const boolean ValidChatChar (const char c)
{
    static const char validChars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789-+=!@#$%^&*()_[];:'\"\\,<>./? ";

    return strchr(validChars, c) != NULL;
}

// -----------------------------------------------------------------------------
// CT_Responder
// [JN] Handles chat-related events during gameplay.
// [PN] Improved readability and reduced redundant checks.
// -----------------------------------------------------------------------------

boolean CT_Responder (event_t *ev)
{
    // [PN] Ignore if not in netgame mode.
    if (!netgame)
    {
        return false;
    }

    // [PN] Handle ALT and SHIFT key states.
    if (ev->data1 == KEY_LALT || ev->data1 == KEY_RALT)
    {
        altdown = (ev->type == ev_keydown);
        return false;
    }
    if (ev->data1 == KEY_RSHIFT)
    {
        shiftdown = (ev->type == ev_keydown);
        return false;
    }

    // [PN] Ignore events that are not key presses.
    if (ev->type != ev_keydown)
    {
        return false;
    }

    // [PN] Handle chat activation keys when chat mode is off.
    if (!chatmodeon)
    {
        int sendto;

        // [PN] Map keys to players or broadcast.
        static const int key_to_player[] = {
            CT_PLR_GREEN, CT_PLR_INDIGO, CT_PLR_BROWN, CT_PLR_RED
        };

        if (ev->data1 == key_multi_msg)
        {
            sendto = CT_PLR_ALL;
        }
        else
        {
            sendto = 0;
            for (int i = 0; i < 4; i++)
            {
                if (ev->data1 == key_multi_msgplayer[i])
                {
                    sendto = key_to_player[i];
                    break;
                }
            }
        }

        // [PN] Validate the recipient.
        if (sendto == 0 || (sendto != CT_PLR_ALL && !playeringame[sendto - 1]) || sendto == consoleplayer + 1)
        {
            return false;
        }

        // [PN] Activate chat mode.
        CT_queueChatChar(sendto);
        chatmodeon = true;
        I_StartTextInput(25, 10, SCREENWIDTH, 18);
        return true;
    }
    else
    {
        // [PN] Handle macros triggered by ALT + number keys.
        if (altdown && ev->data1 >= '0' && ev->data1 <= '9')
        {
            const char *macro;

            if (ev->data1 == '0') ev->data1 = '9' + 1; // [PN] Macro 0 comes after macro 9.
            macro = chat_macros[ev->data1 - '1'];

            // [PN] Send the macro.
            CT_queueChatChar(KEY_ENTER);
            CT_queueChatChar(chat_dest[consoleplayer]);
            while (*macro) CT_queueChatChar(toupper(*macro++));
            CT_queueChatChar(KEY_ENTER);
            CT_Stop();
            return true;
        }

        // [PN] Handle special keys: Enter, Escape, Backspace.
        switch (ev->data1)
        {
            case KEY_ENTER:
                CT_queueChatChar(KEY_ENTER);
                CT_Stop();
                return true;
            case KEY_ESCAPE:
                CT_queueChatChar(CT_ESCAPE);
                CT_Stop();
                return true;
            case KEY_BACKSPACE:
                CT_queueChatChar(KEY_BACKSPACE);
                return true;
        }

        // [PN] Queue valid chat characters.
        if (ValidChatChar(ev->data3))
        {
            CT_queueChatChar(toupper(ev->data3));
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// CT_Ticker
// [JN] Processes chat-related game ticks.
// [PN] Improved readability, reduced nested logic, and consolidated checks.
// -----------------------------------------------------------------------------

void CT_Ticker (void)
{
    int i, j;
    char c;
    int numplayers;

    for (i = 0; i < MAXPLAYERS; i++)
    {
        // [PN] Skip players not in game.
        if (!playeringame[i])
        {
            continue;
        }

        c = players[i].cmd.chatchar;

        // [PN] Process character if set.
        if (c != 0)
        {
            switch (c)
            {
                case CT_ESCAPE:
                    // [PN] Clear chat message on ESC.
                    CT_ClearChatMessage(i);
                    break;

                case KEY_ENTER:
                    // [PN] Count players currently in game.
                    numplayers = 0;
                    for (j = 0; j < MAXPLAYERS; j++)
                    {
                        numplayers += playeringame[j];
                    }

                    // [PN] Finalize message with end character.
                    CT_AddChar(i, 0);

                    // [PN] Consolidate player name with message.
                    M_StringCopy(plr_lastmsg[i], DEH_String(player_names[i]), sizeof(plr_lastmsg[i]));
                    M_StringConcat(plr_lastmsg[i], chat_msg[i], sizeof(plr_lastmsg[i]));

                    // [PN] Display message for relevant players.
                    if (i != consoleplayer && 
                        (chat_dest[i] == consoleplayer + 1 || chat_dest[i] == CT_PLR_ALL) &&
                        *chat_msg[i])
                    {
                        CT_SetMessage(&players[consoleplayer], plr_lastmsg[i], true, NULL);
                        S_StartSound(NULL, chat_sfx);
                    }
                    else if (i == consoleplayer && *chat_msg[i])
                    {
                        if (numplayers > 1)
                        {
                            // [PN] Show message locally if more than one player.
                            CT_SetMessage(&players[consoleplayer], plr_lastmsg[i], true, NULL);
                            S_StartSound(NULL, chat_sfx);
                        }
                        else
                        {
                            // [PN] Notify no other players in game.
                            CT_SetMessage(&players[consoleplayer], ID_NOPLAYERS, true, NULL);
                            S_StartSound(NULL, chat_sfx);
                        }
                    }
                    CT_ClearChatMessage(i);
                    break;

                case KEY_BACKSPACE:
                    // [PN] Handle backspace key.
                    CT_BackSpace(i);
                    break;

                default:
                    if (c <= 5)
                    {
                        // [PN] Update chat destination for system messages.
                        chat_dest[i] = c;
                    }
                    else
                    {
                        // [PN] Add regular characters to the chat message.
                        CT_AddChar(i, c);
                    }
                    break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// CT_Drawer
// [JN] Draws the chat message on screen.
// [PN] Simplified loop and reduced redundant calculations.
// -----------------------------------------------------------------------------

void CT_Drawer (void)
{
    int x = 0;

    // [PN] Iterate through chat message characters.
    for (int i = 0; i < msgptr[consoleplayer]; i++)
    {
        char c = chat_msg[consoleplayer][i];

        if (c < 33)
        {
            // [PN] Handle non-printable characters (e.g., spaces).
            x += 4;
        }
        else
        {
            // [PN] Fetch and draw font patch for printable characters.
            patch_t *patch = W_CacheLumpNum(ChatFontBaseLump + c - 33, PU_STATIC);
            V_DrawShadowedPatchOptional(x - WIDESCREENDELTA, 10, 0, patch);
            x += patch->width;
        }
    }

    // [PN] Draw cursor at the end of the message.
    V_DrawShadowedPatchOptional(x - WIDESCREENDELTA, 10, 0,
                                W_CacheLumpName(DEH_String("STCFN095"), PU_STATIC));
}

// -----------------------------------------------------------------------------
// CT_queueChatChar
// [JN] Queues a character for chat processing.
// [PN] Added clarity with comments and formatting.
// -----------------------------------------------------------------------------

static void CT_queueChatChar (const char ch)
{
    // [PN] Check if the queue is full.
    if (((tail + 1) & (QUEUESIZE - 1)) == head)
    {
        return;  // [PN] Exit if no space left in the queue.
    }

    // [PN] Add character to the queue and update tail pointer.
    ChatQueue[tail] = ch;
    tail = (tail + 1) & (QUEUESIZE - 1);
}

// -----------------------------------------------------------------------------
// CT_dequeueChatChar
// [JN] Dequeues a character from the chat queue.
// [PN] Improved clarity with comments and formatting.
// -----------------------------------------------------------------------------

char CT_dequeueChatChar (void)
{
    byte temp;

    // [PN] Check if the queue is empty.
    if (head == tail)
    {
        return 0;  // [PN] Return 0 to indicate an empty queue.
    }

    // [PN] Fetch the character at the head of the queue.
    temp = ChatQueue[head];

    // [PN] Update head pointer to the next position.
    head = (head + 1) & (QUEUESIZE - 1);

    // [PN] Return the dequeued character.
    return temp;
}

// -----------------------------------------------------------------------------
// CT_AddChar
// [JN] Adds a character to the player's chat message buffer.
// [PN] Enhanced clarity with comments and minor formatting improvements.
// -----------------------------------------------------------------------------

static void CT_AddChar (const int player, const char c)
{
    // [PN] Check if the message buffer or visual length is full.
    if (msgptr[player] + 1 >= MESSAGESIZE || msglen[player] >= MESSAGELEN)
    {
        return;  // [PN] Exit if there's no space to add the character.
    }

    // [PN] Add character to the chat message buffer.
    chat_msg[player][msgptr[player]] = c;
    msgptr[player]++;

    // [PN] Update message length based on character type.
    if (c < 33)
    {
        // [PN] Non-printable characters add fixed width.
        msglen[player] += 4;
    }
    else
    {
        // [PN] Printable characters require fetching the font patch for width.
        const patch_t *const patch = W_CacheLumpNum(ChatFontBaseLump + c - 33, PU_STATIC);
        msglen[player] += patch->width;
    }
}

// -----------------------------------------------------------------------------
// CT_BackSpace
// [JN] Handles backspace action, removing the last character from the message.
// [PN] Improved readability with comments and formatting.
// -----------------------------------------------------------------------------

static void CT_BackSpace (const int player)
{
    // [PN] If the message is empty, there's nothing to backspace.
    if (msgptr[player] == 0)
    {
        return;  // [PN] Exit if the message is already blank.
    }

    // [PN] Move the message pointer back and fetch the last character.
    msgptr[player]--;
    const char c = chat_msg[player][msgptr[player]];

    // [PN] Adjust the message length based on the character type.
    if (c < 33)
    {
        // [PN] Non-printable characters have a fixed width.
        msglen[player] -= 6;
    }
    else
    {
        // [PN] Printable characters require fetching the font patch for width.
        const patch_t *const patch = W_CacheLumpNum(ChatFontBaseLump + c - 33, PU_STATIC);
        msglen[player] -= patch->width;
    }

    // [PN] Remove the character from the message buffer.
    chat_msg[player][msgptr[player]] = 0;
}

// -----------------------------------------------------------------------------
// CT_ClearChatMessage
// Clears out the data for the chat message, but the player's message 
// is still saved in plrmsg.
// -----------------------------------------------------------------------------

static void CT_ClearChatMessage (const int player)
{
    memset(chat_msg[player], 0, MESSAGESIZE);
    msgptr[player] = 0;
    msglen[player] = 0;
}

// -----------------------------------------------------------------------------
// CT_SetMessage
// [JN] Sets message parameters.
// -----------------------------------------------------------------------------

void CT_SetMessage (player_t *player, const char *message, boolean ultmsg, byte *table)
{
    lastmessage = message;

    if ((ultimatemsg || !msg_show) && !ultmsg)
    {
        return;
    }

    player->message = message;
    player->messageTics = MESSAGETICS;
    player->messageColor = table;

    if (ultmsg)
    {
        ultimatemsg = true;
    }
}

// -----------------------------------------------------------------------------
// CT_SetMessageCentered
// [JN] Sets centered message parameters.
// -----------------------------------------------------------------------------

void CT_SetMessageCentered (player_t *player, const char *message, byte *table)
{
    player->messageCentered = message;
    player->messageCenteredTics = 5*TICRATE/2; // [crispy] 2.5 seconds
    player->messageCenteredColor = table;
}

// -----------------------------------------------------------------------------
// MSG_Ticker
// [JN] Reduces message tics independently from framerate and game states.
// [PN] Improved readability with comments and formatting.
// -----------------------------------------------------------------------------

void MSG_Ticker (void)
{
    player_t *player = &players[displayplayer];

    // [PN] Decrease the countdown for normal messages.
    if (player->messageTics > 0)
    {
        player->messageTics--;
    }

    // [PN] Clear ultimate message state if message time has expired.
    if (player->messageTics == 0)
    {
        ultimatemsg = false;  // [PN] Clear out any remaining chat messages.
    }

    // [PN] Decrease the countdown for centered messages.
    if (player->messageCenteredTics > 0)
    {
        player->messageCenteredTics--;
    }
}
