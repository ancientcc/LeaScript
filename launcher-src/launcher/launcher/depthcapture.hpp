#ifndef LIBROSE_DEPTHCAPTURE_HPP_INCLUDED
#define LIBROSE_DEPTHCAPTURE_HPP_INCLUDED

#include "camera.hpp"
#include "dcamera_driver.hpp"


class tdcamera_slot: public tcamera::tslot
{
public:
	tdcamera_slot()
		: tcamera::tslot()
	{
		is_dcamera = true;
	}

	virtual void dcamera_did_OnFrame(int task, const tdcframe_C* frames, int count) {}
};

class tdepthcapture: public trtc_client, public aplt::tdcamera_slot::treceiver
{
public:
	tdepthcapture(tcamera& camera, tdcamera_driver& dcamera_driver, int task, int id, rtc::MessageHandler& dlg_handler, tadapter& adapter, const tpoint& desire_size = tpoint(nposm, nposm), const int desire_fps = nposm, bool idle_screen_saver = true);

	~tdepthcapture();

	struct td2c_pair
	{
		td2c_pair()
			: depth_data(nullptr)
			, depth_scale(0.0)
			, valid(false)
		{}

		~td2c_pair()
		{
			if (depth_data) {
				free(depth_data);
			}
		}

		void set(const uint8_t* _argb_data, const int16_t* _depth_data, double _depth_scale)
		{
			VALIDATE(!argb_mat.empty() && depth_data != nullptr, null_str);

			memcpy(argb_mat.data, _argb_data, argb_mat.cols * argb_mat.rows * 4);
			memcpy(depth_data, _depth_data, argb_mat.cols * argb_mat.rows * 2);
			depth_scale = _depth_scale;

			valid = true;
		}

		cv::Mat argb_mat;
		int16_t* depth_data;
		double depth_scale;
		bool valid;
	};

	class VideoRenderer2: public trtc_client::VideoRenderer
	{
	public:
		explicit VideoRenderer2(trtc_client& client, webrtc::VideoTrackInterface* track_to_render, const std::string& name, bool remote, int at, bool encode)
			: VideoRenderer(client, track_to_render, name, remote, at, encode)
			, depthcapture_(*static_cast<tdepthcapture*>(&client))
		{}

		void OnFrame(tdcamera_driver& dcamera_driver, int task, const tdcframe_C* frames, int count);
		const cv::Mat* desire_deliver_frame() override;

	public:
		td2c_pair realtime_d2c_;
		td2c_pair last_deliver_d2c_;

	private:
		tdepthcapture& depthcapture_;
	};

private:
	//
	// trtc_client
	//
	std::vector<std::string> recalculate_existing_cameras() override;
	void main_OnFrame() override;
	void snapshot_depth(bool use_dcpitch, const std::string& png, const std::string& depth_data_file) override;

	//
	// aplt::tdcamera_slot::treceiver
	//
    void dcamera_did_frames(int task, const tdcframe_C* frames, int count) override;

	bool snapshot_depth_rt(gui2::tprogress_& progress, const uint8_t* tex_pixels, int task, const tdcframe_C* frames, int count);

private:
	tcamera& camera_;
	tdcamera_driver& dcamera_driver_;
	const int task_;
	VideoRenderer2* sink_;

	std::string depth_png_;
	std::string depth_data_file_;
	bool snapshot_use_dcpitch_;
};


#endif
