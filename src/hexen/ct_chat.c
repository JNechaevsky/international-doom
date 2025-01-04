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


#include <string.h>
#include <ctype.h>
#include "h2def.h"
#include "i_input.h"
#include "s_sound.h"
#include "doomkeys.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "v_video.h"
#include "ct_chat.h"

#define QUEUESIZE		128
#define MESSAGESIZE		128
#define MESSAGELEN 		265
#define CT_ESCAPE 6


// 8-player note:  Change this stuff (CT_PLR_*, and the key mappings)
enum
{
    CT_PLR_BLUE = 1,
    CT_PLR_RED,
    CT_PLR_YELLOW,
    CT_PLR_GREEN,
    CT_PLR_PLAYER5,
    CT_PLR_PLAYER6,
    CT_PLR_PLAYER7,
    CT_PLR_PLAYER8,
    CT_PLR_ALL
};

static void CT_queueChatChar (char ch);
static void CT_ClearChatMessage (int player);
static void CT_AddChar (int player, char c);
static void CT_BackSpace (int player);

static int  head;
static int  tail;
static int  msgptr[MAXPLAYERS];
static int  msglen[MAXPLAYERS];
static int  chat_dest[MAXPLAYERS];
static char chat_msg[MAXPLAYERS][MESSAGESIZE];
static char plr_lastmsg[MAXPLAYERS][MESSAGESIZE + 9];  // add in the length of the pre-string
static byte ChatQueue[QUEUESIZE];

static int FontABaseLump;

const char *CT_FromPlrText[MAXPLAYERS] = {
    "BLUE:  ",
    "RED:  ",
    "YELLOW:  ",
    "GREEN:  ",
    "JADE:  ",
    "WHITE:  ",
    "HAZEL:  ",
    "PURPLE:  "
};

char *chat_macros[10];

boolean altdown;
boolean shiftdown;
boolean chatmodeon;

// -----------------------------------------------------------------------------
// CT_Init
// Initialize chat mode data.
// -----------------------------------------------------------------------------

