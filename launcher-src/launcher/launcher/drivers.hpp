#ifndef GAME_DRIVERS_HPP
#define GAME_DRIVERS_HPP

#include "drivers_core.hpp"

// class tbase_driver;

class tdrivers: public tdrivers_core
{
public:
	tdrivers(base_instance& instance, std::map<aplt::taplt_key, aplt::tapplet>& applets, tbase_driver_core& base_driver, const SDL_OsInfo& os_info, const std::function<void (const std::map<int, aplt::tapplet*>& id_applets, bool quiet, bool& require_start_base_node)>& did_driver_refreshed)
		: tdrivers_core(instance, applets, base_driver, os_info, did_driver_refreshed)
	{}
	~tdrivers()
	{
	}
};

#endif

