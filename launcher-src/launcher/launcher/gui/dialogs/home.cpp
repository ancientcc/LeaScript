#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/home.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "rose_version.hpp"
#include "base_instance.hpp"

#include "serialization/parser.hpp"
#include "game_config.hpp"
#include "chinese.hpp"
#include "minizip/minizip.hpp"
#include "game_latex.hpp"
#include "health.hpp"

using namespace std::placeholders;

class tlatex_rsp2
{
public:
	tlatex_rsp2()
	{
		clear();
	}

	bool valid() const 
	{ 
		if (rspfile.empty() || latex80bytes.type == nposm) {
			return false;
		}
		return true;
	}

	void clear()
	{
		rspfile.clear();
		latex80bytes.type = nposm;
	}

	void set(const std::string& _rspfile, const trsp_header& _header, const trsp_latex80bytes& _latex80bytes)
	{
		VALIDATE(!_rspfile.empty(), null_str);
		VALIDATE(_latex80bytes.type >= rsplatextype_min && _latex80bytes.type <= rsplatextype_max, null_str);

		rspfile = _rspfile;
		memcpy(&header, &_header, sizeof(header));
		memcpy(&latex80bytes, &_latex80bytes, sizeof(latex80bytes));

		version = version_from_2uint32(header.version, header.build_date);
	}

public:
	std::string rspfile; // full rspfile name
	trsp_header header;
	trsp_latex80bytes latex80bytes;
	version_info version;
};

bool read_latex_rsp(const std::string& path_to_rsp, trsp_header& header, trsp_latex80bytes& latex80bytes, const std::string& zip_file)
{
	// latex_rsp2.clear();

    tsha1reader src(path_to_rsp, false, NULL);
	if (!src.valid()) {
		return false;
	}
	int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_latex80bytes)) {
		return false;
	}

	// trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_latex))) {
		return false;
	}
    if (header.version != SDL_FOURCC(0, 0, 0, RSP_LATEX_VER)) {
        return false;
    }
	if (payload_size != sizeof(trsp_header) + header.zip_size) {
		// SDL_Log("upgrade_app, verify payload_size fail");
		return false;
	}

	// trsp_latex80bytes latex80bytes;
	memset(&latex80bytes, 0, sizeof(latex80bytes));
	posix_fread(src.fp, &latex80bytes, sizeof(latex80bytes));

	if (latex80bytes.type < rsplatextype_min || latex80bytes.type > rsplatextype_max) {
		return false;
	}
	if (latex80bytes.desc[RSP_MAXDESCBYTES] != '\0' || !utils::is_utf8str(latex80bytes.desc, SDL_strlen(latex80bytes.desc))) {
		return false;
	}
	if (latex80bytes.reserve0 != 0 || latex80bytes.reserve1 != 0 || latex80bytes.reserve2 != 0) {
		return false;
	}

	const int one_block = 4 * 1024 * 1024;
	src.resize_data(one_block);

	const int start = sizeof(trsp_header) + sizeof(trsp_latex80bytes);
	const int end = payload_size;

	// generate iaccess.apk
	tfile dest(zip_file, GENERIC_WRITE, CREATE_ALWAYS);
	if (!dest.valid()) {
		// std::string err = _("Failed to open the file to be written");
		// if (!quiet && !err.empty()) {
		//	gui2::show_message(null_str, err);
		// }
		return false;
	}

	int pos = start;
	posix_fseek(src.fp, pos);

	while (pos < end) {
		int bytes = one_block;
		if (pos + bytes > end) {
			bytes = end - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(dest.fp, src.data, bytes);

		pos += bytes;
	}

	// latex_rsp2.set(path_to_rsp, header, latex80bytes);
    return true;
}

