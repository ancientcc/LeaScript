#define GETTEXT_DOMAIN "rose-lib"

#include "rose_var.hpp"
#include "gettext.hpp"
#include "rose_string_utils_dll.hpp"
#include <SDL_log.h>
#include <SDL_thread.h>

#include "aplt_clazz.hpp"
#include <boost/foreach.hpp>

#include "rose_config_3rdparty.hpp"


static bool_set_t bool_set_type_from_id(const std::string& id, bool* is_invalid)
{
	VALIDATE(!bool_set_types.empty(), null_str);

	if (is_invalid != nullptr) {
		*is_invalid = false;
	}

	for (std::map<bool_set_t, tcode3>::const_iterator it = bool_set_types.begin(); it != bool_set_types.end(); ++ it) {
		const tcode3& type = it->second;
		if (type.id == id) {
			return it->first;
		}
	}

	if (is_invalid != nullptr) {
		*is_invalid = true;
	}

	return bool_set_none;
}

namespace aplt {

std::map<var_type_t, tcode3> var_types;

bool is_aplt_task_var_type(var_type_t type)
{
	return type == var_type_bool || type == var_type_integer || type == var_type_double ||
		type == var_type_string || type == var_type_array;
}

int BI_env_var_last_2_normal(int last_var_type)
{
	VALIDATE(var_type_is_BI_env_last(last_var_type), null_str);

	int normal_type = last_var_type - aplt::BI_env_last_var_min;
	if (BI_env_normal_var_is_hour24_time(last_var_type)) {
		normal_type = aplt::var_env_hour24_time;
	}
	return normal_type;
}

std::map<int, tcode3> BI_env_vars;
std::map<int, tcode3> BI_task_vars;
std::map<std::string, int> BI_env_var_id_2_types;
std::map<std::string, int> BI_var_id_2_types;

const tcode3& builtin_var(int var)
{
	VALIDATE(var >= 0 && var < BI_var_count, null_str);
	if (var_type_is_BI_task(var)) {
		VALIDATE(BI_task_vars.count(var) != 0, null_str);
		return BI_task_vars.find(var)->second;
	}

	VALIDATE(BI_env_vars.count(var) != 0, null_str);
	return BI_env_vars.find(var)->second;
}

std::map<int, tcode3> BI_ble_vars;

const std::string ble_builtin_var_name(const std::string& bundleid, int type)
{
	VALIDATE(BI_ble_vars.count(type) != 0, null_str);
	if (bundleid.empty()) {
		return BI_ble_vars.find(type)->second.id;
	}
	return utils::join_app_prefix_id(bundleid, BI_ble_vars.find(type)->second.id);
}

//
// ttask_vars
//

std::string ttask_var::valx_2_str() const
{
	if (!is_array) {
		return val.str();
	}

	const std::string s = ",";
    std::stringstream str;
    for (std::vector<config::attribute_value>::const_iterator it = vals.begin(); it != vals.end(); ++ it) {
		const config::attribute_value& val = *it;
		if (it != vals.begin()) {
			str << s;
		}
        str << val.str();
    }

    return str.str();
}

static void update_volatile_var(ttask_var& var)
{
	if (var.type == var_env_time) {
		var.val.from_int64(time(nullptr));

	} else if (var.type == var_env_hour24_time) {
		int hour24_time = utils::calculate_hour24_time(time(nullptr));
		var.val.from_int64(hour24_time);
	}
}

const ttask_var& ttask_vars::get_var(const std::string& name) const
{
	VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	const ttask_var& var = data_.find(name)->second;
	if (var.type != nposm) {
		ttask_var* mutable_var = const_cast<ttask_var*>(&var);
		update_volatile_var(*mutable_var);
	}
	return var;
}
/*
ttask_var& ttask_vars::get_mutable_var(const std::string& name)
{
	VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	ttask_var& var = data_.find(name)->second;
	if (var.type != nposm) {
		update_volatile_var(var);
	}
	return var;
}
*/
bool ttask_vars::insert_th(const std::string& name, bool is_array, int type)
{
	VALIDATE_IN_MAIN_THREAD();

	bool inserted = true;
	VALIDATE(!name.empty(), null_str);
	if (aplt::BI_env_var_id_2_types.count(name) != 0) {
		VALIDATE(type != nposm, null_str);
	} else {
		VALIDATE(type == nposm, null_str);
	}
	if (data_.count(name) == 0) {
		inserted = true;

		VALIDATE(symbols_.count(name) == 0, null_str);
		std::pair<std::map<std::string, aplt::ttask_var>::iterator, bool> ins = data_.insert(std::make_pair(name, aplt::ttask_var(name, is_array, type)));
		VALIDATE(ins.second, null_str);

	} else {
		VALIDATE(symbols_.count(name) != 0, null_str);
		const ttask_var& var = data_.find(name)->second;
		// const ttask_var& var = get_var(name);
		VALIDATE(var.is_array == is_array, null_str);
		VALIDATE(var.type == type, null_str);

		inserted = false;
	}

	return inserted;
}

void ttask_vars::insert_bool(const std::string& name, bool is_array, bool val, int type)
{
	VALIDATE(aplt::BI_env_var_id_2_types.count(name) == 0 && type == nposm, null_str);

	insert_th(name, is_array, type);
	aplt::ttask_var& var = data_.find(name)->second;
	if (!is_array) {
		var.val.from_bool(val);
	} else {
		var.vals.push_back(config::attribute_value());
		var.vals.back().from_bool(val);
	}
	symbols_[name] = var.valx_2_str();
}

void ttask_vars::insert_integer(const std::string& name, bool is_array, int64_t val, int type)
{
	VALIDATE(aplt::BI_env_var_id_2_types.count(name) == 0 && type == nposm, null_str);

	insert_th(name, is_array, type);
	aplt::ttask_var& var = data_.find(name)->second;
	// aplt::ttask_var& var = get_mutable_var(name);
	if (!is_array) {
		var.val.from_int64(val);
	} else {
		var.vals.push_back(config::attribute_value());
		var.vals.back().from_int64(val);
	}
	symbols_[name] = var.valx_2_str();
}

void ttask_vars::insert_double(const std::string& name, bool is_array, double val)
{
	insert_th(name, is_array, nposm);
	aplt::ttask_var& var = data_.find(name)->second;
	if (!is_array) {
		var.val.from_double(val);
	} else {
		var.vals.push_back(config::attribute_value());
		var.vals.back().from_double(val);
	}
	symbols_[name] = var.valx_2_str();
}

void ttask_vars::insert_string(const std::string& name, bool is_array, const std::string& val)
{
	insert_th(name, is_array, nposm);
	aplt::ttask_var& var = data_.find(name)->second;
	if (!is_array) {
		var.val.from_string(val, true);
	} else {
		var.vals.push_back(config::attribute_value());
		var.vals.back().from_string(val, true);
	}
	symbols_[name] = var.valx_2_str();
}

void ttask_vars::insert_attribute(const std::string& name, bool is_array, const config::attribute_value& val, int type)
{
	insert_th(name, is_array, type);
	aplt::ttask_var& var = data_.find(name)->second;
	if (!is_array) {
		var.val = val;
	} else {
		var.vals.push_back(val);
	}
	symbols_[name] = var.valx_2_str();
}

void ttask_vars::erase(const std::string& name)
{
	VALIDATE(!name.empty(), null_str);
	if (data_.count(name) != 0) {
		VALIDATE(symbols_.count(name) != 0, null_str);

		data_.erase(name);
		symbols_.erase(name);

	} else {
		VALIDATE(symbols_.count(name) == 0, null_str);
	}
}

var_type_t ttask_vars::type(const std::string& name) const
{
	VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	const ttask_var& var = data_.find(name)->second;
	VALIDATE(!var.is_array, null_str);
	return var.val.type();
}

bool ttask_vars::get_bool(const std::string& name) const
{
	// VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	// const aplt::ttask_var& var = data_.find(name)->second;
	const aplt::ttask_var& var = get_var(name);
	VALIDATE(!var.is_array, null_str);
	return var.val.to_bool2();
}

int ttask_vars::get_int(const std::string& name) const
{
	// VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	// const aplt::ttask_var& var = data_.find(name)->second;
	const aplt::ttask_var& var = get_var(name);
	VALIDATE(!var.is_array, null_str);
	return var.val.to_int2();
}

int ttask_vars::get_int64(const std::string& name) const
{
	// VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	// const aplt::ttask_var& var = data_.find(name)->second;
	const aplt::ttask_var& var = get_var(name);
	VALIDATE(!var.is_array, null_str);
	return var.val.to_int64_2();
}

double ttask_vars::get_double(const std::string& name) const
{
	// VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	// const aplt::ttask_var& var = data_.find(name)->second;
	const aplt::ttask_var& var = get_var(name);
	VALIDATE(!var.is_array, null_str);
	return var.val.to_double();
}

std::string ttask_vars::get_string(const std::string& name) const
{
	// VALIDATE(!name.empty() && data_.count(name) != 0, null_str);
	// const aplt::ttask_var& var = data_.find(name)->second;
	const aplt::ttask_var& var = get_var(name);
	VALIDATE(!var.is_array, null_str);
	return var.val.str();
}

ttask_vars env_vars;

void init_env_vars()
{
	VALIDATE(!BI_env_vars.empty() && env_vars.empty(), null_str);

	config::attribute_value val;

	int64_t ts = time(nullptr);
	const int hour24_time = utils::calculate_hour24_time(ts);
	val.from_int64(ts);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_time).id, false, val, var_env_time);
	val.from_int64(hour24_time);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_hour24_time).id, false, val, var_env_hour24_time);
	// both xxx and xxx_last, initial value of ineteger is 0.

	val.from_int64(ts);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_time_last).id, false, val, var_env_time_last);

	val.from_int64(hour24_time);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_hour24_time1_last).id, false, val, var_env_hour24_time1_last);
	val.from_int64(hour24_time);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_hour24_time2_last).id, false, val, var_env_hour24_time2_last);
	val.from_int64(hour24_time);
	env_vars.insert_attribute(aplt::builtin_var(aplt::var_env_hour24_time3_last).id, false, val, var_env_hour24_time3_last);

	VALIDATE(env_vars.size() == 2 + 1 + 3, null_str);
}

