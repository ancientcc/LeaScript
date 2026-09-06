#define GETTEXT_DOMAIN "rose-lib"

#include "camera.hpp"
#include "sdl_utils.hpp"
#include "filesystem.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"

#include "config_cache.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "serialization/parser.hpp"

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/string_util.h"

#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/dialog.hpp"

#include <opencv2/imgproc.hpp>

#include "rose_config.hpp"
#include "preferences.hpp"
#include "font.hpp"

void tcamera::tslot::camera_post_enter_task()
{
	if (camera_viewer_ != nullptr) {
		camera_viewer_->camera_post_enter_task_c();
	}
}
void tcamera::tslot::camera_pre_exit_task()
{
	if (camera_viewer_ != nullptr) {
		camera_viewer_->camera_pre_exit_task_c();
	}
}
void tcamera::tslot::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	if (camera_viewer_ != nullptr) {
		camera_viewer_->camera_did_draw_slice_c(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
	}
}

SDL_Point tcamera::tslot::camera_did_video_align()
{
	if (camera_viewer_ != nullptr) {
		return camera_viewer_->camera_did_video_align_c();
	}

	return SDL_Point{halign_center, valign_center};
}

void tcamera::tslot::camera_did_button_clicked(int msgid)
{
	if (camera_viewer_ != nullptr) {
		camera_viewer_->camera_did_button_clicked_c(msgid);
	}
}

void tcamera::tslot::set_camera_viewer(tcamera::tviewer* viewer)
{
	if (viewer != nullptr) {
		VALIDATE(camera_viewer_ == nullptr, null_str);
	} else {
		VALIDATE(camera_viewer_ != nullptr, null_str);
	}
	camera_viewer_ = viewer;
}

void tcamera::load_tflite_mode()
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

static bool scene_exiting = false; 
void tcamera::DoWork()
{
	DoWork_running_ = true;
	while (avcapture_.get()) {
		if (setting_) {
			// main thread is setting.
			// seting action may be risk setting_mutex_.
			SDL_Delay(10);
			continue;
		}
		if (scene_exiting) {
			SDL_Log("%u {DoWork}(0.0)scene_exiting is true, taskid_: %i", SDL_GetTicks(), taskid_);
		}

		threading::lock lock(setting_mutex_);
		if (!avcapture_.get()) {
			continue;
		}
		if (!current_surf_.first) {
			SDL_Delay(10);
			continue;
		}

		if (scene_exiting) {
			SDL_Log("%u {DoWork}(1)scene_exiting is true, taskid_: %i", SDL_GetTicks(), taskid_);
		}
		if (taskid_ == nposm) {
			current_surf_.first = false;
			SDL_Delay(10);
			continue;
		}

		if (is_use_tflite()) {
			if (script_.scenario == tflite::scenario_classifier) {
				VALIDATE(current_session_.valid(), null_str);

				tflite::tresult result;
				classifier_surface(current_surf_.second, result);
				current_surf_.first = false;
				{
					threading::lock lock(variable_mutex_);
					result_ = result;
				}

			} else if (script_.scenario == tflite::scenario_detect) {
				VALIDATE(false, "Not supported");
				VALIDATE(current_session_.valid(), null_str);

				example_detector_internal(current_surf_.second);
				current_surf_.first = false;
				{
					threading::lock lock(variable_mutex_);
					// result_ = result;
				}

			} else if (script_.scenario == tflite::scenario_pr) {
				VALIDATE(false, "don't support");

			} else {
				// SDL_Delay(10);
				VALIDATE(false, "unknown script_.scenario");
			}
		}
		
		if (slot_ != nullptr) {
			slot_->camera_work_frame(current_surf_.second);
			current_surf_.first = false;

		} else {
			current_surf_.first = false;
			SDL_Delay(10);
		}
	}
	DoWork_running_ = false;
}

void tcamera::OnWorkStart()
{
}

void tcamera::OnWorkDone()
{
}

