#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rdnn.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/rdnn_runner.hpp"
#include "gui/dialogs/rdnn_tflite_schema.hpp"
#include "gui/dialogs/rdnn_browse_script.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "preferences_display.hpp"
#include "language.hpp"
// #include "net.hpp"

#include "config_cache.hpp"
#include "base_instance.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video.hpp>

using namespace std::placeholders;

#define MAX_SETION_ITEMS	1000



namespace gui2 {

REGISTER_DIALOG(rose, rdnn)

trdnn::trdnn(tslot& slot, tcamera& camera, const config& app_cfg)
	: slot_(slot)
	, camera_(camera)
	, app_cfg_(app_cfg)
	, script_(tflite::curr_script)
	, start_layer_(DEVICE_LAYER)
	, limit_followed_(nullptr)
	, scripts_dirty_(false)
	, tflites_dirty_(false)
	, local_images_dirty_(false)
	, network_items_dirty_(true)
	, discover_tflites_received_(false)
	, discover_tflites_dirty_(true)
{
}

trdnn::~trdnn()
{
}

void trdnn::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	if (!slot_.dnn_warnning().empty()) {
		find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(slot_.dnn_warnning(), 0xffff0000));
	}

	VALIDATE(tflites_.empty(), null_str);
	collect_tflite(game_config::preferences_dir + "/tflites", scripts_, tflites_);
	collect_tflite(game_config::app_dir_root + "/tflites", scripts_, tflites_);
	// collect_tflite(game_config::path + "/data/core/tflites", scripts_, tflites_);
	collect_image(game_config::preferences_dir + "/images/misc", local_images_);

	if (tflite::current_user.valid()) {
/*
		??no-net
		gui2::tprogress_default_slot slot(std::bind(&net::do_filelist, current_user.sessionid, std::ref(network_items_)));
		run_with_progress(slot, null_str, null_str, 3000);
*/
	}

	if (!scripts_.empty()) {
		const std::string script_file = preferences::script();
		const tflite::tscript* script = script_exist_same_path(script_file);
		if (!script) {
			script = &*scripts_.begin();
		}
		set_test_script(*script);
	}

	body_ = find_widget<tstack>(window_, "body", false, true);
	pre_device();
	pre_test();
	// pre_service();
	// pre_showcase();

	treport* report = find_widget<treport>(window_, "navigation", false, true);
	tcontrol* item = &report->insert_item(null_str, _("Model"));

	item = &report->insert_item(null_str, _("Test"));

	// item = &report->insert_item(null_str, _("Service"));

	// item = &report->insert_item(null_str, _("Showcase"));

	report->set_did_item_changed(std::bind(&trdnn::did_body_changed, this, _1, _2));
	report->select_item(start_layer_);
}

void trdnn::click_notification(int layer)
{
}
/*
int trdnn::token_in_netdisk_tflites(const std::string& token) const
{
	VALIDATE(!token.empty(), null_str);
	for (std::set<tnetwork_item>::const_iterator it = network_items_.begin(); it != network_items_.end(); ++ it) {
		const tnetwork_item& item = *it;
		if (item.xmit_type == xmit_tflite && item.token == token) {
			return std::distance(network_items_.begin(), it);
		}
	}
	return nposm;
}

int trdnn::token_in_discover_tflites(const std::string& token) const
{
	VALIDATE(!token.empty(), null_str);
	for (std::set<ttoken_tflite>::const_iterator it = discover_tflites_.begin(); it != discover_tflites_.end(); ++ it) {
		const ttoken_tflite& tflite = *it;
		if (tflite.token == token) {
			return std::distance(discover_tflites_.begin(), it);
		}
	}
	return nposm;
}
*/
//
// test
//
void trdnn::pre_test()
{
	tgrid* test_layer = body_->layer(TEST_LAYER);

	//
	// start
	//
	tbutton* button = find_widget<tbutton>(test_layer, "start", false, true);
	button->set_label("misc/start.png");
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trdnn::click_start
				, this));
	const bool ios_disable_camera = true;
	if (ios_disable_camera && game_config::os == os_ios) {
		button->set_visible(twidget::INVISIBLE);
	}

	tlistbox& description = find_widget<tlistbox>(test_layer, "description", false);
	description.enable_select(false);
}

void trdnn::click_start()
{
	VALIDATE(!scripts_.empty() && !tflites_.empty(), null_str);
	const tflite::tscript& script = *scripts_.begin();
	VALIDATE(script.test && script.valid(), null_str);

	int at = tflites_exist_same_id(script.tflite, false);
	if (at == nposm) {
		at = tflites_exist_same_id(script.tflite, true);
	}
	VALIDATE(at != nposm, null_str);
	std::set<tflite::ttflite>::const_iterator it = tflites_.begin();
	std::advance(it, at);

	if (script.scenario == tflite::scenario_ocr) {
		VALIDATE(false, "don't support");
		// script_ = script;
		// window_->set_retval(OCR);
		
	} else {
		instance->popup_rcamera(true);
		{
			gui2::trdnn_runner dlg(camera_, script, *it, slot_.dnn_warnning());
			dlg.show();
		}
		instance->popup_rcamera(false);
	}
}

int64_t calculate_max_space()
{
	const int M = 1 << 20;
	if (tflite::current_user.vip == 1) {
		return 40 * M;
	} else if (tflite::current_user.vip == 2) {
		return 100 * M;
	}
	return 10 * M;
}

int64_t trdnn::calculate_available_space() const
{
	int64_t used_size = 0;
	for (std::set<tflite::tnetwork_item>::const_iterator it = network_items_.begin(); it != network_items_.end(); ++ it) {
		used_size += it->size;
	}

	const int64_t max_space = calculate_max_space();
	int64_t avaliable = max_space - used_size;
	if (avaliable < 0) {
		avaliable = 0;
	}

	return avaliable;
}

void trdnn::network_items_summary(tgrid& layer) const
{
	std::stringstream ss;
	utils::string_map symbols;

	symbols["count"] = str_cast(network_items_.size());
	ss << vgettext2("$count items", symbols);

	ss << "      " << _("space^Available") << ": ";
	
	int avaliable = calculate_available_space();
	ss << utils::format_i64size(avaliable);

	tlabel& label = find_widget<tlabel>(&layer, "summary", false);
	label.set_label(ss.str());
}

