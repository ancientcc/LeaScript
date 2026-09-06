-- var.
aplt_leagor_khome__query = {
	GOODS_LAYER = 0,
	POSITION_LAYER = 1,

	max_disp_size = 200,
	var = {},
};

function aplt_leagor_khome__query.construct()
	local var = aplt_leagor_khome__query.var;
	var.startup_ticks_ = rose.SDL_GetTicks();
	return 0;
end

function aplt_leagor_khome__query.pre_show(dlg, window)
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local aplt = aplt_leagor_khome;
	local _ = aplt_leagor_khome._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	local widget = gui2.find_widget(window, "title", false, true):set_label(_("Query"));

	widget = gui2.find_widget(window, "back", false, true);
	widget:set_did_left_click("click_back");

	widget = gui2.find_widget(window, "reload", false, true);
	widget:set_did_left_click("click_reload");

	widget = gui2.find_widget(window, "list_stack", false, true);
	var.list_stack_ = widget;

	this.pre_goods(widget:layer(this.GOODS_LAYER));
	this.pre_position(widget:layer(this.POSITION_LAYER));

	-- get current items and positions.
	local err_msg, item_size, items, positions = aplt.cpp_block2_:query_reload(false, this.max_disp_size);
	var.positions_ = positions;

	-- rose.cpp_breakpoint();

	this.reload_goods_list(items);
	this.reload_position_list(positions);

	widget = gui2.find_widget(window, "tag_report", false, true);
	var.tag_report_ = widget;

	local item = widget:insert_item("", _("Goods"));
	-- item:set_icon("misc/task.png");

	item = widget:insert_item("", _("Position"));
	-- item:set_icon("misc/log.png");

	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.GOODS_LAYER);

end

function aplt_leagor_khome__query.post_show()
	aplt_leagor_khome__query.var = {};
	-- rose.cpp_breakpoint();
end

function aplt_leagor_khome__query.pre_goods(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local _ = aplt._

	var.goods_list_ = gui2.find_widget(grid, "list", false, true);
end

function aplt_leagor_khome__query.pre_position(grid)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local _ = aplt._

	local list = gui2.find_widget(grid, "list", false, true);
	list:enable_select(false);
	var.position_list_ = list;

	-- local widget = list:set_did_left_drag_widget_click({id = "edit", method = "click_position_edit"});
	-- widget:set_icon("misc/bg_f3f3f3.png");
end

function aplt_leagor_khome__query.did_navigation_changed(report, widget)
	local this = aplt_leagor_khome__query;
	local var = this.var;
	
	local current_layer = widget:at();
	var.list_stack_:set_radio_layer(current_layer);
end

function aplt_leagor_khome__query.click_back()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;

	var.window_:set_retval(aplt.DLG_HOME);
end

function aplt_leagor_khome__query.click_reload()
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local _ = aplt._;
	local vgettext2 = aplt.vgettext2;

	local retval = gui2.show_message(nil, vgettext2("Do you want to import goods from file($file)?",
		{file = rose.normalize_path("query.csv", false)}), gui2k.tmessage_yes_no_buttons);
	if (not retval) then
		return;
	end

	local err_msg, item_size, items, positions = aplt.cpp_block2_:query_reload(true, this.max_disp_size);

	if (#err_msg ~= 0) then
		-- gui2.show_message(nil, string.format("login[http] Err:%i", status));
		gui2.show_message(nil, err_msg);
		return;
	else
		gui2.show_message(nil, _("Import finished"));
	end

	var.positions_ = positions;

	-- rose.cpp_breakpoint();

	this.reload_goods_list(items);
	this.reload_position_list(positions);
end

function aplt_leagor_khome__query.reload_goods_list(items)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;

	local list = var.goods_list_;

	list:clear();
	local size = #items;
	for at = 0, size - 1 do
		local item = items[at + 1];

		-- rose.cpp_breakpoint();
		local data = {
			id = item[1],
			position = item[2],
			name = rose.truncate_to_max_bytes(item[3], 40, true),
			desc = rose.truncate_to_max_bytes(item[4], 40, true);
			brand = rose.truncate_to_max_bytes(item[5], 40, true);
		};
		list:insert_row(data);
	end
end

function aplt_leagor_khome__query.reload_position_list(positions)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;

	local list = var.position_list_;

	list:clear();
	local size = #positions;
	for at = 0, size - 1 do
		local item = positions[at + 1];

		local map_position = ros.curmap_get_position(item[2]);

		local data = {
			goods_position = item[1],
			map_position = (map_position ~= nil) and map_position["name"] or "";
		};
		local row = list:insert_row(data);

		local widget = gui2.find_widget(row, "map_position", false, true);
		widget:set_did_left_click("click_map_position", {at = at});
	end
end

function aplt_leagor_khome__query.click_map_position(widget, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local _ = aplt._

	local at = cfg.at;
	local goods_position_name = var.positions_[at + 1][1];
	local goods_position_uuid = var.positions_[at + 1][2];
	
	local positions = ros.curmap_get_positions();
	if (#positions == 0) then
		gui2.show_message("", _("Need to set the current map, or have at least one position on the map"));
		return;
	end

	-- rose.cpp_breakpoint();

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
	aplt.cpp_block2_:query_set_position(goods_position_name, new_uuid);
	var.positions_[at + 1][2] = new_uuid;
end

function aplt_leagor_khome__query.click_position_edit(list, row, cfg)
	local aplt = aplt_leagor_khome;
	local this = aplt_leagor_khome__query;
	local var = this.var;

	local method = cfg.method;

	local items = {};

end

function aplt_leagor_khome__query.timer_handler(now)
	local this = aplt_leagor_khome__query;
	local var = this.var;
	local _ = aplt_leagor_khome._
end