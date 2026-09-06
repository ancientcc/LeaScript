#define GETTEXT_DOMAIN "rose-lib"

#include "rose_tflite.hpp"
#include "gettext.hpp"
#include "rose_string_utils_dll.hpp"
#include "rose_filesystem.hpp"

/*
#include "filesystem.hpp"
#include "tflite.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"

#include "config_cache.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "serialization/parser.hpp"

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/string_util.h"

#include <opencv2/imgproc.hpp>
#include <sstream>
#include <queue>

#include <openssl/sha.h>

#include "rose_config.hpp"
#include "preferences.hpp"
*/
namespace tflite {
/*
const bool use_tflits = false;
const std::string labels_txt = "labels.txt";
std::string showcase_sha256 = "F2C29A2A41DD6943B0EE9BA77D1E1D1BD976B1E1F2C29A2A41DD6943B0EE9BA7";

bool is_valid_tflite(const std::string& file)
{
	size_t pos = file.rfind(".tflite");
	if (pos == std::string::npos || pos + 7 != file.size()) {
		return false;
	}

	tfile tflite(file, GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = tflite.read_2_data();
	if (!fsize) {
		return false;
	}

	std::unique_ptr<FlatBufferModel> model = tflite::FlatBufferModel::BuildFromBuffer(tflite.data, fsize);
	if (!model.get()) {
		// Failed to mmap model
		return false;
	}

	return true;
}

bool is_valid_tflits(const std::string& file, const std::string& sha256)
{
	size_t pos = file.rfind(".tflits");
	if (pos == std::string::npos || pos + 7 != file.size()) {
		return false;
	}

	tfile tflite(file, GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = tflite.read_2_data();
	if (!fsize) {
		return false;
	}
	if (!decrypt_tflite((uint8_t*)tflite.data, fsize, sha256)) {
		return false;
	}

	std::unique_ptr<FlatBufferModel> model = tflite::FlatBufferModel::BuildFromBuffer(tflite.data + sizeof(ttflits_header), fsize - sizeof(ttflits_header));
	if (!model.get()) {
		// Failed to mmap model
		return false;
	}

	return true;
}

class tnull_session_lock
{
public:
	tnull_session_lock(tflite::tsession& session)
		: session_(session)
		, ok_(false)
	{}

	~tnull_session_lock()
	{
		if (!ok_) {
			session_.reset();
		}
	}
	void set_ok(bool val) { ok_ = val; }

private:
	tflite::tsession& session_;
	bool ok_;
};

// An error reporter that simplify writes the message to stderr.
struct errReporter: public ErrorReporter 
{
	int Report(const char* format, va_list args) override;
};

int errReporter::Report(const char* format, va_list args) 
{
	const int result = vfprintf(stderr, format, args);
	fputc('\n', stderr);
	return result;
}

static bool load_model_internal(const std::string& file, tsession& session, const std::string& sha256)
{
	tnull_session_lock lock(session);

	if (use_tflits) {
		size_t pos = file.rfind(".tflits");
		VALIDATE(pos != std::string::npos && pos + 7 == file.size(), null_str);
	} else {
		size_t pos = file.rfind(".tflite");
		VALIDATE(pos != std::string::npos && pos + 7 == file.size(), null_str);
	}

	session.fp.reset(new tfile(file, GENERIC_READ, OPEN_EXISTING));
	tfile& fp = *session.fp.get();
	VALIDATE(fp.valid(), null_str);
	int64_t fsize = fp.read_2_data();
	int header_size = 0;
	if (use_tflits) {
		header_size = sizeof(ttflits_header);
		if (!decrypt_tflite((uint8_t*)fp.data, fsize, sha256)) {
			return false;
		}
	}

	// errReporter error_reporter;
	// session.model = tflite::FlatBufferModel::BuildFromBuffer((const char*)fp.data + sizeof(ttflits_header), fsize - sizeof(ttflits_header), &error_reporter);
	session.model = tflite::FlatBufferModel::BuildFromBuffer((const char*)fp.data + header_size, fsize - header_size);
	if (!session.model.get()) {
		// Failed to mmap model
		return false;
	}
	// session.model->error_reporter();

	tflite::ops::builtin::BuiltinOpResolver resolver;
	tflite::InterpreterBuilder(*session.model, resolver)(&session.interpreter);
	if (!session.interpreter) {
		// Failed to construct interpreter
		return false;
	}

	// Explicitly resize the input tensor.
	{
	//	int input = session.interpreter->inputs()[0];
	//	std::vector<int> sizes = {1, 224, 224, 3};
	//	session.interpreter->ResizeInputTensor(input, sizes);
	}

	if (session.interpreter->AllocateTensors() != kTfLiteOk) {
		// Failed to allocate tensors!
		return false;
	}

	lock.set_ok(true);
	return true;
}

bool load_model(const std::string& file, tsession& session, const std::string& sha256)
{
	const std::string key = utils::extract_file(file);
	if (key != session.key) {
		uint32_t start_ticks = SDL_GetTicks();
		bool ret = load_model_internal(file, session, sha256);
		SDL_Log("load_model_internal(file: %s, ...) spend %u ms, ret: %s, key: %s", 
			file.c_str(), SDL_GetTicks() - start_ticks, ret? "true": "false", key.c_str());
		if (!ret) {
			session.key.clear();
			return ret;
		}
		session.key = key;
	} else {
		VALIDATE(session.fp.get() && session.interpreter.get(), null_str);
	}
	return true;
}

// Returns the top N confidence values over threshold in the provided vector,
// sorted by confidence in descending order.
static void GetTopN(const uint8_t* prediction, const int prediction_size, const int num_results,
                    const float threshold, std::vector<std::pair<float, int>>* top_results) 
{
	// Will contain top N results in ascending order.
	std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> top_result_pq;

	const long count = prediction_size;
	for (int i = 0; i < count; ++i) {
		const float value = prediction[i] / 255.0;
		// Only add it if it beats the threshold and has a chance at being in
		// the top N.
		if (value < threshold) {
			continue;
		}

		top_result_pq.push(std::pair<float, int>(value, i));

		// If at capacity, kick the smallest value out.
		if ((int)top_result_pq.size() > num_results) {
			top_result_pq.pop();
		}
	}

	// Copy to output vector and reverse into descending order.
	while (!top_result_pq.empty()) {
		top_results->push_back(top_result_pq.top());
		top_result_pq.pop();
	}
	std::reverse(top_results->begin(), top_results->end());
}

uint32_t invoke_classifier(Interpreter& interpreter, const int output_size, const int N, const float kThreshold, std::vector<std::pair<float, int> >& top_results)
{
	uint32_t start = SDL_GetTicks();

	std::stringstream res;
	if (interpreter.Invoke() != kTfLiteOk) {
		// Failed to invoke!
		return nposm;
	}

	top_results.clear();

	uint8_t* output = interpreter.typed_output_tensor<uint8_t>(0);
	GetTopN(output, output_size, N, kThreshold, &top_results);

	return SDL_GetTicks() - start;
}

std::vector<std::string> load_labels_txt(const std::string& path)
{
	std::vector<std::string> labels;
	{
		tfile file(path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), path);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != nposm) {
					labels.push_back(std::string(file.data + start, at - start));
					start = nposm;
				}
			} else if (start == nposm) {
				start = at;
			}
		}
		if (start != nposm) {
			labels.push_back(std::string(file.data + start, fsize - start));
		}
	}
	return labels;
}

static void parse_line(std::string& line, std::set<wchar_t>& labels, std::set<wchar_t>& same)
{
	utils::strip(line);
	int size = line.size();
	const char* c_str = line.c_str();
	if (size == 0 || c_str[0] == '#') {
		return;
	}
	std::vector<std::string> vstr = utils::split(line);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		wchar_t ch = utils::to_int(str);
		if (ch != 0) {
			if (labels.count(ch)) {
				same.insert(ch);
			}
			labels.insert(ch);
		}
	}
}

std::set<wchar_t> load_labels_txt_unicode(const std::string& path)
{
	std::set<wchar_t> labels, same;
	{
		tfile file(path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), path);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != nposm) {
					std::string line(file.data + start, at - start);
					utils::strip(line);
					parse_line(line, labels, same);
					start = nposm;
				}
			} else if (start == nposm) {
				start = at;
			}
		}
		if (start != nposm) {
			std::string line(file.data + start, fsize - start);
			parse_line(line, labels, same);
		}
	}

	VALIDATE(same.empty(), null_str);

	return labels;
}

// src must be tflite require's params: width, height, color.
wchar_t inference_char(tsession& session, const std::string& tflite_file, const std::string& sha256, cv::Mat& src, uint32_t* used_ticks)
{
	bool s = tflite::load_model(tflite_file, session, sha256);
	VALIDATE(s, std::string("err load: ") + tflite_file);

	std::set<wchar_t> labels2 = tflite::load_labels_txt_unicode(utils::extract_directory(tflite_file) + "/" + labels_txt);
	std::vector<wchar_t> labels;
	for (std::set<wchar_t>::const_iterator it = labels2.begin(); it != labels2.end(); ++ it) {
		labels.push_back(*it);
	}
	VALIDATE(!labels.empty(), null_str);

	std::unique_ptr<tflite::Interpreter>& interpreter = session.interpreter;

	int input = interpreter->inputs()[0];
	uint8_t* out = interpreter->typed_tensor<uint8_t>(input);

	const int channels = src.channels();
	for (int y = 0; y < src.rows; ++y) {
		const uint8_t* in_row = src.ptr(y);
		uint8_t* out_row = out + (y * src.cols * channels);
		for (int x = 0; x < src.cols; ++ x) {
			memcpy(out_row, in_row, src.cols * channels);
		}
	}

	std::stringstream res;
	const int N = 3;
	const float kThreshold = 0.1f;
	std::vector<std::pair<float, int> > top_results;

	uint32_t start = SDL_GetTicks();

	wchar_t wch = 0;
	const int loops = 1;
	std::map<wchar_t, int> wchs;
	for (int loop = 0; loop < loops; loop ++) {
		if (interpreter->Invoke() != kTfLiteOk) {
			// Failed to invoke!
			return 0;
		}

		uint8_t* output = interpreter->typed_output_tensor<uint8_t>(0);

		std::map<wchar_t, int>::iterator find;
		const long count = labels.size();
		std::vector<int> values;
		int max_value = INT_MIN;
		for (int i = 0; i < count; ++i) {
			const int value = output[i];
			if (value > max_value) {
				wch = labels[i];
				max_value = value;
			}
			values.push_back(value);
		}
		find = wchs.find(wch);
		if (find != wchs.end()) {
			find->second ++;
		} else {
			wchs.insert(std::make_pair(wch, 1));
		}
	}

	int max_loop = 0;
	for (std::map<wchar_t, int>::const_iterator it = wchs.begin(); it != wchs.end(); ++ it) {
		if (it->second > max_loop) {
			wch = it->first;
			max_loop = it->second;
		}
	}

	uint32_t end = SDL_GetTicks();
	if (used_ticks) {
		*used_ticks = end - start;
	}

	return wch;
}


std::string sha256(const std::string& private_key, const std::string& appid)
{
	VALIDATE(private_key.size() == 16 && appid.size() == 32, null_str);

	const std::string in = private_key + appid;
	uint8_t md[SHA256_DIGEST_LENGTH];
	SHA256((const uint8_t*)in.c_str(), in.size(), md);

	return utils::hex_encode((const char*)md, SHA256_DIGEST_LENGTH);
}

void encrypt_tflite(const std::string& src_path, const std::string& dst_path, const std::string& sha256)
{
	VALIDATE(sha256.size() == SHA256_DIGEST_LENGTH * 2, null_str);

	uint8_t key[SHA256_DIGEST_LENGTH];
	utils::hex_decode((char*)key, SHA256_DIGEST_LENGTH, sha256);

	uint8_t iv[16];
	memset(iv, 0x00, 16);

	tfile src(src_path, GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = src.read_2_data(sizeof(ttflits_header));
	

	const int encrypt_bytes = 512;
	VALIDATE((encrypt_bytes % SHA256_DIGEST_LENGTH == 0) && fsize >= encrypt_bytes, null_str);

	std::unique_ptr<uint8_t[]> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	utils::aes256_encrypt(key, iv, (uint8_t*)src.data + sizeof(ttflits_header), encrypt_bytes, out_ptr);

	SDL_memcpy(src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	ttflits_header header;
	SDL_memset(&header, 0, sizeof(ttflits_header));
	header.timestamp = time(nullptr);
	header.fourcc = SDL_FOURCC('T', 'F', 'S', '1');
	SDL_memcpy(src.data, &header, sizeof(ttflits_header));

	tfile dst(dst_path, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(dst.fp, src.data, sizeof(ttflits_header) + fsize);
}

// @fdata: address of file's first byte.
// @fsize: file size of *.tflits, include header.
bool decrypt_tflite(uint8_t* fdata, int64_t fsize, const std::string& sha256)
{
	VALIDATE(sha256.size() == SHA256_DIGEST_LENGTH * 2, null_str);

	uint8_t key[SHA256_DIGEST_LENGTH];
	utils::hex_decode((char*)key, SHA256_DIGEST_LENGTH, sha256);

	uint8_t iv[16];
	memset(iv, 0x00, 16);

	const int encrypt_bytes = 512;
	VALIDATE(encrypt_bytes % SHA256_DIGEST_LENGTH == 0, null_str);
	if (fsize < sizeof(ttflits_header) + encrypt_bytes) {
		return false;
	}
	ttflits_header header;
	SDL_memcpy(&header, fdata, sizeof(ttflits_header));
	if (header.fourcc != SDL_FOURCC('T', 'F', 'S', '1')) {
		return false;
	}

	std::unique_ptr<uint8_t[]> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	VALIDATE((encrypt_bytes % SHA256_DIGEST_LENGTH == 0) && fsize >= sizeof(ttflits_header) + encrypt_bytes, null_str);
	utils::aes256_decrypt(key, iv, (uint8_t*)fdata + sizeof(ttflits_header), encrypt_bytes, out_ptr);
	SDL_memcpy(fdata + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	return true;
}
*/

int script_parse_source(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.source must not empty.");
		return result;
	}
	if (value == "camera") {
		result = source_camera;

	} else {
		size_t pos = value.rfind(".png");
		if (pos != std::string::npos && pos + 4 == value.size()) {
			if (value.find('\\') == std::string::npos) {
				if (value[0] != '/') {
					result = source_img;

				} else {
					err_report = _("if tflite.source is image, it's first must not be '/'.");
					return result;
				}
			} else {
				err_report = _("if tflite.source is image, must not contain '\\'.");
				return result;
			}
		}
	}

	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.source: ") << value;
		err_report = err.str();
	}
	return result;
}

