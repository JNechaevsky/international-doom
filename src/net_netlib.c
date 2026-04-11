//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2026 Julia Nechaevskaya
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
// DESCRIPTION:
//     Networking module which uses netlib.
//

// netlib adapted from SDL_Net
//
// Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
// Copyright (C) 2012 Simeon Maxein <smaxein@googlemail.com>
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#if defined(_WIN32) || defined(_WIN64)
  #define __USE_W32_SOCKETS
  #define _WINSOCK_DEPRECATED_NO_WARNINGS
  #define boolean netlib_win_boolean
  #ifdef _WIN64
    #include <winsock2.h>
    #include <ws2tcpip.h>
  #else
    #include <winsock.h>
    /* NOTE: windows socklen_t is signed
     * and is defined only for winsock2. */
    typedef int socklen_t;
  #endif /* W64 */
  #include <iphlpapi.h>
  #undef boolean
#else /* UNIX */
  #include <sys/types.h>
  #ifdef __FreeBSD__
    #include <sys/socket.h>
  #endif
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <sys/ioctl.h>
  #include <sys/time.h>
  #include <unistd.h>
  #ifndef __BEOS__
    #include <arpa/inet.h>
  #endif
  #include <net/if.h>
  #include <netdb.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
#endif /* WIN32 */

#ifndef __USE_W32_SOCKETS
  #ifdef __OS2__
    #define closesocket soclose
  #else /* !__OS2__ */
    #define closesocket close
  #endif /* __OS2__ */
  #define SOCKET         int
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR   -1
#endif /* __USE_W32_SOCKETS */

#ifdef __USE_W32_SOCKETS
  #define netlib_get_last_error WSAGetLastError
  #define netlib_set_last_error WSASetLastError
  #ifndef EINTR
    #define EINTR WSAEINTR
  #endif
#else
  #include <signal.h>
  #include <errno.h>
  static int netlib_get_last_error(void)
  {
      return errno;
  }
  static void netlib_set_last_error(int err)
  {
      errno = err;
  }
#endif

#include <stdint.h>

int netlib_init(void);
void netlib_quit(void);
const char *netlib_get_error(void);

typedef struct
{
    uint32_t host; // 32-bit IPv4 host address
    uint16_t port; // 16-bit protocol port
} ip_address_t;

#ifndef INADDR_ANY
  #define INADDR_ANY 0x00000000
#endif
#ifndef INADDR_NONE
  #define INADDR_NONE 0xFFFFFFFF
#endif
#ifndef INADDR_LOOPBACK
  #define INADDR_LOOPBACK 0x7f000001
#endif
#ifndef INADDR_BROADCAST
  #define INADDR_BROADCAST 0xFFFFFFFF
#endif

// Resolve a host name and port to an IP address in network form.
// If the function succeeds, it will return 0.
// If the host couldn't be resolved, the host portion of the returned
// address will be INADDR_NONE, and the function will return -1.
// If 'host' is NULL, the resolved host will be set to INADDR_ANY.
//
int netlib_resolve_host(ip_address_t *address, const char *host, uint16_t port);

// The maximum channels on a a UDP socket
#define NETLIB_MAX_UDPCHANNELS  32
// The maximum addresses bound to a single UDP socket channel
#define NETLIB_MAX_UDPADDRESSES 4

typedef struct
{
    int channel;   // The src/dst channel of the packet
    uint8_t *data; // The packet data
    int len;       // The length of the packet data
    int maxlen;    // The length of the data data
    int status;    // packet status after sending
    ip_address_t address; // The source/dest address of an incoming/outgoing packet
} udp_packet_t;

typedef struct udp_socket_s *udp_socket_t;

// Allocate/free a single UDP packet 'length' bytes long.
// The new packet is returned, or NULL if the function ran out of memory.
//
udp_packet_t *netlib_alloc_packet(int size);
void netlib_free_packet(udp_packet_t *packet);

