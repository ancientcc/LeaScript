-- var.
aplt_leagor_basic__simple = {
	var = {},
};

function aplt_leagor_basic__simple.construct()
	local var = aplt_leagor_basic__simple.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	return 0;
end

function aplt_leagor_basic__simple.pre_show(dlg, window)
	local this = aplt_leagor_basic__simple;
	local var = this.var;
	local aplt = aplt_leagor_basic;
	local _ = aplt_leagor_basic._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/white_background.png");

	local widget = gui2.find_widget(window, "back", false, true);
	widget:set_did_left_click("click_back");

end

function aplt_leagor_basic__simple.post_show()
	aplt_leagor_basic__simple.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_basic__simple.click_back()
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__simple;
	local var = this.var;

	-- var.window_:set_retval(aplt.DLG_HOME);
end

function aplt_leagor_basic__simple.timer_handler(now)
	local this = aplt_leagor_basic__simple;
	local var = this.var;
	local _ = aplt_leagor_basic._
end