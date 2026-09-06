/* $Id: sdl_utils.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/**
 *  @file
 *  Support-routines for the SDL-graphics-library.
 */

#include "rose_global.hpp"

#include "SDL_image.h"
#include "sdl_utils.hpp"
#include "video.hpp"
#include "image.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"
#include "font.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

#include <opencv2/imgproc.hpp>

trose_dbg_texture::~trose_dbg_texture()
{
	if (items_ != nullptr) {
		VALIDATE(game_config::os == os_windows, null_str);

		VALIDATE(item_size_ != 0, null_str);
		// item_vsize_ require is 0. no
		VALIDATE(item_vsize_ == 0, null_str);
		free(items_);

		items_ = nullptr;
	}
}

void trose_dbg_texture::insert_texture(bool is_surf, const SDL_Texture* tex, int w, int h, const std::string& file, const std::string& func, int line)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(game_config::os == os_windows, null_str);
	if (tex == nullptr) {
		return;
	}

	VALIDATE(w > 0 && h > 0, null_str);
	for (int at = 0; at < item_vsize_; at ++) {
		const titem_C& item = items_[at];
		VALIDATE(item.tex != tex, null_str);
	}

	resize_items(item_vsize_ + 1);

	titem_C& item = items_[item_vsize_];
	item.is_surf = is_surf;
	item.tex = tex;
	item.w = w;
	item.h = h;

	size_t pos = std::string::npos;
	if (!file_start_key_.empty()) {
		pos = file.find(file_start_key_);
	}
	if (pos != std::string::npos) {
		std::string sub = file.substr(pos + file_start_key_.size() + 1);
		SDL_strlcpy(item.file, sub.c_str(), sizeof(item.file));
	} else {
		SDL_strlcpy(item.file, file.c_str(), sizeof(item.file));
	}

	SDL_strlcpy(item.function, func.c_str(), sizeof(item.function));
	item.line = line;

	item_vsize_ ++;
}

void trose_dbg_texture::erase_texture(const SDL_Texture* tex)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(game_config::os == os_windows, null_str);

	VALIDATE(item_vsize_ > 0, null_str);

	int hit_at = nposm;
	for (int at = 0; at < item_vsize_; at ++) {
		const titem_C& item = items_[at];
		if (item.tex == tex) {
			hit_at = at;
			break;
		}
	}
	VALIDATE(hit_at != nposm, null_str);

	if (hit_at != item_vsize_ - 1) {
		memcpy(&items_[hit_at], &items_[hit_at + 1], (item_vsize_ - hit_at - 1) * sizeof(titem_C));
	}

	item_vsize_ --;
}

void trose_dbg_texture::log_items(const std::string& scene) const
{
	// VALIDATE(game_config::os == os_windows, null_str);

	SDL_Log("---{%s} item vszie/size: %i/%i---", scene.c_str(), item_vsize_, item_size_);
	for (int at = 0; at < item_vsize_; at ++) {
		const titem_C& item = items_[at];
		SDL_Log("(%i/%i)tex: 0x%p, is_surf: %s, func: %s(%s), line: %i", at, item_vsize_,
			item.tex, item.is_surf? "true": "false", item.function, item.file, item.line);
	}
	
	SDL_Log("-----------------------------");
}

void trose_dbg_texture::reset()
{
	VALIDATE_IN_MAIN_THREAD();

	// item_size_ = 0;
	item_vsize_ = 0;
}

trose_dbg_texture rose_dbg_texture("apps-src");


SDL_Point get_screen_size()
{
	SDL_Rect rect;
	SDL_GetDisplayBounds(0, &rect);
	VALIDATE(rect.x == 0 && rect.y == 0 && rect.w > 0 && rect.h > 0, null_str);
	return SDL_Point{rect.w, rect.h};
}

void render_points(SDL_Renderer* renderer, Uint32 argb, const SDL_Point* points, int count)
{
    SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoints(renderer, points, count);
}

