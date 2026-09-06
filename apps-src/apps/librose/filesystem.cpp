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
#define GETTEXT_DOMAIN "rose-lib"

#include "rose_global.hpp"

// Include files for opendir(3), readdir(3), etc.
// These files may vary from platform to platform,
// since these functions are NOT ANSI-conforming functions.
// They may have to be altered to port to new platforms

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

// for getenv
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <set>
// #include <boost/algorithm/string.hpp>

// for strerror
#include <cstring>

#include "config.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "loadscreen.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "saes.hpp"
#include "rose_version.hpp"
#include "wml_exception.hpp"
#include "theme.hpp"
#include "config_cache.hpp"
#include "libyuv/convert_argb.h"
#include "libyuv/video_common.h"

#include <openssl/sha.h>
#include <openssl/mem.h>

#include <boost/foreach.hpp>
using namespace std::placeholders;

namespace {
	// const mode_t AccessMode = 00770;

	// These are the filenames that get special processing
	const std::string maincfg_filename = "_main.cfg";
	const std::string finalcfg_filename = "_final.cfg";
	const std::string initialcfg_filename = "_initial.cfg";

}

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBase.h>
#endif

static std::set<std::string> predef_dirs;
static std::map<int, std::set<std::string> > predef_files;

bool ends_with(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() && std::equal(suffix.begin(),suffix.end(),str.end()-suffix.size());
}

void get_files_in_dir(const std::string& directory,
					std::vector<std::string>* files,
					std::vector<std::string>* dirs,
					bool entire_file_path,
					bool skip_media_dir,
					bool reorder_wml,
					file_tree_checksum* checksum,
                    bool subdir)
{
	// If we have a path to find directories in,
	// then convert relative pathnames to be rooted
	// on the wesnoth path
	if (!SDL_IsFromRootPath(directory.c_str())) {
		std::string dir = game_config::path + "/" + directory;
		if (is_directory(dir)) {
			get_files_in_dir(dir, files, dirs, entire_file_path, skip_media_dir, reorder_wml, checksum);
		}
		return;
	}

	if (reorder_wml) {
		std::string maincfg;
		if (directory.empty() || directory[directory.size()-1] == '/') {
			maincfg = directory + maincfg_filename;
		} else {
			maincfg = (directory + "/") + maincfg_filename;
		}

		if (file_exists(maincfg)) {
			if (files != NULL) {
				if (entire_file_path) {
					files->push_back(maincfg);
				} else {
					files->push_back(maincfg_filename);
				}
			}
			return;
		}
	}

	SDL_DIR* dir = SDL_OpenDir(directory.c_str());
	if (dir == NULL) {
		return;
	}

	SDL_dirent2* entry;
	while ((entry = SDL_ReadDir(dir)) != NULL) {
		if (entry->name[0] == '.') {
			continue;
		}

		// generic Unix
		const std::string basename = entry->name;

		std::string fullname;
		if (directory.empty() || directory[directory.size()-1] == '/') {
			fullname = directory + basename;
		} else {
			fullname = directory + "/" + basename;
		}

		if (SDL_DIRENT_DIR(entry->mode)) {
			if (skip_media_dir && (basename == "images" || basename == "sounds" || basename == "music")) {
				continue;
			}

			if (reorder_wml && file_exists(fullname + "/" + maincfg_filename)) {
				if (files != NULL) {
					if (entire_file_path) {
						files->push_back(fullname + "/" + maincfg_filename);
					} else {
						files->push_back(basename + "/" + maincfg_filename);
					}
				} else {
					// Show what I consider strange
					// <fullname>/<maincfg_filename> not used now but skip the directory.
				}
			} else if (dirs != NULL) {
				if (entire_file_path) {
					dirs->push_back(fullname);
				} else {
					dirs->push_back(basename);
				}
			}
		} else {
			if (files != NULL) {
				if (entire_file_path) {
					files->push_back(fullname);
				} else {
					files->push_back(basename);
				}
			}
			if (checksum != NULL) {
				if (entry->mtime > checksum->modified) {
					checksum->modified = entry->mtime;
				}
				checksum->sum_size += entry->size;
				checksum->nfiles++;
			}
		}
	}

	SDL_CloseDir(dir);

	if (files != NULL) {
		std::sort(files->begin(),files->end());
	}

	if (dirs != NULL) {
		std::sort(dirs->begin(),dirs->end());
	}

	if (files != NULL && reorder_wml) {
		// move finalcfg_filename, if present, to the end of the vector
		for (unsigned int i = 0; i < files->size(); i++) {
			if (ends_with((*files)[i], "/" + finalcfg_filename)) {
				files->push_back((*files)[i]);
				files->erase(files->begin()+i);
				break;
			}
		}
		// move initialcfg_filename, if present, to the beginning of the vector
		int foundit = -1;
		for (unsigned int i = 0; i < files->size(); i++)
			if (ends_with((*files)[i], "/" + initialcfg_filename)) {
				foundit = i;
				break;
			}
		if (foundit > 0) {
			std::string initialcfg = (*files)[foundit];
			for (unsigned int i = foundit; i > 0; i--)
				(*files)[i] = (*files)[i-1];
			(*files)[0] = initialcfg;
		}
	}
}

