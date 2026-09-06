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

#ifndef LIBROSE2_FILESYSTEM_DLL_HPP_INCLUDED
#define LIBROSE2_FILESYSTEM_DLL_HPP_INCLUDED

#include "rose_global.hpp"

#include <time.h>
#include <stdlib.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <set>
#include <map>


#include <SDL_filesystem.h>
#include "rose_util.hpp"
#include "rose_version.hpp"
#include "rose_string_utils.hpp"

extern LIB3RDPARTY_DECL const std::string backup_postfix;

LIB3RDPARTY_DECL void backup_did_file_write_simple(const std::string& file, int backup_type);
typedef void (*fbackup_did_file_write)(const std::string& file, int backup_type);
extern LIB3RDPARTY_DECL fbackup_did_file_write backup_did_file_write;

class LIB3RDPARTY_DECL tbakreader: public tfile
{
public:
	explicit tbakreader(const std::string& src_name, bool use_backup, const std::function<bool (tfile& file, int64_t dsize, bool bak)>& did_read);

	virtual int64_t read();

protected:
	const std::string src_name_;
	std::string bak_name_;
	bool use_backup_;
	const std::function<bool (tfile& file, int64_t dsize, bool bak)> did_read_;
};

class LIB3RDPARTY_DECL tsha1reader: public tbakreader
{
public:
	explicit tsha1reader(const std::string& src_name, bool use_backup, const std::function<bool (tfile& file, int64_t dsize, bool bak)>& did_read)
		: tbakreader(src_name, use_backup, did_read)
	{}

	int64_t verify_sha1() { return verify_sha1(*this); }
	int64_t verify_sha1(tfile& file);

	int64_t read() override;
};

enum {backup_on_fixed_delay, backup_on_idle, backup_type_count};

class LIB3RDPARTY_DECL tbakwriter
{
public:
	explicit tbakwriter(const std::string& src_name, int backup_type, const std::function<bool (tfile& file)>& did_write, const std::function<bool ()>& did_pre_write = NULL);

	virtual bool write();

protected:
	const std::string src_name_;
	std::string bak_name_;
	int backup_type_;
	const std::function<bool ()> did_pre_write_;
	const std::function<bool (tfile& file)> did_write_;
};

class LIB3RDPARTY_DECL tsha1writer: public tbakwriter
{
public:
	explicit tsha1writer(const std::string& src_name, int backup_type, const std::function<bool (tfile& file)>& did_write)
		: tbakwriter(src_name, backup_type, did_write)
	{}

	bool write() override;
};

// 2thpart, zip package's type. value must be in [1, 15]
enum {zipt_applet = 1, zipt_apk = 2, zipt_rosmap = 3, zipt_moveit = 4, zipt_pinyin = 5, zipt_khome = 6,
	zipt_material = 7, zipt_latex = 8, zipt_3rdparty13 = 13, zipt_3rdparty14 = 14};

#pragma pack(1)
#define RSP_MAXBUNDLEIDBYTES	23
#define RSP_MAXAPPLETIDBYTES	39 // <bundleid> + (development/studio), reference to tapplet::id()

struct trsp_header {
	uint32_t fourcc;
	uint32_t version;
	uint32_t build_date;
	uint32_t rose_version;
	char bundleid[RSP_MAXBUNDLEIDBYTES + 1];
	uint32_t manufactor;
	uint32_t zip_size;
};

#define RSP_HEADER_BYTES	sizeof(trsp_header)


// header of *.wav file
struct twave_pcm_hdr_44bytes {
	char            riff[4];                // = "RIFF"
	int				size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int				fmt_size;				// = 16

	short int       format_tag;             // = PCM : 1
	short int       channels;               // = channels : 1
	int				samples_per_sec;        // = samples : 8000 | 6000 | 11025 | 16000
	int				avg_bytes_per_sec;      // =  : samples_per_sec * bits_per_sample / 8
	short int       block_align;            // =  : wBitsPerSample / 8
	short int       bits_per_sample;        // = : 8 | 16

	char            data[4];                // = "data";
	int				data_size;              // =  : FileSize - 44 
};

#pragma pack()

// Basic disk I/O - read file.
// The bool relative_from_game_path determines whether relative paths should be treated as relative
// to the game path (true) or to the current directory from which Wesnoth was run (false).
LIB3RDPARTY_DECL std::string read_file(const std::string &fname);

LIB3RDPARTY_DECL void write_file(const std::string& fname, const void* data, int len);
LIB3RDPARTY_DECL int64_t file_size(const std::string& fname);
LIB3RDPARTY_DECL time_t file_3times(const std::string& fname, time_t* creation_time, time_t* last_access_time);

LIB3RDPARTY_DECL std::string get_saves_logs_path();

LIB3RDPARTY_DECL std::string get_aplt_user_data_dir(const std::string& lua_bundleid);
LIB3RDPARTY_DECL std::string get_aplt_prefs_file(const std::string& lua_bundleid);

LIB3RDPARTY_DECL std::string aplt_so_path(const std::string& res_path, const std::string& file);
#define get_libroseaplt_so_path(res_path)	aplt_so_path(res_path, LIBROSEAPLT_SO)

typedef std::function<bool (const std::string& dir, const SDL_dirent2* dirent)> twalk_dir_function;
// @rootdir: root directory desired walk
// @subfolders: walk sub-directory or not.
// remark:
//  1. You cannot delete use this function. when is directory, call RemoveDirectory always fail(errcode:32, other process using it)
//    must after FindClose. If delete, call SHFileOperation.
LIB3RDPARTY_DECL bool walk_dir(const std::string& rootdir, bool subfolders, const twalk_dir_function& fn);


//
// === *.rsp section ===
//
#pragma pack(1)

