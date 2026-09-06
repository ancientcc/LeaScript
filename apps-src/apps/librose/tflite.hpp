#ifndef LIBROSE_TFLITE_HPP_INCLUDED
#define LIBROSE_TFLITE_HPP_INCLUDED

#include "rose_tflite.hpp"
#include "ocr/ocr.hpp"
// #include "filesystem.hpp"
// #include "rose_config.hpp"
// #include "wml_exception.hpp"

#include "tensorflow/lite/model.h"
#include <opencv2/core/mat.hpp>

class surface;
class CVideo;

namespace tflite {

extern const bool use_tflits;
extern const std::string labels_txt;
extern std::string showcase_sha256;

// byte sequence, same as opencv.
// enum {color_gray, color_rgb, color_bgr};

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

enum {xmit_script, xmit_tflite, xmit_image};

extern tscript curr_script;

class tslot_impl: public tslot
{
public:
	tslot_impl();

	void set_tflite(const tflite::tscript& script, const tflite::ttflite& tflite) override;
	std::vector<std::pair<float, SDL_Rect> > classifier_image(const cv::Mat& argb, tflite::tresult& result) override;

private:
	void load_tflite_mode();

private:
	tflite::tscript script_;
	tflite::ttflite tflite_;

	tflite::tsession current_session_;

	tflite::tresult result_;
	// std::vector<std::pair<float, SDL_Rect> > classifier_rects_;
};

struct treadme
{
	static std::map<std::string, std::string> support_keywords;
	static std::map<std::string, std::string> support_states;

	static void prepare_stats_keywords();
	static const std::string& keyword_name(const std::string& id);
	static std::string keywords_name(const std::vector<std::string>& ids);

	static const std::string& state_name(const std::string& id);
};

struct tnetwork_item 
{
	explicit tnetwork_item(const std::string& name, int size, int xmit_type)
		: name(name)
		, size(size)
		, xmit_type(xmit_type)
		, timestamp(0)
		, follows(0)
		, comments(0)
		, showcase(false)
	{}

	void set_model_special(const std::string& _token, const int64_t _timestamp, const std::string& state_id, const std::string& keywords_id, const std::string& _desc, const bool _showcase, const int _follows, const int _comments);

	bool operator<(const tnetwork_item& that) const 
	{
		if (xmit_type != that.xmit_type) {
			if (xmit_type == xmit_script) {
				return true;
			} else if (xmit_type == xmit_tflite) {
				if (that.xmit_type == xmit_script) {
					return false;
				}
				return true;
			}
			return false;
		}
		int cmp = strcmp(name.c_str(), that.name.c_str());
		return cmp < 0;
	}

	std::string name;
	int size;
	int xmit_type;

	// model special
	std::string token;
	int64_t timestamp;
	std::string state;
	std::vector<std::string> keywords;
	std::string description;
	int follows;
	int comments;
	bool showcase;
};

struct ttoken_tflite
{
	explicit ttoken_tflite(const std::string& token, const int timestamp, const std::string& name, const bool followed, const std::string& nickname, const std::string& keywords_id, const std::string& state_id, const std::string& desc, int follows, int comments, int size);

	bool operator<(const ttoken_tflite& that) const 
	{
		if (follows != that.follows) {
			return follows > that.follows;
		}
		if (comments != that.comments) {
			return comments > that.comments;
		}

		const int name_cmp = strcmp(name.c_str(), that.name.c_str());
		if (name_cmp) {
			return name_cmp < 0;
		}

		const int nickname_cmp = strcmp(nickname.c_str(), that.nickname.c_str());
		if (nickname_cmp) {
			return nickname_cmp < 0;
		}

		const int token_cmp = strcmp(token.c_str(), that.token.c_str());
		return token_cmp < 0;
	}

	const std::string token;
	int64_t timestamp;
	std::string name;
	bool followed;
	const std::string nickname;
	int follows;
	int comments;
	int size;

	std::vector<std::string> keywords;
	std::string state;
	std::string description;
};

struct tuser {
	tuser(const std::string& sessionid = "", const std::string& nick = "", int vip = 0, int64_t vip_ts = 0, const int vip_op = nposm)
		: nick(nick)
		, sessionid(sessionid)
		, vip(vip)
		, vip_ts(vip_ts)
		, vip_op(vip_op)
	{}

	bool valid() const;

	std::string mobile;
	std::string sessionid;
	std::string nick;
	int vip;
	int64_t vip_ts;
	int vip_op;
};

extern const tuser null_user2;
extern tuser current_user;

} // namespace tflite

namespace preferences {
	std::string script();
	void set_script(const std::string& value);
}

#endif
