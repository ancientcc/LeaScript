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

#include "rose_filesystem_dll.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_exception.hpp"

#ifdef _WIN32
#else // !_WIN32
#include <unistd.h>
// #include <dirent.h>
// #include <libgen.h>
// #ifndef ANDROID
// #include <sys/param.h> // statfs 
// #include <sys/mount.h> // statfs
// #else
// #include <sys/vfs.h> // statfs 
// #endif
#endif // !_WIN32

#include <SDL.h>

#include <openssl/sha.h>
#include <openssl/mem.h>

using namespace std::placeholders;


tfile::tfile(const std::string& file, uint32_t desired_access, uint32_t create_disposition)
	: fp(INVALID_FILE)
	, data(nullptr)
	, data_size(0)
	, truncate_size_(0)
	, can_truncate_(create_disposition == OPEN_EXISTING && (desired_access & GENERIC_WRITE))
{
	VALIDATE(create_disposition == OPEN_EXISTING || create_disposition == CREATE_ALWAYS, null_str);
	// CREATE_ALWAYS: create always. if file exist, it will truncate to 0 immediately.
	posix_fopen(file.c_str(), desired_access, create_disposition, fp);
}

tfile::tfile(posix_file_t _fp)
	: fp(_fp)
	, data(nullptr)
	, data_size(0)
	, truncate_size_(false)
{
	// hop don't use it.
	VALIDATE(fp != INVALID_FILE, null_str);
}

void tfile::close()
{
	if (data) {
		free(data);
		data = NULL;
		data_size = 0;
	}
	if (fp == INVALID_FILE) {
		return;
	}

	if (can_truncate_ && truncate_size_ > 0) {
		// when visual studio is using apps.sln, cannot delete it.
		// so must truncate apps.sln.
		// now SDL doesn't support truncate file. if I add, it necessary modify more.
		// in windows, use SetEndOfFile directly.
		// if linux-like, use ftruncate to truncate a file.
		int64_t fsize = posix_fsize(fp);
		if (fsize > truncate_size_) {
#ifdef _WIN32
			posix_fseek(fp, truncate_size_);
			SetEndOfFile(fp->hidden.windowsio.h);
#else
			// http://www.cnblogs.com/sky-heaven/p/4663630.html
			fflush(fp->hidden.stdio.fp);
			ftruncate(fileno(fp->hidden.stdio.fp), truncate_size_);
			rewind(fp->hidden.stdio.fp);
#endif
		}
	}

	posix_fclose(fp);
	fp = INVALID_FILE;
}

void tfile::resize_data(int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size) {
		char* tmp = (char*)malloc(size);
		if (data != nullptr) {
			if (vsize) {
				memcpy(tmp, data, vsize);
			}
			free(data);
		}
		data = tmp;
		data_size = size;
	}
}

int tfile::replace_span(const int start, int original_size, const char* new_data, int new_size, const int fsize)
{
	VALIDATE(start >= 0, null_str);

	int new_fsize = fsize + new_size - original_size;
	resize_data(new_fsize + 1, fsize); // extra 1 is for string terminate: '\0'.

	if (new_size <= original_size) {
		memcpy(data + start + new_size, data + start + original_size, fsize - start - original_size);
	} else {
		memmove(data + start + new_size, data + start + original_size, fsize - start - original_size);
	}
	if (new_data) {
		memcpy(data + start, new_data, new_size);
	}

	data[new_fsize] = '\0';
	return new_fsize; 
}

int64_t tfile::read_2_data(const int reserve_pre_bytes, const int reserve_post_bytes)
{
	VALIDATE(reserve_pre_bytes >= 0 && reserve_post_bytes >= 0, null_str);
	if (!valid()) {
		return 0;
	}
	int64_t fsize = posix_fsize(fp);
	if (!fsize) {
		return 0;
	}
	VALIDATE(fsize, null_str);

	posix_fseek(fp, 0);
	resize_data(reserve_pre_bytes + fsize + 1 + reserve_post_bytes);
	posix_fread(fp, data + reserve_pre_bytes, fsize);
	// let easy change to std::string.
	data[reserve_pre_bytes + fsize] = '\0';
	return fsize;
}

