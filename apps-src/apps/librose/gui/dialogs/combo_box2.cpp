/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/combo_box2.hpp"

#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_panel.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, combo_box2)

tcombo_box2::tcombo_box2(const std::string& title, const std::string& remark, const std::vector<tcode2>& items, 
	int initial_sel, bool allow_cancel, const std::set<int>* initial_sels)
	: title_(title)
	, remark_(remark)
	, items_(items)
	, initial_sel_(initial_sel)
	, allow_cancel_(allow_cancel)
	, initial_sels_(initial_sels)
	, row_at_(nposm)
	, cursel_(nposm)
{
	VALIDATE(!items.empty(), null_str);
	std::set<int> codes;
	int at = 0;
	for (std::vector<tcode2>::const_iterator it = items_.begin(); it != items_.end(); ++ it, at ++) {
		const tcode2& item = *it;
		VALIDATE(item.code >= 0, null_str);
		VALIDATE(codes.count(item.code) == 0, str_cast(item.code));
		codes.insert(item.code);

		if (initial_sels_ == nullptr) {
			if (item.code == initial_sel_) {
				row_at_ = at;
			}
		} else {
			if (initial_sels_->count(item.code) != 0) {
				row_ats_.insert(at);
			}
		}
	}

	if (initial_sels_ == nullptr) {
		if (row_at_ == nposm) {
			VALIDATE(initial_sel == nposm, null_str);
			row_at_ = 0;
		}

	} else {
		VALIDATE(initial_sel == nposm, null_str);
		VALIDATE(row_ats_.size() == initial_sels_->size(), null_str);
	}
}

void tcombo_box2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tlabel* title = find_widget<tlabel>(window_, "title", false, true);
	title->set_label(title_);

	if (!remark_.empty()) {
		find_widget<tlabel>(window_, "remark", false, true)->set_label(remark_);
	}

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcombo_box2::click_back
			, this, std::ref(*button)));

	cancel_widget_ = find_widget<tbutton>(window_, "cancel", false, true);
	if (!allow_cancel_) {
		cancel_widget_->set_visible(twidget::HIDDEN);
	}

	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	window_->keyboard_capture(&list);
	if (is_multiselect()) {
		list.enable_select(false);
		list.enable_multiselect(true);
	}

	std::map<std::string, std::string> data;

	int at = 0;
	for (std::vector<tcode2>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const tcode2& item = *it;
		
		data["label"] = item.id;
		list.insert_row(data);

		at ++;
	}

	if (!is_multiselect()) {
		list.set_did_row_changed(std::bind(&tcombo_box2::item_selected, this, std::ref(*window_), _1));
		list.select_row(row_at_);

	} else {
		for (std::set<int>::const_iterator it = row_ats_.begin(); it != row_ats_.end(); ++ it) {
			int at = *it;
			list.select_row(at);
		}
	}
}

void tcombo_box2::post_show()
{
}

void tcombo_box2::item_selected(twindow& window, tlistbox& list)
{
	VALIDATE(!is_multiselect(), null_str);
/*
	ttoggle_panel* selected = list.cursel();
	cursel_ = selected? selected->at(): nposm;

	if (did_item_changed_) {
		did_item_changed_(list, cursel_);
	}
*/
}

void tcombo_box2::click_back(tbutton& widget)
{
	VALIDATE(cursel_ == nposm, null_str);
	VALIDATE(cursels_.empty(), null_str);

	bool dirty = false;
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	if (!is_multiselect()) {
		ttoggle_panel* selected = list.cursel();
		VALIDATE(selected, null_str);
		cursel_ = items_[selected->at()].code;
		dirty = cursel_ != initial_sel_;

	} else {
		const std::set<int>& rows = list.multiselected_rows();
		for (std::set<int>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
			int at = *it;
			cursels_.insert(items_[at].code);
		}
		dirty = cursels_ != *initial_sels_;
	}

	window_->set_retval(dirty? twindow::OK: twindow::CANCEL);
}

}
