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


#include <vector>
#include "thread2.h"

namespace net {
tworker::tworker(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart, const std::function<void ()>& DoWorkDone, 
		const std::function<void ()>& OnTriggerExit, const std::string& name)
	: e_(base::WaitableEvent::ResetPolicy::AUTOMATIC, base::WaitableEvent::InitialState::NOT_SIGNALED)
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
	e_.Signal();
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
	thread_->SetRoseRun(DoWork_, e_, exit_);

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));
/*
	if (DoWork_ != NULL) {
		// Sync mode. The worker thread will be blocked DoWork_() until exit_= true.
		// Once DoWork_() exits, the worker is end.
		thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&tworker::start_internal, base::Unretained(this)));
		e_.Wait();
	} else {
		// Asynchronous mode. Tasks are sent by other threads to worker thread.
	}
*/
	e_.Wait();
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

void rose_destroy_worker(net::tworker* worker)
{
	if (worker == nullptr) {
		return;
	}
	delete worker;
}

}