int script_parse_scenario(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.scenario must not empty.");
		return result;
	}
	if (value == "classifier") {
		result = scenario_classifier;

	} else if (value == "detect") {
		err_report = _("tflite.scenario doesn't support object detect.");
		return result;
		result = scenario_detect;

	} else if (value == "ocr") {
		result = scenario_ocr;

	} else if (value == "pr") {
		result = scenario_pr;
	}
	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.scenario: ") << value;
		err_report = err.str();
	}
	return result;
}

int script_parse_color(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.color must not empty.");
		return result;
	}
	if (value == "gray") {
		result = tflite::color_gray;

	} else if (value == "rgb") {
		result = tflite::color_rgb;

	} else if (value == "bgr") {
		result = tflite::color_bgr;

	}
	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.color: ") << value;
		err_report = err.str();
	}
	return result;
}

bool tscript::from(const config& cfg, std::string& err_report)
{
	err_report.clear();

	tflite = cfg["tflite"].str();
	if (tflite.empty()) {
		err_report = _("tflite.tflite must be set.");
	}

	if (err_report.empty()) {
		source = script_parse_source(cfg["source"].str(), err_report);
		if (source == source_img) {
			img = cfg["source"].str();
		}
	}
	if (err_report.empty()) {
		scenario = script_parse_scenario(cfg["scenario"].str(), err_report);
	}
	if (err_report.empty()) {
		width = cfg["width"].to_int();
		if (width <= 0) {
			err_report = _("tflite.width must be > 0.");
		}
	}
	if (err_report.empty()) {
		height = cfg["height"].to_int();
		if (height <= 0) {
			err_report = _("tflite.height must be > 0.");
		}
	}
	if (err_report.empty()) {
		color = script_parse_color(cfg["color"].str(), err_report);
	}

	if (err_report.empty()) {
		kthreshold = cfg["kthreshold"].to_double(0.1);
		if (kthreshold < 0 || kthreshold > 1) {
			err_report = _("tflite.kthreshold must be (0, 1).");
		}
	}

	if (!err_report.empty()) {
		clear();
	}
	
	warn = cfg["warn"].str();

	return err_report.empty();
}

