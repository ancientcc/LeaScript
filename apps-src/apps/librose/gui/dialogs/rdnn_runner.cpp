#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rdnn_runner.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "chinese.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video.hpp>


using namespace std::placeholders;

// #include "easypr/core/plate_recognize.h"


namespace gui2 {

REGISTER_DIALOG(rose, rdnn_runner)

trdnn_runner::trdnn_runner(tcamera& camera, const tflite::tscript& script, const tflite::ttflite& tflite, const std::string& warnning)
	: camera_(camera)
	, script_(script)
	, tflite_(tflite)
	, warnning_(warnning)
	// , last_coordinate_(construct_null_coordinate())
	, rng_(12345)
{
	camera_.set_slot(this);
}

trdnn_runner::~trdnn_runner()
{
	camera_.set_slot(nullptr);
}

void trdnn_runner::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tbutton* button = find_widget<tbutton>(window_, "cancel", false, true);
	button->set_icon("misc/back.png");

	if (!warnning_.empty()) {
		find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(warnning_, 0xffff0000));
	}

	button = find_widget<tbutton>(window_, "ok", false, true);
	button->set_visible(twidget::HIDDEN);

	pre_base(*window_);
}

void trdnn_runner::post_show()
{
	camera_.exit_task(tcamera::taskid_dnn);
}


void trdnn_runner::app_first_drawn()
{
	camera_.set_tflite(script_, tflite_);

	VALIDATE(blits_.empty(), null_str);
	if (script_.source == tflite::source_camera) {
		camera_.enter_task(tcamera::taskid_dnn, tcamera::FLAG_USE_TFLITE, false);
	}

	if (script_.scenario == tflite::scenario_classifier) {
		if (script_.source == tflite::source_img) {
			classifier_image();
		} else {
			paper_->immediate_draw();
		}

	} else if (script_.scenario == tflite::scenario_detect) {
		if (script_.source == tflite::source_img) {

		} else {
			paper_->immediate_draw();
		}

	} else if (script_.scenario == tflite::scenario_pr) {
		VALIDATE(false, "don't support");
	}
}

