#ifndef LIBROSE_DRIVERS_CORE_HPP
#define LIBROSE_DRIVERS_CORE_HPP

#include "aplt.hpp"
#include "aplt_common.hpp"
#include <memory>
#include "speech_slot.hpp"

class base_instance;
class tbase_driver_core;

struct tty2
{
	void refresh_ttys();
	const SDL_ttyUSB* from_path(const std::string& path) const;

	bool valid_tty(const SDL_ttyUSB& tty) const { return tty.dev_node[0] != '\0' && tty.path[0] != '\0'; }
	std::string to_string(const SDL_ttyUSB& tty) const;

	std::map<std::string, SDL_ttyUSB> ttys;
};

struct trose_library
{
	trose_library(base_instance& instance, aplt::tapplet& _aplt, const std::string& _res_path, const std::string& _preferences_dir);
	~trose_library();

	void open(const std::string& _res_path, const std::string& _preferences_dir);
	
	void* load_function(const std::string& name)
	{
		VALIDATE(!name.empty() && fp != nullptr, null_str);
		return SDL_LoadFunction(fp, name.c_str());
	}

	void close()
	{
		if (fp != nullptr || aplt.lua_base_slot || aplt.lua_task_api) {
			if (aplt::task_api_map.count(aplt.id) != 0) {
				std::map<std::string, std::set<const aplt::ttask_api*> >& task_api_map = aplt::task_api_map;
				VALIDATE(false, null_str);
			}
		}
		if (fp != nullptr) {
			aplt_unload();

			SDL_UnloadObject(fp);
			fp = nullptr;
		}
		res_path.clear();
		reset_function_ptrs();
	}

	// bool opened() const { return fp != nullptr; }
	bool opened() const { return fp != nullptr || aplt.lua_base_slot || aplt.lua_task_api; }

	// int get_serial_path(int driver_type, char* path, int maxlen);

	void reset_function_ptrs()
	{
		// 3-driver applet
		aplt_load = nullptr;
		aplt_unload = nullptr;
		// get_serial_path = nullptr;

		create_base_slot = nullptr;
		create_moveit_slot = nullptr;
		create_laser_slot = nullptr;
		create_dcamera_slot = nullptr;
		create_iot_slot = nullptr;
		create_speech_slot = nullptr;
		create_ai_slot = nullptr;

		create_task_api = nullptr;
	}


	aplt::tapplet& aplt;
	std::string res_path;
	std::string preferences_dir;
	void* fp;

	faplt_load aplt_load;
	faplt_unload aplt_unload;

	// base driver
	faplt_create_base_slot create_base_slot;

	// moveit driver
	faplt_create_moveit_slot create_moveit_slot;

	// laser driver
	faplt_create_laser_slot create_laser_slot;

	// dcamera driver
	faplt_create_dcamera_slot create_dcamera_slot;

	// iot driver
	faplt_create_iot_slot create_iot_slot;

	// speech driver
	faplt_create_speech_slot create_speech_slot;

	// aiagent driver
	faplt_create_ai_slot create_ai_slot;

	// task api
	faplt_create_task_api create_task_api;

private:
	base_instance& instance_;
};

void rose_destroy_library(trose_library* lib);

// trose_library hasn't refcount member, use std::shared_ptr.
struct library: public std::shared_ptr<trose_library>
{
	library()
		: std::shared_ptr<trose_library>()
	{}

	library(trose_library* lib)
		: std::shared_ptr<trose_library>(lib, rose_destroy_library)
	{}

	void reset(trose_library* lib) { std::shared_ptr<trose_library>::reset(lib, rose_destroy_library); }
};

class tdrivers_core
{
public:
	struct tserialp {
		tserialp()
			: driver_main(nullptr)
			, baudrate(nposm)
		{}

		tserialp(faplt_serial_driver_main _driver_main, const std::string& _path, const std::string& _model, int _baudrate)
			: driver_main(_driver_main)
			, path(_path)
			, model(_model)
			, baudrate(_baudrate)
		{}

		bool valid(bool with_dev) const
		{
			if (driver_main == nullptr || path.empty() || baudrate == nposm) {
				return false;
			}
			return !with_dev || !dev.empty();
		}

		std::string to_str() const
		{
			if (path.empty()) {
				return null_str;
			}
			char buf[260];
			SDL_snprintf(buf, sizeof(buf), "%s(%i)", path.c_str(), baudrate);
			return buf;
		}

		faplt_serial_driver_main driver_main;
		std::string path;
		std::string model;
		int baudrate;
		std::string dev; // path + tty2_ => node
	};

	struct tvars {
		tserialp base;
		tserialp laser;
		tserialp moveit;
	};