std::string get_prefs_file()
{
	return get_user_data_dir() + "/preferences";
}

std::string get_prefs_back_file()
{
	return get_user_data_dir() + "/preferences" + backup_postfix;
}

std::string get_critical_prefs_file()
{
	return get_user_data_dir() + "/critical_preferences";
}

std::string get_saves_dir()
{
	return get_user_data_dir() + "/saves";
}

std::string get_addon_campaigns_dir()
{
	return get_user_data_dir() + "/data/add-ons";
}

std::string get_intl_dir()
{
	return game_config::path + "/translations";
}

std::string get_screenshot_dir()
{
	return get_user_data_dir() + "/screenshots";
}

std::string get_next_filename(const std::string& name, const std::string& extension)
{
	std::string next_filename;
	int counter = 0;

	do {
		std::stringstream filename;

		filename << name;
		filename.width(3);
		filename.fill('0');
		filename.setf(std::ios_base::right);
		filename << counter << extension;
		counter++;
		next_filename = filename.str();
	} while(file_exists(next_filename) && counter < 1000);
	return next_filename;
}


std::string get_dir(const std::string& dir_path)
{
	if (is_directory(dir_path)) {
		return dir_path;
	}
	if (SDL_MakeDirectory(dir_path.c_str())) {
		return dir_path;
	}
	return null_str;
}

std::string get_cwd()
{
	char buf[1024];
	const char* const res = getcwd(buf,sizeof(buf));
	if(res != NULL) {
		std::string str(res);

#ifdef _WIN32
		conv_ansi_utf8(str, true);
		std::replace(str.begin(), str.end(),'\\','/');
#endif

		return str;
	} else {
		return "";
	}
}

std::string get_exe_dir()
{
#ifndef _WIN32
	char buf[1024];
	size_t path_size = readlink("/proc/self/exe", buf, 1024);
	if(path_size == static_cast<size_t>(-1))
		return std::string();
	buf[path_size] = 0;
	return std::string(dirname(buf));
#else
	const std::string path = get_cwd();
	return path;
#endif
}

bool create_directory_if_missing_recursive(const std::string& dirname)
{
	if (is_directory(dirname) == false && dirname.empty() == false)
	{
		std::string tmp_dirname = dirname;
		// remove trailing slashes or backslashes
		while ((tmp_dirname[tmp_dirname.size()-1] == '/' ||
			  tmp_dirname[tmp_dirname.size()-1] == '\\') &&
			  tmp_dirname.size()>0)
		{
			tmp_dirname.erase(tmp_dirname.size()-1);
		}

		// create the first non-existing directory
		size_t pos = tmp_dirname.rfind("/");

		// we get the most right directory and *skip* it
		// we are creating it when we get back here
		if (tmp_dirname.rfind('\\') != std::string::npos &&
			tmp_dirname.rfind('\\') > pos )
			pos = tmp_dirname.rfind('\\');

		if (pos != std::string::npos)
			create_directory_if_missing_recursive(tmp_dirname.substr(0,pos));

		return create_directory_if_missing(tmp_dirname);
	}
	return create_directory_if_missing(dirname);
}

void preprocess_res_explorer()
{
	Uint32 start = SDL_GetTicks();

	// copy font directory from res/fonts to user_data_dir/fonts
	std::vector<std::string> v;
	v.push_back("Andagii.ttf");
	v.push_back("DejaVuSans.ttf");
	// v.push_back("wqy-zenhei.ttc");
	v.push_back("PingFang-SC-Regular.ttc");
	create_directory_if_missing(game_config::preferences_dir + "/fonts");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = game_config::path + "/fonts/" + *it;
		const std::string dst = game_config::preferences_dir + "/fonts/" + *it;

		std::unique_ptr<tfile> dst_file;
		dst_file.reset(new tfile(dst, GENERIC_READ, OPEN_EXISTING));

		if (!dst_file->valid() || posix_fsize(dst_file->fp) == 0) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			dst_file.reset(nullptr);
			dst_file.reset(new tfile(dst, GENERIC_WRITE, CREATE_ALWAYS));
			if (src_file.valid() && dst_file->valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file->fp, src_file.data, fsize);
			}
		}
	}
