#define GETTEXT_DOMAIN "launcher-lib"

#include "moveit_calculator.hpp"

#include "gettext.hpp"
#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/message.hpp"

#include <opencv2/opencv.hpp>
#include "qr_code.hpp"
#include "dcamera_driver.hpp"

#include <base_local_planner/line_iterator.h>
#include <tf2/utils.h>

#include "base_instance.hpp"

using namespace std::placeholders;


namespace tf2 {

extern void doTransform_position(const tf2::Transform& t, const SDL_DPoint3& p_in, SDL_DPoint3& p_out);
extern void doTransform_position(const geometry_msgs::Transform& transform, const SDL_DPoint3& p_in, SDL_DPoint3& p_out);

}

double calculate_ik_b_f_radius(double z)
{
	double a = -36.015877;
	double b = 2.618202;
	double c = -1.772948;

	return exp(a * z * z + b * z + c);
}

tmoveit_calculator::tmoveit_calculator(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdcamera_driver& dcamera_driver, tros_instance& ros_instance, tdcamera_slot_impl& dcamera_slot_impl)
	: applets_(applets)
	, dcamera_driver_(dcamera_driver)
	, ros_instance_(ros_instance)
	, dcamera_slot_impl_(dcamera_slot_impl)
	, width_(nposm)
	, height_(nposm)
	, short_dbg_contours_png_("dbg_contours-1nd.png")
	, short_dbg_RP_surf_png_("dbg_RP_surf.png")
	, short_dbg_RP_depth_data_dat_("dbg_RP_depth_data.dat")
	, dbg_RP_(false)
	, coor_x_data(nullptr)
	, coor_x_data_size_(0)
	, coor_y_data(nullptr)
	, coor_y_data_size_(0)
	// task that execute by DoWork
	, task_finished_(false)
	, task_event_(false, false)
	, operate_()
	, next_RP_sample_index_(0)
	, next_targrt_sample_index_(0)
	// xyz="-0.027 -0.0222 0.0432"
	// SDL_DPoint3 joint5_xyz{-0.027, -0.0222, 0.0432};
	// rotate RPY(0, 90, 0) from joint5.xyz
	// const SDL_DPoint3 PRP_offset{0.0432, 0, 0.027};
	, PRP_offset_(SDL_DPoint3{float_nposm, float_nposm, float_nposm})
	, PRP_2offset_(SDL_DPoint3{float_nposm, float_nposm, float_nposm})
	, RGB_diff_y_(0.011) // between dcamera's RGB and center. 0.011/0.006/0.016
	// delta_twist: [   0.0505859,       0.002,   0.0466934, 5.17015e-17,    -2.61759, 2.96563e-16] q_out: [   -0.489953    0.248543    0.487098]
	, ik_bound_(0.005) // 0.006
	, near_threshold_(5000) // 5 second
	, move_robot_threshold_(2500) // 2.5 second
	, second_dcpitch_threshold_(4000)
	, same_initial_fk_x_thresould_(0.01) // 1cm
	, no_rotate_robot_range_(SDL_DRange{DEG2RAD(-30), DEG2RAD(3)}) // (-20, 3)
	, no_rotate_joint1_range_(SDL_DRange{DEG2RAD(-2), DEG2RAD(2)})
	, joint1_0degree_(ros_instance.moveit_driver().installed()? ros_instance.get_joint1_0degree(): float_nposm) // -1.57/-1.43
	, joint1_axis_negative_(true)
	, joint1_position_(nullptr)
	, joint1_model_(nullptr)
	, PRP_joint_model_(nullptr)
	, claw_(ros_instance_.rsp_of_moveit_model(false))
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(near_threshold_ > move_robot_threshold_, null_str);

	VALIDATE(no_rotate_robot_range_.min < 0 && no_rotate_robot_range_.max > 0, null_str);
	VALIDATE(no_rotate_joint1_range_.min < 0 && no_rotate_joint1_range_.max > 0, null_str);
	VALIDATE(no_rotate_robot_range_.min < no_rotate_joint1_range_.min, null_str);

	// 640x480
	intrinsics_.cx = 325.33935546875000;
	intrinsics_.cy = 240.74371337890625;
	intrinsics_.fx = 519.22186279296875;
	intrinsics_.fy = 519.15301513671875;

	intrinsics_.valid = false;

	task_thread_.reset(new net::tworker(std::bind(&tmoveit_calculator::DoWork, this, _1), std::bind(&tmoveit_calculator::OnWorkStart, this), 
		std::bind(&tmoveit_calculator::OnWorkDone, this), std::bind(&tmoveit_calculator::OnTriggerExit, this), "DepthCalculatorThread"));

	int font_size = font::SIZE_DEFAULT;
	SDL_Color color = font::BIGMAP_COLOR;
	state_texs_.insert(std::make_pair(state_distance, create_texture_from_text("state_distance", font_size, color)));
	state_texs_.insert(std::make_pair(state_ik, create_texture_from_text("state_ik", font_size, color)));
	state_texs_.insert(std::make_pair(state_align, create_texture_from_text("state_align", font_size, color)));
	state_texs_.insert(std::make_pair(state_claw, create_texture_from_text("state_claw", font_size, color)));
	state_texs_.insert(std::make_pair(state_grasp, create_texture_from_text("state_grasp", font_size, color)));
	VALIDATE(state_texs_.size() == state_count, null_str);

	SDL_Color scene_color = font::GOOD_COLOR;
	std::pair<std::map<int, tscene_desc>::iterator, bool> ins = scenes_.insert(std::make_pair(scene_normal, tscene_desc(scene_normal, "normal")));
	// ins.first->second.tex = nullptr;
	ins = scenes_.insert(std::make_pair(scene_always_distance, tscene_desc(scene_always_distance, _("Always distance"))));
	ins.first->second.tex = create_texture_from_text("(Always distance)", font_size, scene_color);

	ins = scenes_.insert(std::make_pair(scene_always_initial, tscene_desc(scene_always_initial, _("Always initial"))));
	ins.first->second.tex = create_texture_from_text("(Always initial)", font_size, scene_color);

	ins = scenes_.insert(std::make_pair(scene_always_near, tscene_desc(scene_always_near, _("Always near"))));
	ins.first->second.tex = create_texture_from_text("(Always near)", font_size, scene_color);

	ins = scenes_.insert(std::make_pair(scene_calibrate_near, tscene_desc(scene_calibrate_near, _("Calibrate near"))));
	ins.first->second.tex = create_texture_from_text("(Calibrate near)", font_size, scene_color);
	VALIDATE(scenes_.size() == scene_count, null_str);

	if (ros_instance_.moveit_driver().installed()) {
		joint1_position_ = ros_instance_.joint_position_from_name(joint_name_joint1);
		joint1_model_ = &ros_instance_.joint_model_from_name(joint_name_joint1);
		PRP_joint_model_ = &ros_instance_.joint_model_from_name(joint_name_PRP);

		// PRP_offset_(SDL_DPoint3{0.0432, 0, 0.027})
		// PRP_2offset_(SDL_DPoint3{0.02, 0, 0.003})
		const trsp_moveit2& rsp_moveit = ros_instance_.rsp_of_moveit_model();
		PRP_offset_ = rsp_moveit.header.PRP_offset;
		PRP_2offset_ = rsp_moveit.header.PRP_2offset;

		VALIDATE(KDL::Equal(PRP_offset_.x, 0.0432) && KDL::Equal(PRP_offset_.y, 0) && KDL::Equal(PRP_offset_.z, 0.027), null_str);
		VALIDATE(KDL::Equal(PRP_2offset_.x, 0.02) && KDL::Equal(PRP_2offset_.y, 0) && KDL::Equal(PRP_2offset_.z, 0.003), null_str);


		if (ros_instance_.has_task()) {
			// When running moveit in bg-task, enter 'dcamera', a reentrancy occurs in this 'dcamera'.
			// now only construct/destruct.
			// aplt::tapplet* aplt = aplt::mutable_aplt_from_id2(applets_, aplt::src_studio, "aplt.leagor.basic");
			// aplt_task_ = ros_instance_.drivers().create_task_api(*aplt);
		}

	} else {
		VALIDATE(!ros_instance_.has_task(), null_str);
	}
}

tmoveit_calculator::~tmoveit_calculator()
{
	// must call stop_operate() before task_thread_.rest().
	// if is operating, stop_operate() will 'operate_.stopping = true', it can exit task_thread_.
	if (operating()) {
		stop_operate();
	}
	task_thread_.reset();

	if (coor_x_data != nullptr) {
		free(coor_x_data);
	}

	if (coor_y_data != nullptr) {
		free(coor_y_data);
	}
}

void tmoveit_calculator::resize_coor_data(bool is_x, int size)
{
	size = posix_align_ceil(size, 64);
    VALIDATE(size >= 0, null_str);

	int& coor_data_size = is_x? coor_x_data_size_: coor_y_data_size_;
	int16_t** coor_data_pp = is_x? &coor_x_data: &coor_y_data;

	if (size > coor_data_size) {
	    int16_t* tmp = (int16_t*)malloc(size * sizeof(int16_t));
		// memset(tmp, 0, size * sizeof(uint16_t));
	    if ((*coor_data_pp) != nullptr) {
			free(*coor_data_pp);
		}
		*coor_data_pp = tmp;
		coor_data_size = size;
    }
}

void tmoveit_calculator::did_started(const tdcintrinsics_C& intrinsics, int width, int height)
{
	VALIDATE(!intrinsics_.valid, null_str);
	VALIDATE(width_ == nposm && height_ == nposm, null_str);

	VALIDATE(intrinsics.valid, null_str);

	memset(&intrinsics_, 0, sizeof(tdcintrinsics_C));
	intrinsics_ = intrinsics;

	width_ = width;
	height_ = height;

	resize_coor_data(true, width * height);
	resize_coor_data(false, width * height);
}

void tmoveit_calculator::did_stopped()
{
	if (intrinsics_.valid) {
		VALIDATE(intrinsics_.valid, null_str);
		VALIDATE(width_ > 0 && height_ > 0, null_str);

		intrinsics_.valid = false;
		width_ = nposm;
		height_ = nposm;

	} else {
		// maybe not receive any frame.
		VALIDATE(width_ == nposm && height_ == nposm, null_str);
	}
}

void dcamera_xy_from_depth(float fdx, float fdy, float u0, float v0, int depth, const SDL_Range& depth_range, float depth_scale, int16_t& x, int16_t& y)
{
    if (depth < depth_range.min || depth > depth_range.max) {
        x = Y16_NO_DEPTH;
        y = Y16_NO_DEPTH;
        return;
    }
    float xf = (x - u0) * fdx;
    float yf = (y - v0) * fdy;
    float zf = depth * depth_scale;
    // *iter_x = zf * xf / 1000.0;
    // *iter_y = zf * yf / 1000.0;
    // *iter_z = zf / 1000.0;

    x = (int16_t)(zf * xf);
    y = (int16_t)(zf * yf);
}

std::vector<SDL_Point3> tmoveit_calculator::calculate(const int16_t* depth_data, double depth_scale, const std::vector<SDL_2Point>& color_points)
{
    VALIDATE(is_started(), null_str);

	VALIDATE(depth_data != nullptr, null_str);

	const int width = width_;
	const int height = height_;

    const tdcintrinsics_C& intrinsics = intrinsics_;
	VALIDATE(intrinsics.valid, null_str);

    float fdx = 1 / intrinsics.fx;
    float fdy = 1 / intrinsics.fy;
    float u0 = intrinsics.cx;
    float v0 = intrinsics.cy;

    size_t valid_count = 0;
    // const float MIN_DISTANCE = 20.0;
    const float MIN_DISTANCE = 5.0;
    const float MAX_DISTANCE = 10000.0;
    
    const int min_depth = (int)(MIN_DISTANCE / depth_scale);
    const int max_depth = (int)(MAX_DISTANCE / depth_scale);
    for (int y = 0; y < height; y++) {
        const int y_start = y * width;
        for (int x = 0; x < width; x++) {
            int index = y_start + x;
            // if (depth_data[y * width + x] < min_depth || depth_data[y * width + x] > max_depth) {
			const int depth = depth_data[index];
            if (depth < min_depth || depth > max_depth) {
                *(coor_x_data + index) = Y16_NO_DEPTH;
                *(coor_y_data + index) = Y16_NO_DEPTH;
                continue;
            }
            float xf = (x - u0) * fdx;
            float yf = (y - v0) * fdy;
            float zf = depth_data[y * width + x] * depth_scale;
            // *iter_x = zf * xf / 1000.0;
            // *iter_y = zf * yf / 1000.0;
            // *iter_z = zf / 1000.0;
            // ++ iter_x, ++iter_y, ++iter_z;
            *(coor_x_data + index) = (uint16_t)(zf * xf);
            *(coor_y_data + index) = (uint16_t)(zf * yf);

            valid_count++;
        }
    }

	SDL_Range depth_range{min_depth, max_depth};
	const int flat_depth_threshold = 6; // 6mm, 8mm

	std::vector<SDL_Point3> result;
	for (std::vector<SDL_2Point>::const_iterator it = color_points.begin(); it != color_points.end(); ++ it) {
		const SDL_2Point& pair = *it;
		VALIDATE(pair.x1 >= 0 && pair.x1 < width, null_str);
		VALIDATE(pair.y1 >= 0 && pair.y1 < height, null_str);

		SDL_Point src{pair.x1, pair.y1};
		if (pair.x2 != nposm) {
			VALIDATE(pair.x2 >= 0 && pair.x2 < width, null_str);
			VALIDATE(pair.y2 >= 0 && pair.y2 < height, null_str);
			VALIDATE(pair.x1 != pair.x2 || pair.y1 != pair.y2, null_str);

			const bool verbose = false;
			const int max_search_points_640 = 6;
			const int max_search_points = max_search_points_640 * width / 640;
			const int min_valid_search_points_640 = 3;
			const int min_valid_search_points = min_valid_search_points_640 * width / 640;

			int search_points = 0;
			int valid_search_points = 0;
			int last_depth = Y16_NO_DEPTH;
			int index;
			SDL_Point waypoint;
			for (base_local_planner::LineIterator line(pair.x1, pair.y1, pair.x2, pair.y2); 
				line.isValid() && search_points < max_search_points; line.advance(), search_points ++) {
				waypoint.x = line.getX();
				waypoint.y = line.getY();

				index = waypoint.x + waypoint.y * width;
				int depth = depth_data[index];
				if (verbose) {
					SDL_Log("{calculate}valid_search/search[%i/%i]xy(%i, %i) ->depth(%i) last_depth: %i min_valid_search_points_640: %i", 
						valid_search_points, search_points, waypoint.x, waypoint.y, depth, last_depth, min_valid_search_points_640);
				}
				if (depth < min_depth || depth > max_depth) {
					continue;
				}

				if (last_depth != Y16_NO_DEPTH) {
					int diff = depth - last_depth;
					if (posix_abs(diff) > flat_depth_threshold) {
						SDL_Log("%u {calculate}depth(%i) - last_depth(%i) > flat_depth_threshold(%i) valid_search/search: %i/%i", 
							SDL_GetTicks(), depth, last_depth, flat_depth_threshold, valid_search_points, search_points);
						src = waypoint;
					}

				} else if (waypoint.x != src.x || waypoint.y != src.y) {
					VALIDATE(valid_search_points == 0, null_str);
					if (verbose) {
						SDL_Log("{calculate}src(%i, %i) is no depth, modify to waypoint(%i, %i), valid_search/search: %i/%i", 
							src.x, src.y, waypoint.x, waypoint.y, valid_search_points, search_points);
					}
					src = waypoint;
				}

				last_depth = depth;
				valid_search_points ++;
			}

			if (valid_search_points < min_valid_search_points_640) {
				src.x = nposm;
			}

		} else {
			VALIDATE(pair.y2 == nposm, null_str);
			int index = src.x + src.y * width;
			int depth = depth_data[index];
			if (depth < min_depth || depth > max_depth) {
				// Search for 8 cells around, and at least 2 has depth, take 2th.
				int first_depth = Y16_NO_DEPTH;
				bool found = false;
				const int s = 1;
				for (int y_diff = -s; y_diff <= s && !found; y_diff ++){
					for (int x_diff = -s; x_diff <= s; x_diff ++){
						if (x_diff == 0 && y_diff == 0) {
							continue;
						}
						const int new_x = src.x + x_diff;
						const int new_y = src.y + y_diff;
						if (new_x < 0 || new_x >= width) {
							continue;
						}
						if (new_y < 0 || new_y >= height) {
							continue;
						}

						index = new_x + width * new_y;
						depth = depth_data[index];
						if (depth < min_depth || depth > max_depth) {
							continue;
						}

						if (first_depth == Y16_NO_DEPTH) {
							first_depth = depth;

						} else {
							int diff = depth - first_depth;
							if (posix_abs(diff) <= flat_depth_threshold) {
								src.x = new_x;
								src.y = new_y;
								found = true;
								break;
							}
						}
					}
				}
			}
		}

		result.push_back(SDL_Point3());
		SDL_Point3& dst = result.back();

		if (src.x != nposm) {
			int index = src.x + src.y * width;
			dst.x = coor_x_data[index];
			dst.y = coor_y_data[index];
			dst.z = depth_data[index];
/*
			{
				int16_t x1;
				int16_t y1;
				dcamera_xy_from_depth(fdx, fdy, u0, v0, dst.z, depth_range, depth_scale, x1, y1);
				VALIDATE(dst.x == x1 && dst.y == y1, null_str);
			}
*/
		} else {
			// dst.x = 0;
			// dst.y = 0;
			// dst.z = Y16_NO_DEPTH;

			dst.x = Y16_NO_DEPTH;
			dst.y = Y16_NO_DEPTH;
			dst.z = 0;
		}
	}

    return result;
}

void tmoveit_calculator::save_dbg_d2c(const cv::Mat& argb_mat, const int16_t* depth_data, double depth_scale, bool is_RP) const
{
	VALIDATE(is_started(), null_str);

	const int width = argb_mat.cols;
    const int height = argb_mat.rows;

	imwrite(argb_mat, is_RP? short_dbg_RP_surf_png_: short_dbg_normal_surf_png);

	const std::string depth_file = game_config::preferences_dir + "/" + (is_RP? short_dbg_RP_depth_data_dat_: short_dbg_normal_depth_data_dat);
	save_y16_depth_file(intrinsics_, width, height, depth_data, depth_scale, float_nposm, depth_file);
}

