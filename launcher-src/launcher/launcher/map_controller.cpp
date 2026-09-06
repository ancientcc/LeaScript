#define GETTEXT_DOMAIN "launcher-lib"

/*
 * How to use...
 * display_lock lock(game.disp());
 * hotkey::scope_changer changer(game.app_cfg(), "hotkey_ocr");
 * map_controller controller(game.app_cfg(), game.video());
 * controller.initialize(display::ZOOM_72);
 * int ret = controller.main_loop();
 */

#include "map_controller.hpp"
#include "map_display.hpp"
#include "gui/dialogs/map_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/track.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/menu.hpp"
#include "game_config.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_version.hpp"
#include "formula_string_utils.hpp"
#include "base_instance.hpp"

#include <opencv2/imgproc.hpp>
#include <SDL_image.h>

#include <sensor_msgs/LaserScan.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/utils.h>

#include <rose_ros/utils.hpp>
#include <rose_ros/cartographer_utils.h>
#include <rose_ros/kidnap.hpp>

#include <costmap_2d/cost_values.h>
#include <costmap_2d/costmap_2d.h>
#include <ros/serialized_message.h>
#include "rose_sdl_utils.hpp"

#include "gui/dialogs/float_panel.hpp"

using namespace std::placeholders;


namespace gui2 {
class tmarker_placer: public tfloat_panel
{
public:
	tmarker_placer(map_controller& controller, gui2::twindow& window);

private:
	void did_item_clicked(const tdraw_item& item) override;

private:
    map_controller& controller_;
};

tmarker_placer::tmarker_placer(map_controller& controller, twindow& window)
	: tfloat_panel(window)
    , controller_(controller)
{
	std::pair<std::map<int, tdraw_item>::iterator, bool> ins = draw_items_.insert(std::make_pair(POST_MSG_ROTATE, 
		tdraw_item(POST_MSG_ROTATE, "misc/placer_rotate.png")));

	draw_items_.insert(std::make_pair(POST_MSG_SCALE, tdraw_item(POST_MSG_SCALE, "misc/placer_scale.png")));
	draw_items_.insert(std::make_pair(POST_MSG_OK, tdraw_item(POST_MSG_OK, "misc/placer_ok.png")));
	draw_items_.insert(std::make_pair(POST_MSG_CANCEL, tdraw_item(POST_MSG_CANCEL, "misc/placer_cancel.png")));
}

void tmarker_placer::did_item_clicked(const tdraw_item& item)
{
	if (item.msg_id == POST_MSG_CANCEL || item.msg_id == POST_MSG_OK) {        
        tbar& bar = bar_;
        int x = bar.std_rect.x + bar.size;
        int y = bar.std_rect.y + bar.std_rect.h / 2;

        base_instance::tmsg_data_other_id* pdata = new base_instance::tmsg_data_other_id(
            std::bind(&map_controller::close_map_marker, &controller_, _1, x, y, bar.line_width_meter, bar.theta, false));
		rtc::Thread::Current()->Post(RTC_FROM_HERE, &instance->msg_handler(), item.msg_id, pdata);
	}
}

}

const bool buildmap_with_move_base = true;

static void UpdateDepthImage(cv::Mat& image, const sensor_msgs::LaserScan& scan_msg, const SDL_2Point& charging, int pixels_per_meter, int mode)
{
    VALIDATE(pixels_per_meter > 0, null_str);

    const int kWindowHeight = pixels_per_meter * 12;
    const int kWindowWidth = pixels_per_meter * 12;

    int min_angle = 0;
    int max_angle = 359;
    // const float fov = (float)(ctx.depth_fov / 180 * CV_PI);

    const float fov_start = (float)scan_msg.angle_min;
    const float fov_end = (float)scan_msg.angle_max;

    if (scan_msg.ranges.empty()) {
        return;
    }

    int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

    // ctx.image.create(kWindowHeight, kWindowWidth, CV_8UC3);
    image.create(kWindowHeight, kWindowWidth, CV_8UC4);
    image.setTo(0x00);
    // circle diagram
    // dots
    cv::Scalar point_color(0x00, 0x00, 0xff, 0xff);
    for (int idx = 0; idx < (int)scan_msg.ranges.size(); idx++) {
        if (scan_msg.ranges[idx] == std::numeric_limits<float>::infinity()) {
            continue;
        }
        if (idx >= (int)scan_msg.ranges.size() * 3 / 4) {
            // continue;
        }
        float depth_data = scan_msg.ranges[idx] * pixels_per_meter;
        float rou = fov_start + idx * scan_msg.angle_increment;
        float dx = cos(rou) * depth_data;
        float dy = sin(rou) * depth_data;
        int x = (int)(dx + kWindowWidth / 2);
        int y = (int)(kWindowHeight / 2 - dy);
        cv::circle(image, cv::Point(x, y), 1, point_color, -1);

        int int100_x = (int)(dx);
		int int100_y = (int)(dy);
        posix_touch_i32(int100_x, int100_y, &min_x, &min_y, &max_x, &max_y);
    }

    if (charging.x1 != nposm) {
        const int mm5_per_meter = 250;
        const int ratio = mm5_per_meter / pixels_per_meter;
        cv::Point pt1(charging.x1 / ratio + kWindowWidth / 2, kWindowHeight / 2 - charging.y1 / ratio);
        cv::Point pt2(charging.x2 / ratio + kWindowWidth / 2, kWindowHeight / 2 - charging.y2 / ratio);
        cv::line(image, pt1, pt2, cv::Scalar(0x00, 0xff, 0x00, 0xff), 1, cv::LINE_AA);
    }
}

static void mapToWorld(const ::nav_msgs::MapMetaData& info, unsigned int mx, unsigned int my, double& wx, double& wy)
{
    double origin_x_ = info.origin.position.x;
    double origin_y_ = info.origin.position.y;
    double resolution_ = info.resolution;

  wx = origin_x_ + (mx + 0.5) * resolution_;
  wy = origin_y_ + (my + 0.5) * resolution_;
}

static bool worldToMap(const ::nav_msgs::MapMetaData& info, 
    double wx, double wy, unsigned int& mx, unsigned int& my)
{
    double origin_x_ = info.origin.position.x;
    double origin_y_ = info.origin.position.y;
    unsigned int size_x_ = info.width;
    unsigned int size_y_ = info.height;
    double resolution_ = info.resolution;

    if (wx < origin_x_ || wy < origin_y_)
        return false;

    mx = (int)((wx - origin_x_) / resolution_);
    my = (int)((wy - origin_y_) / resolution_);

    if (mx < size_x_ && my < size_y_) {
        return true;
    }

    return false;
}

static void worldToMapNoBounds(double origin_x_, double origin_y_, double resolution_, double wx, double wy, int& mx, int& my)
{
    mx = (int)((wx - origin_x_) / resolution_);
    my = (int)((wy - origin_y_) / resolution_);
}


void trobot_imu::set_base_pitch(double pitch)
{
    VALIDATE(!has_magnetometer, null_str);
	base.pitch = pitch;
}

void trobot_imu::set_base_yaw(tbase_driver_core& base_driver, double robot_yaw)
{
	VALIDATE(!has_magnetometer, null_str);

    VALIDATE(IN_MAIN_THREAD(), null_str);
    // call it to refresh, and make aplt::valuex.euler[2] as the latest value,
	base_driver.slice();
    double yaw = robot_yaw - aplt::valuex.euler[2];

    kidnap.add_dbg_msg_update_base_yaw(tkidnap::msg_update_base_yaw, base.yaw, yaw);
	base.yaw = yaw;

    require_full2_position = false;
	last_full2_ticks = SDL_GetTicks();
}

static std::string generate_map_data2(int width, int height, bool colorful)
{
    VALIDATE((width % UNIT_LOCS) == 0 && (height % UNIT_LOCS) == 0, null_str);
    return generate_map_data(width, height, colorful, square_terrain_white);
}

map_controller* map_controller::sigleton = nullptr;

#define DEFAULT_LASER_PIXELS_PER_METER  100
#define MAX_MAP_W   60 // 60m, at least make safe 50m
#define MAX_MAP_H   60 // 60m, at least make safe 50m

map_controller::map_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tdrivers& drivers, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera, 
    tros_map& curmap, tbase_driver& base_driver, tmoveit_driver& moveit_driver, const trobot_imu& robot_imu, const config& app_cfg, CVideo& video, 
    std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task, const std::string& saves_map_dir)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
    , privacy_(privacy)
    , drivers_(drivers)
    , ros_instance_(ros_instance)
    , dcamera_driver_(dcamera_driver)
    , camera_(camera)
    , curmap_(curmap)
    , base_driver_(base_driver)
    , moveit_driver_(moveit_driver)
    , robot_imu_(robot_imu)
    , moveit_aplt_task_(moveit_aplt_task)
    , saves_map_dir_(saves_map_dir)
    // , mapviewer_(ros_instance.has_task())
    , mapviewer_(ros_instance.in_map_viewer())
    , disable_new_aplt_lock_(aplt::tdisable_new_klink_task_lock::reason_map)
	, gui_(nullptr)
    , units_(*this, map_, false)
	, map_(null_str)
    , dlg_(nullptr)
    , window_(nullptr)
    , reserve_rspfile_index_(0)
    , cancel_goal_widget_(nullptr)
    , save_map_widget_(nullptr)
    , switch_slam_widget_(nullptr)
    , auto_buildmap_widget_(nullptr)
    , reposition_robot_widget_(nullptr)
    , start_widget_(nullptr)
    , slam_major_widget_(nullptr)
    , slam_minor_widget_(nullptr)
    , cur_mode_(find_gui_mode(preferences::mapop_mode(), nposm))
    , auto_buildmap_(false)
    , position_tl_offset_(SDL_Point{0, 0})
    , longpressing_position_(nullptr)
    , position_downing_at_(SDL_FPoint{float_nposm, float_nposm})
    , will_longpress_ticks_(0)
    , downing_pt_(construct_null_coordinate()) 
    , last_map_origin_({0, 0})
    , last_map_offset_({0, 0})
    , laser_png_rotate_point_({0, 0})
    , current_goal_png_rotate_point_({nposm, nposm})
    , current_goal_offset_({nposm, nposm})
    , last_laser_origin_({nposm, nposm})
    , last_laser_offset_({0, 0})
    , laser_pixels_per_meter_(DEFAULT_LASER_PIXELS_PER_METER)
    , last_laser_yaw_(0)
    , with_laserscan_(true)
    , show_help_(!ros_instance_.has_task())
    , next_kidnap_msg_ticks_(0)
    , kdinap_track_top_gap_h_(0)
    , next_1second_ticks_(0)
    , global_plan_(nullptr)
	, global_plan_size_(0)
	, global_plan_vsize_(0)
    , local_plan_vsize_(0)
    , last_local_plan_start_({nposm, nposm})
    , map_saving_(false)
    , denoise_map_data_(nullptr)
    , denoise_map_data_size_(0)
	, denoising_map_(false)
    , slam_track_4_dcamera_(false)
    , draw_dcamera_points_settings_(tdraw_dcamera_points_settings{6, 0.2})
    , rosbag_play_stop_ticks_(0)
{
    int tkidnap_tdbg_msg_size = sizeof(tkidnap::tdbg_msg);

    if (mapviewer_) {
        VALIDATE(cur_mode_.mode == mode_navigation, null_str);
    }

    sigleton = this;
    memset(areas_denoised_, 0, sizeof(areas_denoised_));
    // {r, g, b, a}
    colors_[color_denoise_reserve] = SDL_Color{0, 255, 0, 255};
    colors_[color_denoise_erase] = SDL_Color{255, 0, 0, 255};

	map_ = tmap(generate_map_data2(6, 4, false));
	units_.create_coor_map(map_.w(), map_.h());

    goal_.target_pose.header.seq = 0;

    VALIDATE(MAX_MAP_W % UNIT_LOCS == 0 && MAX_MAP_H % UNIT_LOCS == 0, null_str);

    map_pin_surf_ = image::get_image("misc/map_pin.png");
    VALIDATE(map_pin_surf_.get() != nullptr, null_str);
    if (game_config::os == os_windows) {
        position_tl_offset_ = SDL_Point{map_pin_surf_->w, map_pin_surf_->h};
    } else {
        // To be sure 144 * gui2::twidget::hdpi_scale is too large.
        position_tl_offset_ = SDL_Point{(int)(72 * gui2::twidget::hdpi_scale), map_pin_surf_->h};
    }
    map_pin_pt_.x = map_pin_surf_->w / 2;
    map_pin_pt_.y = map_pin_surf_->h - 2;

    collect_rsp_files(saves_map_dir_, files_);
    ros_instance_.register_slot(*this);

    cartographer::rose_slot.hdpi_scale = gui2::twidget::hdpi_scale;
    ros_instance_.set_msg_sink(this);
}

map_controller::~map_controller()
{
    marker_placer_.reset();

    sigleton = nullptr;
    ros_instance_.set_msg_sink(nullptr);
    cartographer::rose_slot.switch_page(cartographer::trose_slot::NORMAL_PAGE);
/*
    if (!mapviewer_) {
        if (ros_instance_.navigation_node_started()) {
            ros_instance_.stop_navigation_node();
        }
    }
*/
    ros_instance_.deregister_slot(*this);
	if (gui_) {
		delete gui_;
		gui_ = nullptr;
	}
	// rose_ros::ros_ctrl_exit();
    if (global_plan_ != nullptr) {
        free(global_plan_);
        global_plan_ = nullptr;
    }

    if (denoise_map_data_ != nullptr) {
        VALIDATE(denoise_map_data_size_ > 0, null_str);
        free(denoise_map_data_);
        denoise_map_data_ = nullptr;
    }
}

void map_controller::app_create_display(int initial_zoom)
{
	gui_ = new map_display(rdpd_mgr_, pble_, privacy_, ros_instance_, dcamera_driver_, camera_, *this, units_, video_, map_, initial_zoom);
}


void map_controller::app_post_initialize()
{
    dlg_ = static_cast<gui2::tmap_scene*>(gui_->get_theme());
    window_ = dlg_->get_window();

    VALIDATE(marker_placer_.get() == nullptr, null_str);
	marker_placer_.reset(new gui2::tmarker_placer(*this, *window_));

    const int zoom = gui_->zoom();

    int w = map_.w();
    int h = map_.h();
    const int unit_size = zoom * UNIT_LOCS;
    for (int row = 0; row < h; row += UNIT_LOCS) {
        for (int col = 0; col < w; col += UNIT_LOCS) {
            map_unit* u = new map_unit(*this, *gui_, units_);
            SDL_Rect rect{col * zoom, row * zoom, unit_size, unit_size};
            u->set_rect(rect);
	        units_.insert2(*gui_, u);

            const map_location& loc = u->get_location();
            VALIDATE(loc.x == col && loc.y == row, null_str);
        }
    }

    // did_run_state_changed
    cancel_goal_widget_ = window_->find_float_widget("cancel_goal");
    save_map_widget_ = window_->find_float_widget("save_map");
    switch_slam_widget_ = window_->find_float_widget("switch_slam");
    auto_buildmap_widget_ = window_->find_float_widget("auto_buildmap");
    reposition_robot_widget_ = window_->find_float_widget("reposition_robot");
    start_widget_ = gui2::find_widget<gui2::tbutton>(window_, "start", false, true);


    gui2::ttrack* track = gui2::find_widget<gui2::ttrack>(window_, "slam_major_track", false, true);
	track->set_did_draw(std::bind(&map_controller::did_draw_cartographer, this, _1, _2, _3, track_major));
    track->set_did_create_background_tex(std::bind(&map_controller::did_create_background_tex, this, _1, _2, track_major));
    track->connect_signal<gui2::event::LONGPRESS>(
		std::bind(
			&map_controller::signal_handler_longpress_cartographer
			, this
			, _4, _5, track_major)
		, gui2::event::tdispatcher::back_child);
    slam_major_widget_ = track;

    track = gui2::find_widget<gui2::ttrack>(window_, "slam_minor_track", false, true);
    if (!slam_track_4_dcamera_) {
	    track->set_did_draw(std::bind(&map_controller::did_draw_cartographer, this, _1, _2, _3, track_minor));
        track->set_did_create_background_tex(std::bind(&map_controller::did_create_background_tex, this, _1, _2, track_minor));
	    track->connect_signal<gui2::event::LONGPRESS>(
		    std::bind(
			    &map_controller::signal_handler_longpress_cartographer
			    , this
			    , _4, _5, track_minor)
		    , gui2::event::tdispatcher::back_child);
    }
    slam_minor_widget_ = track;

    gui2::twidget* widget = window_->find("_main_map", false);
    widget->connect_signal<gui2::event::LONGPRESS>(
		std::bind(
			&map_controller::longpress_widget, this,
			_4, _5, std::ref(*window_)), gui2::event::tdispatcher::back_child);

    tdrivers::tvars vars = drivers_.curvars(false);
    std::stringstream ss;
    ss << game_config::driver_names.find(apltsotype_base)->second << " " << vars.base.to_str() << "\n";
    ss << game_config::driver_names.find(apltsotype_laser)->second << " " << vars.laser.to_str();
    gui_->refresh_report(gui2::tmap_scene::DRIVER, 
            reports::report(ss.str(), null_str));

    if (cur_mode_.mode == mode_position) {
        set_status_report(_("Click on the red dot to choose to edit the wall"));
    }

    VALIDATE(navigation_rspfile_.empty(), null_str);
    // if (!ros_instance_.has_task()) {
    if (!mapviewer_) {
        VALIDATE(!mapviewer_, null_str);
        dlg_->did_run_state_changed(cur_mode_.mode, false);

    } else {
        VALIDATE(mapviewer_, null_str);
        if (moveit_aplt_task_.get() == nullptr) {
            // VALIDATE(ros_instance_.navigation_node_started(), null_str);
        }

        start_widget_->set_label("misc/stop96.png");
        start_widget_->set_active(false);

        dlg_->did_run_state_changed(cur_mode_.mode, true);
    }


}

void map_controller::app_play_slice()
{
    VALIDATE(ros_instance_.started(), null_str);
    if (cur_mode_.mode == mode_buildmap || cur_mode_.mode == mode_navigation) {
        if (cur_mode_.mode == mode_navigation) {
            calculate_laser_origin();
        } else if (auto_buildmap_ && !ros_instance_.goaling() && ros_instance_.move_base_constructed()) {
            // mode_ == mode_buildmap
            send_exploration_detect();
        }
    }

    std::string euler_z_msg;

    bool navigation_node_started = ros_instance_.navigation_node_started();
    if (cur_mode_.rosbag == nposm || !navigation_node_started) {
        euler_z_msg = ros_instance_.get_imu_desc();

    } else {
        VALIDATE(cur_mode_.mode == mode_buildmap, null_str);
        char buf[128] = "-- / --";
        if (cur_mode_.rosbag == rosbag_record) {
            int elapse = (SDL_GetTicks() - ros_instance_.navigation_start_ticks()) / 1000;
            SDL_snprintf(buf, sizeof(buf), "%s %s", utils::format_second_24hoursys(elapse).c_str(),
                utils::format_i64size(ros_instance_.rosbag_record_result().fsize).c_str());
        } else {
            const rosbag::tplay_result& result = ros_instance_.rosbag_play_result();
            if (!is_float_nposm(result.length_time_s)) {
                int curr_s = (int)result.current_time_s;
                int length_s = (int)result.length_time_s;

                SDL_snprintf(buf, sizeof(buf), "%s / %s", utils::format_second_24hoursys(curr_s).c_str(),
                    utils::format_second_24hoursys(length_s).c_str());
            }
        }

        euler_z_msg = buf;
    }
    gui_->refresh_report(gui2::tmap_scene::EULER_Z, reports::report(euler_z_msg, null_str));

    const std::string battery_level_str = HAS_BATTERY()? str_cast(aplt::valuex.battery_level): "NoBat";
    gui_->refresh_report(gui2::tmap_scene::BATTERY, 
            reports::report(battery_level_str, null_str));
    bool start_active = !mapviewer_ && !ros_instance_.has_task() && cur_mode_.mode <= mode_maxros && instance->fg_aplt() == nullptr && !denoising_map_;
    if (start_active && !start_widget_->get_active()) {
        start_widget_->set_active(true);
    }

    uint32_t now = SDL_GetTicks();
    if (now > next_1second_ticks_) {
        dlg_->refresh_statusbar_grid(now);

        const int threshold_1s = 1000;
        next_1second_ticks_ = now + threshold_1s;
    }

    if (kidnap.dbg_msgs_dirty && is_kidnap_page() && now >= next_kidnap_msg_ticks_) {
        if (navigation_node_started) {
            immediate_draw_major_slam();
            const int threshold_ms = 1000;
            next_kidnap_msg_ticks_ = SDL_GetTicks() + threshold_ms;
        }
    }

    if (rosbag_play_stop_ticks_ != 0 && SDL_GetTicks() >= rosbag_play_stop_ticks_) {
        VALIDATE(ros_instance_.use_external_laser() && navigation_node_started, null_str);
        rosbag_play_stop_ticks_ = 0;
        // stop buildmap with rosbag play.
        click_start();
    }
}

