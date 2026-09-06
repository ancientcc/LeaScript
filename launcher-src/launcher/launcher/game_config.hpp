#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

// #define FAKE_LIBKOSAPI_SO

#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "aplt.hpp"
#include "aplt_common.hpp"

#include <rose_ros/utils.hpp>
#include <rose_ros/aplt.hpp>
#include "aplt_clazz.hpp"
#include "speech_slot.hpp"

namespace gui2 {
class tprogress_;
}

class tbase_driver;

#define DEFAULT_SUPRESS_THRESHOLD		1800 // 30 minite
#define DEFAULT_SN		"kos-device"
#define DEFAULT_BLEPASSWORD		"123456"
#define LOGS_PB_MAX_DAYS	2
#define LOGS_PB_MAX_LOGS	300

// for aplt::ttask_cpp_pair
#define TASK_CPP_THRESHOLD_S_MIN	5
#define TASK_CPP_THRESHOLD_S_MAX	600 // 10 minutes

// #define COURSEWARE_UPLOAD_UID			0
// #define is_valid_courseware_uid(uid)	(is_valid_uid(uid) || (uid) == COURSEWARE_UPLOAD_UID)

// enum {mode_buildmap, mode_navigation, mode_maxros = mode_navigation, mode_position, mode_count};
enum {velocity_slower, velocity_slow, velocity_normal, velocity_fast, velocity_custom, velocity_count};
enum {teleop_forward, teleop_forwardleft, teleop_forwardright,
        teleop_backward, teleop_backwardleft, teleop_backwardright};
enum {LOGS_PB, KLINK_PB};
enum {task_type_chat, task_type_moveto, task_type_aplt_task_no_position, task_type_aplt_task, task_type_aplt_task_2position, task_type_charge2, task_type_count2};
enum {board_lubancat3, board_lubancat4, board_roc_rk3588s_pc, board_count};

extern const bool fake_libkosapi_so;

extern const uint8_t NEAR_OBSTACLE;

extern const std::string group_name_arm;
extern const std::string group_name_ik_arm;
extern const std::string group_name_claw;
extern const std::string joint_name_PRP;
extern const std::string joint_name_joint1;

extern const std::string frame_id_camera;
extern const std::string short_dbg_normal_surf_png;
extern const std::string short_dbg_normal_depth_data_dat;

extern const SDL_Range charge_width_range;
// extern const std::string wko_new_dir_prefix;


struct ttemp_task_type
{
	ttemp_task_type(int type, const std::string& id, const std::string& name, const std::set<aplt::per_t>& permissions)
		: type(type)
		, id(id) // 
		, name(name)
		, permissions(permissions)
	{}

	const int type;
	// A alias represented by a string. now use for locate image, path = "misc/" + id + ".png"
	const std::string id;
	const std::string name;
	const std::set<aplt::per_t> permissions;
};

enum {rosbag_record, rosbag_play, rosbag_count};
struct tgui_mode
{
	tgui_mode(int mode, const std::string& name, int rosbag);