// caller require fill data before it.
// only modify data. doesn't touch file data.
int tfile::replace_string(int fsize, const std::vector<std::pair<std::string, std::string> >& replaces, bool* dirty)
{
	VALIDATE(data && data_size >= fsize, null_str);
	resize_data(fsize + 1, fsize);
	data[fsize] = '\0';

	const char* start2 = data; // search from 0.
	if (utils::bom_magic_started((const uint8_t*)data, fsize)) {
		start2 += BOM_LENGTH;
	}

	if (dirty) {
		*dirty = false;
	}
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
		const std::pair<std::string, std::string>& replace = *it;
		if (replace.first != replace.second) {
			const std::string& src = replace.first;
			const std::string& dst = replace.second;
			const char* ptr = strstr(start2, src.c_str());
			while (ptr) {
				// replace_span may remalloc data!
				int pos = ptr - data;
				fsize = replace_span(pos, src.size(), dst.c_str(), dst.size(), fsize);
				// replace next
				ptr = strstr(data + pos + dst.size(), src.c_str());
				if (dirty) {
					*dirty = true;
				}
			}
		}
	}

	return fsize;
}

const std::string backup_postfix = ".bak";

void backup_did_file_write_simple(const std::string& file, int backup_type)
{
	const std::string& bak_name = file + backup_postfix;
	SDL_CopyFiles(file.c_str(), bak_name.c_str());
}

fbackup_did_file_write backup_did_file_write = backup_did_file_write_simple;

tbakreader::tbakreader(const std::string& src_name, bool use_backup, const std::function<bool (tfile& file, int64_t dsize, bool bak)>& did_read)
	: tfile(src_name, GENERIC_READ, OPEN_EXISTING)
	, src_name_(src_name)
	, use_backup_(use_backup)
	, did_read_(did_read)
{
	if (use_backup) {
		bak_name_ = src_name_ + backup_postfix;
	}
}

int64_t tbakreader::read()
{
	int64_t fsize = 0;
	if (valid()) {
		fsize = posix_fsize(fp);
	}
	if (fsize > 0) {
		if (did_read_ == NULL) {
			int64_t fsize2 = read_2_data();
			if (fsize2 == fsize) {
				return fsize;
			}
		} else {
			posix_fseek(fp, 0);
			if (did_read_(*this, fsize, false)) {
				return fsize;
			}
		}
	}

	if (!use_backup_) {
		return 0;
	}

	VALIDATE(!bak_name_.empty(), null_str);
	tfile bak(bak_name_, GENERIC_READ, OPEN_EXISTING);
	fsize = 0;
	if (bak.valid()) {
		fsize = posix_fsize(bak.fp);
	}
	if (fsize > 0) {
		if (did_read_ == NULL) {
			int64_t fsize2 = bak.read_2_data();
			if (fsize2 == fsize) {
				return fsize;
			}
		} else {
			posix_fseek(bak.fp, 0);
			if (did_read_(bak, fsize, true)) {
				return fsize;
			}
		}
	}
	return 0;
}

int64_t tsha1reader::read()
{
	int64_t payload_size = verify_sha1(*this);
	if (payload_size >= 0) {
		posix_fseek(fp, 0);
		if (did_read_(*this, payload_size, false)) {
			return payload_size + SHA_DIGEST_LENGTH;
		}
	}

	if (!use_backup_) {
		return 0;
	}

	VALIDATE(!bak_name_.empty(), null_str);
	tfile bak(bak_name_, GENERIC_READ, OPEN_EXISTING);
	payload_size = verify_sha1(bak);
	if (payload_size >= 0) {
		posix_fseek(bak.fp, 0);
		if (did_read_(bak, payload_size, true)) {
			return payload_size + SHA_DIGEST_LENGTH;
		}
	}
	return 0;
}

int64_t tsha1reader::verify_sha1(tfile& file)
{
	if (!file.valid()) {
		return nposm;
	}
	posix_fseek(file.fp, 0);
	int64_t fsize = posix_fsize(file.fp);
	posix_fseek(file.fp, 0);
	if (fsize < SHA_DIGEST_LENGTH) {
		return nposm;
	}
	// allow dsize = 0.

	const int one_block = 2 * 1024 * 1024;
	file.resize_data(one_block);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	int64_t pos = 0;
	const int64_t end = fsize - SHA_DIGEST_LENGTH;
	while (pos < end) {
		int bytes = one_block;
		if (pos + bytes > end) {
			bytes = end - pos;
		}
		posix_fread(file.fp, file.data, bytes);
		SHA1_Update(&ctx, file.data, bytes);

		pos += bytes;
	}

	SHA1_Final(md, &ctx);
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	posix_fread(file.fp, file.data, SHA_DIGEST_LENGTH);
	posix_fseek(file.fp, 0);

	return memcmp(md, file.data, SHA_DIGEST_LENGTH) == 0? fsize - SHA_DIGEST_LENGTH: nposm;
}

tbakwriter::tbakwriter(const std::string& src_name, int backup_type, const std::function<bool (tfile& file)>& did_write, const std::function<bool ()>& did_pre_write)
	: src_name_(src_name)
	, backup_type_(backup_type)
	, did_pre_write_(did_pre_write)
	, did_write_(did_write)
{
	VALIDATE(backup_type == nposm || (backup_type >= 0 && backup_type < backup_type_count), null_str);
	VALIDATE(did_write_ != NULL, null_str);
	if (backup_type != nposm) {
		bak_name_ = src_name_ + backup_postfix;
	}
}