ttask_vars clone_env_vars(bool only_normal)
{
	if (!only_normal) {
		return env_vars;
	}

	ttask_vars result;
	const std::map<std::string, ttask_var>& data = env_vars.data();
	for (std::map<std::string, ttask_var>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		const ttask_var& var = it->second;
		VALIDATE(!var.is_array, null_str);
		if (!var_type_is_BI_env_normal(var.type)) {
			continue;
		}
		result.insert_attribute(var.name, var.is_array, var.val, var.type);
	}
	return result;
}

const ttask_var* get_env_var(const std::string& name)
{
	if (!env_vars.existed(name)) {
		return nullptr;
	}
	const ttask_var& result = env_vars.get_var(name);
	return &result;
}

void set_env_var(int type, const config::attribute_value& val)
{
	// const ttask_var* var = get_env_var(name);
	// VALIDATE(var != nullptr, null_str);

	VALIDATE(var_type_is_BI_env(type), null_str);
	VALIDATE(!BI_var_is_auto_update(type), null_str);

	if (BI_var_val_type_is_bool(type)) {
		VALIDATE(val.type() == var_type_bool, null_str);

	} else if (BI_var_val_type_is_string(type)) {
		VALIDATE(val.type() == var_type_string, null_str);

	} else {
		VALIDATE(val.type() == var_type_integer, null_str);
	}

	env_vars.insert_attribute(BI_env_vars.find(type)->second.id, false, val, type);
}

// a.b.c__a
#define MIN_APLT_VAR_NAME_SIZE	8

