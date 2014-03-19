/*
  HawkNL cross platform network library
  Copyright (C) 2000-2002 Phil Frisbie, Jr. (phil@hawksoft.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA  02111-1307, USA.

  Or go to http://www.gnu.org/copyleft/lgpl.html
*/

#include <string.h>
#include "nlinternal.h"
#include "sock.h"
#include "serial.h"
#include "parallel.h"

#ifdef NL_INCLUDE_LOOPBACK
#include "loopback.h"
#endif

#if defined WINDOWS_APP && defined NL_INCLUDE_IPX
  #include "ipx.h"
#endif

#define MAX_NET_DRIVERS     6

volatile nl_state_t nlState = {NL_FALSE, NL_TRUE};

/* mutexes for global variables */
static NLmutex  socklock, instatlock, outstatlock;

static volatile NLboolean nlBlocking = NL_FALSE;

static volatile nl_stats_t nlInstats;
static volatile nl_stats_t nlOutstats;

static volatile NLsocket nlNextsocket = 0;
static volatile NLint nlNumsockets = 0;
static volatile NLint nlMaxNumsockets = NL_MIN_SOCKETS; /* this is dynamic, and can grow as needed */
static volatile NLint nlInitCount = 0;

pnl_socket_t *nlSockets = NULL;

/* the current selected driver */
static nl_netdriver_t /*@null@*/*driver = NULL;

static nl_netdriver_t netdrivers[] =
{
    {
        (NLchar*)TEXT("NL_IP"),
        (NLchar*)TEXT("NL_RELIABLE NL_UNRELIABLE NL_RELIABLE_PACKETS NL_BROADCAST NL_MULTICAST"),
        NL_IP,
        NL_FALSE,
        sock_Init,
        sock_Shutdown,
        sock_Listen,
        sock_AcceptConnection,
        sock_Open,
        sock_Connect,
        sock_Close,
        sock_Read,
        sock_Write,
        sock_AddrToString,
        sock_StringToAddr,
        sock_GetLocalAddr,
        sock_GetAllLocalAddr,
        sock_SetLocalAddr,
        sock_GetNameFromAddr,
        sock_GetNameFromAddrAsync,
        sock_GetAddrFromName,
        sock_GetAddrFromNameAsync,
        sock_AddrCompare,
        sock_GetPortFromAddr,
        sock_SetAddrPort,
        sock_GetSystemError,
        sock_PollGroup,
        sock_Hint
    }
#ifdef NL_INCLUDE_LOOPBACK
    ,
    {
        (NLchar*)TEXT("NL_LOOP_BACK"),
        (NLchar*)TEXT("NL_RELIABLE NL_UNRELIABLE NL_RELIABLE_PACKETS NL_BROADCAST"),
        NL_LOOP_BACK,
        NL_FALSE,
        loopback_Init,
        loopback_Shutdown,
        loopback_Listen,
        loopback_AcceptConnection,
        loopback_Open,
        loopback_Connect,
        loopback_Close,
        loopback_Read,
        loopback_Write,
        loopback_AddrToString,
        loopback_StringToAddr,
        loopback_GetLocalAddr,
        loopback_GetAllLocalAddr,
        loopback_SetLocalAddr,
        loopback_GetNameFromAddr,
        loopback_GetNameFromAddrAsync,
        loopback_GetAddrFromName,
        loopback_GetAddrFromNameAsync,
        loopback_AddrCompare,
        loopback_GetPortFromAddr,
        loopback_SetAddrPort,
        loopback_GetSystemError,
        loopback_PollGroup,
        loopback_Hint
    }
#endif /* NL_INCLUDE_LOOPBACK */
#if defined WINDOWS_APP && defined NL_INCLUDE_IPX
    ,
    {
        (NLchar*)TEXT("NL_IPX"),
        (NLchar*)TEXT("NL_RELIABLE NL_UNRELIABLE NL_RELIABLE_PACKETS NL_BROADCAST"),
        NL_IPX,
        NL_FALSE,
        ipx_Init,
        ipx_Shutdown,
        ipx_Listen,
        ipx_AcceptConnection,
        ipx_Open,
        ipx_Connect,
        ipx_Close,
        ipx_Read,
        ipx_Write,
        ipx_AddrToString,
        ipx_StringToAddr,
        ipx_GetLocalAddr,
        ipx_GetAllLocalAddr,
        ipx_SetLocalAddr,
        ipx_GetNameFromAddr,
        ipx_GetNameFromAddrAsync,
        ipx_GetAddrFromName,
        ipx_GetAddrFromNameAsync,
        ipx_AddrCompare,
        ipx_GetPortFromAddr,
        ipx_SetAddrPort,
        ipx_GetSystemError,
        ipx_PollGroup,
        ipx_Hint
    }
#endif /* WINDOWS_APP && NL_INCLUDE_IPX */
    ,
    {
        (NLchar*)NULL,
    }
};

/*

 Internal helper functions.

*/

/* make sure string is null terminated at less than NL_MAX_STRING_LENGTH */
static NLboolean isSafeString(const NLchar *string)
{
    int i;

    for(i=0;i<NL_MAX_STRING_LENGTH;i++)
    {
        if(string[i] == 0)
        {
            return NL_TRUE;
        }
    }
    return NL_FALSE;
}