void map_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;

	switch (command) {
		case tmap_scene::HOTKEY_RETURN:
/*
            if (mode_ == mode_navigation && ros_instance_.has_task() && ros_instance_.task()->bh == navigation_bh_moveit){
                // when navigation_bh_moveit, now require a track-widget,
                // so once map_controller destroy, require destroy task.
                cancel_goal();
            }
*/
			do_quit_ = true;
			break;

		case tmap_scene::HOTKEY_START:
			click_start();
			break;

        case tmap_scene::HOTKEY_SAVE_MAP:
            click_save_map();
            break;

        case tmap_scene::HOTKEY_AUTO_BUILDMAP:
            click_auto_buildmap();
            break;

        case tmap_scene::HOTKEY_CANCEL_GOAL:
            click_cancel_goal();
            break;

        case tmap_scene::HOTKEY_SWITCH_SLAM:
            click_switch_slam();
            break;

        case tmap_scene::HOTKEY_DENOISE_MAP:
            {
                // save_map_by_ros_map(curmap_);
                // gui2::show_message(null_str, curmap_.rspfile);
                // break;
            }
            click_denoising_map();
            break;

        case tmap_scene::HOTKEY_REPOSITION_ROBOT:
            click_reposition_robot(tkidnap::reason_manual);
            break;

        case tmap_scene::HOTKEY_CONFIRM_OK:
        case tmap_scene::HOTKEY_CONFIRM_CANCEL:
            click_confirm(command == tmap_scene::HOTKEY_CONFIRM_OK);
            break;
/*
        case HOTKEY_ZOOM_IN:
        case HOTKEY_ZOOM_OUT:
            click_reposition_robot(tkidnap::reason_manual_second);
            break;
*/
		case HOTKEY_SYSTEM:
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

void map_controller::app_left_mouse_up(const int x, const int y, const bool click)
{
    if (!click) {
        return;
    }

    if (!denoising_map_) {
        if (cur_mode_.mode != mode_position || marker_placer_->is_visible()) {
            return;
        }
        std::map<std::string, tmap_marker>& markers = curmap_.markers;
        for (std::map<std::string, tmap_marker>::const_iterator it = markers.begin(); it != markers.end(); ++ it) {
            const tmap_marker& marker = it->second;
            SDL_Point mouse_xy = meter_2_mouse_xy(marker.rsp.x, marker.rsp.y);
            int radius = 10 * gui2::twidget::hdpi_scale;
            SDL_Rect fix_pt_rect = ::create_rect(mouse_xy.x - radius, mouse_xy.y - radius, radius * 2, radius * 2);
            if (point_in_rect(x, y, fix_pt_rect)) {
                edit_map_marker(marker.rsp.uuid);
                break;
            }
        }
        return;
    }

    SDL_Rect view_rect = gui_->main_map_view_rect();
    if (!point_in_rect(x, y, view_rect)) {
        return;
    }

    SDL_DPoint ros_map_xy = mouse_xy_2_ros_world_xy(x, y);

    int map_x;
    int map_y;
    worldToMapNoBounds(last_map_.origin.position.x, last_map_.origin.position.y, last_map_.resolution, ros_map_xy.x, ros_map_xy.y, map_x, map_y);

    const int width = last_map_.width;
    const int height = last_map_.height;
    if (map_x < 0 || map_x >= width || map_y < 0 || map_y >= height) {
        return;
    }

    SDL_Log("%u app_left_mouse_up(x:y(%i, %i), click:%s) -> ros_map_xy: (%.3f, %.3f), map:(%i, %i[raw:%i])", 
        SDL_GetTicks(), x, y, click? "true": "false", ros_map_xy.x, ros_map_xy.y, map_x, height - 1 - map_y, map_y);
    map_y = height - 1 - map_y;

    uint8_t denoise_u8 = denoise_map_data_[map_y * width + map_x];
    if (denoise_u8 < MIN_DENOISE_AREA_INDEX || denoise_u8 > MAX_DENOISE_AREA_INDEX) {
        return;
    }

    const int area_index = denoise_u8 - MIN_DENOISE_AREA_INDEX;

    areas_denoised_[area_index] = !areas_denoised_[area_index];
    const SDL_Color& color = colors_[areas_denoised_[area_index]? color_denoise_erase: color_denoise_reserve];
    for (int y = 0; y < height; y ++) {
        uint32_t* to_data = last_denoise_bgra_.ptr<uint32_t>(y);
        int yoffset = y * width;
        for (int x = 0; x < width; x ++) {
            uint8_t denoise_u8 = denoise_map_data_[yoffset + x];
            if (denoise_u8 - MIN_DENOISE_AREA_INDEX == area_index) {
                to_data[x] = posix_mku32(posix_mku16(color.b, color.g), posix_mku16(color.r, color.a));
            }
        }
    }
}

void map_controller::pre_start()
{
    last_map_.width = 0;
    last_map_.height = 0;

    last_map_origin_ = SDL_Point{0, 0};
    last_map_offset_ = SDL_Point{0, 0};
    last_laser_origin_ = SDL_Point{nposm, nposm};
    last_laser_offset_ = SDL_Point{0, 0};

    last_src_gray_ = cv::Mat();
    last_laser_mat_ = cv::Mat();

    laser_png_surf_ = nullptr;

    reset_movebase_variables();
}

void map_controller::reset_movebase_variables()
{
    ros_global_plan_.poses.clear();
    global_plan_vsize_ = 0;

    ros_local_plan_.poses.clear();
    local_plan_vsize_ = 0;
    current_goal_offset_.x = nposm;

    ros_current_goal_.valid = false;
}

int show_rosbag_warning(const tgui_mode& mode, const tros_instance& ros_instance)
{
    VALIDATE(support_rosbag(mode.mode), null_str);
    VALIDATE(mode.rosbag != nposm, null_str);

    utils::string_map symbols;
    symbols["mode"] = find_gui_mode(mode.mode, nposm).name;
    symbols["filename"] = game_config::rosbag_filename;
    std::string msg;
    
    if (mode.rosbag == rosbag_record) {
        const rosbag::trecord_input& input = ros_instance.rosbag_record_input();
        symbols["topics"] = utils::join(input.topics);
        symbols["max_duration"] = utils::format_elapse_hms(input.max_duration_s);
        msg = vgettext2("rosbag warning, record, $mode, $filename, $max_duration, $topics", symbols);

    } else {
        VALIDATE(mode.rosbag == rosbag_play, null_str);

        symbols["topics"] = "/scan";
        msg = vgettext2("rosbag warning, play, $mode, $filename, $topics", symbols);
    }
    return gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons);
}

void map_controller::click_start()
{
    const bool test = false;
    if (test) {
        if (node1_.get() == nullptr) {
            // node1_.reset(new net::tworker(std::bind(test__talker, _1), NULL, NULL, NULL, "talker"));
            // node2_.reset(new net::tworker(std::bind(test__listener, _1), NULL, NULL, NULL, "listener"));

            // node1_.reset(new net::tworker(std::bind(test__transform, _1), NULL, NULL, NULL, "talker"));
            {
                node1_.reset(new net::tworker(std::bind(rosbag__play, _1, ros_instance_.rosbag_play_input(), 
                    std::ref(ros_instance_.rosbag_play_result())), 
                    NULL, NULL, NULL, "rosbag_play_node"));
            }
            start_widget_->set_label("misc/stop96.png");

        } else {
            start_widget_->set_label("misc/start.png");
            node1_.reset();
            node2_.reset();
        }
        return;
    }

    VALIDATE(cur_mode_.mode >= 0 && cur_mode_.mode <= mode_maxros, null_str);

    if (!ros_instance_.navigation_node_started()) {
        if (cur_mode_.mode == mode_navigation) {
            if (navigation_rspfile_.empty()) {
                gui2::show_message(null_str, _("Before navigating, select the map."));
                return;
            }
        }
        if (cur_mode_.rosbag != nposm) {
            if (show_rosbag_warning(cur_mode_, ros_instance_) != gui2::twindow::OK) {
                return;
            }
        }
    }

    tdrivers::tvars curvars = drivers_.curvars(true);
    if (!ros_instance_.check_navigation_env(curvars, nullptr, true)) {
        return;
    }

    faplt_serial_driver_main base_main = curvars.base.driver_main;
    faplt_serial_driver_main laser_main = curvars.laser.driver_main;
    VALIDATE(base_main != nullptr && laser_main != nullptr, null_str);

    dlg_->did_run_state_changed(cur_mode_.mode, !ros_instance_.navigation_node_started());

    if (!ros_instance_.navigation_node_started()) {

        pre_start();
        switch_slam_widget_->set_visible(true);
        if (cur_mode_.mode == mode_navigation) {
            VALIDATE(!mapviewer(), null_str);
            reposition_robot_widget_->set_visible(true); // game_config::os == os_windows
        }

        start_widget_->set_label("misc/stop96.png");

        ros_instance_.start_navigation_node(cur_mode_.mode, cur_mode_.rosbag, curvars, navigation_rspfile_);
        const bool start_moveit_auto = true;
        
        if (cur_mode_.mode == mode_navigation) {
            navigation_rspfile_.clear();
            navigation_markers_.clear();
        }
        set_show_help(false);

        if (ros_instance_.use_external_laser()) {
            // rosbag_play_stop_ticks_ = SDL_GetTicks() + (12 * 60 + 30) * 1000; 
            rosbag_play_stop_ticks_ = SDL_GetTicks() + (30 * 60) * 1000; 
        } else {
            rosbag_play_stop_ticks_ = 0;
        }

        if (ros::get_use_follow()) {
            // dlg_->start_follow();
        }

	} else {
        start_widget_->set_label("misc/start.png");
        cancel_goal_widget_->set_visible(false);
        save_map_widget_->set_visible(false);

        switch_slam_widget_->set_visible(false);

        auto_buildmap_widget_->set_visible(false);
        reposition_robot_widget_->set_visible(false);

        ros_instance_.stop_navigation_node();

        base_footprint_pose_.valid = false;
        if (cur_mode_.mode == mode_buildmap) {
            auto_buildmap_ = false;
        } else {
            VALIDATE(!auto_buildmap_, null_str);
        }
        set_auto_buildmap_label();

        // app_play_slice() maybe not called.

        rosbag_play_stop_ticks_ = 0;
        if (ros::get_use_follow()) {
           // dlg_->stop_follow();
        }
	}
}

void map_controller::set_auto_buildmap_label()
{
    gui2::twindow* window_ = dlg_->get_window();
    // const std::string label = auto_buildmap_? _("Stop auto-buildmap"): _("Auto buildmap");
    const std::string label = auto_buildmap_? "misc/auto_buildmap_cancel.png": "misc/auto_buildmap.png";
    gui2::find_widget<gui2::tcontrol>(window_, "auto_buildmap", false, true)->set_label(label);
}

void map_controller::save_map_when_buildmap(const nav_msgs::OccupancyGrid& map)
{
    VALIDATE(cur_mode_.mode == mode_buildmap, null_str);

    char rspfile[MAX_PATH];
    uint32_t last_max_index = -1;
    if (!files_.empty()) {
        const std::string short_file = utils::extract_file(*files_.rbegin());
        std::string name = short_file.substr(3, 4);
        last_max_index = utils::to_uint32(name);
    }
    if (last_max_index + 1 == reserve_rspfile_index_) {
        last_max_index ++;
    }

    SDL_snprintf(rspfile, sizeof(rspfile), "%s/map%04u.rsp", saves_map_dir_.c_str(), last_max_index + 1);

    std::map<std::string, tmap_position> positions;
    positions.insert(std::make_pair(charge_pos_uuid, ros::get_special_position(charge_pos_uuid)));

    save_map_internal(map, positions, std::map<std::string, tmap_marker>(), rspfile);

    files_.insert(rspfile);
    last_saved_rspfile_ = rspfile;
}

void map_controller::save_map_when_position() const
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    VALIDATE(curmap_.valid(), null_str);

    save_map_by_ros_map(curmap_);
}

void map_controller::click_debug(gui2::tbutton& widget)
{
    VALIDATE(cur_mode_.mode == mode_navigation, null_str);

    utils::string_map symbols;
    bool open_or_close = false;

    std::string serial_path;
    
    enum {costmap_2_png, costmap_2_cell_value_png, visual_laser, intl_buildings};

    std::vector<gui2::tmenu::titem> items;
    items.push_back(gui2::tmenu::titem(_("1.costmap to 2.png"), costmap_2_png, null_str));
    items.push_back(gui2::tmenu::titem(_("1.costmap to 2-cell_value.png"), costmap_2_cell_value_png, null_str));
    items.push_back(gui2::tmenu::titem(_("visual laser"), visual_laser, null_str));
    items.push_back(gui2::tmenu::titem(_("intl buildings"), intl_buildings, null_str));

    int selected;
	{
		gui2::tmenu dlg(items, nposm);
		dlg.show(widget.get_x(), widget.get_y() - widget.get_height() * 2 - 4 * gui2::twidget::hdpi_scale);
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		// absolute_draw();
		selected = dlg.selected_val();
	}

    if (selected == costmap_2_png) {
        const std::string filename = game_config::preferences_dir + "/1.costmap";

        costmap_2d::tcostmap_header header;
        surface surf = ros::costmap2d_file_2_SDL_Surface(filename, header);
        if (surf.get() == nullptr) {
            symbols["file"] = filename;
            gui2::show_message(null_str, vgettext2("Open fail: $file", symbols));
            return;
        }

        char buf[128];
        SDL_snprintf(buf, sizeof(buf), "tf: (%.3f, %.3f, %.3f) yaw: %.3f src: (%i, %i) dst: (%i, %i)", 
            header.odom_2_map_pose.x, header.odom_2_map_pose.y, RAD2DEG(header.odom_2_map_pose.yaw), RAD2DEG(header.robot_yaw), header.src.x, header.src.y, header.dst.x, header.dst.y);

        ros::calcuate_target_pose2d(tpose2d(header.robot_x, header.robot_y, header.robot_yaw), header.odom_2_map_pose, true);

	    const int margin_x = COSTMAP2D_SURF_MARGIN_X;
        const int margin_y = COSTMAP2D_SURF_MARGIN_Y;
	    const int cell_size = COSTMAP2D_SURF_CELL_SIZE;

	    surf = surf_overlay_mark(surf, 0xff000000, margin_x, margin_y, cell_size, true, buf);
        VALIDATE(surf.get() != nullptr, null_str);

        const std::string to_file = "2.png";
        imwrite(surf, to_file);
        symbols["file"] = to_file;
        gui2::show_message(null_str, vgettext2("costmap to png finisehd. flie: $file", symbols));

    } else if (selected == costmap_2_cell_value_png) {
        const std::string filename = game_config::preferences_dir + "/1.costmap";
        costmap_2d::tcostmap_header header;
        costmap_2d::Costmap2D costmap;

        memset(&header, 0, sizeof(header));
        std::vector<geometry_msgs::PoseStamped> transformed_plan;
        std::vector<geometry_msgs::Point> footprint;
        bool valid = costmap.loadMap(filename, header, transformed_plan, footprint);
        if (!valid) {
            symbols["file"] = filename;
            gui2::show_message(null_str, vgettext2("Open fail: $file", symbols));
            return;
        }


        const uint8_t* costmap_data = costmap.getCharMap();
        const int width = costmap.getSizeInCellsX();
        const int height = costmap.getSizeInCellsY();

        const int margin_x = COSTMAP2D_SURF_MARGIN_X;
        const int margin_y = COSTMAP2D_SURF_MARGIN_Y;
	    const int cell_size = 16;
        surface surf = u8_data_2_cell_value_surf(costmap_data, margin_x, margin_y, width, height, maptype_costmap, nullptr, nullptr);

        char buf[128];
        SDL_snprintf(buf, sizeof(buf), "yaw: %.3f src: (%i, %i) dst: (%i, %i)", 
            RAD2DEG(header.robot_yaw), header.src.x, header.src.y, header.dst.x, header.dst.y);

	    surf = surf_overlay_mark(surf, 0xff000000, margin_x, margin_y, cell_size, true, buf);
        VALIDATE(surf.get() != nullptr, null_str);

        const std::string to_file = "2-cell_value.png";
        imwrite(surf, to_file);

        symbols["file"] = to_file;
        gui2::show_message(null_str, vgettext2("costmap to cell_value png finisehd. flie: $file", symbols));

    } else if (selected == visual_laser) {

        
    } else if (selected == intl_buildings) {
        tintl_buildings buildings;
        tintl_loaded_l10nfiles loaded_l10nfiles;
        SDL_Log("%s", loaded_l10nfiles.to_string().c_str());

        gui2::show_message(null_str, ht::generate_format(buildings.to_string(), 0, font::SIZE_SMALLER));

    }
}

void map_controller::click_save_map()
{
    if (!ros_instance_.navigation_node_started()) {
        return;
    }

    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0) {
        return;
    }

    map_saving_ = true;
    while (map_saving_) {
        ros_instance_.slice();
    }

    utils::string_map symbols;
    symbols["file"] = utils::extract_file(last_saved_rspfile_);
    gui2::show_message(null_str, vgettext2("Successfully saved the map to $file", symbols));
}

void map_controller::click_reposition_robot(int estimate_reason)
{
    VALIDATE(estimate_reason == tkidnap::reason_manual, null_str);
    if (!ros_instance_.navigation_node_started()) {
        return;
    }

    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0) {
        return;
    }

    if (!ros_instance_.move_base_constructed()) {
        return;
    }

    if (!kidnap.can_estimate() || kidnap.estimating()) {
        return;
    }

    // kidnap.cartographer_pose2d.valid = false;
    // VALIDATE(!kidnap.cartographer_pose2d.valid, null_str);

    kidnap.set_estimate(estimate_reason, tpose2d());
    while (kidnap.estimating()) {
        ros_instance_.slice();
    }

    // VALIDATE(kidnap.cartographer_pose2d.valid, null_str);
    const tpose2d& new_pose2d = kidnap.cartographer_pose2d;

    char buf[128];
    if (new_pose2d.valid) {
        SDL_snprintf(buf, sizeof(buf), "%s %s spend %i ms unknown(%u), success", utils::format_time_hms(time(nullptr)).c_str(),
            new_pose2d.to_string().c_str(), kidnap.result4.ms, kidnap.result4.unknowns);
    } else {
        SDL_snprintf(buf, sizeof(buf), "%s %s spend %i ms, fail", utils::format_time_hms(time(nullptr)).c_str(), 
            new_pose2d.to_string().c_str(), kidnap.result4.ms);
    }
    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(buf, null_str));

    // gui2::show_message(null_str, "Successfully robot locatte");
    return;
}

void map_controller::set_status_report(const std::string& msg)
{
    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(msg, null_str));
}

void map_controller::insert_map_marker(int type)
{
    VALIDATE(type == rspmapmarkertype_wall, null_str);
    VALIDATE(cur_mode_.mode == mode_position, null_str);

    if (marker_placer_->is_visible()) {
        utils::string_map symbols;
        symbols["marker"] = game_config::markers.find(rspmapmarkertype_wall)->second;
        const std::string msg = vgettext2("Editing a marker, want to continue without saving and end edit, add a $marker?", symbols);
	    if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
            return;
        }

        cancel_edit_map_marker();
    }

    const std::string uuid = utils::create_uuid(true);
    trsp_rosmapmarker wall;
    memset(&wall, 0, sizeof(wall));
    memcpy(wall.uuid, uuid.c_str(), uuid.size());
    wall.type = rspmapmarkertype_wall;
    wall.x = 0.0;
    wall.y = 0.0;
    wall.width = 4.0;
    wall.theta = 0.0;
    
    tros_map& ros_map = curmap_;
    ros_map.markers.insert(std::make_pair(uuid, tmap_marker(wall)));

    save_map_by_ros_map(ros_map);
}

void map_controller::edit_map_marker(const std::string& uuid)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    VALIDATE(exclude_marker_halo_.empty(), null_str);
    VALIDATE(curmap_.markers.count(uuid) != 0, null_str);
    tmap_marker& marker = curmap_.markers.find(uuid)->second;

    exclude_marker_halo_ = uuid;
    if (marker_halos_.count(uuid) != 0) {
        marker_halos_.erase(marker_halos_.find(uuid));
    }

    SDL_Point mouse_xy = meter_2_mouse_xy(marker.rsp.x, marker.rsp.y);
    marker_placer_->show(mouse_xy.x, mouse_xy.y, marker.rsp.width, marker.rsp.theta, 1.0 * gui_->zoom());

    // remember anchor's maimap xy in current zoom.
    set_anchor_mainmap_xy();
}