bool tmoveit_calculator::find_reference_point(const cv::Mat& surf, const int16_t* depth_data, double depth_scale, SDL_Rect* result)
{
	VALIDATE(is_started(), null_str);

	VALIDATE(!surf.empty(), null_str);

	const int width = surf.cols;
    const int height = surf.rows;
	VALIDATE(width == width_ && height == height_, null_str);
	VALIDATE((surf.cols % 8) == 0 && (surf.rows % 8) == 0, null_str);

	const bool debuging = dbg_RP_;

	if (debuging)  {
		imwrite(surf.u == nullptr? surf.clone(): surf, "1-raw.png");
    }

	int roi_x = width / 4;
	if (width >= 1280) {
		roi_x += width / 8;
	}
	int roi_y = height * 3 / 4; // 7/8, when 90, too small

	int roi_width = width - roi_x;
	if (width >= 1280) {
		roi_width -= width / 4;
	}
	int roi_height = height - roi_y;
	cv::Mat roi_mat_argb(surf, cv::Rect(roi_x, roi_y, roi_width, roi_height));

	cv::Mat gray;
	{
		// tsurface_2_mat_lock lock(surf);
		// cv::cvtColor(lock.mat, gray, cv::COLOR_BGRA2GRAY);
		cv::cvtColor(roi_mat_argb, gray, cv::COLOR_BGRA2GRAY);
	}

	if (debuging) {
		// surface surf = u8_data_2_cell_value_surf((const uint8_t*)gray.data, 4, 4, gray.cols, gray.rows, nposm, nullptr, nullptr);
        // imwrite(surf, "1-cell_value.png");

        imwrite_gray(gray, "1-gray.png");
    }

    // const int cells = width * height;

    // cv::Mat threshold_mat = cv::Mat(height, width, CV_8UC1);
    // uint8_t* threshold_mat_data = threshold_mat.ptr<uint8_t>(0);
	// memcpy(threshold_mat_data, gray.data, width * height);

	cv::Mat roi_mat;

    const uint8_t OBSTACLE_VALUE = 255;

    double threshold_occupied = NEAR_OBSTACLE; // think >= 60 as obstacle
    // cv::threshold(gray, roi_mat, threshold_occupied - 1, OBSTACLE_VALUE, cv::THRESH_BINARY); // cv::THRESH_BINARY_INV
	// if use cv::THRESH_OTSU, will ignore @thresh.
	cv::threshold(gray, roi_mat, 0, OBSTACLE_VALUE, cv::THRESH_BINARY | cv::THRESH_OTSU);
	// cv::threshold(gray, roi_mat, 0, OBSTACLE_VALUE, cv::THRESH_BINARY);

	if (debuging) {
        imwrite_gray(roi_mat, "1-roi.png");
    }

	cv::Mat erode_mat;
	int radius = width >= 1280? 2: 1; // must not use 3
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
	cv::erode(roi_mat, erode_mat, element);
    if (debuging) {
		imwrite_gray(erode_mat, "1-gray-erode-1st.png");
    }

    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(erode_mat, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    const int max_support_contours = 400; // 75 is too small
    if ((int)contours.size() > max_support_contours) {
		SDL_Log("find reference point fail. Too many contours(%i), up to %i are supported", (int)contours.size(), max_support_contours);
        return false;

    } else if (contours.size() < 2) {
		SDL_Log("find reference point fail. There are '< 2' contour(%i) in the image", (int)contours.size());
        return false;
    }

    cv::Mat o = cv::Mat::zeros(erode_mat.rows, erode_mat.cols, CV_8UC3);

    std::set<int> hit_indexs;
	std::vector<SDL_Rect> left_hit_rects;
	std::vector<SDL_Rect> right_hit_rects;
	std::map<int, cv::Rect> all_rects;

    // hierarchy[.][0]: index of next contour

	SDL_Rect mid_sdl_rect{erode_mat.cols / 2 - 16, 0, 32, erode_mat.rows};
    for (int index = 0; index >= 0; index = hierarchy[index][0]) {
        VALIDATE(index >= 0 && index < (int)contours.size(), null_str);
		cv::Rect rect = cv::boundingRect(contours[index]);

		if (debuging) {
		    // cv::Scalar color(rand() % 255, rand() % 255, rand() % 255);
			cv::Scalar color(rand() % 128, rand() % 128, rand() % 128);
            cv::drawContours(o, contours, index, color, cv::FILLED, 8, hierarchy);

			all_rects.insert(std::make_pair(index, rect));
        }

		if (rect.height >= roi_mat.rows / 2) {
			continue;
		}
		if (rect.width > roi_mat.cols / 2) {
			continue;
		}
		if (rect.y == 0) {
			continue;
		}
		if (width == 1280) {
			const int min_contour_width = 16;
			if (rect.width < min_contour_width) {
				continue;
			}
		}

		SDL_Point center{roi_x + rect.x + rect.width / 2, roi_y + rect.y + rect.height / 2};

		std::vector<SDL_2Point> color_points;
		color_points.clear();
		color_points.push_back(SDL_2Point{center.x, center.y, nposm, nposm});

		std::vector<SDL_Point3> result = calculate(depth_data, depth_scale, color_points);
		const SDL_Point3& xyz = result[0];
		// l(-17, 83, 188), r(47, 78, 187)
		// l(-14, 75, 188), r(50, 70, 186)

		if (debuging) {
			SDL_Log("#%i center: (%i, %i) --> xyz: (%i, %i, %i)", index, center.x, center.y, xyz.x, xyz.y, xyz.z);
		}

		bool left = true;
		if (xyz.x >= -24 && xyz.x <= -11) {
			
		} else if (xyz.x >= 40 && xyz.x <= 55) {
			left = false;

		} else {
			continue;
		}

		if (xyz.y < 57 || xyz.y > 98) { // 90 is too small
			continue;
		}

		if (xyz.z < 180 || xyz.z > 192) {
			continue;
		}

		if (debuging) {
			SDL_Log("hit it.");
		}

		hit_indexs.insert(index);
		if (left) {
			left_hit_rects.push_back(SDL_Rect{rect.x, rect.y, rect.width, rect.height});

		} else {
			right_hit_rects.push_back(SDL_Rect{rect.x, rect.y, rect.width, rect.height});
		}
    }

	if ((left_hit_rects.size() == 1 && right_hit_rects.size() > 1) || 
		(left_hit_rects.size() > 1 && right_hit_rects.size() == 1)) {

		bool adjust_left = left_hit_rects.size() > 1;
		const SDL_Rect& this_rect = adjust_left? right_hit_rects[0]: left_hit_rects[0];
		std::vector<SDL_Rect>& that_rects = adjust_left? left_hit_rects: right_hit_rects;
		VALIDATE(that_rects.size() > 1, null_str);

		const bool compare_width = true;

		int this_value = this_rect.w; // default compare width
		if (!compare_width) {
			// compare center_y
			this_value = this_rect.y + this_rect.h / 2;
		}

		int min_abs_diff = INT32_MAX;
		int min_abs_diff_at = nposm;
		int at = 0;
		for (std::vector<SDL_Rect>::const_iterator it = that_rects.begin(); it != that_rects.end(); ++ it, at ++) {
			const SDL_Rect& rect = *it;

			int that_value = rect.w;
			if (!compare_width) {
				that_value = rect.y + rect.h / 2;
			}
			int abs_diff = SDL_abs(that_value - this_value);
			if (abs_diff < min_abs_diff) {
				min_abs_diff = abs_diff;
				min_abs_diff_at = at;
			}
		}

		VALIDATE(min_abs_diff_at >= 0, null_str);
		SDL_Rect selected = that_rects[min_abs_diff_at];

		that_rects.clear();
		that_rects.push_back(selected);
	}

    if (debuging) {
		surface bg_surf(o);

		const int font_size = 9; // 7
		SDL_Color font_color = uint32_to_color(0xffffffff);
		SDL_Rect dstrect;

		for (std::map<int, cv::Rect>::const_iterator it = all_rects.begin(); it != all_rects.end(); ++ it) {
			int index = it->first;
			const cv::Rect& rect = it->second;
			SDL_Point center{rect.x + rect.width / 2, rect.y + rect.height / 2};

			surface text_surf = font::get_rendered_text(str_cast(index), 0, font_size, font_color);

			dstrect.x = center.x - text_surf->w / 2;
			dstrect.y = center.y - text_surf->h / 2;
			dstrect.w = text_surf->w;
			dstrect.h = text_surf->h;
			SDL_BlitSurface(text_surf.get(), nullptr, bg_surf.get(), &dstrect);
		}

        imwrite(o, "1-contours-1nd-all.png");
    }

	if (left_hit_rects.size() != 1 || right_hit_rects.size() != 1) {
		SDL_Log("find reference point fail, because hit_rects.size(%i) != 2, fram: (%ix%i), depth_scale: %.3f", 
			(int)(left_hit_rects.size() + right_hit_rects.size()), width, height, depth_scale);

		if (!dbg_RP_ && game_config::os == os_windows) {
			save_dbg_d2c(surf, depth_data, depth_scale, true);

			if (!hit_indexs.empty()) {
				cv::Mat o = cv::Mat::zeros(erode_mat.rows, erode_mat.cols, CV_8UC3);
				for (std::set<int>::const_iterator it = hit_indexs.begin(); it != hit_indexs.end(); ++ it) {
					int index = *it;
					
					cv::Scalar color(rand() % 255, rand() % 255, rand() % 255);
					cv::drawContours(o, contours, index, color, cv::FILLED, 8, hierarchy);
				}
				imwrite(o, short_dbg_contours_png_);
			}
		}
		return false;
	}

	result[0] = left_hit_rects[0];
	result[1] = right_hit_rects[0];

	result[0].x += roi_x;
	result[0].y += roi_y;

	result[1].x += roi_x;
	result[1].y += roi_y;
/*
	SDL_Log("contour_rects[#0(%i, %i, %i, %i), #1(%i, %i, %i, %i)] result[#0(%i, %i, %i, %i), #1(%i, %i, %i, %i)]", 
		left_hit_rects[0].x, left_hit_rects[0].y, left_hit_rects[0].w, left_hit_rects[0].h,
		right_hit_rects[1].x, right_hit_rects[1].y, right_hit_rects[0].w, right_hit_rects[0].h, 
		result[0].x, result[0].y, result[0].w, result[0].h,
		result[1].x, result[1].y, result[1].w, result[1].h);
*/
	VALIDATE(result[0].x >= 0 && result[0].y >= 0 && result[1].x >= 0 && result[1].y >= 0, null_str);

	return true;
}

#pragma pack(1)

struct tclearing_observation_header {
	SDL_DPoint laser_origin;
    double yaw;
    uint32_t cloud_point_size;
    uint32_t fills;
    uint32_t world_point_size;
};

#pragma pack()

void dbg_clearing_observation_dat()
{
	std::string filename = game_config::preferences_dir + "/1-clearing_observation.dat";

	tclearing_observation_header header;
	memset(&header, 0, sizeof(header));
	std::vector<SDL_DPoint> world_points;

	{
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		if (!file.valid()) {
			return;
		}
		posix_fseek(file.fp, 0);
		int fsize = posix_fsize(file.fp);
		if (fsize < sizeof(tclearing_observation_header)) {
			return;
		}
		posix_fseek(file.fp, 0);

		posix_fread(file.fp, &header, sizeof(tclearing_observation_header));
		// if (header.fourcc != SDL_FOURCC('D', 'E', 'P', 'T')) {
		//	return;
		// }
		if (fsize != sizeof(tclearing_observation_header) + header.world_point_size * sizeof(SDL_DPoint)) {
			return;
		}
	
		file.resize_data(header.world_point_size * sizeof(SDL_DPoint));
		posix_fread(file.fp, file.data, header.world_point_size * sizeof(SDL_DPoint));

		const SDL_DPoint* world_points_ptr = (const SDL_DPoint*)file.data;
		for (int at = 0; at < (int)header.world_point_size; at ++) {
			const SDL_DPoint& point = world_points_ptr[at];
			world_points.push_back(point);
		}
	}

	const int min_angle_degree = -MOVEIT_SECTOR_HALF_ANGLE_DEG;
	const int max_angle_degree = MOVEIT_SECTOR_HALF_ANGLE_DEG;
	// const int node_count = (max_angle_degree - min_angle_degree) * 2;
	const int node_count = (max_angle_degree - min_angle_degree) * 1;

	// It's not really about using sensor_msgs::LaserScan, it's just borrowing,
	// avoid to introduce too many variables.
	sensor_msgs::LaserScan scan_msg;
	scan_msg.angle_min = DEG2RAD(min_angle_degree);
	scan_msg.angle_max = DEG2RAD(max_angle_degree);
	scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min) / node_count;

	scan_msg.ranges.resize(node_count);
	float* range_data = &scan_msg.ranges[0];
	memset(range_data, 0, sizeof(float) * node_count);

	double ox = header.laser_origin.x;
	double oy = header.laser_origin.y;
	int at2 = 0;
	const SDL_DPoint* world_points_ptr = &world_points[0];
	for (int at2 = 0; at2 < (int)header.cloud_point_size; at2 ++) {
		const SDL_DPoint& point = world_points[at2];
		double wx = point.x;
		double wy = point.y;

        double dx = wx - ox;
        double dy = wy - oy;
        double theta = atan2(dy, dx);
        if (KDL::Equal(dx, 0) && KDL::Equal(dy, 0)) {
            theta = 0;
        }
		theta -= header.yaw;
        if (theta >= scan_msg.angle_min && theta <= scan_msg.angle_max) {
            float angle_positivization = theta - scan_msg.angle_min;
            int at = angle_positivization / scan_msg.angle_increment;
            if (at >= node_count) {
                at = node_count - 1;
            }
			SDL_Log("[%i]yaw: %.3f point: (%.3f, %.3f) - o(%.3f, %.3f) = dx: (%.3f, %.3f) theta: %.3f => at: %i", 
				at2, RAD2DEG(header.yaw), wx, wy, ox, oy, dx, dy, RAD2DEG(theta), at);
            float non_zero_value = 8.0;
            range_data[at] = non_zero_value;
            if (at > 0) {
                range_data[at - 1] = non_zero_value;
            }
            if (at > 1) {
                range_data[at - 2] = non_zero_value;
            }
            if (at < node_count - 1) {
                range_data[at + 1] = non_zero_value;
            }
            if (at < node_count - 2) {
                range_data[at + 2] = non_zero_value;
            }
              
        }
    }

	std::map<float, float> theta_dist_ranges;
    const double r = 0.8; // 0.8m, 1.5m?
    float angle = scan_msg.angle_min + header.yaw;
    int fills = 0;
    for (int at = 0; at < node_count; at ++, angle += scan_msg.angle_increment) {
        if (range_data[at] == 0) {
            double wx = r * cos(angle) + ox;
            double wy = r * sin(angle) + oy;

			const SDL_DPoint& src = world_points_ptr[header.cloud_point_size + fills];
			VALIDATE(KDL::Equal(wx, src.x) && KDL::Equal(wy, src.y), null_str);
            // world_points.push_back(SDL_DPoint{wx, wy});
            fills ++;
        }
      
        theta_dist_ranges.insert(std::make_pair(RAD2DEG(angle - header.yaw), range_data[at]));
    }

	VALIDATE(fills == header.fills, null_str);
}

bool tmoveit_calculator::dbg_load_files(const std::string& short_dbg_surf_png, const std::string& short_dbg_depth_data_dat, surface* surf_ptr, tdcdepth_data& dcdepth_data) const
{
	VALIDATE(!short_dbg_depth_data_dat.empty(), null_str);

	utils::string_map symbols;
	std::string err;

	if (surf_ptr != nullptr) {
		VALIDATE(!short_dbg_surf_png.empty(), null_str);
		const std::string filename = game_config::preferences_dir + "/" + short_dbg_surf_png;
		surface surf = image::get_image(filename);
		if (surf.get() == nullptr) {
			symbols["file"] = filename;
			err = vgettext2("$file isn't a valid image file", symbols);
			gui2::show_message(null_str, err);
			return false;
		}
		*surf_ptr = surf;
	}

	const std::string depth_data_filename = game_config::preferences_dir + "/" + short_dbg_depth_data_dat;

	bool loaded = dcdepth_data.load_from_file(depth_data_filename);
	if (!loaded) {
		symbols["file"] = depth_data_filename;
		err = vgettext2("Load depth data from $file fail", symbols);
		gui2::show_message(null_str, err);
		return false;
	}
	if (surf_ptr != nullptr) {
		if (dcdepth_data.header.width != surf_ptr->get()->w || dcdepth_data.header.height != surf_ptr->get()->h) {
			err = vgettext2("width or height of dbg_surf are not same as depth_data", symbols);
			gui2::show_message(null_str, err);
			return false;
		}
	}

	return true;
}

void tmoveit_calculator::dbg_find_reference_point()
{
	surface surf;
	tdcdepth_data dcdepth_data;
	bool loaded = dbg_load_files(short_dbg_RP_surf_png_, short_dbg_RP_depth_data_dat_, &surf, dcdepth_data);
	if (!loaded) {
		return;
	}

	const tdepth_file_header& header = dcdepth_data.header;
	const int16_t* depth_data = (const int16_t*)dcdepth_data.data;

	tdbg_RP_lock dbg_RP_lock(*this, header.intrinsics, header.width, header.height);

	tsurface_2_mat_lock lock(surf);

	SDL_Rect reference_rects[2];
	bool ret = find_reference_point(lock.mat, depth_data, header.depth_scale, reference_rects);

	SDL_Log("dbg_find_reference_point(depth_scale: %.3f) return %s, frame: (%ix%i)", header.depth_scale, ret? "true": "false", surf->w, surf->h);

}

bool tmoveit_calculator::dbg_save_d2c_png(gui2::tprogress_& progress, const tdcdepth_data& dcdepth_data, const uint8_t* tex_pixels, int task, bool all_rows)
{
	VALIDATE(task == dctask_d2c || task == dctask_depth, null_str);

	const tdepth_file_header& header = dcdepth_data.header;
	const int16_t* depth_data = (const int16_t*)dcdepth_data.data;
	const int depth_data_len = header.width * header.height * 2;

	tdcframe_C depth_frame;
	aplt::set_dcamera_frame(depth_frame, dcframetype_depth, dcformat_y16,
		header.width, header.height, (const uint8_t*)depth_data, depth_data_len, header.depth_scale);

	std::string result_png;
	if (task == dctask_d2c) {
		result_png = all_rows? "dbg_d2c_all_rows.png": "dbg_d2c.png";
	} else {
		result_png = all_rows? "dbg_depth_all_rows.png": "dbg_depth.png";
	}

	const double dcpitch = header.dcpitch;
	// const double dcpitch = float_nposm;
	save_d2c_png(header.intrinsics, &progress, tex_pixels, task, depth_frame, dcpitch, all_rows, result_png, null_str);

    return true;
}

