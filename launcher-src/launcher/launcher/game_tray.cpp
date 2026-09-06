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
#ifdef _WIN32

#define GETTEXT_DOMAIN "launcher-lib"

#include "game_tray.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "shellapi.h"
#include "video.hpp"
#include "font.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/dialogs/rqrcode.hpp"

using namespace std::placeholders;


#define TRAYMENUID_MAIN		10

// tray window
#define TRAYMENUITEMID_WINDOW_CLOSE_CURRENT_ALERT	1100
#define TRAYMENUITEMID_WINDOW_HIDE		1200

// scene
#define TRAYMENUITEMID_SCENE_IDLE		2100
#define TRAYMENUITEMID_SCENE_SWITCH		2200
#define TRAYMENUITEMID_SCENE_SUBMENU_0	2210

// privacy
#define TRAYMENUITEMID_PRIVACY_ENABLE_PRIVACY	3100
#define TRAYMENUITEMID_PRIVACY_START_LISTEN		3200
#define TRAYMENUITEMID_PRIVACY_NEXT_COURSEWARE	3300
#define TRAYMENUITEMID_PRIVACY_STOP_SPEAK		3400

// checkbox
#define TRAYMENUITEMID_CHK_VOICE_SPEAK	5000

// system
#define TRAYMENUITEMID_SYS_HELP			6100
#define TRAYMENUITEMID_HELP_USERGUIDE	6110
#define TRAYMENUITEMID_HELP_FEEDBACK	6120
#define TRAYMENUITEMID_HELP_ONLINE_DOC	6130

#define TRAYMENUITEMID_SYS_QUIT			6200

#define CTX_HIDE_WINDOW		1
#define CTX_SHOW_WINDOW		2

#define CTX_IDLE_SCENE		1
#define CTX_RESUME_SCENE	2

#define CTX_DISABLE_PRIVACY	0
#define CTX_ENABLE_PRIVACY	1

#define CTX_START_LISTEN	0
#define CTX_STOP_LISTEN		1


int calculate_best_font_size(int max_width, const std::string& msg)
{
	int font_size = font::SIZE_DEFAULT;
	while (font_size >= font::SIZE_SMALLER) { // font::SIZE_SMALLEST
		tpoint size = font::get_rendered_text_size(msg, INT32_MAX, font_size);
		if (size.x < max_width) {
			return font_size;
		}
		font_size -= 1;
	}
	return font_size;
}

void ttray2::ttask::set(int max_width, const std::string& _msg, int _duration_ms)
{
	// SDL_Log("%u {dbg-tray-render}ttask::set, msg: %s, duration_ms: %i", SDL_GetTicks(), _msg.c_str(), _duration_ms);
	VALIDATE(!_msg.empty(), null_str);

	msg = _msg;
	duration_ms = _duration_ms;
	blink = duration_ms != nposm;
	font_size = calculate_best_font_size(max_width, msg);
	VALIDATE(font_size > 0, null_str);
	uint32_t now = SDL_GetTicks();
	start_ticks = now;

	// Even for persistent task, frist time it is displayed, 
	// it rely on next_blink_ticks, and immediately after that, next_blink_ticks are set to 0.
	next_blink_ticks = now;
	is_hidden2 = true;
}

ttray2::ttray2(CVideo& video)
	: ttray(video)
	, b_api_(aplt::get_b_api())
	, pinyin_(aplt::get_curr_pinyin())
	, scene_obj_item_start_(nullptr)
	, scene_userdata_start_(nullptr)
	, first_coordinate_(construct_null_coordinate())
	, last_task_()
	, persistent_task_()
	, tray_bg_color_({30, 30, 30, 178}) // RGBA: 30, 30, 30, 0.7
	, is_session_first_render_(true)
	, next_render_idle_ticks_(0)
	, font_size_(nposm)
{
}

// draw float window