/*
	v.clear();
	v.push_back("wechat_qrcode/detect.caffemodel");
	v.push_back("wechat_qrcode/detect.prototxt");
	v.push_back("wechat_qrcode/sr.caffemodel");
	v.push_back("wechat_qrcode/sr.prototxt");
	create_directory_if_missing(game_config::preferences_dir + "/tflites/wechat_qrcode");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = game_config::path + "/data/core/tflites/" + *it;
		const std::string dst = game_config::preferences_dir + "/tflites/" + *it;
		if (!file_exists(dst)) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			if (src_file.valid() && dst_file.valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file.fp, src_file.data, fsize);
			}
		}
	}
*/
	SDL_Log("preprocess res explorer, used %u", SDL_GetTicks() - start);
}

std::string read_map(const std::string& name)
{
	std::string res;
	std::string map_location = get_wml_location("maps/" + name);
	if(!map_location.empty()) {
		res = read_file(map_location);
	}

	if (res.empty()) {
		res = read_file(get_user_data_dir() + "/editor/maps/" + name);
	}

	return res;
}
/*
time_t file_create_time(const std::string& fname)
{
	SDL_dirent dirent;
	if (!SDL_GetStat(fname.c_str(), &dirent)) {
		return 0;
	}
	return dirent.mtime;
}

int64_t file_size(const std::string& file)
{
	VALIDATE(!file.empty(), null_str);

	int64_t fsize = 0;
	SDL_RWops* __h = SDL_RWFromFile(file.c_str(), "rb");
	if (__h) {
		posix_fseek(__h, 0);
		fsize = posix_fsize(__h);
		posix_fclose(__h);
    }
	return fsize;
}
*/
/**
 * Returns true if the file ends with '.gz'.
 *
 * @param filename                The name to test.
 */
bool is_gzip_file(const std::string& filename)
{
	return (filename.length() > 3
		&& filename.substr(filename.length() - 3) == ".gz");
}

file_tree_checksum* preprocessor_checksum = nullptr;

file_tree_checksum::file_tree_checksum()
	: nfiles(0), sum_size(0), modified(0)
{}

file_tree_checksum::file_tree_checksum(const config& cfg) :
	nfiles	(lexical_cast_default<size_t>(cfg["nfiles"])),
	sum_size(lexical_cast_default<size_t>(cfg["size"])),
	modified(lexical_cast_default<time_t>(cfg["modified"]))
{
}

void file_tree_checksum::write(config& cfg) const
{
	cfg["nfiles"] = lexical_cast<std::string>(nfiles);
	cfg["size"] = lexical_cast<std::string>(sum_size);
	cfg["modified"] = lexical_cast<std::string>(modified);
}

bool file_tree_checksum::operator==(const file_tree_checksum &rhs) const
{
	return nfiles == rhs.nfiles && sum_size == rhs.sum_size &&
		modified == rhs.modified;
}

static void get_file_tree_checksum_internal(const std::string& path, file_tree_checksum& res, bool skip_media_dir)
{

	std::vector<std::string> dirs;
	get_files_in_dir(path, NULL, &dirs, true, skip_media_dir, false, &res);
	loadscreen::increment_progress();

	for (std::vector<std::string>::const_iterator j = dirs.begin(); j != dirs.end(); ++j) {
		get_file_tree_checksum_internal(*j, res, skip_media_dir);
	}
}

const file_tree_checksum& data_tree_checksum(bool reset, bool skip_media_dir)
{
	static file_tree_checksum checksum;
	if (reset) {
		checksum.reset();
	}
	if (checksum.nfiles == 0) {
		get_file_tree_checksum_internal("data/", checksum, skip_media_dir);
		get_file_tree_checksum_internal(get_user_data_dir() + "/data/", checksum, skip_media_dir);
	}

	return checksum;
}

void data_tree_checksum(const std::vector<std::string>& paths, file_tree_checksum& checksum, bool skip_media_dir)
{
	checksum.reset();
	for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		get_file_tree_checksum_internal(*it, checksum, skip_media_dir);
	}
}

struct tpreprocessor_checksum_lock
{
	tpreprocessor_checksum_lock(file_tree_checksum& checksum)
	{
		VALIDATE(!preprocessor_checksum, null_str);
		preprocessor_checksum = &checksum;
	}
	~tpreprocessor_checksum_lock()
	{
		preprocessor_checksum = nullptr;
	}
};

