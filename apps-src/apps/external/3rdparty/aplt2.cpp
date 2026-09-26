/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "aplt2.hpp"

#include "rose_string_utils_dll.hpp"
#include "rose_config_3rdparty.hpp"
#include "gettext.hpp"
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"

#include <boost/foreach.hpp>

namespace aplt {

const bool per_conflicted_always = true;

const std::map<int, std::string> sources {
	{src_distribution, "distribution"},
	{src_development, "development"},
	{src_studio, "studio"},
};

const std::map<task_type_t, std::string> task_types {
	{task_moveit, "moveit"},
	{task_ble, "ble"},
	{task_camera, "camera"},
	{task_block, "block"},
	{task_nonblock, "nonblock"},
	{task_aiagent, "aiagent"},
	{task_cpp, "cpp"},
};

bool is_single_task(int type)
{
	return type == aplt::task_ble || type == aplt::task_moveit || type == aplt::task_camera ||
		type == aplt::task_block || type == aplt::task_nonblock;
}

std::string src_bundleid_2_id(int source, const std::string& bundleid)
{
	VALIDATE(sources.count(source) != 0, null_str);
	// VALIDATE(is_bundleid(bundleid) != 0, null_str);
/*
	std::stringstream ss;
	ss << bundleid;
	if (source != src_distribution) {
		ss << "(" << sources.find(source)->second << ")";
	}
	return ss.str();
*/
	char buf[64];
	if (source != src_distribution) {
		SDL_snprintf(buf, sizeof(buf), "%s(%s)", bundleid.c_str(), sources.find(source)->second.c_str());
	} else {
		SDL_strlcpy(buf, bundleid.c_str(), sizeof(buf));
	}
	return buf;
}

std::string get_bundleid(int type)
{
	if (type == bundleid_leagor_basic) {
		return "aplt.leagor.basic";

	} else if (type == bundleid_leagor_khome) {
		return "aplt.leagor.khome";

	} else if (type == bundleid_leagor_basiclua) {
		return "aplt.leagor.basiclua";

	} else if (type == bundleid_leagor_khomelua) {
		return "aplt.leagor.khomelua";
	}
	VALIDATE(false, null_str);
	return null_str;
}

std::map<int, tcode3> reserved_tasks;
std::map<int, tpermission> sys_permissions;

void fill_per_if_necessary(std::set<per_t>& permissions)
{
	if (per_conflicted_always) {
		for (std::map<int, tpermission>::const_iterator it = sys_permissions.begin(); it != sys_permissions.end(); ++ it) {
			per_t per = it->second.per;
			if (permissions.count(per) == 0) {
				permissions.insert(per);
			}
		}
	}
}

const std::map<int, std::string> apps {
	{app_kdesktop, "kdesktop"},
	{app_launcher, "launcher"},
};

tapplet::tvar::tvar(const std::string& name, bool input, var_type_t type, bool optional)
	: name(name)
	, input(input)
	, type(type)
	, optional(optional)
{
	VALIDATE(!name.empty(), null_str);
	VALIDATE(type >= 0 && type < var_type_count, null_str);
	if (!input) {
		VALIDATE(!optional, null_str);
	}
}

tapplet::ttask::ttask(const std::string& id, task_type_t _type, int _subtype, bool _nonpreemptive, bool _recoverable, int _iot_src, const std::set<per_t>& _permissions,
	int max_fails, const std::vector<tvar>& _vars, bool _upload_image, bool _no_swap_wh_for_screen)
	: id(id)
	, type(_type)
	, subtype(_subtype)
	, nonpreemptive(_nonpreemptive)
	, recoverable(_recoverable)
	, iot_src(_iot_src)
	, permissions(_permissions)
	, max_fails(max_fails)
	, vars(_vars)
	, upload_image(_upload_image)
	, no_swap_wh_for_screen(_no_swap_wh_for_screen)
{
	VALIDATE(type >= 0 && type < task_type_count, null_str);
	if (type == task_nonblock) {
		VALIDATE(subtype >= 0 && subtype <= tnonblock_api::subtype_count, null_str);
	} else {
		VALIDATE(subtype == nposm, null_str);
	}
	if (type == task_ble) {
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(iot_src), null_str);
	} else {
		VALIDATE(iot_src == nposm, null_str);
	}
}

std::string tapplet::ttask::name2() const
{
	std::stringstream ss;
	ss << name;
/*
	if (!vars.empty()) {
		for (std::vector<tvar>::const_iterator it = vars.begin(); it != vars.end(); ++ it) {
			const tvar& var = *it;
			if (var.input) {
				ss << "(" << _("Require param") << ")";
				break;
			}
		}
	}
*/
	std::vector<std::string> specials;
	if (nonpreemptive) {
		specials.push_back(_("Non-preemptive"));
	}
	if (recoverable) {
		specials.push_back(_("Recoverable"));
	}
	if (!specials.empty()) {
		ss << "(" << utils::join(specials) << ")";
	}
	return ss.str();
}

void tapplet::fill_input_output_vars()
{
	VALIDATE(input_vars.empty() && output_vars.empty(), null_str);

	for (std::map<std::string, aplt::tapplet::ttask>::const_iterator task_it = tasks.begin(); task_it != tasks.end(); ++ task_it) {
		const aplt::tapplet::ttask& task = task_it->second;
		for (std::vector<aplt::tapplet::tvar>::const_iterator var_it = task.vars.begin(); var_it != task.vars.end(); ++ var_it) {
			const aplt::tapplet::tvar& var = *var_it;
			if (var.input) {
				input_vars.push_back(aplt::taplt_var_pair(*this, task.id, var.name, null_str));
			} else {
				output_vars.push_back(aplt::taplt_var_pair(*this, task.id, var.name, null_str));
			}
		}
	}
}

void tapplet::write_distribution_cfg() const
{
	VALIDATE(valid(), null_str);
	VALIDATE(source == src_distribution || source == src_development, null_str);
	std::stringstream ss;

	ss << "distribution = " << (source == src_distribution? config::attribute_value::s_yes: config::attribute_value::s_no) << "\n";
	ss << "name = \"" << name << "\"\n";
	ss << "subtitle = \"" << subtitle << "\"\n";
	ss << "username = \"" << username << "\"\n";
	ss << "ts = " << ts << "\n";
	ss << "rose_version = \"" << rose_version.str(true) << "\"\n";

	write_file(res_path + "/" + APLT_DISTRIBUTION_CFG, ss.str().c_str(), ss.str().size());
}

void tapplet::clear_vars(ttask_vars& task_vars, const std::string& task_id, bool input) const
{
	const std::vector<taplt_var_pair>& aplt_vars = input? input_vars: output_vars;

	for (std::vector<taplt_var_pair>::const_iterator it = aplt_vars.begin(); it != aplt_vars.end(); ++ it) {
		const taplt_var_pair& pair = *it;
		if (pair.task_id != task_id) {
			continue;
		}
		const std::string& name = it->var2;
		if (task_vars.existed(name) != 0) {
			task_vars.erase(name);
		}
	}
}

tapplet fake_aplt;

const std::string fake_task_id_empty = "empty_";
std::map<std::string, std::string> fake_fixed_tasks;
void fake_aplt_erase_nonfixed_tasks()
{
	for (std::map<std::string, tapplet::ttask>::iterator it = fake_aplt.tasks.begin(); it != fake_aplt.tasks.end(); ) {
		const tapplet::ttask& task = it->second;
		if (fake_fixed_tasks.count(task.id) != 0) {
			++ it;
		} else {
			fake_aplt.tasks.erase(it ++);
		}
	}
	VALIDATE(fake_aplt.tasks.size() == fake_fixed_tasks.size(), null_str);
}

/*
void tapplet::set_msgstr()
{
	const std::string textdomain = bundleid_2_lua_bundleid(bundleid) + "-lib";
	tbind_textdomain_lock textdomain_lock(*this);
	char buf[128];
	for (std::map<std::string, std::string>::iterator it = objects.begin(); it != objects.end(); ++ it) {
		SDL_snprintf(buf, sizeof(buf), "position^%s", it->first.c_str());
		it->second = dsgettext(textdomain.c_str(), buf);
	}

	for (std::map<std::string, tapplet::ttask>::iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		tapplet::ttask& timing = it->second;
		SDL_snprintf(buf, sizeof(buf), "timing^%s", timing.id.c_str());
		timing.name = dsgettext(textdomain.c_str(), buf);
	}
}
*/

const tapplet* aplt_from_id(const std::map<taplt_key, tapplet>& applets, const std::string& id)
{
	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const tapplet& applet = it->second;
		if (id == applet.id) {
			return &applet;
		}
	}
	return nullptr;
}

const tapplet* aplt_from_id_ex(const std::map<taplt_key, tapplet>& applets, const std::string& id)
{
	if (fake_aplt.id == id) {
		return &fake_aplt;
	}

	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const tapplet& applet = it->second;
		if (id == applet.id) {
			return &applet;
		}
	}
	return nullptr;
}

tapplet* mutable_aplt_from_id(std::map<taplt_key, tapplet>& applets, const std::string& id)
{
	for (std::map<taplt_key, tapplet>::iterator it = applets.begin(); it != applets.end(); ++ it) {
		tapplet& applet = it->second;
		if (id == applet.id) {
			return &applet;
		}
	}
	return nullptr;
}

