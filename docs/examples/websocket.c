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
/* <DESC>
 * WebSocket using CONNECT_ONLY
 * </DESC>
 */
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define sleep(s) Sleep((DWORD)((s)*1000))
#else
#include <unistd.h>
#endif

#include <curl/curl.h>

/* Auxiliary function that waits on the socket.
   If 'for_recv' is true it waits until data is received, if false it waits
   until data can be sent on the socket.
   If 'timeout_ms' is negative then it waits indefinitely.
   */
static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms)
{
  struct timeval tv;
  fd_set infd, outfd, errfd;
  int res;

  if(timeout_ms >= 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (int)(timeout_ms % 1000) * 1000;
  }

  FD_ZERO(&infd);
  FD_ZERO(&outfd);
  FD_ZERO(&errfd);

/* Avoid this warning with pre-2020 Cygwin/MSYS releases:
 * warning: conversion to 'long unsigned int' from 'curl_socket_t' {aka 'int'}
 * may change the sign of the result [-Wsign-conversion]
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
  FD_SET(sockfd, &errfd); /* always check for error */

  if(for_recv) {
    FD_SET(sockfd, &infd);
  }
  else {
    FD_SET(sockfd, &outfd);
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  /* select() returns the number of signalled sockets or -1 */
  res = select((int)sockfd + 1, &infd, &outfd, &errfd,
               ((timeout_ms >= 0) ? &tv : NULL));
  return res;
}

struct ws_message {
  int type;
  size_t len;
  size_t data_alloc_size;
  char data[1]; /* expanded memory, must be last element in struct */
};


/* blocking_ws_recv shows how to block waiting for a complete message of type
   CURLWS_TEXT, CURLWS_BINARY, CURLWS_PING, CURLWS_PONG or CURLWS_CLOSE.

   This function calls the low-level function curl_ws_recv repeatedly for parts
   of a message until the whole message is received in memory.

   Websockets allows for control messages (PING, PONG or CLOSE) to interrupt
   non-control messages (TEXT or BINARY). A complete control message is
   expected to be processed immediately. For that reason, this function may
   return a 'complete' message that is a control message and an 'incomplete'
   message at the same time. After the complete control message is handled, if
   this function is called again then the caller must pass the incomplete
   message to wait for its completion.

   'complete': a ws_message with a complete message. for input the caller must
   pass a pointer to a NULL pointer.

   'incomplete': a ws_message with an incomplete non-control message. for input
   the caller must pass a pointer to a NULL pointer or a pointer to the pointer
   to the previously returned incomplete message if there was one.

   The caller must free 'complete' and 'incomplete' on return if they point to
   messages.

   CURLE_OK: 'complete' contains a complete message. 'incomplete' contains an
   incomplete message or is set to NULL.

   CURLE_ error codes other than CURLE_OK: 'complete' is set to NULL and
   'incomplete' contains an incomplete message or is set to NULL.

   CURLE_GOT_NOTHING: the server closed the connection.
   */
static CURLcode blocking_ws_recv(
  CURL *curl, struct ws_message **complete, struct ws_message **incomplete)
{
  CURLcode result = CURLE_OK;
  curl_socket_t sockfd = CURL_SOCKET_BAD;
  struct ws_message *msg, *disrupted = NULL;

  if(curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd) ||
     sockfd == CURL_SOCKET_BAD) {
    fprintf(stderr, "blocking_ws_recv: unexpected dead connection\n");
    return CURLE_RECV_ERROR;
  }

  if(!complete || *complete || !incomplete ||
     (*incomplete && !(*incomplete)->type))
    return CURLE_BAD_FUNCTION_ARGUMENT;

  if(*incomplete) {
    msg = *incomplete;
    *incomplete = NULL;
  }
  else {
    size_t data_alloc_size = 1;
    msg = calloc(1, (sizeof(*msg) + data_alloc_size));
    if(!msg) {
      fprintf(stderr, "blocking_ws_recv: out of memory\n");
      return CURLE_OUT_OF_MEMORY;
    }
    msg->data_alloc_size = data_alloc_size;
  }

  for(;;) {
    int rtype;
    size_t rlen;
    const struct curl_ws_frame *meta;

    /* grow the buffer. for example purposes the size is incremented by only 1
       byte to force several loop iterations. */
    if(msg->len == msg->data_alloc_size) {
      struct ws_message *temp;
      size_t more = 1;
      if((sizeof(*msg) + msg->data_alloc_size) >
         (sizeof(*msg) + msg->data_alloc_size + more)) {
        fprintf(stderr, "blocking_ws_recv: out of memory\n");
        result = CURLE_OUT_OF_MEMORY;
        break;
      }
      temp = realloc(msg, sizeof(*msg) + msg->data_alloc_size + more);
      if(!temp) {
        fprintf(stderr, "blocking_ws_recv: out of memory\n");
        result = CURLE_OUT_OF_MEMORY;
        break;
      }
      msg = temp;
      msg->data_alloc_size += more;
    }

    /* continue reading the message into the buffer */
    result = curl_ws_recv(curl, &msg->data[msg->len],
                          msg->data_alloc_size - msg->len,
                          &rlen, &meta);
    if(result == CURLE_AGAIN) {
      int sockres;
      fprintf(stderr, "blocking_ws_recv: waiting for socket to have data\n");
      sockres = wait_on_socket(sockfd, 1, -1);
      if(sockres == 1)
        continue;
      else if(sockres == 0) {
        fprintf(stderr, "blocking_ws_recv: timed out waiting for socket\n");
        result = CURLE_OPERATION_TIMEDOUT;
        break;
      }
      else {
        fprintf(stderr, "blocking_ws_recv: socket error\n");
        result = CURLE_RECV_ERROR;
        break;
      }
    }
    /* CURLE_GOT_NOTHING means the connection was closed by the server */
    else if(result == CURLE_GOT_NOTHING) {
      fprintf(stderr, "blocking_ws_recv: connection closed\n");
      break;
    }
    else if(result) {
      fprintf(stderr, "curl_ws_recv: (%d) %s\n", result,
              curl_easy_strerror(result));
      break;
    }

    if((meta->flags & CURLWS_TEXT))
      rtype = CURLWS_TEXT;
    else if((meta->flags & CURLWS_BINARY))
      rtype = CURLWS_BINARY;
    else if((meta->flags & CURLWS_PING))
      rtype = CURLWS_PING;
    else if((meta->flags & CURLWS_PONG))
      rtype = CURLWS_PONG;
    else if((meta->flags & CURLWS_CLOSE))
      rtype = CURLWS_CLOSE;
    else {
      fprintf(stderr, "blocking_ws_recv: unknown frame type. "
              "meta->flags: %d\n", meta->flags);
      result = CURLE_RECV_ERROR;
      break;
    }

    if(!msg->type)
      msg->type = rtype;
    else if(msg->type != rtype) {
      /* A different frame type may not be an error. According to RFC6455,
         control frames (PING, PONG, CLOSE) may be injected in the middle of a
         fragmented message. Control frames cannot be fragmented themselves. */

      if((msg->type == CURLWS_TEXT || msg->type == CURLWS_BINARY) &&
         (rtype == CURLWS_PING || rtype == CURLWS_PONG ||
          rtype == CURLWS_CLOSE) &&
         !(meta->flags & CURLWS_CONT) && !disrupted) {
        /* move the control message into its own ws_message */
        struct ws_message *temp = calloc(1, sizeof(*temp) + rlen);
        if(!temp) {
          fprintf(stderr, "blocking_ws_recv: out of memory\n");
          result = CURLE_OUT_OF_MEMORY;
          break;
        }
        memcpy(temp->data, &msg->data[msg->len], rlen);
        temp->len = rlen;
        temp->data_alloc_size = rlen;
        temp->type = rtype;
        disrupted = msg;
        msg = temp;
        if(meta->bytesleft) {
          fprintf(stderr, "blocking_ws_recv: incomplete control message, "
                  "trying again\n");
          continue;
        }
        else
          break;
      }
      else {
        fprintf(stderr, "blocking_ws_recv: unexpected frame type\n");
        result = CURLE_RECV_ERROR;
        break;
      }
    }

    msg->len += rlen;

    /* the message is incomplete if the current fragment is incomplete
       (meta->bytesleft) or there are more fragments to come (CURLWS_CONT) */
    if(meta->bytesleft || (meta->flags & CURLWS_CONT)) {
      fprintf(stderr, "blocking_ws_recv: incomplete message, trying again\n");
      continue;
    }
    else
      break;
  }

  if(!result) {
    *complete = msg;
    *incomplete = (disrupted ? disrupted : NULL);
  }
  else {
    *complete = NULL;

    if(disrupted) {
      *incomplete = disrupted;
      /* It may happen that there are two incomplete messages, an incomplete
         non-control message 'disrupted' that was disrupted by an incomplete
         control message 'msg' which is discarded */
      free(msg);
    }
    else if(msg->type)
      *incomplete = msg;
    else {
      *incomplete = NULL;
      free(msg);
    }
  }

  return result;
}

