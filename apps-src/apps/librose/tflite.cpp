#define GETTEXT_DOMAIN "rose-lib"

#include "sdl_utils.hpp"
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

namespace tflite {

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
/*
	// Explicitly resize the input tensor.
	{
		int input = session.interpreter->inputs()[0];
		std::vector<int> sizes = {1, 224, 224, 3};
		session.interpreter->ResizeInputTensor(input, sizes);
	}
*/
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

/*
// if fail return 0.
wchar_t inference_char(std::pair<std::string, std::unique_ptr<tensorflow::Session> >& current_session, const std::string& pb_path, cv::Mat& src2, uint32_t* used_ticks)
{
	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session);
	if (!s.ok()) {
		std::stringstream err;
		err << "load model fail: " << s;
		return 0;
	}
	std::unique_ptr<tensorflow::Session>& session = current_session.second;

	// Read the label list
	std::vector<std::string> label_strings;
	
	cv::Mat src = get_adaption_ratio_mat(src2, 28, 28);

	int image_width = src.cols;
	int image_height = src.rows;
	int image_channels = 1;
	const int wanted_width = 28;
	const int wanted_height = 28;
	const int wanted_channels = 1;
	const float input_mean = 117.0f;
	const float input_std = 1.0f;
	VALIDATE(image_channels == wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels}));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	float* out = image_tensor_mapped.data();
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* in_row = src.ptr<uint8_t>(row);
		for (int x = 0; x < src.cols; x ++) {
			float* out_pixel = out + row * src.cols;
			if (in_row[x] == 255) {
				out_pixel[x] = 0;
			} else {
				out_pixel[x] = 1;
			}
		}
	}

	uint32_t start = SDL_GetTicks();

	wchar_t wch = 0;
	const int loops = 3;
	std::map<wchar_t, int> wchs;
	for (int loop = 0; loop < loops; loop ++) {
		std::vector<tensorflow::Tensor> outputs;
		tensorflow::Status run_status = session->Run({{"x-input", image_tensor}}, {"layer6-fc2/logit"}, {}, &outputs);
		if (!run_status.ok()) {
			std::stringstream err;
			err << "Running model failed: " << run_status;
			tensorflow::LogAllRegisteredKernels();
			return 0;
		}

		tensorflow::Tensor* output = &outputs[0];
		const Eigen::TensorMap<Eigen::Tensor<float, 1, Eigen::RowMajor>, Eigen::Aligned>& prediction = output->flat<float>();
	
		std::map<wchar_t, int>::iterator find;
		const long count = prediction.size();
		std::vector<float> floats;
		float max_value = INT_MIN;
		for (int i = 0; i < count; ++i) {
			const float value = prediction(i);
			if (value > max_value) {
				wch = '0' + i;
				max_value = value;
			}
			floats.push_back(value);
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
*/

tscript curr_script;

tslot_impl::tslot_impl()
	: script_()
	, tflite_(null_str, false)
{}

void tslot_impl::set_tflite(const tflite::tscript& script, const tflite::ttflite& tflite)
{
	// VALIDATE(taskid_ == nposm, null_str);
	// avcapture_ maybe isn't nullptr, is captureing.
	// VALIDATE(!avcapture_.get(), null_str);
	SDL_Log("%u {set_tflite} called", SDL_GetTicks());

	if (script == script_ && tflite == tflite_) {
		return;
	}

	// threading::lock lock(setting_mutex_);

	script_ = script;
	tflite_ = tflite;
	SDL_Log("%u {set_tflite} end", SDL_GetTicks());

	current_session_.reset();
	load_tflite_mode();
}

void tslot_impl::load_tflite_mode()
{
	std::string file = get_binary_file_location("tflites", tflite_.id + "/" + tflite_.id + ".tflits");
	if (!tflite::use_tflits) {
		file = get_binary_file_location("tflites", tflite_.id + "/" + tflite_.id + ".tflite");
	}
	const std::string tmp_path = tflite_.path();
	VALIDATE(file.find(tflite_.path()) == 0, null_str);

	bool s = tflite::load_model(file, current_session_, tflite::showcase_sha256);
	VALIDATE(s, std::string("err load: ") + file);

	// VALIDATE(avcapture_.get() == nullptr, null_str);
}

std::vector<std::pair<float, SDL_Rect> > tslot_impl::classifier_image(const cv::Mat& argb, tflite::tresult& result)
{
	VALIDATE(!argb.empty(), null_str);
	result.clear();

	std::unique_ptr<tflite::Interpreter>& interpreter = current_session_.interpreter;

	std::vector<std::string> label_strings = tflite::load_labels_txt(tflite_.path() + "/" + tflite::labels_txt);
	VALIDATE(label_strings.size() > 1, null_str);

	// Read the Grace Hopper image.
	cv::Mat src;
	{
		// tsurface_2_mat_lock lock(surf);
		if (script_.color == tflite::color_gray) {
			cv::cvtColor(argb, src, cv::COLOR_BGRA2GRAY);
		} else if (script_.color == tflite::color_bgr) {
			cv::cvtColor(argb, src, cv::COLOR_BGRA2BGR);
		} else if (script_.color == tflite::color_rgb) {
			cv::cvtColor(argb, src, cv::COLOR_BGRA2RGB);
		} else {
			VALIDATE(false, "unknown color format");
		}
	}

	const int image_channels = src.channels();
	const int sourceRowBytes = src.cols * image_channels;

	tpoint ratio_size = calculate_adaption_ratio_size(src.cols, src.rows, script_.width, script_.height);
	int image_width = ratio_size.x;
	int image_height = ratio_size.y;

	int marginX = (src.cols - image_width) / 2;
	int marginY = (src.rows - image_height) / 2;

/*
	if (src.rows <= src.cols) {
		// height <= width
		image_width = image_height = src.rows;
		marginX = (src.cols - image_width) / 2;

	} else {
		image_width = image_height = src.cols;
		marginY = (src.rows - image_width) / 2;
	}
*/
	src = src(cv::Rect(marginX, marginY, image_width, image_height)).clone();

	std::vector<std::pair<float, SDL_Rect> > classifier_rects_;
	{
		// threading::lock variable_lock(variable_mutex_);
		classifier_rects_.clear();
		SDL_Rect rc {marginX, marginY, image_width, image_height};
		classifier_rects_.push_back(std::make_pair((float)0.0, rc));
	}

	const int wanted_input_width = script_.width;
	const int wanted_input_height = script_.height;
	const int wanted_input_channels = script_.color == tflite::color_gray? 1: 3;
	VALIDATE(wanted_input_channels == image_channels, null_str);

	int input = interpreter->inputs()[0];
	uint8_t* out = interpreter->typed_tensor<uint8_t>(input);
/*
	cv::Mat dest;
	cv::resize(src, dest, cvSize(wanted_input_width, wanted_input_height));
	for (int y = 0; y < wanted_input_height; ++y) {
		const uint8_t* in_row = dest.ptr(y);
		uint8_t* out_row = out + (y * wanted_input_width * wanted_input_channels);
		for (int x = 0; x < wanted_input_width; ++ x) {
			memcpy(out_row, in_row, wanted_input_width * wanted_input_channels);
		}
	}
*/

	for (int y = 0; y < wanted_input_height; ++y) {
		const int in_y = (y * image_height) / wanted_input_height;
		const uint8_t* in_row = src.ptr(in_y);
		uint8_t* out_row = out + (y * wanted_input_width * wanted_input_channels);
		for (int x = 0; x < wanted_input_width; ++x) {
			const int in_x = (x * image_width) / wanted_input_width;
			const uint8_t* in_pixel = in_row + (in_x * image_channels);
			uint8_t* out_pixel = out_row + (x * wanted_input_channels);
			for (int c = 0; c < wanted_input_channels; ++c) {
				out_pixel[c] = in_pixel[c];
			}
		}
	}

	std::vector<std::pair<float, int> > top_results;

	uint32_t used_ms = tflite::invoke_classifier(*interpreter, label_strings.size() - 1, 5, script_.kthreshold, top_results);
	if (used_ms == nposm) {
		return classifier_rects_;
	}

	for (const auto& r : top_results) {
		const float confidence = r.first;
		const int index = r.second;

		// Write out the result as a string
		if (index < (int)label_strings.size()) {
			// just for safety: theoretically, the output is under 1000 unless there
			// is some numerical issues leading to a wrong prediction.
			result.items.push_back(tflite::tresult::titem(confidence, index, label_strings[index]));

		} else {
			SDL_Log("classifier_surface, Prediction index: %i score: %.3f, label_strings.size: %i",
				index, confidence, (int)label_strings.size());
		}
	}

	result.used_ms = used_ms;

	return classifier_rects_;
}

std::map<std::string, std::string> treadme::support_keywords;
std::map<std::string, std::string> treadme::support_states;

void treadme::prepare_stats_keywords()
{
	if (support_keywords.empty()) {
		support_keywords.insert(std::make_pair("cnn", _("CNN")));
		support_keywords.insert(std::make_pair("rnn", _("RNN")));
		support_keywords.insert(std::make_pair("mobilenets", _("MobileNets")));
		support_keywords.insert(std::make_pair("inception_v3", _("Inception V3")));
		support_keywords.insert(std::make_pair("inception_v5", _("Inception V5")));
		support_keywords.insert(std::make_pair("vgg", _("VGG")));
		support_keywords.insert(std::make_pair("lstm", _("LSTM")));
		support_keywords.insert(std::make_pair("smart_reply", _("Smart Replay")));
	}

	if (support_states.empty()) {
		support_states.insert(std::make_pair("debugging", _("Debugging")));
		support_states.insert(std::make_pair("training", _("Training")));
		support_states.insert(std::make_pair("commercially", _("Commercially")));
	}
}

const std::string& treadme::keyword_name(const std::string& id)
{
	std::map<std::string, std::string>::const_iterator it = support_keywords.find(id);
	if (it != support_keywords.end()) {
		return it->second;
	}
	return null_str;
}

std::string treadme::keywords_name(const std::vector<std::string>& ids)
{
	std::stringstream ret;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		if (it != ids.begin()) {
			ret << ", ";
		}
		ret << keyword_name(*it);
	}
	return ret.str();
}

