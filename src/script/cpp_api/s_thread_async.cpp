/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "s_thread_async.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "log.h"
#include "filesys.h"
#include "scriptapi.h"
#include "server.h"
#include "lua_api/l_base.h"

extern int script_ErrorHandler(lua_State *L);

AsyncThread::AsyncThread()
	: SimpleThread() {
}

void* AsyncThread::Thread() {

	ThreadStarted();

	log_register_thread("AsyncLuaThread");

	DSTACK(__FUNCTION_NAME);

	BEGIN_DEBUG_EXCEPTION_HANDLER

	lua_State* L = luaL_newstate();

	luaL_openlibs(L);

	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "async_engine");

	lua_pushlightuserdata(L, m_scriptapi);
	lua_setfield(L, LUA_REGISTRYINDEX, "scriptapi");

	lua_pushlightuserdata(L, m_jobstore);
	lua_setfield(L, LUA_REGISTRYINDEX, "jobstore");

	lua_newtable(L);
	lua_setglobal(L, "minetest");

	std::string path = m_scriptapi->getServer()->getBuiltinLuaPath()
									+ DIR_DELIM + "async_thread.lua";

	//Add simplified (safe) api
	for (std::vector<ModApiBase*>::iterator i =
				m_modlist.begin();
			i != m_modlist.end(); i++) {
		//initializers are called within minetest global table!
		lua_getglobal(L, "minetest");
		int top = lua_gettop(L);
		bool ModInitializedSuccessfull = (*i)->InitializeAsync(L,top);
		assert(ModInitializedSuccessfull);
	}

	//load builtin
	verbosestream<<"Loading and running script from "<<path<<std::endl;

	lua_pushcfunction(L, script_ErrorHandler);
	int errorhandler = lua_gettop(L);

	int ret = luaL_loadfile(L, path.c_str()) || lua_pcall(L, 0, 0, errorhandler);
	if(ret){
		errorstream<<"========== ERROR FROM LUA ==========="<<std::endl;
		errorstream<<"Failed to load and run script from "<<std::endl;
		errorstream<<path<<":"<<std::endl;
		errorstream<<std::endl;
		errorstream<<lua_tostring(L, -1)<<std::endl;
		errorstream<<std::endl;
		errorstream<<"=======END OF ERROR FROM LUA ========"<<std::endl;
		lua_pop(L, 1); // Pop error message from stack
		lua_pop(L, 1); // Pop the error handler from stack
	}
	lua_pop(L, 1); // Pop the error handler from stack
	lua_close(L);
	return 0;
}