void tcamera::classifier_surface(const surface& surf, tflite::tresult& result)
{
	VALIDATE(surf, null_str);
	result.clear();

	std::unique_ptr<tflite::Interpreter>& interpreter = current_session_.interpreter;

	std::vector<std::string> label_strings = tflite::load_labels_txt(tflite_.path() + "/" + tflite::labels_txt);
	VALIDATE(label_strings.size() > 1, null_str);

	// Read the Grace Hopper image.
	cv::Mat src;
	{
		tsurface_2_mat_lock lock(surf);
		if (script_.color == tflite::color_gray) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2GRAY);
		} else if (script_.color == tflite::color_bgr) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2BGR);
		} else if (script_.color == tflite::color_rgb) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2RGB);
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

	{
		threading::lock variable_lock(variable_mutex_);
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

	uint32_t used_ms = tflite::invoke_classifier(*interpreter, (int)label_strings.size() - 1, 5, script_.kthreshold, top_results);
	if (used_ms == nposm) {
		return;
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
}
/*
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
*/
// Converts an encoded location to an actual box placement with the provided
// box priors.
static void DecodeLocation(const float* encoded_location, const float* box_priors, float* decoded_location) 
{
	bool non_zero = false;
	for (int i = 0; i < 4; ++i) {
		const float curr_encoding = encoded_location[i];
		non_zero = non_zero || curr_encoding != 0.0f;

		const float mean = box_priors[i * 2];
		const float std_dev = box_priors[i * 2 + 1];

		float currentLocation = curr_encoding * std_dev + mean;

		currentLocation = std::max(currentLocation, 0.0f);
		currentLocation = std::min(currentLocation, 1.0f);
		decoded_location[i] = currentLocation;
	}
}

static float DecodeScore(float encoded_score) 
{ 
	return 1 / (1 + exp(-encoded_score)); 
}

static void cv_draw_rectangle(cv::RNG& rng, const cv::Mat& img, const cv::Rect& rect)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255), 255));
}

static void cv_draw_rectangle2(const cv::Mat& img, const cv::Rect& rect, const uint32_t color)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(color & 0xff, (color & 0xff00) >> 8, (color & 0xff0000) >> 16, (color & 0xff000000) >> 24));
}

void tcamera::example_detector_internal(surface& surf)
{
	VALIDATE(false, "Now not support");
/*
	std::stringstream result;

	const std::string data_path = game_config::app_dir_root + "/tensorflow";
	const std::string pb_path = data_path + "/mobile_multibox_v1a/multibox_model.pb";

	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session_);
	if (!s.ok()) {
		result << "load model fail: " << s;
		return result.str();
	}
	std::unique_ptr<tensorflow::Session>& session = current_session_.second;

	int32_t num_detections = 5;
	int32_t num_boxes = 784;
	if (locations_.empty()) {
		const std::string imagenet_comp_path = data_path + "/mobile_multibox_v1a/multibox_location_priors.txt";

		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != nposm) {
					std::string line(file.data + start, at - start);
					std::vector<float> tokens;
					CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
					for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
						locations_.push_back(*it);
					}

					start = nposm;
				}
			} else if (start == nposm) {
				start = at;
			}
		}
		if (start != nposm) {
			std::string line(file.data + start, fsize - start);
			std::vector<float> tokens;
			CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
			for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
				locations_.push_back(*it);
			}
		}
	}
	CHECK_EQ(locations_.size(), num_boxes * 8);

	// Read the Grace Hopper image.
	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGRA2RGBA);
	}

	int image_width = surf->w;
	int image_height = surf->h;
	int image_channels = 4;
	const int wanted_width = 224;
	const int wanted_height = 224;
	const int wanted_channels = 3;
	const float input_mean = 128.0f;
	const float input_std = 128.0f;
	VALIDATE(image_channels >= wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels }));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	surface_lock dst_lock(surf);
	tensorflow::uint8* in = (uint8_t*)(dst_lock.pixels());
	// tensorflow::uint8* in_end = (in + (image_height * image_width * image_channels));
	float* out = image_tensor_mapped.data();
	for (int y = 0; y < wanted_height; ++y) {
		const int in_y = (y * image_height) / wanted_height;
		tensorflow::uint8* in_row = in + (in_y * image_width * image_channels);
		float* out_row = out + (y * wanted_width * wanted_channels);
		for (int x = 0; x < wanted_width; ++x) {
			const int in_x = (x * image_width) / wanted_width;
			tensorflow::uint8* in_pixel = in_row + (in_x * image_channels);
			float* out_pixel = out_row + (x * wanted_channels);
			for (int c = 0; c < wanted_channels; ++c) {
				out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
	}

	result << " - " << locations_.size() << ", " << image_width << "x" << image_height;

	uint32_t start = SDL_GetTicks();

	std::string input_layer = "ResizeBilinear";
	std::string output_score_layer = "output_scores/Reshape";
	std::string output_location_layer = "output_locations/Reshape";
	std::vector<tensorflow::Tensor> outputs;
	tensorflow::Status run_status = session->Run({ {input_layer, image_tensor} },
		{output_score_layer, output_location_layer}, {}, &outputs);

	if (!run_status.ok()) {
		result << "Running model failed: " << run_status;
		tensorflow::LogAllRegisteredKernels();
		return result.str();
	}

	uint32_t end = SDL_GetTicks();

	tensorflow::string status_string = run_status.ToString();
	result << " - " << status_string << "\n";

	tensorflow::Tensor& scores = outputs[0];
	const tensorflow::TTypes<float>::Flat scores_flat = scores.flat<float>();
	const int how_many_labels = scores_flat.size();

	const tensorflow::Tensor& encoded_locations = outputs[1];
	auto locations_encoded = encoded_locations.flat<float>();

	tsurface_2_mat_lock lock(surf);
	cv::cvtColor(lock.mat, lock.mat, cv::COLOR_RGBA2BGRA);

	threading::lock variable_lock(variable_mutex_);
	classifier_rects_.clear();
	const float kThreshold = 0.1f;
	for (int pos = 0; pos < how_many_labels; ++pos) {
		const float score = DecodeScore(scores_flat(pos));

		if (score < kThreshold) {
			continue;
		}

		float decoded_location[4];
		DecodeLocation(&locations_encoded(pos * 4), &locations_[pos * 8], decoded_location);

		float left = decoded_location[0] * image_width;
		float top = decoded_location[1] * image_height;
		float right = decoded_location[2] * image_width;
		float bottom = decoded_location[3] * image_height;

		classifier_rects_.push_back(std::make_pair(score, SDL_Rect({(int)left, (int)top, (int)(right - left), (int)(bottom - top)})));
	}

	result << " - use " << end - start << " ms";

	return result.str();
*/
}

