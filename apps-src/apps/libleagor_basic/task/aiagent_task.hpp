#ifndef _LIBLEAGOR_BASIC_AIAGENT_TASK_HPP_
#define _LIBLEAGOR_BASIC_AIAGENT_TASK_HPP_

#include "rose_sdl_utils.hpp"
#include "so_aplt_task_helper.hpp"

namespace aplt {

class ttimed_reminder: public thelper_aiagent_task_slot
{
public:
	ttimed_reminder(tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished);
	~ttimed_reminder() {}

	std::string pre_start_task() override;

	void nonmain_start_task(bool& exit) override {}
	void task_finished(const tapplet::ttask& cfg_task) override;

	void slice() override;

private:
	void clear()
	{
		// question_.clear();
		state_ = nposm;
		add_timed_task_cfg_.clear();
	}

	void validate_nposm()
	{
		// VALIDATE(question_.empty(), null_str);
		VALIDATE(state_ == nposm, null_str);
		VALIDATE(add_timed_task_cfg_.empty(), null_str);

		// VALIDATE(!ds_retbool_, null_str);
		// VALIDATE(ds_answer_.empty(), null_str);
	}

	void did_nlp_answer(bool retbool, const std::string& answer, int input_tokens, int output_tokens);
	void did_aplt_task_finished(const aplt::ttask_vars& task_vars);

	config answer_2_add_timed_task_cfg(const std::string& answer) const;

private:
	const std::string dbg_answer_md_;
	const std::string var_name_ds_retbool_;
	const std::string var_name_ds_answer_;
	std::string aplt_id_leagor_basic_;

	enum {state_send_nlp, state_desire_add_timing_task, state_call_add_timing_task};
	int state_;
	config add_timed_task_cfg_;

	bool retbool_;
	std::string answer_;
};

thelper_aiagent_task_slot* create_aiagent_task_slot(int code, tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished);

class tlua_aiagent: public thelper_lua_aiagent
{
};

void luaW_pushvaiagent(lua_State* L, tlua_aiagent& widget);

}

#endif // LIBLEAGOR_BASIC_AIAGENT_CORE_HPP
