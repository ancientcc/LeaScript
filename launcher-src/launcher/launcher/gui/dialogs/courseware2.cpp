#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/courseware2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/center.hpp"
#include "gui/dialogs/latex_editor.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
// #include "aplt_net.hpp"
#include "net.hpp"

#include "serialization/parser.hpp"
#include "chinese.hpp"
#include "speech_driver.hpp"
#include "ros_instance.hpp"
#include "minizip/minizip.hpp"

#include "game_latex.hpp"

using namespace std::placeholders;

extern std::string miktex_output_dir;
extern void user_not_valid_try_again();

const std::string unfavorite_png = "misc/favorite.png";
const std::string favorited_png = "misc/favorite_red.png";

const std::string unlisten_png = "misc/listen.png";
const std::string listening_png = "misc/stop_listen.png";

void tresizable_data::resize_data(int desire_size, int vsize)
{
	desire_size = posix_align_ceil(desire_size, 4096);
	VALIDATE(desire_size >= 0, null_str);

	if (desire_size > size) {
		char* tmp = (char*)malloc(desire_size);
		if (data != nullptr) {
			if (vsize) {
				memcpy(tmp, data, vsize);
			}
			free(data);
		}
		data = tmp;
		size = desire_size;
	}
}

void tresizable_data::append(const char* c_str, int l)
{
	const int max_l = CONSTANT_1G;
	if (l <= 0 || l > max_l) {
		return;
	}

	resize_data(vsize + l, vsize);
	memcpy(data + vsize, c_str, l);
	vsize += l;
}

void tresizable_data::append_1ch(int ch)
{
	resize_data(vsize + 1, vsize);
	data[vsize] = (uint8_t)ch;
	vsize += 1;
}

void tresizable_data::drop_first(int _size)
{
	VALIDATE(vsize >= _size, null_str);
	memcpy(data, data + _size, vsize - _size);
	vsize -= _size;
}

/*
trexam_str_type rexam_str_types_[4] = {
	{rexam_type_choice, "{choice}", (int)SDL_strlen("{choice}"), "\0"}, 
	{rexam_type_fillin, "{fillin}", (int)SDL_strlen("{fillin}"), "\0"}, 
	{rexam_type_solving, "{solving}", (int)SDL_strlen("{solving}"), "\0"},
	{rexam_type_other, "{other}", (int)SDL_strlen("{other}"), "\0"}
};
*/
trexam_str_type rexam_str_types_C[4];

enum {examsubject_mathematics, examsubject_physics, examsubject_chemistry, examsubject_biology, 
	examsubject_geography, examsubject_chinese, examsubject_english, examsubject_politics};

std::string texam_problem::to_tex(int sub_at) const
{
	std::string prefix;
	if (sub_at != nposm) {
		VALIDATE(sub_at >= 0 && sub_at < (int)subproblems.size(), null_str);
		VALIDATE(type == rexam_type_solving, null_str);
		prefix = "    ";
	}
	const texam_problem& this_p = sub_at == nposm? *this: subproblems[sub_at];
	const bool is_main = sub_at == nposm;

	std::stringstream question_ss;

	if (is_main) {
		question_ss << "    \\question";
	} else {
		question_ss << prefix << "    \\part";
	}

	question_ss << "[" << this_p.points << "]" << this_p.question << "\n\n";

	if (this_p.type == rexam_type_choice) {
		question_ss << prefix << "    ";
		question_ss << (this_p.tex_cols != EXAM_TEX_COLS1? "\\twochoiceswithanswerB": "\\onechoiceswithanswerB");
		question_ss << "\n"; // twochoiceswithanswerB, onechoiceswithanswerB
		for (std::vector<std::string>::const_iterator it2 = this_p.options.begin(); it2 != this_p.options.end(); ++ it2) {
			const std::string& option = *it2;
			question_ss << prefix << "        {" << option << "}\n";
		}
		question_ss << prefix << "        {" << utils::join(this_p.correct_options, null_str) << "}\n";
		question_ss << prefix << "        {" << this_p.explanation << "}";

	} else if (this_p.type == rexam_type_fillin) {
		question_ss << prefix << "    \\showexplanationforfillin\n";
		question_ss << prefix << "    {" << this_p.explanation << "}";

	} else if (this_p.type == rexam_type_solving) {
		VALIDATE(is_main, null_str);
		VALIDATE(prefix.empty(), null_str);

		if (this_p.subproblems.empty()) {
			question_ss << "    \\showexplanationforsolving[3.0in]\n";
			question_ss << "    {" << this_p.explanation << "}";

		} else {
			int at = 0;
			question_ss << "    \\begin{parts}\n";
			for (std::vector<texam_problem>::const_iterator it = this_p.subproblems.begin(); it != this_p.subproblems.end(); ++ it, at ++) {
				const texam_problem& sub = *it;
				if (it != this_p.subproblems.begin()) {
					question_ss << "\n        \\vspace{0.3cm}\n\n";
				}
				question_ss << to_tex(at);
				question_ss << "\n";
			}
			question_ss << "    \\end{parts}\n";
		}

	} else if (this_p.type == rexam_type_other) {
		VALIDATE(!is_main, null_str);

		question_ss << prefix << "    \\showexplanationforsolving[3.0in]\n";
		question_ss << prefix << "    {" << this_p.explanation << "}";

	} else {
		VALIDATE(false, null_str);
	}

	return question_ss.str();
}

std::string texam_problem::to_rose_msg(int sub_at) const
{
	// rexam_str_types_C
	if (sub_at != nposm) {
		VALIDATE(sub_at >= 0 && sub_at < (int)subproblems.size(), null_str);
		VALIDATE(type == rexam_type_solving, null_str);
	}
	const texam_problem& this_p = sub_at == nposm? *this: subproblems[sub_at];
	const bool is_main = sub_at == nposm;

	std::stringstream ss;
	ss << rexam_str_types_C[this_p.type].str << "\n";
	if (this_p.points <= 0) {
		return null_str;
	}
	ss << this_p.points << "\n";
	if (this_p.question.empty()) {
		return null_str;
	}
	ss << "[**&question##@]" << this_p.question << "[/**&question##@]\n";
	if (this_p.type == rexam_type_choice) {
		if (this_p.options.empty()) {
			return null_str;
		}
		for (std::vector<std::string>::const_iterator it = this_p.options.begin(); it != this_p.options.end(); ++ it) {
			const std::string& option = *it;
			ss << "[**&opt##@]" << option << "[/**&opt##@]\n";
		}

	} else if (this_p.type == rexam_type_solving) {
		if (!this_p.subproblems.empty()) {
			int s = this_p.subproblems.size();
			std::string subproblem_str;
			for (int at = 0; at < s; at ++) {
				if (at != 0) {
					ss << "\n";
				}
				subproblem_str = to_rose_msg(at);
				if (subproblem_str.empty()) {
					return null_str;
				}
				ss << subproblem_str;
			}
			// ss << "\n\n";
			return ss.str();
		}
	}

	ss << "[**&expl##@]" << this_p.explanation << "[/**&expl##@]";
	if (this_p.type == rexam_type_choice) {
		if (this_p.correct_options.empty()) {
			return null_str;
		}
		for (std::set<int>::const_iterator it = this_p.correct_options.begin(); it != this_p.correct_options.end(); ++ it) {
			int option = *it;
			if (it == this_p.correct_options.begin()) {
				ss << "\n";
			} else {
				ss << ",";
			}
			ss << (char)(option + 'A' - 1);
		}
		if (this_p.tex_cols != EXAM_DEF_TEX_COLS) {
			ss << "\n";
			ss << "tex_cols=" << this_p.tex_cols;
		}
	}
	if (sub_at == nposm) {
		// ss << "\n\n";
	}
	return ss.str();
}
/*
std::string texam_problem::to_rose_msg(int sub_at) const
{
	// rexam_str_types_C
	if (sub_at != nposm) {
		VALIDATE(sub_at >= 0 && sub_at < (int)subproblems.size(), null_str);
		VALIDATE(type == rexam_type_solving, null_str);
	}
	const texam_problem& this_p = sub_at == nposm? *this: subproblems[sub_at];
	const bool is_main = sub_at == nposm;

	std::stringstream ss;
	ss << rexam_str_types_C[this_p.type].str << "\n";
	if (this_p.points <= 0) {
		return null_str;
	}
	ss << this_p.points << "\n";
	if (this_p.question.empty()) {
		return null_str;
	}
	ss << "[**&question##@]" << this_p.question << "[**&question##@]\n";
	if (this_p.type == rexam_type_choice) {
		if (this_p.options.empty()) {
			return null_str;
		}
		for (std::vector<std::string>::const_iterator it = this_p.options.begin(); it != this_p.options.end(); ++ it) {
			const std::string& option = *it;
			ss << "[**&opt##@]" << option << "[**&opt##@]\n";
		}

	} else if (this_p.type == rexam_type_solving) {
		if (!this_p.subproblems.empty()) {
			int s = this_p.subproblems.size();
			std::string subproblem_str;
			for (int at = 0; at < s; at ++) {
				if (at != 0) {
					ss << "\n";
				}
				subproblem_str = to_rose_msg(at);
				if (subproblem_str.empty()) {
					return null_str;
				}
				ss << subproblem_str;
			}
			// ss << "\n\n";
			return ss.str();
		}
	}

	ss << "[**&expl##@]" << this_p.explanation << "[**&expl##@]";
	if (this_p.type == rexam_type_choice) {
		if (this_p.correct_options.empty()) {
			return null_str;
		}
		for (std::set<int>::const_iterator it = this_p.correct_options.begin(); it != this_p.correct_options.end(); ++ it) {
			int option = *it;
			if (it == this_p.correct_options.begin()) {
				ss << "\n";
			} else {
				ss << ",";
			}
			ss << (char)(option + 'A' - 1);
		}
		if (this_p.tex_cols != EXAM_DEF_TEX_COLS) {
			ss << "\n";
			ss << "tex_cols=" << this_p.tex_cols;
		}
	}
	if (sub_at == nposm) {
		// ss << "\n\n";
	}
	return ss.str();
}
*/
namespace aplt {

std::map<int, tcode3> tcourseware::types;

tcourseware::tcourseware()
	: type(nposm)
{
	uuid = utils::create_uuid(false);
	if (types.empty()) {
		types.insert(std::make_pair(type_nposm, tcode3(type_nposm, "nposm", _("coursewaretype^nposm"))));
		types.insert(std::make_pair(type_exam, tcode3(type_exam, "exam", _("coursewaretype^exam"))));
	}
}

const tcode3& tcourseware::type_from_str(const std::string& str)
{
	VALIDATE(!types.empty(), null_str);
	const tcode3& nposm_result = types.find(nposm)->second;
	if (str.empty()) {
		return nposm_result;
	}

	for (std::map<int, tcode3>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const tcode3& code3 = it->second;
		if (code3.id == str) {
			return it->second;
		}
	}
	return nposm_result;
}

bool tcourseware::from_cfg(const config& cfg)
{
	clear();

	bool fail = false;
	uuid = cfg["uuid"].str();
	type = type_from_str(cfg["type"].str()).code;

	tex_header = cfg["tex_header"].str();
	tex_tail = cfg["tex_tail"].str();
	title = cfg["title"].str();
	content = cfg["content"].str();
	annotation = cfg["annotation"].str();
	analysis = cfg["analysis"].str();
	reference = cfg["reference"].str();

	std::vector<const config*> keypoint2_cfgs;
	BOOST_FOREACH (const config& keypoint_cfg, cfg.child_range("keypoint")) {
		keypoint2_cfgs.push_back(&keypoint_cfg);
	}

	for (std::vector<const config*>::const_iterator it = keypoint2_cfgs.begin(); it != keypoint2_cfgs.end(); ++ it) {
		const config& keypoint_cfg = **it;
		if (!from_keypoint_cfg(keypoint_cfg)) {
			fail = true;
			VALIDATE(false, null_str);
			break;
		}
	}

	bool startup_next_state_parsed = false;
	BOOST_FOREACH (const config& exercise_cfg, cfg.child_range("exercise")) {
		if (!from_exercise_cfg(exercise_cfg)) {
			fail = true;
			VALIDATE(false, null_str);
			break;
		}
	}

	return !fail;
}

bool tcourseware::from_keypoint_cfg(const config& cfg)
{
	const std::string section = cfg["section"].str();
	const std::string name = cfg["name"].str();
	const std::string annotation = cfg["annotation"].str();
	const std::string analysis = cfg["analysis"].str();
	keypoints.push_back(tkeypoint(section, name, annotation, analysis));
	return true;
}

bool tcourseware::from_exercise_cfg(const config& cfg)
{
	const std::string section = cfg["section"].str();
	const std::string question = cfg["question"].str();
	const std::string analysis = cfg["analysis"].str();
	const std::string answer = cfg["answer"].str();
	exercises.push_back(texercise(section, question, analysis, answer));
	return true;
}

void tcourseware::to_cfg(config& cfg) const
{
	cfg.clear();
	VALIDATE(utils::is_uuid(uuid, false), null_str);
	cfg["uuid"] = uuid;
	VALIDATE(types.count(type) != 0, null_str);
	cfg["type"] = types.find(type)->second.id;
	cfg["tex_header"] = tex_header;
	cfg["tex_tail"] = tex_tail;
	cfg["title"] = title;
	cfg["content"] = content;
	cfg["annotation"] = annotation;
	cfg["analysis"] = analysis;
	cfg["reference"] = reference;

	for (std::vector<tkeypoint>::const_iterator it = keypoints.begin(); it != keypoints.end(); ++ it) {
		const tkeypoint& keypoint = *it;

		config& keypoint_cfg = cfg.add_child("keypoint");
		keypoint_cfg["section"] = keypoint.section;
		keypoint_cfg["name"] = keypoint.name;
		keypoint_cfg["annotation"] = keypoint.annotation;
		keypoint_cfg["analysis"] = keypoint.analysis;
	}

	for (std::vector<texercise>::const_iterator it = exercises.begin(); it != exercises.end(); ++ it) {
		const texercise& exercise = *it;

		config& exercise_cfg = cfg.add_child("exercise");
		exercise_cfg["section"] = exercise.section;
		exercise_cfg["question"] = exercise.question;
		exercise_cfg["analysis"] = exercise.analysis;
		exercise_cfg["answer"] = exercise.answer;
	}
}

bool tcourseware::equal(const tcourseware& that) const
{
	if (uuid != that.uuid) {
		return false;
	}
	if (type != that.type) {
		return false;
	}
	if (tex_header != that.tex_header || tex_tail != that.tex_tail) {
		return false;
	}

	if (title != that.title || content != that.content || annotation != that.annotation) {
		return false;
	}

	if (analysis != that.analysis || reference != that.reference) {
		return false;
	}

	if (keypoints.size() != that.keypoints.size() || keypoints != that.keypoints) {
		return false;
	}

	if (exercises.size() != that.exercises.size() || exercises != that.exercises) {
		return false;
	}

	return true;
}

void tcourseware::keypoint_swap(int at1, int at2)
{
	int size = keypoints.size();
	VALIDATE(at1 >= 0 && at1 < size && at2 >= 0 && at2 < size, null_str);

	std::swap(keypoints[at1], keypoints[at2]);
}

void tcourseware::exercise_swap(int at1, int at2)
{
	int size = exercises.size();
	VALIDATE(at1 >= 0 && at1 < size && at2 >= 0 && at2 < size, null_str);

	std::swap(exercises[at1], exercises[at2]);
}

std::string tcourseware::text_for_listen() const
{
	std::stringstream ss;
	ss << content;

	const bool only_content = false;
	if (game_config::os == os_windows && only_content) {
		return ss.str();
	}

	if (!analysis.empty()) {
		ss << analysis;
	}

	utils::string_map symbols;
	int at = 0;
	for (std::vector<tkeypoint>::const_iterator it = keypoints.begin(); it != keypoints.end(); ++ it, at ++) {
		const tkeypoint& keypoint = *it;
		symbols["number"] = str_cast(at + 1);
		symbols["name"] = keypoint.name;
		ss << vgettext2("The $number key point. $name.", symbols);

		ss << keypoint.annotation;
	}

	VALIDATE(!ss.str().empty(), null_str);
	return ss.str();
}

std::map<int, tcode3> rexam_str_types = {
	{rexam_type_choice, tcode3(rexam_type_choice, "{choice}", null_str)}, 
	{rexam_type_fillin, tcode3(rexam_type_fillin, "{fillin}", null_str)}, 
	{rexam_type_solving, tcode3(rexam_type_solving, "{solving}", null_str)},
	{rexam_type_other, tcode3(rexam_type_other, "{other}", null_str)}
};

}

namespace gui2 {

REGISTER_DIALOG(launcher, courseware2)

tcourseware2::tcourseware2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tinstance_slot& instance_slot, const std::map<aplt::taplt_key, aplt::tapplet>& applets,
	aplt::tcfg_cpp_api& cfg_cpp_api, tspeech_driver& speech_driver, const std::string& saves_courseware_dir)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, tbase_courseware(saves_courseware_dir)
	, instance_slot_(instance_slot)
	, applets_(applets)
	, cfg_cpp_api_(cfg_cpp_api)
	, speech_driver_(speech_driver)
	, ancientcc_uid_(11112)
	, pdf_output_dir_(game_config::os == os_android? "/sdcard/apk/latex": game_config::preferences_dir + "/saves")
/*
	, rexam_str_types_C{{"{choice}", (int)SDL_strlen("{choice}"), rexam_type_choice}, 
		{"{fillin}", (int)SDL_strlen("{fillin}"), rexam_type_fillin}, 
		{"{solving}", (int)SDL_strlen("{solving}"), rexam_type_solving},
		{"{other}", (int)SDL_strlen("{other}"), rexam_type_other}}
*/
	, title_widget_(nullptr)
	, back_widget_(nullptr)
	, bar_report_(nullptr)
	, courselist_item_widget_(nullptr)
	, main_stack_(nullptr)
	, toolbar_stack_(nullptr)
	, file_list_(nullptr)
	, tree_widget_(nullptr)
	, v_line2_widget_(nullptr)
	, item_list_stack_(nullptr)
	, first_list_(nullptr)
	, second_list_(nullptr)
	, find_user_widget_(nullptr)
	, may_invisible_cw_tree_top_grid_(nullptr)
	, insert_courseware_widget_(nullptr)
	, tb_upload_change_cw_widget_(nullptr)
	, save_widget_(nullptr)
	, upload_widget_(nullptr)
	, pdf_widget_(nullptr)
	, ai_exercise_widget_(nullptr)
	, fill_default_widget_(nullptr)
	, insert_keypoint_widget_(nullptr)
	, insert_exercise_widget_(nullptr)
	, move_down_widget_(nullptr)
	, move_up_widget_(nullptr)
	, erase_widget_(nullptr)
	, favorite_widget_(nullptr)
	, enter_edit_widget_(nullptr)
	, listen_widget_(nullptr)
	, input_grid_(nullptr)
	, input_toolbar_grid_(nullptr)
	, input_stack_(nullptr)
	, tree_msg_widget_(nullptr)
	, build_msg_widget_(nullptr)
	, status_widget_(nullptr)
	, editing_(false)
	, curr_layer_(nposm)
	, curr_main_layer_(nposm)
	, def_item_list_layer_(FIRST_LIST_LAYER)
	, curr_list_task_type_(nposm)
	, ignore_tree_msg_text_changed_(false)
	, ignore_file_list_row_changed_(false)
	, curr_problem_(tmp_problems_)
	, ignore_input_exam_changed_(false)
	, list_state_(nposm)
	, main_(rtc::Thread::Current())
	, building_(bool_set_none)
	, valid_lines_(0)
{
	create_directory_if_missing(pdf_output_dir_);

	set_timer_interval(1000);

	if (aplt::rexam_str_types.begin()->second.name.empty()) {
		aplt::rexam_str_types.find(rexam_type_choice)->second.name = _("examtype^choice");
		aplt::rexam_str_types.find(rexam_type_fillin)->second.name = _("examtype^fillin");
		aplt::rexam_str_types.find(rexam_type_solving)->second.name = _("examtype^solving");
		aplt::rexam_str_types.find(rexam_type_other)->second.name = _("examtype^other");

		VALIDATE(sizeof(rexam_str_types_C) / sizeof(rexam_str_types_C[0]) == aplt::rexam_str_types.size(), null_str);
		for (std::map<int, tcode3>::const_iterator it= aplt::rexam_str_types.begin(); it != aplt::rexam_str_types.end(); ++ it) {
			const tcode3& type = it->second;
			trexam_str_type& to = rexam_str_types_C[it->second.code];
			to.type = type.code;
			SDL_strlcpy(to.str, type.id.c_str(), sizeof(to.str));
			to.size = type.id.size();
			SDL_strlcpy(to.msgstr, type.name.c_str(), sizeof(to.msgstr));
		}
	}

	load_cw_default();
}