void data_tree_checksum(const std::vector<std::string>& defines, const std::vector<std::string>& paths, file_tree_checksum& checksum)
{
	tpreprocessor_checksum_lock lock(checksum);
	checksum.reset();

	config_cache_transaction transaction;
	config_cache& cache = config_cache::instance();
	cache.clear_defines();
	for (std::vector<std::string>::const_iterator it = defines.begin(); it != defines.end(); ++ it) {
		const std::string& macro = *it;
		cache.add_define(macro);
	}

	config fake_cfg;
	for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		const std::string& path = *it;
#ifdef ANDROID
		// now, android cannot suprt *.cfg in assets.
#else
		cache.get_config(path, fake_cfg);
#endif
	}
}


namespace {

#define PRIORITIEST_BINARY_PATHS	1
std::vector<std::string> binary_paths;

typedef std::map<std::string,std::vector<std::string> > paths_map;
paths_map binary_paths_cache;

}

static void init_binary_paths()
{
	if (binary_paths.empty()) {
		binary_paths.push_back(""); // userdata directory
		// here is self-add paths
		binary_paths.push_back(game_config::app_dir + "/");
		binary_paths.push_back("data/core/");
	}
}

binary_paths_manager::binary_paths_manager() : paths_()
{}

binary_paths_manager::binary_paths_manager(const config& cfg) : paths_()
{
	set_paths(cfg);
}

binary_paths_manager::~binary_paths_manager()
{
	cleanup();
}

void binary_paths_manager::set_paths(const config& cfg)
{
	cleanup();
	init_binary_paths();

	int inserted = 0;
	BOOST_FOREACH (const config &bp, cfg.child_range("binary_path")) {
		std::string path = bp["path"].str();
		if (path.find("..") != std::string::npos) {
			// Invalid binary path $path
			continue;
		}
		if (!path.empty() && path[path.size()-1] != '/') {
			path += "/";
		}
		std::vector<std::string>::iterator it = std::find(binary_paths.begin(), binary_paths.end(), path);
		if (it == binary_paths.end()) {
			it = binary_paths.begin();
			std::advance(it, PRIORITIEST_BINARY_PATHS + inserted);
			binary_paths.insert(it, path);

			VALIDATE(std::find(paths_.begin(), paths_.end(), path) == paths_.end(), null_str);

			paths_.push_back(path);
			inserted ++;
		}
	}
}

void binary_paths_manager::cleanup()
{
	binary_paths_cache.clear();

	for (std::vector<std::string>::const_iterator i = paths_.begin(); i != paths_.end(); ++i) {
		std::vector<std::string>::iterator it2 = std::find(binary_paths.begin(), binary_paths.end(), *i);
		binary_paths.erase(it2);
	}
	paths_.clear();
}

void clear_binary_paths_cache()
{
	binary_paths_cache.clear();
}

const std::vector<std::string>& get_binary_paths(const std::string& type)
{
	const paths_map::const_iterator itor = binary_paths_cache.find(type);
	if(itor != binary_paths_cache.end()) {
		return itor->second;
	}

	if (type.find("..") != std::string::npos) {
		// Not an assertion, as language.cpp is passing user data as type.
		// Invalid WML type $type for binary paths
		static std::vector<std::string> dummy;
		return dummy;
	}

	std::vector<std::string>& res = binary_paths_cache[type];

	init_binary_paths();

	BOOST_FOREACH (const std::string &path, binary_paths)
	{
		if (path.empty()) {
			res.push_back(get_user_data_dir() + "/" + type + "/");

		} else if (path[0] == binary_paths_manager::full_path_indicator) {
			res.push_back(path.substr(1) + type + "/");

		} else {
			res.push_back(game_config::path + "/" + path + type + "/");
		}
	}
	return res;
}

std::string get_binary_file_location(const std::string& type, const std::string& filename)
{
	if (filename.empty()) {
		return null_str;
	}

	if (filename.find("..") != std::string::npos) {
		return null_str;
	}

	const size_t theme_end_chars_size = theme::path_end_chars.size();

	const std::vector<std::string>& binary_paths = get_binary_paths(type);
	for (std::vector<std::string>::const_iterator it = binary_paths.begin(); it != binary_paths.end(); ++ it) {
		const std::string& path = *it;
		const std::string file = path + filename;
		
		if (theme_end_chars_size && !path.compare(path.size() - theme_end_chars_size, theme_end_chars_size, theme::path_end_chars)) {
			// if maybe require theme special, priority get it.
			std::string dir = utils::extract_directory(file);
			std::string file2 = dir + "/" + theme::instance.id + (file.c_str() + dir.size());
			if (file_exists(file2)) {
				return file2;
			}	
		}

		if (file_exists(file)) {
			return file;
		}
	}

	return std::string();
}

