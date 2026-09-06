#define C4PEXTERN extern

// #include "C:/ddksample/miktex/output/Programs/TeXAndFriends/pdftex/pdftex/pdftexd.h"
// #include "C:/ddksample/miktex/Programs/TeXAndFriends/pdftex/pdftex/miktex-pdftex.h"
#include "pdftexd.h"
/*
#if !defined(_stat)
#error sys/stat.h(2.1) must not be included before miktex/utf8wrap.h
#else
#error sys/stat.h(2.2) must not be included before miktex/utf8wrap.h
#endif
*/

// #include "../Programs/TeXAndFriends/pdftex/pdftex/miktex-pdftex.h"
#include "miktex-pdftex.h"
MIKTEX_DEFINE_WEBAPP(MiKTeX_PDFTEX,
                     PDFTEX,
                     g_PDFTEXApp,
                     pdfTeXProgram,
                     g_pdfTeXProg)