const tapplet* aplt_from_id2(const std::map<taplt_key, tapplet>& applets, int src, const std::string& bundleid)
{
	taplt_key key(src, bundleid);
	if (applets.count(key) == 0) {
		return nullptr;
	}
	return &applets.find(key)->second;
}

tapplet* mutable_aplt_from_id2(std::map<taplt_key, tapplet>& applets, int src, const std::string& bundleid)
{
	taplt_key key(src, bundleid);
	if (applets.count(key) == 0) {
		return nullptr;
	}
	return &applets.find(key)->second;
}

const tapplet* aplt_from_bundleid(const std::map<taplt_key, tapplet>& applets, const std::string& bundleid)
{
	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const tapplet& applet = it->second;
		if (bundleid == applet.bundleid) {
			return &applet;
		}
	}
	return nullptr;
}

const tapplet* aplt_from_bundleid_ex(const std::map<taplt_key, tapplet>& applets, const std::string& bundleid)
{
	if (fake_aplt.bundleid == bundleid) {
		return &fake_aplt;
	}

	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const tapplet& applet = it->second;
		if (bundleid == applet.bundleid) {
			return &applet;
		}
	}
	return nullptr;
}

tapplet* mutable_aplt_from_bundleid(std::map<taplt_key, tapplet>& applets, const std::string& bundleid)
{
	for (std::map<taplt_key, tapplet>::iterator it = applets.begin(); it != applets.end(); ++ it) {
		tapplet& applet = it->second;
		if (bundleid == applet.bundleid) {
			return &applet;
		}
	}
	return nullptr;
}

const tapplet& aplt_from_at(const std::map<taplt_key, tapplet>& applets, int at)
{
	VALIDATE(at >= 0 && at < (int)applets.size(), null_str);
	std::map<taplt_key, tapplet>::const_iterator aplt_it = applets.begin();
	if (at != 0) {
		std::advance(aplt_it, at);
	}
	return aplt_it->second;
}

tapplet& mutable_aplt_from_at(std::map<taplt_key, tapplet>& applets, int at)
{
	VALIDATE(at >= 0 && at < (int)applets.size(), null_str);
	std::map<taplt_key, tapplet>::iterator aplt_it = applets.begin();
	if (at != 0) {
		std::advance(aplt_it, at);
	}
	return aplt_it->second;
}

void calculate_id_aplt_map(const std::map<taplt_key, tapplet>& applets, std::map<std::string, const tapplet*>& result)
{
	result.clear();

	result.insert(std::make_pair(fake_aplt.id, &fake_aplt));
	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const tapplet& applet = it->second;
		const std::string id = applet.id;
		result.insert(std::make_pair(id, &applet));
	}
}

ttask_pair task_pair_from_task_id(const tapplet& aplt, const std::string& task_id, bool must_valid)
{
	ttask_pair result;

	result.aplt = &aplt;
	if (aplt.tasks.count(task_id) != 0) {
		result.task = &aplt.tasks.find(task_id)->second;
	} else {
		VALIDATE(!must_valid, null_str);
	}
	return result;
}

ttask_pair task_pair_from_2_id(const std::map<taplt_key, tapplet>& applets, const std::string& aplt_id, const std::string& task_id,
	bool include_fake_aplt, bool aplt_id_is_bundleid)
{
	ttask_pair result;
	const tapplet* aplt;
	if (include_fake_aplt) {
		aplt = aplt_id_is_bundleid? aplt_from_bundleid_ex(applets, aplt_id): aplt_from_id_ex(applets, aplt_id);
	} else {
		aplt = aplt_id_is_bundleid? aplt_from_bundleid(applets, aplt_id): aplt_from_id(applets, aplt_id);
	}
	if (aplt == nullptr) {
		return result;
	}
	result.aplt = aplt;

	if (aplt->tasks.count(task_id) == 0) {
		return result;
	}
	result.task = &aplt->tasks.find(task_id)->second;
	return result;
}

ttask_pair task_pair_for_move(const std::map<taplt_key, tapplet>& applets)
{
	aplt::ttask_pair pair(fake_aplt, fake_aplt.tasks.find(fake_task_id_empty)->second);
	VALIDATE(pair.aplt != nullptr && pair.task != nullptr, null_str);
	return pair;
}

ttask_pair split_aplt_task_id2(const std::map<taplt_key, tapplet>& applets, const std::string& task_id2, bool aplt_id_is_bundleid)
{
	const std::pair<std::string, std::string> pair = utils::split_app_prefix_id(task_id2);
	const std::string& aplt_id = pair.first;
	const std::string& task_id = pair.second;

	aplt::ttask_pair result;
	if (aplt_id.empty() || task_id.empty()) {
		return result;
	}

	bool found = false;
	for (std::map<taplt_key, tapplet>::const_iterator it = applets.begin(); !found && it != applets.end(); ++ it) {
		const tapplet& aplt = it->second;
		const std::string that_aplt_id = aplt_id_is_bundleid? aplt.bundleid: aplt.id;
		if (that_aplt_id != aplt_id) {
			continue;
		}
		for (std::map<std::string, tapplet::ttask>::const_iterator it2 = aplt.tasks.begin(); !found && it2 != aplt.tasks.end(); ++ it2) {
			const tapplet::ttask& task = it2->second;
			if (task.id == task_id) {
				result.aplt = &aplt;
				result.task = &task;
				found = true;
			}
		}
	}
	return result;
}

std::string task_name2_from_3id(const std::map<aplt::taplt_key, aplt::tapplet>& applets, 
	const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, bool aplt_id_is_bundleid)
{
	const aplt::tapplet* hit_aplt = aplt_id_is_bundleid? aplt_from_bundleid_ex(applets, aplt_id): aplt_from_id_ex(applets, aplt_id);

	const aplt::tapplet::ttask* cfg_task = nullptr;
	std::string task_name = task_id;
	if (hit_aplt != nullptr && hit_aplt->tasks.count(task_id) != 0) {
		cfg_task = &hit_aplt->tasks.find(task_id)->second;
		task_name = cfg_task->name;
	}

	std::string aplt_name;
	if (hit_aplt != nullptr) {
		if (cfg_task != nullptr) {
			aplt_name = hit_aplt->name2();
		} else {
			aplt_name = hit_aplt->name2() + _("[Task absent]");
		}

	} else {
		aplt_name = aplt_id + _("[Unloaded]");
	}


	utils::string_map symbols;
	std::stringstream ss;
	ss << aplt_name<< "(" << task_name << ")";
	if (!ble_device_id.empty()) {
		ss << "-" << ble_device_id;
	}
	return ss.str();
}

//
// treq_task
//
static uint32_t sid = 0;


treq_task::treq_task()
	: id(sid ++)
	// , type(nposm)
	, aplt(nullptr)
	, task(nullptr)
{
	// VALIDATE(!position_uuid.empty(), null_str);
}

void treq_task::set_aplt_task(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id, 
	const std::string& _position1_uuid, const std::string& _position2_uuid)
{
	if (!_position1_uuid.empty()) {
		VALIDATE(utils::is_uuid(_position1_uuid, true), null_str);
		if (!_position2_uuid.empty()) {
			VALIDATE(utils::is_uuid(_position2_uuid, true), null_str);
		}

	} else {
		VALIDATE(_position2_uuid.empty(), null_str);
	}

	clear();
	// type = type_aplt_task;

	aplt = &_aplt;
	task = &_task;
	ble_device_id = _device_id;
	position1_uuid = _position1_uuid;
	position2_uuid = _position2_uuid;
}

void treq_task::assign10(const treq_task& that)
{
	id = that.id;
	// type = that.type;
	position1_uuid = that.position1_uuid;
	position2_uuid = that.position2_uuid;

	aplt = that.aplt;
	task = that.task;

	ble_device_id = that.ble_device_id;
}

bool treq_task::equal10(const treq_task& that) const
{
	return id == that.id &&
		position1_uuid == that.position1_uuid && position2_uuid == that.position2_uuid &&
		aplt == that.aplt && task == that.task &&
		ble_device_id == that.ble_device_id;
}

bool treq_task::valid() const
{ 
	// VALIDATE(type == nposm || type == aplt::treq_task::type_aplt_task, null_str);

	if (aplt != nullptr) {
		VALIDATE(task != nullptr, null_str);
		return true;
	}

	VALIDATE(task == nullptr, null_str);
	return false;
}

void treq_task::validate() const
{
	VALIDATE(valid(), null_str);
}

void treq_task::clear()
{
	// type = nposm;
	position1_uuid.clear();
	position2_uuid.clear();

	aplt = nullptr;
	task = nullptr;

	ble_device_id.clear();

	// input_vars.clear();
	// position1_if_block.clear();
	// position2_if_block.clear();
}


ttaskpoint::ttaskpoint(const taplt_task& klink_cpp_aplt_task2, const aplt::tapplet::ttask& cfg_task, int _state, const ttask_vars& _task_vars)
	: task_cpp_type(klink_cpp_aplt_task2.type)
	, task_cpp_pb_at(klink_cpp_aplt_task2.pb_at)
	, task_name(cfg_task.name)
	, state(_state)
	, task_vars(_task_vars)
	, ticks(SDL_GetTicks())
{
	VALIDATE(task_cpp_pb_at != nposm, null_str);
}

