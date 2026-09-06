/* $Id: font.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#ifndef LIBROSE_FONT_HPP_INCLUDED
#define LIBROSE_FONT_HPP_INCLUDED

#include <SDL_ttf.h>
#include "rose_sdl_utils.hpp"

namespace font {

//various standard colors
extern LIB3RDPARTY_DECL const SDL_Color NORMAL_COLOR, GRAY_COLOR, LOBBY_COLOR, GOOD_COLOR, BAD_COLOR,
                       BLACK_COLOR, YELLOW_COLOR, BUTTON_COLOR, BIGMAP_COLOR,
                       PETRIFIED_COLOR, TITLE_COLOR, DISABLED_COLOR, LABEL_COLOR, BLUE_COLOR;

extern LIB3RDPARTY_DECL int SIZE_SMALLEST;
extern LIB3RDPARTY_DECL int SIZE_SMALLER;
extern LIB3RDPARTY_DECL int SIZE_SMALL;
extern LIB3RDPARTY_DECL int SIZE_DEFAULT;
extern LIB3RDPARTY_DECL int SIZE_LARGE;
extern LIB3RDPARTY_DECL int SIZE_LARGER;
extern LIB3RDPARTY_DECL int SIZE_LARGEST;

typedef surface (*fget_rendered_text)(const std::string& text, int maximum_width, int font_size, const SDL_Color& color);
extern LIB3RDPARTY_DECL fget_rendered_text rose_get_rendered_text;

LIB3RDPARTY_DECL SDL_Color SDL_DColor_to_SDL_Color(const SDL_DColor& from);

}

#endif
