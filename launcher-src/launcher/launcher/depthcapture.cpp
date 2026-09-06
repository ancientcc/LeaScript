#define GETTEXT_DOMAIN "launcher-lib"

#include "depthcapture.hpp"
#include "rose_config.hpp"

#include "libyuv/convert_argb.h"
#include "sensor_msgs/LaserScan.h"
#include "filesystem.hpp"

#include "aplt_clazz.hpp"
#include <rose_ros/aplt.hpp>

#include "dcamera_driver.hpp"

#include <opencv2/imgproc.hpp>
#include <tf2/utils.h>
#include <SDL_image.h>

using namespace std::placeholders;

tdepthcapture::tdepthcapture(tcamera& camera, tdcamera_driver& dcamera_driver, int task, int id, rtc::MessageHandler& dlg_handler, tadapter& adapter, const tpoint& desire_size, const int desire_fps, bool idle_screen_saver)
    : trtc_client(id, dlg_handler, adapter, desire_size, true, std::vector<trtsp_settings>(), desire_fps, false, idle_screen_saver, false)
    , camera_(camera)
    , dcamera_driver_(dcamera_driver)
    , task_(task)
    , sink_(nullptr)
    , snapshot_use_dcpitch_(false)
{
    caller_ = true;
	state_ = CONNECTED;

	// ConnectToPeer("fake", true, null_str);
    bool ret = dcamera_driver_.main_start(task, *this);
    if (!ret) {
        return;
    }

    std::vector<std::string> device_names;
    device_names.push_back("Depth Camera");

    std::vector<tusing_vidcap> using_vidcaps = adapter_->app_video_capturer(id_, false, device_names);
	if (using_vidcaps.empty()) {
		dcamera_driver_.main_stop();
		return;
	}
    allocate_vrenderers(false, using_vidcaps.size());
    int at = 0;
    for (std::vector<tusing_vidcap>::const_iterator it = using_vidcaps.begin(); it != using_vidcaps.end(); ++ it, at ++) {
        const tusing_vidcap& using_vidcap = *it;
        StartLocalRenderer(nullptr, using_vidcap.name, at, false);
    }

    trtc_client::VideoRenderer* sink = vrenderer(false, 0);
    VALIDATE(sink != nullptr, null_str);
    sink_ = static_cast<VideoRenderer2*>(sink);
}

tdepthcapture::~tdepthcapture()
{
    if (dcamera_driver_.main_tasking()) {
        dcamera_driver_.main_stop();
    }

    state_ = NOT_CONNECTED;
    DeletePeerConnection();
}

std::vector<std::string> tdepthcapture::recalculate_existing_cameras()
{
    std::vector<std::string> device_names;
    device_names.push_back("Depth Camera");

    return device_names;
}

void tdepthcapture::main_OnFrame()
{
    dcamera_driver_.main_slice();
}

void tdepthcapture::dcamera_did_frames(int task, const tdcframe_C* frames, int count)
{
    if (frames == nullptr) {
        return;
    }

    sink_->OnFrame(dcamera_driver_, task, frames, count);
}

