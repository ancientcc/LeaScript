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

#ifndef LIBROSE_FILESYSTEM_HPP_INCLUDED
#define LIBROSE_FILESYSTEM_HPP_INCLUDED

#include <time.h>
#include <stdlib.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "util.hpp"
#include "exceptions.hpp"
#include <rose_filesystem.hpp>

/** An exception object used when an IO error occurs */
struct io_exception : public game::error {
	io_exception() : game::error("") {}
	io_exception(const std::string& msg) : game::error(msg) {}
};

struct file_tree_checksum;

/**
 * Populates 'files' with all the files and
 * 'dirs' with all the directories in dir.
 * If files or dirs are NULL they will not be used.
 *
 * mode: determines whether the entire path or just the filename is retrieved.
 * filter: determines if we skip images and sounds directories
 * reorder: triggers the special handling of _main.cfg and _final.cfg
 * checksum: can be used to store checksum info
 */
void get_files_in_dir(const std::string &dir,
					std::vector<std::string>* files,
					std::vector<std::string>* dirs = nullptr,
					bool entire_file_path = false,
					bool skip_media_dir = false,
					bool reorder_wml = false,
					file_tree_checksum* checksum = nullptr,
					bool subdir = true);

std::string get_dir(const std::string &dir);

// The location of various important files:
std::string get_prefs_file();
std::string get_prefs_back_file();
std::string get_critical_prefs_file();
std::string get_saves_dir();
std::string get_intl_dir();
std::string get_screenshot_dir();
std::string get_addon_campaigns_dir();

/**
 * Get the next free filename using "name + number (3 digits) + extension"
 * maximum 1000 files then start always giving 999
 */
std::string get_next_filename(const std::string& name, const std::string& extension);


std::string get_cwd();
std::string get_exe_dir();

// Basic disk I/O:
std::string read_map(const std::string& name);

/**
 * Creates a recursive directory tree if it does not exist already
 * @param dirname                 Full path of target directory. Non existing parents
 *                                will be created
 * @return                        True if the directory exists or could be
 *                                successfully created; false otherwise.
 */
bool create_directory_if_missing_recursive(const std::string& dirname);

/** Get the creation time of a file. */
// time_t file_create_time(const std::string& fname);

/** Returns true if the file ends with '.gz'. */
bool is_gzip_file(const std::string& filename);

struct file_tree_checksum
{
	file_tree_checksum();
	explicit file_tree_checksum(const class config& cfg);
	void write(class config& cfg) const;
	void reset() {nfiles = 0;modified = 0;sum_size=0;};
	// @todo make variables private!
	size_t nfiles, sum_size;
	time_t modified;
	bool operator==(const file_tree_checksum &rhs) const;
	bool operator!=(const file_tree_checksum &rhs) const
	{ return !operator==(rhs); }
};

// only used for preprocessor system.
extern file_tree_checksum* preprocessor_checksum;

/** Get the time at which the data/ tree was last modified at. */
const file_tree_checksum& data_tree_checksum(bool reset = false, bool skip_media_dir = true);

/** Get the time at which the data/ tree was last modified at. (used by editor)*/
void data_tree_checksum(const std::vector<std::string>& paths, file_tree_checksum& checksum, bool skip_media_dir);

void data_tree_checksum(const std::vector<std::string>& defines, const std::vector<std::string>& paths, file_tree_checksum& checksum);

// Returns the size of a file, or -1 if the file doesn't exist.
// int64_t file_size(const std::string& file);

bool ends_with(const std::string& str, const std::string& suffix);

std::set<std::string> files_in_directory(const std::string& path);
char path_sep(bool standard);


std::string get_hdpi_name(const std::string& name, double hdpi_scale);
/*
typedef std::function<bool (const std::string& dir, const SDL_dirent2* dirent)> twalk_dir_function;
// @rootdir: root directory desired walk
// @subfolders: walk sub-directory or not.
// remark:
//  1. You cannot delete use this function. when is directory, call RemoveDirectory always fail(errcode:32, other process using it)
//    must after FindClose. If delete, call SHFileOperation.
bool walk_dir(const std::string& rootdir, bool subfolders, const twalk_dir_function& fn);
*/
bool did_walk_white_extname(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, std::string>& files, const std::string& root, const std::vector<std::string>& white_extname);
bool copy_root_files(const std::string& src, const std::string& dst, std::set<std::string>* files, const std::function<bool(const std::string& src)>& filter = NULL);
bool compare_directory(const std::string& dir1, const std::string& dir2);

