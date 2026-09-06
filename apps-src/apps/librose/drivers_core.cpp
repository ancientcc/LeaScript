#define GETTEXT_DOMAIN "rose-lib"

#include "rose_global.hpp"
#include "drivers_core.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
// #include "game_config.hpp"
#include "base_instance.hpp"
#include "base_driver_core.hpp"
#include "lua_task_api.hpp"


void tty2::refresh_ttys()
{
	ttys.clear();

	SDL_ttyUSB* ttyUSBs;
	int count = instance->sdl_GetTtyUSB(&ttyUSBs);
	for (int at = 0; at < count; at ++) {
		const SDL_ttyUSB& tty = ttyUSBs[at];
		if (!valid_tty(tty)) {
			continue;
		}
		ttys.insert(std::make_pair(ttyUSBs[at].path, ttyUSBs[at]));
	}
	if (count != 0) {
		SDL_free(ttyUSBs);
	}
}

const SDL_ttyUSB* tty2::from_path(const std::string& path) const
{
	if (ttys.count(path) == 0) {
		return nullptr;
	}
	return &ttys.find(path)->second;
}

std::string tty2::to_string(const SDL_ttyUSB& tty) const
{
	VALIDATE(valid_tty(tty), null_str);

	std::stringstream ss;
	ss << tty.path;
	if (tty.name[0] != '\0') {
		ss << "(" << tty.name << ")";
	}
	return ss.str();
}
/*
std::string get_libroseaplt_so_path(const std::string& res_path)
{
	return aplt_so_path(res_path, LIBROSEAPLT_SO);
}
*/
trose_library::trose_library(base_instance& instance, aplt::tapplet& _aplt, const std::string& _res_path, const std::string& _preferences_dir)
	: instance_(instance)
	, aplt(_aplt)
	, fp(nullptr)
	, aplt_load(nullptr)
	, aplt_unload(nullptr)
{
	// instance_.increment_textdomain_usage(aplt.id);
	instance_.increment_textdomain_usage(aplt);
	VALIDATE(!_res_path.empty() && !_preferences_dir.empty(), null_str);
	reset_function_ptrs();
	open(_res_path, _preferences_dir);
}

trose_library::~trose_library()
{
	close();
	// must not use global variable 'instance'. it is nullptr when ~trose_library.
	// instance_.decrement_textdomain_usage(aplt.id);
	instance_.decrement_textdomain_usage(aplt);
}

void trose_library::open(const std::string& _res_path, const std::string& _preferences_dir)
{
	VALIDATE(!_res_path.empty() && !_preferences_dir.empty() && fp == nullptr, null_str);

	// SDL_UpdateApp(get_so_path(_res_path, LIBMSC_SO).c_str());

	const std::string path = get_libroseaplt_so_path(_res_path);
	fp = SDL_LoadObject(path.c_str());
	SDL_Log("{trose_library::open}SDL_LoadObject(%s), fp: %p", path.c_str(), fp);

	// as if fail, save res_path
	res_path = _res_path;
	preferences_dir = _preferences_dir;
	if (fp != nullptr) {
		aplt_load = (faplt_load)SDL_LoadFunction(fp, "aplt_load");
		aplt_unload = (faplt_unload)SDL_LoadFunction(fp, "aplt_unload");
		VALIDATE(aplt_load != nullptr && aplt_unload != nullptr, null_str);

		// read /preferences if necessary
		if (aplt.prefs.cfg().empty()) {
			instance->lua().load_aplt_prefs(aplt);
		}
		aplt_load(aplt.source, aplt.bundleid.c_str());

		create_base_slot = (faplt_create_base_slot)SDL_LoadFunction(fp, "aplt_create_base_slot");
		create_moveit_slot = (faplt_create_moveit_slot)SDL_LoadFunction(fp, "aplt_create_moveit_slot");
		create_laser_slot = (faplt_create_laser_slot)SDL_LoadFunction(fp, "aplt_create_laser_slot");
		create_dcamera_slot = (faplt_create_dcamera_slot)SDL_LoadFunction(fp, "aplt_create_dcamera_slot");
		create_iot_slot = (faplt_create_iot_slot)SDL_LoadFunction(fp, "aplt_create_iot_slot");
		create_speech_slot = (faplt_create_speech_slot)SDL_LoadFunction(fp, "aplt_create_speech_slot");
		create_ai_slot = (faplt_create_ai_slot)SDL_LoadFunction(fp, "aplt_create_ai_slot");

		create_task_api = (faplt_create_task_api)SDL_LoadFunction(fp, "aplt_create_task_api");

	} else if (aplt.lua_base_slot) {
		create_base_slot = lua_aplt_create_base_slot;

	} else if (aplt.lua_task_api) {
		create_task_api = lua_aplt_create_task_api;
	}
}

