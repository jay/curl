/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
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

#ifdef WIN32

 /*
  * MultiByte conversions using Windows kernel32 library.
  */

#include "curl_multibyte.h"
#include "curl_memory.h"

/* The last #include file should be: */
#include "memdebug.h"

wchar_t *Curl_convert_UTF8_to_wchar(const char *str_utf8)
{
  wchar_t *str_w = NULL;

  if(str_utf8) {
    int str_w_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        str_utf8, -1, NULL, 0);
    if(str_w_len > 0) {
      str_w = malloc(str_w_len * sizeof(wchar_t));
      if(str_w) {
        if(MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, str_w,
                               str_w_len) == 0) {
          free(str_w);
          return NULL;
        }
      }
    }
  }

  return str_w;
}

char *Curl_convert_wchar_to_UTF8(const wchar_t *str_w)
{
  char *str_utf8 = NULL;

  if(str_w) {
    int str_utf8_len = WideCharToMultiByte(CP_UTF8, 0, str_w, -1, NULL,
                                           0, NULL, NULL);
    if(str_utf8_len > 0) {
      str_utf8 = malloc(str_utf8_len * sizeof(wchar_t));
      if(str_utf8) {
        if(WideCharToMultiByte(CP_UTF8, 0, str_w, -1, str_utf8, str_utf8_len,
                               NULL, FALSE) == 0) {
          free(str_utf8);
          return NULL;
        }
      }
    }
  }

  return str_utf8;
}

FILE *win32_fopen(const char *filename, const char *mode)
{
  if(g_curl_tool_args_are_utf8 && filename && *filename && mode && *mode) {
    /* filename might be UTF-8 from the command line */
    const wchar_t *filename_w = Curl_convert_UTF8_to_wchar(filename);
    const wchar_t *mode_w = Curl_convert_UTF8_to_wchar(mode);
    if(filename_w && mode_w)
      return _wfopen(filename_w, mode_w);
  }
  return (fopen)(filename, mode);
}

int win32_stat(const char *path, struct _stat *buffer)
{
  if(g_curl_tool_args_are_utf8 && path) {
    /* path might be UTF-8 from the command line */
    const wchar_t *path_w = Curl_convert_UTF8_to_wchar(path);
    if(path_w)
      return _wstat(path_w, buffer);
  }
  return (_stat)(path, buffer);
}

int win32_stat64(const char *path, struct __stat64 *buffer)
{
  if(g_curl_tool_args_are_utf8 && path) {
    /* path might be UTF-8 from the command line */
    const wchar_t *path_w = Curl_convert_UTF8_to_wchar(path);
    if(path_w)
      return _wstat64(path_w, buffer);
  }
  return (_stat64)(path, buffer);
}

int win32_stati64(const char *path, struct _stati64 *buffer)
{
  if(g_curl_tool_args_are_utf8 && path) {
    /* path might be UTF-8 from the command line */
    const wchar_t *path_w = Curl_convert_UTF8_to_wchar(path);
    if(path_w)
      return _wstati64(path_w, buffer);
  }
  return (_stati64)(path, buffer);
}

#endif /* WIN32 */