void map_controller::close_map_marker(int msg_id, int mouse_x, int mouse_y, double line_width_meter, double theta, bool cancel_only)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    VALIDATE(!exclude_marker_halo_.empty(), null_str);

    tauto_destruct_executor destruct_executor(std::bind(&gui2::tmarker_placer::hide, marker_placer_.get()));

    const std::string uuid = exclude_marker_halo_;
    exclude_marker_halo_.clear();

    tros_map& ros_map = curmap_;
    VALIDATE(ros_map.markers.count(uuid) != 0, null_str);
    tmap_marker& marker = ros_map.markers.find(uuid)->second;

    bool markers_dirty = false;
    if (msg_id == gui2::tfloat_panel::POST_MSG_OK) {
        SDL_DPoint map_xy = mouse_xy_2_ros_world_xy(mouse_x, mouse_y);

        if (fabs(marker.rsp.x - map_xy.x) < 0.01 && fabs(marker.rsp.y - map_xy.y) < 0.01 &&
            fabs(marker.rsp.width - line_width_meter) < 0.01 && RAD2DEG(fabs(marker.rsp.theta - theta)) < 1.0) {
            return;
        }

        marker.rsp.x = map_xy.x;
        marker.rsp.y = map_xy.y;
        marker.rsp.width = line_width_meter;
        marker.rsp.theta = theta;

        markers_dirty = true;

    } else {
        VALIDATE(msg_id == gui2::tfloat_panel::POST_MSG_CANCEL, null_str);
        int result = gui2::twindow::OK;
        if (!cancel_only) {
            utils::string_map symbols;
            const std::string cancel_caption = _("Close without saving");
            const std::string erase_caption = _("Erase");
	        symbols["cancel"] = cancel_caption;
            symbols["erase"] = erase_caption;
            const std::string msg = vgettext2("Do you want to '$cancel' or '$erase'?", symbols);
	        result = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, null_str, null_str, cancel_caption, erase_caption);
        }
	    if (result == gui2::twindow::OK) {
            // cancel

        } else {
            // erase
            if (marker_halos_.count(uuid) != 0) {
                marker_halos_.erase(marker_halos_.find(uuid));
            }

            ros_map.markers.erase(ros_map.markers.find(uuid));
            markers_dirty = true;
        }
    }

    if (markers_dirty) {
        save_map_by_ros_map(ros_map);
    }
}

void map_controller::cancel_edit_map_marker()
{
    close_map_marker(gui2::tfloat_panel::POST_MSG_CANCEL, 0, 0, 0, 0, true);
}

void map_controller::click_auto_buildmap()
{
    VALIDATE(cur_mode_.mode == mode_buildmap, null_str);

    auto_buildmap_ = !auto_buildmap_;
    set_auto_buildmap_label();

    // if goal is set, cancel it.
    cancel_goal();
    if (!auto_buildmap_) {
        return;
    }
}

void map_controller::send_exploration_detect()
{
    if (!base_footprint_pose_.valid) {
        return;
    }
    // if (!ros_instance_.is_mapping()) {
    //    ros_instance_.start_mapping();
    // }

    geometry_msgs::Point unk;
    bool res = ros_instance_.exploration_detect(base_footprint_pose_, unk);
    if (res) {
        goal_.target_pose.header.seq ++;
        goal_.target_pose.header.frame_id = "map";
        goal_.target_pose.pose.position = unk;

        goal_.target_pose.header.stamp = ros::Time::now();
        tf2::Quaternion q;
        double theta = 0;
        q.setRPY(0, 0, theta);
        goal_.target_pose.pose.orientation.x = q.x();
        goal_.target_pose.pose.orientation.y = q.y();
        goal_.target_pose.pose.orientation.z = q.z();
        goal_.target_pose.pose.orientation.w = q.w();

        SDL_Log("exploration_detect, deg: %.5f, position(%.5f, %.5f, %.5f), orientation(%.5f, %.5f, %.5f, %.5f)", 
            RAD2DEG(theta), 
            goal_.target_pose.pose.position.x, goal_.target_pose.pose.position.y, goal_.target_pose.pose.position.z,
            goal_.target_pose.pose.orientation.x, goal_.target_pose.pose.orientation.y, goal_.target_pose.pose.orientation.z, goal_.target_pose.pose.orientation.w);

        ros_instance_.do_send_goal(goal_);
    }
}

void map_controller::cancel_goal()
{
    VALIDATE(cur_mode_.mode == mode_buildmap || cur_mode_.mode == mode_navigation, null_str);
    if (!ros_instance_.navigation_node_started()) {
        return;
    }

    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0) {
        return;
    }

    if (ros_instance_.goaling()) {
        // in navigationing
        ros_instance_.do_cancel_goal();
    } else {
        // if (dlg_->is_pickplace()) {
        //    VALIDATE(ros_instance_.has_task(), null_str);
        //    dlg_->cancel_pickplace();
        // }
    }
}

void map_controller::click_cancel_goal()
{
    cancel_goal();
}

void map_controller::click_switch_slam()
{
    cartographer::rose_slot.switch_page();

    slam_major_widget_->set_background_tex_dirty();
    immediate_draw_slam();
}

bool map_controller::require_show_help() const
{
	return show_help_ && cartographer::rose_slot.current_page() == cartographer::trose_slot::NORMAL_PAGE;
}

void map_controller::set_show_help(bool val)
{
    // don't use below if. for example change 'mode_'.
    // if (show_help_ == val) {
    //    return;
    // }
    show_help_ = val;
    slam_major_widget_->set_background_tex_dirty();
    immediate_draw_slam();
}

bool map_controller::is_normal_page() const
{
    return cartographer::rose_slot.current_page() == cartographer::trose_slot::NORMAL_PAGE;
}

bool map_controller::is_kidnap_page() const
{
    return cartographer::rose_slot.current_page() == cartographer::trose_slot::KIDNAP_PAGE;
}

void map_controller::set_denoising_map(bool val)
{
    VALIDATE(denoising_map_ != val, null_str);

    denoising_map_ = val;
    dlg_->did_denoise_map(val);

    slam_major_widget_->set_background_tex_dirty();
    immediate_draw_slam();
}

void map_controller::immediate_draw_major_slam()
{
    slam_major_widget_->immediate_draw();
    if (is_normal_page() && cartographer::rose_slot.show_camera_scan()) {
        SDL_Rect bg_rect = slam_major_widget_->get_rect();
        bg_rect.y = bg_rect.y + bg_rect.h - bg_rect.w;
        bg_rect.h = bg_rect.w;
        ros_instance_.draw_dcamera_points(*slam_major_widget_, bg_rect, draw_dcamera_points_settings_.ranges, draw_dcamera_points_settings_.meter_per_range);
    }
}

void map_controller::immediate_draw_slam()
{
    immediate_draw_major_slam();
    if (!slam_track_4_dcamera_) {
        slam_minor_widget_->immediate_draw();
    }
}

void map_controller::click_confirm(bool ok)
{
    VALIDATE(cur_mode_.mode == mode_navigation, null_str);
    VALIDATE(denoising_map_, null_str);

    stop_denoising_map(ok);
}

void map_controller::calculate_laser_origin()
{
    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0 || !ros_instance_.navigation_node_started()) {
        return;
    }

    const int originx = last_map_origin_.x;
    const int originy = last_map_origin_.y;

    geometry_msgs::TransformStamped transform;
    if (ros_instance_.get_laser_tf(transform)) {
        {
            geometry_msgs::Pose robot_pose;
            tf2::toMsg(tf2::Transform::getIdentity(), robot_pose);

            geometry_msgs::Pose t_out;
            tf2::doTransform(robot_pose, t_out, transform);

            base_footprint_pose_.x = t_out.position.x;
            base_footprint_pose_.y = t_out.position.y;
            base_footprint_pose_.yaw = tf2::getYaw(t_out.orientation);

            // SDL_Log("%u base_footprint_pose_: %s", SDL_GetTicks(), base_footprint_pose_.to_string(true).c_str());
            base_footprint_pose_.valid = true;
        }
/*
        {
            geometry_msgs::TransformStamped transform2;
            ros_instance_.get_laser_2_base_footprint_tf(transform2);

            geometry_msgs::Pose robot_pose;
            tf2::toMsg(tf2::Transform::getIdentity(), robot_pose);

            tpose2d pose2d;
            geometry_msgs::Pose t_out;
            tf2::doTransform(robot_pose, t_out, transform2);

            pose2d.x = t_out.position.x,
            pose2d.y = t_out.position.y,
            pose2d.yaw = tf2::getYaw(t_out.orientation);
            SDL_Log("%u laser_2_base_footprint_tf: %s", SDL_GetTicks(), pose2d.to_string(true).c_str());
        }
*/
        const geometry_msgs::Vector3& v3 = transform.transform.translation;
        int ox = v3.x / last_map_.resolution;
        int oy = v3.y / last_map_.resolution;
        const int raw_laser_oy = oy;
        ox = originx + ox;
        // oy = last_map_.height - (raw_originy + oy) = originy - oy
        oy = originy - oy;
        last_laser_origin_.x = ox;
        last_laser_origin_.y = oy;

        const geometry_msgs::Quaternion& q = transform.transform.rotation;
        tf2::Matrix3x3 mat(tf2::Quaternion(q.x, q.y, q.z, q.w));
        double yaw, pitch, roll;
        mat.getEulerYPR(yaw, pitch, roll);

        surface surf = image::get_image("misc/laser_origin.png");
        SDL_Point& fix_pt = laser_png_rotate_point_;
        fix_pt.x = 14;
        fix_pt.y = 36;
        laser_png_surf_ = rotate_surface(surf, RAD2DEG(yaw), &fix_pt, 1);

        const int simple_per_meter = 1 / last_map_.resolution;
        laser_pixels_per_meter_ = simple_per_meter;
        last_laser_yaw_ = yaw;
    }
}

void map_controller::zoom_src_gray()
{
    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0) {
        return;
    }
    if (last_map_.width == 0 || last_map_.height == 0) {
        return;
    }

    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0, null_str);
    VALIDATE(last_map_.resolution != 0 && last_map_.width == last_src_gray_.cols && last_map_.height == last_src_gray_.rows, null_str);

    const int simple_per_meter = 1 / last_map_.resolution;
    const int simple_per_meter2 = 1.0 / last_map_.resolution;
    // SDL_Log("zoom_src_gray(), simple_per_meter: %i, simple_per_meter2: %i", simple_per_meter, simple_per_meter2);
    VALIDATE(simple_per_meter == 20, null_str);

    int originx = posix_abs(last_map_.origin.position.x) * simple_per_meter;
    if (originx < 0 || originx > (int)last_map_.width) {
        SDL_Log("Error originx(%i) isn't in [0, %i], reset to 0", originx, last_map_.width);
        originx = 0;
    }
    int originy = posix_abs(last_map_.origin.position.y) * simple_per_meter;
    const int raw_originy = originy;
    originy = last_map_.height - originy;
    if (originy < 0 || originy > (int)last_map_.height) {
        SDL_Log("Error originy(%i) isn't in [0, %i], reset to 0", originy, last_map_.height);
        originy = 0;
    }

    // below will move right/bottom to cross, extern 1 grid.
    make_sure_map(last_map_.width + simple_per_meter, last_map_.height + simple_per_meter, 1 / last_map_.resolution);

    int tmp_originx = posix_pages(originx, simple_per_meter);
    int tmp_originy = posix_pages(originy, simple_per_meter);
    last_map_offset_.x = tmp_originx * simple_per_meter - originx;
    last_map_offset_.y = tmp_originy * simple_per_meter - originy;

    last_map_origin_.x = originx;
    last_map_origin_.y = originy;

    calculate_laser_origin();

    if (cur_mode_.mode == mode_buildmap && ros_instance_.goaling()) {
        if (ros_current_goal_.valid) {
            parse_ros_current_goal();
        }
        parse_ros_global_plan();
        parse_ros_local_plan();
    }

    marker_halos_.clear();
    if (cur_mode_.mode == mode_position) {
        position_halos_.clear();
    }
}

void map_controller::reload_map(int w, int h)
{
    const int original_w = map_.w();
    const int original_h = map_.h();

	map_ = tmap(generate_map_data2(w, h, false));
	gui_->reload_map();
	units_.create_coor_map(map_.w(), map_.h());

    VALIDATE(w * h == units_.size() * UNIT_LOCS * UNIT_LOCS, null_str);

    std::stringstream ss;
    ss << "reload_map(" << w << ", " << h << ")";
    // units_.dump(ss.str());
}

void map_controller::make_sure_map(int width, int height, int simple_per_meter)
{
    VALIDATE(width > 0 && height > 0, null_str);
    VALIDATE(simple_per_meter == 20 || simple_per_meter == DEFAULT_LASER_PIXELS_PER_METER, null_str);

    int cols = posix_pages(width, simple_per_meter);
    int rows = posix_pages(height, simple_per_meter);

    const int divisor = UNIT_LOCS;
    cols = posix_align_ceil2(cols, divisor);
    cols = SDL_min(cols, MAX_MAP_W);

    rows = posix_align_ceil2(rows, divisor);
    rows = SDL_min(rows, MAX_MAP_H);

    int map_w = map_.w();
    int map_h = map_.h();
    bool require = false;
    if (cols > map_w) {
        map_w = cols;
        require = true;
    }
    if (rows > map_h) {
        map_h = rows;
        require = true;
    }

    if (!require) {
        return;
    }
    // 
    const int right_padding_cells = 2;
    reload_map(map_w + right_padding_cells, map_h);
}

void map_controller::set_cur_mode(const tgui_mode& new_mode)
{
    VALIDATE(new_mode.mode >= 0 && new_mode.mode < mode_count, null_str);
    VALIDATE(!ros_instance_.navigation_node_started(), null_str);
    SDL_Log("{set_mode}set_mode, %i --> prev(%i)", new_mode.mode, cur_mode_.mode);

    cur_mode_ = new_mode;
    navigation_rspfile_.clear();
    navigation_markers_.clear();
    marker_halos_.clear();
    position_halos_.clear();
    dlg_->did_run_state_changed(cur_mode_.mode, false);

    cartographer::rose_slot.switch_page(cartographer::trose_slot::NORMAL_PAGE);
    set_show_help(true);
}

bool map_controller::can_swtich_to_mode(int desire_mode) const
{
    if (desire_mode != mode_position) {
        return true;
    }
    if (cur_mode_.mode == mode_position) {
        return true;
    }
    // now support curmap only.
    return false;
}

bool map_controller::update_mainmap_from_rspfile(const tros_map& ros_map)
{
    pre_start();
    if (!ros_map.valid()) {
        utils::string_map symbols;
		symbols["file"] = utils::extract_file(ros_map.rspfile);
        set_status_report(vgettext2("$file isn't a valid map file", symbols));
		return false;
	}

    if (cur_mode_.mode != mode_position) {
        set_status_report(null_str);
    }

    ros_instance_.refresh_map_data(ros_map.map, false);
    did_map_subscribed(ros_map.map);
    return true;
}

bool map_controller::set_navigation_rspfile(const std::string& rspfile)
{
    VALIDATE(cur_mode_.mode == mode_navigation, null_str);
    VALIDATE(!rspfile.empty(), null_str);

    tros_map ros_map;
    ros::load_map_from_rsp(rspfile, ros_map);
    bool ret = update_mainmap_from_rspfile(ros_map);
    if (ret) {
        navigation_rspfile_ = rspfile;
        navigation_markers_ = ros_map.markers;
    }
    return ret;
}

void map_controller::did_rspfile_deleted(const std::string& rspfile)
{
    VALIDATE(files_.count(rspfile) != 0, null_str);
    collect_rsp_files(saves_map_dir_, files_);

    if (curmap_.rspfile == rspfile) {
        curmap_.clear();
        preferences::set_curmap(null_str);
    }

    bool deleted = files_.count(rspfile) == 0;
    if (deleted && rspfile == navigation_rspfile_) {
        navigation_rspfile_.clear();
        navigation_markers_.clear();
        pre_start();
    }
}

void map_controller::did_position_entered()
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    start_widget_->set_active(false);

    if (!curmap_.valid()) {
        utils::string_map symbols;
		symbols["settings"] = _("icon^Settings");
        gui_->refresh_report(gui2::tmap_scene::STATUS, 
            reports::report(vgettext2("Eneter $settings, and set current map", symbols), null_str));
        return;
    }
    update_mainmap_from_rspfile(curmap_);
}

void map_controller::did_position_insert(const std::string& name, double theta)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    VALIDATE(!name.empty(), null_str);

    const std::string uuid = utils::create_uuid(true);
    curmap_.positions.insert(std::make_pair(uuid, tmap_position(uuid, name, float_nposm, float_nposm, theta)));
    save_map_when_position();
}

void map_controller::did_position_edit(const std::string& uuid, const std::string& new_name, double new_theta)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    tmap_position* position = position_from_uuid(uuid);

    VALIDATE(!new_name.empty(), null_str);
    position->name = new_name;
    position->theta = new_theta;
    if (position_halos_.count(uuid) != 0) {
        position_halos_.erase(position_halos_.find(uuid));
    }
    save_map_when_position();
}

void map_controller::did_position_erase(const std::string& uuid)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);
    std::map<std::string, tmap_position>::iterator find_it = curmap_.positions.find(uuid);
    VALIDATE(find_it != curmap_.positions.end(), null_str);
    const tmap_position& erasing_position = find_it->second;

    // step[1/2] erase relatvie from tros_map::bind_uuid_map

    // step[2/2] erase from tros_map::positions
    curmap_.positions.erase(find_it);

    if (position_halos_.count(uuid) != 0) {
        position_halos_.erase(position_halos_.find(uuid));
    }
    save_map_when_position();
}

void map_controller::did_scan_subscribed(const sensor_msgs::LaserScan& scan_msg, const SDL_2Point& charging)
{
    cv::Mat image;
    VALIDATE(laser_pixels_per_meter_ > 0, null_str);
    UpdateDepthImage(image, scan_msg, charging, laser_pixels_per_meter_, cur_mode_.mode);
    if (image.empty()) {
        return;
    }
    if (!is_kidnap_page() && cartographer::rose_slot.can_draw_track() && gui2::top_is_scene()) {
        // SDL_Log("%u call immediate_draw_slam()", SDL_GetTicks());
        immediate_draw_slam();
    }

    SDL_Point fix_pt{image.cols / 2, image.rows / 2};
    if (last_laser_yaw_ != 0) {
        last_laser_mat_ = rotate_mat(image, RAD2DEG(last_laser_yaw_), &fix_pt, 1);
    } else {
        last_laser_mat_ = image;
    }

    
    int simple_per_meter = laser_pixels_per_meter_;
    int originx = fix_pt.x;
    int originy = fix_pt.y;

    
    last_laser_offset_.x = last_map_offset_.x + last_laser_origin_.x - originx;
    last_laser_offset_.y = last_map_offset_.y + last_laser_origin_.y - originy;
}

#define CONTOUR_INTER_VALUE     128
#define CONTOUR_TRAVERSAL_VALUE 1

static bool in_contour(const int cell_x, const int cell_y, uint8_t* map_data, int width, int height, uint8_t OBSTACLE_VALUE)
{
    // x-axis
    uint8_t* y_ptr = map_data + cell_y * width;
    if (y_ptr[cell_x] == CONTOUR_INTER_VALUE) {
        return true;
    }
    int left_x = nposm;
    int right_x = nposm;
    for (int x = cell_x - 1; x >= 0; x --) {
        if (y_ptr[x] == OBSTACLE_VALUE) {
            left_x = x;
            break;
        }
    }
    if (left_x != nposm) {
        for (int x = cell_x + 1; x < width; x ++) {
            if (y_ptr[x] == OBSTACLE_VALUE) {
                right_x = x;
                break;
            }
        }
        if (right_x != nposm) {
            memset(y_ptr + left_x + 1, CONTOUR_INTER_VALUE, right_x - left_x - 1);
            return true;
        }
    }
    // y-axis
    int above_y = nposm;
    int under_y = nposm;
    for (int y = cell_y - 1; y >= 0; y --) {
        if (map_data[y * width + cell_x] == OBSTACLE_VALUE) {
            above_y = y;
            break;
        }
    }
    if (above_y != nposm) {
        for (int y = cell_y + 1; y < height; y ++) {
            if (map_data[y * width + cell_x] == OBSTACLE_VALUE) {
                under_y = y;
                break;
            }
        }
        if (under_y != nposm) {
            map_data[cell_y * width + cell_x] = CONTOUR_INTER_VALUE;
            return true;
        }
    }
    return false;
}

void master_find_first_xy(int width, int height, const uint8_t* work_map_data, uint8_t OBSTACLE_VALUE, int& x, int& y)
{
    for (; y < height; y ++) {
        int yoffset = y * width;
        const uint8_t* y_ptr = work_map_data + y * width;

        for (x = 0; x < width; x ++) {
            if (y_ptr[x] == OBSTACLE_VALUE) {
                return;
            }
        }
    }
}

void find_first_xy(int width, int height, const uint8_t* work_map_data, const uint8_t* threshold_map_data, uint8_t OBSTACLE_VALUE, int& x, int& y)
{
    for (; y < height; y ++) {
        int yoffset = y * width;
        const uint8_t* y_ptr = work_map_data + y * width;

        for (x = 0; x < width; x ++) {
            if (y_ptr[x] == 0 && threshold_map_data[yoffset + x] != OBSTACLE_VALUE) {
                return;
            }
        }
    }
}

