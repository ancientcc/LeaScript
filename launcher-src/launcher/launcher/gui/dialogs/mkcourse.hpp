#ifndef GUI_DIALOGS_MKCOURSE_HPP
#define GUI_DIALOGS_MKCOURSE_HPP

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "aplt2.hpp"
#include "wkocourse.hpp"
#include "wkoscript.hpp"

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;
class treport;
class ttoggle_button;

class tmkcourse: public tdialog, public tstatusbar
{
public:
	tmkcourse(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_first_drawn() override;
	void app_resize_screen() override;

	std::string target_wkocourse_dir() const;
	std::string candidate_wkoscript_dir() const;

	void init_wkocourse();

	void click_target_aplt(tbutton& widget);
	void reload_candidate_workout_list(tlistbox& list);
	void did_candidate_workout_list_row_changed(tlistbox& list, ttoggle_panel& row);
	void longpress_widget(bool& halt, const tpoint& coordinate, ttoggle_panel& row);
	bool did_drag_mouse_motion(const int x, const int y, gui2::twindow& window, int row_at);
	void did_drag_mouse_leave(const int x, const int y, bool up_result, int row_at);

	void reload_calendar_report(int sel_at);
	void update_calendar_day(int day_at);
	void did_calendar_changed(treport& report, ttoggle_button& row);

	void reload_day_workout_list(tlistbox& list);
	void did_day_workout_list_row_changed(tlistbox& list, ttoggle_panel& row);
	void click_rounds(ttoggle_panel& row, tbutton& widget);
	void click_down_day_workout(tlistbox& list, ttoggle_panel& row);
	void click_erase_day_workout(tlistbox& list, ttoggle_panel& row);

	void update_title_label();
	void update_day_title_label(const std::string& msg);
	void update_status_label(bool valid);
	bool wkocourse_dirty() const;
	void refresh_toolbar_ui();
	void draw_flowchart_from_course(bool is_initial, const std::string& filename, const aplt::twkocourse& course);
	void new_empty_flowchart(bool is_initial);
	void course_clear_and_set_valid_id();
	void draw_flowchart();
	void click_file(tbutton& widget);
	bool confirm_file_op(int sel);
	void open_cfg_file_bh(const std::string& filename);
	void handle_file_op(int sel);
	void handle_file_menu(int sel);
	bool handle_pre_save();
	void click_save();
	SDL_Size do_save_image(bool todo, std::map<std::string, aplt::twkoscript>& scripts, surface* p_surf) const;
	void click_share();

	void update_edittype_course_ui(int type);

	enum {btntype_total_days, btntype_grace_period_days, btntype_count};
	void update_btntype_course_ui(int type);

	void list_wkocourse_files2(int type, std::set<std::string>& result_set);
	enum {edittype_course_id, edittype_course_title, edittype_course_author, edittype_course_desc, edittype_course_reference,
		edittype_day_title, edittype_workout_note};
	bool did_verify_text_changed(const std::string& label, const std::string& initial, int type, const std::set<std::string>& xcludes) const;
	void handle_edit_text(int type, int row_at);
	void click_btn_type(int type, tbutton& widget);

	void app_timer_handler(uint32_t now) override;

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	enum {file_new_from_benchmark, file_new_empty, file_open, file_save_as, file_exit};
	const std::map<int, std::string> file_ops_;
	const std::vector<tcode3> edit_widget_ids_;
	const std::vector<tcode3> btn_widget_ids_;
	const std::string day_title_msgstr_;
	const std::string workout_note_msgstr_;
	const int max_show_days_non_maximized_;
	aplt::tapplet* target_aplt_;
	std::string wkocourse_dir_;

	tlabel* title_widget_;
	tbutton* save_widget_;
	tlistbox* candidate_workout_list_;
	treport* calendar_;
	tbutton* day_title_widget_;
	tlistbox* day_workout_list_;
	tlabel* status_widget_;

	std::vector<aplt::twkoscript> candidate_scripts_;

	std::string filename_;
	aplt::twkocourse course_;
	aplt::twkocourse tmp_course_;
	int day_at_;

	class tclear_drag_candidate_lock
	{
	public:
		tclear_drag_candidate_lock(tmkcourse& mkcourse)
			: mkcourse_(mkcourse)
		{}

		~tclear_drag_candidate_lock();

	private:
		tmkcourse& mkcourse_;
	};
};

} // namespace gui2

#endif