//
// zipt_rosmap
//
#define RSP_MAP_VER	1

#define RSP_MAXDESCBYTES	55
struct trsp_rosmap80bytes {
	char desc[RSP_MAXDESCBYTES + 1];
    int64_t ts;
	uint32_t map;
	uint32_t positions;
	uint32_t markers;
	uint32_t reserve0;
};

#define RSP_MAP_MAXPOSNAMEBYTES	20
struct trsp_rosmapposition {
	char uuid[36 + 1]; // 38a08106-e2c9-48ed-a19f-874f5690a7bd.
	char name[RSP_MAP_MAXPOSNAMEBYTES + 1];
	double x;
	double y;
	double theta;
};

enum {rspmapmarkertype_virtual, rspmapmarkertype_wall, rspmapmarkertype_count};

#define RSP_MIN_WALL_WIDTH	1.5
#define RSP_MAX_WALL_WIDTH	12.0
struct trsp_rosmapmarker {
	char uuid[36 + 1]; // 38a08106-e2c9-48ed-a19f-874f5690a7bd.
	int type;
	double x;
	double y;
	double width;
	double height;
	double altitude; // 3D: width(->length), height(->width), altitude(->height)
	double theta;
};

//
// zipt_moveit
//
#define RSP_MOVEIT_VER	1

//
// zipt_pinyin
//
#define RSP_PINYIN_VER_1	1

#define WAV_PCM_CODE        0x0001
struct twave_format_16bytes {
    uint16_t encoding;        // Actual encoding, possibly from the extensible header.
    uint16_t channels;        // Number of channels.
    uint32_t frequency;       // Sampling rate in Hz.
    uint32_t byterate;        // Average bytes per second.
    uint16_t blockalign;      // Bytes per block.
    uint16_t bitspersample;   // Currently supported are 8, 16, 24, 32, and 4 for ADPCM.
};

#define RSP_PINYIN_BYTES_PER_INDEX_CODE		2
struct trsp_pinyin92bytes {
	int64_t ts;
	twave_format_16bytes format;
	uint32_t pinyins;
	uint32_t reserve;
	uint32_t wav_start;
	char desc[RSP_MAXDESCBYTES + 1];
};

#define RSP_PINYIN_NO_INDEX_CODE	0xffff
#define RSP_PINYINS_PER_WORD		4
struct trsp_pinyinunicode {
	uint16_t idxs[RSP_PINYINS_PER_WORD]; // 
};

#define MAX_PINYIN_BYTES	7		// chuang2
struct trsp_pinyinindex {
	char pinyin[8]; // chuang2\0, must > MAX_PINYIN_BYTES.
	int offset; // 0 is wav-data
	int16_t size;
};

// RIFF file header. 12 bytes
struct triff_header {
    uint32_t riff;   // FOURCC of the chunk.
    uint32_t size;     // Size of the chunk data.
    uint32_t format_type; // Position of the data in the stream.
};

// Generic struct for the chunks in the WAVE file.
struct tfourcc_chunk_header {
    uint32_t fourcc;   // FOURCC of the chunk.
    uint32_t size;   // Size of the chunk data.
    uint8_t* data;     // When allocated, this points to the chunk data. length is used for the malloc size.
};

// save/load dcamera's depth data
struct tdepth_file_header {
	uint32_t fourcc;
	uint32_t width;
	uint32_t height;
	tdcintrinsics_C intrinsics;
	double depth_scale;
	double dcpitch;
};

//
// zipt_material
//
#define RSP_MATERIAL_VER	1

enum {rspmaterialtype_min = 1, rspmaterialtype_courseware = rspmaterialtype_min, rspmaterialtype_max = rspmaterialtype_courseware};

struct trsp_material113bytes {
	int64_t ts;
	int32_t type;
	char desc[RSP_MAXDESCBYTES + 1];
	char uuid[UUID_STR_LEN + 1];
	uint32_t reserve0;
	uint32_t reserve1;
	uint32_t reserve2;
};

//
// zipt_latex
//
#define RSP_LATEX_VER	1

enum {rsplatextype_min = 1, rsplatextype_miktex = rsplatextype_min, rsplatextype_max = rsplatextype_miktex};

struct trsp_latex80bytes {
	int64_t ts;
	int32_t type;
	char desc[RSP_MAXDESCBYTES + 1];
	uint32_t reserve0;
	uint32_t reserve1;
	uint32_t reserve2;
};

#pragma pack()

struct tmap_position
{
	tmap_position(const std::string& _uuid, const std::string& _name, double _x = float_nposm, double _y = float_nposm, double _theta = float_nposm)
		: uuid(_uuid)
		, name(_name)
		, x(_x)
		, y(_y)
		, theta(_theta)
	{
		// VALIDATE(utils::is_uuid(uuid, true), null_str);
		// VALIDATE(!name.empty(), null_str);
	}

	bool valid() const { return utils::is_uuid(uuid, true) && !name.empty() && name.size() <= RSP_MAP_MAXPOSNAMEBYTES; }
	// if doesn't restrict angle, theta is float_nposm.
	bool valid_xy() const { return !is_float_nposm(x) && !is_float_nposm(y); }

	std::string uuid;
	std::string name;
	double x;
	double y;
	double theta;
};

struct tmap_marker
{
	explicit tmap_marker(const trsp_rosmapmarker& rsp_marker)
		: rsp(rsp_marker)
	{
		// VALIDATE(utils::is_uuid(uuid, true), null_str);
		// VALIDATE(!name.empty(), null_str);
	}

	bool valid() const { return utils::is_uuid(rsp.uuid, true) && rsp.type >= 0 && rsp.type < rspmapmarkertype_count; }

	trsp_rosmapmarker rsp;
};

#endif
