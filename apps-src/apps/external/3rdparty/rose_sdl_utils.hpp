/* $Id: sdl_utils.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#ifndef LIBROSE_SDL_UTILS_INCLUDED
#define LIBROSE_SDL_UTILS_INCLUDED

#include "rose_util.hpp"
#include "rose_exception.hpp"
#include <SDL.h>
#include <SDL_render.h>

// <opencv\opencv2\core\cvdef.h(73,10): fatal error C1083: Cannot open include file: 'cvconfig.h': No such file or directory
// #include <opencv2/core/mat.hpp>
#include <opencv2/core/mat.hpp>
// #include <opencv2/core.hpp>

#include <cstdlib>
#include <iosfwd>
#include <map>
#include <string>

#include <zlib/zlib.h>


#define FINGER_HIT_THRESHOLD		4
#define FINGER_MOTION_THRESHOLD		10

#define MOUSE_HIT_THRESHOLD			0
#define MOUSE_MOTION_THRESHOLD		1

#define NO_MODULATE_ALPHA	255

typedef SDL_Window* (*fget_sdl_window)();
extern LIB3RDPARTY_DECL fget_sdl_window rose_get_sdl_window;

LIB3RDPARTY_DECL SDL_Keycode sdl_keysym_from_name(std::string const &keyname);

LIB3RDPARTY_DECL bool point_in_rect(int x, int y, const SDL_Rect& rect);
LIB3RDPARTY_DECL bool rects_overlap(const SDL_Rect& rect1, const SDL_Rect& rect2);
LIB3RDPARTY_DECL SDL_Rect intersect_rects(SDL_Rect const &rect1, SDL_Rect const &rect2);
LIB3RDPARTY_DECL SDL_Rect union_rects(const SDL_Rect &rect1, const SDL_Rect &rect2);

/**
 *  Creates an empty SDL_Rect.
 *
 *  Since SDL_Rect doesn't have a constructor it's not possible to create it as
 *  a temporary for a function parameter. This functions overcomes this limit.
 */
LIB3RDPARTY_DECL SDL_Rect create_rect(const int x, const int y, const int w, const int h);
LIB3RDPARTY_DECL SDL_Point create_point(const int x, const int y);

class LIB3RDPARTY_DECL surface
{
private:
	static void sdl_add_ref(SDL_Surface *surf);

public:
	surface()
		: surface_(nullptr)
		, mat_(nullptr)
	{}

	surface(SDL_Surface *surf) 
		: surface_(surf)
		, mat_(nullptr)
	{}

	// @pitch: if mat is partial(roi) of mat-A, caller require to set correct pitch value.
	//         ==> pitch = mat-A.cols * mat-A.channels();
	surface(const cv::Mat& mat, int pitch = nposm);

	surface(const surface& o)
		: surface_(o.surface_)
		, mat_(o.mat_)
	{
		sdl_add_ref(surface_);
	}

	~surface();

	void assign(const surface& o)
	{
		SDL_Surface* surf = o.surface_;
		sdl_add_ref(surf); // need to be done before assign to avoid corruption on "a=a;"
		assign2(surf, o.mat_);
	}

	surface& operator=(const surface& o)
	{
		assign(o);
		return *this;
	}

	operator SDL_Surface*() const { return surface_; }

	SDL_Surface* get() const { return surface_; }

	SDL_Surface* operator->() const { return surface_; }

	bool null() const { return surface_ == nullptr; }

private:
	void assign2(SDL_Surface* surf, cv::Mat* mat);
	void free_sdl_surface();

private:
	SDL_Surface* surface_;
	cv::Mat* mat_;
};

LIB3RDPARTY_DECL bool operator<(const surface& a, const surface& b);

// SDL_Texture hasn't refcount member, use std::shared_ptr.
class LIB3RDPARTY_DECL texture: public std::shared_ptr<SDL_Texture>
{
public:
	texture()
		: std::shared_ptr<SDL_Texture>()
	{}

	texture(SDL_Texture* tex)
		: std::shared_ptr<SDL_Texture>(tex, SDL_DestroyTexture)
	{}

	void reset(SDL_Texture* tex) { std::shared_ptr<SDL_Texture>::reset(tex, SDL_DestroyTexture); }
};

