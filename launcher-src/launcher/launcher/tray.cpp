/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "launcher-lib"

#include "tray.hpp"
#include "base_instance.hpp"

void tray_entry_cb(void* userdata, SDL_TrayEntry* entry)
{
	ttray::tcb_userdata* p = reinterpret_cast<ttray::tcb_userdata*>(userdata);
	p->tray->handle_menu_entry(p->id, p->ctx);
}

void ttray::create(const surface& surf, const std::string& tooltip)
{
	VALIDATE(surf.get() != nullptr, null_str);
	VALIDATE(!tooltip.empty(), null_str);

	VALIDATE(tray_ == nullptr, null_str);
	tray_ = SDL_CreateTray(surf.get(), tooltip.c_str());
}

void ttray::destroy()
{
	VALIDATE(tray_ != nullptr, null_str);

	SDL_DestroyTray(tray_);
	tray_ = nullptr;
}

SDL_TrayMenu* ttray::insert_TrayMenu(int menu_id)
{	
	VALIDATE(tray_ != nullptr, null_str);

	SDL_TrayMenu* menu = SDL_CreateTrayMenu(tray_);
	malloc_TrayMenu_item(menu_id, *menu);

	return menu;
}

SDL_TrayMenu* ttray::insert_TraySubmenu(int menu_id, int pos, const std::string& label, int submenu_id)
{
	tobj_item& item = find_obj_item(menu_id);
	VALIDATE(item.type == objt_TrayMenu, null_str);

	SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(item.menu, pos, label.c_str(), SDL_TRAYENTRY_SUBMENU);

	SDL_TrayMenu* submenu = SDL_CreateTraySubmenu(entry);
	malloc_TrayMenu_item(submenu_id, *submenu);

	return submenu;
}

SDL_TrayEntry* ttray::insert_TrayEntry(int menu_id, int pos, const std::string& label, SDL_TrayEntryFlags flags, 
	int entry_id)
{
	tobj_item& item = find_obj_item(menu_id);
	VALIDATE(item.type == objt_TrayMenu, null_str);

	SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(item.menu, pos, label.c_str(), flags);
	malloc_TrayEntry_item(entry_id, *entry);

	tcb_userdata* userdata = malloc_cb_userdata(entry_id);
	SDL_SetTrayEntryCallback(entry, tray_entry_cb, userdata);

	return entry;
}

ttray::tcb_userdata* ttray::malloc_cb_userdata(int id)
{
	tcb_userdata* result = (tcb_userdata*)cb_userdata_pool_.append_1();
	// int s = sizeof(tcb_userdata);
	// VALIDATE(((size_t)result % s) == 0, null_str);

	result->tray = this;
	result->id = id;
	result->ctx = 0;

	return result;
}

void ttray::dump_cb_userdata_pool()
{
	SDL_Log("---cb_userdata_pool, data: %p, size: %i, vsize: %i---", cb_userdata_pool_.data, cb_userdata_pool_.size, cb_userdata_pool_.vsize);
	for (int at = 0; at < cb_userdata_pool_.vsize; at ++) {
		const ttray::tcb_userdata* user_data = (const ttray::tcb_userdata*)cb_userdata_pool_.data + at;
		SDL_Log("{dbg-pool}[%i/%i]cb_userdata, user_data: %p, tray: %p, id: %i", 
			at, cb_userdata_pool_.vsize, user_data, user_data->tray, user_data->id);
	}
	SDL_Log("------");
}

void ttray::dump_obj_items()
{
	SDL_Log("---obj_items, data: %p, size: %i, vsize: %i---", obj_items_.data, obj_items_.size, obj_items_.vsize);
	for (int at = 0; at < obj_items_.vsize; at ++) {
		const ttray::tobj_item* item = (const ttray::tobj_item*)obj_items_.data + at;
		if (item->type == objt_TrayMenu) {
			SDL_Log("{dbg-pool}[%i/%i]TrayMenu, id: %i, obj: %p", at, obj_items_.vsize, item->id, item->menu);

		} else if (item->type == objt_TrayEntry) {
			SDL_Log("{dbg-pool}[%i/%i]TrayEntry, id: %i, obj: %p", at, obj_items_.vsize, item->id, item->entry);
		}
	}
	SDL_Log("------");
}

ttray::tobj_item* ttray::malloc_obj_item(int id)
{
	for (int at = 0; at < obj_items_.vsize; at ++) {
		const ttray::tobj_item* item = (const ttray::tobj_item*)obj_items_.data + at;
		if (item->id == id) {
			VALIDATE(false, null_str);
		}
	}

	tobj_item* result = (tobj_item*)obj_items_.append_1();
	result->id = id;

	return result;
}

void ttray::malloc_TrayMenu_item(int id, SDL_TrayMenu& obj)
{
	ttray::tobj_item* result = malloc_obj_item(id);
	result->type = objt_TrayMenu;
	result->menu = &obj;

	// SDL_Log("{dbg-pool}malloc TrayMenu, data: %p, id: %i, obj: %p", obj_items_.data, id, &obj);
}

void ttray::malloc_TrayEntry_item(int id, SDL_TrayEntry& obj)
{
	ttray::tobj_item* result = malloc_obj_item(id);
	result->type = objt_TrayEntry;
	result->entry = &obj;

	// SDL_Log("{dbg-pool}malloc TrayEntry, data: %p, id: %i, obj: %p", obj_items_.data, id, &obj);
}

ttray::tobj_item& ttray::find_obj_item(int id)
{
	for (int at = 0; at < obj_items_.vsize; at ++) {
		tobj_item* item = (tobj_item*)obj_items_.data + at;
		if (item->id == id) {
			return *item;
		}
	}

	VALIDATE(false, null_str);
	return *(ttray::tobj_item*)obj_items_.data;
}

ttray::tcb_userdata& ttray::find_cb_userdata(int id)
{
	for (int at = 0; at < cb_userdata_pool_.vsize; at ++) {
		tcb_userdata* userdata = (tcb_userdata*)cb_userdata_pool_.data + at;
		if (userdata->id == id) {
			return *userdata;
		}
	}

	VALIDATE(false, null_str);
	return *(tcb_userdata*)cb_userdata_pool_.data;
}

/*
void ttray::free_tray_objs()
{
	for (int at = obj_items_.vsize - 1; at >= 0; at --) {
		const ttray::tobj_item* item = (const ttray::tobj_item*)obj_items_.data + at;
		if (item->type == objt_TrayMenu) {
		} else if (item->type == objt_TrayMenu) {
		} else {
			VALIDATE(false, null_str);
		}
	}
}
*/

void ttray::quit_app()
{
	instance->handle_app_event(SDL_QUIT);
}