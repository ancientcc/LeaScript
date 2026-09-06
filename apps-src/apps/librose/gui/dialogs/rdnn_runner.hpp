#ifndef GUI_DIALOGS_RDNN_RUNNER_HPP_INCLUDED
#define GUI_DIALOGS_RDNN_RUNNER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "rtc_client.hpp"
#include "camera.hpp"

#include <opencv2/core.hpp>

namespace gui2 {

class ttoggle_button;
class ttrack;

class trdnn_runner: public tdialog, public tcamera::tslot
{
public:
	enum tresult {OCR = 1, CHAT};
	enum {BASE_LAYER, OCR_LAYER, MORE_LAYER};

	explicit trdnn_runner(tcamera& camera, const tflite::tscript& script, const tflite::ttflite& tflite, const std::string& warnning);
	~trdnn_runner();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_first_drawn() override;


	// base
	void pre_base(twindow& window);

	void insert_blits(std::vector<image::tblit>& blits, const surface& surf, const std::string& result, const std::vector<std::pair<float, SDL_Rect> >& rects);

	void classifier_image();

	void did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn);
	// void did_left_button_down_paper(ttrack& widget, const tpoint& coordinate);
	void did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	// void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);


	void camera_post_enter_task() override;
	void camera_pre_exit_task() override;
	void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;

	void app_OnMessage(rtc::Message* msg) override;

private:
	tcamera& camera_;
	const tflite::tscript script_;
	const tflite::ttflite tflite_;
	std::string warnning_;
	ttrack* paper_;
	std::vector<image::tblit> blits_;

	std::vector<std::string> label_strings_;
	std::vector<float> locations_;

	uint32_t start_ticks_;
	// tpoint last_coordinate_;
	cv::RNG rng_;
	std::string last_object_;
};

} // namespace gui2

#endif

