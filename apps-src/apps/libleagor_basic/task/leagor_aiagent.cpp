#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_aiagent.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "aiagent_task.hpp"
#include "common.hpp"

using namespace std::placeholders;

enum {examsubject_mathematics, examsubject_physics, examsubject_chemistry, examsubject_biology, 
	examsubject_geography, examsubject_chinese, examsubject_english, examsubject_politics};

namespace aplt {

//
// aiagent task
//
tleagor_aiagent_api::tleagor_aiagent_api()
	: thelper_aiagent_api(*curr_aplt)
{
	const std::string id_add_timed_reminder = "add_timed_reminder";
	task_ids_.insert(std::make_pair(id_add_timed_reminder, aiagent_add_timed_reminder));

	std::vector<tprompt> add_timed_reminder_prompts = {
		{_("add_timed_reminder prompt^study #0"), _("add_timed_reminder prompt's note^3 columns")},
		{_("add_timed_reminder prompt^study #1"), _("add_timed_reminder prompt's note^3 columns")},
		{_("add_timed_reminder prompt^rehabilitation #0"), _("add_timed_reminder prompt's note^3 columns")},
		{_("add_timed_reminder prompt^rehabilitation #1"), _("add_timed_reminder prompt's note^3 columns")},
	};
	prompts_.insert(std::make_pair(id_add_timed_reminder, add_timed_reminder_prompts));
}

void tleagor_aiagent_api::app_prompts(const std::string& task_id, int subject, const std::string& header, std::vector<tprompt>& result)
{
	if (task_id == "add_timed_reminder") {
		result = prompts_.find(task_id)->second;

	} else if (task_id == "exam") {
		if (header.empty()) {
			result.push_back(tprompt(null_str, _("No questions are available.")));
			return;
		}
		std::string prompt;
		std::string note;

		if (subject == examsubject_mathematics) {
			prompt = header + _("exam prompt^math, full exam");
			note = _("exam note^math");
			result.push_back(tprompt(prompt, note, true));

			prompt = header + _("exam prompt^math, choice only");
			note = _("exam note^math, choice only");
			result.push_back(tprompt(prompt, note, true));

			prompt = header + _("exam prompt^math, solving only");
			note = _("exam note^math");
			result.push_back(tprompt(prompt, note, true));

		} else {
			prompt = header + _("exam prompt^other, full exam");
			note = _("exam note^math");
			result.push_back(tprompt(header, str_cast(subject), true));
		}

	} else {
		VALIDATE(false, null_str);
	}
}

thelper_aiagent_task_slot* tleagor_aiagent_api::app_create_aiagent_task_slot(int code, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished)
{
	return create_aiagent_task_slot(code, aplt_, cfg_task, question, surf, task_vars, finished);
}

}
