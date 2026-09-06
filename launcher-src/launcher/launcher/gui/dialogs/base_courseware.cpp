#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/base_courseware.hpp"
#include "gui/widgets/listbox.hpp"
#include "filesystem.hpp"
#include "config_cache.hpp"
#include "gettext.hpp"

#include <boost/foreach.hpp>

using namespace std::placeholders;

namespace gui2 {

tbase_courseware::tbase_courseware(const std::string& saves_courseware_dir)
	: upload_path_(saves_courseware_dir + "/upload")
	, download_path_(saves_courseware_dir + "/download")
	, msgstr_notempty_and_utf8str_(_("Value must not be empty, and utf-8 format string"))
	, msgstr_empty_or_utf8str_(_("Value is empty, or utf-8 format string"))
	, cookie3f_nposm_(0, 255, 0) // A value that would normally not be possible
	, global_atts_(9)
{}

const tbase_courseware::tcourseware_file& tbase_courseware::courseware_file_from_at(int at) const
{
	if (use_map_courseware_files()) {
		VALIDATE(at >= 0 && at < (int)courseware_files_.size(), null_str);
		std::map<std::string, tcourseware_file>::const_iterator find_it = courseware_files_.begin();
		if (at != 0) {
			std::advance(find_it, at);
		}
		return find_it->second;
	}

	VALIDATE(at >= 0 && at < (int)courseware_files2_.size(), null_str);
	return courseware_files2_[at];
}

tbase_courseware::tcourseware_file& tbase_courseware::mutable_courseware_file_from_at(int at)
{
	if (use_map_courseware_files()) {
		VALIDATE(at >= 0 && at < (int)courseware_files_.size(), null_str);
		std::map<std::string, tcourseware_file>::iterator find_it = courseware_files_.begin();
		if (at != 0) {
			std::advance(find_it, at);
		}
		return find_it->second;
	}

	VALIDATE(at >= 0 && at < (int)courseware_files2_.size(), null_str);
	return courseware_files2_[at];
}

std::string tbase_courseware::join_courseware_dir2(const std::string& root_path, const std::string& dir_name) const
{
	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%s/%s", root_path.c_str(), dir_name.c_str());
	return buf;
}

std::string tbase_courseware::join_main_cfg_filename2(const std::string& root_path, const std::string& dir_name) const
{
	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%s/%s/main.cfg", root_path.c_str(), dir_name.c_str());
	return buf;
}

std::string tbase_courseware::join_distribution_cfg_filename(const std::string& dir_name) const
{
	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%s/%s/distribution.cfg", courseware_load_path().c_str(), dir_name.c_str());
	return buf;
}

bool tbase_courseware::did_walk_courseware(const std::string& dir, const SDL_dirent2* dirent, bool download, std::map<std::string, tbase_courseware::tcourseware_file>& files, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		const std::string cfgfile = join_main_cfg_filename2(dir, dirent->name);
		if (SDL_IsFile(cfgfile.c_str())) {
			tcourseware_distribution_vals vals = get_distribution_vals(join_courseware_dir2(root, dirent->name));
			files.insert(std::make_pair(dirent->name, tbase_courseware::tcourseware_file(download, dirent->name, vals.username, vals.uuid, vals.ts, 0)));
		}
	}
	return true;
}

void tbase_courseware::collect_courseware(bool download, const std::string& dir, std::map<std::string, tbase_courseware::tcourseware_file>& files)
{
	files.clear();
	const std::string& root_dir = dir;

	std::map<std::string, std::string> full_images;
	walk_dir(root_dir, false, std::bind(&tbase_courseware::did_walk_courseware, this, _1, _2, download, std::ref(files), std::ref(root_dir)));
}

void tbase_courseware::load_courseware_cfg2(const std::string& cfgfile, aplt::tcourseware& result)
{
	result.clear();

	// don't use config_cache::instance(). it will result '{exam}' is macro, try to them.
/*
	config_cache_transaction transaction;
	config_cache& cache = config_cache::instance();
	cache.clear_defines();

	config cfg;
	cache.get_config(cfgfile, cfg);
	// if (cfg.empty()) {
	//	return;
	// }
*/
	std::string stream;
	{
		const int max_task_cpp_cfg_size = 5 * CONSTANT_1M; // 5M bytes
		tfile file(cfgfile, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			return;
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			return;
		}
		stream.assign(file.data, fsize);
	}

	config cfg;
	aplt::read_config_ex(stream, true, cfg);

	BOOST_FOREACH (const config& courseware_cfg, cfg.child_range("courseware")) {
		result.from_cfg(courseware_cfg);
	}
}