NLsocket nlGetNewSocket(void)
{
    NLsocket    newsocket = NL_INVALID;
    nl_socket_t *sock = NULL;

    nlMutexLock(&socklock);
    if(nlNumsockets == nlMaxNumsockets)
    {
        nl_socket_t **temp;
        /* expand the list of sockets pointers */
        nlMaxNumsockets *= 2;
        temp = (nl_socket_t **)realloc((void *)nlSockets, nlMaxNumsockets * sizeof(nl_socket_t *));
        if(temp == NULL)
        {
            nlSetError(NL_OUT_OF_MEMORY);
            nlMutexUnlock(&socklock);
            return NL_INVALID;
        }
        nlSockets = temp;
    }
    /* get a socket number */
    if(nlNumsockets == nlNextsocket)
    {
        newsocket = nlNextsocket++;
        /* allocate the memory */
        sock = (nl_socket_t *)malloc(sizeof(nl_socket_t));
        if(sock == NULL)
        {
            nlSetError(NL_OUT_OF_MEMORY);
            nlMutexUnlock(&socklock);
            return NL_INVALID;
        }
        else
        {
            nlSockets[newsocket] = sock;
        }
        /* clear the structure */
        memset(sock, 0, sizeof(nl_socket_t));

        nlMutexInit(&sock->readlock);
        nlMutexInit(&sock->writelock);
        nlLockSocket(newsocket, NL_BOTH);
    }
    else
    /* there is an open socket slot somewhere below nlNextsocket */
    {
        NLsocket    i;
#ifdef NL_THREAD_SAFE
        NLmutex     readlock, writelock;
#endif

        for(i=0;i<nlNextsocket;i++)
        {
            if(nlSockets[i]->inuse == NL_FALSE)
            {
                /* found an open socket slot */
                newsocket = i;
                sock = nlSockets[i];
                break;
            }
        }
        /* let's check just to make sure we did find a socket */
        if(sock == NULL)
        {
            nlSetError(NL_OUT_OF_MEMORY);
            nlMutexUnlock(&socklock);
            return NL_INVALID;
        }
        nlLockSocket(newsocket, NL_BOTH);
#ifdef NL_THREAD_SAFE
        readlock = sock->readlock;
        writelock = sock->writelock;
#endif
        /* clear the structure */
        memset(sock, 0, sizeof(nl_socket_t));
#ifdef NL_THREAD_SAFE
        sock->readlock = readlock;
        sock->writelock = writelock;
#endif
    }

    /* sockets are blocking until set for non-blocking */
    sock->blocking = nlBlocking;
    sock->inuse = NL_TRUE;
    nlNumsockets++;
    nlMutexUnlock(&socklock);
    return newsocket;
}

static void nlReturnSocket(NLsocket socket)
{
    nl_socket_t     *sock = nlSockets[socket];

    if((sock != NULL) && (sock->inuse == NL_TRUE))
    {
        sock->inuse = NL_FALSE;
        if(sock->inbuf != NULL)
        {
            free(sock->inbuf);
        }
        if(sock->outbuf != NULL)
        {
            free(sock->outbuf);
        }
        nlNumsockets--;
        nlMutexUnlock(&socklock);
    }
}

void nlFreeSocket(NLsocket socket)
{
    nl_socket_t     *sock = nlSockets[socket];

    if((sock != NULL) && (sock->inuse == NL_TRUE))
    {
        if(sock->inbuf != NULL)
        {
            free(sock->inbuf);
        }
        if(sock->outbuf != NULL)
        {
            free(sock->outbuf);
        }
        nlMutexDestroy(&sock->readlock);
        nlMutexDestroy(&sock->writelock);
        nlNumsockets--;
        free(sock);
    }
}

NLboolean nlIsValidSocket(NLsocket socket)
{
    nl_socket_t *sock;

    if(socket < 0 || socket > nlMaxNumsockets)
    {
        nlSetError(NL_INVALID_SOCKET);
        return NL_FALSE;
    }
    sock = nlSockets[socket];
    if(sock == NULL)
    {
        nlSetError(NL_INVALID_SOCKET);
        return NL_FALSE;
    }
    if(sock->inuse == NL_FALSE)
    {
        nlSetError(NL_INVALID_SOCKET);
        return NL_FALSE;
    }
    return NL_TRUE;
}

void nlLockSocket(NLsocket socket, NLint which){
  
  nl_socket_t     *sock = nlSockets[socket];
  
  
  if((which&NL_READ) > 0)
    nlMutexLock2(&sock->readlock);
  //nlMutexLock(&sock->writelock);
  if((which&NL_WRITE) > 0)
    nlMutexLock(&sock->writelock);
  
}

void nlUnlockSocket(NLsocket socket, NLint which)
{
    nl_socket_t     *sock = nlSockets[socket];

    if((which&NL_READ) > 0)
    {
        nlMutexUnlock(&sock->readlock);
    }
    if((which&NL_WRITE) > 0)
    {
        nlMutexUnlock(&sock->writelock);
    }
}

