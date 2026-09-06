/* $Id: string_utils.cpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Copyright (C) 2005 - 2013 by Philippe Plantier <ayin@anathas.org>


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
 * Various string-routines.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "rose_global.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "chinese.hpp"
// #include "util.hpp"
// #include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "wml_exception.hpp"
#include "sound.hpp"
#include <SDL_mixer.h>

// #include <algorithm>
#include <iomanip>

#include "audio_capturer.hpp"
#include "base_instance.hpp"
#include "minizip/minizip.hpp"

using namespace std::placeholders;

namespace chinese {

// --- Internal tool: generate a lookup table with intelligent clipping. ---
void tamplify::generate_lut(int16_t* lut, float gain)
{
    for (int i = 0; i < LUT_SIZE; i++) {
        // 1. 还原成有符号原始值 (-32768 ~ 32767)
        int16_t raw = (int16_t)(i - 32768);
        float sample = (float)raw;
        
        // 2. 放大
        float amplified = sample * gain;

        // 3. 智能软裁切（防止破音的关键）
        // 如果放大后正好没溢出，直接存。
        // 如果放大后溢出（大于32767或小于-32768），使用平滑算法把超标部分"压"回来。
        if (amplified > 32767.0f) {
            float overflow = amplified - 32767.0f;
            // 超过部分用 0.85 倍的衰减硬拉回来，比简单粗暴的 if > 32767 听感好得多
            amplified = 32767.0f - (overflow * 0.85f); 
        } 
        else if (amplified < -32768.0f) {
            float overflow = -32768.0f - amplified;
            amplified = -32768.0f + (overflow * 0.85f);
        }

        // 4. 最终安全兜底，确保绝对不超过 16 位范围
        if (amplified > 32767.0f) {
			amplified = 32767.0f;
		}
        if (amplified < -32768.0f) {
			amplified = -32768.0f;
		}

        // 5. 写入查表
        lut[i] = (int16_t)amplified;
    }
}

int tamplify::audio_amp_init(void)
{
    // Allocate 4 blocks of 128 KB each (4 * 65536 * 2 bytes = 512 KB).
    lut_1x    = (int16_t*)malloc(LUT_SIZE * sizeof(int16_t));
    lut_1_25x = (int16_t*)malloc(LUT_SIZE * sizeof(int16_t));
    lut_1_6x  = (int16_t*)malloc(LUT_SIZE * sizeof(int16_t));
    lut_2x    = (int16_t*)malloc(LUT_SIZE * sizeof(int16_t));

    // Memory safety check.
    if (!lut_1x || !lut_1_25x || !lut_1_6x || !lut_2x) {
        VALIDATE(false, null_str);
        audio_amp_deinit();
        return -1;
    }

    // Generate the corresponding mapping table.
    generate_lut(lut_1x,    1.00f); // 无放大（保底）
    generate_lut(lut_1_25x, 1.25f); // 轻度放大
    generate_lut(lut_1_6x,  1.60f); // 标准远距
    generate_lut(lut_2x,    2.00f); // 极限户外

    // Default set to within-1-meter mode (1.0x).
    current_lut = lut_1x;
	curr_mode = aplt::ampmode_1x;

    return 0;
}

void tamplify::audio_amp_deinit(void)
{
    if (lut_1x != nullptr) { 
		free(lut_1x);    
		lut_1x = NULL; 
	}
    if (lut_1_25x != nullptr) {
		free(lut_1_25x);
		lut_1_25x = NULL;
	}
    if (lut_1_6x != nullptr) {
		free(lut_1_6x);
		lut_1_6x = NULL;
	}
    if (lut_2x != nullptr) {
		free(lut_2x);
		lut_2x = NULL;
	}
    current_lut = NULL;
}

void tamplify::audio_amp_set_mode(int mode)
{
	VALIDATE(mode >= 0 && mode < aplt::ampmode_count, null_str);
    switch (mode) {
	case aplt::ampmode_1x: 
		current_lut = lut_1x;
		break; // 1.0x
	case aplt::ampmode_1_25x:
		current_lut = lut_1_25x;
		break; // 1.25x
	case aplt::ampmode_1_6x:
		current_lut = lut_1_6x;
		break; // 1.6x
	case aplt::ampmode_2x:
		current_lut = lut_2x;
		break; // 2.0x
    default:
		current_lut = lut_1x;
		break;
    }
	curr_mode = mode;
}

// 核心播放循环处理函数
/*
void tamplify::audio_amp_process(int16_t* audio_data, int frames)
{
    // 极限优化：如果当前是 1.0 倍模式(即指向 lut_1x)，直接跳过查表！
    // 因为 1.0 倍时，audio_data[i] 查出来的值还是它自己，做了无用功。
    if (current_lut == lut_1x) {
        // 什么都不做，直接返回。音频原样播放，0 CPU 开销。
        return;
    }

    // 如果需要放大（1.25x, 1.6x, 2.0x），走查表通道
    for (int i = 0; i < frames; i++) {
        // 将带符号的 int16_t 强转为无符号 uint16_t，作为 0~65535 的下标
		if (audio_data[i] != 0) {
			int ii = 0;
		}

        // uint16_t index = (uint16_t)audio_data[i];
        // 直接从当前查表里拿放大/压缩后的值，覆盖原缓冲区
		uint16_t index = (uint16_t)(audio_data[i] + 32768); 

        audio_data[i] = current_lut[index];
    }


	float ratio = 1.2f;
	if (curr_mode == aplt::ampmode_1_25x) {
		ratio = 1.25f;

	} else if (curr_mode == ampmode_far_5m) {
		ratio = 1.6f;

	} else {
		VALIDATE(curr_mode == ampmode_outdoor, null_str);
		ratio = 2.0f;
	}

	for (int i = 0; i < frames; i++) {
        // 将带符号的 int16_t 强转为无符号 uint16_t，作为 0~65535 的下标

        float data = ratio * audio_data[i];
		if (data > 32767.0f) {
			data = 32767.0f;
		}
        if (data < -32768.0f) {
			data = -32768.0f;
		}
        // 直接从当前查表里拿放大/压缩后的值，覆盖原缓冲区
        audio_data[i] = (int16_t)(data);
    }
}
*/

void tamplify::audio_amp_process(int16_t* audio_data, int frames)
{
    // 1.0倍原声，直接跳过
    if (current_lut == lut_1x) {
        return;
    }

    // 核心查表循环（无浮点、无分支、绝对安全）
    for (int i = 0; i < frames; i++) {
        // 带符号 16 位数据 + 32768 映射到 0~65535 的合法索引
        uint16_t index = (uint16_t)(audio_data[i] + 32768); 
        audio_data[i] = current_lut[index];
    }
}

/*
void tamplify::audio_amp_process(int16_t* audio_data, int frames)
{
    // 极限优化：如果当前是 1.0 倍模式(即指向 lut_1x)，直接跳过查表！
    // 因为 1.0 倍时，audio_data[i] 查出来的值还是它自己，做了无用功。
    if (current_lut == lut_1x) {
        // 什么都不做，直接返回。音频原样播放，0 CPU 开销。
        return;
    }

    // 如果需要放大（1.25x, 1.6x, 2.0x），走查表通道
    for (int i = 0; i < frames; i++) {
        // 将带符号的 int16_t 强转为无符号 uint16_t，作为 0~65535 的下标
        uint16_t index = (uint16_t)audio_data[i];
        // 直接从当前查表里拿放大/压缩后的值，覆盖原缓冲区
        audio_data[i] = current_lut[index];
    }
}
*/
const int fix_bytes_in_code = 12;
const wchar_t min_unicode = 0x4E00;
const wchar_t max_unicode = 0x9FA5;

#define QUAN_YI4	0x4ebf // yi4
#define QUAN_WAN4	0x4e07 // wan4
#define QUAN_QIAN1	0x5343 // qian1
#define QUAN_BAI3	0x767e // bai3;
#define QUAN_SHI1	0x5341 // shi1

static std::unique_ptr<char[]> pinyin_code;

const char* get_pinyin_code()
{ 
	return pinyin_code.get(); 
}

int get_pinyin_code_bytes()
{
	return max_unicode - min_unicode + 1;
}

void create_chinesefrequence_raw()
{
	VALIDATE(game_config::os == os_windows, null_str);
	std::set<wchar_t> label_gb2312_1th;
	{
		// reference to http://ash.jp/code/cn/gb2312tbl.htm
		for (wchar_t wch = 0xb0a0; wch <= 0xd7f9; ) {
			if ((wch & 0xff) == 0xa0) {
				wch ++;
				continue;
			} else if ((wch & 0xff) == 0xff) {
				wch ++;
				wch += 0xa0;
				continue;
			}
			// uint8_t data[3] = {wch & 0xff, (wch & 0xff00) >> 8, '\0'};
			uint8_t data[3] = {(uint8_t)((wch & 0xff00) >> 8), (uint8_t)(wch & 0xff), '\0'};
			std::string str((char*)data, 2);
			str = ansi_2_utf8(str);

			utils::utf8_iterator it(str);
			bool first = false;
			for (; it != utils::utf8_iterator::end(str); ++ it) {
				VALIDATE(!first, null_str);
				label_gb2312_1th.insert(*it);
				first = true;
			}
			wch ++;
		}
	}

	std::set<wchar_t> label_gb2312_2th;
	{
		for (wchar_t wch = 0xd8a0; wch <= 0xf7fe; ) {
			if ((wch & 0xff) == 0xa0) {
				wch ++;
				continue;
			} else if ((wch & 0xff) == 0xff) {
				wch ++;
				wch += 0xa0;
				continue;
			}
			// uint8_t data[3] = {wch & 0xff, (wch & 0xff00) >> 8, '\0'};
			uint8_t data[3] = {(uint8_t)((wch & 0xff00) >> 8), (uint8_t)(wch & 0xff), '\0'};
			std::string str((char*)data, 2);
			str = ansi_2_utf8(str);

			utils::utf8_iterator it(str);
			bool first = false;
			for (; it != utils::utf8_iterator::end(str); ++ it) {
				VALIDATE(!first, null_str);
				VALIDATE(label_gb2312_1th.count(*it) == 0, null_str);
				label_gb2312_2th.insert(*it);

				first = true;
			}
			wch ++;
		}
		// master gb2312 has 3755 chinese. but maybe gb2312 exited but 3500 not, 
		// so label_gb2312_extra.size() maybe larger 3755 - 3500.
	}

	std::set<wchar_t> label_3500_extra;
	{
		const std::string imagenet_comp_path = game_config::path + "/data/core/cert/chinese-3500.txt";
		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch & 0x80) {
				int wch = posix_mku16(file.data[at], file.data[at + 1]);
				std::string str(file.data + at, 2);
				str = ansi_2_utf8(str);

				utils::utf8_iterator it(str);
				bool first = false;
				for (; it != utils::utf8_iterator::end(str); ++ it) {
					VALIDATE(!first, null_str);
					wchar_t ch = *it;
					if (ch != 0x3000) {
						// may be exist chinese-space.
						if (label_gb2312_1th.count(ch) == 0 && label_gb2312_2th.count(ch) == 0) {
							label_3500_extra.insert(ch);
						}
					}
					first = true;
				}
				at ++;
			}
		}
	}

	tfile file(game_config::path + "/data/core/cert/chinesefrequence.raw", GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);
	std::stringstream fp_ss;

	fp_ss << "# total count: " << label_gb2312_1th.size() << " + " << label_gb2312_2th.size() << " + " << label_3500_extra.size();
	fp_ss << " = " << (label_gb2312_1th.size() + label_gb2312_2th.size() + label_3500_extra.size());
	fp_ss << "\n\n";

	fp_ss << "#\n";
	fp_ss << "# gb2312[1th] chinese section. count: " << label_gb2312_1th.size() << "\n";
	fp_ss << "#\n";

	int at = 0;
	for (std::set<wchar_t>::const_iterator it = label_gb2312_1th.begin(); it != label_gb2312_1th.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << utils::UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}
	fp_ss << "\n\n";
	fp_ss << std::setbase(10);

	fp_ss << "#\n";
	fp_ss << "# gb2312[2th] chinese section. count: " << label_gb2312_2th.size() << "\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_gb2312_2th.begin(); it != label_gb2312_2th.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << utils::UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}
	fp_ss << "\n\n";
	fp_ss << std::setbase(10);

	fp_ss << "#\n";
	fp_ss << "# 3500 chinese section that not in gb2312. reference <Chinese curriculum standards (2011 Edition)>. count: " << label_3500_extra.size() << "\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_3500_extra.begin(); it != label_3500_extra.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << utils::UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}

	posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().size());
}

