#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/import2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "game_config.hpp"

#include "cfg_cpp_api.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(launcher, import2)

timport2::timport2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::tcfg_cpp_api& cfg_cpp_api)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, cfg_cpp_api_(cfg_cpp_api)
	, product_list_(nullptr)
	, import_widget_(nullptr)
{
	set_timer_interval(1000);
}

void timport2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Import kLink cfgs"));

	utils::string_map symbols;

	symbols["klink_cfg"] = klink_cfgs.find(cfgtype_klink)->second.name;
	symbols["task_cpp_cfg"] = klink_cfgs.find(cfgtype_task_cpp)->second.name;
	symbols["speech_cfg"] = klink_cfgs.find(cfgtype_speech)->second.name;
	const std::string msg = vgettext2("This operation will replace some of the configurations and scripts currently being used on your device, including:\n\n"
		"$klink_cfg\n$task_cpp_cfg\n$speech_cfg", symbols);
	find_widget<tlabel>(window_, "remark", false).set_label(msg);

	const std::string klink_cfg_dir = get_klink_cfg_dir(cfgtype_klink, false);
	const std::string filename = klink_cfg_dir + "/product_def.cfg";
	symbols["file"] = filename;

	tlistbox* list = find_widget<tlistbox>(window_, "product_list", false, true);
	list->set_did_row_changed(std::bind(&timport2::did_product_changed, this, _1, _2));
	// list->enable_select(false);
	product_list_ = list;

	tbutton* button = find_widget<tbutton>(window_, "import", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&timport2::click_import
			, this, std::ref(*button)));
	import_widget_ = button;

	load_product_def_cfg();
	reload_product_list();

	if (product_list_->rows() > 0) {
		product_list_->select_row(0);

	} else {
		import_widget_->set_visible(twidget::INVISIBLE);
	}
}

void timport2::post_show()
{
}

std::string timport2::load_product_def_cfg()
{
	VALIDATE(products_.empty(), null_str);

	utils::string_map symbols;
	const std::string klink_cfg_dir = get_klink_cfg_dir(cfgtype_klink, false);
	const std::string filename = klink_cfg_dir + "/product_def.cfg";
	symbols["file"] = filename;

	std::string stream;
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			return vgettext2("Cann't find $file, or size must be <= 512K bytes", symbols);
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			return vgettext2("$file isn't utf-8 format", symbols);
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	aplt::read_config_ex(stream, true, top_cfg);

	int index = 0;
	BOOST_FOREACH (const config& c, top_cfg.child_range("product")) {
		products_.push_back(tproduct(klink_cfg_dir, c));
	}

	return null_str;
}

void timport2::reload_product_list()
{
	tlistbox& list = *product_list_;
	list.clear();

	std::map<std::string, std::string> data;
	for (std::vector<tproduct>::const_iterator it = products_.begin(); it != products_.end(); ++ it) {
		const tproduct& product = *it;

		data["name"] = product.name;
		ttoggle_panel& row = list.insert_row(data);
	}
}

void timport2::did_product_changed(tlistbox& list, ttoggle_panel& row)
{
}

void timport2::click_import(tbutton& widget)
{
	VALIDATE(product_list_->rows() != 0, null_str);
	ttoggle_panel* row = product_list_->cursel();
	VALIDATE(row != nullptr, null_str);

	utils::string_map symbols;
	const tproduct& product = products_[row->at()];

	std::string errmsg;
	tauto_destruct_executor destruct_executor(std::bind(&did_post_show_err_message, std::ref(errmsg)));

	//
	// klink cfg
	//
	errmsg = cfg_cpp_api_.import_klink_cfg(product.klink_cfg);
	if (!errmsg.empty()) {
		return;
	}

	//
	// task_cpp cfg
	//
	std::map<std::string, aplt::ttask_cpp_pair> pairs_from_cfg;
	errmsg = cfg_cpp_api_.import_task_cpp_cfg(product.task_cpp_cfg, pairs_from_cfg);
	if (!errmsg.empty()) {
		return;
	}

	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = pairs_from_cfg.begin(); it != pairs_from_cfg.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		std::string err_msg;
		uint64_t res = aplt::task_cpp_is_valid(pair, err_msg);
		if (res != TCOOKIE3F_CHECK_OK) {
			tcookie3f cookie3f(res);
			std::stringstream err;
			if (err_msg.empty()) {
				err << "type: " << cookie3f.type << "field: " << cookie3f.field;
				// err << get_error_msg(pair, cookie3f.type, cookie3f.field);
			} else {
				err << err_msg;
			}
			errmsg = err.str();
			return;
		}
	}

	cfg_cpp_api_.save_task_pairs(pairs_from_cfg);
	cfg_cpp_api_.did_gui2task2_back();

	//
	// speech cfg
	//
	std::string nick;
	std::map<std::string, aplt::tspeech_sensor> sensors_from_cfg;
	errmsg = cfg_cpp_api_.import_speech_cfg(product.speech_cfg, nick, sensors_from_cfg);
	if (!errmsg.empty()) {
		return;
	}

	std::set<std::string> existed_names;
	for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = sensors_from_cfg.begin(); it != sensors_from_cfg.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;
		std::string err_msg;
		uint64_t res = speech_sensor_is_valid(sensor, err_msg);
		if (res == TCOOKIE3F_CHECK_OK) {
			VALIDATE(err_msg.empty(), null_str);
			VALIDATE(existed_names.count(sensor.name) == 0, null_str);
		}
		existed_names.insert(sensor.name);

		if (res != TCOOKIE3F_CHECK_OK) {
			tcookie3f cookie3f(res);
			std::stringstream err;
			
			if (err_msg.empty()) {
				// err << get_error_msg(sensor, cookie3f.type, cookie3f.field);
				err << "type: " << cookie3f.type << "field: " << cookie3f.field;
			} else {
				err << err_msg;
			}
			errmsg = err.str();
			return;
		}
	}
	cfg_cpp_api_.save_speech_sensors(nick, sensors_from_cfg);

	errmsg = _("Import finished");
}

void timport2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

