-- var.
aplt_leagor_basic__home = {
	-- body stack
	DRIVER_LAYER = 0,
	NAVIGATION_LAYER = 1,
	SPEECH_LAYER = 2,
	IOT_LAYER = 3,
	AI_LAYER = 4,

	-- base help stack
	BASE_DOLL_LAYER = 0,
	BASE_WHEELTEC_LAYER = 1,
	BASE_ROSSERIAL_LAYER = 2,
	BASE_YAHBOOM_LAYER = 3,
	BASE_UNKNOWN_LAYER = 4,

	WHEELTEC_MINOR_BOX = 0,
	WHEELTEC_MINOR_OPEN = 1,
	WHEELTEC_MINOR_OPEN_M = 2,

	-- driver type
	DRIVERTYPE_BASE = 0,
	DRIVERTYPE_LASER = 1,
	DRIVERTYPE_MOVEIT = 2,

	base_products = {},
	laser_products = {},

	BASE_DOLL = "doll";
	BASE_WHEELTEC_BOX = "wheeltec_box";
	BASE_WHEELTEC_OPEN = "wheeltec_open";
	BASE_WHEELTEC_OPEN_M = "wheeltec_open_m";
	BASE_ROSSERIAL = "rosserial";
	BASE_YAHBOOM = "yahboom";

	LASER_RPLIDAR_A1M8 = "rplidar_a1m8";
	LASER_N10 = "lslidar_n10";

	MIN_YAHBOOM_MOTOR_THRESHOLD = 300;
	MAX_YAHBOOM_MOTOR_THRESHOLD = 2000;

	MIN_VOICE_THRESHOLD = 500;
	MAX_VOICE_THRESHOLD = 32768;

	MIN_SPARK_MAXTOKEN = 10;

	counter = 20;
	var = {},
};

function aplt_leagor_basic__home.construct()
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._;

	this.base_products = {doll = {_("name^base_doll"), 0},
		wheeltec_box = {_("name^base_wheeltec_box"), 1}, 
		wheeltec_open = {_("name^base_wheeltec_open"), 1}, 
		wheeltec_open_m = {_("name^base_wheeltec_open_m"), 1},
		rosserial = {_("name^base_rosserial"), 2},
		yahboom = {_("name^base_yahboom"), 3},
	};

	this.laser_products = {rplidar_a1m8 = {"RPLIDAR A1M8", nposm},
		lslidar_n10 = {"N10", nposm},
	};

	var.startup_ticks_ = rose.SDL_GetTicks();
	var.current_layer_ = nposm;

	return 0;
end

function aplt_leagor_basic__home.pre_show(dlg, window)
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local aplt = aplt_leagor_basic;
	local _ = aplt_leagor_basic._;

	var.dlg_ = dlg;
	var.window_ = window;

	window:set_label("misc/bg_ffffff.png");

	gui2.find_widget(window, "title", false, true):set_label(_("Basic") .. "(v" .. aplt.version .. ")" );

	local widget = gui2.find_widget(window, "body", false, true);
	var.body_stack_ = widget;

	this.pre_driver(widget:layer(this.DRIVER_LAYER));
	this.pre_navigation(widget:layer(this.NAVIGATION_LAYER));
	this.pre_speech(widget:layer(this.SPEECH_LAYER));
	this.pre_iot(widget:layer(this.IOT_LAYER));
	this.pre_ai(widget:layer(this.AI_LAYER));

	widget = gui2.find_widget(window, "navigation", false, true);
	local items = {
		{label = _("Driver"), icon = "misc/driver.png"},
		{label = _("Navigation"), icon = "misc/navigation.png"},
		{label = _("Speech"), icon = "misc/speech.png"},
		{label = _("IoT"), icon = "misc/iot.png"},
		{label = _("AI"), icon = "misc/ai.png"},
	};
	for k, v in ipairs(items) do
		local item = widget:insert_item("", v.label);
		item:set_icon(v.icon);
	end

	widget:set_did_item_pre_change("did_navigation_pre_change");
	widget:set_did_item_changed("did_navigation_changed");
	widget:select_item(this.DRIVER_LAYER);

