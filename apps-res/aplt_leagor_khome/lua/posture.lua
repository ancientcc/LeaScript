-- var.
aplt_leagor_khome__posture = {
	README_LAYER = 0,
	SIT_FRONT_LAYER = 1,
	SIT_SIDE_LAYER = 2,
	SIT_SHARE_LAYER = 3,
	SHOULDER_NECK_LAYER = 4,

	-- POS_NAME_AT = 2,
	POS_UUID_LUA_IDX = 2,

	max_disp_size = 200,
	show_image = false,

	RATIO_MIN = 0.05;
	RATIO_MAX = 0.95;

	TIME_MIN = 5;
	TIME_MAX = 1000;

	ANGLE_MIN = 10;
	ANGLE_MAX = 350;

	ftype_ratio = 1;
	ftype_time = 2;
	ftype_angle = 3;

	var = {},
};

function aplt_leagor_khome__posture.construct()
	local this = aplt_leagor_khome__posture;
	local var = this.var;

	this.field_types = {
		-- sf <=> Sit Front
		sf_shoulder_width_min = this.ftype_ratio,
		sf_shoulder_width_max = this.ftype_ratio,
		sf_ear_width_min = this.ftype_ratio,
		sf_ear_diff_threshold = this.ftype_ratio,
		sf_head_forward_threshold = this.ftype_ratio,

		ss_spine_length_min = this.ftype_ratio,
		ss_spine_length_max = this.ftype_ratio,
		ss_shoulder_width_max = this.ftype_ratio,
		ss_shoulder_nose_angle_threshold = this.ftype_angle,
		ss_improper_spine_angle_min = this.ftype_angle,
		ss_improper_spine_angle_max = this.ftype_angle,

		sshare_improper_time = this.ftype_time,
		sshare_noperson_time = this.ftype_time,
		sshare_sedentary_time = this.ftype_time,
	};

	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;
	return 0;
end

function aplt_leagor_khome__posture.pre_show(dlg, window)
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local aplt = aplt_leagor_khome;
	local _ = aplt_leagor_khome._;

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	local widget = gui2.find_widget(window, "title", false, true):set_label(_("Posture"));

	widget = gui2.find_widget(window, "back", false, true);
	widget:set_did_left_click("click_back");

	widget = gui2.find_widget(window, "status", false, true);
	var.status_ = widget;

	local shoulder_width_min, shoulder_width_max, ear_width_min, ear_diff_threshold, head_forward_threshold,
		improper_time, noperson_time, sedentary_time,
		ss_spine_length_min, ss_spine_length_max, ss_shoulder_nose_angle_threshold, ss_improper_spine_angle_min, ss_improper_spine_angle_max, ss_shoulder_width_max = aplt.preferences:_value(
		"sf_shoulder_width_min", "sf_shoulder_width_max", "sf_ear_width_min", "sf_ear_diff_threshold", "sf_head_forward_threshold",
		"sshare_improper_time", "sshare_noperson_time", "sshare_sedentary_time",
		"ss_spine_length_min", "ss_spine_length_max", "ss_shoulder_nose_angle_threshold", "ss_improper_spine_angle_min", "ss_improper_spine_angle_max", "ss_shoulder_width_max");

	-- if (not rose.is_format(alias_id, rosek.FMT_APLT_IOT_ALIAS_ID)) then
	--	alias_id = aplt.def_alias_id;
	--	aplt.preferences:_set_value("alias_id", alias_id);
	-- end

	widget = gui2.find_widget(window, "list_stack", false, true);
	var.list_stack_ = widget;

	this.pre_readme(widget:layer(this.README_LAYER));
	this.pre_sit_front(widget:layer(this.SIT_FRONT_LAYER), shoulder_width_min, shoulder_width_max, ear_width_min, ear_diff_threshold, head_forward_threshold);
	this.pre_sit_side(widget:layer(this.SIT_SIDE_LAYER), ss_spine_length_min, ss_spine_length_max, ss_shoulder_nose_angle_threshold, ss_improper_spine_angle_min, ss_improper_spine_angle_max, ss_shoulder_width_max);
	this.pre_sit_share(widget:layer(this.SIT_SHARE_LAYER), improper_time, noperson_time, sedentary_time);
	this.pre_shoulder_neck_layer(widget:layer(this.SHOULDER_NECK_LAYER));

	widget = gui2.find_widget(window, "tag_report", false, true);
	var.tag_report_ = widget;

	local item = widget:insert_item("", _("Readme"));
	-- item:set_icon("misc/log.png");

	item = widget:insert_item("", _("Sit(front)"));
	-- item:set_icon("misc/log.png");

	item = widget:insert_item("", _("Sit(rside)"));
	-- item:set_icon("misc/log.png");

	item = widget:insert_item("", _("Sit(share)"));
	-- item:set_icon("misc/log.png");

	item = widget:insert_item("", _("Shoulder neck"));
	-- item:set_icon("misc/log.png");

	widget:set_did_item_pre_change("did_navigation_pre_change");
	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.README_LAYER);

	-- var.tag_report_:set_visible(gui2k.tvisible_INVISIBLE);