const std::string& treadme::state_name(const std::string& id)
{
	std::map<std::string, std::string>::const_iterator it = support_states.find(id);
	if (it != support_states.end()) {
		return it->second;
	}
	return null_str;
}

void tnetwork_item::set_model_special(const std::string& _token, const int64_t _timestamp, const std::string& state_id, const std::string& keywords_id, const std::string& _desc, const bool _showcase, const int _follows, const int _comments)
{
	token = _token;
	timestamp = _timestamp;

	keywords.clear();
	std::vector<std::string> vstr = utils::split(keywords_id);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		if (treadme::support_keywords.count(id)) {
			keywords.push_back(id);
		}
	}

	// state.empty();
	if (treadme::support_states.count(state_id)) {
		state = state_id;
	}

	description = _desc;
	showcase = _showcase;
	follows = _follows;
	comments = _comments;
}

ttoken_tflite::ttoken_tflite(const std::string& token, const int timestamp, const std::string& name, const bool followed, const std::string& nickname, const std::string& keywords_id, const std::string& state_id, const std::string& desc, int follows, int comments, int size)
	: token(token)
	, timestamp(timestamp)
	, name(name)
	, followed(followed)
	, nickname(nickname)
	, description(desc)
	, follows(follows)
	, comments(comments)
	, size(size)
{
	keywords.clear();
	std::vector<std::string> vstr = utils::split(keywords_id);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		if (treadme::support_keywords.count(id)) {
			keywords.push_back(id);
		}
	}

	// state.empty();
	if (treadme::support_states.count(state_id)) {
		state = state_id;
	}
}

const tuser null_user2;
tuser current_user;

bool tuser::valid() const 
{ 
	bool _valid = !sessionid.empty();
	if (_valid) {
		VALIDATE(!mobile.empty(), null_str);
	}
	return _valid;
}

}


namespace preferences {

std::string script()
{
	return preferences::get_str("tflite_script");
}

void set_script(const std::string& value)
{
	preferences::set_str("tflite_script", value);
}

}