void CT_Init (void)
{
    head = 0;  // Initialize the queue index.
    tail = 0;
    chatmodeon = false;
    memset(ChatQueue, 0, QUEUESIZE);

    for (int i = 0; i < maxplayers; i++)
    {
        chat_dest[i] = 0;
        msgptr[i] = 0;
        memset(plr_lastmsg[i], 0, MESSAGESIZE);
        memset(chat_msg[i], 0, MESSAGESIZE);
    }

    FontABaseLump = W_GetNumForName("FONTA_S") + 1;
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
// These keys are allowed by Vanilla Hexen:
// -----------------------------------------------------------------------------

static boolean ValidChatChar (char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        ||  c == '!' || c == '?'
        ||  c == ' ' || c == '\''
        ||  c == ',' || c == '.'
        ||  c == '-' || c == '=';
}

// -----------------------------------------------------------------------------
// CT_Responder
// -----------------------------------------------------------------------------

boolean CT_Responder (event_t *ev)
{
    int sendto;
    const char *macro;

    if (!netgame)
    {
        return false;
    }
    if (ev->data1 == KEY_RALT)
    {
        altdown = (ev->type == ev_keydown);
        return false;
    }
    if (ev->data1 == KEY_RSHIFT)
    {
        shiftdown = (ev->type == ev_keydown);
        return false;
    }
    if (gamestate != GS_LEVEL || ev->type != ev_keydown)
    {
        return false;
    }
    if (!chatmodeon)
    {
        sendto = 0;
        if (ev->data1 == key_multi_msg)
        {
            sendto = CT_PLR_ALL;
        }
        else if (ev->data1 == key_multi_msgplayer[0])
        {
            sendto = CT_PLR_BLUE;
        }
        else if (ev->data1 == key_multi_msgplayer[1])
        {
            sendto = CT_PLR_RED;
        }
        else if (ev->data1 == key_multi_msgplayer[2])
        {
            sendto = CT_PLR_YELLOW;
        }
        else if (ev->data1 == key_multi_msgplayer[3])
        {
            sendto = CT_PLR_GREEN;
        }
        else if (ev->data1 == key_multi_msgplayer[4])
        {
            sendto = CT_PLR_PLAYER5;
        }
        else if (ev->data1 == key_multi_msgplayer[5])
        {
            sendto = CT_PLR_PLAYER6;
        }
        else if (ev->data1 == key_multi_msgplayer[6])
        {
            sendto = CT_PLR_PLAYER7;
        }
        else if (ev->data1 == key_multi_msgplayer[7])
        {
            sendto = CT_PLR_PLAYER8;
        }
        if (sendto == 0 || (sendto != CT_PLR_ALL && !playeringame[sendto - 1])
            || sendto == consoleplayer + 1)
        {
            return false;
        }
        CT_queueChatChar(sendto);
        chatmodeon = true;
        I_StartTextInput(25, 10, SCREENWIDTH, 18);
        return true;
    }
    else
    {
        if (altdown)
        {
            if (ev->data1 >= '0' && ev->data1 <= '9')
            {
                if (ev->data1 == '0')
                {
                    // macro 0 comes after macro 9
                    ev->data1 = '9' + 1;
                }
                macro = chat_macros[ev->data1 - '1'];
                CT_queueChatChar(KEY_ENTER);  //send old message
                CT_queueChatChar(chat_dest[consoleplayer]);  // chose the dest.
                while (*macro)
                {
                    CT_queueChatChar(toupper(*macro++));
                }
                CT_queueChatChar(KEY_ENTER);  // send it off...
                CT_Stop();
                return true;
            }
        }
        if (ev->data1 == KEY_ENTER)
        {
            CT_queueChatChar(KEY_ENTER);
            usearti = false;
            CT_Stop();
            return true;
        }
        else if (ev->data1 == KEY_ESCAPE)
        {
            CT_queueChatChar(CT_ESCAPE);
            CT_Stop();
            return true;
        }
        else if (ev->data1 == KEY_BACKSPACE)
        {
            CT_queueChatChar(KEY_BACKSPACE);
            return true;
        }
        else if (ValidChatChar(ev->data3))
        {
            CT_queueChatChar(toupper(ev->data3));
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// CT_Ticker
// -----------------------------------------------------------------------------

void CT_Ticker(void)
{
    int i, j;
    char c;
    int numplayers;

    for (i = 0; i < maxplayers; i++)
    {
        if (!playeringame[i])
        {
            continue;
        }
        if ((c = players[i].cmd.chatchar) != 0)
        {
            if (c <= CT_PLR_ALL)
            {
                chat_dest[i] = c;
                continue;
            }
            else if (c == CT_ESCAPE)
            {
                CT_ClearChatMessage(i);
            }
            else if (c == KEY_ENTER)
            {
                numplayers = 0;
                for (j = 0; j < maxplayers; j++)
                {
                    numplayers += playeringame[j];
                }
                CT_AddChar(i, 0);  // set the end of message character
                if (numplayers > 2)
                {
                    M_StringCopy(plr_lastmsg[i], CT_FromPlrText[i],
                                 sizeof(plr_lastmsg[i]));
                    M_StringConcat(plr_lastmsg[i], chat_msg[i],
                                   sizeof(plr_lastmsg[i]));
                }
                else
                {
                    M_StringCopy(plr_lastmsg[i], chat_msg[i],
                                 sizeof(plr_lastmsg[i]));
                }
                if (i != consoleplayer && (chat_dest[i] == consoleplayer + 1
                                           || chat_dest[i] == CT_PLR_ALL)
                    && *chat_msg[i])
                {
                    CT_SetMessage(&players[consoleplayer], plr_lastmsg[i],
                                 true, NULL);
                    S_StartSound(NULL, SFX_CHAT);
                }
                else if (i == consoleplayer && (*chat_msg[i]))
                {
                    if (numplayers <= 1)
                    {
                        CT_SetMessage(&players[consoleplayer],
                                     "THERE ARE NO OTHER PLAYERS IN THE GAME!",
                                     true, NULL);
                        S_StartSound(NULL, SFX_CHAT);
                    }
                }
                CT_ClearChatMessage(i);
            }
            else if (c == KEY_BACKSPACE)
            {
                CT_BackSpace(i);
            }
            else
            {
                CT_AddChar(i, c);
            }
        }
    }
    return;
}

// -----------------------------------------------------------------------------
// CT_Drawer
// -----------------------------------------------------------------------------

void CT_Drawer (void)
{
    if (chatmodeon)
    {
        int x = 25;

        for (int i = 0; i < msgptr[consoleplayer]; i++)
        {
            if (chat_msg[consoleplayer][i] < 33)
            {
                x += 6;
            }
            else
            {
                patch_t *patch = W_CacheLumpNum(FontABaseLump +
                                                chat_msg[consoleplayer][i] - 33,
                                                PU_CACHE);
                V_DrawShadowedPatchOptional(x, 10, 1, patch);
                x += patch->width;
            }
        }
        V_DrawShadowedPatchOptional(x, 10, 1, W_CacheLumpName("FONTA59", PU_CACHE));
    }
}

// -----------------------------------------------------------------------------
// CT_queueChatChar
// -----------------------------------------------------------------------------

static void CT_queueChatChar (char ch)
{
    if (((tail + 1) & (QUEUESIZE - 1)) == head)
    {
        return;  // the queue is full
    }
    ChatQueue[tail] = ch;
    tail = (tail + 1) & (QUEUESIZE - 1);
}

// -----------------------------------------------------------------------------
// CT_dequeueChatChar
// -----------------------------------------------------------------------------

char CT_dequeueChatChar (void)
{
    byte temp;

    if (head == tail)
    {
        return 0;  // queue is empty
    }
    temp = ChatQueue[head];
    head = (head + 1) & (QUEUESIZE - 1);
    return temp;
}

// -----------------------------------------------------------------------------
// CT_AddChar
// -----------------------------------------------------------------------------

static void CT_AddChar (int player, char c)
{
    if (msgptr[player] + 1 >= MESSAGESIZE || msglen[player] >= MESSAGELEN)
    {
        return;  // full.
    }

    chat_msg[player][msgptr[player]] = c;
    msgptr[player]++;

    if (c < 33)
    {
        msglen[player] += 6;
    }
    else
    {
        patch_t *patch;

        patch = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
        msglen[player] += patch->width;
    }
}

// -----------------------------------------------------------------------------
// CT_BackSpace
// Backs up a space, when the user hits (obviously) backspace.
// -----------------------------------------------------------------------------

void CT_BackSpace (int player)
{
    char c;

    if (msgptr[player] == 0)
    {
        return;  // message is already blank
    }

    msgptr[player]--;
    c = chat_msg[player][msgptr[player]];

    if (c < 33)
    {
        msglen[player] -= 6;
    }
    else
    {
        patch_t *patch = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
        msglen[player] -= patch->width;
    }

    chat_msg[player][msgptr[player]] = 0;
}

// -----------------------------------------------------------------------------
// CT_ClearChatMessage
// Clears out the data for the chat message, but the player's message 
// is still saved in plrmsg.
// -----------------------------------------------------------------------------

static void CT_ClearChatMessage (int player)
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
    if ((player->ultimateMessage || !msg_show) && !ultmsg)
    {
        return;
    }
    M_StringCopy(player->message, message, sizeof(player->message));
    player->messageTics = MESSAGETICS;
    player->messageColor = table;
    player->yellowMessage = false;

    if (ultmsg)
    {
        player->ultimateMessage = true;
    }
}

// -----------------------------------------------------------------------------
// CT_SetYellowMessage
// [JN] Sets yellow message parameters.
// -----------------------------------------------------------------------------

void CT_SetYellowMessage (player_t *player, const char *message, boolean ultmsg)
{
    if ((player->ultimateMessage || !msg_show) && !ultmsg)
    {
        return;
    }
    M_StringCopy(player->message, message, sizeof(player->message));
    player->messageTics = 5 * MESSAGETICS;      // Bold messages last longer
    player->yellowMessage = true;

    if (ultmsg)
    {
        player->ultimateMessage = true;
    }
}

// -----------------------------------------------------------------------------
// CT_ClearMessage
// -----------------------------------------------------------------------------

void CT_ClearMessage (player_t *player)
{
    player->messageTics = 0;
}

// -----------------------------------------------------------------------------
// MSG_Ticker
// [JN] Reduces message tics independently from framerate and game states.
// Not to be confused with CT_Ticker.
// -----------------------------------------------------------------------------

void MSG_Ticker (void)
{
    player_t *player = &players[displayplayer];

    if (player->messageTics > 0)
    {
        player->messageTics--;
    }
    if (!player->messageTics)
    {                           // Refresh the screen when a message goes away
        player->ultimateMessage = false;        // clear out any chat messages.
        player->yellowMessage = false;
    }
}