void contours_2_map_data(int width, int height, const std::vector<std::vector<cv::Point> >& contours, const std::set<int>& indexs, std::vector<cv::Vec4i> hierarchy, uint8_t* map_data, uint8_t fill_value)
{
    cv::Mat o = cv::Mat::zeros(height, width, CV_8UC1);
    for (std::set<int>::const_iterator it = indexs.begin(); it != indexs.end(); ++ it) {
        int index = *it;
        // cv::drawContours(o, contours, index, cv::Scalar(0, 255, 0), cv::FILLED, 8, hierarchy);
        cv::drawContours(o, contours, index, cv::Scalar(fill_value == 0? index + 1: fill_value), cv::FILLED, 8, hierarchy);
    }

    memset(map_data, 0, width * height);
    for (int row = 0; row < o.rows; row ++) {
		const uint8_t* data = o.ptr<uint8_t>(row);
		for (int col = 0; col < o.cols; col ++) {
			uint8_t gray_value = data[col];
            if (gray_value != 0) {
                map_data[row * width + col] = gray_value;
            }
        }
    }
}

void map_controller::click_denoising_map()
{
    if (navigation_rspfile_.empty()) {
        gui2::show_message(null_str, _("Before denoising, select the map."));
        return;
    }

    const bool verbose_png = game_config::os == os_windows;

    const int8_t* last_map_data = ros_instance_.last_map_data();
	int last_map_data_size = ros_instance_.last_map_data_size();
    VALIDATE(last_map_data != nullptr && last_map_data_size >= (int)last_map_.width * (int)last_map_.height, null_str);

    const int width = last_map_.width;
    const int height = last_map_.height;
    const int cells = width * height;

    if (verbose_png)  {
        surface surf = u8_data_2_cell_value_surf((const uint8_t*)last_map_data, 4, 4, width, height, maptype_OccupancyGrid, nullptr, nullptr);
        imwrite(surf, "1-cell_value.png");
    }

    uint8_t* threshold_map_data = (uint8_t*)malloc(cells);
    cv::Mat threshold_mat = cv::Mat(height, width, CV_8UC1);
    uint8_t* threshold_mat_data = threshold_mat.ptr<uint8_t>(0);

    int pos = 0;

    for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			int i = x + (height - y - 1) * width;
            uint8_t u8 = last_map_data[i];
            threshold_mat_data[pos] = u8 != costmap_2d::NO_INFORMATION? u8: 0;
            threshold_map_data[pos] = u8;
            pos ++;
		}
    }
    VALIDATE(pos == width * height, null_str);
    if (verbose_png) {
        surface surf = u8_data_2_argb_surf(threshold_map_data, width, height);
        imwrite(surf, "1-raw.png");
    }

    const uint8_t OBSTACLE_VALUE = 255;
    VALIDATE(OBSTACLE_VALUE == costmap_2d::NO_INFORMATION, null_str);

    double threshold_occupied = NEAR_OBSTACLE; // think >= 60 as obstacle
    cv::threshold(threshold_mat, threshold_mat, threshold_occupied - 1, OBSTACLE_VALUE, cv::THRESH_BINARY); // cv::THRESH_BINARY_INV

    if (verbose_png) {
        imwrite_gray(threshold_mat, "1-gray.png");
    }
    pos = 0;
    for (int y = 0; y < threshold_mat.rows; y ++) {
		const uint8_t* data = threshold_mat.ptr<uint8_t>(y);
        for (int x = 0; x < threshold_mat.cols; x ++) {
            if (data[x] == OBSTACLE_VALUE) {
                threshold_map_data[pos] = data[x];
            }
            pos ++;
        }
    }
    VALIDATE(pos == width * height, null_str);
    if (verbose_png) {
        surface surf = u8_data_2_argb_surf(threshold_map_data, width, height);
        imwrite(surf, "1-gray-lt60.png");
    }

    cv::Mat dilate_mat;
    int radius = 1; // 2
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
    cv::dilate(threshold_mat, dilate_mat, element);
    if (verbose_png) {
        imwrite_gray(dilate_mat, "1-gray-dilate-1st.png");
    }

    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;
	// cv::findContours(dilate_mat, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    cv::findContours(dilate_mat, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    const int max_support_contours = 254; // display only one byte, and MUST NOT display 0.
    if ((int)contours.size() > max_support_contours) {
        utils::string_map symbols;
        symbols["max_contours"] = str_cast(max_support_contours);
        gui2::show_message(null_str, vgettext2("Denoising failed. Too many contours, up to $max_contours are supported", symbols));
        return;

    } else if (contours.empty()) {
        gui2::show_message(null_str, _("Denoising failed. There are no contour in the map"));
        return;
    }

    cv::Mat o = cv::Mat::zeros(dilate_mat.rows, dilate_mat.cols, CV_8UC3);

    std::set<int> all_indexs;
    // hierarchy[.][0]: index of next contour
    for (int index = 0; index >= 0; index = hierarchy[index][0]) {
        VALIDATE(index >= 0 && (int)contours.size(), null_str);
        all_indexs.insert(index);
        if (verbose_png) {
            cv::Scalar color(rand() % 255, rand() % 255, rand() % 255);
            cv::drawContours(o, contours, index, color, cv::FILLED, 8, hierarchy);
        }
    }
    if (verbose_png) {
        imwrite(o, "1-contours-1nd-all.png");
    }

    const int desire_size = posix_align_ceil(2 * cells, 4096);
    if (desire_size > denoise_map_data_size_) {
        free(denoise_map_data_);
        denoise_map_data_ = (uint8_t*)malloc(desire_size);
        denoise_map_data_size_ = desire_size;
    }
    uint8_t* denoise_map_data = denoise_map_data_;
    uint8_t* aux_map_data = denoise_map_data + cells;
    contours_2_map_data(width, height, contours, all_indexs, hierarchy, aux_map_data, 0);
    if (verbose_png) {
        // surface work_surf = u8_data_2_argb_surf(aux_map_data, width, height);
        surface surf = u8_data_2_cell_value_surf(aux_map_data, 4, 4, width, height, nposm, nullptr, nullptr);
        imwrite(surf, "1-contours-1nd-all-cell_value.png");
    }

    // 2nd dilate
    radius = 2; // (1 + 2) = 3
    element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
    cv::dilate(dilate_mat, dilate_mat, element);
    if (verbose_png) {
        imwrite_gray(dilate_mat, "1-gray-dilate-2nd.png");
    }
    // find 2nd-dilate's master contour
    std::vector<std::vector<cv::Point> > contours2;
    std::vector<cv::Vec4i> hierarchy2;
    cv::findContours(dilate_mat, contours2, hierarchy2, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int master_contour_at = nposm;
    int max_contour_points = 0;
    for (int index = 0; index < (int)contours2.size(); index ++) {
        const std::vector<cv::Point>& points = contours2[index];
        if ((int)points.size() > max_contour_points) {
            max_contour_points = points.size();
            master_contour_at = index;
        }
    }
 
    VALIDATE(master_contour_at != nposm, null_str);
    o = cv::Mat::zeros(dilate_mat.rows, dilate_mat.cols, CV_8UC1);
    cv::drawContours(o, contours2, master_contour_at, cv::Scalar(255), cv::FILLED, 8, hierarchy2);
    if (verbose_png) {
        imwrite_gray(o, "1-contours-2nd-master.png");
        // cv::Rect master_bounding = cv::boundingRect(contours[master_contour_at]);
        // cv::rectangle(o2, master_bounding, cv::Scalar(0, 0, 255));
        // imwrite(o2, "1-contours-2nd-master-bound.png");
    }

    std::set<int> master_indexs;
    for (int y = 0; y < height; y ++) {
        const uint8_t* y_ptr = o.ptr<uint8_t>(y);
        int yoffset = y * width;
        for (int x = 0; x < width; x ++) {
            uint8_t u8_aux = aux_map_data[yoffset + x];
            if (y_ptr[x] != 0 && u8_aux != 0) {
                if (master_indexs.count(u8_aux - 1) == 0) {
                    int index = u8_aux - 1;
                    VALIDATE(all_indexs.count(index) != 0, null_str);
                    master_indexs.insert(index);
                }
            }
        }
    }

    contours_2_map_data(width, height, contours, master_indexs, hierarchy, denoise_map_data, OBSTACLE_VALUE);

    VALIDATE(last_map_origin_.x >= 0 && last_map_origin_.x <= (int)last_map_.width, null_str);
    VALIDATE(last_map_origin_.y >= 0 && last_map_origin_.y <= (int)last_map_.height, null_str);
    if (verbose_png) {
        denoise_map_data[last_map_origin_.y * width + last_map_origin_.x] = OBSTACLE_VALUE;
        surface work_surf = u8_data_2_argb_surf(denoise_map_data, width, height);
        imwrite(work_surf, "1-denoise-master.png");
        denoise_map_data[last_map_origin_.y * width + last_map_origin_.x] = 0;
    }

    // ----------------------------
    // ----------------------------
     
    // fill master contous
    memcpy(aux_map_data, denoise_map_data, cells);
    int last_col = width - 1;
    int last_row = height - 1;

    std::queue<SDL_Point> dist_queue;
    dist_queue.push(SDL_Point{last_map_origin_.x, last_map_origin_.y});

    SDL_Point current_cell;
    int max_queue_size = 0;
    uint32_t start_ticks = SDL_GetTicks();
    while (!dist_queue.empty()) {
        current_cell = dist_queue.front();
        dist_queue.pop();

        // int yoffset = current_cell.y * width;
        uint8_t* y_ptr = aux_map_data + current_cell.y * width;

        if ((int)dist_queue.size() > max_queue_size) {
            max_queue_size = (int)dist_queue.size();
        }
        // SDL_Log("cell(%i, %i) val: %i, dist_queue.size: %i", current_cell.x, current_cell.y,
        //    y_ptr[current_cell.x], (int)dist_queue.size());

        int x2;
        if (current_cell.x > 0) {
            x2 = current_cell.x - 1;
            if (y_ptr[x2] == 0 && in_contour(x2, current_cell.y, denoise_map_data, width, height, OBSTACLE_VALUE)) {
                // y_ptr[x] needs to be changed immediately.
                // Otherwise, the next time you need it, it's still in the queue
                y_ptr[x2] = CONTOUR_TRAVERSAL_VALUE;
                dist_queue.push(SDL_Point{x2, current_cell.y});
            }
        }

        if (current_cell.x < last_col) {
            x2 = current_cell.x + 1;
            if (y_ptr[x2] == 0 && in_contour(x2, current_cell.y, denoise_map_data, width, height, OBSTACLE_VALUE)) {
                y_ptr[x2] = CONTOUR_TRAVERSAL_VALUE;
                dist_queue.push(SDL_Point{x2, current_cell.y});
            }
        }

        int y2;
        if (current_cell.y > 0) {
            y2 = current_cell.y - 1;
            y_ptr = aux_map_data + y2 * width;

            if (y_ptr[current_cell.x] == 0 && in_contour(current_cell.x, y2, denoise_map_data, width, height, OBSTACLE_VALUE)) {
                y_ptr[current_cell.x] = CONTOUR_TRAVERSAL_VALUE;
                dist_queue.push(SDL_Point{current_cell.x, y2});
            }
        }

        if (current_cell.y < last_row) {
            y2 = current_cell.y + 1;
            y_ptr = aux_map_data + y2 * width;

            if (y_ptr[current_cell.x] == 0 && in_contour(current_cell.x, y2, denoise_map_data, width, height, OBSTACLE_VALUE)) {
                y_ptr[current_cell.x] = CONTOUR_TRAVERSAL_VALUE;
                dist_queue.push(SDL_Point{current_cell.x, y2});
            }
        }
    }
    VALIDATE(dist_queue.empty(), null_str);
    SDL_Log("[denoise map]post fill denose map, max_queue_size: %i, spend %u ms", max_queue_size, SDL_GetTicks() - start_ticks);

    if (verbose_png) {
        surface work_surf = u8_data_2_argb_surf(denoise_map_data, width, height);
        imwrite(work_surf, "1-denoise-filled-inter.png");
    }
    for (int y = 0; y < height; y ++) {
        uint8_t* y_ptr = denoise_map_data + y * width;
        for (int x = 0; x < width; x ++) {
            if (y_ptr[x] == CONTOUR_INTER_VALUE) {
                y_ptr[x] = OBSTACLE_VALUE;
            }
        }
    }
    if (verbose_png) {
        surface work_surf = u8_data_2_argb_surf(denoise_map_data, width, height);
        imwrite(work_surf, "1-denoise-filled.png");
    }

    int areas[MAX_DENOISE_AREAS] = {0};
    last_col = width - 1;
    last_row = height - 1;

    int curr_area_index = -1;
    int global_x = 0;
    int global_y = 0;

    while (!dist_queue.empty() || global_x != width) {
        if (dist_queue.empty()) {
            VALIDATE(global_x < width && global_y < height, null_str);
            if (curr_area_index != nposm) {
                SDL_Log("finished area[index:%i-cells:%i]", curr_area_index, areas[curr_area_index]);
            }
            curr_area_index ++;
            if (curr_area_index == MAX_DENOISE_AREAS) {
                break;
            }
            find_first_xy(width, height, denoise_map_data, threshold_map_data, OBSTACLE_VALUE, global_x, global_y);
            if (global_x < width) {
                VALIDATE(global_y < height, null_str);
                denoise_map_data[global_y * width + global_x] = curr_area_index + 1;
                dist_queue.push(SDL_Point{global_x, global_y});
            } else {
                break;
            }
        }

        current_cell = dist_queue.front();
        dist_queue.pop();

        int yoffset = current_cell.y * width;
        uint8_t* y_ptr = denoise_map_data + current_cell.y * width;

        // SDL_Log("cell(%i, %i) val: %i to curr_area[index:%i-cells:%i]", current_cell.x, current_cell.y,
        //    y_ptr[current_cell.x], curr_area_index, areas[curr_area_index]);
        areas[curr_area_index] ++;

        int x2;
        if (current_cell.x > 0) {
            x2 = current_cell.x - 1;
            if (y_ptr[x2] == 0 && threshold_map_data[yoffset + x2] != OBSTACLE_VALUE) {
                // y_ptr[x] needs to be changed immediately.
                // Otherwise, the next time you need it, it's still in the queue
                y_ptr[x2] = curr_area_index + 1;
                dist_queue.push(SDL_Point{x2, current_cell.y});
            }
        }

        if (current_cell.x < last_col) {
            x2 = current_cell.x + 1;
            if (y_ptr[x2] == 0 && threshold_map_data[yoffset + x2] != OBSTACLE_VALUE) {
                y_ptr[x2] = curr_area_index + 1;
                dist_queue.push(SDL_Point{x2, current_cell.y});
            }
        }

        int y2;
        if (current_cell.y > 0) {
            y2 = current_cell.y - 1;
            yoffset = y2 * width;
            y_ptr = denoise_map_data + y2 * width;

            if (y_ptr[current_cell.x] == 0 && threshold_map_data[yoffset + current_cell.x] != OBSTACLE_VALUE) {
                y_ptr[current_cell.x] = curr_area_index + 1;
                dist_queue.push(SDL_Point{current_cell.x, y2});
            }
        }

        if (current_cell.y < last_row) {
            y2 = current_cell.y + 1;
            yoffset = y2 * width;
            y_ptr = denoise_map_data + y2 * width;

            if (y_ptr[current_cell.x] == 0 && threshold_map_data[yoffset + current_cell.x] != OBSTACLE_VALUE) {
                y_ptr[current_cell.x] = curr_area_index + 1;
                dist_queue.push(SDL_Point{current_cell.x, y2});
            }
        }
    }
    // VALIDATE(global_x == width && global_y == height, null_str);
    VALIDATE(dist_queue.empty(), null_str);

    if (verbose_png) {
        surface surf = u8_data_2_cell_value_surf(denoise_map_data, 4, 4, width, height, nposm, nullptr, nullptr);
        imwrite(surf, "1-denoise-finished.png");
    }

    free(threshold_map_data);

    // curr_area_index maybe is 0.
    memset(areas_denoised_, 0, sizeof(areas_denoised_));
    SDL_Color* colors = nullptr;
    bool single_color = true;
    if (curr_area_index > 0) {
        colors = (SDL_Color*)malloc(sizeof(SDL_Color) * curr_area_index);
        for (int at = 0; at < curr_area_index; at ++) {
            areas_denoised_[at] = true;
            SDL_Color& color = colors[at];
            if (single_color) {
                color = colors_[color_denoise_erase];
            } else {
                color.r = rand() & 255;
                color.g = rand() & 255;
                color.b = rand() & 255;
                color.a = 255;
            }
        }
    }

    last_denoise_bgra_ = cv::Mat(last_map_.height, last_map_.width, CV_8UC4);
    uint32_t* last_denoise_bgra_data = last_denoise_bgra_.ptr<uint32_t>(0);
    for (int y = 0; y < height; y ++) {
        const uint8_t* data = last_src_gray_.ptr<uint8_t>(y);
        int yoffset = y * width;
        for (int x = 0; x < width; x ++) {
            uint8_t denoise_u8 = denoise_map_data_[yoffset + x];
            uint8_t u8 = data[x];
            if (denoise_u8 < MIN_DENOISE_AREA_INDEX || denoise_u8 > MAX_DENOISE_AREA_INDEX) {
                last_denoise_bgra_data[yoffset + x] = posix_mku32(posix_mku16(u8, u8), posix_mku16(u8, 0xff));

            } else {
                VALIDATE(denoise_u8 - MIN_DENOISE_AREA_INDEX < curr_area_index, null_str);
                const SDL_Color& color = colors[denoise_u8 - MIN_DENOISE_AREA_INDEX];
                last_denoise_bgra_data[yoffset + x] = posix_mku32(posix_mku16(color.b, color.g), posix_mku16(color.r, color.a));
            }
        }
    }
    if (colors != nullptr) {
        free(colors);
    }

    if (curr_area_index == 0) {
        gui2::show_message(null_str, _("There are no noise area to erase"));
        return;
    }

    set_denoising_map(true);
}

void map_controller::did_stop_denoising_map()
{
    VALIDATE(denoising_map_, null_str);
    set_denoising_map(false);
}

void map_controller::stop_denoising_map(bool ok)
{
    VALIDATE(denoising_map_, null_str);
    VALIDATE(!navigation_rspfile_.empty(), null_str);

    tauto_destruct_executor destruct_executor(std::bind(&map_controller::did_stop_denoising_map, this));
    if (!ok) {
        return;
    }

    tros_map ros_map_other;
    bool is_curmap = curmap_.valid() && curmap_.rspfile == navigation_rspfile_;
    if (!is_curmap) {
        bool ret = ros::load_map_from_rsp(navigation_rspfile_, ros_map_other);
        if (!ret || !ros_map_other.valid()) {
            gui2::show_message(null_str, _("Denoising failed. Failed to read the source map files"));
            return;
        }
    }
    
    tros_map& ros_map = is_curmap? curmap_: ros_map_other;
    const int width = ros_map.map.info.width;
    const int height = ros_map.map.info.height;

    if (width != last_map_.width || height != last_map_.height) {
        gui2::show_message(null_str, _("Denoising failed. The map dimensions change"));
        return;
    }

    const bool os_windows_in_place_save = false;
    const bool in_place_save = is_curmap || (game_config::os != os_windows? true: os_windows_in_place_save);
    if (in_place_save) {
        const std::string msg = _("Denoising map to overwrite the current map, do you want to continue?");
        if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
            return;
        }
    }


    int8_t* to_map_data = &ros_map.map.data[0];

    const SDL_Color& color = colors_[color_denoise_erase];
    uint32_t u32_erase = posix_mku32(posix_mku16(color.b, color.g), posix_mku16(color.r, color.a));

    const int8_t* src_map_data = ros_instance_.last_map_data();

    for (int y = 0; y < height; y ++) {
        uint32_t* denoise_bgra_data = last_denoise_bgra_.ptr<uint32_t>(y);
        int yoffset = y * width;
        int yoffset_flip = (height - y - 1) * width;
        for (int x = 0; x < width; x ++) {
            uint8_t denoise_u8 = denoise_map_data_[yoffset + x];
            uint8_t src_u8 = src_map_data[yoffset_flip + x];
            if (denoise_bgra_data[x] != u32_erase) {
                to_map_data[yoffset_flip + x] = src_u8;
            } else {
                VALIDATE(denoise_u8 >= MIN_DENOISE_AREA_INDEX && denoise_u8 <= MAX_DENOISE_AREA_INDEX, null_str);
                to_map_data[yoffset_flip + x] = costmap_2d::NO_INFORMATION;
            }
        }
    }

    if (!in_place_save) {
        char rspfile[MAX_PATH];
        SDL_snprintf(rspfile, sizeof(rspfile), "%s/map%04u.rsp", saves_map_dir_.c_str(), reserve_rspfile_index_);
        ros_map.rspfile = rspfile;
    }

    if (is_curmap) {
        // curmap_.charge = tpose2d(charge_position_.x, charge_position_.y, charge_position_.theta);
    }
    save_map_by_ros_map(ros_map);

    if (in_place_save) {
        update_mainmap_from_rspfile(ros_map);
    }
}

