/* $Id: preferences.cpp 47642 2010-11-21 13:58:27Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Get and set user-preferences.
 */

#include "rose_global.hpp"

#define GETTEXT_DOMAIN "rose-lib"

#include "rose_prefs.hpp"
#include "filesystem.hpp"
#include "gui/widgets/settings.hpp"
#include "hotkeys.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "display.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "lobby.hpp"
#include "base_instance.hpp"


static bool color_cursors = false;
static config critical_prefs;

class tapp_preferences: public trose_prefs
{
public:
	static tapp_preferences* singleton;

	tapp_preferences()
		: trose_prefs()
	{
		VALIDATE(singleton == nullptr, null_str);
		singleton = this;
	}

	~tapp_preferences()
	{
		VALIDATE(singleton != nullptr, null_str);
		singleton = nullptr;
	}

	void load_prefs(const std::string& prefs_file) override;
	void write_prefs() override;

};

tapp_preferences* tapp_preferences::singleton = nullptr;

#define VALIDATE_SINGLETON()		VALIDATE(tapp_preferences::singleton != nullptr, null_str);

static tapp_preferences app_pref;

#define prefs	app_pref.mutable_cfg()

void tapp_preferences::load_prefs(const std::string& prefs_file)
{
	const bool copy_prefs = false;
	if (copy_prefs) {
		// Power down, restart, will destroy the 'preferences'. 
		// Here's a copy of it to see what kind of destruction it is.
		const std::string prefs_startup = get_user_data_dir() + "/preferences_startup";
		SDL_CopyFiles(get_prefs_file().c_str(), prefs_startup.c_str());
	}

	std::string stream;
	{
		tfile file(get_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = file.read_2_data();
		SDL_Log("base_manager::base_manager--- prefs_file: %s, fsize: %i", get_prefs_file().c_str(), fsize);
		if (fsize > 0) {
			stream.assign(file.data, fsize);
			try {
				read(cfg_, stream);
				const config& hero_cfg = cfg_.child(signature_tag_);
				if (!hero_cfg) {
					// exist preferences, but isn't valid
					SDL_Log("base_manager::base_manager, exist preferences, but isn't valid, because hero_cfg is <nil>");
					cfg_.clear();
				} else {
					SDL_Log("base_manager::base_manager, use preferences, fsize: %i", fsize);
				}
			} catch (twml_exception& ) {
				cfg_.clear();
			}
		}
	}

	if (cfg_.empty()) {
		tfile bak(get_prefs_back_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = bak.read_2_data();
		SDL_Log("base_manager::base_manager, preferences is valid, try preferences.bak, fsize: %i", fsize);
		if (fsize > 0) {
			stream.assign(bak.data, fsize);
			try {
				read(cfg_, stream);
				const config& hero_cfg = cfg_.child(signature_tag_);
				if (!hero_cfg) {
					// exist preferences.bak, but isn't valid
					SDL_Log("base_manager::base_manager, exist preferences.bak, but isn't valid, because hero_cfg is <nil>");
					cfg_.clear();
				} else {
					SDL_Log("base_manager::base_manager, use preferences.bak, fsize: %u", (uint32_t)stream.size());
					// preferences doesn't exist, but preferences.bak is valid. use preferences.bak replace preferences.
					single_open_copy(get_prefs_back_file(), get_prefs_file());
				}
			} catch (twml_exception& ) {
				cfg_.clear();
			}
		}
	}
	SDL_Log("base_manager::base_manager, %s find valid preferences or preferences.bak", cfg_.empty()? "cannot": "can");

	if (!cfg_.has_child(signature_tag_.c_str())) {
		VALIDATE(cfg_.empty(), null_str);
		VALIDATE(data_ == nullptr && data_vsize_ == 0 && data_size_ == 0, null_str);
		cfg_.add_child(signature_tag_.c_str());
		// Although cfg_ are not in agreement with data_, it is not a big deal.
		// Once cfg_ is changed later, it will make data_ consistent with cfg_ when saving.
	}

	{
		// critical preferences
		tfile file(get_critical_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = file.read_2_data();
		SDL_Log("base_manager::base_manager, prefs_file: %s, fsize: %i", get_critical_prefs_file().c_str(), fsize);
		if (fsize > 0) {
			std::string stream(file.data, fsize);
			try {
				read(critical_prefs, stream);
			} catch (twml_exception& ) {
				critical_prefs.clear();
			}
		}
	}

	if (preferences::member().empty()) {
		std::stringstream strstr;
		std::map<int, int> member;
		member.insert(std::make_pair(172, 5));
		member.insert(std::make_pair(176, 6));
		member.insert(std::make_pair(244, 7));
		member.insert(std::make_pair(279, 8));

		member.insert(std::make_pair(103, 8));
		member.insert(std::make_pair(106, 9));
		member.insert(std::make_pair(120, 9));
		member.insert(std::make_pair(381, 9));
		for (std::map<int ,int>::const_iterator it = member.begin(); it != member.end(); ++ it) {
			if (it != member.begin()) {
				strstr << ", ";
			}
			strstr << ((it->second << 16) | it->first);
		}
		preferences::set_member(strstr.str());
	}
}

void tapp_preferences::write_prefs()
{
	VALIDATE_IN_MAIN_THREAD();

	std::stringstream out;

	// write critical prefs
	if (instance) {
		::config cfg = instance->app_critical_prefs();
		config diff_cfg;
		if (!cfg.empty()) {
			cfg.get_diff(critical_prefs, diff_cfg);
		}
		if (!diff_cfg.empty()) {
			try {
				write(out, cfg);
				critical_prefs = cfg;
			} catch (io_exception&) {
				// error writing to preferences file '$get_prefs_file()'
			}
			tfile file(get_critical_prefs_file(), GENERIC_WRITE, CREATE_ALWAYS);
			VALIDATE(file.valid(), null_str);
			posix_fwrite(file.fp, out.str().c_str(), out.str().size());
		}
	}
	out.str("");
	
	// wirte full prefs
	try {
		write(out, cfg_);

	} catch (io_exception&) {
		// error writing to preferences file '$get_prefs_file()'
		return;
	}
/*
	std::string data;
	{
		tfile file(get_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0) {
			data.assign(file.data, fsize);
		}
	}
	if (!data.empty()) {
		if (data.size() == out.str().size()) {
			if (memcmp(data.c_str(), out.str().c_str(), data.size()) == 0) {
				return;
			}
		}
		tfile file(get_prefs_back_file(), GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, data.c_str(), data.size());
	}
*/
	{
		tfile file(get_prefs_file(), GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, out.str().c_str(), out.str().size());
	}
	backup_did_file_write(get_prefs_file(), backup_on_idle);
	preferences::last_write_ticks = SDL_GetTicks();
}

namespace preferences {

std::string public_account = "";
std::string public_password = "";
uint32_t last_write_ticks = 0;

base_manager::base_manager()
{
	VALIDATE(app_pref.cfg().empty(), null_str);
	app_pref.load_prefs(get_prefs_file());
}

base_manager::~base_manager()
{
	// must not call write_prefs()
}

void set_bool(const std::string& key, bool value)
{
	VALIDATE_SINGLETON();
	app_pref.set_bool(key, value);
}

void set_int(const std::string &key, int value)
{
	VALIDATE_SINGLETON();
	app_pref.set_int(key, value);
}

void set_int64(const std::string &key, int64_t value)
{
	VALIDATE_SINGLETON();
	app_pref.set_int64(key, value);
}

void set_str(const std::string &key, const std::string &value)
{
	VALIDATE_SINGLETON();
	app_pref.set_str(key, value);
}

// void clear(const std::string& key)
// {
//	VALIDATE_IN_MAIN_THREAD();
//	prefs.recursive_clear_value(key);
// }

void set_child(const std::string& key, const config& val) 
{
	VALIDATE_SINGLETON();
	app_pref.set_child(key, val);
}

const config& get_child(const std::string& key)
{
	VALIDATE_SINGLETON();
	return app_pref.get_child(key);
}

void erase(const std::string& key) 
{
	VALIDATE_SINGLETON();
	app_pref.erase(key);
}

std::string get_str(const std::string& key) 
{
	VALIDATE_SINGLETON();
	return app_pref.get_str(key);
}

bool get_bool(const std::string &key, bool def)
{
	VALIDATE_SINGLETON();
	return app_pref.get_bool(key, def);
}

int get_int(const std::string &key, int def)
{
	VALIDATE_SINGLETON();
	return app_pref.get_int(key, def);
}

uint32_t get_uint32(const std::string &key, uint32_t def)
{
	VALIDATE_SINGLETON();
	return app_pref.get_uint32(key, def);
}

int64_t get_int64(const std::string &key, int64_t def)
{
	VALIDATE_SINGLETON();
	return app_pref.get_int64(key, def);
}

config* get_prefs()
{
	VALIDATE_SINGLETON();
	config* pointer = &app_pref.mutable_cfg();
	return pointer;
}

const config& get_critical_prefs()
{
	return critical_prefs;
}

std::string title()
{
	std::string ret = preferences::get_str("title");
	if (ret.empty()) {
		ret = instance->app_defaut_title();
	}
	return ret;
}

void set_title(const std::string& str)
{
	VALIDATE_SINGLETON();

	if (str != title()) {
		set_str("title", str);
		app_pref.write_prefs();
	}
}

bool get_hero(hero& h, int default_image)
{
	const config& cfg = app_pref.cfg().child("hero");
	if (cfg) {
		h.set_name(cfg["name"].str());
		h.set_surname(cfg["surname"].str());
		h.set_biography(cfg["biography"].str());

		h.gender_ = cfg["gender"].to_int(hero_gender_male);
		h.image_ = cfg["image"].to_int(default_image);

		h.leadership_ = ftofxp9(cfg["leadership"].to_int(1));
		h.force_ = ftofxp9(cfg["force"].to_int(1));
		h.intellect_ = ftofxp9(cfg["intellect"].to_int(1));
		h.spirit_ = ftofxp9(cfg["spirit"].to_int(1));
		h.charm_ = ftofxp9(cfg["charm"].to_int(1));

		std::stringstream str;
		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			str.str("");
			str << "arms" << i;
			h.arms_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			str.str("");
			str << "skill" << i;
			h.skill_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}

		h.side_feature_ = cfg["side_feature"].to_int(HEROS_NO_FEATURE);
		if (h.side_feature_ < 0 || h.side_feature_ > HEROS_MAX_FEATURE) {
			h.side_feature_ = HEROS_NO_FEATURE;
		}
		h.feature_ = cfg["feature"].to_int(HEROS_NO_FEATURE);
		if (h.feature_ < 0 || h.feature_ > HEROS_MAX_FEATURE) {
			h.feature_ = HEROS_NO_FEATURE;
		}
		h.tactic_ = cfg["tactic"].to_int(HEROS_NO_TACTIC);
		h.utype_ = cfg["utype"].to_int(HEROS_NO_UTYPE);
		h.character_ = cfg["character"].to_int(HEROS_NO_CHARACTER);

		h.base_catalog_ = cfg["base_catalog"].to_int();
		h.float_catalog_ = ftofxp8(h.base_catalog_);

		return true;
	} else {
		// h.set_name(_("Press left button to create hero"));
		h.set_name(public_account);
		h.set_surname("Mr.");

		h.gender_ = hero_gender_male;
		h.image_ = default_image;

		h.leadership_ = ftofxp9(1);
		h.force_ = ftofxp9(1);
		h.intellect_ = ftofxp9(1);
		h.spirit_ = ftofxp9(1);
		h.charm_ = ftofxp9(1);

		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			h.arms_[i] = 0;
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			h.skill_[i] = 0;
		}

		return false;
	}
}

void set_hero(const hero_map& heros, hero& h)
{
	VALIDATE_SINGLETON();

	VALIDATE_IN_MAIN_THREAD();
	config* ptr = NULL;
	if (config& cfg = prefs.child("hero")) {
		ptr = &cfg;
	} else {
		ptr = &prefs.add_child("hero");
	}
	config& cfg = *ptr;
	std::stringstream str;

	cfg["name"] = h.name();
	cfg["surname"] = h.surname();
	cfg["biography"] = h.biography2(heros);

	cfg["leadership"].from_int(fxptoi9(h.leadership_));
	cfg["force"].from_int(fxptoi9(h.force_));
	cfg["intellect"].from_int(fxptoi9(h.intellect_));
	cfg["spirit"].from_int(fxptoi9(h.spirit_));
	cfg["charm"].from_int(fxptoi9(h.charm_));

	cfg["gender"].from_int(h.gender_);
	cfg["image"].from_int(h.image_);

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		str.str("");
		str << "arms" << i;
		cfg[str.str()].from_int(fxptoi12(h.arms_[i]));
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		str.str("");
		str << "skill" << i;
		cfg[str.str()].from_int(fxptoi12(h.skill_[i]));
	}

	cfg["base_catalog"].from_int(h.base_catalog_);

	cfg["feature"].from_int(h.feature_);
	cfg["side_feature"].from_int(h.side_feature_);

	cfg["tactic"].from_int(h.tactic_);
	cfg["utype"].from_int(h.utype_);
	cfg["character"].from_int(h.character_);

	app_pref.write_prefs();
}

void set_username(const std::string& username)
{
	hero& h = group.leader();
	if (h.name() != username) {
		h.set_name(username);
		set_hero(instance->heros(), h);
	}
}

bool fullscreen()
{
	return get_bool("fullscreen", false);
}

void _set_fullscreen(bool ison)
{
	set_bool("fullscreen", ison);
}

bool maximized()
{
	return get_bool("maximized", false);
}

void _set_maximized(bool ison)
{
	set_bool("maximized", ison);
}

bool scroll_to_action()
{
	return get_bool("scroll_to_action", false);
}

void _set_scroll_to_action(bool ison)
{
	set_bool("scroll_to_action", ison);
}

tpoint landscape_size(const CVideo& video)
{
	VALIDATE_SINGLETON();

	int width, height;
	if (!fullscreen()) {
		const std::string postfix = "windowsize";
		std::string x = prefs['x' + postfix], y = prefs['y' + postfix];
		if (!x.empty() && !y.empty()) {
			width = std::max((int)(atoi(x.c_str()) * gui2::twidget::hdpi_scale), game_config::min_allowed_width());
			height = std::max((int)(atoi(y.c_str()) * gui2::twidget::hdpi_scale), game_config::min_allowed_height());
			// width/height from preferences.cfg must be landscape.
			if (width < height) {
				int tmp = width;
				width = height;
				height = tmp;
			}

		} else {
			width = game_config::min_allowed_width();
			height = game_config::min_allowed_height();
		}
	} else {
		SDL_Rect bound = video.bound();
		width = bound.w;
		height = bound.h;
	}

	return tpoint(posix_align_floor(width, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale)), 
		posix_align_floor(height, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale)));
}

