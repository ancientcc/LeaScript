#define GETTEXT_DOMAIN "kdesktop-lib"

#include "gui/dialogs/home.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/dlg_utils.hpp"
#include "gettext.hpp"
#include "rose_version.hpp"
#include "formula_string_utils.hpp"
#include "config_cache.hpp"
#include <SDL_peripheral.h>
#include "base_driver_core.hpp"
#include "drivers_core.hpp"
#include "cfg_cpp_api_core.hpp"

#include <net/base/ip_endpoint.h>

#include "sound.hpp"
#include "net.hpp"
#include "base_instance.hpp"

#include "chinese.hpp"

using namespace std::placeholders;

extern bool will_enter_landscape_module(tbase_driver_core& base_driver, const std::string& module);

namespace gui2 {

REGISTER_DIALOG(kdesktop, home)


thome::thome(gui2::trstore::tslot& slot, tpbremotes& pbremotes, tble2& ble, std::map<aplt::taplt_key, aplt::tapplet>& applets,
	tbase_driver_core& base_driver, tdrivers_core& drivers, aplt::tcfg_cpp_api_core& cfg_cpp_api, int startup_layer)
	: tscan(ble)
	, aplt::tdesktop(*this, slot, applets, APPLET0)
	, trcswamp_login(current_user)
	// , pbremotes_(pbremotes)
	, applets_(applets)
	, base_driver_(base_driver)
	, drivers_(drivers)
	, cfg_cpp_api_(cfg_cpp_api)
	, startup_layer_(startup_layer)
	, current_layer_(nposm)
	, body_widget_(nullptr)
	, navigation_report_(nullptr)
	, ipaddr_(nullptr)
	, scene_list_(nullptr)
	, battery_widget_(nullptr)
	, start_base_node_widget_(nullptr)
	, scroll_logs_to_bottom_(true)
	, event_list_(nullptr)
{
	set_timer_interval(800);
}

thome::~thome()
{
}

void thome::pre_show()
{
	window_->set_escape_disabled(true);
	window_->set_label("misc/bg_ffffff.png");

	find_widget<tlabel>(window_, "title", false).set_label(game_config::get_app_msgstr(null_str));

	body_widget_ = find_widget<tstack>(window_, "body", false, true);
	pre_rdp(*body_widget_->layer(RDP_LAYER));
	pre_scan(*body_widget_->layer(SCAN_LAYER), *this);
	pre_applet(*body_widget_->layer(APPLET_LAYER));
	pre_event(*body_widget_->layer(EVENT_LAYER));
	pre_more(*body_widget_->layer(MORE_LAYER));

	treport* report = find_widget<treport>(window_, "navigation", false, true);
	// tcontrol* item = &report->insert_item(null_str, _("Remote desktop"));
	tcontrol* item;
	// const bool english_error = false;
	item = &report->insert_item(null_str, _("home^rdp_layer"));
	item->set_icon("misc/rdp.png");

	item = &report->insert_item(null_str, _("Remote IP"));
	item->set_icon("misc/scan.png");

	item = &report->insert_item(null_str, _("home^apps_layer"));
	item->set_icon("misc/applet.png");

	item = &report->insert_item(null_str, _("Event"));
	item->set_icon("misc/event.png");

	item = &report->insert_item(null_str, _("Me"));
	item->set_icon("misc/me.png");

	report->set_did_item_pre_change(std::bind(&thome::did_navigation_pre_change, this, _1, _2, _3));
	report->set_did_item_changed(std::bind(&thome::did_navigation_changed, this, _1, _2));
	report->select_item(startup_layer_);
	navigation_report_ = report;
}

void thome::post_show()
{
	scan_post_show();
	rdpcookie_.landscape = preferences::landscape();
}


void thome::app_first_drawn()
{
	// event_list in in event_layer, place it here is no effect.
/*
	if (event_list_->rows() > 0) {
		tmsg_data_listbox_scroll_to_bottom* pdata = new tmsg_data_listbox_scroll_to_bottom(*window_, *event_list_);
		rtc::Thread::Current()->Post(RTC_FROM_HERE, this, POST_MSG_LOGS_SCROLL_TO_BOTTOM, pdata);
	}
*/
}

void thome::pre_rdp(tgrid& grid)
{
	std::stringstream ss;
	utils::string_map symbols;

	const int max_ipv4_str_chars = 3 + 1 + 3 + 1 + 3 + 1 + 3;
	// ip addr
	ipaddr_ = find_widget<ttext_box>(&grid, "ipaddr", false, true);
	ipaddr_->set_did_text_changed(std::bind(&thome::did_text_box_changed, this, std::ref(grid), _1));
	ipaddr_->set_maximum_chars(max_ipv4_str_chars);
	ipaddr_->set_placeholder("192.168.1.109");
	ipaddr_->set_label(preferences::currentremote());

	tbutton* button = find_widget<tbutton>(&grid, "orientation", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&thome::click_orientation
			, this, std::ref(*button)));
	button->set_label(preferences::landscape()? _("Landscape"): _("Portrait"));