static void nlUpdateStats(volatile nl_stats_t *stats, NLint nbytes, NLint npackets)
{
    time_t  t;

    (void)time(&t);
    if(stats->stime == 0)
    {
        /* must be the first time through */
        stats->stime = t;
        stats->lastbucket = -1;
    }
    /* do the basic update */
    stats->packets += npackets;
    stats->bytes += nbytes;

    /* check to see if we need to do the full update */
    if(stats->stime != t)
    {
        NLint   i;
        NLlong  count = 0;
        time_t  diff = t - stats->stime;

        stats->stime = t;

        if(stats->curbytes > stats->highest)
        {
            stats->highest = stats->curbytes;
        }
        if(diff >= NL_NUM_BUCKETS)
        {
            diff = NL_NUM_BUCKETS;
        }

        while(diff > 1)
        {
            /* we need to zero out skipped over buckets */
            stats->lastbucket++;
            if(stats->lastbucket == NL_NUM_BUCKETS)
            {
                stats->lastbucket = 0;
            }
            stats->bucket[stats->lastbucket] = 0;
            diff--;
        }
        stats->lastbucket++;
        if(stats->lastbucket == NL_NUM_BUCKETS)
        {
            stats->lastbucket = 0;
        }
        stats->bucket[stats->lastbucket] = stats->curbytes;
        stats->curbytes = 0;

        for(i=0;i<NL_NUM_BUCKETS;i++)
        {
            count += stats->bucket[i];
        }
        stats->average =  count / NL_NUM_BUCKETS;
    }
    stats->curbytes += nbytes;
}

static void nlUpdateInStats(NLint nbytes, NLint npackets)
{
    if(nlState.socketStats == NL_FALSE)
    {
        return;
    }
    nlMutexLock(&instatlock);
    nlUpdateStats(&nlInstats, nbytes, npackets);
    nlMutexUnlock(&instatlock);
}

static void nlUpdateOutStats(NLint nbytes, NLint npackets)
{
    if(nlState.socketStats == NL_FALSE)
    {
        return;
    }
    nlMutexLock(&outstatlock);
    nlUpdateStats(&nlOutstats, nbytes, npackets);
    nlMutexUnlock(&outstatlock);
}

static void nlUpdateSocketInStats(NLsocket socket, NLint nbytes, NLint npackets)
{
    nl_socket_t     *sock = nlSockets[socket];

    if(nlState.socketStats == NL_FALSE)
    {
        return;
    }
    nlUpdateStats(&sock->instats, nbytes, npackets);
}

static void nlUpdateSocketOutStats(NLsocket socket, NLint nbytes, NLint npackets)
{
    nl_socket_t     *sock = nlSockets[socket];
    
    if(nlState.socketStats == NL_FALSE)
    {
        return;
    }
    nlUpdateStats(&sock->outstats, nbytes, npackets);
}

/*

  Low level functions, a thin layer over Sockets.

*/

/*
   Trys to init all drivers, BUT DOES NOT SELECT A DRIVER
*/

NL_EXP NLboolean NL_APIENTRY nlInit(void)
{
    int i, numdrivers = 0;
    
    /* init socket memory, mutexes, and global variables */
    if(nlInitCount == 0)
    {
        if(nlSockets == NULL)
        {
            nlSockets = (nl_socket_t **)malloc(nlMaxNumsockets * sizeof(nl_socket_t *));
        }
        if(nlSockets == NULL)
        {
            nlSetError(NL_OUT_OF_MEMORY);
            nlShutdown();
            return NL_FALSE;
        }
        if(nlGroupInit() == NL_FALSE)
        {
            nlShutdown();
            return NL_FALSE;
        }
        nlMutexInit(&socklock);
        nlMutexInit(&instatlock);
        nlMutexInit(&outstatlock);
        nlNumsockets = 0;
        nlNextsocket = 0;
        nlBlocking = NL_FALSE;
        nlState.socketStats = NL_FALSE;
        nlState.nl_big_endian_data = NL_TRUE;

        for(i=0;i<MAX_NET_DRIVERS;i++)
        {
            if(netdrivers[i].name == NULL)
            {
                break;
            }
            if(netdrivers[i].initialized == NL_TRUE)
            {
                numdrivers++;
            }
            else if(netdrivers[i].Init() == NL_TRUE)
            {
                netdrivers[i].initialized = NL_TRUE;
                numdrivers++;
            }
        }
        if(numdrivers == 0)
        {
            nlSetError(NL_NO_NETWORK);
            nlShutdown();
            return NL_FALSE;
        }
    }
    nlInitCount++;
    return NL_TRUE;
}

/*
   Called at the end of your program, shuts down the active driver and frees memory
*/