int noble()
{
	VALIDATE_SINGLETON();

	int noble =  prefs["noble"].to_int();
	// if (noble < 0 || noble > unit_types.max_noble_level()) {
	// 	noble = 0;
	// }
	return noble;
}

void set_noble(int value)
{
	// if (value < 0 || value > unit_types.max_noble_level()) {
	// 	value = 0;
	// }
	set_int("noble", value);
}

void set_float_button_xy(const std::string& prefix, int x, int y)
{
	VALIDATE_SINGLETON();

	tpoint pt = float_button_xy(prefix);
	bool dirty = false;
	if (x != pt.x) {
		prefs[prefix + "x"].from_int(x);
		dirty = true;
	}
	if (y != pt.y) {
		prefs[prefix + "y"].from_int(y);
		dirty = true;
	}
	if (dirty) {
		app_pref.write_prefs();
	}
}

tpoint float_button_xy(const std::string& prefix)
{
	VALIDATE_SINGLETON();

	return tpoint(prefs[prefix + "x"].to_int(), prefs[prefix + "y"].to_int());
}

int uid()
{
	VALIDATE_SINGLETON();

	return prefs["uid"].to_int();
}

void set_uid(int value)
{
	VALIDATE_SINGLETON();

	prefs["uid"].from_int(value);
}

