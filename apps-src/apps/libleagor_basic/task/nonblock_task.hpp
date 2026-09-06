#ifndef LIBLEAGOR_BASIC_NONBLOCK_CORE_HPP
#define LIBLEAGOR_BASIC_NONBLOCK_CORE_HPP

#include "rose_sdl_utils.hpp"
#include "so_aplt_task_helper.hpp"

namespace aplt {

class tdeepseek2: public thelper_nonblock_task_slot
{
public:
	tdeepseek2(tapplet& aplt, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished);
	~tdeepseek2() {}

	std::string pre_start_task() override;

	void nonmain_start_task(bool& exit) override {}
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override { return false; }

private:
	void clear()
	{
		question_.clear();
		surf_ = nullptr;
	}

	void validate_nposm()
	{
		VALIDATE(question_.empty(), null_str);
		VALIDATE(surf_.get() == nullptr, null_str);

		// VALIDATE(!ds_retbool_, null_str);
		// VALIDATE(ds_answer_.empty(), null_str);
	}

	void did_nlp_answer(bool retbool, const std::string& answer, int input_tokens, int output_tokens);

private:
	const std::string var_name_ds_retbool_;
	const std::string var_name_ds_answer_;

	std::string question_;
	surface surf_;

	bool retbool_;
	std::string answer_;
};

thelper_nonblock_task_slot* create_nonblock_task_slot(int code, tapplet& aplt, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished);

class tlua_nonblock: public thelper_lua_nonblock
{
};

void luaW_pushvnonblock2(lua_State* L, tlua_nonblock& widget);

}

#endif // LEAGOR_BASIC_NONBLOCK2_HPP_INCLUDED