end

function aplt_leagor_khome__posture.post_show()
	local this = aplt_leagor_khome__posture;
	local var = this.var;

	this.save_pref_fields(var.current_layer_);
	var = {};
end

function aplt_leagor_khome__posture.pre_readme(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;

	local widget = gui2.find_widget(grid, "sit_remark", false, true);
	local sit_front = _("Sit(front)");
	local sit_rside = _("Sit(rside)");
	local sit_share = _("Sit(share)");
	local label = vgettext2("readme's sit remark, $sit_front, $sit_rside, $sit_share",
		{sit_front = sit_front, sit_rside = sit_rside, sit_share = sit_share});
	widget:set_label(label);
end

function aplt_leagor_khome__posture.pre_sit_front(grid, shoulder_width_min, shoulder_width_max, ear_width_min, ear_diff_threshold, head_forward_threshold)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._;

	local widget = gui2.find_widget(grid, "remark", false, true);
	var.sit_front_remark_ = widget;
	widget:set_label(_("sit_front remark"));

	-- report_improper = 1
	widget = gui2.find_widget(grid, "shoulder_width_max", false, true);
	widget:set_placeholder("0.42");
	widget:set_label(string.format("%.3f", shoulder_width_max));
	widget:set_did_text_changed("did_text_box_changed", {key = "sf_shoulder_width_max"});

	widget = gui2.find_widget(grid, "ear_diff_threshold", false, true);
	widget:set_placeholder("0.05");
	widget:set_label(string.format("%.3f", ear_diff_threshold));
	widget:set_did_text_changed("did_text_box_changed", {key = "sf_ear_diff_threshold"});

	widget = gui2.find_widget(grid, "head_forward_threshold", false, true);
	widget:set_placeholder("0.25");
	widget:set_label(string.format("%.3f", head_forward_threshold));
	widget:set_did_text_changed("did_text_box_changed", {key = "sf_head_forward_threshold"});

	widget = gui2.find_widget(grid, "improper_summary", false, true);
	widget:set_label(this.sf_improper_summary_label(shoulder_width_max, ear_diff_threshold, head_forward_threshold));

	-- report_noperson = 2
	widget = gui2.find_widget(grid, "ear_width_min", false, true);
	widget:set_placeholder("0.10");
	widget:set_label(string.format("%.3f", ear_width_min));
	widget:set_did_text_changed("did_text_box_changed", {key = "sf_ear_width_min"});

	widget = gui2.find_widget(grid, "shoulder_width_min", false, true);
	widget:set_placeholder("0.25");
	widget:set_label(string.format("%.3f", shoulder_width_min));
	widget:set_did_text_changed("did_text_box_changed", {key = "sf_shoulder_width_min"});

	widget = gui2.find_widget(grid, "noperson_summary", false, true);
	widget:set_label(this.sf_noperson_summary_label(ear_width_min, shoulder_width_min));
end

function aplt_leagor_khome__posture.pre_sit_side(grid, ss_spine_length_min, ss_spine_length_max, ss_shoulder_nose_angle_threshold, ss_improper_spine_angle_min, ss_improper_spine_angle_max, ss_shoulder_width_max)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._

	local widget = gui2.find_widget(grid, "remark", false, true);
	var.sit_front_remark_ = widget;
	widget:set_label(_("sit_side remark"));

	-- report_improper = 1
	widget = gui2.find_widget(grid, "improper_spine_angle_min", false, true);
	widget:set_placeholder("268");
	widget:set_label(tostring(ss_improper_spine_angle_min));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_improper_spine_angle_min"});

	widget = gui2.find_widget(grid, "improper_spine_angle_max", false, true);
	widget:set_placeholder("280");
	widget:set_label(tostring(ss_improper_spine_angle_max));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_improper_spine_angle_max"});

	widget = gui2.find_widget(grid, "shoulder_nose_angle_threshold", false, true);
	widget:set_placeholder("45");
	widget:set_label(tostring(ss_shoulder_nose_angle_threshold));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_shoulder_nose_angle_threshold"});

	widget = gui2.find_widget(grid, "shoulder_width_max", false, true);
	widget:set_placeholder("0.13");
	widget:set_label(tostring(ss_shoulder_width_max));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_shoulder_width_max"});

	widget = gui2.find_widget(grid, "spine_length_max", false, true);
	widget:set_placeholder("0.60");
	widget:set_label(string.format("%.3f", ss_spine_length_max));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_spine_length_max"});

	widget = gui2.find_widget(grid, "improper_summary", false, true);
	widget:set_label(this.ss_improper_summary_label(ss_shoulder_nose_angle_threshold, ss_improper_spine_angle_min, ss_improper_spine_angle_max, ss_shoulder_width_max, ss_spine_length_max));

	-- report_noperson = 2
	widget = gui2.find_widget(grid, "spine_length_min", false, true);
	widget:set_placeholder("0.25");
	widget:set_label(string.format("%.3f", ss_spine_length_min));
	widget:set_did_text_changed("did_text_box_changed", {key = "ss_spine_length_min"});


	widget = gui2.find_widget(grid, "noperson_summary", false, true);
	widget:set_label(this.ss_noperson_summary_label(ss_spine_length_min, ss_spine_length_max));
end

function aplt_leagor_khome__posture.pre_sit_share(grid, improper_time, noperson_time, sedentary_time)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._;

	local widget = gui2.find_widget(grid, "remark", false, true);
	var.sit_front_remark_ = widget;
	widget:set_label(this.sit_share_remark_label());

	-- report_improper = 1
	widget = gui2.find_widget(grid, "improper_time", false, true);
	widget:set_placeholder("8");
	widget:set_label(tostring(improper_time));
	widget:set_did_text_changed("did_text_box_changed", {key = "sshare_improper_time"});

	widget = gui2.find_widget(grid, "improper_summary", false, true);
	widget:set_label(this.share_improper_summary_label(improper_time));

	-- report_noperson = 2
	widget = gui2.find_widget(grid, "noperson_time", false, true);
	widget:set_placeholder("120");
	widget:set_label(tostring(noperson_time));
	widget:set_did_text_changed("did_text_box_changed", {key = "sshare_noperson_time"});

	widget = gui2.find_widget(grid, "noperson_summary", false, true);
	widget:set_label(this.share_noperson_summary_label(noperson_time));

	-- report_sedentary = 3
	widget = gui2.find_widget(grid, "sedentary_time", false, true);
	widget:set_placeholder("30");
	widget:set_label(tostring(sedentary_time));
	widget:set_did_text_changed("did_text_box_changed", {key = "sshare_sedentary_time"});

	widget = gui2.find_widget(grid, "sedentary_summary", false, true);
	widget:set_label(this.share_sedentary_summary_label(sedentary_time));
end

function aplt_leagor_khome__posture.pre_shoulder_neck_layer(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._;

	local widget = gui2.find_widget(grid, "remark", false, true);
	widget:set_label(_("shoulder_neck_layer remark"));
end

function aplt_leagor_khome__posture.save_pref_fields(current_layer)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;

	if (current_layer == this.SIT_FRONT_LAYER) then
		var.window_:send_cpp_id(aplt.cpp_id_save_sf_6fields);

	elseif (current_layer == this.SIT_SIDE_LAYER) then
		var.window_:send_cpp_id(aplt.cpp_id_save_ss_4fields);

	elseif (current_layer == this.SIT_SHARE_LAYER) then
		var.window_:send_cpp_id(aplt.cpp_id_save_sshare_3fields);
	end

	return true;
end

function aplt_leagor_khome__posture.did_navigation_pre_change(report, from, to)
	local this = aplt_leagor_khome__posture;
	local var = this.var;

	this.save_pref_fields(from:at());

	return true;
end

function aplt_leagor_khome__posture.did_navigation_changed(report, widget)
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	
	local current_layer = widget:at();
	var.list_stack_:set_radio_layer(current_layer);
	var.current_layer_ = current_layer;
end

function aplt_leagor_khome__posture.click_back()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_HOME);
end

function aplt_leagor_khome__posture.sit_share_remark_label()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local var_name = "fake__env_basesubtask_";
	local improper = 1;
	local noperson = 2;
	local sedentary = 3;
	return vgettext2("sit_share remark, $var_name, $improper, $noperson, $sedentary",
		{var_name = var_name, improper = improper, noperson = noperson, sedentary = sedentary});
end

function aplt_leagor_khome__posture.sf_improper_summary_label(shoulder_width_max, ear_diff_threshold, head_forward_threshold)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_front improper summary, $shoulder_width_max, $ear_diff_threshold, $head_forward_threshold",
		{shoulder_width_max = shoulder_width_max, ear_diff_threshold = ear_diff_threshold, 
		head_forward_threshold = head_forward_threshold});
