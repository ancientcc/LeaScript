-- var.
aplt_nlsd_basic__home = {
	DRIVER_LAYER = 0,
	MORE_LAYER = 1,

	-- driver type
	DRIVERTYPE_BASE = 0,

	base_products = {},

	BUTTON_THRESHOLD_MIN = 5;
	BUTTON_THRESHOLD_MAX = 100;

	BASE_LAMP_V1 = "lamp_v1";

	ftype_button_threshold = 1;

	var = {},
};

function aplt_nlsd_basic__home.construct()
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local _ = aplt_nlsd_basic._;

	this.base_products = {
		lamp_v1 = {_("name^base_lamp_v1"), 0},
	};

	this.field_types = {
		-- sf <=> Sit Front
		power_threshold = this.ftype_button_threshold,
		non_power_threshold = this.ftype_button_threshold,
	};

	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;
	return 0;
end

function aplt_nlsd_basic__home.pre_show(dlg, window)
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local aplt = aplt_nlsd_basic;
	local _ = aplt._

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	gui2.find_widget(window, "title", false, true):set_label(_("Basic") .. "(v" .. aplt.version .. ")" );

	local widget = gui2.find_widget(window, "body", false, true);
	var.body_stack_ = widget;

	this.pre_driver(widget:layer(this.DRIVER_LAYER));
	this.pre_more(widget:layer(this.MORE_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("Driver"), icon = "misc/driver.png"},
		{label = _("More"), icon = "misc/more.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_pre_change("did_navigation_pre_change");
	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.DRIVER_LAYER);

end

function aplt_nlsd_basic__home.post_show()
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	this.save_pref_fields(var.current_layer_);
	var = {};
	-- rose.cpp_breakpoint();
end

function aplt_nlsd_basic__home.pre_driver(grid)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local _ = aplt._
	local vgettext2 = aplt.vgettext2;

	local base_serial, base_baudrate, longpress_threshold, power_threshold, non_power_threshold = 
		aplt.preferences:_value("base_serial", "base_serial_baudrate", "longpress_threshold", "power_threshold", "non_power_threshold");

	-- base's product
	local widget = gui2.find_widget(grid, "base_product", false, true);
	local name = "";
	local enum_product = nposm;
	if (aplt.cpp_lua_block_ ~= nil) then
		name, enum_product = aplt.cpp_lua_block_:lamp_product_name();
		if #name == 0 then
			name = "---";
		end
	end
	widget:set_label(name);
	
	-- rose.cpp_breakpoint();

	-- if want use joybot only, execute below statement.
	-- widget:set_active(false); 

	-- base's serial
	widget = gui2.find_widget(grid, "base_serial", false, true);
	widget:set_did_left_click("click_serial", {type = this.DRIVERTYPE_BASE});
	widget:set_label(base_serial);

	-- base's baudrate
	var.base_baudrate_ = gui2.find_widget(grid, "base_baudrate", false, true);
	var.base_baudrate_:set_did_left_click("click_serial_baudrate", {type = this.DRIVERTYPE_BASE});
	var.base_baudrate_:set_label(tostring(base_baudrate));

	widget = gui2.find_widget(grid, "remark", false, true);
	widget:set_label(_("driver^remark"));

	-- longpress threshold
	var.longpress_threshold_ = gui2.find_widget(grid, "longpress_threshold", false, true);
	var.longpress_threshold_:set_did_left_click("click_longpress_threshold");
	var.longpress_threshold_:set_label(this.longpress_threshold_label(longpress_threshold));

	-- button threshold grid
	widget = gui2.find_widget(grid, "button_threshold_grid", false, true);
	if (enum_product ~= nposm) then
	-- if (true) then
		widget = gui2.find_widget(grid, "power_threshold", false, true);
		widget:set_label(tostring(power_threshold));
		widget:set_did_text_changed("did_text_box_changed", {key = "power_threshold"});
		var.power_threshold_ = widget;

		widget = gui2.find_widget(grid, "non_power_threshold", false, true);
		widget:set_label(tostring(non_power_threshold));
		widget:set_did_text_changed("did_text_box_changed", {key = "non_power_threshold"});
		var.non_power_threshold_ = widget;

		widget = gui2.find_widget(grid, "button_threshold", false, true);
		widget:set_did_left_click("click_button_threshold");
		var.button_threshold_ = widget;

	else
		widget:set_visible(gui2k.tvisible_INVISIBLE);
	end

	var.status_ = gui2.find_widget(grid, "status", false, true);
end

function aplt_nlsd_basic__home.pre_more(grid)
	local _ = aplt_nlsd_basic._;

	gui2.find_widget(grid, "copyright", false, true):set_label(_("Copyright"));
end

function aplt_nlsd_basic__home.save_pref_fields(current_layer)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	if (current_layer == this.DRIVER_LAYER) then
		var.window_:send_cpp_id(aplt.cpp_id_driver_layer);
	end

	return true;
end

function aplt_nlsd_basic__home.did_navigation_pre_change(report, from, to)
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	this.save_pref_fields(from:at());

	return true;
end

function aplt_nlsd_basic__home.did_navigation_changed(report, widget)
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	
	local current_layer = widget:at();
	var.body_stack_:set_radio_layer(current_layer);
	var.current_layer_ = current_layer;
end

function aplt_nlsd_basic__home.click_product(widget, cfg)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local _ = aplt_nlsd_basic._;

	local key;
	local key_items;
	local items = {};

	if cfg.type == this.DRIVERTYPE_BASE then
		key = "base_product";
		-- items' second(value) must unique.
		key_items = {
			this.BASE_LAMP_V1
		};
		item3s = this.base_products;

	elseif cfg.type == this.DRIVERTYPE_LASER then
		key = "laser_product";
		key_items = {this.LASER_RPLIDAR_A1M8, this.LASER_N10}
		item3s = this.laser_products;

	elseif cfg.type == this.DRIVERTYPE_MOVEIT then
		-- key = "moveit_product";
		return;
	end

	assert(key ~= nil and key_items ~= nil and item3s ~= nil);

	local original_product = aplt.preferences:_value(key) or "";

	local initial = nposm;
	for k, v in ipairs(key_items) do
		if v == original_product then
			initial = #items;
			-- break; require all item3s's item fill to items.
		end
		table.insert(items, {item3s[v][1], #items});
	end

	-- for k = 1, #item3s do
	--	if item3s[k][2] == original_product then
	--		initial = v[2];
			-- break; require all item3s's item fill to items.
	--	end
	--	table.insert(items, {v[1], v[2]});
	-- end

	local x, y, width, height = widget:rect();
	local cursel = gui2.show_menu(x, y + height + 16 * gui2k.hdpi_scale, items, initial);
	if cursel == nposm then
		return;
	end

	local new_product_name = items[cursel + 1][1];
	local new_product = key_items[cursel + 1];

	widget:set_label(new_product_name);
	aplt.preferences:_set_value(key, new_product);

	if (cfg.type == this.DRIVERTYPE_BASE) then
		-- local layer = this.base_products[new_product][2];
		-- var.base_help_:set_radio_layer(layer);
	end
end

function aplt_nlsd_basic__home.click_serial(widget, cfg)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	local key = "base_serial";
	if cfg.type == this.DRIVERTYPE_LASER then
		key = "laser_serial";
	elseif cfg.type == this.DRIVERTYPE_MOVEIT then
		key = "moveit_serial";
	end
	local original_path = aplt.preferences:_value(key) or "";

	local ttys = rose.SDL_GetTtyUSB();
	-- if ttys == nil then
	--	return;
	-- end

	-- rose.cpp_breakpoint();

	local initial = nposm;
	local items = {};

	local label;
	for k, v in ipairs(ttys) do
		label = v["path"] or "";
		if #original_path ~= 0 then
			initial = k;
		end
		local name = v["name"] or ""; 
		if #name ~= 0 then
			label = label.. "(" .. name .. ")";
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

	local new_path = ttys[cursel + 1]["path"];

	widget:set_label(new_path);
	aplt.preferences:_set_value(key, new_path);
end

function aplt_nlsd_basic__home.click_serial_baudrate(widget, cfg)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	local key = "base_serial_baudrate";
	if cfg.type == this.DRIVERTYPE_LASER then
		key = "laser_serial_baudrate";
	elseif cfg.type == this.DRIVERTYPE_MOVEIT then
		key = "moveit_serial_baudrate";
	end

	local original_baudrate = aplt.preferences:_value(key) or "";

	local initial = nposm;
	local items = {{9600, 0}, {115200, 1}, {230400, 2}};

	local label;
	for k, v in ipairs(items) do
		if v[1] == original_baudrate then
			initial = v[2];
			break;
		end
	end

	local x, y, width, height = widget:rect();
	local cursel = gui2.show_menu(x, y + height + 16 * gui2k.hdpi_scale, items, initial);
	if cursel == nposm then
		return;
	end

	local new_baudrate = items[cursel + 1][1];
	widget:set_label(tostring(new_baudrate));
	aplt.preferences:_set_value(key, new_baudrate);
end

function aplt_nlsd_basic__home.click_longpress_threshold(widget)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;

	local key = "longpress_threshold";

	local original_value = aplt.preferences:_value(key) or "";

	local initial = nposm;
	local options = {{15}, {20}, {25}, {30}, {35}};
	local items = {};

	for k, v in ipairs(options) do
		if v[1] == original_value then
			initial = #items;
		end
		table.insert(items, {this.longpress_threshold_label(v[1]), #items});
	end

	local x, y, width, height = widget:rect();
	local cursel = gui2.show_menu(x, y + height + 16 * gui2k.hdpi_scale, items, initial);
	if cursel == nposm then
		return;
	end

	local new_threshold = options[cursel + 1][1];
	widget:set_label(this.longpress_threshold_label(new_threshold));
	aplt.preferences:_set_value(key, new_threshold);
end

function aplt_nlsd_basic__home.click_button_threshold(widget)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local _ = aplt._;

	assert(aplt.cpp_lua_block_ ~= nil);

	local power_threshold = tonumber(var.power_threshold_:label());
	local non_power_threshold = tonumber(var.non_power_threshold_:label());

	aplt.cpp_lua_block_:lamp_set_button_threshold(power_threshold, non_power_threshold);
	gui2.show_message("", _("It has been set. To confirm whether the set was successful, you can exit the applet and then enter it again to check whether the thresholds displayed for the two buttons here match what was previously set."));
end

function aplt_nlsd_basic__home.longpress_threshold_label(mult10)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local vgettext2 = aplt.vgettext2;

	local s = string.format("%.1f", mult10 / 10);
	return vgettext2("$threshold seconds", {threshold = s});
end

function aplt_nlsd_basic__home.set_button_threshold_active()
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local vgettext2 = aplt.vgettext2;

	local power_threshold = tonumber(var.power_threshold_:label());
	local non_power_threshold = tonumber(var.non_power_threshold_:label());

	local power_is_valid = power_threshold ~= nil and power_threshold >= this.BUTTON_THRESHOLD_MIN and power_threshold <= this.BUTTON_THRESHOLD_MAX and math.type(power_threshold) == "integer";
	local non_power_is_valid = non_power_threshold ~= nil and non_power_threshold >= this.BUTTON_THRESHOLD_MIN and non_power_threshold <= this.BUTTON_THRESHOLD_MAX and math.type(non_power_threshold) == "integer";
	var.button_threshold_:set_active(power_is_valid and non_power_is_valid);
end

function aplt_nlsd_basic__home.did_text_box_changed(widget, cfg)
	local aplt = aplt_nlsd_basic;
	local this = aplt_nlsd_basic__home;
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

	elseif (ftype == this.ftype_button_threshold) then
		num = tonumber(label);
		if (num == nil or (math.type(num) ~= "integer")) then
			err_msg = vgettext2("Value type must be $type", {type = "integer"});

		elseif (num < this.BUTTON_THRESHOLD_MIN or num > this.BUTTON_THRESHOLD_MAX) then
			err_msg = vgettext2("The value needs to be in the range of [$min, $max]", 
				{min = this.BUTTON_THRESHOLD_MIN, max = this.BUTTON_THRESHOLD_MAX});
		end

	else 
		assert(false);
	end

	-- if (#err_msg == 0) then
	--	assert(num ~= nil);
	--	aplt.preferences:_set_value(cfg.key, num);
	-- end

	if (#err_msg == 0) then
		this.set_button_threshold_active();
	else
		var.button_threshold_:set_active(false);
	end
	var.status_:set_label(err_msg);
end

function aplt_nlsd_basic__home.timer_handler(now)
	local this = aplt_nlsd_basic__home;
	local var = this.var;
	local _ = aplt_nlsd_basic._
end