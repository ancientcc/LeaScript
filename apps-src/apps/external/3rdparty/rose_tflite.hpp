#ifndef LIBROSE2_TFLITE_HPP_INCLUDED
#define LIBROSE2_TFLITE_HPP_INCLUDED

#include "config.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_exception.hpp"
#include "rose_filesystem.hpp"

// #include <opencv2/core/mat.hpp>

namespace cv {
class Mat;
};

namespace tflite {
/*
 
extern const bool use_tflits;
extern const std::string labels_txt;
extern std::string showcase_sha256;
*/
// byte sequence, same as opencv.
enum {color_gray, color_rgb, color_bgr};
/*
struct tparams
{
	tparams(int width, int height, int color)
		: width(width)
		, height(height)
		, color(color)
	{}

	int width;
	int height;
	int color;
};

struct tsession {
	std::string key;
	std::unique_ptr<tfile> fp;
	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;

	bool valid() const { return fp.get() && model.get() && interpreter.get(); }
	void reset() 
	{
		// must clear key. becuase load_model(...)'s logic.
		key.clear();

		fp.reset(nullptr);
		model.reset(nullptr);
		interpreter.reset(nullptr);
	}
};

bool is_valid_tflite(const std::string& file);
bool is_valid_tflits(const std::string& file, const std::string& sha256);
bool load_model(const std::string& file, tsession& session, const std::string& sha256);
std::map<std::string, tocr_result> ocr(const config& app_cfg, CVideo& video, const surface& surf, const std::vector<std::string>& fields, const std::string& pb_short_path, const std::string& sha256, const tflite::tparams& tflite_params, const bool bg_color_partition);

std::vector<std::string> load_labels_txt(const std::string& path);
std::set<wchar_t> load_labels_txt_unicode(const std::string& path);

uint32_t invoke_classifier(Interpreter& interpreter, const int output_size, const int N, const float kThreshold, std::vector<std::pair<float, int> >& top_results);
wchar_t inference_char(tsession& session, const std::string& tflite_file, const std::string& sha256, cv::Mat& src, uint32_t* used_ticks);

std::string sha256(const std::string& private_key, const std::string& appid);
#pragma pack(4)
struct ttflits_header {
	uint64_t timestamp;
	uint32_t fourcc;
	uint32_t reserve;
};
#pragma pack()
void encrypt_tflite(const std::string& src_path, const std::string& dst_path, const std::string& sha256);
bool decrypt_tflite(uint8_t* fdata, int64_t fsize, const std::string& sha256);
*/

// enum {xmit_script, xmit_tflite, xmit_image};
enum {source_camera, source_img};
enum {scenario_classifier, scenario_detect, scenario_ocr, scenario_pr};

class DECLSPEC tscript
{
public:
	tscript()
	{
		clear();
	}

	tscript(const std::string& file, bool res, std::string* err_report);

	bool from(const config& cfg, std::string& err_report);
	bool valid() const { return !id.empty() && !tflite.empty() && scenario != nposm && color != nposm; }

	void clear();

	std::string path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::app_dir_root: game_config::preferences_dir);
		ss << "/tflites/" + id;
		return ss.str();
	}

	bool operator<(const tscript& that) const 
	{
		if (!!test != !!that.test) {
			return test;
		}
		VALIDATE(!test && !that.test, null_str);

		if (!!res != !!that.res) {
			return !res;
		}
		int cmp = strcmp(id.c_str(), that.id.c_str());
		return cmp < 0;
	}

	bool operator==(const tscript& that) const
	{
		return res == that.res && test == that.test && id == that.id && 
			tflite == that.tflite;
	}

public:
	bool test;
	bool res;
	std::string id;
	std::string tflite;

	std::string warn;
	int source;
	std::string img;
	int scenario;
	int width;
	int height;
	int color;
	float kthreshold;
};

class DECLSPEC ttflite
{
public:
	ttflite(const std::string& id, bool res)
		: id(id)
		, res(res)
		, use_tflits(false)
	{}

	bool valid() const { return !id.empty(); }

	std::string path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::app_dir_root: game_config::preferences_dir);
		ss << "/tflites/" + id;
		return ss.str();
	}

	std::string tflits_path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::app_dir_root: game_config::preferences_dir);
		ss << "/tflites/" + id + "/" + id;
		ss << (use_tflits? ".tflits": ".tflite");
		return ss.str();
	}

	bool aes_encrypt(const std::string& key) const;
	bool aes_decrypt(const std::string& key) const;

	bool operator<(const ttflite& that) const 
	{
		if (res != that.res) {
			return !res;
		}
		int cmp = strcmp(id.c_str(), that.id.c_str());
		return cmp < 0;
	}

	bool operator==(const ttflite& that) const
	{
		return id == that.id && res == that.res;
	}

public:
	std::string id;
	bool res;
	bool use_tflits;
};

class DECLSPEC tresult
{
public:
	struct titem {
		titem(float score, int index, const std::string& name = null_str)
			: score(score)
			, index(index)
			, name(name)
		{}

		float score;
		int index;
		std::string name;
	};

	void clear()
	{
		items.clear();
		used_ms = 0;
	}

public:
	std::string to_string() const;

	std::vector<titem> items;
	uint32_t used_ms;
};

class DECLSPEC tslot
{
public:
	tslot();
	virtual ~tslot();

	virtual void set_tflite(const tflite::tscript& script, const tflite::ttflite& tflite) = 0;
	virtual std::vector<std::pair<float, SDL_Rect> > classifier_image(const cv::Mat& argb, tflite::tresult& result) = 0;

	// virtual const tflite::tresult& get_result() const = 0;
};

DECLSPEC tslot& get_curr_slot();

} // namespace tflite

#endif
