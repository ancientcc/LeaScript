#ifndef COMPAT_H
#define COMPAT_H

#include "lua53/source/src/lua.h"
#include "lua53/source/src/lauxlib.h"

#if LUA_VERSION_NUM<501
void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup);
#endif

#endif