void chinesefrequence_text_2_code()
{
	VALIDATE(game_config::os == os_windows, null_str);
	const int bytes_in_code = sizeof(wchar_t);

	std::string filename = game_config::path + "/data/core/cert/chinesefrequence.txt";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize > 0, null_str);

	std::vector<std::string> vstr = utils::split(file.data, '\n');
	char* data = (char*)malloc(vstr.size() * bytes_in_code);
	memset(data, 0, vstr.size() * bytes_in_code);

	std::set<int> unicodes;
	std::pair<int, size_t> max_size(0, 0);
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		std::string line = *it;
		utils::strip(line);
		const int size = line.size();
		const char* c_str = line.c_str();
		if (size == 0 || c_str[0] == '#') {
			continue;
		}
		size_t pos = line.find('(');
		VALIDATE(pos != std::string::npos, null_str);
		VALIDATE(size == (int)pos + 1 + 6 + 1, null_str);
		VALIDATE(c_str[size - 1] == ')', null_str);
		
		int code = utils::to_int(line.substr(pos + 1, 6));
		VALIDATE(unicodes.count(code) == 0, null_str);
		unicodes.insert(code);

		VALIDATE(code >= 0 && code <= 65535, null_str);
		wchar_t code2 = code;
		VALIDATE(code2 >= min_unicode && code2 <= max_unicode, null_str);
		memcpy(data + at * bytes_in_code, &code2, bytes_in_code); 
		at ++;
	}
	VALIDATE(at == unicodes.size(), null_str);

	filename = game_config::path + "/data/core/cert/chinesefrequence.code";
	tfile file2(filename, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file2.fp, data, unicodes.size() * bytes_in_code);
}

typedef int (*ftext_2_code)(const std::vector<std::string>& pinyins, uint8_t* to_mem, uint64_t param);

void pinyin_text_2_code_internal(uint8_t* mem, int fix_code_bytes, const ftext_2_code text_2_code, uint64_t param)
{
	VALIDATE(game_config::os == os_windows, null_str);
	VALIDATE(fix_code_bytes > 0, null_str);
	VALIDATE(text_2_code != nullptr, null_str);

	std::string filename = game_config::path + "/data/core/cert/chinesepinyin.txt";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize > 0, null_str);

	std::vector<std::string> vstr = utils::split(file.data, '\n');
	VALIDATE((int)vstr.size() == get_pinyin_code_bytes(), null_str);

	std::pair<int, int> max_size(0, 0); // <at, max_worte_bytes>
	int at = 0;
	char err[128];
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it, at ++) {
		std::vector<std::string> vstr2 = utils::split(*it, ':');
		VALIDATE(vstr2.size() == 2, null_str);
		std::vector<std::string> pinyins = utils::split(vstr2[1], '|');
		for (std::vector<std::string>::const_iterator it2 = pinyins.begin(); it2 != pinyins.end(); ++ it2) {
			const std::string& pinyin = *it2;
			const char* c_str = pinyin.c_str();
			int s = pinyin.size();
			if (s > MAX_PINYIN_BYTES) {
				SDL_snprintf(err, sizeof(err), "(0x%04x) %s too long", min_unicode + at, pinyin.c_str());
				VALIDATE(false, err); 
			}
			int last_ch = c_str[s - 1];
			if (last_ch >= '0' && last_ch <= '9') {
				if (last_ch > '4') {
					SDL_snprintf(err, sizeof(err), "(0x%04x) %s's tone isn't in [0, 4]", min_unicode + at, pinyin.c_str());
					VALIDATE(false, err); 
				}
			}
		}
		
		int wrote_bytes = text_2_code(pinyins, mem + at * fix_code_bytes, param);
		if (wrote_bytes > max_size.second) {
			max_size = std::make_pair(min_unicode + at, wrote_bytes);
		}
		VALIDATE((int)max_size.second <= fix_code_bytes, null_str);
	}
}

int text_2_code_4sort(const std::vector<std::string>& pinyins, uint8_t* to_mem, uint64_t param)
{
	// only handle first pinyin
	const std::string& pinyin = pinyins[0];
	const char* c_str = pinyin.c_str();
	const int size = pinyin.size();
	for (int at2 = 0; at2 < size; at2 ++) {
		char ch = c_str[at2];
		if (at2 != size - 1) {
			VALIDATE(ch >= 'a' && ch <= 'z', null_str);
		} else {
			VALIDATE((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '4'), null_str);
		}
	}

	memcpy(to_mem, c_str, size);
	to_mem[size] = '\0';
	return size + 1; // + 1 is '\0'
}

void pinyin_text_2_code_4sort()
{
	VALIDATE(game_config::os == os_windows, null_str);
	const int fix_code_bytes_4sort = fix_bytes_in_code; // 8

	int pinyins = get_pinyin_code_bytes();
	uint8_t* mem = (uint8_t*)malloc(pinyins * fix_code_bytes_4sort);
	memset(mem, 0, pinyins * fix_code_bytes_4sort);

	pinyin_text_2_code_internal(mem, fix_code_bytes_4sort, text_2_code_4sort, 0);

	const std::string filename = game_config::path + "/data/core/cert/chinesepinyin.code";
	tfile file2(filename, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file2.fp, mem, pinyins * fix_code_bytes_4sort);

	free(mem);
}

int rsp_text_2_code(const std::vector<std::string>& pinyins, uint8_t* to_mem, uint64_t param)
{
	const int bytes_per_index_code = RSP_PINYIN_BYTES_PER_INDEX_CODE;
	const int pinyins_per_word = RSP_PINYINS_PER_WORD;
	const std::map<std::string, int>* indexs = reinterpret_cast<const std::map<std::string, int>* >(param);

	int to_at = 0;
	std::map<std::string, int>::const_iterator find_it;
	// std::set<std::string> candidates;
	for (std::vector<std::string>::const_iterator it = pinyins.begin(); to_at < pinyins_per_word && it != pinyins.end(); ++ it) {
		const std::string& pinyin = *it;
/*
		// 5 --> 1
		char pinyin2[16]; // MAX_PINYIN_BYTES + 1
		int s = pinyin.size();
		const char* c_str = pinyin.c_str();
		for (int at = 0; at < s; at ++) {
			char ch = c_str[at];
			if (ch != '5') {
				pinyin2[at] = c_str[at];
			} else {
				pinyin2[at] = '1';
			}
		}
		pinyin2[s] = '\0';
		if (candidates.count(pinyin2) != 0) {
			// git ride of: [me1 .. me5]
			continue;
		}
		candidates.insert(pinyin2);
*/
		find_it = indexs->find(pinyin);
		if (find_it != indexs->end()) {
			int16_t index = find_it->second;
			to_mem[to_at * bytes_per_index_code] = posix_lo8(index);
			to_mem[to_at * bytes_per_index_code + 1] = posix_hi8(index);
			to_at ++;
		}
	}
	return to_at * bytes_per_index_code;
}

void pinyin_text_2_code_4rsp(uint8_t* mem, int fix_code_bytes, const std::map<std::string, int>& indexs)
{
	pinyin_text_2_code_internal(mem, fix_code_bytes, rsp_text_2_code, reinterpret_cast<uint64_t>(&indexs));
}

void load_pinyin_code_4sort()
{
	std::string filename = game_config::path + "/data/core/cert/chinesepinyin.code";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize == (max_unicode - min_unicode + 1) * fix_bytes_in_code, null_str);

	pinyin_code.reset(new char[fsize]);
	memcpy(pinyin_code.get(), file.data, fsize);
}

const char* pinyin(wchar_t wch)
{
	if (wch < min_unicode || wch > max_unicode) {
		std::stringstream err;
		err << "0x" << std::setbase(16) << std::setfill('0') << std::setw(4) << wch << " is not chinese unicode.";
		VALIDATE(false, err.str());
		return nullptr;
	}
	return pinyin_code.get() + (wch - min_unicode) * fix_bytes_in_code;
}

bool key_matched(const std::string& src, const std::string& key)
{
	VALIDATE(!src.empty() && !key.empty(), null_str);
	
	utils::utf8_iterator key_ch(key);
	utils::utf8_iterator key_end = utils::utf8_iterator::end(key);
	wchar_t key_wch = *key_ch;
	if (key_wch >= 'A' && key_wch <= 'Z') {
		key_wch += 0x20;
	}

	bool at_least_one = false;
	bool require_calculate_key_ch = false;
	utils::utf8_iterator ch(src);
	for (utils::utf8_iterator end = utils::utf8_iterator::end(src); ch != end; ++ ch) {
		const wchar_t wch = *ch;
		while (true) {
			wchar_t wch2 = wch;
			if (wch2 >= 'A' && wch2 <= 'Z') {
				wch2 += 0x20;
			} else if (key_wch < 0x80 && chinese::is_chinese(wch2)) {
				wch2 = chinese::pinyin(wch2)[0];
			}

			require_calculate_key_ch = false;
			if (wch2 == key_wch) {
				++ key_ch;
				if (key_ch == key_end) {
					return true;
				}
				key_wch = *key_ch;
				if (key_wch >= 'A' && key_wch <= 'Z') {
					key_wch += 0x20;
				}
				at_least_one = true;

			} else if (at_least_one) {
				// if has one dis-match, reset compare.
				at_least_one = false;

				key_ch = utils::utf8_iterator::begin(key);
				key_wch = *key_ch;
				if (key_wch >= 'A' && key_wch <= 'Z') {
					key_wch += 0x20;
				}
				// previous match partial, this dismatch, compare last ch.
				continue;
			}
			break;
		}
	}
	return false;
}

std::string calculate_string_pinyin(const std::string& utf8str)
{
	std::stringstream ss;
	utils::utf8_iterator ch(utf8str);
	int at = 0;
	for (utils::utf8_iterator end = utils::utf8_iterator::end(utf8str); ch != end; ++ ch, at ++) {
		if (at > 0) {
			ss << " ";
		}
		wchar_t wch = *ch;
		if (chinese::is_chinese(wch)) {
			ss << chinese::pinyin(wch);
		} else {
			ss << std::string(ch.substr().first, ch.substr().second);
		}
	}
	return ss.str();
}