//
// ttask_api
//

void tcpp_api::treq_task2::set_aplt_task2(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id, 
	const tif_block& _position1, const tif_block& _position2)
{
	if (!_position1.branches.empty() && !_position1.branches[0].do_str.empty()) {
		// VALIDATE(utils::is_uuid(_position1_uuid, true), null_str);
		// if (!_position2_uuid.empty()) {
		//	VALIDATE(utils::is_uuid(_position2_uuid, true), null_str);
		// }

	} else {
		VALIDATE(_position2.branches.empty() || _position2.branches[0].do_str.empty(), null_str);
	}

	clear();
	is_aplt_task = true;

	aplt = &_aplt;
	task = &_task;
	ble_device_id = _device_id;
	
	position1_if_block = _position1;
	position2_if_block = _position2;
	 
	// position1_uuid = _position1_uuid;
	// position2_uuid = _position2_uuid;

	// input_vars = aplt->input_vars;
}

bool tcpp_api::treq_task2::gui2_task2_equal(const treq_task2& that) const
{
	if (is_aplt_task != that.is_aplt_task) {
		return false;
	}

	if (is_aplt_task) {
		if (input_vars.size() != that.input_vars.size() || input_vars != that.input_vars) {
			return false;
		}
		if (ble_device_id != that.ble_device_id) {
			return false;
		}
		if (position1_if_block != that.position1_if_block || position2_if_block != that.position2_if_block) {
			return false;
		}

		VALIDATE(position1_uuid.empty() && position2_uuid.empty() && that.position1_uuid.empty() && that.position2_uuid.empty(), null_str);
		VALIDATE(aplt == nullptr && task == nullptr && that.aplt == nullptr && that.task == nullptr, null_str);

		return aplt_id == that.aplt_id && task_id == that.task_id;
	}

	return true;
}

bool tcpp_api::treq_task2::valid() const
{
	// return treq_task::valid();
	return !aplt_id.empty() && !task_id.empty();
}

void tcpp_api::treq_task2::clear()
{
	treq_task::clear();

	is_aplt_task = false;

	input_vars.clear();
	position1_if_block.clear();
	position2_if_block.clear();
}

void tcpp_api::tstate2::sync_task_input_vars(const aplt::tapplet& aplt, const aplt::tapplet::ttask& task)
{
	VALIDATE(async_task.is_aplt_task, null_str);

	std::vector<std::string> var_names_in_task;
	for (std::vector<aplt::tapplet::tvar>::const_iterator var_it = task.vars.begin(); var_it != task.vars.end(); ++ var_it) {
		const aplt::tapplet::tvar& var = *var_it;
		if (!var.input) {
			continue;
		}
		var_names_in_task.push_back(utils::join_app_prefix_id(aplt.bundleid, var.name));
	}
	bool is_same = var_names_in_task.size() == async_task.input_vars.size();
	if (is_same && !async_task.input_vars.empty()) {
		int at = 0;
		for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = 
			async_task.input_vars.begin(); it != async_task.input_vars.end(); ++ it, at ++) {
			const std::string& var_name = it->first;
			if (var_name != var_names_in_task[at]) {
				is_same = false;
				break;
			}
		}
	}
	if (is_same) {
		return;
	}

	std::vector<std::pair<std::string, aplt::tif_block> > old_input_vars = async_task.input_vars;
	async_task.input_vars.clear();

	std::map<std::string, const aplt::tif_block*> old_input_vars2;
	for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = old_input_vars.begin(); it != old_input_vars.end(); ++ it) {
		const std::string& var_name = it->first;
		const aplt::tif_block& if_block = it->second;
		old_input_vars2.insert(std::make_pair(var_name, &if_block));
	}

	aplt::tif_block initial_if_block;
	for (std::vector<std::string>::const_iterator it = var_names_in_task.begin(); it != var_names_in_task.end(); ++ it) {
		const std::string& var_name = *it;
		const aplt::tif_block* if_block_ptr = &initial_if_block;
		if (old_input_vars2.count(var_name) != 0) {
			if_block_ptr = old_input_vars2.find(var_name)->second;
		}
		async_task.input_vars.push_back(std::make_pair(var_name, *if_block_ptr));
	}
}

bool tcpp_api::tstate2::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	bool modified = false;
	if (state == s1) {
		state = s2;
		modified = true;

	} else if (state == s2) {
		state = s1;
		modified = true;
	}

	modified |= finished.state_swap(s1, s2);

	return modified;
}

bool tcpp_api::tkey_2_state::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	bool modified = false;
	if (from_state == s1) {
		from_state = s2;
		modified = true;

	} else if (from_state == s2) {
		from_state = s1;
		modified = true;
	}

	return modified;
}

bool tcpp_api::tnext_state::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	bool modified = false;
	if (from_state == s1) {
		from_state = s2;
		modified = true;

	} else if (from_state == s2) {
		from_state = s1;
		modified = true;
	}
	if (to_state.state_swap(s1, s2)) {
		modified = true;
	}

	return modified;
}

tcpp_api::tcpp_api()
	: task_vars_(nullptr)
	// , taskpoint_(nullptr)
{
}

tcpp_api::~tcpp_api()
{}

//
// cpp aplt session
//
std::string tcpp_api::start_task(const std::string& id, ttask_vars& vars, const ttaskpoint* taskpoint)
{
	VALIDATE(!id.empty(), null_str);
    VALIDATE(task_id_.empty(), null_str);
	VALIDATE(task_vars_ == nullptr, null_str);
	// VALIDATE(taskpoint_ == nullptr, null_str);

	task_id_ = id;
	task_vars_ = &vars;
	// taskpoint_ = taskpoint;

	pre_2th_start_task();

	const std::string err_msg = app_start_task(taskpoint);

	if (err_msg.empty()) {
		post_2th_start_task();

	} else {
		// during app_start_task(), some variable of 2th-class maybe dirty. call clear_2th() clear. 
		clear_2th();

		// Even if it fails, do not clear the output variables generated during the above process, 
		// as these variables may be useful. for example, 'result_msg' identifying the cause of the error.

		task_id_.clear();
		task_vars_ = nullptr;
		// taskpoint_ = nullptr;
	}

	return err_msg;
}

void tcpp_api::task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result)
{
	VALIDATE(cfg_task.id == task_id_, null_str);
	app_task_finished(aplt, cfg_task, result);

	post_2th_task_finished(aplt, cfg_task);

	clear_2th();

	task_id_.clear();
	task_vars_ = nullptr;
	// taskpoint_ = nullptr;
}

//
// moveit aplt session
//
tmoveit_api::tmoveit_api()
{
	target_info_.op = nposm;
}

tmoveit_api::~tmoveit_api()
{}

tmoveit_target_info_C tmoveit_api::start_task(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
    VALIDATE(!cfg_task.id.empty(), null_str);
    VALIDATE(task_id_.empty(), null_str);
	VALIDATE(target_info_.op == nposm, null_str);
 
	task_id_ = cfg_task.id;

	target_info_.size = SDL_DSize3{0.03, 0.03, 0.03};
	target_info_.first_dcpitch = float_nposm;
	target_info_.second_dcpitch = float_nposm;
	target_info_.set_ik_dcpitch = float_nposm;

	app_start_task(target_info_);
	VALIDATE(target_info_.op >= 0 && target_info_.op < moveit_op_count, null_str);

	return target_info_;
}

void tmoveit_api::stop_task(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
    VALIDATE(!cfg_task.id.empty(), null_str);
    VALIDATE(task_id_ == cfg_task.id, null_str);

    app_stop_task();

    task_id_.clear();
	target_info_.op = nposm;
}

//
// tcamera_api session
//
std::string tcamera_api::start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars)
{
	VALIDATE(!cfg_task.id.empty(), null_str);
	VALIDATE(task_id_.empty(), null_str);

	task_id_ = cfg_task.id;
	task_vars_ = &vars;

	const std::string err_msg = app_start_task(vars);

	if (err_msg.empty()) {

	} else {
		aplt.clear_vars(vars, cfg_task.id, true);

		task_id_.clear();
		task_vars_ = nullptr;
	}

	return err_msg;
}

void tcamera_api::task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
	VALIDATE(cfg_task.id == task_id_, null_str);
	app_task_finished(cfg_task);

	// post_2th_task_finished(aplt, cfg_task);

	// clear_2th();
	aplt.clear_vars(*task_vars_, cfg_task.id, true);

	task_id_.clear();
	task_vars_ = nullptr;
}

//
// tblock_api session
//
std::string tblock_api::start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars)
{
	VALIDATE(!cfg_task.id.empty(), null_str);

	task_id_ = cfg_task.id;

	const std::string err_msg = app_start_task(vars);

	task_id_.clear();
	if (err_msg.empty()) {

	} else {
	}

	aplt.clear_vars(vars, cfg_task.id, true);

	return err_msg;
}

//
// tnonblock_api session
//
const std::string charge_task_id_charge = "charge";

