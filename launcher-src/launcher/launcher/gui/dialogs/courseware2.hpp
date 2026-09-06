#ifndef GUI_DIALOGS_COURSEWARE2_HPP_INCLUDED
#define GUI_DIALOGS_COURSEWARE2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/base_courseware.hpp"
#include "aplt.hpp"
#include "cfg_cpp_api.hpp"

#include "../Programs/TeXAndFriends/luatex/source/lua/luatex-api.h"

extern const std::string unfavorite_png;
extern const std::string favorited_png;

class tinstance_slot;

enum {rexam_type_choice, rexam_type_fillin, rexam_type_solving, rexam_type_other};
struct trexam_str_type {
	int type;
	char str[16];
	int size;
	char msgstr[24];
};

#define EXAM_TEX_COLS1		1
#define EXAM_TEX_COLS2		2
#define EXAM_DEF_TEX_COLS	EXAM_TEX_COLS2

class texam_problem
{
public:
	texam_problem(int type, int points, const std::string& question, const std::string& explanation)
		: type(type)
		, points(points)
		, question(question)
		, explanation(explanation)
		, tex_cols(EXAM_TEX_COLS2)
	{}

	void set_extra_choice(const std::vector<std::string>& _options, const std::set<int>& _correct_options, int _tex_cols)
	{
		options = _options;
		correct_options = _correct_options;
		tex_cols = _tex_cols;
	}

	bool valid() const { return type != nposm && points != nposm && !question.empty(); }

	bool solving_points_equaled() const
	{
		VALIDATE(type == rexam_type_solving, null_str);
		int subpoints = 0;
		for (std::vector<texam_problem>::const_iterator it = subproblems.begin(); it != subproblems.end(); ++ it) {
			const texam_problem& sub = *it;
			subpoints += sub.points;
		}
		return subpoints == points;
	}

	void clear()
	{
		type = nposm;
		points = nposm;
		question.clear();
		explanation.clear();
		options.clear();
		correct_options.clear();
		subproblems.clear();
	}

	std::string to_tex(int sub_at = nposm) const;

	std::string to_rose_msg(int sub_at = nposm) const;

public:
	int type;
	int points;
	std::string question;
	std::string explanation;
	std::vector<std::string> options;
	std::set<int> correct_options;
	int tex_cols;
	std::vector<texam_problem> subproblems;
};

struct tresizable_data
{
	tresizable_data()
		: data(nullptr)
		, size(0)
		, vsize(0)
	{}

	~tresizable_data()
	{
		if (data != nullptr) {
			free(data);
		}
	}

	void resize_data(int size, int vsize = 0);
	void append(const char* c_str, int l);
	void append_1ch(int ch);
	void drop_first(int size);

	char* data;
	int size;
	int vsize;
};

namespace gui2 {

class tbutton;
class tlistbox;
class treport;
class ttoggle_panel;
class ttoggle_button;
class ttree;
class ttree_node;
class tstack;
class tscroll_text_box;
class ttext_box2;
class timage;

class tcourseware2: public tdialog, public tstatusbar, public tbase_courseware
{
public:
	enum {UPLOAD_LAYER, DOWNLOAD_LAYER, COURSELIST_LAYER, SPACER_LAYER, BAR_REPORT_LAYERS = SPACER_LAYER};
	enum {MAIN_TREE_LAYER, MAIN_LIST_LAYER};
	enum {FIRST_LIST_LAYER, SECOND_LIST_LAYER};
	enum {INPUT_MSG_LAYER, INPUT_EXAM_LAYER, INPUT_BUILD_LAYER};
	tcourseware2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tinstance_slot& instance_slot, const std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		aplt::tcfg_cpp_api& cfg_cpp_api, tspeech_driver& speech_driver, const std::string& saves_courseware_dir);
	~tcourseware2();

	void lualatex_woutput_fputc(int ch);
	void lualatex_woutput_fputs_l(const char* c_str, int l);
	void lualatex_woutput_cr();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	bool courseware_dirty() const;
	void courseware_dirty_4_exam();

	std::string courseware_load_path() const override
	{
		VALIDATE(curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER || curr_layer_ == COURSELIST_LAYER, null_str);
		return curr_layer_ == UPLOAD_LAYER? upload_path_: download_path_;
	}

	bool use_map_courseware_files() const override { return curr_layer_ != COURSELIST_LAYER; }
	void assign_tmp_courseware();