void map_controller::did_map_subscribed(const nav_msgs::OccupancyGrid& map)
{
    SDL_Log("[map_controller]%u Received a %d X %d map @ %.3f m/pix o(%.5f, %.5f)",
            SDL_GetTicks(), map.info.width,
            map.info.height,
            map.info.resolution,
            map.info.origin.position.x,
            map.info.origin.position.y);

    if (map.info.width <= 0 || map.info.height <= 0) {
        return;
    }
    VALIDATE(map.data.size() == map.info.width * map.info.height, null_str);
    VALIDATE(map.info.resolution != 0, null_str);
    if (last_src_gray_.cols == 0) {
        if (cur_mode_.mode == mode_buildmap) {
            save_map_widget_->set_visible(true);
            auto_buildmap_widget_->set_visible(game_config::os == os_windows);
        }
    }
    gui_->redraw_minimap();
    last_map_ = map.info;

    if (map_saving_) {
        map_saving_ = false;
        save_map_when_buildmap(map);
        if (game_config::os == os_windows) {
            // map_server_map_saver(map, "C:/ddksample/catkin_ws/src/clbrobot_project/clbrobot/maps/house");
        }
    }

    if (false) {
        int pos = 0;
        SDL_Surface* photo = SDL_CreateRGBSurface(0, map.info.width, map.info.height, 4 * 8,
				0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
		uint32_t* pixels = (uint32_t*)photo->pixels;
        memset(pixels, 0, map.info.width * map.info.height);
        for (unsigned int y = 0; y < map.info.height; y++) {
		    for(unsigned int x = 0; x < map.info.width; x++) {
			    unsigned int i = x + (map.info.height - y - 1) * map.info.width;
                int raw_u8 = map.data[i];
                if (raw_u8 < 0) {
                    VALIDATE(raw_u8 == -1, null_str);
                    pixels[pos ++] = 0xffffffff;
                } else {
                    uint8_t u8 = raw_u8;
                    uint32_t val = posix_mku32(posix_mku16(u8, u8), posix_mku16(u8, 0xff));
                    pixels[pos ++] = val;
                }
		    }
        }
        VALIDATE(pos == map.info.width * map.info.height, null_str);

        std::string file = game_config::preferences_dir + "/1.png";
        const char* c_str = file.c_str();
        IMG_SavePNG(photo, c_str);
        SDL_FreeSurface(photo);
    }

    // const std::string mapdatafile = game_config::preferences_dir + "/house1.pgm";
	// OccupancyGrid_2_pgm(map, mapdatafile);
    last_src_gray_ = cv::Mat(map.info.height, map.info.width, CV_8UC1);
    uint8_t* data = last_src_gray_.ptr<uint8_t>(0);
    const int cells = map.info.width * map.info.height;

    const int8_t* last_map_data = ros_instance_.last_map_data();
    // A bigger value indicates that there is more likely to be an OccupancyGrid. However, when -1(0xff), this point is not touched.
    // To draw this Occupancy Graph more accurately, need to understand the cartographer.
	int threshold_occupied_ = 65;
    // in <libros>/map_server/src/map_saver.cpp, threshold_free_ is 25.
    int threshold_free_ = 49;

	int pos = 0;
    for (unsigned int y = 0; y < map.info.height; y++) {
		for (unsigned int x = 0; x < map.info.width; x++) {
			unsigned int i = x + (map.info.height - y - 1) * map.info.width;

            uint8_t u8;
            const int raw_u8 = last_map_data[i];
            VALIDATE(raw_u8 >= -1 && raw_u8 <= MAX_CARTOGRAPHER_CELL_VAL, null_str);
            if (raw_u8 < 0) {
                // VALIDATE(raw_u8 == -1, null_str);
                u8 = 0xff;

            } else if (raw_u8 >= 0 && raw_u8 <= threshold_free_) { // [0,free)
				// u8 = 254;
                u8 = 0xd0;

			} else if (raw_u8 >= threshold_occupied_) { // (occ,255]
				u8 = 0;

			} else { //occ [0.25, 0.65]
				// u8 = 0xc0;
                // u8 = 0xd0 - (threshold_occupied_ - raw_u8) * 3;
                u8 = 0x90;
			}

            // uint32_t val = posix_mku32(posix_mku16(u8, u8), posix_mku16(u8, 0xff));
            uint8_t val = u8;
            data[pos ++] = val;
		}
    }
    VALIDATE(pos == map.info.width * map.info.height, null_str);

    zoom_src_gray();
/*
    gui2::tdialog* dlg = gui_->get_theme();
	gui2::twindow* window_ = dlg->get_window();
	gui2::find_widget<gui2::tcontrol>(window_, "base_serial", false, true)->set_label(gpose_str);
*/
}

void map_controller::did_current_goal_subscribed(const geometry_msgs::PoseStamped& goal)
{
    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0 && last_map_.resolution > 0, null_str);

    const geometry_msgs::Quaternion& q = goal.pose.orientation;
    tf2::Matrix3x3 mat(tf2::Quaternion(q.x, q.y, q.z, q.w));
    double yaw, pitch, roll;
    mat.getEulerYPR(yaw, pitch, roll);

    ros_current_goal_.x = goal.pose.position.x;
    ros_current_goal_.y = goal.pose.position.y;
    ros_current_goal_.yaw = yaw;
    ros_current_goal_.valid = true;

    gui_->refresh_report(gui2::tmap_scene::STATUS, 
            reports::report(_("navigation^makePlan"), null_str));

    parse_ros_current_goal();

}

void map_controller::parse_ros_current_goal()
{
    VALIDATE(ros_current_goal_.valid, null_str);

    const bool unrestricted_yaw = ros::get_yaw_goal_tolerance() == ROS_UNRESTRICTED_YAW_GOAL_TOLERANCE;
    const std::string filename = unrestricted_yaw? "misc/target32.png": "misc/move_goal.png";
	surface surf(image::get_image(filename));
    SDL_Point& fix_pt = current_goal_png_rotate_point_;
    if (!unrestricted_yaw) {
        fix_pt.x = 0;
        fix_pt.y = surf->h / 2;
        current_goal_surf_ = rotate_surface(surf, RAD2DEG(ros_current_goal_.yaw), &fix_pt, 1);

    } else {
        fix_pt.x = surf->w / 2;
        fix_pt.y = surf->h / 2;
        current_goal_surf_ = surf;
    }

    current_goal_offset_.x = last_map_origin_.x + ros_current_goal_.x / last_map_.resolution;
    current_goal_offset_.y = last_map_origin_.y + -1 * ros_current_goal_.y / last_map_.resolution;

    last_local_plan_start_.x = nposm;
}

void map_controller::resize_global_plan(int size)
{
	size = posix_align_ceil(size, 512);
	VALIDATE(size >= 0, null_str);

	if (size > global_plan_size_) {
		SDL_Point* tmp = (SDL_Point*)malloc(sizeof(SDL_Point) * size);
		if (global_plan_ != nullptr) {
            const int vsize = 0;
            if (vsize != 0) {
				memcpy(tmp, global_plan_, sizeof(SDL_Point) * vsize);
			}
			free(global_plan_);
		}
		global_plan_ = tmp;
		global_plan_size_ = size;
	}
}

void map_controller::did_global_plan_subscribed(const nav_msgs::Path& path)
{
    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0 && last_map_.resolution > 0, null_str);
    ros_global_plan_ = path;
    parse_ros_global_plan();
}

void map_controller::parse_ros_global_plan()
{
    const nav_msgs::Path& path = ros_global_plan_;
    const int size = path.poses.size();

    resize_global_plan(size);
    global_plan_vsize_ = 0;
    int at = 0;
    const int samples = 4; // 4
    bool adjusted = false;
    for (at = 0; at < size; ) {
        const geometry_msgs::PoseStamped& pose_stamped = path.poses[at];
        VALIDATE(pose_stamped.header.frame_id == "map", null_str);
        const geometry_msgs::Pose& pose = pose_stamped.pose;

        int x = last_map_origin_.x + pose.position.x / last_map_.resolution;
        int y = last_map_origin_.y + -1 * pose.position.y / last_map_.resolution;
        if (x >= 0 && x < last_src_gray_.cols && y >= 0 && y < last_src_gray_.rows) {
            global_plan_[global_plan_vsize_].x = x;
            global_plan_[global_plan_vsize_].y = y;
            global_plan_vsize_ ++;
        }
        at += samples;
        if (samples != 1 && at >= size && !adjusted) {
            at = size - 1;
            adjusted = true;
        }
    }
    if (global_plan_vsize_ < 2) {
        global_plan_vsize_ = 0;
    }
}

void map_controller::did_local_plan_subscribed(const nav_msgs::Path& path)
{
    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0 && last_map_.resolution > 0, null_str);
    ros_local_plan_ = path;
    parse_ros_local_plan();
    
}

void map_controller::parse_ros_local_plan()
{
    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0 && last_map_.resolution > 0, null_str);
    const nav_msgs::Path& path = ros_local_plan_;
    const int size = path.poses.size();

    if (size < 2) {
        return;
    }

    local_plan_vsize_ = 0;
    int samples = 1;
    if (size > MAX_LOCAL_PLAN) {
        samples = size / MAX_LOCAL_PLAN + 1;
    }
    int at = 0;
    SDL_Point start;
    for (at = 0; at < size; at += samples) {
        const geometry_msgs::PoseStamped& pose_stamped = path.poses[at];
        // VALIDATE(pose_stamped.header.frame_id == "map", null_str); // odom
        const geometry_msgs::Pose& pose = pose_stamped.pose;

        int x = last_map_origin_.x + pose.position.x / last_map_.resolution;
        int y = last_map_origin_.y + -1 * pose.position.y / last_map_.resolution;
        if (at == 0) {
            start.x = x;
            start.y = y;
        }
        if (x >= 0 && x < last_src_gray_.cols && y >= 0 && y < last_src_gray_.rows) {
            local_plan_[local_plan_vsize_].x = x;
            local_plan_[local_plan_vsize_].y = y;
            local_plan_vsize_ ++;
        }
    }

    if (last_local_plan_start_.x != nposm) { 
        int deltax = SDL_abs(last_local_plan_start_.x - start.x);
        int deltay = SDL_abs(last_local_plan_start_.y - start.y);
        int threashold = 8;
        if (deltax * deltax + deltay * deltay > threashold) {
            utils::string_map symbols;
            char buf[32];
            SDL_snprintf(buf, sizeof(buf), "%i, %i", start.x, start.y);
            symbols["point"] = buf;
            const std::string msg = vgettext2("navigation^move to $point", symbols);
            gui_->refresh_report(gui2::tmap_scene::STATUS, 
                reports::report(msg, null_str));
        }
    }
    last_local_plan_start_ = start;
}

void map_controller::did_recovery_status(const move_base_msgs::RecoveryStatus& status)
{
    utils::string_map symbols;
    // current_recovery_number is base on 0.
    symbols["index"] = utils::index_2_tstr(status.current_recovery_number + 1);
    symbols["total"] = utils::index_2_tstr(status.total_number_of_recoveries);
    const std::string msg = vgettext2("navigation^RecoveryStatus $index, $total", symbols);

    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(msg, null_str));
}

void map_controller::did_move_base_result(const move_base_msgs::MoveBaseActionResult& result, const tros_instance::ttask* task)
{
/*
enum {
    PENDING = 0u,
    ACTIVE = 1u,
    PREEMPTED = 2u,
    SUCCEEDED = 3u,
    ABORTED = 4u,
    REJECTED = 5u,
    PREEMPTING = 6u,
    RECALLING = 7u,
    RECALLED = 8u,
    LOST = 9u,
};
*/
    if (game_config::goal_status.empty()) {
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::PENDING, "PENDING"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::ACTIVE, "ACTIVE"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::PREEMPTED, "PREEMPTED"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::SUCCEEDED, "SUCCEEDED"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::ABORTED, "ABORTED"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::REJECTED, "REJECTED"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::RECALLING, "RECALLING"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::RECALLED, "RECALLED"));
        game_config::goal_status.insert(std::make_pair(actionlib_msgs::GoalStatus::LOST, "LOST"));
    }


    utils::string_map symbols;
    int status = result.status.status;
    symbols["result"] = status == actionlib_msgs::GoalStatus::SUCCEEDED? _("Success"): _("Fail");
    std::string msg;
    if (status == actionlib_msgs::GoalStatus::SUCCEEDED) {
        msg = vgettext2("navigation^$result", symbols);
    } else {
        symbols["errcode"] = game_config::goal_status.count(status) != 0? game_config::goal_status.find(status)->second: "unknown";
        msg = vgettext2("navigation^$result, $errcode", symbols);
    }

    SDL_Log("did_move_base_result, msg: %s, status: %i", msg.c_str(), status);

    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(msg, null_str));
    
    if (!ros_instance_.goaling()) {
        // maybe PREEMPTED
        cancel_goal_widget_->set_visible(false);
    }

    reset_movebase_variables();
}

SDL_DPoint map_controller::mouse_xy_2_ros_world_xy(int mouse_x, int mouse_y) const
{
    SDL_Rect view_rect = gui_->main_map_view_rect();
    // VALIDATE(point_in_rect(mouse_x, mouse_y, view_rect), null_str);
    if (!point_in_rect(mouse_x, mouse_y, view_rect)) {
        // caller maybe pass (mouse_x, mouse_y) that not in view_rect.
    }

    const int zoom = gui_->zoom();
    const int simple_per_meter = 20;

    int rose_mapx = mouse_x;
    int rose_mapy = mouse_y;
    gui_->screen_2_map(rose_mapx, rose_mapy);

    double mapx = 1.0 * rose_mapx * simple_per_meter / zoom;
    double mapy = 1.0 * rose_mapy * simple_per_meter / zoom;

    double offsetx = mapx - last_map_offset_.x - last_map_origin_.x;
    double offsety = mapy - last_map_offset_.y - last_map_origin_.y;

    return SDL_DPoint{offsetx * last_map_.resolution, -1 * offsety * last_map_.resolution};
}

SDL_Point map_controller::meter_2_mouse_xy(double x, double y) const
{
    SDL_Point ret = meter_2_mainmap(x, y);
    gui_->map_2_screen(ret.x, ret.y);
    return ret;
}

void map_controller::immediate_kidnap_msg(int msg, int reason)
{
    VALIDATE(cur_mode_.mode == mode_navigation, null_str);

    kidnap.add_dbg_msg_reason(msg, reason);
    immediate_draw_major_slam();
    gui2::absolute_draw();
}

void map_controller::longpress_widget(bool& halt, const tpoint& coordinate, gui2::twindow& window)
{
	VALIDATE(is_null_coordinate(downing_pt_), null_str);
	halt = true;

    if (last_src_gray_.cols == 0 || last_src_gray_.rows == 0) {
        return;
    }

    if (!ros_instance_.navigation_node_started()) {
        return;
    }

    if (cur_mode_.mode == mode_navigation || (cur_mode_.mode == mode_buildmap && buildmap_with_move_base && !auto_buildmap_)) {
    } else {
        return;
    }

    if (!ros_instance_.move_base_constructed()) {
        return;
    }

    SDL_Rect view_rect = gui_->main_map_view_rect();
    if (!point_in_rect(coordinate.x, coordinate.y, view_rect)) {
        return;
    }

    if (mapviewer_) {
        return;
    }

/*
    const int zoom = gui_->zoom();
    const int simple_per_meter = 1 / last_map_.resolution;
    {
        downing_pt_.x = coordinate.x;
        downing_pt_.y = coordinate.y;

        int rose_mapx = coordinate.x;
        int rose_mapy = coordinate.y;
        gui_->screen_2_map(rose_mapx, rose_mapy);

        double mapx = 1.0 * rose_mapx * simple_per_meter / zoom;
        double mapy = 1.0 * rose_mapy * simple_per_meter / zoom;

        double offsetx = mapx - last_map_offset_.x - last_map_origin_.x;
        double offsety = mapy - last_map_offset_.y - last_map_origin_.y;
        goal_.target_pose.header.seq ++;
        goal_.target_pose.header.frame_id = "map";
        goal_.target_pose.pose.position.x = offsetx * last_map_.resolution;
        // position.y of MoveBaseGoal is reverse.
        goal_.target_pose.pose.position.y = -1 * offsety * last_map_.resolution;
        goal_.target_pose.pose.position.z = 0;
    }
*/
    {
        downing_pt_.x = coordinate.x;
        downing_pt_.y = coordinate.y;

        SDL_DPoint map_xy = mouse_xy_2_ros_world_xy(coordinate.x, coordinate.y);
        {
            SDL_Log("%u coordinate: (%i, %i) ---> map_xy: (%.3f, %.3f)", 
                SDL_GetTicks(), coordinate.x, coordinate.y, map_xy.x, map_xy.y);
            int ii = 0;
        }

        goal_.target_pose.header.seq ++;
        goal_.target_pose.header.frame_id = "map";
        goal_.target_pose.pose.position.x = map_xy.x;
        // position.y of MoveBaseGoal is reverse.
        goal_.target_pose.pose.position.y = map_xy.y;
        goal_.target_pose.pose.position.z = 0;
    }

    const std::string filename = "misc/navigate.png";
	surface surf(image::get_image(filename));
	window.set_drag_surface(surf, false);
/*
    {
        int deltax = x - downing_pt_.x;
        int deltay = y - downing_pt_.y;
        double theta = atan2(deltay, deltax);
        const int r = 100;
        int incx = r * cos(theta);
        int incy = r * sin(theta);

        // atan2's return value: >0(clockwise), <0(anticlockwise). 
        // It is the opposite of what mathematics teaches
        double ros_theta = -1 * theta;
        SDL_Log("did_draw_paper, theta: %.5f, deg: %.5f, [ros]theta: %.5f, deg: %.5f", 
            theta, RAD2DEG(theta), ros_theta, RAD2DEG(ros_theta));

		// if not motion, did_mouse_motion_paper cannot be called, so place in this timer handler.
		render_line(renderer, 0xffff0000, downing_pt_.x, downing_pt_.y, 
            downing_pt_.x + incx, downing_pt_.y + incy);
    }
*/
    int x2 = coordinate.x;
    int y2 = coordinate.y - surf->h / 2;
    SDL_Point custom_xy_formula{0, 0};
	window.start_drag(&custom_xy_formula, tpoint(x2, y2), std::bind(&map_controller::did_drag_mouse_motion, this, _1, _2, _3, std::ref(window)),
		std::bind(&map_controller::did_drag_mouse_leave, this, _1, _2, _3));
}

bool map_controller::did_drag_mouse_motion(const int x, const int y, tpoint& new_mouse, gui2::twindow& window)
{
    VALIDATE(last_src_gray_.cols > 0 && last_src_gray_.rows > 0, null_str);

    SDL_Rect view_rect = gui_->main_map_view_rect();
    if (!point_in_rect(x, y, view_rect)) {
        return false;
    }

    int deltax = x - downing_pt_.x;
    int deltay = y - downing_pt_.y;
    double theta = atan2(deltay, deltax);
    // const int r = 100;
    // int incx = r * cos(theta);
    // int incy = r * sin(theta);

    // atan2's return value: >0(clockwise), <0(anticlockwise). 
    // It is the opposite of what mathematics teaches
    double ros_theta = -1 * theta;
    SDL_Log("did_draw_paper, theta: %.5f, deg: %.5f, [ros]theta: %.5f, deg: %.5f", 
        theta, RAD2DEG(theta), ros_theta, RAD2DEG(ros_theta));

    const std::string filename = "misc/navigate.png";
	surface surf(image::get_image(filename));
    SDL_Point fix_pt{0, surf->h / 2};
    surface rotated_surf = rotate_surface(surf, RAD2DEG(ros_theta), &fix_pt, 1);

    window.set_drag_surface(rotated_surf, false);
    new_mouse.x = downing_pt_.x - fix_pt.x;
    new_mouse.y = downing_pt_.y - fix_pt.y;

	return true;
}

