-- reserved table: files, preferences_def
aplt_leagor_basiclua = {
	files = {
		"home.lua",
	};

	preferences_def = {
	};

	-- DLG_xxx value must >= 1
	DLG_HOME = 1;
};

function aplt_leagor_basiclua.start()
	local aplt = aplt_leagor_basiclua;
	local _ = aplt._;

	local windows = {
		home = aplt.DLG_HOME,
	};

	local launcher = aplt.DLG_HOME;
	return windows, launcher;
end

function aplt_leagor_basiclua._(msgid)
	return rose.gettext(aplt_leagor_basiclua.GETTEXT_DOMAIN, msgid) or "";
end

function aplt_leagor_basiclua.vgettext2(msgid, symbols)
	return rose.vgettext2(aplt_leagor_basiclua.GETTEXT_DOMAIN, msgid, symbols) or "";
end