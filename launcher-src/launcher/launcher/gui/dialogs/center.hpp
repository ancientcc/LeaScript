#ifndef GUI_DIALOGS_CENTER_HPP_INCLUDED
#define GUI_DIALOGS_CENTER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/base_courseware.hpp"
#include "base_driver.hpp"
#include "speech_driver.hpp"
#include "base_instance.hpp"
#include "rose_ros/aplt.hpp"

namespace aplt {
class tbg_task2;
}

class tros_map;
struct ttemp_task_type;

class tros_instance;
class tdcamera_driver;
class tai_driver;

namespace gui2 {

class tbutton;
class tlabel;
class tlistbox;
class tscroll_text_box;
class ttoggle_panel;
class tstack;
class ttrack;
class treport;
class ttoggle_button;
class ttext_box2;

class tcenter: public tdialog, public tstatusbar, public tbase_msg_subscriber, public tcamera::tviewer, public tbase_courseware
{
public:
	enum {AI_LAYER, NONAI_LAYER};
	enum {NONAI_HOME_LAYER, NONAI_CAMERA_LAYER};
	enum {POSITIONS_LAYER, TIMING_TASKS_LAYER};
	enum {TASK_TOOLBAR_EMPTY_LAYER, TASK_TOOLBAR_CHAT_LAYER, TASK_TOOLBAR_FOLLOWUP_LAYER,
		TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER};
	tcenter(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, aplt::tbg_task2& temp_task, const std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tb_api& b_api, 
		tros_map& curmap, aplt::tbg_task& bg_task, tbase_driver& base_driver, tspeech_driver& speech_driver, tdcamera_driver& dcamera_driver, tai_driver& ai_driver, const std::string& saves_courseware_dir,
		const tstart_aiagent* start_aiagent);
	~tcenter();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_first_drawn() override;

	void did_tasks_item_click(tbutton& widget);
	void did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer);

	void pre_home(tgrid& grid);
	void pre_camera(tgrid& grid);

	void click_back(tbutton& widget);
	void click_camera(tbutton& widget);

	bool did_ai2_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_ai2_report_item_changed(ttoggle_button& widget);
	void pre_position_list(tgrid& grid);
	void pre_timing_task_list(tgrid& grid);

	struct titem3 {
		titem3(int val, const std::string& id, const std::string& label)
			: val(val)
			, id(id)
			, label(label)
		{}
		const int val;
		const std::string id;
		const std::string label;
	};
	void reload_items_list(tlistbox& list, const std::vector<titem3>& items, bool truncate_major);

	struct tcand_task3 {
		tcand_task3(const aplt::tapplet& _aplt, const aplt::tapplet::ttask& _cfg_task, const std::string& _ble_device_id)
			: aplt(_aplt)
			, cfg_task(_cfg_task)
			, ble_device_id(_ble_device_id)
		{}
		
		const aplt::tapplet& aplt;
		const aplt::tapplet::ttask& cfg_task;
		std::string ble_device_id;
	};
	void get_task_items(bool task_cpp, bool task_aiagent, uint32_t type_mask, std::vector<tcand_task3>& timings, std::vector<titem3>& items) const;

	//
	// ai layer
	//
	void ai_click_new_conversation(tbutton& widget);

