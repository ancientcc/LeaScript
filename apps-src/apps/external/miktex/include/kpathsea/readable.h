/**
 * @file kpathsea/kpathsea.h
 * @author Christian Schenk
 * @brief kpathsea emulation
 *
 * @copyright Copyright © 2000-2024 Christian Schenk
 *
 * This file is part of the MiKTeX KPSEMU Library.
 *
 * MiKTeX KPSEMU Library is licensed under GNU General Public License version 2
 * or any later version.
 */

#pragma once

#include <miktex/First.h>
#include <miktex/KPSE/Emulation.h>

// C:\ddksample\miktex\output\Libraries\3rd\getopt\include\getopt.h
#include <getopt/source/posix/getopt.h>

// kpathsea/c-auto.in
/* #undef HAVE_DIRENT_H */
/* #undef HAVE_UNISTD_H */
#define MAKE_TEX_TFM_BY_DEFAULT 1
#define MAKE_TEX_TEX_BY_DEFAULT 1

// kpathsea/c-dir.h
#if defined(HAVE_DIRENT_H)
#include <dirent.h>
#endif

// kpathsea/c-fopen.h
#include <fcntl.h>
#if !defined(O_BINARY)
#if defined(MIKTEX_WINDOWS)
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

// kpathsea/c-stat.h
#include <sys/stat.h>

// kpathsea/c-std.h
#if defined(__cplusplus)
#include <cstdarg>
#else
#include <stdarg.h>
#endif

// kpathsea/c-unistd.h
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

// kpathsea/simpletypes.h
#if !defined(TRUE)
#if defined(__cplusplus)
#define TRUE true
#else
#define TRUE 1
#endif
#endif
#if !defined(FALSE)
#if defined(__cplusplus)
#define FALSE false
#else
#define FALSE 0
#endif
#endif

// kpathsea/types.h
#define KPATHSEA_TYPES_H