std::string tnonblock_api::start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars)
{
	VALIDATE(!cfg_task.id.empty(), null_str);
	VALIDATE(task_id_.empty(), null_str);
	VALIDATE(is_float_nposm(charge_width_), null_str);

	// now task_nonblock is only one task_id.
	// VALIDATE(id == charge_task_id_charge, null_str);

	task_id_ = cfg_task.id;
	task_vars_ = &vars;
	subtype_ = cfg_task.subtype;

	const std::string err_msg = app_start_task(vars);

	if (err_msg.empty()) {

	} else {
		aplt.clear_vars(vars, cfg_task.id, true);

		task_id_.clear();
		task_vars_ = nullptr;
		subtype_ = nposm;
		charge_width_ = float_nposm;
	}

	return err_msg;
}

void tnonblock_api::task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
	VALIDATE(cfg_task.id == task_id_, null_str);
	app_task_finished(cfg_task);

	// post_2th_task_finished(aplt, cfg_task);

	// clear_2th();
	aplt.clear_vars(*task_vars_, cfg_task.id, true);

	task_id_.clear();
	task_vars_ = nullptr;
	subtype_ = nposm;
	charge_width_ = float_nposm;
}

void tnonblock_api::set_charge_width(double charge_width)
{
	VALIDATE(subtype_ == subtype_charge, null_str);
	VALIDATE(charge_width > 0, null_str);

	charge_width_ = charge_width;
}

//
// taiagent_api
//
const std::vector<taiagent_api::tprompt>& taiagent_api::prompts(const std::string& task_id, int subject, const std::string& header)
{
	VALIDATE(!task_id.empty(), null_str);
	// VALIDATE(prompts_.count(task_id) != 0, null_str);

	prompts_.clear();
	app_prompts(task_id, subject, header, prompts_);

	return prompts_;
}
/*
std::vector<taiagent_api::tprompt> taiagent_api::latex_prompts(const std::string& task_id, int subject, const std::string& header)
{
	std::vector<tprompt> result;
	result.push_back(tprompt(_("No questions are available."), _("This task does not support using the latex_prompts API to fetch prompts.")));
	return result;
}
*/
std::string taiagent_api::start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, aplt::ttask_vars& vars)
{
	VALIDATE(!cfg_task.id.empty(), null_str);
	VALIDATE(task_id_.empty(), null_str);

	// now task_nonblock is only one task_id.
	// VALIDATE(id == charge_task_id_charge, null_str);

	task_id_ = cfg_task.id;
	task_vars_ = &vars;

	const std::string err_msg = app_start_task(question, surf, vars);

	if (err_msg.empty()) {

	} else {
		aplt.clear_vars(vars, cfg_task.id, true);

		task_id_.clear();
		task_vars_ = nullptr;
	}

	return err_msg;
}

void taiagent_api::task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
	VALIDATE(cfg_task.id == task_id_, null_str);
	VALIDATE(terminating_, null_str);

	app_task_finished(cfg_task);

	// post_2th_task_finished(aplt, cfg_task);

	// clear_2th();
	aplt.clear_vars(*task_vars_, cfg_task.id, true);

	task_id_.clear();
	task_vars_ = nullptr;

	terminating_ = false;
}

std::map<std::string, std::set<const ttask_api*> > task_api_map;

ttask_api::ttask_api(const tapplet& _aplt, tcpp_api* _cpp, tmoveit_api* _moveit, tcamera_api* _camera, tblock_api* _block, tnonblock_api* _nonblock, taiagent_api* _aiagent)
	: aplt(_aplt)
	, cpp(_cpp)
	, moveit(_moveit)
	, camera(_camera)
	, block(_block)
	, nonblock(_nonblock)
	, aiagent(_aiagent)
{
	VALIDATE_IN_MAIN_THREAD();

	std::map<std::string, std::set<const ttask_api*> >::iterator it = task_api_map.find(aplt.id);
	if (it == task_api_map.end()) {
		std::pair<std::map<std::string, std::set<const ttask_api*> >::iterator, bool> ins = 
			task_api_map.insert(std::make_pair(aplt.id, std::set<const ttask_api*>({this})));
		it = ins.first;
	} else {
		it->second.insert(this);
	}
}

ttask_api::~ttask_api()
{
	VALIDATE_IN_MAIN_THREAD();

	std::map<std::string, std::set<const ttask_api*> >::iterator it = task_api_map.find(aplt.id);
	VALIDATE(it != task_api_map.end(), null_str);

	std::set<const ttask_api*>& api_set = it->second;
	std::set<const ttask_api*>::iterator set_it = api_set.find(this);
	VALIDATE(set_it != api_set.end(), null_str);
	api_set.erase(set_it);
	if (api_set.empty()) {
		task_api_map.erase(it);
	}

	if (cpp != nullptr) {
		delete cpp;
		cpp = nullptr;
	}

	if (moveit != nullptr) {
		delete moveit;
		moveit = nullptr;
	}

	if (camera != nullptr) {
		delete camera;
		camera = nullptr;
	}

	if (block != nullptr) {
		delete block;
		block = nullptr;
	}

	if (nonblock != nullptr) {
		delete nonblock;
		nonblock = nullptr;
	}

	if (aiagent != nullptr) {
		delete aiagent;
		aiagent = nullptr;
	}
}


ttask_cpp_pair::ttask_cpp_pair()
	: pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
	, input_var_signature_("input_var_")
{
	clear();
}


std::map<int, std::string> minor_key_strategies;

int minor_key_strategy_from_str(const std::string& str)
{
	VALIDATE(!minor_key_strategies.empty(), null_str);

	for (std::map<int, std::string>::const_iterator it = minor_key_strategies.begin(); it != minor_key_strategies.end(); ++ it) {
		const std::string& type = it->second;
		if (type == str) {
			return it->first;
		}
	}
	return nposm;
}

void parse_py_words_from_cfg_str(tpinyin& pinyin, int tone, bool eng_lowercase, const std::string& cfg_str, std::vector<std::string>& words, std::vector<std::string>& py_words)
{
	std::vector<std::string> word_vec = utils::split(cfg_str);
	for (std::vector<std::string>::const_iterator it = word_vec.begin(); it != word_vec.end(); ++ it) {
		const std::string& word = *it;
		words.push_back(word);
		py_words.push_back(pinyin.from_utf8str2(word, tone, eng_lowercase));
	}
}

bool word_match_fields_from_cfg_no_minor(tpinyin& pinyin, int tone, bool eng_lowercase, const config& cfg, std::string& major_word, std::string& py_major_word, 
	int& strategy)
{
	major_word = cfg["major_word"].str();
	py_major_word = pinyin.from_utf8str2(major_word, tone, eng_lowercase);

	const int default_strategy = mkeys_any_one;
	strategy = default_strategy;
	const std::string strategy_str = cfg["strategy"].str();
	if (!strategy_str.empty()) {
		strategy = aplt::minor_key_strategy_from_str(strategy_str);
		if (strategy == nposm) {
			// Probably, there has to be a fail here
			strategy = default_strategy;
		}
	}

	return true;
}

bool word_match_fields_from_cfg(tpinyin& pinyin, int tone, bool eng_lowercase, const config& cfg, std::string& major_word, std::string& py_major_word, 
	std::vector<std::string>& minor_words, std::vector<std::string>& py_minor_words, int& strategy)
{
	word_match_fields_from_cfg_no_minor(pinyin, tone, eng_lowercase, cfg, major_word, py_major_word, strategy);
	const std::string minor_words_str = cfg["minor_words"].str();
	aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, minor_words_str, minor_words, py_minor_words);
/*
	major_word = cfg["major_word"].str();
	py_major_word = pinyin.from_utf8str(major_word, tone, eng_lowercase, nullptr);

	const std::string minor_keys_str = cfg["minor_words"].str();
	aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, minor_keys_str, minor_words, py_minor_words);

	const int default_strategy = mkeys_any_one;
	strategy = default_strategy;
	const std::string strategy_str = cfg["strategy"].str();
	if (!strategy_str.empty()) {
		strategy = aplt::minor_key_strategy_from_str(strategy_str);
		if (strategy == nposm) {
			// Probably, there has to be a fail here
			strategy = default_strategy;
		}
	}
*/
	return true;
}

void word_match_fields_to_cfg_no_minor(const std::string& major_word, int strategy, config& cfg)
{
	cfg["major_word"].from_string(major_word, true);
	// cfg["minor_words"].from_string(utils::join(minor_words), true); // "01234567890"
	VALIDATE(strategy >= 0 && strategy < aplt::mkeys_count, null_str);
	cfg["strategy"] = aplt::minor_key_strategies.find(strategy)->second;
}

void word_match_fields_to_cfg(const std::string& major_word, const std::vector<std::string>& minor_words, int strategy, config& cfg)
{
	word_match_fields_to_cfg_no_minor(major_word, strategy, cfg);
	cfg["minor_words"].from_string(utils::join(minor_words), true); // "01234567890"
/*
	cfg["major_word"].from_string(major_word, true);
	cfg["minor_words"].from_string(utils::join(minor_words), true); // "01234567890"
	VALIDATE(strategy >= 0 && strategy < aplt::mkeys_count, null_str);
	cfg["strategy"] = aplt::minor_key_strategies.find(strategy)->second;
*/
}