void rose_destroy_library(trose_library* lib)
{
	if (lib == nullptr) {
		return;
	}
	delete lib;
}


void tdrivers_core::init(const std::map<aplt::taplt_key, aplt::tapplet>& applets)
{
	VALIDATE(&applets == &applets_, null_str);
	libs.insert(std::make_pair(apltsotype_base, library()));
	libs.insert(std::make_pair(apltsotype_laser, library()));
	libs.insert(std::make_pair(apltsotype_moveit, library()));
	libs.insert(std::make_pair(apltsotype_dcamera, library()));
	libs.insert(std::make_pair(apltsotype_iot, library()));
	libs.insert(std::make_pair(apltsotype_speech, library()));
	libs.insert(std::make_pair(apltsotype_ai, library()));
	libs.insert(std::make_pair(apltsotype_fgaplt, library()));
	libs.insert(std::make_pair(apltsotype_base2th, library()));
	libs.insert(std::make_pair(apltsotype_aiagent_task, library()));
	VALIDATE((int)libs.size() == apltsotype_count, null_str);

	// last maybe unexpected exit.
	int clear_types[] = {apltsotype_fgaplt, apltsotype_base2th, apltsotype_aiagent_task};
	for (int at = 0; at < sizeof(clear_types) / sizeof(clear_types[0]); at ++) {
		preferences::set_driver(clear_types[at], null_str);
	}

	// Until launcher display desktop, must not pop up any dialog.
	// since refresh(...) maybe pop up error dialog, quiet(=true) it. 
	refresh(true);
}

void tdrivers_core::refresh(bool quiet)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE((int)libs.size() == apltsotype_count, null_str);

	std::unique_ptr<tdisable_refresh_lock> lock;
	lock.reset(new tdisable_refresh_lock(*this));

	std::map<aplt::taplt_key, aplt::tapplet>& applets = applets_;
	std::map<int, aplt::tapplet*> type_applets;

	std::map<std::string, aplt::tapplet*> id_applets;
	for (std::map<aplt::taplt_key, aplt::tapplet>::iterator it = applets.begin(); it != applets.end(); ++ it) {
		aplt::tapplet& applet = it->second;
		id_applets.insert(std::make_pair(applet.id, &applet));
	}
	for (int type = 0; type < apltsotype_count; type ++) {
		std::string id = preferences::driver(type);
		SDL_Log("{dbg-kdesktop}[tdrivers_core::refresh]type: %i, id: %s", type, id.c_str());
		if (id_applets.count(id) != 0) {
			aplt::tapplet& applet = *(id_applets.find(id)->second);

			std::map<int, library>::iterator hit_it = libs.find(type);
			library* lib = is_opened_in_driver_libs(applet.res_path);
			if (lib == nullptr) {
				lib = is_opened_in_deps(applet.res_path);
			}
			if (lib == nullptr) {
				trose_library* rose_lib = new trose_library(instance_, applet, applet.res_path, applet.preferences_dir);
				hit_it->second.reset(rose_lib);
			} else {
				trose_library* rose_lib = lib->get();
				if (hit_it->second.get() == nullptr || hit_it->second->res_path != rose_lib->res_path) {
					hit_it->second = *lib;
				}
			}
			SDL_Log("{dbg-kdesktop}[tdrivers_core::refresh]type: %i, id: %s, aplt.id: %s, lib: %p", type, id.c_str(), applet.id.c_str(), lib);

			check_and_correct(applet, type, quiet);

			{
				hit_it = libs.find(type);
				const library& lib = hit_it->second;
				trose_library* rose_lib = lib.get();

				if (rose_lib != nullptr) {
					type_applets.insert(std::make_pair(type, &applet));
				}
			}

		} else {
			// this driver is uninstalled
			clear_by_type(type);
		}
	}

	bool require_start_base_node = false;
	if (did_driver_refreshed_ != NULL) {
		did_driver_refreshed_(type_applets, quiet, std::ref(require_start_base_node));
	}

	if (require_start_base_node) {
		lock.reset();
		tdrivers_core::tvars vars = curvars(true);
		if (vars.base.valid(true)) {
			base_driver_.start_node(vars.base.dev, vars.base.baudrate, vars.base.model);
		}
	}
}