std::string get_binary_dir_location(const std::string &type, const std::string &filename)
{
	if (filename.empty()) {
		return std::string();
	}

	if (filename.find("..") != std::string::npos) {
		// Illegal path $filename ' (\"..\" not allowed).
		return std::string();
	}

	BOOST_FOREACH (const std::string &path, get_binary_paths(type))
	{
		const std::string file = path + filename;
		if (is_directory(file)) {
			return file;
		}
	}

	return std::string();
}

std::string get_wml_location(const std::string &filename, const std::string &current_dir)
{
	std::string result;

	if (filename.empty()) {
		return result;
	}

	if (filename.find("..") != std::string::npos) {
		return result;
	}

	const char first = filename[0];
	if (first == '~')  {
		// If the filename starts with '~', look in the user data directory.
		// result = get_user_data_dir() + "/data/" + filename.substr(1);
		result = get_user_data_dir() + "/" + filename.substr(1);

	} else if (filename.size() >= 2 && first == '.' && filename[1] == '/') {
		// !!!FIX, I want get rid of this branch in future.
		// If the filename begins with a "./", look in the same directory
		// as the file currrently being preprocessed.
		result = current_dir + filename.substr(2);

	} else if (first == '^') {
		// If the filename starts with '^', look in the topest directory.
		result = game_config::path + "/" + filename.substr(1);

	} else if (!game_config::path.empty()) {
		result = game_config::path + "/data/" + filename;
	}

	if (result.empty() || (!file_exists(result) && !is_directory(result))) {
		result.clear();
	}

	return result;
}

std::string get_short_wml_path(const std::string &filename)
{
	std::string match = get_user_data_dir() + "/data/";
	if (filename.find(match) == 0) {
		return "~" + filename.substr(match.size());
	}
	match = game_config::path + "/data/";
	if (filename.find(match) == 0) {
		return filename.substr(match.size());
	}
	return filename;
}

static bool collect_app(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& apps)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		const std::string app = game_config::app_from_app_dir(dirent->name);
		if (!app.empty()) {
			apps.insert(app);
		}
	}

	return true;
}

std::set<std::string> apps_in_res()
{
	std::set<std::string> ret;
	::walk_dir(game_config::path, false, std::bind(
				&collect_app
				, _1, _2, std::ref(ret)));
	return ret;
}

static bool collect_aplt(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& aplts)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		if (is_lua_bundleid(dirent->name)) {
			aplts.insert(dirent->name);
		}
	}

	return true;
}

std::set<std::string> aplts_in_dir(const std::string& dir)
{
	std::set<std::string> ret;
	::walk_dir(dir, false, std::bind(
				&collect_aplt
				, _1, _2, std::ref(ret)));
	return ret;
}

std::vector<std::string> get_disks()
{
	std::vector<std::string> ret;
#ifdef _WIN32
	int DiskCount = 0;
	DWORD DiskInfo = GetLogicalDrives();
	while (DiskInfo) {
		if (DiskInfo & 1) {
			DiskCount ++;
		}
		DiskInfo = DiskInfo >> 1;
	}

	int DSLength = GetLogicalDriveStrings(0, NULL);
	wchar_t* DStr = new wchar_t[DSLength];
	GetLogicalDriveStrings(DSLength, (LPTSTR)DStr);

	char utf8_str[64];

	wchar_t* lpDriveStr = DStr;
	ULARGE_INTEGER i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	for (int i = 0; i < DiskCount; ++ i, lpDriveStr += 4) {
		int DType = GetDriveType(lpDriveStr);
		if (DType != DRIVE_FIXED && DType != DRIVE_REMOVABLE) {
			continue;
			
		}

		BOOL fResult = GetDiskFreeSpaceEx(lpDriveStr, (PULARGE_INTEGER)&i64FreeBytesToCaller,(PULARGE_INTEGER)&i64TotalBytes, (PULARGE_INTEGER)&i64FreeBytes);
		if (!fResult || i64TotalBytes.QuadPart < 4I64 * 1024 * 1024) {
			continue;
		}

		// unicode to utf8
		WideCharToMultiByte(CP_UTF8, 0, lpDriveStr, -1, utf8_str, MAX_PATH, NULL, NULL);
		ret.push_back(utils::normalize_path(utf8_str));
	}
#else
	ret.push_back("/");
#endif

	return ret;
}

static bool collect_file(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& files)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		files.insert(dirent->name);
	}

	return true;
}