std::string city()
{
	VALIDATE_SINGLETON();

	return prefs["city"].str();
}

void set_city(const std::string& str)
{
	VALIDATE_SINGLETON();

	prefs["city"] = str;
}

std::string interior()
{
	return prefs["interior"].str();
}

void set_interior(const std::string& str)
{
	prefs["interior"] = str;
}

std::string signin()
{
	return prefs["signin"].str();
}

void set_signin(const std::string& str)
{
	prefs["signin"] = str;
}

std::string member()
{
	return prefs["member"].str();
}

void set_member(const std::string& str)
{
	prefs["member"] = str;
}

std::string exile()
{
	return prefs["exile"].str();
}

void set_exile(const std::string& str)
{
	prefs["exile"] = str;
}

std::string associate()
{
	return prefs["associate"].str();
}

void set_associate(const std::string& str)
{
	prefs["associate"] = str;
}

std::string layout()
{
	return prefs["layout"].str();
}

void set_layout(const std::string& str)
{
	prefs["layout"] = str;
}

std::string map()
{
	return prefs["map"].str();
}

void set_map(const std::string& str)
{
	prefs["map"] = str;
}

void set_coin(int value)
{
	prefs["coin"].from_int(value);
}

int coin()
{
	return prefs["coin"].to_int();
}