namespace gui2 {

REGISTER_DIALOG(launcher, home)

thome::thome(gui2::trstore::tslot& slot, net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, 
	std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers& drivers, tbase_driver& base_driver, tspeech_driver& speech_driver, tiot_driver& iot_driver, const SDL_OsInfo& os_info,
	aplt::thealth& health)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, aplt::tdesktop(*this, slot, applets, APPLET0)
	, applets_(applets)
	, drivers_(drivers)
	, base_driver_(base_driver)
	, speech_driver_(speech_driver)
	, iot_driver_(iot_driver)
	, os_info_(os_info)
	, health_(health)
	// , peripheral_name_(RDPD_PERIPHERAL_NAME)
	, peripheral_name_()
	, manufacturer_id_(65520) // 0xfff0
	, start_ticks_(SDL_GetTicks())
	, battery_widget_(nullptr)
	, start_base_node_widget_(nullptr)
	, status_widget_(nullptr)
	, battery_max_str_(_("battery^max"))
	, battery_charge_str_(_("battery^charge"))
	, battery_cutoff_str_(_("battery^cutoff"))
	, battery_curr_str_(_("battery^curr"))
{
	set_timer_interval(800);
}

static std::string board_model_name()
{
	if (game_config::board_model != nposm) {
		return game_config::board_models.find(game_config::board_model)->second;
	}
	return _("Unknown");
}

void thome::pre_show()
{
	window_->set_escape_disabled(true);
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	std::stringstream ss;
	utils::string_map symbols;

	ss.str("");
	if (game_config::os == os_windows) {
		symbols["icon"] = ht::generate_img("misc/lq48.png~SCALE(24, 24)");
		// ss << "<img>src=" << filename << " align=left box=no</img>";
		// symbols["icon"] = "LQ";
		symbols["user_guide"] = _("User guide");
		ss << vgettext2("help desc, $icon, $user_guide", symbols);
		ss << "\n";
	}
	ss << "V" << game_config::version.str(true) << (game_config::b64? "-b64": "-b32");
	if (game_config::no_check_ble) {
		ss << "-nocheckble";
	}
	ss << "    " << "(" << board_model_name() << ")";
	ss << "libkosapi: V" << game_config::kosapi_ver.str(true);
	if (fake_libkosapi_so) {
		ss << ht::generate_format("(fake libkosapi.so)", 0xffff0000);
	}
	ss << "    S/N: " << os_info_.serialnumber;
	ss << "(" << os_info_.cpuid << ")";
	find_widget<tlabel>(window_, "version", false).set_label(ss.str());

	treport* report = find_widget<treport>(window_, "applets", false, true);
	tlabel* label = find_widget<tlabel>(window_, "pinyin_label", false, true);
	tbutton* button = find_widget<tbutton>(window_, "upgrade_pinyin", false, true);
	rapplets_pre_show(*window_, *report, label, button);

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);
	status_widget_->set_visible(twidget::INVISIBLE);

	find_widget<tlabel>(window_, "sn_label", false).set_label(_("Device name"));
	button = find_widget<tbutton>(window_, "update_sn", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_update_sn
			, this, std::ref(*button)));
	button->set_label(preferences::sn());

	button = find_widget<tbutton>(window_, "update_blepassword", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_update_blepassword
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "upgrade_app", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_upgrade_app
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "stop_speak", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_stop_speak
			, this, std::ref(*button)));
/*
	button = find_widget<tbutton>(window_, "upgrade_pinyin", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_upgrade_pinyin
			, this, std::ref(*button)));
	refresh_pinyin_label();
*/
	button = find_widget<tbutton>(window_, "upgrade_latex", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_upgrade_latex
			, this, std::ref(*button)));
	refresh_latex_label();

	battery_widget_ = find_widget<tlabel>(window_, "battery_info", false, true);
	button = find_widget<tbutton>(window_, "start_base_node", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&thome::click_start_base_node
			, this, std::ref(*button)));
	button->set_label(_("Restart base node"));
	start_base_node_widget_ = button;
	refresh_battery_label();

	// enable/disable bleperipheral
	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "bleperipheral", false, true);
	toggle->set_did_state_changed(std::bind(&thome::did_bleperipheral_changed, this, _1));
	toggle->set_value(preferences::bleperipheral());

	if (game_config::os != os_windows) {
		toggle = find_widget<ttoggle_button>(window_, "disable_keyboard", false, true);
		toggle->set_did_state_changed(std::bind(&thome::did_disable_keyboard_changed, this, _1));
		toggle->set_value(preferences::keyboard_style() == keyboard_style_disable);

	} else {
		find_widget<tgrid>(window_, "disable_keyboard_grid", false, true)->set_visible(twidget::INVISIBLE);
	}
}

