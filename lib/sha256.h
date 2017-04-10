#ifndef __CURL_SHA256_H
#define __CURL_SHA256_H
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 ***************************************************************************
 *
 * This SHA-256 implementation was modified for libcurl, refer to sha256.c.
 */
/*
*   SHA-256 implementation.
*
*   Copyright (c) 2010 Ilya O. Levin, http://www.literatecode.com
*
*   Permission to use, copy, modify, and distribute this software for any
*   purpose with or without fee is hereby granted, provided that the above
*   copyright notice and this permission notice appear in all copies.
*
*   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
*   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
*   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
*   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
*   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
*   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
*   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include "curl_setup.h"

#ifndef CURL_DISABLE_SHA256
   typedef struct {
       unsigned int buf[16];
       unsigned int hash[8];
       unsigned int len[2];
   } sha256_context;

   void sha256_init(sha256_context *);
   void sha256_hash(sha256_context *, unsigned char * /* data */,
                    unsigned int /* len */);
   void sha256_done(sha256_context *, unsigned char * /* hash */);
#endif /* !CURL_DISABLE_SHA256 */
#endif