// Open a UDP network socket
// If 'port' is non-zero, the UDP socket is bound to a local port.
// The 'port' should be given in native byte order, but is used
// internally in network (big endian) byte order, in addresses, etc.
// This allows other systems to send to this socket via a known port.
//
udp_socket_t netlib_udp_open(uint16_t port);

// Close a UDP network socket
void netlib_udp_close(udp_socket_t sock);

// Send a single packet to the specified channel.
// If the channel specified in the packet is -1, the packet will be sent to
// the address in the 'src' member of the packet.
// The packet will be updated with the status of the packet after it has
// been sent.
// This function returns 1 if the packet was sent, or 0 on error.
//
// NOTE:
// The maximum length of the packet is limited by the MTU (Maximum Transfer Unit)
// of the transport medium.  It can be as low as 250 bytes for some PPP links,
// and as high as 1500 bytes for ethernet.
//
int netlib_udp_send(udp_socket_t sock, int channel, udp_packet_t *packet);

// Receive a single packet from the UDP socket.
// The returned packet contains the source address and the channel it arrived
// on.  If it did not arrive on a bound channel, the the channel will be set
// to -1.
// The channels are checked in highest to lowest order, so if an address is
// bound to multiple channels, the highest channel with the source address
// bound will be returned.
// This function returns the number of packets read from the network, or -1
// on error.  This function does not block, so can return 0 packets pending.
//
int netlib_udp_recv(udp_socket_t sock, udp_packet_t *packet);


// Write a 16-bit value to network packet data
static inline uint16_t netlib_read16(const void *areap)
{
    const uint8_t *area = (uint8_t *)areap;
    return ((uint16_t)area[0]) << 8 | ((uint16_t)area[1]);
}

// Write a 32-bit value to network packet data
static inline uint32_t netlib_read32(const void *areap)
{
    const uint8_t *area = (const uint8_t *)areap;
    return ((uint32_t)area[0]) << 24 | ((uint32_t)area[1]) << 16
           | ((uint32_t)area[2]) << 8 | ((uint32_t)area[3]);
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined(__GNUC__) || defined(__clang__)
  #define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#else
  #define PRINTF_ATTR(fmt, first)
#endif

static char errorbuf[1024];

static PRINTF_ATTR(1, 2) void netlib_set_error(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vsnprintf(errorbuf, sizeof(errorbuf), fmt, argp);
    va_end(argp);
}

const char *netlib_get_error(void)
{
    return errorbuf;
}

static int netlib_started;

int netlib_init(void)
{
    if (!netlib_started)
    {
#ifdef __USE_W32_SOCKETS
        // Start up the windows networking
        WORD version_wanted = MAKEWORD(1, 1);
        WSADATA wsaData;

        if (WSAStartup(version_wanted, &wsaData) != 0)
        {
            netlib_set_error("Couldn't initialize Winsock 1.1\n");
            return -1;
        }
#else
        // SIGPIPE is generated when a remote socket is closed
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_IGN);
        if (handler != SIG_DFL)
        {
            signal(SIGPIPE, handler);
        }
#endif
    }

    ++netlib_started;
    return 0;
}

void netlib_quit(void)
{
    if (netlib_started == 0)
    {
        return;
    }

    if (--netlib_started == 0)
    {
#ifdef __USE_W32_SOCKETS
        // Clean up windows networking
        if (WSACleanup() == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINPROGRESS)
            {
  #ifndef _WIN32_WCE
                WSACancelBlockingCall();
  #endif
                WSACleanup();
            }
        }
#else
        // Restore the SIGPIPE handler
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_DFL);
        if (handler != SIG_IGN)
        {
            signal(SIGPIPE, handler);
        }
#endif
    }
}

