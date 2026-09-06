/* $Id: filesystem.cpp 47496 2010-11-07 15:53:11Z silene $ */
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
 * @file
 * File-IO
 */

#ifdef _WIN32
#include <shlobj.h>	// CSIDL_PROGRAM_FILES
#include <direct.h>
#include <cctype>
#else /* !_WIN32 */
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#ifndef ANDROID
#include <sys/param.h> // statfs 
#include <sys/mount.h> // statfs
#else
#include <sys/vfs.h> // statfs 
#endif
#endif /* !_WIN32 */

#include "rose_filesystem.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_exception.hpp"

#include <SDL.h>
#include <openssl/sha.h>
#include <openssl/mem.h>

using namespace std::placeholders;

bool tdcdepth_data::load_from_file(const std::string& filename)
{
	VALIDATE(!filename.empty(), null_str);
	clear();

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		return false;
	}
	posix_fseek(file.fp, 0);
	int fsize = posix_fsize(file.fp);
	if (fsize < sizeof(tdepth_file_header)) {
		return false;
	}
	posix_fseek(file.fp, 0);

	memset(&header, 0, sizeof(header));
	posix_fread(file.fp, &header, sizeof(tdepth_file_header));
	if (header.fourcc != SDL_FOURCC('D', 'E', 'P', 'T')) {
		return false;
	}
	if (header.width <= 0 || header.height <= 0) {
		return false;
	}
	if (header.depth_scale <= 0) {
		return false;
	}
	data_len = header.width * header.height * 2;
	if (fsize != sizeof(tdepth_file_header) + data_len) {
		return false;
	}
	datafile = filename;

	data = (uint16_t*)malloc(data_len);
	posix_fread(file.fp, data, data_len);

	return true;
}

bool did_write_rsp_applet(tfile& file, const std::string& bundleid, const version_info& rose_version, uint32_t version, uint32_t build_date, const std::string& src_zip)
{
	tfile src(src_zip, GENERIC_READ, OPEN_EXISTING);
	const int fsize = posix_fsize(src.fp);
	VALIDATE(fsize > 0, null_str);

	const int one_block = 1024 * 1024;
	src.resize_data(one_block);

	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_applet));
	header.version = version;
	header.build_date = build_date;

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.manufactor = utils::to_uint32(rose_version.special_version());
	header.zip_size = fsize;
	posix_fwrite(file.fp, &header, sizeof(header));

	int pos = 0;
	while (pos < fsize) {
		int bytes = one_block;
		if (pos + bytes > fsize) {
			bytes = fsize - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(file.fp, src.data, bytes);

		pos += bytes;
	}
	return true;
}

bool did_write_rsp_apk(tfile& file, const std::string& bundleid, const version_info& rose_version, uint32_t version, uint32_t build_date, const std::string& apkfile)
{
	tfile src(apkfile, GENERIC_READ, OPEN_EXISTING);
	const int fsize = posix_fsize(src.fp);
	VALIDATE(fsize > 0, null_str);

	const int one_block = 1024 * 1024;
	src.resize_data(one_block);

	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_apk));
	header.version = version;
	header.build_date = build_date;

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.zip_size = fsize;
	posix_fwrite(file.fp, &header, sizeof(header));

	int pos = 0;
	while (pos < fsize) {
		int bytes = one_block;
		if (pos + bytes > fsize) {
			bytes = fsize - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(file.fp, src.data, bytes);

		pos += bytes;
	}
	return true;
}