tcourseware2::~tcourseware2()
{
	task_thread_.reset();
}

bool net_get_user_info(int64_t uid, aplt::tcswamp_user& cswamp_user)
{	
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getuserinfo, _1, uid,
		false, std::ref(cswamp_user)));
	return gui2::run_with_progress(slot, null_str, _("Get user info"), 1000);
}

void tcourseware2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	
	title_widget_ = find_widget<tlabel>(window_, "title", false, true);

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_back
			, this
			, std::ref(*button)));
	back_widget_ = button;

	treport* report = find_widget<treport>(window_, "bar_report", false, true);
	report->insert_item(null_str, _("courseware^upload layer"));
	report->insert_item(null_str, _("courseware^download layer"));
	courselist_item_widget_ = &report->insert_item(null_str, null_str);
	refresh_courselist_item_label();
	report->set_did_item_pre_change(std::bind(&tcourseware2::did_report_item_pre_change, this, _1, _2, _3));
	report->set_did_item_changed(std::bind(&tcourseware2::did_report_item_changed, this, _2));
	bar_report_ = report;

	// 'report->select_item(UPLOAD_LAYER)' require toolbar_task is ready.
	tstack* stack = find_widget<tstack>(window_, "main_stack", false, true);
	pre_main_tree(*stack->layer(MAIN_TREE_LAYER));
	pre_main_list(*stack->layer(MAIN_LIST_LAYER));
	main_stack_ = stack;

	may_invisible_cw_tree_top_grid_ = find_widget<tgrid>(window_, "may_invisible_cw_tree_top_grid", false, true);

	button = find_widget<tbutton>(window_, "insert_courseware", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_insert_courseware
			, this
			, std::ref(*button)));
	insert_courseware_widget_ = button;

	button = find_widget<tbutton>(window_, "favorite", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_favorite
			, this
			, std::ref(*button)));
	favorite_widget_ = button;

	button = find_widget<tbutton>(window_, "enter_edit", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_enter_edit
			, this
			, std::ref(*button)));
	 enter_edit_widget_ = button;

/*
	button = find_widget<tbutton>(window_, "listen", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcourseware2::click_listen
			, this, std::ref(*button)));
	listen_widget_ = button;
*/
	// 'report->select_item(UPLOAD_LAYER)' require toolbar_task is ready.
	stack = find_widget<tstack>(window_, "toolbar_stack", false, true);
	pre_toolbar_upload(*stack->layer(UPLOAD_LAYER));
	pre_toolbar_download(*stack->layer(DOWNLOAD_LAYER));
	pre_toolbar_courselist(*stack->layer(COURSELIST_LAYER));
	toolbar_stack_ = stack;

	tlistbox* list = find_widget<tlistbox>(window_, "file_list", false, true);
	// list->enable_select(false);
	list->set_did_row_pre_change(std::bind(&tcourseware2::did_file_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&tcourseware2::did_file_list_row_changed, this, _1, _2));
	list->set_did_can_drag(std::bind(&tcourseware2::did_file_list_can_drag, this, _1, _2));

	button = dynamic_cast<tbutton*>(list->left_drag_grid()->find("erase", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_erase_courseware
			, this
			, std::ref(*list)));

	file_list_ = list;

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);

	// get_cswamp_materials();

	int start_layer = UPLOAD_LAYER;
	if (game_config::os != os_windows) {
		// start_layer = DOWNLOAD_LAYER;
	}
	bar_report_->select_item(start_layer);

	main_stack_->set_radio_layer(MAIN_TREE_LAYER);

	refresh_change_cw_type_label();

	editing_ = true;
	enter_or_exit_edit(false);
}

void tcourseware2::post_show()
{
}

bool tcourseware2::courseware_dirty() const
{
	if (file_list_->cursel() == nullptr) {
		return false;
	}

	const tcourseware_file& courseware_file = courseware_file_from_at(file_list_->cursel()->at());
	if (curr_layer_ == UPLOAD_LAYER && courseware_file.dir_name != tmp_courseware_.title) {
		return true;
	}

	const bool dirty = !courseware_.equal(tmp_courseware_);
	if (dirty) {
		VALIDATE(curr_layer_ == UPLOAD_LAYER, null_str);
	}
	return dirty;
}

void tcourseware2::courseware_dirty_4_exam()
{
	VALIDATE(curr_problem_.valid(), null_str);

	aplt::tcourseware& courseware = tmp_courseware_;
	VALIDATE(courseware.type == aplt::tcourseware::type_exam, null_str);

	tmp_courseware_.exercises[curr_problem_.at].question = curr_problem_.p->to_rose_msg();

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

void tcourseware2::get_cswamp_materials()
{
	cswamp_materials_.clear();

	aplt::tcswamp_user user;
	if (net_get_user_info(ancientcc_uid_, user)) {
		for (std::vector<aplt::tcswamp_material>::const_iterator it = user.materials.begin(); it != user.materials.end(); ++ it) {
			const aplt::tcswamp_material& material = *it;
			cswamp_materials_.insert(std::make_pair(utils::file_stem_name(material.file), material));
		}
	}
}

const char* sscanf_int(const char* ptr, int& result)
{
	ptr = utils::skip_blank_characters(ptr);
	if (ptr[0] == '\0') {
		return nullptr;
	}

	char ch = ptr[0];
	if (ch < '0' || ch > '9') {
		if (ch != '-' || ch != '+') {
			return nullptr;
		}
	}

	const char* ptr2 = ptr;
	ptr = utils::until_blank_characters(ptr, true);
	result = utils::to_int(std::string(ptr2, ptr - ptr2));
	return ptr;
}

const char* sscanf_str(const char* ptr, const std::string& prefix, bool start_with_prefix, const std::string& postfix, std::string& result)
{
	VALIDATE(!postfix.empty(), null_str);
	if (!prefix.empty()) {
		if (start_with_prefix) {
			if (SDL_memcmp(ptr, prefix.c_str(), prefix.size()) != 0) {
				return nullptr;
			}
		} else {
			ptr = SDL_strstr(ptr, prefix.c_str());
			if (ptr == nullptr) {
				return nullptr;
			}
		}
		
	} else {
		VALIDATE(!start_with_prefix, null_str);
	}

	ptr += prefix.size();
	const char* ptr2 = SDL_strstr(ptr, postfix.c_str());
	if (ptr2 == nullptr) {
		return nullptr;
	}
	result.assign(ptr, ptr2 - ptr);
	return ptr2 + postfix.size();
}

const char* sscanf_str_until_blank_characters(const char* ptr, bool include_space, std::string& result)
{
	const char* ptr2 = utils::until_blank_characters(ptr, include_space);
	result.assign(ptr, ptr2 - ptr);
	return ptr2;
}

const char* sscanf_extra(const char* ptr, int& tex_cols)
{
	tex_cols = EXAM_DEF_TEX_COLS;
	if (ptr[0] == '\0') {
		return ptr;
	}

	ptr = utils::skip_blank_characters(ptr);
	if (ptr[0] == '\0' || ptr[0] == '{') {
		return ptr;
	}

	std::string extra_str;
	ptr = sscanf_str_until_blank_characters(ptr, false, extra_str);
	std::vector<std::string> vsize = utils::split(extra_str);
	if (vsize.empty()) {
		return ptr;
	}

	std::vector<std::string> vsize2;
	for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
		const std::string& str = *it;
		vsize2 = utils::split(str, '=');
		if (vsize2.size() == 2) {
			if (vsize2[0] == "tex_cols") {
				tex_cols = utils::to_int(vsize2[1]);
			}
		}
	}
	return ptr;
}

void rexam_str_2_problems(const trexam_str_type (&str_types)[4], const char* c_str, int size, std::vector<texam_problem>& problems)
{
	problems.clear();

	const char* this_start = c_str;
	int type_count = sizeof(str_types) / sizeof(str_types[0]);

	std::string explanation;

	int points = nposm;
	std::string question;
	std::vector<std::string> options;
	std::set<int> correct_options;
	std::vector<std::string> vsize;

	texam_problem curr_solving(nposm, nposm, null_str, null_str);

	// someone might not want to follow the order of choice, fillin, and solving problems, 
	// and rstr-formatted string has been generated in this order. so sorting is not enforced.
	// std::map<int, std::vector<texam_problem> > map_problems = {{rexam_type_choice, std::vector<texam_problem>()},
	//	{rexam_type_fillin, std::vector<texam_problem>()},
	//	{rexam_type_solving, std::vector<texam_problem>()}
	// };
	while (true) {
		const trexam_str_type* this_type = nullptr;
		const char* ptr = nullptr;
		options.clear();
		correct_options.clear();

		ptr = utils::skip_blank_characters(this_start);
		if (ptr[0] == '\0') {
			return;
		}
		for (int at = 0; at < type_count; at ++) {
			const trexam_str_type& type = str_types[at];
			if (SDL_memcmp(ptr, type.str, type.size) == 0) {
				this_type = &type;
				break;
			}
		}
		if (this_type == nullptr) {
			return;
		}
		if (this_type->type == rexam_type_other && !curr_solving.valid()) {
			// type 'other' subproblem can only exist within solving problem.
			return;
		}
		ptr += this_type->size;

		int points;
		ptr = sscanf_int(ptr, points);
		if (ptr == nullptr) {
			return;
		}
		if (points <= 0) {
			return;
		}
		ptr = utils::skip_blank_characters(ptr);
		if (ptr[0] == '\0') {
			return;
		}
		ptr = sscanf_str(ptr, "[**&question##@]", true, "[/**&question##@]", question);
		if (ptr == nullptr) {
			return;
		}
		ptr = utils::skip_blank_characters(ptr);
		if (ptr[0] == '\0') {
			return;
		}
		if (this_type->type == rexam_type_choice) {
			std::string opt;
			for (int n = 0; n < 4; n ++) {
				ptr = sscanf_str(ptr, "[**&opt##@]", true, "[/**&opt##@]", opt);
				if (ptr == nullptr) {
					return;
				}
				options.push_back(opt);

				ptr = utils::skip_blank_characters(ptr);
				if (ptr[0] == '\0') {
					return;
				}
			}

		} else if (this_type->type == rexam_type_solving) {
			if (curr_solving.valid()) {
				// Handing on a solving-problem, and another solving-problem comes along, causing an error.
				return;
			}
			if (ptr[0] == '{') {
				// solving-problem containing sub problems
				curr_solving = texam_problem(this_type->type, points, question, null_str);
				this_start = ptr;
				continue;
			}
		}

		ptr = sscanf_str(ptr, "[**&expl##@]", true, "[/**&expl##@]", explanation);
		if (ptr == nullptr) {
			return;
		}

		std::vector<texam_problem>& desire_problems = curr_solving.valid()? curr_solving.subproblems: problems;
		// std::vector<texam_problem>& desire_problems = 
		//	curr_solving.valid()? curr_solving.subproblems: map_problems.find(this_type->type)->second;

		if (this_type->type == rexam_type_choice) {
			ptr = utils::skip_blank_characters(ptr);
			if (ptr[0] == '\0') {
				return;
			}
			std::string correct_option_str;
			ptr = sscanf_str_until_blank_characters(ptr, false, correct_option_str);
			vsize = utils::split(correct_option_str);
			for (std::vector<std::string>::const_iterator it = vsize.begin(); it != vsize.end(); ++ it) {
				const std::string& str = *it;
				if (str.size() != 1) {
					return;
				}
				const char ch = str.c_str()[0];
				if (ch >= 'A' && ch <= 'D') {
					correct_options.insert(ch - 'A' + 1);

				} else if (ch >= 'a' && ch <= 'd') {
					correct_options.insert(ch - 'a' + 1);

				} else {
					return;
				}
			}
			if (correct_options.empty()) {
				return;
			}

			int tex_cols = nposm;
			ptr = sscanf_extra(ptr, tex_cols);

			VALIDATE(options.size() == 4, null_str);
			desire_problems.push_back(texam_problem(this_type->type, points, question, explanation));
			desire_problems.back().set_extra_choice(options, correct_options, tex_cols);

		} else {
			desire_problems.push_back(texam_problem(this_type->type, points, question, explanation));
		}

		if (curr_solving.valid() && curr_solving.solving_points_equaled()) {
			VALIDATE(!curr_solving.subproblems.empty(), null_str);
			problems.push_back(curr_solving);
			curr_solving.clear();
		}

		if (ptr[0] == '\0') {
			return;
		}
		this_start = ptr;
	}
}

// 1) change courseware.question to exam_problems. and gether fail to 'fails'.
// 2) for 'fails', update 'empty' to corresponding courseware.question.
// return value: which are to proboem fail. and 'question' is changed.
std::vector<int> courseware_2_exam_problems(aplt::tcourseware& courseware, std::vector<texam_problem>& result)
{
	VALIDATE(courseware.type == aplt::tcourseware::type_exam, null_str);

	result.clear();

	std::vector<int> fails;

	int at = 0;
	std::vector<texam_problem> tmp;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, at ++) {
		const aplt::tcourseware::texercise& exercise = *it;
		if (!exercise.question.empty()) {
			rexam_str_2_problems(rexam_str_types_C, exercise.question.c_str(), exercise.question.size(), tmp);

		} else {
			tmp.clear();
		}
		if (!tmp.empty()) {
			result.push_back(tmp[0]);
		} else {
			result.push_back(texam_problem(nposm, nposm, null_str, null_str));
			fails.push_back(at);
		}

		// Why regenerate '.question' using to_rose_msg() even when rexam_str_2_problems() succeeds?  
		// When parsing from the '.question', rexam_str_2_problems() extracts the texam_problem. 
		// However, when regenerating the '.question' from texam_problem, whitespace may be lost. 
		// For example, there are blank lines between the problem's question and the explanation, 
		// but to_rose_msg() does not preserve these blank lines. 
		// Although the texam_problem remains the same in both cases, the string representations differ.
		courseware.exercises[at].question = result.back().to_rose_msg();
	}

	VALIDATE(result.size() == courseware.exercises.size(), null_str);
	return fails;
}

void tcourseware2::assign_tmp_courseware()
{
	tmp_courseware_ = courseware_;

	aplt::tcourseware& courseware = tmp_courseware_;
	if (curr_layer_ == UPLOAD_LAYER && !utils::is_uuid(courseware.uuid, false)) {
		courseware.uuid = utils::create_uuid(false);
	}

	if (courseware.type == aplt::tcourseware::type_exam) {
		courseware_2_exam_problems(courseware, tmp_problems_);
	} else {
		tmp_problems_.clear();
	}
}

void tcourseware2::load_cw_default()
{
	std::string stream;
	std::string filename = game_config::app_dir_root + "/cert/cw_default.cfg";
	{
		const int max_cw_default_cfg_size = 256 * 1024; // 256K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		VALIDATE(fsize > 0 && fsize <= max_cw_default_cfg_size, null_str);

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		VALIDATE(all_is_utf8, null_str);
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	aplt::read_config_ex(stream, true, top_cfg);

	const config& exam_default_cfg = top_cfg.child("exam_default");
	VALIDATE(exam_default_cfg && !exam_default_cfg.empty(), null_str);
	// if (exam_default_cfg && !exam_default_cfg.empty()) {
		exam_default_.tex_header = exam_default_cfg["tex_header"].str();
		exam_default_.tex_tail = exam_default_cfg["tex_tail"].str();
		exam_default_.content = exam_default_cfg["content"].str();
		exam_default_.exercise_section = exam_default_cfg["exercise_section"].str();
		exam_default_.exercise_answer = exam_default_cfg["exercise_answer"].str();
		VALIDATE(!exam_default_.exercise_answer.empty(), null_str);
	// }

	const std::string prompt_header_end_key = "% ======prompt header end";
	size_t pos = exam_default_.tex_header.find(prompt_header_end_key);
	VALIDATE(pos != std::string::npos, null_str);
	exam_default_.prompt_header = exam_default_.tex_header.substr(0, pos);
	exam_default_.prompt_header.append(exam_default_cfg["rstr_format"].str()).append("\n\n");
}

bool tcourseware2::did_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to)
{
/*
	if (!if_dirty_confirm_save(_("switching"))) {
		return false;
	}
*/
	return true;
}

void tcourseware2::did_report_item_changed(ttoggle_button& widget)
{
	int desire_layer = widget.at();

	if (desire_layer == UPLOAD_LAYER || desire_layer == COURSELIST_LAYER) {
		main_stack_->set_radio_layer(MAIN_TREE_LAYER);
	}

	toolbar_stack_->set_radio_layer(desire_layer);
	curr_layer_ = desire_layer;

	bool files_is_empty = false;
	courseware_files2_.clear();
	if (curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER) {
		collect_courseware(curr_layer_ == DOWNLOAD_LAYER, courseware_load_path(), courseware_files_);
		files_is_empty = courseware_files_.empty();

	} else {
		courseware_files_.clear();
		VALIDATE(curr_layer_ == COURSELIST_LAYER, null_str);
		std::vector<tcourseware_file>& files = courseware_files2_;
		const std::vector<aplt::tcourselist::tcourse>& courses = cfg_cpp_api_.courselist().courses();
		for (std::vector<aplt::tcourselist::tcourse>::const_iterator it = courses.begin(); it != courses.end(); ++ it) {
			const aplt::tcourselist::tcourse& course = *it;
			std::string dir_name = course.to_dir_name();
			std::string path = join_courseware_dir_courselist(course.uid, dir_name);
			tcourseware_distribution_vals local = get_distribution_vals(path);
			if (course.uid != COURSEWARE_UPLOAD_UID) {
				files.push_back(tbase_courseware::tcourseware_file(true, dir_name, local.username, local.uuid, local.ts, 0));

			} else {
				files.push_back(tbase_courseware::tcourseware_file(false, dir_name, local.username, local.uuid, local.ts, 0));
			}
		}
		files_is_empty = courseware_files2_.empty();
	}

	insert_courseware_widget_->set_visible(curr_layer_ == UPLOAD_LAYER? twidget::VISIBLE: twidget::INVISIBLE);
	favorite_widget_->set_visible(curr_layer_ != COURSELIST_LAYER? twidget::VISIBLE: twidget::INVISIBLE);

	reload_courseware_list(*file_list_);

	if (!files_is_empty) {
		file_list_->select_row(0);

	} else {
		VALIDATE(file_list_->cursel() == nullptr, null_str);
		refresh_toolbar_active(nullptr);

		bool dirty = courseware_dirty();
		save_widget_->set_active(dirty);

		clear_tree2();
	}

	input_toolbar_grid_->set_visible(desire_layer == UPLOAD_LAYER? twidget::VISIBLE: twidget::INVISIBLE);
	tree_msg_widget_->tb()->set_active(desire_layer == UPLOAD_LAYER);
}

void tcourseware2::pre_input_msg_layer(tgrid& grid)
{
	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&grid, "tree_msg", false, true);
	scroll_text_box->set_did_text_changed(std::bind(&tcourseware2::did_tree_msg_text_changed, this, _1));
	scroll_text_box->tb()->set_SDLK_RETURN_as_text_input(true);
	tree_msg_widget_ = scroll_text_box;
}