bool is_var_name(const std::string& str, const ttask_vars* task_vars, std::string* var_name)
{
	if (var_name != nullptr) {
		var_name->clear();
	}

	int size = str.size();
	// a.b.c__a
	if (size < MIN_APLT_VAR_NAME_SIZE) {
		return false;
	}
	const char* c_str = str.c_str();
	if (c_str[0] != '$') {
		return false;
	}

	std::string str2 = str.substr(1);
	std::pair<std::string, std::string> pair = utils::split_app_prefix_id(str2);
	const std::string& bundleid = pair.first;
	const std::string& name = pair.second;

	if (bundleid.empty() || name.empty()) {
		return false;
	}
	if (!utils::is_rose_bundleid(bundleid, '.')) {
		return false;
	}

	if (task_vars != nullptr) {
		if (!task_vars->existed(str2)) {
			return false;
		}
	}

	if (var_name != nullptr) {
		*var_name = str.substr(1);
	}
	return true;
}

int var_exp_which_type(const std::string& str, std::string* no_symboled_str)
{
	if (no_symboled_str != nullptr) {
		no_symboled_str->clear();
	}

	int size = str.size();
	// $(a)
	const int min_func_statement_size = 4; // MIN_APLT_VAR_NAME_SIZE > (it) always
	const char* c_str = str.c_str();
	if (size < min_func_statement_size || c_str[0] != '$') {
		return nposm;
	}

	int type = nposm;
	if (c_str[1] == '(') {
		if (c_str[size - 1] != ')') {
			return nposm;
		}
		type = var_exp_type_func;
	} else {
		if (size < MIN_APLT_VAR_NAME_SIZE) {
			return nposm;
		}
		type = var_exp_type_var;
	}
	
	if (no_symboled_str != nullptr) {
		if (type == var_exp_type_var) {
			*no_symboled_str = str.substr(1);
		} else {
			VALIDATE(type == var_exp_type_func, null_str);
			*no_symboled_str = str.substr(2, size - 3);
		}
	}

	return type;
}

const config::attribute_value* var_val_from_no_symboled_str(const ttask_vars& task_vars, int type, const std::string& no_symboled_str, tfunction::tresult& result)
{
	const config::attribute_value* var_val = nullptr;
	if (type == var_exp_type_func) {
		result = curr_func->calculate(task_vars, no_symboled_str);
		if (!result.is_ok()) {
			return nullptr;
		}
		VALIDATE(result.val.type() != var_type_nposm, null_str);

		var_val = &result.val;

	} else {
		if (!task_vars.existed(no_symboled_str)) {
			// below 'op' requrie var must be existed. since this var isn't existed, think false always.
			return nullptr;
		}
		const ttask_var& var2 = task_vars.get_var(no_symboled_str);
		var_val = &var2.val;
	}
	return var_val;
}

int split_no_symboled_func_exp(const std::string& str, std::vector<std::string>& params)
{
	size_t pos = str.find(',');
	if (pos == std::string::npos) {
		return nposm;
	}
	std::string func_id = str.substr(0, pos);
	utils::strip(func_id);
	if (aplt::function_keys.count(func_id) == 0) {
		return nposm;
	}

	int func_code = aplt::function_keys.find(func_id)->second;
	VALIDATE(aplt::functions.count(func_code) != 0, null_str);

	std::string param_str = str.substr(pos + 1);
	if (!utils::is_empty_or_all_portable_space(param_str)) {
		params = utils::split(param_str, ',', utils::STRIP_SPACES);
	} else {
		params.clear();
	}
	return func_code;
}

int split_var_exp_4_func(const std::string& str, std::vector<std::string>& params)
{
	std::string no_symboled_str;
	if (aplt::var_exp_which_type(str, &no_symboled_str) != aplt::var_exp_type_func) {
		return nposm;
	}
	return split_no_symboled_func_exp(no_symboled_str, params);
}

std::string var_exp_for_browser(const std::string& var_exp)
{
	std::vector<std::string> params;
	int func_code = split_var_exp_4_func(var_exp, params);
	if (func_code != nposm) {
		const tfunction_code& func = functions.find(func_code)->second;
		return curr_func->func_2_var_exp(func, params, true);
	}
	return var_exp;
}

std::map<int, tfunction_code> functions;
std::map<std::string, int> function_keys;
tfunction* curr_func = nullptr;

tfunction::tfunction()
{
	VALIDATE(curr_func == nullptr, null_str);
	VALIDATE((int)aplt::functions.size() == aplt::tfunction::func_builtin_count, null_str);

	curr_func = this;
}

tfunction::~tfunction()
{
	VALIDATE(curr_func != nullptr, null_str);
	curr_func = nullptr;
}

