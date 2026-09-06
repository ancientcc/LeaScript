/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "launcher-lib"

#include "game_latex.hpp"

#include "rose_config.hpp"
#include "rose_filesystem_dll.hpp"
#include "font.hpp"
#include "aplt.hpp"

#include <mupdf/fitz.h>
#include "libyuv/convert_argb.h"
#include <opencv2/imgproc.hpp>


namespace latex {

bool calculate_scale(fz_context *ctx, fz_document *doc, int page_number, float target_width, fz_matrix& ctm)
{
	// Get the original page dimensions to calculate the scaling ratio.
	fz_page *page = NULL;
	fz_rect page_rect;
	float original_width;

	fz_try(ctx)
	{
		page = fz_load_page(ctx, doc, page_number);
		page_rect = fz_bound_page(ctx, page);
		original_width = page_rect.x1 - page_rect.x0;
    
		// Calculate the scaling ratio.
		float scale_x = target_width / original_width;
		float scale_y = scale_x;  // Maintain the aspect ratio.
    
		// Compute a transformation matrix for the zoom and rotation desired.
		ctm = fz_scale(scale_x, scale_y);
		// ctm = fz_pre_rotate(ctm, rotate);
    
		fz_drop_page(ctx, page);
		page = NULL;
	}
	fz_catch(ctx)
	{
		if (page) fz_drop_page(ctx, page);
		fz_report_error(ctx);
		SDL_Log("cannot load page to calculate dimensions");
		return false;
	}
	return true;
}

surface pdf_2_surf(const std::string& input, int page_number, int target_width, float rotate)
{
	fz_context *ctx;
	fz_document *doc;
	fz_pixmap *pix;
	fz_matrix ctm;
	// int x, y;
	int page_count;

	float zoom = 100;

	SDL_Log("{mupdf}input: %s, page_number: %i, zoom: %.3f, rotate: %.3f", input.c_str(), page_number, zoom, rotate);

	// Create a context to hold the exception stack and various caches.
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return nullptr;
	}

	// Register the default file types to handle.
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot register document handlers\n");
		fz_drop_context(ctx);
		return nullptr;
	}

	// Open the document.
	fz_try(ctx)
		doc = fz_open_document(ctx, input.c_str());
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot open document\n");
		fz_drop_context(ctx);
		return nullptr;
	}

	// Count the number of pages.
	fz_try(ctx)
		page_count = fz_count_pages(ctx, doc);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot count number of pages\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return nullptr;
	}

	if (page_number < 0 || page_number >= page_count)
	{
		fprintf(stderr, "page number out of range: %d (page count %d)\n", page_number + 1, page_count);
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return nullptr;
	}

	/* Compute a transformation matrix for the zoom and rotation desired. */
	/* The default resolution without scaling is 72 dpi. */
	if (target_width == nposm) {
		ctm = fz_scale(zoom / 100, zoom / 100);

	} else {
		if (!calculate_scale(ctx, doc, page_number, target_width, ctm)) {
			fz_drop_document(ctx, doc);
			fz_drop_context(ctx);
			return nullptr;
		}
	}
	ctm = fz_pre_rotate(ctm, rotate);

	// Render page to an RGB pixmap.
	// if alpha is enabled(1), some PDF pages may display abnormally, such as page_number(1) in zzzzzz.pdf. 
	// DeepSeek suggests this is caused by premultiplication.
	int alpha = 1;
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_number(ctx, doc, page_number, ctm, fz_device_bgr(ctx), alpha);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot render page\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return nullptr;
	}

	surface surf;
	if (pix->n == 4) {
		VALIDATE(pix->n == 4 && pix->w * pix->n == pix->stride, null_str);

		SDL_Surface* res = SDL_CreateRGBSurface(0, pix->w, pix->h, 4 * 8,
				0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
		uint8_t* pixels = reinterpret_cast<uint8_t*>(res->pixels);
		memcpy(pixels, pix->samples, pix->stride * pix->h);

		surf = res;

	} else {
		VALIDATE(pix->n == 3 && pix->w * pix->n == pix->stride, null_str);

		SDL_Surface* res = SDL_CreateRGBSurface(0, pix->w, pix->h, 4 * 8,
				0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
		uint8_t* pixels = reinterpret_cast<uint8_t*>(res->pixels);

		bool use_cvtColor = true;
		if (use_cvtColor) {
			// uint32_t start_ticks = SDL_GetTicks();
			cv::Mat src(pix->h, pix->w, CV_8UC3, (uint8_t*)pix->samples);
			cv::Mat tmp;
			cv::cvtColor(src, tmp, cv::COLOR_BGR2BGRA);
			memcpy(pixels, tmp.data, pix->w * 4 * pix->h);
			// SDL_Log("use cvtcolor, cost %i ms", SDL_GetTicks() - start_ticks);

		} else {
			uint32_t start_ticks = SDL_GetTicks();
			libyuv::RGB24ToARGB(pix->samples, pix->w * 3,
                             pixels, pix->w * 4,
                             pix->w, pix->h);
			// SDL_Log("use RGB24ToARGB, cost %i ms", SDL_GetTicks() - start_ticks);
		}
		surf = res;
	}

	// Clean up.
	fz_drop_pixmap(ctx, pix);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);

	SDL_Log("{mupdf}---test finished ok---");
	return surf;
}

