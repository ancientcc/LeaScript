/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef IOT_DRIVER_HPP_INCLUDED
#define IOT_DRIVER_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"

#include "rtc_base/signalthread.h"
#include "rtc_base/bind.h"

class tworker;

namespace rtc {

class worker_thread: public SignalThread
{
public:
	worker_thread(tworker& worker)
		: worker_(worker)
	{}

protected:
	void DoWork() override;
	void OnWorkStart() override;
	void OnWorkDone() override;

protected:
	tworker& worker_;
};

}

class tworker
{
public:
	virtual void DoWork() = 0;
	virtual void OnWorkStart() = 0;
	virtual void OnWorkDone() = 0;

protected:
	tworker()
		: main_(rtc::Thread::Current())
		, thread_(new rtc::worker_thread(*this))
	{}
	virtual ~tworker()
	{
		thread_->Destroy(true);
		thread_ = NULL;
	}

protected:
	rtc::Thread* main_;
	rtc::worker_thread* thread_;
};

class tiot_driver
{
public:
	tiot_driver(aplt::tslot_subscriber& subscriber);

	~tiot_driver()
	{
		VALIDATE(thread_.get() == nullptr, null_str);
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tiot_driver);

	void set_slot(const std::string& _aplt_id, aplt::tiot_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }

	bool started() const { return thread_.get() != nullptr; }

	void start_iot();
	void stop_iot();

	void slice();
	
public:
	aplt::tiot_slot* slot;

private:
	std::unique_ptr<net::tworker> thread_;
	std::string aplt_id_;
};


#endif