tfunction::tresult tfunction::calculate(const aplt::ttask_vars& task_vars, const std::string& no_symboled_str)
{
	// At least until now, no function is supported without parameters
	VALIDATE(!no_symboled_str.empty(), null_str);

	tresult result;

	std::vector<std::string> params;
	int func_code = split_no_symboled_func_exp(no_symboled_str, params);
	if (func_code == nposm) {
		result.set_err_novar(err_no_func);
		return result;
	}

	const tfunction_code& func = functions.find(func_code)->second;
	if (params.size() != func.params.size()) {
		result.set_err_novar(err_param_count);
		return result;
	}

	if (calculate2(task_vars, func_code, params, result)) {
		return result;
	}

	// 
	std::string var0_name;
	if (!params.empty()) {
		is_var_name(params[0], &task_vars, &var0_name);
	}

	const ttask_var* var0 = nullptr;
	if (!var0_name.empty()) {
		var0 = &task_vars.get_var(var0_name);
	}

	if (func_code == func_add || func_code == func_sub || func_code == func_mul || func_code == func_div) {
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		const int var_type = var0->val.type();
		if (var0->is_array || (var_type != var_type_integer && var_type != var_type_double)) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}
		int operand = ::utils::to_int(params[1]);
		int val = 0;
		if (func_code == func_add) {
			val = var0->val.to_int() + operand;
		} else if (func_code == func_sub) {
			val = var0->val.to_int() - operand;
		} else if (func_code == func_mul) {
			val = var0->val.to_int() * operand;
		} else {
			// func_code == func_div
			val = var0->val.to_int() / operand;
		}
		result.val.from_int(val);

	} else if (func_code == func_inv_bool) {
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		const int var_type = var0->val.type();
		if (var0->is_array || var_type != var_type_bool) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		result.val.from_bool(!var0->val.to_bool());

	} else if (func_code == func_size) {
		// $(size, $aplt.leagor.khome__ids)
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		if (!var0->is_array) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		SDL_Log("func_size: (int)%i", (int)var0->vals.size());
		result.val.from_int(var0->vals.size());

	} else if (func_code == func_sum) {
		// $(size, $aplt.leagor.khome__ids)
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		if (!var0->is_array) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		int64_t int64_val = nposm;
		double double_val = float_nposm;

		for (std::vector<config::attribute_value>::const_iterator it = var0->vals.begin(); it != var0->vals.end(); ++ it) {
			const config::attribute_value& val = *it;
			if (val.type() == var_type_integer) {
				if (int64_val == nposm) {
					int64_val = 0;
				}
				int64_val += val.to_int64();

			} else if (val.type() == var_type_double) {
				if (is_float_nposm(double_val)) {
					double_val = 0;
				}
				double_val += val.to_double();
			} else {
				continue;
			}
		}

		if (int64_val == nposm) {
			int64_val = 0;
		}
		if (is_float_nposm(double_val)) {
			result.val.from_int64(int64_val);

		} else {
			result.val.from_double(double_val + int64_val);
		}

	} else if (func_code == func_element) {
		// $(element, $aplt.leagor.khome__ids, $aplt.launcher.fake__last_matched_index_)
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		if (!var0->is_array) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		int at = nposm;
		const std::string at_str = params[1];
		std::string at_var_name;
		is_var_name(at_str, nullptr, &at_var_name);
		if (!at_var_name.empty()) {
			if (!task_vars.existed(at_var_name)) {
				result.set_err_var_1var(err_not_exist_var, at_var_name);
				return result;
			}
			const ttask_var& at_var = task_vars.get_var(at_var_name);
			if (at_var.is_array || at_var.val.type() != var_type_integer) {
				result.set_err_var_1var(err_var_type_mismatch, at_var_name);
				return result;
			}
			at = at_var.val.to_int();
		} else {
			at = ::utils::to_int(params[1]);
		}
		if (at < 0 || at >= (int)var0->vals.size()) {
			result.set_err_var_1var(err_at_out_range, null_str);
			return result;
		}

		SDL_Log("func element: %s", var0->vals[at].str().c_str());

		result.val = var0->vals[at];

	} else if (func_code == func_join_element) {
		// $(join_element, $aplt.leagor.khome__descs, 1, prefix, postfix,  , false)
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		if (!var0->is_array) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		int base = ::utils::to_int(params[1]);
		const std::string& prefix = params[2];
		const std::string& postfix = params[3];
		const std::string& connector = params[4];
		bool add_comma = ::utils::to_bool(params[5]);

		std::stringstream ss;
		int at = 0;
		for (std::vector<config::attribute_value>::const_iterator it = var0->vals.begin(); it != var0->vals.end(); ++ it, at ++) {
			std::string str = it->str();
			if (at != 0) {
				ss << connector;
				if (add_comma) {
					ss << ',';
				}
			}
			ss << prefix << (base + at) << postfix << str;
		}

		SDL_Log("func_join_element: %s", ss.str().c_str());
		result.val.from_string(ss.str(), true);

	} else if (func_code == func_join_index) {
		// $(join_index, $(size, $aplt.leagor.khome__ids), 1, prefix, postfix, , true)
		const std::string& size_str = params[0];
		int size = ::utils::to_int(size_str);

		int base = ::utils::to_int(params[1]);
		const std::string& prefix = params[2];
		const std::string& postfix = params[3];
		const std::string& connector = params[4];
		bool add_comma = ::utils::to_bool(params[5]);

		std::stringstream ss;
		int at = 0;
		for (int at = 0; at < size; at ++) {
			if (at != 0) {
				ss << connector;
				if (add_comma) {
					ss << ',';
				}
			}
			ss << prefix << (base + at) << postfix;
		}

		SDL_Log("func_join_inex: %s", ss.str().c_str());
		result.val.from_string(ss.str(), true);

	} else if (func_code == func_join2_2element) {
		// $(join_element, $aplt.leagor.khome__var0, $aplt.leagor.khome__var1, prefix, middle, postfix, connector, false)
		std::string var1_name;
		is_var_name(params[1], &task_vars, &var1_name);

		const ttask_var* var1 = nullptr;
		if (!var1_name.empty()) {
			var1 = &task_vars.get_var(var1_name);
		}

		const ttask_var* vars[2] = {var0, var1};
		for (int at = 0; at < sizeof(vars) / sizeof(vars[0]); at ++) {
			const ttask_var* var = vars[at];
			if (var == nullptr) {
				result.set_err_var_1var(err_not_exist_var, params[2]);
				return result;
			}
			if (!var->is_array) {
				result.set_err_var_1var(err_var_type_mismatch, var0_name);
				return result;
			}
		}

		if (var0->vals.size() != var1->vals.size()) {
			const std::string& var_name = var0->vals.size() < var1->vals.size()? var0_name: var1_name;
			result.set_err_var_1var(err_at_out_range, var_name);
			return result;
		}

		const int max_size = ::utils::to_int(params[2]);

		const std::string& prefix = params[3];
		const std::string& middle = params[4];
		const std::string& postfix = params[5];
		const std::string& connector = params[6];
		bool add_comma = ::utils::to_bool(params[7]);

		std::stringstream ss;
		int at = 0;
		for (std::vector<config::attribute_value>::const_iterator it = var0->vals.begin(); it != var0->vals.end(); ++ it, at ++) {
			if (at >= max_size) {
				break;
			}
			const std::string val0_str = it->str();
			if (at != 0) {
				ss << connector;
				if (add_comma) {
					ss << ',';
				}
			}
			ss << prefix << val0_str << middle << var1->vals[at].str() << postfix;
		}

		SDL_Log("func_join2_2element: %s", ss.str().c_str());
		result.val.from_string(ss.str(), true);

	} else if (func_code == func_format_elapse_hms) {
		// $(format_elapse_hms, $aplt.leagor.khome__cooking_time)
		if (var0 == nullptr) {
			result.set_err_var_1var(err_not_exist_var, params[0]);
			return result;
		}
		if (!var0->is_array || var0->val.type() != var_type_integer) {
			result.set_err_var_1var(err_var_type_mismatch, var0_name);
			return result;
		}

		const std::string fmt = utils::format_elapse_hms(var0->val.to_int(), utils::timesep_i18n, true);

		SDL_Log("format_elapse_hms: elapse(%i) --> %s", var0->val.to_int(), fmt.c_str());
		result.val.from_string(fmt, true);

	} else if (func_code == func_is_speaking) {
		// $(is_speaking, )
		VALIDATE(var0 == nullptr, null_str);

		result.val.from_bool(aplt::get_curr_pinyin().is_speaking());

		// SDL_Log("format_elapse_hms: elapse(%i) --> %s", var0->val.to_int(), fmt.c_str());
		// result.val.from_string(fmt, true);

	} else {
		// Maybe this function is old and not supported in this version.
		result.set_err_novar(err_unsupport);
		return result;
	}
	
	return result;

}

