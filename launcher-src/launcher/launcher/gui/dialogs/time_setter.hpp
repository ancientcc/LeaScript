#ifndef GUI_DIALOGS_RTIME_SETTER_HPP_INCLUDED
#define GUI_DIALOGS_RTIME_SETTER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "aplt.hpp"

namespace gui2 {

class tlabel;
class tbutton;

class ttime_setter: public tdialog
{
public:
	explicit ttime_setter(const aplt::tapplet& applet, aplt::tbg_task& timing, aplt::taplt_task& task);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {type_hour, type_minute, type_second};
	void click_clock(tbutton& widget, int type);
	void click_state(tbutton& widget);
	void did_task_zerotz_t_changed();

	void app_timer_handler(uint32_t now) override;

private:
	const aplt::tapplet& applet_;
	aplt::tbg_task& timing_;
	aplt::taplt_task& task_;
	tlabel* current_time_widget_;
	tbutton* state_widget_;
	tbutton* ok_widget_;
	std::set<int> others_zerotz_t_;

	enum {strategy_original, strategy_fresh, strategy_finished, strategy_count};
	std::map<int, std::string> strategies_;
	int state_strategy_;
};

} // namespace gui2

#endif