tcamera::tcamera(const int cameraid, rtc::MessageHandler& dlg_handler, int expired_threshold)
	: cameraid_(cameraid)
	, dlg_handler_(dlg_handler)
	// default: 30 second
	, expired_threshold_(expired_threshold == nposm? 30 * 1000: expired_threshold)
	, slot_(nullptr)
	, script_()
	, tflite_(null_str, false)
	, cancel_rect_(empty_rect)
	, switch_camera_rect_(empty_rect)
	, posting_msg_id_(nposm)
	, depth_slot_(nullptr)
	, taskid_(nposm)
	, flags_(0)
	, render_overlay_texs_ticks_(0)
	, tasking_check_singal_threshold_(10000) // 10 second
	, idle_check_singal_threshold_(1000) // 1 second
	, frames_x_sec_ago_(0)
	, check_nosignal_ticks_(0)
	, expired_ticks_(0)
	, DoWork_running_(false)
	, current_surf_(std::make_pair(false, surface()))
	, setting_(false)
	, next_recognize_frame_(0)
	, draw_interval_(30) // 30ms
	, next_draw_ticks_(0)
	, score_threshold_(0.8f)
	, last_coordinate_(construct_null_coordinate())
{
	// [0, 2min]
	VALIDATE(expired_threshold_ >= 0 && expired_threshold_ <= 120 * 1000, null_str);

}

void tcamera::set_depth_slot(tdepth_slot* depth_slot)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(avcapture_.get() == nullptr, null_str);

	if (depth_slot != nullptr) {
		VALIDATE(depth_slot_ == nullptr, null_str);

	} else {
		VALIDATE(depth_slot_ != nullptr, null_str);
	}

	depth_slot_ = depth_slot;
}

void tcamera::snapshot_depth(bool use_dcpitch, const std::string& png, const std::string& depth_data_file)
{
	VALIDATE(!png.empty(), null_str);
	VALIDATE(avcapture_.get() != nullptr, null_str);
	avcapture_->snapshot_depth(use_dcpitch, png, depth_data_file);
}

void tcamera::set_slot(tslot* slot)
{
	VALIDATE_IN_MAIN_THREAD();

	tsetting_lock setting_lock(*this);
	threading::lock lock(setting_mutex_);

	if (slot != nullptr) {
		VALIDATE(slot_ == nullptr, null_str);
		VALIDATE(taskid_ == nposm, null_str);

	} else {
		VALIDATE(slot_ != nullptr, null_str);
		// must call @exit_task() before @set_slot(nullptr).
		VALIDATE(taskid_ == nposm, null_str);

	}
	slot_ = slot;
}

void tcamera::set_flags(uint32_t flags)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(taskid_ != nposm, null_str);

	flags_ = flags;
}

void tcamera::set_tflite(const tflite::tscript& script, const tflite::ttflite& tflite)
{
	VALIDATE(taskid_ == nposm, null_str);
	// avcapture_ maybe isn't nullptr, is captureing.
	// VALIDATE(!avcapture_.get(), null_str);
	SDL_Log("%u {set_tflite} called", SDL_GetTicks());

	// if (script == script_ && tflite == tflite_) {
	//	return;
	// }

	// threading::lock lock(setting_mutex_);

	current_session_.reset();

	script_ = script;
	tflite_ = tflite;
	SDL_Log("%u {set_tflite} end", SDL_GetTicks());
}