end

function aplt_leagor_basic__home.post_show()
	local this = aplt_leagor_basic__home;
	local var = this.var;

	this.save_pref_fields(var.current_layer_);

	-- rose.cpp_breakpoint();

	var = {};
end

function aplt_leagor_basic__home.pre_driver(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	local base_product, base_serial, laser_product, laser_serial, moveit_serial = aplt.preferences:_value("base_product", 
		"base_serial", "laser_product", "laser_serial", "moveit_serial");

	local laser_baudrate, base_baudrate, moveit_baudrate, yahboom_motor_threshold = aplt.preferences:_value("laser_serial_baudrate", 
		"base_serial_baudrate", "moveit_serial_baudrate", "yahboom_motor_threshold");

	-- base's product
	local widget = gui2.find_widget(grid, "base_product", false, true);
	widget:set_did_left_click("click_product", {type = this.DRIVERTYPE_BASE});
	
	if #base_product == 0 or this.base_products[base_product] == nil then
		base_product = this.BASE_DOLL;
	end

	widget:set_label(this.base_products[base_product][1]);
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

	-- laser's product
	if #laser_product == 0 or this.laser_products[laser_product] == nil then
		laser_product = this.LASER_N10;
	end

	widget = gui2.find_widget(grid, "laser_product", false, true);
	widget:set_did_left_click("click_product", {type = this.DRIVERTYPE_LASER});
	widget:set_label(this.laser_products[laser_product][1]);

	-- laser's serial
	widget = gui2.find_widget(grid, "laser_serial", false, true);
	widget:set_did_left_click("click_serial", {type = this.DRIVERTYPE_LASER});
	widget:set_label(laser_serial);

	-- laser's baudrate
	widget = gui2.find_widget(grid, "laser_baudrate", false, true);
	widget:set_did_left_click("click_serial_baudrate", {type = this.DRIVERTYPE_LASER});
	widget:set_label(tostring(laser_baudrate));

	-- moveit's serial
	widget = gui2.find_widget(grid, "moveit_serial", false, true);
	widget:set_did_left_click("click_serial", {type = this.DRIVERTYPE_MOVEIT});
	widget:set_label(moveit_serial);

	-- base's baudrate
	widget = gui2.find_widget(grid, "moveit_baudrate", false, true);
	widget:set_did_left_click("click_serial_baudrate", {type = this.DRIVERTYPE_MOVEIT});
	widget:set_label(tostring(moveit_baudrate));

	-- base help
	widget = gui2.find_widget(grid, "base_help", false, true);
	this.pre_base_doll(widget:layer(this.BASE_DOLL_LAYER));
	this.pre_base_wheeltec(widget:layer(this.BASE_WHEELTEC_LAYER));
	this.pre_base_rosserial(widget:layer(this.BASE_ROSSERIAL_LAYER));
	this.pre_base_yahboom(widget:layer(this.BASE_YAHBOOM_LAYER), yahboom_motor_threshold);
	this.pre_base_unknown(widget:layer(this.BASE_UNKNOWN_LAYER));

	if (base_product == this.BASE_WHEELTEC_BOX or base_product == this.BASE_WHEELTEC_OPEN or base_product == this.BASE_WHEELTEC_OPEN_M) then
		this.set_wheeltec_desc_label(base_product);
	end

	widget:set_radio_layer(this.base_products[base_product][2]);
	var.base_help_ = widget;

	var.driver_remark_ = gui2.find_widget(grid, "remark", false, true);
	var.driver_remark_:set_label(_("driver^remark"));
end

function aplt_leagor_basic__home.pre_base_rosserial(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	local widget = gui2.find_widget(grid, "desc", false, true);
	widget:set_label(_("rosserial base help"));
end

function aplt_leagor_basic__home.set_wheeltec_desc_label(minor)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._;

	local widget = var.wheeltec_desc_widget_;
	if (minor == this.BASE_WHEELTEC_OPEN) then
		widget:set_label(_("wheeltec_open base help"));

	elseif (minor == this.BASE_WHEELTEC_OPEN_M) then
		widget:set_label(_("wheeltec_open_m base help"));

	else
		-- WHEELTEC_MINOR_BOX
		assert(minor == this.BASE_WHEELTEC_BOX);
		widget:set_label(_("wheeltec_box base help"));
	end
end

function aplt_leagor_basic__home.pre_base_wheeltec(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	local widget = gui2.find_widget(grid, "desc", false, true);
	var.wheeltec_desc_widget_ = widget;
end

function aplt_leagor_basic__home.pre_base_yahboom(grid, yahboom_motor_threshold)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	-- rose.cpp_breakpoint();

	local widget = gui2.find_widget(grid, "motor_threshold", false, true);
	widget:set_placeholder("[" .. this.MIN_YAHBOOM_MOTOR_THRESHOLD .. "," .. this.MAX_YAHBOOM_MOTOR_THRESHOLD .. "]");
	widget:set_label(tostring(yahboom_motor_threshold));
	widget:set_did_text_changed("did_text_box_changed", {key = "yahboom_motor_threshold"});

	widget = gui2.find_widget(grid, "desc", false, true);
	widget:set_label(_("yahboom base help"));
end

function aplt_leagor_basic__home.pre_base_doll(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	local widget = gui2.find_widget(grid, "desc", false, true);
	widget:set_label(_("doll base help"));
end

function aplt_leagor_basic__home.pre_base_unknown(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._

	local widget = gui2.find_widget(grid, "desc", false, true);
	widget:set_label(_("unknown base help"));
end

function aplt_leagor_basic__home.pre_navigation(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._
	local vgettext2 = aplt_leagor_basic.vgettext2

	local position_uuid = aplt.preferences:_value("navigation_position");

	local test_navigation = _("Test navigation");
	gui2.find_widget(grid, "note", false, true):set_label(vgettext2("navigation^note $ble", 
		{ble = test_navigation}));

	local widget = gui2.find_widget(grid, "position", false, true);
	widget:set_border("textbox");
	widget:set_did_left_click("click_navigation_position");
	if (ros.curmap_has_position(position_uuid)) then
		widget:set_label(ros.curmap_get_position(position_uuid)["name"]);
	end
	var.navigation_position_ = widget;

	var.navigation_counter_ = gui2.find_widget(grid, "counter", false, true);
	var.navigation_counter_:set_label(tostring(this.counter));

	-- send goal(bh = navigation_bh_luafunc)
	var.navigation_ble_goal_ = gui2.find_widget(grid, "ble_goal", false, true);
	var.navigation_ble_goal_:set_label(test_navigation);
	var.navigation_ble_goal_:set_did_left_click("click_navigation_goal");

end

function aplt_leagor_basic__home.pre_speech(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._
	local vgettext2 = aplt_leagor_basic.vgettext2

	gui2.find_widget(grid, "spark_help", false, true):set_label(vgettext2("spark help $min_maxtoken", 
		{min_maxtoken = this.MIN_SPARK_MAXTOKEN}));

	local disable_recognition, disable_question, goaling_enable, voice_threshold, spark_version, spark_appid, spark_apisecret, spark_apikey, spark_maxtoken = 
		aplt.preferences:_value("disable_recognition", "disable_question", "goaling_enable", "voice_threshold", "spark_version", "spark_appid", "spark_apisecret", "spark_apikey", "spark_maxtoken");

	local widget = gui2.find_widget(grid, "disable_recognition", false, true);
	widget:set_value(disable_recognition);
	widget:set_did_state_changed("did_toggle_changed", {key = "disable_recognition"});

	widget = gui2.find_widget(grid, "disable_question", false, true);
	widget:set_value(disable_question);
	widget:set_did_state_changed("did_toggle_changed", {key = "disable_question"});

	widget = gui2.find_widget(grid, "goaling_enable", false, true);
	widget:set_value(goaling_enable);
	widget:set_did_state_changed("did_toggle_changed", {key = "goaling_enable"});

	widget = gui2.find_widget(grid, "voice_threshold", false, true);
	widget:set_placeholder("[" .. this.MIN_VOICE_THRESHOLD .. "," .. this.MAX_VOICE_THRESHOLD .. "]");
	widget:set_label(tostring(voice_threshold));
	widget:set_did_text_changed("did_text_box_changed", {key = "voice_threshold"});

	widget = gui2.find_widget(grid, "spark_version", false, true);
	widget:set_did_left_click("click_spark_version");
	widget:set_label(spark_version);
	widget:set_visible(gui2k.tvisible_INVISIBLE);
	gui2.find_widget(grid, "spark_version_key", false, true):set_visible(gui2k.tvisible_INVISIBLE);

	local spark_key = "spark_appid";
	widget = gui2.find_widget(grid, spark_key, false, true);
	widget:set_maximum_chars(8);
	widget:set_placeholder("APPID");
	widget:set_label(spark_appid);
	widget:set_did_text_changed("did_spark_string_changed", {key = spark_key});

	spark_key = "spark_apisecret";
	widget = gui2.find_widget(grid, spark_key, false, true);
	widget:set_maximum_chars(32);
	widget:set_placeholder("APISecret");
	widget:set_label(spark_apisecret);
	widget:set_did_text_changed("did_spark_string_changed", {key = spark_key});
	widget:set_visible(gui2k.tvisible_INVISIBLE);
	gui2.find_widget(grid, "spark_apisecret_key", false, true):set_visible(gui2k.tvisible_INVISIBLE);

	spark_key = "spark_apikey";
	widget = gui2.find_widget(grid, spark_key, false, true);
	widget:set_maximum_chars(32);
	widget:set_placeholder("APIKey");
	widget:set_label(spark_apikey);
	widget:set_did_text_changed("did_spark_string_changed", {key = spark_key});
	widget:set_visible(gui2k.tvisible_INVISIBLE);
	gui2.find_widget(grid, "spark_apikey_key", false, true):set_visible(gui2k.tvisible_INVISIBLE);

	widget = gui2.find_widget(grid, "spark_maxtoken", false, true);
	widget:set_maximum_chars(3);
	widget:set_placeholder(vgettext2(">= $maxtoken or -1", {maxtoken = this.MIN_SPARK_MAXTOKEN}));
	widget:set_label(tostring(spark_maxtoken));
	widget:set_did_text_changed("did_spark_maxtoken_changed");
	widget:set_visible(gui2k.tvisible_INVISIBLE);
	gui2.find_widget(grid, "spark_maxtoken_key", false, true):set_visible(gui2k.tvisible_INVISIBLE);

end

function aplt_leagor_basic__home.pre_iot(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._
	local vgettext2 = aplt_leagor_basic.vgettext2

	gui2.find_widget(grid, "iot_help", false, true):set_label(vgettext2("iot help $device_id", 
		{device_id = _("Device ID")}));

	local tuya_client_id, tuya_client_secret, iot_deviceid1, iot_deviceid2, iot_deviceid3 = aplt.preferences:_value(
		"tuya_client_id", "tuya_client_secret", "iot_deviceid1", "iot_deviceid2", "iot_deviceid3");

	local iot_key = "tuya_client_id";
	local widget = gui2.find_widget(grid, iot_key, false, true);
	widget:set_maximum_chars(64);
	-- widget:set_placeholder("");
	widget:set_label(tuya_client_id);
	widget:set_did_text_changed("did_iot_deviceid_changed", {key = iot_key});

	iot_key = "tuya_client_secret";
	widget = gui2.find_widget(grid, iot_key, false, true);
	widget:set_maximum_chars(64);
	-- widget:set_placeholder("");
	widget:set_label(tuya_client_secret);
	widget:set_did_text_changed("did_iot_deviceid_changed", {key = iot_key});

	var.iot_sources_map = map.new();
	-- Since have var.iot_sources_map, why does var.iot_sources exist? 
	--   var.iot_sources_map can't guarantee the order, 
	--   but 'click_iot_src()' shows all src for use to select, and them need to be in order.
	var.iot_sources = ros.iot_sources();
	for k, v in ipairs(var.iot_sources) do
		local code = v["code"];
		local source = {code = code, 
			name = v["name"],
		}
		var.iot_sources_map:insert(code, source);
	end

	local items = {{"iot_src1", "iot_deviceid1", iot_deviceid1}, 
		{"iot_src2", "iot_deviceid2", iot_deviceid2}, 
		{"iot_src3", "iot_deviceid3", iot_deviceid3}};

	var.iot_src = {}; 
	local vstr;
	local src_label = nil;
	local deviceid;
	for k, v in ipairs(items) do
		local vstr = rose.split(v[3], string.byte(','), rosek.SPLIT_REMOVE_EMPTY | rosek.SPLIT_STRIP_SPACES);
		local code = 0; -- iot_src_doorbell
		if #vstr == 2 then
			code = tonumber(vstr[1]);
			local source = var.iot_sources_map:find(code);
			if (source ~= nil) then
				src_label = source.name;
			end
			deviceid = vstr[2];
		else
			deviceid = "";
		end

		widget = gui2.find_widget(grid, v[1], false, true);
		if (src_label ~= nil) then
			widget:set_label(src_label);
		end
		widget:set_did_left_click("click_iot_src", {at = k - 1});

		widget = gui2.find_widget(grid, v[2], false, true);
		widget:set_maximum_chars(64);
		widget:set_placeholder(_("IoT Device ID"));
		widget:set_label(deviceid);
		widget:set_did_text_changed("did_iot_deviceid_changed", {key = v[2]});

		var.iot_src[k] = code;
	end

	-- rose.cpp_breakpoint();
end

function aplt_leagor_basic__home.pre_ai(grid)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._
	local vgettext2 = aplt_leagor_basic.vgettext2

	gui2.find_widget(grid, "ai_help", false, true):set_label(vgettext2("ai help $deepseek", 
		{deepseek = _("Deepseek")}));

	local ds_api_key = aplt.preferences:_value("ds_api_key");

	local ai_key = "ds_api_key";
	local widget = gui2.find_widget(grid, ai_key, false, true);
	widget:set_maximum_chars(64);
	widget:set_placeholder("sk-01234567890123456789012345678901");
	widget:set_label(ds_api_key);
	widget:set_did_text_changed("did_ai_text_changed", {key = ai_key});
end

function aplt_leagor_basic__home.save_appid_3field()
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local grid = var.body_stack_:layer(this.SPEECH_LAYER);

	local widget = gui2.find_widget(grid, "spark_appid", false, true);
	local spark_appid = widget:label();

	widget = gui2.find_widget(grid, "spark_apisecret", false, true);
	local spark_apisecret = widget:label();

	widget = gui2.find_widget(grid, "spark_apikey", false, true);
	local spark_apikey = widget:label();

	aplt.preferences:_set_value("spark_appid", spark_appid, "spark_apisecret", spark_apisecret, "spark_apikey", spark_apikey);

	var.window_:send_cpp_id(aplt.cpp_id_save_xfyun_3fields);
end

function aplt_leagor_basic__home.save_3iot_deviceid()
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local grid = var.body_stack_:layer(this.IOT_LAYER);

	local id_keys = {"tuya_client_id", "tuya_client_secret", 
		"iot_deviceid1", "iot_deviceid2", "iot_deviceid3"};
	local vals = {};

	local widget;
	for k, v in ipairs(id_keys) do
		widget = gui2.find_widget(grid, v, false, true);
		if (k < 3) then
			vals[v] = rose.convert_string(widget:label(), rosek.STRCVT_STRIP);
		else
			vals[v] = var.iot_src[k - 2] .. ", " .. rose.convert_string(widget:label(), rosek.STRCVT_STRIP);
		end
	end

	aplt.preferences:_set_value(id_keys[1], vals[id_keys[1]], 
		id_keys[2], vals[id_keys[2]], id_keys[3], vals[id_keys[3]],
		id_keys[4], vals[id_keys[4]], id_keys[5], vals[id_keys[5]]);

	var.window_:send_cpp_id(aplt.cpp_id_save_iot_5fields);

end

function aplt_leagor_basic__home.save_ai_1fields()
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local grid = var.body_stack_:layer(this.AI_LAYER);

	local id_keys = {"ds_api_key"};
	local vals = {};

	local widget;
	for k, v in ipairs(id_keys) do
		widget = gui2.find_widget(grid, v, false, true);
		vals[v] = rose.convert_string(widget:label(), rosek.STRCVT_STRIP);
	end

	aplt.preferences:_set_value(id_keys[1], vals[id_keys[1]]);

	var.window_:send_cpp_id(aplt.cpp_id_save_ai_1fields);
end

function aplt_leagor_basic__home.save_pref_fields(current_layer)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	if (current_layer == this.SPEECH_LAYER) then
		this.save_appid_3field();

	elseif (current_layer == this.IOT_LAYER) then
		this.save_3iot_deviceid();

	elseif (current_layer == this.AI_LAYER) then
		this.save_ai_1fields();
	end

	return true;
end

function aplt_leagor_basic__home.did_navigation_pre_change(report, from, to)
	local this = aplt_leagor_basic__home;
	local var = this.var;

	this.save_pref_fields(from:at());

	return true;
end

function aplt_leagor_basic__home.did_navigation_changed(report, widget)
	local this = aplt_leagor_basic__home;
	local var = this.var;
	
	local current_layer = widget:at();
	var.body_stack_:set_radio_layer(current_layer);
	var.current_layer_ = current_layer;
end

function aplt_leagor_basic__home.click_product(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._;

	local key;
	local key_items;
	local items = {};

	if cfg.type == this.DRIVERTYPE_BASE then
		key = "base_product";
		-- items' second(value) must unique.
		local all_base = true;
		if (all_base) then
			key_items = {this.BASE_DOLL,
				this.BASE_WHEELTEC_BOX, this.BASE_WHEELTEC_OPEN, this.BASE_WHEELTEC_OPEN_M,
				this.BASE_ROSSERIAL, this.BASE_YAHBOOM,
			};
		else
			key_items = {this.BASE_DOLL,
				this.BASE_WHEELTEC_BOX, this.BASE_WHEELTEC_OPEN, this.BASE_WHEELTEC_OPEN_M,
			};
		end
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
		local layer = this.base_products[new_product][2];
		var.base_help_:set_radio_layer(layer);

		if (layer == this.BASE_WHEELTEC_LAYER) then
			this.set_wheeltec_desc_label(new_product);
		end
	end
end

function aplt_leagor_basic__home.click_serial(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
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

function aplt_leagor_basic__home.click_serial_baudrate(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
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

function aplt_leagor_basic__home.did_text_box_changed(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local label = widget:label();
	if #label == 0 then
		return;
	end
	local value = tonumber(label);
	if (value == nil) then
		return;
	end
	if (cfg.key == "yahboom_motor_threshold") then
		if (value < this.MIN_YAHBOOM_MOTOR_THRESHOLD or value > this.MAX_YAHBOOM_MOTOR_THRESHOLD) then
			return;
		end

	elseif (cfg.key == "voice_threshold") then
		if (value < this.MIN_VOICE_THRESHOLD or value > this.MAX_VOICE_THRESHOLD) then
			return;
		end

	else 
		assert(false);
	end

	aplt.preferences:_set_value(cfg.key, value);
end

function aplt_leagor_basic__home.click_navigation_position(widget)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._;

	local key = "navigation_position";
	local original_uuid = aplt.preferences:_value(key);

	local positions = ros.curmap_get_positions();
	if (#positions == 0) then
		gui2.show_message("", _("Need to set the current map, or have at least one position on the map"));
		return;
	end

	local initial = nposm;
	local items = {};

	local label;
	for k, v in ipairs(positions) do
		if original_uuid == v["uuid"] then
			initial = k - 1;
		end
		label = v["name"]; 
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

	local new_position = positions[cursel + 1];

	-- rose.cpp_breakpoint();

	widget:set_label(new_position["name"]);
	aplt.preferences:_set_value(key, new_position["uuid"]);
end

function aplt_leagor_basic__home.get_navigation_uuid(widget)
	local aplt = aplt_leagor_basic;
	local _ = aplt._;

	local label = widget:label();
	if (#label == 0) then
		gui2.show_message(nil, _("Set the position you want to navigate to"));
		return "";
	end

	local key = "navigation_position";
	return aplt.preferences:_value(key);
end

function aplt_leagor_basic__home.click_navigation_goal(widget)
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local uuid = this.get_navigation_uuid(var.navigation_position_);
	if (#uuid == 0) then
		return;
	end

	widget:navigation_start(uuid, "did_navigation_goal", "1a2b3c");
end

function aplt_leagor_basic__home.did_navigation_goal(result, pin, widget)
	local this = aplt_leagor_basic__home;
	local aplt = aplt_leagor_basic;
	local var = this.var;

	if result then
		this.counter = this.counter + 1;
	else
		this.counter = this.counter - 1;
	end
	var.navigation_counter_:set_label(tostring(this.counter));
end

function aplt_leagor_basic__home.click_spark_version(widget)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local key = "spark_version";
	local original_version = aplt.preferences:_value(key) or "";
	-- local items = {{"V1.5", 0}, {"V2.0", 1}};
	local items = {{"V2.0", 0}};

	local initial = nposm;
	for k, v in ipairs(items) do
		if v[1] == original_ver then
			initial = v[2];
			break;
		end
	end

	local x, y, width, height = widget:rect();
	local cursel = gui2.show_menu(x, y + height + 16 * gui2k.hdpi_scale, items, initial);
	if cursel == nposm then
		return;
	end

	local new_version = items[cursel + 1][1];
	widget:set_label(new_version);
	aplt.preferences:_set_value(key, new_version);
end

function aplt_leagor_basic__home.did_toggle_changed(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	assert(cfg.key ~= nil and #cfg.key > 0);

	local val = widget:get_value();

	aplt.preferences:_set_value(cfg.key, val);
end

function aplt_leagor_basic__home.did_spark_string_changed(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	assert(cfg.key ~= nil and #cfg.key > 0);
	local label = widget:label();

	-- aplt.preferences:_set_value(cfg.key, label);
end

function aplt_leagor_basic__home.did_iot_deviceid_changed(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	assert(cfg.key ~= nil and #cfg.key > 0);
	local label = widget:label();
end

function aplt_leagor_basic__home.did_ai_text_changed(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	assert(cfg.key ~= nil and #cfg.key > 0);
	local label = widget:label();
end

function aplt_leagor_basic__home.click_iot_src(widget, cfg)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	assert(cfg.at ~= nil and cfg.at >= 0 and cfg.at < 3);

	local original_code = var.iot_src[cfg.at + 1];

	local sources = var.iot_sources;
	if (#sources == 0) then
		gui2.show_message("", _("No supported iot source"));
		return;
	end

	local initial = nposm;
	local items = {};

	local label;

	for k, v in ipairs(sources) do
		if original_code == v["code"] then
			initial = k - 1;
		end
		label = v["name"]; 
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

	local new_source = sources[cursel + 1];
	var.iot_src[cfg.at + 1] = new_source["code"];
	-- rose.cpp_breakpoint();

	widget:set_label(new_source["name"]);
	-- aplt.preferences:_set_value(key, new_position["uuid"]);
end

function aplt_leagor_basic__home.did_spark_maxtoken_changed(widget)
	local aplt = aplt_leagor_basic;
	local this = aplt_leagor_basic__home;
	local var = this.var;

	local label = widget:label();
	if #label == 0 then
		return;
	end

	local maxtoken = tonumber(label);
	if maxtoken == nil then
		return;
	end

	if maxtoken < 0 then
		if maxtoken ~= -1 then
			return;
		end
	elseif maxtoken < this.MIN_SPARK_MAXTOKEN then
		return;
	end

	aplt.preferences:_set_value("spark_maxtoken", maxtoken);
end

function aplt_leagor_basic__home.timer_handler(now)
	local this = aplt_leagor_basic__home;
	local var = this.var;
	local _ = aplt_leagor_basic._
end