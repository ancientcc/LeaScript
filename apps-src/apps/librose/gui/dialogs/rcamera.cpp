#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rcamera.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "qr_code.hpp"
#include "font.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, rcamera)

trcamera::trcamera(tcamera& camera, treceiver& receiver, const std::string& remark)
	: camera_(camera)
	, receiver_(receiver)
	, remark_(remark)
	, disable_new_aplt_lock_(aplt::tdisable_new_klink_task_lock::reason_camera)
	, paper_(nullptr)
	, qrcode_dirty_(false)
	, highlight_ticks_(0)
{
	camera_slot_lock_.reset(new tcamera_slot_lock(camera_, *this));
}

trcamera::~trcamera()
{
	// camera_slot_lock_.reset();
}

void trcamera::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	tlabel* label = find_widget<tlabel>(window_, "remark", false, true);
	if (!remark_.empty()) {
		label->set_label(remark_);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	ttrack* track = find_widget<ttrack>(window_, "paper", false, true);
	track->set_timer_interval(0);
	track->set_did_draw(std::bind(&trcamera::did_draw_paper, this, _1, _2, _3));
	track->set_did_left_button_down(std::bind(&tcamera::did_left_button_down_paper, &camera_, _1, _2));
	track->set_did_mouse_motion(std::bind(&tcamera::did_mouse_motion_paper, &camera_, _1, _2, _3));
	track->set_did_mouse_leave(std::bind(&trcamera::did_mouse_leave_paper, this, _1, _2, _3));
	paper_ = track;

	camera_.enter_task(tcamera::taskid_rcamera, tcamera::FLAG_SHOW_CANCEL, false);
}

void trcamera::post_show()
{
	camera_.exit_task(tcamera::taskid_rcamera);
}

void trcamera::camera_post_enter_task()
{
		start_avcapture_message_ = _("Starting Camera");

		if (window_->drawn()) {
			gui2::absolute_draw();
		}

		paper_->set_timer_interval(30);
}

void trcamera::camera_pre_exit_task()
{
	qrcode_dirty_ = false;;
	qrcode_.clear();

	paper_->set_timer_interval(0);
}

void trcamera::did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	
	if (camera_.is_avcapture_started()) {
		// trtc_client::VideoRenderer* sink = camera_.get_avcapture().vrenderer(false, 0);
		// if (sink->frame_thread_frames != 0) {
			camera_.slice(widget_rect, true);

		// } else {
			// camera_.render_overlay_texs(renderer, widget_rect);
		// }

	} else if (!start_avcapture_message_.empty()) {
		surface text_surf = font::get_rendered_text(start_avcapture_message_, widget_rect.w, font::SIZE_DEFAULT, font::BAD_COLOR);
		// if (require_render) {
			SDL_Rect dst {widget_rect.x + (widget_rect.w - text_surf->w) / 2, widget_rect.y + (widget_rect.h - text_surf->h) / 2, text_surf->w, text_surf->h};
			render_surface(renderer, text_surf, nullptr, &dst);
		// }
	}

	// highlight message
	if (!highlight_msg_.empty()) {
		// new lighlight message task
		surface surf = font::get_rendered_text(highlight_msg_, 0, font::SIZE_DEFAULT, font::BAD_COLOR);
		highlight_tex_ = SDL_CreateTextureFromSurface2(renderer, surf);
		// now highlight_tex_ is highlight_msg_, can clear highlight_msg_.
		highlight_msg_.clear();
	}
	if (highlight_tex_.get() != nullptr) {
		int width2, height2;
		SDL_QueryTexture(highlight_tex_.get(), NULL, NULL, &width2, &height2);

		SDL_Rect dstrect{widget_rect.x + (widget_rect.w - width2) / 2, (widget_rect.y + widget_rect.h - height2) / 2, width2, height2};
		SDL_RenderCopy(renderer, highlight_tex_.get(), nullptr, &dstrect);

		if (highlight_ticks_ != 0 && SDL_GetTicks() >= highlight_ticks_) {
			highlight_tex_ = nullptr;
			highlight_ticks_ = 0;
		}
	}
}

void trcamera::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	VALIDATE(locals_count == 1, null_str);
	trtc_client::VideoRenderer& vsink = *locals[0];
	ttexture_2_mat_lock mat_lock(vsink.tex_);

	cv::Mat frame;

	bool use_cv_tex = false;

	if (use_cv_tex) {
		frame = mat_lock.mat.clone();

		// cv::Point point(frame.cols / 4, frame.rows / 2);
		// cv::circle(frame, point, 10, cv::Scalar(0, 0, 255, 255), 10);

		ttexture_2_mat_lock cv_mat_lock(vsink.cv_tex_);
		memcpy(cv_mat_lock.mat.data, frame.data, frame.cols * frame.rows * 4);
		SDL_UnlockTexture(vsink.cv_tex_.get());

	} else {
		frame = mat_lock.mat;
	}


	const tpoint ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame.cols, frame.rows);
	const SDL_Rect video_dst {draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};

	if (use_cv_tex) {
		SDL_RenderCopy(renderer, vsink.cv_tex_.get(), nullptr, &video_dst);
	} else {
		SDL_RenderCopy(renderer, vsink.tex_.get(), nullptr, &video_dst);
	}

	camera_.render_overlay_texs(renderer, video_dst);

	std::string new_qrcode;
	std::vector<cv::Point> new_qrcode_corners;
	if (qrcode_dirty_) {
		threading::lock lock(camera_.get_variable_mutex());
		qrcode_dirty_ = false;
		new_qrcode = qrcode_;
		new_qrcode_corners = qrcode_corners_;
	}

	if (!new_qrcode.empty() && qrcode_result_.empty()) {
		bool valid = receiver_.rcamera_verify_new_qrcode(new_qrcode);
		if (valid) {
			qrcode_result_ = new_qrcode;
			tmsg_data_valid_qrcode* pdata = new tmsg_data_valid_qrcode(*window_, new_qrcode, qrcode_result_);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_VALID_QRCODE, pdata);
		}
	}

	
