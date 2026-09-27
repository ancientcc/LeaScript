#define GETTEXT_DOMAIN "launcher-lib"

#include "rose_global.hpp"
#include "game_config.hpp"
#include "rose_version.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/settings.hpp"

#include <sstream>
#include <iomanip>
using namespace std::placeholders;

#include <openssl/sha.h>
#include <openssl/mem.h>
#include "base_instance.hpp"
#include "base_driver.hpp"

#include <rose_ros/kidnap.hpp>

namespace game_config {

int board_model = nposm;
std::map<int, std::string> board_models;
int max_fps_to_encoder = 25;
std::map<int, std::string> suppress_thresholds;
std::map<int, std::string> driver_names;
std::map<int, std::string> driver_keys;
std::map<int, std::string> goal_status;
std::map<int, std::string> velocities;
std::map<int, std::string> markers;
std::map<int, ttemp_task_type> temp_task_types;
std::vector<tgui_mode> gui_modes;
version_info kosapi_ver;
void* explorer_singleton = nullptr;
SDL_threadID rdpd_tid = nposm;
const bool no_check_ble = false;
std::string rosbag_filename;
}

#ifdef FAKE_LIBKOSAPI_SO
const bool fake_libkosapi_so = true;
#else
const bool fake_libkosapi_so = false;
#endif

const uint8_t NEAR_OBSTACLE = 60;

const std::string group_name_arm = "arm";
const std::string group_name_ik_arm = "ik_arm";
const std::string group_name_claw = "hand";
const std::string joint_name_PRP = "joint4";
const std::string joint_name_joint1 = "joint1";

const std::string frame_id_camera = "dcamera"; // optical
const std::string short_dbg_normal_surf_png = "dbg_surf.png";
const std::string short_dbg_normal_depth_data_dat = "dbg_depth_data.dat";

const SDL_Range charge_width_range{150, 300}; // [15cm, 30cm]
// const std::string wko_new_dir_prefix = "__new_";
/*
std::map<int, tcfg_4field> klink_cfgs {
	{cfgtype_klink, tcfg_4field(cfgtype_klink, "klink_", "klink")},
	{cfgtype_task_cpp, tcfg_4field(cfgtype_task_cpp, "task_cpp_", "task_cpp")},
	{cfgtype_speech, tcfg_4field(cfgtype_speech, "speech_", "speech")}
};
*/
void VALIDATE_IN_RDPD_THREAD()
{
	VALIDATE(game_config::rdpd_tid == SDL_ThreadID(), null_str);
}

