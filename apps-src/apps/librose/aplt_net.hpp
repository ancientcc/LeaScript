#ifndef APLT_NET_HPP_INCLUDED
#define APLT_NET_HPP_INCLUDED

#include "rose_config.hpp"
#include "sdl_utils.hpp"
#include "rose_version.hpp"
#include "aplt.hpp"
#include "rose_net_api.hpp"

#include <json/json.h>
#include <net/url_request/url_request_http_job_rose.hpp>

namespace aplt {

class twkoevent;

extern const std::map<int, std::string> material_types;

struct tcswamp_material
{
public:
	tcswamp_material()
	{
		clear();
	}

	bool valid() const { return !file.empty() && type != nposm && time != nposm && fsize > 0; }
	bool valid2() const { return valid() && utils::is_uuid(uuid, false); }

	void clear()
	{
		file.clear();
		type = nposm;
		desc.clear();
		uuid.clear();
		time = nposm;
		fsize = nposm;
	}

public:
	std::string file;
	int type;
	std::string desc;
	std::string uuid;
	int64_t time;
	int fsize;
};

struct tcswamp_user 
{
public:
	tcswamp_user()
	{
		clear();
	}

	bool valid() const { return uid > 0 && !username.empty(); }

	void clear()
	{
		uid = nposm;
		username.clear();
		materials.clear();
	}

public:
	int64_t uid;
	std::string username;
	std::vector<tcswamp_material> materials;
};

}

namespace net {

void initial();

extern const int max_vericode_size;
extern const int vericode_reserve_secs;
bool check_vericode_size(const int size);

extern std::map<int, tcode3> cswamp_login_usernametypes;
int cswamp_login_usernametype_from_str(const std::string& str);
int cswamp_username_type(bool keep_nposm = false);

struct truser
{
public:
	truser();

	bool valid() const { return !deviceid.empty() && !sessionid.empty() && uid >= 0; }
	void clear()
	{
		sessionid.clear();
		uid = nposm;
		expire_time = nposm;
	}

	void did_logout();

public:
	std::string username;
	std::string mobile;
	std::string pwcookie;
	std::string deviceid;

	std::string sessionid;
	int64_t uid;
	int64_t expire_time;

	int keepalive; // unit: second
};

// direct xmit width agbox
bool cswamp_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, 
	const std::function<void (int, const std::string&, const std::string&)>& did_applet_will_uninstall, aplt::tapplet& aplt, bool quiet);
bool cswamp_findapplet(gui2::tprogress_& progress, const std::string& sessionid, int app, 
	int maxapplets, const std::string& bundleids, const std::string& name, bool quiet, std::vector<aplt::tapplet>& result);

struct tcswamp_finduser_result
{
	tcswamp_finduser_result()
	{
		clear();
	}

	tcswamp_finduser_result(int64_t uid, const std::string& username, int materialcount)
		: uid(uid)
		, username(username)
		, materialcount(materialcount)
	{}

	bool valid() const { return uid > 0 && !username.empty() && materialcount >= 0; }

	void clear()
	{
		uid = nposm;
		username.clear();
		materialcount = 0;
	}

	int64_t uid;
	std::string username;
	int materialcount;
};
bool cswamp_finduser(gui2::tprogress_& progress,
	int maximum, const std::string& name, bool quiet, std::vector<net::tcswamp_finduser_result>& result);
bool cswamp_getuserinfo(gui2::tprogress_& progress, int64_t uid, bool quiet, aplt::tcswamp_user& result);

bool do_uploadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& src, const std::string& uuid, const std::string& disk_filename, bool quiet);
bool do_downloadfile(gui2::tprogress_& progress, const version_info& curr_version, const std::string& sessionid, 
	const std::string& filetype, const std::string& src, const std::string& uuid, int64_t uid, const std::string& disk_filename, bool quiet, int* ver_result_ptr);

enum {cswamp_vericodetype_register, cswamp_vericodetype_resetpwd, cswamp_vericodetype_eraseuser, cswamp_vericodetype_count};
bool cswamp_vericode(gui2::tprogress_& progress, const std::string& mobile, int type,
	int timeout, bool quiet);

enum {cswamp_login_usernametype_mobile, cswamp_login_usernametype_username, cswamp_login_usernametype_count};
enum {cswamp_login_passwordtype_password, cswamp_login_passwordtype_cookie, cswamp_login_passwordtype_count};
struct tcswamp_login_result
{
	std::string sessionid;
	std::string pwcookie;
	int64_t uid;
	int64_t expire_time;

	std::string username;
	std::string mobile;
};

bool cswamp_register2(truser& user, const std::string& username, const std::string& password, const std::string& vericode,
	int timeout, bool quiet);

bool cswamp_login(gui2::tprogress_& progress, int username_type, int password_type, const std::string& username, const std::string& password, 
	const std::string& deviceid, const std::string& devicename, int timeout, bool quiet, tcswamp_login_result& result);
bool cswamp_login2(truser& user, int username_type, int password_type, const std::string& username, const std::string& password, 
	int timeout, bool quiet);
void cswamp_login3(truser& user);

bool cswamp_resetpwd2(truser& user, const std::string& mobile, const std::string& vericode, const std::string& password, int timeout, bool quiet);
bool cswamp_updateusername2(truser& user, const std::string& username, int timeout, bool quiet);
bool cswamp_eraseuser2(truser& user, const std::string& vericode, int timeout, bool quiet);

bool cswamp_logout(gui2::tprogress_& progress, const std::string& sessionid, bool quiet);
bool cswamp_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet);

enum {cswamp_evttype_fan_to_courseup, cswamp_evttype_courseup_to_fan, cswamp_evttype_courseup_to_fans, cswamp_evttype_count};
bool cswamp_addevent(gui2::tprogress_& progress, const std::string& sessionid, int64_t ts, int type, int64_t receiver_uid, const Json::Value& json_event, int image_format, const std::vector<timage_pair>& images, bool quiet);
bool cswamp_addevent2(truser& user, int64_t ts, int type, int64_t receiver_uid, const Json::Value& json_event, int image_format, const std::vector<timage_pair>& images, bool quiet);
bool cswamp_getevent(gui2::tprogress_& progress, const std::string& sessionid, int64_t event_time, std::set<aplt::twkoevent>& events, int& count, bool quiet);
bool cswamp_keepalive(gui2::tprogress_& progress, const std::string& sessionid, bool quiet, int64_t& event_time);

bool cswamp_querytablecooking(gui2::tprogress_& progress, const std::string& sessionid, int table, tcswamp_table_result& result, bool quiet);

bool upload_rsp(int zip_type, const std::string& sessionid, const std::string& remote_src, const std::string& uuid, const std::string& disk_filename);
bool upload_materialrsp(const std::string& sessionid, const std::string& remote_src, const std::string& uuid, const std::string& disk_filename);

bool upgrade_app(const std::string& remote_src, const version_info& curr_version, const version_info& new_version, const std::function<void()>& pre_SDL_UpdateApp, bool reboot);
bool download_pinyin_or_latexrsp(int zip_type, const version_info& curr_version, const version_info& new_version);
bool download_materialrsp(const std::string& remote_src, const std::string& uuid, int64_t uid, const std::string& rspfile);

}

#endif