void tcourseware2::pre_input_exam_layer(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "type", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_type
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "prev", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_prev_or_next
			, this
			, std::ref(*button), fid_exam_prev));

	button = find_widget<tbutton>(&grid, "next", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_prev_or_next
			, this
			, std::ref(*button), fid_exam_next));

	button = find_widget<tbutton>(&grid, "tex_cols", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_tex_cols
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "move_down", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_down_or_insert_or_erase
			, this
			, std::ref(*button), fid_exam_down));

	button = find_widget<tbutton>(&grid, "insert", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_down_or_insert_or_erase
			, this
			, std::ref(*button), fid_exam_insert));

	button = find_widget<tbutton>(&grid, "erase", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_input_exam_down_or_insert_or_erase
			, this
			, std::ref(*button), fid_exam_erase));

	// tscroll_text_box
	ttext_box* text_box = find_widget<ttext_box>(&grid, "points", false, true);
	text_box->set_maximum_chars(2);
	text_box->set_did_text_changed(std::bind(&tcourseware2::did_input_exam_text_changed, this, _1, fid_exam_points));

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&grid, "question", false, true);
	scroll_text_box->tb()->set_SDLK_RETURN_as_text_input(true);
	scroll_text_box->set_did_text_changed(std::bind(&tcourseware2::did_input_exam_text_changed, this, _1, fid_exam_question));

	char widget_id[32];
	for (int at = 0; at < 4; at ++) {
		SDL_snprintf(widget_id, sizeof(widget_id), "correct_option_%c", 'a' + at);
		ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, widget_id, false, true);
		toggle->set_did_state_changed(std::bind(&tcourseware2::did_input_exam_state_changed, this, _1, fid_exam_correct_option_a + at));
	}

	

	for (int at = 0; at < 4; at ++) {
		SDL_snprintf(widget_id, sizeof(widget_id), "option_%c", 'a' + at);
		scroll_text_box = find_widget<tscroll_text_box>(&grid, widget_id, false, true);
		scroll_text_box->tb()->set_SDLK_RETURN_as_text_input(true);
		scroll_text_box->set_did_text_changed(std::bind(&tcourseware2::did_input_exam_text_changed, this, _1, fid_exam_option_a + at));
	}

	scroll_text_box = find_widget<tscroll_text_box>(&grid, "explanation", false, true);
	scroll_text_box->tb()->set_SDLK_RETURN_as_text_input(true);
	scroll_text_box->set_did_text_changed(std::bind(&tcourseware2::did_input_exam_text_changed, this, _1, fid_exam_expl));
}

void tcourseware2::pre_input_build_layer(tgrid& grid)
{
	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&grid, "build_msg", false, true);
	// scroll_text_box->set_did_text_changed(std::bind(&tcourseware2::did_tree_msg_text_changed, this, _1));
	scroll_text_box->tb()->set_SDLK_RETURN_as_text_input(true);
	build_msg_widget_ = scroll_text_box;
}

void tcourseware2::pre_main_tree(tgrid& grid)
{
	ttree* tree = find_widget<ttree>(&grid, "courseware_tree", false, true);
	tree->set_did_node_changed(std::bind(&tcourseware2::did_node_changed, this, _2));
	tree_widget_ = tree;

	v_line2_widget_ = find_widget<timage>(&grid, "v_line2", false, true);

	input_grid_ = find_widget<tgrid>(&grid, "input_grid", false, true);
	input_toolbar_grid_ = find_widget<tgrid>(&grid, "input_toolbar_grid", false, true);
	pre_input_toolbar_grid(*input_toolbar_grid_);

	tstack* stack = find_widget<tstack>(&grid, "input_stack", false, true);
	input_stack_ = stack;
	pre_input_msg_layer(*stack->layer(INPUT_MSG_LAYER));
	pre_input_exam_layer(*stack->layer(INPUT_EXAM_LAYER));
	pre_input_build_layer(*stack->layer(INPUT_BUILD_LAYER));

}

void tcourseware2::pre_main_list(tgrid& grid)
{
	tstack* stack = find_widget<tstack>(&grid, "item_list_stack", false, true);
	item_list_stack_ = stack;
	pre_first_second_list(*stack->layer(FIRST_LIST_LAYER), true);
	pre_first_second_list(*stack->layer(SECOND_LIST_LAYER), false);
}

void tcourseware2::pre_first_second_list(tgrid& grid, bool first)
{
	tlistbox* list = find_widget<tlistbox>(&grid, first? "first_list": "second_list", false, true);
	list->enable_select(false);
	list->set_did_row_changed(std::bind(&tcourseware2::did_list_row_changed, this, _1, _2, first? FIRST_LIST_LAYER: SECOND_LIST_LAYER));
	if (first) {
		first_list_ = list;
	} else {
		second_list_ = list;
	}
}

void tcourseware2::pre_toolbar_upload(tgrid& grid)
{
}

void tcourseware2::pre_input_toolbar_grid(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "change_cw_type", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_change_cw_type
			, this
			, std::ref(*button)));
	button->set_border(null_str);
	tb_upload_change_cw_widget_ = button;

	button = find_widget<tbutton>(&grid, "tree_save", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_save
			, this
			, std::ref(*button)));
	button->set_active(false);
	save_widget_ = button;

	button = find_widget<tbutton>(&grid, "upload_rsp", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tcourseware2::click_upload_rsp
			, this, std::ref(*button)));
	upload_widget_ = button;

	button = find_widget<tbutton>(&grid, "pdf", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tcourseware2::click_pdf
			, this, std::ref(*button)));
	pdf_widget_ = button;

	button = find_widget<tbutton>(&grid, "ai_exercise", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_ai_exercise
			, this
			, std::ref(*button), insert_exercise));
	ai_exercise_widget_ = button;

	button = find_widget<tbutton>(&grid, "fill_default", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_fill_default
			, this
			, std::ref(*button)));
	fill_default_widget_ = button;

	button = find_widget<tbutton>(&grid, "insert_keypoint", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_insert
			, this
			, std::ref(*button), insert_keypoint));
	insert_keypoint_widget_ = button;

	button = find_widget<tbutton>(&grid, "insert_exercise", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_insert
			, this
			, std::ref(*button), insert_exercise));
	insert_exercise_widget_ = button;

	button = find_widget<tbutton>(&grid, "move_down", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_move_down_or_up
			, this
			, std::ref(*button), true));
	move_down_widget_ = button;

	button = find_widget<tbutton>(&grid, "move_up", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_move_down_or_up
			, this
			, std::ref(*button), false));
	move_up_widget_ = button;

	button = find_widget<tbutton>(&grid, "erase", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tcourseware2::click_erase
			, this
			, std::ref(*button)));
	erase_widget_ = button;
}

void tcourseware2::pre_toolbar_download(tgrid& grid)
{
	ttext_box2* text_box2 = new ttext_box2(*window_, *find_widget<tcontrol>(&grid, "find_user", false, true), "textbox", null_str, false, "misc/find.png", ttext_box2::button_always_visible);
	const int max_chars = 16;
	text_box2->text_box()->set_maximum_chars(max_chars);
	text_box2->text_box()->set_placeholder(_("Username to search for"));
	text_box2->set_did_text_changed(std::bind(&tcourseware2::did_find_user_text_box_changed, this, _1));
	connect_signal_mouse_left_click(
			*text_box2->button()
		, std::bind(
			&tcourseware2::click_find_user
			, this, std::ref(*text_box2->button())));
	find_user_widget_ = text_box2;
}

void tcourseware2::pre_toolbar_courselist(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "listen", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcourseware2::click_listen
			, this, std::ref(*button)));
	listen_widget_ = button;
}

#pragma pack(1)

struct trsp_material80bytes {
	int64_t ts;
	int32_t type;
	char desc[RSP_MAXDESCBYTES + 1];
	uint32_t reserve0;
	uint32_t reserve1;
	uint32_t reserve2;
};

#pragma pack()

bool did_write_rsp_material(tfile& file, const std::string& bundleid, const version_info& rose_version, int type, const std::string& desc,
	const std::string& uuid, const std::string& material_zip, int64_t& rsp_ts)
{
	VALIDATE(type >= rspmaterialtype_min && type <= rspmaterialtype_max, null_str);
	VALIDATE(utils::is_uuid(uuid, false), null_str);

	tfile src(material_zip, GENERIC_READ, OPEN_EXISTING);
	const int fsize = posix_fsize(src.fp);
	VALIDATE(fsize > 0, null_str);

	const int one_block = 1024 * 1024;
	src.resize_data(one_block);

	// 3.1 trsp_header
	rsp_ts = time(nullptr);
	// part(1/5): rsp header
	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_material));
	header.version = SDL_FOURCC(0, 0, 0, RSP_MATERIAL_VER);

	// const time_t t = ts; // for xcode(ios)
	// tm* timeptr = localtime(&t);
	// VALIDATE(timeptr != nullptr, null_str);
	// int build_date = (1900 + timeptr->tm_year) * 10000 + (timeptr->tm_mon + 1) * 100 + timeptr->tm_mday;
	header.build_date = ts_2_build_date(rsp_ts);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.zip_size = sizeof(trsp_material113bytes) + fsize; // will overwrite later.
	posix_fwrite(file.fp, &header, sizeof(header));

	// 3.2 material80bytes
	trsp_material113bytes material_header;

	memset(&material_header, 0, sizeof(material_header));
	material_header.ts = rsp_ts;
	material_header.type = type;
	SDL_strlcpy(material_header.desc, desc.c_str(), sizeof(material_header.desc));
	memcpy(material_header.uuid, uuid.c_str(), UUID_STR_LEN);
	posix_fwrite(file.fp, &material_header, sizeof(material_header));

	// 3.3 material zip data
	int pos = 0;
	while (pos < fsize) {
		int bytes = one_block;
		if (pos + bytes > fsize) {
			bytes = fsize - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(file.fp, src.data, bytes);

		pos += bytes;
	}
	return true;
}

bool tcourseware2::generate_courseware_rsp(const std::string& desc, const std::string& courseware_name, const std::string& uuid, int64_t& rsp_ts)
{
	VALIDATE(curr_layer_ == UPLOAD_LAYER, null_str);
	VALIDATE(utils::is_uuid(uuid, false), null_str);

	std::string err;
	utils::string_map symbols;

	std::set<std::string> src_dirs;
	src_dirs.insert(courseware_name);
/*
	src_dirs.insert("cert");
	src_dirs.insert("gui");
	src_dirs.insert("images");
	src_dirs.insert("libs");
	src_dirs.insert("lua");
	src_dirs.insert("moveit");
	src_dirs.insert("music");
	src_dirs.insert("po");
	src_dirs.insert("proto");
	src_dirs.insert("sounds");
	src_dirs.insert("tflites");
	src_dirs.insert("translations");
	src_dirs.insert("xwml");
*/
	std::set<std::string> src_files;
/*
	src_files.insert(APPLET_ICON);
	src_files.insert("settings.cfg");
*/

	std::vector<std::string> input;
	const std::string exercise_upload_path_plus1 = upload_path_ + "/";
	for (std::set<std::string>::const_iterator it = src_dirs.begin(); it != src_dirs.end(); ++ it) {
		const std::string path = exercise_upload_path_plus1 + *it;
		if (SDL_IsDirectory(path.c_str())) {
			input.push_back(path);
		}
	}
	for (std::set<std::string>::const_iterator it = src_files.begin(); it != src_files.end(); ++ it) {
		const std::string path = exercise_upload_path_plus1 + *it;
		if (SDL_IsFile(path.c_str())) {
			input.push_back(path);
		} else {
			symbols["file"] = *it;
			gui2::show_message(null_str, vgettext2("$file isn't existed, generate fail", symbols));
			return false;
		}
	}

	const std::string courseware_dir = exercise_upload_path_plus1 + courseware_name;
	// Do not upload distribution.cfg. However, the current minizip::zip_file() cannot exclude a specific file, 
	// so have to move it to another directory first and move it back after the zip is complete.
	const std::string distribution_cfg_file = courseware_dir + "/" + APLT_DISTRIBUTION_CFG;
	const std::string tmp_distribution = game_config::preferences_dir + "/__temp_distribution.cfg";
	SDL_MoveFile(distribution_cfg_file.c_str(), tmp_distribution.c_str());

	const std::string temp_zip = game_config::preferences_dir + "/__temp.zip";
	bool fok = minizip::zip_file(temp_zip, input, null_str);
	SDL_MoveFile(tmp_distribution.c_str(), distribution_cfg_file.c_str());
	if (!fok) {
		symbols["courseware"] = courseware_name;
		symbols["src"] = courseware_dir; // exercise_upload_path_plus1 + courseware_name
		symbols["dst"] = temp_zip;
		symbols["result"] = fok? _("Success"): _("Fail");
		err = vgettext2("Generate rsp. [1/2]Zip $courseware from \"$src\" to \"$dst\", $result!", symbols);
		if (game_config::os == os_windows) {
			err = utils::normalize_path(err, true);
		}
		gui2::show_message(null_str, err);
		return false;
	}

	const std::string courseware_rsp = exercise_upload_path_plus1 + courseware_name + ".rsp";

	const std::string bundleid = "matl.leagor.courseware";
	tsha1writer sha1file(courseware_rsp, nposm, std::bind(&gui2::did_write_rsp_material, _1, bundleid, std::ref(game_config::rose_version), 
		rspmaterialtype_courseware, desc, uuid, temp_zip, std::ref(rsp_ts)));
	sha1file.write();

	// SDL_DeleteFiles(temp_zip.c_str());
/*
	{
		symbols["courseware"] = courseware_name;
		symbols["src"] = exercise_upload_path_plus1 + courseware_name;
		symbols["dst"] = courseware_rsp;
		symbols["result"] = fok? _("Success"): _("Fail");
		err = vgettext2("Generate $courseware|'rsp from \"$src\" to \"$dst\", $result!", symbols);
		if (game_config::os == os_windows) {
			err = utils::normalize_path(err, true);
		}
		gui2::show_message(null_str, err);
	}
*/
	return true;
}

bool tcourseware2::upload_courseware_rsp(const std::string& courseware_name, const std::string& uuid, int64_t rsp_ts)
{
	VALIDATE(curr_layer_ == UPLOAD_LAYER, null_str);

	user_not_valid_try_again();
	if (!current_user.valid()) {
		gui2::show_message(null_str, _("Not logged in, cannot upload"));
		return false;
	}

	const std::string exercise_upload_path = upload_path_;

	const std::string exercise_upload_path_plus1 = exercise_upload_path + "/";
	const std::string courseware_rsp = exercise_upload_path_plus1 + courseware_name + ".rsp";

	const std::string remote_src = courseware_name + ".rsp";
	bool ret = net::upload_materialrsp(current_user.sessionid, remote_src, uuid, courseware_rsp);
	if (!ret) {
		return false;
	}

	aplt::tcswamp_user user;
	user.uid = current_user.uid;
	user.username = current_user.username;

	aplt::tcswamp_material material;
	material.file = remote_src;
	material.type = rspmaterialtype_courseware;
	material.uuid = uuid;
	material.time = rsp_ts;
	material.fsize = file_size(courseware_rsp);

	const std::string courseware_path = join_courseware_dir(courseware_name);
	write_distribution_cfg(courseware_path, user, material);

	return true;
}

void tcourseware2::save_main_cfg(const std::string& filename, const aplt::tcourseware& courseware)
{
	VALIDATE(utils::extract_file(filename) == "main.cfg", null_str);

	config top_cfg;

	config& exercise_cfg = top_cfg.add_child("courseware");
	courseware.to_cfg(exercise_cfg);

	std::stringstream out;
	if (!top_cfg.empty()) {
		write(out, top_cfg);
	}
	VALIDATE(!out.str().empty(), null_str);

	write_file(filename, out.str().c_str(), out.str().size());
}

void tcourseware2::refresh_title(const std::string& label)
{
	int max_chars = 18;
	bool ellipsis = true;
	std::string label2 = utils::truncate_to_max_chars2(label, max_chars, ellipsis);

	std::stringstream ss;
	if (editing_ && curr_layer_ != UPLOAD_LAYER) {
		ss << "(" << _("Read only") << ")";
	}
	ss << label2;
	title_widget_->set_label(ss.str());
}

void tcourseware2::refresh_courselist_item_label()
{
	std::stringstream ss;
	ss << _("courseware^courselist layer");
	ss << "(" << cfg_cpp_api_.courselist().courses().size() << ")";

	courselist_item_widget_->set_label(ss.str());
}

void tcourseware2::refresh_enter_edit_widget_label()
{
	enter_edit_widget_->set_label(editing_? "misc/exit_edit.png": "misc/enter_edit.png");
}

void tcourseware2::enter_or_exit_edit(bool from_click)
{
	if (from_click && editing_) {
		if (!if_dirty_confirm_save(_("exiting"))) {
			return;
		}
		if (courseware_dirty()) {
			// dirty and select don't save. restroe from file
			assign_tmp_courseware();
/*
			tmp_courseware_ = courseware_;
			if (curr_layer_ == UPLOAD_LAYER && !utils::is_uuid(tmp_courseware_.uuid, false)) {
				tmp_courseware_.uuid = utils::create_uuid(false);
			}
*/
			tcookie3f cookie3f = courseware_update_to_tree_with_cursel(tmp_courseware_);

			post_tree_recreated(cookie3f);
		}
	}

	if (!editing_) {
		// enter edit
		back_widget_->set_visible(twidget::INVISIBLE);

		file_list_->set_visible(twidget::INVISIBLE);
		may_invisible_cw_tree_top_grid_->set_visible(twidget::INVISIBLE);
		
		toolbar_stack_->set_radio_layer(SPACER_LAYER);
		toolbar_stack_->set_visible(twidget::INVISIBLE);

		v_line2_widget_->set_visible(twidget::VISIBLE);
		input_grid_->set_visible(twidget::VISIBLE);
		
	} else {
		// exit edit
		back_widget_->set_visible(twidget::VISIBLE);

		file_list_->set_visible(twidget::VISIBLE);
		may_invisible_cw_tree_top_grid_->set_visible(twidget::VISIBLE);

		toolbar_stack_->set_visible(twidget::VISIBLE);
		toolbar_stack_->set_radio_layer(curr_layer_);

		v_line2_widget_->set_visible(twidget::INVISIBLE);
		input_grid_->set_visible(twidget::INVISIBLE);
	}

	editing_ = !editing_;
	for (int at = 0; at < BAR_REPORT_LAYERS; at ++) {
		if (at != curr_layer_) {
			bar_report_->set_item_visible(at, !editing_);
		}
	}

	refresh_enter_edit_widget_label();
	refresh_change_cw_type_label();

	if (editing_) {
		if (curr_layer_ == UPLOAD_LAYER) {
			VALIDATE(utils::is_uuid(tmp_courseware_.uuid, false), null_str);
		}

		ttree_node* cursel = tree_widget_->cursel();
		if (cursel != nullptr) {
			did_node_changed(*cursel);
		}
	}

	refresh_title(tmp_courseware_.title);
}