void map_controller::did_drag_mouse_leave(const int x, const int y, bool up_result)
{
    tpoint downing_pt = downing_pt_;
    set_null_coordinate(downing_pt_);

	if (up_result) {
        unsigned int goal_map_x;
        unsigned int goal_map_y;
        bool goal_valid = worldToMap(last_map_, goal_.target_pose.pose.position.x, goal_.target_pose.pose.position.y, goal_map_x, goal_map_y);
        if (!goal_valid) {
            set_status_report(_("Set the Nav Goal within the rectangle where the map is located"));
            return;
        }

		int deltax = x - downing_pt.x;
        int deltay = y - downing_pt.y;
        double theta = atan2(deltay, deltax);

        // atan2's return value: >0(clockwise), <0(anticlockwise). 
        // It is the opposite of what mathematics teaches
        double ros_theta = -1 * theta;

        goal_.target_pose.header.stamp = ros::Time::now();
        tf2::Quaternion q;
        q.setRPY(0, 0, ros_theta);
        goal_.target_pose.pose.orientation.x = q.x();
        goal_.target_pose.pose.orientation.y = q.y();
        goal_.target_pose.pose.orientation.z = q.z();
        goal_.target_pose.pose.orientation.w = q.w();

        {
            // goal_.target_pose.pose.position.x = 0.58333;
            // goal_.target_pose.pose.position.y = -0.39583;
            // set_quaternion_raw_nposm(goal_.target_pose.pose.orientation);
        }

        SDL_Log("did_mouse_leave_camera, deg: %.5f, position(%.5f, %.5f, %.5f), orientation(%.5f, %.5f, %.5f, %.5f)", 
            RAD2DEG(ros_theta), 
            goal_.target_pose.pose.position.x, goal_.target_pose.pose.position.y, goal_.target_pose.pose.position.z,
            goal_.target_pose.pose.orientation.x, goal_.target_pose.pose.orientation.y, goal_.target_pose.pose.orientation.z, goal_.target_pose.pose.orientation.w);

        ros_instance_.do_send_goal(goal_);
        if (ros_instance_.goaling()) {
	        cancel_goal_widget_->set_visible(true);
        }
	}
}

static surface get_normal_position_surf(const tmap_position& position)
{
    return font::get_rendered_text(position.name, 0, font::SIZE_DEFAULT, font::BLACK_COLOR);
}

tmap_position* map_controller::position_from_uuid(const std::string& uuid)
{
    VALIDATE(curmap_.positions.count(uuid) != 0, null_str);
    return &curmap_.positions.find(uuid)->second;
}

void map_controller::longpress_position(bool& halt, const tpoint& coordinate, gui2::twindow& window, gui2::ttoggle_panel& row)
{
	VALIDATE(cur_mode_.mode == mode_position, null_str);
    VALIDATE(longpressing_position_ == nullptr, null_str);
    VALIDATE(is_float_nposm(position_downing_at_.x) && is_float_nposm(position_downing_at_.y), null_str);

	halt = true;

    const std::string& select_uuid = dlg_->row_uuid(row.at());
    longpressing_position_ = position_from_uuid(select_uuid);

	surface surf = get_normal_position_surf(*longpressing_position_);

	window.set_drag_surface(surf, false);

    // tpoint new_coordinate(coordinate.x - position_tl_offset_.x, coordinate.y - position_tl_offset_.y);

    SDL_Point custom_xy_formula{-position_tl_offset_.x, -position_tl_offset_.y};
	window.start_drag(&custom_xy_formula, coordinate, std::bind(&map_controller::did_position_drag_mouse_motion, this, _1, _2, _3, std::ref(window)),
		std::bind(&map_controller::did_position_drag_mouse_leave, this, _1, _2, _3));
}

bool map_controller::did_position_drag_mouse_motion(const int x, const int y, tpoint& new_mouse, gui2::twindow& window)
{
    VALIDATE(cur_mode_.mode == mode_position, null_str);

    bool allow = false;
    SDL_Rect view_rect = gui_->main_map_view_rect();
    if (point_in_rect(x, y, view_rect)) {
        const int zoom = gui_->zoom();
        const int simple_per_meter = 1 / last_map_.resolution;

        int rose_mapx = x - (position_tl_offset_.x - map_pin_pt_.x);
        int rose_mapy = y - (position_tl_offset_.y - map_pin_pt_.y);
        gui_->screen_2_map(rose_mapx, rose_mapy);

        double mapx = 1.0 * rose_mapx * simple_per_meter / zoom;
        double mapy = 1.0 * rose_mapy * simple_per_meter / zoom;

        // offset relative top-left-point.
        int tl_offsetx = (int)(mapx - last_map_offset_.x);
        int tl_offsety = (int)(mapy - last_map_offset_.y);
        
        if (tl_offsetx > 1 && tl_offsety > 1) {
            if (tl_offsetx < last_src_gray_.cols && tl_offsety < last_src_gray_.rows) {
                const uint8_t* data = last_src_gray_.ptr<uint8_t>(0);
                const uint8_t u8 = data[tl_offsety * last_src_gray_.cols + tl_offsetx];
                allow = u8 != 0xff && u8 != 0;
            }
        }

        // offset relative origin-point.
        double offsetx = mapx - last_map_offset_.x - last_map_origin_.x;
        double offsety = mapy - last_map_offset_.y - last_map_origin_.y;

        double position_x = offsetx * last_map_.resolution;
        double position_y = -1 * offsety * last_map_.resolution;

        // SDL_Log("did_position_drag_mouse_motion, offset:(%.2f, %.2f) map position:(%.2f, %.2f)", 
        //    offsetx, offsety, position_x, position_y);
        if (allow) {
            position_downing_at_.x = (float)position_x;
            position_downing_at_.y = (float)position_y;
        } else {
            position_downing_at_ = SDL_FPoint{float_nposm, float_nposm};
        }
    }

    surface surf = allow? map_pin_surf_: get_normal_position_surf(*longpressing_position_);
    window.set_drag_surface(surf, false);

    // new_mouse.x -= position_tl_offset_.x;
    // new_mouse.y -= position_tl_offset_.y;

	return true;
}

static bool outof_threshold(double a, double b)
{
    double diff = a - b;
    const double threshold = 0.1; // 10cm
    return posix_abs(diff) > threshold;
}

void map_controller::did_position_drag_mouse_leave(const int x, const int y, bool up_result)
{
	VALIDATE(cur_mode_.mode == mode_position, null_str);

	if (up_result) {
        if (!is_float_nposm(position_downing_at_.x)) {
            VALIDATE(!is_float_nposm(position_downing_at_.x), null_str);

            if (outof_threshold(position_downing_at_.x, longpressing_position_->x) || outof_threshold(position_downing_at_.y, longpressing_position_->y)) {
                gui2::tlistbox& list = dlg_->positions_widget();
                gui2::ttoggle_panel& row = list.row_panel(0);

                longpressing_position_->x = position_downing_at_.x;
                longpressing_position_->y = position_downing_at_.y;
                // double default_theta = 0; // -1 * DEG2RAD(30);
                // longpressing_position_->theta = default_theta;
                save_map_when_position();
                dlg_->reload_position_list(dlg_->positions_widget());

                std::map<std::string, thalo>::iterator halo_it = position_halos_.find(longpressing_position_->uuid);
                if (halo_it != position_halos_.end()) {
                    position_halos_.erase(halo_it);
                }
            }
        }
	}
    longpressing_position_ = nullptr;
    position_downing_at_ = SDL_FPoint{float_nposm, float_nposm};
}

void map_controller::did_draw_cartographer(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, int type)
{
    SDL_Renderer* renderer = get_renderer();
    if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), nullptr, &widget_rect);
	}
    {
        // After 'clear' use white color below, 
        // there is still a problem of displaying redundant point clouds
        
        // texture bg_tex = get_white_texture();
        // SDL_RenderCopy(renderer, bg_tex.get(), nullptr, &widget_rect);
    }

    if (require_show_help()) {
        return;
    }

    if (type == track_major) {
        if (is_kidnap_page()) {
            draw_kidnap_track(widget_rect);
        } else {
            cartographer::rose_slot.draw_major_track(widget_rect);
        }
        
    } else if (type == track_minor) {
        if (is_kidnap_page()) {

        } else {
            cartographer::rose_slot.draw_minor_track(widget_rect);
        }

    }
}

void map_controller::signal_handler_longpress_cartographer(bool& halt, const tpoint& coordinate, int type)
{
	halt = true;
    if (type == track_major) {
        if (is_kidnap_page()) {
            save_kidnap_dbg_msgs();

        } else {
            save_cartographer_pose_msgs();
        }

    } else if (type == track_minor) {
        bool dirty = cartographer::rose_slot.did_minor_longpress();
        if (dirty) {
            if (is_normal_page()) {
                // show/hide 'camera_scan'
                slam_major_widget_->set_background_tex_dirty();
                immediate_draw_major_slam();
            }
            if (!slam_track_4_dcamera_) {
                slam_minor_widget_->immediate_draw();
            }
        }
    }
}

texture map_controller::did_create_background_tex(gui2::ttrack& widget, const SDL_Rect& draw_rect, int type)
{
	surface bg_surf = create_neutral_surface(draw_rect.w, draw_rect.h);
	uint32_t bg_color = 0xfffdfdfd;
    // uint32_t bg_color = 0xff808080;
	fill_surface(bg_surf, bg_color);

    SDL_Renderer* renderer = get_renderer();

    SDL_Rect dstrect{0, 0, 0, 0};
    if (require_show_help()) {
        utils::string_map symbols;
        std::string msg;
        if (type == track_major) {
            if (cur_mode_.mode == mode_buildmap) {
                msg = _("help^mode_buildmap");

            } else if (cur_mode_.mode == mode_navigation) {
                if (denoising_map_) {
                    symbols["save"] = _("Save");
                    msg = vgettext2("help^mode_navigation denoising $save", symbols);
                } else {
                    msg = _("help^mode_navigation");
                }

            } else if (cur_mode_.mode == mode_position) {
                msg = _("help^mode_position");
            }
        }
        if (!msg.empty()) {
            const int gap_w = 4 * gui2::twidget::hdpi_scale;
            surface text_surf = font::get_rendered_text(msg, draw_rect.w - gap_w, font::SIZE_SMALLER, font::BLACK_COLOR);

            dstrect.w = text_surf->w;
            dstrect.h = text_surf->h;
            sdl_blit(text_surf, nullptr, bg_surf, &dstrect);
        }

    } else if (type == track_major && cartographer::rose_slot.current_page() == cartographer::trose_slot::LOCAL_SLAM_PAGE) {
        std::vector<std::string> labels;
        labels.push_back("accumulated_range_data_");
        labels.push_back("gravity_aligned_range_data");
        labels.push_back("range_data_in_local");

        // const int gap_h = cartographer::rose_slot.range_data_gap_h;
        const int gap_h = 0;
        int range_data_h = (draw_rect.h - 2 * gap_h) / 3;
        for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
            const std::string& str = *it;
            surface text_surf = font::get_rendered_text(str, 0, font::SIZE_SMALLER, font::BLACK_COLOR);

            dstrect.w = text_surf->w;
            dstrect.h = text_surf->h;
            sdl_blit(text_surf, nullptr, bg_surf, &dstrect);
            dstrect.y += range_data_h + gap_h;
        }

    } else if (type == track_major && is_normal_page()) {
        if (cartographer::rose_slot.show_camera_scan()) {
            const int circle_count = draw_dcamera_points_settings_.ranges;
            cv::Scalar axis_color(0x00, 0xff, 0x00, 0xff);
            const int axis_thinkness = 1;

            cv::Mat image;
            image.create(bg_surf->w, bg_surf->w, CV_8UC4);
            image.setTo(0x00);
            cv::Point center{0, image.rows / 2};
            for (int at = 0; at < circle_count; at ++) {
                cv::circle(image, center, image.cols / circle_count * (at + 1), axis_color, axis_thinkness);
            }
            cv::line(image, center, cv::Point(center.x + bg_surf->w, center.y), axis_color, axis_thinkness);
            // +30 rayline
            int y_offset = image.cols * tan(DEG2RAD(MOVEIT_SECTOR_HALF_ANGLE_DEG));
            cv::line(image, center, cv::Point(center.x + bg_surf->w, center.y - y_offset), axis_color, axis_thinkness);
            // -30 rayline
            y_offset = image.cols * tan(DEG2RAD(-1 * MOVEIT_SECTOR_HALF_ANGLE_DEG));
            cv::line(image, center, cv::Point(center.x + bg_surf->w, center.y - y_offset), axis_color, axis_thinkness);

            // draw origin
            cv::circle(image, center, 3, cv::Scalar(0xff, 0x00, 0x00, 0xff), -1);

            surface surf(image);
            dstrect.y = bg_surf->h - surf->h;
            dstrect.w = surf->w;
            dstrect.h = surf->h;
            sdl_blit(surf, nullptr, bg_surf, &dstrect);

            // draw x-axis scale
            for (int at = 0; at < circle_count; at ++) {
                const std::string msg = utils::from_double(draw_dcamera_points_settings_.meter_per_range * (at + 1));
                surface text_surf = font::get_rendered_text(msg, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);

                dstrect.x = image.cols / circle_count * (at + 1);
                dstrect.y = bg_surf->h - (image.rows / 2) - text_surf->h / 2;
                dstrect.w = text_surf->w;
                dstrect.h = text_surf->h;
                sdl_blit(text_surf, nullptr, bg_surf, &dstrect);
            }
        }

    } else if (type == track_major && is_kidnap_page()) {
        const std::string msg = _("Long press to save kdinap msg");
        if (!msg.empty()) {
            surface text_surf = font::get_rendered_text(msg, 0, font::SIZE_SMALLER, font::BLACK_COLOR);

            dstrect.y = bg_surf->h - text_surf->h;
            dstrect.w = text_surf->w;
            dstrect.h = text_surf->h;
            sdl_blit(text_surf, nullptr, bg_surf, &dstrect);

            kdinap_track_top_gap_h_ = text_surf->h;
        }

    }

	return SDL_CreateTextureFromSurface2(renderer, bg_surf.get());
}

