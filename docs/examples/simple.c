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
 * Very simple HTTP GET
 * </DESC>
 */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURL *curl;
  CURLcode res;

#if defined(CURLDEBUG) && defined(_CRTDBG_MAP_ALLOC)
#error "CURLDEBUG and _CRTDBG_MAP_ALLOC both defined, pick one"
#elif defined(CURLDEBUG)
  /* libcurl heap memory tracking. run tests/memanalyze.pl <logfile> */
  {
    const char *env = getenv("CURL_MEMDEBUG");
    if(env)
      curl_dbg_memdebug(env);
  }
#elif defined(_CRTDBG_MAP_ALLOC)
#ifndef _INC_CRTDBG
#error "missing crtdbg.h forced include: cl /FIcrtdbg.h /D_CRTDBG_MAP_ALLOC"
#endif
  /* Windows CRT heap memory tracking. on exit it prints heap memory leaks.
     if libcurl was not built with _CRTDBG_MAP_ALLOC as well then there are no
     filename or line numbers for libcurl allocations in the leak report. */
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#ifdef CURL_FORCEMEMLEAK
  /* MEMORY LEAK TEST. See winbuild\README_HEAP_DEBUG.md. */
  malloc(5);
#endif

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
    /* example.com is redirected, so we tell libcurl to follow redirection */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}