inline void sdl_blit(const surface& src, const SDL_Rect* src_rect, surface& dst, SDL_Rect* dst_rect){
	SDL_BlitSurface(src, src_rect, dst, dst_rect);
}

inline void sdl_fill_rect(surface& dst, SDL_Rect* dst_rect, const Uint32 color){
	SDL_FillRect(dst, dst_rect, color);
}
/*
void render_points(SDL_Renderer* renderer, Uint32 argb, const SDL_Point* points, int count);
void render_line(SDL_Renderer* renderer, Uint32 argb, int x1, int y1, int x2, int y2);
void render_rect_frame(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb, int thickness);
void render_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb);
void render_rects(SDL_Renderer* renderer, Uint32 argb, const SDL_Rect* rects, int count);
*/
/**
 * Check that the surface is neutral bpp 32.
 *
 * The surface may have an empty alpha channel.
 *
 * @param surf                    The surface to test.
 *
 * @returns                       The status @c true if neutral, @c false if not.
 */
LIB3RDPARTY_DECL bool is_neutral_surface(const surface& surf);

LIB3RDPARTY_DECL const SDL_PixelFormat& get_neutral_pixel_format();
LIB3RDPARTY_DECL surface create_neutral_surface(int w, int h, bool use_rle = true);
LIB3RDPARTY_DECL surface makesure_neutral_surface(const surface& surf);
LIB3RDPARTY_DECL surface clone_surface(const surface& surf);
LIB3RDPARTY_DECL void fill_surface(surface& surf, uint32_t color);
LIB3RDPARTY_DECL SDL_Rect calculate_max_foreground_region(const surface& surf, uint32_t background);
/*
texture create_neutral_texture(const int w, const int h, const int access);
// I cannot use render_rect(renderer, dstrect, 0x80000000) to get alpha-mask effect, so use below.
texture create_alpha_texture(int width, int height, uint32_t color);
texture create_texture_from_text(const std::string& text, int font_size, const SDL_Color& color);
*/
/**
 * Stretches a surface in the horizontal direction.
 *
 *  The stretches a surface it uses the first pixel in the horizontal
 *  direction of the original surface and copies that to the destination.
 *  This means only the first column of the original is used for the destination.
 *  @param surf              The source surface.
 *  @param w                 The width of the resulting surface.
 *  @param optimize          Should the return surface be RLE optimized.
 *
 *  @return                  An optimized surface.
 *                           returned.
 *  @retval 0                Returned upon error.
 *  @retval surf             Returned if w == surf->w, note this ignores the
 *                           optimize flag.
 */
LIB3RDPARTY_DECL surface stretch_surface_horizontal(const surface& surf, const unsigned w, int pixels);

/**
 *  Stretches a surface in the vertical direction.
 *
 *  The stretches a surface it uses the first pixel in the vertical
 *  direction of the original surface and copies that to the destination.
 *  This means only the first row of the original is used for the destination.
 *  @param surf              The source surface.
 *  @param h                 The height of the resulting surface.
 *  @param optimize          Should the return surface be RLE optimized.
 *
 *  @return                  An optimized surface.
 *                           returned.
 *
 *  @retval surf             Returned if h == surf->h, note this ignores the
 *                           optimize flag.
 */
LIB3RDPARTY_DECL surface stretch_surface_vertical(const surface& surf, const unsigned h, int pixels);

LIB3RDPARTY_DECL SDL_Size calculate_max_size_with_ratio(const int ratio_w, const int ratio_h, const int content_w, const int content_h);
LIB3RDPARTY_DECL tpoint calculate_adaption_ratio_size(const int restrict_w, const int restrict_h, const int content_w, const int content_h);
LIB3RDPARTY_DECL surface get_adaption_ratio_surface(const surface& src, const int restrict_w, const int restrict_h);
LIB3RDPARTY_DECL cv::Mat get_adaption_ratio_mat(const cv::Mat& src, const int restrict_w, const int restrict_h);
LIB3RDPARTY_DECL tpoint calculate_adaption_ratio_size_cut(const int restrict_w, const int restrict_h, const int content_w, const int content_h);
LIB3RDPARTY_DECL void erase_isolated_pixel(cv::Mat& src, const int font_gray, const int background_gray);
LIB3RDPARTY_DECL uint32_t surf_calculate_most_color(const surface& surf);
LIB3RDPARTY_DECL double calculate_surface_blur(const surface& surf, const cv::Rect& roi);