void tcamera::enter_task(int taskid, uint32_t flags, bool no_swap_wh_for_screen)
{
	VALIDATE(taskid_ == nposm, null_str);

	SDL_Log("%u, {enter_task}lock(setting_mutex_) pre", SDL_GetTicks());

	tsetting_lock setting_lock(*this);
	threading::lock lock(setting_mutex_);

	SDL_Log("%u, {enter_task}lock(setting_mutex_) post", SDL_GetTicks());

	if (is_use_tflite(flags) && !current_session_.valid()) {
		load_tflite_mode();
	}

	if (avcapture_.get() != nullptr) {
		// make expired() return false always.
		clear_result();
		frames_x_sec_ago_ = 0;
		check_nosignal_ticks_ = 0;
		expired_ticks_ = 0;
		taskid_ = taskid;
		flags_ = flags;

		// sequence: exit_task --> (idle time, but avcapture is ruuning) --> enter_task
		// some device maybe plug in or out during 'idle time'.
		
		// refresh existing_cameras_ for whether display 'switch_camera' or not.
		existing_cameras_ = avcapture_->recalculate_existing_cameras();

	} else {
		VALIDATE(expired_ticks_ == 0, null_str);
		VALIDATE(frames_x_sec_ago_ == 0 && check_nosignal_ticks_ == 0, null_str);
		taskid_ = taskid;
		flags_ = flags;
		start_avcapture(no_swap_wh_for_screen);
	}

	const int render_overlay_texts_threshold = 2000;
	render_overlay_texs_ticks_ = SDL_GetTicks() + render_overlay_texts_threshold;

	if (avcapture_->using_vidcap_count() > 0) {
		VALIDATE(frames_x_sec_ago_ == 0, null_str);
		check_nosignal_ticks_ = SDL_GetTicks() + tasking_check_singal_threshold_;
	}

	if (slot_ != nullptr) {
		slot_->camera_post_enter_task();
	}
}

void tcamera::exit_task(int taskid)
{
	VALIDATE(taskid_ != nposm, null_str);
	VALIDATE(taskid_ == taskid, null_str);
	VALIDATE(avcapture_.get() != nullptr, null_str);

	SDL_Log("%u, {exit_task}lock(setting_mutex_) pre", SDL_GetTicks());
	scene_exiting = true;
	tsetting_lock setting_lock(*this);
	threading::lock lock(setting_mutex_);
	scene_exiting = false;
	SDL_Log("%u, {exit_task}lock(setting_mutex_) post", SDL_GetTicks());

	if (slot_ != nullptr) {
		slot_->camera_pre_exit_task();
	}

	taskid_ = nposm;
	expired_ticks_ = SDL_GetTicks() + expired_threshold_;

	if (avcapture_->using_vidcap_count() > 0) {
		trtc_client::VideoRenderer* sink = avcapture_->vrenderer(false, 0);
		frames_x_sec_ago_ = sink->frame_thread_frames;
		check_nosignal_ticks_ = SDL_GetTicks() + idle_check_singal_threshold_;

	} else {
		VALIDATE(frames_x_sec_ago_ == 0 && check_nosignal_ticks_ == 0, null_str);
	}

	//
	cancel_rect_ = empty_rect;
	switch_camera_rect_ = empty_rect;

	if (expired_threshold_ == 0 || avcapture_->using_vidcap_count() == 0) {
		stop_avcapture();
	}
}

void tcamera::main_OnFrame_when_idle()
{
	if (taskid_ != nposm) {
		return;
	}
	trtc_client* avcapture = avcapture_.get();
	if (avcapture != nullptr) {
		avcapture->main_OnFrame();
	}
}

// if has signal, check every @xxx_check_singal_threshold, else check every time.
bool tcamera::nosignal() 
{
	if (check_nosignal_ticks_ == 0 || SDL_GetTicks() < check_nosignal_ticks_) {
		return false;
	}

	trtc_client::VideoRenderer* sink = avcapture_->vrenderer(false, 0);
	if (frames_x_sec_ago_ == sink->frame_thread_frames) {
		return true;
	}

	frames_x_sec_ago_ = sink->frame_thread_frames;
	// why when tasking() '/2'?
	//  -- in order to reduce first time of find nosignal.
	//     if not '/2', first time maybe max to (2*tasking_check_singal_threshold_) in theoretically.
	const int threshold = tasking()? (tasking_check_singal_threshold_ / 2): idle_check_singal_threshold_;
	check_nosignal_ticks_ = SDL_GetTicks() + threshold;
	return false;
}