void tcourseware2::did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer)
{
	VALIDATE(curr_layer_ == DOWNLOAD_LAYER, null_str);

	const int cookie = uint64_2_int(row.cookie());

	utils::string_map symbols;
	std::stringstream ss;
	if (curr_list_task_type_ == listtasktype_find_user) {
		if (list_state_ == list_state_users) {
			VALIDATE(layer == FIRST_LIST_LAYER, null_str);
			VALIDATE(list.rows() == (int)curr_finduser_result_.size(), null_str);

			const net::tcswamp_finduser_result& user = curr_finduser_result_[cookie];
			if (user.materialcount == 0) {
				return;
			}

			handle_list_state_users_bh(user);

		} else if (list_state_ == list_state_cswamp_coursewares) {
			VALIDATE(layer == SECOND_LIST_LAYER, null_str);
			VALIDATE(list.rows() == (int)curr_cswamp_user_.materials.size(), null_str);

			const aplt::tcswamp_material& material = curr_cswamp_user_.materials[cookie];
			handle_list_state_cswamp_coursewares_bh(material);
		}

	} else {
		VALIDATE(false, null_str);
/*
		VALIDATE(ai_curr_task_type_ == ai_tasktype_aiagent, null_str);
		if (ai_curr_aiagent_prompts_.empty()) {
			VALIDATE(layer == POSITIONS_LAYER, null_str);

			VALIDATE(list.rows() == (int)ai_curr_timings_.size(), null_str);
			const tcand_task3& task3 = ai_curr_timings_[cookie];

			const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);
			VALIDATE(lib.get() == nullptr, null_str);


			aplt::ttask_api* task_api = nullptr;
			std::string err_msg;
			tauto_destruct_executor destruct_executor(std::bind(&tcenter::ai_did_load_task_api_quited, this, std::ref(err_msg)));
			{
				drivers_.set_special_applet(apltsotype_aiagent_task, &task3.aplt);
				if (lib.get() == nullptr) {
					err_msg = vgettext2("Cannot load applet '$aplt_task' located in *. so.", symbols);
					// set_status_task_label(ai_curr_task_type_, err_msg);
					return;
				}
				trose_library* rose_lib = lib.get();
				void* v_task = nullptr;
				if (rose_lib->create_task_api != nullptr) {
					v_task = rose_lib->create_task_api();
				}
				if (v_task == nullptr) {
					err_msg = _("Not implete 'aplt_create_task_api' method.");
					// set_status_task_label(ai_curr_task_type_, err_msg);
					return;
				}
				task_api = reinterpret_cast<aplt::ttask_api*>(v_task);
				if (task_api->aiagent == nullptr) {
					delete task_api;
					err_msg = _("Not implete aplt::taiagent_api object.");
					return;
				}
			}
			ai_curr_task3_ = &task3;
			ai_curr_aiagent_task_api_ = task_api;
			const std::vector<aplt::taiagent_api::tprompt>& prompts = task_api->aiagent->prompts(task3.cfg_task.id);
			// ai_curr_aiagent_prompts_ = prompts;

			for (std::vector<aplt::taiagent_api::tprompt>::const_iterator it = prompts.begin(); it != prompts.end(); ++ it) {
				ai_curr_aiagent_prompts_.push_back(aplt::taiagent_api::tprompt(it->prompt, it->note));
			}

			std::vector<titem3> items;
			ai_get_aiagent_prompt_items(items);

			reload_items_list(*timing_task_list_, items);
			item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);

		} else {
			VALIDATE(cookie < ai_curr_aiagent_prompts_.size(), null_str);
			ai_input_->set_label(ai_curr_aiagent_prompts_[cookie].prompt);
		}
*/
	}
}

void tcourseware2::reload_items_list(tlistbox& list, const std::vector<tcode3>& items)
{
	list.clear();

	std::map<std::string, std::string> data;

	std::stringstream ss;
	
	for (std::vector<tcode3>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const tcode3& item = *it;
		
		data["major_label"] = item.id;
		data["minor_label"] = item.name;

		ttoggle_panel& panel = list.insert_row(data);
		panel.set_cookie(item.code);
	}
}

void tcourseware2::clear_list_task()
{
	curr_list_task_type_ = nposm;

	list_state_ = nposm;
	curr_finduser_result_.clear();
	curr_cswamp_user_.clear();

	first_list_->clear();
	second_list_->clear();
	item_list_stack_->set_radio_layer(def_item_list_layer_);
}

void tcourseware2::get_finduser_result_items(const std::vector<net::tcswamp_finduser_result>& result, std::vector<tcode3>& items)
{
	items.clear();

	utils::string_map symbols;
	for (std::vector<net::tcswamp_finduser_result>::const_iterator it = result.begin(); it != result.end(); ++ it) {
		const net::tcswamp_finduser_result& user = *it;
		symbols["count"] = str_cast(user.materialcount);
		items.push_back(tcode3(items.size(), user.username, vgettext2("Courseware: $count", symbols)));
	}
}

void tcourseware2::get_cswamp_user_items(const aplt::tcswamp_user& user, std::vector<tcode3>& items)
{
	items.clear();

	std::stringstream ss;
	utils::string_map symbols;
	for (std::vector<aplt::tcswamp_material>::const_iterator it = user.materials.begin(); it != user.materials.end(); ++ it) {
		const aplt::tcswamp_material& material = *it;
		ss.str("");
		ss << utils::format_time_ymdhms(material.time);
		ss << " UUID: " << material.uuid;
		items.push_back(tcode3(items.size(), utils::file_stem_name(material.file), ss.str()));
	}
}

void tcourseware2::did_find_user_text_box_changed(ttext_box& widget)
{
	VALIDATE(curr_layer_ == DOWNLOAD_LAYER, null_str);
}

void tcourseware2::click_find_user(tbutton& widget)
{
	VALIDATE(curr_layer_ == DOWNLOAD_LAYER, null_str);

	clear_list_task();
	curr_list_task_type_ = listtasktype_find_user;


	const std::string& name = find_user_widget_->text_box()->label();

	std::vector<net::tcswamp_finduser_result>& result = curr_finduser_result_;
	{
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_finduser, _1, nposm, name,
			false, std::ref(result)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Find user"), 1000);
		if (!ret) {
			return;
		}
	}


	std::vector<tcode3> items;
	get_finduser_result_items(result, items);

	reload_items_list(*first_list_, items);
	item_list_stack_->set_radio_layer(FIRST_LIST_LAYER);

	if (curr_main_layer_ != MAIN_LIST_LAYER) {
		main_stack_->set_radio_layer(MAIN_LIST_LAYER);
	}

	list_state_ = list_state_users;
}

void tcourseware2::handle_list_state_users_bh(const net::tcswamp_finduser_result& user)
{
	VALIDATE(curr_list_task_type_ == listtasktype_find_user, null_str);
	VALIDATE(list_state_ == list_state_users, null_str);
	VALIDATE(user.materialcount > 0, null_str);

	aplt::tcswamp_user& cswamp_user = curr_cswamp_user_;
	{
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getuserinfo, _1, user.uid,
			false, std::ref(cswamp_user)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Get user info"), 1000);
		if (!ret) {
			return;
		}
	}

	std::vector<tcode3> items;
	get_cswamp_user_items(cswamp_user, items);
	reload_items_list(*second_list_, items);
	item_list_stack_->set_radio_layer(SECOND_LIST_LAYER);
	list_state_ = list_state_cswamp_coursewares;
}

void tcourseware2::write_distribution_cfg(const std::string& path, const aplt::tcswamp_user& user, const aplt::tcswamp_material& material) const
{
	VALIDATE(user.valid(), null_str);
	VALIDATE(material.valid2(), null_str);
	
	config cfg;
	cfg["uid"].from_int64(user.uid);
	cfg["username"].from_string(user.username, true);
	cfg["uuid"].from_string(material.uuid, true);
	cfg["ts"].from_int64(material.time);

	std::stringstream out;
	if (!cfg.empty()) {
		write(out, cfg);
	}
	VALIDATE(!out.str().empty(), null_str);

	write_file(path + "/" + APLT_DISTRIBUTION_CFG, out.str().c_str(), out.str().size());
}

std::string courseware_dir_from_uid_uuid(int64 uid, const std::string& uuid, const std::string& save_path, 
	const std::map<std::string, tbase_courseware::tcourseware_file>& files)
{
	for (std::map<std::string, tbase_courseware::tcourseware_file>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const tbase_courseware::tcourseware_file& file = it->second;
		if (file.uid == uid && file.uuid == uuid) {
			return it->first;
		}
	}
	return null_str;
}

void tcourseware2::handle_list_state_cswamp_coursewares_bh(const aplt::tcswamp_material& material)
{
	VALIDATE(curr_list_task_type_ == listtasktype_find_user, null_str);
	VALIDATE(list_state_ == list_state_cswamp_coursewares, null_str);
	VALIDATE(curr_cswamp_user_.valid(), null_str);

	const aplt::tcswamp_user& user = curr_cswamp_user_;
	const std::string courseware_name = utils::file_stem_name(material.file);
	std::string old_courseware_dir;

	utils::string_map symbols;
	symbols["upload"] = _("courseware^upload layer");
	symbols["download"] = _("courseware^download layer");
	std::string msg;
	bool to_download = true;
	if (user.username == current_user.username) {
		if (!current_user.valid()) {
			msg = vgettext2("the username is the same, but not logged in, $upload, $download", symbols);
			if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, null_str, null_str,
				_("Login first"), _("Continue downloading")) == gui2::twindow::OK) {
				return;
			}
		} else {
			const std::string courseware_dir_name = courseware_name;
			const std::string courseware_dir = join_courseware_dir2(upload_path_, courseware_dir_name);

			if (SDL_IsDirectory(courseware_dir.c_str())) {
				msg = vgettext2("Duplicate courseware detected in the '$upload' directory. Downloading will overwrite it. Do you want to continue?", symbols);
				if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
					return;
				}
			}
			to_download = false;
		}
	}

	std::string courseware_dir_name;
	std::string courseware_dir;
	std::string save_path;
	if (to_download) {
		courseware_dir_name = utils::join_app_prefix_id(str_cast(user.uid), courseware_name);
		courseware_dir = join_courseware_dir(courseware_dir_name);
		save_path = download_path_;

	} else {
		courseware_dir_name = courseware_name;
		courseware_dir = join_courseware_dir2(upload_path_, courseware_dir_name);
		save_path = upload_path_;
	}
	VALIDATE(!courseware_dir_name.empty() && !courseware_dir.empty() && !save_path.empty(), null_str);

	const std::string cfgfile = join_main_cfg_filename(courseware_dir_name);
	bool files_dirty = true;
	bool require_donwload = true;

	tcourseware_distribution_vals local = get_distribution_vals(courseware_dir);
	if (to_download) {
		require_donwload = local.uid != user.uid || local.ts != material.time || local.uuid != material.uuid;
		if (!require_donwload) {
			aplt::tcourseware courseware;

			load_courseware_cfg2(cfgfile, courseware);
			require_donwload = !courseware.valid();
		}
	}

	if (require_donwload) {
		const std::string remote_src = material.file;
		const std::string courseware_rsp = save_path + "/" + material.file;
		bool ret = net::download_materialrsp(remote_src, material.uuid, user.uid, courseware_rsp);
		if (!ret) {
			return;
		}

		if (file_size(courseware_rsp) == 0) {
			return;
		}

		// 1/5: delete local dir (<uid>__<courseware_name>)
		SDL_DeleteFiles(courseware_dir.c_str());
		// The directory where material.uuid is located may not be @courseware_dir.
		std::map<std::string, tcourseware_file> files;
		std::string same_uuid_dir;
		if (to_download) {
			files = courseware_files_;
			if (files.count(courseware_dir_name) != 0) {
				files.erase(files.find(courseware_dir_name));
			}
		} else {
			collect_courseware(false, save_path, files);
		}
		same_uuid_dir = courseware_dir_from_uid_uuid(to_download? user.uid: COURSEWARE_UPLOAD_UID, 
			material.uuid, save_path, files);
		if (!same_uuid_dir.empty() && same_uuid_dir != courseware_dir) {
			const std::string same_uuid_dir2 = join_courseware_dir2(save_path, same_uuid_dir);
			SDL_DeleteFiles(same_uuid_dir2.c_str());
		}

		// 2/5: unzip *.rsp to <courseware_name>
		minizip::unzip_file(courseware_rsp, save_path.c_str(), null_str, null_str);

		// 3/5: delete *.rsp
		SDL_DeleteFiles(courseware_rsp.c_str());
 
		// 4/5: raname <courseware_name> to (<uid>__<courseware_name>)
		const std::string upzip_courseware_file = join_courseware_dir2(save_path, courseware_name);
		if (utils::extract_file(upzip_courseware_file) != courseware_dir_name) {
			SDL_RenameFile(upzip_courseware_file.c_str(), courseware_dir_name.c_str());
		}

		// 5/5: generate <courseware_dir>/distribution.cfg
		write_distribution_cfg(courseware_dir, user, material);

		if (to_download) {
			if (to_download) {
				// SDL_dirent stat;
				// SDL_bool ret2 = SDL_GetStat(cfgfile.c_str(), &stat);
				// int64_t local_time = ret2? stat.mtime: 0;
				courseware_files_[courseware_dir_name] = tbase_courseware::tcourseware_file(true, courseware_dir_name, user.username, material.uuid, material.time, 0);
			}
		}

	} else if (local.username != user.username) {
		VALIDATE(to_download, null_str);
		write_distribution_cfg(courseware_dir, user, material);

	} else {
		VALIDATE(to_download, null_str);
		files_dirty = false;
	}

	if (to_download) {
		VALIDATE(courseware_files_.count(courseware_dir_name) != 0, null_str);

		if (files_dirty) {
			reload_courseware_list(*file_list_);
		}

		item_list_stack_->set_radio_layer(FIRST_LIST_LAYER);
		main_stack_->set_radio_layer(MAIN_TREE_LAYER);

	} else {
		std::string yes_label = vgettext2("Go to $upload", symbols);
		std::string no_label = vgettext2("Stay in $download", symbols);
		msg = vgettext2("It has been saved to the '$upload' directory. Do you want to go to '$upload' or stay in '$download'?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, null_str, null_str,
			yes_label, no_label) != gui2::twindow::OK) {
			return;
		}
		bar_report_->select_item(UPLOAD_LAYER);
		VALIDATE(courseware_files_.count(courseware_dir_name) != 0, null_str);
	}

	{
		std::map<std::string, tcourseware_file>::iterator it = courseware_files_.find(courseware_dir_name);
		int at = std::distance(courseware_files_.begin(), it);
		file_list_->select_row(at);
	}
}

static bool did_walk_curr_dir_subdirs(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& files, const std::string& root, bool include_root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		// const std::string name = utils::lowercase(dirent->name);
		if (include_root) {
			files.insert(root + "/" + dirent->name);
		} else {
			files.insert(dirent->name);
		}
	}
	return true;
}

static void collect_curr_dir_dirs(const std::string& root, std::set<std::string>& files, bool include_root)
{
	files.clear();

	walk_dir(root, false, std::bind(&did_walk_curr_dir_subdirs, _1, _2, std::ref(files), std::ref(root), include_root));
}

void tcourseware2::evaluate_uuids()
{
	VALIDATE(game_config::os == os_windows, null_str);

	const bool evaluate = false;
	std::vector<std::string> files;
	if (evaluate) {
		files.push_back("nouuid_12.rsp");
		files.push_back("nouuid_16.rsp");
		files.push_back("nouuid_17.rsp");
		files.push_back("nouuid_18.rsp");
		files.push_back("nouuid_19.rsp");
		files.push_back("nouuid_20.rsp");
		files.push_back("nouuid_21.rsp");

	} else {
		files.push_back("2b0648594e0a48018a903ace16979201_18.rsp");
		files.push_back("45d6eee3b58f4f9ab38a374012052367_12.rsp");
		files.push_back("77d828040d01454eac9167756e812b85_19.rsp");
		files.push_back("1255ec652c414a6eb63fd063eda8486d_21.rsp");
		files.push_back("410341f1c61240e885617ba42b6d72a8_17.rsp");
		files.push_back("f5f8c12b83894f6d9ab49e8352f88165_16.rsp");
		files.push_back("fb5954f9c3574b078a3a30cb02d9f5e5_20.rsp");
	}

	const std::string prefix = "nouuid";

	std::string save_path = upload_path_;

	aplt::tcourseware courseware;
	std::set<std::string> subdirs;
	collect_curr_dir_dirs(save_path, subdirs, false);
	std::set<std::string> subdirs2;
	SDL_Log("---%s---", evaluate? "1th: evaluate": "2th: verify");
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& short_filename = *it;
		const std::string courseware_rsp = save_path + "/" + short_filename;
		VALIDATE(file_size(courseware_rsp) > 0, null_str);

		minizip::unzip_file(courseware_rsp, save_path.c_str(), null_str, null_str);
		collect_curr_dir_dirs(save_path, subdirs2, false);
		std::string courseware_dir_name;
		for (std::set<std::string>::const_iterator it2 = subdirs2.begin(); it2 != subdirs2.end(); ++ it2) {
			const std::string& subdir = *it2;
			if (subdirs.count(subdir) == 0) {
				VALIDATE(courseware_dir_name.empty(), null_str);
				courseware_dir_name = subdir;
			}
		}
		VALIDATE(!courseware_dir_name.empty(), null_str);
		subdirs = subdirs2;

		const std::string cfgfile = join_main_cfg_filename2(save_path, courseware_dir_name);

		load_courseware_cfg2(cfgfile, courseware);
		VALIDATE(courseware.valid(), null_str);
		if (evaluate) {
			VALIDATE(!utils::is_uuid(courseware.uuid, false), null_str);

			const std::string uuid = utils::create_uuid(false);
			SDL_Log("%s, evaludate uuid: %s ---> %s", courseware_dir_name.c_str(), uuid.c_str(), short_filename.c_str());

			courseware.uuid = uuid;
			save_main_cfg(cfgfile, courseware);

			const std::string desc;
			int64_t rsp_ts = 0;
			bool ret = generate_courseware_rsp(desc, courseware_dir_name, tmp_courseware_.uuid, rsp_ts);
			VALIDATE(ret, null_str);

			std::string new_rspfile = save_path + "/" + courseware_dir_name + ".rsp";
			VALIDATE(short_filename.find(prefix) == 0, null_str);
			std::string newname = uuid + short_filename.substr(prefix.size());
			SDL_RenameFile(new_rspfile.c_str(), newname.c_str());

		} else {
			VALIDATE(utils::is_uuid(courseware.uuid, false), null_str);
			VALIDATE(short_filename.size() > UUID_STR_LEN, null_str);
			const std::string uuid_in_title = short_filename.substr(0, UUID_STR_LEN);
			VALIDATE(utils::is_uuid(uuid_in_title, false), null_str);
			VALIDATE(uuid_in_title == courseware.uuid, null_str);
			SDL_Log("%s, verify: same uuid: %s, %s", courseware_dir_name.c_str(), courseware.uuid.c_str(), short_filename.c_str());
		}
	}
	SDL_Log("------------");
}

void tcourseware2::click_save(tbutton& widget)
{
	do_save(widget);
}

void tcourseware2::refresh_change_cw_type_label()
{
	aplt::tcourseware& courseware = tmp_courseware_;

	VALIDATE(aplt::tcourseware::types.count(courseware.type) != 0, null_str);
	tb_upload_change_cw_widget_->set_label(aplt::tcourseware::types.find(courseware.type)->second.name);
}

void tcourseware2::click_change_cw_type(tbutton& widget)
{
	aplt::tcourseware& courseware = tmp_courseware_;

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (std::map<int, tcode3>::const_iterator it = aplt::tcourseware::types.begin(); it != aplt::tcourseware::types.end(); ++ it) {
		const tcode3& cw_type = it->second;

		items.push_back(gui2::tmenu::titem(cw_type.name, cw_type.code));
		if (cw_type.code == courseware.type) {
			 initial_sel = cw_type.code;
		}
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}


	const int new_value = dlg.selected_val();

	courseware.type = new_value;

	tmp_problems_.clear();
	if (new_value == aplt::tcourseware::type_exam) {
		courseware_2_exam_problems(courseware, tmp_problems_);
	}
	refresh_change_cw_type_label();

	// refresh tree
	tcookie3f cookie3f = courseware_update_to_tree_with_cursel(courseware);
/*
	ttree& tree = *tree_widget_;
	ttree_node* node = tree.cursel();
	VALIDATE(node != nullptr, null_str);

	uint64_t cookie = node->cookie();
	tcookie3f cookie3f(cookie);

	courseware_update_to_tree(courseware);

	node = tree.get_root_node().find_node_from_cookie(cookie3f.u64);
	VALIDATE(node != nullptr, null_str);
	tree.select_node(node);
*/
	post_tree_recreated(cookie3f);
}