/** Scale a surface
 *  @param surf              The source surface.
 *  @param w                 The width of the resulting surface.
 *  @param h                 The height of the resulting surface.
 *  @param optimize          Should the return surface be RLE optimized.
 *  @return                  A surface containing the scaled version of the source.
 *  @retval 0                Returned upon error.
 *  @retval surf             Returned if w == surf->w and h == surf->h
 *                           note this ignores the optimize flag.
 */
LIB3RDPARTY_DECL surface scale_surface(const surface &surf, int w, int h);
LIB3RDPARTY_DECL surface scale_surface_blended(const surface &surf, int w, int h, bool optimize=true);
LIB3RDPARTY_DECL void add_white_overlay_surface(surface& surf, double alpha);

LIB3RDPARTY_DECL surface rotate_landscape_anticolockwise90(const surface& surf);

LIB3RDPARTY_DECL surface adjust_surface_color(const surface &surf, int r, int g, int b, bool optimize=true);
LIB3RDPARTY_DECL void adjust_surface_color2(surface &surf, int red, int green, int blue);
LIB3RDPARTY_DECL surface greyscale_image(const surface &surf, bool optimize=true);
/** create an heavy shadow of the image, by blurring, increasing alpha and darkening */
LIB3RDPARTY_DECL surface shadow_image(const surface &surf, bool optimize=true);

/**
 * Recolors a surface using a map with source and converted palette values.
 * This is most often used for team-coloring.
 *
 * @param surf               The source surface.
 * @param map_rgb            Map of color values, with the keys corresponding to the
 *                           source palette, and the values to the recolored palette.
 * @param optimize           Whether the new surface should be RLE encoded. Only
 *                           useful when the source is not the screen and it is
 *                           going to be used multiple times.
 * @return                   A recolored surface, or a null surface if there are
 *                           problems with the source.
 */
LIB3RDPARTY_DECL surface recolor_image(surface surf, const std::map<Uint32, Uint32>& map_rgb,
	bool optimize=true);

LIB3RDPARTY_DECL surface brighten_image(const surface &surf, fixed_t amount, bool optimize=true);

/** Get a portion of the screen.
 *  Send NULL if the portion is outside of the screen.
 *  @param surf              The source surface.
 *  @param rect              The portion of the source surface to copy.
 *  @param optimize_format   Optimize by converting to result to display format.
 *                           Only useful if the source is not the screen and you
 *                           plan to blit the result on screen several times.
 *  @return                  A surface containing the portion of the source.
 *                           No RLE or Alpha bits are set.
 *  @retval 0                if error or the portion is outside of the surface.
 */
// surface get_surface_portion(const texture& tex, SDL_Rect &rect);
LIB3RDPARTY_DECL surface get_surface_portion2(const surface& surf, SDL_Rect &rect);

LIB3RDPARTY_DECL surface adjust_surface_alpha(const surface& surf, fixed_t amount);
LIB3RDPARTY_DECL void adjust_surface_rect_alpha2(surface& surf, fixed_t amount, const SDL_Rect& rect, bool inside);
LIB3RDPARTY_DECL surface adjust_surface_alpha_add(const surface& surf, int amount, bool optimize=true);

/** Applies a mask on a surface. */
LIB3RDPARTY_DECL surface mask_surface(const surface &surf, const surface &mask, bool* empty_result = NULL);

/** Check if a surface fit into a mask */
LIB3RDPARTY_DECL bool in_mask_surface(const surface &surf, const surface &mask);

LIB3RDPARTY_DECL bool has_alpha_le(const surface& surf, uint8_t threshold);

/** Progressively reduce alpha of bottom part of the surface
 *  @param surf              The source surface.
 *  @param depth             The height of the bottom part in pixels
 *  @param alpha_base        The alpha adjustement at the interface
 *  @param alpha_delta       The alpha adjustement reduction rate by pixel depth
 *  @param optimize_format   Optimize by converting to result to display
*/
LIB3RDPARTY_DECL surface submerge_alpha(const surface &surf, int depth, float alpha_base, float alpha_delta, bool optimize=true);