void tcamera::start_avcapture(bool no_swap_wh_for_screen)
{
	VALIDATE(!avcapture_.get(), null_str);

	if (is_use_tflite()) {
		VALIDATE(current_session_.valid(), null_str);
	}

	tsetting_lock setting_lock(*this);
	threading::lock lock(setting_mutex_);

	// int video_width = game_config::os == os_windows? 640: 1280;
	int video_width = 1280;

	trtc_client* avcapture = nullptr;
	if (depth_slot_ != nullptr) {
		avcapture = depth_slot_->camera_create_avcapture(cameraid_, dlg_handler_, *this, tpoint(video_width, nposm));
		VALIDATE(avcapture != nullptr, null_str);
	}
	if (avcapture == nullptr) {
		avcapture_.reset(new tavcapture(cameraid_, dlg_handler_, *this, tpoint(video_width, nposm), nposm, true, no_swap_wh_for_screen));

	} else {
		avcapture_.reset(avcapture);
	}

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	classifier_rects_.clear();
	next_draw_ticks_ = SDL_GetTicks();

	load_overlay_texs();

	executor_.reset(new net::tworker(std::bind(&tcamera::DoWork, this), std::bind(&tcamera::OnWorkStart, this),
		std::bind(&tcamera::OnWorkDone, this), NULL, "BaseCameraThread"));
}

void tcamera::stop_avcapture()
{
	VALIDATE(taskid_ == nposm, null_str);
	tsetting_lock setting_lock(*this);
	threading::lock lock(setting_mutex_);

	avcapture_.reset();

	frames_x_sec_ago_ = 0;
	check_nosignal_ticks_ = 0;
	expired_ticks_ = 0;

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	classifier_rects_.clear();
	release_overlay_texs();

	executor_.reset();
}

void tcamera::deliver_frame_to_worker(trtc_client::VideoRenderer& vsink)
{
	if (!vsink.new_frame) {
		return;
	}

	// must judge current_surf_.first before slot_->keep_DoWork_frame_.
	if (current_surf_.first) {
		return;
	}

	if (slot_ != nullptr && slot_->keep_DoWork_frame_) {
		SDL_Log("%u because keep_DoWork_frame_ is true, don't deliver frame to worker", SDL_GetTicks());
		return;
	}
	VALIDATE(!current_surf_.first, null_str);

	ttexture_2_mat_lock mat_lock(vsink.tex_);
	cv::Mat& frame2 = mat_lock.mat;

	const cv::Mat* frame_ptr = vsink.desire_deliver_frame();
	if (frame_ptr == nullptr) {
		frame_ptr = &frame2;
	}

	const cv::Mat& frame = *frame_ptr;

	const int max_value = SDL_max(frame.cols, frame.rows);
	bool require_copy = true;
	if (require_copy) {
		if (current_surf_.second.get() == nullptr) {
			current_surf_.second = create_neutral_surface(frame.cols, frame.rows);
		}
		memcpy(current_surf_.second->pixels, frame.data, frame.cols * frame.rows * 4); 
		current_surf_.first = true;

		const int recognize_threshold = 5;
		next_recognize_frame_ = vsink.frames + recognize_threshold;
	}
}

void tcamera::switch_camera(int id)
{
	VALIDATE(avcapture_.get() != nullptr, null_str);
	VALIDATE(can_switch_camera(), null_str);

	const int original_taskid = taskid_;
	const bool original_no_swap_wh_for_screen = avcapture_->no_swap_wh_for_screen();
	// tslot* original_temp_slot = slot_ != nullptr && slot_->temporary? slot_: nullptr;
	tslot* original_temp_slot = nullptr;
	if (original_taskid != nposm) {
		exit_task(taskid_);
	} else {
		VALIDATE(original_temp_slot == nullptr, null_str);
	}
	stop_avcapture();

	int at = 0;
	for (std::vector<std::string>::const_iterator it = existing_cameras_.begin(); it != existing_cameras_.end(); ++ it, at ++) {
		const std::string& name = *it;
		if (name == preferences::usingcamera()) {
			break;
		}
	}
	if (at == existing_cameras_.size()) {
		at = 0;
	}

	// next camera
	at ++;
	if (at == existing_cameras_.size()) {
		at = 0;
	}
	preferences::set_usingcamera(existing_cameras_[at]);
		
	start_avcapture(original_no_swap_wh_for_screen);
	if (original_taskid != nposm) {
		enter_task(original_taskid, flags_, original_no_swap_wh_for_screen);

		if (original_temp_slot != nullptr) {
			set_slot(original_temp_slot);
		}
	}

	if (slot_ != nullptr) {
		slot_->camera_post_switch_camera();
	}
}

