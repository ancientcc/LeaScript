#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/pinyin2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "chinese.hpp"
#include "filesystem.hpp"
#include "sound.hpp"
#include "formula_string_utils.hpp"
#include <SDL_mixer.h>

using namespace std::placeholders;

twave_pcm_hdr_44bytes default_wav_hdr = 
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0  
};

const twave_pcm_hdr_44bytes to_wav_hdr = 
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	44100,
	88200,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0  
};

void convert_16k_44100(const std::string& src, const std::string& dest)
{
	tfile file(src, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		return;
	}
	twave_pcm_hdr_44bytes hdr;
	posix_fread(file.fp, &hdr, sizeof(hdr));
	if (memcmp(hdr.riff, default_wav_hdr.riff, 4) != 0) {
		return;
	}
	if (memcmp(hdr.fmt, default_wav_hdr.fmt, 4) != 0) {
		return;
	}
	if (memcmp(hdr.data, default_wav_hdr.data, 4) != 0) {
		return;
	}
	if (hdr.samples_per_sec != default_wav_hdr.samples_per_sec) {
		return;
	}


	Mix_Chunk* chunk = (Mix_Chunk *)SDL_malloc(sizeof(Mix_Chunk));
	chunk->abuf = (uint8_t*)SDL_malloc(hdr.data_size);
	chunk->alen = hdr.data_size;
	memset(chunk->abuf, 0, chunk->alen);

	// posix_fseek(file.fp, rsp.hdr.wav_start + index.offset);
	int result = posix_fread(file.fp, chunk->abuf, chunk->alen);
	if (result != chunk->alen) {
		return;
	}

	// write_file("c:/ddksample/1.dat", chunk->abuf, chunk->alen);

	// convert
	SDL_AudioSpec wavespec;
	SDL_AudioSpec mixer;
	SDL_AudioCVT wavecvt;

	memset(&wavespec, 0, sizeof(wavespec));
	wavespec.freq = hdr.samples_per_sec;
	wavespec.channels = hdr.channels;
	wavespec.samples = 4096;
	wavespec.format = AUDIO_S16LSB;

	// Uint16 format;
	memset(&mixer, 0, sizeof(mixer));
	mixer.freq = 44100;
	mixer.format = AUDIO_S16LSB;
	mixer.channels = 1;
	mixer.samples = 4096;
	// mixer.format = AUDIO_S16LSB;
	mixer.size = mixer.samples * mixer.channels * 2; // 16384, 2 is 16bit.

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
			return;
		}
		samplesize = ((wavespec.format & 0xFF)/8)*wavespec.channels;
		wavecvt.len = chunk->alen & ~(samplesize-1);
		wavecvt.buf = (Uint8 *)SDL_calloc(1, wavecvt.len*wavecvt.len_mult);
		if (wavecvt.buf == NULL) {
			SDL_SetError("Out of memory");
			SDL_free(chunk->abuf);
			SDL_free(chunk);
			return;
		}
		SDL_memcpy(wavecvt.buf, chunk->abuf, wavecvt.len);
		SDL_free(chunk->abuf);

		/* Run the audio converter */
		if (SDL_ConvertAudio(&wavecvt) < 0) {
			SDL_free(wavecvt.buf);
			SDL_free(chunk);
			return;
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

	twave_pcm_hdr_44bytes wav_hdr = to_wav_hdr;
	wav_hdr.data_size = chunk->alen;
	wav_hdr.size_8 = wav_hdr.data_size + (sizeof(wav_hdr) - 8);

	{
		tfile file2(dest, GENERIC_WRITE, CREATE_ALWAYS);
		if (!file2.valid()) {
			return;
		}
		posix_fwrite(file2.fp, &wav_hdr, sizeof(wav_hdr));
		posix_fwrite(file2.fp, chunk->abuf, chunk->alen);
	}
	SDL_free(chunk->abuf);
	SDL_free(chunk);
}

