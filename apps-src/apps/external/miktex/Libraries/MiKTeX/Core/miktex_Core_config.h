/**
 * @file config.h
 * @author Christian Schenk
 * @brief Internal configuration
 *
 * @copyright Copyright © 1996-2023 Christian Schenk
 *
 * This file is part of the MiKTeX Core Library.
 *
 * The MiKTeX Core Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#include <miktex/First>

#ifdef _WIN32
#define HAVE_ATLBASE_H 1
#endif

/* #undef HAVE_DIRENT_H */
#define HAVE_INTTYPES_H 1
/* #undef HAVE_SYS_MMAN_H */
/* #undef HAVE_SYS_STATVFS_H */
#define HAVE_SYS_STAT_H 1
/* #undef HAVE_SYS_TIME_H */
#define HAVE_SYS_UTIME_H 1
/* #undef HAVE_SYS_UTSNAME_H */
/* #undef HAVE_SYS_WAIT_H */
/* #undef HAVE_UNISTD_H */
/* #undef HAVE_UTIME_H */

/* #undef HAVE_CHOWN */
/* #undef HAVE_CONFSTR */
/* #undef HAVE_FORK */
/* #undef HAVE_FUTIMES */
/* #undef HAVE_MMAP */
/* #undef HAVE_STATVFS */
/* #undef HAVE_UNAME_SYSCALL */
/* #undef HAVE_VFORK */

/* #undef HAVE_STRUCT_DIRENT_D_TYPE */

/* #undef CMAKE_USE_PTHREADS_INIT */

#if defined(CMAKE_USE_PTHREADS_INIT) && CMAKE_USE_PTHREADS_INIT
#  define HAVE_PTHREAD 1
#endif

/* #undef REPORT_EVENTS */

/* #undef USE_SYSTEM_OPENSSL_CRYPTO */
// #define WITH_LIBRESSL_CRYPTO 1

#if defined(WITH_LIBRESSL_CRYPTO) || defined(USE_SYSTEM_OPENSSL_CRYPTO)
#  define ENABLE_OPENSSL 1
#endif

#define MIKTEX_SESSION_TLB "MiKTeX251000-session.tlb"

#define MIKTEX_SOURCE_DIR "C:/ddksample/apps-src/apps/external/3rdparty/miktex"
#define MIKTEX_BINARY_DIR "C:/ddksample/miktex/output"

#define MIKTEX_BINARY_DESTINATION_DIR "texmf/miktex/bin/x64"
#define MIKTEX_INTERNAL_BINARY_DESTINATION_DIR "texmf/miktex/bin/x64/internal"

#if defined(MIKTEX_MACOS_BUNDLE)
#  define MIKTEX_MACOS_DESTINATION_DIR ""
#endif

#define MIKTEX_FNDB_VERSION 5

#define EAD86981C92C904D808A5E6CEC64B90E

#if defined(MIKTEX_CORE_SHARED)
#  define MIKTEXCOREEXPORT MIKTEXDLLEXPORT
#else
#  define MIKTEXCOREEXPORT
#endif