	void load_cw_default();
	void get_cswamp_materials();

	bool did_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_report_item_changed(ttoggle_button& widget);
	void pre_input_toolbar_grid(tgrid& grid);
	void pre_input_msg_layer(tgrid& grid);
	void pre_input_exam_layer(tgrid& grid);
	void pre_input_build_layer(tgrid& grid);
	void pre_main_tree(tgrid& grid);
	void pre_main_list(tgrid& grid);

	void pre_first_second_list(tgrid& grid, bool first);

	void pre_toolbar_upload(tgrid& grid);
	void pre_toolbar_download(tgrid& grid);
	void pre_toolbar_courselist(tgrid& grid);

	bool generate_courseware_rsp(const std::string& desc, const std::string& courseware_name, const std::string& uuid, int64_t& rsp_ts);
	bool upload_courseware_rsp(const std::string& courseware_name, const std::string& uuid, int64_t rsp_ts);

	void refresh_title(const std::string& label);
	void refresh_courselist_item_label();
	void refresh_enter_edit_widget_label();
	void enter_or_exit_edit(bool from_click);

	void did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer);
	void reload_items_list(tlistbox& list, const std::vector<tcode3>& items);
	void clear_list_task();
	void get_finduser_result_items(const std::vector<net::tcswamp_finduser_result>& result, std::vector<tcode3>& items);
	void get_cswamp_user_items(const aplt::tcswamp_user& user, std::vector<tcode3>& items);
	void did_find_user_text_box_changed(ttext_box& widget);
	void click_find_user(tbutton& widget);
	void handle_list_state_users_bh(const net::tcswamp_finduser_result& user);
	void write_distribution_cfg(const std::string& path, const aplt::tcswamp_user& user, const aplt::tcswamp_material& material) const;
	void handle_list_state_cswamp_coursewares_bh(const aplt::tcswamp_material& material);
	void evaluate_uuids();

	void refresh_change_cw_type_label();
	void click_change_cw_type(tbutton& widget);
	void click_save(tbutton& widget);
	void click_upload_rsp(tbutton& widget);
	bool do_save_tex(const std::string& tex_file, bool student, bool show_dlg);

	void did_receive_woutput_cr(const char* data, int size);
	void build_finished(bool result);
	void OnWorkStart();
	void OnWorkDone(const std::string tex_file, const std::string title, int pdfver);
	void OnTriggerExit() {}
	void DoWork(const std::string tex_file, const tluatex_hook hook, bool& exit);
	bool is_building() const { return building_ != bool_set_none; }

	void click_pdf(tbutton& widget);
	bool if_dirty_confirm_save(const std::string& action);
	void click_back(tbutton& widget);
	void save_main_cfg(const std::string& filename, const aplt::tcourseware& courseware);
	uint64_t exam_is_valid(const aplt::tcourseware& courseware, std::string& err_msg) const;
	uint64_t courseware_is_valid(const aplt::tcourseware& courseware, std::string& err_msg) const;
	bool rename_courseware_dir(const tcourseware_file& courseware_file, const std::string& new_name);
	bool gui_courseware_is_valid(std::string* new_dir_name);
	bool do_save(tbutton& widget);
	bool verify_edit_courseware_title(const std::string& label, const std::string& initial, const std::set<std::string>& excludes) const;
	void click_insert_courseware(tbutton& widget);
	void click_erase_courseware(tlistbox& list);
	enum {insert_keypoint, insert_exercise};
	void post_tree_recreated(const tcookie3f& cookie3f);
	SDL_Point3 handle_clipboard_rstr(const char* c_str, int size);
	void click_ai_exercise(tbutton& widget, int type);
	void click_fill_default(tbutton& widget);
	void click_insert(tbutton& widget, int type);
	void click_move_down_or_up(tbutton& widget, bool down);
	void click_erase(tbutton& widget);
	void click_favorite(tbutton& widget);
	void click_enter_edit(tbutton& widget);
	void click_listen(tbutton& widget);

	// void collect_courseware(const std::string& dir, std::map<std::string, tcourseware_file>& files);
	void insert_courseware_list_row(const tcourseware_file& file, tlistbox& list, std::map<std::string, std::string>& data);
	void reload_courseware_list(tlistbox& list);
	bool did_file_list_row_pre_change(tlistbox& list, ttoggle_panel& row);
	void did_file_list_row_changed(tlistbox& list, ttoggle_panel& row);
	bool did_file_list_can_drag(tlistbox& list, ttoggle_panel& row);

	void load_courseware_cfg(const std::string& cfgfile);

	void keypoint_update_to_tree(ttree_node& branch, int index, const aplt::tcourseware::tkeypoint& keynote);
	void exercise_update_to_tree(ttree_node& branch, int index, const aplt::tcourseware::texercise& exercise);
	void courseware_update_to_tree(const aplt::tcourseware& courseware);
	tcookie3f courseware_update_to_tree_with_cursel(const aplt::tcourseware& courseware);
	void validate_tree_cookie() const;
	bool is_active_fill_default(const tcookie3f& cookie3f) const;
	void refresh_toolbar_active(const tcookie3f* cookie3f);
	void did_node_changed(ttree_node& node);
	void set_tree_node_label_from_cookie(ttree& tree, uint64_t cookie, const std::string& node_label);
	void did_tree_msg_text_changed(ttext_box& widget);

	void clear_tree2();
	void set_tree_msg_label(const std::string& label, const tcookie3f& cookie3f, bool read_only);
	void reload_input_exam();
	enum {fid_exam_prev, fid_exam_next, fid_exam_down, fid_exam_insert, fid_exam_erase, fid_exam_points, fid_exam_question,
		fid_exam_correct_option_a, fid_exam_correct_option_b, fid_exam_correct_option_c, fid_exam_correct_option_d, 
		fid_exam_option_a, fid_exam_option_b, fid_exam_option_c, fid_exam_option_d, 
		fid_exam_expl};
	void did_input_exam_changed_quited(bool dirty);
	void click_input_exam_type(tbutton& widget);
	void problem_swap(std::vector<texam_problem>& problems, int at1, int at2);
	void click_input_exam_prev_or_next(tbutton& widget, int fid);
	void click_input_exam_tex_cols(tbutton& widget);
	void click_input_exam_down_or_insert_or_erase(tbutton& widget, int fid);
	void did_input_exam_text_changed(ttext_box& widget, int fid);
	void did_input_exam_state_changed(ttoggle_button& widget, int fid);

	void set_status_label(const std::string& label, bool add_time = false);

	void build_timer_handler();
	void app_timer_handler(uint32_t now) override;

	struct tmsg_data_build_finished: public rtc::MessageData {
		explicit tmsg_data_build_finished(bool result)
			: result(result)
		{}

		const bool result;
	};
	enum {MSG_BUILD_FINISHED = POST_MSG_MIN_APP};
	void app_OnMessage(rtc::Message* msg) override;

