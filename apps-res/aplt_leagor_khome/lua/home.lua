-- var.
aplt_leagor_khome__home = {
	var = {},
};

function aplt_leagor_khome__home.construct()
	local var = aplt_leagor_khome__home.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	return 0;
end

function aplt_leagor_khome__home.pre_show(dlg, window)
	local this = aplt_leagor_khome__home;
	local var = this.var;
	local aplt = aplt_leagor_khome;
	local _ = aplt_leagor_khome._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	gui2.find_widget(window, "title", false, true):set_label(_("kHome") .. "(v" .. aplt.version .. ")" );

	local widget = gui2.find_widget(window, "query", false, true);

	widget:set_icon("misc/query.png");
	widget:set_did_left_click("click_query");
	if (aplt.cpp_block2_ == nil) then
		widget:set_visible(gui2k.tvisible_INVISIBLE);
	end

	widget = gui2.find_widget(window, "restaurant", false, true);
	widget:set_icon("misc/restaurant.png");
	widget:set_did_left_click("click_restaurant");
	if (aplt.cpp_block2_ == nil) then
		widget:set_visible(gui2k.tvisible_INVISIBLE);
	end

	widget = gui2.find_widget(window, "posture", false, true);
	widget:set_icon("misc/posture.png");
	widget:set_did_left_click("click_icon", {retval = aplt.DLG_POSTURE});

	widget = gui2.find_widget(window, "face", false, true);
	widget:set_icon("misc/face.png");
	widget:set_did_left_click("click_icon", {retval = aplt.DLG_FACE});
	if (aplt.cpp_camera2_ == nil) then
		widget:set_visible(gui2k.tvisible_INVISIBLE);
	end

	widget = gui2.find_widget(window, "remark", false, true);
	widget:set_label(_("home^remark"));
end

function aplt_leagor_khome__home.post_show()
	aplt_leagor_khome__home.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khome__home.click_query()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__home;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_QUERY);
end

function aplt_leagor_khome__home.click_restaurant()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__home;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_RESTAURANT);
end

function aplt_leagor_khome__home.click_icon(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__home;
	local var = this.var;

	var.window_:set_retval(cfg.retval);
end

function aplt_leagor_khome__home.timer_handler(now)
	local this = aplt_leagor_khome__home;
	local var = this.var;
	local _ = aplt_leagor_khome._
end