tgui_mode::tgui_mode(int mode, const std::string& name, int rosbag)
	: mode(mode)
	, name(name)
	, rosbag(rosbag)
{
	VALIDATE(mode >= 0 && mode < mode_count, null_str);
	if (!support_rosbag(mode)) {
		VALIDATE(rosbag == nposm, null_str);
	} else {
		VALIDATE(rosbag == nposm || (rosbag >= 0 && rosbag < rosbag_count), null_str);
	}
}
/*
void tprivacy::set_protect(bool enable)
{
	if (enable) {
		VALIDATE(!protect_, null_str);
		if (next_protect_ticks_ != 0) {
			reset_next_protect_ticks();
		}
	} else {
		VALIDATE(protect_, null_str);
		VALIDATE(next_protect_ticks_ == 0, null_str);
	}
	protect_ = enable;

	if (base_driver_.slot != nullptr) {
		aplt::tbase_ext_lamp* lamp = base_driver_.slot->query_ext_lamp();
		if (lamp != nullptr) {
			lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_privacy, 
				enable? aplt::tbase_ext_lamp::ledact_on: aplt::tbase_ext_lamp::ledact_off);
		}
	}
}

bool tprivacy::protect() const
{
	if (protect_) {
		VALIDATE(next_protect_ticks_ == 0, null_str);
	}
	return protect_;
}

std::string tprivacy::auto_protect_msgstr(int threshold_s) const
{
	utils::string_map symbols;

	symbols["threshold"] = utils::format_elapse_hms(threshold_s);
	return vgettext2("privacy^automatic enter protection $threshold", symbols);
}

std::string tprivacy::desc() const
{
	std::stringstream ss;
	if (protect_) {
		ss << _("privacy^protect");
	} else {
		// ss << _("privacy^not protect");
	}

	if (auto_protect_threshold_s_ != nposm) {
		ss << auto_protect_msgstr(auto_protect_threshold_s_);
	}
	return ss.str();
}
*/
namespace preferences {

std::string sn()
{
	std::string str = preferences::get_str("sn");
	if (str.empty()) {
		str = DEFAULT_SN;
	}
	return str;
}

void set_sn(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	preferences::set_str("sn", value);
}

int mapop_mode()
{
	int mode = preferences::get_int("mapop_mode", 0);
	if (mode < 0 || mode >= mode_count) {
		mode = 0;
	}
	return mode;
}

void set_mapop_mode(int mode)
{
	VALIDATE(tspecial_mapop_mode_lock::mode == nposm, null_str);
	VALIDATE(mode >= 0 && mode < mode_count, null_str);
	preferences::set_int("mapop_mode", mode);
}

int velocity()
{
	const int def = velocity_normal;
	int value = preferences::get_int("velocity", def);
	if (value < 0 || value >= velocity_count) {
		value = def;
	}
	return value;
}

void set_velocity(int value)
{
	VALIDATE(value >= 0 && value < velocity_count, null_str);
	preferences::set_int("velocity", value);
}

void custom_vel(double* twist)
{
	twist[0] = 0.0;
	twist[1] = 0.0;
	twist[2] = 0.0;

	const std::string str = preferences::get_str("custom_vel");
	std::vector<std::string> vstr = utils::split(str, ',');

	int size = SDL_min(vstr.size(), 3);
	for (int at = 0; at < size; at ++) {
		double val = SDL_atof(vstr[at].c_str());
		twist[at] = val;
	}
}

void set_custom_vel(double* twist)
{
	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "%.5f,%.5f,%.5f", twist[0], twist[1], twist[2]);
	preferences::set_str("custom_vel", buf);
}
/*
std::string driver(int type)
{
	VALIDATE(game_config::driver_keys.count(type) != 0, null_str);
	const std::string& key = game_config::driver_keys.find(type)->second;

	return preferences::get_str(key);
}

void set_driver(int type, const std::string& value)
{
	VALIDATE(game_config::driver_keys.count(type) != 0, null_str);
	const std::string& key = game_config::driver_keys.find(type)->second;

	preferences::set_str(key, value);
}
*/
std::string curmap(const std::string& saves_map_dir)
{
	std::string str = preferences::get_str("curmap");
	if (!str.empty()) {
		if (!is_valid_map_rsp_name(str.c_str())) {
			str.clear();
		}
		if (!str.empty()) {
			const std::string full = rspfile_short_2_full(saves_map_dir, str);
			if (!SDL_IsFile(full.c_str())) {
				str.clear();
			}
		}
	}
	return str;
}

void set_curmap(const std::string& value)
{
	// value may be is empty
	preferences::set_str("curmap", value);
}

std::string im948serial()
{
	return preferences::get_str("im948_serial");
}

void set_im948serial(const std::string& value)
{
	// if value is empty, mean to not use im948 
	preferences::set_str("im948_serial", value);
}

bool bleperipheral()
{
	return preferences::get_bool("bleperipheral", true);
}

void set_bleperipheral(bool value)
{
	preferences::set_bool("bleperipheral", value);
}

std::string blepassword()
{
	return preferences::get_str("blepassword");
}

void set_blepassword(const std::string& value, bool sha1ed)
{
	VALIDATE(!value.empty(), null_str);

	std::string sha1text = sha1ed? value: sha1_blepassword(value);
	preferences::set_str("blepassword", sha1text);
}

