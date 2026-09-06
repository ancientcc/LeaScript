#ifndef GUI_DIALOGS_IF_BLOCK2_HPP_INCLUDED
#define GUI_DIALOGS_IF_BLOCK2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "aplt2.hpp"
#include <rose_ros/utils.hpp>


enum {if_block_startup_state, if_block_finished, if_block_input_var, if_block_position, 
	if_block_minor_words, if_block_var_task, if_block_type_count};

std::string if_branch_do_to_string(const aplt::tif_branch& branch, int type, const aplt::ttask_cpp_pair& cpp_pair, const tros_map& curmap);
std::string tif_block_to_string(const aplt::tif_block& if_block, int type, const aplt::ttask_cpp_pair& cpp_pair, const tros_map& curmap);

extern std::string label_state_nposm;

namespace gui2 {

class tlistbox;
class ttoggle_panel;
class tbutton;
class tgrid;
class ttext_box;
class tstack;
class ttoggle_button;

class tif_block2: public tdialog, public tstatusbar
{
public:
	enum {FINISHED_LAYER, INPUT_VAR_LAYER, POSITION_LAYER, MINOR_WORDS_LAYER, VAR_TASK_LAYER};

	tif_block2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const std::set<aplt::taplt_var_pair>& vars, 
		const aplt::ttask_cpp_pair& pair, const tros_map& curmap, const std::string& title, 
		const aplt::tapplet::ttask* cfg_task, aplt::tif_block& if_block, int type);

	// only valid when return is twindow::OK
	bool get_dirty() const { return dirty_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_finished(tgrid& grid);
	void pre_input_var(tgrid& grid);
	void pre_position(tgrid& grid);
	void pre_minor_words(tgrid& grid);
	void pre_var_task(tgrid& grid);

	void click_back(tbutton& widget);
	void click_import(tbutton& widget);
	void click_export(tbutton& widget);

	void did_if_branch_changed(tlistbox& list, ttoggle_panel& row);
	void reload_if_branch_list(int desire_sel);
	void did_if_branchs_changed(int desire_sel);
	bool did_if_branchs_can_drag(tlistbox& list, ttoggle_panel& row);
	void click_downward_if_branch(tlistbox& list);
	void click_insert_if_judge(tlistbox& list);
	void click_insert_if_branch(tbutton& widget);
	void click_erase_if_branch(tlistbox& list);

	void reload_detial_list(const aplt::tif_branch& branch);

	bool did_verify_var_exp(const std::string& label, const std::string& initial) const;
	enum {type_logic, type_judge_op, type_var_exp, type_r_exp};
	void click_if_judge(ttoggle_panel& row, tbutton& widget, aplt::tif_judge& if_judge, int type);
	void did_var_r_val_text_changed(const std::string& label, aplt::tif_judge& if_judge);
	void click_erase_if_judge(ttoggle_panel& row, tbutton& widget);

	void click_do_startup_state(tbutton& widget, int type);
	void did_do_str_text_changed(ttext_box& widget, int type);
	void click_do_scroll_txt(tbutton& widget, ttext_box& text_widget);
	void click_do_edit_vars(tbutton& widget, int type);
	void did_do_execute_changed(ttoggle_button& widget, int type);

	void click_do_freq_val(tbutton& widget, int type);
	void click_do_position_map_position(tbutton& widget);

	void app_timer_handler(uint32_t now) override;

private:
	const std::set<aplt::taplt_var_pair>& vars_;
	const aplt::ttask_cpp_pair& cpp_pair_;
	const tros_map& curmap_;
	const std::string title_;
	const aplt::tapplet::ttask* cfg_task_;
	aplt::tif_block& orig_if_block_;
	aplt::tif_block if_block_;
	const int type_;
	bool dirty_;

	const std::string label_logic_nposm_;
	std::vector<std::string> freq_vals_;

	tlistbox* if_branch_list_;
	tgrid* detail_grid_;
	tlistbox* detail_list_;
	tstack* do_stack_;
	ttext_box* do_finished_str_widget_;
	tbutton* do_finished_to_state_widget_;

	tbutton* do_special_constant_widget_;
	ttext_box* do_input_var_widget_;
	ttext_box* do_position_str_widget_;
	tbutton* do_position_map_position_widget_;

	ttext_box* minor_words_widget_;

	ttoggle_button* do_var_task_execute_widget_;
};

} // namespace gui2

#endif

