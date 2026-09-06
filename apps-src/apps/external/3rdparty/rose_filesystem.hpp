/* $Id: filesystem.hpp 47496 2010-11-07 15:53:11Z silene $ */
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
 * Declarations for File-IO.
 */

#ifndef LIBROSE2_FILESYSTEM_HPP_INCLUDED
#define LIBROSE2_FILESYSTEM_HPP_INCLUDED

#include "rose_filesystem_dll.hpp"



class tdcdepth_data
{
	// now only support y16 format
public:
	tdcdepth_data()
		: data(nullptr)
		, data_len(0)
	{
		clear();
	}

	~tdcdepth_data()
	{
		clear();
	}

	bool load_from_file(const std::string& filename);
	bool valid() const { return !datafile.empty() && data != nullptr && data_len > 0; }

	void clear()
	{
		datafile.clear();
		if (data != nullptr) {
			// VALIDATE(data_len != 0, null_str);
			free(data);
			data = nullptr;

			data_len = 0;
		}
	}

public:
	std::string datafile; // full rspfile name
	tdepth_file_header header;

	uint16_t* data;
	int data_len;
};

bool did_write_rsp_applet(tfile& file, const std::string& bundleid, const version_info& rose_version, uint32_t version, uint32_t build_date, const std::string& src_zip);
bool did_write_rsp_apk(tfile& file, const std::string& bundleid, const version_info& rose_version, uint32_t version, uint32_t build_date, const std::string& apkfile);
bool did_write_rsp_rosmap(tfile& file, const std::string& bundleid, const version_info& rose_version, const std::string& desc, const uint8_t* map_data, int map_len, const std::map<std::string, tmap_position>& positions, const std::map<std::string, tmap_marker>& markers);

version_info version_from_2uint32(uint32_t version, uint32_t build_date);
uint32_t ts_2_build_date(int64_t ts);

void save_y16_depth_file(const tdcintrinsics_C& intrinsics, int width, int height, const int16_t* depth_data, double depth_scale, double dcpitch, const std::string& result_file);

void extract_file_data(const std::string& src, const std::string& dst, int64_t from, int64_t size);
void single_open_copy(const std::string& src, const std::string& dst);
bool file_is_all_ascii(const std::string& filename, bool verbose);

/**
 * Creates a directory if it does not exist already.
 *
 * @param dirname                 Path to directory. All parents should exist.
 * @returns                       True if the directory exists or could be
 *                                successfully created; false otherwise.
 */
bool create_directory_if_missing(const std::string& dirname);

/** Returns true if the given file is a directory. */
bool is_directory(const std::string& fname);

/** Returns true if a file or directory with such name already exists. */
bool file_exists(const std::string& name);

void set_preferences_dir(std::string path);

const std::string &get_user_config_dir();
const std::string &get_user_data_dir();

#endif