void tdrivers_core::check_and_correct(const aplt::tapplet& aplt, int type, bool quiet)
{
	std::map<int, library>::iterator hit_it = libs.find(type);
	const library& lib = hit_it->second;
	trose_library* rose_lib = lib.get();

	VALIDATE(rose_lib != nullptr, null_str);

	bool open_fail = false;
	const std::string ver_err = aplt::aplt_can_run(aplt);
	std::string absence;

	if (!ver_err.empty()) {

	} else if (rose_lib->opened()) {
		if (type == apltsotype_base) {
			if (rose_lib->create_base_slot == nullptr) {
				absence = "aplt_create_base_slot";
			}

		} else if (type == apltsotype_laser) {
			if (rose_lib->create_laser_slot == nullptr) {
				absence = "aplt_create_laser_slot";
			}

		} else if (type == apltsotype_moveit) {
			if (rose_lib->create_moveit_slot == nullptr) {
				absence = "aplt_create_moveit_slot";
			}

		} else if (type == apltsotype_dcamera) {
			if (rose_lib->create_dcamera_slot == nullptr) {
				absence = "aplt_create_dcamera_slot";
			}

		} else if (type == apltsotype_iot) {
			if (rose_lib->create_iot_slot == nullptr) {
				absence = "aplt_create_iot_slot";
			}

		} else if (type == apltsotype_speech) {
			if (rose_lib->create_speech_slot == nullptr) {
				absence = "aplt_create_speech_slot";
			}

		} else if (type == apltsotype_ai) {
			if (rose_lib->create_ai_slot == nullptr) {
				absence = "aplt_create_ai_slot";
			}
		}

		if (!absence.empty()) {
		}

	} else {
		open_fail = true;
	}
	if (!ver_err.empty() || open_fail || !absence.empty()) {
		utils::string_map symbols;
		symbols["drivername"] = aplt::aplt_drivers.find(type)->second.name;
		symbols["drivervalue"] = preferences::driver(type);
		std::string err;
		if (!ver_err.empty()) {
			symbols["ver_err"] = ver_err;
			err = vgettext2("Chosen '$drivervalue' as $drivername, but $ver_err", symbols);

		} else if (open_fail) {
			symbols["libroseaplt"] = LIBROSEAPLT_SO;
			err = vgettext2("Chosen '$drivervalue' as $drivername, but can not open $libroseaplt", symbols);
		} else {
			symbols["absence"] = absence;
			err = vgettext2("Chosen '$drivervalue' as $drivername, but the driver did not implement method: $absence", symbols);
		}
		clear_by_type(type);
		if (!quiet) {
			gui2::show_message(null_str, err);
		}
	}
}

void tdrivers_core::clear_by_type(int type)
{
	preferences::set_driver(type, null_str);
	std::map<int, library>::iterator find_it = libs.find(type);
	find_it->second.reset(nullptr);
}

// extern "C" void fake_aplt_serial_driver_main(bool& exit, const char* node, int baudrate)
static void fake_aplt_serial_driver_main(bool& exit, const char* node, int baudrate)
{
    VALIDATE(false, "MUST not call fake_aplt_serial_driver_main");
}

tdrivers_core::tserialp tdrivers_core::get_serial_path(int driver_type) const
{
	SDL_Log("{dbg-kdesktop}tdrivers_core::get_serial_path, driver_type: %i", driver_type);
	tserialp ret;
    const library& driver = find_by_type(driver_type);
    if (driver.get() != nullptr && driver->opened()) {
        trose_library& lib = *driver.get();
		ret.baudrate = instance_.get_serial_path(driver_type, ret.path, ret.model);
		ret.driver_main = fake_aplt_serial_driver_main;
    }
    return ret;
}

tdrivers_core::tvars tdrivers_core::curvars(bool with_node)
{
	tvars ret;
	ret.base = get_serial_path(apltsotype_base);
	ret.laser = get_serial_path(apltsotype_laser);
	ret.moveit = get_serial_path(apltsotype_moveit);

	if (with_node) {
		tty2_.refresh_ttys();
		SDL_Log("{dbg-kdesktop}[tdrivers_core::curvars(with_node: %s), ret.base.path: %s, ret.base.baudrate: %i", 
			with_node? "true": "false", ret.base.path.c_str(), ret.base.baudrate);
		if (ret.base.valid(false)) {
			const SDL_ttyUSB* base_tty = tty2_.from_path(ret.base.path);
			SDL_Log("{dbg-kdesktop}[tdrivers_core::curvars, base_tty: %p", base_tty);
			if (base_tty != nullptr) {
				ret.base.dev = base_tty->dev_node;
			}
		}

		if (ret.laser.valid(false)) {
			const SDL_ttyUSB* rplidar_tty = tty2_.from_path(ret.laser.path);
			if (rplidar_tty != nullptr) {
				ret.laser.dev = rplidar_tty->dev_node;
			}
		}

		if (ret.moveit.valid(false)) {
			const SDL_ttyUSB* moveit_tty = tty2_.from_path(ret.moveit.path);
			if (moveit_tty != nullptr) {
				ret.moveit.dev = moveit_tty->dev_node;
			}
		}
	}
	return ret;
}

