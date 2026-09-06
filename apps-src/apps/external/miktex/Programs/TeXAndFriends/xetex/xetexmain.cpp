#define C4PEXTERN extern
// #include "C:/ddksample/miktex/output/Programs/TeXAndFriends/xetex/xetexd.h"
// #include "C:/ddksample/miktex/Programs/TeXAndFriends/xetex/miktex-xetex.h"
#include "xetexd.h"
#include "miktex-xetex.h"

MIKTEX_DEFINE_WEBAPP(MiKTeX_XETEX,
                     XETEX,
                     g_XETEXApp,
                     XeTeXProgram,
                     g_XeTeXProg)
