/**
 * @file miktex/unxemu-config.h
 * @defgroup Unx emulation
 * @author Christian Schenk
 * @brief Library configuration
 *
 * @copyright Copyright © 2007-2024 Christian Schenk
 *
 * This file is part of the MiKTeX UNXEMU Library.
 *
 * MiKTeX UNXEMU Library is licensed under GNU General Public License version 2
 * or any later version.
 */

#pragma once

#include <miktex/Definitions.h>

// DLL import/export switch
#if !defined(D2A2BA842ACE40C6A8A17A9358F2147E)
#define MIKTEXUNXEXPORT MIKTEXDLLIMPORT
#endif

// API decoration for exported functions
#define MIKTEXUNXCEEAPI(type) MIKTEXUNXEXPORT type MIKTEXCEECALL

#define HAVE_ACCESS 1
#define HAVE_ALLOCA 1
#define HAVE_CHDIR 1
#define HAVE_CHMOD 1
/* #undef HAVE_FINITE */
#define HAVE_GETCWD 1
#define HAVE_GETPID 1
/* #undef HAVE_INDEX */
#define HAVE_MKDIR 1
/* #undef HAVE_MKSTEMP */
#define HAVE_OPEN 1
/* #undef HAVE_PCLOSE */
/* #undef HAVE_POPEN */
/* #undef HAVE_RENAME */
/* #undef HAVE_RINDEX */
#define HAVE_RMDIR 1
#define HAVE_STAT 1
#define HAVE_UNLINK 1
#define HAVE_UTIME 1
