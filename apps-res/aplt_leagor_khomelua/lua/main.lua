-- reserved table: files, preferences_def
aplt_leagor_khomelua = {
	files = {
		"home.lua",
	};

	preferences_def = {
	};

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_leagor_khomelua.start()
	local aplt = aplt_leagor_khomelua;
	local _ = aplt._;

	local windows = {
		home = aplt.DLG_HOME,
	};

	aplt.msgstrs = {
		-- _("task^workout"),
	};

	local launcher = aplt.DLG_HOME;
	return windows, launcher;
end

function aplt_leagor_khomelua._(msgid)
	return rose.gettext(aplt_leagor_khomelua.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_leagor_khomelua.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_leagor_khomelua.GETTEXT_DOMAIN, msgid, symbols) or "";
end