//
// pinyin section
//
#define SYMBOL_DASH			0x2014	// chinese (--)
#define SYMBOL_ELLIPSIS		0x2026	// chinese (.......)

tpinyin::tusing_id_lock::tusing_id_lock(tpinyin& pinyin, int id)
	: pinyin_(pinyin)
{
	VALIDATE(id != speakid_nposm, null_str);
	VALIDATE(pinyin_.using_id_ == speakid_nposm, null_str);
	pinyin_.using_id_ = id;
}

tpinyin::tusing_id_lock::~tusing_id_lock()
{
	VALIDATE(pinyin_.using_id_ != speakid_nposm, null_str);
	pinyin_.using_id_ = speakid_nposm;
}

tpinyin::tdisable_speak_lock::tdisable_speak_lock(tpinyin& pinyin)
	: pinyin_(pinyin)
{
	VALIDATE(!pinyin_.disable_speak_, null_str);
	pinyin_.disable_speak_ = true;
}

tpinyin::tdisable_speak_lock::~tdisable_speak_lock()
{
	VALIDATE(pinyin_.disable_speak_, null_str);
	pinyin_.disable_speak_ = false;
}

tpinyin::tpinyin()
	: text_all_loaded(false)
	, interval(330)
	, curr_itor(null_str)
	, play_data_(nullptr)
	, play_data_vsize_(0)
	, play_data_size_(0)
	, async_text_dirty_(0)
	, curr_id_(speakid_nposm)
	, async_id_(speakid_nposm)
	, using_id_(speakid_nposm)
	, disable_speak_(false)
	, pure_digits_(0)
{
	VALIDATE(RSP_PINYIN_CHINESE_UNICODES == get_pinyin_code_bytes(), null_str);

	reset();

	pair_symbols_.insert(SYMBOL_DASH);
	pair_symbols_.insert(SYMBOL_ELLIPSIS);

	standstill_.insert(std::make_pair(0x0021, 1000)); // english (!)
	standstill_.insert(std::make_pair(0x002c, 500)); // english (,)
	standstill_.insert(std::make_pair(0x002e, 500)); // english (.)
	standstill_.insert(std::make_pair(0x003a, 500)); // english (:)
	standstill_.insert(std::make_pair(0x003b, 750)); // english (;)
	standstill_.insert(std::make_pair(0x003f, 1000)); // english (?)

	standstill_.insert(std::make_pair(SYMBOL_DASH, 500));		// chinese (--)
	standstill_.insert(std::make_pair(SYMBOL_ELLIPSIS, 1000));	// chinese (.......)
	standstill_.insert(std::make_pair(0x3001, 400)); // chinese (\ a slight-pause mark)
	standstill_.insert(std::make_pair(0x3002, 1000)); // chinese (.)
	standstill_.insert(std::make_pair(0xff01, 1000)); // chinese (!)
	standstill_.insert(std::make_pair(0xff0c, 500)); // chinese (,)
	standstill_.insert(std::make_pair(0xff1a, 500)); // chinese (:)
	standstill_.insert(std::make_pair(0xff1b, 750)); // chinese (;)
	standstill_.insert(std::make_pair(0xff1f, 1000)); // chinese (?)

	digits_[0] = 0x96f6;
	digits_[1] = 0x4e00;
	digits_[2] = 0x4e8c;
	digits_[3] = 0x4e09;
	digits_[4] = 0x56db;
	digits_[5] = 0x4e94;
	digits_[6] = 0x516d;
	digits_[7] = 0x4e03;
	digits_[8] = 0x516b;
	digits_[9] = 0x4e5d;
}

tpinyin::~tpinyin()
{
	if (play_data_ != nullptr) {
		free(play_data_);
	}
}

void get_mix_data(uint8_t* stream, int len, void* context)
{
	tpinyin* device = reinterpret_cast<tpinyin*>(context);
	device->get_mix_data(stream, len);
}

bool dbg_50 = false;

void tpinyin::get_mix_data(uint8_t* stream, int len)
{
	VALIDATE(((uintptr_t)stream & 1) == 0 && (len & 1) == 0, null_str);

	threading::lock lock(play_data_mutex_);
	VALIDATE((play_data_vsize_ & 1) == 0, null_str);
	int copy_bytes = play_data_vsize_ <= len? play_data_vsize_: len;
	int rest = len - copy_bytes;
	if (dbg_50) {
		SDL_Log("%u {dbg-50}tpinyin::get_mix_data, len: %i, copy_bytes: %i, rest: %i", 
			SDL_GetTicks(), len, copy_bytes, rest);
	}
	if (copy_bytes > 0) {
		VALIDATE((copy_bytes & 1) == 0, null_str);
		memcpy(stream, play_data_, copy_bytes);
		// SDL_Log("%u {chinese}get_mix_data, play_data_vsize_ from %i to %i", SDL_GetTicks(), play_data_vsize_, play_data_vsize_ - copy_bytes);
		amp_.audio_amp_process((int16_t*)stream, copy_bytes / 2);
		play_data_vsize_ -= copy_bytes;
		if (play_data_vsize_ > 0) {
			memcpy(play_data_, play_data_ + copy_bytes, play_data_vsize_);
		}
	}
	if (rest > 0) {
		SDL_memset(stream + copy_bytes, 0, rest);
	}
}

void tpinyin::resize_play_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > play_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (play_data_ != nullptr) {
			if (play_data_vsize_) {
				memcpy(tmp, play_data_, play_data_vsize_);
			}
			free(play_data_);
		}
		play_data_ = tmp;
		play_data_size_ = size;
	}
}

uint32_t tpinyin::speak(const std::string& _text)
{
	VALIDATE(!disable_speak_, null_str);

	dbg_50 = false;
	if (_text.size() == 2) {
		int val = utils::to_int(_text);
		if (val > 10) {
			// dbg_50 = true;
			// SDL_Log("%u {dbg-50}tpinyin::speak, _text: %s", SDL_GetTicks(), _text.c_str());
		}
	}

	// if _text is empty, mean to stop speak.
	if (!rsp.valid()) {
		return speakid_nposm;
	}

	if (!IN_MAIN_THREAD()) {
		async_speak(_text);
		async_id_ = next_id();
		return async_id_;
	}

	if (curr_id_ != speakid_nposm) {
		VALIDATE(!text.empty(), null_str);
		const std::pair<std::string::const_iterator, std::string::const_iterator>& substr = curr_itor.substr();
		const int unplayed_start = std::distance(text.cbegin(), substr.first);
		did_speak_stopped(curr_id_, text, unplayed_start);
		curr_id_ = speakid_nposm;
	}

	reset();
	if (!_text.empty()) {
		text = _text;
		curr_itor = text; // utils::utf8_iterator itor(text);
		curr_id_ = using_id_ == speakid_nposm? next_id(): using_id_;
		hook_get_mix_data.reset(new sound::thook_get_mix_data(chinese::get_mix_data, this));

	} else if (!repeat_text.empty()) {
		// 2/2)Manually stop speaking, if there is a repeat-text, start
		return speak(repeat_text);

	}

	if (!text.empty()) {
		VALIDATE(curr_id_ != speakid_nposm, null_str);
	} else {
		VALIDATE(curr_id_ == speakid_nposm, null_str);
	}
	return curr_id_;
}

void tpinyin::repeat_speak(const std::string& _text)
{
	// if _text is empty, mean to stop repeat speak.
	if (!IN_MAIN_THREAD()) {
		async_speak(_text);
		async_repeat_text_ = _text;
		return;
	}
	if (!rsp.valid()) {
		return;
	}

	repeat_text = _text;
	speak(_text);
}

void tpinyin::did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start)
{
	tdisable_speak_lock lock(*this);
	if (instance != nullptr) {
		instance->pinyin_did_speak_stopped(id, text, unplayed_start);
	}
}

void tpinyin::async_speak(const std::string& _text)
{
	VALIDATE_NOT_MAIN_THREAD();
	threading::lock lock(async_text_mutex_);

	async_text_ = _text;
	async_text_dirty_ = true;
}

void parse_integer_to_chinese(int64_t integer, int& billion, int& ten_thousand, int& residue)
{
	billion = integer / 100000000;

	residue = integer - billion * 100000000;
	ten_thousand = residue / 10000;
	residue = residue - ten_thousand * 10000;
}

#define TEN_THOUSAND	10000

void parse_integer_to_chinese_le10000(int integer, int& thousand, int& hundred, int& ten, int& residue)
{
	VALIDATE(integer < TEN_THOUSAND, null_str);

	thousand = integer / 1000;
	residue = integer - thousand * 1000;

	hundred = residue / 100;
	residue = residue - hundred * 100;

	ten = residue / 10;

	residue = residue % 10;
}

static bool is_quantifier(wchar_t wch, int64_t integer, int& pure_digits)
{
	wchar_t quantifiers[] = {0x5e74 /*nian2*/, 0x6708 /*yue4*/, 0x65e5 /*ri4*/, 0x5929 /*tian1*/, 0x70b9 /*dian3*/, 0x65f6 /*shi2*/, 0x5206 /*fen1*/, 0x79d2 /*miao3*/,
		QUAN_YI4, QUAN_WAN4,
		0x4eba /*ren2*/,
		0x4e2a /*ge4*/,
		0x5ea6 /*du4*/,
		0x5f20 /*zhang1*/,
		0x6b21 /*ci4*/,
	};
	int quantifier_count = sizeof(quantifiers) / sizeof(quantifiers[0]);

	bool found = false;
	for (int at = 0; at < quantifier_count; at ++) {
		if (quantifiers[at] == wch) {
			if (wch == 0x5e74) {
				if (integer >= 1000 && integer <= 2040) {
					pure_digits = 4;
					// 2025 nian2, To read it digit by digit

				} else {
					found = true;
				}
			} else {
				found = true;
			}
			break;
		}
	}

	return found;
}

void tpinyin::play_le_ten_thousand(int integer)
{
	VALIDATE(integer < TEN_THOUSAND, null_str);

	int thousand;
	int hundred;
	int ten;
	int residue;

	parse_integer_to_chinese_le10000(integer, thousand, hundred, ten, residue);

	if (dbg_50) {
		SDL_Log("{dbg-50}tpinyin::play_le_ten_thousand, integer: %i -> thousand: %i, hundred: %i, ten: %i, residue: %i",
			integer, thousand, hundred, ten, residue);
	}

	// how to read '0'? https://www.zhihu.com/question/322790069
	if (thousand != 0) {
		VALIDATE(thousand >= 0 && thousand <= 9, null_str);
		play_one_unicode(digits_[thousand], nullptr);
		play_one_unicode(QUAN_QIAN1, nullptr);

	} else if (hundred != 0) {
		// play_one_unicode(digits_[0], nullptr);
	}

	if (hundred != 0) {
		VALIDATE(hundred >= 0 && hundred <= 9, null_str);
		play_one_unicode(digits_[hundred], nullptr);
		play_one_unicode(QUAN_BAI3, nullptr);

	} else if (thousand != 0 && ten != 0) {
		play_one_unicode(digits_[0], nullptr);
	}

	if (ten != 0) {
		VALIDATE(ten >= 0 && ten <= 9, null_str);
		if (ten != 1) {
			play_one_unicode(digits_[ten], nullptr);
		}
		play_one_unicode(QUAN_SHI1, nullptr);

	} else if ((thousand != 0 || hundred != 0) && residue != 0) {
		play_one_unicode(digits_[0], nullptr);
	}

	if (residue != 0) {
		VALIDATE(residue >= 0 && residue <= 9, null_str);
		play_one_unicode(digits_[residue], nullptr);
	}
}