void render_line(SDL_Renderer* renderer, Uint32 argb, int x1, int y1, int x2, int y2)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void render_rect_frame(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb, int thickness)
{
	if (rect.w <= 0 || rect.h <= 0) {
		return;
	}
	VALIDATE(thickness > 0 && rect.w > 0 && rect.h > 0, null_str);
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	if (thickness == 1) {
		SDL_RenderDrawRect(renderer, &rect);
	} else {
		// top
		SDL_Rect rects[4];
		rects[0] = create_rect(rect.x, rect.y, rect.w, thickness);

		// bottom
		rects[1] = create_rect(rect.x, rect.y + rect.h - thickness, rect.w, thickness);

		// left
		rects[2] = create_rect(rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness);

		// right
		rects[3] = create_rect(rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - 2 * thickness);

		SDL_RenderFillRects(renderer, rects, 4);
	}
}

void render_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

void render_rects(SDL_Renderer* renderer, Uint32 argb, const SDL_Rect* rects, int count)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRects(renderer, rects, count);
}

texture create_neutral_texture(const int w, const int h, const int access)
{
	VALIDATE(w > 0 && h > 0, null_str);

	const SDL_PixelFormat format = get_neutral_pixel_format();
	texture result = SDL_CreateTexture2(get_renderer(), format.format, access, w, h);
	if (result.get() != nullptr) {
		// if app is in background, SDL_CreateTexture2 will be nullptr(android).
		SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND);
	}
	return result;
}

texture create_alpha_texture(int width, int height, uint32_t color)
{
	VALIDATE(width > 0 && height > 0, null_str);
	surface bg_surf = create_neutral_surface(width, height);
	
	SDL_Rect dst_rect{0, 0, bg_surf->w, bg_surf->h};
	sdl_fill_rect(bg_surf, &dst_rect, color);
	return SDL_CreateTextureFromSurface2(get_renderer(), bg_surf);
}

texture create_texture_from_text(const std::string& text, int font_size, const SDL_Color& color)
{
	VALIDATE(!text.empty() && font_size > 0, null_str);
	surface text_surf = font::get_rendered_text(text, 0, font_size, color);
	return SDL_CreateTextureFromSurface2(get_renderer(), text_surf);
}

void render_surface(SDL_Renderer* renderer, const surface& surf, const SDL_Rect* srcrect, const SDL_Rect* dstrect)
{
	if (surf) {
		texture src = SDL_CreateTextureFromSurface2(renderer, surf);
		SDL_RenderCopy(renderer, src.get(), srcrect, dstrect);
	}
}

void texture_from_texture(const texture& src, texture& dst, const SDL_Rect* srcrect, const int dstwidth, const int dstheight)
{
	SDL_Renderer* renderer = get_renderer();
	int src_width, src_height;
	uint32_t format;
	bool create_locally = false;
	SDL_QueryTexture(src.get(), &format, NULL, &src_width, &src_height);

	SDL_Rect real_srcrect = ::create_rect(0, 0, src_width, src_height);
	if (srcrect) {
		real_srcrect = *srcrect;
	}

	if (dst.get() == NULL) {
		const int real_dstwidth = dstwidth? dstwidth: real_srcrect.w;
		const int real_dstheight = dstheight? dstheight: real_srcrect.h;
		dst = SDL_CreateTexture2(renderer, format, SDL_TEXTUREACCESS_TARGET, real_dstwidth, real_dstheight);
		create_locally = true;
	}

	// below will change target. require save/recover clip setting of preview target.
	texture_clip_rect_setter clip(NULL);
	if (dst != nullptr) {
		// if app is in background, SDL_CreateTexture2 will be nullptr(android).
		trender_target_lock lock(renderer, dst);
		ttexture_blend_none_lock lock2(src);
		if (create_locally) {
			SDL_RenderClear(renderer);
		}
		int dst_width, dst_height;
		SDL_QueryTexture(dst.get(), NULL, NULL, &dst_width, &dst_height);
		SDL_Rect dst_rect = ::create_rect(0, 0, dst_width, dst_height);
		// if x/y < 0, dst, requrie offset.
		if (real_srcrect.x < 0) {
			dst_rect.x = -1 * real_srcrect.x;
			dst_rect.w += real_srcrect.x;
		}
		if (real_srcrect.y < 0) {
			dst_rect.y = -1 * real_srcrect.y;
			dst_rect.h += real_srcrect.y;
		}

		// if w/h < dst.w/dst.h, require clip.
		const SDL_Rect src_texture_rect = ::create_rect(0, 0, src_width, src_height);
		real_srcrect = intersect_rects(src_texture_rect, real_srcrect);

		if (real_srcrect.w < dst_rect.w) {
			dst_rect.w = real_srcrect.w;
		}
		if (real_srcrect.h < dst_rect.h) {
			dst_rect.h = real_srcrect.h;
		}
		SDL_RenderCopy(renderer, src.get(), &real_srcrect, &dst_rect);
	}
}