void tdrivers_core::set_special_applet(int type, const aplt::tapplet* applet)
{
	VALIDATE(type == apltsotype_fgaplt || type == apltsotype_base2th || type == apltsotype_aiagent_task, null_str);

	// now maybe exist both bgaplt and curraplt aren't nullptr.
	// const bool aplt_unique = false;

	// if @applet is nullptr, clear driver.
	std::string id;
	if (applet != nullptr) {
		id = applet->id;

		if (!applet->lua_base_slot && !applet->lua_task_api) {
			const std::string libroseaplt_so_path = get_libroseaplt_so_path(applet->res_path);
			if (!SDL_IsFile(libroseaplt_so_path.c_str())) {
				// this applet doesn' exist libroseaplt.so
				id.clear();
			}
		}
	}

	preferences::set_driver(type, id);
	if (applet != nullptr) {
		refresh();

	} else {
		// see: tbase_driver::stop_node()
		// if call refresh(), it will result reenter.
		clear_by_type(type);
	}
}

trose_library* tdrivers_core::fgaplt_applet() const
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_fgaplt);
	return hit_it->second.get();
}

void* tdrivers_core::create_base_slot()
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_base);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_base_slot(os_info_.serialnumber, os_info_.cpuid);
}

void* tdrivers_core::create_moveit_slot()
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_moveit);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_moveit_slot();
}

void* tdrivers_core::create_laser_slot()
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_laser);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_laser_slot();
}

void* tdrivers_core::create_dcamera_slot()
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_dcamera);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_dcamera_slot();
}

void* tdrivers_core::create_iot_slot(aplt::tslot_subscriber& subscriber)
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_iot);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_iot_slot(&subscriber);
}

void* tdrivers_core::create_speech_slot(aplt::tslot_subscriber& subscriber)
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_speech);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_speech_slot(&subscriber);
}

void* tdrivers_core::create_ai_slot(aplt::tslot_subscriber& subscriber)
{
	std::map<int, library>::const_iterator hit_it = libs.find(apltsotype_ai);
	trose_library* lib = hit_it->second.get();
	VALIDATE(lib != nullptr, null_str);

	return lib->create_ai_slot(&subscriber);
}

aplt::ttask_api* tdrivers_core::create_task_api(aplt::tapplet& aplt)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!aplt.fake, null_str);

	if (deps.count(&aplt) != 0) {
		return deps.find(&aplt)->second.task_api;
	}
	VALIDATE(is_opened_in_deps(aplt.res_path) == nullptr, null_str);

	library* lib2 = is_opened_in_driver_libs(aplt.res_path);
	library my_lib;
	trose_library* rose_lib = nullptr;
	if (lib2 == nullptr) {
		rose_lib = new trose_library(instance_, aplt, aplt.res_path, aplt.preferences_dir);
		my_lib.reset(rose_lib);

	} else {
		rose_lib = lib2->get();
		my_lib = *lib2;
	}

	VALIDATE(rose_lib != nullptr, null_str);

	void* v_task = nullptr;
	if (rose_lib->create_task_api != nullptr) {
		v_task = rose_lib->create_task_api(&aplt);
	}

	if (v_task == nullptr) {
		return nullptr;
	}
	aplt::ttask_api* result = reinterpret_cast<aplt::ttask_api*>(v_task);
	std::pair<std::map<const aplt::tapplet*, tdep_item>::iterator, bool> ins = deps.insert(std::make_pair(&aplt, 
		tdep_item(aplt, my_lib, result)));

	return result;
}

void tdrivers_core::unload_deps()
{
	VALIDATE_IN_MAIN_THREAD();

	for (std::map<const aplt::tapplet*, tdep_item>::iterator it = deps.begin(); it != deps.end(); ++ it) {
		tdep_item& dep = it->second;

		delete dep.task_api;
		dep.task_api = nullptr;
	}

	deps.clear();
}

bool tdrivers_core::has_lib_using(const std::string& res_path) const
{
	for (std::map<int, library>::const_iterator it = libs.begin(); it != libs.end(); ++ it) {
		if (it->second.get() != nullptr && it->second->res_path == res_path) {
			return true;
		}
	}
	return false;
}

void tdrivers_core::did_uninstall(const std::string& res_path)
{
	bool dirty = true;
	while (dirty) {
		dirty = false;
		for (std::map<int, library>::iterator it = libs.begin(); it != libs.end(); ++ it) {
			if (it->second.get() != nullptr && it->second->res_path == res_path) {
				// this library will be uninstall.
				// clear_by_type(it->first);
				// install applet[1/2]. close old libroseaplt.so
				// don't call preferences::set_dirver(type, null_str)
				it->second.reset(nullptr);
				dirty = true;
				break;
			}
		}
	}
}