void trdnn::did_body_changed(treport& report, ttoggle_button& row)
{
	tgrid* current_layer = body_->layer(row.at());
	body_->set_radio_layer(row.at());

	if (row.at() == TEST_LAYER) {
		std::string script_file = preferences::script();

		const tflite::tscript* script = script_exist_same_path(script_file);
		if (!script || !script->test) {
			if (!scripts_.empty()) {
				script = &*scripts_.begin();
				VALIDATE(script->test, null_str);
				script_file = script->path();
			} else {
				script_file.clear();
			}
			preferences::set_script(null_str);
		}

		if (script) {
			fill_description_list(*current_layer, *script);
		} else {
			find_widget<tbutton>(current_layer, "start", false).set_active(false);
		}

	} else if (row.at() == DEVICE_LAYER) {
		if (scripts_dirty_ || tflites_dirty_ || local_images_dirty_) {
			tlistbox* tflites = find_widget<tlistbox>(current_layer, "tflites", false, true);
			fill_tflites_list(*tflites);

			scripts_dirty_ = false;
			tflites_dirty_ = false;
			local_images_dirty_ = false;
		}

	} else if (row.at() == NETWORK_LAYER) {
		if (network_items_dirty_) {
			tlistbox& list = find_widget<tlistbox>(current_layer, "items", false);
			fill_items_list(list);
			network_items_dirty_ = false;
		}

		network_items_summary(*current_layer);

	} else if (row.at() == SHOWCASE_LAYER) {
		treport* report = find_widget<treport>(current_layer, "showcase_navigation", false, true);
		tgrid* showcase_layer = showcase_stack_->layer(report->cursel()->at());
		if (report->cursel()->at() == DISCOVER_LAYER) {
			if (tflite::current_user.valid()) {
/*
				??no-net
				refresh_discover_tflites(*showcase_layer, 3000, SDL_Rect{nposm, nposm, nposm, nposm}, false);
*/
			}
		}
	}
}

void trdnn::fill_description_list(tgrid& layer, const tflite::tscript& script)
{
	VALIDATE(script.valid(), null_str);
	bool can_start = true;
	std::string err_report;

	std::stringstream ss;
	utils::string_map symbols;
	std::vector<std::pair<std::string, std::string> > values;

	ss.str("");
	if (game_config::mobile) {
		std::string str = script.path();
		size_t pos = str.find(game_config::path);
		if (pos == 0) {
			VALIDATE(script.res, null_str);
			ss << "<res>" << str.substr(game_config::path.size());
		} else {
			ss << "<Documents>" << str.substr(game_config::preferences_dir.size());
		}
	} else {
		ss << script.path();
	}
	values.push_back(std::make_pair(_("Script"), ss.str()));

	// tflite
	const std::string tflits_file = get_binary_file_location("tflites", script.tflite + "/" + script.tflite + ".tflite");
	ss.str("");
	if (game_config::mobile) {
		size_t pos = tflits_file.find(game_config::path);
		if (pos == 0) {
			ss << "<res>" << tflits_file.substr(game_config::path.size());
		} else {
			ss << "<Documents>" << tflits_file.substr(game_config::preferences_dir.size());
		}
	} else {
		ss << tflits_file;
	}
	if (!tflits_file.empty() && tflite::is_valid_tflite(tflits_file)) {

	} else {
		symbols["file"] = ht::generate_format(ss.str(), 0xffff0000);
		ss.str("");
		if (tflits_file.empty()) {
			symbols["file"] = ht::generate_format(script.tflite, 0xffff0000);
			ss << vgettext2("$file (Abscent)", symbols);
		} else {
			ss << vgettext2("$file (Invalid)", symbols);
		}

		can_start = false;
	}
	values.push_back(std::make_pair(_("tflits"), ss.str()));

	// source
	ss.str("");
	if (script.source == tflite::source_camera) {
		ss << _("Camera");
	} else {
		if (image::get_image(script.img).get() != nullptr) {
			ss << script.img;
		} else {
			symbols["png"] = ht::generate_format(script.img, 0xffff0000);
			ss << vgettext2("$png (Abscent)", symbols);
			can_start = false;
		}
	}
	values.push_back(std::make_pair("source", ss.str()));

	// scenario
	ss.str("");
	if (script.scenario == tflite::scenario_classifier) {
		ss << _("classifier");

	} else if (script.scenario == tflite::scenario_detect) {
		ss << _("obejct detect");

	} else if (script.scenario == tflite::scenario_ocr) {
		ss << _("OCR");

	} else if (script.scenario == tflite::scenario_pr) {
		ss << _("plate recognition");
	}
	values.push_back(std::make_pair("scenario", ss.str()));

	// width x height
	ss.str("");
	symbols["width"] = str_cast(script.width);
	symbols["height"] = str_cast(script.height);
	std::string color = "gray";
	if (script.color == tflite::color_rgb) {
		color = "rgb";
	} else if (script.color == tflite::color_bgr) {
		color = "bgr";
	}
	symbols["color"] = color;
	ss << vgettext2("$width x $height x $color", symbols);
	values.push_back(std::make_pair(_("Model input"), ss.str()));

	if (script.scenario == tflite::scenario_classifier) {
		// kthreshold
		ss.str("");

		if (true) {
			SDL_OsInfo osinfo;
			SDL_GetOsInfo(&osinfo);
			ss << osinfo.display;
		} else {
			ss << script.kthreshold;
		}
		values.push_back(std::make_pair(_("kthreshold"), ss.str()));
	}

	tlabel& warn = find_widget<tlabel>(&layer, "warn", false);
	warn.set_label(ht::generate_format(script.warn, 0xffff0000));

	tlistbox& description = find_widget<tlistbox>(&layer, "description", false);
	description.clear();
	std::map<std::string, std::string> data;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = values.begin(); it != values.end(); ++ it) {
		data["key"] = it->first;
		data["value"] = it->second;

		ttoggle_panel& row = description.insert_row(data);
		row.set_canvas_highlight(false, true);
	}

	find_widget<tbutton>(&layer, "start", false).set_active(can_start);
}