void tdepthcapture::VideoRenderer2::OnFrame(tdcamera_driver& dcamera_driver, int task, const tdcframe_C* frames, int count)
{
    VALIDATE_IN_MAIN_THREAD();

    // -------------------------

    tcamera::tslot* slot = depthcapture_.camera_.get_slot();
    if (slot != nullptr && slot->is_dcamera) {
        tdcamera_slot* dslot = static_cast<tdcamera_slot*>(slot);
        dslot->dcamera_did_OnFrame(task, frames, count);
    }

    int frame_width = nposm;
    int frame_height = nposm;

    if (task == dctask_color) {
        const tdcframe_C& frame = frames[0];
        VALIDATE(frame.type == dcframetype_color, null_str);
        VALIDATE(count == 1, null_str);

        frame_width = frame.width;
        frame_height = frame.height;

        if (frame.format == dcformat_rgb) {

        } else {
            VALIDATE(frame.format == dcformat_mjpeg, null_str);
        }

        if (pixels_ == nullptr) {
		    // must execute before lock sink_mutex, because main thread maybe waiting for sink_mutex. 
		    // for exmaple, did_draw_slice.
			client_.allocate_texture(remote_, at_, frame_width, frame_height);

            VALIDATE(!realtime_d2c_.valid, null_str);
            // realtime_d2c_.argb_mat = cv::Mat();
	    }

        if (frame.format == dcformat_rgb) {
            cv::Mat src(frame_height, frame_width, CV_8UC3, const_cast<uint8_t*>(frame.data));
            cv::Mat result;
            cv::cvtColor(src, result, cv::COLOR_RGB2BGRA);

            memcpy(pixels_, result.data, frame_width * frame_height * 4);
                
        } else {
            int pitch = frame_width * 4;

            libyuv::MJPGToARGB((const uint8_t*)frame.data,
                frame.data_size,
                pixels_, // uint8_t* dst_argb
                pitch, // int dst_stride_argb
                frame_width, // int src_width
                frame_height, // int src_height
                frame_width, // int dst_width
                frame_height); // int dst_height
        }


    } else if (task == dctask_depth) {
        const tdcframe_C& frame = frames[0];
        VALIDATE(frame.type == dcframetype_depth, null_str);
        VALIDATE(count == 1, null_str);

        frame_width = frame.width;
        frame_height = frame.height;

        if (pixels_ == nullptr) {
		    // must execute before lock sink_mutex, because main thread maybe waiting for sink_mutex. 
		    // for exmaple, did_draw_slice.
			client_.allocate_texture(remote_, at_, frame_width, frame_height);

            VALIDATE(!realtime_d2c_.valid, null_str);
	    }

        float scale = frame.scale;
/*
        {
            cv::Mat rstMat;
            cv::Mat cvtMat;
            cv::Mat rawMat = cv::Mat(videoFrame->height(), videoFrame->width(), CV_16UC1, videoFrame->data());
            // depth frame pixel value multiply scale to get distance in millimeter

            // threshold to 5.12m
            cv::threshold(rawMat, cvtMat, 5120.0f / scale, 0, cv::THRESH_TRUNC);
            // cvtMat.convertTo(cvtMat, CV_8UC1, scale * 0.05);
            // cv::applyColorMap(cvtMat, rstMat, cv::COLORMAP_JET);
            cv::cvtColor(rawMat, rstMat, cv::COLOR_GRAY2BGR);

            int channels1 = rawMat.channels();
            int channels2 = cvtMat.channels();
            int channels3 = rstMat.channels();
            int ii = 0;
        }
*/
        uint16_t* depth_data = (uint16_t *)frame.data;

        if (!depthcapture_.depth_png_.empty()) {
            gui2::tprogress_default_slot slot(std::bind(&tdepthcapture::snapshot_depth_rt, &depthcapture_, _1, pixels_, task, frames, count));
		    gui2::run_with_progress(slot, null_str, null_str, 0);
        }

        int dst_index = 0;
        // 5120.0f / scale
        for (int y = 0; y < frame_height; y ++) {
            for (int x = 0; x < frame_width; x ++) {
                int val = depth_data[y * frame_width + x] * 0.05;
                if (val > 255) {
                    val = 255;
                }
                pixels_[dst_index ++] = val;
                pixels_[dst_index ++] = val;
                pixels_[dst_index ++] = val;
                pixels_[dst_index ++] = 0xff;
            }
        }

        // memcpy(pixels_, rstMat.data, frame_width * frame_height * 3);

        // for Y16 format depth frame, print the distance of the center pixel every 30 frames
        // if (depthFrame->index() % 30 == 0 && depthFrame->format() == OB_FORMAT_Y16) {
            uint32_t  width  = frame.width;
            uint32_t  height = frame.height;

            // pixel value multiplied by scale is the actual distance value in millimeters
            float centerDistance = depth_data[width * height / 2 + width / 2] * scale;

            // attention: if the distance is 0, it means that the depth camera cannot detect the object£¨may be out of detection range£©
            // SDL_Log("(%ix%i) Facing an object %.5f mm away. scale: %.5f", width, height, centerDistance, scale);
        // }



    } else if (task == dctask_d2c) {
        const tdcframe_C& color_frame = frames[dcframeidx_color];
        const tdcframe_C& depth_frame = frames[dcframeidx_depth];
        VALIDATE(color_frame.type == dcframetype_color, null_str);
        VALIDATE(depth_frame.type == dcframetype_depth, null_str);
        VALIDATE(count == 2, null_str);

        frame_width = color_frame.width;
        frame_height = color_frame.height;

        int frame_width2 = depth_frame.width;
        int frame_height2 = depth_frame.height;

        VALIDATE(frame_width == frame_width2, null_str);
        VALIDATE(frame_height == frame_height2, null_str);

        if (pixels_ == nullptr) {
		    // must execute before lock sink_mutex, because main thread maybe waiting for sink_mutex. 
		    // for exmaple, did_draw_slice.
			client_.allocate_texture(remote_, at_, frame_width, frame_height);

            VALIDATE(!realtime_d2c_.valid, null_str);
            realtime_d2c_.argb_mat = cv::Mat(frame_height, frame_width, CV_8UC4);
            realtime_d2c_.depth_data = (int16_t*)malloc(frame_width * frame_height * 2);

            last_deliver_d2c_.argb_mat = cv::Mat(frame_height, frame_width, CV_8UC4);
            last_deliver_d2c_.depth_data = (int16_t*)malloc(frame_width * frame_height * 2);

            // memset(argb_mat_.data, 0xff, frame_width * 4 * frame_height);
	    }

        
        int pitch = frame_width * 4;
        libyuv::MJPGToARGB((const uint8_t*)color_frame.data,
            color_frame.data_size,
            pixels_, // uint8_t* dst_argb
            pitch, // int dst_stride_argb
            frame_width, // int src_width
            frame_height, // int src_height
            frame_width, // int dst_width
            frame_height); // int dst_height

        const int16_t* depth_data = (const int16_t*)depth_frame.data;
        int dst_index = 0;
        float alpha = 0.6f; // 0.6f

        realtime_d2c_.set(pixels_, depth_data, depth_frame.scale);
        if (!depthcapture_.depth_png_.empty()) {
            gui2::tprogress_default_slot slot(std::bind(&tdepthcapture::snapshot_depth_rt, &depthcapture_, _1, pixels_, task, frames, count));
		    gui2::run_with_progress(slot, null_str, null_str, 0);
        }

        for (int y = 0; y < frame_height; y ++) {
            for (int x = 0; x < frame_width; x ++) {
                uint8_t* outRgb = pixels_ + dst_index * 4;
                const uint16_t depth16 = depth_data[dst_index];

                int val = depth16 * 0.05;
                if (val > 255) {
                    val = 255;
                }
                uint8_t depth = val;

                outRgb[0] = (uint8_t)(outRgb[0] * (1.0f - alpha) + depth * alpha);
                outRgb[1] = (uint8_t)(outRgb[1] * (1.0f - alpha) + depth * alpha);
                outRgb[2] = (uint8_t)(outRgb[2] * (1.0f - alpha) + depth * alpha);
                // outRgb[3] = (uint8_t)(outRgb[3] * (1.0f - alpha) + depth * alpha);  // A comp
                dst_index ++;
            }
        }
        VALIDATE(dst_index == frame_width * frame_height, null_str);


    } else {
        VALIDATE(false, null_str);
    }

    frame_thread_frames ++;

	if (hflip_) {
		int offset, index1, index2;
		uint8_t* src;
		uint8_t* dst;
		uint8_t tmp;
		for (int y = 0; y != app_height_; ++y) {
			offset = y * app_width_;
			for (int x = 0; x != app_width_ / 2; ++x) {
				index1 = offset + x;
				index2 = offset + app_width_ - x - 1;
				src = pixels_ + (index1 << 2);
				dst = pixels_ + (index2 << 2);
				tmp = src[0];
				src[0] = dst[0];
				dst[0] = tmp;
				tmp = src[1];
				src[1] = dst[1];
				dst[1] = tmp;
				tmp = src[2];
				src[2] = dst[2];
				dst[2] = tmp;
				tmp = src[3];
				src[3] = dst[3];
				dst[3] = tmp;
			}
		}
	}

	frame_thread_new_frame_ = true;
	verbose_log();
}