void trdnn_runner::pre_base(twindow& window)
{
	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	paper_->set_did_draw(std::bind(&trdnn_runner::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_left_button_down(std::bind(&tcamera::did_left_button_down_paper, &camera_, _1, _2));
	paper_->set_did_mouse_motion(std::bind(&tcamera::did_mouse_motion_paper, &camera_, _1, _2, _3));
	paper_->set_did_mouse_leave(std::bind(&trdnn_runner::did_mouse_leave_paper, this, _1, _2, _3));
}

void trdnn_runner::insert_blits(std::vector<image::tblit>& blits, const surface& surf, const std::string& result, const std::vector<std::pair<float, SDL_Rect> >& rects)
{
	blits.clear();
	surface text_surf = font::get_rendered_text(result, paper_->get_width(), font::SIZE_DEFAULT, font::BLACK_COLOR);
	blits.push_back(image::tblit(text_surf, (paper_->get_width() - text_surf->w) / 2, 0, text_surf->w, text_surf->h));

	const int outer_h = paper_->get_height() - text_surf->h;
	tpoint size = calculate_adaption_ratio_size(paper_->get_width(), outer_h, surf->w, surf->h);
	const int xsrc = (paper_->get_width() - size.x) / 2;
	const int ysrc = text_surf->h;
	blits.push_back(image::tblit(surf, (paper_->get_width() - size.x) / 2, text_surf->h, size.x, size.y));

	// char score_str[32];
	const double width_ratio = 1.0 * size.x / surf->w;
	const double height_ratio = 1.0 * size.y / surf->h;
	for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& rect = it->second;

		blits.push_back(image::tblit(image::BLITM_FRAME, xsrc + rect.x * width_ratio, ysrc + rect.y * height_ratio, rect.w * width_ratio, rect.h * height_ratio, 0xffff0000));

		if (script_.scenario == tflite::scenario_pr) {
			VALIDATE(false, "don't support");
		}
	}
}

void trdnn_runner::classifier_image()
{
	surface surf = image::get_image(script_.img);
	VALIDATE(surf, null_str);
	camera_.classifier_surface(surf, camera_.get_mutex_result());
	const std::string result = camera_.get_result().to_string();

	insert_blits(blits_, surf, result, camera_.classifier_rects());

	paper_->immediate_draw();
}

void trdnn_runner::camera_post_enter_task()
{
	start_ticks_ = SDL_GetTicks();
	paper_->set_timer_interval(30);
	last_object_.clear();
}

void trdnn_runner::camera_pre_exit_task()
{
	paper_->set_timer_interval(0);
}

void trdnn_runner::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	trtc_client::VideoRenderer& vsink = *locals[0];
	ttexture_2_mat_lock mat_lock(vsink.tex_);
	cv::Mat& frame = mat_lock.mat;
	// ttexture_2_mat_lock cv_mat_lock(vsink.cv_tex_);

	if (script_.source == tflite::source_camera) {
		// camera_.did_camera_slice(frame, vsink.new_frame, vsink.frames);
	}

	tpoint ratio_size = calculate_adaption_ratio_size(paper_->get_width(), paper_->get_height(), frame.cols, frame.rows);
	const int xsrc = paper_->get_x() + (paper_->get_width() - ratio_size.x) / 2;
	const int ysrc = paper_->get_y() + (paper_->get_height() - ratio_size.y) / 2;
	SDL_Rect video_dst {xsrc, ysrc, ratio_size.x, ratio_size.y};
	SDL_RenderCopy(renderer, vsink.tex_.get(), NULL, &video_dst);

	camera_.render_overlay_texs(renderer, video_dst);

	camera_.render_classifier_result(renderer, frame, video_dst, false);

	std::string this_object;
	{
		threading::lock lock(camera_.get_variable_mutex());
		const tflite::tresult& result = camera_.get_result();
		for (std::vector<tflite::tresult::titem>::const_iterator it = result.items.begin(); it != result.items.end(); ++ it) {
			const tflite::tresult::titem& item = *it;
			if (item.score >= 0.6) {
				this_object = item.name;
				break;
			}
		}
	}
	if (!this_object.empty() && this_object != last_object_) {
		size_t pos = this_object.find('(');
		const std::string text = pos == std::string::npos? this_object: this_object.substr(0, pos);
		chinese::curr_pinyin.speak(text);
		last_object_ = this_object;
	}

/*
	std::vector<std::pair<float, SDL_Rect> > rects;
	std::string this_object;
	std::string result_str;
	{
		threading::lock lock(camera_.get_variable_mutex());
		rects = camera_.classifier_rects();
		const tcamera::tresult& result = camera_.get_result();
		result_str = result.to_string();
		for (std::vector<camera::tresult::titem>::const_iterator it = result.items.begin(); it != result.items.end(); ++ it) {
			const tcamera::tresult::titem& item = *it;
			if (item.score >= 0.6) {
				this_object = item.name;
				break;
			}
		}
	}
	if (!this_object.empty() && this_object != last_object_) {
		size_t pos = this_object.find('(');
		const std::string text = pos == std::string::npos? this_object: this_object.substr(0, pos);
		chinese::curr_pinyin.speak(text);
		last_object_ = this_object;
	}

	surface text_surf;
	// char score_str[32];
	const double width_ratio = 1.0 * dst.w / frame.cols;
	const double height_ratio = 1.0 * dst.h / frame.rows;
	for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& rect = it->second;

		render_rect_frame(renderer, SDL_Rect{(int)(dst.x + rect.x * width_ratio), int(dst.y + rect.y * height_ratio), int(rect.w * width_ratio), int(rect.h * height_ratio)}, 0xffff0000, 1);

		if (script_.scenario == tflite::scenario_pr) {
			VALIDATE(false, "don't support");
		}
	}

	if (!result_str.empty()) {
		text_surf = font::get_rendered_text(result_str, 0, font::SIZE_LARGE, font::GOOD_COLOR);
		dst.w = text_surf->w;
		dst.h = text_surf->h;
		render_surface(renderer, text_surf, NULL, &dst);
	}
*/
}
/*
void trdnn_runner::did_left_button_down_paper(ttrack& widget, const tpoint& coordinate)
{
	last_coordinate_ = coordinate;
}
*/

void trdnn_runner::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return;
	}

	camera_.did_mouse_leave_paper(widget, first, last);
}

/*
void trdnn_runner::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	last_coordinate_ = last;
}
*/

void trdnn_runner::did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();

	for (std::vector<image::tblit>::const_iterator it = blits_.begin(); it != blits_.end(); ++ it) {
		const image::tblit& blit = *it;
		image::render_blit(renderer, blit, widget.get_x(), widget.get_y());
	}

	camera_.slice(draw_rect, true);
}

void trdnn_runner::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case POST_MSG_SWITCH_CAMERA:
		VALIDATE(false, null_str);
		camera_.camera_OnMessage(msg);
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata) {
		delete msg->pdata;
	}
}

} // namespace gui2