void thome::post_show()
{
}

void thome::did_bleperipheral_changed(ttoggle_button& widget)
{
	if (widget.get_value()) {
		// disable --> enable
		if (!pble_.is_advertising()) {
			start_advertising();
		}

	} else {
		// enable --> disable
		if (pble_.is_advertising()) {
			pble_.stop_advertising();
		}
	}
	preferences::set_bleperipheral(widget.get_value());
}

void thome::did_disable_keyboard_changed(ttoggle_button& widget)
{
	preferences::set_keyboard_style(widget.get_value()? keyboard_style_disable: keyboard_style_default);
}

bool thome::did_verify_sn(const std::string& label)
{
	if (label == preferences::sn()) {
		return false;
	}
	return is_valid_rdpd_MFG_data(label);
}

void thome::start_advertising()
{
	const std::string sn = preferences::sn();
	VALIDATE(!sn.empty(), null_str);
	const std::string uuid_rdpd_service = "5356";
	const std::string sn2 = utils::truncate_to_max_bytes(sn.c_str(), sn.size(), MAX_RDPD_MFG_DATA_BYTES);
	pble_.start_advertising(uuid_rdpd_service, peripheral_name_, manufacturer_id_, (const uint8_t*)sn2.c_str(), sn2.size());
	if (!game_config::no_check_ble && !pble_.is_advertising()) {
		gui2::show_message(null_str, _("Failed to start Bluetooth advertising(Peripheral). Please check whether the device has a Bluetooth hardware module installed."));
		throw CVideo::quit();
	}
}

void thome::click_update_sn(tbutton& widget)
{
/*
	// if (game_config::os == os_windows) {
		latex::make_lualatex_fmt();
		int ii = 0;
		return;
	// }
*/

	std::string title = find_widget<tlabel>(window_, "sn_label", false, true)->label();
	std::string placeholder = _("ASCII printable characters or chinese");

	utils::string_map symbols;
	symbols["min"] = str_cast(MIN_RDPD_MFG_DATA_BYTES);
	symbols["max"] = str_cast(MAX_RDPD_MFG_DATA_BYTES);
	std::string remark = vgettext2("device name remark [$min, $max]", symbols);
	gui2::tedit_box_param param(title, null_str, placeholder, preferences::sn(), remark, null_str, _("OK"), MAX_RDPD_MFG_DATA_BYTES, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&thome::did_verify_sn, this, _1);
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 8);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	widget.set_label(param.result);
	preferences::set_sn(param.result);
	if (pble_.is_advertising()) {
		pble_.stop_advertising();
		start_advertising();
	}
}

void thome::click_update_blepassword(tbutton& widget)
{
	{
		// SDL_Reboot(30 * 1000);
		// SDL_Reboot(0);
		// SDL_Shutdown(0);

		// return;
	}

	utils::string_map symbols;
	symbols["password"] = DEFAULT_BLEPASSWORD;

	const std::string title = _("Update BLE password");
	const std::string placeholder = _("Digit or alpha");
	const std::string remark = vgettext2("If not modified, the password is $password", symbols);
	gui2::tedit_box_param param(title, null_str, placeholder, null_str, remark, null_str, _("OK"), MAX_BLEPASSWORD_SIZE, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&leagor_verify_blepassword, _1);
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 8);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	preferences::set_blepassword(param.result, false);
}

static std::string get_sn_path(const std::string& str)
{
	if (str.empty()) {
		return null_str;
	}

	size_t pos;
	if (game_config::os == os_android) {
		pos = game_config::preferences_dir.find("Android/data");

	} else if (game_config::os == os_windows) {
		pos = game_config::preferences_dir.find("RoseApp");
		
	} else {
		VALIDATE(false, null_str); // unknown os
	}

	VALIDATE(pos != std::string::npos, null_str);
	std::string result = game_config::preferences_dir.substr(0, pos) + str;
	if (game_config::os == os_windows) {
		SDL_MakeDirectory(result.c_str());
	} else {
		SDL_MakeDirectory(result.c_str());
		VALIDATE(SDL_IsDirectory(result.c_str()), null_str);
	}
	SDL_Log("sn_path: %s", result.c_str());
	return result;
}

