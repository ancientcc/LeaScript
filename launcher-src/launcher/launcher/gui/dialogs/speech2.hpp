#ifndef GUI_DIALOGS_SPEECH2_HPP_INCLUDED
#define GUI_DIALOGS_SPEECH2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
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

class tspeech2: public tdialog, public tstatusbar
{
public:
	enum {VAR_VAL_LABEL_LAYER, VAR_VAL_EDITBOX_LAYER, VAR_VAL_DROPDOWN_LAYER};

	tspeech2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api, aplt::tbg_task& bg_task);
	~tspeech2();
	posix_noncopyable(tspeech2);

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
	void set_status_label(const std::string& label, bool add_t = false) const;
	void did_nick_changed(ttext_box& widget);
	void did_var_val_text_changed(ttext_box& widget);
	void click_var_val_dropdown(tbutton& widget);
	void click_back(tbutton& widget);
	void click_import(tbutton& widget);
	void click_export(tbutton& widget);
	void click_insert_task(tbutton& widget);
	void click_erase_task(tbutton& widget);
	void click_save(tbutton& widget);
	enum {insert_var};
	void click_insert(tbutton& widget, int type);
	void click_erase(tbutton& widget);
	void click_task(tbutton& widget);

	bool do_save(tbutton& widget, bool is_back);
	void new_speech_sensors_loaded();
	void curr_tmp_sensor_change_to(aplt::tspeech_sensor* sensor_ptr);
	void sensor_update_to_l_tree(const aplt::tspeech_sensor& sensor);
	void sensor_update_to_r_tree(const aplt::tspeech_sensor& sensor);
	void sensor_update_to_tree(const aplt::tspeech_sensor& sensor);
	void validate_l_tree_cookie() const;
	void validate_r_tree_cookie() const;
	void varabile_update_to_tree(ttree_node& branch, int index, const aplt::tspeech_sensor::tvar& var);
/*
	enum {type_global, type_var};
	enum {field_typeself, field_id, field_name, field_countdown_s, field_first_words, 
		field_major_word, field_minor_words, field_strategy,
		field_optional, field_aplt_id, field_first_words_is_prefix, field_prefix_words, field_postfix_words
	};
*/
	void refresh_toolbar_active(const tcookie3f* cookie3f);
	void deselect_another_tree(bool l_tree);
	void did_node_changed(ttree_node& node);
	void curr_tmp_sensor_id_changed();
	bool speech_sensors_dirty() const;
	void empty_val_stack();
	void select_r_tree_child_by_at(const aplt::tspeech_sensor& sensor, int child_at, tcookie3f& result_cookie3f);
	// std::string get_field_str(int type, int field) const;
	std::string get_placeholder_msg(int type, int field) const;
	std::string get_remark_msg(int type, int field) const;
	std::string get_error_msg(const aplt::tspeech_sensor& sensor, int type, int field) const;

	// uint64_t speech_sensor_is_valid(const aplt::tspeech_sensor& sensor, std::string& err_msg) const;

	std::string get_node_label_global(const aplt::tspeech_sensor& sensor, int field) const;
	std::string get_node_label_var(const aplt::tspeech_sensor::tvar& var, int field) const;

	void app_timer_handler(uint32_t now) override;

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	const tros_map& curmap_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	std::string& nick_;
	std::map<std::string, aplt::tspeech_sensor>& speech_sensors_;
	aplt::tbg_task& bg_task_;
	const std::string label_req_task_nposm_;
	const int startup_from_state_index_nposm_;
	const tcookie3f cookie3f_nposm_;
	const SDL_Range task_name_chars_range_;
	const SDL_Range countdown_s_range_;
	const std::string msgstr_notempty_and_utf8str_;
	const std::string msgstr_empty_or_utf8str_;
	aplt::tdisable_new_klink_task_lock disable_new_aplt_lock_;

	std::string tmp_nick_;
	std::map<std::string, aplt::tspeech_sensor> tmp_speech_sensors_;
	aplt::tspeech_sensor* curr_tmp_sensor_;
	bool show_py_key_;

	tbutton* save_widget_;
	ttext_box* nick_widget_;
	tbutton* insert_task_widget_;
	tbutton* erase_task_widget_;
	tbutton* insert_var_widget_;
	tbutton* erase_widget_;
	tbutton* task_widget_;
	tstack* var_val_stack_;
	tlabel* var_name_widget_;
	tlabel* var_val_label_;
	ttext_box2* var_val_editbox_;
	tbutton* var_val_dropdown_;
	tlabel* status_widget_;
	ttree* l_tree_;
	ttree* r_tree_;

	struct tignore_var_val_text_changed_lock
	{
		tignore_var_val_text_changed_lock(tspeech2& speech2)
			: speech2_(speech2)
		{
			VALIDATE(!speech2_.ignore_var_val_text_changed_, null_str);
			speech2_.ignore_var_val_text_changed_ = true;
		}
		~tignore_var_val_text_changed_lock()
		{
			VALIDATE(speech2_.ignore_var_val_text_changed_, null_str);
			speech2_.ignore_var_val_text_changed_ = false;
		}

		tspeech2& speech2_;
	};
	bool ignore_var_val_text_changed_;
};

} // namespace gui2

#endif