std::set<std::string> files_in_directory(const std::string& path)
{
	VALIDATE(!path.empty(), null_str);

	std::set<std::string> ret;
	::walk_dir(path, false, std::bind(
				&collect_file
				, _1, _2, std::ref(ret)));
	return ret;
}

char path_sep(bool standard)
{
#ifdef _WIN32
	return standard? '\\': '/';
#else
	return standard? '/': '\\';
#endif
}

std::string get_hdpi_name(const std::string& name, double hdpi_scale)
{
	size_t pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name;
	}
	int _hdpi_scale = round(hdpi_scale);
	char flag[4] = {'@', static_cast<char>(_hdpi_scale + 0x30), 'x', '\0'};

	std::string ret = name;
	ret.insert(pos, flag);
	return ret;
}
/*
bool walk_dir(const std::string& rootdir, bool subfolders, const twalk_dir_function& fn)
{
	bool ret = true;
	std::stringstream ss;
	SDL_DIR* dir = SDL_OpenDir(rootdir.c_str());
	if (!dir) {
		return false;
	}
	SDL_dirent2* dirent;
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
				if (fn) {
					// shallow->deep: first call fn, then walk it.
					if (!fn(rootdir, dirent)) {
						ret = false;
						break;
					}
				}
				ss.str("");
				ss << rootdir << "/" << dirent->name;
				if (subfolders) {
					walk_dir(ss.str(), true, fn);
				}
			}
		} else {
			// file
			if (fn) {
				if (!fn(rootdir, dirent)) {
					ret = false;
					break;
				}
			}
		}
	}
	SDL_CloseDir(dir);

	return ret;
}
*/
bool did_walk_white_extname(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, std::string>& files, const std::string& root, const std::vector<std::string>& white_extname)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		const std::string name = utils::lowercase(dirent->name);

		bool hit = false;
		for (std::vector<std::string>::const_iterator it = white_extname.begin(); it != white_extname.end(); ++ it) {
			const std::string& extname = *it;
			size_t pos = name.rfind(extname);
			if (pos != std::string::npos && pos + extname.size() == name.size()) {
				hit = true;
				break;
			}
		}
		if (hit) {
			// files example. root = c:/movie/test
			// [0]804s/0804001.jpeg   --> c:/movie/test/804s/0804001.jpEg
			// [1]804s/jz0804002.jpg  --> c:/movie/test/804s/Jz0804002.jpg
			// [2]806s/jz0804002.jpg  --> c:/movie/test/806s/JZ0804002.jpg
			// [3]0804001.jpeg  --> c:/movie/test/0804001.Jpeg
			// [4]jz0804002.jpg  --> c:/movie/test/JZ0804002.jpg
			// rules:
			// 1) characters in key must lowercase. and in value keep original.
			// 2) key is value cut by root.

			VALIDATE(dir.find(root) == 0, null_str);
			std::string key = dir.substr(root.size());
			if (key.empty()) {
				key = name;
			} else {
				key = key.substr(1) + "/" + name;
			}
			files.insert(std::make_pair(utils::lowercase(key), dir + "/" + dirent->name));
		}
	}
	return true;
}

bool copy_root_files(const std::string& src, const std::string& dst, std::set<std::string>* files, const std::function<bool(const std::string& src)>& filter)
{
	if (files) {
		files->clear();
	}

	if (src.empty() || dst.empty()) {
		return false;
	}

	SDL_bool ret = SDL_TRUE;
	std::stringstream src_ss, dst_ss;
	SDL_DIR* dir = SDL_OpenDir(src.c_str());
	if (!dir) {
		return false;
	}
	SDL_dirent2* dirent;
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (!SDL_DIRENT_DIR(dirent->mode)) {
			src_ss.str("");
			dst_ss.str("");
			src_ss << src << '/' << dirent->name;
			dst_ss << dst << '/' << dirent->name;
			if (filter && !filter(src_ss.str())) {
				continue;
			}
			ret = SDL_CopyFiles(src_ss.str().c_str(), dst_ss.str().c_str());

			if (!ret) {
				break;
			}
			if (files) {
				files->insert(dirent->name);
			}
		}
	}
	SDL_CloseDir(dir);

	return ret? true: false;
}

class tcompare_dir_param
{
public:
	tcompare_dir_param(const std::string& dir1, const std::string& dir2, int& recursion_count)
		: current_path1_(dir1)
		, current_path2_(dir2)
		, recursion_count_(recursion_count)
	{
		const char c = current_path1_.at(current_path1_.size() - 1);
		if (c == '\\' || c == '/') {
			current_path1_.erase(current_path1_.size() - 1);
		}

		if (!dir2.empty()) {
			const char c = current_path2_.at(current_path2_.size() - 1);
			if (c == '\\' || c == '/') {
				current_path2_.erase(current_path2_.size() - 1);
			}
		}
	}

