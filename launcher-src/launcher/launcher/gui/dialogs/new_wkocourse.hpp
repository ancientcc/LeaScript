#ifndef GUI_DIALOGS_NEW_WKOCOURSE_HPP
#define GUI_DIALOGS_NEW_WKOCOURSE_HPP

#include "gui/dialogs/statusbar.hpp"
#include "wkoscript.hpp"
#include "wkocourse.hpp"

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_button;

class tnew_wkocourse: public tdialog, public tstatusbar
{
public:
	tnew_wkocourse(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const std::string& csv_file,
		std::vector<aplt::twkoscript>& scripts, aplt::twkocourse& course, const aplt::tapplet& aplt);

	std::string course_filename() const
	{
		const std::string wkocourse_dir = aplt_.wkocourse_dir(false);
		return course_.build_course_filename(wkocourse_dir);
	}

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {flag_new, flag_same, flag_vermismatch, flag_diff, flag_count};
	bool is_flag_require_write(int flag) const
	{
		VALIDATE(flag >= 0 && flag < flag_count && flag != flag_diff, null_str);
		return flag == flag_new || flag == flag_vermismatch;
	}

	void reload_legend_grid();

	bool can_next() const;
	void click_back(tbutton& widget);
	void click_working_dir(tbutton& widget);
	void click_new_dirs(tbutton& widget);
	int get_wkoscript_file_flag(const std::string& wkoscript_dir, const aplt::twkoscript& script) const;
	int get_wkocourse_file_flag(const std::string& wkocourse_dir, const aplt::twkocourse& course) const;
	std::string method_str(const std::string& str, int flag_code) const;
	void reload_action_list(tlistbox& list);

	void app_timer_handler(uint32_t now) override;

private:
	const std::string csv_file_;
	const std::vector<aplt::twkoscript>& scripts_;
	const aplt::twkocourse& course_;
	const aplt::tapplet& aplt_;

	struct tflag4
	{
		tflag4(int code, const std::string& png, const std::string& msgstr, const std::string& method)
			: code(code)
			, png(png)
			, msgstr(msgstr)
			, method(method)
		{}

		bool operator<(const tcode3& that) const { return code < that.code; }

		int code;
		std::string png;
		std::string msgstr;
		std::string method;
	};
	const std::map<int, tflag4> flags_;

	tbutton* back_widget_;
	// tlistbox* legend_list_;
	tlistbox* file_list_;

	std::vector<int> script_flags_;
	int course_flag_;
};

} // namespace gui2

#endif

