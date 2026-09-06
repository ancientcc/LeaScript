#ifndef LIBROSE2_VAR_HPP_INCLUDED
#define LIBROSE2_VAR_HPP_INCLUDED

#include "config.hpp"
#include "rose_exception.hpp"

namespace aplt {

extern LIB3RDPARTY_DECL std::map<var_type_t, tcode3> var_types;
LIB3RDPARTY_DECL bool is_aplt_task_var_type(var_type_t type);

enum {var_env_time, var_env_hour24_time, var_env_temperature, var_env_light, var_env_humidity, var_env_integer, var_env_bool, BI_env_sync_count,
	var_env_basesubtask_code = BI_env_sync_count, var_env_basesubtask_str, var_env_button_code, var_env_ocr_pen_code, var_env_ocr_pen_text, BI_env_last_var_min,
	var_env_time_last = BI_env_last_var_min, 
	var_env_hour24_time1_last, var_env_temperature_last, var_env_light_last, var_env_humidity_last, var_env_integer_last, var_env_bool_last, var_env_hour24_time2_last, var_env_hour24_time3_last, BI_task_var_min,
	var_last_matched_index = BI_task_var_min, var_iot_device_id, var_iot_alias, BI_var_count};

// BI is the abbreviation for "Built-in".
#define var_type_is_BI(type)	((type) >= 0 && (type) < aplt::BI_var_count)
#define var_type_is_BI_env(type)	((type) >= 0 && (type) < aplt::BI_task_var_min)
#define var_type_is_BI_env_normal(type)	((type) >= 0 && (type) < aplt::BI_env_last_var_min)
#define var_type_is_BI_env_last(type)	((type) >= aplt::BI_env_last_var_min && (type) < aplt::BI_task_var_min)
#define var_type_is_BI_task(type)	((type) >= aplt::BI_task_var_min && (type) < aplt::BI_var_count)

#define BI_var_val_type_is_bool(type)	((type) == aplt::var_env_bool || (type) == aplt::var_env_bool_last)
#define BI_var_val_type_is_string(type)	((type) == aplt::var_env_basesubtask_str || (type) == aplt::var_env_ocr_pen_text)
#define BI_var_val_type_is_integer(type)	(var_type_is_BI_env(type) && !BI_var_val_type_is_bool(type) && !BI_var_val_type_is_string(type))
#define BI_var_is_auto_update(type)		((type) == aplt::var_env_time || (type) == aplt::var_env_hour24_time)

#define BI_env_hour24_time_last_count		3
#define BI_env_normal_var_is_hour24_time(type) \
	((type) == aplt::var_env_hour24_time1_last || (type) == aplt::var_env_hour24_time2_last || (type) == aplt::var_env_hour24_time3_last)

LIB3RDPARTY_DECL int BI_env_var_last_2_normal(int last_var_type);

extern LIB3RDPARTY_DECL std::map<int, tcode3> BI_env_vars;
extern LIB3RDPARTY_DECL std::map<int, tcode3> BI_task_vars;
extern LIB3RDPARTY_DECL std::map<std::string, int> BI_env_var_id_2_types;
extern LIB3RDPARTY_DECL std::map<std::string, int> BI_var_id_2_types;

LIB3RDPARTY_DECL const tcode3& builtin_var(int type);

enum {ble_var_device_id, BI_ble_var_count};
extern LIB3RDPARTY_DECL std::map<int, tcode3> BI_ble_vars;
LIB3RDPARTY_DECL const std::string ble_builtin_var_name(const std::string& bundleid, int type);

struct ttask_var
{
public:
	ttask_var(const std::string& name, bool is_array, int type = nposm)
		: name(name)
		, is_array(is_array)
		, type(type)
	{}

	std::string valx_2_str() const;

	bool operator==(const ttask_var& that) const
	{
		if (name != that.name || is_array != that.is_array || val != that.val) {
			return false;
		}
		if (vals != that.vals) {
			return false;
		}

		return true;
	}
	bool operator!=(const ttask_var& that) const { return !operator==(that); }

public:
	std::string name;
	bool is_array;
	// type only use to 'aplt::env_vars'.
	// 1)efficiently update 'var_env_time' and 'var_env_hour24_time', see ttask_vars::get_var(const std::string& name).
	int type;

	config::attribute_value val;
	std::vector<config::attribute_value> vals;
};

class DECLSPEC ttask_vars
{
public:
	bool empty() const { return data_.empty() && symbols_.empty(); }
	bool existed(const std::string& name) const { return data_.count(name) != 0; }