	button = find_widget<tbutton>(&grid, "desktop", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&thome::click_rdp
			, this, std::ref(*button)));
	button->set_active(can_rdp());

	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "ratio_switchable", false, true);
	toggle->set_value(preferences::ratioswitchable());
	toggle->set_did_state_changed(std::bind(&thome::did_ratio_switchable_changed, this, _1));
	symbols.clear();
	symbols["item"] = game_config::screen_modes.find(screenmode_ratio)->second;
	toggle->set_label(vgettext2("When switching mode, can select '$item'", symbols));


	tlabel* label = nullptr;
	if (game_config::os == os_ios) {
		symbols["desktop"] = _("Enter desktop");
		symbols["applet"] = _("Applet");
		symbols["store"] = aplt::all_fake_applets.find(aplt::builtinid_store)->second.name;
		label = find_widget<tlabel>(&grid, "remark", false, true);
		label->set_label(vgettext2("If '$desktop', and connect fails, you may enter the '$store' in '$applet' and let connect to network. Then come back to this interface.", symbols));
	}

	if (cfg_cpp_api_.base_scenes().empty()) {
		symbols["rdp_layer"] = _("home^rdp_layer");
		symbols["apps_layer"] = _("home^apps_layer");
		symbols["import_cfg"] = "klink_phone(default)(zh_CN).cfg";

		label = find_widget<tlabel>(&grid, "scene_list_remark", false, true);
		label->set_label(vgettext2("scene_list remark $rdp_layer, $apps_layer, $import_cfg", symbols));

		tbutton* button = find_widget<tbutton>(&grid, "vlog_example", false, true);
		button->set_icon("misc/start.png");
		connect_signal_mouse_left_click(
					*button
				, std::bind(
				&thome::click_open_url
				, this, std::ref(*button), true));

		button = find_widget<tbutton>(&grid, "user_guide", false, true);
		button->set_icon("misc/start.png");
		connect_signal_mouse_left_click(
					*button
				, std::bind(
				&thome::click_open_url
				, this, std::ref(*button), false));
	} else {
		find_widget<tgrid>(&grid, "url_grid", false, true)->set_visible(twidget::INVISIBLE);
	}
	// during running, scene_list maybe empty later, auto_enter_dcamera require dynmic.
	toggle = find_widget<ttoggle_button>(&grid, "auto_enter_dcamera", false, true);
	toggle->set_value(preferences::auto_enter_dcamera());
	toggle->set_did_state_changed(std::bind(&thome::did_auto_enter_dcamera_changed, this, _1));

	symbols["dcamera"] = aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name;
	std::string msg = vgettext2("When workout starts, it will automatically enter the '$dcamera'.", symbols);
	find_widget<tlabel>(&grid, "auto_enter_dcamera_label", false, true)->set_label(msg);

	tlistbox* list = find_widget<tlistbox>(&grid, "scene_list", false, true);
	list->enable_select(false);
	// list->set_did_can_drag(std::bind(&thelper_klink::did_scene_can_drag, this, _1, _2));
	scene_list_ = list;

	// reload_scene_list(*scene_list_);
}

void thome::pre_applet(tgrid& grid)
{
	treport* report = find_widget<treport>(&grid, "applets", false, true);
	tlabel* label = find_widget<tlabel>(window_, "pinyin_label", false, true);
	tbutton* button = find_widget<tbutton>(window_, "upgrade_pinyin", false, true);
	rapplets_pre_show(*window_, *report, label, button);

	battery_widget_ = find_widget<tlabel>(window_, "battery_info", false, true);
	button = find_widget<tbutton>(window_, "install_base_driver", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_install_base_driver
			, this, std::ref(*button)));
	button->set_label(_("Install"));
	start_base_node_widget_ = button;
	refresh_battery_label();
}