void map_controller::draw_kidnap_track(const SDL_Rect& _bg_rect)
{
    if (!ros_instance_.navigation_node_started()) {
        kidnap.dbg_msgs_dirty = false;
        return;
    }

    const int max_disp_msgs = 22; // 17 line. but some msg is 2 lines.
    tcharcdata_C dbg_msgs = kidnap.dbg_msgs_to_string(max_disp_msgs, '\n');
    if (dbg_msgs.len == 0) {
        return;
    }

    SDL_Renderer* renderer = get_renderer();
    //
    SDL_Rect bg_rect = _bg_rect;
    // bg_rect.y += kdinap_track_top_gap_h_;
    // bg_rect.h -= kdinap_track_top_gap_h_;

    // int bottom_gap_h = 4 * gui2::twidget::hdpi_scale;
    int bottom_gap_h = kdinap_track_top_gap_h_;

    surface text_surf = font::get_rendered_text(dbg_msgs.ptr, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    int hidden_h = text_surf->h + bottom_gap_h > bg_rect.h? text_surf->h + bottom_gap_h - bg_rect.h: 0;
    SDL_Rect dstrect = bg_rect;
    if (hidden_h != 0) {
        dstrect.y -= hidden_h;
    }
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;

    SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void map_controller::save_cartographer_pose_msgs() const
{
    const std::string filename = "cartogapher_pose_msgs.txt";
    tcharcdata_C pose_msgs = cartographer::rose_slot.pose_msgs_to_string(nposm, ' ');
    tfile file(game_config::preferences_dir + "/" + filename, GENERIC_WRITE, CREATE_ALWAYS);
    if (!file.valid()) {
        return;
    }

    const std::string time_str = utils::format_time_ymdhms(time(nullptr));
    std::stringstream ss;
    ss << time_str;
    ss << "\n";
    ss << "range_data_poses.back() is same as non_gravity_aligned_pose_prediction\n";
    ss << "transform_to_gravity_aligned_frame and pose_prediction are inverses of each other\n\n";

    posix_fwrite(file.fp, ss.str().c_str(), ss.str().size());
    if (pose_msgs.len != 0) {
        posix_fwrite(file.fp, pose_msgs.ptr, pose_msgs.len);
    }

    utils::string_map symbols;
    symbols["time"] = utils::format_time_hms(time(nullptr));
    symbols["file"] = filename;
    const std::string msg = vgettext2("$time save pose-message to $file successfully", symbols);
    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(msg, null_str));
}

void map_controller::save_kidnap_dbg_msgs() const
{
    // if (ros_instance_.navigation_node_started()) {
    //    return;
    // }

    const std::string filename = "kidnap_dbg_msgs.txt";
    tcharcdata_C dbg_msgs = kidnap.dbg_msgs_to_string(nposm, ' ');
    tfile file(game_config::preferences_dir + "/" + filename, GENERIC_WRITE, CREATE_ALWAYS);
    if (!file.valid()) {
        return;
    }

    const std::string time_str = utils::format_time_ymdhms(time(nullptr));
    std::stringstream ss;
    ss << time_str;
    ss << "\n\n";

    posix_fwrite(file.fp, ss.str().c_str(), ss.str().size());
    if (dbg_msgs.len != 0) {
        posix_fwrite(file.fp, dbg_msgs.ptr, dbg_msgs.len);
    }

    utils::string_map symbols;
    symbols["time"] = utils::format_time_hms(time(nullptr));
    symbols["file"] = filename;
    const std::string msg = vgettext2("$time save kidnap-debug-message to $file successfully", symbols);
    gui_->refresh_report(gui2::tmap_scene::STATUS, 
        reports::report(msg, null_str));
}

SDL_Point map_controller::meter_2_mainmap(double x, double y) const
{
/*
    double mapx = 1.0 * rose_mapx * simple_per_meter / zoom;
    double mapy = 1.0 * rose_mapy * simple_per_meter / zoom;

    // offset relative origin-point.
    double offsetx = mapx - last_map_offset_.x - last_map_origin_.x;
    double offsety = mapy - last_map_offset_.y - last_map_origin_.y;

    double position_x = offsetx * last_map_.resolution;
    double position_y = -1 * offsety * last_map_.resolution;
*/

    double offsetx = x / last_map_.resolution;
    double offsety = -1 * y / last_map_.resolution;

    double mapx = last_map_offset_.x + last_map_origin_.x + offsetx;
    double mapy = last_map_offset_.y + last_map_origin_.y + offsety;

    const int zoom = gui_->zoom();
    const int simple_per_meter = 1 / last_map_.resolution;

    int rose_mapx = round(mapx * zoom / simple_per_meter);
    int rose_mapy = round(mapy * zoom / simple_per_meter);
    return SDL_Point{rose_mapx, rose_mapy};
}

static surface create_red_line_surf(const tmap_marker& marker, int zoom, SDL_Point& fix_blit_pt)
{
    surface icon = image::get_image("misc/red_line.png");

    const float scale_ratio = 1.0 * zoom;
    int width = marker.rsp.width * scale_ratio;
    icon = scale_surface(icon, width, icon->h);

    SDL_Point fix_pt{0, icon->h / 2};
    if (marker.rsp.theta != 0) {
        fix_pt.x = 0;
        fix_pt.y = icon->h / 2;
        icon = rotate_surface(icon, RAD2DEG(marker.rsp.theta), &fix_pt, 1);
    }
    fix_blit_pt = fix_pt;
    // rule: text_surf's center is align with fix_pt.
    return icon;
}

void map_controller::add_marker_haloe(const tmap_marker& marker)
{
    VALIDATE(marker_halos_.count(marker.rsp.uuid) == 0, null_str);

    SDL_Point fix_blit_pt;
    surface surf = create_red_line_surf(marker, gui_->zoom(), fix_blit_pt);
	image::tblit blit(surf, -fix_blit_pt.x, -fix_blit_pt.y, surf->w, surf->h);
    SDL_Point rose_map_xy = meter_2_mainmap(marker.rsp.x, marker.rsp.y);
	int halo = halo::add(rose_map_xy.x, rose_map_xy.y, false, blit);
    // must not use thalo(halo), it will result a ~thalo.
    marker_halos_.insert(std::make_pair(marker.rsp.uuid, halo));
}

static surface create_position_surf(const tmap_position& position, SDL_Point& fix_blit_pt)
{
    const SDL_Color& color = position.uuid == charge_pos_uuid? font::GOOD_COLOR: font::BLUE_COLOR;
    surface text_surf = font::get_rendered_text(position.name, 0, font::SIZE_DEFAULT, color);
    surface icon = image::get_image(is_float_nposm(position.theta)? "misc/red_circle.png": "misc/map_position.png");

    SDL_Point fix_pt;
    if (!is_float_nposm(position.theta)) {
        fix_pt.x = 14;
        fix_pt.y = icon->h / 2;
        icon = rotate_surface(icon, RAD2DEG(position.theta), &fix_pt, 1);

    } else {
        fix_pt.x = icon->w / 2;
        fix_pt.y = icon->h / 2;
    }
    // rule: text_surf's center is align with fix_pt.

    // SDL_Point fix_blit_pt;
    int text_half_width = text_surf->w / 2;
    int text_half_height = text_surf->h / 2;
    int width = fix_pt.x;
    fix_blit_pt.x = fix_pt.x;
    if (fix_pt.x < text_half_width) {
        width = text_half_width;
        fix_blit_pt.x = text_half_width;
    }
    width += icon->w - fix_pt.x >= text_half_width? icon->w - fix_pt.x: text_half_width;

    int height = fix_pt.y;
    fix_blit_pt.y = fix_pt.y;
    if (fix_pt.y < text_half_height) {
        height = text_half_height;
        fix_blit_pt.y = text_half_height;
    }
    height += icon->h - fix_pt.y >= text_half_height? icon->h - fix_pt.y: text_half_height;

    surface ret = create_neutral_surface(width, height);
    SDL_Rect dst_rect = create_rect(fix_blit_pt.x - fix_pt.x, fix_blit_pt.y - fix_pt.y, icon->w, icon->h);
    sdl_blit(icon, nullptr, ret, &dst_rect);

    dst_rect = create_rect(fix_blit_pt.x - text_surf->w / 2, fix_blit_pt.y - text_surf->h / 2, icon->w, text_surf->h);
    sdl_blit(text_surf, nullptr, ret, &dst_rect);
    return ret;
}

void map_controller::add_position_haloe(const tmap_position& position)
{
    VALIDATE(position_halos_.count(position.uuid) == 0, null_str);

    SDL_Point fix_blit_pt;
    surface surf = create_position_surf(position, fix_blit_pt);
	image::tblit blit(surf, -fix_blit_pt.x, -fix_blit_pt.y, surf->w, surf->h);
    SDL_Point rose_map_xy = meter_2_mainmap(position.x, position.y);
	int halo = halo::add(rose_map_xy.x, rose_map_xy.y, false, blit);
    // must not use thalo(halo), it will result a ~thalo.
    position_halos_.insert(std::make_pair(position.uuid, halo));
}

void map_controller::add_haloes()
{
    if (cur_mode_.mode == mode_navigation) {
        // before add haloe, must make sure map has display. 
        // else haloe.x/y calcuated is error.
        if (navigation_rspfile_.empty()) {
            return;
        }

    } else if (cur_mode_.mode == mode_position) {

    } else {
        return;
    }

    const std::map<std::string, tmap_marker>& markers = cur_mode_.mode == mode_navigation? navigation_markers_: curmap_.markers;
    for (std::map<std::string, tmap_marker>::const_iterator it = markers.begin(); it != markers.end(); ++ it) {
        // uuid in tmap_marker is char[], not std::string.
        const std::string& uuid = it->first;
        const tmap_marker& marker = it->second;
        if (exclude_marker_halo_ == uuid) {
            continue;
        }

        // if (marker_halos_.count(marker.uuid) == 0 && position.valid_xy()) {
        if (marker_halos_.count(uuid) == 0) {
            add_marker_haloe(marker);
        }
    }

    if (cur_mode_.mode != mode_position) {
        return;
    }
    // dispaly poistion only in mode_position.
    
    // if (curmap_.valid() && position_halos_.count(charge_pos_uuid) == 0) {
    //   add_position_haloe(charge_position_);
    // }

    for (std::map<std::string, tmap_position>::iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
        const tmap_position& position = it->second;
        if (position_halos_.count(position.uuid) == 0 && position.valid_xy()) {
            add_position_haloe(position);
        }
    }    
}

void map_controller::did_post_scroll(int dx, int dy)
{
    if (!marker_placer_->is_visible()) {
        return;
    }
    marker_placer_->scroll(dx, dy);
}

void map_controller::set_anchor_mainmap_xy()
{
    VALIDATE(marker_placer_->is_visible(), null_str);

    // {gui_ratio changed}[1/3]get anchor_pt's mainmap_xy when old-zoom.
    // must use anchor_pt, not left-top of std_rect. during zoom change, xy-diff is changed also.
    SDL_Point mouse_xy = marker_placer_->anchor_mouse_xy();

    SDL_Point mainmap_xy = mouse_xy;
    gui_->screen_2_map(mainmap_xy.x, mainmap_xy.y);
    marker_placer_->set_anchor_mainmap_xy(mainmap_xy);
}

void map_controller::post_set_zoom()
{
    zoom_src_gray();

    if (marker_placer_->is_visible()) {
        VALIDATE(!exclude_marker_halo_.empty(), null_str);
        const std::string& uuid = exclude_marker_halo_;
        VALIDATE(curmap_.markers.count(uuid) != 0, null_str);

        SDL_Point mainmap_xy = marker_placer_->anchor_mainmap_xy();

        // {gui_ratio changed}[2/3]calcuate anchor_pt's mainmap_xy when new-zoom.
        double gui_ratio = marker_placer_->gui_ratio();
        mainmap_xy.x = mainmap_xy.x * gui_->zoom() / gui_ratio;
	    mainmap_xy.y = mainmap_xy.y * gui_->zoom() / gui_ratio;

        // {gui_ratio changed}[3/3]get anchor_pt's mouse_xy when new-zoom. it will set to marker_placer_.
        SDL_Point mouse_xy = mainmap_xy;
        gui_->map_2_screen(mouse_xy.x, mouse_xy.y);
        
        // tmap_marker& marker = curmap_.markers.find(uuid)->second;
        // SDL_Point mouse_xy = meter_2_mouse_xy(marker.rsp.x, marker.rsp.y);
        marker_placer_->gui_ratio_changed(mouse_xy.x, mouse_xy.y, 1.0 * gui_->zoom());

        // remember anchor's maimap xy in current zoom.
        set_anchor_mainmap_xy();
    }
}

void tros_instance::calculate_ros_map_4_server(const std::string& rspfile, bool verbose)
{
    VALIDATE(!rspfile.empty(), null_str);

    VALIDATE(map_4_map_server_.data.empty(), null_str);

    tros_map ros_map_other;
    bool is_curmap = curmap_.valid() && curmap_.rspfile == rspfile;
    if (!is_curmap) {
        bool ret = ros::load_map_from_rsp(rspfile, ros_map_other);
        if (!ret || !ros_map_other.valid()) {
            gui2::show_message(null_str, "generate_map_with_walls failed. Failed to read the source map files");
            return;
        }
    }
    const tros_map& ros_map = is_curmap? curmap_: ros_map_other;
    if (!verbose && ros_map.markers.empty()) {
        map_4_map_server_ = ros_map.map;
        return;
    }

    map_4_map_server_.header = ros_map.map.header;
    map_4_map_server_.info = ros_map.map.info;


    const ::nav_msgs::MapMetaData& info = ros_map.map.info;

    const int8_t* last_map_data = &ros_map.map.data[0];
    const int width = info.width;
    const int height = info.height;
    const int cells = width * height;

    uint8_t* yflip_map_data = (uint8_t*)malloc(cells);
    // 1)y-flip. last_map_data->yflip_map_data[0]
    int pos = 0;
    for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			int i = x + (height - y - 1) * width;
            uint8_t u8 = last_map_data[i];
            yflip_map_data[pos] = u8;
            pos ++;
		}
    }
    VALIDATE(pos == width * height, null_str);

    if (verbose) {
        surface bg_surf = u8_data_2_argb_surf((const uint8_t*)yflip_map_data, width, height);
        imwrite(bg_surf, "2-raw.png");

        surface cell_value_surf = u8_data_2_cell_value_surf((const uint8_t*)last_map_data, 4, 4, info.width, info.height, maptype_OccupancyGrid, nullptr, nullptr);
        imwrite(cell_value_surf, "2-cell_value.png");
    }

    // 2)blit markers to yflip_map_data
    for (std::map<std::string, tmap_marker>::const_iterator it = ros_map.markers.begin(); it != ros_map.markers.end(); ++ it) {
        const tmap_marker& marker = it->second;
        SDL_Point fix_blit_pt;
        surface surf = create_red_line_surf(marker, PIXELS_PER_METER20, fix_blit_pt);
	    
        // unsigned int map_x;
        // unsigned int map_y;
        // bool valid = worldToMap(info, marker.rsp.x, marker.rsp.y, map_x, map_y);
        // VALIDATE(valid, null_str);
        
        int map_x;
        int map_y;
        worldToMapNoBounds(info.origin.position.x, info.origin.position.y, info.resolution, marker.rsp.x, marker.rsp.y, map_x, map_y);

        map_y = info.height - 1 - map_y;

        const_surface_lock lock(surf);
        const int surf_w = surf->w;
        const int surf_h = surf->h;

        const int min_red_line_aplha = 220;
        const uint32_t* surf_pixels = lock.pixels();
        int base = (map_x - fix_blit_pt.x) + (map_y - fix_blit_pt.y) * width;
	    for (int y = 0; y < surf_h; y ++) {
		    const uint32_t* data = surf_pixels + y * surf_w;
		    for (int x = 0; x < surf_w; x ++) {
			    uint32_t value = data[x];
                uint32_t alpha = (value & 0xff000000) >> 24;
                if (alpha >= min_red_line_aplha) {
                    int index = y * width + x;
                    int to_index = base + index;
                    if (to_index >= 0 && to_index < cells) {
                        yflip_map_data[to_index] = MAX_CARTOGRAPHER_CELL_VAL;
                    }
                }
		    }
	    }
        
        // SDL_Rect dst_rect = create_rect(map_x - fix_blit_pt.x, map_y - fix_blit_pt.y, surf->w, surf->h);
        // sdl_blit(surf, nullptr, bg_surf, &dst_rect);
    }

    // 3)y-flip again. yflip_map_data -> map_4_map_server_.data
    map_4_map_server_.data.resize(cells);
    int8_t* map_server_data = &map_4_map_server_.data[0];

    pos = 0;
    // uint8_t* yflip_map_data1 = yflip_map_data + cells;
    for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			int i = x + (height - y - 1) * width;
            uint8_t u8 = yflip_map_data[i];
            map_server_data[pos] = u8;
            pos ++;
		}
    }
    VALIDATE(pos == width * height, null_str);

    if (verbose) {
        surface cell_value_surf = u8_data_2_cell_value_surf((const uint8_t*)map_server_data, 4, 4, width, height, maptype_OccupancyGrid, nullptr, nullptr);
        imwrite(cell_value_surf, "2-cell_value-post.png");
    }

    free(yflip_map_data);
    if (verbose) {
        gui2::show_message(null_str, "generate map with walls finished.");
    }
}

// pass as std::string object, and not 'const char*'. main-thread may had free 'const char*' owner.
static void safe_aplt_serial_driver_main(bool& exit, const std::string& node, int baudrate, faplt_serial_driver_main driver_main)
{
    driver_main(exit, node.c_str(), baudrate);
}

static void safe_aplt_serial_driver_fake(bool& exit, const std::string& node, int baudrate)
{
    SDL_Log("safe_aplt_serial_driver_fake, node: %s, baudrate: %i", node.c_str(), baudrate);
}

void tros_instance::start_navigation_node(int mode, int rosbag, const tdrivers::tvars& vars, const std::string& rspfile)
{
    SDL_Log("start_navigation_node(mode: %i, rosbag: %i, rspfile: %s)", mode, rosbag, rspfile.c_str());
    // validate [mode, rosbag] is valid.
    find_gui_mode(mode, rosbag);

    VALIDATE(vars.base.valid(true), null_str);
    VALIDATE(vars.laser.valid(true), null_str);

    const trpy& rpy = get_imu_rpy();

    if (moveit_driver_.installed()) {
        VALIDATE(vars.moveit.valid(true), null_str);
        VALIDATE(rpy.valid, null_str);
    }

	VALIDATE(mode_ == nposm, null_str);
    VALIDATE(rosbag_ == nposm, null_str);
	VALIDATE(mode >= 0 && mode < mode_count, null_str);
    if (mode == mode_navigation) {
        VALIDATE(!rspfile.empty(), null_str);
        if (!base_driver_.is_same_navigation_rspfile(rspfile)) {
            // navigation file changed, 1)give up last pose2d, 2)require full2 position.
            kidnap.last_pose2d.valid = false;
            robot_imu_.require_full2_position = true;
            robot_imu_.has_result_ok = false;

        } else if (!robot_imu_.require_full2_position && !robot_imu_.has_result_ok) {
            SDL_Log("Even require_full2_position is false, because move didn't ok, still changed require_full2_position to true");
            robot_imu_.require_full2_position = true;
        }
    }
    VALIDATE(started_, null_str);
    VALIDATE(navigation_map_.info.width == 0 && navigation_map_.info.height == 0, null_str);
    VALIDATE(navigation_goal_.header.frame_id.empty(), null_str);
    VALIDATE(dcamera_point_vsize_ == 0, null_str);
    VALIDATE(is_float_nposm(dcamera_2_laser_xy_.x), null_str);

    VALIDATE(navigation_start_ticks_ == 0, null_str);
    navigation_start_ticks_ = SDL_GetTicks();

    mode_ = mode;
    rosbag_ = rosbag;
    move_base_constructed_ = false;

    subscribe_movebase_topics();

    VALIDATE(base_driver_.node_started(), null_str);
    const int restart_base_node_threshold = 5000; // 5 second
    if (SDL_GetTicks() > aplt::valuex.last_NMTHREAD_battery_level_ticks() + restart_base_node_threshold) {
        base_driver_.restart_node();
    }

    tpose2d laser_pose2d;
    tpose2d dcamera_pose2d;
    base_driver_.enter_navigation(mode_ == mode_buildmap, laser_pose2d, dcamera_pose2d);
    // start_base_or_moveit_serial(true, mode_ == mode_buildmap);

    cartographer::rose_slot.did_navigation_start(mode_ == mode_buildmap, dcamera_driver_.installed());
    VALIDATE(!kidnap.last_ok_estimated_pose2d().valid, null_str);
    VALIDATE(!kidnap.estimating(), null_str);

    kidnap.cartographer_pose2d.valid = false;
    if (mode == mode_navigation) {
        // don't use 'if (kidnap.is_allowed())', because imu maybe use base-serial.
        kidnap.did_start_navigation();
        if (kidnap.last_pose2d.valid) {
            // kidnap.cartographer_pose2d = tpose2d(1, 2, DEG2RAD(90));
            kidnap.cartographer_pose2d = kidnap.last_pose2d;
        }
    }

    // <!-- Filter and fuse raw imu data -->
    // <node pkg="imu_filter_madgwick" type="imu_filter_node" name="imu_filter_madgwick" output="screen" respawn="false" >
    // imu_filter_madgwick_.reset(new net::tworker(std::bind(imu_filter_madgwick__imu_filter_madgwick, _1), NULL, NULL, NULL, "imu_filter_madgwick"));

    // <!-- Filter and fuse raw imu data -->
    // <node pkg="tf" type="static_transform_publisher" name="base_footprint_to_imu_link" args="0 0 0 0 0 0  /base_footprint /imu_link  100"/>
    geometry_msgs::TransformStamped msg;
    tf2::Quaternion quat;
    // cartographer does not use_imu as default. If you decide to use imu, set below use_imu = true.
    const bool use_imu = false;
    if (use_imu) {
        msg.transform.translation.x = 0;
        msg.transform.translation.y = 0;
        msg.transform.translation.z = 0;

        quat.setRPY(0, 0, 0);
        msg.transform.rotation.x = quat.x();
        msg.transform.rotation.y = quat.y();
        msg.transform.rotation.z = quat.z();
        msg.transform.rotation.w = quat.w();

        msg.header.frame_id = "/base_footprint";
        msg.child_frame_id = "/imu_link";
        // base_footprint_2_imu_link_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, std::ref(msg)), NULL, NULL, NULL, "base_footprint_2_imu_link"));
        // must not us std::ref(msg)---result null??
        base_footprint_2_imu_link_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, msg), NULL, NULL, NULL, "base_footprint_2_imu_link"));
    }

    // <!-- Publish clbrobot odometry -->
    // <node pkg="clbrobot" name="riki_base_node" type="riki_base_node">
    //    <param name="linear_scale" type="double" value="1.0" />
    // </node>
    // riki_base_node_.reset(new net::tworker(std::bind(rikirobot__riki_base_node, _1), NULL, NULL, NULL, "riki_base_node"));

    // <node pkg="tf" type="static_transform_publisher" name="base_footprint_to_laser" args="0 0 0.098 0 0 0  /base_footprint /laser  100"/>

    // laser_translation_z must not meet with 'if (pz > max_obstacle_height_)' in ObstacleLayer::updateBounds(...).
    // 'pz' is here's laser_translation_z. max_obstacle_height_ is 2.0
    const double laser_translation_z = 0.098;

    msg.transform.translation.x = laser_pose2d.x;
    msg.transform.translation.y = laser_pose2d.y;
    // msg.transform.translation.y = -0.08;
    msg.transform.translation.z = laser_translation_z;

    quat.setRPY(0, 0, laser_pose2d.yaw); // (M_PI, 0, 0)
    // quat.setRPY(0, 0, M_PI); // (M_PI, 0, 0)
    msg.transform.rotation.x = quat.x();
    msg.transform.rotation.y = quat.y();
    msg.transform.rotation.z = quat.z();
    msg.transform.rotation.w = quat.w();

    msg.header.frame_id = "/base_footprint";
    msg.child_frame_id = "/laser";
    // base_footprint_2_imu_link_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, std::ref(msg)), NULL, NULL, "base_footprint_2_imu_link"));
    // must not us std::ref(msg)---result null??
    base_footprint_2_laser_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, msg), NULL, NULL, NULL, "base_footprint_2_laser"));

    if (dcamera_driver_.installed()) {
        msg.transform.translation.x = dcamera_pose2d.x;
        msg.transform.translation.y = dcamera_pose2d.y;
        msg.transform.translation.z = 0;

        msg.header.frame_id = "/base_footprint";
        msg.child_frame_id = frame_id_camera; // "/camera"
        // must not us std::ref(msg)---result null??
        base_footprint_2_camera_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, msg), NULL, NULL, NULL, "base_footprint_2_camera"));
    }

    // <!-- Odom-IMU Extended Kalman Filter-->
    // <node pkg="robot_localization" type="ekf_localization_node" name="ekf_localization"> 
    //      <remap from="odometry/filtered" to="odom" />
    //      <rosparam command="load" file="$(find clbrobot)/param/ekf/robot_localization.yaml" />
    // </node>
    // ekf_localization_node_.reset(new net::tworker(std::bind(robot_localization__ekf_localization_node, _1), NULL, NULL, NULL, "ekf_localization_node"));

    // <node name="rplidarNode"          pkg="rplidar_ros"  type="rplidarNode" output="screen">
    //    <param name="serial_port"         type="string" value="COM4"/>  
    //    <param name="serial_baudrate"     type="int"    value="115200"/>
    //    <param name="frame_id"            type="string" value="laser"/>
    //    <param name="inverted"            type="bool"   value="false"/>
    //    <param name="angle_compensate"    type="bool"   value="true"/>
    // </node>
    if (!use_external_laser()) {
        laser_driver_.start_laser(vars.laser.dev.c_str(), vars.laser.baudrate);
    }

    // cartographer_node_.reset(new net::tworker(std::bind(cartographer_ros__cartographer_node, _1), NULL, NULL, NULL, "cartographer_node"));
    restart_cartographer_node(cartographer_start);

    if (mode_ == mode_buildmap) {
        cartographer_occupancy_grid_node_.reset(new net::tworker(std::bind(cartographer_ros__cartographer_occupancy_grid_node, _1), NULL, NULL, NULL, "cartographer_occupancy_grid_node"));
        // global_rrt_detector_.reset(new net::tworker(std::bind(rrt_exploration__global_rrt_detector, _1), NULL, NULL, NULL, "global_rrt_detector"));
        
        if (buildmap_with_move_base) {
            move_base_node_.reset(new net::tworker(std::bind(move_base__move_base_node, _1, &move_base_constructed_), NULL, NULL, NULL, "move_base_node"));
        }
    } else {
        // const std::string rspfile = game_config::preferences_dir + "/house.rsp";
        nav_msgs::OccupancyGrid* special_map = nullptr;
        calculate_ros_map_4_server(rspfile, false);
        VALIDATE(!map_4_map_server_.data.empty(), null_str);

        if (!map_4_map_server_.data.empty()) {
            // When navigating, make sure get a valid map at the start. 
            // If you want to wait until 'did_map_subscribed', that's too late for a cause module, like 'kestimate'.
            refresh_map_data(map_4_map_server_, false);
            map_server_.reset(new net::tworker(std::bind(map_server__map_server, _1, std::ref(map_4_map_server_)), NULL, NULL, NULL, "map_server"));
        }

        // amcl_.reset(new net::tworker(std::bind(amcl__amcl, _1), NULL, NULL, NULL, "amcl"));
        // throttle_.reset(new net::tworker(std::bind(topic_tools__throttle, _1), NULL, NULL, NULL, "throttle"));
        

        // <node pkg="move_base" type="move_base" respawn="false" name="move_base" output="screen">
	    //    <rosparam file="$(find clbrobot)/param/navigation/tank/costmap_common_params.yaml" command="load" ns="global_costmap" />
	    //    <rosparam file="$(find clbrobot)/param/navigation/tank/costmap_common_params.yaml" command="load" ns="local_costmap" />
        //    <rosparam file="$(find clbrobot)/param/navigation/local_costmap_params.yaml" command="load" />
        //    <rosparam file="$(find clbrobot)/param/navigation/global_costmap_params.yaml" command="load" />
	    //    <rosparam file="$(find clbrobot)/param/navigation/tank/base_local_planner_params.yaml" command="load" />
        //    <rosparam file="$(find clbrobot)/param/navigation/move_base_params.yaml" command="load" />
        // </node>
        move_base_node_.reset(new net::tworker(std::bind(move_base__move_base_node, _1, &move_base_constructed_), NULL, NULL, NULL, "move_base_node"));
    }

    if (dcamera_driver_.installed()) {
        dcamera_driver_.ros_start_dcamera_node();
    }

    subscribe_laserscan_topics();

    if (moveit_driver_.installed()) {
        start_moveit_node(true);
    }

    if (mode_ == mode_navigation) {
        base_driver_.set_last_navigation_rspfile(rspfile);

    } else if (mode == mode_buildmap) {
        base_driver_.set_last_navigation_rspfile(null_str);
    }

    if (rosbag != nposm) {
        if (rosbag == rosbag_record) {
            rosbag_record_result_.clear();
            rosbag_record_.reset(new net::tworker(std::bind(rosbag__record, _1, rosbag_record_input_, std::ref(rosbag_record_result_)), 
                NULL, NULL, NULL, "rosbag_record_node"));

        } else {
            VALIDATE(rosbag == rosbag_play, null_str);
            rosbag_play_result_.clear();
            rosbag_play_.reset(new net::tworker(std::bind(rosbag__play, _1, rosbag_play_input_, std::ref(rosbag_play_result_)), 
                NULL, NULL, NULL, "rosbag_play_node"));
        }
    }
}

