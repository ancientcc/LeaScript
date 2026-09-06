#ifndef GUI_DIALOGS_BASE_COURSEWARE_HPP_INCLUDED
#define GUI_DIALOGS_BASE_COURSEWARE_HPP_INCLUDED

#include "aplt2.hpp"
#include "game_config.hpp"
#include "aplt_net.hpp"


namespace aplt {

class tcourseware
{
public:
	enum {type_nposm = nposm, type_exam = 0};

	struct tkeypoint
	{
		tkeypoint(const std::string& section, const std::string& name, const std::string& annotation, const std::string& analysis)
			: section(section)
			, name(name)
			, annotation(annotation)
			, analysis(analysis)
		{}

		bool operator==(const tkeypoint& that) const
		{
			if (section != that.section || name != that.name || annotation != that.annotation || analysis != that.analysis) {
				return false;
			}

			return true;
		}
		bool operator!=(const tkeypoint& that) const { return !operator==(that); }

		std::string section;
		std::string name;
		std::string annotation;
		std::string analysis;
	};

	struct texercise
	{
		texercise(const std::string& section, const std::string& question, const std::string& analysis, const std::string& answer)
			: section(section)
			, question(question)
			, analysis(analysis)
			, answer(answer)
		{}

		bool operator==(const texercise& that) const
		{
			if (section != that.section || question != that.question || analysis != that.analysis || answer != that.answer) {
				return false;
			}

			return true;
		}
		bool operator!=(const texercise& that) const { return !operator==(that); }

		std::string section;
		std::string question;
		std::string analysis;
		std::string answer;
	};

	tcourseware();

	static const tcode3& type_from_str(const std::string& str);

	bool from_cfg(const config& cfg);
	bool from_keypoint_cfg(const config& cfg);
	bool from_exercise_cfg(const config& cfg);
	void to_cfg(config& cfg) const;
	bool equal(const tcourseware& that) const;

	// because member 'pinyin_', cannot use b = a. 
	// void assign(const aplt::ttask_cpp_pair& that);

	bool valid() const { return !title.empty() && !content.empty(); }

	void clear()
	{
		// uuid.clear();
		type = nposm;
		tex_header.clear();
		tex_tail.clear();
		title.clear();
		content.clear();
		annotation.clear();
		analysis.clear();
		reference.clear();

		keypoints.clear();
		exercises.clear();
	}

	void keypoint_swap(int at1, int at2);
	void exercise_swap(int at1, int at2);

	std::string text_for_listen() const;

public:
	static std::map<int, tcode3> types;

	std::string uuid;
	int type;
	std::string tex_header;
	std::string title;
	std::string content;
	std::string annotation;
	std::string analysis;
	std::string reference;

	std::vector<tkeypoint> keypoints;
	std::vector<texercise> exercises;

	std::string tex_tail;
};

}


namespace gui2 {

class tlistbox;

class tbase_courseware
{
public:
	struct tcourseware_file
	{
	public:
		tcourseware_file()
			: download(false)
			, uid(uid_nposm)
			, local_time(0)
			, remote_time1(0)
		{}

		tcourseware_file(bool download, const std::string& dir_name, const std::string& username, const std::string& uuid, int64_t local_time, int64_t remote_time1)
			: download(download)
			, dir_name(dir_name)
			, uid(uid_nposm)
			, username(username)
			, uuid(uuid)
			, title(dir_name)
			, local_time(local_time)
			, remote_time1(remote_time1)
		{
			if (download) {
				std::pair<std::string, std::string> pair = utils::split_app_prefix_id(dir_name);
				uid = utils::to_int64(pair.first);
				title = pair.second;

			} else {
				uid = COURSEWARE_UPLOAD_UID;
			}
		}

	public:
		bool download;
		std::string dir_name;

		int64_t uid;
		std::string username;
		std::string uuid;
		std::string title;
		int64_t local_time;
		int64_t remote_time1;
	};

