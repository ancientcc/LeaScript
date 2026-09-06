-- reserved table: files, preferences_def
aplt_nlsd_basic = {
	files = {
		"home.lua",
	};

	preferences_def = {
		base_product = "",
		base_serial = "",
		base_serial_baudrate = 115200,

		longpress_threshold = 20,
		power_threshold = 40,
		non_power_threshold = 40,
	};

	cpp_id_driver_layer = rosek.cpp_id_aplt_min;

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_nlsd_basic.start()
	local aplt = aplt_nlsd_basic;
	local _ = aplt._;

	-- if ''aplt_nlsd_basic__cpp == nil', means loading libroseaplalt.so failed.
	if (aplt_nlsd_basic__cpp ~= nil) then
		aplt.cpp_lua_block_ = aplt_nlsd_basic__cpp.cpp_lua_block();
	end

	local windows = {
		home = aplt.DLG_HOME,
	};

	aplt.msgstrs = {
		_("task^turn_off"),
	};

	local launcher = aplt.DLG_HOME;
	return windows, launcher;
end

function aplt_nlsd_basic._(msgid)
	return rose.gettext(aplt_nlsd_basic.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_nlsd_basic.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_nlsd_basic.GETTEXT_DOMAIN, msgid, symbols) or "";
end