namespace gui2 {

REGISTER_DIALOG(studio, pinyin2)

tpinyin2::tpinyin2()
	: pinyinrsp_path_("c:/librose/pinyinrsp")
	, rspfile_(game_config::preferences_dir + "/cert/pinyin.rsp")
	, test_file_("c:/librose/pinyinrsp/test.txt")
	, frequence_file_(game_config::path + "/data/core/cert/chinesefrequence.txt")
	, pinyin_4sort_file_(game_config::path + "/data/core/cert/chinesepinyin.txt")
	, errors_list_(nullptr)
{
	VALIDATE(game_config::os == os_windows, null_str);
}

std::string tpinyin2::version_str() const
{
	char buf[256];

	SDL_snprintf(buf, sizeof(buf), "V%s %s", 
		chinese::curr_pinyin.rsp.version.str(true).c_str(), utils::format_time_hms(chinese::curr_pinyin.rsp.hdr.ts).c_str());
	return buf;
}

void tpinyin2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Pinyin"));
	ver_widget_ = find_widget<tlabel>(window_, "version", false, true);
	ver_widget_->set_label(version_str());

	utils::string_map symbols;
	symbols["pinyinrsp"] = pinyinrsp_path_;
	symbols["rspfile"] = rspfile_;
	symbols["min_size"] = str_cast(MIN_WAV_SIZE) + "(" + utils::format_i64size(MIN_WAV_SIZE) + ")";
	symbols["max_size"] = str_cast(MAX_WAV_SIZE) + "(" + utils::format_i64size(MAX_WAV_SIZE) + ")";
	std::string msg = vgettext2("remark($pinyinrsp $rspfile $min_size $max_size)", symbols);
	find_widget<tlabel>(window_, "remark", false).set_label(msg);

	// pingyin rsp grid
	ttext_box* text_box = find_widget<ttext_box>(window_, "pinyinrsp_path", false, true);
	text_box->set_label(pinyinrsp_path_);
	text_box->set_active(false);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(window_, "generate_pinyinrsp", false)
		, std::bind(
		&tpinyin2::click_generate_pinyinrsp
		, this));

	tlistbox* list = find_widget<tlistbox>(window_, "errors", false, true);
	errors_list_ = list;
	list->enable_select(false);

	text_box = find_widget<ttext_box>(window_, "test_file", false, true);
	text_box->set_label(test_file_);
	text_box->set_active(false);
	tbutton* button = find_widget<tbutton>(window_, "play", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
		&tpinyin2::click_play
		, this));

	text_box = find_widget<ttext_box>(window_, "frequence_txt", false, true);
	text_box->set_label(frequence_file_);
	text_box->set_active(false);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(window_, "frequence_txt2code", false)
		, std::bind(
		&tpinyin2::click_frequence_text2code
		, this));

	text_box = find_widget<ttext_box>(window_, "pinyin_4sort_txt", false, true);
	text_box->set_label(pinyin_4sort_file_);
	text_box->set_active(false);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(window_, "pinyin_4sort_txt2code", false)
		, std::bind(
		&tpinyin2::click_4sort_text2code
		, this));
}

void tpinyin2::post_show()
{
	chinese::curr_pinyin.speak(null_str);
}

void tpinyin2::reload_errors(gui2::tlistbox& list, int pinyins, const std::vector<std::string>& errors)
{
	const int max_show_errors = 10;
	int show_errors = SDL_min(errors.size(), max_show_errors);
	utils::string_map symbols;
	symbols["time"] = utils::format_time_date(time(nullptr));
	symbols["pinyins"] = str_cast(pinyins);
	symbols["total"] = str_cast(errors.size());
	symbols["show"] = str_cast(show_errors);
	std::string msg = vgettext2("$time Contains $pinyins pinyin. $total errors were found while generate. Only $show are shown below", symbols);
	find_widget<tlabel>(window_, "generate_rsp_result", false).set_label(msg);

	list.clear();

	std::map<std::string, std::string> data;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = errors.begin(); it != errors.end(); ++ it, at ++) {
		if (at >= show_errors) {
			break;
		}
		const std::string& msg = *it;
		
		data["label"] = msg;

		gui2::ttoggle_panel& row = list.insert_row(data);
	}
}

bool is_valid_pinyin_wav(const std::string& name)
{
	const int size = name.size();
	const int min_chars = 5; // x.wav
	if (size < min_chars) {
		return false;
	}

	enum step_t {step_alpha, step_wav};
	char must_ch = 0;
	step_t step = step_alpha;
	const char* c_str = name.c_str();
	for (int at = 0; at < size; at ++) {
		const char ch = c_str[at];
		if (step == step_alpha) {
			if (ch < 'a' || ch > 'z') {
				if (ch >= '0' && ch <= '4') {
					step = step_wav;
					must_ch = '.';
				} else if (ch == '.' && at == 1) {
					// english-26
					step = step_wav;
					must_ch = 'w';

				} else {
					return false;
				}
			}

		} else {
			VALIDATE(step == step_wav && must_ch != 0, null_str);
			if (must_ch == '.') {
				must_ch = 'w';

			} else if (must_ch == 'w') {
				must_ch = 'a';

			} else if (must_ch == 'a') {
				must_ch = 'v';

			} else if (must_ch == 'v') {
				if (at != size - 1) {
					return false;
				}
			}
		}
	}
	return true;
}