int netlib_resolve_host(ip_address_t *address, const char *host, uint16_t port)
{
    int retval = 0;

    // Perform the actual host resolution
    if (host == NULL)
    {
        address->host = INADDR_ANY;
    }
    else
    {
        address->host = inet_addr(host);
        if (address->host == INADDR_NONE)
        {
            struct hostent *hp;
            hp = gethostbyname(host);
            if (hp)
            {
                memcpy(&address->host, hp->h_addr, hp->h_length);
            }
            else
            {
                retval = -1;
                netlib_set_error("failed to get host from '%s'", host);
            }
        }
    }
    address->port = netlib_read16(&port);
    return retval;
}

typedef struct
{
    int numbound;
    ip_address_t address[NETLIB_MAX_UDPADDRESSES];
} udp_channel_t;

struct udp_socket_s
{
    int ready;
    SOCKET channel;
    ip_address_t address;
    udp_channel_t binding[NETLIB_MAX_UDPCHANNELS];
};

void netlib_udp_close(udp_socket_t sock)
{
    if (sock != NULL)
    {
        if (sock->channel != INVALID_SOCKET)
        {
            closesocket(sock->channel);
        }
        free(sock);
    }
}

udp_socket_t netlib_udp_open(uint16_t port)
{
    udp_socket_t sock;
    struct sockaddr_in sock_addr;
    socklen_t sock_len;

    // Allocate a UDP socket structure
    sock = (udp_socket_t)malloc(sizeof(*sock));

    if (sock == NULL)
    {
        netlib_set_error("Out of memory");
        goto error_return;
    }

    memset(sock, 0, sizeof(*sock));
    memset(&sock_addr, 0, sizeof(sock_addr));

    // Open the socket
    sock->channel = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->channel == INVALID_SOCKET)
    {
        netlib_set_error("Couldn't create socket");
        goto error_return;
    }

    // Bind locally, if appropriate
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = netlib_read16(&port);

    // Bind the socket for listening
    if (bind(sock->channel, (struct sockaddr *)&sock_addr, sizeof(sock_addr))
        == SOCKET_ERROR)
    {
        netlib_set_error("Couldn't bind to local port");
        goto error_return;
    }

    // Get the bound address and port
    sock_len = sizeof(sock_addr);
    if (getsockname(sock->channel, (struct sockaddr *)&sock_addr, &sock_len)
        < 0)
    {
        netlib_set_error("Couldn't get socket address");
        goto error_return;
    }

    // Fill in the channel host address
    sock->address.host = sock_addr.sin_addr.s_addr;
    sock->address.port = sock_addr.sin_port;

#ifdef SO_BROADCAST
    // Allow LAN broadcasts with the socket
    {
        int yes = 1;
        setsockopt(sock->channel, SOL_SOCKET, SO_BROADCAST, (char *)&yes,
                   sizeof(yes));
    }
#endif

#ifdef IP_ADD_MEMBERSHIP
    // Receive LAN multicast packets on 224.0.0.1
    // This automatically works on Mac OS X, Linux and BSD, but needs
    // this code on Windows.

    // A good description of multicast can be found here:
    // http://www.docs.hp.com/en/B2355-90136/ch05s05.html

    // FIXME: Add support for joining arbitrary groups to the API
    {
        struct ip_mreq g;

        g.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
        g.imr_interface.s_addr = INADDR_ANY;
        setsockopt(sock->channel, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&g,
                   sizeof(g));
    }
#endif

    // The socket is ready
    return sock;

error_return:
    netlib_udp_close(sock);

    return NULL;
}

