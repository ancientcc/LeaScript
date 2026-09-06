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

#ifndef MOVEIT_DRIVER_HPP_INCLUDED
#define MOVEIT_DRIVER_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"
#include "aplt2.hpp"

#include <urdf_model/pose.h>

class tros_instance;

namespace aplt {
class tjoint_model
{
public:
	tjoint_model(const std::string& name, const int type, double min_position, double max_position)
		: name_(name)
		, type_(type)
		, min_position_(min_position)
		, max_position_(max_position)
		, variable_index_(nposm)
	{}

public:
	const std::string name_;
	const int type_;
	double min_position_;
	double max_position_;
	int variable_index_;

	urdf::Vector3 axis_;
	urdf::Pose origin_;
};

class tjoint_group_model
{
public:
	tjoint_group_model(const std::string& name)
		: name_(name)
	{}

public:
	const std::string name_;

	std::vector<tjoint_model*> joint_model_vector_;
	std::vector<std::string> joint_model_name_vector_;

	std::vector<std::string> variable_names_;
	// variable_index_list_ is index in trobot_model::variable_names_, not trobot_model::joint_model_vector_.
	std::vector<int> variable_index_list_;
	std::set<std::string> variable_name_set_;

};

class trobot_model
{
public:
	~trobot_model() { clear(); }

	bool valid() const { return !joint_model_vector_.empty() && !joint_model_group_map_.empty(); }

	void clear()
	{
		for (std::map<std::string, tjoint_group_model*>::const_iterator it = joint_model_group_map_.begin(); it != joint_model_group_map_.end(); ++ it) {
			delete it->second;
		}
		for (std::vector<tjoint_model*>::const_iterator it = joint_model_vector_.begin(); it != joint_model_vector_.end(); ++ it) {
			tjoint_model* joint_model = *it;
			delete joint_model;
		}

		// for (LinkModel* link_model : link_model_vector_) {
		//	delete link_model;
		// }

		joint_model_vector_.clear();
		joint_model_group_map_.clear();

		variable_names_.clear();
		joint_variables_index_map_.clear();

		joint_model_map_.clear();
		joint_model_groups_.clear();
	}


public:
	// why use pointer? --if use object, modify joint_model_vector_ will result re-allocate memory.
	std::vector<tjoint_model*> joint_model_vector_;
	std::map<std::string, tjoint_group_model*> joint_model_group_map_;

	std::vector<std::string> variable_names_;
	std::map<std::string, int> joint_variables_index_map_;

	// auxiliary vairiables
	std::map<std::string, tjoint_model*> joint_model_map_;
	std::vector<tjoint_group_model*> joint_model_groups_;
};

class tvariable_positions
{
public:
	tvariable_positions(double* positions = nullptr, int count = 0)
		: positions(positions)
		, count(count)
	{}

	void set_position(int at, double val);

	bool valid() const { return positions != nullptr && count > 0; }

	std::string to_string() const;

public:
	double* positions;
	int count;
};
/*
class taction_state
{
public:
	taction_state(const std::string& name)
		: name(name)
	{}

	const std::string name;
	std::vector<tgroup_state> states;
};
*/
}

class tmoveit_driver
{
public:
	tmoveit_driver(tros_instance& ros_instance, const std::map<aplt::taplt_key, aplt::tapplet>& applets);

	~tmoveit_driver()
	{
		VALIDATE(!shared_thread_, null_str);
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tmoveit_driver);

	void set_slot(const std::string& _aplt_id, aplt::tmoveit_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }
	bool installed() const { return slot != nullptr; }

	const std::vector<aplt::tgroup_state>& get_common_state(int state);

	void slice();
	bool started() const { return thread_.get() != nullptr; }

	void start_moveit(const std::string& serial_path, int baudrate);
	// void set_thread(net::tshared_worker& thread);
	void set_thread();
	void stop_moveit();

	net::tshared_worker& get_thread() { return thread_; }

public:
	aplt::tmoveit_slot* slot;

private:
	tros_instance& ros_instance_;
	const std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	net::tshared_worker thread_;
	std::string aplt_id_;

	bool shared_thread_;
};


#endif