	const ttask_var& get_var(const std::string& name) const;
	// ttask_var& get_mutable_var(const std::string& name);
	var_type_t type(const std::string& name) const;
	int size() const { return (int)data_.size(); }
	const std::map<std::string, ttask_var>& data() const { return data_; }

	void insert_bool(const std::string& name, bool is_array, bool val, int type = nposm);
	void insert_integer(const std::string& name, bool is_array, int64_t val, int type = nposm);
	void insert_double(const std::string& name, bool is_array, double val);
	void insert_string(const std::string& name, bool is_array, const std::string& val);
	void insert_attribute(const std::string& name, bool is_array, const config::attribute_value& val, int type = nposm);

	void erase(const std::string& name);

	bool get_bool(const std::string& name) const;
	int get_int(const std::string& name) const;
	int get_int64(const std::string& name) const;
	double get_double(const std::string& name) const;
	std::string get_string(const std::string& name) const;

	const utils::string_map& symbols() const { return symbols_; }

	void clear()
	{
		data_.clear();
		symbols_.clear();
	}

	bool operator==(const ttask_vars& that) const
	{
		if (data_.size() != that.data_.size() || symbols_.size() != that.symbols_.size()) {
			return false;
		}

		if (data_ != that.data_ || symbols_ != that.symbols_) {
			return false;
		}		

		return true;
	}
	bool operator!=(const ttask_vars& that) const { return !operator==(that); }

private:
	bool insert_th(const std::string& name, bool is_array, int type);

private:
	std::map<std::string, ttask_var> data_;
	utils::string_map symbols_;
};

extern LIB3RDPARTY_DECL ttask_vars env_vars;
LIB3RDPARTY_DECL void init_env_vars();
LIB3RDPARTY_DECL ttask_vars clone_env_vars(bool only_normal);
LIB3RDPARTY_DECL const ttask_var* get_env_var(const std::string& name);
LIB3RDPARTY_DECL void set_env_var(int type, const config::attribute_value& val);

LIB3RDPARTY_DECL bool is_var_name(const std::string& str, const ttask_vars* task_vars, std::string* var_name);
enum {var_exp_type_var, var_exp_type_func};
LIB3RDPARTY_DECL int var_exp_which_type(const std::string& str, std::string* no_symboled_str);
#define str_is_var_exp(str)	(aplt::var_exp_which_type(str, nullptr) != nposm)
LIB3RDPARTY_DECL int split_no_symboled_func_exp(const std::string& str, std::vector<std::string>& params);
LIB3RDPARTY_DECL int split_var_exp_4_func(const std::string& str, std::vector<std::string>& params);
LIB3RDPARTY_DECL std::string var_exp_for_browser(const std::string& var_exp);


//
// function
//
struct tfunction_code
{
/*
	tfunction_code()
		: code(nposm)
		, params(nposm)
	{}
*/
	tfunction_code(int code, const std::string& id, const std::vector<std::string>& params, const std::string& desc)
		: code(code)
		, id(id)
		, params(params)
		, desc(desc)
	{}

	bool operator<(const tfunction_code& that) const { return code < that.code; }

	int code;
	std::string id;
	std::vector<std::string> params;
	std::string desc;
};

extern LIB3RDPARTY_DECL std::map<int, tfunction_code> functions;
extern LIB3RDPARTY_DECL std::map<std::string, int> function_keys;

class LIB3RDPARTY_DECL tfunction
{
public:
	enum {err_ok = 0, err_no_func = -2, err_unsupport = -3, err_param_count = -4, err_not_exist_var = -5, 
		err_var_type_mismatch = -6, 
		err_at_out_range = -7 // @var myabe empty. This means that the intermediate computation is out of the array index.
	};

	struct tresult
	{
		tresult()
			: err(err_ok)
		{}

		void set_err_novar(int _err)
		{
			VALIDATE(_err == err_no_func || _err == err_unsupport || _err == err_param_count, null_str);

			VALIDATE(err == err_ok, null_str);
			VALIDATE(err_var.empty(), null_str);

			err = _err;
		}

		void set_err_var_1var(int _err, const std::string& var)
		{
			if (_err == err_var_type_mismatch) {
				VALIDATE(!var.empty(), null_str);
			} else {
				// @var maybe empty.
				VALIDATE(_err == err_not_exist_var || _err == err_at_out_range, null_str);
			}

			VALIDATE(err == err_ok, null_str);
			VALIDATE(err_var.empty(), null_str);

			err = _err;
			err_var = var;
		}

		void clear()
		{
			val = config::attribute_value();
			err = err_ok;
			err_var.clear();
		}

		bool is_ok() const
		{
			return err == err_ok;
		}

