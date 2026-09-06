#ifndef GAME_CONFIG_HPP_INCLUDED
#define GAME_CONFIG_HPP_INCLUDED

#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "rose_version.hpp"
#include "rose_string_utils.hpp"

enum {screenmode_scale, screenmode_min = screenmode_scale, screenmode_ratio, screenmode_partial, screenmode_count};
#define DEFAULT_SCREEN_MODE		screenmode_scale
#define MIN_VISIBLE_PERCENT		20
#define MAX_VISIBLE_PERCENT		99
#define DEFAULT_VISIBLE_PERCENT	95

#define LOGS_PB_MAX_DAYS	2
#define LOGS_PB_MAX_LOGS	300

enum {LOGS_PB, KLINK_PB};

struct tvlog_cfg
{
public:
	tvlog_cfg();

	void read_pref();
	void from_pref(const config& cfg);

	void write_pref() const;
	void to_pref(config& cfg) const;

	void clear()
	{
		memset(hides, 0, sizeof(hides));
		video_on_center = false;
		start_caption_msg.clear();
		finish_caption_msg.clear();
		poses_on_top = false;

		misc_sfx_disabled = false;
	}

	tvlog_cfg& operator=(const tvlog_cfg & that)
	{
		memcpy(hides, that.hides, sizeof(hides));
		video_on_center = that.video_on_center;
		start_caption_msg = that.start_caption_msg;
		finish_caption_msg = that.finish_caption_msg;
		poses_on_top = that.poses_on_top;

		misc_sfx_disabled = that.misc_sfx_disabled;

		return *this;
	}

	bool operator==(const tvlog_cfg& that) const
	{
		if (memcmp(hides, that.hides, sizeof(hides)) != 0) {
			return false;
		}

		if (video_on_center != that.video_on_center) {
			return false;
		}

		if (start_caption_msg != that.start_caption_msg) {
			return false;
		}

		if (finish_caption_msg != that.finish_caption_msg) {
			return false;
		}

		if (poses_on_top != that.poses_on_top) {
			return false;
		}

		if (misc_sfx_disabled != that.misc_sfx_disabled) {
			return false;
		}
		return true;
	}
	bool operator!=(const tvlog_cfg& that) const { return !operator==(that); }

public:
	enum {hid_landmarks, hid_face_cover,
		hid_start_date, hid_start_caption, hid_start_workout_name, hid_start_history,
		hid_active_state_name, hid_active_progress, hid_active_timer, hid_active_poses,
		hid_finish_date, hid_finish_caption, hid_finish_workout_summary, hid_finish_history,
		hid_finish_chart, hid_count
	};

	const std::map<int, std::string> hide_id_2_names_;
	std::map<std::string, int> hide_name_2_ids_;
	const std::vector<std::pair<std::string, std::string> > freq_caption_msgs;

	bool hides[hid_count];
	bool video_on_center;
	std::string start_caption_msg;
	std::string finish_caption_msg;
	bool poses_on_top;

	bool misc_sfx_disabled;
};

namespace game_config {
extern std::map<int, std::string> screen_modes;
extern SDL_threadID rdpc_tid;
}

void VALIDATE_IN_RDPC_THREAD();

struct trdpcookie
{
	trdpcookie()
	{
		clear();
	}

	bool valid() const { return ipv4 != 0; }

	void clear()
	{
		ipv4 = 0;
		landscape = true;
	}

	uint32_t ipv4;
	bool landscape;
};

struct tpbdevice
{
	tpbdevice(const std::string& uuid, const std::string& name, uint32_t ip, int64_t last_access)
		: uuid(utils::lowercase(uuid))
		, name(name)
		, ip(ip)
		, last_access(last_access)
	{}

	bool operator<(const tpbdevice& that) const 
	{
		int cmp = strcmp(name.c_str(), that.name.c_str());
		return cmp < 0;
	}

	std::string uuid;
	std::string name;
	uint32_t ip;
	int64_t last_access;
};

struct tpbgroup
{
	tpbgroup(const std::string& uuid, const std::string& name, const std::set<tpbdevice>& devices)
		: uuid(utils::lowercase(uuid))
		, name(name)
		, devices(devices)
	{}

	bool operator<(const tpbgroup& that) const 
	{
		int cmp = strcmp(name.c_str(), that.name.c_str());
		return cmp < 0;
	}

	std::string uuid;
	std::string name;
	std::set<tpbdevice> devices;
};

struct tpbremotes
{
	int version;
	int64_t timestamp;
	std::set<tpbgroup> groups;
};

namespace preferences {

int mainbarx();
void set_mainbarx(int value);

int minimapbarx();
void set_minimapbarx(int value);

int screenmode();
void set_screenmode(int value);

int visiblepercent();
void set_visiblepercent(int value);

bool ratioswitchable();
void set_ratioswitchable(bool value);

bool auto_enter_dcamera();
void set_auto_enter_dcamera(bool value);

std::string currentremote();
void set_currentremote(const std::string& value);

bool landscape();
void set_landscape(bool value);

int64_t event_time();
void set_event_time(int64_t value);

std::string vlog_cfg();
void set_vlog_cfg(const std::string& value);


}


#endif