/*
	if (vsink.new_frame) {
		cv::Mat edges;
		cv::cvtColor(mat_lock.mat, edges, cv::COLOR_BGRA2GRAY);
		cv::blur(edges, edges, cv::Size(7, 7));
		cv::Canny(edges, edges, 0, 30, 3);
		cv::cvtColor(edges, cv_mat_lock.mat, cv::COLOR_GRAY2BGRA);

		SDL_UnlockTexture(vsink.cv_tex_.get());
	}
*/
	// SDL_RenderCopy(renderer, vsink.tex_.get(), nullptr, &video_dst);

	double xratio = 1.0 * video_dst.w / frame.cols;
	double yratio = 1.0 * video_dst.h / frame.rows;

	if (!new_qrcode_corners.empty()) {
		VALIDATE(new_qrcode_corners.size() == 4, null_str);

		// center
		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[0].x * xratio, video_dst.y + new_qrcode_corners[0].y * yratio, 
			video_dst.x + new_qrcode_corners[1].x * xratio, video_dst.y + new_qrcode_corners[1].y * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[1].x * xratio, video_dst.y + new_qrcode_corners[1].y * yratio, 
			video_dst.x + new_qrcode_corners[2].x * xratio, video_dst.y + new_qrcode_corners[2].y * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[2].x * xratio, video_dst.y + new_qrcode_corners[2].y * yratio, 
			video_dst.x + new_qrcode_corners[3].x * xratio, video_dst.y + new_qrcode_corners[3].y * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[3].x * xratio, video_dst.y + new_qrcode_corners[3].y * yratio, 
			video_dst.x + new_qrcode_corners[0].x * xratio, video_dst.y + new_qrcode_corners[0].y * yratio);
	}

	

	SDL_Rect dst = draw_rect;
	std::string task_str;
	// if (tasks_.count(task_) != 0) {
	//	task_str = tasks_.find(task_)->second;
	// }
	if (!task_str.empty()) {
		surface text_surf = font::get_rendered_text(task_str, 0, 36, font::GOOD_COLOR);
		dst.w = text_surf->w;
		dst.h = text_surf->h;
		render_surface(renderer, text_surf, nullptr, &dst);
	}

	// camera_.did_camera_slice(frame, vsink.new_frame, vsink.frames);
}

void trcamera::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return;
	}

	camera_.did_mouse_leave_paper(widget, first, last);
}

void trcamera::camera_work_frame(const surface& surf)
{
	tsurface_2_mat_lock mat_lock(surf);

	cv::Mat rgb;
	cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);
	std::vector<cv::Point> corners;
	const std::string qrcode = find_qr(rgb, &corners);

	if (!qrcode.empty()) {
		threading::lock lock(camera_.get_variable_mutex());
		qrcode_dirty_ = true;
		qrcode_ = qrcode;
		qrcode_corners_ = corners;
		SDL_Log("%u, {camera_work_frame}qrcode: %s", SDL_GetTicks(), qrcode.c_str());
	}
}

void trcamera::camera_did_button_clicked(int msgid)
{
	if (msgid == POST_MSG_CANCEL_CAMERA) {
		window_->set_retval(gui2::twindow::CANCEL);
	}
}

void trcamera::set_highlight(const std::string& msg, int threshold)
{
	VALIDATE(!msg.empty(), null_str);
	VALIDATE(threshold >= 0, null_str);

	highlight_msg_ = msg;
	highlight_ticks_ = SDL_GetTicks() + threshold;
}

void trcamera::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_VALID_QRCODE:
		{
			tmsg_data_valid_qrcode* pdata = static_cast<tmsg_data_valid_qrcode*>(msg->pdata);
			bool exit = receiver_.rcamera_did_valid_qrcode(pdata->qrcode);
			if (exit) {
				window_->set_retval(twindow::OK);

			} else {
				qrcode_result_.clear();
				// maybe exist 'stocked' valid qrcode, clear it. avoid popup new dialog immediately.
				threading::lock lock(camera_.get_variable_mutex());
				qrcode_dirty_ = false;
			}
		}
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

int show_rcamera(tcamera& camera, trcamera::treceiver& receiver, const std::string& remark, std::string& qrcode_result)
{
	qrcode_result.clear();
	if (!instance->will_enter_sys_module(aplt::builtinid_dcamera)) {
		return twindow::CANCEL;
	}

	instance->popup_rcamera(true);
	instance->set_desire_depth_task(dctask_color);

	int retval = nposm;
	{
		gui2::trcamera dlg(camera, receiver, remark);
		dlg.show();

		retval = dlg.get_retval();
		if (retval != gui2::twindow::OK) {
			qrcode_result = dlg.get_qrcode_result();
		}
	}

	instance->popup_rcamera(false);
	return retval;
}

} // namespace gui2