end

function aplt_leagor_khome__posture.sf_noperson_summary_label(ear_width_min, shoulder_width_min)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_front noperson summary, $ear_width_min, $shoulder_width_min",
		{ear_width_min = ear_width_min, shoulder_width_min = shoulder_width_min});
end

function aplt_leagor_khome__posture.ss_improper_summary_label(shoulder_nose_angle_threshold, ss_improper_spine_angle_min, ss_improper_spine_angle_max, ss_shoulder_width_max, ss_spine_length_max)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_side improper summary, $shoulder_nose_angle_threshold, $improper_spine_angle_min, $improper_spine_angle_max, $shoulder_width_max, $spine_length_max",
		{shoulder_nose_angle_threshold = shoulder_nose_angle_threshold, improper_spine_angle_min = ss_improper_spine_angle_min, improper_spine_angle_max = ss_improper_spine_angle_max,
		shoulder_width_max = ss_shoulder_width_max, spine_length_max = ss_spine_length_max});
end

function aplt_leagor_khome__posture.ss_noperson_summary_label(spine_length_min)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_side noperson summary, $spine_length_min",
		{spine_length_min = spine_length_min});
end

function aplt_leagor_khome__posture.share_improper_summary_label(improper_time)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_share improper summary, $improper_time",
		{improper_time = improper_time});
