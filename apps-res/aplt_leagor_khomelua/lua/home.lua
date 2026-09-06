-- var.
aplt_leagor_khomelua__home = {
	MSG0_LAYER = 0,
	MSG1_LAYER = 1,

	var = {},
};

function aplt_leagor_khomelua__home.construct()
	local var = aplt_leagor_khomelua__home.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;
	return 0;
end

function aplt_leagor_khomelua__home.pre_show(dlg, window)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local aplt = aplt_leagor_khomelua;
	local _ = aplt_leagor_khomelua._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	-- gui2.find_widget(window, "title", false, true):set_label(_("kHome Lua") .. "(v" .. aplt.version .. ")" );
	gui2.find_widget(window, "title", false, true):set_label(_("kHome Lua") );

	local widget = gui2.find_widget(window, "body", false, true);
	var.body_stack_ = widget;

	this.pre_msg0(widget:layer(this.MSG0_LAYER));
	this.pre_msg1(widget:layer(this.MSG1_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("task^wokrout"), icon = "misc/multiselect.png"},
		{label = _("Me"), icon = "misc/multiselect.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.MSG0_LAYER);

end

function aplt_leagor_khomelua__home.post_show()
	local this = aplt_leagor_khomelua__home;
	local var = this.var;

	var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khomelua__home.pre_msg0(grid)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local aplt = aplt_leagor_khomelua;
	local _ = aplt._

	local widget = gui2.find_widget(grid, "wkofile_list", false, true);
	widget:enable_select(false);
	var.wkofile_list_ = widget;

	local wkoscript_dir = string.format("%s/wkoscript", aplt.res_path);
	local file_count, files = rose.wkoscript_list_files_metadata(wkoscript_dir);
	-- rose.cpp_breakpoint();

	var.wkofiles_ = files;

	this.reload_wkofile_list(var.wkofiles_);
end

function aplt_leagor_khomelua__home.pre_msg1(grid)
	local aplt = aplt_leagor_khomelua;
	local _ = aplt._

	gui2.find_widget(grid, "version", false, true):set_label(_("kHome Lua") .. "(v" .. aplt.version .. ")");
end

function aplt_leagor_khomelua__home.did_navigation_changed(report, widget)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	
	local current_layer = widget:at();
	var.body_stack_:set_radio_layer(current_layer);
	var.current_layer_ = current_layer;
end

function aplt_leagor_khomelua__home.reload_wkofile_list(files)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local list = var.wkofile_list_;

	list:clear();
	local size = #files;
	local has_position_size = 0;
	for at = 0, size - 1 do
		local file = files[at + 1];

		local data = {
			name = file[2] .. '@' .. file[3];
			file = file[1]
			-- open_url = position_uuid;
		};
		local row = list:insert_row(data);

		local widget = gui2.find_widget(row, "open_url", false, true);
		if (#file[4] ~= 0) then
			widget:set_icon("misc/start.png");
			widget:set_did_left_click("click_open_url", {at = at});
		else
			widget:set_visible(gui2k.tvisible_HIDDEN);
		end
	end

	-- var.device_status_:set_label(vgettext2("$count devices    $positions has position", {count = tonumber(size), positions = tonumber(has_position_size)}));
end

function aplt_leagor_khomelua__home.click_open_url(widget, cfg)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;
	local _ = aplt._;

	local at = cfg.at;
	
	local reference = var.wkofiles_[at + 1][4];
	
	assert(#reference ~= 0);
	-- if (#reference == 0) then
	--	gui2.show_message("", _("Need to set the current map, or have at least one position on the map"));
	--	return;
	-- end

	rose.open_url(reference);
end

function aplt_leagor_khomelua__home.timer_handler(now)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt_leagor_khomelua._
end