std::string tfunction::func_2_var_exp(const aplt::tfunction_code& func, const std::vector<std::string>& params, bool /*browser*/) const
{
	VALIDATE(params.size() >= func.params.size(), null_str);

	std::stringstream ss;
	ss << "$(" << func.id;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = params.begin(); it != params.end(); ++ it, at ++) {
		// if at == 0, will separate function_id and first param.
		ss << ", ";
		std::string val = *it;
		utils::strip(val);
		ss << val;
	}
	if (params.empty()) {
		// Even when there are no parameters, the English comma "," is still required.
		ss << ",";
	}
	ss << ")";
	return ss.str();
}

std::map<int, tcode3> if_judge_ops;
std::map<int, tcode3> if_logic_ops;

int if_judge_op_from_id(const std::string& id)
{
	VALIDATE(!if_judge_ops.empty(), null_str);

	for (std::map<int, tcode3>::const_iterator it = if_judge_ops.begin(); it != if_judge_ops.end(); ++ it) {
		const tcode3& type = it->second;
		if (type.id == id) {
			return it->first;
		}
	}

	return nposm;
}

int if_logic_op_from_id(const std::string& id)
{
	VALIDATE(!if_logic_ops.empty(), null_str);

	for (std::map<int, tcode3>::const_iterator it = if_logic_ops.begin(); it != if_logic_ops.end(); ++ it) {
		const tcode3& type = it->second;
		if (type.id == id) {
			return it->first;
		}
	}

	return nposm;
}

bool tif_judge::op_operand_is_string(int op)
{
	return op == op_string_equal;
}

bool tif_judge::op_operand_is_bool(int op)
{
	return op == op_bool_equal || op == op_bool_not_equal;
}

// numerical = integer + double
bool tif_judge::op_operand_is_numerical(int op)
{
	return op == op_numerical_equal || op == op_numerical_not_equal ||
		op == op_greater_than ||
		op == op_less_than ||
		op == op_greater_than_equal_to ||
		op == op_less_than_equal_to;
}

bool tif_judge::op_has_operand(int op)
{
	return op_operand_is_string(op) || op_operand_is_bool(op) || op_operand_is_numerical(op);
}
/*
bool r_exp_is_var_exp(const std::string& str)
{
 	return var_exp_which_type(str, nullptr) != nposm;
}
*/
tif_judge::tif_judge(int logic, int op, const std::string& var_exp, const std::string& r_exp)
	: logic(logic)
	, op(op)
	, var_exp(var_exp)
	, r_exp(r_exp)
{
	VALIDATE(logic == nposm || (logic >= 0 && logic < logic_count), null_str);
	VALIDATE(op >= 0 && op < op_count, null_str);
	VALIDATE(!var_exp.empty(), null_str);
	VALIDATE(!r_exp.empty(), null_str);
	VALIDATE(r_val.type() == var_type_nposm, null_str);
}

bool tif_judge::from_cfg(const config& cfg)
{
	logic = if_logic_op_from_id(cfg["logic"].str());
	op = if_judge_op_from_id(cfg["op"].str());
	if (op == nposm) {
		return false;
	}
	var_exp = cfg["var_exp"].str();

	r_exp.clear();
	r_val.set_nposm();
	if (op_has_operand(op)) {
		r_exp = cfg["r_exp"].str();
		if (str_is_var_exp(r_exp)) {
			r_val.set_nposm();

		} else {
			if (op_operand_is_string(op)) {
				r_val.from_string(r_exp, true);

			} else {
				r_val.from_string(r_exp, false);
				if (op_operand_is_bool(op)) {
					if (r_val.type() != var_type_bool) {
						r_val.from_bool(cfg["r_exp"].to_bool());
					}

				} else { 
					VALIDATE(op_operand_is_numerical(op), null_str);
					if (r_val.type() != var_type_integer && r_val.type() != var_type_double) {
						r_val.from_int64(cfg["r_exp"].to_int64());
					}
				}
			}
			r_exp.clear();
		}
	}

	return true;
}