void tmoveit_calculator::dbg_depthdata_2_png(bool d2c, bool all_rows)
{
	surface surf;
	tdcdepth_data dcdepth_data;
	bool loaded = dbg_load_files(short_dbg_normal_surf_png, short_dbg_normal_depth_data_dat, d2c? &surf: nullptr, dcdepth_data);
	if (!loaded) {
		return;
	}

	const tdepth_file_header& header = dcdepth_data.header;
	const int16_t* depth_data = (const int16_t*)dcdepth_data.data;
	const int depth_data_len = header.width * header.height * 2;

	tdbg_RP_lock dbg_RP_lock(*this, header.intrinsics, header.width, header.height);

	int value = 0;
	if (value == 0) {
		// tdcframe_C depth_frame;
		// aplt::set_dcamera_frame(depth_frame, dcframetype_depth, dcformat_y16,
		//	header.width, header.height, (const uint8_t*)depth_data, depth_data_len, header.depth_scale);

		if (d2c) {
			VALIDATE(surf.get() != nullptr, null_str);
			tsurface_2_mat_lock lock(surf);

			gui2::tprogress_default_slot slot(std::bind(&tmoveit_calculator::dbg_save_d2c_png, 
				this, _1, std::ref(dcdepth_data), lock.mat.data, dctask_d2c, all_rows));
		    gui2::run_with_progress(slot, null_str, null_str, 0);

		} else {
			uint8_t* mat_data = (uint8_t*)malloc(header.width * header.height * 4);
			gui2::tprogress_default_slot slot(std::bind(&tmoveit_calculator::dbg_save_d2c_png, 
				this, _1, std::ref(dcdepth_data), mat_data, dctask_depth, all_rows));
		    gui2::run_with_progress(slot, null_str, null_str, 0);
			free(mat_data);
		}

	} else if (value == 1) {
/*
		std::vector<cv::Point> corners;
		const std::string qrcode = find_qr(lock.mat, &corners);

		std::vector<SDL_2Point> color_points;
		color_points.push_back(SDL_2Point{corners[0].x, corners[0].y, corners[2].x, corners[2].y});
		// color_points.push_back(SDL_2Point{corners[0].x, corners[0].y, nposm, nposm});
		color_points.push_back(SDL_2Point{corners[1].x, corners[1].y, corners[3].x, corners[3].y});
		// color_points.push_back(SDL_2Point{corners[1].x, corners[1].y, nposm, nposm});

		std::vector<SDL_Point3> depth4 = calculate(depth_data, header.depth_scale, color_points);

		// if (game_config::os == os_windows && depth4[0].z > 400) {
			SDL_Log("depth6[0].xyz: (%i, %i, %i) {points}[0](%i, %i) [1](%i, %i) [2](%i, %i) [3](%i, %i)", 
				depth4[0].x, depth4[0].y, depth4[0].z,
				corners[0].x, corners[0].y, corners[1].x, corners[1].y,
				corners[2].x, corners[2].y, corners[3].x, corners[3].y);
		// }
			int ii = 0;
*/
	}
}

void tmoveit_calculator::OnTriggerExit()
{
    task_event_.Set();
}

SDL_DPoint3 RP_2_PRP(const SDL_DPoint3& RP, double dcpitch, const SDL_DPoint3& PRP_offset, const SDL_DPoint3& PRP_2offset)
{
	VALIDATE(PRP_offset.y == 0, null_str);
	VALIDATE(PRP_2offset.y == 0, null_str);

	tf2::Quaternion q;
	q.setRPY(0, dcpitch, 0);
	const tf2::Transform transform2(q);

	SDL_DPoint3 in_xyz{PRP_offset.x + PRP_2offset.x, PRP_offset.y, PRP_offset.z + PRP_2offset.z};
	SDL_DPoint3 joint5_xyz;
	tf2::doTransform_position(transform2, in_xyz, joint5_xyz);

	SDL_DPoint3 PRP = RP;
	PRP.x -= joint5_xyz.x;
	// don't modify y.
	// P0P.y -= joint5_xyz.y;
	PRP.z -= joint5_xyz.z;

	SDL_Log("RP: (%.6f, %.6f, %.6f) PRP: (%.6f, %.6f, %.6f) PRP_offset: (%.6f, %.6f, %.6f) t_out_joint5: (%.6f, %.6f, %.6f)", 
		RP.x, RP.y, RP.z, PRP.x, PRP.y, PRP.z, PRP_offset.x, PRP_offset.y, PRP_offset.z,
		joint5_xyz.x, joint5_xyz.y, joint5_xyz.z);

	return PRP;
}

namespace tf2
{

SDL_DPoint3 doTransform_position(double roll, double pitch, double yaw, const SDL_DPoint3& p_in)
{
	tf2::Quaternion q;
	q.setRPY(roll, pitch, yaw);
	const tf2::Transform transform(q);

	SDL_DPoint3 result;
	tf2::doTransform_position(transform, p_in, result);
	return result;
}

}

void tmoveit_calculator::set_ik(const SDL_DPoint3& PRP_offset, const SDL_DPoint3& RP, const SDL_DPoint3& PRP, const SDL_DPoint3& TP)
{
	const std::string ik_arm_name = group_name_ik_arm;
	const SDL_DSize3& target_size = operate_.target_info.size;

	SDL_DPoint3 PRP_diff{TP.x - PRP.x, TP.y - PRP.y, TP.z - PRP.z};
	geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name);

	const SDL_DPoint3 claw_model_center = claw_.model_grasp_point(target_size, operate_.claw_tip_pos, nullptr);
	const SDL_DPoint3 PRP_model_center{PRP_offset.x + claw_model_center.x, PRP_offset.y + claw_model_center.y, PRP_offset.z + claw_model_center.z};
	const SDL_DPoint3 t_out = tf2::doTransform_position(0, operate_.set_ik_dcpitch, 0, PRP_model_center);

	double precise_gap_x = float_nposm;
	double gap_z = float_nposm;
	if (!operate_.grasp) {
		precise_gap_x = t_out.x;
		gap_z = t_out.z;

		SDL_DPoint3 tmp{PRP.x + t_out.x, PRP.y + t_out.y, PRP.z + t_out.z};
		SDL_Log("{delta.x}{press}precise_gap_x(%.6f), gap_z: %.6f, tmp: (%.6f, %.6f, %.6f)", 
			precise_gap_x, gap_z, tmp.x, tmp.y, tmp.z);
		
	} else if (operate_.grasp_method == grasp_oblique) {
		precise_gap_x = t_out.x - target_size.l / 2;
		gap_z = t_out.z + target_size.h;

		SDL_DPoint3 tmp{PRP.x + t_out.x, PRP.y + t_out.y, PRP.z + t_out.z};
		SDL_Log("{delta.x}{grasp}precise_gap_x(%.6f), gap_z: %.6f, tmp: (%.6f, %.6f, %.6f)", 
			precise_gap_x, gap_z, tmp.x, tmp.y, tmp.z);

	} else if (operate_.grasp_method == grasp_horizontal) {
		// TP is at right-top corner
		double closeto_target_dist = target_size.l;
		double leave_dist = PRP_model_center.x - closeto_target_dist;
		precise_gap_x = leave_dist + target_size.l;

		gap_z = target_size.h / 2;

	} else {
		VALIDATE(false, null_str);
	}

	VALIDATE(!is_float_nposm(precise_gap_x), null_str);
	VALIDATE(!is_float_nposm(gap_z), null_str);

	// const double arm_error_threshold = 0.010; // 1.5cm
	const double arm_error_threshold = 0.005; // 1.5cm
	// const double recognition_2bonus_x = 0; // (using)0.005cm
	// const double recognition_bonus_x = arm_error_threshold + recognition_2bonus_x;
	const double recognition_bonus_x = arm_error_threshold;
	// const double blind_x_threshold = 0.030; // 0.030
	// const double blind_x_threshold = 0.015; // 0.030
	const double blind_x_threshold = 0.010; // 0.030
	VALIDATE(blind_x_threshold > recognition_bonus_x, null_str);
	if (operate_.ik_has_success && PRP_diff.x <= precise_gap_x + blind_x_threshold) {
		// why operate_.ik_has_success == true?
		// Target is very close to the robot, and after action_nearing, result to operate_.near_move_finished = true, 
		// and Ik has not executed it once. --- At least once Ike has succeeded.
		dcamera_slot_impl_.set_set_ik_diff_label(PRP_diff.x - precise_gap_x, PRP_diff.z - gap_z);
		SDL_Log("{delta.x}set_ik_dcpitch: %.2f, PRP_diff.x(%.6f) <= precise_gap_x(%.6f) + blind_x_threshold(%.6f), error_x: %.6f, near move finished, start blind move", 
			RAD2DEG(operate_.set_ik_dcpitch), PRP_diff.x, precise_gap_x, blind_x_threshold, PRP_diff.x - precise_gap_x);
		operate_.near_move_finished = true;
		operate_.near_moved_fk = curr_fk;

	} else {
		SDL_Log("{delta.x}set_ik_dcpitch: %.2f, PRP_diff.x(%.6f) > precise_gap_x(%.6f) + blind_x_threshold(%.6f), continue near move", 
			RAD2DEG(operate_.set_ik_dcpitch), PRP_diff.x, precise_gap_x, blind_x_threshold);
	}

	const double bonus_x = operate_.near_move_finished? 0: recognition_bonus_x;

	const double gap_x = bonus_x + precise_gap_x;

	SDL_DPoint3 delta = PRP_diff;
	delta.x = PRP_diff.x - gap_x;
	delta.z = PRP_diff.z - gap_z;
	// if want to look about 0.035cm's arm-error, may execut it.
	// if (delta.x > 0.04) {
	//	delta.x -= 0.035;
	// }

	geometry_msgs::Pose query_fk = curr_fk;
	query_fk.position.x += delta.x;
	query_fk.position.y += delta.y;
	query_fk.position.z += delta.z;

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "(%.6f, %.6f, %.6f) precise_gap_x: %.6f gap:(%.6f, 0.0, %.6f) delta:(%.6f, %.6f, %.6f) joint1: %.3f", 
		query_fk.position.x, query_fk.position.y, query_fk.position.z, precise_gap_x, gap_x, gap_z, delta.x, delta.y, delta.z, RAD2DEG(*joint1_position_));
	SDL_Log("{dbg_ik}(#2)ik_query: %s", buf);

	double roll = 0.0;
	double pitch = 0.0;
	double yaw = 0.0;

	if (!operate_.near_move_finished) {
		operate_.ik_query.set(query_fk.position.x, query_fk.position.y, query_fk.position.z, roll, pitch, yaw, true);

	} else {
		SDL_Log("it doesn't use blind move, and near move is finished, this is virtual ik.");
		// operate_.ik_query.set(curr_fk.position.x, curr_fk.position.y, curr_fk.position.z, roll, pitch, yaw, true);
		operate_.ik_query.valid = false;
	}
/*
	std::vector<double>& result = operate_.ik_result;
	result.clear();
	double nolimit = std::numeric_limits<float>::max();
	const tpose3d bounds(ik_bound_, nolimit, ik_bound_, nolimit, nolimit, nolimit);
	bool use_as_seed = false;

	if (operate_.near_move_finished) {
		SDL_Log("it doesn't use blind move, and near move is finished, this is virtual ik.");
		operate_.ik_query.set(curr_fk.position.x, curr_fk.position.y, curr_fk.position.z, roll, pitch, yaw, true);
		result = ros_instance_.get_group_joint_values(ik_arm_name);

	} else {
		operate_.ik_query.set(query_fk.position.x, query_fk.position.y, query_fk.position.z, roll, pitch, yaw, true);
		result = ros_instance_.do_light_joint_pose_target(ik_arm_name, operate_.ik_query, bounds, use_as_seed);
	}

	return !result.empty();
*/
}

bool tmoveit_calculator::do_ik()
{
	const std::string ik_arm_name = group_name_ik_arm;

	std::vector<double>& result = operate_.ik_result;
	result.clear();
	double nolimit = std::numeric_limits<float>::max();
	const tpose3d bounds(ik_bound_, nolimit, ik_bound_, nolimit, nolimit, nolimit);
	bool use_as_seed = false;

	result = ros_instance_.do_light_joint_pose_target(ik_arm_name, operate_.ik_query, bounds, use_as_seed);
	return !result.empty();
}

bool tmoveit_calculator::reach_dcpitch_use_joint(const std::string& scene, const std::string& joint_name, double desire_dcpitch, double error_threshold)
{
	double* joint_position = ros_instance_.joint_position_from_name(joint_name);
	const aplt::tjoint_model& joint_model = ros_instance_.joint_model_from_name(joint_name);

	VALIDATE(error_threshold > 0, null_str);
	const double curr_dcpitch = ros_instance_.get_stable_imu(rpy_pitch, false);

	uint32_t start_ticks = SDL_GetTicks();
	SDL_Log("%u (%s-%s)reach_dcpitch_use_joint(desire_dcpitch: %.3f, error_threshold: %.3f) curr_dcpitch: %.3f near_move_finished: %s", 
		start_ticks, scene.c_str(), joint_name.c_str(), RAD2DEG(desire_dcpitch), RAD2DEG(error_threshold), RAD2DEG(curr_dcpitch), operate_.near_move_finished? "true": "false"); 

	const aplt::tvariable_positions& aplt_position = ros_instance_.aplt_positions_of_moveit_model();

	double diff_pitch = desire_dcpitch - curr_dcpitch;
	double curr_joint4_value = *joint_position; // aplt_position.positions[joint_index];
	double desire_joint4_value = curr_joint4_value + diff_pitch;
	std::map<std::string, double> values;
	values.insert(std::make_pair(joint_name_PRP, desire_joint4_value));

	double imu_pitch = float_nposm;
	// double error_threshold = DEG2RAD(0.3);
	const int max_jitter_times = 5;
	int jitter_times = nposm;
	bool fail = false;

	while (!operate_.stopping && !fail && fabs(diff_pitch) > error_threshold && jitter_times < max_jitter_times) {
		if (fabs(diff_pitch) < DEG2RAD(1.0)) {
			SDL_Log("#%i think jitter, change diff_pitch from %.6f to %.6f", 
				jitter_times, diff_pitch, diff_pitch / 2.0); 

			diff_pitch /= 2.0;
			if (jitter_times == nposm) {
				jitter_times = 0;
			}
		}
		SDL_Log("%u #%i desire_dcpitch: %.6f(deg: %.3f) diff_pitch: %.6f(deg: %.3f) curr_joint4_value: %.6f(deg: %.3f) desire_joint4_value: %.6f(deg: %.3f)", 
			SDL_GetTicks(), jitter_times, desire_dcpitch, RAD2DEG(desire_dcpitch),
			diff_pitch, RAD2DEG(diff_pitch), curr_joint4_value, RAD2DEG(curr_joint4_value),
			desire_joint4_value, RAD2DEG(desire_joint4_value));

		if (fabs(diff_pitch) < DEG2RAD(5)) {
			ros_instance_.do_JointState_action(values);
		} else {
			ros_instance_.do_light_single_joint_target(joint_name_PRP, desire_joint4_value);
		}

		// read a stable imu's pitch.
		imu_pitch = ros_instance_.get_stable_imu(rpy_pitch, false);
		VALIDATE(!is_float_nposm(imu_pitch), null_str);

		diff_pitch = desire_dcpitch - imu_pitch;
		curr_joint4_value = *joint_position; // aplt_position.positions[joint_index];
		desire_joint4_value = curr_joint4_value + diff_pitch;
		if (desire_joint4_value < joint_model.min_position_ || desire_joint4_value > joint_model.max_position_) {
			fail = true;
			break;
		}
		values[joint_name_PRP] = desire_joint4_value;

		if (jitter_times != nposm) {
			jitter_times ++;
		}
	}

	uint32_t end_ticks = SDL_GetTicks();
	SDL_Log("%u (%s-%s)reach_dcpitch_use_joint finished, use %u ms", end_ticks, scene.c_str(), joint_name.c_str(), end_ticks - start_ticks);

	return !operate_.stopping && !fail;
}

bool tmoveit_calculator::reach_dcpitch_use_PRP_joint(const std::string& scene, double desire_dcpitch)
{
	return reach_dcpitch_use_joint(scene, joint_name_PRP, desire_dcpitch, DEG2RAD(2));
}

void tmoveit_calculator::dbg_claw_joint()
{
	{
		const std::string joint_name_claw = "joint6";
		ros_instance_.do_light_single_joint_target(joint_name_claw, -0.8);
		return;
	}
}