int netlib_udp_send(udp_socket_t sock, int channel, udp_packet_t *packet)
{
    if (sock == NULL)
    {
        netlib_set_error("Passed a NULL socket");
        return 0;
    }

    udp_channel_t *binding;
    struct sockaddr_in sock_addr;
    int status;

    // Set up the variables to send packets
    int sock_len = sizeof(sock_addr);
    
    int numsent = 0;

    // if channel is < 0, then use channel specified in sock
    if (channel < 0)
    {
        sock_addr.sin_addr.s_addr = packet->address.host;
        sock_addr.sin_port = packet->address.port;
        sock_addr.sin_family = AF_INET;
        status = sendto(sock->channel, (const char *)packet->data, packet->len,
                        0, (struct sockaddr *)&sock_addr, sock_len);
        if (status >= 0)
        {
            packet->status = status;
            ++numsent;
        }
    }
    else
    {
        // Send to each of the bound addresses on the channel
        binding = &sock->binding[channel];

        for (int i = binding->numbound - 1; i >= 0; --i)
        {
            sock_addr.sin_addr.s_addr = binding->address[i].host;
            sock_addr.sin_port = binding->address[i].port;
            sock_addr.sin_family = AF_INET;
            status = sendto(sock->channel, (const char *)packet->data,
                            packet->len, 0, (struct sockaddr *)&sock_addr,
                            sock_len);
            if (status >= 0)
            {
                packet->status = status;
                ++numsent;
            }
        }
    }

    return numsent;
}

// Returns true if a socket is has data available for reading right now
static int socket_ready(SOCKET sock)
{
    int retval = 0;
    struct timeval tv;
    fd_set mask;

    // Check the file descriptors for available data
    do
    {
        netlib_set_last_error(0);

        // Set up the mask of file descriptors
        FD_ZERO(&mask);
        FD_SET(sock, &mask);

        // Set up the timeout
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        // Look!
        retval = select(sock + 1, &mask, NULL, NULL, &tv);
    } while (netlib_get_last_error() == EINTR);

    return retval == 1;
}

int netlib_udp_recv(udp_socket_t sock, udp_packet_t *packet)
{
    if (sock == NULL)
    {
        return 0;
    }

    udp_channel_t *binding;
    socklen_t sock_len;
    struct sockaddr_in sock_addr;

    int numrecv = 0;

    while (numrecv == 0 && socket_ready(sock->channel))
    {
        sock_len = sizeof(sock_addr);
        packet->status =
            recvfrom(sock->channel, (char *)packet->data, packet->maxlen, 0,
                     (struct sockaddr *)&sock_addr, &sock_len);

        if (packet->status >= 0)
        {
            packet->len = packet->status;
            packet->address.host = sock_addr.sin_addr.s_addr;
            packet->address.port = sock_addr.sin_port;
            packet->channel = -1;

            for (int i = (NETLIB_MAX_UDPCHANNELS - 1); i >= 0; --i)
            {
                binding = &sock->binding[i];

                for (int j = binding->numbound - 1; j >= 0; --j)
                {
                    if ((packet->address.host == binding->address[j].host)
                        && (packet->address.port == binding->address[j].port))
                    {
                        packet->channel = i;
                        goto foundit; // break twice
                    }
                }
            }
        foundit:
            ++numrecv;
        }
        else
        {
            packet->len = 0;
        }
    }

    sock->ready = 0;

    return numrecv;
}

udp_packet_t *netlib_alloc_packet(int size)
{
    udp_packet_t *packet;
    int error;

    error = 1;
    packet = (udp_packet_t *)malloc(sizeof(*packet));

    if (packet != NULL)
    {
        packet->maxlen = size;
        packet->data = (uint8_t *)malloc(size);
        if (packet->data != NULL)
        {
            error = 0;
        }
    }

    if (error)
    {
        netlib_set_error("Out of memory");
        netlib_free_packet(packet);
        packet = NULL;
    }
    return packet;
}

void netlib_free_packet(udp_packet_t *packet)
{
    if (packet)
    {
        free(packet->data);
        free(packet);
    }
}

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_netlib.h"
#include "net_packet.h"
#include "z_zone.h"

#define DEFAULT_PORT 2342

static boolean initted = false;
static int port = DEFAULT_PORT;
static udp_socket_t udpsocket;
static udp_packet_t *recvpacket;