static int ping(CURL *curl, const char *send_payload)
{
  size_t sent;
  CURLcode result =
    curl_ws_send(curl, send_payload, strlen(send_payload), &sent, 0,
                 CURLWS_PING);
  return (int)result;
}

static int recv_pong(CURL *curl, const char *expected_payload)
{
  CURLcode result;
  struct ws_message *complete = NULL, *incomplete = NULL;

  /* an 'incomplete' websocket message is saved and restored in the private
     data since it must be passed to a subsequent blocking_ws_recv call */
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &incomplete);
  result = blocking_ws_recv(curl, &complete, &incomplete);
  curl_easy_setopt(curl, CURLOPT_PRIVATE, incomplete);

  if(!result && complete->type == CURLWS_PONG) {
    fprintf(stderr, "ws: got PONG back\n");
    if(complete->len == strlen(expected_payload) &&
       !memcmp(expected_payload, complete->data, complete->len))
      fprintf(stderr, "ws: got the same payload back\n");
    else
      fprintf(stderr, "ws: did NOT get the same payload back\n");
  }
  else {
    fprintf(stderr, "ws: did not get PONG back\n");
  }

  free(complete);
  return (int)result;
}

static CURLcode recv_header(CURL *curl)
{
  CURLcode result;
  struct ws_message *complete = NULL, *incomplete = NULL;

  /* an 'incomplete' websocket message is saved and restored in the private
     data since it must be passed to a subsequent blocking_ws_recv call */
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &incomplete);
  result = blocking_ws_recv(curl, &complete, &incomplete);
  curl_easy_setopt(curl, CURLOPT_PRIVATE, incomplete);

  if(!result && complete->type == CURLWS_TEXT) {
    fprintf(stderr, "ws: got TEXT header: %.*s\n",
            complete->len, complete->data);
  }
  else {
    fprintf(stderr, "ws: did not get TEXT header\n");
  }

  free(complete);
  return (int)result;
}

/* close the connection */
static void websocket_close(CURL *curl)
{
  size_t sent;
  (void)curl_ws_send(curl, "", 0, &sent, 0, CURLWS_CLOSE);
}

static void websocket(CURL *curl)
{
  int i = 0;

  /* header "Request served by xxxxxxxxxxxxxx" */
  recv_header(curl);

  do {
    if(ping(curl, "foobar"))
      return;
    if(recv_pong(curl, "foobar"))
      return;
    sleep(2);
  } while(i++ < 10);

  websocket_close(curl);
}

int main(void)
{
  CURL *curl;
  CURLcode res;
  struct ws_message *incomplete = NULL;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "wss://echo.websocket.org");

    /* an 'incomplete' websocket message is saved and restored in the private
       data since it must be passed to a subsequent blocking_ws_recv call.
       here it is initialized to NULL. */
    curl_easy_setopt(curl, CURLOPT_PRIVATE, NULL);

    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L); /* websocket style */

    /* Perform the request, res gets the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    else {
      /* connected and ready */
      websocket(curl);
    }

    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &incomplete);
    free(incomplete);

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}