void tros_instance::stop_navigation_node()
{
	VALIDATE(mode_ >= 0 && mode_ <= mode_maxros, null_str);

	base_driver_.exit_navigation();

    // imu_filter_madgwick_.reset();
    base_footprint_2_imu_link_.reset();
    // riki_base_node_.reset();
    SDL_Log("%u [stop_node]pre base_footprint_2_laser_.reset()", SDL_GetTicks());
    base_footprint_2_laser_.reset();
    SDL_Log("%u [stop_node]pre base_footprint_2_camera_.reset()", SDL_GetTicks());
    base_footprint_2_camera_.reset();
    // ekf_localization_node_.reset();
    SDL_Log("%u [stop_node]pre stop_laser()", SDL_GetTicks());
    if (!use_external_laser()) {
	    laser_driver_.stop_laser();
    }
    
    // MUST destroy cartographer_occupancy_grid_node before destroy cartographer_node_. see https://www.cswamp.com/post/80
    SDL_Log("%u [stop_node]pre cartographer_occupancy_grid_node_.reset()", SDL_GetTicks());
    cartographer_occupancy_grid_node_.reset();
    SDL_Log("%u [stop_node]pre cartographer_node_.reset()", SDL_GetTicks());
    cartographer_node_.reset();

    SDL_Log("%u [stop_node]pre map_2_laser_.reset()", SDL_GetTicks());
    map_2_laser_.reset();
	SDL_Log("%u [stop_node]pre global_rrt_detector_.reset()", SDL_GetTicks());
	global_rrt_detector_.reset();

    SDL_Log("%u [stop_node]pre map_server_.reset()", SDL_GetTicks());
    map_server_.reset();
    map_4_map_server_.data.clear();
    // amcl_.reset();
    // throttle_.reset();

    SDL_Log("%u [stop_node]pre move_base_node_.reset()", SDL_GetTicks());
    move_base_node_.reset();

    if (dcamera_driver_.installed()) {
        SDL_Log("%u [stop_node]pre orbbec_camera_node_.reset()", SDL_GetTicks());
        dcamera_driver_.ros_stop_dcamera_node();
    }

    SDL_Log("%u [stop_node]pre shutdown_movebase_topics", SDL_GetTicks());
	shutdown_movebase_topics();
	shutdown_laserscan_topics();

	if (moveit_driver_.installed()) {
        stop_moveit_node();
    }

    SDL_Log("%u [stop_node]rosbag_record_.reset()", SDL_GetTicks());
    rosbag_record_.reset();

    SDL_Log("%u [stop_node]rosbag_play_.reset()", SDL_GetTicks());
    rosbag_play_.reset();

	// Clear the StaticCache/TimeCache, avoid outdated tf result error canTransform/lookupTransform.
	clear_tfl();

    // cartographer::rose_slot.did_navigation_start(mode_ == mode_buildmap, false);
	if (kidnap.estimating()) {
		kidnap.clear_estimate(tkidnap::clrreason_stop);
	}
	kidnap.clear_last_ok_estimated_pose2d();

    SDL_Log("{set_mode}stop_navigation_node, nposm --> prev(%i)", mode_);
	mode_ = nposm;
    rosbag_ = nposm;

    navigation_start_ticks_ = 0;

	// when stopping, although move_base has sent move_base/result, Launcher could not receive it.
	goaling_ = false;

	navigation_map_.info.width = 0;
	navigation_map_.info.height = 0;
	navigation_map_.data.clear();
	navigation_goal_.header.frame_id.clear();

    dcamera_point_vsize_ = 0;
    dcamera_2_laser_xy_ = SDL_FPoint{float_nposm, float_nposm};
}

void tros_instance::stop_dwa_local_planner_node()
{
    // MUST destroy cartographer_occupancy_grid_node before destroy cartographer_node_. see https://www.cswamp.com/post/80
    SDL_Log("%u [stop_node]pre cartographer_occupancy_grid_node_.reset()", SDL_GetTicks());
    cartographer_occupancy_grid_node_.reset();
    SDL_Log("%u [stop_node]pre cartographer_node_.reset()", SDL_GetTicks());
    cartographer_node_.reset();

    move_base_node_.reset();
}

void tros_instance::restart_cartographer_node(int reason)
{
    VALIDATE(reason >= 0 && reason < cartographer_count, null_str);

    char reasons[][20] = {"start", "apply_immediately", "ok_esitmated"};
	VALIDATE(sizeof(reasons) / sizeof(reasons[0]) == cartographer_count, null_str);

    SDL_Log("restart_cartographer_node(%s), cartographer_pose2d: %s", reasons[reason], kidnap.cartographer_pose2d.to_string(true).c_str());
 
    cartographer_node_.reset();

    cartographer::rose_slot.did_cartographer_node_will_start();
    cartographer_node_.reset(new net::tworker(std::bind(cartographer_ros__cartographer_node, _1), NULL, NULL, NULL, "cartographer_node"));
}

void tros_instance::wait_cartographer_node_ready()
{
    SDL_Log("%u wait_cartographer_node_ready... node_ready: %s", SDL_GetTicks(), cartographer::rose_slot.node_ready()? "true": "false");
 
    while (!cartographer::rose_slot.node_ready()) {
        SDL_Delay(100);
    }

    SDL_Log("%u wait_cartographer_node_ready finished", SDL_GetTicks());
}

void tros_instance::start_base_or_moveit_serial(bool base, bool buildmap)
{
    tdrivers::tvars vars = drivers_.curvars(true);
    bool is_same = vars.base.dev == vars.moveit.dev;

    tdrivers::tserialp* seiralp = nullptr;
    if (base) {
        VALIDATE(false, null_str);
        if (!is_same || !moveit_driver_.started()) {
            // base_driver_.enter_navigation(buildmap);
        } else {
            SDL_Log("base and moveit use same serial, and moveit has started, so base run fake");
            // base_driver_.set_thread(buildmap, moveit_driver_.get_thread());
        }

    } else {
        if (!is_same || !base_driver_.node_started()) {
            VALIDATE(base_driver_.node_started(), null_str);

            moveit_driver_.start_moveit(vars.moveit.dev, vars.moveit.baudrate);
        } else {
            SDL_Log("base and moveit use same serial, and base has started, so moveit run fake");
            // moveit_driver_.set_thread(base_driver_.get_thread());
            moveit_driver_.set_thread();
        }
    }
}

void tros_instance::start_moveit_node(bool for_navigation)
{
    VALIDATE(is_moveit_model_valid(), null_str);
    VALIDATE(started(), null_str);

    VALIDATE(!moveit_node_started_, null_str);
    moveit_node_started_ = true;

    tdrivers::tvars vars = drivers_.curvars(true);
    VALIDATE(vars.moveit.valid(true), null_str);

    if (!light_moveit_) {
        move_group_.reset(new net::tworker(std::bind(moveit_ros_move_group__move_group, _1), NULL, NULL, NULL, "move_group"));
    }
    
    VALIDATE(joint_state_publisher_.get() == nullptr, null_str);

    start_base_or_moveit_serial(false, false);

    trose_event* e = rose_create_event(false, false);
	joint_state_publisher_.reset(new net::tworker(std::bind(joint_state_publisher__joint_state_publisher, _1, RobotState_of_moveit_model(), std::ref(*e)), NULL, NULL, NULL, "robot_state_publisher"));

    robot_state_publisher_.reset(new net::tworker(std::bind(robot_state_publisher__robot_state_publisher, _1), NULL, NULL, NULL, "robot_state_publisher"));

    e->Wait(trose_event::kForever);
    delete e;
    if (for_navigation) {
        // joint_stater_node must be work before below statement.
        do_light_group_values_target(aplt::tmoveit_slot::state_navigation);
    }

    geometry_msgs::TransformStamped msg;
    tf2::Quaternion quat;
    {
        VALIDATE(base_footprint_2_base_link_.get() == nullptr, null_str);

        // quat.setRPY(-M_PI / 2, 0.0, -M_PI / 2);
        quat.setRPY(0.0, 0.0, 0.0);
        msg.transform.rotation.x = quat.x();
        msg.transform.rotation.y = quat.y();
        msg.transform.rotation.z = quat.z();
        msg.transform.rotation.w = quat.w();

        msg.transform.translation.x = 0.5;
        msg.transform.translation.y = -0.2;
        msg.transform.translation.z = 0;

        msg.header.frame_id = for_navigation? "/base_footprint": "/map";
        msg.child_frame_id = "/base_link";
        // must not us std::ref(msg)---result null??
        base_footprint_2_base_link_.reset(new net::tworker(std::bind(tf__static_transform_publisher, _1, msg), NULL, NULL, NULL, "base_footprint_2_base_link"));
    }
}

void tros_instance::report_navigation_error(bool caller_is_fg_aplt, const std::string& msg)
{
    if (caller_is_fg_aplt) {
        gui2::show_message(null_str, msg);
    } else {
        instance->bg_task().add_log2(time(nullptr), msg, 0, false);
    }
}


std::string position_name_from_uuid(const tros_map& curmap, const std::string& uuid)
{
    if (uuid == charge_pos_uuid) {
        return _("Charge");
    }

    if (curmap.positions.count(uuid) == 0) {
        return null_str;
    }
    return curmap.positions.find(uuid)->second.name;
}

bool tros_instance::check_navigation_env(const tdrivers::tvars& vars, const aplt::tapplet* aplt, bool caller_is_fg_aplt_or_app)
{
    utils::string_map symbols;
    std::string err_msg;

    if (vars.base.path.empty() || vars.laser.path.empty()) {
        err_msg = _("Start navigation fail. Base and laser's serial path must not be empty.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }

    if (!vars.base.valid(true) || !vars.laser.valid(true)) {
        err_msg = _("Start navigation fail. Base and laser's serial baudrate isn't valid.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }

    if (vars.base.dev == vars.laser.dev) {
        err_msg = _("Start navigation fail. Base and laser's serial must not be same.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }

    if (aplt != nullptr) {
        if (aplt->permissions.count(aplt::per_navigation) == 0) {
            symbols["per"] = aplt::sys_permissions.find(aplt::per_navigation)->second.name;
		    err_msg = vgettext2("Start navigation fail. The applet hasn't $per permission.", symbols);
            // gui2::show_message(null_str, err_msg);
            report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
            return false;
        }

        if (!curmap_.valid()) {
            symbols["settings"] = _("icon^Settings");
            err_msg = vgettext2("Start navigation fail. Enter '$settings', Set current map.", symbols);
            // gui2::show_message(null_str, err_msg);
            report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
            return false;
        }
    }

    if (moveit_driver_.installed()) {
        if (!get_imu_rpy().valid) {
            err_msg = _("Start navigation fail. No available IMU.");
            report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
            return false;
        }

        if (!dcamera_driver_.installed()) {
            err_msg = _("Start navigation fail. A moveit is used, but there is no depth camera.");
            report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
		    return false;
        }

        VALIDATE(vars.moveit.driver_main != nullptr, null_str);
        if (vars.moveit.path.empty() || vars.moveit.baudrate == nposm) {
            err_msg = _("Start navigation fail. A moveit is used, but serial path or baudrate isn't valid.");
            report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
            return false;
        }
	}

    return true;
}

bool tros_instance::check_charge_env(bool caller_is_fg_aplt_or_app, aplt::ttask_pair* task_pair_result, int* width_result)
{
    VALIDATE(curmap_.positions.count(charge_pos_uuid) != 0, null_str);
    const tmap_position& position = curmap_.positions.find(charge_pos_uuid)->second;

    std::string err_msg;
    if (is_float_nposm(position.x) || is_float_nposm(position.y)) {
        err_msg = _("Start charge fail. Coordiate of charge position isn't valid.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }
    if (is_float_nposm(position.theta)) {
        err_msg = _("Start charge fail. Must set angle of charge position.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }

    const std::string task_id2 = preferences::charge_task_id2();
	aplt::ttask_pair pair = aplt::split_aplt_task_id2(instance->applets(), task_id2, false);
	if (pair.aplt == nullptr || pair.task == nullptr || pair.task->type != aplt::task_nonblock) {
		err_msg = _("Start charge fail. Please set a valid charge applet first.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
	}

    int width_mm = preferences::charge_width_mm();
    if (!is_valid_charge_width(width_mm)) {
        err_msg = _("Start charge fail. Please set a valid charging pile width first.");
        report_navigation_error(caller_is_fg_aplt_or_app, err_msg);
        return false;
    }

    if (task_pair_result != nullptr) {
        task_pair_result->aplt = pair.aplt;
        task_pair_result->task = pair.task;
    }

    if (width_result != nullptr) {
        *width_result = width_mm;
    }

    return true;
}

tros_instance::ttask* tros_instance::moveto_guid(const aplt::tapplet& aplt, const std::string& position_uuid, const fn_navigation_bh& luafunc)
{
    aplt::tbg_task::trunning::tverifier running_verifier(instance->bg_task());

    if (task_.get() != nullptr) {
        // there maybe has task_, destroy it.
        makesure_cancel_goal(true);
        VALIDATE(task_.get() == nullptr, null_str);
    }

	VALIDATE(mode_ == nposm, null_str);
    VALIDATE(task_.get() == nullptr, null_str);
    bool caller_is_fg_aplt = &aplt == instance->fg_aplt();
    
    const tdrivers::tvars vars = drivers_.curvars(true);

    utils::string_map symbols;
    std::string err_msg;

    if (!check_navigation_env(vars, &aplt, caller_is_fg_aplt)) {
        return nullptr;
    }

    // tpose2d position2d = curmap_.charge;
    // if (position_uuid != charge_pos_uuid) {
        std::string uuid = position_uuid;
        if (!uuid.empty() && !utils::is_uuid(position_uuid, true)) {
            VALIDATE(false, null_str);

        }
        if (curmap_.positions.count(uuid) == 0) {
            return nullptr;
        }
        const tmap_position& position = curmap_.positions.find(uuid)->second;
        tpose2d position2d = tpose2d(position.x, position.y, position.theta);
    // }

    if (is_float_nposm(position2d.x) || is_float_nposm(position2d.y)) {
        err_msg = _("Start navigation fail. Coordiate of position isn't valid.");
        report_navigation_error(caller_is_fg_aplt, err_msg);
        return nullptr;
    }
    
    // because tros_base_node is started always, must be started.
    VALIDATE(started_, null_str);

    // if (!initialized_) {
        // want below is_moveit_model_valid() is true, require at least initialized.
	//	initialize();
	// }

    ttask* task_ptr = new ttask(aplt, curmap_.rspfile, position_uuid, navigation_bh_luafunc, luafunc);
	task_ptr->x = position2d.x; // 1m
	task_ptr->y = position2d.y; // -2m
	// task_ptr->theta = -1 * DEG2RAD(30);
    task_ptr->theta = position2d.yaw;
    register_slot(*task_ptr);

    if (!moveto_guid_bh(task_ptr)) {
        symbols["driver"] = game_config::driver_names.find(apltsotype_moveit)->second;
        err_msg = vgettext2("Start navigation fail. MoveBase node is not working properly.", symbols);
        report_navigation_error(caller_is_fg_aplt, err_msg);
        return nullptr;
    }
    return task_ptr;
}

bool tros_instance::moveto_guid_bh(tros_instance::ttask* task_ptr)
{
/*
    // {leagor-stop_timing_lock}
    // aplt::tbg_task::tdisable_stop_timing_lock lock(instance->bg_task());
*/
    VALIDATE(task_.get() == nullptr, null_str);
    task_.reset(task_ptr);

    tdrivers::tvars vars = drivers_.curvars(true);
	gui2::tprogress_default_slot slot(std::bind(&tros_instance::moveto_function, this, _1, std::ref(*task_ptr), std::ref(vars)));
	bool ret = gui2::run_with_progress(slot, null_str, _("Starting navigation"), 1);
    if (ret) {
        // MUST place task_.reset(task_ptr) after moveto_function.
        // moveto_function will call tpropress_::show_slice, if has task, it will receive ros-topic.
        
        // task_.reset(task_ptr);
    } else {
        // deregister_slot(task_ptr);
        // delete task_ptr;

        deregister_slot(*task_.get());
        task_.reset();
    }
    return ret;
}

void tros_instance::did_move_base_result(const move_base_msgs::MoveBaseActionResult& result)
{
	// result.status.status == actionlib_msgs::GoalStatus::PREEMPTED 
	//   && !result.status.text.empty() ==> ::acceptNewGoal(). "This goal was canceled because another goal was recieved by the simple action server"
	//                    see <include/actionlib/server/simple_action_server_imp.h>
	//   && result.status.text.empty() ==> cancel_goal. see move_base node's MoveBase::executeCb.
	bool is_canceled = result.status.status == actionlib_msgs::GoalStatus::PREEMPTED && result.status.text.empty();
	bool is_preempted = result.status.status == actionlib_msgs::GoalStatus::PREEMPTED && !result.status.text.empty();
	if (!is_preempted) {
		goaling_ = false;
        if (kidnap.estimating()) {
		    kidnap.clear_estimate(tkidnap::clrreason_result);
	    }
        kidnap.clear_last_ok_estimated_pose2d();
	}
	
	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.did_move_base_result(result, task_.get());
	}

    bool ok = result.status.status == actionlib_msgs::GoalStatus::SUCCEEDED;
    if (ok) {
        robot_imu_.has_result_ok = true;
    }

    bool keep_task = false;
	if (task_.get() != nullptr) {
        ttask& task = *task_.get();
        aplt::tbg_task& bg_task = instance->bg_task();
		VALIDATE(instance->fg_aplt() != nullptr || bg_task.is_ing(), null_str);

        if (bg_task.is_ing()) {
            if (bg_task.bg_task2().in_task_cpp()) {
		        if (speech_driver_.installed()) {
			        speech_driver_.set_allow_short_voice(true);
		        }
	        }
        }

        bool caller_is_fg_aplt = &task.aplt == instance->fg_aplt();
		if (!is_canceled && !is_preempted && task.bh == navigation_bh_luafunc) {
            if (!caller_is_fg_aplt) {
                VALIDATE(bg_task.is_ing(), null_str);
                const aplt::tbg_task::tbase_bg_task2& sys_task = bg_task.bg_task2();
                bool nav2th = bg_task.single_task_nav2th_is_started();

                VALIDATE(sys_task.aplt_task != nullptr, null_str);
				const aplt::taplt_task& task = *sys_task.aplt_task;
                std::string position_uuid = nav2th? task.position2: task.position1;
			    
                utils::string_map symbols;
                symbols["go"] = nav2th? _("Be back"): _("Go");
                symbols["position"] = position_name_from_uuid(curmap_, position_uuid);
                symbols["result"] = ok? _("Success"): _("Fail");
                const std::string msg = vgettext2("Navigation finished($go|$position): $result", symbols);
		        bg_task.add_log2(time(nullptr), msg, 0, false);

                if (bg_task.bg_task2().aplt_task != nullptr) {
                    // here task maybe type_aplt_task, also be other, for example type_move_to
                    bg_task.mutable_bg_task2().aplt_task_navigation_stopped(nav2th, ok);
                }
            }
			keep_task = task.luafunc(ok);
		}
        if (caller_is_fg_aplt && task.bh == navigation_bh_luafunc) {
            // Perhaps, in the future, caller_is_aplt(true) should also be able to 'keep_task == false', 
            // but set it like this for now.
            VALIDATE(!keep_task, null_str);
        }
		if (!keep_task) {
            SDL_Log("did_move_base_result(3.1), post luafunc and keep_task is false, so call erase_task()");
			erase_task();

		} else {
            SDL_Log("did_move_base_result(3.2), post luafunc and keep_task is true, don't call erase_task()");
        }

	} else {
        SDL_Log("did_move_base_result(3.3), task_.get() == nullptr");
    }
}