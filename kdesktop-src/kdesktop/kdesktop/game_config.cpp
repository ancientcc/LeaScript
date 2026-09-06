#define GETTEXT_DOMAIN "kdesktop-lib"

#include "game_config.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"

tvlog_cfg::tvlog_cfg()
	: hide_id_2_names_({
		{hid_landmarks, "landmarks"},
		{hid_face_cover, "face_cover"},
		{hid_start_date, "start_date"},
		{hid_start_caption, "start_caption"},
		{hid_start_workout_name, "start_workout_name"},
		{hid_start_history, "start_history"},
		{hid_active_state_name, "active_state_name"},
		{hid_active_progress, "active_progress"},
		{hid_active_timer, "active_timer"},
		{hid_active_poses, "active_poses"},
		{hid_finish_date, "finish_date"},
		{hid_finish_caption, "finish_caption"},
		{hid_finish_workout_summary, "finish_workout_summary"},
		{hid_finish_history, "finish_history"},
		{hid_finish_chart, "finish_chart"},
	})
	, freq_caption_msgs({
		{_("caption_msg^start1"), _("caption_msg^finish1")},
		{_("caption_msg^start2"), _("caption_msg^finish2")},
		{_("caption_msg^start3"), _("caption_msg^finish3")},
	})
{
	VALIDATE((int)hide_id_2_names_.size() == hid_count, null_str);
	for (std::map<int, std::string>::const_iterator it = hide_id_2_names_.begin(); it != hide_id_2_names_.end(); ++ it) {
		hide_name_2_ids_.insert(std::make_pair(it->second, it->first));
	}
	VALIDATE((int)hide_name_2_ids_.size() == hid_count, null_str);

	clear();
}

void tvlog_cfg::read_pref()
{
	std::string vlog_cfg_str = preferences::vlog_cfg();

	config top_cfg;
	bool ret = aplt::read_config_ex(vlog_cfg_str, true, top_cfg);
	VALIDATE(ret, null_str);

	from_pref(top_cfg);
}

void tvlog_cfg::from_pref(const config& cfg)
{
	clear();

	std::vector<std::string> v_str = utils::split(cfg["hides"].str());
	for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it) {
		const std::string& name = *it;
		if (hide_name_2_ids_.count(name) != 0) {
			hides[hide_name_2_ids_.find(name)->second] = true;
		}
	}

	video_on_center = cfg["video_on_center"].to_bool();
	start_caption_msg = cfg["start_caption_msg"].str();
	finish_caption_msg = cfg["finish_caption_msg"].str();
	poses_on_top = cfg["poses_on_top"].to_bool();

	misc_sfx_disabled = cfg["misc_sfx_disabled"].to_bool();
}

void tvlog_cfg::write_pref() const
{
	std::stringstream out;

	config top_cfg;
	to_pref(top_cfg);
	aplt::write_config(out, top_cfg);
	
	preferences::set_vlog_cfg(out.str());
}

void tvlog_cfg::to_pref(config& cfg) const
{
	cfg.clear();

	std::stringstream hides_ss;
	for (int at = 0; at < hid_count; at ++) {
		if (hides[at]) {
			if (!hides_ss.str().empty()) {
				hides_ss << ",";
			}
			hides_ss << hide_id_2_names_.find(at)->second;
		}
	}
	if (!hides_ss.str().empty()) {
		cfg["hides"] = hides_ss.str();
	}

	if (video_on_center) {
		cfg["video_on_center"].from_bool(video_on_center);
	}
	if (!start_caption_msg.empty()) {
		cfg["start_caption_msg"] = start_caption_msg;
	}
	if (!finish_caption_msg.empty()) {
		cfg["finish_caption_msg"] = finish_caption_msg;
	}
	if (poses_on_top) {
		cfg["poses_on_top"].from_bool(poses_on_top);
	}

	if (misc_sfx_disabled) {
		cfg["misc_sfx_disabled"].from_bool(misc_sfx_disabled);
	}
}

namespace game_config {
std::map<int, std::string> screen_modes;
SDL_threadID rdpc_tid = nposm;

}

void VALIDATE_IN_RDPC_THREAD()
{
	VALIDATE(game_config::rdpc_tid == SDL_ThreadID(), null_str);
}

namespace preferences {

int mainbarx()
{
	int val = preferences::get_int("mainbarx", 0);
	if (val <= 0) {
		val = 0;
	}
	return val;
}

void set_mainbarx(int value)
{
	VALIDATE(value >= 0, null_str);
	preferences::set_int("mainbarx", value);
}

int minimapbarx()
{
	int val = preferences::get_int("minimapbarx", 0);
	if (val <= 0) {
		val = 0;
	}
	return val;
}

void set_minimapbarx(int value)
{
	VALIDATE(value >= 0, null_str);
	preferences::set_int("minimapbarx", value);
}

int screenmode()
{
	int val = preferences::get_int("screenmode", DEFAULT_SCREEN_MODE);
	if (val < 0 || val >= (int)game_config::screen_modes.size()) {
		val = DEFAULT_SCREEN_MODE;
	}
	return val;
}

void set_screenmode(int value)
{
	VALIDATE(value >= 0 && (int)game_config::screen_modes.size(), null_str);
	preferences::set_int("screenmode", value);
}

int visiblepercent()
{
	int val = preferences::get_int("visiblepercent", DEFAULT_VISIBLE_PERCENT);
	if (val < MIN_VISIBLE_PERCENT || val > MAX_VISIBLE_PERCENT) {
		val = DEFAULT_VISIBLE_PERCENT;
	}
	return val;
}

void set_visiblepercent(int value)
{
	VALIDATE(value >= MIN_VISIBLE_PERCENT && value <= MAX_VISIBLE_PERCENT, null_str);
	preferences::set_int("visiblepercent", value);
}

bool ratioswitchable()
{
	return preferences::get_bool("ratioswitchable", false);
}

void set_ratioswitchable(bool value)
{
	preferences::set_bool("ratioswitchable", value);
}

bool auto_enter_dcamera()
{
	{
		// now always auto enter dcamera.
		return true;
	}
	return preferences::get_bool("auto_enter_dcamera", true);
}

void set_auto_enter_dcamera(bool value)
{
	preferences::set_bool("auto_enter_dcamera", value);
}

std::string currentremote()
{
	std::string value = preferences::get_str("currentremote");
	if (utils::to_ipv4(value) == 0) {
		value.clear();
	}
	return value;
}

void set_currentremote(const std::string& value)
{
	preferences::set_str("currentremote", value);
}

bool landscape()
{
	return preferences::get_bool("landscape", true);
}

void set_landscape(bool value)
{
	preferences::set_bool("landscape", value);
}

int64_t event_time()
{
	return preferences::get_int64("event_time", 0);
}

void set_event_time(int64_t value)
{
	preferences::set_int64("event_time", value);
}

std::string vlog_cfg()
{
	return preferences::get_str("vlog_cfg");
}

void set_vlog_cfg(const std::string& value)
{
	preferences::set_str("vlog_cfg", value);
}

}