void tpinyin::play_integer(int64_t integer)
{
/*
	if (dbg_50) {
		SDL_Log("{dbg-50}tpinyin::play_integer, integer: %i", (int)integer);
	}
*/
	if (integer == 0) {
		play_one_unicode(digits_[0], nullptr);
		return;
	}

	int billion;
	int ten_thousand;
	int residue;
	parse_integer_to_chinese(integer, billion, ten_thousand, residue);
/*
	if (dbg_50) {
		SDL_Log("{dbg-50}tpinyin::play_integer, integer: %i -> billion: %i, ten_thousand: %i, residue: %i",
			(int)integer, billion, ten_thousand, residue);
	}
*/
	if (billion != 0) {
		// can pay max: 9999(yi)
		play_le_ten_thousand(billion % TEN_THOUSAND);
		play_one_unicode(QUAN_YI4, nullptr);
	}

	if (ten_thousand != 0) {
		play_le_ten_thousand(ten_thousand);
		play_one_unicode(QUAN_WAN4, nullptr);
	}

	play_le_ten_thousand(residue);
}

// false: The following string isn't treated as a number. do not interrupt the current pump's for loop.
// true: The following string is treated as a number. goto next pump's for loop immedimate. same as use 'break' in for loop.
bool tpinyin::speak_when_first_digit(wchar_t wch)
{
	VALIDATE(wch >= '0' && wch <= '9', null_str);
	VALIDATE(pure_digits_ == 0, null_str);

	utils::utf8_iterator curr_itor2 = curr_itor;
	utils::utf8_iterator first_decimal_itor = curr_itor;
	int64_t integer = wch - '0';
	bool decimal_point = false;
	int decimal = 0; 
	wchar_t first_nondigit_wch = 0;
	const int max_integer_count = 12; // thou
	int integer_count = 1; // integer has (wch - '0')

	int decimal_count = 0;
	for (++ curr_itor2; curr_itor2 != utils::utf8_iterator::end(text); ++ curr_itor2) {
		wchar_t wch2 = *curr_itor2;
		if (wch2 >= '0' && wch2 <= '9') {
			if (!decimal_point) {
				if (integer_count == max_integer_count) {
					break;
				}
				integer = integer * 10 + (wch2 - '0');
				integer_count ++;

			} else {
				if (decimal_count == 0) {
					first_decimal_itor = curr_itor2;
				}
				decimal_count ++;
			}
		} else if (wch2 == '.') {
			if (!decimal_point) {
				decimal_point = true;

			} else {
				// Determine if the next one is a digit.
				// if it is still a digit, consider the previous string not a number.
				// example: 192.168.1.102
				utils::utf8_iterator curr_itor3 = curr_itor2;
				++ curr_itor3;
				if (curr_itor3 != utils::utf8_iterator::end(text)) {
					wchar_t wch3 = *curr_itor3;
					if (wch3 >= '0' && wch3 <= '9') {
						return false;
					}
				}
			}

		} else {
			first_nondigit_wch = wch2;
			break;
		}
	}

	// 1. Contains a decimal.
	// 2. 'integer' is the last part of this section of 'text'.
	// 3. 'integer' is followed by a quantifier. 
	const bool as_number = decimal_count > 0 || (first_nondigit_wch == 0 || is_quantifier(first_nondigit_wch, integer, pure_digits_));
	if (pure_digits_ != 0) {
		// it is trigger by is_quantifier(&pure_digits_). for example 2025(nian2)
		VALIDATE(decimal_count == 0, null_str);
		VALIDATE(!as_number, null_str);
		VALIDATE(pure_digits_ > 0, null_str);
		pure_digits_ --; // curr_itor is first digit, and this will consume it.
	}
	if (as_number) {
		play_integer(integer);
		if (decimal_count == 0) {
			curr_itor = curr_itor2;
							
		} else {
			wchar_t dian3 = 0x70b9; /*dian3*/
			play_one_unicode(dian3, nullptr);
			pure_digits_ = decimal_count;
			curr_itor = first_decimal_itor;
		}
		return true;
	}

	return false;
}

void tpinyin::pump()
{
	if (async_text_dirty_) {
		std::string new_text;
		int using_id = speakid_nposm;
		{
			threading::lock lock(async_text_mutex_);
			async_text_dirty_ = false;
			new_text = async_text_;
			async_text_.clear();

			repeat_text = async_repeat_text_;
			async_repeat_text_.clear();

			VALIDATE(async_id_ != speakid_nposm, null_str);
			using_id = async_id_;
			async_id_ = speakid_nposm;
		}
		tusing_id_lock lock(*this, using_id);
		speak(new_text);
	}

	if (text.empty()) {
		return;
	}


	const int low_threshold = 64 * 1024;
	if (play_data_vsize_ >= low_threshold) {
		return;
	}
	if (text_all_loaded) {
		VALIDATE(curr_itor == utils::utf8_iterator::end(text), null_str);
		if (play_data_vsize_ == 0) {
			{
				VALIDATE(curr_id_ != speakid_nposm, null_str);
				VALIDATE(!text.empty(), null_str);
				int unplayed_start = text.size();
				did_speak_stopped(curr_id_, text, unplayed_start);
				curr_id_ = speakid_nposm;
			}

			reset();

			if (!repeat_text.empty()) {
				// 1/2)Automatically stop speaking, if there is a repeat-text, start
				speak(repeat_text);
			}
		}
		return;
	}

	try {
		if (unicodes[IDX_LAST] == 0) {
			// first
			// curr_itor = text; // utils::utf8_iterator itor(text);
		} else {
			// second or later
			++ curr_itor;
		}

		text_all_loaded = true;
		for (; curr_itor != utils::utf8_iterator::end(text); ) {
			wchar_t wch = *curr_itor;
			unicodes[IDX_LAST] = unicodes[IDX_CURR];
			unicodes[IDX_CURR] = wch;

			bool is_digit = false;
			if (chinese::is_chinese(wch)) {
				if (play_one_unicode(wch, nullptr)) {
				}

			} else if (wch >= 'a' && wch <= 'z') {
				play_one_unicode(wch, nullptr);

			} else if (wch >= 'A' && wch <= 'Z') {
				play_one_unicode(wch - 'A' + 'a', nullptr);

			} else if (wch >= '0' && wch <= '9') {
				is_digit = true;
				if (pure_digits_ == 0) {
					if (speak_when_first_digit(wch)) {
						continue;
					}
				} else {
					VALIDATE(pure_digits_ > 0, null_str);
					pure_digits_ --;
				}
				
				play_one_unicode(digits_[wch - '0'], nullptr);

			} else if (standstill_.count(wch) != 0) {
				wchar_t key = 0;
				if (pair_symbols_.count(wch) == 0) {
					key = wch;

				} else if (unicodes[IDX_LAST] == wch) {
					key = wch;
				}
				if (key != 0) {
					int ms = standstill_.find(key)->second;
					play_standstill(ms);
				}
			}

			if (!is_digit) {
				VALIDATE(pure_digits_ == 0, null_str);
			}
			if (play_data_vsize_ >= low_threshold) {
				text_all_loaded = false;
				break;
			}
			++ curr_itor;
		}

	} catch (utils::invalid_utf8_exception&) {
		reset();
	}
}


#define ADJUST_VOLUME_S32B(s, v, min, max) {	\
	if (v >= 0) {	\
		int s1 = (s*v)/SDL_MIX_MAXVOLUME;		\
		if (s1 < (min)) {	\
			s1 = (min);		\
		} else if (s1 > (max)) {	\
			s1 = (max);		\
		}	\
		s = s1;	\
	} else {	\
		s = (s*SDL_MIX_MAXVOLUME)/(-1*v);		\
	}	\
}