void set_score(int value)
{
	prefs["score"].from_int(value);
}

int score()
{
	return prefs["score"].to_int();
}

std::string login()
{
	const config& cfg = get_child("hero");

	if (cfg && !cfg["name"].empty()) {
		return cfg["name"].str();
	}
	return public_account;
}

void set_vip_expire(time_t value)
{
	std::stringstream strstr;
	strstr << login() << ", " << (long)value;
	prefs["vip_expire"] = strstr.str();
}

time_t vip_expire()
{
	std::vector<std::string> vstr = utils::split(prefs["vip_expire"].str());
	if (vstr.size() != 2) {
		return 0;
	}
	if (vstr[0] != login()) {
		return 0;
	}
	return lexical_cast_default<long>(vstr[1]);
}

void set_developer(bool ison)
{
	VALIDATE_SINGLETON();

	prefs["developer"].from_bool(ison);
}

bool developer()
{
	return prefs["developer"].to_bool();
}

std::string nick()
{
	return prefs["nick"].str();
}

void set_nick(const std::string& nick)
{
	prefs["nick"] = nick;
}

bool chat()
{
	return prefs["chat"].to_bool(true);
}

void set_chat(bool val)
{
	prefs["chat"].from_bool(val);
}

std::string chat_person()
{
	return prefs["chat_person"].str();
}