		void validate_nposm() const
		{
			// 1th do_interpolation2 may change val. although 'err == err_ok'
			// VALIDATE(val.type() == var_type_nposm, null_str);

			VALIDATE(err == err_ok, null_str);
			VALIDATE(err_var.empty(), null_str);
		}

		config::attribute_value val;

		// Below is used for caller to assist in judgment
		int err;
		std::string err_var;
	};

	enum {func_add, func_sub, func_mul, func_div, func_inv_bool, 
		func_size, func_sum, func_element, func_join_element, func_join_index, func_join2_2element,
		func_res_path, func_preferences_dir,
		func_format_time_date, func_format_elapse_hms, 
		func_is_speaking, func_is_wko_task_finished,
		func_builtin_count};

	tfunction();
	virtual ~tfunction();

	tresult calculate(const aplt::ttask_vars& task_vars, const std::string& no_symboled_str);
	virtual std::string func_2_var_exp(const aplt::tfunction_code& func, const std::vector<std::string>& params, bool browser) const;

private:
	virtual bool calculate2(const aplt::ttask_vars& task_vars, int func_code, const std::vector<std::string>& params, tresult& result) { return false; }
};

extern LIB3RDPARTY_DECL tfunction* curr_func;
LIB3RDPARTY_DECL const config::attribute_value* var_val_from_no_symboled_str(const ttask_vars& task_vars, int type, const std::string& no_symboled_str, tfunction::tresult& result);


//
// if_judge
//
#define MAX_IF_JUDGES	4
#define MAX_IF_BRANCHES	6

/*
TEST_STR_ATTR("equals",                str_value == attr_str);
TEST_STR_ATTR("not_equals",            str_value != attr_str);
TEST_NUM_ATTR("numerical_equals",      num_value == attr_num);
TEST_NUM_ATTR("numerical_not_equals",  num_value != attr_num);
TEST_NUM_ATTR("greater_than",          num_value >  attr_num);
TEST_NUM_ATTR("less_than",             num_value <  attr_num);
TEST_NUM_ATTR("greater_than_equal_to", num_value >= attr_num);
TEST_NUM_ATTR("less_than_equal_to",    num_value <= attr_num);
TEST_ATTR("boolean_equals",     value.to_bool() == attr.to_bool());
TEST_ATTR("boolean_not_equals", value.to_bool() != attr.to_bool());
TEST_STR_ATTR("contains", value.str().find(attr_str) != std::string::npos);
*/

struct DECLSPEC tif_judge
{
public:
	// #0  (aplt.leagor.khome__id == "notebook")
	// #1  && (aplt.leagor.khome__brand == "huawei")
	enum {logic_and, logic_or, logic_count};

	// aplt.leagor.khome__id == "notebook"

	// numerical = integer + double
	enum {op_var_exist, op_var_not_exist, // type: all
		op_string_equal, // type: string
		op_bool_equal, op_bool_not_equal, // type: bool
		op_numerical_equal, op_numerical_not_equal, // type: integer + double
		op_greater_than, op_less_than, op_greater_than_equal_to, op_less_than_equal_to, // type: integer, double
		op_count
	};

	static bool op_operand_is_string(int op);
	static bool op_operand_is_bool(int op);
	static bool op_operand_is_numerical(int op);
	static bool op_has_operand(int op);

	tif_judge()
	{
		clear();
	}

	tif_judge(int logic, int op, const std::string& var_exp)
		: logic(logic)
		, op(op)
		, var_exp(var_exp)
	{
		VALIDATE(logic == nposm || (logic >= 0 && logic < logic_count), null_str);
		VALIDATE(op >= 0 && op < op_count, null_str);
		VALIDATE(!var_exp.empty(), null_str);
	}

	tif_judge(int logic, int op, const std::string& var_exp, const config::attribute_value& r_val)
		: logic(logic)
		, op(op)
		, var_exp(var_exp)
		, r_val(r_val)
	{
		VALIDATE(logic == nposm || (logic >= 0 && logic < logic_count), null_str);
		VALIDATE(op >= 0 && op < op_count, null_str);
		VALIDATE(!var_exp.empty(), null_str);
		VALIDATE(r_val.type() != var_type_nposm, null_str);
	}

	tif_judge(int logic, int op, const std::string& var_exp, const std::string& r_exp);

	bool from_cfg(const config& cfg);
	void to_cfg(config& cfg) const;

	bool calculate(const ttask_vars& task_vars) const;

	bool valid() const { return op != nposm && !var_exp.empty(); }
	void clear()
	{
		logic = nposm;
		op = nposm;
		var_exp.clear();
	}