bool did_write_rsp_rosmap(tfile& file, const std::string& bundleid, const version_info& rose_version, const std::string& desc, const uint8_t* map_data, int map_len, const std::map<std::string, tmap_position>& positions, const std::map<std::string, tmap_marker>& markers)
{
	VALIDATE(desc.size() <= RSP_MAXDESCBYTES, null_str);
	VALIDATE(map_data != nullptr && map_len > 0, null_str);
	VALIDATE(positions.count(charge_pos_uuid) != 0, null_str);

	const int64_t ts = time(nullptr);

	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_rosmap));
	header.version = SDL_FOURCC(0, 0, 0, RSP_MAP_VER);
	header.build_date = ts_2_build_date(ts);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());

	const uint32_t positions_len = positions.size() * sizeof(trsp_rosmapposition);
	const uint32_t markers_len = markers.size() * sizeof(trsp_rosmapmarker);

	header.zip_size = sizeof(trsp_rosmap80bytes) + map_len + positions_len + markers_len;
	posix_fwrite(file.fp, &header, sizeof(header));

	trsp_rosmap80bytes rosmap;
	memset(&rosmap, 0, sizeof(trsp_rosmap80bytes));
	rosmap.ts = ts;
	// rosmap.fourcc = SDL_FOURCC('M', 'A', 'P', RSP_MAP_VER);
	rosmap.map = map_len;
	rosmap.positions = positions_len;
	rosmap.markers = markers_len;
	posix_fwrite(file.fp, &rosmap, sizeof(trsp_rosmap80bytes));

	posix_fwrite(file.fp, map_data, map_len);

	if (!positions.empty()) {
		const int bytes = rosmap.positions;
		file.resize_data(bytes);
		memset(file.data, 0, bytes);

		trsp_rosmapposition* to = (trsp_rosmapposition*)file.data;

		for (std::map<std::string, tmap_position>::const_iterator it = positions.begin(); it != positions.end(); ++ it, to = to + 1) {
			const tmap_position& from = it->second;
			VALIDATE(from.valid(), null_str);
			SDL_memcpy(to->uuid, from.uuid.c_str(), from.uuid.size());
			SDL_memcpy(to->name, from.name.c_str(), from.name.size());
			to->x = from.x;
			to->y = from.y;
			to->theta = from.theta;
		}
		posix_fwrite(file.fp, file.data, bytes);
	}

	if (!markers.empty()) {
		const int bytes = rosmap.markers;
		file.resize_data(bytes);
		memset(file.data, 0, bytes);

		trsp_rosmapmarker* to = (trsp_rosmapmarker*)file.data;
		for (std::map<std::string, tmap_marker>::const_iterator it2 = markers.begin(); it2 != markers.end(); ++ it2, to = to + 1) {
			const trsp_rosmapmarker& from = it2->second.rsp;
			VALIDATE(from.type >= 0 && from.type < rspmapmarkertype_count, null_str);
			SDL_memcpy(to, &from, sizeof(trsp_rosmapmarker));
		}
		posix_fwrite(file.fp, file.data, bytes);
	}

	return true;
}

version_info version_from_2uint32(uint32_t version, uint32_t build_date)
{
	int major = posix_hi8(posix_lo16(version));
	int minor = posix_lo8(posix_hi16(version));
	int revision_level = posix_hi8(posix_hi16(version));
	return version_info(major, minor, revision_level, true, '-', str_cast(build_date));
}

uint32_t ts_2_build_date(int64_t ts)
{
	const time_t t = ts; // for xcode(ios)
	tm* timeptr = localtime(&t);
	VALIDATE(timeptr != nullptr, null_str);
	uint32_t build_date = (1900 + timeptr->tm_year) * 10000 + (timeptr->tm_mon + 1) * 100 + timeptr->tm_mday;
	return build_date;
}

void save_y16_depth_file(const tdcintrinsics_C& intrinsics, int width, int height, const int16_t* depth_data, double depth_scale, double dcpitch, const std::string& result_file)
{
	VALIDATE(!result_file.empty(), null_str);

	tdepth_file_header header;
	memset(&header, 0, sizeof(tdepth_file_header));
	header.fourcc = SDL_FOURCC('D', 'E', 'P', 'T');
	header.width = width;
	header.height = height;
	header.intrinsics = intrinsics;
	header.depth_scale = depth_scale;
	header.dcpitch = dcpitch;

	{
		tfile file(result_file, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, &header, sizeof(header));
		posix_fwrite(file.fp, (const char*)depth_data, width * height * 2);
	}
}

void extract_file_data(const std::string& src, const std::string& dst, int64_t from, int64_t size)
{
	tfile file(src, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);

	int64_t fsize = posix_fsize(file.fp);
	if (!fsize) {
		return;
	}

	tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(dst_file.valid(), null_str);

	const int block_size = 4096;
	int actual_size;
	file.resize_data(block_size);

	int64_t stop = fsize <= from + size? fsize: from + size;
	posix_fseek(file.fp, from);

	int64_t pos = from;
	while (pos < stop) {
		actual_size = block_size <= stop - pos? block_size: stop - pos;
		posix_fread(file.fp, file.data, actual_size);
		posix_fwrite(dst_file.fp, file.data, actual_size);
		pos += actual_size;
	}
}

void single_open_copy(const std::string& src, const std::string& dst)
{
	// in common, to effect, copy file use "little" block, it require open both file at same time.
	// if shutdown, it maybe breakdown both file.
	// this function open at most one file at same time. it require more memory.
	std::unique_ptr<tfile> file;
	file.reset(new tfile(src, GENERIC_READ, OPEN_EXISTING));
	int fsize = file->read_2_data();
	if (fsize == 0) {
		return;
	}
	std::string data(file->data, fsize);
	file.reset();

	file.reset(new tfile(dst, GENERIC_WRITE, CREATE_ALWAYS));
	VALIDATE(file->valid(), null_str);
	posix_fwrite(file->fp, data.c_str(), fsize);
	file.reset();
}