tbase_courseware::tcourseware_distribution_vals tbase_courseware::get_distribution_vals(const std::string& courseware_dir)
{
	config cfg = aplt::get_distribution_cfg(courseware_dir);

	int64_t uid = cfg["uid"].to_int64();
	std::string username = cfg["username"].str();
	std::string uuid = cfg["uuid"].str();
	int64_t ts = cfg["ts"].to_int64();

	tcourseware_distribution_vals result(uid, username, uuid, ts);
	return result;
}

std::string tbase_courseware::get_field_str(int type, int field) const
{
	if (type == type_global) {
		if (field == field_uuid) {
			return _("UUID");
		} else if (field == field_type) {
			return _("Type");
		} else if (field == field_tex_header) {
			return _("Tex header");
		} else if (field == field_tex_tail) {
			return _("Tex tail");
		} else if (field == field_title) {
			return _("Title");
		} else if (field == field_content) {
			return _("Content");
		} else if (field == field_annotation) {
			return _("Annotation");
		} else if (field == field_analysis) {
			return _("field^Analysis");
		} else if (field == field_reference) {
			return _("Reference");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == type_keypoint) {
		if (field == field_typeself) {
			return _("keypoint");
		} else if (field == field_section) {
			return _("courseware^section");
		} else if (field == field_name) {
			return _("Name");
		} else if (field == field_annotation) {
			return _("Annotation");
		} else if (field == field_analysis) {
			return _("keypoint^Analysis");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == type_exercise) {
		if (field == field_typeself) {
			return _("Exeercise");
		} else if (field == field_section) {
			return _("courseware^section");
		} else if (field == field_question) {
			return _("Question");
		} else if (field == field_analysis) {
			return _("exercise^Analysis");
		} else if (field == field_answer) {
			return _("Answer");
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

std::string tbase_courseware::get_placeholder_msg(int type, int field) const
{
	utils::string_map symbols;
	char buf[256];
	std::string placeholder;
	if (type == type_global) {
		if (field == field_uuid) {

		} else if (field == field_type) {

		} else if (field == field_tex_header) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_tex_tail) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_title) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS);
			symbols["range"] = buf;
			placeholder = vgettext2("utf-8 format string, and the number of characters is in the range $range", symbols);

		} else if (field == field_content) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == field_annotation) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_analysis) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_reference) {
			placeholder = msgstr_empty_or_utf8str_;
		} 

	} else if (type == type_keypoint) {
		if (field == field_typeself) {

		} else if (field == field_section) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_name) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == field_annotation) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == field_analysis) {
			placeholder = msgstr_notempty_and_utf8str_;

		}

	} else if (type == type_exercise) {
		if (field == field_section) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == field_question) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == field_analysis) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == field_answer) {
			placeholder = msgstr_empty_or_utf8str_;

		}
	}
	
	return placeholder;
}

std::string tbase_courseware::get_error_msg(const aplt::tcourseware& courseware, int type, int field) const
{
	utils::string_map symbols;
	symbols["field"] = get_field_str(type, field);
	const std::string note = get_placeholder_msg(type, field);

	std::string err_msg;
	if (note.empty()) {
		err_msg = vgettext2("Invalid '$field'", symbols);
	} else {
		symbols["note"] = note;
		err_msg = vgettext2("Invalid '$field'. $note", symbols);
	}
	return err_msg;
}