NL_EXP void NL_APIENTRY nlShutdown(void){

  --nlInitCount;
  if(nlInitCount > 0)
    return;
  
  /* close any open sockets */
  nlMutexLock(&socklock);
//  sleep(1);
  if(nlSockets != NULL){
    NLsocket i;
    
    i=0;//for(i=0;i<nlNextsocket;i++){  REMOOOOOOOOOOOOOOOOOOOOOOVVVEEEEE THIIIISSSSSSSSSSSSSSS
      if(nlSockets[i] != NULL){
	nlLockSocket(i, NL_BOTH);
	driver->Close(i);
	nlReturnSocket(i);
	nlUnlockSocket(i, NL_BOTH);
	nlFreeSocket(i);
      }
      //}
    free(nlSockets);
    nlSockets = NULL;
  }
  nlMutexUnlock(&socklock);
  
  /* now we can shutdown the driver */
  if(driver != NULL){
    driver->Shutdown();
    driver->initialized = NL_FALSE;
    driver = NULL;
  
  }else{
    nlSetError(NL_NO_NETWORK);
  }
  nlGroupShutdown();
  /* destroy the mutexes */
  nlMutexDestroy(&socklock);
  nlMutexDestroy(&instatlock);
  nlMutexDestroy(&outstatlock);
}

/*
   Enables a socket to listen for incomming connections
*/

NL_EXP NLboolean NL_APIENTRY nlListen(NLsocket socket)
{
    if(driver != NULL)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            NLboolean result;

            nlLockSocket(socket, NL_BOTH);
            result = driver->Listen(socket);
            nlUnlockSocket(socket, NL_BOTH);
            return result;
        }
        nlSetError(NL_INVALID_SOCKET);
        return NL_FALSE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Accepts a pending connection.
   Creates a new socket object for this connection.
*/

NL_EXP NLsocket NL_APIENTRY nlAcceptConnection(NLsocket socket)
{
    if(driver)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            NLsocket newsocket;

            nlLockSocket(socket, NL_BOTH);
            newsocket = driver->AcceptConnection(socket);
            nlUnlockSocket(socket, NL_BOTH);
            if(newsocket != NL_INVALID)
            {
                /* the new socket was locked when it is created */
                nlUnlockSocket(newsocket, NL_BOTH);
            }
            return newsocket;
        }
        nlSetError(NL_INVALID_SOCKET);
        return NL_INVALID;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_INVALID;
}

/*
   Creates a new socket object.
   Can be used for reading or broadcast as is.
   For non-broadcast use, call nlConnectSocket to connect to a remote address.
*/

NL_EXP NLsocket NL_APIENTRY nlOpen(NLushort port, NLenum type)
{
    if(driver)
    {
        return (driver->Open(port, type));
    }

    nlSetError(NL_NO_NETWORK);
    return NL_INVALID;
}

/*
   Connect a socket to a remote address.
*/

NL_EXP NLboolean NL_APIENTRY nlConnect(NLsocket socket, const NLaddress *address)
{
    if(driver)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            if(address == NULL)
            {
                nlSetError(NL_NULL_POINTER);
            }
            else
            {
                NLboolean result;

                nlLockSocket(socket, NL_BOTH);
                result = driver->Connect(socket, address);
                nlUnlockSocket(socket, NL_BOTH);
                return result;
            }
        }
        else
        {
            nlSetError(NL_INVALID_SOCKET);
        }
        return NL_FALSE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Close the socket.
*/
NL_EXP NLboolean NL_APIENTRY nlClose(NLsocket socket){
  
  if(driver){
    if(nlIsValidSocket(socket) == NL_TRUE){
      
      nlLockSocket(socket, NL_BOTH);
     // sleep(1);
      driver->Close(socket);
      /* return the socket for reuse */
      nlMutexLock(&socklock);
      nlReturnSocket(socket);
      nlMutexUnlock(&socklock);
      nlUnlockSocket(socket, NL_BOTH);
      return NL_TRUE;
    
    }else{
      nlSetError(NL_INVALID_SOCKET);
      return NL_TRUE;
    }
  }
  nlSetError(NL_NO_NETWORK);
  return NL_FALSE;
}

/*
   Reads from a socket.
*/

NL_EXP NLint NL_APIENTRY nlRead(NLsocket socket, NLvoid *buffer, NLint nbytes)
{
    if(driver)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            if(buffer == NULL)
            {
                nlSetError(NL_NULL_POINTER);
            }
            else
            {
                NLint received;

                nlLockSocket(socket, NL_READ);
                received = driver->Read(socket, buffer, nbytes);

                if(received > 0)
                {
                    nlUpdateSocketInStats(socket, received, 1);
                    nlUpdateInStats(received, 1);
                }
                nlUnlockSocket(socket, NL_READ);
                return received;
            }
        }
        else
        {
            nlSetError(NL_INVALID_SOCKET);
        }
        return NL_INVALID;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_INVALID;
}

/*
   Writes to a socket.
*/