void pre_SDL_UpdateApp()
{
	bool close = false;
	if (close) {
		SDL_Log("for fixed android kill new-launcher, close all rdp connection before SDL_UpdateApp");
		instance->unregister_server(server_rdpd);

	}
	else {
		SDL_Log("don't close all rdp connection before SDL_UpdateApp");
	}
}

void thome::click_upgrade_app(tbutton& widget)
{
	if (game_config::os == os_windows) {
		gui2::show_message(null_str, _("upgrade launcher on windows"));
		return;
	}

	// const version_info curr_version("0.1.1-20230112");
	const version_info curr_version = game_config::version;
	// const version_info new_version("0.1.1-20230113");
	const version_info new_version;
	bool reboot = true;
	if (game_config::board_model == board_lubancat3) {
		reboot = false;
	}
	net::upgrade_app(aplt::file_launcher_android, curr_version, new_version, NULL, reboot);

/*
	uint32_t flags = SDL_WifiGetFlags();
	if (flags & SDL_WifiFlagEnable) {
		SDL_Log("click_upgrade_app, now is enabled, ->disable");
		SDL_WifiSetEnable(SDL_FALSE);
	} else {
		SDL_Log("click_upgrade_app, now is disabled, ->enable");
		SDL_WifiSetEnable(SDL_TRUE);
	}
*/
}
/*
void thome::refresh_pinyin_label()
{
	tlabel* label = find_widget<tlabel>(window_, "pinyin_label", false, true);
	tbutton* button = find_widget<tbutton>(window_, "upgrade_pinyin", false, true);

	std::string label_str;
	std::string button_str;
	if (chinese::curr_pinyin.rsp.valid()) {
		label->set_visible(twidget::VISIBLE);

		utils::string_map symbols;
		symbols["version"] = chinese::curr_pinyin.rsp.version.str(true);
		label_str = vgettext2("Is using pinyin pack(V$version)", symbols);

		button_str = _("Upgrade");

	} else {
		label_str = _("No pinyin pack");
		button_str = _("Install");
	}
	label->set_label(label_str);
	button->set_label(button_str);
}

void thome::click_upgrade_pinyin(tbutton& widget)
{
	version_info curr_version;
	if (chinese::curr_pinyin.rsp.valid()) {
		curr_version = chinese::curr_pinyin.rsp.version;
	}
	const version_info new_version;
	bool result = net::download_pinyin_or_latexrsp(zipt_pinyin, curr_version, new_version);
	if (result) {
		chinese::load_pinyin_rsp();
		// speech_driver may be some class-member variables that require pinyin library to generate, for example: moveto_prefix_
		// since pinyin library has changed, those variables must also be recalculated.
		speech_driver_.restart();
		refresh_pinyin_label();
	}
}
*/
void thome::rapplets_did_pinyin_upgraded()
{
	// speech_driver may be some class-member variables that require pinyin library to generate, for example: moveto_prefix_
	// since pinyin library has changed, those variables must also be recalculated.
	speech_driver_.restart();
}

void thome::click_stop_speak(tbutton& widget)
{
	aplt::tpinyin& pinyin = chinese::curr_pinyin;
	if (chinese::curr_pinyin.rsp.valid()) {
		pinyin.stop_speak2();
	}
}

void thome::refresh_latex_label()
{
	tlabel* label = find_widget<tlabel>(window_, "latex_label", false, true);
	tbutton* button = find_widget<tbutton>(window_, "upgrade_latex", false, true);

	std::string label_str;
	std::string button_str;
	version_info version;
	if (latex::sandbox_is_valid(&version, nullptr, nullptr, nullptr)) {
		label->set_visible(twidget::VISIBLE);

		utils::string_map symbols;
		symbols["version"] = version.str(true);
		label_str = vgettext2("Is using LaTex pack(V$version)", symbols);

		button_str = _("Upgrade");

	} else {
		label_str = _("No LaTex pack");
		button_str = _("Install");
	}
	label->set_label(label_str);
	button->set_label(button_str);
}