static bool did_collect_pinyin_wav(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, std::string>& wavs, 
	std::vector<std::string>& err_names, std::vector<std::string>& repeat_names, std::vector<std::string>& err_size)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	// const std::string name = utils::lowercase(dirent->name);
	const std::string name = dirent->name;
	if (isdir) {
		const std::string dir2 = dir + "/" + name;
		const int size = name.size();
		const char* c_str = name.c_str();
		bool conti = false;
		if (size == 1) {
			if (c_str[0] >= 'a' && c_str[0] <= 'z') {
				conti = true;
			}
		}
		if (conti) {
			::walk_dir(dir2, false, std::bind(
				&did_collect_pinyin_wav
				, _1, _2, std::ref(wavs), std::ref(err_names), std::ref(repeat_names), std::ref(err_size)));
		} else {
			err_names.push_back(dir2);
		}

	} else {
		const std::string file = dir + "/" + name;

		if (is_valid_pinyin_wav(name)) {
			if (wavs.count(name) == 0) {
				if (dirent->size >= MIN_WAV_SIZE && dirent->size <= MAX_WAV_SIZE) {
					wavs.insert(std::make_pair(name, file));
				} else {
					err_size.push_back(file);
				}
			} else {
				repeat_names.push_back(file);
			}
		} else {
			err_names.push_back(file);
		}
	}

	return true;
}

void tpinyin2::click_generate_pinyinrsp()
{
	VALIDATE(SDL_IsDirectory(pinyinrsp_path_.c_str()), null_str);
	// std::string result;

	std::map<std::string, std::string> wavs;
	std::vector<std::string> err_names;
	std::vector<std::string> repeat_names;
	std::vector<std::string> err_size;
	std::vector<std::string> err_open;
	std::vector<std::string> err_format;
	::walk_dir(pinyinrsp_path_, false, std::bind(
				&did_collect_pinyin_wav
				, _1, _2, std::ref(wavs), std::ref(err_names), std::ref(repeat_names), std::ref(err_size)));

	{
		const std::string bundleid = "py.leagor.chinese";
		tsha1writer sha1file(rspfile_, nposm, std::bind(&chinese::did_write_rsp_pinyin, _1, bundleid, std::ref(game_config::rose_version), 
			std::ref(wavs), std::ref(err_open), std::ref(err_size), std::ref(err_format)));
		sha1file.write();

		const std::string rsp_11 = game_config::preferences_dir + "/cert/pinyin_zip.rsp";
		// bool retval = chinese::xchange_pinyin_rsp_ver_1_to_11(rspfile_, rsp_11);
		bool retval = chinese::xchange_pinyin_rsp_ver_1_and_11(rspfile_, rsp_11, true);
		VALIDATE(retval, null_str);

		const std::string rsp_1 = game_config::preferences_dir + "/cert/pinyin_unzipped.rsp";
		// retval = chinese::xchange_pinyin_rsp_ver_11_to_1(rsp_11, rsp_1);
		retval = chinese::xchange_pinyin_rsp_ver_1_and_11(rsp_1, rsp_11, false);
	}

	struct titem {
		titem(const std::vector<std::string>& vec, const std::string& reason)
			: vec(vec)
			, reason(reason)
		{}

		const std::vector<std::string>& vec;
		const std::string reason;
	};

	std::vector<std::string> errors;

	char buf[512];
	std::vector<titem> items; 
	items.push_back(titem(err_names, _("Err file name")));
	items.push_back(titem(repeat_names, _("Repeat file name")));
	items.push_back(titem(err_size, _("Err file size")));
	items.push_back(titem(err_open, _("Open fail")));
	items.push_back(titem(err_format, _("Err file format")));

	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		for (std::vector<std::string>::const_iterator it2 = item.vec.begin(); it2 != item.vec.end(); ++ it2) {
			const std::string& file = *it2;
			SDL_snprintf(buf, sizeof(buf), "%s(%s)", file.c_str(), item.reason.c_str());
			errors.push_back(buf);
		}
	}

	chinese::load_pinyin_rsp();
	ver_widget_->set_label(version_str());
	reload_errors(*errors_list_, chinese::curr_pinyin.rsp.hdr.pinyins, errors);
	// return result;
}