void tif_judge::to_cfg(config& cfg) const
{
	cfg.clear();

	if (aplt::if_logic_ops.count(logic) != 0) {
		cfg["logic"].from_string(aplt::if_logic_ops.find(logic)->second.id, true);
	} else {
		VALIDATE(logic == nposm, null_str);
	}

	VALIDATE(aplt::if_judge_ops.count(op) != 0, null_str);
	cfg["op"].from_string(aplt::if_judge_ops.find(op)->second.id, true);

	VALIDATE(!var_exp.empty(), null_str);
	cfg["var_exp"].from_string(var_exp, true);

	if (op_has_operand(op)) {
		if (!use_r_val()) {
			cfg["r_exp"].from_string(r_exp, true);

		} else {
			if (op_operand_is_string(op)) {
				VALIDATE(r_val.type() == var_type_string, null_str);

			} else if (op_operand_is_bool(op)) { 
				VALIDATE(r_val.type() == var_type_bool, null_str);

			} else {
				VALIDATE(op_operand_is_numerical(op), null_str);
				VALIDATE(r_val.type() == var_type_integer || r_val.type() == var_type_double, null_str);
			}
			cfg["r_exp"] = r_val;
		}
	}
}

bool tif_judge::calculate(const ttask_vars& task_vars) const
{
	std::string no_symboled_str;
	int type = var_exp_which_type(var_exp, &no_symboled_str);

	if (op == op_var_exist) {
		return task_vars.existed(no_symboled_str);

	} else if (op == op_var_not_exist) {
		return !task_vars.existed(no_symboled_str);

	}

	if (type == nposm) {
		return false;
	}

	tfunction::tresult l_result;
	const config::attribute_value* var_val = var_val_from_no_symboled_str(task_vars, type, no_symboled_str, l_result);
	if (var_val == nullptr) {
		return false;
	}

	// parse right_exp
	const config::attribute_value* r_val2 = &r_val;
	tfunction::tresult r_result;
	if (!use_r_val()) {
		int type = var_exp_which_type(r_exp, &no_symboled_str);
		if (type == nposm) {
			return false;
		}
		r_val2 = var_val_from_no_symboled_str(task_vars, type, no_symboled_str, r_result);
		if (r_val2 == nullptr) {
			return false;
		}
	}
	VALIDATE(r_val2->type() != var_type_nposm, null_str);
	
	if (op == op_string_equal) {
		if (var_val->type() != var_type_string) {
			// I can't make sure 'var_val' has right type.
			return false;
		}
		return var_val->str() == r_val2->str();

	} else if (op == op_bool_equal || op == op_bool_not_equal) {
		if (var_val->type() != var_type_bool) {
			return false;
		}
		if (op == op_bool_equal) {
			return var_val->to_bool() == r_val2->to_bool();
		} else {
			return var_val->to_bool() != r_val2->to_bool();
		}

	} else {
		VALIDATE(op_operand_is_numerical(op), null_str);
		const int var_val_type = var_val->type();
		if (var_val_type != var_type_integer && var_val_type != var_type_double) {
			return false;
		}
		if (op == op_numerical_equal || op == op_numerical_not_equal) {
			if (var_val_type == var_type_integer) {
				if (op == op_numerical_equal) {
					return var_val->to_int64() == r_val2->to_int64();
				} else {
					return var_val->to_int64() != r_val2->to_int64();
				}
			} else {
				if (op == op_numerical_equal) {
					return var_val->to_double() == r_val2->to_double();
				} else {
					return var_val->to_double() != r_val2->to_double();
				}
			}

		} else if (op == op_greater_than) {
			if (var_val_type == var_type_integer) {
				return var_val->to_int64() > r_val2->to_int64();
			} else {
				return var_val->to_double() > r_val2->to_double();
			}

		} else if (op == op_less_than) {
			if (var_val_type == var_type_integer) {
				return var_val->to_int64() < r_val2->to_int64();
			} else {
				return var_val->to_double() < r_val2->to_double();
			}

		} else if (op == op_greater_than_equal_to) {
			if (var_val_type == var_type_integer) {
				return var_val->to_int64() >= r_val2->to_int64();
			} else {
				return var_val->to_double() >= r_val2->to_double();
			}

		} else {
			VALIDATE(op == op_less_than_equal_to, null_str);
			if (var_val_type == var_type_integer) {
				return var_val->to_int64() <= r_val2->to_int64();
			} else {
				return var_val->to_double() <= r_val2->to_double();
			}
		}
	}

	VALIDATE(false, null_str);
	return 0;
}

std::string tif_judge::to_string(bool unique) const
{
	VALIDATE(valid(), null_str);
	std::string logic_str;
	if (logic == logic_and) {
		logic_str = "&& ";

	} else if (logic == logic_or) {
		logic_str = "|| ";

	} else {
		VALIDATE(logic == nposm, null_str);
	}

	std::string var_exp2 = var_exp_for_browser(var_exp);
	std::string r_exp2 = r_exp;

	// std::string op_str;
	std::stringstream data_ss;
	if (op == op_var_exist) {
		// op_str = "var_exist";
		data_ss << "var_exist(" << var_exp2 << ")";

	} else if (op == op_var_not_exist) {
		// op_str = "var_not_exist";
		data_ss << "var_not_exist(" << var_exp2 << ")";

	} else if (op == op_string_equal) {
		// op_str = "string_equal";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_string, null_str);
			r_exp2 = r_val.str();
		}
		data_ss << var_exp2 << " str== \"" << r_exp2 << "\""; 
	
	} else if (op == op_bool_equal || op == op_bool_not_equal) {
		// op_str = "bool_equal";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_bool, null_str);
			r_exp2 = r_val.str();
		}
		if (op == op_bool_equal) {
			data_ss << var_exp2 << " bool== " << r_exp2;
		} else {
			data_ss << var_exp2 << " bool!= " << r_exp2;
		}
	
	} else if (op == op_numerical_equal || op == op_numerical_not_equal) {
		// op_str = "numerical_equal";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_integer, null_str);
			r_exp2 = r_val.str();
		}
		if (op == op_numerical_equal) {
			data_ss << var_exp2 << " num== " << r_exp2;
		} else {
			data_ss << var_exp2 << " num!= " << r_exp2;
		}

	} else if (op == op_greater_than) {
		// op_str = "greater_than";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_integer || r_val.type() == var_type_double, null_str);
			r_exp2 = r_val.str();
		}
		data_ss << var_exp2 << " > " << r_exp2;

	} else if (op == op_less_than) {
		// op_str = "less_than";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_integer || r_val.type() == var_type_double, null_str);
			r_exp2 = r_val.str();
		}
		data_ss << var_exp2 << " < " << r_exp2;

	} else if (op == op_greater_than_equal_to) {
		// op_str = "greater_than_equal_to";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_integer || r_val.type() == var_type_double, null_str);
			r_exp2 = r_val.str();
		}
		data_ss << var_exp2 << " >= " << r_exp2;

	} else {
		VALIDATE(op == op_less_than_equal_to, null_str);
		// op_str = "less_than_equal_to";
		if (use_r_val()) {
			VALIDATE(r_val.type() == var_type_integer || r_val.type() == var_type_double, null_str);
			r_exp2 = r_val.str();
		}
		data_ss << var_exp2 << " <= " << r_exp2;

	} 

	char buf[128];
	if (unique) {
		SDL_snprintf(buf, sizeof(buf), "%s%s", logic_str.c_str(), data_ss.str().c_str());

	} else {
		SDL_snprintf(buf, sizeof(buf), "%s(%s)", logic_str.c_str(), data_ss.str().c_str());
	}
	return buf;
}