bool tbakwriter::write()
{
	bool continue_write = true;
	if (did_pre_write_ != NULL) {
		continue_write = did_pre_write_();
	}

	if (continue_write) {
		tfile file(src_name_, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);

		if (!did_write_(file)) {
			file.close();
			SDL_DeleteFiles(src_name_.c_str());
			return false;
		}
	} else {
		return true;
	}

	if (backup_type_ != nposm) {
		tfile file(src_name_, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = posix_fsize(file.fp);
		file.close();

		if (fsize > 0) {
			backup_did_file_write(src_name_, backup_type_);
		}
	}
	return true;
}

bool tsha1writer::write()
{
	{
		tfile file(src_name_, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);

		if (!did_write_(file)) {
			file.close();
			SDL_DeleteFiles(src_name_.c_str());
			return false;
		}

		uint8_t md[SHA_DIGEST_LENGTH];
		memset(md, 0, sizeof(md));
		SHA_CTX ctx;
		SHA1_Init(&ctx);

		posix_fseek(file.fp, 0);
		const int one_block = 2 * 1024 * 1024;
		int read_bytes = one_block;
		file.resize_data(one_block);
		while (read_bytes == one_block) {
			read_bytes = posix_fread(file.fp, file.data, one_block);
			if (read_bytes > 0) {
				SHA1_Update(&ctx, file.data, read_bytes);
			}
		}

		SHA1_Final(md, &ctx);
		OPENSSL_cleanse(&ctx, sizeof(ctx));
		posix_fwrite(file.fp, md, SHA_DIGEST_LENGTH);
	}

	if (backup_type_ != nposm) {
		bool require_copy = false;
		{
			// tsha1reader reader(src_name_, false, NULL);
			// require_copy = reader.verify_sha1() > 0;
		}
		require_copy = true;

		if (require_copy) {
			// make sure at more one file for writting.
			// SDL_CopyFiles(src_name_.c_str(), bak_name_.c_str());
			backup_did_file_write(src_name_, backup_type_);
		}
	}
	return true;
}

std::string read_file(const std::string &fname)
{
	tfile lock(fname, GENERIC_READ, OPEN_EXISTING);
	int32_t fsize = lock.read_2_data();
	if (!fsize) {
		return null_str;
	}
	std::string ret(lock.data, fsize);
	return ret;
}

void write_file(const std::string& fname, const void* data, int len)
{
	tfile file(fname, GENERIC_WRITE, CREATE_ALWAYS);
	if (!file.valid()) {
		return;
	}
	posix_fwrite(file.fp, data, len);
}

int64_t file_size(const std::string& file)
{
	VALIDATE(!file.empty(), null_str);

	int64_t fsize = 0;
	SDL_RWops* __h = SDL_RWFromFile(file.c_str(), "rb");
	if (__h != nullptr) {
		posix_fseek(__h, 0);
		fsize = posix_fsize(__h);
		posix_fclose(__h);
    }
	return fsize;
}

time_t file_3times(const std::string& fname, time_t* creation_time, time_t* last_access_time)
{
	if (creation_time != nullptr) {
		*creation_time = 0;
	}
	if (last_access_time != nullptr) {
		*last_access_time = 0;
	}

	SDL_dirent dirent;
	if (!SDL_GetStat(fname.c_str(), &dirent)) {
		return 0;
	}

	if (creation_time != nullptr) {
		*creation_time = dirent.ctime;
	}
	if (last_access_time != nullptr) {
		*last_access_time = dirent.atime;
	}
	return dirent.mtime;
}

std::string get_saves_logs_path()
{
	return game_config::preferences_dir + "/saves/logs";
}

std::string get_aplt_user_data_dir(const std::string& lua_bundleid)
{
	const std::string documents = "documents";
	return game_config::preferences_dir + "/" + utils::join_app_prefix_id(lua_bundleid, documents);
}

std::string get_aplt_prefs_file(const std::string& lua_bundleid)
{
	return get_aplt_user_data_dir(lua_bundleid) + "/preferences";
}

std::string aplt_so_path(const std::string& res_path, const std::string& file)
{
	std::stringstream ss;
	ss << res_path << "/libs/";
	if (game_config::os == os_windows) {
		ss << "windows/";
	} else if (game_config::os == os_android) {
		ss << "android/arm64-v8a/";
	} else {
		VALIDATE(game_config::os == os_ios, null_str);
		ss << "ios/arm64/";
	}
	ss << file;
	return ss.str();
}

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