/** Light surf using lightmap (RGB=128,128,128 means no change) */
LIB3RDPARTY_DECL surface light_surface(const surface &surf, const surface &lightmap, bool optimize=true);

/** Cross-fades a surface. */
LIB3RDPARTY_DECL surface blur_surface(const surface &surf, int depth = 1, bool optimize=true);

/**
 * Cross-fades a surface in place.
 *
 * @param surf                    The surface to blur, must be not optimized
 *                                and have 32 bits per pixel.
 * @param rect                    The part of the surface to blur.
 * @param depth                   The depth of the blurring.
 */
LIB3RDPARTY_DECL void blur_surface(surface& surf, SDL_Rect rect, int depth = 1);

/**
 * Cross-fades a surface with alpha channel.
 *
 * @todo FIXME: This is just an adapted copy-paste
 * of the normal blur but with blur alpha channel too
 */
LIB3RDPARTY_DECL surface blur_alpha_surface(const surface &surf, int depth = 1, bool optimize=true);

/** Cuts a rectangle from a surface. */
LIB3RDPARTY_DECL surface cut_surface(const surface &surf, SDL_Rect const &r);
LIB3RDPARTY_DECL surface blend_surface(const surface &surf, double amount, Uint32 color, bool optimize=true);
LIB3RDPARTY_DECL surface flip_surface(const surface &surf, bool optimize=true);
LIB3RDPARTY_DECL surface flop_surface(const surface &surf, bool optimize=true);
LIB3RDPARTY_DECL surface create_compatible_surface(const surface &surf, int width = -1, int height = -1);
/*
void render_surface(SDL_Renderer* renderer, const surface& surf, const SDL_Rect* srcrect, const SDL_Rect* dstrect);
void texture_from_texture(const texture& src, texture& dst, const SDL_Rect* srcrect, const int dstwidth, const int dstheight);
texture clone_texture(const texture& src, const uint8_t r = NO_MODULATE_ALPHA, const uint8_t g = NO_MODULATE_ALPHA, const uint8_t b = NO_MODULATE_ALPHA);
void brighten_texture(const texture& tex, const uint8_t r, const uint8_t g, const uint8_t b);
void brighten_renderer(SDL_Renderer* renderer, const uint8_t r, const uint8_t g, const uint8_t b, const SDL_Rect* dstrect);
void grayscale_renderer(SDL_Renderer* renderer);
texture target_texture_from_surface(const surface& surf);
*/
// (SDL)SDL_PIXELFORMAT_ARGB8888 <==> (opencv/opengl)BGRA
LIB3RDPARTY_DECL cv::Mat imread(const std::string& path, uint32_t sdl_format = SDL_PIXELFORMAT_ARGB8888);

/*
// @path: always append prefix: game_config::preferences_dir + "/"
void imwrite(const texture& tex, const std::string& path);
*/

// @path: always append prefix: game_config::preferences_dir + "/"
LIB3RDPARTY_DECL void imwrite(const surface& surf, const std::string& path);

// @path: always append prefix: game_config::preferences_dir + "/"
LIB3RDPARTY_DECL void imwrite_gray(const cv::Mat& src, const std::string& path);

// surface save_texture_to_surface(const texture& tex);

enum {img_png, img_jpg};
LIB3RDPARTY_DECL uint8_t* imwrite_mem(const surface& surf, int format, int* len_ptr);
LIB3RDPARTY_DECL surface imread_mem(const void* data, int size);

LIB3RDPARTY_DECL void fill_rect_alpha(SDL_Rect &rect, Uint32 color, Uint8 alpha, surface &target);

LIB3RDPARTY_DECL SDL_Rect get_non_transparent_portion(const surface &surf);

LIB3RDPARTY_DECL bool operator==(const SDL_Rect& a, const SDL_Rect& b);
LIB3RDPARTY_DECL bool operator!=(const SDL_Rect& a, const SDL_Rect& b);