end

function aplt_leagor_khome__posture.share_noperson_summary_label(noperson_time)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_share noperson summary, $noperson_time",
		{noperson_time = noperson_time});
end

function aplt_leagor_khome__posture.share_sedentary_summary_label(sedentary_time)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local vgettext2 = aplt.vgettext2;

	return vgettext2("sit_share sedentary summary, $sedentary_time",
		{sedentary_time = sedentary_time});
end

function aplt_leagor_khome__posture.did_text_box_changed(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt._
	local vgettext2 = aplt.vgettext2;

	local ftype = this.field_types[cfg.key];
	assert(ftype ~= nil);

	local err_msg = "";
	local num = nil;
	local label = widget:label();

	if #label == 0 then
		err_msg = _("Value cannot be empty");

	elseif (ftype == this.ftype_ratio) then
		num = tonumber(label);
		if (num == nil) then
			err_msg = vgettext2("Value type must be $type", {type = "float"});

		elseif (num < this.RATIO_MIN or num > this.RATIO_MAX) then
			err_msg = vgettext2("The value needs to be in the range of [$min, $max]", 
				{min = this.RATIO_MIN, max = this.RATIO_MAX})
		end

	elseif (ftype == this.ftype_time) then
		num = tonumber(label);
		if (num == nil or (math.type(num) ~= "integer")) then
			err_msg = vgettext2("Value type must be $type", {type = "integer"});

		elseif (num < this.TIME_MIN or num > this.TIME_MAX) then
			err_msg = vgettext2("The value needs to be in the range of [$min, $max]", 
				{min = this.TIME_MIN, max = this.TIME_MAX});
		end

	elseif (ftype == this.ftype_angle) then
		num = tonumber(label);
		if (num == nil or (math.type(num) ~= "integer")) then
			err_msg = vgettext2("Value type must be $type", {type = "integer"});

		elseif (num < this.ANGLE_MIN or num > this.ANGLE_MAX) then
			err_msg = vgettext2("The value needs to be in the range of [$min, $max]", 
				{min = this.ANGLE_MIN, max = this.ANGLE_MAX});
		end

	else 
		assert(false);
	end

	if (#err_msg == 0) then
		assert(num ~= nil);
		aplt.preferences:_set_value(cfg.key, num);
	end
	var.status_:set_label(err_msg);
end

function aplt_leagor_khome__posture.timer_handler(now)
	local this = aplt_leagor_khome__posture;
	local var = this.var;
	local _ = aplt_leagor_khome._
end