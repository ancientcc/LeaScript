/* pdftexextra.c: Hand-coded routines for pdfTeX.

   This file is public domain.  */

#if defined(MIKTEX)
#include "../pdftex/miktex-pdftex.h"
using namespace MiKTeX::TeXAndFriends;
#else
#define	EXTERN /* Instantiate data from pdftexd.h here.  */
#define DLLPROC dllpdftexmain

/* This file defines TeX and pdfTeX.  */
#include <pdftexd.h>
#endif


/* Hand-coded routines for TeX or Metafont in C.  */
#include <web2c/source/lib/texmfmp.c>