LIB3RDPARTY_DECL bool operator==(const SDL_Color& a, const SDL_Color& b);
LIB3RDPARTY_DECL bool operator!=(const SDL_Color& a, const SDL_Color& b);
LIB3RDPARTY_DECL SDL_Color inverse(const SDL_Color& color);

#define FORMULA_COLOR		0x1 // 0 indicate no color.
#define PREDEFINE_COLOR		0x2
LIB3RDPARTY_DECL uint32_t decode_color(const std::string& color);
LIB3RDPARTY_DECL std::string encode_color(const uint32_t argb);

LIB3RDPARTY_DECL SDL_Color uint32_to_color(const Uint32 argb);
LIB3RDPARTY_DECL uint32_t color_to_uint32(const SDL_Color& color);

LIB3RDPARTY_DECL SDL_Color create_color(const unsigned char red
		, unsigned char green
		, unsigned char blue
		, unsigned char unused = 255);

/***** ***** ***** ***** ***** DRAWING PRIMITIVES ***** ***** ***** ***** *****/

/**
 * Draws a single pixel on a surface.
 *
 * @pre                   The caller needs to make sure the selected coordinate
 *                        fits on the @p surface.
 * @pre                   The @p canvas is locked.
 *
 * @param start           The memory address which is the start of the surface
 *                        buffer to draw in.
 * @param color           The color of the pixel to draw.
 * @param w               The width of the surface.
 * @param x               The x coordinate of the pixel to draw.
 * @param y               The y coordinate of the pixel to draw.
 */
LIB3RDPARTY_DECL void put_pixel(
		  const ptrdiff_t start
		, const Uint32 color
		, const unsigned w
		, const unsigned x
		, const unsigned y);

/**
 * Draws a line on a surface.
 *
 * @pre                   The caller needs to make sure the entire line fits on
 *                        the @p surface.
 * @pre                   @p x2 >= @p x1
 * @pre                   The @p surface is locked.
 *
 * @param canvas          The canvas to draw upon, the caller should lock the
 *                        surface before calling.
 * @param color           The color of the line to draw.
 * @param x1              The start x coordinate of the line to draw.
 * @param y1              The start y coordinate of the line to draw.
 * @param x2              The end x coordinate of the line to draw.
 * @param y2              The end y coordinate of the line to draw.
 */
LIB3RDPARTY_DECL void draw_line(
		  surface& canvas
		, Uint32 color
		, int x1
		, int y1
		, int x2
		, int y2);

/**
 * Draws a circle on a surface.
 *
 * @pre                   The circle must fit on the canvas.
 * @pre                   The @p surface is locked.
 *
 * @param canvas          The canvas to draw upon, the caller should lock the
 *                        surface before calling.
 * @param color           The color of the circle to draw.
 * @param x_centre        The x coordinate of the centre of the circle to draw.
 * @param y_centre        The y coordinate of the centre of the circle to draw.
 * @param radius          The radius of the circle to draw.
 */
LIB3RDPARTY_DECL void draw_circle(
		  surface& canvas
		, Uint32 color
		, const unsigned x_centre
		, const unsigned y_centre
		, const unsigned radius
		, bool require_map);

/**
 * Helper class for pinning SDL surfaces into memory.
 * @note This class should be used only with neutral surfaces, so that
 *       the pointer returned by #pixels is meaningful.
 */
struct LIB3RDPARTY_DECL surface_lock
{
	surface_lock(surface &surf);
	~surface_lock();

	Uint32* pixels() { return reinterpret_cast<Uint32*>(surface_->pixels); }
private:
	surface& surface_;
	bool locked_;
};

struct LIB3RDPARTY_DECL const_surface_lock
{
	const_surface_lock(const surface &surf);
	~const_surface_lock();

	const Uint32* pixels() const { return reinterpret_cast<const Uint32*>(surface_->pixels); }
private:
	const surface& surface_;
	bool locked_;
};

struct LIB3RDPARTY_DECL tsurface_2_mat_lock
{
	tsurface_2_mat_lock(const surface &surf);
	~tsurface_2_mat_lock();

	cv::Mat mat;

private:
	const surface& surface_;
	bool locked_;
};

struct LIB3RDPARTY_DECL ttexture_2_mat_lock
{
	ttexture_2_mat_lock(texture &tex);