bool privacy_protect()
{
	return preferences::get_bool("privacy_protect", false);
}

void set_privacy_protect(bool value)
{
	preferences::set_bool("privacy_protect", value);
}

int privacy_auto_protect_threshold()
{
	int value = preferences::get_int("privacy_auto_protect_threshold", nposm);
	if (value != nposm && value != PRIVACY_AUTO_PROTECT_THRESHOLD_S) {
		value = nposm;
	}
	return value;
}

void set_privacy_auto_protect_threshold(int value)
{
	VALIDATE(value == nposm || value == PRIVACY_AUTO_PROTECT_THRESHOLD_S, null_str);
	preferences::set_int("privacy_auto_protect_threshold", value);
}

static std::string camera_K2_str()
{
	return preferences::get_str("camera_K2");
}

void camera_K2(cv::Mat& K, cv::Mat& coeffs)
{
	K = cv::Mat::eye(3, 3, CV_64FC1); // K.at<double>(2, 2) must be 1;
    coeffs = cv::Mat(1, 5, CV_64FC1, cv::Scalar::all(0));

	const std::string str = camera_K2_str();
	if (str.empty()) {
		return;
	}

	// A rough way to check validity. all charactor must be [0..9], '.', ',', '-'
	const char* c_str = str.c_str();
	int s = str.size();
	for (int at = 0; at < s; at ++) {
		char ch = c_str[at];
		if ((ch >= '0' && ch <= '9') || ch == '.' || ch == ',' || ch == '-') {
		} else {
			return;
		}
	}

	std::vector<std::string> vstr = utils::split(str, ',');
	if (vstr.size() != 9) {
		return;
	}
	K.at<double>(0, 0) = SDL_atof(vstr[0].c_str());
	K.at<double>(0, 2) = SDL_atof(vstr[1].c_str());
	K.at<double>(1, 1) = SDL_atof(vstr[2].c_str());
	K.at<double>(1, 2) = SDL_atof(vstr[3].c_str());

	coeffs.at<double>(0, 0) = SDL_atof(vstr[4].c_str());
	coeffs.at<double>(0, 1) = SDL_atof(vstr[5].c_str());
	coeffs.at<double>(0, 4) = SDL_atof(vstr[6].c_str());
	coeffs.at<double>(0, 2) = SDL_atof(vstr[7].c_str());
	coeffs.at<double>(0, 3) = SDL_atof(vstr[8].c_str());
}

void set_camera_K2(const cv::Mat& K, const cv::Mat& coeffs)
{
	VALIDATE(K.rows == 3 && K.cols == 3, null_str);
	VALIDATE(coeffs.rows == 1 && coeffs.cols == 5, null_str);
	VALIDATE(K.depth() == CV_64F && coeffs.depth() == CV_64F, null_str);
	
	char buf[128];
	// fx, cx, fy, cy, k1, k2, k3, p1, p2
	// must not exist ' '. see void camera_K2(...)
	SDL_snprintf(buf, sizeof(buf), "%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f",
		K.at<double>(0, 0), K.at<double>(0, 2), K.at<double>(1, 1), K.at<double>(1, 2),
		coeffs.at<double>(0, 0), coeffs.at<double>(0, 1), coeffs.at<double>(0, 4),
		coeffs.at<double>(0, 2), coeffs.at<double>(0, 3));
	const std::string new_K2(buf);
	preferences::set_str("camera_K2", new_K2);
}

std::string moveit_task_id2()
{
	std::string task_id2 = preferences::get_str("moveit_task_id2");
	return task_id2;
}

void set_moveit_task_id2(const std::string& task_id2)
{
	preferences::set_str("moveit_task_id2", task_id2);
}
/*
std::string base_scene_id()
{
	std::string id = preferences::get_str("base_scene_id");
	return id;
}

void set_base_scene_id(const std::string& id)
{
	preferences::set_str("base_scene_id", id);
}
*/
std::string charge_task_id2()
{
	std::string task_id2 = preferences::get_str("charge_task_id2");
	return task_id2;
}