void ttray2::render_tray_window_idle()
{
	// SDL_Log("%u {dbg-tray-render}render idle", SDL_GetTicks());
	ttray_window& w2 = video_.tray_window();

	SDL_SetRenderTarget(w2.renderer2, w2.frameTexture2.get());
    
	// const SDL_Color bg_color_idle{30, 30, 30, 0}; // RGBA: {30, 30, 30, 0.7}

	// const SDL_Color bg_color_idle{30, 30, 30, 178}; // RGBA: {30, 30, 30, 0.7}
	// const SDL_Color bg_color_idle{0, 0, 0, 255}; // RGBA: {30, 30, 30, 0.7}
	const SDL_Color bg_color_idle{255, 255, 255, 180}; // RGBA: {30, 30, 30, 0.7}

	SDL_SetRenderDrawColor(w2.renderer2, bg_color_idle.r, bg_color_idle.g, bg_color_idle.b, bg_color_idle.a);
    SDL_RenderClear(w2.renderer2);

	const std::string msg = utils::format_time_hms(time(nullptr));

	int font_size = font::SIZE_DEFAULT;
	// SDL_Color font_color{0, 255, 0, 255};
	SDL_Color font_color{51, 51, 51, 255};
	surface text_surf = font::get_rendered_text(msg, 0, font_size, font_color);
	texture tex = SDL_CreateTextureFromSurface2(w2.renderer2, text_surf);

	int width2, height2;
	SDL_QueryTexture(tex.get(), NULL, NULL, &width2, &height2);

	SDL_Rect draw_rect{0, 0, w2.width, w2.height};

	SDL_Rect dstrect{draw_rect.x + (draw_rect.w - width2) / 2, 
		draw_rect.y + (draw_rect.h - height2) / 2, width2, height2};
	SDL_RenderCopy(w2.renderer2, tex.get(), nullptr, &dstrect);
/*
    SDL_SetRenderDrawColor(w2.renderer2, 255, 255, 255, 255);
    SDL_Rect border = {0, 0, w2.width, w2.height};
    SDL_RenderDrawRect(w2.renderer2, &border);
    
    SDL_Rect closeBtn = {w2.width - 25, 5, 20, 20};
    SDL_SetRenderDrawColor(w2.renderer2, 255, 0, 0, 255);
    SDL_RenderFillRect(w2.renderer2, &closeBtn);
*/    
	w2.flip();
}

extern void force_window_on_top_native(SDL_Window* window);

#include <SDL_syswm.h>
void SDL_ShowWindowWithoutFocus(SDL_Window* window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        // Show the window using the SW_SHOWNOACTIVATE flag; the window will not be activated.
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
}

void SDL_ShowWindowWithoutFocusATop(SDL_Window* window)
{
	SDL_ShowWindowWithoutFocus(window);
	force_window_on_top_native(window);
}

