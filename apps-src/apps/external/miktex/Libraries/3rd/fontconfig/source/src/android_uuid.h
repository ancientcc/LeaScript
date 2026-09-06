/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2010 Behdad Esfahbod
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef FONTCONFIG_ANDROID_UUID_H
#define FONTCONFIG_ANDROID_UUID_H

#ifndef ANDROID
#error it use only for android.
#endif

// my_uuid.h - UUID compatibility for Android
#include <stdint.h>
#include <string.h>
#include <unistd.h>

typedef unsigned char uuid_t[16];

void uuid_generate_random(uuid_t out);
void uuid_unparse(const uuid_t uu, char *out);
int uuid_parse(const char *in, uuid_t uu);

static inline void uuid_copy(uuid_t dest, const uuid_t src) {
    memcpy(dest, src, 16);
}

static inline void uuid_clear(uuid_t uu) {
    memset(uu, 0, 16);
}

static inline int uuid_compare(const uuid_t uu1, const uuid_t uu2) {
    return memcmp(uu1, uu2, 16);
}

#endif /* FONTCONFIG_ANDROID_UUID_H */