std::string tex_from_document(const std::string& doc, int max_width)
{
	VALIDATE(!doc.empty(), null_str);
	VALIDATE(max_width == nposm || max_width > 0, null_str);

	int s = font::SIZE_DEFAULT;
	const std::string header_restrict_width = 
		// "\\documentclass{article}\n"
		"\\documentclass{standalone}\n"
		// "\\usepackage{ctex}\n"
		"\\usepackage{amssymb}\n"
		"\\usepackage{mathtools}\n"
		"\\begin{document}\n"
		"\\begin{minipage}{%ipt}\n" // {256pt}
		// "\\parbox{384pt}{\n"
		"\\fontsize{%i}{%i}\\selectfont\n"; // fontsize{22}{26}
	const std::string tail_restrict_width = 
		"\n"
		// "}\n"
		"\\end{minipage}\n"
		"\\end{document}";

	const std::string header = 
		// "\\documentclass{article}\n"
		"\\documentclass{standalone}\n"
		// "\\usepackage{ctex}\n"
		"\\usepackage{amssymb}\n"
		"\\usepackage{mathtools}\n"
		"\\begin{document}\n"
		// "\\begin{minipage}{%ipt}\n" // {256pt}
		// "\\parbox{384pt}{\n"
		"\\fontsize{%i}{%i}\\selectfont\n"; // fontsize{22}{26}
	const std::string tail = 
		"\n"
		// "}\n"
		// "\\end{minipage}"
		"\\end{document}";

	int font_size = 22;
	int line_spacing = 26;

	std::stringstream ss;
	char buf[256];
	if (max_width != nposm) {
		SDL_snprintf(buf, sizeof(buf), header_restrict_width.c_str(), max_width, font_size, line_spacing);
	} else {
		SDL_snprintf(buf, sizeof(buf), header.c_str(), font_size, line_spacing);
	}
	ss << buf;
	ss << doc;

	if (max_width != nposm) {
		ss << tail_restrict_width;
	} else {
		ss << tail;
	}
	return ss.str();
}

bool tex_file_to_pdf_file(const std::string& tex_file, const tluatex_hook& hook)
{
	VALIDATE(!tex_file.empty(), null_str);

	std::vector<std::string> args;
	args.push_back("rose_luahbtex");
	args.push_back("-synctex=-1");
	args.push_back("--fmt=lualatex");
	args.push_back(tex_file);

	int retval = miktex_luahbtex_main(args, &hook);
	return retval == EXIT_SUCCESS;
}

surface doc_to_surf(const std::string& formula, int max_width)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!formula.empty(), null_str);

	const std::string tex = tex_from_document(formula, max_width);

	tlocator i_locator(tex);
	int index = nposm;
	// return the image if already cached
	if ((index = i_locator.in_cache()) >= 0) {
		return i_locator.locate_in_cache(index);
	}

	std::string tmp_tex_filename = game_config::preferences_dir + "/saves/__tmp_tex.tex";
	// std::string tmp_tex_filename = "c:/ddksample/__tmp_tex.tex";
	write_file(tmp_tex_filename, tex.c_str(), tex.size());

	std::vector<std::string> args;
	args.push_back("rose_luahbtex");
	args.push_back("-synctex=-1");
	args.push_back("--fmt=lualatex");
	args.push_back(tmp_tex_filename);

	miktex_luahbtex_main(args, nullptr);

	surface surf = pdf_2_surf(miktex_output_dir + "/__tmp_tex.pdf", 0, nposm, 0.0f);
	// surface surf = pdf_2_surf(game_config::preferences_dir + "/saves/test3.pdf", 0, nposm, 0.0f);
	// surface surf = pdf_2_surf(game_config::preferences_dir + "/saves/zzzzzz.pdf", 1, 1021, 0.0f);

	i_locator.add_to_cache(surf);

	// imwrite(surf, "1.png");
	return surf;
}

void make_lualatex_fmt()
{
	std::vector<std::string> miktex_args;
	// miktex_args.push_back("C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");
	miktex_args.push_back("rose_miktex");

	// formats build lualatex --engine luahbtex
	miktex_args.push_back("formats");
	miktex_args.push_back("build");

	// lualatex --engine luahbtex
	miktex_args.push_back("lualatex");
	miktex_args.push_back("--engine");
	miktex_args.push_back("luahbtex");

	miktex_miktex_main(miktex_args);
}

const std::map<int, std::string> latex_types = {
	{rsplatextype_miktex, "miktex"}
};

bool sandbox_is_valid(version_info* _version, int64_t* _ts, int* _type, std::string* _desc)
{
	if (_version != nullptr) {
		*_version = null_str;
	}
	const std::string miktex_dir = game_config::preferences_dir + "/miktex";

	std::vector<std::string> files;
	files.push_back("sandbox/tex/latex/ctex/fontset/ctex-fontset-android.def");
	files.push_back("sandbox/tex/latex/ctex/fontset/ctex-fontset-windows.def");
	files.push_back("sandbox/tpm/packages/exam.tpm");
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string file = miktex_dir + "/" + *it;
		if (!SDL_IsFile(file.c_str())) {
			return false;
		}
	}

	config cfg = aplt::get_distribution_cfg(miktex_dir);
	if (cfg.empty()) {
		return false;
	}

	int type = cfg["type"].to_int();
	if (latex_types.count(type) == 0) {
		return false;
	}

	version_info version(cfg["version"].str());
	if (!version.is_rose_recommended()) {
		return false;
	}

	if (_version != nullptr) {
		*_version = version;
	}
	if (_ts != nullptr) {
		*_ts = cfg["ts"].to_int64();
	}
	if (_type != nullptr) {
		*_type = type;
	}
	if (_desc != nullptr) {
		*_desc = cfg["desc"].str();
	}
	
	return type;
}

}