	void ai_pre_input(tscroll_text_box& input);
	void ai_signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode);
	void ai_enter_inputing(bool enter);
	void ai_do_send();
	void ai_click_send(tbutton& widget);

	void ai_pre_task_toolbar_empty(tgrid& grid);
	void ai_pre_task_toolbar_chat(tgrid& grid);
	void ai_pre_task_toolbar_followup(tgrid& grid);
	void ai_pre_task_toolbar_courseware_main_cfg(tgrid& grid);
	enum {help_clipboard_to_exam};
	void ai_click_help(tbutton& widget, int type);
	void ai_click_task_toolbar_clipboard_to_exam(tbutton& widget);
	void ai_did_new_conversation_always_changed(ttoggle_button& widget);
	void ai_click_task_toolbar_reference(tbutton& widget, int layer);
	void ai_click_task_toolbar_listen(tbutton& widget, int layer);

	void ai_did_find_user_text_box_changed(ttext_box& widget);
	void ai_click_find_user(tbutton& widget);
	void ai_handle_list_state_users_bh(const net::tcswamp_finduser_result& user);
	void ai_write_distribution_cfg(const std::string& path, const aplt::tcswamp_user& user, const aplt::tcswamp_material& material) const;
	void ai_handle_list_state_cswamp_coursewares_bh(const aplt::tcswamp_material& material);
	void ai_did_into_list_state_main_cfg();

	// robot panel
	void ai_reload_tasks();

	void ai_do_new_conversation();
	bool ai_can_switch(const std::string& action);
	void ai_did_tasks_item_click(tbutton& widget);

	void ai_did_load_task_api_quited(const std::string& err_msg);
	void ai_did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer);
	void ai_set_task_type(int type);
	void ai_set_tokens_label();
	void ai_set_task_label(const std::string& label);
	void ai_set_send_label(bool show_send);
	bool ai_send_label_is_send_png() const;

	void ai_clear_task();
	void ai_get_scene_items(const aplt::tai_slot::tscene* exclude, std::vector<titem3>& items) const;
	void ai_get_followup_prompt_items(aplt::tai_slot::tfollowup& followup, std::vector<aplt::tai_slot::tprompt>& prompts, std::vector<titem3>& items) const;
	void ai_get_download_courseware_items(std::vector<titem3>& items);
	void ai_get_finduser_result_items(const std::vector<net::tcswamp_finduser_result>& result, std::vector<titem3>& items);
	void ai_get_cswamp_user_items(const aplt::tcswamp_user& user, std::vector<titem3>& items);
	void ai_get_aiagent_prompt_items(std::vector<titem3>& items);

	int ai_courseware_2_main_cfg_list_rows(const aplt::tcourseware& courseware) const;

	std::string ai_get_label_from_list_row_cookie(const aplt::tcourseware& courseware, const tcookie3f& cookie3f) const;
	void ai_keynote_update_to_list(tlistbox& list, int index, const aplt::tcourseware::tkeypoint& keypoint);
	void ai_exercise_update_to_list(tlistbox& list, int index, const aplt::tcourseware::texercise& exercise);
	void ai_courseware_update_to_list(const aplt::tcourseware& courseware);
	void ai_validate_main_cfg_list_cookie() const;

	enum {ai_list_state_local_coursewares, ai_list_state_main_cfg, ai_list_state_users, ai_list_state_cswamp_coursewares};

	enum {ai_tasktype_chat, ai_tasktype_followup, ai_tasktype_courseware, ai_tasktype_aiagent};
	struct tai_task_type
	{
		tai_task_type(int type, const std::string& id, const std::string& name, const std::string& note)
			: type(type)
			, id(id) // 
			, name(name)
			, note(note)
		{}

		const int type;
		// A alias represented by a string. now use for locate image, path = "misc/" + id + ".png"
		const std::string id;
		const std::string name;
		const std::string note;
	};
	const tai_task_type& ai_task_from_type(int type) const;

	//
	// ai2 layer
	//
	void nonai_reload_tasks();

	void nonai_did_tasks_item_click(tbutton& widget);
	void nonai_did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer);

	void nonai_clear_task();
	void nonai_get_position_items(const tmap_position* exclude, std::vector<titem3>& items) const;
	bool nonai_can_clear_task() const;

	const ttemp_task_type& nonai_task_from_type(int type) const;

	// log list
	void reload_log_list(tlistbox& list);

	void reset_status();
	void set_status_task_label(int type, const std::string& desc);
	void camera_layer_back_home();
	void did_camera_layer_back_home();

	//
	// camera layer
	//
	bool camera_is_using() const;
	tcamera::tslot& get_camera_slot();

	void did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	
	//
	// tbase_msg_subscriber
	//
	void bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void logs_pb_log_added(int count, const pb2::tlog& log) override;
	std::string can_send_nlp_question2(int src) const override;
	void aiagent_did_nlp_answer2(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens) override;

	// tcamera::tslot
	void camera_post_enter_task_c() override;
	void camera_pre_exit_task_c() override;
	void camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	// void camera_work_frame_c(const surface& surf) override {}
	// void camera_post_switch_camera_c() override {}
	void camera_did_button_clicked_c(int msgid) override;

	//
	// tbase_courseware
	//
	// std::string courseware_load_path() const override
	// {
	//	return download_path_;
	// }

	void app_timer_handler(uint32_t now) override;
	enum {MSG_NONAI_CLEAR_TASK = POST_MSG_MIN_APP};

	struct tmsg_data_nonai_clear_task: public rtc::MessageData {
		explicit tmsg_data_nonai_clear_task(tcenter& _center)
			: center(_center)
		{
			VALIDATE(!center.nonai_task_clearing_, null_str);
			center.nonai_task_clearing_ = true;
		}

		~tmsg_data_nonai_clear_task()
		{
			VALIDATE(center.nonai_task_clearing_, null_str);
			center.nonai_task_clearing_ = false;
		}

		tcenter& center;
	};

	struct tmsg_data_select_timing_task: public rtc::MessageData {
		explicit tmsg_data_select_timing_task(tcenter& _center)
			: center(_center)
		{
			VALIDATE(!center.nonai_task_clearing_, null_str);
			center.nonai_task_clearing_ = true;
		}

		~tmsg_data_select_timing_task()
		{
			VALIDATE(center.nonai_task_clearing_, null_str);
			center.nonai_task_clearing_ = false;
		}

		tcenter& center;
	};

	void app_OnMessage(rtc::Message* msg) override;

