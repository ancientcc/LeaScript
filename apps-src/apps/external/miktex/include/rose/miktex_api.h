/* luatex-api.h

   Copyright 2006-2012 Taco Hoekwater <taco@luatex.org>

   This file is part of LuaTeX.

   LuaTeX is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   LuaTeX is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU General Public License along
   with LuaTeX; if not, see <http://www.gnu.org/licenses/>. */

#ifndef MIKTEX_ROSE_API_H
#define MIKTEX_ROSE_API_H

#  include <stdlib.h>
#  include <stdio.h>
#  include <stdarg.h>


// #define SDLCALL __cdecl
typedef struct
{
    void (* woutput_fputc)(int ch, void* user);
	void (* woutput_fputs)(const char* c_str, void* user);
    void (* woutput_fputs_l)(const char* c_str, int l, void* user);
    void (* woutput_fprintf)(void* user, const char *fmt, ...);
    void (* woutput_cr)(void* user);
    void* user;
} tluatex_hook;
extern tluatex_hook curr_hook;

#endif                          /* MIKTEX_ROSE_API_H */