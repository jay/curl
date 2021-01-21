/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2019 - 2020, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include "curl_setup.h"
#include "socketpair.h"
#include "strerror.h"

#if !defined(HAVE_SOCKETPAIR) && !defined(CURL_DISABLE_SOCKETPAIR)
#ifdef WIN32
/*
 * This is a socketpair() implementation for Windows.
 */
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#else
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h> /* IPPROTO_TCP */
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif /* !INADDR_LOOPBACK */
#endif /* !WIN32 */

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

int Curl_socketpair(int domain, int type, int protocol,
                    curl_socket_t socks[2])
{
  union {
    struct sockaddr_in inaddr;
    struct sockaddr addr;
  } a;
  curl_socket_t listener;
  curl_socklen_t addrlen = sizeof(a.inaddr);
  int reuse = 1;
  char data[2][12];
  ssize_t dlen;
  (void)domain;
  (void)type;
  (void)protocol;

  WSASetLastError(0);

#define WSAERR(func) \
do { \
  char buffer[STRERROR_LEN]; \
  DWORD gle = WSAGetLastError(); \
  Curl_strerror(gle, buffer, sizeof(buffer)); \
  fprintf(stderr, func " failed, line %u, WSAGetLastError %u: %s\n", \
          __LINE__, gle, buffer); \
  goto error; \
} while(0)

  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(listener == CURL_SOCKET_BAD)
    WSAERR("socket listener");

  memset(&a, 0, sizeof(a));
  a.inaddr.sin_family = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port = 0;

  socks[0] = socks[1] = CURL_SOCKET_BAD;

  if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                (char *)&reuse, (curl_socklen_t)sizeof(reuse)) == -1)
    WSAERR("setsockopt listener");
  if(bind(listener, &a.addr, sizeof(a.inaddr)) == -1)
    WSAERR("bind listener");
  if(getsockname(listener, &a.addr, &addrlen) == -1)
    WSAERR("getsockname listener");
  if(listen(listener, 1) == -1)
    WSAERR("listen listener");
  socks[0] = socket(AF_INET, SOCK_STREAM, 0);
  if(socks[0] == CURL_SOCKET_BAD)
    WSAERR("socket socks[0]");
  if(connect(socks[0], &a.addr, sizeof(a.inaddr)) == -1)
    WSAERR("connect socks[0]");
  socks[1] = accept(listener, NULL, NULL);
  if(socks[1] == CURL_SOCKET_BAD)
    WSAERR("accept listener socks[1]");

  /* verify that nothing else connected */
  msnprintf(data[0], sizeof(data[0]), "%p", socks);
  dlen = strlen(data[0]);
  if(swrite(socks[0], data[0], dlen) != dlen)
    WSAERR("swrite socks[0]");
  if(sread(socks[1], data[1], sizeof(data[1])) != dlen)
    WSAERR("sread socks[1]");
  if(memcmp(data[0], data[1], dlen))
    WSAERR("memcmp");

  sclose(listener);
  return 0;

  error:
  sclose(listener);
  sclose(socks[0]);
  sclose(socks[1]);
  return -1;
}

#endif /* ! HAVE_SOCKETPAIR */