bool tif_branch::from_cfg(int states, const config& cfg)
{
	do_to_state = cfg["do_to_state"].to_int(nposm);
	if (states != nposm) {
		if (do_to_state != nposm && do_to_state >= states) {
			return false;
		}
	}

	do_str = cfg["do_str"].str();
	bool is_invalid = true;
	do_bool_set = bool_set_type_from_id(cfg["do_bool_set"].str(), &is_invalid);

	do_map_vals.clear();
	if (cfg.has_child("map_vals")) {
		const config& map_vals_cfg = cfg.child("map_vals");
		for (const config::attribute& pair: map_vals_cfg.attribute_range()) {
			const std::string& key = pair.first;
			do_map_vals.insert(std::make_pair(pair.first, pair.second));
		}
	}

	bool fail = false;

	judges.clear();
	BOOST_FOREACH (const config& judge2_cfg, cfg.child_range("judge")) {
		judges.push_back(tif_judge());
		if (!judges.back().from_cfg(judge2_cfg)) {
			fail = true;
			break;
		}
	}

	return !fail;
}
/*
std::string tif_branch::do_to_string() const
{
	std::stringstream ss;
	VALIDATE(do_bool_set >= 0 && do_bool_set < bool_set_count, null_str);
	if (do_bool_set != bool_set_none) {
		ss << _("End task_cpp") << ": " << bool_set_types.find(do_bool_set)->second.name << "|";
	}
	ss << do_str;

	return ss.str();
}
*/
void tif_branch::to_cfg(int states, config& cfg) const
{
	cfg.clear();

	if (states != nposm) {
		VALIDATE(do_to_state < states, null_str);
	}

	cfg["do_to_state"].from_int(do_to_state);

	cfg["do_str"] = do_str;
	cfg["do_bool_set"].from_string(bool_set_types.find(do_bool_set)->second.id, true);

	if (!do_map_vals.empty()) {
		config& map_vals_cfg = cfg.add_child("map_vals");
		for (std::map<std::string, std::string>::const_iterator it = do_map_vals.begin(); it != do_map_vals.end(); ++ it) {
			const std::string& var_name = it->first;
			VALIDATE(!var_name.empty(), null_str);
			const std::string& var_val = it->second;
			map_vals_cfg[var_name].from_string(var_val, true);
		}
	}

	for (std::vector<tif_judge>::const_iterator it = judges.begin(); it != judges.end(); ++ it) {
		const tif_judge& judge = *it;

		config& judge_cfg = cfg.add_child("judge");
		judge.to_cfg(judge_cfg);
	}
}

std::string tif_branch::judges_to_string(int at) const
{
	if (judges.empty()) {
		if (at == 0) {
			return null_str;
		} else {
			return "else";
		}
	}
	std::stringstream ss;
	if (at == 0) {
		ss << "if (";
	} else if (at != 0) {
		ss << "else if (";
	}

	for (std::vector<tif_judge>::const_iterator it = judges.begin(); it != judges.end(); ++ it) {
		const tif_judge& if_judge = *it;
		if (it != judges.begin()) {
			ss << " ";
		}
		ss << if_judge.to_string(judges.size() == 1);
	}
	ss << ")";

	return ss.str();
}

bool tif_block::from_cfg(const std::string& name, int states, const config& cfg)
{
	clear();

	const config& sub_cfg = cfg.child(name);
	if (!sub_cfg) {
		return false;
	}

	bool fail = false;

	BOOST_FOREACH (const config& branch_cfg, sub_cfg.child_range("branch")) {
		branches.push_back(tif_branch());
		if (!branches.back().from_cfg(states, branch_cfg)) {
			fail = true;
			break;
		}
	}

	if (fail) {
		clear();
	}

	return !fail;
}

void tif_block::to_cfg(const std::string& name, int states, config& cfg) const
{
	VALIDATE(!name.empty(), null_str);

	// whay don't call cfg.clear()?
	//   'finished' is just one field under [state2], and the 'name', 'threshold_s' and more fields have been filled in before it is called. 
	//   Once @cfg is clear here, All the fields written before that were also cleared.
	//   If the caller wants a clean [if_block] config, it has to be call cfg.clear() for itself.
	// cfg.clear();

	config& sub_cfg = cfg.add_child(name);

	for (std::vector<tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;

		config& branch_cfg = sub_cfg.add_child("branch");
		branch.to_cfg(states, branch_cfg);
	}
}