void thome::pre_event(tgrid& grid)
{
	tkeepalive::pre_show(*window_, *find_widget<tlabel>(&grid, "network_status", false, true));

	utils::string_map symbols;
	symbols["days"] = str_cast(LOGS_PB_MAX_DAYS);
	symbols["max_logs"] = str_cast(LOGS_PB_MAX_LOGS);
	std::string msg = vgettext2("A maximum of $days days of logs can be stored, and no more than $max_logs logs can be stored", symbols);
	find_widget<tlabel>(&grid, "remark", false, true)->set_label(msg);

	tlistbox* list = find_widget<tlistbox>(&grid, "event_list", false, true);
	list->enable_select(false);
	list->set_did_pullrefresh(cfg_2_os_size(48), std::bind(&thome::did_keepalive_pullrefresh_refresh, this, _1, _2, std::ref(grid)));

	event_list_ = list;
	reload_log_list(*event_list_);
}

void thome::pre_more(tgrid& grid)
{
	std::stringstream ss;

	trcswamp_login::pre_show(*window_, grid);
	{
		tstack* stack = find_widget<tstack>(&grid, "login_stack", false, true);
		stack->set_visible(twidget::INVISIBLE);
	}


	tbutton* button = find_widget<tbutton>(&grid, "syncapplet", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&thome::click_syncapplets
			, this, std::ref(*button)));
	button->set_visible(twidget::INVISIBLE);


	button = find_widget<tbutton>(&grid, "upgrade", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&thome::click_me_upgrade
			, this, std::ref(*button)));
	if (game_config::os == os_ios) {
		find_widget<tbutton>(&grid, "upgrade", false, true)->set_visible(twidget::INVISIBLE);
	}

	ss.str("");
	ss << "hdpi: " << str_cast(twidget::hdpi_scale);
	ss << " statusbar: " << str_cast(game_config::statusbar_height);
	ss << " navigationbar: " << str_cast(game_config::navigation_height);
	ss << " fullscreen_awlays: " << (game_config::fullscreen_awlays? "true": "fasle");
	SDL_Rect rect = video_.bound();
	ss << " video_.bound: (" << rect.x << ", " << rect.y << ", " << rect.w << ", " << rect.h << ")";
	find_widget<tlabel>(&grid, "devel", false).set_label(ss.str());

	ss.str("");
	ss << " V" << game_config::version.str(true) << (game_config::b64? "-b64": "-b32");
	if (game_config::os == os_windows) {
		SDL_OsInfo info;
		SDL_GetOsInfo(&info);
		ss << " model: " << info.model << " S/N: " << info.serialnumber << "(" << info.cpuid << ")";
	}
	find_widget<tlabel>(&grid, "version", false).set_label(ss.str());
}

bool thome::did_navigation_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to)
{
	if (from.at() == SCAN_LAYER) {
		scan_navigation_pre_change();

	} else if (from.at() == MORE_LAYER) {
		// tgrid& grid = *body_widget_->layer(MORE_LAYER);
		// find_widget<tgrid>(&grid, "generate_grid", false, true)->set_visible(twidget::INVISIBLE);
	}
	return true;
}

void thome::did_navigation_changed(treport& report, ttoggle_button& row)
{
	tgrid* current_layer = body_widget_->layer(row.at());
	body_widget_->set_radio_layer(row.at());

	current_layer_ = row.at();

	if (row.at() == RDP_LAYER) {
		reload_scene_list(*scene_list_);

	} else if (row.at() == SCAN_LAYER) {
		scan_navigation_changed();

	} else if (row.at() == EVENT_LAYER) {
		tcontrol& item = navigation_report_->item(EVENT_LAYER);
		item.set_icon("misc/event.png");

		instance->adjust_logs_pb();
		if (scroll_logs_to_bottom_ && event_list_->rows() > 0) {
			scroll_logs_to_bottom_ = false;

			tmsg_data_listbox_scroll_to_bottom* pdata = new tmsg_data_listbox_scroll_to_bottom(*window_, *event_list_);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, POST_MSG_LOGS_SCROLL_TO_BOTTOM, pdata);
		}

	} else if (row.at() == MORE_LAYER) {
		set_login_layer();
	}
}

// extern std::string scene_name_for_gui(const aplt::tbase_scene& scene);