void set_chat_person(const std::string& person)
{
	prefs["chat_person"] = person;
}

std::string chat_channel()
{
	return prefs["chat_channel"].str();
}

void set_chat_channel(const std::string& channel)
{
	prefs["chat_channel"] = channel;
}

bool startup_login()
{
	return prefs["startup_login"].to_bool(true);
}

void set_startup_login(bool val)
{
	prefs["startup_login"].from_bool(val);
}

std::string encode_pw(const std::string& str)
{
	VALIDATE_SINGLETON();

	std::stringstream ss;
	ss << "pw_";
	if (utils::is_utf8str(str.c_str(), str.size())) {
		ss << str;
	}
	return ss.str();
}

std::string decode_pw(const std::string& str)
{
	VALIDATE_SINGLETON();

	if (!utils::is_utf8str(str.c_str(), str.size())) {
		return null_str;
	}

	int pos = str.find("pw_");
	if (pos != std::string::npos) {
		return str.substr(3);
	} else {
		return str;
	}
}

std::string password()
{
	VALIDATE_SINGLETON();

	if (login() == public_account) {
		return public_password;
	}
	return decode_pw(preferences::get_str("password"));
}

void set_password(const std::string& password)
{
	VALIDATE_SINGLETON();

	preferences::set_str("password", encode_pw(password));
}

double turbo_speed()
{
	return prefs["turbo_speed"].to_double(2.0);
}

void save_turbo_speed(const double speed)
{
	prefs["turbo_speed"].from_double(speed);
}

bool default_move()
{
	return  get_bool("default_move", true);
}

void _set_default_move(const bool ison)
{
	prefs["default_move"].from_bool(ison);
}

std::string language()
{
	std::string str = prefs["locale"];
	if (str.empty()) {
		str = "zh_CN";
	}
	return str;
}

