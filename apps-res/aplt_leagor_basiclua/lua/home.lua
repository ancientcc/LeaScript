-- var.
aplt_leagor_basiclua__home = {
	MSG0_LAYER = 0,
	MSG1_LAYER = 1,

	var = {},
};

function aplt_leagor_basiclua__home.construct()
	local var = aplt_leagor_basiclua__home.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;
	return 0;
end

function aplt_leagor_basiclua__home.pre_show(dlg, window)
	local this = aplt_leagor_basiclua__home;
	local var = this.var;
	local aplt = aplt_leagor_basiclua;
	local _ = aplt_leagor_basiclua._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	-- gui2.find_widget(window, "title", false, true):set_label(_("Basic Lua") .. "(v" .. aplt.version .. ")" );
	gui2.find_widget(window, "title", false, true):set_label(_("Basic Lua"));

	local widget = gui2.find_widget(window, "body", false, true);
	var.body_stack_ = widget;

	this.pre_msg0(widget:layer(this.MSG0_LAYER));
	this.pre_msg1(widget:layer(this.MSG1_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("Driver"), icon = "misc/multiselect.png"},
		{label = _("Me"), icon = "misc/me.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.MSG0_LAYER);

end

function aplt_leagor_basiclua__home.post_show()
	local this = aplt_leagor_basiclua__home;
	local var = this.var;

	var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_basiclua__home.pre_msg0(grid)
	local _ = aplt_leagor_basiclua._

	gui2.find_widget(grid, "msg", false, true):set_label(_("base_driver remark"));
end

function aplt_leagor_basiclua__home.pre_msg1(grid)
	local aplt = aplt_leagor_basiclua;
	local _ = aplt._
	gui2.find_widget(grid, "version", false, true):set_label(_("Basic Lua") .. "(v" .. aplt.version .. ")");
end

function aplt_leagor_basiclua__home.did_navigation_changed(report, widget)
	local this = aplt_leagor_basiclua__home;
	local var = this.var;
	
	local current_layer = widget:at();
	var.body_stack_:set_radio_layer(current_layer);
	var.current_layer_ = current_layer;
end

function aplt_leagor_basiclua__home.timer_handler(now)
	local this = aplt_leagor_basiclua__home;
	local var = this.var;
	local _ = aplt_leagor_basiclua._
end