//
// device
//
void trdnn::pre_device()
{
	tgrid* local_layer = body_->layer(DEVICE_LAYER);

	tbutton* button = nullptr;
/*
	tbutton* button = find_widget<tbutton>(local_layer, "user_manager", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trdnn::click_user_manager
				, this));

	{
		std::vector<gui2::tformula_blit> blits;
		blits.push_back(gui2::tformula_blit("misc/documents.png", null_str, null_str, "(width)", "(height)"));
		blits.push_back(gui2::tformula_blit("misc/copy.png", null_str, null_str, "(width / 2)", "(width / 2)"));
		button->set_blits(blits);
	}
*/
	//
	// tflites
	//
	tlistbox* tflites = find_widget<tlistbox>(local_layer, "tflites", false, true);
	tflites->enable_select(false);
	fill_tflites_list(*tflites);

	tflites->set_did_can_drag(std::bind(&trdnn::did_local_tflite_can_drag, this, _1, _2));
	tflites->set_did_row_changed(std::bind(&trdnn::did_visualize_tflite, this, _2));

	button = &tflites->set_did_left_drag_widget_click("test", std::bind(&trdnn::click_set_test_script, this, _1, _2));
	button->set_icon("misc/bg_f3f3f3.png");

	button = &tflites->set_did_left_drag_widget_click("lookover", std::bind(&trdnn::click_lookover_tflite, this, _1, _2));
	button->set_icon("misc/bg_ff0000.png");
/*
	??no-net
	button = &tflites->set_did_left_drag_widget_click("upload", std::bind(&trdnn::click_upload_tflite, this, _1, _2));
	button->set_icon("misc/bg_f3f3f3.png");
*/
	button = &tflites->set_did_left_drag_widget_click("erase", std::bind(&trdnn::click_erase_tflite, this, _1, _2));
	button->set_icon("misc/bg_ff0000.png");
}
/*
void trdnn::click_user_manager()
{
	window_->set_retval(USER_MANAGER);
}
*/
void trdnn::fill_tflites_list(tlistbox& listbox)
{
	utils::string_map symbols;
	listbox.clear();

	std::map<std::string, std::string> data;
	int at = 0;
	for (std::set<tflite::tscript>::const_iterator it = scripts_.begin(); it != scripts_.end(); ++ it, at ++) {
		const tflite::tscript& script = *it;
		if (it == scripts_.begin()) {
			VALIDATE(script.test, null_str);
		} else {
			VALIDATE(!script.test, null_str);
		}

		data["label"] = script.id;
		data["size"] = utils::format_i64size(file_size(script.path()));

		ttoggle_panel& row = listbox.insert_row(data);
		row.set_cookie(tflite::xmit_script * MAX_SETION_ITEMS + at);

		tcontrol& panel = *dynamic_cast<tcontrol*>(row.find("panel", false));
		if (script.test) {
			panel.set_label("misc/red-translucent10_background.png");
		}

		std::vector<gui2::tformula_blit> blits;
		tcontrol& portrait = *dynamic_cast<tcontrol*>(row.find("portrait", false));
		blits.push_back(gui2::tformula_blit("misc/script.png", null_str, null_str, "(width)", "(height)"));
		if (script.res) {
			blits.push_back(gui2::tformula_blit("misc/pin.png", null_str, null_str, "(width / 2)", "(width / 2)"));
		}

		portrait.set_blits(blits);
	}

	at = 0;
	for (std::set<tflite::ttflite>::const_iterator it = tflites_.begin(); it != tflites_.end(); ++ it, at ++) {
		const tflite::ttflite& tflite = *it;
		data["label"] = tflite.id;
		data["size"] = utils::format_i64size(file_size(tflite.tflits_path()));

		ttoggle_panel& row = listbox.insert_row(data);
		row.set_cookie(tflite::xmit_tflite * MAX_SETION_ITEMS + at);

		tcontrol& panel = *dynamic_cast<tcontrol*>(row.find("panel", false));

		std::vector<gui2::tformula_blit> blits;
		tcontrol& portrait = *dynamic_cast<tcontrol*>(row.find("portrait", false));
		blits.push_back(gui2::tformula_blit("misc/tensorflow.png", null_str, null_str, "(width)", "(height)"));
		if (tflite.res) {
			blits.push_back(gui2::tformula_blit("misc/pin.png", null_str, null_str, "(width / 2)", "(width / 2)"));
		}

		portrait.set_blits(blits);
	}

	at = 0;
	for (std::set<std::string>::const_iterator it = local_images_.begin(); it != local_images_.end(); ++ it, at ++) {
		const std::string& file = *it;
		data["label"] = utils::extract_file(file);
		data["size"] = utils::format_i64size(file_size(file));

		ttoggle_panel& row = listbox.insert_row(data);
		row.set_cookie(tflite::xmit_image * MAX_SETION_ITEMS + at);

		tcontrol& panel = *dynamic_cast<tcontrol*>(row.find("panel", false));

		std::vector<gui2::tformula_blit> blits;
		tcontrol& portrait = *dynamic_cast<tcontrol*>(row.find("portrait", false));
		surface surf = image::get_image(file);
		if (surf.get()) {
			blits.push_back(gui2::tformula_blit(surf, null_str, null_str, "(width)", "(height)"));
		} else {
			blits.push_back(gui2::tformula_blit("misc/image.png", null_str, null_str, "(width)", "(height)"));
		}

		portrait.set_blits(blits);
	}
}

bool trdnn::did_collect_tflite(const std::string& dir, const SDL_dirent2* dirent, std::set<tflite::tscript>& scripts, std::set<tflite::ttflite>& files) const
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		const std::string dir2 = dir + "/" + dirent->name;
		std::set<std::string> files2 = files_in_directory(dir2);
		bool exist_tflite = false;
		for (std::set<std::string>::const_iterator it = files2.begin(); it != files2.end(); ++ it) {
			const std::string file = dir2 + "/" + *it;
			if (!exist_tflite && it->find(dirent->name) == 0) {
				if (it->size() == strlen(dirent->name) + strlen(".tflite") && tflite::is_valid_tflite(file)) {
					exist_tflite = true;
					continue;
				}
			}
		}
		if (exist_tflite) {
			tflite::ttflite tflite(dirent->name, dir.find(game_config::path) == 0);
			VALIDATE(tflites_exist_same_id(tflite.id, tflite.res) == nposm, null_str);
			files.insert(tflite);
		}
	} else {
		const std::string file = dir + "/" + dirent->name;

		size_t pos = file.rfind(".cfg");
		if (pos != std::string::npos && pos + 4 == file.size()) {
			std::string err_report;
			tflite::tscript script(file, dir.find(game_config::path) == 0, &err_report);
			if (script.valid()) {
				scripts.insert(script);
			}
		}
	}

	return true;
}

void trdnn::collect_tflite(const std::string& path, std::set<tflite::tscript>& scripts, std::set<tflite::ttflite>& tflites) const
{
	::walk_dir(path, false, std::bind(
				&trdnn::did_collect_tflite
				, this
				, _1, _2, std::ref(scripts), std::ref(tflites)));
}