void tscript::clear()
{
	test = false;
	res = false;
	id.clear();

	tflite.clear();
	warn.clear();
	source = nposm;
	img.clear();
	scenario = nposm;
	width = 0;
	height = 0;
	color = nposm;
	kthreshold = 0;
}

tscript::tscript(const std::string& filename, bool res, std::string* err_report)
	: test(false)
	, res(res)
	, id()
	, source(nposm)
	, scenario(nposm)
	, width(0)
	, height(0)
	, color(nposm)
	, kthreshold(0)
{
	if (filename.empty()) {
		return;
	}

	const size_t pos = filename.rfind(".cfg");
	if (pos == std::string::npos || pos + 4 != filename.size()) {
		return;
	}

	config script_cfg;
	{
		// config_cache& cache = config_cache::instance();
		// cache.get_config(file, script_cfg);

		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0) {
			aplt::read_config(file.data, script_cfg);
		}
	}
	if (script_cfg.empty()) {
		return;
	}
	const config& tflite_cfg = script_cfg.child("script", 0);
	if (!tflite_cfg) {
		return;
	}

	id = utils::extract_file(filename);

	std::string _err_report;
	from(tflite_cfg, _err_report);
	if (err_report != nullptr) {
		*err_report = _err_report;
	}
}

bool ttflite::aes_encrypt(const std::string& key) const
{
/*
	const int key_bytes = 32;
	VALIDATE(valid() && (int)key.size() == key_bytes, null_str);

	tfile src(tflits_path(), GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = src.read_2_data(sizeof(ttflits_header));

	const int encrypt_bytes = 512;
	std::unique_ptr<uint8_t[]> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	VALIDATE((encrypt_bytes % key_bytes == 0) && fsize >= encrypt_bytes, null_str);
	utils::aes256_encrypt((const uint8_t*)key.c_str(), (uint8_t*)src.data + sizeof(ttflits_header), encrypt_bytes, out_ptr);

	SDL_memcpy(src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	ttflits_header header;
	SDL_memset(&header, 0, sizeof(ttflits_header));
	header.timestamp = time(nullptr);
	header.fourcc = SDL_FOURCC('T', 'F', 'S', '1');
	SDL_memcpy(src.data, &header, sizeof(ttflits_header));

	tfile dst(tflits_path(), GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(dst.fp, src.data, sizeof(ttflits_header) + fsize);
*/
	return true;
}