void tcourseware2::click_upload_rsp(tbutton& widget)
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);

	if (courseware_dirty() && !gui_courseware_is_valid(nullptr)) {
		const std::string msg = _("The courseware data has an error and cannot be uploaded.");
		gui2::show_message(null_str, msg);
		return;
	}

	if (courseware_dirty()) {
		const std::string msg = _("The courseware data has been modified. Save first before uploading.");
		gui2::show_message(null_str, msg);
		return;
	}

	const tcourseware_file& courseware_file = courseware_file_from_at(file_list_->cursel()->at());
	if (courseware_file.dir_name != tmp_courseware_.title) {
		utils::string_map symbols;
		symbols["dir"] = courseware_file.dir_name;
		symbols["title"] = tmp_courseware_.title;
		symbols["title_field"] = get_field_str(type_global, field_title);

		const std::string msg = vgettext2("The directory name($dir) and courseware $title_field|($title) don't match. Save first before uploading", symbols);
		gui2::show_message(null_str, msg);
		return;
	}

	const std::string courseware_name = courseware_file.dir_name;

	// const std::string desc = _("2025-09-15 Physics homework");
	const std::string desc;
	int64_t rsp_ts = 0;
	generate_courseware_rsp(desc, courseware_name, tmp_courseware_.uuid, rsp_ts);
	bool ret = upload_courseware_rsp(courseware_name, tmp_courseware_.uuid, rsp_ts);

	const std::string courseware_rsp = join_courseware_dir(courseware_name) + ".rsp";
	SDL_DeleteFiles(courseware_rsp.c_str());

	if (ret) {
		tcourseware_distribution_vals local = get_distribution_vals(join_courseware_dir(courseware_name));

		courseware_files_[courseware_name] = tbase_courseware::tcourseware_file(false, courseware_name, local.username, local.uuid, local.ts, 0);
		reload_courseware_list(*file_list_);

		std::map<std::string, tcourseware_file>::iterator it = courseware_files_.find(courseware_name);
		int at = std::distance(courseware_files_.begin(), it);
		file_list_->select_row(at);

		gui2::show_message(null_str, _("Upload successful."));
	}
}

bool tcourseware2::do_save_tex(const std::string& tex_file, bool student, bool show_dlg)
{
	if (courseware_dirty() && !gui_courseware_is_valid(nullptr)) {
		return false;
	}

	const aplt::tcourseware& courseware = tmp_courseware_;

	VALIDATE(courseware.type == aplt::tcourseware::type_exam, null_str);
	VALIDATE(tmp_problems_.size() == courseware.exercises.size(), null_str);

	std::stringstream ss;
	std::string tex_header;
	if (!student) {
		tex_header = courseware.tex_header;
	} else {
		const std::string exam_switch_key = "\\showanswerstrue";
		tex_header = utils::replace_all(courseware.tex_header, exam_switch_key, "\\showanswersfalse");
	}
	ss << tex_header << "\n\n";

	if (!courseware.content.empty()) {
		ss << courseware.content << "\n";
	}
	int at = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != tmp_courseware_.exercises.end(); ++ it, at ++) {
		const aplt::tcourseware::texercise& exercise = *it;
		if (!exercise.section.empty()) {
			ss << exercise.section << "\n\n";
		}
		const texam_problem& this_p = tmp_problems_[at];

		VALIDATE(!exercise.question.empty(), null_str);
		if (game_config::os == os_windows && exercise.question != this_p.to_rose_msg()) {
			write_file(game_config::preferences_dir + "/saves/error1.dat", exercise.question.c_str(), exercise.question.size());
			write_file(game_config::preferences_dir + "/saves/error2.dat", this_p.to_rose_msg().c_str(), this_p.to_rose_msg().size());
		}
		VALIDATE(exercise.question == this_p.to_rose_msg(), null_str);

		ss << this_p.to_tex() << "\n\n";

		// if (!exercise.analysis.empty()) {
		//	ss << exercise.analysis << "\n\n";
		// }
		if (!exercise.answer.empty()) {
			ss << exercise.answer << "\n\n";
		}
	}

	ss << tmp_courseware_.tex_tail;
	write_file(tex_file, ss.str().c_str(), ss.str().size());

	if (show_dlg) {
		utils::string_map symbols;
		symbols["file"] = tex_file;
		gui2::show_message(null_str, vgettext2("Tex file has been generated.\nPath: $file", symbols));
	}
	return true;
}

enum {pdfver_student, pdfver_teacher};
std::string join_pdf_filename(const std::string& dir, const std::string& main_name, int ver)
{
	char buf[256];
	std::string dir2;
	if (!dir.empty()) {
		dir2 = dir + "/";
	}
	if (ver == pdfver_student) {
		SDL_snprintf(buf, sizeof(buf), "%s%s(%s).pdf", dir2.c_str(), main_name.c_str(), _("Student Version"));
	} else if (ver == pdfver_teacher) {
		SDL_snprintf(buf, sizeof(buf), "%s%s(%s).pdf", dir2.c_str(), main_name.c_str(), _("Teacher Version"));
	} else {
		SDL_snprintf(buf, sizeof(buf), "%s%s.pdf", dir2.c_str(), main_name.c_str());
	}
	return buf;
}

tfile* woutput_file = nullptr;
// static void lualatex_hook_woutput_fputc(int ch)
void lualatex_hook_woutput_fputc(int ch, void* user)
{
	// 'ch' myabe one byte of chinese's utf-8.
	// if (ch <= 0 || ch >= 0x7f) {
	//	return;
	// }
	tcourseware2* cw = reinterpret_cast<tcourseware2*>(user);
	cw->lualatex_woutput_fputc(ch);
}

void lualatex_hook_woutput_fputs(const char* c_str, void* user)
{
	tcourseware2* cw = reinterpret_cast<tcourseware2*>(user);
	cw->lualatex_woutput_fputs_l(c_str, SDL_strlen(c_str));
}

void lualatex_hook_woutput_fputs_l(const char* c_str, int l, void* user)
{
	tcourseware2* cw = reinterpret_cast<tcourseware2*>(user);
	cw->lualatex_woutput_fputs_l(c_str, l);
}

void lualatex_hook_woutput_fprintf(void* user, const char *fmt, ...)
{
	char buf[512];
	va_list ap;

    va_start(ap, fmt);
	int l = SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

	tcourseware2* cw = reinterpret_cast<tcourseware2*>(user);
	cw->lualatex_woutput_fputs_l(buf, l);
}

void lualatex_hook_woutput_cr(void* user)
{
	tcourseware2* cw = reinterpret_cast<tcourseware2*>(user);
	cw->lualatex_woutput_cr();
}

void tcourseware2::lualatex_woutput_fputc(int ch)
{
	woutput_data_.append_1ch(ch);
}

void tcourseware2::lualatex_woutput_fputs_l(const char* c_str, int l)
{
	if (l <= 0) {
		return;
	}
	woutput_data_.append(c_str, l);	
}

void tcourseware2::did_receive_woutput_cr(const char* data, int size)
{
	if (valid_lines_ == MAX_MSG_LINES) {
		build_msg_.drop_first(line_bytes_[0]);
		memcpy(line_bytes_, line_bytes_ + 1, (valid_lines_ - 1) * sizeof(line_bytes_[0])); 
		valid_lines_ --;
	}
	build_msg_.append(data, size);
	line_bytes_[valid_lines_ ++] = size;

	VALIDATE(valid_lines_ <= MAX_MSG_LINES, null_str);
}

void tcourseware2::lualatex_woutput_cr()
{
	const bool enable_woutput_file = game_config::os == os_windows;
	// const bool enable_woutput_file = false;
	if (enable_woutput_file && woutput_file == nullptr) {
		woutput_file = new tfile(game_config::preferences_dir + "/woutput.dat", GENERIC_WRITE, CREATE_ALWAYS);
	}
	woutput_data_.append_1ch('\n');
	if (woutput_file != nullptr) {
		posix_fwrite(woutput_file->fp, woutput_data_.data, woutput_data_.vsize);
	}

	if (utils::is_utf8str(woutput_data_.data, woutput_data_.vsize)) {
		threading::lock lock(build_msg_mutex_);
		did_receive_woutput_cr(woutput_data_.data, woutput_data_.vsize);
	}

	woutput_data_.vsize = 0; // clear data.
}
void tcourseware2::OnWorkStart()
{
	VALIDATE(building_ == bool_set_none, null_str);
	building_ = bool_set_false;

	enter_edit_widget_->set_active(false);
	input_toolbar_grid_->set_visible(twidget::INVISIBLE);

	input_stack_->set_radio_layer(INPUT_BUILD_LAYER);
	build_msg_.vsize = 0;

	valid_lines_ = 0;
	build_timer_.reset(50, *window_, std::bind(&tcourseware2::build_timer_handler, this));
}

void tcourseware2::DoWork(const std::string tex_file, const tluatex_hook hook, bool& exit)
{
	bool result = latex::tex_file_to_pdf_file(tex_file, hook);

	tmsg_data_build_finished* pdata = new tmsg_data_build_finished(result);
	// it isn't in main-thread, must not use rtc::Thread::Current()->Post.
	main_->Post(RTC_FROM_HERE, this, MSG_BUILD_FINISHED, pdata);
}

void tcourseware2::build_finished(bool result)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(building_ == bool_set_false, null_str);
	building_ = result? bool_set_true: bool_set_false;

	VALIDATE(task_thread_.get() != nullptr, null_str);
	task_thread_.reset();
}

void tcourseware2::OnWorkDone(const std::string tex_file, const std::string title, int pdfver)
{
	build_timer_.reset();
	if (woutput_data_.vsize > 0) {
		did_receive_woutput_cr(woutput_data_.data, woutput_data_.vsize);

		build_timer_handler();
	}

	if (woutput_file != nullptr) {
		if (woutput_data_.vsize > 0) {
			posix_fwrite(woutput_file->fp, woutput_data_.data, woutput_data_.vsize);
		}
		delete woutput_file;
		woutput_file = nullptr;
	}

	utils::string_map symbols;
	std::string msg;
	if (building_ == bool_set_true) {
		std::string from = miktex_output_dir + "/tmp_exam.pdf";
		// std::string to = join_pdf_filename(utils::extract_directory(tex_file), title, pdfver);
		std::string to = join_pdf_filename(pdf_output_dir_, title, pdfver);

		SDL_CopyFiles(from.c_str(), to.c_str());
		symbols["type"] = "Pdf";
		symbols["file"] = to;
		msg = vgettext2("$type file has been generated.\nPath: $file", symbols);

	} else {
		msg = _("Build failed. You can check the log for the reason.");
	}
	if (!msg.empty()) {
		gui2::show_message(null_str, msg);
	}

	building_ = bool_set_none;

	enter_edit_widget_->set_active(true);
	input_toolbar_grid_->set_visible(twidget::VISIBLE);
}

void tcourseware2::click_pdf(tbutton& widget)
{
	VALIDATE(task_thread_.get() == nullptr, null_str);

	VALIDATE(file_list_->cursel() != nullptr, null_str);

	enum {pdf_student, pdf_teacher, tex, build_log};

	const std::vector<gui2::tmenu::titem> full_items = {
		{_("PDF(Student)"), pdf_student},
		{_("PDF(Teacher)"), pdf_teacher},
		{_("Tex(Latex)"), tex},
		{_("Show build log"), build_log},
	};

	std::vector<gui2::tmenu::titem> items;
	utils::string_map symbols;

	for (std::vector<gui2::tmenu::titem>::const_iterator it = full_items.begin(); it != full_items.end(); ++ it) {
		const gui2::tmenu::titem& item = *it;
		if (item.val == pdf_student) {
			// continue;
		}
		items.push_back(item);
	}

	int selected;
	{
		gui2::tmenu dlg(items, nposm);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		// absolute_draw();
		selected = dlg.selected_val();
	}

	const aplt::tcourseware& courseware = tmp_courseware_;
	if (selected == pdf_student || selected == pdf_teacher) {
		if (!latex::sandbox_is_valid(nullptr, nullptr, nullptr, nullptr)) {
			symbols["scene"] = _("Build failed");
			const std::string msg = vgettext2("Missing latex packate, $scene", symbols);;
			gui2::show_message(null_str, msg);
			return;
		}

		const std::string tex_file = game_config::preferences_dir + "/saves/tmp_exam.tex";
		// if (game_config::os != os_windows) {
			if (!do_save_tex(tex_file, selected == pdf_student, false)) {
				return;
			}
		// }

		const int pdfver = selected == pdf_student? pdfver_student: pdfver_teacher;

		VALIDATE(woutput_file == nullptr, null_str);
		tluatex_hook hook = {lualatex_hook_woutput_fputc, 
			lualatex_hook_woutput_fputs,
			lualatex_hook_woutput_fputs_l,
			lualatex_hook_woutput_fprintf,
			lualatex_hook_woutput_cr,
			this
		};

		task_thread_.reset(new net::tworker(std::bind(&tcourseware2::DoWork, this, tex_file, hook, _1), std::bind(&tcourseware2::OnWorkStart, this), 
			std::bind(&tcourseware2::OnWorkDone, this, tex_file, courseware.title, pdfver), 
			std::bind(&tcourseware2::OnTriggerExit, this), "TextoPdfThread"));

	} else if (selected == tex) {
		const std::string tex_file = pdf_output_dir_ + "/tmp_exam.tex";
		do_save_tex(tex_file, false, true);

	} else if (selected == build_log) {
		input_stack_->set_radio_layer(INPUT_BUILD_LAYER);
		build_timer_handler();

	} else {
		VALIDATE(false, null_str);
		gui2::tlatex_editor dlg;
		dlg.show();
	}
}

// true: continue. 
// false: require stop.
bool tcourseware2::if_dirty_confirm_save(const std::string& action)
{
	VALIDATE(!action.empty(), null_str);
	if (!courseware_dirty()) {
		return true;
	}

	std::string msg = _("Is there change in the courseware data, do you want to save");
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK) {
		if (!do_save(*save_widget_)) {
			utils::string_map symbols;
			symbols["action"] = action;
			msg = vgettext2("There is a data error, and the save failed. Do you want to keep $action without saving?", symbols);
			const int res = gui2::show_messagefs(msg, _("Yes"), _("No"));
			if (res != gui2::twindow::OK) {
				return false;
			}
		}
	}
	return true;
}

void tcourseware2::click_back(tbutton& widget)
{
/*
	if (!if_dirty_confirm_save(_("exiting"))) {
		return;
	}
*/

	window_->set_retval(twindow::CANCEL);
}

enum {pos_prefix, pos_postfix, pos_any};
bool is_contain_key(const std::string& field, const std::string& value, const std::string& key, int pos, std::string& err_msg)
{
	VALIDATE(!key.empty(), null_str);

	utils::string_map symbols;
	if (pos == pos_prefix) {
		if (value.find(key) != 0) {
			symbols["field"] = field;
			symbols["key"] = key;
			err_msg = vgettext2("$field must begin with the prefix '$key'.", symbols);
			return false;
		}
	} else if (pos == pos_postfix) {
		size_t pos = value.find(key);
		if (pos == std::string::npos || pos != value.size() - key.size()) {
			symbols["field"] = field;
			symbols["key"] = key;
			err_msg = vgettext2("$field must end with the suffix '$key'.", symbols);
			return false;
		}

	} else if (pos == pos_any) {
		if (value.find(key) == std::string::npos) {
			symbols["field"] = field;
			symbols["key"] = key;
			err_msg = vgettext2("$field must contain '$key'.", symbols);
			return false;
		}

	} else {
		VALIDATE(false, null_str);
	}

	return true;
}

std::string exam_problem_is_valid(const texam_problem& problem, utils::string_map& symbols)
{
	if (problem.points <= 0) {
		return _("The exam^points must be greater than zero.");
	}
	if (problem.question.empty()) {
		symbols["field"] = _("exam^question");
		return vgettext2("'$field' cannot be empty.", symbols);
	}

	if (problem.explanation.empty()) {
		if (problem.type != rexam_type_solving || problem.subproblems.empty()) {
			symbols["field"] = _("exam^explanation");
			return vgettext2("'$field' cannot be empty.", symbols);
		}
	}

	if (problem.type == rexam_type_choice) {
		if (problem.options.empty() || problem.correct_options.empty()) {
			return _("For choice problem, both the option and the correct option must not be empty");
		}
	}
	return null_str;
}