void set_language(const std::string& s)
{
	preferences::set_str("locale", s);
}

int grid_style()
{
	return get_int("grid_style", false);
}

int keyboard_style()
{
	int def = keyboard_style_default;
	int style = get_int("keyboard_style", def);
	if (style < 0 || style >= keyboard_styles) {
		style = def;
	}
	return style;
}

void set_keyboard_style(int value)
{
	VALIDATE(value >= 0 && value < keyboard_styles, null_str);

	set_int("keyboard_style", value);
}

std::string usingcamera()
{
	VALIDATE_SINGLETON();

	return preferences::get_str("usingcamera");
}

void set_usingcamera(const std::string& value)
{
	VALIDATE_SINGLETON();

	preferences::set_str("usingcamera", value);
}

int zoom()
{
	VALIDATE_SINGLETON();

	return display::adjust_zoom(prefs["zoom"].to_int(game_config::svga? display::ZOOM_72: display::ZOOM_48));
}

void _set_zoom(int value)
{
	VALIDATE_SINGLETON();

	preferences::set_int("zoom", display::adjust_zoom(value));
}

int music_volume()
{
	VALIDATE_SINGLETON();

	return prefs["music_volume"].to_int(100);
}

void set_music_volume(int vol)
{
	VALIDATE_SINGLETON();

	if(music_volume() == vol) {
		return;
	}

	prefs["music_volume"].from_int(vol);
	sound::set_music_volume(music_volume());
}

int sound_volume()
{
	return preferences::get_int("sound_volume", 100);
}

void set_sound_volume(int vol)
{
	preferences::set_int("sound_volume", vol);
	sound::set_sound_volume(sound_volume());
}

int bell_volume()
{
	VALIDATE_SINGLETON();

	return prefs["bell_volume"].to_int(100);
}

void set_bell_volume(int vol)
{
	VALIDATE_SINGLETON();

	if(bell_volume() == vol) {
		return;
	}

	prefs["bell_volume"].from_int(vol);
	sound::set_bell_volume(bell_volume());
}

int UI_volume()
{
	VALIDATE_SINGLETON();
	return prefs["UI_volume"].to_int(100);
}

void set_UI_volume(int vol)
{
	VALIDATE_SINGLETON();
	if(UI_volume() == vol) {
		return;
	}

	prefs["UI_volume"].from_int(vol);
	sound::set_UI_volume(UI_volume());
}

bool turn_bell()
{
	VALIDATE_SINGLETON();
	return get_bool("turn_bell", true);
}