	std::string to_string(bool unique) const;

	bool operator==(const tif_judge& that) const
	{
		if (logic != that.logic) {
			return false;
		}

		if (op != that.op) {
			return false;
		}

		if (var_exp != that.var_exp) {
			return false;
		}

		if (r_exp != that.r_exp) {
			return false;
		}

		if (r_val != that.r_val) {
			return false;
		}
		return true;
	}
	bool operator!=(const tif_judge& that) const { return !operator==(that); }

	bool use_r_val() const { return r_val.type() != var_type_nposm; }

public:
	// first tif_judge's logic logic always is nposm.
	int logic;

	int op;
	std::string var_exp; // variable expression/left expression
	// right expression. variable expression or constant value.
	// if 'r_val.type == var_type_nposm', use r_exp, else use r_val.
	std::string r_exp;
	config::attribute_value r_val;
};

//
// if_branch
//
struct DECLSPEC tif_branch
{
public:
	tif_branch()
		: do_to_state(nposm)
		, do_bool_set(bool_set_none)
	{}

	// (aplt.leagor.khome__id == "notebook") && (aplt.leagor.khome__brand == "huawei")
	explicit tif_branch(const std::string& str)
		: do_to_state(nposm)
		, do_str(str)
		, do_bool_set(bool_set_none)
	{}

	bool from_cfg(int states, const config& cfg);
	void to_cfg(int states, config& cfg) const;

	std::string judges_to_string(int at) const;

	// std::string do_to_string() const;

	bool operator==(const tif_branch& that) const
	{
		if (do_to_state != that.do_to_state) {
			return false;
		}

		if (do_str != that.do_str) {
			return false;
		}

		if (do_bool_set != that.do_bool_set) {
			return false;
		}

		if (do_map_vals != that.do_map_vals) {
			return false;
		}

		if (judges.size() != that.judges.size()) {
			return false;
		}

		if (judges != that.judges) {
			return false;
		}
		return true;
	}

public:
	std::vector<tif_judge> judges;
	// do
	int do_to_state; // type: startup_state/to_state

	std::string do_str;  // type: finished
	bool_set_t do_bool_set; // type: finished

	std::map<std::string, std::string> do_map_vals;
};

struct DECLSPEC tif_block
{
public:
	tif_block()
	{}

	explicit tif_block(const std::string& str)
	{
		branches.push_back(tif_branch(str));
	}

	void set_do_str_only(const std::string& str)
	{
		branches.clear();
		branches.push_back(tif_branch(str));
	}

	// if @states is nposm, don't check @do_startup_state and @do_to_state.
	bool from_cfg(const std::string& name, int states, const config& cfg);
	// if @states is nposm, don't check @do_startup_state and @do_to_state.
	void to_cfg(const std::string& name, int states, config& cfg) const;

	struct tresult {
		tresult()
		{
			clear();
		}

		void clear()
		{
			is_ok = false;
			do_to_state = nposm;
			do_bool_set = bool_set_none;
		}

		bool is_ok;
		int do_to_state;

		std::string do_str;
		bool_set_t do_bool_set;

		std::map<std::string, std::string> do_map_vals;
	};
	tresult calculate(const ttask_vars& task_vars) const;

	std::string is_valid(bool allow_empty = true) const;

	void clear()
	{
		branches.clear();
	}

	void branch_downward1(int at);

	// do_to_state
	bool is_using_state(int state) const;
	bool state_sub1(int erase_state);
	bool state_swap(int s1, int s2);
	bool all_are_same_state() const;  // when branches >= 2
	void validate_state(int states) const;

	// do_str
	bool is_utf8str() const;
	bool is_valid_position() const;
	bool all_are_same_str() const; // when branches >= 2
	bool str_are_empty() const;
	bool str_may_empty() const;
	std::set<std::string> may_strs() const;


	bool operator==(const tif_block& that) const
	{
		if (branches.size() != that.branches.size()) {
			return false;
		}

		if (branches != that.branches) {
			return false;
		}
		return true;
	}
	bool operator!=(const tif_block& that) const { return !operator==(that); }

public:
	// 'else {' --> tif_branch's '@cond' is empty.
	std::vector<tif_branch> branches;
};

extern LIB3RDPARTY_DECL std::map<int, tcode3> if_judge_ops;
LIB3RDPARTY_DECL int if_judge_op_from_id(const std::string& id);

extern LIB3RDPARTY_DECL std::map<int, tcode3> if_logic_ops;
LIB3RDPARTY_DECL int if_logic_op_from_id(const std::string& id);

}


#endif

