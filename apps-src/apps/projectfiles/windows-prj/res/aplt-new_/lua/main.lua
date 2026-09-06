-- reserved table: files, preferences_def
aplt_leagor_basic = {
	files = {
		"home.lua",
	};

	preferences_def = {
	};

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_leagor_basic.start()
	local aplt = aplt_leagor_basic;
	local _ = aplt._;

	local windows = {
		home = aplt.DLG_HOME,
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