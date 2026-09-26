-- var.
aplt_leagor_khomelua__home = {
	COURSE_LAYER = 0,
	WORKOUT_LAYER = 1,
	ME_LAYER = 2,

	seg_interval = 10,
	course_reference = nil,

	var = {},
};

function aplt_leagor_khomelua__home.construct()
	local var = aplt_leagor_khomelua__home.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;
	var.curr_course_at = npoms;
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

	this.pre_course(widget:layer(this.COURSE_LAYER));
	this.pre_workout(widget:layer(this.WORKOUT_LAYER));
	this.pre_me(widget:layer(this.ME_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("task^course"), icon = "misc/course.png"},
		{label = _("task^workout"), icon = "misc/workout.png"},
		{label = _("Me"), icon = "misc/me.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.COURSE_LAYER);

end

function aplt_leagor_khomelua__home.post_show()
	local this = aplt_leagor_khomelua__home;
	local var = this.var;

	var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khomelua__home.pre_course(grid)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local aplt = aplt_leagor_khomelua;
	local _ = aplt._;

	local widget = gui2.find_widget(grid, "purchase_grid", false, true);
	var.purchase_grid_widget_ = widget;

	widget = gui2.find_widget(grid, "currency", false, true);
	var.currency_widget_ = widget;

	widget = gui2.find_widget(grid, "price", false, true);
	var.price_widget_ = widget;

	widget = gui2.find_widget(grid, "purchase", false, true);
	widget:set_did_left_click("click_course_purchase");
	widget:set_border("blue_ellipse");
	var.purchase_widget_ = widget;

	widget = gui2.find_widget(grid, "enroll", false, true);
	var.course_enroll_widget_ = widget;

	widget = gui2.find_widget(grid, "description", false, true);
	var.course_description_widget_ = widget;

	widget = gui2.find_widget(grid, "course_reference", false, true);
	widget:set_icon("misc/start.png");
	widget:set_did_left_click("click_open_url", {reference = nil});
	var.course_reference_widget_ = widget;

	var.wkocourse_enrolls_ = map.new();

	this.reload_wkocourse_enrolls_metadata();

	widget = gui2.find_widget(grid, "day_list", false, true);
	widget:enable_select(false);
	var.day_list_ = widget;

	local wkocourse_dir = string.format("%s/wkocourse", aplt.res_path);
	local file_count, files = rose.wkocourse_list_files_metadata(aplt.id, wkocourse_dir);

	-- rose.cpp_breakpoint();

	widget = gui2.find_widget(grid, "day_segment_report", false, true);
	widget:set_did_item_changed("did_day_segment_report_changed");
	var.day_segment_report_ = widget;

	var.wkocourses_ = {};
	widget = gui2.find_widget(grid, "course_report", false, true);
	local size = #files;
	for at = 0, size - 1 do
		local file = files[at + 1];
		table.insert(var.wkocourses_, file);

		local item = widget:insert_item("", #file.id > 0 and file.title or file.file);
	end

	-- rose.cpp_breakpoint();

	widget:set_did_item_changed("did_course_report_changed");
	if (file_count ~= 0) then
		widget:select_item(0);
	end
end

function aplt_leagor_khomelua__home.pre_workout(grid)
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

	var.wkoscripts_ = files;

	this.reload_wkoscript_list(var.wkoscripts_);
end

function aplt_leagor_khomelua__home.pre_me(grid)
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

function aplt_leagor_khomelua__home.reload_wkocourse_enrolls_metadata()
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local aplt = aplt_leagor_khomelua;
	local _ = aplt._;

	local enroll_count, enrolls = rose.wkocourse_enrolls_metadata();
	var.wkocourse_enrolls_:clear();

	-- rose.cpp_breakpoint();
	for at = 0, enroll_count - 1 do
		local enroll = enrolls[at + 1];
		var.wkocourse_enrolls_:insert(enroll["id2"], enroll);
	end
end

function aplt_leagor_khomelua__home.click_course_purchase(widget)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt._

	local wkocourse = var.wkocourses_[var.curr_course_at + 1];
	local id2 = rose.convert_string(aplt.bundleid, rosek.STRCVT_JOIN_APP_PREFIX_ID, wkocourse["id"])
	
	local msg = _("This course is free. Do you want to purchase it?");
	local retval = gui2.show_message(nil, msg, gui2k.tmessage_yes_no_buttons);
	if (not retval) then
		return;
	end

	-- _("This course is free. Do you want to purchase it?")

	local enroll = var.wkocourse_enrolls_:find(id2);
	assert(not enroll);

	local retbool = rose.wkocourse_purchase(aplt.bundleid, wkocourse["id"]);
	if (retbool) then
		this.reload_wkocourse_enrolls_metadata();
		this.update_course_ui();
	end
end

function aplt_leagor_khomelua__home.update_course_ui()
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;

	local wkocourse = var.wkocourses_[var.curr_course_at + 1];
	local wkocourse_is_valid = #wkocourse.id > 0;
	local id2 = rose.convert_string(aplt.bundleid, rosek.STRCVT_JOIN_APP_PREFIX_ID, wkocourse["id"])
	
	local enroll = var.wkocourse_enrolls_:find(id2);

	local enroll_msg = "";
	local allow_purchase = wkocourse_is_valid and (not enroll);
	var.purchase_grid_widget_:set_visible(allow_purchase and gui2k.tvisible_VISIBLE or gui2k.tvisible_INVISIBLE)

	if (not wkocourse_is_valid) then
		enroll_msg = _("Not a valid course script.");

	elseif (enroll) then
		local purchase_str = rose.integer_tostr(enroll["purchase"], rosek.FMT_TIME_ymdhms);
		if (enroll["active"] ~= nposm) then
			local active_str = rose.integer_tostr(enroll["active"], rosek.FMT_TIME_ymdhms);
			local expire_ts = rose.wkocourse_enroll_calc_int64(enroll["id2"], rosek.enrollcalctype_expire, wkocourse["total_days"]);
			local expire_str = rose.integer_tostr(expire_ts, rosek.FMT_TIME_ymdhms);
			if (os.time() <= expire_ts) then
				enroll_msg = vgettext2("Expires on $expire. Purchase time: $purchase, Active time: $active.", 
					{expire = expire_str,
					purchase = purchase_str,
					active = active_str});
			else
				enroll_msg = vgettext2("Expired (expiration time: $expire). Purchase time: $purchase, Active time: $active.", 
					{expire = expire_str,
					purchase = purchase_str,
					active = active_str});
				-- is_expired = true;
			end

		else
			local force_active_ts = rose.wkocourse_enroll_calc_int64(enroll["id2"], rosek.enrollcalctype_force_active, wkocourse["grace_period_days"]);
			enroll_msg = vgettext2("Not activated. $purchase, $force_active, $first_day",
				{purchase = purchase_str,
				force_active = rose.integer_tostr(force_active_ts, rosek.FMT_TIME_ymdhms),
				first_day = rose.integer_tostr(force_active_ts + 1, rosek.FMT_TIME_ymdhms)});

			--[[
			int64_t force_acive_ts = enroll.calc_force_active(wkocourse->grace_period_days);
			symbols["force_active"] = utils::format_time_ymdhms(force_acive_ts);
			symbols["first_day"] = utils::format_time_ymd3(force_acive_ts + 1);
			enroll_msg = vgettext2("Not activated. $purchase, $force_active, $first_day", symbols);
			--]]
		end
	else
		local price_label = nil;
		if (wkocourse.price ~= 0) then
			price_msg = string.format("%.2f", wkocourse.price / 100)
		else
			price_msg = _("wkocourse^Free");
		end
		var.price_widget_:set_label(price_msg);
		var.currency_widget_:set_visible(wkocourse.price ~= 0 and gui2k.tvisible_VISIBLE or gui2k.tvisible_INVISIBLE);
	end

	local is_valid_reference = #wkocourse["reference"] ~= 0;
	this.course_reference = is_valid_reference and wkocourse["reference"] or nil;
	var.course_reference_widget_:set_visible(is_valid_reference and gui2k.tvisible_VISIBLE or gui2k.tvisible_INVISIBLE);

	var.course_enroll_widget_:set_label(enroll_msg);
	var.course_description_widget_:set_label(wkocourse["description"]);

	-- rose.cpp_breakpoint();

	-- day_segment_report
	var.day_segment_report_:clear();

	local n = math.ceil(wkocourse["total_days"] / this.seg_interval); -- 10

	for at = 0, n - 1 do
		-- local start_day = at == 0 and 1 or this.seg_interval * at;
		local start_day = this.seg_interval * at + 1;
		local label = start_day .. " - " .. (at == n - 1 and wkocourse["total_days"] or this.seg_interval * (at + 1));
		var.day_segment_report_:insert_item("", label);
	end
	if (var.day_segment_report_:items() > 0) then
		var.day_segment_report_:select_item(0);
	else
		var.day_list_:clear();
	end
end

function aplt_leagor_khomelua__home.did_course_report_changed(report, widget)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;
	
	local current_layer = widget:at();
	var.curr_course_at = current_layer;

	this.update_course_ui();
end

function aplt_leagor_khomelua__home.did_day_segment_report_changed(report, widget)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;
	
	var.curr_coruse_day_at = widget:at();

	this.reload_day_list();
end

function aplt_leagor_khomelua__home.reload_day_list(files)
	local aplt = aplt_leagor_khomelua;
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local wkocourse = var.wkocourses_[var.curr_course_at + 1];
	local days = var.wkocourses_[var.curr_course_at + 1].days;
	local list = var.day_list_;

	local day_start_at = var.curr_coruse_day_at * this.seg_interval;
	local day_end = (var.curr_coruse_day_at + 1) * this.seg_interval;
	if (wkocourse["total_days"] < day_end) then
		day_end = wkocourse["total_days"];
	end

	-- rose.cpp_breakpoint();

	list:clear();
	local has_position_size = 0;
	for at = day_start_at, day_end - 1 do
		local day = days[at + 1];

		local data = {
			day = "Day " .. at + 1;
			day_title = day["title"]
		};
		local row = list:insert_row(data);

		local widget = gui2.find_widget(row, "workout_grid", false, true);
		widget:set_visible(gui2k.tvisible_INVISIBLE);

		local workout_count = #day.workouts;
		for at2 = 0, workout_count - 1 do
			local workout = day.workouts[at2 + 1];
			local data = {
				name = "(" .. workout.rounds .. ")" .. workout.title;
				note = workout.note;
				file = workout.id .. ".cfg";
			};
			local row = list:insert_row(data);

			widget = gui2.find_widget(row, "day_grid", false, true);
			widget:set_visible(gui2k.tvisible_INVISIBLE);

			widget = gui2.find_widget(row, "open_url", false, true);
			if (#workout.reference ~= 0) then
				widget:set_icon("misc/start.png");
				widget:set_did_left_click("click_open_url", {reference = workout.reference});
			else
				widget:set_visible(gui2k.tvisible_HIDDEN);
			end
		end
	end

	-- var.device_status_:set_label(vgettext2("$count devices    $positions has position", {count = tonumber(size), positions = tonumber(has_position_size)}));
end

function aplt_leagor_khomelua__home.reload_wkoscript_list(files)
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
			name = file["title"] .. '@' .. file["author"];
			file = file["file"]
			-- open_url = position_uuid;
		};
		local row = list:insert_row(data);

		local widget = gui2.find_widget(row, "open_url", false, true);
		if (#file["reference"] ~= 0) then
			widget:set_icon("misc/start.png");

			widget:set_did_left_click("click_open_url", {reference = file["reference"]});
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

	local reference = cfg.reference;
	if (not reference) then
		-- course reference
		assert(#this.course_reference ~= 0);
		reference = this.course_reference;
	end

	rose.open_url(reference);
end

function aplt_leagor_khomelua__home.timer_handler(now)
	local this = aplt_leagor_khomelua__home;
	local var = this.var;
	local _ = aplt_leagor_khomelua._
end