void tmoveit_calculator::DoWork(bool& exit)
{
	const std::string ik_arm_name = group_name_ik_arm;

	while (!exit) {
		SDL_Log("%u {tmoveit_calculator::DoWork}curr state: %i", SDL_GetTicks(), operate_.state);

		if (operate_.state == state_ik) {
			VALIDATE(!is_float_nposm(operate_.RP.x) && !is_float_nposm(operate_.TP.x) && !is_float_nposm(operate_.PRP.x), null_str);

			bool ret = do_ik();

			const bool dbg_ik_fail_1th = false;
			if (ret && dbg_ik_fail_1th && operate_.ik_times == 0) {
				operate_.ik_times ++;

				operate_.ik_result.clear();
				ret = false;
				SDL_Log("%u {tmoveit_calculator::DoWork} set ik fail 1th for debug", SDL_GetTicks());
			}

			if (ret) {
				std::vector<double>& result = operate_.ik_result;
				double PRP_joint_value = 0.0;
				if (!operate_.adjust_TP) {
					PRP_joint_value = DEG2RAD(-10); // -10(rad:-0.174444)
				} else {
					PRP_joint_value = DEG2RAD(40); // 30
				}
				result[result.size() - 1] = PRP_joint_value;

				ros_instance_.do_light_joint_value_target(ik_arm_name, result);

				operate_.ik_has_success = true;
				operate_.ik_times ++;


				const double desire_dcpitch = operate_.first_dcpitch;
				reach_dcpitch_use_PRP_joint("after ik", desire_dcpitch);
/*
				const double error_threshold = DEG2RAD(2);

				double curr_dcpitch = ros_instance_.get_imu_rpy().pitch;
				double diff_pitch = desire_dcpitch - curr_dcpitch;
				if (fabs(diff_pitch) > error_threshold) {
					const aplt::tvariable_positions& aplt_position = ros_instance_.aplt_positions_of_moveit_model();

					double curr_joint4_value = aplt_position.positions[3];
					double desire_joint4_value = curr_joint4_value + diff_pitch;

					if (fabs(diff_pitch) < DEG2RAD(5)) {
						SDL_Log("{reach dcpitch after ik}do_JointState_action, desire_dcpitch(%.3f) - curr_dcpitch(%.3f) = diff_pitch(%.3f)", 
							RAD2DEG(desire_dcpitch), RAD2DEG(curr_dcpitch), RAD2DEG(diff_pitch));
						std::map<std::string, double> values;
						values.insert(std::make_pair(joint_name_PRP, desire_joint4_value));
						ros_instance_.do_JointState_action(values);
					} else {
						SDL_Log("{reach dcpitch after ik}do_light_single_joint_target, desire_dcpitch(%.3f) - curr_dcpitch(%.3f) = diff_pitch(%.3f) desire_joint4_value: %.3f", 
							RAD2DEG(desire_dcpitch), RAD2DEG(curr_dcpitch), RAD2DEG(diff_pitch), RAD2DEG(desire_joint4_value));
						ros_instance_.do_light_single_joint_target(joint_name_PRP, desire_joint4_value);
					}
				}
*/
			}

			threading::lock lock(task_data_mutex_);
			task_finished_ = true;

		} else if (operate_.state == state_claw) {
			VALIDATE(operate_.near_move_finished, null_str);
			SDL_DPoint3 PRP = operate_.PRP;
			{
				geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name);
				SDL_Log("{state_claw}curr_fk(%.6f, %.6f, %.6f) - near_moved_fk(%.6f, %.6f, %.6f): diff(%.6f, %.6f, %.6f)",
					curr_fk.position.x, curr_fk.position.y, curr_fk.position.z,
					operate_.near_moved_fk.position.x, operate_.near_moved_fk.position.y, operate_.near_moved_fk.position.z,
					curr_fk.position.x - operate_.near_moved_fk.position.x,
					curr_fk.position.y - operate_.near_moved_fk.position.y,
					curr_fk.position.z - operate_.near_moved_fk.position.z);
				// RP.x += curr_fk.position.x - operate_.near_moved_fk.position.x;
				// RP.z += curr_fk.position.z - operate_.near_moved_fk.position.z;

				PRP.x += curr_fk.position.x - operate_.near_moved_fk.position.x;
				PRP.z += curr_fk.position.z - operate_.near_moved_fk.position.z;
			}
			SDL_DPoint3 diff{operate_.TP.x - PRP.x,
						operate_.TP.y - PRP.y,
						operate_.TP.z - PRP.z};

			double curr_dcpitch = ros_instance_.get_stable_imu(rpy_pitch, false);
			SDL_Log("{state_claw}dcpitch: %.6f(deg: %.3f) TP(%.6f, %.6f, %.6f) - PRP = diff:(%.6f, %.6f, %.6f)",
				curr_dcpitch, RAD2DEG(curr_dcpitch), operate_.TP.x, operate_.TP.y, operate_.TP.z,
				diff.x, diff.y, diff.z);

			const SDL_DPoint3& PRP_offset = PRP_offset_;
			const SDL_DSize3& target_size = operate_.target_info.size;

			SDL_DPoint3 TP = operate_.TP;

			tclaw_snapshot snapshot;
			SDL_DPoint3 claw_model_center = claw_.model_grasp_point(target_size, operate_.claw_tip_pos, &snapshot);
			SDL_DPoint3 PRP_model_center{PRP_offset.x + claw_model_center.x, PRP_offset.y + claw_model_center.y, PRP_offset.z + claw_model_center.z};
			SDL_DPoint3 RP_offset{PRP_offset.x + PRP_2offset_.x, PRP_offset.y + PRP_2offset_.y, PRP_offset.z + PRP_2offset_.z};

			double desire_dcpitch = nposm;
			if (!operate_.grasp) {
				desire_dcpitch = find_best_dcpitch_4_press(curr_dcpitch, PRP, RP_offset, PRP_model_center, TP, target_size);

			} else if (operate_.grasp_method == grasp_oblique) {
				desire_dcpitch = find_best_dcpitch_4_grasp(curr_dcpitch, PRP, RP_offset, PRP_model_center, TP, target_size);

			} else if (operate_.grasp_method == grasp_horizontal) {
				desire_dcpitch = DEG2RAD(0);

			} else {
				VALIDATE(false, null_str);
			}
			VALIDATE(!is_float_nposm(desire_dcpitch), null_str);

			if (!operate_.grasp) {
				if (!operate_.adjust_TP) {
					reach_dcpitch_use_joint("claw_state", joint_name_PRP, DEG2RAD(35), DEG2RAD(4));
				} else {
					reach_dcpitch_use_joint("claw_state", joint_name_PRP, DEG2RAD(70), DEG2RAD(4));
				}

				std::vector<double> values;
				values.push_back(-0.8);
				ros_instance_.do_light_joint_value_target(group_name_claw, values);
			}

			bool reached = reach_dcpitch_use_joint("claw_state", joint_name_PRP, desire_dcpitch, DEG2RAD(0.3));
			static int times = 0;
			if (times == 0) {
				SDL_Log("#%i {claw_fail}, set reached to false", times);
				// reached = false;
			} else {
				SDL_Log("#%i {claw_fail}, don't modify reached", times);
			}
			if (reached) {
				VALIDATE(!operate_.claw_fail, null_str);

				std::vector<double> values;
				values.push_back(snapshot.value);

				if (!operate_.grasp) {
/*
					if (operate_.adjust_TP) {
						std::vector<double> values;
						values.push_back(-0.8);
						ros_instance_.do_light_joint_value_target(group_name_claw, values);
					}
*/
				} else if (operate_.grasp_method != grasp_horizontal) {
					ros_instance_.do_light_joint_value_target(group_name_claw, values);

					ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_place);
				}
			} else {
				operate_.claw_fail = true;
			}
			times ++;

			threading::lock lock(task_data_mutex_);
			task_finished_ = true;

		} else if (operate_.state == state_grasp) {
			threading::lock lock(task_data_mutex_);
			task_finished_ = true;

		} else {
			// if (curr_task_.state != nposm)
			VALIDATE(!task_finished_, null_str);
		}

		task_event_.Wait(trose_event::kForever);
	}
}

void tmoveit_calculator::start_operate(int scene, aplt::ttask_api& task_api, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(scene >= 0 && scene < scene_count, null_str);
	VALIDATE(operate_.state == nposm, null_str);
	VALIDATE(!task_finished_, null_str);

	operate_.scene = scene;
	operate_.task_api = &task_api;
	operate_.aplt = &aplt;
	operate_.cfg_task = &cfg_task;
	// operate_.task_id = task.id;
	operate_.target_info = task_api.moveit->start_task(aplt, cfg_task);

	operate_.initial_fk = ros_instance_.calculate_group_fk_tf(group_name_ik_arm);
	operate_.near_fk.position.x = float_nposm;

	operate_.nearing = false;
	will_goto_state_distance();
	operate_.state = state_distance;
	operate_.RP = SDL_DPoint3{float_nposm, float_nposm, float_nposm};
	operate_.TP = SDL_DPoint3{float_nposm, float_nposm, float_nposm};
	operate_.PRP = SDL_DPoint3{float_nposm, float_nposm, float_nposm};

	int moveit_op = operate_.target_info.op;
	if (moveit_op == moveit_op_grasp) {
		operate_.grasp = true;
		operate_.adjust_TP = false;
		operate_.claw_tip_pos = tclaw::tip_center;
		// operate_.claw_tip_pos = tclaw::tip_bottom;

		// If the set_ik_dcpitch is too small and a small target_size.h,
		// the end of the claw may hit the ground when the same dcpitch is made.
		// 
		// if target_size.h is >= 5, use 45
		operate_.set_ik_dcpitch = DEG2RAD(45); // 45 --> 40
		// operate_.set_ik_dcpitch = DEG2RAD(50);
		operate_.first_dcpitch = DEG2RAD(50);
		operate_.second_dcpitch = DEG2RAD(60);

	} else if (moveit_op == moveit_op_press_top) {
		operate_.grasp = false;
		operate_.adjust_TP = false;
		operate_.claw_tip_pos = tclaw::tip_bottom;
		operate_.set_ik_dcpitch = DEG2RAD(45);
		operate_.first_dcpitch = DEG2RAD(52);
		operate_.second_dcpitch = DEG2RAD(60);

	} else if (moveit_op == moveit_op_press_middle) {
		operate_.grasp = false;
		operate_.adjust_TP = true;
		operate_.claw_tip_pos = tclaw::tip_top;
		operate_.set_ik_dcpitch = DEG2RAD(55);
		operate_.first_dcpitch = DEG2RAD(60);
		operate_.second_dcpitch = DEG2RAD(65);

	} else {
		VALIDATE(false, null_str);
	}
	operate_.grasp_method = grasp_oblique;
	// operate_.grasp_method = grasp_horizontal;
	operate_.use_task_api = true; // false: barcode, true: qrcode
	operate_.ik_query.valid = false;
	operate_.near_move_finished = false;
	operate_.ik_has_success = false;
	operate_.m4ik.clear();
	operate_.rtyaw.clear();
	operate_.rtyaw.searched360 = false;
	operate_.claw_fail = false;
	operate_.stopping = false;
	operate_.ik_times = 0;
	// verbose_last_RP_.x = float_nposm;
	verbose_last_PRP_.x = float_nposm;

	operate_.min_abs_dcamera_theta_y0 = INT32_MAX;

	operate_.goto_second_dcpitch_ticks = 0;
	operate_.in_second_dcpitch = false;

	operate_.calibrate_near_PRP_diff = SDL_DPoint3{float_nposm, float_nposm, float_nposm};

	operate_.start_ticks = SDL_GetTicks();

	dcamera_slot_impl_.set_set_ik_diff_label(float_nposm, float_nposm);
}

void tmoveit_calculator::move_public_vel(double dist, double moveit_theta, bool first, std::string& msg)
{
	if (game_config::os == os_windows) {
		// before call 'move_public_vel', will make sure moveit is in state_recognize or state_recognize_near.
		VALIDATE(KDL::Equal(*joint1_position_, joint1_0degree_), null_str);
	}

	if (first) {
		operate_.m4ik.move_dist = dist;
		operate_.m4ik.moveit_theta = moveit_theta;
		operate_.m4ik.TP_PRP_diff_x = operate_.initial_fk_TP_PRP_diff_x; // operate_.TP.x - operate_.RP.x;
	}

	const double support_min_linear_x = 0.03; // 0.004
	// const double max_linear_x = 0.16;

	dist = dist * cos(fabs(moveit_theta));
	
	VALIDATE(!KDL::Equal(dist, 0), null_str);
	double twist[3] = {0, 0, 0};
	if (dist > 0) {
		twist[0] = SDL_max(dist, support_min_linear_x);
	} else if (dist < 0) {
		twist[0] = -1 * SDL_max(fabs(dist), support_min_linear_x);
	}

	const double abs_tune_angular_z = DEG2RAD(2);
	const double counterclockwise_threshold = DEG2RAD(-4);
	const double clockwise_threshold = DEG2RAD(-12);
	if (dist < 0) {
		twist[2] = abs_tune_angular_z;

	} else if (moveit_theta > counterclockwise_threshold) {
		// Rotate counterclockwise. roate joint1 as soon as possible.
		twist[2] = abs_tune_angular_z;

	} else if (moveit_theta < clockwise_threshold) {
		if (dist > 0) {
			twist[2] = -1 * abs_tune_angular_z;
		}
	}

	SDL_Log("%u ik fail{moving_4_ik} dist: %.5f, first: %s, pulbic vel: (%.5f, %.3f, %.3f)", SDL_GetTicks(), 
		dist, first? "true": "false", twist[0], twist[1], RAD2DEG(twist[2]));
	bool disble_public_vel = false;
	if (!disble_public_vel) {
		ros_instance_.public_vel(twist[0], twist[1], twist[2]);
	}

	operate_.m4ik.stop_ticks = SDL_GetTicks() + move_robot_threshold_;

	utils::string_map symbols;
	symbols["bf"] = dist > 0? _("forward"): _("backward");
	symbols["dist"] = utils::from_double(fabs(dist));
	msg = vgettext2("Desire $bf move $dist meters", symbols);
}

void tmoveit_calculator::rotate_360_public_vel(double ang_diff)
{
	double abs_ang_diff = fabs(ang_diff);
	const double max_yaw = DEG2RAD(20);

	double angular_z = ang_diff;
	if (abs_ang_diff > max_yaw) {
		const double use_yaw = max_yaw * 2 / 3;
		angular_z = ang_diff > 0? use_yaw: -1 * use_yaw;
	}

	double twist[3] = {0, 0, angular_z};
	ros_instance_.public_vel(twist[0], twist[1], twist[2]);

	SDL_Log("%u {360 rotate}ang_diff: %.3f pulbic vel: (%.5f, %.3f, %.3f)", SDL_GetTicks(), 
		RAD2DEG(ang_diff), twist[0], twist[1], RAD2DEG(twist[2]));

	operate_.rtyaw.move_end_ticks = SDL_GetTicks() + move_robot_threshold_;
}

void tmoveit_calculator::set_goto_near_ticks(bool force)
{
	if (force || operate_.goto_near_ticks != 0) {
		operate_.goto_near_ticks = SDL_GetTicks() + near_threshold_;
		// SDL_Log("%u {goto_near_ticks}2.2 has valid user recognized frame, set future ticks again", SDL_GetTicks());
	}
}

void tmoveit_calculator::set_goto_second_dcpitch_ticks(bool force)
{
	if (force || operate_.goto_second_dcpitch_ticks != 0) {
		operate_.goto_second_dcpitch_ticks = SDL_GetTicks() + second_dcpitch_threshold_;
		// SDL_Log("%u {second_dcpitch}set future goto ticks again", SDL_GetTicks());
	}
}

void tmoveit_calculator::clear_second_dcpitch(const std::string& scene)
{
	if (!operate_.in_second_dcpitch) {
		SDL_Log("%u {second_dcpitch}clear_second_dcpitch[%s]. not in second dcptih, goto_second_dcpitch_ticks: %u", SDL_GetTicks(), scene.c_str(), operate_.goto_second_dcpitch_ticks);
		operate_.goto_second_dcpitch_ticks = 0;

	} else {
		SDL_Log("%u {second_dcpitch}clear_second_dcpitch[%s]. be in second dcptih, do nothing", SDL_GetTicks(), scene.c_str());
		VALIDATE(operate_.goto_second_dcpitch_ticks == 0, null_str);
		operate_.in_second_dcpitch = false;
	}
}

SDL_DPoint3 tmoveit_calculator::calculate_TP_PRP_diff_diff(const SDL_DPoint3& PRP, const SDL_DPoint3& TP)
{
	const SDL_DPoint3& last_TP = verbose_last_TP_;
	const SDL_DPoint3& last_PRP = verbose_last_PRP_;

	if (is_float_nposm(last_PRP.x)) {
		return SDL_DPoint3{float_nposm, float_nposm, float_nposm};
	}

	// first's TP_PRP_diff
	SDL_DPoint3 first_TP_PRP_diff{last_TP.x - last_PRP.x, 
		last_TP.y - last_PRP.y,
		last_TP.z - last_PRP.z};

	// second's TP_PRP_diff
	SDL_DPoint3 second_TP_PRP_diff{TP.x - PRP.x, 
		TP.y - PRP.y,
		TP.z - PRP.z};

	SDL_DPoint3 TP_PRP_diff_diff {first_TP_PRP_diff.x - second_TP_PRP_diff.x,
		first_TP_PRP_diff.y - second_TP_PRP_diff.y,
		first_TP_PRP_diff.z - second_TP_PRP_diff.z};

	return TP_PRP_diff_diff;
}

double tmoveit_calculator::fail_revert_to_initial_state()
{
	double moveit_theta = moveit_theta_from_joint1_value();
	geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(group_name_ik_arm);
	if (!KDL::Equal(curr_fk.position.x, operate_.initial_fk.position.x, same_initial_fk_x_thresould_)) {
		ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);
	} else {
		double rotated = zero_joint1_if_necessary();
		if (!is_float_nposm(rotated)) {
			VALIDATE(KDL::Equal(rotated, -1 * moveit_theta), null_str);
		}
	}

	if (operate_.ik_has_success) {
		operate_.ik_has_success = false;
	}

	if (operate_.near_move_finished) {
		SDL_Log("{fail_revert_to_initial_state}set near_move_finished to false");
		operate_.near_move_finished = false;
	}

	return moveit_theta;
}

