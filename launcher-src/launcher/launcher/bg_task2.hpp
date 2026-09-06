#ifndef SYS_TASK_H_INCLUDED
#define SYS_TASK_H_INCLUDED


#include "sdl_utils.hpp"
#include "aplt.hpp"
#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include <rose_ros/aplt.hpp>
#include <rose_ros/vros.hpp>
#include "aplt_clazz.hpp"
#include "camera.hpp"
#include "ros_instance.hpp"
#include "cfg_cpp_api.hpp"

class tai_driver;

namespace aplt {

class tbg_task2: public aplt::tbg_task::tbase_bg_task2, public tcamera::tslot
{
public:
	tbg_task2(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers& drivers, tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api,
		aplt::tbg_task& bg_task, tcamera& camera, const tflite::tscript& def_script, const tflite::ttflite& def_tflite, tros_instance& ros_instance, 
		tbase_driver& base_driver, tdcamera_driver& dcamera_driver, tai_driver& ai_driver);
	~tbg_task2();

	class treq_task2: public aplt::treq_task
	{
	public:
		aplt::ttask_vars task_vars;
	};

	void set_req_task(const aplt::treq_task& task, const aplt::ttask_vars& task_vars);
	void request_single_aplt_task(const aplt::treq_task& req_task);
	std::string request_gui_aplt_task(const aplt::treq_task& req_task, const aplt::ttask_vars& task_vars,
		const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished);
	aplt::tbg_task::tbase_bg_task2* request_klink_aplt_task(const aplt::taplt_task& aplt_task, const aplt::ttask_vars& task_vars);
	void request_retry_aplt_task();
	// void disallowed() override;

	const aplt::treq_task& req_task() const { return req_task_; }
	void slice();
	void finish_task_camera_or_charge(bool when_stop_aplt_task, const tapplet& aplt, const aplt::tapplet::ttask& cfg_task, int task_type);

	bool in_task_cpp() const override { return task_cpp_aplt_ != nullptr; }
	bool in_pure_task_cpp() const override { return task_cpp_aplt_ != nullptr && aplt_task == nullptr; }
	aplt::ttask_pair task_cpp_pair() const override;

	bool task_cpp_from_link() const { return klink_cpp_aplt_task2 != nullptr;}

	const aplt::ttaskpoint* get_taskpoint() const override 
	{
		if (cfg_cpp_api_.has_taskpoint()) {
			return &cfg_cpp_api_.taskpoint();
		}
		return nullptr; 
	}

	const tapplet::ttask* get_aiagent_cfg_task() const override;

	void set_camera_api(aplt::tcamera_api* camera_api);
	aplt::tcamera_api* camera_api() const { return camera_api_; }

	void set_nonblock_api(aplt::tnonblock_api* nonblock_api);
	aplt::tnonblock_api* nonblock_api() const { return nonblock_api_; }

	std::string curr_base_scene_name(int& subtask_state) const;

	void did_scan_subscribed(const sensor_msgs::LaserScan& msg, SDL_2Point& charging);

	bool speech_did_recognition_result(const std::string& result);

	bool can_save_taskpoint(const aplt::taplt_task* new_klink_aplt_task) const;
	void save_taskpoint()
	{
		VALIDATE(klink_cpp_aplt_task2 != nullptr, null_str);
		VALIDATE(task_cpp_cfg_task_ != nullptr, null_str);
		cfg_cpp_api_.save_taskpoint(*klink_cpp_aplt_task2, *task_cpp_cfg_task_, task_vars_);
	}

private:
	std::string shedule();

	void aplt_task_pre_navigation() override;
	void aplt_task_navigation_stopped(bool nav2th, bool result) override;
	void task_finished(const aplt::taplt_task* aplt_task) override;

	void modify_state_in_shedule(int state) const;
	std::string single_task_can_start(const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task) const;
	std::string req_task_can_start() const;
	void cancel_req_task(const std::string& err_msg);
	void set_req_task_dirty(bool val);
	void set_klink_aplt_task_state(int type, int pb_at, int state);
	void set_klink_aplt_task_state(const aplt::taplt_task& aplt_task, int state);

	void nullptr_aplt_task();

	// tcamera::tslot
	void camera_post_enter_task() override
	{
		tcamera::tslot::camera_post_enter_task();
	}
	void camera_pre_exit_task() override
	{
		tcamera::tslot::camera_pre_exit_task();
	}
	void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override
	{
		tcamera::tslot::camera_did_draw_slice(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
	}

	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override
	{
		VALIDATE(camera_viewer_ != nullptr, null_str);
		VALIDATE(camera_api_ != nullptr, null_str);

		return camera_api_->camera_use_cv_frame(argb, cv_argb);
	}
	void camera_work_frame(const surface& surf) override;
	// void camera_post_switch_camera() override {}
	void camera_did_button_clicked(int msgid) override
	{
		tcamera::tslot::camera_did_button_clicked(msgid);
	}

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tdrivers& drivers_;
	tros_map& curmap_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	// aplt::tbg_task& bg_task_;
	tcamera& camera_;
	const tflite::tscript& def_script_; // owner is game_instance.
	const tflite::ttflite& def_tflite_; // owner is game_instance.

	tros_instance& ros_instance_;
	tbase_driver& base_driver_;
	tdcamera_driver& dcamera_driver_;
	tai_driver& ai_driver_;

	treq_task2 req_task_;
	bool req_task_dirty_;
	uint32_t task_require_start_ticks_;

	aplt::tcpp_api* task_cpp_api_;
	const aplt::tapplet* task_cpp_aplt_;
	const aplt::tapplet::ttask* task_cpp_cfg_task_;
	// int task_cpp_type_;
	// int task_cpp_pb_at_;
	aplt::taplt_task* retry_aplt_task_;

	aplt::tcamera_api* camera_api_;
	aplt::tnonblock_api* nonblock_api_;

	const aplt::taplt_task* klink_aplt_task_;
	class tklink_aplt_task_setter
	{
	public:
		tklink_aplt_task_setter(tbg_task2& temp_task, const aplt::taplt_task& klink_aplt_task)
			: temp_task_(temp_task)
		{
			VALIDATE(temp_task_.klink_aplt_task_ == nullptr, null_str);
			// VALIDATE(temp_task_.tmp_task_vars_.empty(), null_str);

			temp_task_.klink_aplt_task_ = &klink_aplt_task;
			// temp_task_.tmp_task_vars_ = task_vars;
		}

		~tklink_aplt_task_setter()
		{
			VALIDATE(temp_task_.klink_aplt_task_ != nullptr, null_str);

			temp_task_.klink_aplt_task_ = nullptr;
			// temp_task_.tmp_task_vars_.clear();
		}

	private:
		tbg_task2& temp_task_;
	};

	bool trigger_by_gui_;
	class ttrigger_by_gui_setter
	{
	public:
		ttrigger_by_gui_setter(tbg_task2& temp_task)
			: temp_task_(temp_task)
		{
			VALIDATE(!temp_task_.trigger_by_gui_, null_str);

			temp_task_.trigger_by_gui_ = true;
		}

		~ttrigger_by_gui_setter()
		{
			VALIDATE(temp_task_.trigger_by_gui_, null_str);

			temp_task_.trigger_by_gui_ = false;
		}

	private:
		tbg_task2& temp_task_;
	};


	std::function<void (const aplt::ttask_vars& task_vars)> did_requester_task_finished_;
	// tcamera::tcenter_slot* center_dlg_;
};

}

#endif