bool trdnn::did_collect_image(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& images) const
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		const std::string file = dir + "/" + dirent->name;

		size_t pos = file.rfind(".png");
		if (pos != std::string::npos && pos + 4 == file.size()) {
			images.insert(file);
		}
	}

	return true;
}

void trdnn::collect_image(const std::string& path, std::set<std::string>& images) const
{
	::walk_dir(path, false, std::bind(
				&trdnn::did_collect_image
				, this
				, _1, _2, std::ref(images)));
}

int trdnn::split_cookie(const int cookie, int& xmit_type) const
{
	int at;
	if (cookie >= tflite::xmit_image * MAX_SETION_ITEMS) {
		xmit_type = tflite::xmit_image;
		at = cookie - tflite::xmit_image * MAX_SETION_ITEMS;
		VALIDATE(at < MAX_SETION_ITEMS, null_str);

	} else if (cookie >= tflite::xmit_tflite * MAX_SETION_ITEMS) {
		xmit_type = tflite::xmit_tflite;
		at = cookie - tflite::xmit_tflite * MAX_SETION_ITEMS;

	} else {
		VALIDATE(cookie >= tflite::xmit_script * MAX_SETION_ITEMS, null_str);

		xmit_type = tflite::xmit_script;
		at = cookie - tflite::xmit_script * MAX_SETION_ITEMS;
	}

	return at;
}

bool trdnn::did_local_tflite_can_drag(tlistbox& listbox, ttoggle_panel& row)
{
	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);
	std::set<tflite::tscript>::const_iterator script_it = scripts_.begin();
	std::set<tflite::ttflite>::const_iterator tflite_it = tflites_.begin();
	std::set<std::string>::const_iterator image_it = local_images_.begin();

	if (xmit_type == tflite::xmit_script) {
		std::advance(script_it, at);
	} else if (xmit_type == tflite::xmit_tflite) {
		std::advance(tflite_it, at);
	} else {
		std::advance(image_it, at);
	}

	// can set test
	bool can_set_test = xmit_type == tflite::xmit_script && at > 0;
	if (can_set_test) {
		std::set<tflite::tscript>::const_iterator it = scripts_.begin();
		VALIDATE(it->test, null_str);
		const tflite::tscript& script = *script_it;
		if (at > 0) {
			VALIDATE(!script.test, null_str);
		}
	}

	// can look over
	bool can_lookover = true;
	if (xmit_type != tflite::xmit_script) {
		can_lookover = false;
	} else {
		{
			// rdnn_browse_script has bug. enable in feature. 
			can_lookover = false;
		}
	}

	// can erase
	bool can_erase = true;
	if (xmit_type == tflite::xmit_script) {
		can_erase = !script_it->res;
	} else if (xmit_type == tflite::xmit_tflite) {
		can_erase = !tflite_it->res;
	}

	std::map<std::string, twidget::tvisible> visibles;
	visibles.insert(std::make_pair("test", can_set_test? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("lookover", can_lookover? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("upload", twidget::INVISIBLE)); // now don't net
	visibles.insert(std::make_pair("erase", can_erase? twidget::VISIBLE: twidget::INVISIBLE));

	listbox.left_drag_grid_set_widget_visible(visibles);
	return true;
}

void trdnn::did_visualize_tflite(ttoggle_panel& row)
{
	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);

	if (xmit_type == tflite::xmit_tflite) {
		std::set<tflite::ttflite>::const_iterator it = tflites_.begin();
		std::advance(it, at);
		const tflite::ttflite& tflite = *it;

		std::string file = tflite.tflits_path();

		gui2::trdnn_tflite_schema dlg(file);
		dlg.show();
	}
}

const tflite::tscript* trdnn::script_exist_same_path(const std::string& path) const
{
	int at = 0;
	for (std::set<tflite::tscript>::const_iterator it = scripts_.begin(); it != scripts_.end(); ++ it, at ++) {
		const tflite::tscript& script = *it;
		const std::string this_path = script.path();
		if (this_path == path) {
			return &script;
		}
	}
	return nullptr;
}

int trdnn::tflites_exist_same_id(const std::string& id, bool res) const
{
	int at = 0;
	for (std::set<tflite::ttflite>::const_iterator it = tflites_.begin(); it != tflites_.end(); ++ it, at ++) {
		const tflite::ttflite& tflite = *it;
		if (tflite.id != id) {
			continue;
		}
		if (!!res == !!tflite.res) {
			return at;
		}
	}
	return nposm;
}

void trdnn::set_test_script(const tflite::tscript& script)
{
	std::set<tflite::tscript>::const_iterator desire_it = scripts_.find(script);
	VALIDATE(desire_it != scripts_.end(), null_str);

	const int at = std::distance(scripts_.begin(), desire_it);

	// set desire tflite's test to true.
	tflite::tscript desire_script = *desire_it;
	VALIDATE(!desire_script.test, null_str);
	desire_script.test = true;

	if (at != 0) {
		tflite::tscript begin = *scripts_.begin();
		if (begin.test) {
			begin.test = false;
			scripts_.erase(scripts_.begin());
			scripts_.insert(begin);
		}
	}
	//  
	scripts_.erase(desire_it);
	scripts_.insert(desire_script);

	preferences::set_script(scripts_.begin()->path());
}

void trdnn::click_set_test_script(tlistbox& list, ttoggle_panel& row)
{
	// const int drag_at = list.drag_at();
	// ttoggle_panel& row = list.row_panel(list.drag_at());

	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);
	VALIDATE(xmit_type == tflite::xmit_script, null_str);

	std::set<tflite::tscript>::const_iterator script_it = scripts_.begin();
	std::advance(script_it, at);

	set_test_script(*script_it);
	fill_tflites_list(list);
}

