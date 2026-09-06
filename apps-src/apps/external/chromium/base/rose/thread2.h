/* $Id: thread.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef THREAD2_H_INCLUDED
#define THREAD2_H_INCLUDED

#include <SDL.h>
#include <SDL_thread.h>

#include <list>
#include <functional>
#include <string>
#include <memory>
#include "base/threading/thread.h"
#include "rose_thread.hpp"

// namespace base {
// class Thread;
// }

namespace net {
class tworker: public trose_thread
{
public:
	tworker(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart = NULL, const std::function<void ()>& DoWorkDone = NULL, 
		const std::function<void ()>& OnTriggerExit = NULL, const std::string& name = "");
	virtual ~tworker();

	base::Thread* thread_ptr() { return thread_.get(); }
	bool& ref_exit() { return exit_; }

private:
	void start(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart, const std::function<void ()>& OnWorkDone, const std::function<void ()>& OnTriggerExit, const std::string& name);
	void stop();

	void did_set_event();
	void start_internal();
	void stop_internal();

private:
	// DoWork maybe temperal variable, but tworker require it persist.
	std::function<void (bool& exit) > DoWork_;
	std::function<void ()> OnWorkDone_;
	std::function<void ()> OnTriggerExit_; 

	std::unique_ptr<base::Thread> thread_;
	base::WaitableEvent e_;
	bool exit_;
};

void rose_destroy_worker(net::tworker* worker);

// tworker hasn't refcount member, use std::shared_ptr.
struct tshared_worker: public std::shared_ptr<net::tworker>
{
	tshared_worker()
		: std::shared_ptr<net::tworker>()
	{}

	tshared_worker(net::tworker* worker)
		: std::shared_ptr<net::tworker>(worker, rose_destroy_worker)
	{}

	void reset(net::tworker* worker) { std::shared_ptr<net::tworker>::reset(worker, rose_destroy_worker); }
};

}

#endif
