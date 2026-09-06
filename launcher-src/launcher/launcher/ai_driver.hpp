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

#ifndef LAUNCHER_AI_DRIVER_HPP
#define LAUNCHER_AI_DRIVER_HPP

#include "base_slot.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"
#include "drivers.hpp"

class tai_driver
{
public:
	tai_driver(tdrivers& drivers, aplt::tbg_task& bg_task);

	~tai_driver()
	{
		if (is_aiagent_tasking()) {
			stop_aiagent_task();
		}
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tai_driver);

	void set_slot(const std::string& _aplt_id, aplt::tai_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }
	bool installed() const { return slot != nullptr; }

	void start_aiagent_task(aplt::ttask_api* caller_task_api, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& question, const surface& surf);
	void stop_aiagent_task();
	bool is_aiagent_tasking() const
	{
		return aiagent_api_ != nullptr;
	}

	const aplt::ttask_pair& aiagent_task_pair() const { return aiagent_task_pair_; }
	bool aiagent_task_is_terminating() const { return aiagent_api_ != nullptr && aiagent_api_->terminating(); }

	// void restart();
	void slice();

	void send_nlp_question(int chatsrc, bool new_conversation, const std::string& question, const surface& surf);
	bool is_nlp_questioning() const;
	void stop_nlp_question();

private:
	void did_start_aiagent_task_quited(const aplt::ttask_api* caller_task_api, std::string& err_msg);

public:
	aplt::tai_slot* slot;

private:
	tdrivers& drivers_;
	aplt::tbg_task& bg_task_;
	// std::unique_ptr<net::tworker> thread_;
	std::string aplt_id_;

	aplt::ttask_vars task_vars_;
	aplt::ttask_api* task_api_;
	aplt::taiagent_api* aiagent_api_;
	aplt::ttask_pair aiagent_task_pair_;
	aplt::ttask_api* aiagent_caller_task_api_;
	bool aiagent_task_finished_;
};


#endif