	cv::Mat mat;

private:
	texture& texture_;
	bool locked_;
};

struct tframe_ticks {
	int frames;
	uint32_t task1;
	uint32_t task2;
	uint32_t task3;
	uint32_t task4;
};
LIB3RDPARTY_DECL extern tframe_ticks frame_ticks;

LIB3RDPARTY_DECL void cvtColor2(const cv::Mat& _src, cv::Mat& _dst, int code);

LIB3RDPARTY_DECL void draw_rectangle(int x, int y, int w, int h, Uint32 color, surface tg);
LIB3RDPARTY_DECL void draw_distinguish_rectangle(const SDL_Rect& rect, int long_size, int short_size, uint32_t color, surface target);

// blit the image on the center of the rectangle
// and a add a colored background
LIB3RDPARTY_DECL void draw_centered_on_background(surface surf, const SDL_Rect& rect,
	const SDL_Color& color, surface target);
/*
void blit_integer_surface(int integer, surface& to, int x, int y);
surface generate_integer_surface(int integer);
surface generate_pip_surface(surface& bg, surface& fg);
surface generate_pip_surface(int width, int height, const std::string& bg, const std::string& fg);
surface generate_surface(int width, int height, const std::string& img, int integer, bool greyscale);
*/
struct tarc_pixel {
	unsigned short x;
	unsigned short y;
	unsigned short degree;
};
LIB3RDPARTY_DECL tarc_pixel* circle_calculate_pixels(surface& surf, int* valid_pixels);
LIB3RDPARTY_DECL void circle_draw_arc(surface& surf, tarc_pixel* circle_pixels, const int valid_pixels, int start, int stop, Uint32 erase_col);

// Calculate the angle of OA to OB, angle value is between 0 and 180. 
// clockwise indicates the direction of rotation. 
LIB3RDPARTY_DECL double calculate_line_angle(bool image_view, const SDL_Point& O, const SDL_Point& A, const SDL_Point& B, bool& clockwise);
LIB3RDPARTY_DECL cv::Mat rotate_mat(const cv::Mat& src, double rotationAngle, SDL_Point* points, int count);
LIB3RDPARTY_DECL surface rotate_surface(const surface& src, double rotationAngle, SDL_Point* points, int count);
LIB3RDPARTY_DECL SDL_Rect enlarge_rect(const SDL_Rect& src, int left, int right, int top, int bottom, int max_width, int max_height);

LIB3RDPARTY_DECL surface u8_data_2_argb_surf(const uint8_t* data, int width, int height);
LIB3RDPARTY_DECL surface u8_data_2_cell_value_surf(const uint8_t* map_data, int margin_x, int margin_y, int width, int height, int map_type, const std::set<int64_t>* gree_set, const std::set<int64_t>* blue_set);
LIB3RDPARTY_DECL void surf_overlay_mark_inplace(surface& surf, uint32_t font_color, int margin_x, int margin_y, int cell_size, bool v_flip, const std::string& bottom_text);
LIB3RDPARTY_DECL surface surf_overlay_mark(const surface& src, uint32_t font_color, int margin_x, int margin_y, int cell_size, bool v_flip, const std::string& bottom_text);

struct LIB3RDPARTY_DECL tsurf_overlay
{
	tsurf_overlay(const uint8_t* _data, int _per_bytes, bool _allow_negative, uint32_t _font_color, 
		bool _has_nposm_value = false, int _nposm_value = nposm)
		: data(_data)
		, per_bytes(_per_bytes)
		, allow_negative(_allow_negative)
		, font_color(_font_color)
		, has_nposm_value(_has_nposm_value)
		, nposm_value(_nposm_value)
	{}

	const uint8_t* data;
	int per_bytes;
	bool allow_negative;
	uint32_t font_color;
	bool has_nposm_value;
	int nposm_value;
};
LIB3RDPARTY_DECL surface overlay_digits_to_surf(const uint8_t* src_pixels, int src_width, int src_height, int src_channels, const std::vector<tsurf_overlay>& overlays);

LIB3RDPARTY_DECL void u8_data_fill_cell_val(int margin_x, int margin_y, int x, int y, int pixel_pitch, int cell_size, uint32_t* pixels, uint32_t val, bool avg);

