/*******************************************************************************
 * Copyright (c) 2023 Orbbec 3D Technology, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/
#include "ros/ros.h"
#include <ros/callback_queue.h>
#include "orbbec_camera/ob_camera_node_driver.h"

#include "../dcamera/dcamera_device.hpp"

#define is_valid_OBCameraParam(param)   \
    ((param).depthIntrinsic.fx > 100 && (param).depthIntrinsic.fx < 1000)

extern void dump_depth_profiles(const std::shared_ptr<ob::StreamProfileList>& depthProfiles);

class torbbec: public aplt::tdcamera_device
{
public:
    torbbec() 
        : task_(nposm)
        , receiver_(nullptr)
    {
        camera_param_.depthIntrinsic.fx = 0;
        depth_intrinsics_.valid = false;
        color_intrinsics_.valid = false;
    }

     ~torbbec() {}

private:
	void ros_pre_start() override {}
	void ros_dcamera_node(bool& exit) override;

	bool main_start(int task, aplt::tdcamera_slot::treceiver& receiver) override;
	void main_slice() override;
	void main_stop() override;

    bool get_intrinsics(bool depth, tdcintrinsics_C& result) override;
    void calculate_intrinsics(bool depth, int width, int height);

private:
    int task_;
    aplt::tdcamera_slot::treceiver* receiver_;
    std::unique_ptr<ob::Pipeline> pipe_;
    OBCameraParam camera_param_;

    tdcframe_C frames_[dcframeidx_count];

    tdcintrinsics_C depth_intrinsics_;
    tdcintrinsics_C color_intrinsics_;
};

void torbbec::ros_dcamera_node(bool& exit)
{
    int argc = 0;
    ros::init(argc, nullptr, "orbbec_camera", ros::init_options::AnonymousName);

    ros::NodeHandle nh;
    ros::NodeHandle nh_private("~");

    ros::CallbackQueue cbqueue;
    nh.setCallbackQueue(&cbqueue);
    nh_private.setCallbackQueue(&cbqueue);

    orbbec_camera::OBCameraNodeDriver ob_camera_node_factory(nh, nh_private);

    // ros::spin();
    ros::WallDuration timeout(0.05f);
    while (!exit & ros::ok()) {
        ob_camera_node_factory.slice();
	    cbqueue.callAvailable(timeout);
    }

    // ros::shutdown();
}

bool torbbec::main_start(int task, aplt::tdcamera_slot::treceiver& receiver)
{
    SDL_Log("%u {orbbec}SDL_OrbbecCreateContext pre", SDL_GetTicks());
    if (!SDL_OrbbecCreateContext()) {
        SDL_Log("%u {orbbec}SDL_OrbbecCreateContext fail", SDL_GetTicks());
        SDL_OrbbecDestroyContext();
        return false;
    }
    SDL_Log("%u {orbbec}SDL_OrbbecCreateContext successfully", SDL_GetTicks());

    {
        // ob::initOBContext();
        // std::shared_ptr<ob::Device> device = ob::createDevice();

/*
        // Create a Context.
        ob::Context ctx;

        // Query the list of connected devices
        auto devList = ctx.queryDeviceList();

        // Get the number of connected devices
        if (devList->deviceCount() == 0) {
            SDL_Log("{orbbec}Device not found!");
            return -1;
        }

        // Create a device, 0 means the index of the first device
        auto dev = devList->getDevice(0);

        // Get device information
        auto devInfo = dev->getDeviceInfo();

        if (dev->getSensor(OB_SENSOR_COLOR) == nullptr) {
            // Get the name of the device
            SDL_Log("{orbbec}Device name: %s, no OB_SENSOR_COLOR", devInfo->name());
            return -1;
        }

        if (dev->getSensor(OB_SENSOR_DEPTH) == nullptr) {
            // Get the name of the device
            SDL_Log("{orbbec}Device name: %s, no OB_SENSOR_DEPTH", devInfo->name());
            return -1;
        }

        // Get the name of the device
        SDL_Log("{orbbec}Device name: %s", devInfo->name());

        // Get the pid, vid, uid of the device
        SDL_Log("{orbbec}Device pid: 0x%x vid: 0x%x << uid: 0x%x", devInfo->pid(), devInfo->vid(), devInfo->uid());
*/
    }

    // Create a pipeline with default device
    VALIDATE(pipe_.get() == nullptr, null_str);

    try {
        pipe_.reset(new ob::Pipeline);

    } catch(ob::Error &e) {
        SDL_Log("function: %s\nargs: %s\nmessage: %s\ntype: %i",
            e.getName(), e.getArgs(), e.getMessage(), e.getExceptionType());
        return false;
    }
    ob::Pipeline& pipe = *pipe_.get();

    const bool use_640 = false;
    const bool use_y14 = false;
    OBFormat depth_format = use_y14? OB_FORMAT_Y14: OB_FORMAT_Y16;

    SDL_Log("{orbbec}(1) std::make_shared<ob::Config>()");
    // Configure which streams to enable or disable for the Pipeline by creating a Config
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    if (task == dctask_color) {
        std::shared_ptr<ob::VideoStreamProfile> colorProfile = nullptr;
        try {
            // Get all stream profiles of the color camera, including stream resolution, frame rate, and frame format
            std::shared_ptr<ob::StreamProfileList> profiles = pipe.getStreamProfileList(OB_SENSOR_COLOR);
            try {
                SDL_Log("{orbbec}task_color(0) profiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>()");
                colorProfile = std::const_pointer_cast<ob::StreamProfile>(profiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();

                // Find the corresponding Profile according to the specified format, and choose the RGB888 format first
                if (colorProfile.get() == nullptr) {
                    SDL_Log("{orbbec}task_color(1) getVideoStreamProfile(1280, OB_HEIGHT_ANY, OB_FORMAT_RGB, 30)");
                    colorProfile = profiles->getVideoStreamProfile(1280, OB_HEIGHT_ANY, OB_FORMAT_RGB, 30);
                }
                if (colorProfile.get() == nullptr) {
                    // [java]StreamProfile streamProfile = getVideoStreamProfile(colorProfileList, 640, 0, Format.RGB888, 30);
                    
                    SDL_Log("{orbbec}task_color(2) getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_RGB, 30)");
                    colorProfile = profiles->getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_RGB, 30);
                }
                if (colorProfile.get() == nullptr) {
                    SDL_Log("{orbbec}task_color(3) getVideoStreamProfile(0, 0, OB_FORMAT_RGB, 30)");
                    colorProfile = profiles->getVideoStreamProfile(0, 0, OB_FORMAT_RGB, 30);
                    // start OB_SENSOR_COLOR stream with profile: 
                    //     {type: OB_STREAM_COLOR, format: OB_FORMAT_RGB, width: 1920, height: 1080, fps: 30}
                }

            }
            catch(ob::Error &e) {
                // If the specified format is not found, select the first one (default stream profile)
                colorProfile = std::const_pointer_cast<ob::StreamProfile>(profiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();
            }
            config->enableStream(colorProfile);
        }
        catch(ob::Error &e) {
            SDL_Log("Current device is not support color sensor!");
            return false;
        }

    } else if (task == dctask_depth) {
        std::shared_ptr<ob::VideoStreamProfile> depthProfile = nullptr;

        SDL_Log("{orbbec}(2) pipe.getStreamProfileList(OB_SENSOR_DEPTH)");

        try {
            // Get all stream profiles of the depth camera, including stream resolution, frame rate, and frame format
            auto depthProfiles = pipe.getStreamProfileList(OB_SENSOR_DEPTH);

            try {
                dump_depth_profiles(depthProfiles);

                // Find the corresponding profile according to the specified format, first look for the y16 format
                SDL_Log("{orbbec}(3) getVideoStreamProfile(1280, OB_HEIGHT_ANY, OB_FORMAT_Y1x, 10)");
                depthProfile = depthProfiles->getVideoStreamProfile(1280, OB_HEIGHT_ANY, depth_format, 10);

                if (use_640) {
                    SDL_Log("{orbbec}(3) getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_Y1x, 30)");
                    depthProfile = depthProfiles->getVideoStreamProfile(640, OB_HEIGHT_ANY, depth_format, 30);
                }

                if (depthProfile.get() == nullptr) {
                    SDL_Log("{orbbec}null == streamProfile, getVideoStreamProfile#2");
                    SDL_Log("{orbbec}(3) getVideoStreamProfile(0, OB_HEIGHT_ANY, OB_FORMAT_UNKNOWN, 30)");
                    // depthProfile = profiles->getVideoStreamProfile(0, OB_HEIGHT_ANY, OB_FORMAT_UNKNOWN, 30);
                    depthProfile = std::const_pointer_cast<ob::StreamProfile>(depthProfiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();
                }
            }
            catch (ob::Error &e) {
                // If the specified format is not found, search for the default profile to open the stream
                SDL_Log("{orbbec}(e.3) ... ->as<ob::VideoStreamProfile>()");
                depthProfile = std::const_pointer_cast<ob::StreamProfile>(depthProfiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();
            }
        }
        catch(ob::Error &e) {
            SDL_Log("Current device is not support depth sensor!");
            return false;
        }

        // By creating config to configure which streams to enable or disable for the pipeline, here the depth stream will be enabled
        // std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
        config->enableStream(depthProfile);

    } else if (task == dctask_d2c) {
        std::shared_ptr<ob::VideoStreamProfile> colorProfile = nullptr;
        try {
            // Get all stream profiles of the color camera, including stream resolution, frame rate, and frame format
            std::shared_ptr<ob::StreamProfileList> colorProfiles = pipe.getStreamProfileList(OB_SENSOR_COLOR);

            if (use_640) {
                SDL_Log("{orbbec}dctask_d2c(%i)'s color, getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_MJPG, 30)", task);
                colorProfile = colorProfiles->getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_MJPG, 30);
            }

            if (colorProfile.get() == nullptr) {
                colorProfile = std::const_pointer_cast<ob::StreamProfile>(colorProfiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();
            }

            VALIDATE(colorProfile.get() != nullptr, null_str);
            config->enableStream(colorProfile);
        }
        catch(...) {
            SDL_Log("Current device is not support color sensor!");
            return false;
        }

        // Get all stream profiles of the depth camera, including stream resolution, frame rate, and frame format
        std::shared_ptr<ob::StreamProfileList> depthProfiles = pipe.getStreamProfileList(OB_SENSOR_DEPTH);

        std::shared_ptr<ob::VideoStreamProfile> depthProfile;
        if (use_640) {
            SDL_Log("{orbbec}dctask_d2c(%i)'s depth getVideoStreamProfile(640, OB_HEIGHT_ANY, OB_FORMAT_Y1x, 30)", task);
            depthProfile = depthProfiles->getVideoStreamProfile(640, OB_HEIGHT_ANY, depth_format, 30);
        }

        if (depthProfile.get() == nullptr) {
            depthProfile = std::const_pointer_cast<ob::StreamProfile>(depthProfiles->getProfile(OB_PROFILE_DEFAULT))->as<ob::VideoStreamProfile>();
        }

        VALIDATE(depthProfile.get() != nullptr, null_str);
        config->enableStream(depthProfile);

        // Configure the alignment mode as hardware D2C alignment
        config->setAlignMode(ALIGN_D2C_HW_MODE);

    } else {
        VALIDATE(false, null_str);
    }

    task_ = task;
    receiver_ = &receiver;

    // Start the pipeline with config
    pipe.start(config);

    camera_param_ = pipe.getCameraParam();
    return true;
}

int ob_2_ros_format(OBFormat format) 
{
    if (format == OB_FORMAT_RGB) {
        return dcformat_rgb;
        
    } else if (format == OB_FORMAT_MJPG) {
        return dcformat_mjpeg;

    } else if (format == OB_FORMAT_Y16) {
        return dcformat_y16;

    } else if (format == OB_FORMAT_Y14) {
        return dcformat_y14;

    } else {
        VALIDATE(false, null_str);
    }

    return nposm;
}

void torbbec::main_slice()
{
    if (task_ == nposm) {
        VALIDATE(receiver_ == nullptr, null_str);
        return;
    }
    VALIDATE(pipe_.get() != nullptr, null_str);

    ob::Pipeline& pipe = *pipe_.get();

    int frame_width = nposm;
    int frame_height = nposm;

    std::shared_ptr<ob::FrameSet> frameSet = pipe.waitForFrames(0);
    if (frameSet != nullptr) {
        // SDL_Log("%u camera_param_.depthIntrinsic.fx: %.3f", SDL_GetTicks(), camera_param_.depthIntrinsic.fx);
        if (!is_valid_OBCameraParam(camera_param_)) {
            camera_param_ = pipe.getCameraParam();
        }
        if (task_ == dctask_color) {
            std::shared_ptr<ob::ColorFrame> frame = frameSet->colorFrame();

            OBFrameType type = frame->type();
            VALIDATE(type == OB_FRAME_COLOR, null_str);

            auto videoFrame = frame->as<ob::VideoFrame>();
            OBFormat format = videoFrame->format();
            VALIDATE(format == OB_FORMAT_RGB || format == OB_FORMAT_MJPEG, null_str);

            frame_width = videoFrame->width();
            frame_height = videoFrame->height();

            if (!color_intrinsics_.valid && is_valid_OBCameraParam(camera_param_)) {
                calculate_intrinsics(false, frame_width, frame_height);
            }

            tdcframe_C& color_frame = frames_[dcframeidx_color];

            aplt::set_dcamera_frame(color_frame, dcframetype_color, ob_2_ros_format(format),
                frame_width, frame_height, (const uint8_t*)videoFrame->data(), videoFrame->dataSize());
            receiver_->dcamera_did_frames(task_, &color_frame, 1);

        } else if (task_ == dctask_depth) {
            std::shared_ptr<ob::DepthFrame> depthFrame = frameSet->depthFrame();

            OBFrameType type = depthFrame->type();
            VALIDATE(type == OB_FRAME_DEPTH, null_str);

            auto videoFrame = depthFrame->as<ob::VideoFrame>();
            OBFormat format = videoFrame->format();
            VALIDATE(format == OB_FORMAT_Y16, null_str);

            frame_width = videoFrame->width();
            frame_height = videoFrame->height();

            if (!depth_intrinsics_.valid && is_valid_OBCameraParam(camera_param_)) {
                calculate_intrinsics(true, frame_width, frame_height);
            }

            tdcframe_C& depth_frame = frames_[dcframeidx_depth];
            aplt::set_dcamera_frame(depth_frame, dcframetype_depth, dcformat_y16,
                frame_width, frame_height, (const uint8_t*)depthFrame->data(), depthFrame->dataSize(), depthFrame->getValueScale());
            receiver_->dcamera_did_frames(task_, &depth_frame, 1);

        } else if (task_ == dctask_d2c) {
            std::shared_ptr<ob::ColorFrame> colorFrame = frameSet->colorFrame();
            std::shared_ptr<ob::DepthFrame> depthFrame = frameSet->depthFrame();
            if (colorFrame == nullptr || depthFrame == nullptr) {
                if (colorFrame != nullptr) {
                    // SDL_Log("%u (1/2)colorFrame != nullptr and depthFrame == nullptr", SDL_GetTicks());
                } else if (depthFrame != nullptr) {
                    // SDL_Log("%u (2/2)colorFrame == nullptr and depthFrame != nullptr", SDL_GetTicks());
                }
                receiver_->dcamera_did_frames(task_, nullptr, 0);
                return;
            }

            OBFrameType type = colorFrame->type();
            VALIDATE(type == OB_FRAME_COLOR, null_str);

            auto videoFrame = colorFrame->as<ob::VideoFrame>();
            OBFormat format = videoFrame->format();
            // VALIDATE(format == OB_FORMAT_RGB, null_str);
            VALIDATE(format == OB_FORMAT_MJPEG, null_str);

            auto videoFrame2 = depthFrame->as<ob::VideoFrame>();
            OBFormat format2 = videoFrame2->format();
            OBFormat format3 = depthFrame->format();
            VALIDATE(format2 == OB_FORMAT_Y16, null_str);

            frame_width = videoFrame->width();
            frame_height = videoFrame->height();

            int frame_width2 = depthFrame->width();
            int frame_height2 = depthFrame->height();

            VALIDATE(frame_width == frame_width2, null_str);
            VALIDATE(frame_height == frame_height2, null_str);

            if (!color_intrinsics_.valid && is_valid_OBCameraParam(camera_param_)) {
                calculate_intrinsics(false, frame_width, frame_height);
            }

            if (!depth_intrinsics_.valid && is_valid_OBCameraParam(camera_param_)) {
                calculate_intrinsics(true, frame_width2, frame_height2);
            }

            // {color_frame_, depth_frame_};
            tdcframe_C& color_frame = frames_[dcframeidx_color];
            tdcframe_C& depth_frame = frames_[dcframeidx_depth];

            aplt::set_dcamera_frame(color_frame, dcframetype_color, dcformat_rgb,
                frame_width, frame_height, (const uint8_t*)videoFrame->data(), videoFrame->dataSize());

            aplt::set_dcamera_frame(depth_frame, dcframetype_depth, dcformat_y16,
                frame_width, frame_height, (const uint8_t*)depthFrame->data(), depthFrame->dataSize(), depthFrame->getValueScale());

            receiver_->dcamera_did_frames(task_, frames_, 2);

        } else {
            VALIDATE(false, null_str);
        }


    } else {
        // 
        receiver_->dcamera_did_frames(task_, nullptr, 0);
    }
}

void torbbec::main_stop()
{
    if (task_ == nposm) {
        return;
    }

    VALIDATE(pipe_.get() != nullptr, null_str);

    SDL_Log("{orbbec}g_pipe->stop() pre");
    pipe_->stop();

    SDL_Log("{orbbec}g_pipe.reset() pre");
    pipe_.reset();

    SDL_OrbbecDestroyContext();

    camera_param_.depthIntrinsic.fx = 0;
    depth_intrinsics_.valid = false;
    color_intrinsics_.valid = false;

    task_ = nposm;
}

void torbbec::calculate_intrinsics(bool depth, int width, int height)
{
    VALIDATE(camera_param_.depthIntrinsic.fx != 0, null_str);

    const OBCameraIntrinsic& ob_intrinsic = depth? camera_param_.depthIntrinsic: camera_param_.rgbIntrinsic;

    float fdx = ob_intrinsic.fx * ((float)(width) / ob_intrinsic.width);
    float fdy = ob_intrinsic.fy * ((float)(height) / ob_intrinsic.height);
    // fdx = 1 / fdx;
    // fdy = 1 / fdy;
    float u0 = ob_intrinsic.cx * ((float)(width) / ob_intrinsic.width);
    float v0 = ob_intrinsic.cy * ((float)(height) / ob_intrinsic.height);

    tdcintrinsics_C& result = depth? depth_intrinsics_: color_intrinsics_;
    result.cx = u0;
    result.cy = v0;
    result.fx = fdx;
    result.fy = fdy;

    result.valid = true;
}

bool torbbec::get_intrinsics(bool depth, tdcintrinsics_C& result)
{
    const tdcintrinsics_C& intrinsics = depth? depth_intrinsics_: color_intrinsics_;
    result = intrinsics;
    return result.valid;
}


aplt::tdcamera_device* orbbec_create_device()
{
    return new torbbec;
}