static bool did_collect_xfyun_wav(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, std::string>& wavs, 
	std::vector<std::string>& err_names, std::vector<std::string>& repeat_names, std::vector<std::string>& err_size)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	// const std::string name = utils::lowercase(dirent->name);
	const std::string name = dirent->name;
	if (isdir) {
		return true;

	} else {
		const std::string file = dir + "/" + name;

		if (is_valid_pinyin_wav(name)) {
			if (wavs.count(name) == 0) {
				// if (dirent->size >= MIN_WAV_SIZE && dirent->size <= MAX_WAV_SIZE) {
				if (true) {
					wavs.insert(std::make_pair(name, file));
				} else {
					err_size.push_back(file);
				}
			} else {
				repeat_names.push_back(file);
			}
		} else {
			err_names.push_back(file);
		}
	}

	return true;
}

void convert_xfyun_wav()
{
	const std::string xfyun_path = "C:/librose/pinyinrsp/xfyun";
	std::map<std::string, std::string> wavs;
	std::vector<std::string> err_names;
	std::vector<std::string> repeat_names;
	std::vector<std::string> err_size;
	std::vector<std::string> err_open;
	std::vector<std::string> err_format;
	::walk_dir(xfyun_path, false, std::bind(
				&did_collect_xfyun_wav
				, _1, _2, std::ref(wavs), std::ref(err_names), std::ref(repeat_names), std::ref(err_size)));

	const std::string x_path = "C:/librose/pinyinrsp/x";
	for (std::map<std::string, std::string>::const_iterator it = wavs.begin(); it != wavs.end(); ++ it) {
		const std::string& key = it->first;
		const std::string& src = it->second;
		// convert_16k_44100("C:/librose/pinyinrsp/xfyun/da4.wav", "C:/librose/pinyinrsp/x/da4.wav");
		convert_16k_44100(src, x_path + "/" + key);
	}
}

void test_utf8()
{
	const uint8_t utf8[] = {0xe4, 0xb8, 0xad, 0xe7, 0xbe, 0x8e, 0xe6, 0x95, 0xb0, 0xe6, 0x8e, 0xa7, 0xe3, 0x80, 0x82, '\0'};
	const std::string text = (const char*)utf8;
	utils::utf8_iterator curr_itor = text;
	for (; curr_itor != utils::utf8_iterator::end(text); ++ curr_itor) {
		wchar_t wch = *curr_itor;
		SDL_Log("wch: 0x%04x", wch);
	}
}

void tpinyin2::click_play()
{
	// test_utf8();

	// convert_xfyun_wav();
	// return;

	if (!chinese::curr_pinyin.rsp.valid()) {
		gui2::show_message(null_str, "No valid pinyin.rsp loaded");
		return;
	}
	// const std::string wav("C:/librose/pinyinrsp/bai2.wav");
	// sound::play_sound("bai2.wav");
	// sound::play_sound("cui4.wav");
	// sound::play_sound("cui4.wav");
	// Mix_LoadWAV("C:/librose/pinyinrsp/bai2.wav");
	// Mix_LoadWAV("C:/librose/pinyinrsp/ge4.wav");

	// alen: 0x3faa
	// 4549 play_chunk(key:bai2.wav)
	// 4724 channel_finished_hook(channel:11)
	// 5833 play_chunk(key:bai2.wav)
	// 5944 channel_finished_hook(channel:11)

	tfile file(test_file_, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (fsize == 0) {
		char buf[256];
		SDL_snprintf(buf, sizeof(buf), "Open %s fail, or is empty", test_file_.c_str());
		gui2::show_message(null_str, buf);
		return;
	}
	std::string text = file.data;

	chinese::curr_pinyin.speak(text);
}

void tpinyin2::click_frequence_text2code()
{
	chinese::chinesefrequence_text_2_code();
	gui2::show_message(null_str, "[chinesefrequence.txt]Conversion finished");
}

void tpinyin2::click_4sort_text2code()
{
	chinese::pinyin_text_2_code_4sort();
	gui2::show_message(null_str, "[chinesepinyin.txt]Conversion finished");
}

} // namespace gui2

