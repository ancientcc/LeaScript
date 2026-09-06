#ifndef LIBLEAGOR_COMMON_HPP
#define LIBLEAGOR_COMMON_HPP


#include "rose_util.hpp"
#include "aplt_clazz.hpp"

namespace aplt {

class tlua_block;
class tlua_camera;
// class tnonblock2;

enum {block_query, block_query_cooking, block_parse_time, block_kbook, block_count};
extern tlua_block* lua_block;

enum {camera_snapshot, camera_kface, camera_kpose, camera_workout, camera_recognition, camera_count};
extern tlua_camera* lua_camera;

enum {cpp_id_save_sf_6fields = cpp_id_aplt_min, cpp_id_save_ss_4fields, cpp_id_save_sshare_3fields};
}

#endif