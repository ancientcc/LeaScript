#ifndef GUI_DIALOGS_NEW_WKOSCRIPT_HPP
#define GUI_DIALOGS_NEW_WKOSCRIPT_HPP

#include "gui/dialogs/statusbar.hpp"
#include "wkoscript.hpp"
#include "mediapipe/rose/mediapipe_api.hpp"

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_button;

class tnew_wkoscript: public tdialog, public tstatusbar
{
public:
	struct taction
	{
	public:
		taction()
		{
			clear();
		}

		void clear()
		{
			task_type = nposm;
			pngs.clear();
			state_at = nposm;
			memset(landmark_valid, 0, sizeof(landmark_valid));
		}

	public:
		int task_type;
		std::vector<std::string> pngs;

		int state_at;
		SDL_FPoint landmarks[WKO_MAX_PHASE_COUNT][mediapipe::kNumPoseLandmarks];
		bool landmark_valid[WKO_MAX_PHASE_COUNT];
	};

	tnew_wkoscript(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript& script, 
		const std::map<int, aplt::tpreset_pose>& preset_poses, const std::string& wkoscript_dir);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_back(tbutton& widget);
	void click_working_dir(tbutton& widget);
	void click_new_dirs(tbutton& widget);
	void did_cam_pos_changed(ttoggle_button& widget);
	void click_generate(tbutton& widget);
	void reload_action_list(tlistbox& list);

	void app_timer_handler(uint32_t now) override;

private:
	aplt::twkoscript& script_;
	// mediapipe::tpose_tracking_api& mediapipe_api_;
	const std::map<int, aplt::tpreset_pose>& preset_poses_;
	const std::string wkoscript_dir_;
	const std::string new_dir_;
	enum {cam_pos_front, cam_pos_right, cam_pos_count};
	const std::map<int, tcode3> cam_positions_;

	tbutton* generate_widget_;
	tlistbox* action_list_;
	
	std::string curr_new_dir_;
	int cam_pos_;
	std::vector<taction> actions_;
};

} // namespace gui2

#endif