	int mode;
	std::string name;
	int rosbag;
};
/*
#define PRIVACY_AUTO_PROTECT_THRESHOLD_S		90

class tprivacy
{
public:
	tprivacy(tbase_driver_core& base_driver)
		: base_driver_(base_driver)
		, protect_(false)
		, rdp_(true)
		, ble_discovery_(true)
		, upload_img_(true)
		, auto_protect_threshold_s_(nposm)
		, next_protect_ticks_(0)
		, disable_slice_(false)
	{}

	// Prevent the presence of a second tprivacy in the system.
	posix_noncopyable(tprivacy);

	void set_protect(bool enable);
	bool protect() const;
	std::string auto_protect_msgstr(int threshold_s) const;
	std::string desc() const;
	bool is_nposm() const
	{
		return !protect_ && auto_protect_threshold_s_ == nposm && next_protect_ticks_ == 0;
	}

	void set_auto_protect_threshold_s(int threshold_s) 
	{ 
		VALIDATE(threshold_s == nposm || threshold_s >= 30, null_str);
		auto_protect_threshold_s_ = threshold_s;
	}
	int auto_protect_threshold_s() const { return auto_protect_threshold_s_; }

	void set_next_protect_ticks()
	{
		VALIDATE(auto_protect_threshold_s_ != nposm, null_str);
		next_protect_ticks_ = SDL_GetTicks() + auto_protect_threshold_s_ * 1000;
	}

	void reset_next_protect_ticks()
	{
		VALIDATE(next_protect_ticks_ != 0, null_str);
		next_protect_ticks_ = 0;
	}
	uint32_t next_protect_ticks() const { return next_protect_ticks_; }

	class tdisable_slice_lock
	{
	public:
		tdisable_slice_lock(tprivacy& privacy)
			: privacy_(privacy)
		{
			VALIDATE(!privacy_.disable_slice_, null_str);
			privacy_.disable_slice_ = true;
		}

		~tdisable_slice_lock()
		{
			VALIDATE(privacy_.disable_slice_, null_str);
			privacy_.disable_slice_ = false;
		}

	private:
		tprivacy& privacy_;
	};
	bool disable_slice() const { return disable_slice_; }

	void validate() const 
	{
		if (protect_ || auto_protect_threshold_s_ == nposm) {
			VALIDATE(next_protect_ticks_ == 0, null_str);
		}
	}

private:
	tbase_driver_core& base_driver_;
	bool protect_; // is enable/disable protect?

	bool rdp_;
	bool ble_discovery_;
	bool upload_img_;
	int auto_protect_threshold_s_; // if nposm, not auto enter protection.
	uint32_t next_protect_ticks_;

	bool disable_slice_;
};
*/

struct tstart_aiagent
{
	tstart_aiagent()
		: subject(nposm)
	{}

	tstart_aiagent(const aplt::tapplet& aplt, const aplt::tapplet::ttask& task, int subject, const std::string& header,
		const std::function<SDL_Point3 (const char* c_str, int size)>& did_handle_text)
		: pair(aplt, task)
		, subject(subject)
		, header(header)
		, did_handle_text(did_handle_text)
	{}

	aplt::ttask_pair pair;
	int subject;
	std::string header;
	std::function<SDL_Point3 (const char* c_str, int size)> did_handle_text;
};

namespace game_config {

extern int board_model;
extern std::map<int, std::string> board_models;
extern int max_fps_to_encoder;
extern std::map<int, std::string> suppress_thresholds;
extern std::map<int, std::string> driver_names;
extern std::map<int, std::string> driver_keys;
extern std::map<int, std::string> goal_status;
extern std::map<int, std::string> velocities;
extern std::map<int, std::string> markers;
extern std::map<int, ttemp_task_type> temp_task_types;
extern std::vector<tgui_mode> gui_modes;
extern version_info kosapi_ver;
extern void* explorer_singleton;
extern SDL_threadID rdpd_tid;
extern const bool no_check_ble;
extern std::string rosbag_filename;
}

void VALIDATE_IN_RDPD_THREAD();

namespace preferences {

std::string sn();
void set_sn(const std::string& value);
int mapop_mode();
void set_mapop_mode(int mode);
int velocity();
void set_velocity(int value);
void custom_vel(double* twist);
void set_custom_vel(double* twist);
bool bleperipheral();
void set_bleperipheral(bool value);
std::string blepassword();
void set_blepassword(const std::string& value, bool sha1ed);
bool privacy_protect();
void set_privacy_protect(bool value);
int privacy_auto_protect_threshold();
void set_privacy_auto_protect_threshold(int value);
// std::string driver(int type);
// void set_driver(int type, const std::string& driver);
std::string curmap(const std::string& saves_map_dir);
void set_curmap(const std::string& value);
void set_im948serial(const std::string& value);
std::string im948serial();
void camera_K2(cv::Mat& K, cv::Mat& coeffs);
void set_camera_K2(const cv::Mat& K, const cv::Mat& coeffs);
std::string moveit_task_id2();
void set_moveit_task_id2(const std::string& task_id2);
// std::string base_scene_id();
// void set_base_scene_id(const std::string& id);
std::string charge_task_id2();
void set_charge_task_id2(const std::string& task_id2);
int charge_width_mm();
void set_charge_width_mm(int width_mm);
bool new_conversation_always();
void set_new_conversation_always(bool value);
bool tray_voice_speak();
void set_tray_voice_speak(bool value);
SDL_Rect tray_window_rect(const SDL_Range& w, const SDL_Range& h);
void set_tray_window_rect(const SDL_Rect& rect);
bool tray_window_hidden();
void set_tray_window_hidden(bool value);
// std::string share_watermark();
// void set_share_watermark(const std::string& value);
std::string last_wkoscript_file();
void set_last_wkoscript_file(const std::string& value);
std::string last_browse_file_path();
void set_last_browse_file_path(const std::string& value);
std::string wkoscript_dir();
void set_wkoscript_dir(const std::string& value);
}