uint64_t tcourseware2::exam_is_valid(const aplt::tcourseware& courseware, std::string& err_msg) const
{
	VALIDATE(courseware.type == aplt::tcourseware::type_exam, null_str);
	VALIDATE(tmp_problems_.size() == courseware.exercises.size(), null_str);

	utils::string_map symbols;
	std::string prefix = "\\documentclass";
	std::string postfix = "\\begin{document}";

	std::string field_str = get_field_str(type_global, field_tex_header);
	if (!is_contain_key(field_str, courseware.tex_header, prefix, pos_prefix, err_msg)) {
		return tcookie3f(0, type_global, field_tex_header).u64;
	}
	if (!is_contain_key(field_str, courseware.tex_header, postfix, pos_postfix, err_msg)) {
		return tcookie3f(0, type_global, field_tex_header).u64;
	}

	prefix = "\\end{questions}";
	postfix = "\\end{document}";
	field_str = get_field_str(type_global, field_tex_tail);
	if (!is_contain_key(field_str, courseware.tex_tail, prefix, pos_prefix, err_msg)) {
		return tcookie3f(0, type_global, field_tex_tail).u64;
	}
	if (!is_contain_key(field_str, courseware.tex_tail, postfix, pos_postfix, err_msg)) {
		return tcookie3f(0, type_global, field_tex_tail).u64;
	}

	postfix = "\\begin{questions}";
	field_str = get_field_str(type_global, field_content);
	if (!is_contain_key(field_str, courseware.content, postfix, pos_postfix, err_msg)) {
		return tcookie3f(0, type_global, field_content).u64;
	}

	int index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;

		// field: section
		if (!exercise.section.empty()) {
			field_str = get_field_str(type_exercise, field_section);
			prefix = "\\end{questions}";
			if (!is_contain_key(field_str, exercise.section, prefix, pos_prefix, err_msg)) {
				return tcookie3f(index, type_exercise, field_section).u64;
			}

			const std::string key = "\\begin{questions}\n"
							"    \\setcounter{question}{0}";
			if (!is_contain_key(field_str, exercise.section, key, pos_any, err_msg)) {
				return tcookie3f(index, type_exercise, field_section).u64;
			}
		}

		// field: question
		uint64_t question_cookie = tcookie3f(index, type_exercise, field_question).u64;
		const texam_problem& this_p = tmp_problems_[index];

		err_msg = exam_problem_is_valid(this_p, symbols);
		if (!err_msg.empty()) {
			return question_cookie;
		}
		if (this_p.type == rexam_type_solving && !this_p.subproblems.empty()) {
			int points = 0;
			for (std::vector<texam_problem>::const_iterator it2 = this_p.subproblems.begin(); it2 != this_p.subproblems.end(); ++ it2) {
				const texam_problem& sub = *it2;
				points += sub.points;

				err_msg = exam_problem_is_valid(sub, symbols);
				if (!err_msg.empty()) {
					return question_cookie;
				}
			}
			if (points != this_p.points) {
				err_msg = _("This solving problem has sub-problems, its points must equal the sum of the sub-problem points.");
				return question_cookie;
			}
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

uint64_t tcourseware2::courseware_is_valid(const aplt::tcourseware& courseware, std::string& err_msg) const
{
	utils::string_map symbols;
	// char buf[256];

	err_msg.clear();
	// std::string uuid;
	if (curr_layer_ == UPLOAD_LAYER) {
		VALIDATE(utils::is_uuid(courseware.uuid, false), null_str);
	}
	if (!utils::is_uuid(courseware.uuid, false)) {
		return tcookie3f(0, type_global, field_uuid).u64;
	}

	VALIDATE(aplt::tcourseware::types.count(courseware.type) != 0, null_str);

	if (courseware.type == aplt::tcourseware::type_exam) {
		// Why call 'exam_is_valid' first? 
		// --If the exam rules are not satisfied, then some fields maybe empty, for example exercise's 'question'. 
		//   In this case, should prompt the user with the reason for the empty field, 
		//   rather than simply stating it cannot be empty.
		uint64_t result = exam_is_valid(courseware, err_msg);
		if (result != TCOOKIE3F_CHECK_OK) {
			return result;
		}
	}

	// std::string title;
	if (!isvalid_normal_utf8_name224(courseware.title)) {
		return tcookie3f(0, type_global, field_title).u64;
	}

	// std::string content
	if (courseware.content.empty() || !utils::is_utf8str(courseware.content.c_str(), courseware.content.size())) {
		return tcookie3f(0, type_global, field_content).u64;
	}

	// std::string annotation
	if (!utils::is_utf8str(courseware.annotation.c_str(), courseware.annotation.size())) {
		return tcookie3f(0, type_global, field_annotation).u64;
	}

	// std::string analysis
	if (!utils::is_utf8str(courseware.analysis.c_str(), courseware.analysis.size())) {
		return tcookie3f(0, type_global, field_analysis).u64;
	}

	// std::string annotation
	if (!utils::is_utf8str(courseware.reference.c_str(), courseware.reference.size())) {
		return tcookie3f(0, type_global, field_reference).u64;
	}

	int index = 0;
	for (std::vector<aplt::tcourseware::tkeypoint>::const_iterator it = courseware.keypoints.begin(); it != courseware.keypoints.end(); ++ it, index ++) {
		const aplt::tcourseware::tkeypoint& keypoint = *it;

		if (!utils::is_utf8str(keypoint.section.c_str(), keypoint.section.size())) {
			return tcookie3f(index, type_keypoint, field_section).u64;
		}

		if (keypoint.name.empty() || !utils::is_utf8str(keypoint.name.c_str(), keypoint.name.size())) {
			return tcookie3f(index, type_keypoint, field_name).u64;
		}

		if (keypoint.annotation.empty() || !utils::is_utf8str(keypoint.annotation.c_str(), keypoint.annotation.size())) {
			return tcookie3f(index, type_keypoint, field_annotation).u64;
		}

		if (!utils::is_utf8str(keypoint.analysis.c_str(), keypoint.analysis.size())) {
			return tcookie3f(0, type_keypoint, field_analysis).u64;
		}
	}

	index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;

		if (!utils::is_utf8str(exercise.section.c_str(), exercise.section.size())) {
			return tcookie3f(index, type_exercise, field_section).u64;
		}

		if (exercise.question.empty() || !utils::is_utf8str(exercise.question.c_str(), exercise.question.size())) {
			return tcookie3f(index, type_exercise, field_question).u64;
		}

		if (!utils::is_utf8str(exercise.analysis.c_str(), exercise.analysis.size())) {
			return tcookie3f(0, type_exercise, field_analysis).u64;
		}

		if (!utils::is_utf8str(exercise.answer.c_str(), exercise.answer.size())) {
			return tcookie3f(index, type_exercise, field_answer).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

bool tcourseware2::rename_courseware_dir(const tcourseware_file& courseware_file, const std::string& new_name)
{
	VALIDATE(courseware_file.dir_name != new_name, null_str);
	VALIDATE(!new_name.empty(), null_str);

	const std::string old = join_courseware_dir2(courseware_load_path(), courseware_file.dir_name);
	SDL_bool ret = SDL_RenameFile(old.c_str(), new_name.c_str());
	if (!ret) {
		return false;
	}
	return true;
}

bool tcourseware2::gui_courseware_is_valid(std::string* new_dir_name)
{
	VALIDATE(courseware_dirty(), null_str);
	// VALIDATE(tmp_courseware_.valid(), null_str);

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const tcourseware_file& courseware_file = courseware_file_from_at(file_list_->cursel()->at());

	std::string err_msg;
	const aplt::tcourseware& courseware = tmp_courseware_;
	uint64_t res = courseware_is_valid(courseware, err_msg);
	if (res == TCOOKIE3F_CHECK_OK && courseware.title != courseware_file.dir_name && new_dir_name != nullptr) {
		if (!rename_courseware_dir(courseware_file, courseware.title)) {
			utils::string_map symbols;
			symbols["src"] = join_courseware_dir2(courseware_load_path(), courseware_file.dir_name);
			symbols["dest"] = courseware.title;
			err_msg = vgettext2("Failed to rename directory '$src' to '$dest'.", symbols);
			res = tcookie3f(0, type_global, field_title).u64;
		} else {
			*new_dir_name = courseware.title;
		}
	}
	if (res != TCOOKIE3F_CHECK_OK) {
		tcookie3f cookie3f(res);
		{
			ttree& tree = *tree_widget_;
			ttree_node* node = tree.get_root_node().find_node_from_cookie(res);
			VALIDATE(node != nullptr, null_str);
			tree.select_node(node);
			tree.scroll_to_node(*node);
		}
		std::stringstream err;
		if (err_msg.empty()) {
			err << get_error_msg(courseware, cookie3f.type, cookie3f.field);
		} else {
			err << err_msg;
		}
		set_status_label(err.str(), true);
		return false;
	}
	return true;
}

bool tcourseware2::do_save(tbutton& widget)
{
	VALIDATE(courseware_dirty(), null_str);
	// VALIDATE(tmp_courseware_.valid(), null_str);

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const tcourseware_file& courseware_file = courseware_file_from_at(file_list_->cursel()->at());

	std::string new_dir_name;
	if (!gui_courseware_is_valid(&new_dir_name)) {
		return false;
	}
/*
	std::string err_msg;
	const aplt::tcourseware& courseware = tmp_courseware_;
	uint64_t res = courseware_is_valid(courseware, err_msg);
	if (res == TCOOKIE3F_CHECK_OK && courseware.title != courseware_file.dir_name) {
		if (!rename_courseware_dir(courseware_file, courseware.title)) {
			utils::string_map symbols;
			symbols["src"] = join_courseware_dir2(courseware_load_path(), courseware_file.dir_name);
			symbols["dest"] = courseware.title;
			err_msg = vgettext2("Failed to rename directory '$src' to '$dest'.", symbols);
			res = tcookie3f(0, type_global, field_title).u64;
		} else {
			new_dir_name = courseware.title;
		}
	}
	if (res != TCOOKIE3F_CHECK_OK) {
		tcookie3f cookie3f(res);
		{
			ttree& tree = *tree_widget_;
			ttree_node* node = tree.get_root_node().find_node_from_cookie(res);
			VALIDATE(node != nullptr, null_str);
			tree.select_node(node);
			tree.scroll_to_node(*node);
		}
		std::stringstream err;
		if (err_msg.empty()) {
			err << get_error_msg(courseware, cookie3f.type, cookie3f.field);
		} else {
			err << err_msg;
		}
		set_status_label(err.str(), true);
		return false;
	}
*/
	if (!tmp_courseware_.equal(courseware_)) {
		save_main_cfg(join_main_cfg_filename(tmp_courseware_.title), tmp_courseware_);
		courseware_ = tmp_courseware_;
	}

	if (!new_dir_name.empty()) {
		VALIDATE(new_dir_name != courseware_file.dir_name, null_str);

		// tcourseware_file(bool download, const std::string& dir_name, const std::string& username, int64_t local_time, int64_t remote_time1)

		tcourseware_file new_courseware_file(false, new_dir_name, courseware_file.username, courseware_file.uuid, courseware_file.local_time, courseware_file.remote_time1);
		// new_courseware_file.dir_name = new_dir_name;

		std::map<std::string, tcourseware_file>::iterator erase_it = courseware_files_.find(courseware_file.dir_name);
		VALIDATE(erase_it != courseware_files_.end(), null_str);
		courseware_files_.erase(erase_it);

		std::pair<std::map<std::string, tcourseware_file>::iterator, bool> ins = courseware_files_.insert(std::make_pair(new_dir_name, new_courseware_file));

		reload_courseware_list(*file_list_);

		{
			tignore_file_list_row_changed_lock lock(*this);
			int at = std::distance(courseware_files_.begin(), ins.first);
			file_list_->select_row(at);
		}
	}

	VALIDATE(!courseware_dirty(), null_str);
	widget.set_active(false);

	return true;
}

bool tcourseware2::verify_edit_courseware_title(const std::string& label, const std::string& initial, const std::set<std::string>& excludes) const
{
	if (label == initial) {
		return false;
	}

	if (label.empty()) {
		return true;
	}
	if (utils::has_portable_space_2end(label)) {
		return false;
	}

	if (excludes.count(label) != 0) {
		return false;
	}

	bool valid = isvalid_normal_utf8_name224(label);
	return valid;
}

void tcourseware2::click_insert_courseware(tbutton& widget)
{
/*
	{
		evaluate_uuids();
		return;
	}
*/
	VALIDATE(!editing_, null_str);

	utils::string_map symbols;

	const std::string title = _("New courseware title");
	std::string prefix;
	std::string placeholder;
	const std::string initial;

	int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
	symbols["max_chars"] = str_cast(max_chars);
	std::string remark = vgettext2("Easy name, can be Chinese, and maximum $max_chars characters", symbols);

	std::set<std::string> excludes;
	for (std::map<std::string, tcourseware_file>::const_iterator it = courseware_files_.begin(); it != courseware_files_.end(); ++ it) {
		const tcourseware_file& file = it->second;
		excludes.insert(file.dir_name);
	}

	std::string dir_name;
	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel);
		param.did_text_changed = std::bind(&tcourseware2::verify_edit_courseware_title, this, _1, std::ref(initial), std::ref(excludes));
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 5);
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
		}
		dir_name = param.result;
	}
	
	// create directory
	aplt::tcourseware new_courseware;
	new_courseware.title = dir_name;

	const std::string full_courseware_dir = join_courseware_dir2(courseware_load_path(), dir_name);
	SDL_MakeDirectory(full_courseware_dir.c_str());
	save_main_cfg(join_main_cfg_filename(dir_name), new_courseware);

	// 
	tcourseware_file new_file(false, dir_name, current_user.username, utils::create_uuid(false), time(nullptr), 0);
	std::pair<std::map<std::string, tcourseware_file>::iterator, bool> ins = 
		courseware_files_.insert(std::make_pair(new_file.dir_name, new_file));

	reload_courseware_list(*file_list_);
	{
		// tignore_file_list_row_changed_lock lock(*this);
		int at = std::distance(courseware_files_.begin(), ins.first);
		file_list_->select_row(at);
	}

/*
	// curr_tmp_pair_ = &pair;
	// pair_update_to_tree(*curr_tmp_pair_);

	// empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
*/
}

void tcourseware2::click_erase_courseware(tlistbox& list)
{
	const int drag_at = list.drag_at();
	const int row_at = drag_at;

	const tcourseware_file& courseware_file = courseware_file_from_at(row_at);

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	aplt::tcourseware& courseware = tmp_courseware_;

	utils::string_map symbols;
	symbols["name"] = courseware_file.title;
	std::string msg;
	if (curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER) {
		msg = vgettext2("Do you want erase courseware($name)?", symbols);
	} else {
		msg = vgettext2("Do you want to unfavorite courseware($name)?", symbols);
	}
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	// first, require cancel left_drag grid.
	list.cancel_drag();

	if (curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER) {
		// erase_courseware_upload_or_download(list, row_at);
		VALIDATE(courseware_files_.count(courseware_file.dir_name) != 0, null_str);
		std::map<std::string, tcourseware_file>::iterator erase_it = courseware_files_.find(courseware_file.dir_name);

		SDL_DeleteFiles(join_courseware_dir2(courseware_load_path(), courseware_file.dir_name).c_str());
		courseware_files_.erase(erase_it);

	} else {
		int course_at = cfg_cpp_api_.course_which_at(courseware_file.uid, courseware_file.title);

		VALIDATE(course_at != nposm, null_str);
		cfg_cpp_api_.erase_course(courseware_file.uid, courseware_file.title);

		std::vector<tcourseware_file>::iterator erase_it = courseware_files2_.begin();
		if (course_at != 0) {
			std::advance(erase_it, course_at);
		}
		courseware_files2_.erase(erase_it);
		refresh_courselist_item_label();
	}

	reload_courseware_list(*file_list_);
	if (file_list_->rows() > 0) {
		file_list_->select_row(row_at % file_list_->rows());

	} else {
		VALIDATE(file_list_->cursel() == nullptr, null_str);
		tmp_courseware_.clear();
		courseware_ = tmp_courseware_;

		clear_tree2();
	}

	// empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

void tcourseware2::post_tree_recreated(const tcookie3f& cookie3f)
{
	validate_tree_cookie();

	refresh_toolbar_active(&cookie3f);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

SDL_Point3 tcourseware2::handle_clipboard_rstr(const char* c_str, int size)
{
/*
	const std::string filename = game_config::preferences_dir + "/saves/1-answer.md";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize != 0, null_str);

	c_str = file.data;
	size = fsize;
*/
	aplt::tcourseware& courseware = tmp_courseware_;
	VALIDATE(tmp_problems_.size() == courseware.exercises.size(), null_str);

	SDL_Point3 result = {0, 0, 0};

	std::vector<texam_problem> problems;
	rexam_str_2_problems(rexam_str_types_C, c_str, size, problems);

	if (problems.empty()) {
		return result;
	}

	for (std::vector<texam_problem>::const_iterator it = problems.begin(); it != problems.end(); ++ it) {
		const texam_problem& problem = *it;
		if (problem.type == rexam_type_choice) {
			result.x ++;
		} else if (problem.type == rexam_type_fillin) {
			result.y ++;
		} else {
			VALIDATE(problem.type == rexam_type_solving, null_str);
			result.z ++;
		}

		std::stringstream question_ss;
		question_ss << problem.to_rose_msg();

		std::stringstream answer_ss;
		answer_ss << exam_default_.exercise_answer;

		courseware.exercises.push_back(aplt::tcourseware::texercise(null_str, question_ss.str(), null_str, answer_ss.str()));
		aplt::tcourseware::texercise& exercise = courseware.exercises.back();
	}
	tmp_problems_.insert(tmp_problems_.end(), problems.begin(), problems.end());
	VALIDATE(tmp_problems_.size() == courseware.exercises.size(), null_str);

	ttree_node* selected_node = nullptr;
	ttree& tree = *tree_widget_;

	ttree_node& htvi_root = tree.get_root_node();
	// key_2_state_update_to_tree(htvi_root, pair.key_2_states.size() - 1, pair.state_names, pair.key_2_states.back());
	courseware_update_to_tree(courseware);

	tcookie3f new_cookie3f(courseware.exercises.size() - 1, type_exercise, field_typeself);
	ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
	VALIDATE(node != nullptr, null_str);
	tree.select_node(node);
	selected_node = node;

	tcookie3f tmp_cookie3f(selected_node->cookie());
	post_tree_recreated(tmp_cookie3f);

	return result;
}

void tcourseware2::click_ai_exercise(tbutton& widget, int type)
{
	VALIDATE(type == insert_exercise, null_str);

	VALIDATE(file_list_->cursel() != nullptr, null_str);

		// net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::tcfg_cpp_api& cfg_cpp_api,
		//	tspeech_driver& speech_driver, const std::string& saves_courseware_dir

	const aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, "aplt.leagor.basic", "exam", false, true);
	VALIDATE(pair.aplt != nullptr && pair.task != nullptr, null_str);
	tstart_aiagent start_aiagent(*pair.aplt, *pair.task, examsubject_mathematics, exam_default_.prompt_header,
		std::bind(&tcourseware2::handle_clipboard_rstr, this, _1, _2));
	instance_slot_.show_gui_center(&start_aiagent);
}

void tcourseware2::click_fill_default(tbutton& widget)
{
	ttree& tree = *tree_widget_;
	ttree_node* node = tree.cursel();
	VALIDATE(node != nullptr, null_str);

	uint64_t cookie = node->cookie();
	tcookie3f cookie3f(cookie);

	const aplt::tcourseware& courseware = tmp_courseware_;
	VALIDATE(courseware.type == aplt::tcourseware::type_exam, null_str);

	const std::string* def_val = nullptr;
	const std::string* to_field = nullptr;
	if (cookie3f.type == type_global) {
		if (cookie3f.field == field_tex_header) {
			def_val = &exam_default_.tex_header;
			to_field = &courseware.tex_header;

		} else if (cookie3f.field == field_tex_tail) {
			def_val = &exam_default_.tex_tail;
			to_field = &courseware.tex_tail;

		} else if (cookie3f.field == field_content) {
			def_val = &exam_default_.content;
			to_field = &courseware.content;
		}

	} else if (cookie3f.type == type_exercise) {
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)courseware.exercises.size(), null_str);
		const aplt::tcourseware::texercise& exercise = courseware.exercises[cookie3f.index];
		if (cookie3f.field == field_section) {
			def_val = &exam_default_.exercise_section;
			to_field = &exercise.section;

		} else if (cookie3f.field == field_answer) {
			def_val = &exam_default_.exercise_answer;
			to_field = &exercise.answer;
		}
	}

	VALIDATE(to_field != nullptr, null_str);
	const std::string& def_val2 = *def_val;
	const std::string& to_field2 = *to_field;

	std::string msg = _("It has been changed to the default value.");
	if (def_val2.empty()) {
		msg = _("No default value is available.");

	} else if (to_field2 == def_val2) {
		msg = _("The value currently in use is already the default.");

	} else {
		set_tree_msg_label(def_val2, cookie3f, false);
	}

	gui2::show_message(null_str, msg);
}

void tcourseware2::click_insert(tbutton& widget, int type)
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);
	aplt::tcourseware& courseware = tmp_courseware_;

	ttree_node* selected_node = nullptr;
	ttree& tree = *tree_widget_;
	if (type == insert_keypoint) {
		courseware.keypoints.push_back(aplt::tcourseware::tkeypoint(null_str, null_str, null_str, null_str));
		aplt::tcourseware::tkeypoint& keypoint = courseware.keypoints.back();

		ttree_node& htvi_root = tree.get_root_node();
		// key_2_state_update_to_tree(htvi_root, pair.key_2_states.size() - 1, pair.state_names, pair.key_2_states.back());
		courseware_update_to_tree(courseware);

		tcookie3f new_cookie3f(courseware.keypoints.size() - 1, type_keypoint, field_typeself);
		ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);
		selected_node = node;

	} else {
		VALIDATE(type == insert_exercise, null_str);

		std::string question;
		std::string answer;
		if (courseware.type == aplt::tcourseware::type_exam) {
			tmp_problems_.push_back(texam_problem(rexam_type_fillin, 5, null_str, null_str));
			question = tmp_problems_.back().to_rose_msg();
			answer = exam_default_.exercise_answer;
		}

		courseware.exercises.push_back(aplt::tcourseware::texercise(null_str, question, null_str, answer));
		aplt::tcourseware::texercise& exercise = courseware.exercises.back();

		ttree_node& htvi_root = tree.get_root_node();
		// key_2_state_update_to_tree(htvi_root, pair.key_2_states.size() - 1, pair.state_names, pair.key_2_states.back());
		courseware_update_to_tree(courseware);

		tcookie3f new_cookie3f(courseware.exercises.size() - 1, type_exercise, field_typeself);
		ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);
		selected_node = node;
	}

/*
	validate_tree_cookie();

	tcookie3f tmp_cookie3f(selected_node->cookie());
	refresh_toolbar_active(&tmp_cookie3f);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
*/
	tcookie3f tmp_cookie3f(selected_node->cookie());
	post_tree_recreated(tmp_cookie3f);
}

void tcourseware2::click_move_down_or_up(tbutton& widget, bool down)
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);
	aplt::tcourseware& courseware = tmp_courseware_;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	VALIDATE(cookie3f.field == field_typeself, null_str);

	tcookie3f result_cookie3f = cookie3f_nposm_;
	int select_child_at = nposm;

	ttree& tree = *tree_widget_;
	utils::string_map symbols;
	std::stringstream ss;

	const int at1 = cookie3f.index;
	const int at2 = down? at1 + 1: at1 - 1;
	if (cookie3f.type == type_keypoint) {
		VALIDATE(at2 < (int)courseware.keypoints.size(), null_str);
		courseware.keypoint_swap(at1, at2);

	} else if (cookie3f.type == type_exercise) {
		VALIDATE(at2 < (int)courseware.exercises.size(), null_str);
		courseware.exercise_swap(at1, at2);

		if (courseware.type == aplt::tcourseware::type_exam) {
			problem_swap(tmp_problems_, at1, at2);
		}

	} else {
		VALIDATE(false, null_str);
	}

	courseware_update_to_tree(courseware);
	result_cookie3f = tcookie3f(at2, cookie3f.type, field_typeself);
	VALIDATE(result_cookie3f != cookie3f_nposm_, null_str);

	ttree_node* node = tree.get_root_node().find_node_from_cookie(result_cookie3f.u64);
	VALIDATE(node != nullptr, null_str);
	tree.select_node(node);