void ttray2::render_tray_window(const std::string& msg, int font_size, const SDL_Color& font_color)
{
	// SDL_Log("%u {dbg-tray-render}render, msg: %s", SDL_GetTicks(), msg.c_str());
	ttray_window& w2 = video_.tray_window();

	if (is_session_first_render_) {
		if (w2.is_hidden) {
			SDL_ShowWindowWithoutFocusATop(w2.window);
		}
		is_session_first_render_ = false;
	}

	SDL_SetRenderTarget(w2.renderer2, w2.frameTexture2.get());
    
    const SDL_Color bg_color{255, 85, 85, 204}; // RGBA: {255, 85, 85, 0.8}
	SDL_SetRenderDrawColor(w2.renderer2, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderClear(w2.renderer2);

	if (!msg.empty()) {
		VALIDATE(font_size > 0, null_str);
		surface text_surf = font::get_rendered_text(msg, 0, font_size, font_color);
		texture temperature_tex_ = SDL_CreateTextureFromSurface2(w2.renderer2, text_surf);

		int width2, height2;
		SDL_QueryTexture(temperature_tex_.get(), NULL, NULL, &width2, &height2);

		SDL_Rect draw_rect{0, 0, w2.width, w2.height};

		SDL_Rect dstrect{draw_rect.x + (draw_rect.w - width2) / 2, 
			draw_rect.y + (draw_rect.h - height2) / 2, width2, height2};
		SDL_RenderCopy(w2.renderer2, temperature_tex_.get(), nullptr, &dstrect);
	}
/*
    SDL_SetRenderDrawColor(w2.renderer2, 255, 255, 255, 255);
    SDL_Rect border = {0, 0, w2.width, w2.height};
    SDL_RenderDrawRect(w2.renderer2, &border);
    
    SDL_Rect closeBtn = {w2.width - 25, 5, 20, 20};
    SDL_SetRenderDrawColor(w2.renderer2, 255, 0, 0, 255);
    SDL_RenderFillRect(w2.renderer2, &closeBtn);
*/    
	w2.flip();
}

void ttray2::render_tray_window_direct()
{
	ttray_window& w2 = video_.tray_window();
    SDL_SetRenderTarget(w2.renderer2, NULL);
    
    // SDL_SetRenderDrawColor(fw->renderer, 0, 120, 215, 200);
	SDL_SetRenderDrawColor(w2.renderer2, 0, 120, 215, 40);
    SDL_RenderClear(w2.renderer2);
    
    // SDL_SetRenderDrawColor(fw->renderer, 255, 255, 255, 255);
	SDL_SetRenderDrawColor(w2.renderer2, 0, 255, 0, 255);
    SDL_Rect border = {0, 0, w2.width, w2.height};
    SDL_RenderDrawRect(w2.renderer2, &border);
    
    SDL_Rect closeBtn = {w2.width - 25, 5, 20, 20};
    SDL_SetRenderDrawColor(w2.renderer2, 255, 0, 0, 255);
    SDL_RenderFillRect(w2.renderer2, &closeBtn);
    
    SDL_RenderPresent(w2.renderer2);
}

void ttray2::create_tray_window()
{
	SDL_Window* parent = get_sdl_window();

	SDL_Range w_range{8 * font::SIZE_DEFAULT, 10 * font::SIZE_DEFAULT};
	SDL_Range h_range{2 * font::SIZE_DEFAULT, 3 * font::SIZE_DEFAULT};

	int win_x;
	int win_y;
    SDL_GetWindowPosition(parent, &win_x, &win_y);
	// SDL_Rect rect = preferences::tray_window_rect(SDL_Range{240, 360}, SDL_Range{80, 120});
	SDL_Rect rect = preferences::tray_window_rect(w_range, h_range);
	rect.x -= win_x;
	rect.y -= win_y;

	ttray_window& w2 = video_.tray_window();
	w2.create(*parent, rect.x, rect.y, rect.w, rect.h);

	if (!preferences::tray_window_hidden()) {
		if (w2.is_hidden) {
			w2.is_hidden = false;
			SDL_ShowWindowWithoutFocusATop(w2.window);
		}
		render_tray_window_idle();

	} else if (!w2.is_hidden) {
		w2.is_hidden = true;
		SDL_HideWindow(w2.window);
	}
}

#define MAX_BASE_SCENES		20
void ttray2::create_main_menu()
{
	create_tray_window();
	tray::window->set_slot(this);

	VALIDATE(tray_ != nullptr, null_str);

	SDL_TrayMenu* menu = insert_TrayMenu(TRAYMENUID_MAIN);

	// tray window
	insert_TrayEntry(TRAYMENUID_MAIN, -1, _("Close current alert"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_WINDOW_CLOSE_CURRENT_ALERT);
	insert_TrayEntry(TRAYMENUID_MAIN, -1, "Hide floating window", SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_WINDOW_HIDE);
	
	// scene
	SDL_TrayEntry* separator = SDL_InsertTrayEntryAt(menu, -1, NULL, 0);
	insert_TrayEntry(TRAYMENUID_MAIN, -1, "Idle scene", SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_SCENE_IDLE);

	insert_TraySubmenu(TRAYMENUID_MAIN, -1, _("Switch scene to"), TRAYMENUITEMID_SCENE_SWITCH);

	char buf[32];
	for (int at = 0; at < MAX_BASE_SCENES; at ++) {
		SDL_snprintf(buf, sizeof(buf), "scene#%i", at);
		insert_TrayEntry(TRAYMENUITEMID_SCENE_SWITCH, -1, buf, SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_SCENE_SUBMENU_0 + at);
		if (at == 0) {
			scene_obj_item_start_ = (ttray::tobj_item*)obj_items_.data + obj_items_.vsize - 1;
			scene_userdata_start_ = (ttray::tcb_userdata*)cb_userdata_pool_.data + cb_userdata_pool_.vsize - 1;
		}
	}

	// SDL_Log("scene_obj_item_start_: %p, scene_userdata_start_: %p", scene_obj_item_start_, scene_userdata_start_);
	// dump_obj_items();
	// dump_cb_userdata_pool();

	// privacy
	separator = SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

	insert_TrayEntry(TRAYMENUID_MAIN, -1, "Enable privacy", SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_PRIVACY_ENABLE_PRIVACY);

	insert_TrayEntry(TRAYMENUID_MAIN, -1, "Start listen", SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_PRIVACY_START_LISTEN);

	insert_TrayEntry(TRAYMENUID_MAIN, -1, _("Listen next courseware"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_PRIVACY_NEXT_COURSEWARE);

	insert_TrayEntry(TRAYMENUID_MAIN, -1, _("Stop speak"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_PRIVACY_STOP_SPEAK);

	// check
	separator = SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

	SDL_TrayEntryFlags flags = SDL_TRAYENTRY_CHECKBOX;
	if (preferences::tray_voice_speak()) {
		flags |= SDL_TRAYENTRY_CHECKED;
	}
	insert_TrayEntry(TRAYMENUID_MAIN, -1, _("Voice speak"), flags , TRAYMENUITEMID_CHK_VOICE_SPEAK);

	// system
	separator = SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

	insert_TraySubmenu(TRAYMENUID_MAIN, -1, _("Help"), TRAYMENUITEMID_SYS_HELP);
	std::stringstream ss;
	ss << "(" << _("Video") << ")" << _("User guide");
	insert_TrayEntry(TRAYMENUITEMID_SYS_HELP, -1, ss.str(), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_HELP_USERGUIDE);
	insert_TrayEntry(TRAYMENUITEMID_SYS_HELP, -1, _("Feedback"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_HELP_FEEDBACK);
	insert_TrayEntry(TRAYMENUITEMID_SYS_HELP, -1, _("Online document"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_HELP_ONLINE_DOC);

	insert_TrayEntry(TRAYMENUID_MAIN, -1, _("Quit"), SDL_TRAYENTRY_BUTTON, TRAYMENUITEMID_SYS_QUIT);

	// dump_cb_userdata_pool();
}

void ttray2::update_scene_submenu()
{
	std::pair<const aplt::tbase_scene*, int> curr_scene = b_api_.aplt_curr_base_scene();

	tobj_item& idle_item = find_obj_item(TRAYMENUITEMID_SCENE_IDLE);
	VALIDATE(idle_item.type == objt_TrayEntry, null_str);
	std::string label;
	bool enabled;
	tcb_userdata& curr_userdata = find_cb_userdata(TRAYMENUITEMID_SCENE_IDLE);
	curr_userdata.ctx = nposm;
	if (curr_scene.first == nullptr) {
		label = _("Idle scene");
		enabled = false;

	} else if (curr_scene.second == aplt::sts_ing) {
		label = _("Idle scene");
		enabled = true;
		curr_userdata.ctx = CTX_IDLE_SCENE;

	} else if (curr_scene.second == aplt::sts_idle) {
		label = _("Resume scene");
		enabled = true;
		curr_userdata.ctx = CTX_RESUME_SCENE;

	} else {
		label = _("Resume scene");
		enabled = false;
	}

	SDL_SetTrayEntryLabel(idle_item.entry, label.c_str());
	SDL_SetTrayEntryEnabled(idle_item.entry, enabled? SDL_TRUE: SDL_FALSE);

	const std::vector<aplt::tbase_scene>& scenes = b_api_.aplt_base_scenes();
	int scenes_count = scenes.size();

	tobj_item& switch_item = find_obj_item(TRAYMENUITEMID_SCENE_SWITCH);
	VALIDATE(switch_item.type == objt_TrayMenu, null_str);

	int count = 0;
	SDL_TrayEntry** scene_entries = SDL_GetTrayEntries(switch_item.menu, &count);
	if (count > scenes_count) {
		for (int at = count - 1; at >= 0; at --) {
			SDL_RemoveTrayEntry(scene_entries[at]);
			// As long as one entry is removed, SDL will reallocate the memory block for the menu->entries. 
			// Even if the last entry is removed.
			// The only way is to call 'SDL_GetTrayEntries' each time to re-obtain the entries address.
			int count2 = 0;
			scene_entries = SDL_GetTrayEntries(switch_item.menu, &count2);
			// SDL_Log("count: %i, at: %i, count2: %i", count, at, count2);
			VALIDATE(count2 == at, null_str);
		}
		count = 0;
	}
	if (count < scenes_count) {
		for (int at = count; at < scenes_count; at ++) {
			SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(switch_item.menu, -1, "", SDL_TRAYENTRY_BUTTON);
			tobj_item* curr_item = scene_obj_item_start_ + at;
			curr_item->type = objt_TrayEntry;
			curr_item->entry = entry;
			VALIDATE(curr_item->id == TRAYMENUITEMID_SCENE_SUBMENU_0 + at, null_str);

			tcb_userdata* curr_userdata = scene_userdata_start_ + at;
			VALIDATE(curr_userdata->tray == this, null_str);
			VALIDATE(curr_userdata->id == TRAYMENUITEMID_SCENE_SUBMENU_0 + at, null_str);
			SDL_SetTrayEntryCallback(entry, tray_entry_cb, curr_userdata);
		}
	}

	scene_entries = SDL_GetTrayEntries(switch_item.menu, &count);
	VALIDATE(count == scenes_count, null_str);
	// char buf[32];
	for (int at = 0; at < scenes_count; at ++) {
		const aplt::tbase_scene& scene = scenes[at];
		SDL_SetTrayEntryLabel(scene_entries[at], scene.name().c_str());
		SDL_SetTrayEntryEnabled(scene_entries[at], &scene == curr_scene.first? SDL_FALSE: SDL_TRUE);
	}
}

void ttray2::update_menu_on_click()
{
	//
	// tray window
	//
	// close current alert
	const tobj_item* curr_item = &find_obj_item(TRAYMENUITEMID_WINDOW_CLOSE_CURRENT_ALERT);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);

	const bool last_task_is_valid = last_task_.valid();
	SDL_SetTrayEntryEnabled(curr_item->entry, last_task_is_valid? SDL_TRUE: SDL_FALSE);

	// hide/show floating window
	curr_item = &find_obj_item(TRAYMENUITEMID_WINDOW_HIDE);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);
	std::string label;
	tcb_userdata* curr_userdata = &find_cb_userdata(TRAYMENUITEMID_WINDOW_HIDE);
	curr_userdata->ctx = nposm;

	if (video_.tray_window().is_hidden) {
		curr_userdata->ctx = CTX_SHOW_WINDOW;
		label = _("Show floating window");
	} else {
		curr_userdata->ctx = CTX_HIDE_WINDOW;
		label = _("Hide floating window");
	}
	SDL_SetTrayEntryLabel(curr_item->entry, label.c_str());
	SDL_SetTrayEntryEnabled(curr_item->entry, last_task_is_valid? SDL_FALSE: SDL_TRUE);

	//
	// scene
	//
	update_scene_submenu();

	// enable/disable privacy
	curr_item = &find_obj_item(TRAYMENUITEMID_PRIVACY_ENABLE_PRIVACY);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);
	curr_userdata = &find_cb_userdata(TRAYMENUITEMID_PRIVACY_ENABLE_PRIVACY);
	curr_userdata->ctx = nposm;

	if (b_api_.is_privacy_protecting()) {
		curr_userdata->ctx = CTX_DISABLE_PRIVACY;
		label = _("Disable privacy");
	} else {
		curr_userdata->ctx = CTX_ENABLE_PRIVACY;
		label = _("Enable privacy");
	}
	SDL_SetTrayEntryLabel(curr_item->entry, label.c_str());

	// enter/exit listen courseware
	curr_item = &find_obj_item(TRAYMENUITEMID_PRIVACY_START_LISTEN);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);
	curr_userdata = &find_cb_userdata(TRAYMENUITEMID_PRIVACY_START_LISTEN);
	curr_userdata->ctx = nposm;

	bool enable_next_courseware = true;
	if (!b_api_.is_listening()) {
		curr_userdata->ctx = CTX_START_LISTEN;
		label = _("Start listen");
		enable_next_courseware = false;
	} else {
		curr_userdata->ctx = CTX_STOP_LISTEN;
		label = _("Exit listen");
	}
	SDL_SetTrayEntryLabel(curr_item->entry, label.c_str());

	// next courseware
	curr_item = &find_obj_item(TRAYMENUITEMID_PRIVACY_NEXT_COURSEWARE);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);
	// curr_userdata = &find_cb_userdata(TRAYMENUITEMID_PRIVACY_NEXT_COURSEWARE);
	// curr_userdata->ctx = nposm;
	SDL_SetTrayEntryEnabled(curr_item->entry, enable_next_courseware? SDL_TRUE: SDL_FALSE);

	// stop voice
	curr_item = &find_obj_item(TRAYMENUITEMID_PRIVACY_STOP_SPEAK);
	VALIDATE(curr_item->type == objt_TrayEntry, null_str);
	bool enable_stop_voice = pinyin_.is_speaking() && !b_api_.is_listen_speaking();

	SDL_SetTrayEntryEnabled(curr_item->entry, enable_stop_voice? SDL_TRUE: SDL_FALSE);
}

void ttray2::handle_tray_event(uint32_t type)
{
	VALIDATE(type == TRAYEVENT_PRE_POPUPMENU, null_str);

	if (type == TRAYEVENT_PRE_POPUPMENU) {
		update_menu_on_click();
	}
}

void ttray2::slice()
{
	ttray_window& w2 = *tray::window;
	if (!w2.valid()) {
		return;
	}

	uint32_t now = SDL_GetTicks();
	if (!last_task_.valid()) {
		VALIDATE(!persistent_task_.valid(), null_str);
		if (now > next_render_idle_ticks_) {
			render_tray_window_idle();
			int render_idle_interval = 400; // ms
			next_render_idle_ticks_ = SDL_GetTicks() + render_idle_interval;
		}
		return;
	}

	if (last_task_.duration_ms != nposm) {
		if (now >= last_task_.start_ticks + last_task_.duration_ms) {
			stop_last_task();
		}
	}

	if (!last_task_.valid()) {
		return;
	}
	now = SDL_GetTicks();
	if (last_task_.next_blink_ticks != 0 && now >= last_task_.next_blink_ticks) {
		const bool next_is_hidden = !last_task_.is_hidden2;
		std::string msg;
		if (next_is_hidden) {
			VALIDATE(last_task_.duration_ms != nposm, null_str);
			msg.clear();
			last_task_.next_blink_ticks = now + BLINK_HIDE_MS;
		} else {
			msg = last_task_.msg;
			if (last_task_.duration_ms != nposm) {
				last_task_.next_blink_ticks = now + BLINK_SHOW_MS;
			} else {
				last_task_.next_blink_ticks = 0;
			}
		}
		last_task_.is_hidden2 = next_is_hidden;

		const SDL_Color non_persist_color{255, 255, 255, 255}; // RGBA: {255, 255, 255, 1}
		// const SDL_Color persist_color{0xb3, 0xe5, 0xfc, 0xff}; // RGBA: {0xb3, 0xe5, 0xfc, 1}
		const SDL_Color persist_color{0x00, 0x84, 0xff, 0xff}; // RGBA: {0x00, 0x84, 0xff, 1}
		render_tray_window(msg, last_task_.font_size,
			last_task_.duration_ms != nposm? non_persist_color: persist_color);
	}

}

void ttray2::push_task(const std::string& msg, int duration_ms)
{
	VALIDATE_IN_MAIN_THREAD();

	ttray_window& w2 = video_.tray_window();
	VALIDATE(w2.valid(), null_str);
	VALIDATE(!msg.empty(), null_str);

	if (!last_task_.valid() || last_task_.duration_ms != nposm) {
		last_task_.set(w2.width, msg, duration_ms);
		return;
	}
	// last_task is valid && last_task_ is persistent.
	persistent_task_ = last_task_;

	last_task_.set(w2.width, msg, duration_ms);
}

void ttray2::stop_last_task()
{
	VALIDATE(last_task_.valid(), null_str);
	ttray_window& w2 = video_.tray_window();

	last_task_.clear();
	if (persistent_task_.valid()) {
		// persistent => last
		ttask& p = persistent_task_;
		last_task_.set(w2.width, p.msg, p.duration_ms);
		p.clear();
	}

	if (!last_task_.valid()) {
		if (w2.is_hidden) {
			SDL_HideWindow(w2.window);
		} else {
			render_tray_window_idle();
		}
		is_session_first_render_ = true;
	}
}

void ttray2::handle_menu_entry(int id, int ctx)
{
	VALIDATE(ctx != nposm, null_str);
	utils::string_map symbols;
	std::string err_msg;
	std::string msg;

	tobj_item& curr_item = find_obj_item(id);
	VALIDATE(curr_item.type == objt_TrayEntry, null_str);

	ttray_window& w2 = video_.tray_window();
	if (id == TRAYMENUITEMID_WINDOW_CLOSE_CURRENT_ALERT) {
		if (last_task_.valid()) {
			stop_last_task();
		}

	} else if (id == TRAYMENUITEMID_WINDOW_HIDE) {
		if (!last_task_.valid()) {
			if (ctx == CTX_HIDE_WINDOW) {
				VALIDATE(!w2.is_hidden, null_str);
				w2.is_hidden = true;
				SDL_HideWindow(w2.window);

			} else {
				VALIDATE(ctx == CTX_SHOW_WINDOW, null_str);
				VALIDATE(w2.is_hidden, null_str);
				w2.is_hidden = false;
				SDL_ShowWindowWithoutFocusATop(w2.window);

				render_tray_window_idle();
			}
			preferences::set_tray_window_hidden(w2.is_hidden);
		}

	} else if (id == TRAYMENUITEMID_SCENE_IDLE) {
		int action = aplt::bs_action_idle;
		if (ctx == CTX_RESUME_SCENE) {
			action = aplt::bs_action_resume;
		} else {
			VALIDATE(ctx == CTX_IDLE_SCENE, null_str);
		}
		const aplt::tbase_scene* new_scene = aplt::handle_base_scene(b_api_, nposm, null_str, action, err_msg);
		if (new_scene != nullptr) {
			symbols["scene"] = new_scene->name();
			if (action == aplt::bs_action_idle) {
				msg = vgettext2("suspended the current scene '$scene'", symbols);

			} else if (action == aplt::bs_action_resume) {
				msg = vgettext2("Resumed the current scene '$scene'", symbols);

			}
		}

	} else if (id >= TRAYMENUITEMID_SCENE_SUBMENU_0 && id < TRAYMENUITEMID_SCENE_SUBMENU_0 + MAX_BASE_SCENES) {
		const std::vector<aplt::tbase_scene>& scenes = b_api_.aplt_base_scenes();
		int scenes_count = scenes.size();

		const int at = id - TRAYMENUITEMID_SCENE_SUBMENU_0;
		VALIDATE(at >= 0 && at < scenes_count, null_str);
		const aplt::tbase_scene& scene = scenes[at];
		
		const aplt::tbase_scene* new_scene = handle_base_scene(b_api_, at, scene.name(), nposm, err_msg);
		if (new_scene != nullptr) {
			symbols["scene"] = new_scene->name();
			// VALIDATE(action == bs_action_switch_to_next, null_str);
			msg = vgettext2("Switched to the scene '$scene'", symbols);

		} else {
			VALIDATE(!err_msg.empty(), null_str);
			symbols["err_msg"] = err_msg;
			msg = vgettext2("Switch scene failed: $err_msg", symbols);
		}
	
	} else if (id == TRAYMENUITEMID_PRIVACY_ENABLE_PRIVACY) {
		bool protect = true;
		if (ctx == CTX_DISABLE_PRIVACY) {
			protect = false;
			msg = _("Privacy protection is disabled.");

		} else {
			VALIDATE(ctx == CTX_ENABLE_PRIVACY, null_str);
			protect = true;
			msg = _("Privacy protection is enabled.");
		}
		b_api_.set_privacy_protect(protect);

	} else if (id == TRAYMENUITEMID_PRIVACY_START_LISTEN) {
		if (ctx == CTX_START_LISTEN) {
			if (!b_api_.is_listening()) {
				bool ret = b_api_.start_listen();
				if (!ret) {
					msg = _("Failed to enter the lecture listening state. There must be at least one valid courseware in the courselist.");
				} else {
					// msg = _("Entered the lecture listening state.");
				}
			}

		} else {
			VALIDATE(ctx == CTX_STOP_LISTEN, null_str);
			if (b_api_.is_listening()) {
				b_api_.stop_listen();
				msg = _("Exited the lecture listening state.");
			}
		}

	} else if (id == TRAYMENUITEMID_PRIVACY_NEXT_COURSEWARE) {
		VALIDATE(b_api_.is_listening(), null_str);
		b_api_.listen_next_course();

	} else if (id == TRAYMENUITEMID_PRIVACY_STOP_SPEAK) {
		if (pinyin_.is_speaking() && !b_api_.is_listen_speaking()) {
			pinyin_.stop_speak2();
		}

	} else if (id == TRAYMENUITEMID_CHK_VOICE_SPEAK) {
		preferences::set_tray_voice_speak(SDL_GetTrayEntryChecked(curr_item.entry));

	} else if (id == TRAYMENUITEMID_HELP_USERGUIDE) {
		// const std::string url = "https://www.bilibili.com/video/BV1Nz4y1D7sR";
		const std::string url = "https://www.bilibili.com/video/BV11VPvzNE3e";
		wchar_t* urlw = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(url.c_str()), url.size()+1);
		ShellExecuteW(NULL, L"open", urlw, NULL, NULL, SW_SHOWNORMAL);
		SDL_free(urlw);

	} else if (id == TRAYMENUITEMID_HELP_FEEDBACK) {
		int size = SDL_min(gui2::settings::screen_width, gui2::settings::screen_height) * 3 / 5;
		const std::string qrcode = "https://u.wechat.com/MAIbg1qhzTsDNMQ152CVRvI?s=2";
		gui2::trqrcode dlg(_("LanQi Tec"), _("feekback^desc"), size, qrcode, nullptr);
		dlg.show();

	} else if (id == TRAYMENUITEMID_HELP_ONLINE_DOC) {
		const std::string url = "https://www.cswamp.com/column/31";
		wchar_t* urlw = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(url.c_str()), url.size()+1);
		ShellExecuteW(NULL, L"open", urlw, NULL, NULL, SW_SHOWNORMAL);
		SDL_free(urlw);

	} else if (id == TRAYMENUITEMID_SYS_QUIT) {
		quit_app();
	}

	if (preferences::tray_voice_speak() && !msg.empty()) {
		pinyin_.speak(msg);
	}

	if (id != TRAYMENUITEMID_SYS_QUIT) {
		// Is this really necessary? 
		// --Providing tray is to avoid popping up the main window.
		// SDL_RaiseWindow2();
	}
}