enum {maptype_denoising, maptype_OccupancyGrid, maptype_costmap};
#define MAPTYPE_IS_ROSMAP(map_type)	(map_type == maptype_denoising || map_type == maptype_OccupancyGrid  || map_type == maptype_costmap)

LIB3RDPARTY_DECL SDL_Surface* u8_data_2_cell_surf(uint32_t def_value, const uint8_t* map_data, const int margin_x, int margin_y, int width, int height, int cell_size, 
	uint32_t block_line_color, uint32_t cell_line_color, int map_type, const std::map<uint8_t, uint32_t>* palette);

std::ostream& operator<<(std::ostream& s, const SDL_Rect& rect);

#define MIN_ROSE_IMAGE_SIZE		64

class LIB3RDPARTY_DECL timage_pair
{
public:
	timage_pair(const std::string& desc, const surface& surf, int _image_format)
		: desc(desc)
	{
		VALIDATE(surf.get() != nullptr, null_str);
		VALIDATE(_image_format == img_png || _image_format == img_jpg, null_str);

		memset(&image, 0, sizeof(tuint8cdata_C));
		image.ptr = imwrite_mem(surf, _image_format, &image.len);
		VALIDATE(image.len >= MIN_ROSE_IMAGE_SIZE, null_str);
	}

	timage_pair(const std::string& desc, const uint8_t* data, int size)
		: desc(desc)
	{
		VALIDATE(data != nullptr, null_str);
		VALIDATE(size >= MIN_ROSE_IMAGE_SIZE, null_str);

		memset(&image, 0, sizeof(tuint8cdata_C));
		image.ptr = (uint8_t*)SDL_malloc(size);
		memcpy(image.ptr, data, size);
		image.len = size;
	}

	timage_pair(const timage_pair& that)
	{
		assign(that);
	}

	~timage_pair()
	{
		if (image.ptr != nullptr) {
			SDL_free(image.ptr);
		}
	}

	// posix_noncopyable(timage_pair);
	timage_pair& operator=(const timage_pair& that)
	{
		assign(that);
		return *this;
	}

	void assign(const timage_pair& that)
	{
		desc = that.desc;
		if (that.image.ptr != nullptr) {
			VALIDATE(that.image.len >= MIN_ROSE_IMAGE_SIZE, null_str);
			image.ptr = (uint8_t*)SDL_malloc(that.image.len);
			memcpy(image.ptr, that.image.ptr, that.image.len);
			image.len = that.image.len;

		} else {
			VALIDATE(that.image.len == 0, null_str);
			memset(&image, 0, sizeof(tuint8cdata_C));
		}
	}

	int image_format() const
	{
		VALIDATE(image.len >= MIN_ROSE_IMAGE_SIZE, null_str);
		if (image.ptr[0] == 0x89 &&
			image.ptr[1] == 'P' &&
			image.ptr[2] == 'N' &&
			image.ptr[3] == 'G' ) {
			return img_png;
		}
		return img_jpg;
	}

public:
	std::string desc;
	// int image_format;
	tuint8data_C image;
};

LIB3RDPARTY_DECL std::string join_to_log_msg(int64_t ts, const std::string& devicename, const std::string& desc, const std::vector<timage_pair>& images);

class LIB3RDPARTY_DECL trobot_event
{
public:
	trobot_event(int64_t ts, const std::string& devicename, const std::string& desc, std::vector<timage_pair>& images)
		: ts(ts)
		, devicename(devicename)
		, desc(desc)
		, images(images)
	{}

	trobot_event()
		: ts(0)
	{}

	std::string to_log_msg() const
	{
		return join_to_log_msg(ts, devicename, desc, images);
	
	}

	bool operator<(const trobot_event& that) const noexcept
	{
		if (ts != that.ts) {
			return ts > that.ts;
		}
		int cmp = SDL_strcmp(devicename.c_str(), that.devicename.c_str());
		return cmp < 0;
	}

public:
	int64_t ts;
	std::string devicename;
	std::string desc;
	std::vector<timage_pair> images;
};