texture clone_texture(const texture& src, const uint8_t r, const uint8_t g, const uint8_t b)
{
	if (src.get() == NULL) {
		return NULL;
	}

	SDL_Renderer* renderer = get_renderer();
	int src_width, src_height;
	uint32_t format;
	SDL_QueryTexture(src.get(), &format, NULL, &src_width, &src_height);

	texture res = SDL_CreateTexture2(renderer, format, SDL_TEXTUREACCESS_TARGET, src_width, src_height);
	texture_clip_rect_setter clip(NULL);
	{
		trender_target_lock lock(renderer, res);
		ttexture_blend_none_lock lock2(src);
		SDL_RenderClear(renderer);

		ttexture_color_mod_lock lock3(src, r, g, b);
		SDL_RenderCopy(renderer, src.get(), NULL, NULL);
	}

	SDL_BlendMode blendmode;
	SDL_GetTextureBlendMode(src.get(), &blendmode);
	SDL_SetTextureBlendMode(res.get(), blendmode);

	return res;
}

void brighten_renderer(SDL_Renderer* renderer, const uint8_t r, const uint8_t g, const uint8_t b, const SDL_Rect* dstrect)
{
	texture texture = get_white_texture();

	ttexture_color_mod_lock lock3(texture, r, g, b);
	SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_ADD);
	SDL_RenderCopy(renderer, texture.get(), NULL, dstrect);
}

void brighten_texture(const texture& tex, const uint8_t r, const uint8_t g, const uint8_t b)
{
	SDL_Renderer* renderer = get_renderer();
	{
		texture_clip_rect_setter clip_setter(NULL);
		trender_target_lock lock(renderer, tex);
		brighten_renderer(renderer, r, g, b, NULL);
	}
}

void grayscale_renderer(SDL_Renderer* renderer)
{
	SDL_Texture* target = SDL_GetRenderTarget(renderer);
	VALIDATE(target, null_str);

	int tex_width, tex_height;
	uint32_t format;
	SDL_QueryTexture(target, &format, NULL, &tex_width, &tex_height);
	surface surf = create_neutral_surface(tex_width, tex_height);
	{
		surface_lock dst_lock(surf);
		SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, 4 * tex_width);
	}
	surf = greyscale_image(surf, false);
	SDL_UpdateTexture(target, NULL, surf->pixels, 4 * tex_width);
}

texture target_texture_from_surface(const surface& surf)
{
	if (surf.get() == NULL) {
		return NULL;
	}
	SDL_Renderer* renderer = get_renderer();
	SDL_Texture* tex = SDL_CreateTexture2(renderer, surf->format->format, SDL_TEXTUREACCESS_TARGET, surf->w, surf->h);
	{
		// some surf maybe been blit before, must RLE decode.
		const_surface_lock dst_lock(surf);
		SDL_UpdateTexture(tex, NULL, surf->pixels, surf->pitch);
	}

	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
	return tex;
}

void imwrite(const texture& tex, const std::string& path)
{
	VALIDATE(tex.get(), null_str);
	VALIDATE(!path.empty(), null_str);

	int width, height;
	uint32_t format;
	SDL_QueryTexture(tex.get(), &format, NULL, &width, &height);

	surface dst = create_neutral_surface(width, height);
	{
		trender_target_lock target_lock(get_renderer(), tex);
		texture_clip_rect_setter clip_rect(NULL);
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), NULL, format, dst->pixels, 4 * width);
	}

	if (SDL_IsFromRootPath(path.c_str())) {
		IMG_SavePNG(dst, path.c_str());
	} else {
		IMG_SavePNG(dst, (game_config::preferences_dir + "/" + path).c_str());
	}
}

surface save_texture_to_surface(const texture& tex)
{
	int width, height;
	uint32_t format;
	SDL_QueryTexture(tex.get(), &format, NULL, &width, &height);

	surface dst = create_neutral_surface(width, height);
	{
		trender_target_lock target_lock(get_renderer(), tex);
		texture_clip_rect_setter clip_rect(NULL);
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), NULL, format, dst->pixels, 4 * width);
	}
	return dst;
}