void word_match_did_minor_words_changed(aplt::tpinyin& pinyin, int tone, bool eng_lowercase, aplt::tcpp_api::tkey_2_state& key_2_state)
{
	key_2_state.py_minor_words_ready = false;
	key_2_state.py_minor_words.clear();

	const int minor_words_size = key_2_state.minor_words.branches.size();
	if (minor_words_size == 0) {
		// if_block is empty, pinyin is empty of couse.
		key_2_state.py_minor_words_ready = true;

	} else if (minor_words_size == 1) {
		const std::string& minor_words_str = key_2_state.minor_words.branches[0].do_str;
		if (minor_words_str.find("$") == std::string::npos) {
			// To improve efficiency, if there is only one branch and no variables, 
			// pre-line parsing out pinyin.
			std::vector<std::string> word_vec;
			aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, minor_words_str, word_vec, key_2_state.py_minor_words);
			key_2_state.py_minor_words_ready = true;
		}
	}
}

bool major_minor_words_match(const std::string& pinyin, const std::string& py_major_word, const std::vector<std::string>& py_minor_words, int strategy, int* any_one_at)
{
	VALIDATE(!py_major_word.empty() || !py_minor_words.empty(), null_str);
	if (any_one_at != nullptr) {
		*any_one_at = nposm;
	}
	if (!py_major_word.empty()) {
		if (pinyin.find(py_major_word) == std::string::npos) {
			return false;
		}
	}

	bool fail = false;
	bool at_least_one = false;
	size_t off = 0;
	int at = 0;
	for (std::vector<std::string>::const_iterator it2 = py_minor_words.begin(); it2 != py_minor_words.end(); ++ it2, at ++) {
		const std::string& word = *it2;
		VALIDATE(!word.empty(), null_str);
		if (strategy == mkeys_any_one || strategy == mkeys_all_match_no_order) {
			off = 0;
		}
		size_t pos = pinyin.find(word, off);
		if (pos != std::string::npos) {
			// enum {mkeys_any_one, mkeys_all_match_and_order, mkeys_all_match_no_order, mkeys_count};
			if (strategy == mkeys_any_one) {
				// hit_key_2_state = &key_2_state;
				at_least_one = true;
				if (any_one_at != nullptr) {
					*any_one_at = at;
				}
				break;

			} else if (strategy == mkeys_all_match_and_order) {
				// if (pos < last_pos) {
				//	fail = true;
				//	break;
				// }
			}
			off = pos + word.size();

		} else if (strategy == mkeys_all_match_and_order || strategy == mkeys_all_match_no_order) {
			fail = true;
			break;
		}
	}
	if (!py_minor_words.empty()) {
		if (strategy == mkeys_any_one && !at_least_one) {
			fail = true;
		}
	}

	return !fail;
}

bool ttask_cpp_pair::from_state2_cfg(const config& cfg, std::map<std::string, int>& _state_names_map)
{
	const std::map<std::string, int>& state_names_map = _state_names_map;

	const std::string state_name = cfg["state"].str();
	VALIDATE(state_names_map.count(state_name) != 0, null_str);

	// int enum_state = state_names.size();
	// state_names_map.insert(std::make_pair(state_name, enum_state));
	int enum_state = state_names_map.find(state_name)->second;

	state_names.push_back(state_name);
	int threshold_s = cfg["threshold_s"].to_int(nposm);
	VALIDATE(threshold_s != nposm, null_str);
	std::pair<std::map<int, tcpp_api::tstate2>::iterator, bool> ins = states.insert(
		std::make_pair(enum_state, tcpp_api::tstate2(enum_state, threshold_s)));
	VALIDATE(ins.second, null_str);

	tcpp_api::tstate2& state2 = ins.first->second;
	state2.doing = cfg["doing"].str();
	// state2.finished.cfg["finished"].str();
	state2.finished.from_cfg("finished", state_names_map.size(), cfg);

	// [async_task]
	if (cfg.has_child("async_task")) {
		const config& req_task_cfg = cfg.child("async_task");
		state2.async_task.is_aplt_task = true;

		state2.async_task.aplt_id = req_task_cfg["aplt"].str();
		state2.async_task.task_id = req_task_cfg["task"].str();

		BOOST_FOREACH(const config::any_child& cfg2, req_task_cfg.all_children_range()) {
			if (cfg2.key.size() <= input_var_signature_.size()) {
				continue;
			}
			size_t pos = cfg2.key.find(input_var_signature_);
			if (pos != 0) {
				continue;
			}
			std::string name = cfg2.key.substr(input_var_signature_.size());
			std::string name2 = utils::join_app_prefix_id(state2.async_task.aplt_id, name);
			state2.async_task.input_vars.push_back(std::make_pair(name2, tif_block()));
			state2.async_task.input_vars.back().second.from_cfg(cfg2.key, state_names_map.size(), req_task_cfg);
		}

		state2.async_task.ble_device_id = req_task_cfg["ble_device_id"].str();

		state2.async_task.position1_if_block.from_cfg("position1", state_names_map.size(), req_task_cfg);
		state2.async_task.position2_if_block.from_cfg("position2", state_names_map.size(), req_task_cfg);
	}

	return true;
}

void did_minor_words_changed(aplt::tpinyin& pinyin, int tone, bool eng_lowercase, aplt::tcpp_api::tkey_2_state& key_2_state)
{
	key_2_state.py_minor_words_ready = false;
	key_2_state.py_minor_words.clear();

	const int minor_words_size = key_2_state.minor_words.branches.size();
	if (minor_words_size == 0) {
		// if_block is empty, pinyin is empty of couse.
		key_2_state.py_minor_words_ready = true;

	} else if (minor_words_size == 1) {
		const std::string& minor_words_str = key_2_state.minor_words.branches[0].do_str;
		if (minor_words_str.find("$") == std::string::npos) {
			// To improve efficiency, if there is only one branch and no variables, 
			// pre-line parsing out pinyin.
			std::vector<std::string> word_vec;
			aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, minor_words_str, word_vec, key_2_state.py_minor_words);
			key_2_state.py_minor_words_ready = true;
		}
	}
}

bool ttask_cpp_pair::from_key_2_state_cfg(const config& cfg, const std::map<std::string, int>& state_names_map)
{
	const std::string from_state_str = cfg["from_state"].str();
	int from_state = nposm;
	if (!from_state_str.empty()) {
		VALIDATE(state_names_map.count(from_state_str) != 0, null_str);
		from_state = state_names_map.find(from_state_str)->second;
	}
/*
	const std::string to_state_str = cfg["to_state"].str();
	int to_state = nposm;
	if (!to_state_str.empty()) {
		VALIDATE(state_names_map.count(to_state_str) != 0, null_str);
		to_state = state_names_map.find(to_state_str)->second;
	}

	key_2_states.push_back(tcpp_api::tkey_2_state(from_state, to_state));
*/
	key_2_states.push_back(tcpp_api::tkey_2_state(from_state));
	tcpp_api::tkey_2_state& key_2_state = key_2_states.back();

	VALIDATE(!key_2_state.py_minor_words_ready, null_str);
	key_2_state.minor_words.from_cfg("minor_words", state_names_map.size(), cfg);

	word_match_fields_from_cfg_no_minor(pinyin_, tone_, eng_lowercase_, cfg, key_2_state.major_word, key_2_state.py_major_word, 
		key_2_state.strategy);

	word_match_did_minor_words_changed(pinyin_, tone_, eng_lowercase_, key_2_state);

	return true;
}

bool ttask_cpp_pair::from_cfg(const config& cfg)
{
	clear();

	bool fail = false;
	id = cfg["id"].str();
	name = cfg["name"].str();
	reception_state = cfg["reception_state"].to_int(nposm);
	recoverable = cfg["recoverable"].to_bool();
	nonpreemptive = cfg["nonpreemptive"].to_bool();

	std::map<std::string, int> state_names_map;
	int enum_state = 0;
	std::vector<const config*> state2_cfgs;
	BOOST_FOREACH(const config::any_child& task_cpp, cfg.all_children_range()) {
		if (task_cpp.key == "state2") {
			const config& state2_cfg = task_cpp.cfg;
			const std::string state_name = state2_cfg["state"].str();
			state_names_map.insert(std::make_pair(state_name, enum_state));
			enum_state ++;
			state2_cfgs.push_back(&state2_cfg);
		}
	}
	for (std::vector<const config*>::const_iterator it = state2_cfgs.begin(); it != state2_cfgs.end(); ++ it) {
		const config& state2_cfg = **it;
		if (!from_state2_cfg(state2_cfg, state_names_map)) {
			fail = true;
			VALIDATE(false, null_str);
			break;
		}
	}

	startup_state.from_cfg("startup_state", state_names_map.size(), cfg);
	if (reception_state != nposm && (reception_state < 0 || reception_state >= (int)states.size())) {
		reception_state = nposm;
	}

	bool startup_next_state_parsed = false;
	BOOST_FOREACH(const config::any_child& task_cpp, cfg.all_children_range()) {
		if (task_cpp.key == "key_2_state") {
			const config& key_2_state_cfg = task_cpp.cfg;
			if (!from_key_2_state_cfg(key_2_state_cfg, state_names_map)) {
				fail = true;
				VALIDATE(false, null_str);
				break;
			}
		}
	}

	return !fail;
}

