/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
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
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include "curl_setup.h"
#include "socketpair.h"

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

#include "nonblock.h" /* for curlx_nonblock */
#include "timeval.h"  /* needed before select.h */
#include "select.h"   /* for Curl_poll */

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
  struct pollfd pfd[1];
  (void)domain;
  (void)type;
  (void)protocol;

  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(listener == CURL_SOCKET_BAD)
    return -1;

  memset(&a, 0, sizeof(a));
  a.inaddr.sin_family = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port = 0;

  socks[0] = socks[1] = CURL_SOCKET_BAD;

#define xstr(s) str(s)
#define str(s) #s
#define GOTO_ERROR \
  do { \
    OutputDebugStringA("error - line " ## xstr(__LINE__)); \
    goto error; \
  } while(0)

  if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                (char *)&reuse, (curl_socklen_t)sizeof(reuse)) == -1)
    GOTO_ERROR;
  if(bind(listener, &a.addr, sizeof(a.inaddr)) == -1)
    GOTO_ERROR;
  if(getsockname(listener, &a.addr, &addrlen) == -1 ||
     addrlen < (int)sizeof(a.inaddr))
    GOTO_ERROR;
  if(listen(listener, 1) == -1)
    GOTO_ERROR;
  socks[0] = socket(AF_INET, SOCK_STREAM, 0);
  if(socks[0] == CURL_SOCKET_BAD)
    GOTO_ERROR;
  if(connect(socks[0], &a.addr, sizeof(a.inaddr)) == -1)
    GOTO_ERROR;

  /* use non-blocking accept to make sure we don't block forever */
  if(curlx_nonblock(listener, TRUE) < 0)
    GOTO_ERROR;
  pfd[0].fd = listener;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;
  (void)Curl_poll(pfd, 1, 1000); /* one second */
  socks[1] = accept(listener, NULL, NULL);
  if(socks[1] == CURL_SOCKET_BAD)
    GOTO_ERROR;
  else {
    struct curltime check;
    struct curltime now = Curl_now();
    ssize_t out, in;

    /* write data to the socket */
    WSASetLastError(0);
    out = swrite(socks[0], &now, sizeof(now));
    if(out != sizeof(now)) {
      if(out == SOCKET_ERROR)
        OutputDebugStringA(aprintf("send gle: %d", WSAGetLastError()));
      else
        OutputDebugStringA(aprintf("out: %zd", out));
      GOTO_ERROR;
    }

    /* verify that we read the correct data */
    WSASetLastError(0);
    in = sread(socks[1], &check, sizeof(check));
    if(sizeof(now) != in) {
      if(in == SOCKET_ERROR)
        OutputDebugStringA(aprintf("recv gle: %d", WSAGetLastError()));
      else
        OutputDebugStringA(aprintf("in: %zd", in));
      GOTO_ERROR;
    }

    if(memcmp(&now, &check, sizeof(check)))
      GOTO_ERROR;
  }

  sclose(listener);
  return 0;

  error:
  sclose(listener);
  sclose(socks[0]);
  sclose(socks[1]);
  return -1;
}

#endif /* ! HAVE_SOCKETPAIR */