void set_charge_task_id2(const std::string& task_id2)
{
	preferences::set_str("charge_task_id2", task_id2);
}

std::string charge_width_mm(int type)
{
	const std::string& key = game_config::driver_keys.find(type)->second;

	return preferences::get_str(key);
}

void set_charge_width_mm(int type, const std::string& value)
{
	VALIDATE(game_config::driver_keys.count(type) != 0, null_str);
	const std::string& key = game_config::driver_keys.find(type)->second;

	preferences::set_str(key, value);
}

int charge_width_mm()
{
	int value = preferences::get_int("charge_width_mm", nposm);
	if (!is_valid_charge_width(value)) {
		value = nposm;
	}
	return value;
}

void set_charge_width_mm(int value)
{
	VALIDATE(is_valid_charge_width(value) || value == nposm, null_str);
	preferences::set_int("charge_width_mm", value);
}

bool new_conversation_always()
{
	return preferences::get_bool("new_conversation_always", true);
}

void set_new_conversation_always(bool value)
{
	preferences::set_bool("new_conversation_always", value);
}

bool tray_voice_speak()
{
	return preferences::get_bool("tray_voice_speak", true);
}

void set_tray_voice_speak(bool value)
{
	preferences::set_bool("tray_voice_speak", value);
}

SDL_Rect tray_window_rect(const SDL_Range& w, const SDL_Range& h)
{
	uint64_t u64 = preferences::get_int64("tray_window_rect", 0);
	SDL_Rect result = lua_unpack_rect(u64);

	int screen_width = (int)(gui2::settings::screen_width * gui2::twidget::hdpi_scale);
	int screen_height = (int)(gui2::settings::screen_height * gui2::twidget::hdpi_scale);

	SDL_DisplayMode dm;
    int ret = SDL_GetDesktopDisplayMode(0, &dm);
	VALIDATE(ret == 0, null_str);
	screen_width = dm.w;
	screen_height = dm.h;

	if (result.x < 0) {
		result.x = 0;
	}
	if (result.y < 0) {
		result.y = 0;
	}
	if (result.w < w.min) {
		result.w = w.min;
	}
	if (result.w > w.max) {
		result.w = w.max;
	}
	if (result.h < h.min) {
		result.h = h.min;
	}
	if (result.h > h.max) {
		result.h = h.max;
	}
	if (result.x + result.w > screen_width) {
		result.x = screen_width - result.w;
	}
	if (result.y + result.h > screen_height) {
		result.y = screen_height - result.h;
	}

	return result;
}

void set_tray_window_rect(const SDL_Rect& rect)
{
	uint64_t u64 = lua_pack_rect(rect.x, rect.y, rect.w, rect.h);
	preferences::set_int64("tray_window_rect", u64);
}

bool tray_window_hidden()
{
	return preferences::get_bool("tray_window_hidden", false);
}

void set_tray_window_hidden(bool value)
{
	preferences::set_bool("tray_window_hidden", value);
}
/*
std::string share_watermark()
{
	std::string value = preferences::get_str("share_watermark");
	return value;
}

void set_share_watermark(const std::string& value)
{
	preferences::set_str("share_watermark", value);
}
*/
std::string last_wkoscript_file()
{
	return preferences::get_str("last_wkoscript_file");
}

void set_last_wkoscript_file(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (last_wkoscript_file() != value) {
		preferences::set_str("last_wkoscript_file", value);
	}
}

std::string last_browse_file_path()
{
	return preferences::get_str("last_browse_file_path");
}

void set_last_browse_file_path(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (last_browse_file_path() != value) {
		preferences::set_str("last_browse_file_path", value);
	}
}

std::string wkoscript_dir()
{
	return preferences::get_str("wkoscript_dir");
}