const cv::Mat* tdepthcapture::VideoRenderer2::desire_deliver_frame()
{
    if (!realtime_d2c_.valid) {
        VALIDATE(realtime_d2c_.argb_mat.empty() && realtime_d2c_.depth_data == nullptr, null_str);
        return nullptr;
    }

    last_deliver_d2c_.set(realtime_d2c_.argb_mat.data, realtime_d2c_.depth_data, realtime_d2c_.depth_scale);
    // Of course, you can not set realtime_d2c_.valid to false. 
    // But in order to verify that tcamera::deliver_frame_to_worker will call vsink.desire_deliver_frame(), only when the vsink.new_frame is true.
    realtime_d2c_.valid = false;

    return &last_deliver_d2c_.argb_mat;
}

void tdepthcapture::snapshot_depth(bool use_dcpitch, const std::string& png, const std::string& depth_data_file)
{
    VALIDATE(!png.empty(), null_str);

    snapshot_use_dcpitch_ = use_dcpitch;
    depth_png_ = png;
    depth_data_file_ = depth_data_file;
}

namespace tf2
{

void doTransform_position(const tf2::Transform& t, const SDL_DPoint3& p_in, SDL_DPoint3& p_out)
{
    // tf2::Transform save 'rotation' use rotation matrix(Matrix3x3 m_basis)
    
    // this function only position is calculated, no rotation.
    // impletement method 'copy' from:
    //   tf2::doTransform(const geometry_msgs::Pose& t_in, geometry_msgs::Pose& t_out, const geometry_msgs::TransformStamped& transform)
    // Only 3 dots are required.
    tf2::Vector3 v(p_in.x, p_in.y, p_in.z);

    // <libros>/include/tf2/LinearMath/Transform.h
    // Vector3 tf2::Transform::operator()(const Vector3& x) const
    // Only 3 vector3-dot product are required.
    tf2::Vector3 v_out = t(v);
    p_out.x = v_out.m_floats[0];
    p_out.y = v_out.m_floats[1];
    p_out.z = v_out.m_floats[2];
}

void doTransform_position(const geometry_msgs::Transform& transform, const SDL_DPoint3& p_in, SDL_DPoint3& p_out)
{
    tf2::Transform t;
    tf2::fromMsg(transform, t);

    doTransform_position(t, p_in, p_out);
}

}

