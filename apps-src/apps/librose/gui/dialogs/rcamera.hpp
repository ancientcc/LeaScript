#ifndef GUI_DIALOGS_RCAMERA_HPP_INCLUDED
#define GUI_DIALOGS_RCAMERA_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "camera.hpp"
#include "aplt.hpp"

namespace gui2 {

class ttrack;

class trcamera: public tdialog, public tcamera::tslot
{
public:
	class treceiver
	{
	public:
		virtual bool rcamera_verify_new_qrcode(const std::string& qrcode) { return false; }
		virtual bool rcamera_did_valid_qrcode(const std::string& qrcode) { return false; }
	};

	explicit trcamera(tcamera& camera, treceiver& receiver, const std::string& remark);
	~trcamera();

	const std::string get_qrcode_result() const { return qrcode_result_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void camera_post_enter_task() override;
	void camera_pre_exit_task() override;
	void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	void camera_work_frame(const surface& surf) override;
	// void camera_post_switch_camera() override;
	void camera_did_button_clicked(int msgid) override;

	void did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void set_highlight(const std::string& msg, int threshold);

	enum {MSG_VALID_QRCODE = POST_MSG_MIN_APP};
	struct tmsg_data_valid_qrcode: public rtc::MessageData {
		explicit tmsg_data_valid_qrcode(twindow& _window, const std::string& _qrcode, std::string& _result)
			: window(_window)
			, qrcode(_qrcode)
			, result(_result)
		{
			VALIDATE(!utils::is_rose_bundleid(qrcode, '.'), null_str);
		}

		twindow& window;
		const std::string qrcode;
		std::string& result;
	};
	void app_OnMessage(rtc::Message* msg) override;

private:
	tcamera& camera_;
	treceiver& receiver_;
	const std::string remark_;
	aplt::tdisable_new_klink_task_lock disable_new_aplt_lock_;
	ttrack* paper_;

	bool qrcode_dirty_;
	std::string qrcode_;
	std::vector<cv::Point> qrcode_corners_;
	std::string qrcode_result_;

	std::string start_avcapture_message_;
	std::string highlight_msg_;
	texture highlight_tex_;
	uint32_t highlight_ticks_;

	std::unique_ptr<tcamera_slot_lock> camera_slot_lock_;
};

int show_rcamera(tcamera& camera, trcamera::treceiver& receiver, const std::string& remark, std::string& qrcode_result);

} // namespace gui2

#endif