// #define telem_array_C_has_operator_equal
// 'telem_array_C' is to manage a block of memory. 
// The size of this memory block is fixed from the beginning and must be an integer multiple of a certain 'elem_sz', 
// where 'elem_sz' is the byte size of a specific struct, such as 'sizeof(tobj_item)'. 
// Initially, the number of structs stored in it is 0, and it will increase during runtime. This class provides external functions to facilitate adding and deleting.

// Why 'telem_array_C' haven't the ability to dynamically expand its memory size, 
// like some classes that provide a function such as 'resize_data(size, vsize)'? 
// --The reason is that expanding the memory would require creating a new memory block, 
// which would invalidate all previously stored element pointers. 
// These pointers may have already been scattered across other objects, making it impossible to update them all.

// 'telem_array_C' does not use an array of pointers, because doing so would allow the memory to be freed in one go, 
// rather than requiring '1 + N' (where N is the number of elements) separate deallocations.

// To use 'telem_array_C', you need to know the maximum possible number of elements.
class LIB3RDPARTY_DECL telem_array_C
{
public:
	telem_array_C(int elem_sz, int _max_size = nposm)
		: elem_sz(elem_sz)
		, max_size(_max_size)
		, data(nullptr)
		, size(0)
		, vsize(0)
	{
		VALIDATE(elem_sz > 0, null_str);
		if (max_size != nposm) {
			VALIDATE(max_size > 0 && max_size <= 4096, null_str);
			resize_data(max_size, 0);
		}
	}

	~telem_array_C()
	{
		if (data != nullptr) {
			free(data);
		}
	}

	void resize_data(int size, int vsize);
	uint8_t* append_1();

	void put_size(const void* _data, int _size);
	void put_repeat_string(bool append, const std::string& str, int count);

	void drop_first(int _size);
	void drop_at(int at);

	void assign_data(const telem_array_C& that);
#ifdef telem_array_C_has_operator_equal
	telem_array_C& operator=(telem_array_C&& that) noexcept;
#endif

	const uint8_t* elem(int at) const
	{
		VALIDATE(at < vsize, null_str);
		return data + at * elem_sz;
	}

	uint8_t* mutable_elem(int at)
	{
		VALIDATE(at < vsize, null_str);
		return data + at * elem_sz;
	}

	void clear()
	{
		vsize = 0;
	}

public:
	// In a single run, 'elem_sz' and 'max_size' need to be defined as const. 
	// However, 'telem_array_C' will be placed in a 'std::vector. 
	// When the containing object is deleted, a move assignment may occur (see telem_array_C::operator=(telem_array_C&& that)'), 
	// and here these two values must be modified.
#ifdef telem_array_C_has_operator_equal
	int elem_sz; 
	int max_size;
#else
	const int elem_sz;
	const int max_size;
#endif

	uint8_t* data;
	int size;
	int vsize;
};

struct LIB3RDPARTY_DECL clip_rect_setter
{
	// if r is NULL, clip to the full size of the surface.
	clip_rect_setter(const surface &surf, const SDL_Rect* r, bool operate = true) : surface_(surf), rect_(), operate_(operate)
	{
		if(operate_){
			SDL_GetClipRect(surface_, &rect_);
			SDL_SetClipRect(surface_, r);
		}
	}

	~clip_rect_setter() {
		if (operate_)
			SDL_SetClipRect(surface_, &rect_);
	}

private:
	surface surface_;
	SDL_Rect rect_;
	const bool operate_;
};

namespace zlib {
LIB3RDPARTY_DECL int compress(const uint8_t* uncompress_data, int uncompress_len, telem_array_C& compress_str, int level = Z_DEFAULT_COMPRESSION);

// Decompression function: Decompress the compressed string back to the original string
LIB3RDPARTY_DECL int uncompress(const uint8_t* compress_data, int compress_len, telem_array_C& uncompress_str);
}

namespace image {

typedef surface (*fget_image)(const std::string& filename);
extern LIB3RDPARTY_DECL fget_image rose_get_image;

}

namespace sound {

typedef void (*fplay_sound)(const std::string& files, channel_group group, unsigned int repeats);
extern LIB3RDPARTY_DECL fplay_sound rose_play_sound;

LIB3RDPARTY_DECL void rose_play_sound_simple(const std::string& files);

}


#endif