void ttask_cpp_pair::to_cfg(config& cfg) const
{
	cfg.clear();

	VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);
	cfg["id"] = id;
	cfg["name"] = name;
	cfg["reception_state"].from_int(reception_state);
	cfg["recoverable"].from_bool(recoverable);
	cfg["nonpreemptive"].from_bool(nonpreemptive);
	startup_state.to_cfg("startup_state", state_names.size(), cfg);

	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const aplt::tcpp_api::tstate2& state2 = it->second;

		config& state2_cfg = cfg.add_child("state2");
		state2_cfg["state"] = state_names[state2.state];
		state2_cfg["threshold_s"].from_int(state2.threshold_s);
		state2_cfg["doing"] = state2.doing;
		state2.finished.to_cfg("finished", state_names.size(), state2_cfg);

		if (!state2.async_task.is_aplt_task) {
			continue;
		}

		config& async_task_cfg = state2_cfg.add_child("async_task");
		const aplt::tcpp_api::treq_task2& req_task = state2.async_task;

		// const std::string is_aplt_task_str = "aplt_task";
		// async_task_cfg["type"] = is_aplt_task_str;
		{
			async_task_cfg["aplt"] = req_task.aplt_id;
			async_task_cfg["task"] = req_task.task_id;

			for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = state2.async_task.input_vars.begin(); it != state2.async_task.input_vars.end(); ++ it) {
				const std::string& name2 = it->first;
				const aplt::tif_block& input_var = it->second;
				// wml's [] name not support '.'. but it appear in 'aplt.leagor.khome__query_name'.
				const std::string name = utils::split_app_prefix_id(name2).second;
				input_var.to_cfg(input_var_signature_ + name, state_names.size(), async_task_cfg);
			}

			async_task_cfg["ble_device_id"].from_string(req_task.ble_device_id, true); // "002048"

			// async_task_cfg["position1"] = req_task.position1_uuid;
			// async_task_cfg["position2"] = req_task.position2_uuid;
			req_task.position1_if_block.to_cfg("position1", state_names.size(), async_task_cfg);
			req_task.position2_if_block.to_cfg("position2", state_names.size(), async_task_cfg);
		}
	}

	for (std::vector<aplt::tcpp_api::tkey_2_state>::const_iterator it = key_2_states.begin(); it != key_2_states.end(); ++ it) {
		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;

		config& key_2_state_cfg = cfg.add_child("key_2_state");
		key_2_state_cfg["from_state"] = key_2_state.from_state != nposm? state_names[key_2_state.from_state]: null_str;

		word_match_fields_to_cfg_no_minor(key_2_state.major_word, key_2_state.strategy, key_2_state_cfg);
		key_2_state.minor_words.to_cfg("minor_words", state_names.size(), key_2_state_cfg);
	}
}

bool ttask_cpp_pair::equal(const aplt::ttask_cpp_pair& that) const
{
	if (id != that.id || name != that.name || reception_state != that.reception_state || 
		nonpreemptive != that.nonpreemptive || recoverable != that.recoverable || 
		startup_state != that.startup_state) {
		return false;
	}

	if (state_names.size() != that.state_names.size() || state_names != that.state_names) {
		return false;
	}

	if (states.size() != that.states.size() || states != that.states) {
		return false;
	}

	if (key_2_states.size() != that.key_2_states.size() || key_2_states != that.key_2_states) {
		return false;
	}

	return true;
}

void ttask_cpp_pair::assign(const aplt::ttask_cpp_pair& that)
{
	id = that.id;
	name = that.name;
	reception_state = that.reception_state;
	recoverable = that.recoverable;
	nonpreemptive = that.nonpreemptive;
	startup_state = that.startup_state;

	state_names = that.state_names;
	states = that.states;
	key_2_states = that.key_2_states;
}

void ttask_cpp_pair::sync_task_input_vars(const std::map<aplt::taplt_key, aplt::tapplet>& applets)
{
	for (std::map<int, tcpp_api::tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tcpp_api::tstate2& state2 = it->second;
		aplt::ttask_pair task_pair = task_pair_from_2_id(applets, state2.async_task.aplt_id, state2.async_task.task_id, true, true);
		if (task_pair.task != nullptr) {
			state2.sync_task_input_vars(*task_pair.aplt, *task_pair.task);
		}
	}
}

bool ttask_cpp_pair::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	// 1)reception state
	if (reception_state == s1) {
		reception_state = s2;

	} else if (reception_state == s2) {
		reception_state = s1;
	}

	// 2)startup state
	startup_state.state_swap(s1, s2);

	// 3) state2
	std::iter_swap(state_names.begin() + s1, state_names.begin() + s2);

	// !!!std::map don't support std::iter_swap().
	for (std::map<int, tcpp_api::tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tcpp_api::tstate2& state2 = it->second;
		state2.state_swap(s1, s2);
	}

	// 1/3: save all state
	aplt::tcpp_api::tstate2 states_s1 = states.find(s1)->second;
	aplt::tcpp_api::tstate2 states_s2 = states.find(s2)->second;
	// 2/3: erase.
	states.erase(states.find(s1));
	states.erase(states.find(s2));
	// 3/3: insert, must erase all state before insert.
	std::pair<std::map<int, tcpp_api::tstate2>::iterator, bool> ins = states.insert(std::make_pair(states_s1.state, states_s1));
	VALIDATE(ins.second, null_str);
	ins = states.insert(std::make_pair(states_s2.state, states_s2));
	VALIDATE(ins.second, null_str);
		
	for (std::vector<tcpp_api::tkey_2_state>::iterator it = key_2_states.begin(); it != key_2_states.end(); ++ it) {
		tcpp_api::tkey_2_state& key_2_state = *it;
		key_2_state.state_swap(s1, s2);
	}

	return true; // modified
}

void ttask_cpp_pair::green()
{
	for (std::map<int, tcpp_api::tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tcpp_api::tstate2& state2 = it->second;
		VALIDATE(state2.async_task.position1_uuid.empty(), null_str);
		VALIDATE(state2.async_task.position2_uuid.empty(), null_str);

		if (state2.async_task.is_aplt_task) {

		} else {
			if (!state2.async_task.aplt_id.empty()) {
				state2.async_task.aplt_id.clear();
			}
			if (!state2.async_task.task_id.empty()) {
				state2.async_task.task_id.clear();
			}
			if (!state2.async_task.input_vars.empty()) {
				state2.async_task.input_vars.clear();
			}
			if (!state2.async_task.position1_if_block.branches.empty()) {
				state2.async_task.position1_if_block.clear();
			}
			if (!state2.async_task.position2_if_block.branches.empty()) {
				state2.async_task.position2_if_block.clear();
			}
		}

	}
}

std::string ttask_cpp_pair::py_from_utf8str(const std::string& str) const
{ 
	return pinyin_.from_utf8str2(str, tone_, eng_lowercase_);
}

//
// iot
//
std::map<int, tiot_src2> iot_sources;
std::map<int, tcode3> iot_events;

int iot_src_from_str(const std::string& str)
{
	VALIDATE(!iot_sources.empty(), null_str);

	for (std::map<int, tiot_src2>::const_iterator it = iot_sources.begin(); it != iot_sources.end(); ++ it) {
		const std::string& id = it->second.id;
		if (id == str) {
			return it->first;
		}
	}
	return nposm;
}

int iot_event_from_str(const std::string& str)
{
	VALIDATE(!iot_events.empty(), null_str);

	for (std::map<int, tcode3>::const_iterator it = iot_events.begin(); it != iot_events.end(); ++ it) {
		const std::string& id = it->second.id;
		if (id == str) {
			return it->first;
		}
	}
	return nposm;
}

bool tiot_heartbeat::operator<(const tiot_heartbeat& that) const noexcept
{
	if (t != that.t) {
		return t > that.t;
	}
	if (src != that.src) {
		return src < that.src;
	}
	int cmp = SDL_strcmp(device_id.c_str(), that.device_id.c_str());
	return cmp < 0;
}

bool tiot_event::operator<(const tiot_event& that) const noexcept
{
	if (t != that.t) {
		return t > that.t;
	}
	if (src != that.src) {
		return src < that.src;
	}
	if (evt != that.evt) {
		return evt < that.evt;
	}
	int cmp = SDL_strcmp(device_id.c_str(), that.device_id.c_str());
	return cmp < 0;
}

//
// taplt_task 
// 

std::map<int, std::string> nontimed_priorities;

const std::string& nontimed_priority_name(int priority)
{
	VALIDATE(nontimed_priorities.count(priority) != 0, null_str);
	return nontimed_priorities.find(priority)->second;
}

// for ttemp_aplt_task
taplt_task::taplt_task(const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
	const std::string& position1, const std::string& position2, int state)
	: type(type_temp)
	, priority(priority_temp)
	, aplt_id(aplt_id)
	, task_id(task_id)
	, ble_device_id(ble_device_id)
	, position1(position1)
	, position2(position2)
	, state(state)
	, pb_at(nposm)
	, fails(0)
	, last_shedule_ticks(0)
	, iot_src(nposm)
	, src_evt(nposm)
	, zerotz_t(nposm)
	, var_at(nposm)
	, timed_at(nposm)
	, aux_key_id(nposm)
{
	VALIDATE(pb_at == nposm, null_str);
	VALIDATE(iot_src == nposm && src_evt == nposm, null_str);
	VALIDATE(zerotz_t == nposm, null_str);
	VALIDATE(var_at == nposm, null_str);
	VALIDATE(timed_at == nposm, null_str);
}