void thome::reload_scene_list(tlistbox& list)
{
	list.clear();

	const std::string pref_scene_id = preferences::base_scene_id();
	const std::string& driver_scene_id = base_driver_.scene_id();

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	std::stringstream ss;
	std::map<std::string, std::string> data;
	for (std::vector<aplt::tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
		const aplt::tbase_scene& scene = *it;

		// data["id"] = scene.id;
		data["name"] = scene.name_for_gui(18);
		ss.str("");
		ss << task_name2_from_3id(applets_, scene.aplt, scene.task, null_str, true);
		ss << " . " << aplt::amp_modes.find(scene.amp)->second.id;
		data["task"] = ss.str();
		// data["amp"] = aplt::amp_modes.find(scene.amp)->second.id;
		data["input_vars"] = scene.join_input_vars();

		std::string png = "misc/start.png";
		bool is_me = scene.id == driver_scene_id;
		if (is_me) {
			png = base_driver_.subtask_state() == aplt::sts_ing? "misc/stop.png": "misc/start.png";
		}
		data["start"] = png;
		
		ttoggle_panel& row = list.insert_row(data);

		find_widget<tlabel>(&row, "input_vars", false, true)->set_border("label12_f2");

		tbutton* button = find_widget<tbutton>(&row, "start", false, true);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&thome::click_scene_start
				, this
				, std::ref(list), std::ref(*button), row.at()));
	}

	// find_widget<tgrid>(window_, "auto_enter_dcamera_grid", false, true)->set_visible(scenes.empty()? twidget::INVISIBLE: twidget::VISIBLE);
	find_widget<tgrid>(window_, "auto_enter_dcamera_grid", false, true)->set_visible(twidget::INVISIBLE);
}

void thome::click_scene_start(tlistbox& list, tbutton& widget, int row_at)
{
	if (!base_driver_.installed()) {
		utils::string_map symbols;
		symbols["driver"] = aplt::aplt_drivers.find(apltsotype_base)->second.name;
		std::string msg = vgettext2("$driver not installed.", symbols);
		gui2::show_message(null_str, msg);
		return;
	}

	const std::string pref_scene_id = preferences::base_scene_id();
	const std::string& driver_scene_id = base_driver_.scene_id();

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	const aplt::tbase_scene& scene = scenes[row_at];

	base_driver_.start_or_stop_subtask(scene, false);

	reload_scene_list(*scene_list_);

	if (base_driver_.subtask_state() == aplt::sts_ing && preferences::auto_enter_dcamera()) {
		window_->set_retval(APPLET0 + aplt::builtinid_dcamera);
	}
}

void thome::click_orientation(tbutton& widget)
{
	preferences::set_landscape(!preferences::landscape());
	widget.set_label(preferences::landscape()? _("Landscape"): _("Portrait"));
}

void thome::click_rdp(tbutton& widget)
{
	if (!will_enter_landscape_module(base_driver_, _("Enter desktop")))  {
		return;
	}
	VALIDATE(can_rdp(), null_str);
	net::IPAddress address((const uint8_t*)&rdpcookie_.ipv4, 4);
	preferences::set_currentremote(address.ToString());
	window_->set_retval(DESKTOP);
}

void thome::click_open_url(tbutton& widget, bool vlog_example)
{
	std::string url;
	if (vlog_example) {
		url = "https://www.bilibili.com/video/BV1m2bh6VEv8";
	} else {
		// user guide
		url = "https://www.bilibili.com/video/BV19U3E6rEYc";
	}
	SDL_OpenUrl(url.c_str());
}

void thome::did_ratio_switchable_changed(ttoggle_button& widget)
{
	preferences::set_ratioswitchable(widget.get_value());
}

bool thome::can_rdp() const
{
	return rdpcookie_.valid();
}

void thome::did_text_box_changed(tgrid& grid, ttext_box& widget)
{
	rdpcookie_.ipv4 = utils::to_ipv4(widget.label());

	tbutton* button = find_widget<tbutton>(&grid, "desktop", false, true);
	button->set_active(can_rdp());
}

void thome::did_auto_enter_dcamera_changed(ttoggle_button& widget)
{
	preferences::set_auto_enter_dcamera(widget.get_value());
}

void thome::refresh_battery_label()
{
	std::string label;
	bool start_base_node_is_visible = false;

	utils::string_map symbols;
	symbols["driver"] = aplt::aplt_drivers.find(apltsotype_base)->second.name;
	if (base_driver_.installed()) {
		VALIDATE(base_driver_.node_started(), null_str);
		const aplt::tapplet& aplt = *aplt::aplt_from_id(applets_, base_driver_.aplt_id());
		symbols["aplt"] = aplt.name2();	
		label = vgettext2("$driver installed: $aplt", symbols);

	} else {
		label = vgettext2("$driver not installed.", symbols);
		label = ht::generate_format(label, 0xffff0000);
		start_base_node_is_visible = true;
	}
	
	battery_widget_->set_label(label);
	start_base_node_widget_->set_visible(start_base_node_is_visible? twidget::VISIBLE: twidget::INVISIBLE);
}