void tcamera::load_overlay_texs()
{
	VALIDATE(cancel_tex_.get() == nullptr, null_str);

	// back_solid.png
	surface surf = image::get_image("misc/back_solid.png");
	VALIDATE(surf.get(), null_str);
	cancel_tex_ = SDL_CreateTextureFromSurface2(get_renderer(), surf);

	VALIDATE(switch_camera_tex_.get() == nullptr, null_str);

	// switch-camera.png
	surf = image::get_image("misc/switch_camera.png");
	VALIDATE(surf.get(), null_str);
	switch_camera_tex_ = SDL_CreateTextureFromSurface2(get_renderer(), surf);
}

void tcamera::release_overlay_texs()
{
	cancel_tex_ = nullptr;
	cancel_rect_ = empty_rect;

	switch_camera_tex_ = nullptr;
	switch_camera_rect_ = empty_rect;
}

#define DEFAULT_IMG_DOT			36
void tcamera::calculate_overlay_rects(const SDL_Rect& video_dst)
{
	const int x_gap = 8 * gui2::twidget::hdpi_scale;
	const int y_gap = 8 * gui2::twidget::hdpi_scale;

	const int png_width = DEFAULT_IMG_DOT * gui2::twidget::hdpi_scale;
	const int png_height = DEFAULT_IMG_DOT * gui2::twidget::hdpi_scale;
	if ((flags_ & FLAG_SHOW_CANCEL) && cancel_tex_.get() != nullptr) {
		int width, height;
		SDL_QueryTexture(cancel_tex_.get(), nullptr, nullptr, &width, &height);
		width = png_width;
		height = png_height;
		cancel_rect_ = ::create_rect(video_dst.x + x_gap, video_dst.y + y_gap, width, height);

	} else {
		cancel_rect_ = empty_rect;
	}

	if (!(flags_ & FLAG_HIDE_SWITCH) && can_switch_camera() && switch_camera_tex_.get() != nullptr) {
		int width, height;
		SDL_QueryTexture(switch_camera_tex_.get(), nullptr, nullptr, &width, &height);
		width = png_width;
		height = png_height;
		switch_camera_rect_ = ::create_rect(video_dst.x + video_dst.w - width - x_gap, video_dst.y + video_dst.h - height - y_gap, width, height);

	} else {
		switch_camera_rect_ = empty_rect;
	}
}

void tcamera::render_overlay_texs(SDL_Renderer* renderer, const SDL_Rect& video_dst)
{
	if (SDL_GetTicks() < render_overlay_texs_ticks_) {
		return;
	}

	calculate_overlay_rects(video_dst);

	if (!SDL_RectEmpty(&cancel_rect_)) {
		SDL_RenderCopy(renderer, cancel_tex_.get(), nullptr, &cancel_rect_);
	}

	if (!SDL_RectEmpty(&switch_camera_rect_)) {
		SDL_RenderCopy(renderer, switch_camera_tex_.get(), nullptr, &switch_camera_rect_);
	}
}

void tcamera::render_classifier_result(SDL_Renderer* renderer, const cv::Mat& frame, const SDL_Rect& video_dst, bool use_no_swap_wh_tex)
{
	// if ((flags_ & FLAG_USE_TFLITE) == 0) {
		// Although the scene has FLAG_USE_TFLITE, it may reach here before call enter_task(FLAG_USE_TFLITE). 
		// call enter_task() is in tdialog::app_first_drawn, but may reach here before it.
	//	return;
	// }

	std::vector<std::pair<float, SDL_Rect> > rects;
	std::string result_str;
	{
		threading::lock lock(get_variable_mutex());
		rects = classifier_rects_;
		const tflite::tresult& result = get_result();
		if (!result.items.empty()) {
			result_str = result.to_string();
		} else {
			result_str = result_str_;
		}
	}

	surface text_surf;
	// char score_str[32];
	const double width_ratio = 1.0 * video_dst.w / frame.cols;
	const double height_ratio = 1.0 * video_dst.h / frame.rows;
	for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		float score = it->first;
		const SDL_Rect& rect = it->second;

		render_rect_frame(renderer, SDL_Rect{(int)(video_dst.x + rect.x * width_ratio), int(video_dst.y + rect.y * height_ratio), 
			int(rect.w * width_ratio), int(rect.h * height_ratio)}, 0xffff0000, 1);

		if (score != float_nposm) {
			char score_c_str[20];
			// SDL_snprintf(score_c_str, sizeof(score_c_str), "#%i(%.1f%%, %.1f%%)", index, face.score * 100, face.quality * 100);
			SDL_snprintf(score_c_str, sizeof(score_c_str), "(%.1f%%)", score * 100);

			text_surf = font::get_rendered_text(score_c_str, 0, font::SIZE_SMALL, font::GOOD_COLOR);
			SDL_Rect dstrect = create_rect(video_dst.x + rect.x * width_ratio, video_dst.y + rect.y * height_ratio, text_surf->w, text_surf->h);
			dstrect.w = text_surf->w;
			dstrect.h = text_surf->h;
			render_surface(renderer, text_surf, NULL, &dstrect);
		}
	}

	if (!result_str.empty()) {
		int font_size = font::SIZE_LARGEST; // font::SIZE_LARGE
		text_surf = font::get_rendered_text(result_str, 0, font_size, font::GOOD_COLOR);
		SDL_Rect dstrect = video_dst;
		if (use_no_swap_wh_tex) {
			text_surf = rotate_surface(text_surf, -90, nullptr, 0);
			dstrect.x += video_dst.w - text_surf->w;
		}
		dstrect.w = text_surf->w;
		dstrect.h = text_surf->h;
		render_surface(renderer, text_surf, NULL, &dstrect);
	}
}

