/*
 * Copyright (C) 2010, Willow Garage, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the names of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIBMEDIAPIPE_MACROS_H_INCLUDED
#define LIBMEDIAPIPE_MACROS_H_INCLUDED

#if defined(__GNUC__)
#define MEDIAPIPE_DEPRECATED __attribute__((deprecated))
#define MEDIAPIPE_FORCE_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define MEDIAPIPE_DEPRECATED
#define MEDIAPIPE_FORCE_INLINE __forceinline
#else
#define MEDIAPIPE_DEPRECATED
#define MEDIAPIPE_FORCE_INLINE inline
#endif

/*
  Windows import/export and gnu http://gcc.gnu.org/wiki/Visibility
  macros.
 */
#if defined(_MSC_VER)
    #define MEDIAPIPE_HELPER_IMPORT __declspec(dllimport)
    #define MEDIAPIPE_HELPER_EXPORT __declspec(dllexport)
    #define MEDIAPIPE_HELPER_LOCAL
#elif __GNUC__ >= 4
    #define MEDIAPIPE_HELPER_IMPORT __attribute__ ((visibility("default")))
    #define MEDIAPIPE_HELPER_EXPORT __attribute__ ((visibility("default")))
    #define MEDIAPIPE_HELPER_LOCAL  __attribute__ ((visibility("hidden")))
#else
    #define MEDIAPIPE_HELPER_IMPORT
    #define MEDIAPIPE_HELPER_EXPORT
    #define MEDIAPIPE_HELPER_LOCAL
#endif

#ifdef MEDIAPIPE_BUILD_STATIC_LIBS
#  define MEDIAPIPE_API
#else
#  ifdef MEDIAPIPE_BUILD_SHARED_LIBS // ros is being built around shared libraries
#    define MEDIAPIPE_API MEDIAPIPE_HELPER_EXPORT
#  else // we are using shared lib/dll
#    define MEDIAPIPE_API MEDIAPIPE_HELPER_IMPORT
#  endif
#endif

#endif
