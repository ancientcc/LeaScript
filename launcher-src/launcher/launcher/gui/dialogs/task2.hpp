#ifndef GUI_DIALOGS_TASK2_HPP_INCLUDED
#define GUI_DIALOGS_TASK2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/widgets/text_box2.hpp"

#include "base_instance.hpp"
#include <rose_ros/utils.hpp>
#include "cfg_cpp_api.hpp"


namespace gui2 {


class ttoggle_panel;
class ttree_node;
class ttext_box;
class tstack;
class ttree;

class ttask2: public tdialog, public tstatusbar
{
public:
	enum {VAR_VAL_LABEL_LAYER, VAR_VAL_EDITBOX_LAYER, VAR_VAL_DROPDOWN_LAYER, VAR_VAL_DLG_LAYER};

	ttask2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api, aplt::tbg_task& bg_task);
	~ttask2();
	posix_noncopyable(ttask2);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_var_val_label(tgrid& grid);
	void pre_var_val_editbox(tgrid& grid);
	void pre_var_val_dropdown(tgrid& grid);
	void pre_var_val_dlg(tgrid& grid);
	void set_status_label(const std::string& label, bool add_t = false);
	void did_var_val_text_changed(ttext_box& widget);
	ttree_node* did_async_task_aplt_or_task_changed(ttree& tree, const aplt::ttask_cpp_pair& pair, aplt::tcpp_api::tstate2& state2, const aplt::tapplet& new_aplt,
		ttree_node& aplt_or_task_node, const tcookie3f& cookie3f);
	void click_var_val_dropdown(tbutton& widget);
	void click_var_val_dlg_edit(tbutton& widget);
	void click_back(tbutton& widget);
	void click_import(tbutton& widget);
	void click_export(tbutton& widget);
	void click_insert_task(tbutton& widget);
	void click_erase_task(tbutton& widget);
	void click_save(tbutton& widget);
	enum {insert_state, insert_key_2_state};
	void click_insert(tbutton& widget, int type);
	void click_erase(tbutton& widget);
	void click_move_down(tbutton& widget);
	void click_task(tbutton& widget);

	bool show_if_block2_dlg(const aplt::ttask_cpp_pair& pair, const std::string& title, aplt::tif_block& if_block, int if_block_type);

	bool do_save(tbutton& widget, bool is_back);
	void new_task_pairs_loaded();
	void curr_tmp_pair_change_to(aplt::ttask_cpp_pair* pair_ptr);
	void pair_update_to_l_tree(const aplt::ttask_cpp_pair& pair);
	void pair_update_to_r_tree(const aplt::ttask_cpp_pair& pair);
	void pair_update_to_tree(const aplt::ttask_cpp_pair& pair);
	void validate_l_tree_cookie() const;
	void validate_r_tree_cookie() const;
	void req_task_update_to_tree(ttree_node& htvi_req_task, int index, const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2) const;
	void state2_update_to_tree(ttree_node& branch, int index, const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2) const;
	void key_2_state_update_to_tree(ttree_node& branch, int index, const aplt::ttask_cpp_pair& pair, const std::vector<std::string>& state_names, const aplt::tcpp_api::tkey_2_state& key_2_state) const;
	// enum {type_global, type_state2, type_key_2_state};
	// enum {field_typeself, field_id, field_name, field_reception_state, field_nonpreemptive, field_recoverable,
	//	field_startup_state, field_threshold_s, field_doing, field_finished,
	//	field_async_task, field_aplt_id, field_task_id, field_device_id, field_position1, field_position2,
	//	field_from_state, field_major_word, field_minor_words, field_strategy,

	//	field_input_min = 50, field_input_max = 99,
	// };

	void refresh_toolbar_active(const tcookie3f* cookie3f);
	void deselect_another_tree(bool l_tree);
	void did_node_changed(ttree_node& node);
	void curr_tmp_pair_id_changed();
	bool task_pairs_dirty() const;
	void did_erase_state(aplt::ttask_cpp_pair& pair, int erase_state, tcookie3f& result_cookie3f);
	void empty_val_stack();
	void select_r_tree_child_by_at(const aplt::ttask_cpp_pair& pair, int child_at, tcookie3f& result_cookie3f);
	// std::string get_field_str(int type, int field) const;
	std::string get_placeholder_msg(int type, int field) const;
	std::string get_remark_msg(int type, int field) const;
	std::string get_error_msg(const aplt::ttask_cpp_pair& pair, int type, int field) const;
	std::string get_reception_state_label(const std::vector<std::string>& state_names, int reception_state) const;

	std::string state_is_valid(const aplt::tif_block& state, int states, const std::string& field_str, bool allow_empty, bool allow_nposm, bool allow_same) const;
	// uint64_t task_cpp_is_valid(const aplt::ttask_cpp_pair& pair, std::string& err_msg) const;

	std::string get_node_label_global(const aplt::ttask_cpp_pair& pair, int field) const;
	std::string get_node_label_state2(const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2, int field) const;
	std::string get_node_label_key_2_state(const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tkey_2_state& key_2_state, int field) const;

	void app_timer_handler(uint32_t now) override;

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	const tros_map& curmap_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	std::map<std::string, aplt::ttask_cpp_pair>& task_pairs_;
	aplt::tbg_task& bg_task_;
	const std::string label_reception_state_nposm_;
	const std::string label_req_task_nposm_;
	const int startup_from_state_index_nposm_;
	const tcookie3f cookie3f_nposm_;
	const SDL_Range task_name_chars_range_;
	const SDL_Range threshold_s_range_;
	const std::string msgstr_notempty_and_utf8str_;
	const std::string msgstr_empty_or_utf8str_;
	aplt::tdisable_new_klink_task_lock disable_new_klink_task_lock_;
	aplt::tpinyin& pinyin_;
	int tone_;
	bool eng_lowercase_;

	std::map<std::string, aplt::ttask_cpp_pair> tmp_task_pairs_;
	aplt::ttask_cpp_pair* curr_tmp_pair_;

	tbutton* save_widget_;
	tbutton* insert_task_widget_;
	tbutton* erase_task_widget_;
	tbutton* insert_state_widget_;
	tbutton* insert_key_2_state_widget_;
	tbutton* move_down_widget_;
	tbutton* erase_widget_;
	tbutton* task_widget_;
	tstack* var_val_stack_;
	tlabel* var_name_widget_;
	tlabel* var_val_label_;
	ttext_box2* var_val_editbox_;
	tbutton* var_val_dropdown_;
	tlabel* var_val_dlg_label_;
	tbutton* var_val_dlg_edit_;
	tlabel* status_widget_;
	ttree* l_tree_;
	ttree* r_tree_;

	struct tignore_var_val_text_changed_lock
	{
		tignore_var_val_text_changed_lock(ttask2& task2)
			: task2_(task2)
		{
			VALIDATE(!task2_.ignore_var_val_text_changed_, null_str);
			task2_.ignore_var_val_text_changed_ = true;
		}
		~tignore_var_val_text_changed_lock()
		{
			VALIDATE(task2_.ignore_var_val_text_changed_, null_str);
			task2_.ignore_var_val_text_changed_ = false;
		}

		ttask2& task2_;
	};
	bool ignore_var_val_text_changed_;

	enum {async_task_type_nposm, async_task_type_aplt_task, async_task_count};
	std::map<int, std::string> async_task_types_; 
};

} // namespace gui2

#endif