SDL_Rect tcamera::camera_did_draw_slice_use_cv_frame(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	trtc_client::VideoRenderer& vsink = *locals[0];
	ttexture_2_mat_lock mat_lock(vsink.tex_);
	cv::Mat& frame = mat_lock.mat;

	ttexture_2_mat_lock cv_mat_lock(vsink.cv_tex_);
	cv::Mat& cv_frame = cv_mat_lock.mat;

	cv::Mat no_swap_wh_frame;

	cv::Mat* using_frame = &frame;
	texture* using_tex_ = &vsink.tex_;

	VALIDATE(slot_ != nullptr, null_str);
	bool use_cv_frame = slot_->camera_use_cv_frame(frame, cv_frame);
/*
	if (use_cv_frame) {
		VALIDATE(frame.cols == cv_frame.cols && frame.rows == cv_frame.rows && frame.channels() == cv_frame.channels(), null_str);
		if (cv_frame.u != nullptr) {
			uint8_t* pixels = nullptr;
			int pitch = 0;
			SDL_LockTexture(vsink.cv_tex_.get(), NULL, (void**)&pixels, &pitch);
			memcpy(pixels, cv_frame.data, cv_frame.cols * cv_frame.rows * frame.channels());
		}

		SDL_UnlockTexture(vsink.cv_tex_.get());
		using_tex_ = &vsink.cv_tex_;

	}
*/

	bool use_no_swap_wh_tex = false;
	if (use_cv_frame) {
		VALIDATE(frame.cols == cv_frame.cols && frame.rows == cv_frame.rows && frame.channels() == cv_frame.channels(), null_str);
		// if (!avcapture_->no_swap_wh_for_screen() || vsink.rotation_ != 90) {
		use_no_swap_wh_tex = vsink.use_no_swap_wh_tex();
		if (!use_no_swap_wh_tex) {
			if (cv_frame.u != nullptr) {
				uint8_t* pixels = nullptr;
				int pitch = 0;
				SDL_LockTexture(vsink.cv_tex_.get(), NULL, (void**)&pixels, &pitch);
				memcpy(pixels, cv_frame.data, cv_frame.cols * cv_frame.rows * frame.channels());
			}

			SDL_UnlockTexture(vsink.cv_tex_.get());
			using_frame = &cv_frame;
			using_tex_ = &vsink.cv_tex_;

		} else {
			cv::Mat& frame2 = no_swap_wh_frame;
			vsink.fill_no_swap_wh_tex(cv_frame, frame2);
			// imwrite(frame2, "1.png");
			// ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame2.cols, frame2.rows);
			// video_dst = SDL_Rect{draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
			// SDL_RenderCopy(renderer, vsink.no_swap_wh_tex_.get(), nullptr, &video_dst);

			using_frame = &frame2;
			using_tex_ = &vsink.no_swap_wh_tex_;		
		}

	}

	const tpoint ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, using_frame->cols, using_frame->rows);
	SDL_Rect video_dst {draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
	// if (use_no_swap_wh_tex) {
	SDL_Point align = slot_->camera_did_video_align();
	VALIDATE(align.x == halign_center, null_str);
	if (align.y == valign_top) {
		video_dst.y = draw_rect.y;

	} else if (align.y == valign_bottm) {
		// To facilitate vlog cropping, leave a blank area only on one side (the top).
		video_dst.y = draw_rect.y + (draw_rect.h - ratio_size.y);
	}

	SDL_RenderCopy(renderer, using_tex_->get(), nullptr, &video_dst);

	render_overlay_texs(renderer, video_dst);
	render_classifier_result(renderer, *using_frame, video_dst, use_no_swap_wh_tex);
	return video_dst;
}