bool tpinyin::play_one_unicode(wchar_t _unicode, const char* special)
{
	// if (dbg_50) {
	//	SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x", SDL_GetTicks(), _unicode);
	// }
	// bai2(0x767d), tian1(0x5929)
	wchar_t unicode = 0;
	if (is_chinese(_unicode)) {
		unicode = _unicode - chinese::min_unicode;
	} else {
		VALIDATE(_unicode >= 'a' && _unicode <= 'z', null_str);
		unicode = RSP_PINYIN_CHINESE_UNICODES + (_unicode - 'a');
	}
	
	const trsp_pinyinunicode& unicode_data = rsp.unicode_data[unicode];
	uint16_t idx_code = RSP_PINYIN_NO_INDEX_CODE;
	for (int at = 0; at < RSP_PINYINS_PER_WORD; at ++) {
		if (unicode_data.idxs[at] != RSP_PINYIN_NO_INDEX_CODE) {
			idx_code = unicode_data.idxs[at];
			break;
		}
	}
	if (idx_code == RSP_PINYIN_NO_INDEX_CODE) {
		return false;
	}
	const trsp_pinyinindex& index = rsp.index_data[idx_code];

	// int retsult_ms = nposm;
	char file_wav[64];
	SDL_snprintf(file_wav, sizeof(file_wav), "%s.wav", index.pinyin);
	const Mix_Chunk* cached_chunk = sound::fake_sound_cached(file_wav);
	if (dbg_50) {
		SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, file_wav: %s, cached_chunk: 0x%p", 
			SDL_GetTicks(), _unicode, file_wav, cached_chunk);
	}
	if (cached_chunk == nullptr) {
		tfile file(rspfile, GENERIC_READ, OPEN_EXISTING);
		if (!file.valid()) {
			if (dbg_50) {
				SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, result != chunk->alen, fail", SDL_GetTicks(), _unicode);
			}
			return false;
		}

		Mix_Chunk* chunk = (Mix_Chunk *)SDL_malloc(sizeof(Mix_Chunk));
		chunk->abuf = (uint8_t*)SDL_malloc(index.size);
		chunk->alen = index.size;
		memset(chunk->abuf, 0, chunk->alen);

		posix_fseek(file.fp, rsp.hdr.wav_start + index.offset);
		int result = posix_fread(file.fp, chunk->abuf, chunk->alen);
		if (result != chunk->alen) {
			if (dbg_50) {
				SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, result != chunk->alen, fail", SDL_GetTicks(), _unicode);
			}
			return false;
		}

		// write_file("c:/ddksample/1.dat", chunk->abuf, chunk->alen);

		// convert
		SDL_AudioSpec wavespec;
		SDL_AudioSpec mixer;
		SDL_AudioCVT wavecvt;

		memset(&wavespec, 0, sizeof(wavespec));
		wavespec.freq = rsp.hdr.format.frequency;
		wavespec.channels = rsp.hdr.format.channels;
		wavespec.samples = 4096;
		wavespec.format = AUDIO_S16LSB;

		// int numtimesopened, frequency, channels;
		int channels;
		// Uint16 format;
		memset(&mixer, 0, sizeof(mixer));
		int numtimesopened = Mix_QuerySpec(&mixer.freq, &mixer.format, &channels);
		VALIDATE(numtimesopened > 0, null_str);

		// mixer.freq = curr_pinyin.hdr.format.frequency;
		mixer.channels = channels;
		mixer.samples = 4096;
		// mixer.format = AUDIO_S16LSB;
		mixer.size = mixer.samples * channels * 2; // 16384, 2 is 16bit.

		// copy from <SDL2_mixer>/mixer.c's Mix_LoadWAV_RW
		int samplesize;
		// Build the audio converter and create conversion buffers
		if (wavespec.format != mixer.format ||
			 wavespec.channels != mixer.channels ||
			 wavespec.freq != mixer.freq) {
			if (SDL_BuildAudioCVT(&wavecvt,
					wavespec.format, wavespec.channels, wavespec.freq,
					mixer.format, mixer.channels, mixer.freq) < 0) {
				SDL_free(chunk->abuf);
				SDL_free(chunk);
				if (dbg_50) {
					SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, SDL_BuildAudioCVT, fail", SDL_GetTicks(), _unicode);
				}
				return false;
			}
			samplesize = ((wavespec.format & 0xFF)/8)*wavespec.channels;
			wavecvt.len = chunk->alen & ~(samplesize-1);
			wavecvt.buf = (Uint8 *)SDL_calloc(1, wavecvt.len*wavecvt.len_mult);
			if (wavecvt.buf == NULL) {
				SDL_SetError("Out of memory");
				SDL_free(chunk->abuf);
				SDL_free(chunk);
				if (dbg_50) {
					SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, wavecvt.buf == NULL, fail", SDL_GetTicks(), _unicode);
				}
				return false;
			}
			SDL_memcpy(wavecvt.buf, chunk->abuf, wavecvt.len);
			SDL_free(chunk->abuf);

			/* Run the audio converter */
			if (SDL_ConvertAudio(&wavecvt) < 0) {
				SDL_free(wavecvt.buf);
				SDL_free(chunk);
				if (dbg_50) {
					SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, SDL_ConvertAudio, fail", SDL_GetTicks(), _unicode);
				}
				return false;
			}

			chunk->abuf = wavecvt.buf;
			chunk->alen = wavecvt.len_cvt;
		}

		chunk->allocated = 1;
		chunk->volume = MIX_MAX_VOLUME;

		if (chunk->alen != wavecvt.len*wavecvt.len_mult) {
			VALIDATE((int)chunk->alen < wavecvt.len*wavecvt.len_mult, null_str);
			chunk->abuf = (Uint8 *)SDL_malloc(chunk->alen);
			memcpy(chunk->abuf, wavecvt.buf, chunk->alen);
			SDL_free(wavecvt.buf);
		}

		// retsult_ms = 1000 * chunk->alen / (mixer.freq * mixer.channels * 2);
		sound::play_chunk(file_wav, chunk);

	} else {
		sound::play_chunk(file_wav, nullptr);
	}

	cached_chunk = sound::fake_sound_cached(file_wav);
	if (cached_chunk != nullptr) {
		uint8_t* abuf = cached_chunk->abuf;
		int alen = cached_chunk->alen;

		uint8_t* new_abuf = nullptr;
		// if (_unicode == unicodes[IDX_LAST]) {
		if (false) {
			int v = SDL_MIX_MAXVOLUME * 4 / 5;
			// int v = 64;
			int channels = 2;
			int block_align = channels * 2;
			alen = posix_align_floor(alen, block_align);
			new_abuf = (uint8_t*)malloc(alen);
			int src_index = 0;
			// for (int at = 0; at < alen; at += block_align * 2) {
			// for (int at = 8820; at < alen; at += block_align) {
			for (int at = 0; at < alen; at += block_align) {
				int16_t val = posix_mki16(abuf[at], abuf[at + 1]);
				ADJUST_VOLUME_S32B(val, v, -32767, 32767);
				int16_t val1 = posix_mki16(abuf[at + 2], abuf[at + 3]);
				ADJUST_VOLUME_S32B(val1, v, -32767, 32767);

				// int src_start = at / block_align;

				// play_data_[play_data_vsize_ + at] = abuf[at];
				// play_data_[play_data_vsize_ + at + 1] = abuf[at + 1];
				// play_data_[play_data_vsize_ + at + 2] = abuf[at + 2];
				// play_data_[play_data_vsize_ + at + 3] = abuf[at + 3];
				new_abuf[src_index] = posix_lo8(val);
				new_abuf[src_index + 1] = posix_hi8(val);
				new_abuf[src_index + 2] = posix_lo8(val1);
				new_abuf[src_index + 3] = posix_hi8(val1);
				src_index += 4;
			}
			if (dbg_50) {
				SDL_Log("%u {dbg-50}tpinyin::play_one_unicode, _unicode: 0x%04x, _unicode == unicodes[IDX_LAST], src_index: %i, alen: %i", 
					SDL_GetTicks(), _unicode, src_index, alen);
			}
			abuf = new_abuf;
			alen = src_index;

		}

		threading::lock lock(play_data_mutex_);
		resize_play_data(play_data_vsize_ + alen);
		memcpy(play_data_ + play_data_vsize_, abuf, alen);
		// SDL_Log("%u {chinese}play_one_unicode, play_data_vsize_ from %i to %i", SDL_GetTicks(), play_data_vsize_, play_data_vsize_ + alen);
		if (dbg_50) {
			SDL_Log("%u {dbg-50}play_one_unicode, play_data_vsize_ from %i to %i", SDL_GetTicks(), play_data_vsize_, play_data_vsize_ + alen);
		}
		play_data_vsize_ += alen;

		if (new_abuf != nullptr) {
			free(new_abuf);
		}

	}

	return true;
}

void tpinyin::play_standstill(int ms)
{
	VALIDATE(ms > 0 && ms <= 1000, null_str);

	int channels;
	int freq;
	Uint16 format;
	int numtimesopened = Mix_QuerySpec(&freq, &format, &channels);
	if (numtimesopened > 0) {
		int bytes = ms * freq * channels * 2 / 1000;

		resize_play_data(play_data_vsize_ + bytes);
		memset(play_data_ + play_data_vsize_, 0, bytes);
		// SDL_Log("%u {chinese}play_standstill(ms: %i -> %i), play_data_vsize_ from %i to %i", 
		//	SDL_GetTicks(), ms, bytes, play_data_vsize_, play_data_vsize_ + bytes);
		play_data_vsize_ += bytes;
	}
}

std::string tpinyin::from_unicodes(const wchar_t* unicodes, int count, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match)
{  
	VALIDATE(for_match, null_str);

	if (pos_data != nullptr) {
		memset(pos_data, 0, sizeof(tint32data_C));
	}

	if (!rsp.valid()) {
		return null_str;
	}

	VALIDATE(unicodes != nullptr && count > 0, null_str);
	VALIDATE(tone == nposm || (tone >= 0 && tone <= 4), null_str);

	// tint32data* pos_data = (tint32data*)malloc(sizeof(tint32data));
	if (pos_data != nullptr) {
		pos_data->ptr = (int*)malloc(count * sizeof(int));
		pos_data->len = count;
	}

	char* result = (char*)malloc((count + 1) * MAX_PINYIN_BYTES);
	int pos = 0;
	for (int at = 0; at < count; at ++) {
		const int this_start_pos = pos;
		const wchar_t wch = unicodes[at];
		if (at != 0 && pos_data != nullptr) {
			pos_data->ptr[at - 1] = pos;
		}
		int unicode_norm = nposm;
		if (chinese::is_chinese(wch)) {
			unicode_norm = wch - chinese::min_unicode;

		} else if (wch >= '0' && wch <= '9') {
			unicode_norm = digits_[wch - '0'] - chinese::min_unicode;

		} else if ((wch >= 'a' && wch <= 'z') || (wch >= 'A' && wch <= 'Z')) {
			// for english alpha, in ordr to distingute chinese-pinyin, default output uppercase.
			char ch = wch >= 'A' && wch <= 'Z'? wch: wch - 'a' + 'A';
			if (eng_lowercase) {
				ch = ch - 'A' + 'a';
			}
			result[pos ++] = ch;
			// english alpha has no tone.
			continue;
		}

		if (unicode_norm == nposm) {
			// ??require validate
			continue;
		}

		const trsp_pinyinunicode& unicode_data = rsp.unicode_data[unicode_norm];
		uint16_t idx_code = RSP_PINYIN_NO_INDEX_CODE;
		for (int at = 0; at < RSP_PINYINS_PER_WORD; at ++) {
			if (unicode_data.idxs[at] != RSP_PINYIN_NO_INDEX_CODE) {
				idx_code = unicode_data.idxs[at];
				break;
			}
		}
		if (idx_code == RSP_PINYIN_NO_INDEX_CODE) {
			// ??require validate
			continue;
		}
		const trsp_pinyinindex& index = rsp.index_data[idx_code];
		int size = sizeof(index.pinyin);
		for (int at2 = 0; at2 < size; at2 ++) {
			char ch = index.pinyin[at2];
			if (ch >= 'a' && ch <= 'z') {
				result[pos] = ch;
			} else if (ch >= '0' && ch <= '9') {
				char val = tone != nposm? tone + '0': ch;
				result[pos] = val;
			} else {
				if (game_config::os == os_windows) {
					// must be '\0'
					VALIDATE(ch == '\0', null_str);
				}
				continue;
			}
			pos ++;
		}
		if (chinese::is_chinese(wch) && for_match) {
			int s = pos - this_start_pos;
			if (s >= 4) { // chi0
				char* c_str = result + this_start_pos;
				// zh, ch, sh -> z, c, s
				if (c_str[1] == 'h' && (c_str[0] == 'z' || c_str[0] == 'c' || c_str[0] == 's')) {
					memmove(c_str + 1, c_str + 2, s - 2);
					pos --;
					s --;
				}
			}
			if (s >= 4) { // ang0
				// ang, eng, ing -> an, en, in
				char* c_str = result + (pos - 4);
				if (c_str[1] == 'n' && c_str[2] == 'g' && 
					(c_str[0] == 'a' || c_str[0] == 'e' || c_str[0] == 'i')) {
					c_str[2] = c_str[3];
					pos --;
					s --;
				}
			}
			if (s >= 5) { // jian0
				// lian, mian -> lie, mie
				char* c_str = result + (pos - 4);
				if (c_str[0] == 'i' && c_str[1] == 'a' && c_str[2] == 'n') { 
					c_str[1] = 'e';
					c_str[2] = c_str[3];
					pos --;
					s --;
				}
			}
			if (s >= 4) { // jin3
				// jin3 -> jie3(jian3)
				// chang2 (jing3) is similar as chan3 (jian3).
				char* c_str = result + (pos - 3);
				if (c_str[0] == 'i' && c_str[1] == 'n') { 
					c_str[1] = 'e';
				}
			}
		}
	}
	if (pos_data != nullptr) {
		pos_data->ptr[count - 1] = pos;
	}
	result[pos] = '\0';
	std::string res = result;
	free(result);

	return res;
}