// for tiot_aplt_task
taplt_task::taplt_task(int priority, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
	const std::string& position1, const std::string& position2, int state, int pb_at, int iot_src, int src_evt, const std::string& src_device_id)
	: type(type_iot)
	, priority(priority)
	, aplt_id(aplt_id)
	, task_id(task_id)
	, ble_device_id(ble_device_id)
	, position1(position1)
	, position2(position2)
	, state(state)
	, pb_at(pb_at)
	, fails(0)
	, last_shedule_ticks(0)
	, iot_src(iot_src)
	, src_evt(src_evt)
	, src_device_id(src_device_id)
	, zerotz_t(nposm)
	, var_at(nposm)
	, timed_at(nposm)
	, aux_key_id(nposm)
{
	VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority), null_str);
	VALIDATE(pb_at >= 0, null_str);
	VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(iot_src), null_str);
	VALIDATE(!src_device_id.empty(), null_str);
	VALIDATE(zerotz_t == nposm, null_str);
	VALIDATE(var_at == nposm, null_str);
	VALIDATE(timed_at == nposm, null_str);
	VALIDATE(aux_key_id == nposm, null_str);
}

// for tspeech_aplt_task
taplt_task::taplt_task(int priority, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
	const std::string& position1, const std::string& position2, int state, int pb_at, const std::string speech_id)
	: type(type_speech)
	, priority(priority)
	, aplt_id(aplt_id)
	, task_id(task_id)
	, ble_device_id(ble_device_id)
	, position1(position1)
	, position2(position2)
	, state(state)
	, pb_at(pb_at)
	, fails(0)
	, last_shedule_ticks(0)
	, iot_src(nposm)
	, src_evt(nposm)
	, speech_id(speech_id)
	, zerotz_t(nposm)
	, var_at(nposm)
	, timed_at(nposm)
	, aux_key_id(nposm)
{
	VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority), null_str);
	VALIDATE(pb_at >= 0, null_str);
	VALIDATE(iot_src == nposm && src_evt == nposm, null_str);
	VALIDATE(!speech_id.empty(), null_str);
	VALIDATE(zerotz_t == nposm, null_str);
	VALIDATE(var_at == nposm, null_str);
	VALIDATE(timed_at == nposm, null_str);
	VALIDATE(aux_key_id == nposm, null_str);
}

// for tvar_aplt_task
taplt_task::taplt_task(const std::string& aplt_id, const std::string& task_id,
	const std::string& position1, const std::string& position2, int state, int pb_at, int var_at, int aux_key_id)
	: type(type_var)
	, priority(priority_timed)
	, aplt_id(aplt_id)
	, task_id(task_id)
	, ble_device_id(null_str)
	, position1(position1)
	, position2(position2)
	, state(state)
	, pb_at(pb_at)
	, fails(0)
	, last_shedule_ticks(0)
	, iot_src(nposm)
	, src_evt(nposm)
	, zerotz_t(nposm)
	, var_at(var_at)
	, timed_at(nposm)
	, aux_key_id(aux_key_id)
{
	VALIDATE(pb_at >= 0, null_str);
	VALIDATE(iot_src == nposm && src_evt == nposm, null_str);
	VALIDATE(zerotz_t == nposm, null_str);
	VALIDATE(IS_VALID_APLT_TASK_VAR_AT(var_at), null_str);
	VALIDATE(timed_at == nposm, null_str);
	VALIDATE(IS_VALID_APLT_TASK_AUX_KEY_ID(aux_key_id), null_str);
}

// for ttimed_aplt_task
taplt_task::taplt_task(const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
	const std::string& position1, const std::string& position2, int state, int pb_at, int zerotz_t, int timed_at, int aux_key_id)
	: type(type_timed)
	, priority(priority_timed)
	, aplt_id(aplt_id)
	, task_id(task_id)
	, ble_device_id(ble_device_id)
	, position1(position1)
	, position2(position2)
	, state(state)
	, pb_at(pb_at)
	, fails(0)
	, last_shedule_ticks(0)
	, iot_src(nposm)
	, src_evt(nposm)
	, zerotz_t(zerotz_t)
	, var_at(nposm)
	, timed_at(timed_at)
	, aux_key_id(aux_key_id)
{
	VALIDATE(pb_at >= 0, null_str);
	VALIDATE(iot_src == nposm && src_evt == nposm, null_str);
	VALIDATE(IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(zerotz_t), null_str);
	VALIDATE(var_at == nposm, null_str);
	VALIDATE(IS_VALID_APLT_TASK_TIMED_AT(timed_at), null_str);
	VALIDATE(IS_VALID_APLT_TASK_AUX_KEY_ID(aux_key_id), null_str);
}

bool taplt_task_key::operator<(const taplt_task_key& that) const noexcept
{
	VALIDATE(type == that.type, null_str);
/*
	VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
	VALIDATE(IS_VALID_APLT_TASK_PRIORITY(that.priority), null_str);

	if (priority != that.priority) {
		return priority > that.priority;
	}
*/
	if (type == taplt_task::type_iot) {
		// When judging equal, state is not considered.
		if (iot_src != that.iot_src) {
			return iot_src < that.iot_src;
		}
		if (src_evt != that.src_evt) {
			return src_evt < that.src_evt;
		}
		int cmp = SDL_strcmp(src_device_id.c_str(), that.src_device_id.c_str());
		return cmp < 0;

	} else if (type == taplt_task::type_speech) {
		// When judging equal, state is not considered.
		VALIDATE(!speech_id.empty() && !that.speech_id.empty(), null_str);
		int cmp = SDL_strcmp(speech_id.c_str(), that.speech_id.c_str());
		return cmp < 0;

	} else if (type == taplt_task::type_var) {
		// When judging equal, state is not considered.
/*
		if (var_at != that.var_at) {
			return var_at < that.var_at;
		}
*/
		if (aux_key_id != that.aux_key_id) {
			return aux_key_id < that.aux_key_id;
		}
		return false;
		// int cmp = SDL_strcmp(aplt_id.c_str(), that.aplt_id.c_str());
		// if (cmp != 0) {
		//	return cmp < 0;
		// }
		// cmp = SDL_strcmp(task_id.c_str(), that.task_id.c_str());
		// VALIDATE(ble_device_id.empty() && that.ble_device_id.empty(), null_str);
		// return cmp < 0;

	} else if (type == taplt_task::type_timed) {
		// When judging equal, state is not considered.
		if (aux_key_id != that.aux_key_id) {
			return aux_key_id < that.aux_key_id;
		}
		return false;

		int cmp = SDL_strcmp(aplt_id.c_str(), that.aplt_id.c_str());
		if (cmp != 0) {
			return cmp < 0;
		}
		cmp = SDL_strcmp(task_id.c_str(), that.task_id.c_str());
		if (cmp != 0) {
			return cmp < 0;
		}
		cmp = SDL_strcmp(ble_device_id.c_str(), that.ble_device_id.c_str());
		return cmp < 0;

	}

	VALIDATE(false, "must not goto here");
	return false;
}

taplt_task_key aplt_task_key_from_aplt_task(const taplt_task& task)
{
	if (task.type == taplt_task::type_iot) {
		return taplt_task_key(task.iot_src, task.src_evt, task.src_device_id);

	} else if (task.type == taplt_task::type_speech) {
		return taplt_task_key(task.speech_id);

	} else if (task.type == taplt_task::type_var) {
		return taplt_task_key(task.aux_key_id);
	}

	VALIDATE(task.type == taplt_task::type_timed, null_str);
	return taplt_task_key(task.aplt_id, task.task_id, task.ble_device_id, task.aux_key_id);
}

int next_aux_key_id(const std::map<taplt_task_key, taplt_task>& tasks)
{
	std::set<int> existed;
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		existed.insert(it->first.aux_key_id);
	}

	int id = 0;
	while (true) {
		if (existed.count(id) == 0) {
			break;
		}
		id ++;
	}
	return id;
}

bool taplt_task::equal8(const aplt::taplt_task& that) const
{
	if (type == type_iot) {
		return iot_src == that.iot_src && src_evt == that.src_evt && src_device_id == that.src_device_id && equal7(that);

	} else if (type == type_speech) {
		return speech_id == that.speech_id && equal7(that);

	} else if (type == type_timed) {
		return zerotz_t == that.zerotz_t && timed_at == that.timed_at && equal7(that);

	} else if (type == type_var || type == type_timed) {
		return var_at == that.var_at && equal7(that);
	}

	VALIDATE(type == type_temp, null_str);
	VALIDATE(false, "must ont call it");
	return false;
}