void set_wkoscript_dir(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (wkoscript_dir() != value) {
		preferences::set_str("wkoscript_dir", value);
	}
}

std::string last_wkocourse_file()
{
	return preferences::get_str("last_wkocourse_file");
}

void set_last_wkocourse_file(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (last_wkocourse_file() != value) {
		preferences::set_str("last_wkocourse_file", value);
	}
}

std::string wkocourse_dir()
{
	return preferences::get_str("wkocourse_dir");
}

void set_wkocourse_dir(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (wkocourse_dir() != value) {
		preferences::set_str("wkocourse_dir", value);
	}
}

bool skip_action_tpl2_id_check()
{
	return preferences::get_bool("skip_action_tpl2_id_check", true);
}

void skip_action_tpl2_id_check(bool value)
{
	preferences::set_bool("skip_action_tpl2_id_check", value);
}

} // namespace preferences

int board_model_str_2_int(const std::string& str)
{
	const std::string model = utils::lowercase(str);
	if (model == "lubancat-3") {
		// LubanCat-3
		return board_lubancat3;

	} else if (model == "lubancat 4") {
		// LubanCat 4
		return board_lubancat4;

	} else if (model == "roc-rk3588s-pc") {
		// ROC-RK3588S-PC
		return board_roc_rk3588s_pc;

	}

	// VALIDATE(model == "lubancat 4", std::string("Unsupport model: ") + str);
	return nposm;
}

std::string disable_timing_warnning()
{
	utils::string_map symbols;
	symbols["klink"] = aplt::all_fake_applets.find(aplt::builtinid_klink)->second.name;
	return vgettext2("As long as this window is displayed, no new $klink task will run.", symbols);
}

std::string sha1_blepassword(const std::string& password)
{
	VALIDATE(!password.empty(), null_str);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	SHA1_Update(&ctx, password.c_str(), password.size());

	SHA1_Final(md, &ctx);
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	return utils::hex_encode((const char*)md, SHA_DIGEST_LENGTH);
}

std::string rspfile_short_2_full(const std::string& saves_map_dir, const std::string& short_file)
{
	VALIDATE(!saves_map_dir.empty(), null_str);
	VALIDATE(!short_file.empty(), null_str);

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%s/%s", saves_map_dir.c_str(), short_file.c_str());
	return buf;
}

bool is_valid_map_rsp_name(const char* name)
{
	// map0000.rsp --> map9999.rsp
	if (name == nullptr || SDL_strlen(name) != 11) {
		return false;
	}
	if (SDL_strncmp(name, "map", 3) != 0) {
		return false;
	}
	int pos = 3;
	for (int at = 0; at < 4; at ++) {
		char ch = name[pos + at];
		if (ch < '0' || ch > '9') {
			return false;
		}
	}

	pos += 4;
	if (SDL_strncmp(name + pos, ".rsp", 4) != 0) {
		return false;
	}
	return true;
}

static bool did_walk_rsp(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& files, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		// const std::string name = utils::lowercase(dirent->name);		
		if (is_valid_map_rsp_name(dirent->name)) {
			files.insert(root + "/" + dirent->name);
		}
	}
	return true;
}