bool set_turn_bell(bool ison)
{
	VALIDATE_SINGLETON();
	if(!turn_bell() && ison) {
		preferences::set_bool("turn_bell", true);
		if(!music_on() && !sound_on() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("turn_bell", false);
				return false;
			}
		}
	} else if(turn_bell() && !ison) {
		preferences::set_bool("turn_bell", false);
		sound::stop_bell();
		if(!music_on() && !sound_on() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool UI_sound_on()
{
	VALIDATE_SINGLETON();
#ifdef _WIN32
	// fix bug
	return true;
#else
	return get_bool("UI_sound", true);
#endif
}

bool set_UI_sound(bool ison)
{
	VALIDATE_SINGLETON();
	if(!UI_sound_on() && ison) {
		preferences::set_bool("UI_sound", true);
		if(!music_on() && !sound_on() && !turn_bell()) {
			if(!sound::init_sound()) {
				preferences::set_bool("UI_sound", false);
				return false;
			}
		}
	} else if(UI_sound_on() && !ison) {
		preferences::set_bool("UI_sound", false);
		sound::stop_UI_sound();
		if(!music_on() && !sound_on() && !turn_bell())
			sound::close_sound();
	}
	return true;
}

bool message_bell()
{
	VALIDATE_SINGLETON();
	return get_bool("message_bell", true);
}

bool sound_on()
{
	VALIDATE_SINGLETON();
	return get_bool("sound", true);
}

bool set_sound(bool ison)
{
	VALIDATE_SINGLETON();
	if(!sound_on() && ison) {
		preferences::set_bool("sound", true);
		if(!music_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("sound", false);
				return false;
			}
		}
	} else if(sound_on() && !ison) {
		preferences::set_bool("sound", false);
		sound::stop_sound();
		if(!music_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool music_on()
{
	VALIDATE_SINGLETON();
	return get_bool("music", true);
}

bool set_music(bool ison)
{
	VALIDATE_SINGLETON();
	if(!music_on() && ison) {
		preferences::set_bool("music", true);
		if(!sound_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("music", false);
				return false;
			}
		}
		else
			sound::play_music();
	} else if(music_on() && !ison) {
		preferences::set_bool("music", false);
		if(!sound_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
		else
			sound::stop_music();
	}
	return true;
}

namespace {
	double scroll = 0.2;
}

int scroll_speed()
{
	VALIDATE_SINGLETON();
	const int value = lexical_cast_in_range<int>(get_str("scroll"), 50, 1, 100);
	scroll = value/100.0;

	return value;
}

void set_scroll_speed(const int new_speed)
{
	VALIDATE_SINGLETON();
	prefs["scroll"].from_int(new_speed);
	scroll = new_speed / 100.0;
}

bool middle_click_scrolls()
{
	VALIDATE_SINGLETON();
	return get_bool("middle_click_scrolls", true);
}

bool mouse_scroll_enabled()
{
	VALIDATE_SINGLETON();
	return get_bool("mouse_scrolling", true);
}

void enable_mouse_scroll(bool value)
{
	VALIDATE_SINGLETON();
	set_bool("mouse_scrolling", value);
}

int mouse_scroll_threshold()
{
	VALIDATE_SINGLETON();
	return prefs["scroll_threshold"].to_int(10);
}

bool use_color_cursors()
{
	return color_cursors;
}

void _set_color_cursors(bool value)
{
	preferences::set_bool("color_cursors", value);
	color_cursors = value;
}

void load_hotkeys()
{
	hotkey::load_hotkeys(prefs);
}
void save_hotkeys()
{
	hotkey::save_hotkeys(prefs);
}

void add_alias(const std::string &alias, const std::string &command)
{
	VALIDATE_SINGLETON();
	config &alias_list = prefs.child_or_add("alias");
	alias_list[alias] = command;
}


const config &get_alias()
{
	VALIDATE_SINGLETON();
	return get_child("alias");
}

std::string theme()
{
	VALIDATE_SINGLETON();
	return preferences::get_str("theme");
}

void set_theme(const std::string& theme)
{
	VALIDATE_SINGLETON();
	preferences::set_str("theme", theme);
}

std::string login_username()
{
	return preferences::get_str("login_username");
}

void set_login_username(const std::string& value)
{
	preferences::set_str("login_username", value);
}

std::string login_pwcookie()
{
	return preferences::get_str("login_pwcookie");
}

void set_login_pwcookie(const std::string& value)
{
	preferences::set_str("login_pwcookie", value);
}

std::string login_deviceid()
{
	return preferences::get_str("login_deviceid");
}

void set_login_deviceid(const std::string& value)
{
	preferences::set_str("login_deviceid", value);
}

std::string driver(int type)
{
	VALIDATE(aplt::aplt_drivers.count(type) != 0, null_str);
	const std::string& key = aplt::aplt_drivers.find(type)->second.id;

	return preferences::get_str(key);
}

void set_driver(int type, const std::string& value)
{
	VALIDATE(aplt::aplt_drivers.count(type) != 0, null_str);
	const std::string& key = aplt::aplt_drivers.find(type)->second.id;

	preferences::set_str(key, value);
}

std::string base_scene_id()
{
	std::string id = preferences::get_str("base_scene_id");
	return id;
}

void set_base_scene_id(const std::string& id)
{
	preferences::set_str("base_scene_id", id);
}

std::string share_watermark()
{
	std::string value = preferences::get_str("share_watermark");
	return value;
}

void set_share_watermark(const std::string& value)
{
	preferences::set_str("share_watermark", value);
}

std::string dyn_charts()
{
	return preferences::get_str("dyn_charts");
}

void set_dyn_charts(const std::string& value)
{
	preferences::set_str("dyn_charts", value);
}

} // end namespace preferences

