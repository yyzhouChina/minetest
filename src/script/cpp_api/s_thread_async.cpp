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
#include "jthread/jmutexautolock.h"

extern int script_ErrorHandler(lua_State *L);

AsyncThread::AsyncThread()
	: SimpleThread() {
	m_result_lock.Init();
	m_job_lock.Init();
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

	lua_newtable(L);
	lua_setglobal(L, "minetest");

	std::string path = m_scriptapi->getServer()->getBuiltinLuaPath()
									+ DIR_DELIM + "async_thread.lua";

	//Add simplified (safe) api
	for (std::vector<ModApiBase*>::iterator i =
				m_scriptapi->m_mod_api_modules->begin();
			i != m_scriptapi->m_mod_api_modules->end(); i++) {
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

void AsyncThread::queueJob(AsyncJob job) {
	JMutexAutoLock(this->m_job_lock);
	m_jobs.push_back(job);
}

AsyncJob AsyncThread::fetchJob() {
	JMutexAutoLock(this->m_job_lock);
	AsyncJob retval = m_jobs.back();
	m_jobs.pop_back();
	return retval;
}

void AsyncThread::queueJobResult(AsyncResult result) {
	JMutexAutoLock(this->m_result_lock);
	m_results.push_back(result);
}
AsyncResult AsyncThread::popJobResult() {
	JMutexAutoLock(this->m_result_lock);
	AsyncResult retval = m_results.back();
	m_results.pop_back();
	return retval;
}

bool AsyncThread::haveJob() {
	JMutexAutoLock(this->m_job_lock);
	return (m_jobs.size() > 0);
}

bool AsyncThread::haveResult() {
	JMutexAutoLock(this->m_result_lock);
	return (m_results.size() > 0);
}