	tdrivers_core(base_instance& instance, std::map<aplt::taplt_key, aplt::tapplet>& applets, tbase_driver_core& base_driver, const SDL_OsInfo& os_info, const std::function<void (const std::map<int, aplt::tapplet*>& id_applets, bool quiet, bool& require_start_base_node)>& did_driver_refreshed)
		: instance_(instance)
		, applets_(applets)
		, base_driver_(base_driver)
		, os_info_(os_info)
		, did_driver_refreshed_(did_driver_refreshed)
		, disable_refresh_(false)
	{}
	virtual ~tdrivers_core()
	{
		VALIDATE(deps.empty(), null_str);
	}
	posix_noncopyable(tdrivers_core);

	void init(const std::map<aplt::taplt_key, aplt::tapplet>& applets);

	library* is_opened_in_driver_libs(const std::string& res_path)
	{
		for (std::map<int, library>::iterator it = libs.begin(); it != libs.end(); ++ it) {
			if (it->second.get() != nullptr && it->second->res_path == res_path) {
				return &it->second;
			}
		}
		return nullptr;
	}

	library* is_opened_in_deps(const std::string& res_path)
	{
		for (std::map<const aplt::tapplet*, tdep_item>::iterator it = deps.begin(); it != deps.end(); ++ it) {
			tdep_item& item = it->second;
			if (item.lib2.get() != nullptr && item.lib2->res_path == res_path) {
				return &item.lib2;
			}
		}
		return nullptr;
	}

	bool has_lib_using(const std::string& res_path) const;
	void did_uninstall(const std::string& res_path);

	void refresh(bool quiet = false);
	void check_and_correct(const aplt::tapplet& aplt, int type, bool quiet);
	void clear_by_type(int type);

	const library& find_by_type(int type) const
	{
		VALIDATE(type >= 0 && type < apltsotype_count, null_str);
		return libs.find(type)->second;
	}
	tvars curvars(bool with_node);

	void set_special_applet(int type, const aplt::tapplet* applet);
	trose_library* fgaplt_applet() const;
	void* create_base_slot();
	void* create_moveit_slot();
	void* create_laser_slot();
	void* create_dcamera_slot();
	void* create_iot_slot(aplt::tslot_subscriber& subscriber);
	void* create_speech_slot(aplt::tslot_subscriber& subscriber);
	void* create_ai_slot(aplt::tslot_subscriber& subscriber);

	aplt::ttask_api* create_task_api(aplt::tapplet& aplt);
	void unload_deps();

	tty2& get_tty2() { return tty2_; }

private:
	tserialp get_serial_path(int driver_type) const;

public:
	std::map<int, library> libs;

	struct tdep_item {
		tdep_item(const aplt::tapplet& _aplt, library& _lib, aplt::ttask_api* _task_api)
			: aplt(_aplt)
			, lib2(_lib)
			, task_api(_task_api)
		{
			VALIDATE(lib2.get() != nullptr, null_str);
			VALIDATE(task_api != nullptr, null_str);
		}

		~tdep_item()
		{
			VALIDATE(lib2.get() != nullptr, null_str);

			// VALIDATE(task_api != nullptr, null_str);
			// delete task_api;
			// why must not delete task_api in ~tdep_item?
			//  -- ins = deps.insert(std::make_pair(&aplt, tdep_item(aplt, my_lib, result)));
			//     below will occur ~tdep_item(), but shouldn't delete task_api during it.
			//     to here, task_api is non-nullptr(below) or non-nullptr(see tdrivers::unload_deps()).
			
		}

		const aplt::tapplet& aplt;
		library lib2;
		aplt::ttask_api* task_api;
	};
	std::map<const aplt::tapplet*, tdep_item> deps;

private:
	base_instance& instance_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tbase_driver_core& base_driver_;
	const SDL_OsInfo& os_info_;
	std::function<void (const std::map<int, aplt::tapplet*>& type_applets, bool quiet, bool& require_start_base_node)> did_driver_refreshed_;
	tty2 tty2_;

	class tdisable_refresh_lock
	{
	public:
		tdisable_refresh_lock(tdrivers_core& drivers)
			: drivers_(drivers)
		{
			VALIDATE(!drivers_.disable_refresh_, null_str);
			drivers_.disable_refresh_ = true;
		}
		~tdisable_refresh_lock()
		{
			VALIDATE(drivers_.disable_refresh_, null_str);
			drivers_.disable_refresh_ = false;
		}

	private:
		tdrivers_core& drivers_;
	};
	bool disable_refresh_;
};

class tdeps_0lock
{
public:
	tdeps_0lock(tdrivers_core& drivers)
		: drivers_(drivers)
	{
		VALIDATE(drivers.deps.empty(), null_str);
	}

	~tdeps_0lock()
	{
		drivers_.unload_deps();
		VALIDATE(drivers_.deps.empty(), null_str);
	}

private:
	tdrivers_core& drivers_;
};

#endif

