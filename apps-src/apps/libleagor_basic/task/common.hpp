#ifndef LIBLEAGOR_COMMON_HPP
#define LIBLEAGOR_COMMON_HPP


#include "rose_util.hpp"
#include "aplt_clazz.hpp"

namespace aplt {

class tlua_nonblock;

enum {nonblock_deepseek, nonblock_count};
extern tlua_nonblock* lua_nonblock;

class tlua_aiagent;

enum {aiagent_add_timed_reminder, aiagent_count };
extern tlua_aiagent* lua_aiagent;

enum {cpp_id_save_sf_6fields = cpp_id_aplt_min, cpp_id_save_ss_4fields, cpp_id_save_sshare_3fields};
}

#endif