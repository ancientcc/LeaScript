-- var.
aplt_leagor_khome__restaurant = {
	TABLE_LAYER = 0,
	TRIGGER_LAYER = 1,

	-- POS_NAME_AT = 2,
	POS_UUID_LUA_IDX = 2,

	max_disp_size = 200,
	var = {},
};

function aplt_leagor_khome__restaurant.construct()
	local var = aplt_leagor_khome__restaurant.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	return 0;
end

function aplt_leagor_khome__restaurant.pre_show(dlg, window)
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local aplt = aplt_leagor_khome;
	local _ = aplt_leagor_khome._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	local widget = gui2.find_widget(window, "title", false, true):set_label(_("Restaurant"));

	widget = gui2.find_widget(window, "back", false, true);
	widget:set_did_left_click("click_back");

	local alias_id = aplt.preferences:_value("alias_id");
	if (not rose.is_format(alias_id, rosek.FMT_APLT_IOT_ALIAS_ID)) then
		alias_id = aplt.def_alias_id;
		aplt.preferences:_set_value("alias_id", alias_id);
	end

	widget = gui2.find_widget(window, "list_stack", false, true);
	var.list_stack_ = widget;

	this.pre_table(widget:layer(this.TABLE_LAYER), alias_id);
	-- this.pre_trigger(widget:layer(this.TRIGGER_LAYER));

	-- get current devices and positions.
	local device_size, devices = aplt.cpp_block2_:cooking_reload(alias_id);
	var.devices_ = devices;

	this.reload_device_list(devices);

	widget = gui2.find_widget(window, "tag_report", false, true);
	var.tag_report_ = widget;

	local item = widget:insert_item("", _("restaurant^Table"));
	-- item:set_icon("misc/task.png");

	-- item = widget:insert_item("", _("Trigger"));
	-- item:set_icon("misc/log.png");

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.TABLE_LAYER);

	var.tag_report_:set_visible(gui2k.tvisible_INVISIBLE);

end

function aplt_leagor_khome__restaurant.post_show()
	aplt_leagor_khome__restaurant.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khome__restaurant.pre_table(grid, alias_id)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local _ = aplt._

	local widget = gui2.find_widget(grid, "alias_id", false, true);
	widget:set_placeholder(_("Two capital letters"));
	widget:set_label(alias_id);
	widget:set_did_text_changed("did_text_box_changed", {key = "alias_id"});

	widget = gui2.find_widget(grid, "remark", false, true);
	var.device_remark_ = widget;
	widget:set_label(this.device_remark_label());

	widget = gui2.find_widget(grid, "device_list", false, true);
	widget:enable_select(false);
	var.device_list_ = widget;

	widget = gui2.find_widget(grid, "status", false, true);
	var.device_status_ = widget;
end

function aplt_leagor_khome__restaurant.pre_trigger(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local _ = aplt._

	-- local list = gui2.find_widget(grid, "list", false, true);
	-- list:enable_select(false);
	-- var.position_list_ = list;

end

function aplt_leagor_khome__restaurant.did_navigation_changed(report, widget)
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	
	local current_layer = widget:at();
	var.list_stack_:set_radio_layer(current_layer);
end

function aplt_leagor_khome__restaurant.click_back()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_HOME);
end

function aplt_leagor_khome__restaurant.reload_device_list(devices)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local list = var.device_list_;

	list:clear();
	local size = #devices;
	local has_position_size = 0;
	for at = 0, size - 1 do
		local device = devices[at + 1];

		local map_position = ros.curmap_get_position(device[this.POS_UUID_LUA_IDX]);
		local position_uuid = "";
		if (map_position ~= nil) then
			position_uuid = map_position["name"];
			has_position_size = has_position_size + 1;
		end

		local data = {
			number = tostring(at + 1);
			device = device[1],
			position = position_uuid;
		};
		local row = list:insert_row(data);

		local widget = gui2.find_widget(row, "position", false, true);
		widget:set_did_left_click("click_map_position", {at = at});
	end

	var.device_status_:set_label(vgettext2("$count devices    $positions has position", {count = tonumber(size), positions = tonumber(has_position_size)}));
end

function aplt_leagor_khome__restaurant.click_map_position(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local _ = aplt._;

	local at = cfg.at;

	-- rose.cpp_breakpoint();

	local goods_position_uuid = var.devices_[at + 1][this.POS_UUID_LUA_IDX];
	
	local positions = ros.curmap_get_positions();
	if (#positions == 0) then
		gui2.show_message("", _("Need to set the current map, or have at least one position on the map"));
		return;
	end


	local initial = nposm;
	local items = {};

	local label;
	for k, v in ipairs(positions) do
		label = v["name"];

		if (v["uuid"] == goods_position_uuid) then
			initial = #items;
		end
		table.insert(items, {label, #items});
	end

	if #items == 0 then
		return;
	end

	local x, y, width, height = widget:rect();
	local cursel = gui2.show_menu(x, y + height + 16 * gui2k.hdpi_scale, items, initial);
	if cursel == nposm then
		return;
	end

	local new_name = positions[cursel + 1]["name"];
	local new_uuid = positions[cursel + 1]["uuid"];

	widget:set_label(new_name);
	aplt.cpp_block2_:cooking_set_position(at, new_uuid);
	var.devices_[at + 1][this.POS_UUID_LUA_IDX] = new_uuid;
end

function aplt_leagor_khome__restaurant.device_remark_label()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local alias_id = aplt.preferences:_value("alias_id");
	return vgettext2("device remark, $alias_id", {alias_id = alias_id});
end

function aplt_leagor_khome__restaurant.did_text_box_changed(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
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

function aplt_leagor_khome__restaurant.click_position_edit(list, row, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;

	local method = cfg.method;

	local items = {};

end

function aplt_leagor_khome__restaurant.timer_handler(now)
	local this = aplt_leagor_khome__restaurant;
	local var = this.var;
	local _ = aplt_leagor_khome._
end