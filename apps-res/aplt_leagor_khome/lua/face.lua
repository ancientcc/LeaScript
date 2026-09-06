-- var.
aplt_leagor_khome__face = {
	FACE_LAYER = 0,
	TRIGGER_LAYER = 1,

	-- POS_NAME_AT = 2,
	POS_UUID_LUA_IDX = 2,

	max_disp_size = 200,
	show_image = false,

	var = {},
};

function aplt_leagor_khome__face.construct()
	local var = aplt_leagor_khome__face.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	return 0;
end

function aplt_leagor_khome__face.pre_show(dlg, window)
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local aplt = aplt_leagor_khome;
	local _ = aplt_leagor_khome._;

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	local widget = gui2.find_widget(window, "title", false, true):set_label(_("Face"));

	widget = gui2.find_widget(window, "back", false, true);
	widget:set_did_left_click("click_back");

	local alias_id = aplt.preferences:_value("alias_id");
	if (not rose.is_format(alias_id, rosek.FMT_APLT_IOT_ALIAS_ID)) then
		alias_id = aplt.def_alias_id;
		aplt.preferences:_set_value("alias_id", alias_id);
	end

	widget = gui2.find_widget(window, "list_stack", false, true);
	var.list_stack_ = widget;

	this.pre_face(widget:layer(this.FACE_LAYER), alias_id);
	-- this.pre_trigger(widget:layer(this.TRIGGER_LAYER));

	-- get current devices and positions.
	local faces = aplt.cpp_camera2_:kface_reload();
	var.faces_ = faces;

	this.reload_face_list(faces);

	widget = gui2.find_widget(window, "tag_report", false, true);
	var.tag_report_ = widget;

	local item = widget:insert_item("", _("restaurant^Table"));
	-- item:set_icon("misc/task.png");

	-- item = widget:insert_item("", _("Trigger"));
	-- item:set_icon("misc/log.png");

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.FACE_LAYER);

	var.tag_report_:set_visible(gui2k.tvisible_INVISIBLE);

end

function aplt_leagor_khome__face.post_show()
	aplt_leagor_khome__face.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khome__face.pre_face(grid, alias_id)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local _ = aplt._

	local widget = gui2.find_widget(grid, "alias_id", false, true);
	widget:set_placeholder(_("Two capital letters"));
	widget:set_label(alias_id);
	widget:set_did_text_changed("did_text_box_changed", {key = "alias_id"});

	widget = gui2.find_widget(grid, "remark", false, true);
	var.device_remark_ = widget;
	widget:set_label(this.device_remark_label());

	widget = gui2.find_widget(grid, "show_image", false, true);
	widget:set_value(this.show_image);
	widget:set_did_state_changed("did_toggle_changed", {key = "show_image"});

	local list = gui2.find_widget(grid, "face_list", false, true);
	list:enable_select(false);
	var.face_list_ = list;

	widget = list:set_did_left_drag_widget_click({id = "modify_name", method = "click_drag_widget"});
	widget:set_icon("misc/bg_f3f3f3.png");

	widget = list:set_did_left_drag_widget_click({id = "erase", method = "click_drag_widget"});
	widget:set_icon("misc/bg_ff0000.png");

	widget = gui2.find_widget(grid, "status", false, true);
	var.device_status_ = widget;
end

function aplt_leagor_khome__face.pre_trigger(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local _ = aplt._

	-- local list = gui2.find_widget(grid, "list", false, true);
	-- list:enable_select(false);
	-- var.position_list_ = list;

end

function aplt_leagor_khome__face.did_navigation_changed(report, widget)
	local this = aplt_leagor_khome__face;
	local var = this.var;
	
	local current_layer = widget:at();
	var.list_stack_:set_radio_layer(current_layer);
end

function aplt_leagor_khome__face.click_back()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_HOME);
end