std::string tmoveit_calculator::operate_slice(tdepthcapture::VideoRenderer2& vsink2, const std::vector<SDL_2Point>* object_points_ptr)
{
	VALIDATE_IN_MAIN_THREAD();

	std::string msg;

	const int state = operate_.state;
	if (state == nposm) {
		return msg;
	}


	if (operate_.rtyaw.next_rotate_ticks != 0 && SDL_GetTicks() >= operate_.rtyaw.next_rotate_ticks) {
		VALIDATE(!operate_.rtyaw.searched360, null_str); 

		ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);

		// yaw of action_recognize and action_recognize_near is different.
		double curr_yaw = ros_instance_.get_stable_imu(rpy_yaw, false);

		bool can = true;
		if (!operate_.rtyaw.is_360_rotating()) {
			operate_.rtyaw.start_360_rotate(curr_yaw, move_robot_threshold_, msg);
		} else {
			can = operate_.rtyaw.start_next_360_rotate(curr_yaw, move_robot_threshold_, msg);
		}

		if (can) {
			double ang_diff = angles::shortest_angular_distance(curr_yaw, operate_.rtyaw.goal);
			rotate_360_public_vel(ang_diff);

		} else {
			VALIDATE(operate_.rtyaw.searched360, null_str);
		}
		return msg;
	}

	if (operate_.rtyaw.rotating()) {
		VALIDATE(operate_.state == state_distance, null_str);
		VALIDATE(operate_.m4ik.stop_ticks == 0, null_str);
		if (SDL_GetTicks() < operate_.rtyaw.move_end_ticks) {
			return msg;
		}

		double curr_yaw = ros_instance_.get_imu_rpy().yaw;
		if (operate_.rtyaw.is_360_rotating()) {
			if (operate_.rtyaw.reaching_goal) {
				double stable_yaw = ros_instance_.get_stable_imu(rpy_yaw, false);

				double ang_diff = angles::shortest_angular_distance(curr_yaw, operate_.rtyaw.goal);
				const double threshold = DEG2RAD(10);
				if (ang_diff <= threshold) {
					SDL_Log("%u {360 rotate}yaw range(%.3f + %i) ang_diff: %.3f(<= 10) begin search object", 
						SDL_GetTicks(), RAD2DEG(operate_.rtyaw.goal - operate_.rtyaw.initial_yaw), operate_.rtyaw.step_degree, RAD2DEG(ang_diff));
					operate_.rtyaw.reaching_goal = false;

					VALIDATE(operate_.nearing, null_str);
					// can goto near again
					set_goto_near_ticks(true);
					// operate_.goto_near_ticks = SDL_GetTicks() + near_threshold_;
					operate_.nearing = false;

				} else {
					SDL_Log("%u {360 rotate}yaw range(%.3f + %i) ang_diff: %.3f(> 10) continue public vel", 
						SDL_GetTicks(), RAD2DEG(operate_.rtyaw.goal - operate_.rtyaw.initial_yaw), operate_.rtyaw.step_degree, RAD2DEG(ang_diff));

					// If the rotation is too large, I am afraid that it will cause the robot to concuss.
					rotate_360_public_vel(ang_diff);
				}
			}

		} else {
			// it is once rotate
			SDL_Log("%u {rotate_to_yaw}finished, goal: %.3f curr: %.3f", SDL_GetTicks(), RAD2DEG(operate_.rtyaw.goal), RAD2DEG(curr_yaw));
			operate_.rtyaw.clear();
		}
	}

	if (operate_.m4ik.stop_ticks != 0) {
		VALIDATE(operate_.state == state_distance, null_str);
		if (SDL_GetTicks() < operate_.m4ik.stop_ticks) {
			return msg;
		}
		SDL_Log("%u ik fail{moving_4_ik} set operate_.m4ik.stop_ticks = 0", SDL_GetTicks());
		operate_.m4ik.stop_ticks = 0;
	}

	if (!require_wakeup_thread(state)) {
		// is wait for user provide corner's points.
		bool calibrate_require_near = false;
		if (operate_.scene == scene_calibrate_near && !operate_.nearing) {
			if (!is_float_nposm(operate_.TP.x)) {
				SDL_Log("%u {calibrate_near}calculated initial's TP: (%.5f, %.5f, %.5f). for calculate second TP, goto near", SDL_GetTicks(), 
					operate_.TP.x, operate_.TP.y, operate_.TP.z);
				calibrate_require_near = true;
				
				// make sure goto action_near immediately.
				operate_.goto_near_ticks = SDL_GetTicks();

			} else {
				// make sure not goto action_near
				set_goto_near_ticks(true);
				// operate_.goto_near_ticks = SDL_GetTicks() + near_threshold_;
			}

		} else if (operate_.scene == scene_always_initial) {
			operate_.goto_near_ticks = 0;

		} else if (operate_.scene == scene_always_near && !operate_.nearing) {
			// make sure goto action_near immediately.
			operate_.goto_near_ticks = SDL_GetTicks();
		} 

		if (object_points_ptr == nullptr || calibrate_require_near) {
			if (operate_.goto_near_ticks != 0) {
				VALIDATE(operate_.goto_second_dcpitch_ticks == 0, null_str);

				VALIDATE(operate_.state == state_distance, null_str);
				if (SDL_GetTicks() >= operate_.goto_near_ticks) {
					SDL_Log("%u {goto_near_ticks}2.1 set 0, do action_recognize_near, set nearing = true", SDL_GetTicks());
					VALIDATE(!operate_.nearing, null_str);
					operate_.goto_near_ticks = 0;
					operate_.nearing = true;
					ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize_near);
					if (is_float_nposm(operate_.near_fk.position.x)) {
						if (game_config::os == os_windows) {
							// so far, do_light_group_values_target maybe not reach joint_value precise.
							VALIDATE(KDL::Equal(*joint1_position_, joint1_0degree_), null_str);
						}
						operate_.near_fk = ros_instance_.calculate_group_fk_tf(group_name_ik_arm);
						SDL_Log("%u ik fail{moving_4_ik}near_fk.x - initial_fk.x: %.5f, scene: %s", SDL_GetTicks(), 
							operate_.near_fk.position.x - operate_.initial_fk.position.x, get_scene_desc(operate_.scene).name.c_str());
/*
						{
							const double desire_dcpitch = operate_.second_dcpitch;
							reach_dcpitch_use_PRP_joint("enter second dcpitch", desire_dcpitch);
						}
*/
					}

					if (is_normal_distance_state(operate_.scene) && !operate_.rtyaw.searched360) {
						const int no_object_threshold = near_threshold_;
						operate_.rtyaw.did_reach_near(no_object_threshold, msg);
					}
				}
			}

			if (operate_.goto_second_dcpitch_ticks != 0 && SDL_GetTicks() >= operate_.goto_second_dcpitch_ticks) {
				VALIDATE(!calibrate_require_near, null_str);
				VALIDATE(operate_.goto_near_ticks == 0, null_str);
				VALIDATE(!operate_.in_second_dcpitch, null_str);

				clear_second_dcpitch("enter");
				operate_.in_second_dcpitch = true;

				const double desire_dcpitch = operate_.second_dcpitch;
				reach_dcpitch_use_PRP_joint("enter second dcpitch", desire_dcpitch);
				msg = "enter second dcpitch";
			}

			return msg;
		}

		set_goto_near_ticks(false);
		set_goto_second_dcpitch_ticks(false);

		if (!operate_.rtyaw.searched360) {
			operate_.rtyaw.did_receive_object_frame(msg);

			if (!operate_.rtyaw.searched360) {
				return msg;
			}
		}

	} else if (!task_finished()) {
		return msg;
	}

	VALIDATE(operate_.rtyaw.searched360, null_str);

	if (require_wakeup_thread(state)) {
		VALIDATE(task_finished_, null_str);
		if (state != state_count - 1) {
			task_finished_ = false;
		}
	} else {
		VALIDATE(!task_finished_, null_str);
	}

	int next_state = nposm;
	if (state == state_distance || state == state_align) {
		VALIDATE(!require_wakeup_thread(state), null_str);

		VALIDATE(object_points_ptr != nullptr, null_str);
		if (!handle_distance(vsink2, *object_points_ptr, msg)) {
			return msg;
		}

		if (operate_.goto_near_ticks != 0) {
			VALIDATE(state == state_distance, null_str);
			operate_.goto_near_ticks = 0;
			SDL_Log("%u {goto_near_ticks}3 set 0, because enter state_ik", SDL_GetTicks());
		}

		if (operate_.nearing) {
			VALIDATE(state == state_distance, null_str);
			operate_.nearing = false;

			if (operate_.scene == scene_calibrate_near) {
				VALIDATE(!is_float_nposm(operate_.calibrate_near_PRP_diff.x), null_str);
				SDL_Log("%u {calibrate_near}calculated near's TP: (%.5f, %.5f, %.5f), result calibrate_near_TP_diff: (%.5f, %.5f, %.5f)", SDL_GetTicks(),
					operate_.TP.x, operate_.TP.y, operate_.TP.z, 
					operate_.calibrate_near_PRP_diff.x, operate_.calibrate_near_PRP_diff.y, operate_.calibrate_near_PRP_diff.z);
				ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);
				stop_operate();
				return msg;
			}
		}

		if (operate_.scene != scene_normal) {
			// At present, all non-normal scene require in state_distance always.
			
			// Make sure the next handle_distance tests MAX_DIST_SAMPLES samples
			reset_dist_samples();
			return msg;
		}

		if (state == state_distance) {
			if (!operate_.near_move_finished) {
				next_state = state_ik;

			} else {
				SDL_Log("{align}near move finished, moveit_theta: %.3f, dcamera_theta_y0: %.3f, min_abs_dcamera_theta_y0: %.3f",
					RAD2DEG(operate_.last_moveit_theta), RAD2DEG(operate_.last_dcamera_theta_y0), RAD2DEG(operate_.min_abs_dcamera_theta_y0));

				if (game_config::os == os_windows) {
					const std::string ik_arm_name = group_name_ik_arm;
					const geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name);

					const SDL_DPoint3& PRP = operate_.PRP;
					const SDL_DPoint3& TP = operate_.TP;
					SDL_DPoint dcamera_origin{curr_fk.position.x + -1 * PRP.x, RGB_diff_y_};

					const double moveit_theta = atan2(dcamera_origin.y + TP.y, dcamera_origin.x + TP.x);
					const double dcamera_theta_y0 = atan2(RGB_diff_y_ + TP.y, TP.x);
					VALIDATE(KDL::Equal(operate_.last_moveit_theta, moveit_theta), null_str);
					VALIDATE(KDL::Equal(operate_.last_dcamera_theta_y0, dcamera_theta_y0), null_str);
				}

				bool require_again = rotate_joint1_4_align("distance", operate_.last_moveit_theta, operate_.last_dcamera_theta_y0, msg);
				if (operate_.grasp) {
					// for grasp, the center-align's accuracy requirements can be relaxed.
					require_again = false;
				}

				if (require_again) {
					SDL_Log("goto state_align");
					next_state = state_align;

				} else {
					SDL_Log("skip state_align, goto state_claw");
					next_state = state_claw;
				}
			}

		} else if (state == state_align) {
			SDL_Log("claw align finished, goto state_claw");
			next_state = state_claw;
		}

	} else if (state == state_ik) {
		clear_second_dcpitch("ik finished");
		VALIDATE(!operate_.near_move_finished, null_str);

		if (operate_.ik_result.empty()) {
			if (operate_.ik_result.empty()) {
				double moveit_theta = fail_revert_to_initial_state();

				double dist = calculate_desire_move_dist();
				SDL_Log("ik fail, continue state_distance. near_move_finished: %s, desire move %.3fm, moveit_theta: %.3f", 
					operate_.near_move_finished? "true": "false", dist, RAD2DEG(moveit_theta));
				if (operate_.near_move_finished) {
					VALIDATE(false, null_str);
					SDL_Log("ik fail, set near_move_finished to false");
					operate_.near_move_finished = false;
				}

				move_public_vel(dist, moveit_theta, true, msg);

			} else {
				SDL_Log("%u {second_dcpitch}[1] ik success start second dcpitch ticks", SDL_GetTicks());
				VALIDATE(!operate_.in_second_dcpitch && operate_.goto_second_dcpitch_ticks == 0, null_str);
				set_goto_second_dcpitch_ticks(true);
			}
		}

		will_goto_state_distance();
		next_state = state_distance;

	} else if (state == state_claw) {
		// if operate_.stopping == true, don't enter it. is process by stop_operate() all time.
		VALIDATE(!operate_.stopping, null_str);

		if (operate_.claw_fail) {
			operate_.claw_fail = false;

			double moveit_theta = fail_revert_to_initial_state();
			msg = "calw fail, revert to initial state";

			will_goto_state_distance();
			next_state = state_distance;

		} else {
			next_state = state_grasp;
		}

	} else {
		VALIDATE(state == state_grasp, null_str);
	}

	VALIDATE(operate_.state == state, null_str);

	if (next_state != nposm) {
		operate_.state = next_state;
		if (require_wakeup_thread(next_state)) {
			task_event_.Set();
		}

	} else {
		stop_operate();
	}

	return msg;
}

void tmoveit_calculator::stop_operate()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(operate_.state != nposm, null_str);
	VALIDATE(!operate_.stopping, null_str);

	operate_.stopping = true;

	const uint32_t now = SDL_GetTicks();
	SDL_Log("%u {tmoveit_calculator::stop_operate}curr state: %i task_finished_: %s, use %u ms", 
		now, operate_.state, task_finished_? "true": "false", now - operate_.start_ticks);
	if (require_wakeup_thread(operate_.state)) {
		// set task_finished_ to true by DoWork only. 
		// Wait, until DoWork done task-self operator, and enter task_event_.Wait(...)
		while (!task_finished_) {
			SDL_Delay(100);
		}
		task_finished_ = false;

	}

	VALIDATE(!task_finished_, null_str);
	operate_.state = nposm;

	operate_.task_api->moveit->stop_task(*operate_.aplt, *operate_.cfg_task);
}

double tmoveit_calculator::calculate_desire_move_dist() const
{
	VALIDATE(operate_.ik_query.valid, null_str);
	VALIDATE(!operate_.near_move_finished, null_str);

	const double blind_x_threshold = 0.030;

	double left_x = operate_.ik_query.x;
	double right_x = left_x + blind_x_threshold;
	if (operate_.near_move_finished) {
		right_x = operate_.ik_query.x;
		left_x = right_x - blind_x_threshold;
	}
	VALIDATE(right_x > left_x, null_str);

	double middle_x = left_x + (right_x - left_x) / 2;

	double curr_z = operate_.ik_query.z;
	if (curr_z > 0.203) {
		int ii = 0;
	}

	// double moveit_ik_mid_x = calculate_ik_b_f_radius(curr_z);
	double moveit_ik_mid_x = calculate_ik_mid(curr_z);

	// +: move forward, more near
	// -: move backward, more away from
	return middle_x - moveit_ik_mid_x;
}

void tmoveit_calculator::will_goto_state_distance()
{
	VALIDATE(!operate_.nearing, null_str);

	reset_dist_samples();

	geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(group_name_ik_arm);
	const bool use_near = true;
	if (use_near && KDL::Equal(curr_fk.position.x, operate_.initial_fk.position.x, same_initial_fk_x_thresould_)) {
		set_goto_near_ticks(true);
		// operate_.goto_near_ticks = SDL_GetTicks() + near_threshold_;
		SDL_Log("%u {goto_near_ticks}1.1. goto state_distance, set ticks", SDL_GetTicks());
	} else {
		operate_.goto_near_ticks = 0;
		SDL_Log("%u {goto_near_ticks}1.2. goto state_distance, 0", SDL_GetTicks());
	}
}

void tmoveit_calculator::reset_dist_samples()
{
	SDL_Log("---reset_dist_samples---");

	next_RP_sample_index_ = 0;
	next_targrt_sample_index_ = 0;
	memset(dist_samples_, 0, sizeof(dist_samples_));
}

bool tmoveit_calculator::is_sample_stable(int type, const SDL_DPoint3& target_point)
{
	VALIDATE(MAX_DIST_SAMPLES >= 3, null_str);
	VALIDATE(type >= 0 && type < dcpoint_count, null_str);

	// const bool verbose = game_config::os == os_windows;
	const bool verbose = false;

	SDL_DPoint3* samples = dist_samples_[type];
	int& next_sample_index = type == dcpoint_RP? next_RP_sample_index_: next_targrt_sample_index_;

	SDL_DPoint3& cur_dist_sample = samples[next_sample_index];
	cur_dist_sample.x = target_point.x;
	cur_dist_sample.y = target_point.y;
	cur_dist_sample.z = target_point.z;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			type == dcpoint_RP? "RP": "target", next_sample_index);
		for (int at = 0; at < MAX_DIST_SAMPLES; at ++) {
			const SDL_DPoint3& dist = samples[at];
			SDL_Log("[%i/%i]dist: (%.6f, %.6f, %.6f)", at, MAX_DIST_SAMPLES, dist.x, dist.y, dist.z);
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= MAX_DIST_SAMPLES;

	const double threshold = 0.0025; // 2.5mm
	for (int at1 = 0; at1 < MAX_DIST_SAMPLES; at1 ++) {
		const SDL_DPoint3& dist1 = samples[at1];
		for (int at2 = at1 + 1; at2 < MAX_DIST_SAMPLES; at2 ++) {
			const SDL_DPoint3& dist2 = samples[at2];

			SDL_DPoint3 diff{fabs(dist1.x - dist2.x), fabs(dist1.y - dist2.y), fabs(dist1.y - dist2.y)};
			if (verbose) {
				SDL_Log("is_sample_stable: [%i] - [%i]: (%.4f, %.4f, %.4f)", at1, at2, diff.x, diff.y, diff.z);
			}
			if (diff.x > threshold || diff.y > threshold || diff.z > threshold) {
				if (verbose) {
					SDL_Log("is_sample_stable: [%i] - [%i]: (%.4f, %.4f, %.4f) > threshold(%.6f), return false", 
						at1, at2, diff.x, diff.y, diff.z, threshold);
				}
				return false;
			}
		}
	}

	return true;
}

void tmoveit_calculator::verbose_arm_error(const SDL_DPoint3& RP, const SDL_DPoint3& PRP, const SDL_DPoint3& TP, const geometry_msgs::Pose curr_fk)
{
	if (!is_float_nposm(verbose_last_PRP_.x)) {
		const geometry_msgs::Pose& second_fk = curr_fk;
		SDL_DPoint3 fk_diff{second_fk.position.x - verbose_last_fk_.position.x, 
			second_fk.position.y - verbose_last_fk_.position.y,
			second_fk.position.z - verbose_last_fk_.position.z};

		// first's TP_RP_diff
		SDL_DPoint3 first_TP_RP_diff{verbose_last_TP_.x - verbose_last_RP_.x, 
			verbose_last_TP_.y - verbose_last_RP_.y,
			verbose_last_TP_.z - verbose_last_RP_.z};

		// second's TP_RP_diff
		SDL_DPoint3 second_TP_RP_diff{TP.x - RP.x, 
			TP.y - RP.y,
			TP.z - RP.z};

		SDL_DPoint3 TP_RP_diff_diff{first_TP_RP_diff.x - second_TP_RP_diff.x,
			first_TP_RP_diff.y - second_TP_RP_diff.y,
			first_TP_RP_diff.z - second_TP_RP_diff.z};

		// first's TP_PRP_diff
		SDL_DPoint3 first_TP_PRP_diff{verbose_last_TP_.x - verbose_last_PRP_.x, 
			verbose_last_TP_.y - verbose_last_PRP_.y,
			verbose_last_TP_.z - verbose_last_PRP_.z};

		// second's TP_PRP_diff
		SDL_DPoint3 second_TP_PRP_diff{TP.x - PRP.x, 
			TP.y - PRP.y,
			TP.z - PRP.z};

		SDL_DPoint3 TP_PRP_diff_diff2{first_TP_PRP_diff.x - second_TP_PRP_diff.x,
			first_TP_PRP_diff.y - second_TP_PRP_diff.y,
			first_TP_PRP_diff.z - second_TP_PRP_diff.z};

		SDL_DPoint3 TP_PRP_diff_diff = calculate_TP_PRP_diff_diff(PRP, TP);

		VALIDATE(KDL::Equal(TP_PRP_diff_diff2.x, TP_PRP_diff_diff.x) && 
			KDL::Equal(TP_PRP_diff_diff2.y, TP_PRP_diff_diff.y) &&
			KDL::Equal(TP_PRP_diff_diff2.z, TP_PRP_diff_diff.z), null_str);

		SDL_Log("fk_diff: (%.6f, %.6f, %.6f) =<>= TP_PRP_diff_diff: (%.6f, %.6f, %.6f) [TP_RP_diff_diff: (%.6f, %.6f, %.6f)] joint1: %.3f", 
			fk_diff.x, fk_diff.y, fk_diff.z,
			TP_PRP_diff_diff.x, TP_PRP_diff_diff.y, TP_PRP_diff_diff.z,
			TP_RP_diff_diff.x, TP_RP_diff_diff.y, TP_RP_diff_diff.z,
			RAD2DEG(*joint1_position_));
	}

	verbose_last_RP_ = RP;
	verbose_last_PRP_ = PRP;
	verbose_last_TP_ = TP;
	verbose_last_fk_ = curr_fk;
}

