

#define LUA_LIB

#include "rose_lua.hpp"
#include "rose_var.hpp"
#include "rose_string_utils_dll.hpp"

const char tstringKey[] = "translatable string";

void* operator new(size_t sz, lua_State *L)
{
	return lua_newuserdata(L, sz);
}

void operator delete(void*, lua_State *L)
{
	// Not sure if this is needed since it's a no-op
	// It's only called if a constructor throws while using the above operator new
	// By removing the userdata from the stack, this should ensure that Lua frees it
	lua_pop(L, 1);
}

bool luaW_getmetafield(lua_State *L, int idx, const char* key)
{
	if (key == nullptr) {
		return false;
	}
	int n = strlen(key);
	if (n == 0) {
		return false;
	}
	if (n >= 2 && key[0] == '_' && key[1] == '_') {
		return false;
	}
	return luaL_getmetafield(L, idx, key) != 0;
}

bool luaL_checkboolean(lua_State *L, int arg)
{
	luaL_checktype(L, arg, LUA_TBOOLEAN);
	return lua_toboolean(L, arg) != 0;
}

void nil_metatable(lua_State* L, const std::string& tname)
{
	tstack_size_lock lock(L, 0);

	bool existed = luaL_getmetatable(L, tname.c_str()) != LUA_TNIL;
	lua_pop(L, 1); // pop nil or table

	if (existed) {
		lua_pushnil(L);  /* create metatable */
		lua_setfield(L, LUA_REGISTRYINDEX, tname.c_str());  /* registry.name = metatable */
	}
}

void luaW_pushtstring(lua_State *L, const t_string& v)
{
	new(L) t_string(v);
	luaL_setmetatable(L, tstringKey);
}

void luaW_pushscalar(lua_State *L, const config::attribute_value& v)
{
	int type = v.type();
	if (type == var_type_bool) {
		lua_pushboolean(L, v.to_bool());
	} else if (type == var_type_integer) {
		lua_pushinteger(L, v.to_int64());
	} else if (type == var_type_double) {
		lua_pushnumber(L, v.to_double());
	} else if (type == var_type_string) {
		lua_pushstring(L, v.str().c_str());
	} else if (type == var_type_tstring) {
		luaW_pushtstring(L, v.t_str());
	} else {
		VALIDATE(type == var_type_nposm, null_str);
		lua_pushnil(L);
	}
}

bool luaW_toscalar(lua_State *L, int index, bool allow_TUSERDATA, config::attribute_value& v)
{
	switch (lua_type(L, index)) {
		case LUA_TBOOLEAN:
			v.from_bool(luaW_toboolean(L, -1));
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(L, -1)) {
				// Must not 'v = use lua_tointeger(L, -1)'. it will save use double, not int64_t.
				// change double to int64_t, may modify value. 
				v.from_int64(lua_tointeger(L, -1));
			} else {
				v.from_double(lua_tonumber(L, -1));
			}
			break;
		case LUA_TSTRING:
			v.from_string(lua_tostring(L, -1), true);
			break;
		case LUA_TUSERDATA:
		{
			if (!allow_TUSERDATA) {
				return false;
			}
			if (t_string * tptr = static_cast<t_string *>(luaL_testudata(L, -1, tstringKey))) {
				v = *tptr;
				break;
			} else {
				return false;
			}
		}
		default:
			return false;
	}
	return true;
}

void luaW_pushtask_vars(lua_State* L, const aplt::ttask_vars& task_vars)
{
	tstack_size_lock lock(L, 1);

	lua_newtable(L);

	const std::map<std::string, aplt::ttask_var>& data = task_vars.data();
	for (std::map<std::string, aplt::ttask_var>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		const aplt::ttask_var& var = it->second;
		VALIDATE(!var.is_array, null_str);

		luaW_pushscalar(L, var.val);
		lua_setfield(L, -2, var.name.c_str());
	}
}

void nil_applet_cpp_tables(lua_State* L, const std::string& lua_bundleid)
{
	tstack_size_lock lock(L, 0);

	// delete applets's cpp table
	std::vector<std::string> cpp_short_tbls;
	cpp_short_tbls.push_back(APLT_LUA_SHORT_CPP_KEY);
	cpp_short_tbls.push_back(APLT_LUA_SHORT_CPPK_KEY);
	for (std::vector<std::string>::const_iterator it = cpp_short_tbls.begin(); it != cpp_short_tbls.end(); ++ it) {
		const std::string& short_name = *it;
		const std::string tbl_name = utils::join_app_prefix_id(lua_bundleid, short_name);

		lua_getglobal(L, tbl_name.c_str());
		bool isnil = lua_isnil(L, -1);
		if (!isnil) {
			// if isn't nil, it must be table.
			VALIDATE(lua_istable(L, -1), "*.lua must define table");
		}
		// this nil or table is pushed
		lua_pop(L, 1); // pop nil or table.

		if (!isnil) {
			lua_pushnil(L);
			lua_setglobal(L, tbl_name.c_str());
		}
	}
}