std::string tpinyin::from_utf8str(const std::string& str, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match)
{
	if (pos_data != nullptr) {
		memset(pos_data, 0, sizeof(tint32data_C));
	}

	const int max_support_bytes = 16384;   // 4 * 4K(chars)
	const int size = str.size();
	if (size > max_support_bytes) {
		SDL_Log("tpinyin::from_utf8str, %i bytes is > max_support_bytes(%i)", size, max_support_bytes);
		return null_str;
	} else if (size == 0) {
		return null_str;
	}
	int chars = 0;
	wchar_t* unicodes = (wchar_t*)malloc(size * sizeof(wchar_t));

	try {
		utils::utf8_iterator curr_itor = str;
		utils::utf8_iterator end_itor = utils::utf8_iterator::end(str);
		for (; curr_itor != end_itor; ++ curr_itor) {
			wchar_t wch = *curr_itor;
			unicodes[chars ++] = wch;
		}
	} catch (utils::invalid_utf8_exception&) {
		chars = 0;
	}

	std::string result;
	if (chars > 0) {
		result = from_unicodes(unicodes, chars, tone, eng_lowercase, pos_data, for_match);
	}

	free(unicodes);
	return result;
}

void tpinyin::set_amp_mode(int mode)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(mode != amp_.audio_amp_get_mode(), null_str);

	threading::lock lock(play_data_mutex_);
	amp_.audio_amp_set_mode(mode);
}

tpinyin curr_pinyin;

bool did_write_rsp_pinyin(tfile& file, const std::string& bundleid, const version_info& rose_version, const std::map<std::string, std::string>& wavs, 
	std::vector<std::string>& err_open, std::vector<std::string>& err_size, std::vector<std::string>& err_format)
{
	curr_pinyin.reset();
	VALIDATE(!wavs.empty(), null_str);

	const int64_t ts = time(nullptr);
	// part(1/5): rsp header
	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_pinyin));
	header.version = SDL_FOURCC(0, 0, 0, RSP_PINYIN_VER_1);

	// const time_t t = ts; // for xcode(ios)
	// tm* timeptr = localtime(&t);
	// VALIDATE(timeptr != nullptr, null_str);
	// int build_date = (1900 + timeptr->tm_year) * 10000 + (timeptr->tm_mon + 1) * 100 + timeptr->tm_mday;
	header.build_date = ts_2_build_date(ts);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.zip_size = sizeof(trsp_pinyin92bytes); // will overwrite later.
	posix_fwrite(file.fp, &header, sizeof(header));

	// part(2/5): pinyin header
	const int pinyinhdr_start = sizeof(trsp_header);
	const int pinyinhdr_bytes = sizeof(trsp_pinyin92bytes);
	const int bytes_per_index_code = RSP_PINYIN_BYTES_PER_INDEX_CODE;
	const int bytes_per_index = sizeof(trsp_pinyinindex);

	// part(3/5): unicode -> pinyin index
	const int unicode_start = pinyinhdr_start + pinyinhdr_bytes;
	// const int unicode_bytes = chinese::get_pinyin_code_bytes() * bytes_per_index_code * RSP_PINYINS_PER_WORD;
	const int unicode_bytes = RSP_PINYIN_UNICODES * bytes_per_index_code * RSP_PINYINS_PER_WORD;

	// part(4/5): pinyin index -> address in wav data
	const int index_start = unicode_start + unicode_bytes;
	const int index_bytes = wavs.size() * bytes_per_index;

	const int wav_start = index_start + index_bytes;
	const int bytes_2_4 = wav_start - pinyinhdr_start;
	VALIDATE(bytes_2_4 == pinyinhdr_bytes + unicode_bytes + index_bytes, null_str);
	const int bytes_2_4_align = posix_align_ceil(bytes_2_4, 4096);

	const int data2_size = bytes_2_4_align + MAX_WAV_SIZE;
	SDL_Log("(write)bytes_2_4: %i{#2(%i) + #3(%i) + #4(%i)} data2_size: %i{bytes_2_4_align: %i + MAX_WAV_SIZE: %i}", 
		bytes_2_4, pinyinhdr_bytes, unicode_bytes, index_bytes, data2_size, bytes_2_4_align, MAX_WAV_SIZE);
	uint8_t* data2 = (uint8_t*)malloc(data2_size);
	memset(data2, 0, data2_size);
	uint8_t* pinyinhdr_data = data2;
	uint8_t* unicode_data = pinyinhdr_data + pinyinhdr_bytes;
	uint8_t* index_data = unicode_data + unicode_bytes;
	uint8_t* wav_data = data2 + bytes_2_4_align;

	// Enlarge the file, take a place
	posix_fwrite(file.fp, data2, bytes_2_4);

	// initial part(2/5): pinyin header
	trsp_pinyin92bytes& pinyin_header = *((trsp_pinyin92bytes*)pinyinhdr_data);
	// memset(&pinyin_header, 0, sizeof(pinyin_header));
	twave_format_16bytes& hdr_format = pinyin_header.format;
	hdr_format.encoding = WAV_PCM_CODE;
	hdr_format.channels = 1;
	hdr_format.frequency = 44100;
	hdr_format.bitspersample = 16;
	hdr_format.blockalign = hdr_format.channels * (hdr_format.bitspersample / 8);
	hdr_format.byterate = hdr_format.frequency * hdr_format.blockalign;

	pinyin_header.ts = ts;
	pinyin_header.pinyins = 0; // ??
	pinyin_header.wav_start = wav_start;

	// part(5/5): wav data
	std::map<std::string, int> indexs;
	int offset = 0;
	for (std::map<std::string, std::string>::const_iterator it = wavs.begin(); it != wavs.end(); ++ it) {
		const std::string& pinyin = it->first;
		const std::string& fname = it->second;
		// open it

		tfile src(fname, GENERIC_READ, OPEN_EXISTING);
		if (!src.valid()) {
			err_open.push_back(fname);
			continue;
		}
		int bytes_ret = posix_fread(src.fp, wav_data, MAX_WAV_SIZE);
		if (bytes_ret < MIN_WAV_SIZE) {
			err_size.push_back(fname);
			continue;
		}
		const triff_header* riff_header = (const triff_header*)wav_data;
		if (riff_header->riff != SDL_FOURCC('R', 'I', 'F', 'F') || riff_header->format_type != SDL_FOURCC('W', 'A', 'V', 'E')) {
			err_format.push_back(fname);
			continue;
		}
		if (8 + riff_header->size != bytes_ret) {
			err_format.push_back(fname);
			continue;
		}
		int pos = sizeof(triff_header);
		const tfourcc_chunk_header* chunk_header = (const tfourcc_chunk_header*)(wav_data + pos);
		if (chunk_header->fourcc != SDL_FOURCC('f', 'm', 't', ' ') || chunk_header->size != sizeof(twave_format_16bytes)) {
			err_format.push_back(fname);
			continue;
		}

		VALIDATE(sizeof(twave_format_16bytes) == 0x10, null_str);
		pos += 8;
		const twave_format_16bytes* format = (const twave_format_16bytes*)(wav_data + pos);
		if (memcmp(&hdr_format, format, sizeof(twave_format_16bytes)) != 0) {
			err_format.push_back(fname);
			continue;
		}

		pos += sizeof(twave_format_16bytes);
		chunk_header = (const tfourcc_chunk_header*)(wav_data + pos);
		if (chunk_header->fourcc != SDL_FOURCC('d', 'a', 't', 'a')) {
			err_format.push_back(fname);
			continue;
		}
		pos += 8;
		if (chunk_header->size + pos != bytes_ret) {
			err_format.push_back(fname);
			continue;
		}
		posix_fwrite(file.fp, wav_data + pos, chunk_header->size);

		trsp_pinyinindex* index = (trsp_pinyinindex*)(index_data + sizeof(trsp_pinyinindex) * pinyin_header.pinyins);
		int size = pinyin.size() - 4; // 4 is .wav
		VALIDATE(size <= MAX_PINYIN_BYTES, null_str);
		memcpy(index->pinyin, pinyin.c_str(), size);
		index->offset = offset;
		index->size = chunk_header->size;
		SDL_Log("index#%i pinyin: %s offset: %i size: %i", pinyin_header.pinyins, index->pinyin, index->offset, index->size);
		indexs.insert(std::make_pair(index->pinyin, pinyin_header.pinyins));

		pinyin_header.pinyins ++;
		offset += chunk_header->size;
	}

	posix_fseek(file.fp, 0);
	// overwrite rspheader
	header.zip_size = bytes_2_4 + offset;
	posix_fwrite(file.fp, &header, sizeof(header));

	// fill part(3/5): unicode -> pinyin index
	// 1)chinese unicode
	memset(unicode_data, 0xff, unicode_bytes);
	int fix_code_bytes = bytes_per_index_code * RSP_PINYINS_PER_WORD;
	chinese::pinyin_text_2_code_4rsp(unicode_data, fix_code_bytes, indexs);

	// 2)english-26 unicode
	uint8_t* english_base = unicode_data + chinese::get_pinyin_code_bytes() * RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;
	std::map<std::string, int>::const_iterator find_it;
	char buf[32];
	buf[1] = '\0';
	for (char ch = 'a'; ch <= 'z'; ch ++) {
		uint8_t* to_mem = english_base + (ch - 'a') *  RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;
		const std::string pinyin = std::string(ch, 1);
		buf[0] = ch;
		find_it = indexs.find(buf);
		if (find_it != indexs.end()) {
			int16_t index = find_it->second;
			to_mem[0] = posix_lo8(index);
			to_mem[1] = posix_hi8(index);
		}
	}

	posix_fwrite(file.fp, pinyinhdr_data, bytes_2_4);


	free(data2);
	return true;
}

bool load_pinyin_from_rsp(const std::string& path_to_rsp, trsp_pinyin& pinyin)
{
	pinyin.clear();
	pinyin.rspfile = path_to_rsp;


    tsha1reader src(path_to_rsp, false, NULL);
	if (!src.valid()) {
		return false;
	}
	int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_pinyin92bytes)) {
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_pinyin))) {
		return false;
	}
	if (header.version != SDL_FOURCC(0, 0, 0, RSP_PINYIN_VER_1)) {
		return false;
	}

	memset(&pinyin.hdr, 0, sizeof(pinyin.hdr));
	posix_fread(src.fp, &pinyin.hdr, sizeof(pinyin.hdr));

	if (pinyin.hdr.format.encoding != WAV_PCM_CODE) {
		return false;
	}
	if (pinyin.hdr.format.channels != 1) {
		return false;
	}
	if (pinyin.hdr.format.bitspersample != 16) {
		return false;
	}

	if (pinyin.hdr.reserve != 0) {
		return false;
	}

	// part(3/5): unicode -> pinyin index
	const int unicode_start = sizeof(trsp_header) + sizeof(trsp_pinyin92bytes);
	// const int unicode_bytes = chinese::get_pinyin_code_bytes() * RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;
	const int unicode_bytes = RSP_PINYIN_UNICODES * RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;

	// part(4/5): pinyin index -> address in wav data
	const int index_start = unicode_start + unicode_bytes;
	const int index_bytes = pinyin.hdr.pinyins * sizeof(trsp_pinyinindex);

	if ((int)pinyin.hdr.wav_start < index_start + index_bytes) {
		return false;
	}

	pinyin.bytes_3_4 = (uint8_t*)malloc(unicode_bytes + index_bytes);
	posix_fread(src.fp, pinyin.bytes_3_4, unicode_bytes + index_bytes);
	pinyin.unicode_data = (const trsp_pinyinunicode*)pinyin.bytes_3_4;
	pinyin.index_data = (const trsp_pinyinindex*)(pinyin.bytes_3_4 + unicode_bytes);

	SDL_Log("(read)bytes_2_4{#2(%i) + #3(%i) + #4(%i)} wav_start: %u", 
		(int)sizeof(pinyin.hdr), unicode_bytes, index_bytes, pinyin.hdr.wav_start);

	// header.build_date is time(nullptr), not 20230506;
	// time_t t = header.build_date;
	// tm* timeptr = localtime(&t);
	// uint32_t build_data = (1900 + timeptr->tm_year) * 10000 + (timeptr->tm_mon + 1) * 100 + timeptr->tm_mday;
	pinyin.version = version_from_2uint32(header.version, header.build_date);
	// pinyin.build_data = header.build_date;

    return true;
}