typedef struct
{
    net_addr_t net_addr;
    ip_address_t netlib_addr;
} addrpair_t;

static addrpair_t **addr_table;
static int addr_table_size = -1;

// Initializes the address table

static void NETLIB_InitAddrTable(void)
{
    addr_table_size = 16;

    addr_table = Z_Malloc(sizeof(addrpair_t *) * addr_table_size,
                          PU_STATIC, 0);
    memset(addr_table, 0, sizeof(addrpair_t *) * addr_table_size);
}

static boolean NETLIB_AddressesEqual(ip_address_t *a, ip_address_t *b)
{
    return a->host == b->host && a->port == b->port;
}

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.

static net_addr_t *NETLIB_FindAddress(ip_address_t *addr)
{
    addrpair_t *new_entry;
    int empty_entry = -1;
    int i;

    if (addr_table_size < 0)
    {
        NETLIB_InitAddrTable();
    }

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr_table[i] != NULL
         && NETLIB_AddressesEqual(addr, &addr_table[i]->netlib_addr))
        {
            return &addr_table[i]->net_addr;
        }

        if (empty_entry < 0 && addr_table[i] == NULL)
        {
            empty_entry = i;
        }
    }

    // Was not found in list.  We need to add it.

    // Is there any space in the table? If not, increase the table size

    if (empty_entry < 0)
    {
        addrpair_t **new_addr_table;
        int new_addr_table_size;

        // after reallocing, we will add this in as the first entry
        // in the new block of memory

        empty_entry = addr_table_size;

        // allocate a new array twice the size, init to 0 and copy
        // the existing table in.  replace the old table.

        new_addr_table_size = addr_table_size * 2;
        new_addr_table = Z_Malloc(sizeof(addrpair_t *) * new_addr_table_size,
                                  PU_STATIC, 0);
        memset(new_addr_table, 0, sizeof(addrpair_t *) * new_addr_table_size);
        memcpy(new_addr_table, addr_table,
               sizeof(addrpair_t *) * addr_table_size);
        Z_Free(addr_table);
        addr_table = new_addr_table;
        addr_table_size = new_addr_table_size;
    }

    // Add a new entry

    new_entry = Z_Malloc(sizeof(addrpair_t), PU_STATIC, 0);

    new_entry->netlib_addr = *addr;
    new_entry->net_addr.refcount = 0;
    new_entry->net_addr.handle = &new_entry->netlib_addr;
    new_entry->net_addr.module = &netlib_module;

    addr_table[empty_entry] = new_entry;

    return &new_entry->net_addr;
}

static void NETLIB_FreeAddress(net_addr_t *addr)
{
    int i;

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr == &addr_table[i]->net_addr)
        {
            Z_Free(addr_table[i]);
            addr_table[i] = NULL;
            return;
        }
    }

    I_Error("NETLIB_FreeAddress: Attempted to remove an unused address!");
}

