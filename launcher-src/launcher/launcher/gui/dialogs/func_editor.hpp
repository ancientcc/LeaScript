/* $Id: mp_login.hpp 48879 2011-03-13 07:49:40Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/


#ifndef GUI_DIALOGS_FUNC_EDITOR_HPP_INCLUDED
#define GUI_DIALOGS_FUNC_EDITOR_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "aplt2.hpp"
#include <rose_ros/utils.hpp>

namespace gui2 {

class tlabel;
class tbutton;
class ttext_box;
class tlistbox;
class ttoggle_panel;

class tfunc_editor: public tdialog, public tstatusbar
{
public:
	tfunc_editor(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap, const std::vector<std::string>& special_constants, const std::string& initial);
	virtual ~tfunc_editor();

	std::string get_result() const 
	{
		VALIDATE(curr_func_ != nullptr, null_str);
		return aplt::curr_func->func_2_var_exp(*curr_func_, params_, false);
	}

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void insert_row(tlistbox& list, const aplt::tfunction_code& func, int at, const std::string& val);
	void reload_param_list();

	void click_func_id(tbutton& widget);
	void did_var_exp_text_changed(ttext_box& widget);

	void click_freq_val(tlistbox& list, ttoggle_panel& row, tbutton& widget);
	bool did_verify_param_val(const std::string& label, const std::string& initial) const;
	void click_edit_param(tlistbox& list, ttoggle_panel& row, tbutton& widget);
	void did_param_val_changed(ttoggle_panel& row, const std::string& new_val);
	void click_back();

	void app_timer_handler(uint32_t now) override;

private:
	const tros_map& curmap_;
	const std::vector<std::string>& freq_vals_;
	const std::string initial_;

	const aplt::tfunction_code* curr_func_;
	std::vector<std::string> params_;

	tbutton* func_id_widget_;
	tlabel* func_desc_widget_;
	tlistbox* param_list_;
	
	ttext_box* var_exp_widget_;
	uint32_t will_close_ticks_;

	struct tignore_text_changed_lock
	{
		tignore_text_changed_lock(tfunc_editor& editor, bool var_exp)
			: editor_(editor)
			, var_exp_(var_exp)
		{
			if (var_exp) {
				VALIDATE(!editor_.ignore_var_exp_text_changed_, null_str);
				editor_.ignore_var_exp_text_changed_ = true;

			} else {
				VALIDATE(!editor_.ignore_param_val_text_changed_, null_str);
				editor_.ignore_param_val_text_changed_ = true;
			}
		}
		~tignore_text_changed_lock()
		{
			if (var_exp_) {
				VALIDATE(editor_.ignore_var_exp_text_changed_, null_str);
				editor_.ignore_var_exp_text_changed_ = false;

			} else {
				VALIDATE(editor_.ignore_param_val_text_changed_, null_str);
				editor_.ignore_param_val_text_changed_ = false;
			}
		}

		tfunc_editor& editor_;
		bool var_exp_;
	};
	bool ignore_var_exp_text_changed_;
	bool ignore_param_val_text_changed_;

};

} // namespace gui2

#endif

