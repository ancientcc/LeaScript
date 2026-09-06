/**
 * @file config.h
 * @author Christian Schenk
 * @brief Configuration header file
 *
 * @copyright Copyright © 2021-2022 Christian Schenk
 *
 * This file is part of One MiKTeX Utility.
 *
 * One MiKTeX Utility is licensed under GNU General Public
 * License version 2 or any later version.
 */

/* #undef USE_SYSTEM_FONTCONFIG */

#define WITH_KPSEWHICH 1
#define WITH_MKTEXLSR 1
#define WITH_RUNGS 1
#define WITH_TEXDOC 1
#define WITH_TEXHASH 1
#define WITH_TEXLINKS 1
#define WITH_UPDMAP 1

#if defined(MIKTEX_MACOS_BUNDLE)
#  define MIKTEX_MACOS_BUNDLE_NAME ""
#endif