void thome::click_install_base_driver(tbutton& widget)
{
	VALIDATE(!base_driver_.installed(), null_str);

	const std::string bundleid = aplt::get_bundleid(aplt::bundleid_leagor_basiclua);
	const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, bundleid);

	if (aplt == nullptr) {
		utils::string_map symbols;
		symbols["name"] = "kHome Lua";
		std::string msg = vgettext2("Please go to the store and download '$name' first.", symbols);
		gui2::show_message(null_str, msg);
		return;
	}

	preferences::set_driver(apltsotype_base, aplt->id);
	// base_driver_.set_slot(null_str, nullptr);
	drivers_.refresh();

	refresh_battery_label();
}

std::vector<aplt::tbuildin> thome::rapplets_get_fake_applets()
{
	std::vector<aplt::tbuildin> ret;
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_klink)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_health)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second);
	// ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_dnn)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_store)->second);


	return ret;
}

void thome::rapplets_did_click_fake_applet(const aplt::tbuildin& applet)
{
	if (applet.id == aplt::builtinid_health) {
		if (!will_enter_landscape_module(base_driver_, aplt::all_fake_applets.find(aplt::builtinid_health)->second.name))  {
			return;
		}
	}
	tdesktop::rapplets_did_click_fake_applet(applet);
}

void thome::app_did_network_changed(bool connected)
{
	if (current_layer_ == MORE_LAYER) {
		set_login_layer();
	}
/*
	if (connected && !current_user.update_version.empty()) {
		const version_info new_version(current_user.update_version);
		if (new_version > game_config::version) {
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_UPDATE_APP, nullptr);
		}
	}

	if (connected && !village_.valid()) {
		if (net::do_getlist(village_, false)) {
			set_copyright_label();
		}
	}
	set_login_active(mobile_->text_box()->label(), password_->text_box()->label());
*/
}

void thome::app_did_new_events(const std::set<trobot_event>& events)
{
	VALIDATE(!events.empty(), null_str);

	for (std::set<trobot_event>::const_reverse_iterator it = events.rbegin(); it != events.rend(); ++ it) {
		const trobot_event& e = *it;
		instance->add_aplt_ts_msg_log(logtype_warn, e.devicename, e.ts, e.to_log_msg(), 0, false);
	}
	// pb2::tlog new_log;
	// logs_pb_log_added(events.size(), new_log);

	if (current_layer_ != EVENT_LAYER) {
		tcontrol& item = navigation_report_->item(EVENT_LAYER);
		item.set_icon("misc/event_new.png");
	}
}

//
// event layer
//
void thome::did_keepalive_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer)
{
	xmit_keepalive(rect);
/*
	VALIDATE(current_user.valid(), null_str);
	gui2::tprogress_default_slot slot(std::bind(&net::do_filelist, current_user.sessionid, std::ref(network_items_)));
	run_with_progress(slot, null_str, null_str, 0, rect);
*/
}

void thome::reload_log_list(tlistbox& list)
{
	instance->logs_reload_log_list(list, false);
}

void thome::logs_pb_log_added(int count, const pb2::tlog& log)
{
	instance->logs_pb_log_added2(*event_list_, count, log, false);
}

//
// more layer
//

void thome::click_syncapplets(tbutton& widget)
{
	VALIDATE(current_user.valid(), null_str);

	std::vector<std::string> adds;
	adds.push_back("aplt.leagor.blesmart");
	adds.push_back("aplt.leagor.iaccess123");
	// adds.push_back("aplt.leagor.iaccess");
	adds.push_back("aplt.leagor.key");

	std::vector<std::string> removes;
	removes.push_back("aplt.leagor.key123");
	removes.push_back("aplt.leagor.blesmart");

	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_syncappletlist, _1, current_user.sessionid, aplt::app_launcher, std::ref(adds), std::ref(removes), false));
	bool ret = gui2::run_with_progress(slot, null_str, null_str, 1);
	if (!ret) {
		return;
	}
}

void thome::click_me_upgrade(tbutton& widget)
{
	VALIDATE(game_config::os == os_windows || game_config::os == os_android, null_str);

	tdisable_idle_lock lock;
	const version_info curr_version = game_config::version;
	const version_info new_version;
	net::upgrade_app(aplt::file_kdesktop_android, curr_version, new_version, NULL, false);
}

// === magic section

void thome::app_timer_handler(uint32_t now)
{
	if (current_layer_ == SCAN_LAYER) {
		scan_timer_handler(now);

	} else if (current_layer_ == APPLET_LAYER) {
		// battery info
		refresh_battery_label();
	}
}

void thome::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_HOME_POPUP_WIFI_PASSWORD:
		scan_OnMessage(msg);
		break;
	case MSG_LONGPRESS_APPLET:
		rapplets_OnMessage(msg);
		break;
	}
	if (msg->pdata) {
		delete msg->pdata;
		msg->pdata = nullptr;
	}
}

} // namespace gui2

