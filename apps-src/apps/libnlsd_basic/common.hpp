#ifndef LIBLEAGOR_COMMON_HPP
#define LIBLEAGOR_COMMON_HPP


#include "rose_util.hpp"
#include "aplt_clazz.hpp"

namespace aplt {

class tbase_ext_lamp;
class tlua_block;

namespace nlsd {
extern tbase_ext_lamp* lamp;
}

enum {block_turn_off, block_count};
extern tlua_block* lua_block;

enum {cpp_id_driver_layer = cpp_id_aplt_min};

}

#endif