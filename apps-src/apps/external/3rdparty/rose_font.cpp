/* $Id: font.cpp 47191 2010-10-24 18:10:29Z mordante $ */
/* vim:set encoding=utf-8: */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "rose_font.hpp"

namespace font {

const SDL_Color NORMAL_COLOR = {0xDD,0xDD,0xDD,0xff},
                GRAY_COLOR   = {0x77,0x77,0x77,0xff},
                LOBBY_COLOR  = {0xBB,0xBB,0xBB,0xff},
                GOOD_COLOR   = {0x00,0xFF,0x00,0xff},
                BAD_COLOR    = {0xFF,0x00,0x00,0xff},
                BLACK_COLOR  = {0x00,0x00,0x00,0xff},
                YELLOW_COLOR = {0xFF,0xFF,0x00,0xff},
                BUTTON_COLOR = {0xBC,0xB0,0x88,0xff},
                PETRIFIED_COLOR = {0xA0,0xA0,0xA0,0xff},
                TITLE_COLOR  = {0xBC,0xB0,0x88,0xff},
				LABEL_COLOR  = {0x6B,0x8C,0xFF,0xff},
				BLUE_COLOR   = {0x00,0x00,0xFF,0xff}, 
				BIGMAP_COLOR = {0xFF,0xFF,0xFF,0xff};
const SDL_Color DISABLED_COLOR = inverse(PETRIFIED_COLOR);

// below variable will update when presetmode
int SIZE_SMALLEST = 16;
int SIZE_SMALLER = 16;
int SIZE_SMALL = 16;
int SIZE_DEFAULT = 16;
int SIZE_LARGE = 16;
int SIZE_LARGER = 16;
int SIZE_LARGEST = 16;

SDL_Color SDL_DColor_to_SDL_Color(const SDL_DColor& from)
{
	double r = from.r * 255;
	double g = from.g * 255;
	double b = from.b * 255;
	double a = from.a * 255;

	int r_n32 = (int)round(r);
	int g_n32 = (int)round(g);
	int b_n32 = (int)round(b);
	int a_n32 = (int)round(a);

	return SDL_Color{(uint8_t)r_n32, (uint8_t)g_n32, (uint8_t)b_n32, (uint8_t)a_n32};
}

fget_rendered_text rose_get_rendered_text = nullptr;

}