function aplt_leagor_khome__face.did_toggle_changed(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;

	assert(cfg.key ~= nil and #cfg.key > 0);

	local val = widget:get_value();
	if (cfg.key == "show_image") then
		this.show_image = val;

		this.reload_face_list(var.faces_);
	end

	-- aplt.preferences:_set_value(cfg.key, val);
end

function aplt_leagor_khome__face.reload_face_list(faces)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local list = var.face_list_;
	list:clear();

	local size = #faces;
	for at = 0, size - 1 do
		local face = faces[at + 1];

		local data = {
			faceid = tostring(face[1]);
			img = string.format("%s/saves/facestore/fid_%i.png", aplt.preferences_dir, face[1]);
			name = face[2],
			time = face[3];
		};

		local row = list:insert_row(data);

		if (not this.show_image) then
			gui2.find_widget(row, "img", false, true):set_visible(gui2k.tvisible_INVISIBLE);
		end
	end

	var.device_status_:set_label(vgettext2("$count faces", {count = tonumber(size)}));
end

function aplt_leagor_khome__face.verify_face_name(label, cfg)
	if (#label == 0 or label == cfg.initial) then
		return false;
	end
	return true;
end

function aplt_leagor_khome__face.click_drag_widget(list, row, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;

	local at = row:at();

	list:cancel_drag();

	-- rose.cpp_breakpoint();
	local face = var.faces_[at + 1];

	if (cfg.id == "modify_name") then
		local cfg2 = {window_id = "aplt_leagor_khome__face", method = "verify_face_name", 
			title = vgettext2("Modify name(faceid: $faceid)", {faceid = face[1]}), 
			initial = face[2], ok = _("OK")};

		local x, y, width, height = list:get_window():rect();
		local result = gui2.show_edit_box(nposm, height // 5, cfg2);
		if (not result) then
			return;
		end
		aplt.cpp_camera2_:kface_modify_face(face[1], "name", result)

	elseif (cfg.id == "erase") then
		local retval = gui2.show_message(nil, vgettext2("Do you want to erase '$name'(faceid: $faceid)?", 
			{name = face[2], faceid = face[1]}), gui2k.tmessage_yes_no_buttons);
		if (not retval) then
			return;
		end

		aplt.cpp_camera2_:kface_erase_face(face[1], nposm, "");
	else
		assert(false);
	end

	local faces = aplt.cpp_camera2_:kface_reload();
	var.faces_ = faces;

	this.reload_face_list(faces);
end

function aplt_leagor_khome__face.device_remark_label()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local alias_id = aplt.preferences:_value("alias_id");
	return vgettext2("device remark, $alias_id", {alias_id = alias_id});
end

function aplt_leagor_khome__face.did_text_box_changed(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;

	local label = widget:label();
	if #label == 0 then
		return;
	end

	if (cfg.key == "alias_id") then
		if (not rose.is_format(label, rosek.FMT_APLT_IOT_ALIAS_ID)) then
			return;
		end

	elseif (cfg.key == "voice_threshold") then
		local value = tonumber(label);
		if (value == nil) then
			return;
		end
		if (value < this.MIN_VOICE_THRESHOLD or value > this.MAX_VOICE_THRESHOLD) then
			return;
		end

	else 
		assert(false);
	end

	if (cfg.key == "alias_id") then
		local orignal_alias_id = aplt.preferences:_value("alias_id");

		assert(rose.is_format(orignal_alias_id, rosek.FMT_APLT_IOT_ALIAS_ID));
		if (label ~= orignal_alias_id) then
			aplt.preferences:_set_value(cfg.key, label);
			local device_size, devices = aplt.cpp_block2_:cooking_reload(label);
			var.devices_ = devices;

			this.reload_device_list(devices);
		end

		var.device_remark_:set_label(this.device_remark_label());
	end
end

function aplt_leagor_khome__face.click_position_edit(list, row, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__face;
	local var = this.var;

	local method = cfg.method;

	local items = {};

end

function aplt_leagor_khome__face.timer_handler(now)
	local this = aplt_leagor_khome__face;
	local var = this.var;
	local _ = aplt_leagor_khome._
end