surface get_surface_portion(const texture& src, SDL_Rect &area)
{
	if (src == NULL) {
		return NULL;
	}

	if (area.w <= 0 || area.h <= 0) {
		return NULL;
	}

	int src_w, src_h;
	Uint32 src_format;
	SDL_QueryTexture(src.get(), &src_format, NULL, &src_w, &src_h);
	// Check if there is something in the portion
	if (area.x >= src_w || area.y >= src_h || area.x + area.w < 0 || area.y + area.h < 0) {
		return NULL;
	}

	if (area.x + area.w > src_w) {
		area.w = src_w - area.x;
	}
	if (area.y + area.h > src_h) {
		area.h = src_h - area.y;
	}

	SDL_PixelFormat format2 = get_neutral_pixel_format();
	VALIDATE(src_format == get_neutral_pixel_format().format, null_str);

	// use same format as the source (almost always the screen)
	surface dst = create_neutral_surface(area.w, area.h);
	if (dst == NULL) {
		return NULL;
	}

	{
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), &area, src_format, dst->pixels, 4 * area.w);
		// if app is in background, SDL_RenderReadPixels will be fail(android).
	}
	return dst;
}

void blit_integer_surface(int integer, surface& to, int x, int y)
{
	const int digit_width = 8;
	const int digit_height = 12;
	if (to->w < 8) {
		return;
	}
	if (to->h < digit_height) {
		return;
	}
	if (integer < 0) {
		integer *= -1;
	}

	std::stringstream ss;
	SDL_Rect dst_clip = create_rect(x, y, 0, 0);

	int digit, max = 10;
	while (max <= integer) {
		max *= 10;
	}

	do {
		max /= 10;
		if (max) {
			digit = integer / max;
			integer %= max;
		} else {
			digit = integer;
			integer = 0;
		}

		ss.str("");
		ss << "misc/digit.png~CROP(" << (8 * digit) << ", 0, 8, 12)";
		sdl_blit(image::get_image(ss.str()), NULL, to, &dst_clip);
		dst_clip.x += digit_width;
		if (dst_clip.x > to->w) {
			break;
		}
	} while (max > 1);
}

surface generate_integer_surface(int integer)
{
	const int digit_width = 8;
	const int digit_height = 12;

	if (integer < 0) {
		integer *= -1;
	}

	std::stringstream ss;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);

	int digit, digits = 1, max = 10;
	while (max <= integer) {
		max *= 10;
		digits ++;
	}

	surface result = create_neutral_surface(digits * digit_width, digit_height);

	do {
		max /= 10;
		if (max) {
			digit = integer / max;
			integer %= max;
		} else {
			digit = integer;
			integer = 0;
		}

		VALIDATE(dst_clip.x < result->w, null_str);

		ss.str("");
		ss << "misc/digit.png~CROP(" << (8 * digit) << ", 0, 8, 12)";
		sdl_blit(image::get_image(ss.str()), nullptr, result, &dst_clip);
		dst_clip.x += digit_width;

	} while (max > 1);

	VALIDATE(dst_clip.x == result->w, null_str);

	return result;
}

surface generate_pip_surface(surface& bg, surface& fg)
{
	if (!bg) {
		return surface();
	}
	surface result = clone_surface(bg);
	if (fg) {
		SDL_Rect dst_clip = create_rect(0, 0, 0, 0);
		if (result->w > fg->w) {
			dst_clip.x = (result->w - fg->w) / 2;
		}
		if (result->h > fg->h) {
			dst_clip.y = (result->h - fg->h) / 2;
		}
		sdl_blit(fg, NULL, result, &dst_clip);
	}

	return result;
}

surface generate_pip_surface(int width, int height, const std::string& bg, const std::string& fg)
{
	surface bg_surf = image::get_image(bg);
	surface fg_surf = image::get_image(fg);
	surface result = generate_pip_surface(bg_surf, fg_surf);

	if (width && height) {
		result = scale_surface(result, width, height);
	}
	return result;
}

surface generate_surface(int width, int height, const std::string& img, int integer, bool greyscale)
{
	surface surf = image::get_image(img);
	if (!surf) {
		return surf;
	}

	if (greyscale) {
		surf = greyscale_image(surf);
	}
	surf = scale_surface(surf, width, height);

	if (integer > 0) {
		blit_integer_surface(integer, surf, 0, 0);
	}
	return surf;
}