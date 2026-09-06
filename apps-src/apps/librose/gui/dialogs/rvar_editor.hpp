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


#ifndef GUI_DIALOGS_RVAR_EDITOR_HPP_INCLUDED
#define GUI_DIALOGS_RVAR_EDITOR_HPP_INCLUDED

// #include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "aplt2.hpp"
// #include <rose_ros/utils.hpp>

namespace gui2 {

class tlabel;
class tbutton;
class tlistbox;
class ttoggle_panel;

class trvar_editor: public tdialog // , public tstatusbar
{
public:
	struct tslot
	{
	public:
		tslot()
			: rvar_editor_(nullptr)
			, window_(nullptr)
		{}
		virtual ~tslot() {}

		virtual void rvar_editor_pre_show(twindow& window) {}
		virtual void rvar_did_dlg_close() {}
		virtual void rvar_timer_handler(uint32_t now) {}

		// virtual bool select_freq_val(const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias) const { return false; }
		virtual std::string get_freq_val_alias(const std::string& curr_val) const { return curr_val; }

	public:
		trvar_editor* rvar_editor_;
		twindow* window_;
	};

	trvar_editor(/*net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap, */ tslot& slot, const std::string& title, const std::string& remark,
		const aplt::tapplet::ttask* cfg_task, const std::vector<std::string>& special_constants, const std::string& name_prefix, std::map<std::string, std::string>& map_vals);
	virtual ~trvar_editor();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void click_back();
	void click_insert_var(tbutton& widget);
	void reload_var_list();

	void click_freq_val(tlistbox& list, ttoggle_panel& row, tbutton& widget);
	bool did_verify_var_val(const std::string& label, const std::string& initial) const;
	void click_edit_var_val(tlistbox& list, ttoggle_panel& row, tbutton& widget);
	void click_erase_var(tlistbox& list, ttoggle_panel& row);
	void did_var_val_changed(ttoggle_panel& row, const std::string& new_val);
	bool varify_new_var_name(const std::string& label) const;

	void app_timer_handler(uint32_t now) override;

private:
	// const tros_map& curmap_;
	tslot& slot_;
	const std::string title_;
	const std::string remark_;
	const aplt::tapplet::ttask* cfg_task_;
	const std::vector<std::string>& freq_vals_;
	const std::string name_prefix_;
	const std::map<std::string, std::string> original_map_vals_;
	std::map<std::string, std::string>& map_vals_;
	const bool keep_map_vals_name_;

	tlistbox* var_list_;
};

bool select_freq_val(const trvar_editor::tslot& slot, const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias);

class tsimple_var_editor_slot: public trvar_editor::tslot
{
public:
	tsimple_var_editor_slot()
	{}
};

} // namespace gui2

#endif

