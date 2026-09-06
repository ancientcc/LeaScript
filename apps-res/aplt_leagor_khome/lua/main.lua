-- reserved table: files, preferences_def
aplt_leagor_khome = {
	files = {
		"face.lua",
		"home.lua",
		"posture.lua",
		"query.lua",
		"restaurant.lua",
	};

	user_data_dirs = {
		"saves/facestore",
	};

	def_alias_id = "TB";

	preferences_def = {
		alias_id = "TB",

		-- sf <=> Sit Front
		sf_shoulder_width_min = 0.25,
		sf_shoulder_width_max = 0.42,
		sf_ear_width_min = 0.1,
		sf_ear_diff_threshold = 0.05,
		sf_head_forward_threshold = 0.25,

		-- ss <==> Sit Side
		ss_spine_length_min = 0.25,
		ss_spine_length_max = 0.60,
		ss_shoulder_width_max = 0.13,
		ss_improper_spine_angle_min = 268,
		ss_improper_spine_angle_max = 280,
		ss_shoulder_nose_angle_threshold = 45,

		-- sshare <==> Sit share
		sshare_improper_time = 8, -- second
		sshare_noperson_time = 120, -- second
		sshare_sedentary_time = 30, -- minute
	};

	cpp_id_save_sf_6fields = rosek.cpp_id_aplt_min;
	cpp_id_save_ss_4fields = rosek.cpp_id_aplt_min + 1;
	cpp_id_save_sshare_3fields = rosek.cpp_id_aplt_min + 2;

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
	DLG_QUERY = 2;
	DLG_RESTAURANT = 3;
	DLG_POSTURE = 4;
	DLG_FACE = 5;
};

function aplt_leagor_khome.start()

	-- rose.log("aplt_leagor_khome.start()---");

	local aplt = aplt_leagor_khome;
	local _ = aplt._

	-- if ''aplt_leagor_khome__cpp == nil', means loading libroseaplalt.so failed.
	if (aplt_leagor_khome__cpp ~= nil) then
		aplt.cpp_block2_ = aplt_leagor_khome__cpp.cpp_block2();
		aplt.cpp_camera2_ = aplt_leagor_khome__cpp.cpp_camera2();
	end

	local windows = {
		home = aplt.DLG_HOME,
		query = aplt.DLG_QUERY,
		restaurant = aplt.DLG_RESTAURANT,
		posture = aplt.DLG_POSTURE,
		face = aplt.DLG_FACE,
	};

	aplt.msgstrs = {
		_("task^query"),
		_("task^query_cooking"),
		_("task^parse_time"),
		_("task^snapshot"),
		_("task^kpose"),
		-- _("task^workout"),
		_("task^kface"),
		_("task^recognition"),
	};

	local launcher = aplt.DLG_HOME;
	return windows, launcher;
end

function aplt_leagor_khome._(msgid)
	return rose.gettext(aplt_leagor_khome.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_leagor_khome.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_leagor_khome.GETTEXT_DOMAIN, msgid, symbols) or "";
end