void taplt_task::to_cfg(config& cfg) const
{
	config& subcfg = cfg.add_child("aplt_task");
	subcfg["type"] = aplt::aplt_task_types.find(type)->second;
	subcfg["priority"].from_int(priority);
	subcfg["aplt_id"].from_string(aplt_id, true);
	subcfg["task_id"].from_string(task_id, true);
	subcfg["ble_device_id"].from_string(ble_device_id, true);
	subcfg["position1"].from_string(position1, true);
	subcfg["position2"].from_string(position2, true);

	if (type == aplt::taplt_task::type_iot) {
		// only for tiot_aplt_task
		subcfg["iot_src"] = aplt::iot_sources.find(iot_src)->second.id;
		subcfg["src_evt"] = aplt::iot_events.find(src_evt)->second.id;
		subcfg["src_device_id"].from_string(src_device_id, true);

	} else if (type == aplt::taplt_task::type_speech) {
		// only for tspeech_aplt_task
		subcfg["speech_id"].from_string(speech_id, true);

	} else if (type == aplt::taplt_task::type_var) {
		// only for tvar_aplt_task
		subcfg["var_at"].from_int(var_at);

	} else {
		// only for ttiming_aplt_task
		subcfg["zerotz_t"].from_int(zerotz_t);
		subcfg["timed_at"].from_int(timed_at);
	}
}

const std::map<int, std::string> aplt_task_types = {{aplt::taplt_task::type_temp, "temp"},
	{aplt::taplt_task::type_iot, "iot"}, 
	{aplt::taplt_task::type_speech, "speech"},
	{aplt::taplt_task::type_var, "var"},
	{aplt::taplt_task::type_timed, "timed"},
};

int aplt_task_type_from_str(const std::string& str)
{
	VALIDATE(!aplt_task_types.empty(), null_str);

	for (std::map<int, std::string>::const_iterator it = aplt_task_types.begin(); it != aplt_task_types.end(); ++ it) {
		const std::string& type = it->second;
		if (type == str) {
			return it->first;
		}
	}
	return nposm;
}

std::string tiot_device::alias2() const
{
	if (!alias.empty()) {
		return alias;
	}

	// std::string result = device_id;
	std::string result = utils::truncate_to_max_chars2(device_id, MAX_NORMAL_UTF8_NAME_CHARS - 1, true);
	return result;
}

bool tiot_device::operator<(const tiot_device& that) const noexcept
{
	if (src != that.src) {
		// if src isn't same, less src is before.
		return src > that.src;
	}

	if (!alias.empty()) {
		if (!that.alias.empty()) {
			if (number != nposm && that.number != nposm && number != that.number) {
				VALIDATE(number > 0 && that.number > 0, null_str);
				VALIDATE(alias.size() >= MIN_ROSE_IOT_ALIAS_SIZE && that.alias.size() >= MIN_ROSE_IOT_ALIAS_SIZE, null_str);
				const char* c_str = alias.c_str();
				const char* that_c_str = that.alias.c_str();
				if (c_str[0] == that_c_str[0] && c_str[1] == that_c_str[1]) {
					return number < that.number;
				}
			}
			int cmp = SDL_strcmp(alias.c_str(), that.alias.c_str());
			return cmp < 0;
		} else {
			return true;
		}
	} else if (!that.alias.empty()) {
		return false;
	}

	return SDL_strcmp(device_id.c_str(), that.device_id.c_str()) < 0;
}

bool split_iot_alias(const std::string& alias, const std::string& id, int* num_ptr, std::string* name_ptr)
{
	VALIDATE(id.size() == ROSE_IOT_ALIAS_ID_SIZE, null_str);
	int size = alias.size();
	if (size < MIN_ROSE_IOT_ALIAS_SIZE) {
		// TB1-
		return false;
	}
	const char* c_str = alias.c_str();
	const char* id_c_str = id.c_str();
	if (c_str[0] != id_c_str[0] || c_str[1] != id_c_str[1]) {
		return false;
	}
	const char connector = '-';
	int num = 0;
	int at = 2;
	for (; at < size; at ++) {
		char ch = c_str[at];
		if (ch >= '0' && ch <= '9') {
			num = num * 10 + ch - '0';
		} else if (num != 0) {
			if (c_str[at] != connector) {
				return false;
			}
			at ++;
			break;
			
		} else {
			// num == 0
			return false;
		}
	}

	if (at == size) {
		// name must not empty.
		return false;
	}

	if (num_ptr != nullptr) {
		*num_ptr = num;
	}
	if (name_ptr != nullptr) {
		*name_ptr = alias.substr(at);
	}
	return true;
}

bool is_valid_iot_alias_id(const std::string& id)
{
	int id_size = id.size();
	if (id_size != 2) {
		return false;
	}
	const char* id_c_str = id.c_str();
	return id_c_str[0] >= 'A' && id_c_str[0] <= 'Z' && id_c_str[1] >= 'A' && id_c_str[1] <= 'Z';
}

std::string revise_iot_alias_id(const std::string& id, const std::string& def)
{
	VALIDATE(is_valid_iot_alias_id(def), null_str);
	return is_valid_iot_alias_id(id)? id: def;
}

tbase_scene::tbase_scene()
	: file_key("file")
{
	clear();
}

std::string tbase_scene::get_id(bool exclude_wkocourse_id2) const
{
	if (!valid()) {
		return null_str;
	}

	char buf[512];
	if (wkocourse_id2.empty() || exclude_wkocourse_id2) {
		SDL_snprintf(buf, sizeof(buf), "%s__%s__%s", aplt.c_str(), task.c_str(), join_input_vars().c_str());

	} else {
		SDL_snprintf(buf, sizeof(buf), "%s__%s__%s__%s", 
			aplt.c_str(), task.c_str(), join_input_vars().c_str(), wkocourse_id2.c_str());
	}

	return buf;
}

void tbase_scene::set_name(const std::string& name, bool allow_empty)
{
	if (!allow_empty) {
		VALIDATE(!name.empty(), null_str);
	}
	if (name != name_) {
		name_ = name;

		const int tone = PINYIN_DEF_TONE;
		const bool eng_lowercase = PINYIN_DEF_ENG_LOWERCASE;

		py_name = aplt::get_curr_pinyin().from_utf8str2(name, tone, eng_lowercase);
	}
}
/*
std::string tbase_scene::id_for_gui(int _max_chars) const
{
	if (game_config::app_code == app_launcher) {
		return id;
	}

	if (id.empty()) {
		return id;
	}
	const int max_chars = _max_chars == nposm? 15: _max_chars;
	return utils::truncate_to_max_chars2(id, max_chars, true);
}
*/
std::string tbase_scene::name_for_gui(int _max_chars) const
{
	if (game_config::app_code == app_launcher) {
		return name_;
	}

	if (name_.empty()) {
		return name_;
	}
	const int max_chars = _max_chars == nposm? 11: _max_chars; // 12
	return utils::truncate_to_max_chars2(name_, max_chars, true);
}

std::string tbase_scene::file_var_val() const
{
	std::string file;

	if (input_vars.count(file_key) != 0) {
		file = input_vars.find(file_key)->second;
	}
	return file;
}

uint64_t join_log_tokens(int input, int output, uint16_t flags)
{
	VALIDATE(input >= 0 && output >= 0, null_str);
	return posix_mku64(posix_mku32(input, output), posix_mku32(0, flags));
}

uint16_t split_log_tokens(uint64_t tokens, int* input_ptr, int* output_ptr)
{
	uint32_t lo32 = posix_lo32(tokens);
	uint32_t hi32 = posix_hi32(tokens);

	if (input_ptr != nullptr) {
		*input_ptr = posix_lo16(lo32);
	}
	if (output_ptr != nullptr) {
		*output_ptr = posix_hi16(lo32);
	}
	return posix_hi16(hi32);
}

//
// twkocourse_enroll
//
void twkocourse_enroll::to_cfg(config& cfg) const
{
	VALIDATE(valid(), null_str);

	cfg["id"].from_string(id, true);
	cfg["aplt"].from_string(aplt, true);

	cfg["purchase"].from_int64(purchase);

	if (active != nposm) {
		cfg["active"].from_int64(active);
	}
}

bool twkocourse_enroll::from_cfg(const config& cfg)
{
	clear();

	tauto_destruct_executor destruct_executor(std::bind(&twkocourse_enroll::clear, this));

	id = cfg["id"].str();
	if (!isvalid_normal_id_or_var_name224(id)) {
		return false;
	}
	aplt = cfg["aplt"].str();
	if (!is_bundleid(aplt)) {
		return false;
	}

	purchase = cfg["purchase"].to_int64(nposm);
	if (purchase == nposm) {
		return false;
	}

	active = cfg["active"].to_int64(nposm);

	id2 = utils::join_app_prefix_id(aplt, id);
	destruct_executor.cancel_execute();
	return true;
}

void twkocourse_enroll::do_purchase(const std::string& _aplt, const std::string& _id)
{
	// aplt::twkocourse_enroll& enroll = ins.first->second;
	VALIDATE(is_bundleid(_aplt), null_str);
	VALIDATE(!_id.empty(), null_str);

	clear();

	id = _id;
	aplt = _aplt;
	id2 = utils::join_app_prefix_id(aplt, id);
	purchase = time(nullptr);
}

void twkocourse_enroll::do_active()
{
	// aplt::twkocourse_enroll& enroll = ins.first->second;
	VALIDATE(valid(), null_str);
	VALIDATE(active == nposm, null_str);

	active = time(nullptr);
}

}