std::string tbase_courseware::get_node_label_global(const aplt::tcourseware& courseware, int field) const
{
	std::stringstream ss;
	const std::string field_str = get_field_str(type_global, field);
	if (field == field_uuid) {
		ss << field_str << ": " << courseware.uuid;

	} else if (field == field_type) {
		ss << field_str << ": " << aplt::tcourseware::types[courseware.type].name;

	} else if (field == field_tex_header) {
		ss << field_str << ": " << courseware.tex_header;

	} else if (field == field_tex_tail) {
		ss << field_str << ": " << courseware.tex_tail;

	} else if (field == field_title) {
		ss << field_str << ": " << courseware.title;

	} else if (field == field_content) {
		ss << field_str << ": " << courseware.content;

	} else if (field == field_annotation) {
		ss << field_str << ": " << courseware.annotation;

	} else if (field == field_analysis) {
		ss << field_str << ": " << courseware.analysis;

	} else if (field == field_reference) {
		ss << field_str << ": " << courseware.reference;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

std::string tbase_courseware::get_node_label_keypoint(const aplt::tcourseware::tkeypoint& keypoint, int index, int field) const
{
	std::stringstream ss;
	const std::string field_str = get_field_str(type_keypoint, field);
	if (field == field_typeself) {
		ss << "#" << index + 1 << " " << field_str;

	} else if (field == field_section) {
		ss << field_str << ": " << keypoint.section;

	} else if (field == field_name) {
		ss << field_str << ": " << keypoint.name;

	} else if (field == field_annotation) {
		ss << field_str << ": " << keypoint.annotation;

	} else if (field == field_analysis) {
		ss << field_str << ": " << keypoint.analysis;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

std::string tbase_courseware::get_node_label_exercise(const aplt::tcourseware::texercise& exercise, int index, int field) const
{
	std::stringstream ss;
	const std::string field_str = get_field_str(type_exercise, field);
	if (field == field_typeself) {
		ss << "#" << index + 1 << " " << field_str;

	} else if (field == field_section) {
		ss << field_str << ": " << exercise.section;

	} else if (field == field_question) {
		ss << field_str << ": " << exercise.question;

	} else if (field == field_analysis) {
		ss << field_str << ": " << exercise.analysis;

	} else if (field == field_answer) {
		ss << field_str << ": " << exercise.answer;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

#define MAX_ANALYSIS_LABEL_CHARS	1000
static std::string truncate_for_analysis_label(const std::string& label)
{
	if (label.empty()) {
		return label;
	}

	int max_chars = MAX_ANALYSIS_LABEL_CHARS;

	bool ellipsis = true;
	return utils::truncate_to_max_chars2(label, max_chars, ellipsis);
}

void tbase_courseware::insert_list_line_global(tlistbox& list, const aplt::tcourseware& courseware, int field)
{
	const int type = type_global;
	std::string val;
	if (field == field_uuid) {
		val = courseware.uuid;

	} else if (field == field_type) {
		val = aplt::tcourseware::types[courseware.type].name;

	} else if (field == field_tex_header) {
		val = courseware.tex_header;

	} else if (field == field_tex_tail) {
		val = courseware.tex_tail;

	} else if (field == field_title) {
		val = courseware.title;

	} else if (field == field_content) {
		val = courseware.content;

	} else if (field == field_annotation) {
		val = courseware.annotation;

	} else if (field == field_analysis) {
		val = truncate_for_analysis_label(courseware.analysis);

	} else if (field == field_reference) {
		val = courseware.reference;

	} else {
		VALIDATE(false, null_str);
	}

	const std::string field_str = get_field_str(type, field);
	const std::map<std::string, std::string> data {
		{"major_label", field_str}, 
		{"minor_label", val}};

	list.insert_row(data).set_cookie(tcookie3f(0, type, field).u64);
}

void tbase_courseware::insert_list_line_keypoint(tlistbox& list, const aplt::tcourseware::tkeypoint& keypoint, int index, int field)
{
	const int type = type_keypoint;
	std::string val;
	if (field == field_section) {
		val = keypoint.section;

	} else if (field == field_name) {
		val = keypoint.name;

	} else if (field == field_annotation) {
		val = keypoint.annotation;

	} else if (field == field_analysis) {
		val = truncate_for_analysis_label(keypoint.analysis);

	} else {
		VALIDATE(false, null_str);
	}

	const std::string field_str = get_field_str(type, field);

	std::stringstream ss;
	ss << "#" << index + 1 << " " << get_field_str(type, field_typeself);
	ss << "." << field_str;

	const std::map<std::string, std::string> data {
		{"major_label", ss.str()}, 
		{"minor_label", val}};

	list.insert_row(data).set_cookie(tcookie3f(index, type, field).u64);
}

void tbase_courseware::insert_list_line_exercise(tlistbox& list, const aplt::tcourseware::texercise& exercise, int index, int field)
{
	const int type = type_exercise;
	std::string val;
	if (field == field_section) {
		val = exercise.section;

	} else if (field == field_question) {
		val = exercise.question;

	} else if (field == field_answer) {
		const bool desensitize = true;
		if (desensitize) {
			val = _("Click to view the answer");
		} else {
			val = exercise.answer;
		}

	} else if (field == field_analysis) {
		val = truncate_for_analysis_label(exercise.analysis);

	} else {
		VALIDATE(false, null_str);
	}

	const std::string field_str = get_field_str(type, field);

	std::stringstream ss;
	ss << "#" << index + 1 << " " << get_field_str(type, field_typeself);
	ss << "." << field_str;

	const std::map<std::string, std::string> data {
		{"major_label", ss.str()}, 
		{"minor_label", val}};

	list.insert_row(data).set_cookie(tcookie3f(index, type, field).u64);
}

} // namespace gui2