private:
	tinstance_slot& instance_slot_;
	const std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	tspeech_driver& speech_driver_;
	const int64_t ancientcc_uid_;
	const std::string pdf_output_dir_;

	tlabel* title_widget_;
	tbutton* back_widget_;
	treport* bar_report_;
	tcontrol* courselist_item_widget_;
	tstack* main_stack_;
	tstack* toolbar_stack_;
	tlistbox* file_list_;
	ttree* tree_widget_;
	timage* v_line2_widget_;

	tstack* item_list_stack_;
	tlistbox* first_list_;
	tlistbox* second_list_;

	ttext_box2* find_user_widget_;
	tgrid* may_invisible_cw_tree_top_grid_;
	tbutton* insert_courseware_widget_;
	tbutton* tb_upload_change_cw_widget_;
	tbutton* save_widget_;
	tbutton* upload_widget_;
	tbutton* pdf_widget_;
	tbutton* ai_exercise_widget_;
	tbutton* fill_default_widget_;
	tbutton* insert_keypoint_widget_;
	tbutton* insert_exercise_widget_;
	tbutton* move_down_widget_;
	tbutton* move_up_widget_;
	tbutton* erase_widget_;
	tbutton* favorite_widget_;
	tbutton* enter_edit_widget_;
	tbutton* listen_widget_;

	tgrid* input_grid_;
	tgrid* input_toolbar_grid_;
	tstack* input_stack_;
	tscroll_text_box* tree_msg_widget_;
	tscroll_text_box* build_msg_widget_;

	tlabel* status_widget_;

	bool editing_;
	int curr_layer_;
	int curr_main_layer_;

	const int def_item_list_layer_;
	enum {listtasktype_find_user};
	int curr_list_task_type_;

	struct texam_default
	{
		std::string tex_header;
		std::string tex_tail;
		std::string content;
		std::string exercise_section;
		std::string exercise_answer;

		std::string prompt_header;
	};
	texam_default exam_default_;

	struct tignore_tree_msg_text_changed_lock
	{
		tignore_tree_msg_text_changed_lock(tcourseware2& courseware2)
			: courseware2_(courseware2)
		{
			VALIDATE(!courseware2_.ignore_tree_msg_text_changed_, null_str);
			courseware2_.ignore_tree_msg_text_changed_ = true;
		}
		~tignore_tree_msg_text_changed_lock()
		{
			VALIDATE(courseware2_.ignore_tree_msg_text_changed_, null_str);
			courseware2_.ignore_tree_msg_text_changed_ = false;
		}

		tcourseware2& courseware2_;
	};
	bool ignore_tree_msg_text_changed_;

	struct tignore_file_list_row_changed_lock
	{
		tignore_file_list_row_changed_lock(tcourseware2& courseware2)
			: courseware2_(courseware2)
		{
			VALIDATE(!courseware2_.ignore_file_list_row_changed_, null_str);
			courseware2_.ignore_file_list_row_changed_ = true;
		}
		~tignore_file_list_row_changed_lock()
		{
			VALIDATE(courseware2_.ignore_file_list_row_changed_, null_str);
			courseware2_.ignore_file_list_row_changed_ = false;
		}

		tcourseware2& courseware2_;
	};
	bool ignore_file_list_row_changed_;

	aplt::tcourseware courseware_;
	aplt::tcourseware tmp_courseware_;

	std::vector<texam_problem> tmp_problems_;
	struct tcurr_exam_problem
	{
		tcurr_exam_problem(std::vector<texam_problem>& problems)
			: problems(problems)
			, p(nullptr)
			, at(nposm)
			, sub_at(nposm)
		{}

		void set(int _at)
		{
			VALIDATE(_at >= 0 && _at < (int)problems.size(), null_str);
			at = _at;
			p = &problems[at];
			// to solving, when the 'sub_at == nposm', it means we are currently in a big-problem.
			// nposm's valud is -1, 'add 1' is 0, it is first subproblem.
			// sub_at = p->subproblems.empty()? nposm: 0;
			sub_at = nposm;
		}

		bool valid() const 
		{
			if (p != nullptr) {
				VALIDATE(at >= 0 && at < (int)problems.size(), null_str);
				return true;
			}
			VALIDATE(at == nposm, null_str);
			return false;
		}

		texam_problem& this_p()
		{
			if (p->type != rexam_type_solving || p->subproblems.empty()) {
				VALIDATE(sub_at == nposm, null_str);
				return *p;
			}

			VALIDATE(sub_at >= nposm && sub_at < (int)p->subproblems.size(), null_str);
			return sub_at == nposm? *p: p->subproblems[sub_at];
		}

		void clear()
		{
			p = nullptr;
			at = nposm;
		}

		std::vector<texam_problem>& problems;
		texam_problem* p;
		int at;
		int sub_at;
	};
	tcurr_exam_problem curr_problem_;
	struct tignore_input_exam_changed_lock
	{
		tignore_input_exam_changed_lock(tcourseware2& courseware2)
			: courseware2_(courseware2)
		{
			VALIDATE(!courseware2_.ignore_input_exam_changed_, null_str);
			courseware2_.ignore_input_exam_changed_ = true;
		}
		~tignore_input_exam_changed_lock()
		{
			VALIDATE(courseware2_.ignore_input_exam_changed_, null_str);
			courseware2_.ignore_input_exam_changed_ = false;
		}

		tcourseware2& courseware2_;
	};
	bool ignore_input_exam_changed_;

	enum {list_state_users, list_state_cswamp_coursewares, list_state_main_cfg};
	int list_state_;
	std::vector<net::tcswamp_finduser_result> curr_finduser_result_;
	aplt::tcswamp_user curr_cswamp_user_;

	std::map<std::string, aplt::tcswamp_material> cswamp_materials_;

	tresizable_data woutput_data_;
	rtc::Thread* main_;
	threading::mutex build_msg_mutex_;
	bool_set_t building_;
	std::unique_ptr<net::tworker> task_thread_;

	ttimer build_timer_;
#define MAX_MSG_LINES	50  // 50
	int line_bytes_[MAX_MSG_LINES];
	int valid_lines_;
	tresizable_data build_msg_;
};

} // namespace gui2

#endif

