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

#include "common/c_jobstore.h"
#include "jthread/jmutexautolock.h"

JobStore::JobStore() {
	m_result_lock.Init();
	m_job_lock.Init();
}

void JobStore::queueJob(AsyncJob job) {
	JMutexAutoLock(this->m_job_lock);
	m_jobs.push_back(job);
}

AsyncJob JobStore::fetchJob() {
	JMutexAutoLock(this->m_job_lock);
	AsyncJob retval = m_jobs.back();
	m_jobs.pop_back();
	return retval;
}

void JobStore::queueJobResult(AsyncResult result) {
	JMutexAutoLock(this->m_result_lock);
	m_results.push_back(result);
}
AsyncResult JobStore::popJobResult() {
	JMutexAutoLock(this->m_result_lock);
	AsyncResult retval = m_results.back();
	m_results.pop_back();
	return retval;
}

bool JobStore::haveJob() {
	JMutexAutoLock(this->m_job_lock);
	return (m_jobs.size() > 0);
}

bool JobStore::haveResult() {
	JMutexAutoLock(this->m_result_lock);
	return (m_results.size() > 0);
}




