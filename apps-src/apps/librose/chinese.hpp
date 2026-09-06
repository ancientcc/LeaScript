/* $Id: sha1.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010 by Benoit Timbert <benoit.timbert@free.fr>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_CHINESE_HPP_INCLUDED
#define LIBROSE_CHINESE_HPP_INCLUDED

#include <string>
#include <map>
#include <rose_util.hpp>
#include "rose_filesystem.hpp"
#include "serialization/string_utils.hpp"
#include "sound.hpp"
#include "rose_thread.hpp"

#include "aplt_clazz.hpp"

/*
namespace sound {
class thook_get_mix_data;
}
*/

namespace chinese {

extern const int fix_bytes_in_code;
extern const wchar_t min_unicode;
extern const wchar_t max_unicode;

void create_chinesefrequence_raw();
void chinesefrequence_text_2_code();

void pinyin_text_2_code_4sort();
void pinyin_text_2_code_4rsp(uint8_t* mem, int fix_code_bytes, const std::map<std::string, int>& indexs);
void load_pinyin_code_4sort();
const char* get_pinyin_code();
int get_pinyin_code_bytes();

const char* pinyin(wchar_t wch);
bool key_matched(const std::string& src, const std::string& key);

inline bool is_chinese(wchar_t wch)
{ 
	return wch >= min_unicode && wch <= max_unicode;
}

#define RSP_PINYIN_CHINESE_UNICODES	20902	// 20902(chinese[0x9FA5 - 0x4E00 + 1])
#define RSP_PINYIN_UNICODES	20960	// 20902(chinese[0x9FA5 - 0x4E00 + 1]) + 26(english) + 32(future use)

#define MIN_WAV_SIZE		10240	// 10240(10k), 16384(16k), 20480(20k)
#define MAX_WAV_SIZE		30720	// 28672(28K), 30720(30K), 32768(32K)

class tamplify
{
public:
	tamplify()
		// Number of entries in the 16-bit signed PCM lookup table.
		: LUT_SIZE(65536)
		, lut_1x(nullptr)
		, lut_1_25x(nullptr)
		, lut_1_6x(nullptr)
		, lut_2x(nullptr)
		, current_lut(nullptr)
		, curr_mode(nposm)
	{
		audio_amp_init();
	}

	virtual ~tamplify()
	{
		audio_amp_deinit();
	}

	posix_noncopyable(tamplify);

	// 1. 初始化音频表（App启动时调用一次，从堆中分配 512 KB）
	// 返回值：0 成功，-1 内存分配失败
	int audio_amp_init(void);

	// 2. 销毁音频表释放内存（App 退出前调用）
	void audio_amp_deinit(void);

	// 3. 切换当前放大模式
	void audio_amp_set_mode(int mode);

	int audio_amp_get_mode() const { return curr_mode; }

	// 4. 实时处理音频缓冲区（你播放线程里调用的核心函数）
	// audio_data: 16位 PCM 数据指针
	// frames: 采样点个数（如果是立体声，注意这里是 sample 总数，不是字节总数）
	void audio_amp_process(int16_t* audio_data, int frames);

protected:
	void generate_lut(int16_t* lut, float gain);

protected:
	const int LUT_SIZE;
	int16_t* lut_1x;
	int16_t* lut_1_25x;
	int16_t* lut_1_6x;
	int16_t* lut_2x;

	// Pointer to the currently active table (pointing to one of the 4 tables above).
	int16_t* current_lut;
	int curr_mode;
};

class trsp_pinyin
{
public:
	trsp_pinyin()
		: bytes_3_4(nullptr)
	{
		clear();
	}

	~trsp_pinyin()
	{
		clear();
	}
	posix_noncopyable(trsp_pinyin);

	bool valid() const { return !rspfile.empty() && bytes_3_4 != nullptr && hdr.wav_start != nposm; }

	void clear()
	{
		rspfile.clear();
		hdr.wav_start = nposm;
		if (bytes_3_4 != nullptr) {
			free(bytes_3_4);
			bytes_3_4 = nullptr;
		}
		unicode_data = nullptr;
		index_data = nullptr;
	}

public:
	std::string rspfile; // full rspfile name
	trsp_pinyin92bytes hdr;
	version_info version;
	uint8_t* bytes_3_4;
	const trsp_pinyinunicode* unicode_data;
	const trsp_pinyinindex* index_data;
};

class tpinyin: public aplt::tpinyin
{
public:
	tpinyin();
	~tpinyin();
	posix_noncopyable(tpinyin);

	uint32_t speak(const std::string& _text) override;
	bool is_speaking() const override { return !text.empty(); }
	const std::string& get_text() override { return text; }

	void repeat_speak(const std::string& _text) override;
	bool is_repeat_speaking() override { return !repeat_text.empty(); }

	std::string from_unicodes(const wchar_t* unicodes, int count, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match) override;
	std::string from_utf8str(const std::string& str, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match) override;

	void set_amp_mode(int mode) override;
	int get_amp_mode() const override { return amp_.audio_amp_get_mode(); }

	void reset()
	{
		text.clear();
		memset(unicodes, 0, sizeof(unicodes));
		hook_get_mix_data.reset();
		text_all_loaded = false;
		{
			threading::lock lock(play_data_mutex_);
			play_data_vsize_ = 0;
		}
		pure_digits_ = 0;
	}

	void pump();

	bool play_one_unicode(wchar_t _unicode, const char* special);
	void get_mix_data(uint8_t* stream, int len);

private:
	void did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start) override;

	void async_speak(const std::string& _text);
	void resize_play_data(int size);
	void play_standstill(int ms);
	void play_le_ten_thousand(int integer);
	void play_integer(int64_t integer);
	bool speak_when_first_digit(wchar_t wch);

public:
	std::string rspfile;
	trsp_pinyin rsp;

	std::unique_ptr<sound::thook_get_mix_data> hook_get_mix_data;
	bool text_all_loaded;

	int interval;
	utils::utf8_iterator curr_itor;
	enum {IDX_LAST, IDX_CURR, IDX_NEXT};
	wchar_t unicodes[3];
	std::string text;
	std::string repeat_text;

private:
	std::map<wchar_t, int> standstill_;
	std::set<wchar_t> pair_symbols_;
	wchar_t digits_[10];
	threading::mutex play_data_mutex_;
	uint8_t* play_data_;
	int play_data_vsize_;
	int play_data_size_;


	threading::mutex async_text_mutex_;
	std::string async_text_;
	bool async_text_dirty_;

	std::string async_repeat_text_;

	uint32_t curr_id_;
	uint32_t async_id_;

	class tusing_id_lock
	{
	public:
		tusing_id_lock(tpinyin& pinyin, int id);
		~tusing_id_lock();

	private:
		tpinyin& pinyin_;
	};
	uint32_t using_id_;

	class tdisable_speak_lock
	{
	public:
		tdisable_speak_lock(tpinyin& pinyin);
		~tdisable_speak_lock();

	private:
		tpinyin& pinyin_;
	};
	bool disable_speak_;

	int pure_digits_;

	tamplify amp_;
};

extern tpinyin curr_pinyin;

bool did_write_rsp_pinyin(tfile& file, const std::string& bundleid, const version_info& rose_version, const std::map<std::string, std::string>& wavs, 
	std::vector<std::string>& err_open, std::vector<std::string>& err_size, std::vector<std::string>& err_format);
bool load_pinyin_rsp();

bool xchange_pinyin_rsp_ver_1_and_11(const std::string& rsp_1, const std::string& rsp_11, bool to_11);

} // end namespace chinese

#endif