tif_block::tresult tif_block::calculate(const ttask_vars& task_vars) const
{
	tresult result;

	int branch_at = 0;
	for (std::vector<tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it, branch_at ++) {
		const tif_branch& branch = *it;
		bool matches = true;

		int judge_at = 0;
		for (std::vector<tif_judge>::const_iterator judge_it = branch.judges.begin(); judge_it != branch.judges.end(); ++ judge_it, judge_at ++) {
			const tif_judge& judge = *judge_it;
			if (judge_at == 0) {
				matches = judge.calculate(task_vars);
			}
			if (judge.logic == tif_judge::logic_and) {
				matches = matches && judge.calculate(task_vars);

			} else if (judge.logic == tif_judge::logic_or) {
				matches = matches || judge.calculate(task_vars);
			}
		}

		if (matches) {
			result.is_ok = true;

			result.do_to_state = branch.do_to_state;

			result.do_str = branch.do_str;
			result.do_bool_set = branch.do_bool_set;

			result.do_map_vals = branch.do_map_vals;
			break;
		}
	}

	return result;
}

std::string tif_block::is_valid(bool allow_empty) const
{
	std::string err_msg;

	if (!allow_empty && branches.empty()) {
		err_msg = _("The if block must have at least one branch.");
		return err_msg;
	}

	utils::string_map symbols;
	const int branch_size = branches.size();
	int branch_at = 0;
	for (std::vector<tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it, branch_at ++) {
		symbols["branch_index"] = str_cast(branch_at + 1);
		const tif_branch& branch = *it;
		if (branch_at != branch_size - 1) {
			if (branch.judges.empty()) {
				err_msg = vgettext2("For $branch_index branch, it is not the last, and its judgment condition cannot be empty.", symbols);
				return err_msg;
			}
		} else {
			if (!branch.judges.empty()) {
				err_msg = _("The last branch's judgment condition must be empty.");
				return err_msg;
			}
		}

		int judge_at = 0;
		for (std::vector<tif_judge>::const_iterator judge_it = branch.judges.begin(); judge_it != branch.judges.end(); ++ judge_it, judge_at ++) {
			symbols["judge_index"] = str_cast(judge_at + 1);
			const tif_judge& judge = *judge_it;
			if (judge_at == 0) {
				if (judge.logic != nposm) {
					err_msg = vgettext2("In $branch_index branch, for the first judgment on the condition, its logical operation must be empty.", symbols);
					return err_msg;
				}
			} else if (judge.logic == nposm) {
				err_msg = vgettext2("In $branch_index branch, the $judge_index judgment of the condition, its logical operation cannot be empty.", symbols);
				return err_msg;
			}
		}
	}

	VALIDATE(err_msg.empty(), null_str);
	return err_msg;
}

void tif_block::branch_downward1(int at)
{
	int size = branches.size();
	VALIDATE(size >= 2, null_str);
	VALIDATE(at >= 0 && at < size, null_str);

	std::vector<tif_branch>::iterator it1 = branches.begin();
	std::vector<tif_branch>::iterator it2 = branches.begin();

	// std::swap(
	if (size == 2) {
		std::iter_swap(it1, it2 + 1);

	} else if (at != size - 1) {
		if (at != 0) {
			std::advance(it1, at);
		}
		std::advance(it2, at + 1);
		std::iter_swap(it1, it2);

	} else {
		std::advance(it1, at);
		// 1/2 erase last
		tif_branch last_branch = *it1;
		branches.erase(it1);
		// 2/2 insert last at [0]
		branches.insert(branches.begin(), last_branch);
	}
}

bool tif_block::is_using_state(int state) const
{
	VALIDATE(state != nposm, null_str);
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_to_state == state) {
			return true;
		}
	}
	return false;
}

bool tif_block::state_sub1(int erase_state)
{
	VALIDATE(erase_state != nposm, null_str);

	bool modified = false;
	for (std::vector<aplt::tif_branch>::iterator it = branches.begin(); it != branches.end(); ++ it) {
		aplt::tif_branch& branch = *it;
		VALIDATE(branch.do_to_state != erase_state, null_str);
		if (branch.do_to_state > erase_state) {
			branch.do_to_state --;
			modified = true;
		}
	}

	return modified;
}

bool tif_block::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	bool modified = false;
	for (std::vector<aplt::tif_branch>::iterator it = branches.begin(); it != branches.end(); ++ it) {
		aplt::tif_branch& branch = *it;
		if (branch.do_to_state == s1) {
			branch.do_to_state = s2;
			modified = true;

		} else if (branch.do_to_state == s2) {
			branch.do_to_state = s1;
			modified = true;
		}
	}
	return modified;
}

bool tif_block::is_utf8str() const
{
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (!utils::is_utf8str(branch.do_str.c_str(), branch.do_str.size())) {
			return false;
		}
	}

	return true;
}

bool tif_block::is_valid_position() const
{
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_str.empty()) {
			continue;
		}
		if (is_var_name(branch.do_str, nullptr, nullptr)) {
			continue;
		}
		if (utils::is_uuid(branch.do_str, true)) {
			continue;
		}
		return false;
	}
	return true;
}

bool tif_block::all_are_same_state() const
{
	if (branches.size() < 2) {
		return false;
	}

	int to_state = secondary_nposm; // @nposm is "_idle" state, is a valid state. so initial must not use @nposm
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_to_state != to_state) {
			if (to_state != secondary_nposm) {
				return false;
			}
			to_state = branch.do_to_state;
		}
	}

	return true;
}

void tif_block::validate_state(int states) const
{
	int to_state = nposm;
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_to_state != nposm) {
			VALIDATE(branch.do_to_state >= 0 && branch.do_to_state < states, null_str);
		}
	}
}

bool tif_block::all_are_same_str() const
{
	if (branches.size() < 2) {
		return false;
	}
	std::string str = str_nposm;
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_str != str) {
			if (str != str_nposm) {
				return false;
			}
			str = branch.do_str;
		}
	}

	return true;
}

bool tif_block::str_are_empty() const
{
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (!branch.do_str.empty()) {
			return false;
		}
	}
	return true;
}

bool tif_block::str_may_empty() const
{
	if (branches.empty()) {
		return true;
	}
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_str.empty()) {
			return true;
		}
	}
	return false;
}

std::set<std::string> tif_block::may_strs() const
{
	std::set<std::string> result;
	if (branches.empty()) {
		return result;
	}
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (!branch.do_str.empty()) {
			result.insert(branch.do_str);
		}
	}
	return result;
}

}