void inplace_2_draw_1(const uint8_t* src, uint8_t* dest, int width, int height, int channels, const SDL_Point& divisor)
{
    VALIDATE(divisor.x > 0 && divisor.y > 0, null_str);
    VALIDATE(width > 0 && width % divisor.x == 0, null_str);
    if (height != 1) {
        VALIDATE(height > 0 && height % divisor.y == 0, null_str);
    }
    VALIDATE(channels == 2 || channels == 4, null_str);

    int src_y_start = 0;
    int dest_index = 0;
    const int pitch = width * channels;
    const int x_inc = channels * divisor.x;

    for (int y = 0; y < height; y += divisor.y) {
        src_y_start = y * pitch;
        for (int index = src_y_start; index < src_y_start + pitch; index += x_inc, dest_index += channels) {
            if (channels == 2) {
                dest[dest_index] = src[index];
                dest[dest_index + 1] = src[index + 1];

            } else if (channels == 4) {
                dest[dest_index] = src[index];
                dest[dest_index + 1] = src[index + 1];
                dest[dest_index + 2] = src[index + 2];
                dest[dest_index + 3] = src[index + 3];
            }
        }
    }
    VALIDATE(dest_index * divisor.x * divisor.y == width * height * channels, null_str);
}

void save_d2c_png(const tdcintrinsics_C& intrinsics, gui2::tprogress_* progress, const uint8_t* tex_pixels, int task, const tdcframe_C& depth_frame, 
    double dcpitch, bool all_rows, const std::string& result_png, const std::string& depth_data_file)
{
    // overlay_digits_to_surf() will render text, these require in main thread.
    VALIDATE_IN_MAIN_THREAD();

    VALIDATE(task == dctask_depth || task == dctask_d2c, null_str);
    VALIDATE(tex_pixels != nullptr, null_str);
    VALIDATE(!result_png.empty(), null_str);

    if (progress != nullptr) {
        progress->set_message("1/3 Calcuate overlay x/y/z");
    }
    // gui2::absolute_draw();
    // progress.show_slice();
    // SDL_Delay(1000);
    // depth_png_.clear();
    // return true;

    // const tdcframe_C& depth_frame = task == dctask_depth? frames[0]: frames[dcframeidx_depth];
    const int frame_width = depth_frame.width;
    const int frame_height = depth_frame.height;
    const uint16_t* depth_data = (const uint16_t*)depth_frame.data;

    uint16_t* coor_x_data = (uint16_t*)malloc(frame_width * frame_height * 2);
    memset(coor_x_data, 0, frame_width * frame_height * 2);
    uint16_t* coor_x_data_ptr = coor_x_data;

    uint16_t* coor_y_data = (uint16_t*)malloc(frame_width * frame_height * 2);
    memset(coor_y_data, 0, frame_width * frame_height * 2);
    uint16_t* coor_y_data_ptr = coor_y_data;

    uint16_t* coor_z_data = (uint16_t*)malloc(frame_width * frame_height * 2);
    memset(coor_z_data, 0, frame_width * frame_height * 2);
    uint16_t* coor_z_data_ptr = coor_z_data;

    int width = frame_width;
    int height = frame_height;

    // tdcintrinsics_C intrinsics;
    // bool ret = dcamera_driver.get_intrinsics(true, intrinsics);
    // VALIDATE(ret, null_str);

    // double dcpitch = float_nposm;
    // double dcpitch = aplt::valuex.euler[1];
    SDL_Log("{save_d2c_png}dcpitch: %.6f(deg:%.3f)", dcpitch, RAD2DEG(dcpitch));

    tf2::Quaternion q;
	q.setRPY(0, dcpitch, 0);
    const tf2::Transform transform(q);


    float fdx = 1 / intrinsics.fx;
    float fdy = 1 / intrinsics.fy;
    float u0 = intrinsics.cx;
    float v0 = intrinsics.cy;

    size_t valid_count = 0;
    // const float MIN_DISTANCE = 20.0;
    const float MIN_DISTANCE = 5.0;
    const float MAX_DISTANCE = 10000.0;
    double depth_scale = depth_frame.scale;
    const float min_depth = MIN_DISTANCE / depth_scale;
    const float max_depth = MAX_DISTANCE / depth_scale;
    for (int y = 0; y < height; y++) {
        const int y_start = y * width;
        for (int x = 0; x < width; x++) {
            int index = y_start + x;

            // if (depth_data[y * width + x] < min_depth || depth_data[y * width + x] > max_depth) {
            if (depth_data[y * width + x] < min_depth) {
                *(coor_x_data + index) = INT16_MAX;
                *(coor_y_data + index) = INT16_MAX;
                continue;
            }
            float xf = (x - u0) * fdx;
            float yf = (y - v0) * fdy;
            float zf = depth_data[index] * depth_scale;

            float c_x = zf * xf;
            float c_y = zf * yf;
            
            if (is_float_nposm(dcpitch)) {
                *(coor_x_data + index) = (uint16_t)(c_x);
                *(coor_y_data + index) = (uint16_t)(c_y);
                *(coor_z_data + index) = zf;

            } else {
                SDL_DPoint3 camera_xyz{c_x, c_y, zf};
                // change camera-coor to world-coor
                // SDL_DPoint3 world_xyz{camera_xyz.z / 1000.0, camera_xyz.x / -1000.0, camera_xyz.y / -1000.0};
                SDL_DPoint3 world_xyz{camera_xyz.z, camera_xyz.x / -1.0, camera_xyz.y / -1.0};

                SDL_DPoint3 world_xyz2;
                tf2::doTransform_position(transform, world_xyz, world_xyz2);

                *(coor_x_data + index) = (uint16_t)world_xyz2.x;
                *(coor_y_data + index) = (uint16_t)world_xyz2.y;
                *(coor_z_data + index) = (uint16_t)world_xyz2.z;
            }

            valid_count++;
        }
    }

    if (task == dctask_depth) {
        uint8_t* mutable_pixels = const_cast<uint8_t*>(tex_pixels);
        sensor_msgs::LaserScan scan_msg;
        depth_sector_area(intrinsics, scan_msg, frame_width, frame_height, depth_data, depth_scale, mutable_pixels, nullptr, dcpitch);
    }

    if (progress != nullptr) {
        progress->set_message("2/3 Overlay x/y/z");
        progress->set_percentage(5);
    }

    if (!depth_data_file.empty()) {
        const std::string file = game_config::preferences_dir + "/" + depth_data_file;
        save_y16_depth_file(intrinsics, width, height, (const int16_t*)depth_data, depth_scale, dcpitch, file);
    }


    const uint8_t* use_tex_pixels = tex_pixels;
    uint8_t* other_pixels = nullptr;
    int use_frame_width = frame_width;
    int use_frame_height = frame_height;
    if (frame_width > 640) {
        SDL_Point divisor{2, 2}; // default
        if (all_rows) {
            // if you want display all rows
            divisor.x = 4;
            divisor.y = 1;
        }

        use_frame_width = frame_width / divisor.x;
        use_frame_height = frame_height / divisor.y;

        inplace_2_draw_1((uint8_t*)coor_x_data, (uint8_t*)coor_x_data, frame_width, frame_height, 2, divisor);
        inplace_2_draw_1((uint8_t*)coor_y_data, (uint8_t*)coor_y_data, frame_width, frame_height, 2, divisor);
        inplace_2_draw_1((uint8_t*)coor_z_data, (uint8_t*)coor_z_data, frame_width, frame_height, 2, divisor);

        // other_pixels = (uint8_t*)malloc(frame_width * frame_height * 4);
        other_pixels = (uint8_t*)malloc(frame_width * frame_height * 4 / (divisor.x * divisor.y));
        use_tex_pixels = other_pixels;
        inplace_2_draw_1(tex_pixels, other_pixels, frame_width, frame_height, 4, divisor);
    }


    std::vector<tsurf_overlay> overlays;
    overlays.push_back(tsurf_overlay((const uint8_t*)coor_x_data, 2, true, 0xffff0000, true, INT16_MAX));
    overlays.push_back(tsurf_overlay((const uint8_t*)coor_y_data, 2, true, 0xff00ff00, true, INT16_MAX));
    overlays.push_back(tsurf_overlay((const uint8_t*)coor_z_data, 2, true, 0xff0000ff, true, 0));

    surface surf = overlay_digits_to_surf(use_tex_pixels, use_frame_width, use_frame_height, 4, overlays);

    if (progress != nullptr) {
        progress->set_message("3/3 Save result");
        progress->set_percentage(60);
    }
    imwrite(surf, result_png);

    // int quality = 93; // normal is 100. I want reduce size, so use less quality.
	// IMG_SaveJPG(surf, (game_config::preferences_dir + "/" + result_png).c_str(), quality);

    if (other_pixels != nullptr) {
        free(other_pixels);
    }
    free(coor_x_data);
    free(coor_y_data);
}

bool tdepthcapture::snapshot_depth_rt(gui2::tprogress_& progress, const uint8_t* tex_pixels, int task, const tdcframe_C* frames, int count)
{
    tdcintrinsics_C intrinsics;
    bool ret = dcamera_driver_.get_intrinsics(task == dctask_depth, intrinsics);
    VALIDATE(ret, null_str);

    const tdcframe_C& depth_frame = task == dctask_depth? frames[0]: frames[dcframeidx_depth];
    double dcpitch = snapshot_use_dcpitch_? aplt::valuex.euler[1]: float_nposm;
    save_d2c_png(intrinsics, &progress, tex_pixels, task, depth_frame, dcpitch, false, depth_png_, depth_data_file_);

    depth_png_.clear();
    depth_data_file_.clear();
    return true;
}
