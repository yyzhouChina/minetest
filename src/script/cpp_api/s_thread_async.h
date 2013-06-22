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

#ifndef S_THREAD_ASYNC_H_
#define S_THREAD_ASYNC_H_

class ScriptApi;

#include "util/thread.h"

struct AsyncJob {
	std::string tocall;
	std::string parameters;
	unsigned int id;
};

struct AsyncResult {
	std::string retval;
	unsigned int id;
};

class AsyncThread : public SimpleThread {
public:
	AsyncThread();
	void* Thread();
	ScriptApi* m_scriptapi;

	void queueJob(AsyncJob job);
	AsyncJob fetchJob();
	bool haveJob();

	void queueJobResult(AsyncResult);
	AsyncResult popJobResult();
	bool haveResult();

private:
	JMutex m_job_lock;
	JMutex m_result_lock;
	std::vector<AsyncJob> m_jobs;
	std::vector<AsyncResult> m_results;
};

#define GET_ASYNC_THREAD \
		lua_getfield(L, LUA_REGISTRYINDEX, "async_engine"); \
		AsyncThread* thread = (AsyncThread*) lua_touserdata(L, -1); \
		lua_pop(L, 1);

#define MAX_RESULTS_PER_STEP 5

#endif /* S_THREAD_ASYNC_H_ */