/*
	validate_tree_cookie();

	// deselect_another_tree(l_tree);
	refresh_toolbar_active(&result_cookie3f);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
*/
	post_tree_recreated(result_cookie3f);
}

void tcourseware2::click_erase(tbutton& widget)
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);
	aplt::tcourseware& courseware = tmp_courseware_;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	VALIDATE(cookie3f.field == field_typeself, null_str);

	tcookie3f result_cookie3f = cookie3f_nposm_;
	int select_child_at = nposm;

	bool l_tree = true;
	utils::string_map symbols;
	std::stringstream ss;
	if (cookie3f.type == type_keypoint) {
		l_tree = false;

		std::vector<aplt::tcourseware::tkeypoint>::iterator it = courseware.keypoints.begin();
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}

		const aplt::tcourseware::tkeypoint& keypoint = *it;
		symbols["field"] = get_node_label_keypoint(keypoint, cookie3f.index, field_typeself);
		const std::string msg = vgettext2("Do you want erase $field?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		select_child_at = cookie3f.index;
		courseware.keypoints.erase(it);

		// now gui2::ttree doesn't not support erase single node. regenerate tree.
		courseware_update_to_tree(courseware);

	} else if (cookie3f.type == type_exercise) {
		// 1)erase exercise
		std::vector<aplt::tcourseware::texercise>::iterator it = courseware.exercises.begin();
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}

		const aplt::tcourseware::texercise& exercise = *it;
		symbols["field"] = get_node_label_exercise(exercise, cookie3f.index, field_typeself);
		const std::string msg = vgettext2("Do you want erase $field?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		select_child_at = cookie3f.index;
		courseware.exercises.erase(it);

		if (courseware.type == aplt::tcourseware::type_exam) {
			// 2)erase problem 
			std::vector<texam_problem>::iterator problem_it = tmp_problems_.begin();
			if (cookie3f.index != 0) {
				std::advance(problem_it, cookie3f.index);
			}
			tmp_problems_.erase(problem_it);
		}

		// now gui2::ttree doesn't not support erase single node. regenerate tree.
		courseware_update_to_tree(courseware);

	} else {
		VALIDATE(false, null_str);
	}

	validate_tree_cookie();

	// deselect_another_tree(l_tree);
	refresh_toolbar_active(result_cookie3f != cookie3f_nposm_? &result_cookie3f: nullptr);
	if (result_cookie3f == cookie3f_nposm_) {
		// empty_val_stack();
	}

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

void tcourseware2::click_favorite(tbutton& widget)
{
	VALIDATE(!editing_, null_str);

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const int row_at = file_list_->cursel()->at();
	const tcourseware_file& courseware_file = courseware_file_from_at(row_at);

	std::string png;
	int64_t uid = courseware_file.uid;
	const std::string& title = courseware_file.title;
	if (!cfg_cpp_api_.is_existed_course(uid, title)) {
		cfg_cpp_api_.insert_course(uid, title);
		png = favorited_png;
	} else {
		cfg_cpp_api_.erase_course(uid, title);
		png = unfavorite_png;
	}
	favorite_widget_->set_label(png);

	refresh_courselist_item_label();
}

void tcourseware2::click_enter_edit(tbutton& widget)
{
	enter_or_exit_edit(true);
}

void tcourseware2::click_listen(tbutton& widget)
{
	VALIDATE(curr_layer_ == COURSELIST_LAYER, null_str);

	if (!speech_driver_.is_listening()) {
		const aplt::tcourselist::tcourse* course = nullptr;
		if (file_list_->rows() != 0) {
			VALIDATE(file_list_->cursel() != nullptr, null_str);
			const int row_at = file_list_->cursel()->at();
			const tcourseware_file& courseware_file = courseware_file_from_at(row_at);

			course = &cfg_cpp_api_.get_course(courseware_file.uid, courseware_file.title);
		}

		if (cfg_cpp_api_.courselist().courses().empty()) {
			const std::string msg = _("The courselist cannot be empty.");
			gui2::show_message(null_str, msg);
			return;
		}
		speech_driver_.start_listen(course);

	} else {
		speech_driver_.stop_listen();
	}

	listen_widget_->set_label(speech_driver_.is_listening()? "misc/stop_listen.png": "misc/listen.png");
}

void tcourseware2::load_courseware_cfg(const std::string& cfgfile)
{
	load_courseware_cfg2(cfgfile, courseware_);

	assign_tmp_courseware();
/*
	tmp_courseware_ = courseware_;

	aplt::tcourseware& courseware = tmp_courseware_;
	if (curr_layer_ == UPLOAD_LAYER && !utils::is_uuid(courseware.uuid, false)) {
		courseware.uuid = utils::create_uuid(false);
	}

	if (courseware.type == aplt::tcourseware::type_exam) {
		courseware_2_exam_problems(tmp_courseware_, tmp_problems_);
	} else {
		tmp_problems_.clear();
	}
*/
	courseware_update_to_tree(tmp_courseware_);
}

void tcourseware2::insert_courseware_list_row(const tcourseware_file& file, tlistbox& list, std::map<std::string, std::string>& data)
{
	std::string title = file.title;
	std::string icon_png;

	std::stringstream time_ss;
	time_ss.str("");
	if (curr_layer_ == UPLOAD_LAYER) {
		time_ss << utils::format_time_ymdhms(file.local_time);

	} else if (curr_layer_ == DOWNLOAD_LAYER) {
		if (!file.username.empty()) {
			time_ss << file.username;
		} else {
			time_ss << "UID: " << file.uid;
		}
		time_ss << "  " << utils::format_time_ymdhms(file.local_time);

	} else {
		const std::string cfgfile = join_main_cfg_filename_couselist(file.uid, file.dir_name);
		if (SDL_IsFile(cfgfile.c_str())) {
			if (file.uid == COURSEWARE_UPLOAD_UID) {
				icon_png = "misc/me_flag.png";
			} else {
				icon_png = "misc/download.png";
			}
			if (!file.username.empty()) {
				time_ss << file.username;
			} else {
				time_ss << "UID: " << file.uid;
			}
			time_ss << "  " << utils::format_time_ymdhms(file.local_time);

		} else {
			time_ss << _("Not exist");
			icon_png = "misc/alert.png";
		}
	}

	data["title"] = title;
	{
		time_ss << " UUID: " << file.uuid;
	}
	data["time"] = time_ss.str();
	data["icon"] = icon_png;

	ttoggle_panel& row = list.insert_row(data);
	if (curr_layer_ != COURSELIST_LAYER) {
		tcontrol* icon_widget = find_widget<tcontrol>(&row, "icon", false, true);
		icon_widget->set_visible(twidget::INVISIBLE);
	}
}


void tcourseware2::reload_courseware_list(tlistbox& list)
{
	list.clear();

	std::map<std::string, std::string> data;

	if (use_map_courseware_files()) {
		for (std::map<std::string, tcourseware_file>::const_iterator it = courseware_files_.begin(); it != courseware_files_.end(); ++ it) {
			const tcourseware_file& file = it->second;
		
			insert_courseware_list_row(file, list, data);
		}

	} else {
		for (std::vector<tcourseware_file>::const_iterator it = courseware_files2_.begin(); it != courseware_files2_.end(); ++ it) {
			const tcourseware_file& file = *it;
		
			insert_courseware_list_row(file, list, data);
		}
	}
}

bool tcourseware2::did_file_list_row_pre_change(tlistbox& list, ttoggle_panel& row)
{
	int desire_cookie = row.cookie();
	const ttoggle_panel* cursel = list.cursel();
	if (cursel != nullptr) {
/*
		if (!if_dirty_confirm_save(_("changing courseware"))) {
			return false;
		}
*/
	}
	return true;
}

void tcourseware2::did_file_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
	if (ignore_file_list_row_changed_) {
		return;
	}

	int at = row.at();
	const tcourseware_file& courseware_file = courseware_file_from_at(at);

	std::string cfgfile;
	if (courseware_file.uid == COURSEWARE_UPLOAD_UID) {
		cfgfile = join_main_cfg_filename2(upload_path_, courseware_file.dir_name);

	} else {
		cfgfile = join_main_cfg_filename(courseware_file.dir_name);
	}
	load_courseware_cfg(cfgfile);

	clear_tree2();

	{
		// tignore_tree_msg_text_changed_lock lock(*this);

		tcookie3f cookie3f(0, type_global, field_title);
		ttree& tree = *tree_widget_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);
		// node->set_widget_label("label", tmp_courseware_.title);
	}

	refresh_toolbar_active(nullptr);

	if (curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER) {
		bool favorited = cfg_cpp_api_.is_existed_course(courseware_file.uid, courseware_file.title);
		favorite_widget_->set_label(favorited? favorited_png: unfavorite_png);
	}

	if (curr_layer_ == DOWNLOAD_LAYER) {
		if (curr_main_layer_ != MAIN_TREE_LAYER) {
			main_stack_->set_radio_layer(MAIN_TREE_LAYER);
		}
	}

	refresh_title(tmp_courseware_.title);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

bool tcourseware2::did_file_list_can_drag(tlistbox& list, ttoggle_panel& row)
{
	const int at = row.at();

	std::map<std::string, twidget::tvisible> visibles;

	visibles.insert(std::make_pair("edit", twidget::INVISIBLE));
	visibles.insert(std::make_pair("erase", twidget::VISIBLE));

	list.left_drag_grid_set_widget_visible(visibles);
	return true;
}

#define MAX_NODE_CHARS			70
#define MAX_VAL_LABEL_CHARS		256
static std::string truncate_for_field_label(const std::string& label, int max_chars = MAX_NODE_CHARS, bool ellipsis = true)
{
	// text_surface::get_surfaces() have a maximum width limit. see 'max_text_line_width'(8192).
	// some field maybe more characters, the width of the image generated by it can exceed max_text_line_width.
	// for exmaple: tstate2.finished. 
	VALIDATE(max_chars >= MAX_NODE_CHARS, null_str);
	if (label.empty()) {
		return label;
	}
	return utils::truncate_to_max_chars2(label, max_chars, ellipsis);
}

void tcourseware2::keypoint_update_to_tree(ttree_node& branch, int index, const aplt::tcourseware::tkeypoint& keypoint)
{
	std::map<std::string, std::string> data;

	data["label"] = get_node_label_keypoint(keypoint, index, field_typeself);
	ttree_node& htvi_keypiont = branch.insert_node("default", data);
	htvi_keypiont.set_child_icon("label", "misc/state.png");
	htvi_keypiont.set_cookie(tcookie3f(index, type_keypoint, field_typeself).u64);

	data["label"] = truncate_for_field_label(get_node_label_keypoint(keypoint, index, field_section));
	ttree_node* htvi = &htvi_keypiont.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_keypoint, field_section).u64);

	data["label"] = truncate_for_field_label(get_node_label_keypoint(keypoint, index, field_name));
	htvi = &htvi_keypiont.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_keypoint, field_name).u64);

	data["label"] = truncate_for_field_label(get_node_label_keypoint(keypoint, index, field_annotation));
	htvi = &htvi_keypiont.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_keypoint, field_annotation).u64);

	data["label"] = truncate_for_field_label(get_node_label_keypoint(keypoint, index, field_analysis));
	htvi = &htvi_keypiont.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_keypoint, field_analysis).u64);

	htvi_keypiont.unfold();
}

void tcourseware2::exercise_update_to_tree(ttree_node& branch, int index, const aplt::tcourseware::texercise& exercise)
{
	std::map<std::string, std::string> data;

	data["label"] = get_node_label_exercise(exercise, index, field_typeself);
	ttree_node& htvi_exercise = branch.insert_node("default", data);
	htvi_exercise.set_child_icon("label", "misc/state.png");
	htvi_exercise.set_cookie(tcookie3f(index, type_exercise, field_typeself).u64);

	data["label"] = truncate_for_field_label(get_node_label_exercise(exercise, index, field_section));
	ttree_node* htvi = &htvi_exercise.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_exercise, field_section).u64);

	data["label"] = truncate_for_field_label(get_node_label_exercise(exercise, index, field_question));
	htvi = &htvi_exercise.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_exercise, field_question).u64);

	data["label"] = truncate_for_field_label(get_node_label_exercise(exercise, index, field_analysis));
	htvi = &htvi_exercise.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_exercise, field_analysis).u64);

	data["label"] = truncate_for_field_label(get_node_label_exercise(exercise, index, field_answer));
	htvi = &htvi_exercise.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, type_exercise, field_answer).u64);

	htvi_exercise.unfold();
}

void tcourseware2::courseware_update_to_tree(const aplt::tcourseware& courseware)
{
	ttree& l_tree = *tree_widget_;
	l_tree.clear();
	
	std::map<std::string, std::string> data;
	ttree_node* htvi;

	std::stringstream ss;
	ttree_node& htvi_root = l_tree.get_root_node();

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_title));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_title).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_uuid));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_uuid).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_type));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_type).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_tex_header));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_tex_header).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_tex_tail));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_tex_tail).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_content));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_content).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_annotation));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_annotation).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_analysis));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_analysis).u64);

	data["label"] = truncate_for_field_label(get_node_label_global(courseware, field_reference));
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, type_global, field_reference).u64);

	int index = 0;
	for (std::vector<aplt::tcourseware::tkeypoint>::const_iterator it = courseware.keypoints.begin(); it != courseware.keypoints.end(); ++ it, index ++) {
		const aplt::tcourseware::tkeypoint& keypoint = *it;
		keypoint_update_to_tree(htvi_root, index, keypoint);
	}

	index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;
		exercise_update_to_tree(htvi_root, index, exercise);
	}

	validate_tree_cookie();
}

tcookie3f tcourseware2::courseware_update_to_tree_with_cursel(const aplt::tcourseware& courseware)
{
	ttree& tree = *tree_widget_;
	ttree_node* node = tree.cursel();
	VALIDATE(node != nullptr, null_str);

	uint64_t cookie = node->cookie();
	tcookie3f cookie3f(cookie);

	courseware_update_to_tree(courseware);

	node = tree.get_root_node().find_node_from_cookie(cookie3f.u64);
	VALIDATE(node != nullptr, null_str);
	tree.select_node(node);

	return cookie3f;
}

void tcourseware2::validate_tree_cookie() const
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const aplt::tcourseware& courseware = tmp_courseware_;
	ttree& tree = *tree_widget_;
	ttree_node& htvi_root = tree.get_root_node();

	const std::vector<ttree_node*>& children = htvi_root.children();
	VALIDATE(children.size() == global_atts_ + courseware.keypoints.size() + courseware.exercises.size(), null_str);
	int child_at = global_atts_;

	// startup next states
	int index = 0;
	for (std::vector<aplt::tcourseware::tkeypoint>::const_iterator it = courseware.keypoints.begin(); it != courseware.keypoints.end(); ++ it, index ++) {
		const aplt::tcourseware::tkeypoint& keypoint = *it;

		tcookie3f cookie3f(index, type_keypoint, field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}

	index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;

		tcookie3f cookie3f(index, type_exercise, field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}
	VALIDATE(child_at == children.size(), null_str);
}

bool tcourseware2::is_active_fill_default(const tcookie3f& cookie3f) const
{
	const aplt::tcourseware& courseware = tmp_courseware_;
	if (courseware.type != aplt::tcourseware::type_exam) {
		return false;
	}
	if (cookie3f.type == type_global) {
		return cookie3f.field == field_tex_header || cookie3f.field == field_tex_tail || cookie3f.field == field_content;
	}
	if (cookie3f.type == type_exercise) {
		return cookie3f.field == field_section || cookie3f.field == field_answer;
	}
	return false;
}

void tcourseware2::refresh_toolbar_active(const tcookie3f* cookie3f)
{
	if (curr_layer_ == DOWNLOAD_LAYER) {
		if (file_list_->cursel() == nullptr) {
			favorite_widget_->set_label(unfavorite_png);
			return;
		}

		return;

	} else if (curr_layer_ == COURSELIST_LAYER) {
		return;
	}

	VALIDATE(curr_layer_ == UPLOAD_LAYER, null_str);
	if (file_list_->cursel() == nullptr) {
		favorite_widget_->set_label(unfavorite_png);

		upload_widget_->set_active(false);

		insert_keypoint_widget_->set_active(false);
		insert_exercise_widget_->set_active(false);
		move_down_widget_->set_active(false);
		move_up_widget_->set_active(false);
		erase_widget_->set_active(false);

		// erase_task_widget_->set_active(false);
		return;
	}

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const aplt::tcourseware& courseware = tmp_courseware_;

	upload_widget_->set_active(true);

	fill_default_widget_->set_active(cookie3f != nullptr && is_active_fill_default(*cookie3f));

	// erase/erase state2/startup_next_state/key_2_state
	insert_keypoint_widget_->set_active(true);
	insert_exercise_widget_->set_active(true);

	bool active_movedown = cookie3f != nullptr && cookie3f->field == field_typeself;
	bool active_moveup = cookie3f != nullptr && cookie3f->field == field_typeself;
	if (active_movedown) {
		if (cookie3f->type == type_keypoint) {
			active_movedown = courseware.keypoints.size() >= 2 && cookie3f->index != (int)courseware.keypoints.size() - 1;
			active_moveup = courseware.keypoints.size() >= 2 && cookie3f->index > 0;

		} else if (cookie3f->type == type_exercise) {
			active_movedown = courseware.exercises.size() >= 2 && cookie3f->index != (int)courseware.exercises.size() - 1;
			active_moveup = courseware.exercises.size() >= 2 && cookie3f->index > 0;

		} else {
			active_movedown = false;
			active_moveup = false;
		}
	}
	if (active_movedown) {
		move_down_widget_->set_cookie(cookie3f->u64);
	}
	move_down_widget_->set_active(active_movedown);

	if (active_moveup) {
		move_up_widget_->set_cookie(cookie3f->u64);
	}
	move_up_widget_->set_active(active_moveup);

	bool active_erase = cookie3f != nullptr && cookie3f->field == field_typeself;
	if (active_erase) {
		erase_widget_->set_cookie(cookie3f->u64);
	}
	erase_widget_->set_active(active_erase);

	// insert/erase task
	// erase_task_widget_->set_active(true);
}

void tcourseware2::did_node_changed(ttree_node& node)
{
	VALIDATE(file_list_->cursel() != nullptr, null_str);
	const aplt::tcourseware& courseware = tmp_courseware_;

	if (!editing_ || is_building()) {
		return;
	}

	uint64_t cookie = node.cookie();
	tcookie3f cookie3f(cookie);

	bool msg_read_only = false;
	std::string msg;
	curr_problem_.clear();
	if (cookie3f.type == type_global) {
		if (cookie3f.field == field_uuid) {
			utils::string_map symbols;
			symbols["uuid"] = courseware.uuid;
			symbols["uuid_field"] = get_field_str(type_global, field_uuid);
			symbols["title_field"] = get_field_str(type_global, field_title);

			msg = vgettext2("courseware^uuid msg, $uuid, $uuid_field, $title_field", symbols);
			msg_read_only = true;

		} else if (cookie3f.field == field_type) {
			// msg = courseware.tex_header;

		} else if (cookie3f.field == field_tex_header) {
			msg = courseware.tex_header;

		} else if (cookie3f.field == field_tex_tail) {
			msg = courseware.tex_tail;

		} else if (cookie3f.field == field_title) {
			msg = courseware.title;

		} else if (cookie3f.field == field_content) {
			msg = courseware.content;

		} else if (cookie3f.field == field_annotation) {
			msg = courseware.annotation;

		} else if (cookie3f.field == field_analysis) {
			msg = courseware.analysis;

		} else if (cookie3f.field == field_reference) {
			msg = courseware.reference;

		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == type_keypoint) {
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)courseware.keypoints.size(), null_str);
		const aplt::tcourseware::tkeypoint& keypoint = courseware.keypoints[cookie3f.index];
		
		if (cookie3f.field == field_typeself) {

		} else if (cookie3f.field == field_section) {
			msg = keypoint.section;

		} else if (cookie3f.field == field_name) {
			msg = keypoint.name;

		} else if (cookie3f.field == field_annotation) {
			msg = keypoint.annotation;

		} else if (cookie3f.field == field_analysis) {
			msg = keypoint.analysis;

		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == type_exercise) {
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)courseware.exercises.size(), null_str);
		const aplt::tcourseware::texercise& exercise = courseware.exercises[cookie3f.index];
		
		if (cookie3f.field == field_typeself) {

		} else if (cookie3f.field == field_section) {
			msg = exercise.section;

		} else if (cookie3f.field == field_question) {
			msg = exercise.question;
			if (courseware.type == aplt::tcourseware::type_exam) {
				// desire_input_layer = INPUT_EXAM_LAYER;
				curr_problem_.set(cookie3f.index);
			}

		} else if (cookie3f.field == field_answer) {
			msg = exercise.answer;

		} else if (cookie3f.field == field_analysis) {
			msg = exercise.analysis;

		} else {
			VALIDATE(false, null_str);
		}
	}

	int desire_input_layer = INPUT_MSG_LAYER;
	if (!curr_problem_.valid()) {
		tignore_tree_msg_text_changed_lock lock(*this);
		set_tree_msg_label(msg, cookie3f, msg_read_only);

	} else {
		desire_input_layer = INPUT_EXAM_LAYER;
		reload_input_exam();
	}
	input_stack_->set_radio_layer(desire_input_layer);

	refresh_toolbar_active(&cookie3f);
}