void collect_rsp_files(const std::string& saves_map_dir, std::set<std::string>& files)
{
	files.clear();
	const std::string& saves_dir = saves_map_dir;

	std::map<std::string, std::string> full_images;
	walk_dir(saves_dir, false, std::bind(&did_walk_rsp, _1, _2, std::ref(files), std::ref(saves_dir)));
}
/*
bool is_valid_klink_cfg_name(const tcfg_4field& cfg_3field, const char* name)
{
	const std::string& prefix = cfg_3field.prefix;
	// klink_xxx.cfg
	if (name == nullptr) {
		return false;
	}
	int s = SDL_strlen(name);
	if (s <= (int)prefix.size() + 4) {
		return false;
	}
	if (SDL_strncmp(name, prefix.c_str(), prefix.size()) != 0) {
		return false;
	}

	if (SDL_strncmp(name + s - 4, ".cfg", 4) != 0) {
		return false;
	}
	return true;
}

std::string get_klink_cfg_dir(int type, bool preferences)
{
	VALIDATE(type >= 0 && type < klink_cfgtype_count, null_str);
	if (preferences) {
		return get_saves_dir();
	}
	return game_config::app_dir_root + "/cert/klink";
}

static bool did_walk_klink_cfg(const std::string& dir, const SDL_dirent2* dirent, const tcfg_4field& cfg_3field, std::set<std::string>& files, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		bool insert = false;
		// const std::string name = utils::lowercase(dirent->name);
		if (cfg_3field.type == cfgtype_klink) {
			if (is_valid_klink_cfg_name(cfg_3field, dirent->name)) {
				insert = true;		
			}
		} else {
			insert = true;
		}
		if (insert) {
			files.insert(root + "/" + dirent->name);
		}
	}
	return true;
}

void collect_klink_cfg_files(int type, std::set<std::string>& files)
{
	VALIDATE(type == cfgtype_klink, null_str);
	const tcfg_4field& cfg_3field = klink_cfgs.find(type)->second;

	files.clear();

	for (int at = 0; at < 2; at ++) {
		const std::string saves_dir = get_klink_cfg_dir(type, at == 0? true: false);
		walk_dir(saves_dir, false, std::bind(&did_walk_klink_cfg, _1, _2, std::ref(cfg_3field), std::ref(files), std::ref(saves_dir)));
	}
}
*/
void save_map_internal(const nav_msgs::OccupancyGrid& map, const std::map<std::string, tmap_position>& positions, const std::map<std::string, tmap_marker>& markers, const std::string& rspfile)
{
	VALIDATE(map.info.width > 0 && map.info.height > 0, null_str);
	VALIDATE(positions.count(charge_pos_uuid) != 0, null_str);

	const int cells = map.info.width * map.info.height;
	const int8_t* map_data = &map.data[0];
    for (int at = 0; at < cells; at ++) {
        const int8_t raw_i8 = map_data[at];
        VALIDATE(raw_i8 >= -1 && raw_i8 <= MAX_CARTOGRAPHER_CELL_VAL, null_str);
    }

    ros::SerializedMessage serialized = ros::serialization::serializeMessage<nav_msgs::OccupancyGrid>(map);
	int map_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
	const uint8_t* serialized_map_data = serialized.message_start;

	const std::string bundleid = "com.kos.launcher";
	tsha1writer sha1file(rspfile, nposm, std::bind(&did_write_rsp_rosmap, _1, bundleid, std::ref(game_config::rose_version), null_str, serialized_map_data, map_len, positions, markers));
	sha1file.write();
}

void save_map_by_ros_map(const tros_map& ros_map)
{
    VALIDATE(ros_map.valid(), null_str);
    save_map_internal(ros_map.map, ros_map.positions, ros_map.markers, ros_map.rspfile);
}

bool support_rosbag(int mode)
{
	return mode == mode_buildmap;
}

const tgui_mode& find_gui_mode(int mode, int rosbag)
{
    if (!support_rosbag(mode)) {
		VALIDATE(rosbag == nposm, null_str);
	}

    for (std::vector<tgui_mode>::const_iterator it = game_config::gui_modes.begin(); it != game_config::gui_modes.end(); ++ it) {
        const tgui_mode& gui_mode = *it;
        if (gui_mode.mode == mode && gui_mode.rosbag == rosbag) {
            return gui_mode;
        }
    }

    VALIDATE(false, "program error. must not here");
    return *game_config::gui_modes.begin();
}

bool is_valid_charge_width(int width_mm)
{
	return width_mm >= charge_width_range.min && width_mm <= charge_width_range.max;
}

int tspecial_mapop_mode_lock::mode = nposm;