	bool cb_compare_dir_explorer(const std::string& dir, const SDL_dirent2* dirent);

private:
	std::string current_path1_;
	std::string current_path2_;
	int& recursion_count_;
};

bool tcompare_dir_param::cb_compare_dir_explorer(const std::string& dir, const SDL_dirent2* dirent)
{
	bool compair = !current_path2_.empty();
	recursion_count_ ++;
	std::string path2 = current_path2_;
	if (compair) {
		path2.append("/");
		path2.append(dirent->name);
	}
	
	// compair
	if (SDL_DIRENT_DIR(dirent->mode)) {
		if (compair) {
			if (!is_directory(path2.c_str())) {
				return false;
			}
		}

		tcompare_dir_param cdp2(current_path1_ + "/" + dirent->name, path2, recursion_count_);
		if (!walk_dir(cdp2.current_path1_, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp2, _1, _2))) {
			return false;
		}

	} else if (compair) {
		if (!file_exists(path2)) {
			return false;
		}

	}
	return true;
}

bool compare_directory(const std::string& dir1, const std::string& dir2)
{
	int recursion1_count = 0;
	tcompare_dir_param cdp1(dir1, dir2, recursion1_count);
	bool fok = walk_dir(dir1, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp1, _1, _2));
	if (!fok) {
		return false;
	}

	int recursion2_count = 0;
	tcompare_dir_param cdp2(dir2, "", recursion2_count);
	fok = walk_dir(dir2, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp2, _1, _2));

	if (recursion1_count != recursion2_count) {
		return false;
	}
	return true;
}

void path_summary(tpath_summary& summary, const char* name, bool first)
{
	SDL_DIR* dir;
	SDL_dirent2* dirent;
	char* full_name = NULL;
	int deep = 0;

	if (first) {
		memset(&summary, 0, sizeof(summary));
	}

	if (!name || !name[0]) {
		return;
	}
	if (SDL_IsFile(name)) {
		summary.files ++;
		summary.bytes += file_size(name);
		return;
	}

	dir = SDL_OpenDir(name);
	if (!dir) {
		return;
	}
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (!full_name) {
			full_name = (char*)SDL_malloc(1024);
		}
		sprintf(full_name, "%s/%s", name, dirent->name);
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
				path_summary(summary, full_name, false);
			}
		} else {
			// file
			summary.files ++;
			summary.bytes += dirent->size;
		}
	}
	SDL_CloseDir(dir);
	if (full_name) {
		SDL_free(full_name);
	}

	if (!first) {
		summary.dirs ++;
	}
}

scoped_istream& scoped_istream::operator=(std::istream *s)
{
	delete stream;
	stream = s;
	return *this;
}

scoped_istream::~scoped_istream()
{
	delete stream;
}

scoped_ostream& scoped_ostream::operator=(std::ostream *s)
{
	delete stream;
	stream = s;
	return *this;
}

scoped_ostream::~scoped_ostream()
{
	delete stream;
}

#ifdef _WIN32
/**
 * conv_ansi_utf8()
 *   - Convert a string between ANSI encoding (for Windows filename) and UTF-8
 *  string &name
 *     - filename to be converted
 *  bool a2u
 *     - if true, convert the string from ANSI to UTF-8.
 *     - if false, reverse. (convert it from UTF-8 to ANSI)
 */
void conv_ansi_utf8(std::string &name, bool a2u) 
{
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
								   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return;
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return;
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
							wc, wlen) == 0) {
		delete [] wc;
		return;
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
								   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete [] wc;
		return;
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete [] wc;
		return;
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
						ac, alen, NULL, NULL);
	delete [] wc;
	if (ac == NULL) {
		return;
	}
	name = ac;
	delete [] ac;

	return;
}

std::string conv_ansi_utf8_2(const std::string &name, bool a2u) 
{
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
								   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return "";
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return "";
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
							wc, wlen) == 0) {
		delete [] wc;
		return "";
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
								   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete [] wc;
		return "";
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete [] wc;
		return "";
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
						ac, alen, NULL, NULL);
	delete [] wc;
	if (ac == NULL) {
		return "";
	}
	std::string result = ac;
	delete [] ac;

	return result;
}

const char* utf8_2_ansi(const std::string& str)
{
	static std::string ret;
	ret = conv_ansi_utf8_2(str, false);
	return ret.c_str();
}