NL_EXP NLint NL_APIENTRY nlWrite(NLsocket socket, const NLvoid *buffer, NLint nbytes)
{
    if(driver)
    {
        /* check for group */
        if(socket >= NL_FIRST_GROUP)
        {
            NLint       number = NL_MAX_GROUP_SOCKETS;
            NLsocket    s[NL_MAX_GROUP_SOCKETS];
            NLint       i;
            NLint       sent = nbytes;
            
            if(nlGroupGetSockets(socket, (NLsocket *)s, &number) == NL_FALSE)
            {
                return NL_INVALID;
            }
            
            for(i=0;i<number;i++)
            {
                NLint result;

                if(nlIsValidSocket(s[i]) == NL_TRUE)
                {
                    result = nlWrite(s[i], buffer, nbytes);
                    if(result < sent)
                    {
                        sent = result;
                    }
                }
            }
            return sent;
        }
        else
        {
            if(nlIsValidSocket(socket) == NL_TRUE)
            {
                if(buffer == NULL)
                {
                    nlSetError(NL_NULL_POINTER);
                }
                else
                {
                    NLint sent;

                    nlLockSocket(socket, NL_WRITE);
                    sent = driver->Write(socket, buffer, nbytes);
                    if(sent > 0)
                    {
                        nlUpdateSocketOutStats(socket, sent, 1);
                        nlUpdateOutStats(sent, 1);
                    }
                    nlUnlockSocket(socket, NL_WRITE);
                    return sent;
                }
            }
            else
            {
                nlSetError(NL_INVALID_SOCKET);
            }
            return NL_INVALID;
        }
    }
    nlSetError(NL_NO_NETWORK);
    return NL_INVALID;
}

/*
   Polls all sockets in the group to see which have data waiting to be read.
   nlPollGroup uses select() on TCP or UDP sockets.
   Returns number of sockets waiting, and a list of those sockets, or NL_INVALID
   on an error.
*/

NL_EXP NLint NL_APIENTRY nlPollGroup(NLint group, NLenum name, /*@out@*/ NLsocket *sockets, NLint number, NLint timeout)
{
    if(driver)
    {
        if(sockets == NULL )
        {
            nlSetError(NL_NULL_POINTER);
            return 0;
        }
        return (driver->PollGroup(group, name, sockets, number, timeout));
    }
    nlSetError(NL_NO_NETWORK);
    return 0;
}

