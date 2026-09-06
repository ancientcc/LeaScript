/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_COMBO_BOX2_HPP_INCLUDED
#define GUI_DIALOGS_COMBO_BOX2_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>


namespace gui2 {

class tbutton;
class tlistbox;

class tcombo_box2 : public tdialog
{
public:
	tcombo_box2(const std::string& title, const std::string& remark, const std::vector<tcode2>& items, int initial_sel, bool allow_cancel, const std::set<int>* initial_sels = nullptr);

	int cursel() const 
	{
		VALIDATE(!is_multiselect(), null_str);
		return cursel_; 
	}
/*
	bool dirty() const 
	{ 
		VALIDATE(!is_multiselect(), null_str);
		return cursel_ != initial_sel_; 
	}
*/
	const std::set<int>& cursels() const 
	{
		VALIDATE(is_multiselect(), null_str);
		return cursels_; 
	}

	// void set_did_item_changed(const std::function<void (tlistbox& list, int cursel)>& did)
	//	{ did_item_changed_ = did; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	bool is_multiselect() const { return initial_sels_ != nullptr; }
	void click_back(tbutton& widget);

	void item_selected(twindow& window, tlistbox& list);

private:
	tbutton* cancel_widget_;

	const std::string title_;
	const std::string remark_;
	std::vector<tcode2> items_;
	int initial_sel_;
	bool allow_cancel_;
	const std::set<int>* initial_sels_;

	int row_at_;
	std::set<int> row_ats_;

	int cursel_;
	std::set<int> cursels_;

	std::function<void (tlistbox& list, int cursel)> did_item_changed_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