	tbase_courseware(const std::string& saves_courseware_dir);

protected:
	virtual std::string courseware_load_path() const { return download_path_; }
	virtual bool use_map_courseware_files() const { return true; }

	const tcourseware_file& courseware_file_from_at(int at) const;
	tcourseware_file& mutable_courseware_file_from_at(int at);

	std::string join_courseware_dir2(const std::string& root_path, const std::string& dir_name) const;
	std::string join_courseware_dir(const std::string& dir_name) const { return join_courseware_dir2(courseware_load_path(), dir_name); }
	std::string join_courseware_dir_courselist(int64_t uid, const std::string& dir_name) const
	{
		return join_courseware_dir2(uid == COURSEWARE_UPLOAD_UID? upload_path_: download_path_, dir_name); 
	}

	std::string join_main_cfg_filename2(const std::string& root_path, const std::string& dir_name) const;
	std::string join_main_cfg_filename(const std::string& dir_name) const { return join_main_cfg_filename2(courseware_load_path(), dir_name); }

	std::string join_main_cfg_filename_couselist(int64_t uid, const std::string& dir_name) const 
	{ 
		return join_main_cfg_filename2(uid == COURSEWARE_UPLOAD_UID? upload_path_: download_path_, dir_name); 
	}

	std::string join_distribution_cfg_filename(const std::string& dir_name) const;

	bool did_walk_courseware(const std::string& dir, const SDL_dirent2* dirent, bool download, std::map<std::string, tcourseware_file>& files, const std::string& root);

	void collect_courseware(bool download, const std::string& dir, std::map<std::string, tcourseware_file>& files);

	void load_courseware_cfg2(const std::string& cfgfile, aplt::tcourseware& result);

	struct tcourseware_distribution_vals
	{
		tcourseware_distribution_vals(int64_t uid, const std::string& username, const std::string& uuid, int64_t ts)
			: uid(uid)
			, username(username)
			, uuid(uuid)
			, ts(ts)
		{}

		bool valid() const { return uid > 0 && !username.empty(); }

		int64_t uid;
		std::string username;
		std::string uuid;
		int64_t ts;
	};
	tcourseware_distribution_vals get_distribution_vals(const std::string& courseware_dir);

	enum {type_global, type_keypoint, type_exercise};
	enum {field_typeself, field_uuid, field_type, field_tex_header, field_tex_tail, field_title, field_content, field_name, field_annotation, field_analysis, field_reference, 
		field_section, field_question, field_answer,
	};
	std::string get_field_str(int type, int field) const;
	std::string get_placeholder_msg(int type, int field) const;
	std::string get_error_msg(const aplt::tcourseware& courseware, int type, int field) const;
	std::string get_node_label_global(const aplt::tcourseware& courseware, int field) const;
	std::string get_node_label_keypoint(const aplt::tcourseware::tkeypoint& keypoint, int index, int field) const;
	std::string get_node_label_exercise(const aplt::tcourseware::texercise& exercise, int index, int field) const;

	void insert_list_line_global(tlistbox& list, const aplt::tcourseware& courseware, int field);
	void insert_list_line_keypoint(tlistbox& list, const aplt::tcourseware::tkeypoint& keypoint, int index, int field);
	void insert_list_line_exercise(tlistbox& list, const aplt::tcourseware::texercise& exercise, int index, int field);

protected:
	const std::string upload_path_;
	const std::string download_path_;

	const std::string msgstr_notempty_and_utf8str_;
	const std::string msgstr_empty_or_utf8str_;
	const tcookie3f cookie3f_nposm_;
	const int global_atts_;

	// used for UPLOAD_LAYER and DOWNLAOD_LAYER
	std::map<std::string, tcourseware_file> courseware_files_;
	// used for COURSELIST_LAYER
	std::vector<tcourseware_file> courseware_files2_;
};

} // namespace gui2

#endif