double tmoveit_calculator::moveit_theta_from_joint1_value() const
{
	double curr_joint_value = *joint1_position_;
	double result = curr_joint_value - joint1_0degree_;
	if (joint1_axis_negative_) {
		result *= -1;
	}
	return result;
}

double tmoveit_calculator::zero_joint1_if_necessary()
{
	double curr_joint_value = *joint1_position_;

	if (KDL::Equal(curr_joint_value, joint1_0degree_)) {
		return float_nposm;
	}

	const double desire_joint_value = joint1_0degree_;
	ros_instance_.do_light_single_joint_target(joint_name_joint1, desire_joint_value);

	double rotated = joint1_0degree_ - curr_joint_value;
	if (joint1_axis_negative_) {
		rotated *= -1;
	}

	SDL_Log("%u {3rotate}{zero_joint1_if_necessary}rotated: %.3f",  SDL_GetTicks(), RAD2DEG(rotated));

	post_rotate_robot_or_joint1();

	// +: Turned 'rotated' degrees counterclockwise
	// -: Turned 'rotated' degrees clockwise
	return rotated;
}

bool tmoveit_calculator::rotate_joint1_by(double moveit_theta, double& desire_joint1_value)
{
	VALIDATE(moveit_theta != 0, null_str);
	const double curr_joint1_value = *joint1_position_;

	double joint1_diff_yaw = joint1_axis_negative_? -1 * moveit_theta: moveit_theta;
	desire_joint1_value = curr_joint1_value + joint1_diff_yaw;

	double joint_min_value = joint1_model_->min_position_;
	{
		// necessary?
		joint_min_value = joint1_0degree_;
	}
	if (desire_joint1_value < joint_min_value) {
		desire_joint1_value = joint_min_value;
	}
	if (desire_joint1_value > joint1_model_->max_position_) {
		desire_joint1_value = joint1_model_->max_position_;
	}

	if (KDL::Equal(desire_joint1_value, curr_joint1_value)) {
		return false;
	}

	ros_instance_.do_light_single_joint_target(joint_name_joint1, desire_joint1_value);
	return true;
}

bool tmoveit_calculator::rotate_joint1_if_necessary(const std::string& scene, const double moveit_theta, std::string& msg)
{
	if (!is_normal_distance_state(operate_.scene)) {
		return false;
	}

	const double curr_joint1_value = *joint1_position_;

	if (moveit_theta > no_rotate_joint1_range_.max) {
		// moveit_theta > no_rotate_joint1_range_.max]
		if (KDL::Equal(curr_joint1_value, joint1_0degree_)) {
			return false;
		}

	} else if (moveit_theta <= no_rotate_joint1_range_.max && moveit_theta >= no_rotate_joint1_range_.min) {
		// [no_rotate_joint1_range_.min, no_rotate_joint1_range_.max]
		return false;
	}

	double bonus_theta = 0;
/*
	if (KDL::Equal(curr_joint1_value, joint1_0degree_) && moveit_theta < 0) {
		const double max_bonus_theta = DEG2RAD(1);
		if (fabs(moveit_theta) < fabs(no_rotate_robot_range_.min)) {
			bonus_theta = fabs(moveit_theta) * max_bonus_theta / fabs(no_rotate_robot_range_.min);
		} else {
			bonus_theta = max_bonus_theta;
		}
	}
*/
	double moveit_theta2 = moveit_theta + bonus_theta;
	double desire_joint1_value;
	if (!rotate_joint1_by(moveit_theta2, desire_joint1_value)) {
		return false;
	}

	char buf[128];
	if (scene.empty()) {
		SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f, bonus: %.3f joint1 from %.3f to %.3f",
			RAD2DEG(moveit_theta), RAD2DEG(bonus_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));
	} else {
		SDL_snprintf(buf, sizeof(buf), "(%s)moveit_theta: %.3f, bonus: %.3f joint1 from %.3f to %.3f",
			scene.c_str(), RAD2DEG(moveit_theta), RAD2DEG(bonus_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));
	}
	msg = buf;

	SDL_Log("%u {3rotate}{rotate_joint1_if_necessary}(%s)moveit_theta: %.3f, bonus_theta: %.3f rotate_joint1 from %.3f to %.3f", 
		SDL_GetTicks(), scene.c_str(), RAD2DEG(moveit_theta), RAD2DEG(bonus_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));

	post_rotate_robot_or_joint1();
	return true;
}

bool tmoveit_calculator::rotate_joint1_4_align(const std::string& scene, double moveit_theta, double dcamera_theta_y0, std::string& msg)
{
	VALIDATE(operate_.state == state_distance || operate_.state == state_align, null_str);

	const double abs_dcamera_theta_y0 = fabs(dcamera_theta_y0);
	const double claw_align_threshold = DEG2RAD(0.25);
	if (abs_dcamera_theta_y0 < claw_align_threshold) {
		return false;
	}

	const double curr_joint1_value = *joint1_position_;
	// if (KDL::Equal(curr_joint1_value, joint1_0degree_)) {
	//	return false;
	// }

	if (abs_dcamera_theta_y0 >= operate_.min_abs_dcamera_theta_y0 + DEG2RAD(0.1)) {
		return false;
	}

	// make sure symobls are same. It should be rare.
	const double estimate_moveit_theta = dcamera_theta_y0 / 3;
	if (dcamera_theta_y0 > 0) {
		if (moveit_theta < 0) {
			SDL_Log("{rotate_joint1_4_align}change moveit_theta from %.3f to %.3f", RAD2DEG(moveit_theta), RAD2DEG(estimate_moveit_theta));
			moveit_theta = estimate_moveit_theta;
		}
	} else {
		VALIDATE(dcamera_theta_y0 < -1 * claw_align_threshold, null_str);
		if (moveit_theta > 0) {
			SDL_Log("{rotate_joint1_4_align}change moveit_theta from %.3f to %.3f", RAD2DEG(moveit_theta), RAD2DEG(estimate_moveit_theta));
			moveit_theta = estimate_moveit_theta;
		}
	}

	double desire_joint1_value;
	if (!rotate_joint1_by(moveit_theta, desire_joint1_value)) {
		return false;
	}

	if (abs_dcamera_theta_y0 < operate_.min_abs_dcamera_theta_y0) {
		// remember this abs_dcamera_theta_y0
		operate_.min_abs_dcamera_theta_y0 = abs_dcamera_theta_y0;
	}

	char buf[128];
	if (scene.empty()) {
		SDL_snprintf(buf, sizeof(buf), "dcamera_y0: %.3f, moveit_theta: %.3f, joint1 from %.3f to %.3f",
			RAD2DEG(dcamera_theta_y0), RAD2DEG(moveit_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));
	} else {
		SDL_snprintf(buf, sizeof(buf), "(%s)dcamera_y0: %.3f, moveit_theta: %.3f, joint1 from %.3f to %.3f",
			scene.c_str(), RAD2DEG(dcamera_theta_y0), RAD2DEG(moveit_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));
	}
	msg = buf;


	SDL_Log("%u {4rotate}{rotate_joint1_4_align}(%s)dcamera_theta_y0: %.3f, moveit_theta: %.3f, rotate_joint1 from %.3f to %.3f", 
		SDL_GetTicks(), scene.c_str(), RAD2DEG(dcamera_theta_y0), RAD2DEG(moveit_theta), RAD2DEG(curr_joint1_value), RAD2DEG(desire_joint1_value));

	post_rotate_robot_or_joint1();

	const double again_claw_align_threshold = DEG2RAD(0.8);
	if (abs_dcamera_theta_y0 < again_claw_align_threshold) {
		SDL_Log("{rotate_joint1_4_align}abs_dcamera_theta_y0(%.3f) < again_claw_align_threshold(%.3f), return false", 
			RAD2DEG(abs_dcamera_theta_y0), RAD2DEG(again_claw_align_threshold));
		return false;
	}

	return true;
}

bool tmoveit_calculator::rotate_robot_if_necessary(double moveit_theta, bool in_initial_fk, const SDL_DPoint& dcamera_origin, std::string& msg)
{
	const bool disable_rotate_robot = false; // only for debug. MUST not true when release.

	double abs_theta = fabs(moveit_theta);

	if (moveit_theta <= no_rotate_robot_range_.max && moveit_theta >= no_rotate_robot_range_.min) {
		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f is in [%.3f, %.3f]", 
			RAD2DEG(moveit_theta), RAD2DEG(no_rotate_robot_range_.max), RAD2DEG(no_rotate_robot_range_.min));
		msg = buf;
		return false;
	}

	if (!in_initial_fk && !operate_.nearing) {
		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f !in_initial_fk && !operate_.nearing", 
			RAD2DEG(moveit_theta));
		msg = buf;
		return false;
	}

	// if (!disable_rotate_robot) {
		if (moveit_theta > 0) {
			bool rotated = rotate_joint1_if_necessary("robot", moveit_theta, msg);
			if (rotated) {
				SDL_Log("because moveit_theta: %.3f isn't in [%.3f, %.3f] and moveit_theta > 0, perform rotate joint1 first", 
					RAD2DEG(moveit_theta), RAD2DEG(no_rotate_robot_range_.max), RAD2DEG(no_rotate_robot_range_.min));
				return true;
			}
		}

		double joint1_rotated = zero_joint1_if_necessary();
		if (!is_float_nposm(joint1_rotated)) {
			char buf[128];
			SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f joint1 isn't 0, rotate to 0", 
				RAD2DEG(moveit_theta));
			msg = buf;

			SDL_Log("because moveit_theta: %.3f isn't in [%.3f, %.3f], joint1 isn't 0, rotate to 0, joint1_rotated: %.3f", 
				RAD2DEG(moveit_theta), RAD2DEG(no_rotate_robot_range_.max), RAD2DEG(no_rotate_robot_range_.min), RAD2DEG(joint1_rotated));
			return true;
		}
	// }

	double diff_yaw = moveit_theta;

	double curr_yaw = ros_instance_.get_imu_rpy().yaw;
	double desire_yaw = curr_yaw + diff_yaw; 

	// operate_.rtyaw.goal = desire_yaw;

	// If the rotation is too large, I am afraid that it will cause the robot to concuss.
	double min_yaw = DEG2RAD(6);
	double max_yaw = DEG2RAD(25);

	if (abs_theta < min_yaw) {
		diff_yaw = moveit_theta > 0? min_yaw: -1 * min_yaw;

	} else if (abs_theta > max_yaw) {
		const double use_yaw = max_yaw;
		diff_yaw = moveit_theta > 0? use_yaw: -1 * use_yaw;
	}

	char buf[128];
	double twist[3] = {0, 0, diff_yaw};
	bool rotate = true;
	if (!disable_rotate_robot && is_normal_distance_state(operate_.scene)) {
		ros_instance_.public_vel(twist[0], twist[1], twist[2]);

		SDL_Log("{3rotate}{rotate_to_yaw}set goal: %.3f, curr_yaw: %.3f, pulbic vel: (%.5f, %.3f, %.3f)", 
			RAD2DEG(desire_yaw), RAD2DEG(curr_yaw), twist[0], twist[1], RAD2DEG(twist[2]));

		operate_.rtyaw.start_one_rotate(desire_yaw, move_robot_threshold_);

		SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f vel: (%.5f, %.3f, %.3f)", 
			RAD2DEG(moveit_theta), twist[0], twist[1], RAD2DEG(twist[2]));

		post_rotate_robot_or_joint1();

	} else {
		SDL_snprintf(buf, sizeof(buf), "moveit_theta: %.3f don't vel: (%.5f, %.3f, %.3f)", 
			RAD2DEG(moveit_theta), twist[0], twist[1], RAD2DEG(twist[2]));

		rotate = false;
	}
	msg = buf;

	return rotate;
}

void tmoveit_calculator::post_rotate_robot_or_joint1()
{
	// robote robot/joint1 will take some time, don't let it go into near_state,
	set_goto_near_ticks(false);
	set_goto_second_dcpitch_ticks(false);

	// The robot/joint1 is rotated at an angle, 
	// Make sure the next handle_distance tests MAX_DIST_SAMPLES samples.
	// If you only look at the next sample, it may not be accurate.
	reset_dist_samples();
}

