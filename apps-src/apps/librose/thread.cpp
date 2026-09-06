/* $Id: thread.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "rose_global.hpp"

#include <vector>

#include "thread.hpp"
#include "base_instance.hpp"

/*
namespace net {
tworker::tworker(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart, const std::function<void ()>& DoWorkDone, 
		const std::function<void ()>& OnTriggerExit, const std::string& name)
	: e_(false, false)
	, exit_(false)
{
	start(DoWork, OnWorkStart, DoWorkDone, OnTriggerExit, name);
}

tworker::~tworker()
{
	stop();
}

void tworker::did_set_event()
{
	e_.Set();
}

void tworker::start_internal()
{
	// signal tworker::start
	did_set_event();

	DoWork_(exit_);

	// signal tworker::stop
	// did_set_event();
}

void tworker::start(const std::function<void (bool& exit) >& DoWork, const std::function<void ()>& OnWorkStart, const std::function<void ()>& OnWorkDone, const std::function<void ()>& OnTriggerExit, const std::string& name)
{
	CHECK(thread_.get() == nullptr);
	CHECK(!exit_);

	if (OnWorkStart != NULL) {
		OnWorkStart();
	}
	DoWork_ = DoWork;
	OnWorkDone_ = OnWorkDone;
	OnTriggerExit_ = OnTriggerExit;

	thread_.reset(new base::Thread(name.empty()? "WorkerThread": name));

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&tworker::start_internal, base::Unretained(this)));
	e_.Wait(rtc::Event::kForever);
}

void tworker::stop()
{
	if (thread_.get() == nullptr) {
		// VALIDATE(delegate_.get() == nullptr, null_str);
		return;
	}

	exit_ = true;
	// 
	if (OnTriggerExit_ != NULL) {
		OnTriggerExit_();
	}

	thread_.reset();
	exit_ = false;
	if (OnWorkDone_ != NULL) {
		OnWorkDone_();
	}
}
}

namespace threading {

mutex::mutex() : m_(SDL_CreateMutex())
{}

mutex::~mutex()
{
	SDL_DestroyMutex(m_);
}

lock::lock(mutex& m) : m_(m)
{
	SDL_mutexP(m_.m_);
}

lock::~lock()
{
	SDL_mutexV(m_.m_);
}

condition::condition() : cond_(SDL_CreateCond())
{}

condition::~condition()
{
	SDL_DestroyCond(cond_);
}

bool condition::wait(const mutex& m)
{
	return SDL_CondWait(cond_,m.m_) == 0;
}

condition::WAIT_TIMEOUT_RESULT condition::wait_timeout(const mutex& m, unsigned int timeout)
{
	const int res = SDL_CondWaitTimeout(cond_,m.m_,timeout);
	switch(res) {
		case 0: return WAIT_OK;
		case SDL_MUTEX_TIMEDOUT: return WAIT_TIMED_OUT;
		default:
			// SDL_CondWaitTimeout: $SDL_GetError()
			return WAIT_ERROR;
	}
}

bool condition::notify_one()
{
	if(SDL_CondSignal(cond_) < 0) {
		// SDL_CondSignal: $SDL_GetError()
		return false;
	}

	return true;
}

bool condition::notify_all()
{
	if(SDL_CondBroadcast(cond_) < 0) {
		// SDL_CondBroadcast: $SDL_GetError()
		return false;
	}
	return true;
}

semaphore::semaphore(Uint32 initial_value) : sem_(SDL_CreateSemaphore(initial_value))
{}

semaphore::~semaphore()
{
	SDL_DestroySemaphore(sem_);
}

int semaphore::wait(Uint32 timeout)
{
	return SDL_SemWaitTimeout(sem_, timeout);
}

void semaphore::post()
{
	SDL_SemPost(sem_);
}

}
*/

/*
std::unique_ptr<twebrtc_send_helper::tlock> twebrtc_send_helper::get_sender_lock()
{
	VALIDATE_NOT_MAIN_THREAD();

	if (deconstructed_) {
		return NULL;
	}
	std::unique_ptr<tlock> ret(new tlock(*this));
	if (deconstructed_) {
		// maybe deconstructed_ = true when execute below statement.
		return NULL;
	}
	return ret;
}
*/
void twebrtc_send_helper::clear_msg()
{
	VALIDATE_IN_MAIN_THREAD();
	// allow repeated call
	// DCHECK(!deconstructed_);

	deconstructed_ = true;
	while (senders_ > 0) {
		// 2. wait all invoker no invoke.
		instance->sdl_thread().clear_msg(phandler_);
		SDL_Delay(20);
	}
}