void tcourseware2::set_tree_node_label_from_cookie(ttree& tree, uint64_t cookie, const std::string& node_label)
{
	// if (!new_node_label.empty()) {
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
		VALIDATE(node != nullptr, null_str);
		node->set_widget_label("label", truncate_for_field_label(node_label));
		// l_tree_->select_node(node);
	// }
}

void tcourseware2::did_tree_msg_text_changed(ttext_box& widget)
{
	if (ignore_tree_msg_text_changed_) {
		return;
	}

	VALIDATE(file_list_->cursel() != nullptr, null_str);
	aplt::tcourseware& courseware = tmp_courseware_;

	std::string label = widget.label();
	// in some case, '\n' or 'space' is usable, for example latex.
	// utils::strip(label);

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	bool id_changed = false;
	bool id_or_name_changed = false;
	aplt::tcpp_api::tstate2* state2_name_changed = nullptr;
	std::string new_node_label;
	bool l_tree = true;
	std::string status_msg;
	int n32 = nposm;
	if (cookie3f.type == type_global) {
		if (cookie3f.field == field_uuid) {
			// courseware.type = label;

		} else if (cookie3f.field == field_type) {
			// courseware.type = label;

		} else if (cookie3f.field == field_tex_header) {
			courseware.tex_header = label;

		} else if (cookie3f.field == field_tex_tail) {
			courseware.tex_tail = label;

		} else if (cookie3f.field == field_title) {
			// int name_chars = utils::utf8str_len(label);
			// if (name_chars >= task_name_chars_range_.min && name_chars <= task_name_chars_range_.max) {
			if (isvalid_normal_utf8_name224(label)) {
				// if (label == pair.name || task_pair_by_name(tmp_task_pairs_, label, &pair) == nullptr) {
				if (!label.empty()) {

					// id_or_name_changed = label != pair.name;
					courseware.title = label;

					refresh_title(courseware.title);
					
				} else {
					status_msg = _("'title' cannot be the same");
				}
			} else {
				// status_msg = get_error_msg(pair, cookie3f.type, cookie3f.field);
			}

		} else if (cookie3f.field == field_content) {
			courseware.content = label;

		} else if (cookie3f.field == field_annotation) {
			courseware.annotation = label;

		} else if (cookie3f.field == field_analysis) {
			courseware.analysis = label;

		} else if (cookie3f.field == field_reference) {
			courseware.reference = label;

		} else {
			VALIDATE(false, null_str);
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_global(courseware, cookie3f.field);
		}

	} else if (cookie3f.type == type_keypoint) {
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)courseware.keypoints.size(), null_str);
		aplt::tcourseware::tkeypoint& keypoint = courseware.keypoints[cookie3f.index];

		if (cookie3f.field == field_typeself) {

		} else if (cookie3f.field == field_section) {
			keypoint.section = label;

		} else if (cookie3f.field == field_name) {
			keypoint.name = label;

		} else if (cookie3f.field == field_annotation) {
			keypoint.annotation = label;

		} else if (cookie3f.field == field_analysis) {
			keypoint.analysis = label;

		} else {
			VALIDATE(false, null_str);
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_keypoint(keypoint, cookie3f.index, cookie3f.field);
		}

	} else if (cookie3f.type == type_exercise) {
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)courseware.exercises.size(), null_str);
		aplt::tcourseware::texercise& exercise = courseware.exercises[cookie3f.index];

		if (cookie3f.field == field_typeself) {

		} else if (cookie3f.field == field_section) {
			exercise.section = label;

		} else if (cookie3f.field == field_question) {
			exercise.question = label;

		} else if (cookie3f.field == field_answer) {
			exercise.answer = label;

		} else if (cookie3f.field == field_analysis) {
			exercise.analysis = label;

		} else {
			VALIDATE(false, null_str);
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_exercise(exercise, cookie3f.index, cookie3f.field);
		}
	}

	if (!new_node_label.empty()) {
		set_tree_node_label_from_cookie(*tree_widget_, cookie, new_node_label);
/*
		ttree& tree = *tree_widget_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
		VALIDATE(node != nullptr, null_str);
		node->set_widget_label("label", truncate_for_field_label(new_node_label));
		// l_tree_->select_node(node);
*/
	}

	set_status_label(status_msg);

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
}

void tcourseware2::clear_tree2()
{
	if (file_list_->cursel() == nullptr) {
		tree_widget_->clear();
	}

	{
		tignore_tree_msg_text_changed_lock lock(*this);
		tree_msg_widget_->set_label(null_str);
	}
}

void tcourseware2::set_tree_msg_label(const std::string& label, const tcookie3f& cookie3f, bool read_only)
{
	tree_msg_widget_->set_label(label);

	tree_msg_widget_->tb()->set_cookie(cookie3f.u64);
	tree_msg_widget_->tb()->set_placeholder(get_placeholder_msg(cookie3f.type, cookie3f.field));

	// tree_msg_widget_->tb()->set_active(!read_only);
}

void set_tex_cols_widget_label(tbutton& widget, int tex_cols)
{
	widget.set_label(tex_cols != EXAM_TEX_COLS1? "misc/cols2.png": "misc/cols1.png");
}

void tcourseware2::reload_input_exam()
{
	VALIDATE(curr_problem_.valid(), null_str);
	tcurr_exam_problem& curr = curr_problem_;

	tignore_input_exam_changed_lock lock(*this);

	bool visible_prev = false;
	bool visible_next = false;
	if (curr.p->type == rexam_type_solving && !curr.p->subproblems.empty()) {
		VALIDATE(curr.sub_at >= nposm && curr.sub_at < (int)curr.p->subproblems.size(), null_str);
		if (curr.sub_at != nposm) {
			visible_prev = true;
		}
		if (curr.sub_at < (int)curr.p->subproblems.size() - 1) {
			visible_next = true;
		}
	}

	tgrid& grid = *input_stack_->layer(INPUT_EXAM_LAYER);
	find_widget<tbutton>(&grid, "prev", false, true)->set_visible(visible_prev? twidget::VISIBLE: twidget::INVISIBLE);
	find_widget<tbutton>(&grid, "next", false, true)->set_visible(visible_next? twidget::VISIBLE: twidget::INVISIBLE);;

	bool visible_down = curr.p->type == rexam_type_solving && curr.sub_at >= 0 && curr.sub_at < (int)curr.p->subproblems.size() - 1;
	find_widget<tbutton>(&grid, "move_down", false, true)->set_visible(visible_down? twidget::VISIBLE: twidget::INVISIBLE);

	bool visible_insert = curr.p->type == rexam_type_solving && curr.sub_at == nposm;
	bool visible_erase = curr.p->type == rexam_type_solving && curr.sub_at != nposm;
	find_widget<tbutton>(&grid, "insert", false, true)->set_visible(visible_insert? twidget::VISIBLE: twidget::INVISIBLE);
	find_widget<tbutton>(&grid, "erase", false, true)->set_visible(visible_erase? twidget::VISIBLE: twidget::INVISIBLE);;

	char buf[64] = "\0";
	if (curr.p->type == rexam_type_solving && !curr.p->subproblems.empty()) {
		SDL_snprintf(buf, sizeof(buf), "%i/%i", curr.sub_at + 1, (int)curr.p->subproblems.size());
	}
	find_widget<tlabel>(&grid, "solving_label", false, true)->set_label(buf);

	const texam_problem& this_p = curr.this_p();

	tbutton* type_widget = find_widget<tbutton>(&grid, "type", false, true);

	if (aplt::rexam_str_types.count(this_p.type) == 0) {
		// type_widget->set_label(_("Invalid problem."));
		type_widget->set_label(null_str);
		return;
	}

	const tcode3& rexam_str_type = aplt::rexam_str_types.find(this_p.type)->second;
	type_widget->set_label(rexam_str_type.name);

	// tscroll_text_box
	find_widget<ttext_box>(&grid, "points", false, true)->set_label(str_cast(this_p.points));
	find_widget<tscroll_text_box>(&grid, "question", false, true)->set_label(this_p.question);

	tbutton* tex_cols_widget = find_widget<tbutton>(&grid, "tex_cols", false, true);
	if (this_p.type == rexam_type_choice) {
		find_widget<tgrid>(&grid, "correct_option_grid", false, true)->set_visible(twidget::VISIBLE);
		find_widget<tgrid>(&grid, "option_grid", false, true)->set_visible(twidget::VISIBLE);

		for (int at = 0; at < 4; at ++) {
			SDL_snprintf(buf, sizeof(buf), "correct_option_%c", 'a' + at);
			find_widget<ttoggle_button>(&grid, buf, false, true)->set_value(this_p.correct_options.count(at + 1) != 0);
		}

		for (int at = 0; at < 4; at ++) {
			SDL_snprintf(buf, sizeof(buf), "option_%c", 'a' + at);
			find_widget<tscroll_text_box>(&grid, buf, false, true)->set_label(at < (int)this_p.options.size()? this_p.options[at]: null_str);
		}

		set_tex_cols_widget_label(*tex_cols_widget, this_p.tex_cols);
		tex_cols_widget->set_visible(twidget::VISIBLE);

	} else {
		find_widget<tgrid>(&grid, "correct_option_grid", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<tgrid>(&grid, "option_grid", false, true)->set_visible(twidget::INVISIBLE);

		tex_cols_widget->set_visible(twidget::INVISIBLE);
	}

	find_widget<tscroll_text_box>(&grid, "explanation", false, true)->set_label(this_p.explanation);
}

void tcourseware2::did_input_exam_changed_quited(bool dirty)
{
	// set_status_label(err_msg);
	// if (err_msg.empty()) {
	if (dirty) {
		courseware_dirty_4_exam();
	}
}

void tcourseware2::click_input_exam_type(tbutton& widget)
{
	VALIDATE(curr_problem_.valid(), null_str);
	tcurr_exam_problem& curr = curr_problem_;

	if (curr.p->type == rexam_type_solving && !curr.p->subproblems.empty() && curr.sub_at == nposm) {
		std::string err_msg = _("This solving question still contains sub-questions, cannot modfy question type.");
		gui2::show_message(null_str, err_msg);
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	texam_problem& this_p = curr_problem_.this_p();

	int type_count = sizeof(rexam_str_types_C) / sizeof(rexam_str_types_C[0]);
	for (int at = 0; at < type_count; at ++) {
		const trexam_str_type& str_type = rexam_str_types_C[at];

		if (str_type.type == rexam_type_solving) {
			 if (curr.sub_at != nposm) {
				 continue;
			 }

		} else if (str_type.type == rexam_type_other) {
			if (curr.sub_at == nposm) {
				continue;
			}
		}

		items.push_back(gui2::tmenu::titem(str_type.msgstr, at));
		if (this_p.type == at) {
			initial_sel = at;
		}
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	bool dirty = false;
	// VALIDATE(err_msg.empty(), null_str);
	// tauto_destruct_executor destruct_executor(std::bind(&tcourseware2::did_input_exam_changed_quited, this, std::ref(dirty)));

	const int new_value = dlg.selected_val();
	this_p.type = new_value;
	dirty = true;

	// widget.set_label(rexam_str_types_C[new_value].msgstr);
	if (this_p.type == rexam_type_choice) {
		this_p.options.resize(4);
	}
	reload_input_exam();
/*
	if (this_p.type == rexam_type_choice) {
		if (this_p.options.empty() || this_p.correct_options.empty()) {
			err_msg = _("For choice problem, both the option and the correct option must not be empty");
		}
	}
*/
	courseware_dirty_4_exam();
}

void tcourseware2::click_input_exam_prev_or_next(tbutton& widget, int fid)
{
	VALIDATE(curr_problem_.valid(), null_str);
	tcurr_exam_problem& curr = curr_problem_;

	VALIDATE(curr.sub_at >= nposm && curr.sub_at < (int)curr.p->subproblems.size(), null_str);

	VALIDATE(curr.p->type == rexam_type_solving, null_str);

	if (fid == fid_exam_prev) {
		VALIDATE(curr.sub_at >= nposm, null_str);
		curr.sub_at --;

	} else if (fid == fid_exam_next) {
		VALIDATE(curr.sub_at < (int)curr.p->subproblems.size() - 1, null_str);
		curr.sub_at ++;

	} else {
		VALIDATE(false, null_str);
	}
	reload_input_exam();
}

void tcourseware2::click_input_exam_tex_cols(tbutton& widget)
{
	VALIDATE(curr_problem_.valid(), null_str);
	tcurr_exam_problem& curr = curr_problem_;

	texam_problem& this_p = curr_problem_.this_p();
	VALIDATE(this_p.type == rexam_type_choice, null_str);

	if (this_p.tex_cols == EXAM_TEX_COLS2) {
		this_p.tex_cols = EXAM_TEX_COLS1;
	} else {
		this_p.tex_cols = EXAM_TEX_COLS2;
	}
	set_tex_cols_widget_label(widget, this_p.tex_cols);

	courseware_dirty_4_exam();
}

void tcourseware2::problem_swap(std::vector<texam_problem>& problems, int at1, int at2)
{
	int size = problems.size();
	VALIDATE(at1 >= 0 && at1 < size && at2 >= 0 && at2 < size, null_str);

	std::swap(problems[at1], problems[at2]);
}

void tcourseware2::click_input_exam_down_or_insert_or_erase(tbutton& widget, int fid)
{
	VALIDATE(curr_problem_.valid(), null_str);
	tcurr_exam_problem& curr = curr_problem_;

	VALIDATE(curr.sub_at >= nposm && curr.sub_at < (int)curr.p->subproblems.size(), null_str);

	if (fid == fid_exam_down) {
		VALIDATE(curr.sub_at >= 0 && curr.sub_at < (int)curr.p->subproblems.size() - 1, null_str);

		const int at1 = curr.sub_at;
		const int at2 = at1 + 1;
		problem_swap(curr.p->subproblems, at1, at2);
		curr.sub_at = at2;

	} else if (fid == fid_exam_insert) {
		VALIDATE(curr.sub_at == nposm, null_str);
		const int default_points = 5;
		curr.p->subproblems.push_back(texam_problem(rexam_type_fillin, default_points, null_str, null_str));

		const std::string ok_str = _("Stay here");
		const std::string cancel_str = _("Go to the new question");

		utils::string_map symbols;
		symbols["ok"] = ok_str;
		symbols["cancel"] = cancel_str;
		std::string msg = vgettext2("A new sub-question has been added to the end of this problem. Would you like to $ok or $cancel?", symbols);
		const int res = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, null_str, null_str, ok_str, cancel_str);
		if (res != gui2::twindow::OK) {
			curr.sub_at = curr.p->subproblems.size() - 1;
		}

	} else if (fid == fid_exam_erase) {
		VALIDATE(curr.sub_at >= 0 && curr.sub_at < (int)curr.p->subproblems.size(), null_str);
		std::vector<texam_problem>::iterator it = curr.p->subproblems.begin();
		if (curr.sub_at != 0) {
			std::advance(it, curr.sub_at);
		}

		const texam_problem& sub = *it;
		const std::string msg = _("Do you want to delete this sub-question?");
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		curr.p->subproblems.erase(it);
		if (curr.sub_at == curr.p->subproblems.size()) {
			curr.sub_at --;
		}

	} else {
		VALIDATE(false, null_str);
	}

	courseware_dirty_4_exam();
	reload_input_exam();
}

void tcourseware2::did_input_exam_text_changed(ttext_box& widget, int fid)
{
	VALIDATE(curr_problem_.valid(), null_str);

	if (ignore_input_exam_changed_) {
		return;
	}

	const std::string& label = widget.label();
	texam_problem& this_p = curr_problem_.this_p();

	// bool dirty = false;
	// tauto_destruct_executor destruct_executor(std::bind(&tcourseware2::did_input_exam_changed_quited, this, std::ref(dirty)));
/*
	if (!utils::is_utf8str(label.c_str(), label.size())) {
		err_msg = "must be utf-8 format.";
		return;
	}
*/
	if (fid == fid_exam_points) {
		int points = utils::to_int(label);
		this_p.points = points;

	} else if (fid == fid_exam_question) {
		this_p.question = label;

	} else if (fid == fid_exam_expl) {
		this_p.explanation = label;

	} else if (fid >= fid_exam_option_a && fid_exam_option_d) {
		int opt = fid - fid_exam_option_a;
		this_p.options[opt] = label;

	} else {
		VALIDATE(false, null_str);
	}

	courseware_dirty_4_exam();
}

void tcourseware2::did_input_exam_state_changed(ttoggle_button& widget, int fid)
{
	VALIDATE(curr_problem_.valid(), null_str);

	if (ignore_input_exam_changed_) {
		return;
	}

	// bool dirty = false;
	// tauto_destruct_executor destruct_executor(std::bind(&tcourseware2::did_input_exam_changed_quited, this, std::ref(dirty)));
	
	texam_problem& this_p = curr_problem_.this_p();

	if (fid >= fid_exam_correct_option_a && fid_exam_correct_option_d) {
		int opt = fid - fid_exam_correct_option_a + 1;
		if (widget.get_value()) {
			VALIDATE(this_p.correct_options.count(opt) == 0, null_str);
			this_p.correct_options.insert(opt);

		} else {
			VALIDATE(this_p.correct_options.count(opt) != 0, null_str);
			this_p.correct_options.erase(opt);
		}

	} else {
		VALIDATE(false, null_str);
	}

	courseware_dirty_4_exam();
}

void tcourseware2::set_status_label(const std::string& label, bool add_time)
{
	if (add_time) {
		std::stringstream ss;
		time_t t = time(nullptr);
		ss << utils::format_time_hms(t) << " ";
		ss << label;
		status_widget_->set_label(ss.str());

	} else {
		status_widget_->set_label(label);
	}
}

void tcourseware2::build_timer_handler()
{
	std::string msg;
	if (build_msg_.data != nullptr) {
		threading::lock lock(build_msg_mutex_);
		msg.assign(build_msg_.data, build_msg_.vsize);
	}
	build_msg_widget_->set_label(msg);

	// build_msg_widget_->set_scroll_to_end(true);
}

void tcourseware2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);

	const std::string& label = listen_widget_->label();
	if (speech_driver_.is_listening()) {
		if (label == unlisten_png) {
			listen_widget_->set_label(listening_png);
		}
	} else {
		if (label == listening_png) {
			listen_widget_->set_label(unlisten_png);
		}
	}
}

void tcourseware2::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_BUILD_FINISHED:
		{
			tmsg_data_build_finished* pdata = static_cast<tmsg_data_build_finished*>(msg->pdata);
			build_finished(pdata->result);
			break;
		}
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

} // namespace gui2