static boolean NETLIB_InitClient(void)
{
    int p;

    if (initted)
    {
        return true;
    }

    //!
    // @category net
    // @arg <n>
    //
    // Use the specified UDP port for communications, instead of
    // the default (2342).
    //

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
    {
        port = atoi(myargv[p + 1]);
    }

    if (netlib_init() < 0)
    {
        I_Error("NETLIB_InitClient: Failed to initialize netlib: %s",
                netlib_get_error());
    }

    udpsocket = netlib_udp_open(0);

    if (udpsocket == NULL)
    {
        I_Error("NETLIB_InitClient: Unable to open a socket!");
    }

    recvpacket = netlib_alloc_packet(1500);

#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static boolean NETLIB_InitServer(void)
{
    int p;

    if (initted)
    {
        return true;
    }

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
    {
        port = atoi(myargv[p + 1]);
    }

    if (netlib_init() < 0)
    {
        I_Error("NETLIB_InitServer: Failed to initialize netlib: %s",
                netlib_get_error());
    }

    udpsocket = netlib_udp_open(port);

    if (udpsocket == NULL)
    {
        I_Error("NETLIB_InitServer: Unable to bind to port %i", port);
    }

    recvpacket = netlib_alloc_packet(1500);
#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static void NETLIB_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    udp_packet_t netlib_packet;
    ip_address_t ip;

    if (addr == &net_broadcast_addr)
    {
        netlib_resolve_host(&ip, NULL, port);
        ip.host = INADDR_BROADCAST;
    }
    else
    {
        ip = *((ip_address_t *) addr->handle);
    }

#if 0
    {
        static int this_second_sent = 0;
        static int lasttime;

        this_second_sent += packet->len + 64;

        if (I_GetTime() - lasttime > TICRATE)
        {
            printf("%i bytes sent in the last second\n", this_second_sent);
            lasttime = I_GetTime();
            this_second_sent = 0;
        }
    }
#endif

#ifdef DROP_PACKETS
    if ((rand() % 4) == 0)
    {
        return;
    }
#endif

    netlib_packet.channel = 0;
    netlib_packet.data = packet->data;
    netlib_packet.len = packet->len;
    netlib_packet.address = ip;

    if (!netlib_udp_send(udpsocket, -1, &netlib_packet))
    {
        I_Error("NETLIB_SendPacket: Error transmitting packet: %s",
                netlib_get_error());
    }
}

static boolean NETLIB_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    int result;

    result = netlib_udp_recv(udpsocket, recvpacket);

    if (result < 0)
    {
        I_Error("NETLIB_RecvPacket: Error receiving packet: %s",
                netlib_get_error());
    }

    // no packets received

    if (result == 0)
    {
        return false;
    }

    // Put the data into a new packet structure

    *packet = NET_NewPacket(recvpacket->len);
    memcpy((*packet)->data, recvpacket->data, recvpacket->len);
    (*packet)->len = recvpacket->len;

    // Address

    *addr = NETLIB_FindAddress(&recvpacket->address);

    return true;
}

static void NETLIB_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
    ip_address_t *ip;
    uint32_t host;
    uint16_t net_port;

    ip = (ip_address_t *) addr->handle;
    host = netlib_read32(&ip->host);
    net_port = netlib_read16(&ip->port);

    M_snprintf(buffer, buffer_len, "%i.%i.%i.%i",
               (host >> 24) & 0xff, (host >> 16) & 0xff,
               (host >> 8) & 0xff, host & 0xff);

    // If we are using the default port we just need to show the IP address,
    // but otherwise we need to include the port. This is important because
    // we use the string representation in the setup tool to provided an
    // address to connect to.
    if (net_port != DEFAULT_PORT)
    {
        char portbuf[10];
        M_snprintf(portbuf, sizeof(portbuf), ":%i", net_port);
        M_StringConcat(buffer, portbuf, buffer_len);
    }
}

static net_addr_t *NETLIB_ResolveAddress(const char *address)
{
    ip_address_t ip;
    char *addr_hostname;
    int addr_port;
    int result;
    char *colon;

    colon = strchr(address, ':');

    addr_hostname = M_StringDuplicate(address);
    if (colon != NULL)
    {
        addr_hostname[colon - address] = '\0';
        addr_port = atoi(colon + 1);
    }
    else
    {
        addr_port = port;
    }

    result = netlib_resolve_host(&ip, addr_hostname, addr_port);

    free(addr_hostname);

    if (result)
    {
        // unable to resolve

        return NULL;
    }
    else
    {
        return NETLIB_FindAddress(&ip);
    }
}

// Complete module

net_module_t netlib_module =
{
    NETLIB_InitClient,
    NETLIB_InitServer,
    NETLIB_SendPacket,
    NETLIB_RecvPacket,
    NETLIB_AddrToString,
    NETLIB_FreeAddress,
    NETLIB_ResolveAddress,
};