int board_model_str_2_int(const std::string& str);
std::string disable_timing_warnning();
std::string sha1_blepassword(const std::string& password);
std::string rspfile_short_2_full(const std::string& saves_map_dir, const std::string& short_file);
bool is_valid_map_rsp_name(const char* name);
void collect_rsp_files(const std::string& saves_map_dir, std::set<std::string>& files);
/*
enum {cfgtype_klink, cfgtype_task_cpp, cfgtype_speech, klink_cfgtype_count};
struct tcfg_4field
{
	tcfg_4field(int type, const std::string& prefix, const std::string& id)
		: type(type)
		, prefix(prefix)
		, id(id)
	{}

	const int type;
	const std::string prefix;
	const std::string id;
	std::string name;
};
extern std::map<int, tcfg_4field> klink_cfgs;

std::string get_klink_cfg_dir(int type, bool preferences);
void collect_klink_cfg_files(int type, std::set<std::string>& files);
*/
void save_map_internal(const nav_msgs::OccupancyGrid& map, const std::map<std::string, tmap_position>& positions, const std::map<std::string, tmap_marker>& markers, const std::string& rspfile);
void save_map_by_ros_map(const tros_map& ros_map);
void save_d2c_png(const tdcintrinsics_C& intrinsics, gui2::tprogress_* progress, const uint8_t* tex_pixels, int task, const tdcframe_C& depth_frame, double dcpitch, bool all_rows, const std::string& result_png, const std::string& depth_data_file);
std::string base_node_error_str(const tbase_driver& base_driver, const std::string& task);
bool support_rosbag(int mode);
const tgui_mode& find_gui_mode(int mode, int rosbag);
bool is_valid_charge_width(int width_mm);

struct tspecial_mapop_mode_lock
{
	static int mode;
	tspecial_mapop_mode_lock(int _mode)
		: original(preferences::mapop_mode())
	{
		preferences::set_mapop_mode(_mode);
		mode = _mode;
	}

	~tspecial_mapop_mode_lock()
	{
		mode = nposm;
		preferences::set_mapop_mode(original);
	}

	int original;
};


#pragma pack(1)

struct trsp_moveitheader2 {
	uint32_t fourcc;
	uint32_t clawgaps;
	uint32_t ikmids;
	double claw_height; // unit: m
	SDL_DPoint3 PRP_offset;
	SDL_DPoint3 PRP_2offset;
	uint8_t reserve0;
	uint16_t reserve1;
	uint32_t reserve2;
};

// moveit forward kinematic it
struct trsp_clawgap {
	double value;   // joint's value
	double halfgap; // half gap. unit: meter
	double dist;	// distance from RP to tip. unit: meter 
};

struct trsp_ikmid {
	double z;		// unit: meter
	double mid;		// unit: meter
	double range;	// unit: meter 
};

#pragma pack()

class trsp_moveit2
{
public:
	trsp_moveit2()
		: items(nullptr)
	{
		clear();
	}

	~trsp_moveit2()
	{
		clear();
	}

	bool valid() const { return !rspfile.empty() && item_count > 0 && items != nullptr; }

	void clear()
	{
		rspfile.clear();
		if (items != nullptr) {
			free(items);
			items = nullptr;
		}
		item_count = 0;

		if (ikmids != nullptr) {
			free(ikmids);
			ikmids = nullptr;
		}
		ikmid_count = 0;
	}

public:
	std::string rspfile; // full rspfile name
	trsp_moveitheader2 header;

	trsp_clawgap* items;
	int item_count;

	trsp_ikmid* ikmids;
	int ikmid_count;
};

#endif