#include <zstd/lib/zstd.h>


/**
 * @brief Zstd 流式压缩器类
 * 
 * 用途：用于将大块数据分块压缩，压缩后的数据通过回调函数实时消费（写入文件或发送网络）。
 */
class tzstd_zipper
{
public:
    // 定义回调函数类型：压缩好的数据将传给此回调
    typedef std::function<void(const uint8_t* zipped_data, size_t size)> OutputCallback;

    /**
     * @brief 构造函数
     * @param output_cb 消费压缩数据的回调函数
     * @param compression_level zstd压缩级别 (1~22，建议默认 3-9)
     */
    tzstd_zipper(const OutputCallback& output_cb, int compression_level = 9)
        : cctx_(nullptr)
		, output_cb_(output_cb)
		, compression_level_(compression_level)
		, previous_delta_value_(0)
		, in_{ nullptr, 0, 0 }
		, out_{ nullptr, 0, 0 }
		, is_finished_(false)
		, total_zipped_size_(0)
	{
        // 1. 创建压缩上下文
        cctx_ = ZSTD_createCCtx();
        if (!cctx_) {
            SDL_Log("ZstdCompressor: Failed to create ZSTD_CCtx!");
			return;
        }

        // 2. 设置压缩参数 (压缩级别)
        size_t ret = ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, compression_level);
        if (ZSTD_isError(ret)) {
            ZSTD_freeCCtx(cctx_);
			cctx_ = nullptr;
            SDL_Log("ZstdCompressor: Failed to set compression level!");
			return;
        }

        // 3. 初始化内部缓冲区 (64KB 栈缓冲区比较安全，这里用向量可以存储略大的内存)
        in_buf_.resize(64 * 1024);   // 输入缓冲 64KB
        out_buf_.resize(128 * 1024); // 输出缓冲 128KB (防止输出溢出)

        // 初始化 ZSTD 输入输出结构体
        in_.src = in_buf_.data();
        in_.size = 0;
        in_.pos = 0;

        out_.dst = out_buf_.data();
        out_.size = out_buf_.size();
        out_.pos = 0;

        is_finished_ = false;
    }

    // 禁止拷贝和赋值 (防止内存重复释放)
	tzstd_zipper(const tzstd_zipper&) = delete;
    tzstd_zipper& operator=(const tzstd_zipper&) = delete;
    tzstd_zipper(tzstd_zipper&&) = delete;
    tzstd_zipper& operator=(tzstd_zipper&&) = delete;

    /**
     * @brief 析构函数：自动释放 ZSTD 上下文内存
     */
    ~tzstd_zipper()
	{
		VALIDATE(is_finished_, null_str);

        if (cctx_ != nullptr) {
            ZSTD_freeCCtx(cctx_);
            cctx_ = nullptr;
        }
    }

	bool valid() const { return cctx_ != nullptr; }

    /**
     * @brief 使用 PCM 差分编码压缩音频数据。
     * 
     * @param pcm_data 原始的 PCM 数据缓冲区。
     * 特别提醒：调用此函数时，pcm_data 所指向的内存**会被直接修改**，
     * 生成差分编码后的数据，以优化压缩率。请勿在此函数调用后，
     * 依赖该缓冲区内的原始数据。
     * 
     * @param size 数据大小（务必为偶数）。
     * @return true 成功；false 失败。
     */
	bool pcm_delta_write(uint8_t* unzip_data, size_t size)
	{ 
		VALIDATE((size & 1) == 0, null_str);
		int16_t* pcm_ptr = (int16_t*)unzip_data; // 强制转为 16位指针
		int sample_count = size / 2;      // 字节数除以 2，得到采样点个数

		for (size_t i = 0; i < sample_count; ++i) {
			int16_t current = pcm_ptr[i];
			pcm_ptr[i] = current - previous_delta_value_; // 存入差值
			previous_delta_value_ = current;                 // 更新上一个值
		}
		return write(unzip_data, size);
	}

    /**
     * @brief 写入待压缩的数据
     * @param unzip_data 原始未压缩的数据指针
     * @param size 数据大小 (字节)
     * @return true 成功, false 失败
     */
    bool write(const uint8_t* unzip_data, size_t size) {
		if (!cctx_ || is_finished_) return false;

		size_t data_remaining = size;
		const uint8_t* data_ptr = unzip_data;

		// 只要还有数据没喂完，就一直循环
		while (data_remaining > 0 || in_.pos < in_.size) {
        
			// 1. 填充输入缓冲区
			if (in_.pos >= in_.size && data_remaining > 0) {
				size_t chunk = std::min(data_remaining, in_buf_.size());
				std::memcpy(in_buf_.data(), data_ptr, chunk);
				in_.src = in_buf_.data();
				in_.size = chunk;
				in_.pos = 0;
				data_ptr += chunk;
				data_remaining -= chunk;
			}

			// 【核心改变】：只使用 ZSTD_e_continue 和 ZSTD_e_flush
			out_.pos = 0;
        
			// 如果缓冲区还有数据在排队，或者还有新数据，我们就用 continue
			// 用 flush 的目的是：逼迫 zstd 把当前能压的数据都压出来，不要扣在内部缓冲里
			ZSTD_EndDirective directive = ZSTD_e_flush; 
        
			size_t ret = ZSTD_compressStream2(cctx_, &out_, &in_, directive);
			if (ZSTD_isError(ret)) {
				return false;
			}

			if (out_.pos > 0 && output_cb_) {
				total_zipped_size_ += out_.pos;
				output_cb_(static_cast<const uint8_t*>(out_.dst), out_.pos);
			}
		}
		return true;
	}

	bool finish()
	{
		ZSTD_inBuffer in = { nullptr, 0, 0 };
		size_t ret = 0;
		do {
			ZSTD_outBuffer out = { out_buf_.data(), out_buf_.size(), 0 }; // 每次都初始化一个干净的 view
			ret = ZSTD_compressStream2(cctx_, &out, &in, ZSTD_e_end);
			if (ZSTD_isError(ret)) return false;
        
			if (out.pos > 0 && output_cb_) {
				total_zipped_size_ += out.pos;
				output_cb_(static_cast<const uint8_t*>(out.dst), out.pos);
			}
		} while (ret != 0); // 只要 ret != 0 就代表还有数据没吐完

		is_finished_ = true;
		return true;
	}

	int compression_level() const { return compression_level_; }
	int total_zipped_size() const { return total_zipped_size_; }

private:
    ZSTD_CCtx* cctx_;          // Zstd 压缩上下文
    OutputCallback output_cb_;           // 数据输出回调
	const int compression_level_;
	int16_t previous_delta_value_;

    std::vector<uint8_t> in_buf_;        // 内部输入缓冲
    std::vector<uint8_t> out_buf_;       // 内部输出缓冲

    ZSTD_inBuffer in_;  // Zstd 输入视图
    ZSTD_outBuffer out_; // Zstd 输出视图

    bool is_finished_;           // 是否已完成压缩
	int total_zipped_size_;
};

class tzstd_unzipper
{
public:
    // 定义回调函数类型：解压（及反差分）后的数据将传给此回调
    typedef std::function<void(const uint8_t* unzip_data, size_t size)> OutputCallback;

    /**
     * @brief 构造函数
     * @param output_cb 消费解压后数据的回调函数
     * @param enable_pcm_delta 是否开启 PCM 16位差分还原 (对应压缩端的差分预处理)
     */
    tzstd_unzipper(const OutputCallback& output_cb)
        : dctx_(nullptr)
        , output_cb_(output_cb)
		, enable_pcm_delta_(false)
        , in_{ nullptr, 0, 0 }
        , out_{ nullptr, 0, 0 }
        , is_finished_(false)
        , pcm_accumulator_(0)
		, pcm_pending_data_(telem_array_C(sizeof(uint8_t)))
		, total_unzip_size_(0)
    {
        // 1. 创建解压上下文
        dctx_ = ZSTD_createDCtx();
        if (!dctx_) {
            SDL_Log("ZstdDecompressor: Failed to create ZSTD_DCtx!");
            return;
        }

        // 2. 初始化内部缓冲区
        in_buf_.resize(64 * 1024);   // 输入缓冲 64KB (接收压缩数据)
        out_buf_.resize(128 * 1024); // 输出缓冲 128KB (解压后数据)

        // 初始化 ZSTD 输入输出结构体
        in_.src = in_buf_.data();
        in_.size = 0;
        in_.pos = 0;

        out_.dst = out_buf_.data();
        out_.size = out_buf_.size();
        out_.pos = 0;

        is_finished_ = false;
    }

    // 禁止拷贝和赋值
    tzstd_unzipper(const tzstd_unzipper&) = delete;
    tzstd_unzipper& operator=(const tzstd_unzipper&) = delete;
    tzstd_unzipper(tzstd_unzipper&&) = delete;
    tzstd_unzipper& operator=(tzstd_unzipper&&) = delete;

    /**
     * @brief 析构函数
     */
    ~tzstd_unzipper()
    {
		VALIDATE(is_finished_, null_str);
        if (dctx_ != nullptr) {
            ZSTD_freeDCtx(dctx_);
            dctx_ = nullptr;
        }
    }

    bool valid() const { return dctx_ != nullptr; }

	bool pcm_delta_write(const uint8_t* zipped_data, size_t size)
	{
		// VALIDATE((size & 1) == 0, null_str);

		enable_pcm_delta_ = true;
		return write_internal(zipped_data, size);
	}

    /**
     * @brief 写入待解压的压缩数据
     * @param zipped_data 压缩数据指针 (注意：这里是压缩数据，不是原始 PCM)
     * @param size 压缩数据大小 (字节)
     * @return true 成功, false 失败
     */
    bool write(const uint8_t* zipped_data, size_t size)
	{
		VALIDATE(!enable_pcm_delta_, null_str);
		return write_internal(zipped_data, size);
    }