bool tmoveit_calculator::handle_distance(tdepthcapture::VideoRenderer2& vsink2, const std::vector<SDL_2Point>& object_points, std::string& msg)
{
	VALIDATE(!require_wakeup_thread(operate_.state), null_str);
	if (operate_.state == state_distance) {
		VALIDATE(!operate_.near_move_finished, null_str);
	} else {
		VALIDATE(operate_.near_move_finished, null_str);
	}

	std::vector<SDL_2Point> color_points = object_points;

	SDL_Rect reference_rects[2];
		
	bool ret = find_reference_point(vsink2.last_deliver_d2c_.argb_mat, vsink2.last_deliver_d2c_.depth_data, 
		vsink2.last_deliver_d2c_.depth_scale, reference_rects);

	reference_rects_.clear();
	for (int at = 0; at < 2 && ret; at ++) {
		const SDL_Rect& src = reference_rects[at];
		// SDL_Rect rect{video_dst.x + src.x * xratio, video_dst.y + src.y * yratio, src.w, src.h};
		// render_rect_frame(renderer, rect, 0xffff0000, 1);

		reference_rects_.push_back(src);
		color_points.push_back(SDL_2Point{src.x + src.w / 2, src.y + src.h / 2, nposm, nposm});
	}

	std::vector<SDL_Point3> depth4 = calculate(vsink2.last_deliver_d2c_.depth_data, 
		vsink2.last_deliver_d2c_.depth_scale, color_points);

	// if (game_config::os == os_windows && color_points.size() == 4 && color_points.size() != object_points.size() + 2) {
	//	SDL_Log("color_points.size()(%i) != object_points.size()(%i) + 2 {points}[0](%i, %i) [1](%i, %i) [2](%i, %i) [3](%i, %i)", 
	//		(int)color_points.size(), (int)object_points.size(),
	//		color_points[0].x1, color_points[0].y1, color_points[0].x2, color_points[0].y2, 
	//		color_points[1].x1, color_points[1].y1, color_points[1].x2, color_points[1].y2, 
	//		color_points[2].x1, color_points[2].y1, color_points[2].x2, color_points[2].y2, 
	//		color_points[3].x1, color_points[3].y1, color_points[3].x2, color_points[3].y2);
	//	save_dbg_d2c(vsink2.last_deliver_d2c_.argb_mat, vsink2.last_deliver_d2c_.depth_data, vsink2.last_deliver_d2c_.depth_scale, false);
	// }

	if (color_points.size() != object_points.size() + 2) {
		SDL_Log("color_points.size()(%i) != object_points.size()(%i) + 2", (int)color_points.size(), (int)object_points.size());
		return false;
	}

	const int lrp_idx = object_points.size();
	const SDL_Point3 ref_camera_xyz{(depth4[lrp_idx].x + depth4[lrp_idx + 1].x) / 2, (depth4[lrp_idx].y + depth4[lrp_idx + 1].y) / 2, (depth4[lrp_idx].z + depth4[lrp_idx + 1].z) / 2};
	const SDL_Point3 target_camera_xyz = depth4[0];

	if (target_camera_xyz.x == Y16_NO_DEPTH || depth4[2].x == Y16_NO_DEPTH) {
		SDL_Log("target_camera_xyz.x == Y16_NO_DEPTH || depth4[2].x == Y16_NO_DEPTH, target_camera_xyz: (%i, %i, %i) depth4[2].x: %i", target_camera_xyz.x, target_camera_xyz.y, target_camera_xyz.z, depth4[2].x);
/*
		if (game_config::os == os_windows) {
			SDL_Log("color_points.size()(%i) != object_points.size()(%i) + 2 {points}[0](%i, %i)(%i, %i) [1](%i, %i)(%i, %i)", 
				(int)color_points.size(), (int)object_points.size(),
				color_points[0].x1, color_points[0].y1, color_points[0].x2, color_points[0].y2, 
				color_points[1].x1, color_points[1].y1, color_points[1].x2, color_points[1].y2);
			save_dbg_d2c(vsink2.last_deliver_d2c_.argb_mat, vsink2.last_deliver_d2c_.depth_data, vsink2.last_deliver_d2c_.depth_scale);
		}
*/
		return false;
	}

	SDL_DPoint3 ref_map_xyz{ref_camera_xyz.z / 1000.0, ref_camera_xyz.x / -1000.0, ref_camera_xyz.y / -1000.0};
	SDL_DPoint3 target_map_xyz{target_camera_xyz.z / 1000.0, target_camera_xyz.x / -1000.0, target_camera_xyz.y / -1000.0};

	SDL_DPoint3 t_out_ref, t_out_target;

	// dcpitch = ros_instance_.get_stable_imu(rpy_pitch, false);
	// below has is_sample_stable(...), not necessary to use ros_instance_.get_stable_imu(...).
	const trpy& rpy = ros_instance_.get_imu_rpy();
	VALIDATE(rpy.valid, null_str);
	double dcpitch = rpy.pitch;

	tf2::Quaternion q;
	q.setRPY(0, dcpitch, 0);
	const tf2::Transform transform(q);

	tf2::doTransform_position(transform, ref_map_xyz, t_out_ref);
	tf2::doTransform_position(transform, target_map_xyz, t_out_target);

	if (t_out_target.x < -10) {
		SDL_Log("target(%.6f, %.6f, %.6f) No valid depth", t_out_target.x, t_out_target.y, t_out_target.z);
		return false;
	}

	SDL_Log("%u {handle_distance}{%s}pitch: %.6f(deg: %.4f) target(%.6f, %.6f, %.6f) - ref: (%.6f, %.6f, %.6f)",
		SDL_GetTicks(), operate_.state == state_distance? "state_distance": "state_align", dcpitch, RAD2DEG(dcpitch), 
		t_out_target.x, t_out_target.y, t_out_target.z,
		t_out_target.x - t_out_ref.x,
		t_out_target.y - t_out_ref.y,
		t_out_target.z - t_out_ref.z);

	SDL_DPoint3 diff{t_out_target.x - t_out_ref.x, 
		t_out_target.y - t_out_ref.y,
		t_out_target.z - t_out_ref.z};

	// average
	bool RP_stable = is_sample_stable(dcpoint_RP, t_out_ref);
	bool target_stable = is_sample_stable(dcpoint_target, t_out_target);
	if (!RP_stable || !target_stable) {
		SDL_Log("!RP_stable(%s) || !target_stable(%s)", RP_stable? "true": "false", target_stable? "true": "false");
		return false;
	}

	const SDL_DPoint3 RP{t_out_ref.x, t_out_ref.y, t_out_ref.z};
	const SDL_DPoint3 PRP = RP_2_PRP(RP, dcpitch, PRP_offset_, PRP_2offset_);
	// SDL_DPoint3 TP{t_out_target.x, t_out_target.y, t_out_target.z};

	SDL_DPoint3 center_map_xyz2;
	// SDL_DPoint3 center_map_xyz3;

		// VALIDATE(object_points.size() == 4, null_str);

		std::vector<SDL_DPoint3> map_xyz2;
		SDL_DPoint3 pt_map_xyz2;
		for (int at = 0; at < (int)object_points.size(); at ++) {
			const SDL_Point3& camera_xyz = depth4[at];

			SDL_DPoint3 pt_map_xyz{camera_xyz.z / 1000.0, camera_xyz.x / -1000.0, camera_xyz.y / -1000.0};
			tf2::doTransform_position(transform, pt_map_xyz, pt_map_xyz2);
			map_xyz2.push_back(pt_map_xyz2);
		}

		center_map_xyz2 = operate_.task_api->moveit->calculate_TP(vsink2.last_deliver_d2c_.argb_mat, dcpitch, object_points, &map_xyz2[0]);
/*
		center_map_xyz3.x = (map_xyz2[0].x + map_xyz2[2].x) / 2;
		center_map_xyz3.y = (map_xyz2[0].y + map_xyz2[2].y) / 2;
		center_map_xyz3.z = (map_xyz2[0].z + map_xyz2[2].z) / 2;

	SDL_DPoint3 center_map_diff{center_map_xyz2.x - center_map_xyz3.x, center_map_xyz2.y - center_map_xyz3.y, center_map_xyz2.z - center_map_xyz3.z};
	SDL_Log("{dbg_center}center_map_xyz2: (%.6f, %.6f, %.6f) - center_map_xyz2: (%.6f, %.6f, %.6f) = (%.6f, %.6f, %.6f)",
		center_map_xyz2.x, center_map_xyz2.y, center_map_xyz2.z,
		center_map_xyz3.x, center_map_xyz3.y, center_map_xyz3.z,
		center_map_diff.x, center_map_diff.y, center_map_diff.z);
	VALIDATE(fabs(center_map_diff.x) < 0.001 && fabs(center_map_diff.y) < 0.001 && fabs(center_map_diff.z) < 0.001, null_str);
*/
	SDL_DPoint3 TP{center_map_xyz2.x, center_map_xyz2.y, center_map_xyz2.z};
/*
	if (operate_.adjust_TP) {
		// SDL_DPoint3 std_offset{-0.065, 0, -0.100};
		// SDL_DPoint3 std_offset{-0.060, 0, -0.100};
		// SDL_DPoint3 std_offset{-0.065, 0, -0.095};
		SDL_DPoint3 std_offset{-0.063, 0, -0.098};

		SDL_DPoint3 offset2 = std_offset;

		double x_theta = atan2(object_points[2].x1 - object_points[3].x1, object_points[2].y1 - object_points[3].y1);
		double x_theta2 = atan2(object_points[1].x1 - object_points[0].x1, object_points[1].y1 - object_points[0].y1);
		double avg_theta = (x_theta + x_theta2) / 2;

		SDL_DPoint3 result = transform_offset(avg_theta, std_offset);
		SDL_Log("((%i, %i), (%i, %i), (%i, %i), (%i, %i)), theta: %.2f, theta2: %.2f, avg_theta: %.2f, result: (%.6f, %.6f, %.6f)", 
			object_points[0].x1, object_points[0].y1, object_points[1].x1, object_points[1].y1, 
			object_points[2].x1, object_points[2].y1, object_points[3].x1, object_points[3].y1, 
			RAD2DEG(x_theta), RAD2DEG(x_theta2), RAD2DEG(avg_theta),
			result.x, result.y, result.z);

		bool use_transform = true;
		if (use_transform) {
			offset2 = result;
		}

		TP.x += offset2.x;
		TP.y += offset2.y;
		TP.z += offset2.z;
	}
*/
	const double TP_PRP_x_diff = TP.x - PRP.x;

	SDL_Log("TP: (%.6f, %.6f, %.6f), ref_camera_xyz: (%i, %i, %i) target_camera_xyz: (%i, %i, %i)", 
		TP.x, TP.y, TP.z, ref_camera_xyz.x, ref_camera_xyz.y, ref_camera_xyz.z, target_camera_xyz.x, target_camera_xyz.y, target_camera_xyz.z);

	if (operate_.m4ik.confirming()) {
		VALIDATE(operate_.state == state_distance, null_str);
		// In order to avoid misjudgment, it is still necessary to have a stable TP and RP. 
		// Don't be afraid to find TP, it has been proven that TP can be found in the initial or near state.
		if (game_config::os == os_windows) {
			// before call 'move_public_vel', will make sure moveit is in state_recognize or state_recognize_near.
			VALIDATE(KDL::Equal(*joint1_position_, joint1_0degree_), null_str);
		}

		// depth cameras have random diff
		// Even with a large velocity, like linear_x = 0.10, robot may produce only a small velocity, like 0.01.
		// because operate_.m4ik.move_dist is 'first' dist, so moved_threshold don't use large vel.
		double diff_x = TP_PRP_x_diff;
		if (operate_.nearing) {
			VALIDATE(!is_float_nposm(operate_.near_fk.position.x), null_str);
			diff_x += operate_.near_fk.position.x - operate_.initial_fk.position.x;
		}
		const double moved_threshold = 0.01;
		if (KDL::Equal(operate_.m4ik.TP_PRP_diff_x, diff_x, moved_threshold)) {
			SDL_Log("%u ik fail{moving_4_ik}nearing: %s, Equal(TP_RP_x_diff:%.5f, diff_x: %.5f = res: %.5f) is true, think no move, public vel again",
				SDL_GetTicks(), operate_.nearing? "true": "false", operate_.m4ik.TP_PRP_diff_x, diff_x, operate_.m4ik.TP_PRP_diff_x - diff_x);
			move_public_vel(operate_.m4ik.move_dist, operate_.m4ik.moveit_theta, false, msg);
			set_goto_near_ticks(false);
			// if (operate_.goto_near_ticks != 0) {
				// avoid do recgnize_near as soon as over m4ik.stop_ticks, and extend the waiting time.
			//	operate_.goto_near_ticks = SDL_GetTicks() + near_threshold_;
			//	SDL_Log("%u {goto_near_ticks}2.3 handle_distance, set future ticks again", SDL_GetTicks());
			// }
			return false;
		} else {
			SDL_Log("%u ik fail{moving_4_ik}nearing: %s, Equal(TP_RP_x_diff:%.5f, diff_x: %.5f = res: %.5f) is false, clear m4ik",
				SDL_GetTicks(), operate_.nearing? "true": "false", operate_.m4ik.TP_PRP_diff_x, diff_x, operate_.m4ik.TP_PRP_diff_x - diff_x);
			operate_.m4ik.clear();
		}
	}

	const std::string ik_arm_name = group_name_ik_arm;
	const geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name);

	const bool in_initial_fk = KDL::Equal(curr_fk.position.x, operate_.initial_fk.position.x, same_initial_fk_x_thresould_);

	// const double RGB_diff_y = 0.011; // between dcamera's RGB and center. 0.011/0.006/0.016
	SDL_DPoint dcamera_origin{curr_fk.position.x + -1 * PRP.x, RGB_diff_y_};

	const double moveit_theta = atan2(dcamera_origin.y + TP.y, dcamera_origin.x + TP.x);
	const double dcamera_theta_y0 = atan2(RGB_diff_y_ + TP.y, TP.x);
	operate_.last_moveit_theta = moveit_theta;
	operate_.last_dcamera_theta_y0 = dcamera_theta_y0;
	SDL_Log("{rotate_to_yaw}center_point: (%.4f, %.4f, %.4f), operate_.nearing: %s, joint1's moveit_theta: %.3f, moveit_theta: %.3f, dcamera_theta_y0: %.3f",
		TP.x, TP.y, TP.z, operate_.nearing? "true": "false",
		RAD2DEG(moveit_theta_from_joint1_value()), RAD2DEG(moveit_theta), RAD2DEG(dcamera_theta_y0));

	if (operate_.state == state_distance) {
		if (rotate_robot_if_necessary(moveit_theta, in_initial_fk, dcamera_origin, msg)) {
			return false;
		}

		if ((in_initial_fk || operate_.nearing) && KDL::Equal(*joint1_position_, joint1_0degree_)) {
			operate_.initial_fk_TP_PRP_diff_x = TP_PRP_x_diff;
			if (operate_.nearing) {
				operate_.initial_fk_TP_PRP_diff_x += operate_.near_fk.position.x - operate_.initial_fk.position.x;
			}
			// SDL_Log("%u ik fail{moving_4_ik}nearing: %s joint1_value: %.6f operate_.initial_fk_TP_PRP_diff_x = %.5f", 
			//	SDL_GetTicks(), operate_.nearing? "true": "false", *joint1_position_, operate_.initial_fk_TP_PRP_diff_x);
		}

		if (rotate_joint1_if_necessary(null_str, moveit_theta, msg)) {
			return false;
		}

	} else {
		VALIDATE(operate_.state == state_align, null_str);
		SDL_Log("{align}atan2(%.4f(RGB_y) + %.4f, %.4f) => dcamera_theta_y0: %.3f, min_abs_dcamera_theta_y0: %.3f", 
			RGB_diff_y_, TP.y, TP.x, RAD2DEG(dcamera_theta_y0), RAD2DEG(operate_.min_abs_dcamera_theta_y0));
		if (rotate_joint1_4_align(null_str, moveit_theta, dcamera_theta_y0, msg)) {
			return false;
		}
	}

	double roll, pitch, yaw;
	tf2::getEulerYPR(curr_fk.orientation, yaw, pitch, roll);

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "{t:(%.6f, %.6f, %.6f) q:(%.6f(deg:%.5f), %.6f(deg:%.5f), %.6f(deg:%.5f))}", 
		curr_fk.position.x, curr_fk.position.y, curr_fk.position.z,
		roll, RAD2DEG(roll), pitch, RAD2DEG(pitch), yaw, RAD2DEG(yaw));
	SDL_Log("{dbg_ik}(#1)curr_fk: %s", buf);

	// const SDL_DPoint3 RP{t_out_ref.x, t_out_ref.y, t_out_ref.z};
	operate_.RP = RP;

	if (operate_.scene == scene_calibrate_near && operate_.nearing) {
		VALIDATE(is_float_nposm(operate_.calibrate_near_PRP_diff.x), null_str);
		operate_.calibrate_near_PRP_diff.x = (TP.x - PRP.x) - (operate_.TP.x - operate_.PRP.x);
		operate_.calibrate_near_PRP_diff.y = (TP.y - PRP.y) - (operate_.TP.y - operate_.PRP.y);
		operate_.calibrate_near_PRP_diff.z = (TP.z - PRP.z) - (operate_.TP.z - operate_.PRP.z);
	}

	operate_.PRP = PRP;
	operate_.TP = TP;

	verbose_arm_error(RP, PRP, TP, curr_fk);

	if (operate_.state == state_distance) {
		set_ik(PRP_offset_, operate_.RP, operate_.PRP, operate_.TP);
	}

	return true;
}

double tmoveit_calculator::find_best_dcpitch_4_grasp(double curr_dcpitch, const SDL_DPoint3& PRP, const SDL_DPoint3& RP_offset, const SDL_DPoint3& PRP_model_center, 
	const SDL_DPoint3& TP, const SDL_DSize3& target_size)
{
	double min_pitch_deg = 20; // -45
	double max_pitch_deg = 70; // 80

	const SDL_DPoint3 target_top_e{TP.x + target_size.l / 2, TP.y, TP.z};

	geometry_msgs::Pose t_in;
	tf2::toMsg(tf2::Transform::getIdentity(), t_in);

	t_in.position.x = PRP_model_center.x;
	t_in.position.y = PRP_model_center.y;
	t_in.position.z = PRP_model_center.z;

	geometry_msgs::Pose t_in2;
	tf2::toMsg(tf2::Transform::getIdentity(), t_in2);
	t_in2.position.x = RP_offset.x;
	t_in2.position.y = RP_offset.y;
	t_in2.position.z = RP_offset.z;

	geometry_msgs::TransformStamped transform;
	transform.transform.translation.x = 0;
	transform.transform.translation.y = 0;
	transform.transform.translation.z = 0;

	tf2::Quaternion q;

	double best_pitch = float_nposm;
	double best_r = 0;
	SDL_DPoint3 best_diff{float_nposm, float_nposm, float_nposm};

	SDL_Log("---model_center: (%.6f, %.6f, %.6f) PRP: (%.6f, %.6f, %.6f) target_top_e: (%.6f, %.6f, %.6f)---", 
		PRP_model_center.x, PRP_model_center.y, PRP_model_center.z, 
		PRP.x, PRP.y, PRP.z, target_top_e.x, target_top_e.y, target_top_e.z);
	for (double pitch_deg = min_pitch_deg; pitch_deg < max_pitch_deg; pitch_deg += 0.5) {
		double pitch = DEG2RAD(pitch_deg);

		q.setRPY(0, pitch, 0);
		transform.transform.rotation.x = q.x();
		transform.transform.rotation.y = q.y();
		transform.transform.rotation.z = q.z();
		transform.transform.rotation.w = q.w();

		geometry_msgs::Pose t_out;
		tf2::doTransform(t_in, t_out, transform);

		geometry_msgs::Pose t_out2;
		tf2::doTransform(t_in2, t_out2, transform);

		SDL_DPoint3 tmp{PRP.x + t_out.position.x, PRP.y + t_out.position.y, PRP.z + t_out.position.z};

		SDL_DPoint3 diff{tmp.x - target_top_e.x, tmp.y - target_top_e.y, tmp.z - target_top_e.z};
		SDL_DPoint3 abs_diff{fabs(diff.x), fabs(diff.y), fabs(diff.z)};

		SDL_Log("pitch: %.6f(deg:%.3f) offset: (%.6f, %.6f, %.6f) tmp: (%.6f, %.6f, %.6f) diff: (%.6f, %.6f, %.6f)", 
			pitch, RAD2DEG(pitch), PRP.x + t_out2.position.x, PRP.y + t_out2.position.y, PRP.z + t_out2.position.z,
		 	tmp.x, tmp.y, tmp.z, diff.x, diff.y, diff.z);

		double z_error_threshold = 0.005;
		if (diff.z < 0 && abs_diff.z >= target_size.h - 0.005) {
			SDL_Log("Because z_error_threshold, ignore");
			continue;
		}

		// if (diff.z > 0 || abs_diff.z < target_size.height / 3) {
		if (diff.z > 0 || abs_diff.z < 0.001) {
			SDL_Log("Because diff.z > 0 || abs_diff.z < target_size.height / 3");
			continue;
		}

		if (diff.x < 0 && abs_diff.x >= target_size.l - 0.005) {
			SDL_Log("Because diff.x < 0 && abs_diff.x >= target_size.l - 0.005, ignore");
			// continue;
		}

		double r = float_nposm;
		// if (abs_diff.x < 0.001) {
		if (KDL::Equal(pitch, DEG2RAD(0), DEG2RAD(3))) {
			SDL_Log("think pitch is 0, r = abs_diff.x");
			r = abs_diff.x;

		} else if (KDL::Equal(pitch, DEG2RAD(90), DEG2RAD(3))) {
			SDL_Log("think pitch is 90, r = abs_diff.z");
			r = abs_diff.z;

		} else {
			VALIDATE(PRP.x < target_top_e.x, null_str);
			double delta_x = diff.x > 0? 0: -1 * diff.x;
			double tan_x = abs_diff.z / tan(pitch);

			if (delta_x + tan_x <= target_size.l) {
				r = abs_diff.z / sin(pitch);
				double r2 = tan_x / cos(pitch);
				SDL_Log("1.1 delta_x(%.6f) + tan_x(%.6f) <= size.l(%.3f), use sin, r: %.6f r2: %.6f", 
					delta_x, tan_x, target_size.l, r, r2);

			} else {
				r = (target_size.l - delta_x) / cos(pitch);
				SDL_Log("1.2 delta_x(%.6f) + tan_x(%.6f) > size.l(%.3f), use cos, r: %.6f", delta_x, tan_x, target_size.l, r);
			}
		}
		VALIDATE(!is_float_nposm(r), null_str);

		// SDL_Log("pitch: %.6f(deg:%.3f) offset: (%.6f, %.6f, %.6f) tmp: (%.3f, %.3f, %.3f) diff: (%.6f, %.6f, %.6f)", 
		//	pitch, RAD2DEG(pitch), PRP.x + t_out2.position.x, PRP.y + t_out2.position.y, PRP.z + t_out2.position.z,
		//	tmp.x, tmp.y, tmp.z, diff.x, diff.y, diff.z);

		if (r > best_r) {
			best_r = r;

			best_diff.x = diff.x;
			best_diff.y = diff.y;
			best_diff.z = diff.z;

			best_pitch = pitch;
		}
	}
	SDL_Log("------{find_best_dcpitch_4_grasp}best_pitch: %.6f(deg:%.3f) best_diff: (%.3f, %.3f, %.3f)", best_pitch, RAD2DEG(best_pitch), 
			best_diff.x, best_diff.y, best_diff.z);

	return best_pitch;
}