struct tpath_summary {
	int files;
	int dirs;
	int64_t bytes;
};
void path_summary(tpath_summary& summary, const char* name, bool first = true);

/**
 *  The paths manager is responsible for recording the various paths
 *  that binary files may be located at.
 *  It should be passed a config object which holds binary path information.
 *  This is in the format
 *@verbatim
 *    [binary_path]
 *      path=<path>
 *    [/binary_path]
 *  Binaries will be searched for in [wesnoth-path]/data/<path>/images/
 *@endverbatim
 */
struct binary_paths_manager
{
	static const char full_path_indicator = '^';
	binary_paths_manager();
	binary_paths_manager(const class config& cfg);
	~binary_paths_manager();

	void set_paths(const class config& cfg);
	const std::vector<std::string>& paths() const { return paths_; }

private:
	binary_paths_manager(const binary_paths_manager& o);
	binary_paths_manager& operator=(const binary_paths_manager& o);

	void cleanup();

	std::vector<std::string> paths_;
};

void clear_binary_paths_cache();

/**
 * Returns a vector with all possible paths to a given type of binary,
 * e.g. 'images', 'sounds', etc,
 */
const std::vector<std::string>& get_binary_paths(const std::string& type);

/**
 * Returns a complete path to the actual file of a given @a type
 * or an empty string if the file isn't present.
 */
std::string get_binary_file_location(const std::string& type, const std::string& filename);

/**
 * Returns a complete path to the actual directory of a given @a type
 * or an empty string if the directory isn't present.
 */
std::string get_binary_dir_location(const std::string &type, const std::string &filename);

/**
 * Returns a complete path to the actual WML file or directory
 * or an empty string if the file isn't present.
 */
std::string get_wml_location(const std::string &filename, const std::string &current_dir = std::string());

/**
 * Returns a short path to @a filename, skipping the (user) data directory.
 */
std::string get_short_wml_path(const std::string &filename);

std::set<std::string> apps_in_res();
std::set<std::string> aplts_in_dir(const std::string& dir);
#define aplts_in_res()				aplts_in_dir(game_config::path)
#define aplts_in_preferences_dir()	aplts_in_dir(game_config::preferences_dir)
std::vector<std::string> get_disks();

class scoped_istream {
	std::istream *stream;
public:
	scoped_istream(std::istream *s): stream(s) {}
	scoped_istream& operator=(std::istream *);
	std::istream &operator*() { return *stream; }
	std::istream *operator->() { return stream; }
	~scoped_istream();
};

class scoped_ostream {
	std::ostream *stream;
public:
	scoped_ostream(std::ostream *s): stream(s) {}
	scoped_ostream& operator=(std::ostream *);
	std::ostream &operator*() { return *stream; }
	std::ostream *operator->() { return stream; }
	~scoped_ostream();
};

void conv_ansi_utf8(std::string &name, bool a2u);
std::string conv_ansi_utf8_2(const std::string &name, bool a2u);
const char* utf8_2_ansi(const std::string& str);
const char* ansi_2_utf8(const std::string& str);

bool file_replace_string(const std::string& src_file, const std::string& dst_file, const std::vector<std::pair<std::string, std::string> >& replaces);
bool file_replace_string(const std::string& src_file, const std::pair<std::string, std::string>& replace);
bool file_replace_string(const std::string& src_file, const std::vector<std::pair<std::string, std::string> >& replaces);

char* saes_encrypt_heap(const char* ptext, int size, unsigned char* key);
char* saes_decrypt_heap(const char* ctext, int size, unsigned char* key);
class tsaes_encrypt
{
public:
	tsaes_encrypt(const char* ctext, int s, unsigned char* key);
	~tsaes_encrypt()
	{
		if (buf) {
			free(buf);
		}
	}

	char* buf;
	int size;
};

class tsaes_decrypt
{
public:
	tsaes_decrypt(const char* ptext, int s, unsigned char* key);
	~tsaes_decrypt()
	{
		if (buf) {
			free(buf);
		}
	}

	char* buf;
	int size;
};

void convert_yuv_2_argb(const std::string& filename, const int width, const int height, int format, const std::string& out_png);

#endif