void trdnn::click_lookover_tflite(tlistbox& list, ttoggle_panel& row)
{
	// ttoggle_panel& row = list.row_panel(list.drag_at());

	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);
	if (xmit_type == tflite::xmit_script) {
		std::set<tflite::tscript>::const_iterator it = scripts_.begin();
		std::advance(it, at);
		const tflite::tscript& script = *it;

		tfile file(script.path(), GENERIC_READ, OPEN_EXISTING);
		int64_t fsize = file.read_2_data();
		VALIDATE(fsize > 0, null_str);

		gui2::trdnn_browse_script dlg(script.id, file.data, _("Model"), null_str);
		dlg.show();

	} else if (xmit_type == tflite::xmit_tflite) {
		std::set<tflite::ttflite>::const_iterator it = tflites_.begin();
		std::advance(it, at);
		const tflite::ttflite& tflite = *it;

		std::string file = tflite.tflits_path();

		gui2::trdnn_tflite_schema dlg(file);
		dlg.show();
	}
}
/*
??no-net
void trdnn::click_upload_tflite(tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	// ttoggle_panel& row = list.row_panel(list.drag_at());
	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);

	int type;
	std::string name;
	std::vector<std::pair<std::string, std::string> > files;
	int64_t fsize, timestamp;
	if (xmit_type == xmit_script) {
		std::set<tscript>::iterator desire_it = scripts_.begin();
		std::advance(desire_it, at);
		const tscript& script = *desire_it;

		type = xmit_script;
		name = script.id;
		files.push_back(std::make_pair("file", script.path()));
		fsize = file_size(script.path());

	} else if (xmit_type == xmit_tflite) { 
		std::set<ttflite>::iterator desire_it = tflites_.begin();
		std::advance(desire_it, at);
		const ttflite& tflite = *desire_it;

		type = xmit_tflite;
		name = tflite.id;
		files.push_back(std::make_pair("tflits", tflite.path() + "/" + tflite.id + ".tflits"));
		files.push_back(std::make_pair("labels_txt", tflite.path() + "/" + tflite::labels_txt));

		{
			tfile file(tflite.tflits_path(), GENERIC_READ, OPEN_EXISTING);
			fsize = file.read_2_data();
			SDL_memcpy(&timestamp, file.data, 8);
		}

	} else {
		std::set<std::string>::iterator desire_it = local_images_.begin();
		std::advance(desire_it, at);
		const std::string& image = *desire_it;

		type = xmit_image;
		name = utils::extract_file(image);
		files.push_back(std::make_pair("file", image));
		fsize = file_size(image);
	}

	std::string token;
	{
		tdisable_idle_lock lock;
		int64_t available = calculate_available_space();
		gui2::tprogress_default_slot slot(std::bind(&net::upload_file, current_user.sessionid, type, name, std::ref(files), available, std::ref(token)));
		bool ret = run_with_progress(slot, null_str, _("Uploading"), 0);
		if (!ret) {
			return;
		}
		if (type == xmit_tflite) {
			VALIDATE(!token.empty(), null_str);
		}
	}

	tnetwork_item item(name, fsize, type);
	if (type == xmit_tflite) {
		item.set_model_special(token, timestamp, null_str, null_str, null_str, false, 0, 0);
	}
	network_items_.insert(item);
	
	network_items_dirty_ = true;
}
*/
void trdnn::click_erase_tflite(tlistbox& list, ttoggle_panel& row)
{
	// ttoggle_panel& row = list.row_panel(list.drag_at());
	int xmit_type;
	const int at = split_cookie(row.cookie(), xmit_type);

	utils::string_map symbols;
	bool require_set_test = false;
	std::string erase_path;
	if (xmit_type == tflite::xmit_script) {
		std::set<tflite::tscript>::iterator desire_it = scripts_.begin();
		std::advance(desire_it, at);
		const tflite::tscript& script = *desire_it;
		VALIDATE(!script.res, null_str);

		symbols["item"] = script.id;
		const int res = gui2::show_messagefs(vgettext2("Do you want to delete $item from Documents?", symbols), _("Delete"));
		if (res == gui2::twindow::CANCEL) {
			return;
		}

		erase_path = script.path();

		require_set_test = script.test && scripts_.size() > 1;

		// delte row from listbox
		scripts_.erase(desire_it);

	} else if (xmit_type == tflite::xmit_tflite) {
		std::set<tflite::ttflite>::iterator desire_it = tflites_.begin();
		std::advance(desire_it, at);
		const tflite::ttflite& tflite = *desire_it;
		VALIDATE(!tflite.res, null_str);

		symbols["item"] = tflite.id;
		const int res = gui2::show_messagefs(vgettext2("Do you want to delete $item from Documents?", symbols), _("Delete"));
		if (res == gui2::twindow::CANCEL) {
			return;
		}

		// delete directory form filesystem
		erase_path = tflite.path();

		// delte row from listbox
		tflites_.erase(desire_it);

	} else {
		std::set<std::string>::iterator desire_it = local_images_.begin();
		std::advance(desire_it, at);
		const std::string& image = *desire_it;

		symbols["item"] = utils::extract_file(image);
		const int res = gui2::show_messagefs(vgettext2("Do you want to delete $item from Documents?", symbols), _("Delete"));
		if (res == gui2::twindow::CANCEL) {
			return;
		}

		// delete directory form filesystem
		erase_path = image;

		// delte row from listbox
		local_images_.erase(desire_it);
	}

	// delete directory form filesystem
	SDL_DeleteFiles(erase_path.c_str());

	if (require_set_test) {
		VALIDATE(xmit_type == tflite::xmit_script && !scripts_.empty(), null_str);
		set_test_script(*scripts_.begin());
	}

	// erase will change other row's cookie, for sample to fill tflites again.
	fill_tflites_list(list);
}
/*
//
// network
//
void trdnn::pre_service()
{
	tgrid* network_layer = body_->layer(NETWORK_LAYER);

	tbutton* button = find_widget<tbutton>(network_layer, "notification", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trdnn::click_notification
				, this
				, NETWORK_LAYER));
	button->set_visible(twidget::HIDDEN);

	tlistbox& list = find_widget<tlistbox>(network_layer, "items", false);
	list.enable_select(false);
	list.set_did_can_drag(std::bind(&trdnn::did_network_can_drag, this, _1, _2));
	list.set_did_row_changed(std::bind(&trdnn::did_tflite_attrs, this, _1, _2));
	if (current_user.valid()) {
		list.set_did_pullrefresh(cfg_2_os_size(48), std::bind(&trdnn::did_network_pullrefresh_refresh, this, _1, _2, std::ref(*network_layer)));
	} else {
		list.set_did_pullrefresh(nposm, NULL);
	}

	button = &list.set_did_left_drag_widget_click("showcase", std::bind(&trdnn::click_showcase_model, this, _1, _2));
	button->set_icon("misc/bg_ff0000.png");

	button = &list.set_did_left_drag_widget_click("download", std::bind(&trdnn::click_download_network_item, this, _1, _2));
	button->set_icon("misc/bg_f3f3f3.png");

	button = &list.set_did_left_drag_widget_click("erase", std::bind(&trdnn::click_erase_network_item, this, std::ref(*network_layer), _1, _2));
	button->set_icon("misc/bg_ff0000.png");

}
*/
void trdnn::fill_items_list(tlistbox& list)
{
	list.clear();

	std::stringstream ss;
	utils::string_map symbols;

	std::map<std::string, std::string> data;
	for (std::set<tflite::tnetwork_item>::const_iterator it = network_items_.begin(); it != network_items_.end(); ++ it) {
		const tflite::tnetwork_item& item = *it;

		data["label"] = item.name;
		data["size"] = utils::format_i64size(item.size);

		if (item.xmit_type == tflite::xmit_tflite) {
			data["follows"] = str_cast(item.follows);
			data["comments"] = str_cast(item.comments);

			ss.str("");
			ss << utils::format_time_date(item.timestamp) << "(" << tflite::treadme::state_name(item.state) << ")";
			data["timestamp"] = ss.str();
			// data["state"] = tflite::treadme::state_name(item.state);
			data["keywords"] = tflite::treadme::keywords_name(item.keywords);
			data["description"] = item.description;

		} else {
			data["description"] = null_str;
		}

		ttoggle_panel& row = list.insert_row(data);

		tcontrol& panel = *dynamic_cast<tcontrol*>(row.find("panel", false));
		
		std::vector<gui2::tformula_blit> blits;
		timage& portrait = *dynamic_cast<timage*>(row.find("portrait", false));
		timage& showcase = *dynamic_cast<timage*>(row.find("showcase", false));
		tgrid& tflite_grid = *dynamic_cast<tgrid*>(row.find("tflite_grid", false));

		if (item.xmit_type == tflite::xmit_script) {
			blits.push_back(gui2::tformula_blit("misc/script.png", null_str, null_str, "(width)", "(height)"));
			tflite_grid.set_visible(twidget::INVISIBLE);
			showcase.set_visible(twidget::INVISIBLE);

		} else if (item.xmit_type == tflite::xmit_tflite) {
			portrait.set_visible(twidget::INVISIBLE);

			timage& showcase = *dynamic_cast<timage*>(row.find("showcase", false));
			if (!item.showcase) {
				showcase.set_visible(twidget::INVISIBLE);
			}
		} else {
			VALIDATE(item.xmit_type == tflite::xmit_image, null_str);
			blits.push_back(gui2::tformula_blit("misc/image.png", null_str, null_str, "(width)", "(height)"));
			tflite_grid.set_visible(twidget::INVISIBLE);
			showcase.set_visible(twidget::INVISIBLE);
		}
		
		portrait.set_blits(blits);
	}
}
/*
bool trdnn::did_network_can_drag(tlistbox& list, ttoggle_panel& row)
{
	std::set<tnetwork_item>::const_iterator item_it = network_items_.begin();
	std::advance(item_it, row.at());
	const tnetwork_item& item = *item_it;

	std::map<std::string, twidget::tvisible> visibles;
	visibles.insert(std::make_pair("showcase", item.xmit_type == xmit_tflite? twidget::VISIBLE: twidget::INVISIBLE));

	if (item.xmit_type == xmit_tflite) {
		tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("showcase", true));
		button->set_label(item.showcase? _("Out showcase"): _("Into showcase"));
	}

	list.left_drag_grid_set_widget_visible(visibles);
	return true;
}

void trdnn::did_tflite_attrs(tlistbox& list, ttoggle_panel& row)
{
	std::set<tnetwork_item>::const_iterator desire_it = network_items_.begin();
	std::advance(desire_it, row.at());

	tnetwork_item item = *desire_it;

	if (item.xmit_type == xmit_tflite) {
		gui2::ttflite_attrs dlg(item);
		dlg.show();

		const tnetwork_item& original_item = *desire_it;

		std::map<std::string, std::string> attrs;
		if (item.name != original_item.name) {
			attrs.insert(std::make_pair("newname", item.name));
		}
		if (item.state != original_item.state) {
			attrs.insert(std::make_pair("state", item.state));
		}
		if (item.keywords != original_item.keywords) {
			attrs.insert(std::make_pair("keywords", utils::join(item.keywords)));
		}
		if (item.description != original_item.description) {
			attrs.insert(std::make_pair("desc", item.description));
		}
		if (attrs.empty()) {
			return;
		}

		{
			gui2::tprogress_default_slot slot(std::bind(&net::do_updatemodelattrs, current_user.sessionid, original_item.name, std::ref(attrs)));
			bool ret = run_with_progress(slot, null_str, null_str, 3000);
			if (!ret) {
				return;
			}
		}

	} else {
		return;
	}

	network_items_.erase(desire_it);
	network_items_.insert(item);

	fill_items_list(list);

	int at = token_in_discover_tflites(item.token);
	if (at != nposm) {
		std::set<ttoken_tflite>::iterator erase_it = discover_tflites_.begin();
		std::advance(erase_it, at);
		ttoken_tflite item2 = *erase_it;

		item2.name = item.name;
		item2.state = item.state;
		item2.keywords = item.keywords;
		item2.description = item.description;

		discover_tflites_.erase(erase_it);
		discover_tflites_.insert(item2);

		// this token maybe in discover tflites.
		discover_tflites_dirty_ = true;
	}
}

void trdnn::click_showcase_model(tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	const int drag_at = row.at();

	std::set<tnetwork_item>::iterator drag_it = network_items_.begin();
	std::advance(drag_it, drag_at);

	tnetwork_item item = *drag_it;
	const bool into_showcase = item.showcase? false: true;

	if (!into_showcase && item.follows) {
		utils::string_map symbols;
		symbols["item"] = item.name;
		const int res = gui2::show_messagefs(vgettext2("Once out of showcase, follows will be cleared, do you want to out $item?", symbols), _("Out showcase"));
		if (res == gui2::twindow::CANCEL) {
			return;
		}
	}

	std::map<std::string, std::string> attrs;
	attrs.insert(std::make_pair(showcase_identifier, into_showcase? "yes": "no"));

	{
		gui2::tprogress_default_slot slot(std::bind(&net::do_updatemodelattrs, current_user.sessionid, item.name, std::ref(attrs)));
		bool ret = run_with_progress(slot, null_str, null_str, 3000);
		if (!ret) {
			return;
		}
	}

	item.showcase = !item.showcase;

	if (!into_showcase) {
		item.follows = 0;
		item.comments = 0;
	}

	//  
	network_items_.erase(drag_it);
	network_items_.insert(item);

	fill_items_list(list);

	// {
		// this tfilte maybe in discover tfiltes.
		discover_tflites_received_ = false;
	// }
}

void trdnn::click_download_network_item(tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	// ttoggle_panel& row = list.row_panel(download_at);
	const int download_at = row.at();

	std::set<tnetwork_item>::iterator download_it = network_items_.begin();
	std::advance(download_it, download_at);

	const tnetwork_item& item = *download_it;

	std::vector<std::pair<std::string, std::string> > files;
	if (item.xmit_type == xmit_script) {
		files.push_back(std::make_pair("file", game_config::preferences_dir + "/tflites/" + item.name));

	} else if (item.xmit_type == xmit_image) {
		files.push_back(std::make_pair("file", game_config::preferences_dir + "/images/misc/" + item.name));

	} else {
		VALIDATE(item.xmit_type == xmit_tflite, null_str);
		files.push_back(std::make_pair("tflits", game_config::preferences_dir + "/tflites/" + item.name + "/" + item.name + ".tflits"));
		files.push_back(std::make_pair("labels_txt", game_config::preferences_dir + "/tflites/" + item.name + "/" + tflite::labels_txt));

	}

	{
		tdisable_idle_lock lock;
		bool ret = false;
		if (item.xmit_type != xmit_tflite) {
			gui2::tprogress_default_slot slot(std::bind(&net::download_file, current_user.sessionid, item.xmit_type, item.name, std::ref(files)));
			ret = run_with_progress(slot, null_str, _("Downloading"), 0);
		} else {
			gui2::tprogress_default_slot slot(std::bind(&net::download_model, current_user.sessionid, item.token, std::ref(files)));
			ret = run_with_progress(slot, null_str, _("Downloading"), 0);
		}
		if (!ret) {
			return;
		}
	}

	if (item.xmit_type == xmit_script) {
		std::string err_report;
		scripts_.insert(tscript(files.begin()->second, false, err_report));
		scripts_dirty_ = true;

	} else if (item.xmit_type == xmit_image) {
		local_images_.insert(files.begin()->second);
		local_images_dirty_ = true;

	} else {
		// std::string first = files.begin()->first;
		tflites_.insert(ttflite(item.name, false));
		tflites_dirty_ = true;
	}
}

void trdnn::click_erase_network_item(tgrid& layer, tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	// const int erase_at = list.drag_at();
	// ttoggle_panel& row = list.row_panel(erase_at);
	const int erase_at = row.at();

	std::set<tnetwork_item>::iterator erase_it = network_items_.begin();
	std::advance(erase_it, erase_at);

	const tnetwork_item& item = *erase_it;

	utils::string_map symbols;
	symbols["item"] = item.name;
	const int res = gui2::show_messagefs(vgettext2("Do you want to delete $item?", symbols), _("Delete"));
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	{
		gui2::tprogress_default_slot slot(std::bind(&net::do_deletefile, current_user.sessionid, item.xmit_type, item.name));
		bool ret = run_with_progress(slot, null_str, _("Deleting"), 2000);
		if (!ret) {
			return;
		}
	}

	if (item.xmit_type == xmit_tflite) {
		// this tfilte maybe in discover tfiltes.
		discover_tflites_received_ = false;
	}

	list.erase_row(erase_at);
	network_items_.erase(erase_it);

	network_items_summary(layer);
}

void trdnn::did_network_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer)
{
	VALIDATE(current_user.valid(), null_str);
	gui2::tprogress_default_slot slot(std::bind(&net::do_filelist, current_user.sessionid, std::ref(network_items_)));
	run_with_progress(slot, null_str, null_str, 0, rect);

	tlistbox& list = find_widget<tlistbox>(&layer, "items", false);
	fill_items_list(list);
	network_items_dirty_ = false;
}

//
// show
//
void trdnn::pre_showcase()
{
	tgrid* show_layer = body_->layer(SHOWCASE_LAYER);


	showcase_stack_ = find_widget<tstack>(show_layer, "showcase_stack", false, true);
	tgrid& discover_grid = *showcase_stack_->layer(DISCOVER_LAYER);
	pre_showcase_discover(discover_grid);

	treport* report = find_widget<treport>(show_layer, "showcase_navigation", false, true);
	report->insert_item(null_str, _("Discover"));

	limit_followed_ = find_widget<ttoggle_button>(show_layer, "limit_followed", false, true);
	report->set_did_item_changed(std::bind(&trdnn::did_showcase_item_changed, this, _1, _2));
	report->select_item(0);

	limit_followed_->set_did_state_changed(std::bind(&trdnn::did_limit_followed_changed, this, std::ref(discover_grid), _1));
}

void trdnn::pre_showcase_discover(tgrid& showcase_grid)
{
	tgrid* discover_layer = showcase_stack_->layer(DISCOVER_LAYER);

	tlistbox& list = find_widget<tlistbox>(discover_layer, "tflites", false);
	list.enable_select(false);
	list.set_did_can_drag(std::bind(&trdnn::did_discover_tflite_can_drag, this, _1, _2));

	if (current_user.valid()) {
		list.set_did_pullrefresh(cfg_2_os_size(48), std::bind(&trdnn::did_discocver_pullrefresh_refresh, this, _1, _2, std::ref(*discover_layer)));
	} else {
		list.set_did_pullrefresh(nposm, NULL);
	}

	tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("comment", true));
	button->set_icon("misc/bg_f3f3f3.png");

	button = &list.set_did_left_drag_widget_click("download", std::bind(&trdnn::click_download_discover_tflite, this, std::ref(*discover_layer), _1, _2));
	button->set_icon("misc/bg_f3f3f3.png");

	button = &list.set_did_left_drag_widget_click("follow", std::bind(&trdnn::click_follow_token_tflite, this, std::ref(*discover_layer), _1, _2));
	button->set_icon("misc/bg_ff0000.png");
}

void trdnn::did_showcase_item_changed(treport& report, ttoggle_button& row)
{
	tgrid* current_layer = showcase_stack_->layer(row.at());
	showcase_stack_->set_radio_layer(row.at());

	if (row.at() == DISCOVER_LAYER) {
		if (current_user.valid()) {
			refresh_discover_tflites(*current_layer, 3000, SDL_Rect{nposm, nposm, nposm, nposm}, false);
		}
	}
}

void trdnn::fill_discover_tflites_list(tlistbox& list)
{
	list.clear();

	std::stringstream ss;
	utils::string_map symbols;

	std::map<std::string, std::string> data;
	for (std::set<ttoken_tflite>::const_iterator it = discover_tflites_.begin(); it != discover_tflites_.end(); ++ it) {
		const ttoken_tflite& item = *it;

		data["label"] = item.name;
		data["size"] = format_i64size(item.size);

		ss.str("");
		ss << "(" << item.nickname << ")";
		data["author"] = ss.str();

		ss.str("");
		ss << format_time_date(item.timestamp) << "(" << treadme::state_name(item.state) << ")";
		data["timestamp"] = ss.str();
		data["follows"] = str_cast(item.follows);
		data["comments"] = str_cast(item.comments);

		// data["state"] = treadme::state_name(item.state);
		data["keywords"] = treadme::keywords_name(item.keywords);

		data["description"] = item.description;

		ttoggle_panel& row = list.insert_row(data);

		tcontrol& portrait = *dynamic_cast<tcontrol*>(row.find("portrait", false));
		int at = token_in_netdisk_tflites(item.token);
		if (item.followed) {
			VALIDATE(at == nposm, null_str);
			portrait.set_label("misc/followed.png");
			
		} else if (at != nposm) {
			portrait.set_label("misc/supplier.png");

		} else {
			portrait.set_visible(twidget::INVISIBLE);
		}
	}
}

void trdnn::discover_tflites_summary(tgrid& layer) const
{
	std::stringstream ss;
	utils::string_map symbols;

	symbols["count"] = str_cast(discover_tflites_.size());
	ss << vgettext2("$count models", symbols);

	tlabel& label = find_widget<tlabel>(&layer, "summary", false);
	label.set_label(ss.str());
}

bool trdnn::did_discover_tflite_can_drag(tlistbox& list, ttoggle_panel& row)
{
	std::set<ttoken_tflite>::const_iterator item_it = discover_tflites_.begin();
	std::advance(item_it, row.at());
	const ttoken_tflite& item = *item_it;
	bool my = token_in_netdisk_tflites(item.token) != nposm;

	std::map<std::string, twidget::tvisible> visibles;
	visibles.insert(std::make_pair("comment", twidget::INVISIBLE));
	visibles.insert(std::make_pair("download", my? twidget::INVISIBLE: twidget::VISIBLE));

	if (my) {
		visibles.insert(std::make_pair("follow", twidget::INVISIBLE));

	} else {
		tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("follow", true));
		button->set_label(item.followed? _("Unfollow"): _("Follow"));

		visibles.insert(std::make_pair("follow", twidget::VISIBLE));
	}

	list.left_drag_grid_set_widget_visible(visibles);
	return true;
}

void trdnn::did_limit_followed_changed(tgrid& discover_grid, ttoggle_button& widget)
{
	refresh_discover_tflites(discover_grid, 3000, SDL_Rect{nposm, nposm, nposm, nposm}, true);
}

void trdnn::refresh_discover_tflites(tgrid& layer, int hidden_ms, const SDL_Rect& rect, bool force)
{
	VALIDATE(current_user.valid(), null_str);

	if (force || !discover_tflites_received_) {
		gui2::tprogress_default_slot slot(std::bind(&net::do_findmodel, current_user.sessionid, limit_followed_->get_value(), std::ref(discover_tflites_)));
		run_with_progress(slot, null_str, _("Find model"), hidden_ms, rect);

		discover_tflites_received_ = true;
		discover_tflites_dirty_ = true;
	}
	if (discover_tflites_dirty_) {
		tlistbox& list = find_widget<tlistbox>(&layer, "tflites", false);
		fill_discover_tflites_list(list);
		discover_tflites_dirty_ = false;
	}
	discover_tflites_summary(*&layer);
}

void trdnn::did_discocver_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer)
{
	refresh_discover_tflites(layer, 0, rect, true);
}

void trdnn::click_follow_token_tflite(tgrid& layer, tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	// ttoggle_panel& row = list.row_panel(drag_at);
	const int drag_at = row.at();

	std::set<ttoken_tflite>::iterator drag_it = discover_tflites_.begin();
	std::advance(drag_it, drag_at);

	ttoken_tflite item = *drag_it;
	VALIDATE(token_in_netdisk_tflites(item.token) == nposm, null_str);

	bool follow = !item.followed;
	if (follow) {
		gui2::tprogress_default_slot slot(std::bind(&net::do_followmodel, current_user.sessionid, item.token, net::op_follow));
		bool ret = run_with_progress(slot, null_str, null_str, 3000);
		if (!ret) {
			return;
		}
	} else {
		gui2::tprogress_default_slot slot(std::bind(&net::do_followmodel, current_user.sessionid, item.token, net::op_unfollow));
		bool ret = run_with_progress(slot, null_str, null_str, 3000);
		if (!ret) {
			return;
		}
	}

	list.cancel_drag();

	if (follow) {
		item.followed = true;
		item.follows ++;

	} else {
		item.followed = false;
		item.follows --;
	}

	discover_tflites_.erase(drag_it);

	if (!limit_followed_->get_value() || follow) {
		discover_tflites_.insert(item);
	}

	fill_discover_tflites_list(list);
}

void trdnn::click_download_discover_tflite(tgrid& layer, tlistbox& list, ttoggle_panel& row)
{
	if (!network_require_login()) {
		return;
	}

	// const int download_at = list.drag_at();
	// ttoggle_panel& row = list.row_panel(download_at);
	const int download_at = row.at();

	std::set<ttoken_tflite>::iterator download_it = discover_tflites_.begin();
	std::advance(download_it, download_at);

	const ttoken_tflite& item = *download_it;
	VALIDATE(token_in_netdisk_tflites(item.token) == nposm, null_str);

	{
		const std::string path = game_config::app_dir_root + "/cert/model_user_agreement.txt";
		tfile file(path, GENERIC_READ, OPEN_EXISTING);
		int64_t fsize = file.read_2_data();
		VALIDATE(fsize > 0, null_str);

		gui2::tbrowse_script dlg(_("Model User Agreement"), file.data, dsgettext("rose-lib", "Cancel"), _("Download"));
		dlg.show();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	std::vector<std::pair<std::string, std::string> > files;

	const std::string prefix = "";
	const std::string adjust_name = prefix + item.name; // keep local and showcase be same name.
	files.push_back(std::make_pair("tflits", game_config::preferences_dir + "/tflites/" + adjust_name + "/" + prefix + item.name + ".tflits"));
	files.push_back(std::make_pair("labels_txt", game_config::preferences_dir + "/tflites/" + adjust_name + "/" + tflite::labels_txt));

	{
		tdisable_idle_lock lock;
		gui2::tprogress_default_slot slot(std::bind(&net::download_model, current_user.sessionid, item.token, std::ref(files)));
		bool ret = run_with_progress(slot, null_str, _("Downloading"), 0);
		if (!ret) {
			return;
		}
	}
	
	tflites_.insert(ttflite(adjust_name, false));
	tflites_dirty_ = true;
}
*/

} // namespace gui2

