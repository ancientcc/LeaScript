-- reserved table: files, preferences_def
aplt_leagor_basic = {
	files = {
		"home.lua",
	};

	preferences_def = {
		laser_product = "",
		laser_serial = "",
		laser_serial_baudrate = 115200,

		base_product = "",
		base_serial = "",
		base_serial_baudrate = 115200,

		moveit_serial = "",
		moveit_serial_baudrate = 115200,

		navigation_position = "",

		yahboom_motor_threshold = 400,

		voice_threshold = (rose.os() == rosek.os_windows) and 7500 or 4000,
		-- xfyun's spark
		spark_version = "V2.0",
		spark_appid = "",
		spark_apisecret = "",
		spark_apikey = "",
		spark_maxtoken = 50,

		disable_recognition = false,
		disable_question = false,
		goaling_enable = false,

		-- iot
		tuya_client_id = "",
		tuya_client_secret = "",
		iot_deviceid1 = "",
		iot_deviceid2 = "",
		iot_deviceid3 = "",

		-- ai driver
		ds_api_key = "",
	};

	positions = {};

	cpp_id_save_xfyun_3fields = rosek.cpp_id_aplt_min;
	cpp_id_save_iot_5fields = rosek.cpp_id_aplt_min + 1;
	cpp_id_save_ai_1fields = rosek.cpp_id_aplt_min + 2;

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_leagor_basic.start()
	local aplt = aplt_leagor_basic;
	local _ = aplt._;

	local windows = {
		home = aplt.DLG_HOME,
	};

	aplt.msgstrs = {
		_("task^grasp"),
		_("task^press_top"),
		_("task^press_middle"),
		_("task^move2_task"),
		_("task^exe_task_only"),
		_("task^speak_var"),
		_("task^modify_env_var"),
		_("task^alert"),
		_("task^privacy"),
		_("task^base_scene"),
		_("task^timed_task"),
		_("task^charge"),
		_("task^add_timed_reminder"),
		_("task^exam"),
	};

	local launcher = aplt.DLG_HOME;
	return windows, launcher;
end

function aplt_leagor_basic._(msgid)
	return rose.gettext(aplt_leagor_basic.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_leagor_basic.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_leagor_basic.GETTEXT_DOMAIN, msgid, symbols) or "";
end