void thome::write_distribution_cfg(const std::string& path, trsp_header& header, const trsp_latex80bytes& latex80bytes) const
{
	VALIDATE(latex::latex_types.count(latex80bytes.type) != 0, null_str);

	version_info version = version_from_2uint32(header.version, header.build_date);
	
	config cfg;
	cfg["version"].from_string(version.str(true), true);
	cfg["ts"].from_int64(latex80bytes.ts);
	cfg["type"].from_int(latex80bytes.type);
	cfg["desc"].from_string(latex80bytes.desc, true);

	std::stringstream out;
	if (!cfg.empty()) {
		write(out, cfg);
	}
	VALIDATE(!out.str().empty(), null_str);

	write_file(path + "/" + APLT_DISTRIBUTION_CFG, out.str().c_str(), out.str().size());
}

bool thome::load_latex_rsp() const
{
	const std::string rspfile = game_config::preferences_dir + "/latex.rsp";
	const std::string zip_file = game_config::preferences_dir + "/__temp.zip";
	const std::string unzipped_dir = game_config::preferences_dir + "/__temp/sandbox";
	const std::string unzipped_dir2 = game_config::preferences_dir + "/__temp";
	const std::string miktex_dir = game_config::preferences_dir + "/miktex";
	const std::string short_miktex_dir = utils::extract_file(miktex_dir);

	utils::string_map symbols;

	SDL_Log("%u, 1/7: read rspfile(%s), and generate zip_file(%s)", SDL_GetTicks(), rspfile.c_str(), zip_file.c_str());
	trsp_header header;
	trsp_latex80bytes latex80bytes;
	bool valid = read_latex_rsp(rspfile, header, latex80bytes, zip_file);
	if (!valid) {
		symbols["rspfile"] = rspfile;
		std::string err = vgettext2("Failed to extract zip data from File '$rspfile'.", symbols);
		gui2::show_message(null_str, err);
		return false;
	}
	
	SDL_Log("%u, 2/7: delete unzipped_dir(%s)", SDL_GetTicks(), unzipped_dir2.c_str());
	SDL_DeleteFiles(unzipped_dir2.c_str());

	SDL_Log("%u, 3/7: unzip zip_file(%s) to unzipped_dir(%s)", SDL_GetTicks(), zip_file.c_str(), unzipped_dir.c_str());
	minizip::unzip_file(zip_file, unzipped_dir, null_str, null_str);

	SDL_Log("%u, 4/7: delete rspfile(%s)", SDL_GetTicks(), rspfile.c_str());
	SDL_DeleteFiles(rspfile.c_str());

	SDL_Log("%u, 5/7: delete miktex_dir(%s)", SDL_GetTicks(), miktex_dir.c_str());
	SDL_DeleteFiles(miktex_dir.c_str());
 
	SDL_Log("%u, 6/7: raname unzipped_dir2(%s) to short_miktex_dir(%s)", SDL_GetTicks(), unzipped_dir2.c_str(), short_miktex_dir.c_str());
	SDL_RenameFile(unzipped_dir2.c_str(), short_miktex_dir.c_str());

	SDL_Log("%u, 7/7: generate %s/distribution.cfg", SDL_GetTicks(), miktex_dir.c_str());
	write_distribution_cfg(miktex_dir, header, latex80bytes);

	std::vector<std::string> deletes = {
		{miktex_dir + "/sandbox/miktex/data"}
	};
	for (std::vector<std::string>::const_iterator it = deletes.begin(); it != deletes.end(); ++ it) {
		const std::string& name = *it;
		SDL_DeleteFiles(name.c_str());
	}

	latex::make_lualatex_fmt();
	return true;
}