const char* ansi_2_utf8(const std::string& str)
{
	static std::string ret;
	ret = conv_ansi_utf8_2(str, true);
	return ret.c_str();
}

#else

void conv_ansi_utf8(std::string &name, bool a2u)
{
}

std::string conv_ansi_utf8_2(const std::string &name, bool a2u)
{
	return name;
}

const char* utf8_2_ansi(const std::string& str)
{
	static std::string ret = str;
	return ret.c_str();
}

const char* ansi_2_utf8(const std::string& str)
{
	static const std::string ret = str;
	return ret.c_str();
}

#endif



// replace src_str in src_file, and generate to dst_file.
bool file_replace_string(const std::string& src_file, const std::string& dst_file, const std::vector<std::pair<std::string, std::string> >& replaces)
{
	tfile file(src_file,  GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}
	fsize = file.replace_string(fsize, replaces, NULL);
	
	// write data to new file
	tfile file2(dst_file,  GENERIC_WRITE, CREATE_ALWAYS);
	if (file2.valid()) {
		posix_fseek(file2.fp, 0);
		posix_fwrite(file2.fp, file.data, fsize);
	}

	return true;
}

bool file_replace_string(const std::string& src_file, const std::pair<std::string, std::string>& replace)
{
	std::vector<std::pair<std::string, std::string> > replaces;
	replaces.push_back(replace);
	return file_replace_string(src_file, replaces);
}

// replace src_str in src_file, and generate to dst_file.
bool file_replace_string(const std::string& src_file, const std::vector<std::pair<std::string, std::string> >& replaces)
{
	tfile file(src_file,  GENERIC_WRITE, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}

	bool dirty = false;
	fsize = file.replace_string(fsize, replaces, &dirty);

	if (dirty) {
		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
		file.truncate(fsize);
	}
	return true;
}

//
// encrypt/decrypt
//
// if return isn't NULL, caller need free heap.
char* saes_encrypt_heap(const char* ptext, int size, unsigned char* key)
{
	if (size == 0) {
		return NULL;
	}
	char* ctext = (char*)malloc((size & 1)? size + 1: size);
	saes_encrypt_stream((const unsigned char*)ptext, size, key, ctext);
	return ctext;
}

// if return isn't NULL, caller need free heap.
char* saes_decrypt_heap(const char* ctext, int size, unsigned char* key)
{
	if (size == 0 || (size & 1)) {
		return NULL;
	}
	char* ptext = (char*)malloc(size);
	saes_decrypt_stream((const unsigned char*)ctext, size, key, ptext);
	return ptext;
}

tsaes_encrypt::tsaes_encrypt(const char* ctext, int s, unsigned char* key)
	: size(0)
{
	buf = saes_encrypt_heap(ctext, s, key);
	if (buf) {
		size = s & 1? s + 1: s;
	}
}

tsaes_decrypt::tsaes_decrypt(const char* ptext, int s, unsigned char* key)
	: size(s)
{
	buf = saes_decrypt_heap(ptext, size, key);
	if (buf && !buf[size - 1]) {
		size --;
	}
}

void convert_yuv_2_argb(const std::string& filename, const int width, const int height, int format, const std::string& out_png)
{
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (format == libyuv::FOURCC_YUY2) {
		VALIDATE(fsize == width * height * 2, null_str);
	} else if (format == libyuv::FOURCC_NV12 || format == libyuv::FOURCC_NV21) {
		VALIDATE(fsize == width * height * 3 / 2, null_str);
	}

	surface res = SDL_CreateRGBSurface(0, width, height, 4 * 8,
			0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	uint8_t* pixels = reinterpret_cast<uint8_t*>(res->pixels);

	if (format == libyuv::FOURCC_YUY2) {
		libyuv::YUY2ToARGB((uint8_t*)(file.data), width * 2,
               pixels, 4 * width,
               width, height);
	} else if (format == libyuv::FOURCC_NV12) {
		libyuv::NV12ToARGB((uint8_t*)(file.data), width,
               (uint8_t*)(file.data) + width * height, width,
               pixels, 4 * width,
               width, height);

	} else {
		VALIDATE(format == libyuv::FOURCC_NV21, null_str);

		libyuv::NV21ToARGB((uint8_t*)(file.data), width,
            (uint8_t*)(file.data) + width * height, width,
            pixels, 4 * width,
            width, height);
	}
	imwrite(res, out_png);
}

void SDL_SimplerMB(const char* fmt, ...)
{
	char text[2048];
	va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = SDL_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, game_config::get_app_msgstr(null_str).c_str(), text, nullptr);
}