private:
	tros_instance& ros_instance_;
	tdrivers& drivers_;
	tcamera& camera_;
	aplt::tbg_task2& temp_task_;
	const std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	aplt::tb_api& b_api_;
	tros_map& curmap_;
	aplt::tbg_task& bg_task_;
	tbase_driver& base_driver_;
	tspeech_driver& speech_driver_;
	tdcamera_driver& dcamera_driver_;
	tai_driver& ai_driver_;
	tstart_aiagent ai_start_aiagent_;
	aplt::tai_slot* ai_slot_;
	aplt::tpinyin& pinyin_;
	const bool ai_disable_find_user_;
	const int def_item_list_layer_;
	int curr_layer_;

	treport* ai2_report_;
	tbutton* camera_widget_;
	treport* task_report_;
	tstack* body_stack_;
	tlabel* status_widget_;
	tstack* item_list_stack_;
	tlistbox* position_list_;
	tlistbox* timing_task_list_;
	tlistbox* history_;
	ttrack* vrenderer_widget_;

	tgrid* ai_left_top_grid_;
	tlabel* ai_task_type_widget_;
	tlabel* ai_tokens_widget_;
	tlabel* ai_task_widget_;
	tbutton* ai_new_conversation_widget_;
	ttext_box2* ai_find_user_widget_;
	tgrid* ai_input_grid_;
	tscroll_text_box* ai_input_;
	ttext_box* ai_input_tb_;
	tspacer* ai_input_scale_;
	tspacer* ai_keyboard_spacer_;
	tstack* ai_task_toolbar_stack_;
	tbutton* ai_courseware_main_cfg_listen_widget_;
	tbutton* ai_send_widget_;
	tlistbox* ai_main_cfg_list_;

	//
	// ai layer
	//
	const std::map<int, tai_task_type> ai_task_types_;
	int ai_tasktype_aiagent_at_;

	const bool ai_simulate_mobile_;
	bool ai_no_keyboard_;
	const int def_input_scale_height_;

	bool ai_new_conversation_;
	int ai_input_tokens_;
	int ai_output_tokens_;
	bool ai_inputing_;
	int ai_curr_task_type_;
	
	aplt::tai_slot::tfollowup* ai_curr_followup_;
	std::vector<aplt::tai_slot::tprompt> ai_curr_followup_prompts_;
	aplt::tcourseware ai_curr_courseware_;
	std::vector<net::tcswamp_finduser_result> ai_curr_finduser_result_;
	aplt::tcswamp_user ai_curr_cswamp_user_;
	int ai_list_state_;

	struct taiagent_task {
		taiagent_task(const aplt::tapplet& _aplt, const std::string& _task_name)
			: aplt(_aplt)
			, task_name(_task_name)
		{}
		
		const aplt::tapplet& aplt;
		const std::string task_name;
	};
	std::vector<tcand_task3> ai_curr_timings_;
	const tcand_task3* ai_curr_task3_;
	aplt::ttask_api* ai_curr_aiagent_task_api_;
	std::vector<aplt::taiagent_api::tprompt> ai_curr_aiagent_prompts_;

	//
	// non-ai layer
	//
	int nonai_curr_layer_;

	aplt::treq_task nonai_req_task_;
	int nonai_curr_task_type_;
	
	//
	// non-ai layer
	//
	const tmap_position* nonai_curr_position_;
	const tcand_task3* nonai_curr_timing_;
	std::vector<tcand_task3> nonai_curr_timings_;

	bool nonai_task_clearing_;
};

} // namespace gui2

#endif