double tmoveit_calculator::find_best_dcpitch_4_press(double curr_dcpitch, const SDL_DPoint3& PRP, const SDL_DPoint3& RP_offset, const SDL_DPoint3& PRP_model_center, 
	const SDL_DPoint3& TP, const SDL_DSize3& target_size)
{
	double min_pitch_deg = 20; // -45
	double max_pitch_deg = 70; // 80

	geometry_msgs::Pose t_in;
	tf2::toMsg(tf2::Transform::getIdentity(), t_in);

	t_in.position.x = PRP_model_center.x;
	t_in.position.y = PRP_model_center.y;
	t_in.position.z = PRP_model_center.z;

	geometry_msgs::Pose t_in2;
	tf2::toMsg(tf2::Transform::getIdentity(), t_in2);
	t_in2.position.x = RP_offset.x;
	t_in2.position.y = RP_offset.y;
	t_in2.position.z = RP_offset.z;

	geometry_msgs::TransformStamped transform;
	transform.transform.translation.x = 0;
	transform.transform.translation.y = 0;
	transform.transform.translation.z = 0;

	tf2::Quaternion q;

	double best_pitch = float_nposm;
	double best_x = INT_MAX;
	SDL_DPoint3 best_diff{float_nposm, float_nposm, float_nposm};

	SDL_Log("---model_center: (%.6f, %.6f, %.6f) PRP: (%.6f, %.6f, %.6f) TP: (%.6f, %.6f, %.6f)---", 
		PRP_model_center.x, PRP_model_center.y, PRP_model_center.z, 
		PRP.x, PRP.y, PRP.z, TP.x, TP.y, TP.z);
	for (double pitch_deg = min_pitch_deg; pitch_deg < max_pitch_deg; pitch_deg += 0.5) {
		double pitch = DEG2RAD(pitch_deg);

		q.setRPY(0, pitch, 0);
		transform.transform.rotation.x = q.x();
		transform.transform.rotation.y = q.y();
		transform.transform.rotation.z = q.z();
		transform.transform.rotation.w = q.w();

		geometry_msgs::Pose t_out;
		tf2::doTransform(t_in, t_out, transform);

		geometry_msgs::Pose t_out2;
		tf2::doTransform(t_in2, t_out2, transform);

		SDL_DPoint3 tmp{PRP.x + t_out.position.x, PRP.y + t_out.position.y, PRP.z + t_out.position.z};

		SDL_DPoint3 diff{tmp.x - TP.x, tmp.y - TP.y, tmp.z - TP.z};
		SDL_DPoint3 abs_diff{fabs(diff.x), fabs(diff.y), fabs(diff.z)};

		double r = hypot(diff.x, diff.z);
		SDL_Log("pitch: %.6f(deg:%.3f) offset: (%.6f, %.6f, %.6f) tmp: (%.6f, %.6f, %.6f) diff: (%.6f, %.6f, %.6f) r: %.6f", 
			pitch, RAD2DEG(pitch), PRP.x + t_out2.position.x, PRP.y + t_out2.position.y, PRP.z + t_out2.position.z,
		 	tmp.x, tmp.y, tmp.z, diff.x, diff.y, diff.z, r);
/*
		double z_error_threshold = 0.005;
		if (diff.z < 0 && abs_diff.z >= target_size.h - 0.005) {
			SDL_Log("Because z_error_threshold, ignore");
			continue;
		}

		// if (diff.z > 0 || abs_diff.z < target_size.height / 3) {
		if (diff.z > 0 || abs_diff.z < 0.001) {
			SDL_Log("Because diff.z > 0 || abs_diff.z < target_size.height / 3");
			continue;
		}

		if (diff.x < 0 && abs_diff.x >= target_size.l - 0.005) {
			SDL_Log("Because diff.x < 0 && abs_diff.x >= target_size.l - 0.005, ignore");
			// continue;
		}

		double r = float_nposm;
		// if (abs_diff.x < 0.001) {
		if (KDL::Equal(pitch, DEG2RAD(0), DEG2RAD(3))) {
			SDL_Log("think pitch is 0, r = abs_diff.x");
			r = abs_diff.x;

		} else if (KDL::Equal(pitch, DEG2RAD(90), DEG2RAD(3))) {
			SDL_Log("think pitch is 90, r = abs_diff.z");
			r = abs_diff.z;

		} else {
			VALIDATE(PRP.x < target_center.x, null_str);
			double delta_x = diff.x > 0? 0: -1 * diff.x;
			double tan_x = abs_diff.z / tan(pitch);

			if (delta_x + tan_x <= target_size.l) {
				r = abs_diff.z / sin(pitch);
				double r2 = tan_x / cos(pitch);
				SDL_Log("1.1 delta_x(%.6f) + tan_x(%.6f) <= size.l(%.3f), use sin, r: %.6f r2: %.6f", 
					delta_x, tan_x, target_size.l, r, r2);

			} else {
				r = (target_size.l - delta_x) / cos(pitch);
				SDL_Log("1.2 delta_x(%.6f) + tan_x(%.6f) > size.l(%.3f), use cos, r: %.6f", delta_x, tan_x, target_size.l, r);
			}
		}
*/
		VALIDATE(!is_float_nposm(r), null_str);

		// SDL_Log("pitch: %.6f(deg:%.3f) offset: (%.6f, %.6f, %.6f) tmp: (%.3f, %.3f, %.3f) diff: (%.6f, %.6f, %.6f)", 
		//	pitch, RAD2DEG(pitch), PRP.x + t_out2.position.x, PRP.y + t_out2.position.y, PRP.z + t_out2.position.z,
		//	tmp.x, tmp.y, tmp.z, diff.x, diff.y, diff.z);

		// if (r < best_x) {
		if (abs_diff.x < best_x && (diff.z < 0 && abs_diff.z < 0.002)) {
			best_x = abs_diff.x;

			best_diff.x = diff.x;
			best_diff.y = diff.y;
			best_diff.z = diff.z;

			best_pitch = pitch;
		}
	}
	SDL_Log("------{find_best_dcpitch_4_press}best_pitch: %.6f(deg:%.3f) best_diff: (%.6f, %.6f, %.6f) best_x: %.6f", best_pitch, RAD2DEG(best_pitch), 
			best_diff.x, best_diff.y, best_diff.z, best_x);

	return best_pitch;
}

double tmoveit_calculator::calculate_ik_mid(double desire_z) const
{
	const trsp_moveit2& rsp_moveit = ros_instance_.rsp_of_moveit_model();
	const trsp_ikmid* ikmids = rsp_moveit.ikmids;
			
	// Of course, to speed up, you can use dichotomy method.
	int upper_index = 0;
	for (; upper_index < rsp_moveit.ikmid_count; upper_index ++) {
		if (ikmids[upper_index].z > desire_z) {
			break;
		}
	}

	int lower_index = upper_index - 1;
	VALIDATE(lower_index >= 0, null_str);

	const trsp_ikmid& lower_item = rsp_moveit.ikmids[lower_index];
	const trsp_ikmid& upper_item = rsp_moveit.ikmids[upper_index];

	double delta_gap = desire_z - lower_item.z;
	double segment_gap = upper_item.z - lower_item.z;

	double segment_mid = upper_item.mid - lower_item.mid;
	double delta_mid = 0;
	if (!KDL::Equal(segment_gap, 0)) {
		delta_mid = delta_gap * segment_mid / segment_gap;
	}

	trsp_ikmid result{desire_z, lower_item.mid + delta_mid, lower_item.range};

	double mid1 = calculate_ik_b_f_radius(desire_z);
	SDL_Log("{calculate_ik_mid}range: %.6f z: %.6f => mid: %.6f exp's mid1: %.6f", lower_item.range, desire_z, result.mid, mid1);

	return result.mid;
}

bool tmoveit_calculator::tclaw::index_pair_from_object_width(double width, tclaw_snapshot& result) const
{
	const trsp_clawgap* items = rsp_moveit_.items;
	const double frictional_bonus = 0.002;
	double desire_halfgap = width / 2 - frictional_bonus;
			
	// Of course, to speed up, you can use dichotomy method.
	int upper_index = 0;
	for (; upper_index < rsp_moveit_.item_count; upper_index ++) {
		if (items[upper_index].halfgap > desire_halfgap) {
			break;
		}
	}

	int lower_index = upper_index - 1;
	VALIDATE(lower_index >= 0, null_str);

	const trsp_clawgap& lower_item = rsp_moveit_.items[lower_index];
	const trsp_clawgap& upper_item = rsp_moveit_.items[upper_index];

	double delta_gap = desire_halfgap - lower_item.halfgap;
	double segment_gap = upper_item.halfgap - lower_item.halfgap;

	double segment_value = upper_item.value - lower_item.value;
	double delta_value = delta_gap * segment_value / segment_gap;

	double segment_dist = upper_item.dist - lower_item.dist;
	double delta_dist = delta_gap * segment_dist / segment_gap;

	result.set(lower_index, upper_index, lower_item.halfgap + delta_gap,
		lower_item.value + delta_value, lower_item.dist + delta_dist);

	return true;
}

SDL_DPoint3 tmoveit_calculator::tclaw::model_grasp_point(const SDL_DSize3& size, int tip_pos, tclaw_snapshot* snapshot_ptr) const
{
	VALIDATE(tip_pos >= 0 && tip_pos < tip_count, null_str);

	tclaw_snapshot snapshot;
	bool ret = index_pair_from_object_width(size.w, snapshot);
	VALIDATE(ret, null_str);

	const double tip_center_z_offset = 0.000;
	const SDL_DPoint3 tip{snapshot.dist, -snapshot.halfgap, tip_center_z_offset};
	SDL_DPoint3 result = tip;

	// double claw_height = 0.017; // 1.7cm
	double claw_height = rsp_moveit_.header.claw_height;
	VALIDATE(KDL::Equal(claw_height, 0.017), null_str);
	if (tip_pos == tip_top) {
		result.z += claw_height / 2;

	} else if (tip_pos == tip_bottom) {
		result.z -= claw_height / 2;
	}

	if (snapshot_ptr != nullptr) {
		*snapshot_ptr = snapshot;
	}

	SDL_Log("claw_snapshot: %s result: (%.6f, %.6f, %.6f)", snapshot.to_string().c_str(), result.x, result.y, result.z);
	return result;
}

//
// tdcamera_slot_impl
//
std::string tdcamera_slot_impl::impl_did_draw_slice(trtc_client::VideoRenderer& vsink, std::vector<SDL_2Point>& new_qrcode_corners, std::vector<SDL_Rect>& reference_rects)
{
	std::string msg;
	new_qrcode_corners.clear();
	reference_rects.clear();

	ttexture_2_mat_lock mat_lock(vsink.tex_);
	const cv::Mat& frame = mat_lock.mat;

	if (use_calculator() && !moveit_calculator_.is_started()) {
		tdcintrinsics_C intrinsics;
		// moveit use dctask_d2c, get color-intrinsics 
		bool ret = dcamera_driver_.get_intrinsics(false, intrinsics);
		VALIDATE(ret, null_str);

		moveit_calculator_.did_started(intrinsics, frame.cols, frame.rows);
	}

	if (keep_DoWork_frame_) {
		VALIDATE(!qrcode_corners_.empty(), null_str);
		threading::lock lock(camera_.get_variable_mutex());
		keep_DoWork_frame_ = false;
		// new_qrcode = qrcode_;
		new_qrcode_corners = qrcode_corners_;
	}

	tdepthcapture::VideoRenderer2* vsink2 = static_cast<tdepthcapture::VideoRenderer2*>(&vsink);

	if (use_calculator()) {
		bool original_started = moveit_calculator_.operating();

		std::vector<SDL_2Point> color_points;

		if (!new_qrcode_corners.empty() && moveit_calculator_.operating() && !moveit_calculator_.require_wakeup_thread(moveit_calculator_.operate().state)) {
			// is wait for user provide corner's points.
/*
			color_points.push_back(SDL_2Point{new_qrcode_corners[0].x, new_qrcode_corners[0].y, new_qrcode_corners[2].x, new_qrcode_corners[2].y});
			color_points.push_back(SDL_2Point{new_qrcode_corners[1].x, new_qrcode_corners[1].y, new_qrcode_corners[3].x, new_qrcode_corners[3].y});

			color_points.push_back(SDL_2Point{new_qrcode_corners[2].x, new_qrcode_corners[2].y, new_qrcode_corners[0].x, new_qrcode_corners[0].y});
			color_points.push_back(SDL_2Point{new_qrcode_corners[3].x, new_qrcode_corners[3].y, new_qrcode_corners[1].x, new_qrcode_corners[1].y});
*/
			color_points = new_qrcode_corners;
		}

		msg = moveit_calculator_.operate_slice(*vsink2, color_points.empty()? nullptr: &color_points);
		
		reference_rects = moveit_calculator_.get_reference_rects();
		// if (!reference_rects.empty()) {
		//	for (std::vector<SDL_Rect>::const_iterator it = reference_rects.begin(); it != reference_rects.end(); ++ it) {
				// const SDL_Rect& src = *it;
				// SDL_Rect rect{video_dst.x + (int)(src.x * xratio), video_dst.y + (int)(src.y * yratio), src.w, src.h};
				// render_rect_frame(renderer, rect, 0xffff0000, 1);
		//	}
		// }

		if (original_started && !moveit_calculator_.operating()) {
			did_operate_stopped();
		}
	}

	return msg;
}
/*
void tdcamera_slot_impl::camera_work_frame(const surface& surf)
{
	VALIDATE(surf.get() != nullptr, null_str);

	tsurface_2_mat_lock mat_lock(surf);

	cv::Mat rgb;
	cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);

	if (moveit_calculator_.operating() && !moveit_calculator_.require_wakeup_thread(moveit_calculator_.operate().state)) {
		std::vector<cv::Point> corners;
		std::string qrcode;
		if (moveit_calculator_.operate().detect_qrcode) {
			qrcode = find_qr(rgb, &corners);
		} else {
			qrcode = find_barcode(rgb, &corners);
		}

		if (!qrcode.empty()) {
			SDL_Log("%u, {camera_work_frame}qrcode: %s", SDL_GetTicks(), qrcode.c_str());

			threading::lock lock(camera_.get_variable_mutex());
			keep_DoWork_frame_ = true;
			qrcode_ = qrcode;
			qrcode_corners_ = corners;
		}
	}
}
*/

void tdcamera_slot_impl::camera_work_frame(const surface& surf)
{
	VALIDATE(surf.get() != nullptr, null_str);

	tsurface_2_mat_lock mat_lock(surf);

	cv::Mat rgb;
	cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);

	if (moveit_calculator_.operating() && !moveit_calculator_.require_wakeup_thread(moveit_calculator_.operate().state)) {
		std::vector<SDL_2Point> corners;
		moveit_calculator_.operate().task_api->moveit->camera_work_frame(surf, mat_lock.mat, corners);
		
		if (!corners.empty()) {
			threading::lock lock(camera_.get_variable_mutex());
			keep_DoWork_frame_ = true;
			// qrcode_ = qrcode;
			qrcode_corners_ = corners;
		}
	}
}

void tdcamera_slot_impl::camera_post_enter_task()
{
	VALIDATE(!moveit_calculator_.is_started(), null_str);
/*
	start_avcapture_message_ = _("Starting Camera");

	VALIDATE(window_ != nullptr, null_str);
	if (window_->drawn()) {
		gui2::absolute_draw();
	}

	// paper_->set_timer_interval(30);
	paper_->set_timer_interval(10);
*/
}

void tdcamera_slot_impl::camera_pre_exit_task()
{
/*
	paper_->set_timer_interval(0);
*/
	if (use_calculator()) {
		moveit_calculator_.did_stopped();
	}
}

//
// tmoveit_aplt_task
//
void tmoveit_aplt_task::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	VALIDATE(locals_count == 1, null_str);
	trtc_client::VideoRenderer& vsink = *locals[0];

	std::vector<SDL_2Point> new_qrcode_corners;
	std::vector<SDL_Rect> reference_rects;
	const std::string msg = impl_did_draw_slice(vsink, new_qrcode_corners, reference_rects);

	if (did_draw_slice_bh_ != NULL) {
		did_draw_slice_bh_(moveit_calculator_, vsink, draw_rect, msg, new_qrcode_corners, reference_rects);
	}
}

void tmoveit_aplt_task::did_navigation_bh_moveit(bool result, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const aplt::taplt_task& aplt_task)
{
	// Although I hope tros_base_node is started as far, but will fail if base serial's path is empty.
	// Imagine below sequence:
	// 1)base serial's path is empty, so tros_base_node isn't started.
	// 2)bg task: move+task. because base serial's path is empty, move fail. then task, it is here.
	
	// If the navigation was successfully completed before, the ros_instance.task_ is not nullptr at this time. 
	// Once navigation fails, like starting laser fail, ros_instance.task_ is nullptr, and ros_instance.started_ is false.
	// Call ros_instance_.register_slot(*this) at this point, making sure ros_instance.started_ is true.
	// ros_instance_.register_slot(*this);

	VALIDATE(ros_instance_.base_driver().node_started(), null_str);

	// because tros_base_node is started always, must be started.
	VALIDATE(ros_instance_.started(), null_str);

	if (ros_instance_.mode() != nposm) {
		ros_instance_.stop_navigation_node();
	}
	if (ros_instance_.moveit_node_started()) {
		ros_instance_.stop_moveit_node();
	}

	ros_instance_.start_moveit_node(false);

	camera_.set_slot(this);

	// pre_show
	if (dcamera_driver_.installed() && !dcamera_driver_.main_tasking()) {
		int def_depth_task = dctask_d2c;
		dcamera_driver_.set_desire_depth_task(def_depth_task);
	}

	camera_.enter_task(tcamera::taskid_dcamera, 0, false);

	// click_start
	aplt::tapplet& aplt2 = *const_cast<aplt::tapplet*>(&aplt);
	aplt::ttask_api* task_api = ros_instance_.drivers().create_task_api(aplt2);
	if (task_api == nullptr) {
		SDL_Log("create_task_api fail");
		return;
	}

	ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);
	moveit_calculator_.start_operate(tmoveit_calculator::scene_normal, *task_api, aplt, cfg_task);
}

void tmoveit_aplt_task::stop_navigation_aplt_task(const aplt::taplt_task& aplt_task)
{
	VALIDATE(ros_instance_.moveit_node_started(), null_str);

	camera_.exit_task(tcamera::taskid_dcamera);

	camera_.set_slot(nullptr);

	// corresponding to start_moveit_node().
	// because don't use ros_instance_.register_slot/deregister_slot, must call it.
	ros_instance_.stop_moveit_node();

	// corresponding to register_slot(*this)
	// ros_instance_.deregister_slot(*this);
}

void tmoveit_aplt_task::slice()
{
	if (did_draw_slice_bh_ == NULL) {
		// is no temporary slot, tbg_task2 derminate when to call camera_.slice(...)
		camera_.slice(empty_rect, false);
	}

}

void tmoveit_aplt_task::did_operate_stopped()
{
	const std::string msg = _("Moveit finished");

	instance->bg_task().add_log2(time(nullptr), msg, 0, false);
	instance->bg_task().set_luafunc_finished();
}