void save_tray_window_rect()
{
	ttray_window* window = tray::window;
	VALIDATE(window->valid(), null_str);

	int win_x, win_y;
	SDL_GetWindowPosition(window->window, &win_x, &win_y);

	int win_w, win_h;
	SDL_GetWindowSize(window->window, &win_w, &win_h);
	preferences::set_tray_window_rect(SDL_Rect{win_x, win_y, win_w, win_h});
}

void ttray2::start_dragging(int globalX, int globalY, int winX, int winY)
{
	ttray_window& w2 = *tray::window;

	w2.offset.x = globalX - winX;
    w2.offset.y = globalY - winY;
                    
    // SDL_Log("%u {dbg_position}down: global(%d,%d) window(%d,%d) offset(%d,%d)", 
    //        SDL_GetTicks(), globalX, globalY, winX, winY, window->offset.x, window->offset.y);

	w2.dragging = true;
}

void ttray2::tray_handle_WINDOWEVENT(const SDL_WindowEvent& windowevt)
{
	ttray_window& w2 = *tray::window;
	if (windowevt.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        // Get mouse state and global coordinates
        int globalX, globalY;
        Uint32 mouseState = SDL_GetGlobalMouseState(&globalX, &globalY);
        
        // 1. First, check if the left button is pressed.
        if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            // 2. Get the current mouse focus window
            SDL_Window* mouseFocus = SDL_GetMouseFocus();
            
            // 3. Get the floating window position
            int winX, winY;
            SDL_GetWindowPosition(w2.window, &winX, &winY);
            
            // 4. Compute local coordinates
            int localX = globalX - winX;
            int localY = globalY - winY;
            
            // 5. Comprehensive judgment
            bool inWindow = (localX >= 0 && localX < w2.width && 
                            localY >= 0 && localY < w2.height);
            
            SDL_Log("FOCUS_GAINED: mouseFocus=%p, local(%d,%d), inWindow=%s", 
                    mouseFocus, localX, localY, inWindow? "true": "false");
            
            if (inWindow) {
				start_dragging(globalX, globalY, winX, winY);
                // StartDragging(fw);
            }

        }
    }
}