bool file_is_all_ascii(const std::string& filename, bool verbose)
{
	VALIDATE(!filename.empty(), null_str);

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (fsize == 0) {
		return false;
	}

	for (int at = 0; at < fsize; at ++) {
		if (file.data[at] & 0x80) {
			if (verbose) {
				SDL_Log("#%i(0x%x) val: %02x", at, at, file.data[at]);
			}
			return false;
		}
	}
	return true;
}

bool create_directory_if_missing(const std::string& dirname)
{
	if (dirname.empty()) {
		return false;
	}

	if (is_directory(dirname)) {
		return true;
	} else if (file_exists(dirname)) {
		return false;
	}

	return SDL_MakeDirectory(dirname.c_str());
}

bool is_directory(const std::string& dir)
{
	if (dir.empty()) {
		return false;
	}

	return SDL_IsDirectory(dir.c_str());
}

bool file_exists(const std::string& name)
{
	if (name.empty()) {
		return false;
	}

	return SDL_IsFile(name.c_str());
}

static void setup_user_data_dir()
{
	const std::string& dir_path = game_config::preferences_dir;

	const bool res = create_directory_if_missing(dir_path);
	// probe read permissions (if we could make the directory)
	if (!res || !is_directory(dir_path)) {
		// could not open or create preferences directory at $dir_path;
		return;
	}

	// Create user data and add-on directories
	create_directory_if_missing(dir_path + "/cert");
	create_directory_if_missing(dir_path + "/data");
	create_directory_if_missing(dir_path + "/images");
	create_directory_if_missing(dir_path + "/images/misc");
	create_directory_if_missing(dir_path + "/saves");
	create_directory_if_missing(dir_path + "/tflites");

	create_directory_if_missing(dir_path + "/saves/logs");
	create_directory_if_missing(dir_path + "/saves/health");
}

void set_preferences_dir(std::string path)
{
#ifdef _WIN32
	WCHAR wc[MAX_PATH];
	char my_documents_path[MAX_PATH];
	HRESULT hr = SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, wc);
	VALIDATE(SUCCEEDED(hr), "os must support SHGetFolderPath.");

	// unicode to utf8
	WideCharToMultiByte(CP_UTF8, 0, wc, -1, my_documents_path, MAX_PATH, NULL, NULL);
	const char last_char = my_documents_path[strlen(my_documents_path) - 1];
	VALIDATE(last_char != '\\' || last_char != '/', null_str);
	game_config::preferences_dir = std::string(my_documents_path) + "/RoseApp/" + path;

#elif defined(ANDROID)
	// SDL_GetPrefPath use SDL_AndroidGetInternalStoragePath(). format: /data/data/com.leagor.sleep/files
	// SDL_AndroidGetExternalStoragePath(), format: /storage/emulated/0/Android/data/com.leagor.sleep/files
	game_config::preferences_dir = SDL_AndroidGetExternalStoragePath();
	if (!path.empty()) {
		game_config::preferences_dir += "/" + path;
	}

#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
    game_config::preferences_dir = getenv("HOME");
#else
    game_config::preferences_dir = get_cwd() + "/..";
#endif
    if (!path.empty()) {
        game_config::preferences_dir += "/" + path;
    }
#else

	std::string path2 = ".wesnoth" + game_config::version.str(false);

	if (path.empty()) path = path2;

	const char* home_str = getenv("HOME");
	std::string home = home_str ? home_str : ".";

	if (path[0] == '/')
		game_config::preferences_dir = path;
	else
		game_config::preferences_dir = home + std::string("/") + path;

#endif /*_WIN32*/

	game_config::preferences_dir = utils::normalize_path(game_config::preferences_dir);
	SDL_Log("set_preferences, user_data_dir: %s", game_config::preferences_dir.c_str());

	setup_user_data_dir();
}

const std::string& get_user_data_dir()
{
	return game_config::preferences_dir;
}

const std::string& get_user_config_dir()
{
	return game_config::preferences_dir;
}

// Only valid when a and b have the same sign and b > 0
#define posix_pages2(a, b)	(((a) + (b) - 1) / (b))

int posix_pages(int dividend, int divisor)
{
	VALIDATE(dividend >= 0 && divisor > 0, null_str);
	int result = dividend / divisor + (dividend % divisor? 1: 0);
	VALIDATE(result == posix_pages2(dividend, divisor), null_str);

	return result;
}