NL_EXP NLboolean NL_APIENTRY nlHint(NLenum name, NLint arg)
{
    if(driver)
    {
        return driver->Hint(name, arg);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}


/*
   Converts the numeric address in the NLaddress structure to a string.
*/

NL_EXP /*@null@*/ NLchar* NL_APIENTRY nlAddrToString(const NLaddress *address, NLchar *string)
{
    if(driver)
    {
        if((string == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NULL;
        }
        return (driver->AddrToString(address, string));
    }
    nlSetError(NL_NO_NETWORK);
    return NULL;
}

/*
   Takes a string that contains a full network address (ie, for IP 192.168.0.1:27000),
   and adds it to the NLaddress structure.
*/

NL_EXP NLboolean NL_APIENTRY nlStringToAddr(const NLchar *string, NLaddress *address)
{
    if(driver)
    {
        if((string == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        if(isSafeString(string) == NL_FALSE)
        {
            nlSetError(NL_STRING_OVER_RUN);
            return NL_FALSE;
        }
        return driver->StringToAddr(string, address);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Gets the remote address of the socket if connected to a remote host,
   or the local address if not connected.
*/

/* Note: the drivers put a copy of address in nl_socket_t, so we just need to copy it */
NL_EXP NLboolean NL_APIENTRY nlGetRemoteAddr(NLsocket socket, NLaddress *address)
{
    if(driver)
    {
        if(address == NULL)
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            nl_socket_t *sock = nlSockets[socket];

            nlLockSocket(socket, NL_READ);
            memcpy(address, &sock->address, sizeof(NLaddress));
			address->valid = NL_TRUE;
            nlUnlockSocket(socket, NL_READ);
        }
        else
        {
            nlSetError(NL_INVALID_SOCKET);
            memset(address, 0, sizeof(NLaddress));
            return NL_FALSE;
        }
        return NL_TRUE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Sets the remote address of an unconnected UDP socket.
*/

NL_EXP NLboolean NL_APIENTRY nlSetRemoteAddr(NLsocket socket, const NLaddress *address)
{
    if(driver)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            if(address == NULL)
            {
                nlSetError(NL_NULL_POINTER);
                return NL_FALSE;
            }
            else
            {
                nl_socket_t *sock = nlSockets[socket];

                nlLockSocket(socket, NL_BOTH);
                memcpy(&sock->address, address, sizeof(NLaddress));
                nlUnlockSocket(socket, NL_BOTH);
            }
        }
        else
        {
            nlSetError(NL_INVALID_SOCKET);
            return NL_FALSE;
        }
        return NL_TRUE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Gets the local address.
*/

NL_EXP NLboolean NL_APIENTRY nlGetLocalAddr(NLsocket socket, NLaddress *address)
{
    if(driver)
    {
        if(nlIsValidSocket(socket) == NL_TRUE)
        {
            if(address == NULL)
            {
                nlSetError(NL_NULL_POINTER);
                return NL_FALSE;
            }
            nlLockSocket(socket, NL_READ);
            driver->GetLocalAddr(socket, address);
            nlUnlockSocket(socket, NL_READ);
        }
        else
        {
            nlSetError(NL_INVALID_SOCKET);
            return NL_FALSE;
        }
        return NL_TRUE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

NL_EXP NLaddress * NL_APIENTRY nlGetAllLocalAddr(/*@out@*/ NLint *count)
{
    if(driver)
    {
        if(count == NULL)
        {
            nlSetError(NL_NULL_POINTER);
            return NULL;
        }
        return driver->GetAllLocalAddr(count);
    }
    nlSetError(NL_NO_NETWORK);
    return NULL;
}

NL_EXP NLboolean NL_APIENTRY nlSetLocalAddr(const NLaddress *address)
{
    if(driver)
    {
        if(address == NULL)
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        driver->SetLocalAddr(address);
        return NL_TRUE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Resolves the name from the address.
*/

NL_EXP /*@null@*/ NLchar* NL_APIENTRY nlGetNameFromAddr(const NLaddress *address, NLchar *name)
{
    if(driver)
    {
        if((name == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NULL;
        }
        return (driver->GetNameFromAddr(address, name));
    }
    nlSetError(NL_NO_NETWORK);
    return NULL;
}

/*
   Resolves the name from the address asynchronously.
*/

NL_EXP NLboolean NL_APIENTRY nlGetNameFromAddrAsync(const NLaddress *address, NLchar *name)
{
    if(driver)
    {
        if((name == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        return driver->GetNameFromAddrAsync(address, name);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Get the address from a host name.
*/

NL_EXP NLboolean NL_APIENTRY nlGetAddrFromName(const NLchar *name, NLaddress *address)
{
    if(driver)
    {
        if((name == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        if(isSafeString(name) == NL_FALSE)
        {
            nlSetError(NL_STRING_OVER_RUN);
            return NL_FALSE;
        }
        return driver->GetAddrFromName(name, address);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Get the address from a host name asynchronously.
*/

NL_EXP NLboolean NL_APIENTRY nlGetAddrFromNameAsync(const NLchar *name, NLaddress *address)
{
    if(driver)
    {
        if((name == NULL) || (address == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        if(isSafeString(name) == NL_FALSE)
        {
            nlSetError(NL_STRING_OVER_RUN);
            return NL_FALSE;
        }
        return driver->GetAddrFromNameAsync(name, address);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Compare two addresses.
*/

NL_EXP NLboolean NL_APIENTRY nlAddrCompare(const NLaddress *address1, const NLaddress *address2)
{
    if(driver)
    {
        if((address1 == NULL) || (address2 == NULL))
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        return driver->AddrCompare(address1, address2);
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}

/*
   Get the port number from an address.
*/

NL_EXP NLushort NL_APIENTRY nlGetPortFromAddr(const NLaddress *address)
{
    if(driver)
    {
        if(address == NULL)
        {
            nlSetError(NL_NULL_POINTER);
            return 0;
        }
        return driver->GetPortFromAddr(address);
    }
    nlSetError(NL_NO_NETWORK);
    return 0;
}

/*
   Set the port number in the address.
*/

NL_EXP NLboolean NL_APIENTRY nlSetAddrPort(NLaddress *address, NLushort port)
{
    if(driver)
    {
        if(address == NULL)
        {
            nlSetError(NL_NULL_POINTER);
            return NL_FALSE;
        }
        driver->SetAddrPort(address, port);
        return NL_TRUE;
    }
    nlSetError(NL_NO_NETWORK);
    return NL_FALSE;
}


/*

  Non-socket functions

*/

/*
   Select the network to use.
*/

NL_EXP NLboolean NL_APIENTRY nlSelectNetwork(NLenum network)
{
    int i, found = 0;

    if(driver != NULL)
    {
        /* we cannot select a new network without shutting down first */
        nlSetError(NL_SELECT_NET_ERROR);
        return NL_FALSE;
    }

    for(i=0;i<MAX_NET_DRIVERS;i++)
    {
        if(netdrivers[i].name == NULL)
        {
            break;
        }
        if(netdrivers[i].type == network)
        {
            found++;
            if(netdrivers[i].initialized == NL_TRUE)
            {
                driver = &netdrivers[i];
                return NL_TRUE;
            }
        }
    }
    if(found > 0)
    {
        nlSetError(NL_INVALID_TYPE);
    }
    else
    {
        nlSetError(NL_INVALID_ENUM);
    }
    return NL_FALSE;
}

/*
   Returns a string corresponding to the NLenum.
*/

NL_EXP const /*@observer@*//*@null@*/ NLchar* NL_APIENTRY nlGetString(NLenum name)
{
	/* use seperate strings for thread safety */
    static NLchar   vstring[NL_MAX_STRING_LENGTH];
    static NLchar   tstring[NL_MAX_STRING_LENGTH];
    NLint i;

    /* intitialize the version string */
    _tcscpy(vstring, (NLchar *)TEXT(NL_VERSION_STRING));
#ifdef _UNICODE
    /* add the UNICODE string */
    _tcscat(vstring, (NLchar *)TEXT(" UNICODE version"));
#endif
    /* intitialize the network types string */
    memset(tstring, 0, sizeof(NLchar) * NL_MAX_STRING_LENGTH);
    for(i=0;i<MAX_NET_DRIVERS;i++)
    {
        if(netdrivers[i].name == NULL)
        {
            break;
        }
        if(netdrivers[i].initialized == NL_TRUE)
        {
            _tcscat((NLchar *)tstring, (const NLchar *)netdrivers[i].name);
            _tcscat((NLchar *)tstring, (NLchar *)TEXT(" "));
        }
    }

    switch (name) {

    case NL_VERSION:
		return (const NLchar*)vstring;

    case NL_NETWORK_TYPES:
		return (const NLchar*)tstring;

    case NL_CONNECTION_TYPES:
        if(driver != NULL)
        {
		    return (const NLchar*)(driver->connections);
        }
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
    }

    return NULL;
}

/*
   Returns an integer corresponding to the NLenum.
*/

NL_EXP NLlong NL_APIENTRY nlGetInteger(NLenum name)
{
    switch (name) {

    case NL_PACKETS_SENT:
        return nlOutstats.packets;

    case NL_BYTES_SENT:
        return nlOutstats.bytes;

    case NL_AVE_BYTES_SENT:
        nlUpdateOutStats(0, 0);
        return nlOutstats.average;

    case NL_HIGH_BYTES_SENT:
        return nlOutstats.highest;

    case NL_PACKETS_RECEIVED:
        return nlInstats.packets;

    case NL_BYTES_RECEIVED:
        return nlInstats.bytes;

    case NL_AVE_BYTES_RECEIVED:
        nlUpdateInStats(0, 0);
        return nlInstats.average;

    case NL_HIGH_BYTES_RECEIVED:
        return nlInstats.highest;

    case NL_OPEN_SOCKETS:
        return nlNumsockets;

    default:
        nlSetError(NL_INVALID_ENUM);
    }
    return 0;
}

/*
   Clears the stat corresponding to the NLenum.
*/

NL_EXP NLboolean NL_APIENTRY nlClear(NLenum name)
{
    switch (name) {

    case NL_PACKETS_SENT:
        nlMutexLock(&outstatlock);
        nlOutstats.packets = 0;
        nlMutexUnlock(&outstatlock);
        break;

    case NL_BYTES_SENT:
        nlMutexLock(&outstatlock);
        nlOutstats.bytes = 0;
        nlMutexUnlock(&outstatlock);
        break;

    case NL_AVE_BYTES_SENT:
        nlMutexLock(&outstatlock);
        nlOutstats.average = 0;
        memset((NLbyte *)nlOutstats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        nlMutexUnlock(&outstatlock);
        break;

    case NL_HIGH_BYTES_SENT:
        nlMutexLock(&outstatlock);
        nlOutstats.highest = 0;
        nlMutexUnlock(&outstatlock);
        break;

    case NL_PACKETS_RECEIVED:
        nlMutexLock(&instatlock);
        nlInstats.packets = 0;
        nlMutexUnlock(&instatlock);
        break;

    case NL_BYTES_RECEIVED:
        nlMutexLock(&instatlock);
        nlInstats.bytes = 0;
        nlMutexUnlock(&instatlock);
        break;

    case NL_AVE_BYTES_RECEIVED:
        nlMutexLock(&instatlock);
        nlInstats.average = 0;
        memset((NLbyte *)nlInstats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        nlMutexUnlock(&instatlock);
        break;

    case NL_HIGH_BYTES_RECEIVED:
        nlMutexLock(&instatlock);
        nlInstats.highest = 0;
        nlMutexUnlock(&instatlock);
        break;

    case NL_ALL_STATS:
        nlClear(NL_PACKETS_SENT);
        nlClear(NL_BYTES_SENT);
        nlClear(NL_AVE_BYTES_SENT);
        nlClear(NL_HIGH_BYTES_SENT);
        nlClear(NL_PACKETS_RECEIVED);
        nlClear(NL_BYTES_RECEIVED);
        nlClear(NL_AVE_BYTES_RECEIVED);
        nlClear(NL_HIGH_BYTES_RECEIVED);
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
    return NL_TRUE;
}

/*
   Get the socket or system error.
*/

NL_EXP NLint NL_APIENTRY nlGetSystemError(void)
{
    if(driver)
    {
        return driver->GetSystemError();
    }
    nlSetError(NL_NO_NETWORK);
    return 0;
}

NL_EXP NLboolean NL_APIENTRY nlEnable(NLenum name)
{
    switch (name) {

    case NL_BLOCKING_IO:
        nlBlocking = NL_TRUE;
        break;

    case NL_TCP_NO_DELAY:
        nlHint(NL_TCP_NO_DELAY, (NLint)NL_TRUE);
        break;

    case NL_SOCKET_STATS:
        nlState.socketStats = NL_TRUE;
        break;

    case NL_BIG_ENDIAN_DATA:
        nlState.nl_big_endian_data = NL_TRUE;
        break;

    case NL_LITTLE_ENDIAN_DATA:
        nlState.nl_big_endian_data = NL_FALSE;
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlDisable(NLenum name)
{
    switch (name) {

    case NL_BLOCKING_IO:
        nlBlocking = NL_FALSE;
        break;

    case NL_TCP_NO_DELAY:
        nlHint(NL_TCP_NO_DELAY, NL_FALSE);
        break;

    case NL_SOCKET_STATS:
        nlState.socketStats = NL_FALSE;
        break;

    case NL_BIG_ENDIAN_DATA:
        nlState.nl_big_endian_data = NL_FALSE;
        break;

    case NL_LITTLE_ENDIAN_DATA:
        nlState.nl_big_endian_data = NL_TRUE;
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlGetBoolean(NLenum name)
{
    switch (name) {

    case NL_BLOCKING_IO:
        return nlBlocking;

    case NL_SOCKET_STATS:
        return nlState.socketStats;

    case NL_BIG_ENDIAN_DATA:
        return nlState.nl_big_endian_data;

    case NL_LITTLE_ENDIAN_DATA:
        return (NLboolean)(nlState.nl_big_endian_data == NL_TRUE ? NL_FALSE:NL_TRUE);

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
}

NL_EXP NLlong NL_APIENTRY nlGetSocketStat(NLsocket socket, NLenum name)
{
    nl_socket_t     *sock;
    NLlong          result = 0;

    if(nlIsValidSocket(socket) == NL_FALSE)
    {
        nlSetError(NL_INVALID_SOCKET);
        return 0;
    }
    nlLockSocket(socket, NL_BOTH);
    sock = nlSockets[socket];

    switch (name) {

    case NL_PACKETS_SENT:
        result = sock->outstats.packets;
        break;

    case NL_BYTES_SENT:
        result = sock->outstats.bytes;
        break;

    case NL_AVE_BYTES_SENT:
        nlUpdateSocketOutStats(socket, 0, 0);
        result = sock->outstats.average;
        break;

    case NL_HIGH_BYTES_SENT:
        result = sock->outstats.highest;
        break;

    case NL_PACKETS_RECEIVED:
        result = sock->instats.packets;
        break;

    case NL_BYTES_RECEIVED:
        result = sock->instats.bytes;
        break;

    case NL_AVE_BYTES_RECEIVED:
        nlUpdateSocketInStats(socket, 0, 0);
        result = sock->instats.average;
        break;

    case NL_HIGH_BYTES_RECEIVED:
        result = sock->instats.highest;
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
    }
    nlUnlockSocket(socket, NL_BOTH);
    return result;
}

NL_EXP NLboolean NL_APIENTRY nlClearSocketStat(NLsocket socket, NLenum name)
{
    nl_socket_t     *sock;

    if(nlIsValidSocket(socket) == NL_FALSE)
    {
        nlSetError(NL_INVALID_SOCKET);
        return NL_FALSE;
    }
    nlLockSocket(socket, NL_BOTH);
    sock = nlSockets[socket];

    switch (name) {

    case NL_PACKETS_SENT:
        sock->outstats.packets = 0;
        break;

    case NL_BYTES_SENT:
        sock->outstats.bytes = 0;
        break;

    case NL_AVE_BYTES_SENT:
        sock->outstats.average = 0;
        memset((NLbyte *)sock->outstats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        break;

    case NL_HIGH_BYTES_SENT:
        sock->outstats.highest = 0;
        break;

    case NL_PACKETS_RECEIVED:
        sock->instats.packets = 0;
        break;

    case NL_BYTES_RECEIVED:
        sock->instats.bytes = 0;
        break;

    case NL_AVE_BYTES_RECEIVED:
        sock->instats.average = 0;
        memset((NLbyte *)sock->instats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        break;

    case NL_HIGH_BYTES_RECEIVED:
        sock->instats.highest = 0;
        break;

    case NL_ALL_STATS:
        sock->outstats.packets = 0;
        sock->outstats.bytes = 0;
        sock->outstats.average = 0;
        memset((NLbyte *)sock->outstats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        sock->outstats.highest = 0;
        sock->instats.packets = 0;
        sock->instats.bytes = 0;
        sock->instats.average = 0;
        memset((NLbyte *)sock->instats.bucket, 0, sizeof(NLlong) * NL_NUM_BUCKETS);
        sock->instats.highest = 0;
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        nlUnlockSocket(socket, NL_BOTH);
        return NL_FALSE;
    }
    nlUnlockSocket(socket, NL_BOTH);
    return NL_TRUE;
}

NL_EXP NLushort  NL_APIENTRY nlSwaps(NLushort x)
{
    if(NL_SWAP_TRUE)
    {
        return (NLushort)(((((NLushort)x) & 0x00ff) << 8) | ((((NLushort)x) & 0xff00) >> 8));
    }
    else
    {
        return x;
    }
}

NL_EXP NLulong   NL_APIENTRY nlSwapl(NLulong x)
{
    if(NL_SWAP_TRUE)
    {
        return (NLulong)(((((NLulong)x) & 0x000000ff) << 24) | ((((NLulong)x) & 0x0000ff00) << 8) | ((((NLulong)x) & 0x00ff0000) >> 8) | ((((NLulong)x) & 0xff000000) >> 24));
    }
    else
    {
        return x;
    }
}

NL_EXP NLfloat   NL_APIENTRY nlSwapf(NLfloat f)
{
    if(NL_SWAP_TRUE)
    {
        NLulong temp = (NLulong)nlSwapl(*(NLulong *)&f);

        return *((float *)&temp);
    }
    else
    {
        return f;
    }
}

NL_EXP NLdouble  NL_APIENTRY nlSwapd(NLdouble d)
{
    if(NL_SWAP_TRUE)
    {
        union {NLulong l[2]; NLdouble d;} in, out;

        in.d = d;
        out.l[0] = nlSwapl(in.l[1]);
        out.l[1] = nlSwapl(in.l[0]);
        return out.d;
    }
    else
    {
        return d;
    }
}