std::vector<trtc_client::tusing_vidcap> tcamera::app_video_capturer(int id, bool remote, const std::vector<std::string>& device_names)
{
	VALIDATE(!remote, null_str);

	std::string hit;

	std::vector<trtc_client::tusing_vidcap> ret;
	for (std::vector<std::string>::const_iterator it = device_names.begin(); it != device_names.end(); ++ it) {
		const std::string& name = *it;
		if (name == preferences::usingcamera()) {
			hit = name;
			break;
		}
	}

	if (!device_names.empty()) {
		if (hit.empty()) {
			hit = device_names.front();
		}
		ret.push_back(trtc_client::tusing_vidcap(hit, false));
		preferences::set_usingcamera(hit);
	}

	existing_cameras_ = device_names;
	return ret;
}

void tcamera::did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	if (slot_ != nullptr) {
		slot_->camera_did_draw_slice(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
		// return;
	}

	trtc_client::VideoRenderer& vsink = *locals[0];
	deliver_frame_to_worker(vsink);
}

trtc_client::VideoRenderer* tcamera::app_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track, const std::string& name, bool remote, int at, bool encode)
{
	trtc_client::VideoRenderer* result = nullptr;
	if (depth_slot_ != nullptr) {
		result = depth_slot_->camera_create_video_renderer(client, track, name, remote, at, encode);
		VALIDATE(result != nullptr, null_str);
	}
	if (result == nullptr) {
		result = new trtc_client::VideoRenderer(client, track, name, remote, at, encode);
	}
	return result;
}

void tcamera::clear_result()
{
	threading::lock lock(variable_mutex_);
	result_.clear();
}

void tcamera::set_aplt_overlay(const std::string& result_str, const std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	result_str_ = result_str;
	classifier_rects_ = classifier_rects;
}

void tcamera::slice(const SDL_Rect& draw_rect, bool immediately)
{
	SDL_Renderer* renderer = get_renderer();

	bool no_frame = true;
	if (avcapture_.get() != nullptr && avcapture_->using_vidcap_count() > 0) {
		trtc_client& avcapture = *avcapture_.get();
		trtc_client::VideoRenderer* sink = avcapture.vrenderer(false, 0);

		// for depth camera.
		avcapture.main_OnFrame();

		if (sink->frame_thread_frames != 0) {
			uint32_t now = SDL_GetTicks();
			if (immediately) {
				// some scene, it has gui-timer, for example ttrack.
				avcapture.draw_slice(renderer, draw_rect);

			} else if (now >= next_draw_ticks_) {
				// use tcamera's timer.
				avcapture.draw_slice(renderer, draw_rect);
				next_draw_ticks_ += draw_interval_;

			}
			no_frame = false;

		}
	}

	if (no_frame) {
		if (!SDL_RectEmpty(&draw_rect)) {
			render_overlay_texs(renderer, draw_rect);
		}
	}
}

void tcamera::did_left_button_down_paper(gui2::ttrack& widget, const tpoint& coordinate)
{
	last_coordinate_ = coordinate;
}

void tcamera::did_mouse_motion_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	last_coordinate_ = last;
}

bool tcamera::did_mouse_leave_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return false;
	}

	if (posting_msg_id_ != nposm) {
		return true;
	}

	if (avcapture_.get()) {
		int msg_id = nposm;
		if (point_in_rect(first.x, first.y, cancel_rect_)) {
			msg_id = gui2::tdialog::POST_MSG_CANCEL_CAMERA;

		} else if (point_in_rect(first.x, first.y, switch_camera_rect_)) {
			msg_id = gui2::tdialog::POST_MSG_SWITCH_CAMERA;
		}

		if (msg_id != nposm) {
			threading::lock lock(variable_mutex_); // avoid variable: camera_feature_
			tmsg_data_base_camera* pdata = new tmsg_data_base_camera(*this, cameraid_base, msg_id);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, &dlg_handler_, msg_id, pdata);
			return true;
		}
	}

	return false;
}

void tcamera::camera_OnMessage(rtc::Message* msg)
{
	tmsg_data_base_camera* pdata = static_cast<tmsg_data_base_camera*>(msg->pdata);
	if (msg->message_id == gui2::tdialog::POST_MSG_SWITCH_CAMERA) {
		switch_camera(pdata->camera_id);

	} else if (slot_ != nullptr) {
		slot_->camera_did_button_clicked(pdata->msg_id);
	}
}