void thome::click_upgrade_latex(tbutton& widget)
{
	if (game_config::os == os_android) {
		const std::vector<std::string> files = {
			{"/system/fonts/SourceHanSansSC-Bold.otf"},
			{"/system/fonts/SourceHanSansSC-Regular.otf"},
			{"/system/fonts/SourceHanSerifSC-Bold.otf"},
			{"/system/fonts/SourceHanSerifSC-Regular.otf"},
			{"/system/fonts/ukai.ttc"}
		};
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string& file = *it;;
			if (!SDL_IsFile(file.c_str())) {
				utils::string_map symbols;
				symbols["ver"] = "eng.leagor.20260208";
				std::string msg = vgettext2("Please upgrade the Android image. To use LaTeX, the version must be at least '$ver'.", symbols);
				gui2::show_message(null_str, msg);
				return;
			}
		}
	}

	version_info curr_version;
	if (latex::sandbox_is_valid(&curr_version, nullptr, nullptr, nullptr)) {
	}
	const version_info new_version;
	bool result = net::download_pinyin_or_latexrsp(zipt_latex, curr_version, new_version);
	if (result) {
		latex::clear_cache();
		load_latex_rsp();

		// speech_driver may be some class-member variables that require pinyin library to generate, for example: moveto_prefix_
		// since pinyin library has changed, those variables must also be recalculated.
		// speech_driver_.restart();
		refresh_latex_label();
	}
}

void thome::refresh_battery_label()
{
	std::string label;

	bool start_base_node_is_visible = false;
	if (base_driver_.node_started()) {
		char buf[256];

		const uint32_t last_ticks = SDL_max(aplt::valuex.last_NMTHREAD_battery_level_ticks(), base_driver_.last_install_ticks());
		int still_ms = SDL_GetTicks() - last_ticks;
		// const int battery_still_threshold = 10000; // 30 second
		if (still_ms < BATTERY_STILL_THRESHOLD_MS) {
			const tbattery_info_C& info = base_driver_.get_battery_info();
			SDL_snprintf(buf, sizeof(buf), "%s(%.2f)  %s(%.2f)  %s(%.2f)   %s: %.3f",
				battery_max_str_.c_str(), info.max, battery_charge_str_.c_str(), info.charge,
				battery_cutoff_str_.c_str(), info.cutoff, battery_curr_str_.c_str(), aplt::valuex.battery_level);
			label = buf;

		} else {
			utils::string_map symbols;
			symbols["time"] = utils::format_elapse_hms(still_ms / 1000);
			label = vgettext2("It has been $time that the battery voltage has not been received, check the base serial's connection. After improve, click", symbols);
			label = ht::generate_format(label, 0xffff0000);

			start_base_node_is_visible = true;
		}

	} else {
		label = _("Valid base-drive needs to be set up, as well as serial's path and baudrate");
		label = ht::generate_format(label, 0xffff0000);
	}
	battery_widget_->set_label(label);
	start_base_node_widget_->set_visible(start_base_node_is_visible? twidget::VISIBLE: twidget::INVISIBLE);
}

void thome::click_start_base_node(tbutton& widget)
{
	VALIDATE(base_driver_.node_started(), null_str);

	if (!instance->will_enter_sys_module(widget.label())) {
		return;
	}

	base_driver_.restart_node();
}

std::vector<aplt::tbuildin> thome::rapplets_get_fake_applets()
{
	std::vector<aplt::tbuildin> ret;

	const bool allow_dnn = false;
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_settings)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_klink)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_speech)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_task)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_courseware)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_mkscript)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_health)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_center)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_map)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_moveit)->second);
	if (allow_dnn) {
		ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_dnn)->second);
	}
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_mic)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_explorer)->second);
	ret.push_back(aplt::all_fake_applets.find(aplt::builtinid_store)->second);
	return ret;
}

void thome::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);

	// battery info
	refresh_battery_label();

	// advertise
	std::stringstream ss;
	ss.str("");
	if (!pble_.is_advertising()) {
		ss << "Not advertising";

	} else if (pble_.is_connected()) {
		ss << "advertising. Connected" << ": " << pble_.mac_addr().str();

	} else {
		ss << "advertising. Unconnected";
	}
	status_widget_->set_label(ss.str());
}

void thome::app_OnMessage(rtc::Message* msg)
{
	const uint32_t now = SDL_GetTicks();

	switch (msg->message_id) {
	case MSG_LONGPRESS_APPLET:
		rapplets_OnMessage(msg);
		break;
	default:
		break;
	}

	if (msg->pdata) {
		delete msg->pdata;
	}
}

} // namespace gui2