bool ttflite::aes_decrypt(const std::string& key) const
{
/*
	const int key_bytes = 32;
	VALIDATE(valid() && (int)key.size() == key_bytes, null_str);

	tfile src(tflits_path(), GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = src.read_2_data();

	const int encrypt_bytes = 512;
	VALIDATE(encrypt_bytes % key_bytes == 0, null_str);
	if (fsize < sizeof(ttflits_header) + encrypt_bytes) {
		return false;
	}
	ttflits_header header;
	SDL_memcpy(&header, src.data, sizeof(ttflits_header));
	if (header.fourcc != SDL_FOURCC('T', 'F', 'S', '1')) {
		return false;
	}


	std::unique_ptr<uint8_t[]> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	VALIDATE((encrypt_bytes % key_bytes == 0) && fsize >= sizeof(ttflits_header) + encrypt_bytes, null_str);
	utils::aes256_decrypt((uint8_t*)key.c_str(), (uint8_t*)src.data + sizeof(ttflits_header), encrypt_bytes, out_ptr);
	SDL_memcpy(src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	std::string path2 = path() + "/" + id + "_dec.tflite";
	tfile dst(path2, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(dst.fp, src.data + sizeof(ttflits_header), fsize - sizeof(ttflits_header));
*/
	return true;
}

std::string tflite::tresult::to_string() const
{
	std::stringstream res;
	res.precision(3);
	for (std::vector<tresult::titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const tresult::titem& item = *it;
		const float confidence = item.score;
		const int index = item.index;

		res << " - " << confidence << "  ";

		// Write out the result as a string
		// just for safety: theoretically, the output is under 1000 unless there
		// is some numerical issues leading to a wrong prediction.
		res << item.name;

		res << "\n";
	}

	res << " - use " << used_ms << " ms";
	return res.str();
}

//
// tslot
//
static tslot* curr_slot = nullptr;

tslot& get_curr_slot()
{
	VALIDATE(curr_slot != nullptr, null_str);
	return *curr_slot;
}

tslot::tslot()
{
	VALIDATE(curr_slot == nullptr, null_str);
	curr_slot = this;
}

tslot::~tslot()
{
	VALIDATE(curr_slot != nullptr, null_str);
	curr_slot = nullptr;
}

}