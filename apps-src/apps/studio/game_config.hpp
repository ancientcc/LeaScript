#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

#include "rose_config.hpp"
#include "preferences.hpp"

//basic game configuration information is here.
namespace game_config
{
extern std::string absolute_path;
extern std::string apps_src_path;
}

namespace preferences {

std::string window_cfg_path();
void set_window_cfg_path(const std::string& value);

}

#endif