    /**
     * @brief 结束解压流 (处理残留在 zstd 内部缓冲区的最后一个块)
     * @return true 成功, false 失败
     */
    bool finish()
    {
        ZSTD_inBuffer in = { nullptr, 0, 0 };
        size_t ret = 0;
        bool last_chunk_was_empty = false; // 用于标记上一轮是否没有产出数据

        while (!last_chunk_was_empty) {
            ZSTD_outBuffer out = { out_buf_.data(), out_buf_.size(), 0 };
            ret = ZSTD_decompressStream(dctx_, &out, &in);
            
            // 检查解压是否出现错误
            if (ZSTD_isError(ret)) {
                return false;
            }

            // 如果这次解压吐出了数据，处理数据
            if (out.pos > 0) {
				handle_decompressed_output(static_cast<uint8_t*>(out.dst), out.pos);

            } else {
                // 【关键】如果这次调用没有吐出任何数据（out.pos == 0），说明彻底结束了
                last_chunk_was_empty = true;
            }
        }

        is_finished_ = true;
        return true;
    }

	int total_unzip_size() const { return total_unzip_size_; }

private:
	void handle_decompressed_output(uint8_t* _data, size_t _size)
	{
		uint8_t* data2 = _data;
		size_t aligned_bytes = _size;
		if (enable_pcm_delta_) {
			// 如果开启了 PCM 16位差分还原
			if (pcm_pending_data_.vsize != 0 || (_size & 1) != 0) {
				pcm_pending_data_.put_size(_data, _size);
				data2 = pcm_pending_data_.data;
				aligned_bytes = pcm_pending_data_.vsize & ~1;
			}

			VALIDATE((aligned_bytes & 1) == 0, null_str);
			int16_t* pcm_ptr = (int16_t*)data2;
			int sample_count = aligned_bytes / 2;
			for (int i = 0; i < sample_count; ++i) {
				pcm_accumulator_ += pcm_ptr[i];
				pcm_ptr[i] = pcm_accumulator_;
			}
		}
		if (output_cb_) {
			total_unzip_size_ += aligned_bytes;
			output_cb_(data2, aligned_bytes);
		}

		if (enable_pcm_delta_) {
			if (pcm_pending_data_.vsize >= aligned_bytes) {
				pcm_pending_data_.vsize -= aligned_bytes;
				VALIDATE(pcm_pending_data_.vsize == 0 || pcm_pending_data_.vsize == 1, null_str);
			} else {
				VALIDATE(pcm_pending_data_.vsize == 0, null_str);
			}
		}
	}

	bool write_internal(const uint8_t* zipped_data, size_t size)
	{
        if (!dctx_ || is_finished_) return false;

        size_t data_remaining = size;
        const uint8_t* data_ptr = zipped_data;

        // 只要还有压缩数据没喂完，就一直循环
        while (data_remaining > 0 || in_.pos < in_.size) {
        
            // 1. 填充输入缓冲区 (将压缩数据填入)
            if (in_.pos >= in_.size && data_remaining > 0) {
                size_t chunk = std::min(data_remaining, in_buf_.size());
                std::memcpy(in_buf_.data(), data_ptr, chunk);
                in_.src = in_buf_.data();
                in_.size = chunk;
                in_.pos = 0;
                data_ptr += chunk;
                data_remaining -= chunk;
            }

            // 2. 调用 zstd 进行解压
            out_.pos = 0; 
            size_t ret = ZSTD_decompressStream(dctx_, &out_, &in_);
            if (ZSTD_isError(ret)) {
                return false;
            }

            // 3. 如果解压出了数据，进行差分处理并回调
            if (out_.pos > 0) {
				handle_decompressed_output(static_cast<uint8_t*>(out_.dst), out_.pos);
            }
        }
        return true;
    }

private:
    ZSTD_DCtx* dctx_;                 // Zstd 解压上下文
    OutputCallback output_cb_;        // 数据输出回调
    bool enable_pcm_delta_;     // 是否开启 16位 PCM 差分还原

    std::vector<uint8_t> in_buf_;     // 内部输入缓冲 (存储压缩数据)
    std::vector<uint8_t> out_buf_;    // 内部输出缓冲 (存储解压后数据)

    ZSTD_inBuffer in_;  // Zstd 输入视图
    ZSTD_outBuffer out_; // Zstd 输出视图

    bool is_finished_;                // 是否已完成解压
    
    // 反差分所需的累加器 (必须作为成员变量，跨越多次 write 调用保持状态)
    int16_t pcm_accumulator_;
	telem_array_C pcm_pending_data_;

	int total_unzip_size_;
};

void zip_wave_data(const uint8_t* zipped_data, size_t size, tfile& file, int& zipped_bytes)
{
	posix_fwrite(file.fp, zipped_data, size);
	zipped_bytes += size;
}

void did_free_data(uint8_t** pdata)
{
	VALIDATE(*pdata != nullptr, null_str);
	free(*pdata);
	*pdata = nullptr;
}

// #define RSP_PINYIN_VER2_1	SDL_FOURCC(0, 0, 0, RSP_PINYIN_VER_1)
// #define RSP_PINYIN_VER2_11	SDL_FOURCC(0, 0, 1, RSP_PINYIN_VER_1)


bool rsp_header_is_valid(const trsp_header& header, int zip_type)
{
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zip_type))) {
		return false;
	}
	if (header.version != SDL_FOURCC(0, 0, 0, RSP_PINYIN_VER_1)) {
		return false;
	}
	return true;
}

#define RSP_PINYIN_FLAG_ZIP		(1 << 31)
bool rsp_pinyin92bytes_is_valid(const trsp_pinyin92bytes& hdr, uint32_t flags)
{
	if (hdr.format.encoding != WAV_PCM_CODE) {
		return false;
	}
	if (hdr.format.channels != 1) {
		return false;
	}
	if (hdr.format.bitspersample != 16) {
		return false;
	}

	if (hdr.reserve != flags) {
		return false;
	}

	// part(3/5): unicode -> pinyin index
	const int unicode_start = sizeof(trsp_header) + sizeof(trsp_pinyin92bytes);
	// const int unicode_bytes = chinese::get_pinyin_code_bytes() * RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;
	const int unicode_bytes = RSP_PINYIN_UNICODES * RSP_PINYIN_BYTES_PER_INDEX_CODE * RSP_PINYINS_PER_WORD;

	// part(4/5): pinyin index -> address in wav data
	const int index_start = unicode_start + unicode_bytes;
	const int index_bytes = hdr.pinyins * sizeof(trsp_pinyinindex);

	if ((int)hdr.wav_start < index_start + index_bytes) {
		return false;
	}
	return true;
}

bool did_write_xchange_rsp_pinyin(tfile& dst_file, const std::string& src_filename, bool to_11)
{
/*
	trsp_pinyin rsp_pinyin;
	bool retval = load_pinyin_from_rsp(src_filename, rsp_pinyin);
	if (!retval) {
		return false;
	}
*/
	tsha1reader src(src_filename, false, NULL);
	if (!src.valid()) {
		return false;
	}

	int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_pinyin92bytes)) {
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));

	if (!rsp_header_is_valid(header, zipt_pinyin)) {
		return false;
	}

	trsp_pinyin92bytes pinyin_hdr;
	memset(&pinyin_hdr, 0, sizeof(pinyin_hdr));
	posix_fread(src.fp, &pinyin_hdr, sizeof(pinyin_hdr));

	uint32_t src_flags = to_11? 0: RSP_PINYIN_FLAG_ZIP;
	uint32_t dst_flags = to_11? RSP_PINYIN_FLAG_ZIP: 0;
	if (!rsp_pinyin92bytes_is_valid(pinyin_hdr, src_flags)) {
		return false;
	}

	const int wave_bytes = payload_size - (int)pinyin_hdr.wav_start;
	// pinyin.hdr.reserve = wave_bytes;
	SDL_Log("wave_bytes: %i", wave_bytes);

	const int block_size = SDL_max(pinyin_hdr.wav_start, CONSTANT_1M * 4);
	uint8_t* data = (uint8_t*)malloc(block_size);
	tauto_destruct_executor destruct_executor(std::bind(&did_free_data, &data));

	int rd_bytes = pinyin_hdr.wav_start - sizeof(header) - sizeof(pinyin_hdr);
	int ret_bytes = posix_fread(src.fp, data, rd_bytes);
	VALIDATE(ret_bytes == rd_bytes, null_str);

	posix_fwrite(dst_file.fp, &header, sizeof(header));
	posix_fwrite(dst_file.fp, &pinyin_hdr, sizeof(pinyin_hdr));
	posix_fwrite(dst_file.fp, data, rd_bytes);

	int output_bytes = 0;

	if (to_11) {
		VALIDATE((wave_bytes & 1) == 0, null_str);
	}
	{
		std::unique_ptr<tzstd_zipper> zipper_unique;
		tzstd_zipper* zipper = nullptr;
		std::unique_ptr<tzstd_unzipper> unzipper_unique;
		tzstd_unzipper* unzipper = nullptr;

		if (to_11) {
			// 19
			zipper_unique.reset(new tzstd_zipper(std::bind(&zip_wave_data, _1, _2, std::ref(dst_file), std::ref(output_bytes))));
			zipper = zipper_unique.get();

		} else {
			unzipper_unique.reset(new tzstd_unzipper(std::bind(&zip_wave_data, _1, _2, std::ref(dst_file), std::ref(output_bytes))));
			unzipper = unzipper_unique.get();
		}
	
		int offset = 0;
		while (offset < wave_bytes) {
			int bytes = block_size;
			if (offset + bytes > wave_bytes) {
				bytes = wave_bytes - offset;
			}

			int retbytes = posix_fread(src.fp, data, bytes);
			VALIDATE(retbytes == bytes, null_str);

			if (to_11) {
				VALIDATE((bytes & 1) == 0, null_str);
				zipper->pcm_delta_write(data, bytes);

			} else {
				unzipper->pcm_delta_write(data, bytes);
			}

			offset += bytes;
		}
		if (to_11) {
			zipper->finish();
			// zipped_bytes = ziper.total_zipped_size();
			VALIDATE(output_bytes == zipper->total_zipped_size(), null_str);

		} else {
			unzipper->finish();
			// zipped_bytes = ziper.total_zipped_size();
			VALIDATE(output_bytes == unzipper->total_unzip_size(), null_str);
		}

		SDL_Log("%s -> %s", 
			utils::format_i64size(wave_bytes).c_str(), utils::format_i64size(output_bytes).c_str());
	}

	posix_fseek(dst_file.fp, 0);
	// overwrite rspheader
	header.zip_size = header.zip_size + (output_bytes - wave_bytes);
	posix_fwrite(dst_file.fp, &header, sizeof(header));

	pinyin_hdr.reserve = dst_flags;
	posix_fwrite(dst_file.fp, &pinyin_hdr, sizeof(pinyin_hdr));

	return true;
}

bool xchange_pinyin_rsp_ver_1_and_11(const std::string& rsp_1, const std::string& rsp_11, bool to_11)
{
	const std::string& src_filename = to_11? rsp_1: rsp_11;
	const std::string& dst_filename = to_11? rsp_11: rsp_1;

	tsha1writer sha1file(dst_filename, nposm, std::bind(&did_write_xchange_rsp_pinyin, _1, std::ref(src_filename), to_11));
	return sha1file.write();
}

bool load_pinyin_rsp()
{
	curr_pinyin.reset();
	curr_pinyin.rspfile = get_binary_file_location("cert", "pinyin.rsp");
	return load_pinyin_from_rsp(curr_pinyin.rspfile, curr_pinyin.rsp);
}

} // end namespace chinese