void ttray2::tray_handle_MOUSEBUTTONDOWN(int x, int y)
{
	ttray_window* window = tray::window;
	VALIDATE(window->valid(), null_str);

	int globalX, globalY;
    SDL_GetGlobalMouseState(&globalX, &globalY);
                    
    int winX, winY;
    SDL_GetWindowPosition(window->window, &winX, &winY);
	
	start_dragging(globalX, globalY, winX, winY);
}

void ttray2::tray_handle_MOUSEBUTTONUP(int x, int y)
{
	ttray_window* window = tray::window;
	VALIDATE(window->valid(), null_str);

	// SDL_Log("%u {dbg-position}up, dragging: %s, (x: %i, y: %i)", 
	//	SDL_GetTicks(), window->dragging? "true": "false", x, y);

	window->dragging = false;

	save_tray_window_rect();
}

void ttray2::tray_handle_MOUSEMOTION(int x, int y)
{
	ttray_window* window = tray::window;
	VALIDATE(window->valid(), null_str);

	if (window->dragging) {
		Uint32 mouseState = SDL_GetGlobalMouseState(NULL, NULL);
        if (!(mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))) {
            // The left button has been released, but the UP event was not received.
            window->dragging = false;

			save_tray_window_rect();
            // SDL_Log("ttray2::handle_MOUSEMOTION, Detected that the left button has been released, ending the drag.");
            return;
        }

		int globalX, globalY;
        SDL_GetGlobalMouseState(&globalX, &globalY);
                
        int newX = globalX - window->offset.x;
        int newY = globalY - window->offset.y;
                
        SDL_SetWindowPosition(window->window, newX, newY);
                
        // SDL_Log("%u {dbg-position}: global(%d,%d) offset(%d,%d) => new(%d,%d)", 
        //        SDL_GetTicks(), globalX, globalY, window->offset.x, window->offset.y, newX, newY);

    } else {
		// SDL_Log("%u {dbg-position}motion, (x: %i, y: %i), dragging is false, do nothing", SDL_GetTicks(), x, y);
	}
}

#endif