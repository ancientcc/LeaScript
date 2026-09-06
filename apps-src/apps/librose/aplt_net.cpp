#define GETTEXT_DOMAIN "rose-lib"

#include "aplt_net.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"
#include "rose_version.hpp"
#include "base_instance.hpp"
#include "SDL_image.h"
#include "chinese.hpp"

#include <net/base/port_util.h>
#include <net/base/upload_data_stream.h>
#include <iomanip>
#include <openssl/sha.h>


#include "minizip/minizip.hpp"

#define ERRCODE_LOW_VERSION		-2
#define ERRCODE_NO_MOBILE		-3
#define ERRCODE_PASSWORD_ERROR	-7
#define ERRCODE_PERSON_EXISTED	-8
#define ERRCODE_REMOTE_LOGIN	-9

#define TIMEOUT_NORMAL	5000 // 3500
#define TIMEOUT_6S		6000
#define TIMEOUT_10S		10000
#define TIMEOUT_15S		15000
#define TIMEOUT_30S		30000
#define TIMEOUT_45S		45000

// const tuser null_user2;
// tuser current_user;

static int invalid_faceid_in_villageserver = 0;


const std::map<int, std::string> material_types {
	{rspmaterialtype_courseware, "courseware"},
};

static int material_type_from_id(const std::string& id)
{
	VALIDATE(!material_types.empty(), null_str);

	for (std::map<int, std::string>::const_iterator it = material_types.begin(); it != material_types.end(); ++ it) {
		const std::string& _id = it->second;
		if (_id == id) {
			return it->first;
		}
	}
	return nposm;
}

namespace net {
// int agbox_minus_local = 0;

truser::truser()
	: keepalive(10)
	, uid(nposm)
{}

void truser::did_logout()
{
	sessionid.clear();
	uid = nposm;

	pwcookie.clear();
	preferences::set_login_pwcookie(null_str);
}

static std::string generic_handle_response(const int code, Json::Value& json_object, bool& handled)
{
	std::stringstream err;
	utils::string_map symbols;
	
	handled = false;
/*
	if (code == ERRCODE_NO_MOBILE) {
		err << _("Mobile isn't existed");

	} else if (code == ERRCODE_REMOTE_LOGIN) {
		Json::Value& timestamp = json_object["timestamp"];

		symbols["time"] = format_time_date(timestamp.asInt64());
		err << vgettext2("Your account is logined at $time in different places. If it is not your operation, your password has been leaked. Please as soon as possible to modify the password.", symbols);
		current_user.sessionid.clear();

		handled = true;

	} else */ if (code == ERRCODE_PASSWORD_ERROR) {
		err << _("Password error");
		handled = true;

	} else if (code == ERRCODE_LOW_VERSION) {
		symbols["app"] = game_config::get_app_msgstr(null_str);
		err << vgettext2("The version is too low, please upgrade $app.", symbols);
		handled = true;
		// as far not login, so sessionid is invalid. can not do_reporterror.
	}

	return err.str();
}


static std::string form_url(const std::string& category, const std::string& task)
{
	std::stringstream url;

	url << "http://" << game_config::bbs_server.host;
	url << ":" << game_config::bbs_server.port;
	url << game_config::bbs_server.url << category << "/";
	url << task;

	return url.str();
}

static std::string form_url2(const std::string& subpath)
{
	std::stringstream url;

	url << "http://" << "www.cswamp.com";
	url << ":" << 80;
	url << "/" << subpath;

	return url.str();
}


//
// cswamp
//
class ihttp_cswamp
{
public:
	explicit ihttp_cswamp(gui2::tprogress_& _progress, const std::string& _url, bool _quiet)
		: progress(_progress)
		, url(_url)
		, quiet(_quiet)
		, nonce(nposm)
		, disable_new_aplt_lock_(aplt::tdisable_new_klink_task_lock::reason_netxmit)
	{}

	virtual void verify_param() {}
	virtual int timeout_ms() const { return nposm; }
	virtual bool pre(Json::Value& json_params) const { return true; }
	virtual bool pre_binary(Json::Value& json_params, tuint8data2_C& buf) const { return true; }
	// if has error, return value is error-string, else return empty.
	virtual std::string post(Json::Value& results) { return null_str; }
	virtual std::string post_binary(const uint8_t* data, int size) { return null_str; }

public:
	gui2::tprogress_& progress;
	const std::string url;
	bool quiet;
	int nonce;
	aplt::tdisable_new_klink_task_lock disable_new_aplt_lock_;
};

bool cswamp_did_pre_agbox(net::HttpRequestHeaders& headers, std::string& body, const ihttp_cswamp& entity, bool with_binary)
{
	Json::Value json_root;
	json_root["version"] = game_config::rose_version.str(true);
	json_root["nonce"] = entity.nonce == nposm? rand(): entity.nonce;

	if (!with_binary) {
		if (!entity.pre(json_root)) {
			return false;
		}

		Json::FastWriter writer;
		body = writer.write(json_root);

	} else {
		tuint8data2_C buf;
		memset(&buf, 0, sizeof(tuint8data2_C));

		if (!entity.pre_binary(json_root, buf)) {
			if (buf.ptr != nullptr) {
				free(buf.ptr);
			}
			return false;
		}

		//
		Json::FastWriter writer;
		const std::string json_str = writer.write(json_root);
		const int prefix_size = 4;

		const size_t size = prefix_size + json_str.size() + buf.vsize + SHA_DIGEST_LENGTH;
		char* buf2 = (char*)malloc(size);

		const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
		memcpy(buf2, &json_str_size_bg, prefix_size);
		memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());

		if (buf.vsize != 0) {
			memcpy(buf2 + prefix_size + json_str.size(), buf.ptr, buf.vsize);

			// sha1
			std::unique_ptr<uint8_t[]> md = utils::sha1((const uint8_t*)buf.ptr, buf.vsize);
			memcpy(buf2 + prefix_size + json_str.size() + buf.vsize, md.get(), SHA_DIGEST_LENGTH);
		}
		body.assign(buf2, size);

		if (buf.ptr != nullptr) {
			free(buf.ptr);
		}
		free(buf2);
	}

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool cswamp_did_post_agbox(const net::RoseDelegate& delegate, int status, gui2::tprogress_& progress, bool quiet, ihttp_cswamp& entity)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!quiet && !progress.task_cancelled()) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			utils::string_map symbols;
			symbols["url"] = entity.url;
			err << vgettext2("$url   Response is invalid json foramt", symbols);
		} else {
			if (game_config::os == os_windows) {
				tfile file(game_config::preferences_dir + "/4.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
			Json::Value& code_json = json_object["code"];
			int code = code_json.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					GURL gurl(entity.url);
					utils::string_map symbols;
					// symbols["url"] = entity.url;
					symbols["method"] = gurl.ExtractFileName();
					std::string msg = json_object["msg"].asString();
					if (msg.empty()) {
						msg = std::string("Empty error-msg, code: ") + str_cast(code);
					}
					symbols["msg"] = msg;
					err << vgettext2("method: $method\n$msg", symbols);
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						err << entity.post(results);
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool cswamp_did_post_with_binary(const net::RoseDelegate& delegate, int status, gui2::tprogress_& progress, ihttp_cswamp& entity)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		if (!entity.quiet && !progress.task_cancelled()) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			utils::string_map symbols;
			symbols["url"] = entity.url;
			err << vgettext2("$url   Response is invalid json foramt", symbols);
		} else {
			if (game_config::os == os_windows) {
				tfile file(game_config::preferences_dir + "/4.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
			Json::Value& code_json = json_object["code"];
			int code = code_json.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << "login, Unknown error. code: " << code;
					}
				} else {
					Json::Value& results = json_object["results"];
					if (results.isObject()) {
						err << entity.post(results);

						if (err.str().empty()) {
							const uint8_t* binary = (const uint8_t*)content + prefix_size + json_str.size();
							int binary_size = content_size - prefix_size - json_size;
							if (binary_size <= SHA_DIGEST_LENGTH) {
								err << _("SHA1 fail. not enoth bytes for SHA1 verify");
							} else {
								binary_size -= SHA_DIGEST_LENGTH;
							}
							if (err.str().empty()) {
								std::unique_ptr<uint8_t[]> md = utils::sha1(binary, binary_size);
								if (memcmp(binary + binary_size, md.get(), SHA_DIGEST_LENGTH) != 0) {
									err << _("SHA1 fail. verify data fail");
								}
							}
							if (err.str().empty()) {
								err << entity.post_binary(binary, binary_size);
							}
						}
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (!entity.quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool cswamp_do_agbox(gui2::tprogress_& progress, ihttp_cswamp& entity, bool req_with_binary, bool resp_with_binary, int app_received_bytes = 0, int app_expected_bytes = 0)
{
	entity.verify_param();
	std::string request_json;

	int timeout = entity.timeout_ms();
	net::thttp_agent agent(entity.url, "POST", null_str, timeout == nposm? TIMEOUT_6S: timeout);
	agent.app_received_bytes = app_received_bytes;
	agent.app_expected_bytes = app_expected_bytes;
	agent.did_pre = std::bind(&cswamp_did_pre_agbox, _2, _3, std::ref(entity), req_with_binary);
	if (!resp_with_binary) {
		agent.did_post = std::bind(&cswamp_did_post_agbox, _2, _3, std::ref(progress), entity.quiet, std::ref(entity));
	} else {
		agent.did_post = std::bind(&cswamp_did_post_with_binary, _2, _3, std::ref(progress), std::ref(entity));
	}

	return net::handle_http_request(agent);
}

// cswamp/device/getapplet
class thttp_cswamp_device_getapplet: public ihttp_cswamp
{
public:
	thttp_cswamp_device_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, const std::function<void (int, const std::string&, const std::string&)>& did_applet_will_uninstall, ::aplt::tapplet& applet, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/getapplet"), quiet)
		, type_(type)
		, bundleid_(bundleid)
		, did_applet_will_uninstall_(did_applet_will_uninstall)
		, applet_(applet)
		, timeout_(timeout)
		, quiet_(false)
		, iconfsize_(nposm)
		, rspfsize_(nposm)
	{
		applet_.clear();
	}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;
	std::string post_binary(const uint8_t* data, int size) override;

private:
	int type_;
	const std::string bundleid_;
	const std::function<void (int, const std::string&, const std::string&)>& did_applet_will_uninstall_;
	::aplt::tapplet& applet_;
	const int timeout_;
	const bool quiet_;
	int iconfsize_;
	int rspfsize_;
};

void thttp_cswamp_device_getapplet::verify_param()
{
	VALIDATE(type_ == aplt::src_distribution || type_ == aplt::src_development, null_str);
	VALIDATE(!bundleid_.empty(), null_str);
}

bool thttp_cswamp_device_getapplet::pre(Json::Value& json_params) const
{
	json_params["type"] = aplt::sources.find(type_)->second;
	json_params["bundleid"] = bundleid_;

	return true;
}

std::string thttp_cswamp_device_getapplet::post(Json::Value& results)
{
	const std::string name2 = results["title"].asString();
	applet_.name = results["name"].asString();
	VALIDATE(name2 == applet_.name, null_str);
	applet_.subtitle = results["subtitle"].asString();
	applet_.ts = results["time"].asInt64();
	applet_.username = results["username"].asString();
	applet_.app = results["app"].asString();

	// now, binary hasn't icon, so iconfsize_ is 0 always.
	iconfsize_ = results["iconfsize"].asInt();
	rspfsize_ = results["rspfsize"].asInt();
	return null_str;
}

std::string thttp_cswamp_device_getapplet::post_binary(const uint8_t* data, int size)
{
	if (size != iconfsize_ + rspfsize_) {
		return _("binary data length error");
	}
	if (rspfsize_ <= RSP_HEADER_BYTES + SHA_DIGEST_LENGTH) {
		return _("rsp data length is too short");
	}
/*
	{
		SDL_RWops* src = SDL_RWFromMem((void*)(data), iconfsize_);
		applet_.icon = IMG_Load_RW(src, 0);
		SDL_RWclose(src);
		applet_.icon = makesure_neutral_surface(applet_.icon);
	}
*/
	const uint8_t* rsp_data = data + iconfsize_;
	if (!utils::verify_sha1(rsp_data, rspfsize_ - SHA_DIGEST_LENGTH, rsp_data + rspfsize_ - SHA_DIGEST_LENGTH)) {
		return _("Verify rsp data error");
	}
	memcpy(&applet_.rspheader, rsp_data, RSP_HEADER_BYTES);

	const std::string temp_aplt_dl_zip = "__tmp_aplt.zip";
	const std::string aplt_zip = game_config::preferences_dir + "/" + temp_aplt_dl_zip;
	{
		tfile file(aplt_zip, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, rsp_data + RSP_HEADER_BYTES, rspfsize_ - RSP_HEADER_BYTES - SHA_DIGEST_LENGTH);
	}

	const std::string temp_aplt_dl_dir = "__tmp_aplt_dl";
	const std::string dst_path = game_config::preferences_dir + "/" + temp_aplt_dl_dir;
	
	SDL_DeleteFiles(dst_path.c_str());
	// gui2::delete_file(progress, dst_path); 
	minizip::unzip_file(aplt_zip, dst_path.c_str(), null_str, null_str);

	aplt::get_settings_cfg(dst_path, false, applet_);
	if (applet_.bundleid.empty()) {
		return _("Applet's settings.cfg is invalid");
	}

	// rose version
	const trsp_header& rspheader = applet_.rspheader;
	int major = posix_hi8(posix_lo16(applet_.rspheader.rose_version));
	int minor = posix_lo8(posix_hi16(applet_.rspheader.rose_version));
	int revision_level = posix_hi8(posix_hi16(applet_.rspheader.rose_version));
	const std::string special = str_cast(applet_.rspheader.manufactor);
	version_info rose_ver(major, minor, revision_level, true, '-', special);
	if (!rose_ver.is_rose_recommended()) {
		return _("Applet's rose version is invalid");
	}

	const std::string lua_bundleid = utils::replace_all(applet_.bundleid, ".", "_");
	const std::string preferences_aplt_path = game_config::preferences_dir + "/" + lua_bundleid;

	if (did_applet_will_uninstall_ != NULL) {
		did_applet_will_uninstall_(type_, applet_.bundleid, preferences_aplt_path);
	}

	SDL_DeleteFiles(preferences_aplt_path.c_str());
	// gui2::delete_file(progress, preferences_aplt_path);

	SDL_RenameFile(dst_path.c_str(), lua_bundleid.c_str());

	applet_.set_id(type_, applet_.bundleid);
	applet_.set(applet_.name, applet_.subtitle, applet_.username, applet_.ts, 
		preferences_aplt_path, preferences_aplt_path + "/" + APPLET_ICON, rose_ver.str(true));
	aplt_set_msgstr(applet_);
	aplt_set_pinyin(applet_);
	applet_.fill_input_output_vars();

	applet_.write_distribution_cfg();

	// clear old t_string and image cache.
	t_string::reset_translations();
	image::flush_cache();
	return null_str;
}

bool cswamp_getapplet(gui2::tprogress_& progress, int type, const std::string& bundleid, 
	const std::function<void (int, const std::string&, const std::string&)>& did_applet_will_uninstall, ::aplt::tapplet& applet, bool quiet)
{
	thttp_cswamp_device_getapplet entity(progress, type, bundleid, did_applet_will_uninstall, applet, TIMEOUT_10S, quiet);
	return cswamp_do_agbox(progress, entity, false, true);
}


// cswamp/device/findapplet
class thttp_cswamp_device_findapplet: public ihttp_cswamp
{
public:
	thttp_cswamp_device_findapplet(gui2::tprogress_& progress, const std::string& sessionid, int app, int maxapplets, const std::string& bundleids, const std::string& name, bool quiet, std::vector<aplt::tapplet>& result)
		: ihttp_cswamp(progress, form_url2("cswamp/device/findapplet"), quiet)
		, sessionid_(sessionid)
		, app_(app)
		, maxapplets_(maxapplets)
		, bundleids_(bundleids)
		, name_(name)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
		, result_(result)
	{
		result_.clear();
	}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int app_;
	const int maxapplets_;
	const std::string bundleids_;
	const std::string name_;
	const int timeout_;
	const bool quiet_;
	std::vector<aplt::tapplet>& result_;
};

void thttp_cswamp_device_findapplet::verify_param()
{
	if (!sessionid_.empty()) {
		VALIDATE(aplt::apps.count(app_) != 0, null_str);
		VALIDATE(maxapplets_ == nposm, null_str);
		VALIDATE(bundleids_.empty(), null_str);
		VALIDATE(name_.empty(), null_str);

	} else {
		VALIDATE(app_ == nposm || aplt::apps.count(app_) != 0, null_str);
		VALIDATE(maxapplets_ == nposm || maxapplets_ > 0, null_str);
		VALIDATE(!bundleids_.empty() || !name_.empty(), null_str);
	}
}

bool thttp_cswamp_device_findapplet::pre(Json::Value& json_params) const
{
	if (app_ != nposm) {
		json_params["app"] = aplt::apps.find(app_)->second;
	}

	if (!sessionid_.empty()) {
		json_params["sessionid"] = sessionid_;

	} else {
		if (maxapplets_ != nposm) {
			json_params["maxapplets"] = maxapplets_;
		}
		if (!bundleids_.empty()) {
			json_params["bundleids"] = bundleids_;
		}
		if (!name_.empty()) {
			json_params["name"] = name_;
		}
	}

	return true;
}

std::string thttp_cswamp_device_findapplet::post(Json::Value& results)
{
	int count = results["count"].asInt();
	if (results.isObject()) {
		Json::Value& applets = results["applets"];
		const std::string GETAPPLET_PATH = "FAKE_PATH";
		const std::string icon;
		if (applets.isArray()) {
			aplt::tapplet tmp;
			for (int at = 0; at < (int)applets.size(); at ++) {
				tmp.clear();
				const Json::Value& item = applets[at];
				const std::string bundleid = item["bundleid"].asString();
				const std::string name = item["name"].asString();
				const std::string subtitle = item["subtitle"].asString();
				const int64_t ts = item["time"].asInt64();
				const std::string username = item["username"].asString();
				const int rspfsize = item["rspfsize"].asInt64();
				const std::string version = item["version"].asString();
				const std::string roseversion = item["roseversion"].asString();
				const std::string app = item["app"].asString();
				tmp.set_id(aplt::src_distribution, bundleid);
				tmp.set(name, subtitle, username, ts, GETAPPLET_PATH, icon, roseversion);
				tmp.version = version;
				tmp.app = app;
				if (tmp.valid()) {
					result_.push_back(tmp);
				}
			}
		}
	}
	return null_str;
}

bool cswamp_findapplet(gui2::tprogress_& progress, const std::string& sessionid, int app, 
	int maxapplets, const std::string& bundleids, const std::string& title, bool quiet, std::vector<aplt::tapplet>& result)
{
	thttp_cswamp_device_findapplet entity(progress, sessionid, app, maxapplets, bundleids, title, quiet, result);
	return cswamp_do_agbox(progress, entity, false, false);
}

// cswamp/device/finduser
class thttp_cswamp_device_finduser: public ihttp_cswamp
{
public:
	thttp_cswamp_device_finduser(gui2::tprogress_& progress, int maximum, const std::string& name, bool quiet, std::vector<net::tcswamp_finduser_result>& result)
		: ihttp_cswamp(progress, form_url2("cswamp/device/finduser"), quiet)
		, maximum_(maximum)
		, name_(name)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
		, result_(result)
	{
		result_.clear();
	}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const int maximum_;
	const std::string name_;
	const int timeout_;
	const bool quiet_;
	std::vector<net::tcswamp_finduser_result>& result_;
};

void thttp_cswamp_device_finduser::verify_param()
{	
	VALIDATE(maximum_ == nposm || maximum_ > 0, null_str);
	// VALIDATE(!name_.empty(), null_str);
}

bool thttp_cswamp_device_finduser::pre(Json::Value& json_params) const
{
	if (maximum_ != nposm) {
		json_params["maximum"] = maximum_;
	}
	if (!name_.empty()) {
		json_params["name"] = name_;
	}

	return true;
}

std::string thttp_cswamp_device_finduser::post(Json::Value& results)
{
	int count = results["count"].asInt();
	if (results.isObject()) {
		Json::Value& users = results["users"];
		if (users.isArray()) {
			net::tcswamp_finduser_result tmp;
			for (int at = 0; at < (int)users.size(); at ++) {
				tmp.clear();

				const Json::Value& item = users[at];
				tmp.uid = item["uid"].asInt64();
				tmp.username = item["username"].asString();
				tmp.materialcount = item["materialcount"].asInt();
				
				if (tmp.valid()) {
					result_.push_back(tmp);
				}
			}
		}
	}
	return null_str;
}

bool cswamp_finduser(gui2::tprogress_& progress,
	int maximum, const std::string& name, bool quiet, std::vector<net::tcswamp_finduser_result>& result)
{
	thttp_cswamp_device_finduser entity(progress, maximum, name, quiet, result);
	return cswamp_do_agbox(progress, entity, false, false);
}


// cswamp/device/getuserinfo
class thttp_cswamp_device_getuserinfo: public ihttp_cswamp
{
public:
	thttp_cswamp_device_getuserinfo(gui2::tprogress_& progress, int64_t uid, bool quiet, aplt::tcswamp_user& result)
		: ihttp_cswamp(progress, form_url2("cswamp/device/getuserinfo"), quiet)
		, uid_(uid)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
		, result_(result)
	{
		result_.clear();
	}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const int64_t uid_;
	const int timeout_;
	const bool quiet_;
	aplt::tcswamp_user& result_;
};

void thttp_cswamp_device_getuserinfo::verify_param()
{	
	VALIDATE(uid_ > 0, null_str);
}

bool thttp_cswamp_device_getuserinfo::pre(Json::Value& json_params) const
{
	json_params["uid"] = uid_;

	return true;
}

std::string thttp_cswamp_device_getuserinfo::post(Json::Value& results)
{
	result_.uid = uid_;
	result_.username = results["username"].asString();
	if (results.isObject()) {
		Json::Value& materials = results["material"];
		if (materials.isArray()) {
			aplt::tcswamp_material tmp;
			for (int at = 0; at < (int)materials.size(); at ++) {
				tmp.clear();

				const Json::Value& item = materials[at];
				tmp.file = item["file"].asString();
				tmp.type = material_type_from_id(item["type"].asString());
				tmp.desc = item["desc"].asString();
				tmp.uuid = item["uuid"].asString();
				tmp.time = item["time"].asInt64();
				tmp.fsize = item["fsize"].asInt();
				
				if (tmp.valid2()) {
					result_.materials.push_back(tmp);
				}
			}
		}
	}

	if (!result_.valid()) {
		result_.clear();
	}
	return null_str;
}

bool cswamp_getuserinfo(gui2::tprogress_& progress, int64_t uid, bool quiet, aplt::tcswamp_user& result)
{
	thttp_cswamp_device_getuserinfo entity(progress, uid, quiet, result);
	return cswamp_do_agbox(progress, entity, false, false);
}


// cswamp/device/uploadfile
class thttp_cswamp_device_uploadfile: public ihttp_cswamp
{
public:
	thttp_cswamp_device_uploadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& file, const std::string& uuid, int fsize, int _nonce, int offset, uint8_t* data, int size, bool end, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/uploadfile"), quiet)
		, sessionid_(sessionid)
		, filetype_(filetype)
		, file_(file)
		, uuid_(uuid)
		, fsize_(fsize)
		, offset_(offset)
		, data_(data)
		, size_(size)
		, end_(end)
		, timeout_(timeout)
		, quiet_(false)
		, size2(nposm)
	{
		VALIDATE(_nonce != nposm, null_str);
		nonce = _nonce;
	}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre_binary(Json::Value& json_params, tuint8data2_C& buf) const;
	std::string post(Json::Value& results) override;
	std::string post_binary(const uint8_t* data, int size) override;

public:
	int size2;

private:
	const std::string sessionid_;
	const std::string filetype_;
	const std::string file_;
	const std::string uuid_;
	const int fsize_;
	const int offset_;
	uint8_t* data_;
	const int size_;
	const bool end_;
	const int timeout_;
	const bool quiet_;
};

void thttp_cswamp_device_uploadfile::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
	VALIDATE(!filetype_.empty(), null_str);
	VALIDATE(!file_.empty(), null_str);
	VALIDATE(fsize_ > 0, null_str);
	VALIDATE(offset_ >= 0, null_str);
	VALIDATE(size_ >= 0, null_str);
	VALIDATE(data_ != nullptr, null_str);
}

bool thttp_cswamp_device_uploadfile::pre_binary(Json::Value& json_params, tuint8data2_C& buf) const
{
	json_params["sessionid"] = sessionid_;
	json_params["filetype"] = filetype_;
	json_params["file"] = file_;
	if (utils::is_uuid(uuid_, false)) {
		json_params["uuid"] = uuid_;
	}
	json_params["fsize"] = fsize_;
	json_params["offset"] = offset_;
	json_params["size"] = size_;
	json_params["end"] = end_;

	utils::resize_uint8data(buf, size_, buf.vsize);
	memcpy(buf.ptr + buf.vsize, data_, size_);

	buf.vsize += size_;

	SDL_Log("[chromium]thttp_cswamp_device_uploadfile::pre_binary, file: %s, offset_: %i, size_: %i", 
		file_.c_str(), offset_, size_);

	return true;
}

std::string thttp_cswamp_device_uploadfile::post(Json::Value& results)
{
	// fsize = results["fsize"].asInt();
	return null_str;
}

std::string thttp_cswamp_device_uploadfile::post_binary(const uint8_t* data, int size)
{
	// caller has executed sha1-verify, size if length that get ride of SHA_DIGEST_LENGTH.
	if (size == 0) {
		return _("received data length is too short");

	} else if (size > size_) {
		return _("received data length is too large");
	}

	size2 = size;
	memcpy(data_, data, size2);

	return null_str;
}

bool cswamp_uploadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& file, const std::string& uuid, int fsize, int nonce, int offset, uint8_t* data, int size, bool end, int app_received_bytes, int app_expected_bytes, bool quiet, int& size2)
{
	thttp_cswamp_device_uploadfile entity(progress, sessionid, filetype, file, uuid, fsize, nonce, offset, data, size, end, TIMEOUT_10S, quiet);
	bool ret = cswamp_do_agbox(progress, entity, true, false, app_received_bytes, app_expected_bytes);
	if (ret) {
		size2 = entity.size2;
	}
	return ret;
}

bool do_uploadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& src, const std::string& uuid, const std::string& disk_filename, bool quiet)
{
	tfile file(disk_filename, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);

	int64_t fsize = posix_fsize(file.fp);
	if (fsize == 0) {
		SDL_Log("do_uploadfile fail, The file length is 0");
		return false;
	}
	const int nonce = rand();
	int offset = 0; // * 100 maybe exceed int32_t
	const int block_size = 4 * 1024 * 1024;
	file.resize_data(block_size);

	bool ret = true;
	while (offset < fsize) {
		int bytes = block_size;
		if (offset + bytes > fsize) {
			bytes = fsize - offset;
		}

		posix_fread(file.fp, file.data, bytes);

		bool end = offset + bytes == fsize;
		const int app_received_bytes = offset;
		const int app_expected_bytes = fsize != nposm? fsize: 0;
		int size2 = nposm;
		ret = cswamp_uploadfile(progress, sessionid, filetype, src, uuid, fsize, nonce, offset, (uint8_t*)file.data, bytes, end, app_received_bytes, app_expected_bytes, quiet, size2);
		if (!ret) {
			break;
		}

		offset += bytes;
		SDL_Log("do_uploadfile, received %s(%i)", utils::format_i64size(offset).c_str(), offset);
	
		// progress.set_percentage(100.0 * offset / fsize);
		// progress.set_message(utils::format_i64size(offset) + "/" + utils::format_i64size(fsize));
	}

	return ret;
}

// cswamp/device/downloadfile
class thttp_cswamp_device_downloadfile: public ihttp_cswamp
{
public:
	thttp_cswamp_device_downloadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& file, const std::string& uuid, int64_t uid, int offset, uint8_t* data, int size, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/downloadfile"), quiet)
		, sessionid_(sessionid)
		, filetype_(filetype)
		, file_(file)
		, uuid_(uuid)
		, uid_(uid)
		, offset_(offset)
		, data_(data)
		, size_(size)
		, timeout_(timeout)
		, quiet_(false)
		, fsize(0)
		, size2(nposm)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;
	std::string post_binary(const uint8_t* data, int size) override;

public:
	int fsize;
	int size2;

private:
	const std::string sessionid_;
	const std::string filetype_;
	const std::string file_;
	const std::string uuid_;
	const int64_t uid_;
	const int offset_;
	uint8_t* data_;
	const int size_;
	const int timeout_;
	const bool quiet_;
};

void thttp_cswamp_device_downloadfile::verify_param()
{
	VALIDATE(!filetype_.empty(), null_str);
	VALIDATE(!file_.empty(), null_str);
	VALIDATE(offset_ >= 0, null_str);
	VALIDATE(size_ >= 0, null_str);
	VALIDATE(data_ != nullptr, null_str);
}

bool thttp_cswamp_device_downloadfile::pre(Json::Value& json_params) const
{
	if (!sessionid_.empty()) {
		json_params["sessionid"] = sessionid_;
	}
	json_params["filetype"] = filetype_;
	json_params["file"] = file_;
	if (utils::is_uuid(uuid_, false)) {
		json_params["uuid"] = uuid_;
	}
	if (uid_ != nposm) {
		json_params["uid"] = uid_;
	}
	json_params["offset"] = offset_;
	json_params["size"] = size_;

	return true;
}

std::string thttp_cswamp_device_downloadfile::post(Json::Value& results)
{
	fsize = results["fsize"].asInt();
	return null_str;
}

std::string thttp_cswamp_device_downloadfile::post_binary(const uint8_t* data, int size)
{
	// caller has executed sha1-verify, size if length that get ride of SHA_DIGEST_LENGTH.
	if (size == 0) {
		return _("received data length is too short");

	} else if (size > size_) {
		return _("received data length is too large");
	}

	size2 = size;
	memcpy(data_, data, size2);

	return null_str;
}

bool cswamp_downloadfile(gui2::tprogress_& progress, const std::string& sessionid, const std::string& filetype, const std::string& file, const std::string& uuid, int64_t uid, int offset, uint8_t* data, int size, int app_received_bytes, int app_expected_bytes, bool quiet, int& size2, int& fsize)
{
	thttp_cswamp_device_downloadfile entity(progress, sessionid, filetype, file, uuid, uid, offset, data, size, TIMEOUT_10S, quiet);
	bool ret = cswamp_do_agbox(progress, entity, false, true, app_received_bytes, app_expected_bytes);
	if (ret) {
		size2 = entity.size2;
		fsize = entity.fsize;
	}
	return ret;
}

enum {ver_equal, ver_less, ver_greater};
bool do_downloadfile(gui2::tprogress_& progress, const version_info& curr_version, const std::string& sessionid, 
	const std::string& filetype, const std::string& src, const std::string& uuid, int64_t uid, const std::string& disk_filename, bool quiet, int* ver_result_ptr)
{
	int ver_result = nposm;

	tfile file(disk_filename, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);

	int offset = 0, fsize = nposm; // * 100 maybe exceed int32_t
	const int block_size = 4 * 1024 * 1024;
	file.resize_data(block_size);

	bool ret = true;
	while (true) {
		// main_serial.stop_wdg();
		int size = block_size;
		if (fsize != nposm && fsize < offset + block_size) {
			size = fsize - offset;
		} else if (fsize == nposm && curr_version.is_rose_recommended()) {
			size = posix_align_ceil(sizeof(trsp_header), 4096);
		}
		const int app_received_bytes = offset;
		const int app_expected_bytes = fsize != nposm? fsize: 0;
		int size2 = nposm;
		int fsize2 = nposm;
		ret = cswamp_downloadfile(progress, sessionid, filetype, src, uuid, uid, offset, (uint8_t*)file.data, size, app_received_bytes, app_expected_bytes, quiet, size2, fsize2);
		// agent.did_pre = std::bind(&did_pre_downloadfile, _2, _3, src, offset, block_size);
		// agent.did_post = std::bind(&did_post_downloadfile, _2, _3, (uint8_t*)file.data, &once_received_bytes, quiet);

		// if (!net::handle_http_request(agent)) {
		//	ret = false;
		//	break;
		// }
		if (!ret) {
			break;
		}
		// this file maybe 0 bytes.
		VALIDATE(size2 >= 0 && size2 <= size, null_str);
		VALIDATE(fsize2 >= 0, null_str);
		if (fsize != nposm) {
			if (fsize != fsize2) {
				SDL_Log("do_downloadfile fail, The file length is changed, %i --> %i", fsize, fsize2);
				ret = false;
				break;
			}
		} else {
			fsize = fsize2;
		}

		if (size2 > 0) {
			posix_fwrite(file.fp, file.data, size2);
		}

		if (offset == 0 && curr_version.is_rose_recommended()) {
			// if curr_version is set, new_version must > it.
			if (size2 <= sizeof(trsp_header) || (fsize <= sizeof(trsp_header) + SHA_DIGEST_LENGTH)) {
				ret = false;
				break;
			}

			const trsp_header* header = (trsp_header*)(file.data);
			int major = posix_hi8(posix_lo16(header->version));
			int minor = posix_lo8(posix_hi16(header->version));
			int revision_level = posix_hi8(posix_hi16(header->version));
			const version_info new_version(major, minor, revision_level, true, '-', str_cast(header->build_date));
			ver_result = ver_greater;
			if (new_version == curr_version) {
				SDL_Log("new_version(%s) == curr_version(%s), fail", new_version.str(true).c_str(), curr_version.str(true).c_str());
				ver_result = ver_equal;

			} else if (new_version < curr_version) {
				SDL_Log("new_version(%s) < curr_version(%s), fail", new_version.str(true).c_str(), curr_version.str(true).c_str());
				ver_result = ver_less;
			}
			if (ver_result != ver_greater) {
				ret = false;
				break;
			}
		}

		offset += size2;
		SDL_Log("do_downloadfile, received %s", utils::format_i64size(offset).c_str());
		if (offset == fsize) {
			break;

		} else if (offset > fsize) {
			SDL_Log("do_downloadfile fail, offset(%i) > fsize(%i)", offset, fsize);
			ret = false;
			break;
		}
			
		progress.set_percentage(100.0 * offset / fsize);
		progress.set_message(utils::format_i64size(offset) + "/" + utils::format_i64size(fsize));
	}
	if (!ret) {
		file.close();
		SDL_DeleteFiles(disk_filename.c_str());
	}
	if (ver_result_ptr != nullptr) {
		*ver_result_ptr = ver_result;
	}
	return ret;
}

// agbox/village --> getInfo
class thttp_cswamp_device_login: public ihttp_cswamp
{
public:
	thttp_cswamp_device_login(gui2::tprogress_& progress, int type, const std::string& username, const std::string& password, const std::string& deviceid, const std::string& devicename, int timeout, bool quiet, tcswamp_login_result& result)
		: ihttp_cswamp(progress, form_url2("cswamp/device/login"), quiet)
		, type_(type)
		, username_(username)
		, password_(password)
		, deviceid_(deviceid)
		, devicename_(devicename)
		, timeout_(timeout)
		, quiet_(false)
		, result_(result)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	int type_;
	const std::string username_;
	const std::string password_;
	const std::string deviceid_;
	const std::string devicename_;
	const int timeout_;
	const bool quiet_;

	tcswamp_login_result& result_;
};

void thttp_cswamp_device_login::verify_param()
{
	VALIDATE(type_ >= 0 && type_ < cswamp_login_type_count, null_str);
	VALIDATE(!username_.empty(), null_str);
	VALIDATE(!password_.empty(), null_str);
	VALIDATE(!deviceid_.empty(), null_str);
	VALIDATE(!devicename_.empty(), null_str);
}

static std::map<int, std::string> cswamp_login_types;
bool thttp_cswamp_device_login::pre(Json::Value& json_params) const
{
	if (cswamp_login_types.empty()) {
		cswamp_login_types.insert(std::make_pair(cswamp_login_type_password, "password"));
		cswamp_login_types.insert(std::make_pair(cswamp_login_type_cookie, "cookie"));
	}
	json_params["type"] = cswamp_login_types.find(type_)->second;
	json_params["username"] = username_;
	json_params["password"] = password_;
	json_params["deviceid"] = deviceid_;
	json_params["devicename"] = devicename_;
	SDL_Log("[chromium]thttp_cswamp_device_login::pre, type: %s, username_: %s, password: %s", 
		cswamp_login_types.find(type_)->second.c_str(), username_.c_str(), password_.c_str());

	return true;
}

std::string thttp_cswamp_device_login::post(Json::Value& results)
{
	result_.sessionid = results["sessionid"].asString();
	result_.pwcookie = results["pwcookie"].asString();
	result_.uid = results["uid"].asInt64();
	SDL_Log("[chromium]thttp_cswamp_device_login::post, sessionid: %s, pwcookie: %s", result_.sessionid.c_str(), result_.pwcookie.c_str());
	return null_str;
}

bool cswamp_login(gui2::tprogress_& progress, int type, const std::string& username, const std::string& password, 
	const std::string& deviceid, const std::string& devicename, int timeout, bool quiet, tcswamp_login_result& result)
{
	thttp_cswamp_device_login entity(progress, type, username, password, deviceid, devicename, timeout == nposm? TIMEOUT_NORMAL: timeout, quiet, result);
	return cswamp_do_agbox(progress, entity, false, false);
}

static std::string login_devicename()
{
	std::string os = "Windows"; // kdesktop(Win10)
	if (game_config::os == os_android) {
		os = "Andorid";
	} else if (game_config::os == os_ios) {
		os = "iOS";
	}

	char devicename[128];
	SDL_snprintf(devicename, sizeof(devicename), "%s(%s)", game_config::app.c_str(), os.c_str());
	return devicename;
}

bool cswamp_login2(truser& user, int type, const std::string& username, const std::string& password, int timeout, bool quiet)
{
	VALIDATE(!username.empty(), null_str);
	VALIDATE(!password.empty(), null_str);
	VALIDATE(!user.deviceid.empty(), null_str);

	// net::tcswamp_login_result result;
	const std::string devicename = login_devicename();

	user.clear();

	if (type == cswamp_login_type_password) {
		
	} else {
		VALIDATE(type == cswamp_login_type_cookie, null_str);
		VALIDATE(username == user.username, null_str);
		VALIDATE(password == user.pwcookie, null_str);
	}

	tcswamp_login_result result;
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_login, _1, type, 
			username, password, user.deviceid, devicename, timeout, quiet, std::ref(result)));
	bool ret = gui2::run_with_progress(slot, null_str, _("Login"), 1500);
	if (!ret) {
		return false;
	}

	if (username != user.username) {
		user.username = username;
		preferences::set_login_username(user.username);
	}
	user.sessionid = result.sessionid;
	user.pwcookie = result.pwcookie;
	user.uid = result.uid;
	preferences::set_login_pwcookie(user.pwcookie);

	return ret;
}

// cswamp/device --> logout
class thttp_cswamp_device_logout: public ihttp_cswamp
{
public:
	thttp_cswamp_device_logout(gui2::tprogress_& progress, const std::string& sessionid, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/logout"), quiet)
		, sessionid_(sessionid)
		, timeout_(timeout)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int timeout_;
	const bool quiet_;
};

void thttp_cswamp_device_logout::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
}

bool thttp_cswamp_device_logout::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;

	return true;
}

std::string thttp_cswamp_device_logout::post(Json::Value& results)
{
	return null_str;
}

bool cswamp_logout(gui2::tprogress_& progress, const std::string& sessionid, bool quiet)
{
	thttp_cswamp_device_logout entity(progress, sessionid, TIMEOUT_10S, quiet);
	return cswamp_do_agbox(progress, entity, false, false);
}


// cswamp/device --> syncappletlist
class thttp_cswamp_device_syncappletlist: public ihttp_cswamp
{
public:
	thttp_cswamp_device_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/syncappletlist"), quiet)
		, sessionid_(sessionid)
		, app_(app)
		, adds_(adds)
		, removes_(removes)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int app_;
	const std::vector<std::string> adds_;
	const std::vector<std::string> removes_;
	const int timeout_;
	const bool quiet_;
	std::string bundleids_;
};

void thttp_cswamp_device_syncappletlist::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
	VALIDATE(aplt::apps.count(app_) != 0, null_str);
}

bool thttp_cswamp_device_syncappletlist::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;
	json_params["app"] = aplt::apps.find(app_)->second;
	json_params["add"] = utils::join(adds_);
	json_params["remove"] = utils::join(removes_);

	return true;
}

std::string thttp_cswamp_device_syncappletlist::post(Json::Value& results)
{
	bundleids_ = results["applet"].asString();
	return null_str;
}

bool cswamp_syncappletlist(gui2::tprogress_& progress, const std::string& sessionid, int app, const std::vector<std::string>& adds, const std::vector<std::string>& removes, bool quiet)
{
	thttp_cswamp_device_syncappletlist entity(progress, sessionid, app, adds, removes, quiet);
	return cswamp_do_agbox(progress, entity, false, false);
}

// cswamp/device --> addevent
class thttp_cswamp_device_addevent: public ihttp_cswamp
{
public:
	thttp_cswamp_device_addevent(gui2::tprogress_& progress, const std::string& sessionid, 
		int64_t ts, const std::string& devicename, const std::string& desc, int image_format, const std::vector<timage_pair>& images, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/addevent"), quiet)
		, sessionid_(sessionid)
		, ts_(ts)
		, devicename_(devicename)
		, desc_(desc)
		, image_format_(image_format)
		, images_(images)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre_binary(Json::Value& json_params, tuint8data2_C& buf) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	int64_t ts_;
	const std::string devicename_;
	const std::string desc_;
	int image_format_;
	const std::vector<timage_pair> images_;
	const int timeout_;
	const bool quiet_;
	std::string bundleids_;
};

void thttp_cswamp_device_addevent::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
	VALIDATE(!desc_.empty() || !images_.empty(), null_str);
	VALIDATE(image_format_ == img_png || image_format_ == img_jpg, null_str);
}

static const char* image_formats[] = {
	"png",
	"jpeg"
};
static int nb_image_formats = sizeof(image_formats) / sizeof(image_formats[0]);

int image_type_from_str(const std::string& str)
{
	if (str.empty()) {
		return nposm;
	}
	for (int n = 0; n < nb_image_formats; n ++) {
		if (utils::lowercase(str) == utils::lowercase(image_formats[n])) {
			return n;
		}
	}
	return nposm;
}

bool thttp_cswamp_device_addevent::pre_binary(Json::Value& json_params, tuint8data2_C& buf) const
{
	json_params["sessionid"] = sessionid_;
	json_params["time"] = ts_;
	json_params["devicename"] = devicename_;
	json_params["desc"] = desc_;
	json_params["imageformat"] = image_formats[image_format_];

	const int max_image_size = CONSTANT_1M * 5;
	if (!images_.empty()) {
		utils::resize_uint8data(buf, SDL_min(max_image_size, CONSTANT_1M * images_.size()), buf.vsize);
	}

	Json::Value json_images(Json::arrayValue);
	// int image_data_len = 0;
	for (std::vector<timage_pair>::const_iterator it = images_.begin(); it != images_.end(); ++ it) {
		if (buf.vsize > max_image_size) {
			return false;
		}
		const timage_pair& image = *it;
		// uint8_t* image_data = imwrite_mem(image.surf, image_format_, &image_data_len);
		utils::resize_uint8data(buf, buf.vsize + image.image.len, buf.vsize);
		memcpy(buf.ptr + buf.vsize, image.image.ptr, image.image.len);
		// SDL_free(image_data);

		Json::Value json_image;
		json_image["desc"] = image.desc;
		json_image["offset"] = buf.vsize;
		json_image["size"] = image.image.len;

		SDL_Log("time: %" PRIi64 "image_data_len: %i", ts_, image.image.len);
		json_images.append(json_image);

		buf.vsize += image.image.len;
	}
	json_params["imagesize"] = buf.vsize;
	json_params["images"] = json_images;

	SDL_Log("[chromium]thttp_cswamp_device_addevent::pre_binary, devicename_: %s, desc: %s", 
		devicename_.c_str(), desc_.c_str());

	return true;
}

std::string thttp_cswamp_device_addevent::post(Json::Value& results)
{
	bundleids_ = results["applet"].asString();
	return null_str;
}

bool cswamp_addevent(gui2::tprogress_& progress, const std::string& sessionid, int64_t ts, const std::string& devicename, const std::string& desc, int image_format, const std::vector<timage_pair>& images, bool quiet)
{
	thttp_cswamp_device_addevent entity(progress, sessionid, ts, devicename, desc, image_format, images, quiet);
	return cswamp_do_agbox(progress, entity, true, false);
}

// cswamp/device/getevent
class thttp_cswamp_device_getevent: public ihttp_cswamp
{
public:
	thttp_cswamp_device_getevent(gui2::tprogress_& progress, const std::string& sessionid, 
		int64_t event_time, std::set<trobot_event>& events, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/getevent"), quiet)
		, sessionid_(sessionid)
		, event_time_(event_time)
		, events_(events)
		, timeout_(timeout)
		, quiet_(false)
		, count(0)
		, imagesize(nposm)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;
	std::string post_binary(const uint8_t* data, int size) override;

public:
	int count;
	int imagesize;

private:
	const std::string sessionid_;
	const std::string file_;
	int64_t event_time_;
	std::set<trobot_event>& events_;
	const int timeout_;
	const bool quiet_;

	Json::Value json_events_;
};

void thttp_cswamp_device_getevent::verify_param()
{
	VALIDATE(events_.empty(), null_str);
}

bool thttp_cswamp_device_getevent::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;
	json_params["event_time"] = event_time_;

	return true;
}

std::string thttp_cswamp_device_getevent::post(Json::Value& results)
{
	count = results["count"].asInt();
	imagesize = results["imagesize"].asInt();

	json_events_ = results["events"];
	return null_str;
}

std::string thttp_cswamp_device_getevent::post_binary(const uint8_t* data, int size)
{
	if (imagesize != size) {
		return _("received data length isn't same as length that is written json-object");;
	}

	Json::Value& event_list = json_events_;
	if (event_list.isArray()) {
		for (int at = 0; at < (int)event_list.size(); at ++) {
			const Json::Value& json_event = event_list[at];

			// events_.push_back(trobot_event());
			trobot_event new_event;
			new_event.ts = json_event["time"].asInt64();
			new_event.devicename = json_event["devicename"].asString();
			new_event.desc = json_event["desc"].asString();

			const Json::Value& image_list = json_event["images"];
			if (image_list.isArray()) {
				for (int at2 = 0; at2 < (int)image_list.size(); at2 ++) {
					const Json::Value& json_image = image_list[at2];
					const std::string desc = json_image["desc"].asString();
					int offset = json_image["offset"].asInt();
					int size = json_image["size"].asInt();
					if (offset < 0 || size <= 0 || offset + size > imagesize) {
						return "image's offset or size error";
					}
					new_event.images.push_back(timage_pair(desc, data + offset, size));
/*
					char filename[256];
					SDL_snprintf(filename, sizeof(filename), "%s/evt(%i)-img(%i)-offset(%i)-size(%i).png", 
						game_config::preferences_dir.c_str(), at, at2, offset, size);
					write_file(filename, (const char*)new_event.images.back().image.ptr, new_event.images.back().image.len);
*/
/*
					surface surf = imread_mem(data + offset, size);
					if (surf.get() == nullptr) {
						return "image's data error";
					}

					char buf[32];
					SDL_snprintf(buf, sizeof(buf), "%i.png", at2);
					imwrite(surf, buf);
					new_event.images.push_back(timage_pair(desc, surf, image_format));
*/
				}
			}
			events_.insert(new_event);
		}
	}

	return null_str;
}

bool cswamp_getevent(gui2::tprogress_& progress, const std::string& sessionid, int64_t event_time, std::set<trobot_event>& events, int& count, bool quiet)
{
	events.clear();
	count = 0;

	thttp_cswamp_device_getevent entity(progress, sessionid, event_time, events, TIMEOUT_10S, quiet);
	bool ret = cswamp_do_agbox(progress, entity, false, true);
	if (ret) {
		count = entity.count;
	}
	return ret;
}

// cswamp/device --> keepalive
class thttp_cswamp_device_keepalive: public ihttp_cswamp
{
public:
	thttp_cswamp_device_keepalive(gui2::tprogress_& progress, const std::string& sessionid, int timeout, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/keepalive"), quiet)
		, sessionid_(sessionid)
		, event_time(nposm)
		, timeout_(timeout)
		, quiet_(quiet)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

public:
	int64_t event_time;

private:
	const std::string sessionid_;
	const int timeout_;
	const bool quiet_;
};

void thttp_cswamp_device_keepalive::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
}

bool thttp_cswamp_device_keepalive::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;

	return true;
}

std::string thttp_cswamp_device_keepalive::post(Json::Value& results)
{
	event_time = results["event_time"].asInt64();
	return null_str;
}

bool cswamp_keepalive(gui2::tprogress_& progress, const std::string& sessionid, bool quiet, int64_t& event_time)
{
	event_time = 0;
	thttp_cswamp_device_keepalive entity(progress, sessionid, TIMEOUT_NORMAL, quiet);
	bool ret = cswamp_do_agbox(progress, entity, false, false);
	if (ret) {
		event_time = entity.event_time;
	}
	return ret;
}


// cswamp/device --> querytablecooking
class thttp_cswamp_device_querytablecooking: public ihttp_cswamp
{
public:
	thttp_cswamp_device_querytablecooking(gui2::tprogress_& progress, const std::string& sessionid, int table, tcswamp_table_result& result, bool quiet)
		: ihttp_cswamp(progress, form_url2("cswamp/device/querytablecooking"), quiet)
		, sessionid_(sessionid)
		, table_(table)
		, timeout_(TIMEOUT_10S)
		, quiet_(quiet)
		, table_result_(result)
	{}

private:
	void verify_param() override;
	int timeout_ms() const override { return timeout_; }
	bool pre(Json::Value& json_params) const override;
	std::string post(Json::Value& results) override;

private:
	const std::string sessionid_;
	const int table_;
	const int timeout_;
	const bool quiet_;
	tcswamp_table_result& table_result_;
};

void thttp_cswamp_device_querytablecooking::verify_param()
{
	VALIDATE(!sessionid_.empty(), null_str);
	VALIDATE(table_ >= 0, null_str);
}

bool thttp_cswamp_device_querytablecooking::pre(Json::Value& json_params) const
{
	json_params["sessionid"] = sessionid_;
	json_params["table"] = table_;

	return true;
}

std::string thttp_cswamp_device_querytablecooking::post(Json::Value& results)
{
	tcswamp_table_result& result2 = table_result_;
	result2.clear();

	result2.table_name = results["table_name"].asString();
	if (result2.table_name.empty()) {
		// request table isn't existed.
		result2.not_existed = true;
		return null_str;
	}

	result2.total_amount = results["total_amount"].asDouble();
	result2.order_time = results["order_time"].asInt64();

	Json::Value& json_cooking = results["cooking"];
	if (json_cooking.isArray()) {
		for (int at = 0; at < (int)json_cooking.size(); at ++) {
			const Json::Value& item = json_cooking[at];
			std::string name = item["name"].asString();
			int count = item["count"].asInt();
			int time = item["time"].asInt();
			result2.cooking.insert(tcswamp_dish(name, count, time));
		}
	}

	Json::Value& json_done = results["done"];
	if (json_done.isArray()) {
		for (int at = 0; at < (int)json_done.size(); at ++) {
			const Json::Value& item = json_done[at];
			std::string name = item["name"].asString();
			int count = item["count"].asInt();
			result2.done.push_back(tcswamp_dish(name, count));
		}
	}

	return null_str;
}

bool cswamp_querytablecooking(gui2::tprogress_& progress, const std::string& sessionid, int table, tcswamp_table_result& result, bool quiet)
{
	thttp_cswamp_device_querytablecooking entity(progress, sessionid, table, result, quiet);
	return cswamp_do_agbox(progress, entity, false, false);
}

enum {cswamp_filetype_upgrade, cswamp_filetype_user_material, cswamp_filetype_count};
static const std::map<int, std::string> cswamp_filetypes = {
	{cswamp_filetype_upgrade, "upgrade"},
	{cswamp_filetype_user_material, "user_material"},
};

static const std::string filetype_from_zip_type(int zip_type)
{
	if (zip_type == zipt_apk || zip_type == zipt_pinyin || zip_type == zipt_latex) {
		return cswamp_filetypes.find(cswamp_filetype_upgrade)->second;

	} else if (zip_type == zipt_material) {
		return cswamp_filetypes.find(cswamp_filetype_user_material)->second;

	} else {
		VALIDATE(false, "Unknown zip_type");
	}
	return null_str;
}

bool upload_rsp(int zip_type, const std::string& sessionid, const std::string& remote_src, const std::string& uuid, const std::string& disk_filename)
{
	std::string title;
	if (zip_type == zipt_apk) {
		title = _("Upload App");

	} else if (zip_type == zipt_pinyin) {
		title = _("Upload pinyin pack");

	} else if (zip_type == zipt_material) {
		title = _("Upload material pack");
		VALIDATE(utils::is_uuid(uuid, false), null_str);

	} else {
		VALIDATE(false, "Unknown zip_type");
	}
	const std::string filetype = filetype_from_zip_type(zip_type);

	const std::string tmpfile = disk_filename;
	// tauto_destruct_executor destruct_executor(std::bind(&SDL_DeleteFiles, tmpfile.c_str()));

	const bool quiet = false;
	bool ret = true;
	{
		gui2::tprogress_default_slot slot(std::bind(&net::do_uploadfile, _1, sessionid, filetype, remote_src, uuid, tmpfile, quiet), "misc/remove.png");
		ret = gui2::run_with_progress(slot, null_str, title, 0);
	}
	if (!ret) {
		SDL_Log("upload_rsp, Upload %s fail", remote_src.c_str());
		std::string err;
/*
		if (ver_result == ver_less) {
			err = _("Version error. The version on the server is too low");

		} else if (ver_result == ver_equal) {
			err = _("Already the latest version");

		} else {
			// if second or later block fail, ver_result maybe ver_greater.
		}
*/
		if (!quiet && !err.empty()) {
			gui2::show_message(null_str, err);
		}
		return false;
	}

	return true;
}

bool upload_materialrsp(const std::string& sessionid, const std::string& remote_src, const std::string& uuid, const std::string& disk_filename)
{
	VALIDATE(!remote_src.empty(), null_str);

	return upload_rsp(zipt_material, sessionid, remote_src, uuid, disk_filename);
}

static bool download_rsp(int zip_type, const std::string& remote_src, const std::string& uuid, int64_t uid, const version_info& curr_version, const version_info& new_version, const std::string& result_file)
{
	std::string title;
	bool unpack = false;
	if (zip_type == zipt_apk) {
		unpack = true;
		title = _("Upgrade App");

	} else if (zip_type == zipt_pinyin) {
		title = _("Upgrade pinyin pack");

	} else if (zip_type == zipt_material) {
		title = _("Upgrade material pack");
		VALIDATE(utils::is_uuid(uuid, false), null_str);

	} else if (zip_type == zipt_latex) {
		title = _("Upgrade latex pack");

	} else {
		VALIDATE(false, "Unknown zip_type");
	}
	const std::string filetype = filetype_from_zip_type(zip_type);

	const std::string tmpfile = game_config::preferences_dir + "/__tmp_rsp.rsp";
	tauto_destruct_executor destruct_executor(std::bind(&SDL_DeleteFiles, tmpfile.c_str()));

	const bool quiet = false;
	int ver_result = nposm;
	bool ret = true;

	{
		gui2::tprogress_default_slot slot(std::bind(&net::do_downloadfile, _1, curr_version, null_str, filetype, remote_src, uuid, uid, tmpfile, quiet, &ver_result), "misc/remove.png");
		ret = gui2::run_with_progress(slot, null_str, title, 0);
	}
	if (!ret) {
		SDL_Log("upgrade_app, Download %s fail", remote_src.c_str());
		std::string err;
		if (ver_result == ver_less) {
			err = _("Version error. The version on the server is too low");

		} else if (ver_result == ver_equal) {
			err = _("Already the latest version");

		} else {
			// if second or later block fail, ver_result maybe ver_greater.
		}
		if (!quiet && !err.empty()) {
			gui2::show_message(null_str, err);
		}
		return false;
	}

	if (zip_type == zipt_pinyin) {
		const std::string tmpfile2 = game_config::preferences_dir + "/__tmp_rsp2.rsp";
		uint32_t start_ticks = SDL_GetTicks();
		bool retval = chinese::xchange_pinyin_rsp_ver_1_and_11(tmpfile2, tmpfile, false);
		if (retval) {
			SDL_Log("download pinyin.rsp, it is zipped-pinyin.rsp, cons %u ms, remame %s to %s", 
				SDL_GetTicks() - start_ticks, tmpfile2.c_str(), tmpfile.c_str());
			// SDL_DeleteFiles(tmpfile.c_str());
			SDL_RenameFile(tmpfile2.c_str(), utils::extract_file(tmpfile).c_str());

		} else {
			SDL_Log("download pinyin.rsp, it is unzip-pinyin.rsp, do nothing");
		}
	}

	tsha1reader src(tmpfile, false, NULL);
	VALIDATE(src.valid(), null_str);
	const int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header)) {
		SDL_Log("upgrade_app, verify sha1 fail, payload_size: %i", payload_size);
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zip_type))) {
		SDL_Log("upgrade_app, verify header.foucrcc fail");
		return false;
	}
	if (payload_size != sizeof(trsp_header) + header.zip_size) {
		SDL_Log("upgrade_app, verify payload_size fail");
		return false;
	}

	if (new_version.is_rose_recommended()) {
		if (header.version != SDL_FOURCC(0, new_version.major_version(), new_version.minor_version(), new_version.revision_level())) {
			SDL_Log("upgrade_app, check version fail");
			return false;
		}
		if (new_version.special_version().empty() || header.build_date != utils::to_uint32(new_version.special_version())) {
			SDL_Log("upgrade_app, check build_date fail");
			return false;
		}
	}

	const int one_block = 4 * 1024 * 1024;
	src.resize_data(one_block);

	const int start = unpack? sizeof(trsp_header): 0;
	const int end = unpack? payload_size: payload_size + SHA_DIGEST_LENGTH;

	// generate iaccess.apk
	tfile dest(result_file, GENERIC_WRITE, CREATE_ALWAYS);
	if (!dest.valid()) {
		std::string err = _("Failed to open the file to be written");
		if (!quiet && !err.empty()) {
			gui2::show_message(null_str, err);
		}
		return false;
	}

	int pos = start;
	posix_fseek(src.fp, pos);

	while (pos < end) {
		int bytes = one_block;
		if (pos + bytes > end) {
			bytes = end - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(dest.fp, src.data, bytes);

		pos += bytes;
	}

	return true;
}

bool upgrade_app(const std::string& remote_src, const version_info& curr_version, const version_info& new_version, const std::function<void()>& pre_SDL_UpdateApp, bool reboot)
{
	VALIDATE(!remote_src.empty(), null_str);
	if (remote_src != aplt::file_launcher_android) {
		VALIDATE(!reboot, null_str);
	}

	const std::string install_apk = remote_src + ".apk";
	const std::string apk_name = game_config::preferences_dir + "/" + install_apk;

	const bool use_local_apk = false;
	if (!use_local_apk) {
		bool ret = download_rsp(zipt_apk, remote_src, null_str, nposm, curr_version, new_version, apk_name);
		if (!ret) {
			return false;
		}
	} else {
		// Generally used for debugging android install logic.
		SDL_Log("upgrade use local apk: %s", apk_name.c_str());
		if (!SDL_IsFile(apk_name.c_str())) {
			return false;
		}
	}
	
	if (pre_SDL_UpdateApp != NULL) {
		pre_SDL_UpdateApp();
	}
	if (reboot) {
		// It takes about 20 seconds to install, and it's more safe to use 30 seconds.
		SDL_Reboot(30 * 1000);

	} else {
		SDL_Log("After installation, the device will not reboot");
	}

	SDL_Log("call SDL_UpdateApp");
	SDL_UpdateApp(install_apk.c_str());

	SDL_Log("SDL_UpdateApp is finisned, enter infinite loop, make update server is running");
	if (game_config::os != os_windows) {
		// enter infinite loop. left update server running.
		while (true) {
			SDL_Delay(1000);
		}
	}
	else {
		// let main thread throw quit exception.
		// throw CVideo::quit();
	}
	// on window, update is simulate.
	return true;
}

bool download_pinyin_or_latexrsp(int zip_type, const version_info& curr_version, const version_info& new_version)
{
	std::string remote_src;
	std::string rspfile;
	if (zip_type == zipt_pinyin) {
		remote_src = aplt::file_chinese_pinyin;
		rspfile = game_config::preferences_dir + "/cert/pinyin.rsp";

	} else if (zip_type == zipt_latex) {
		remote_src = aplt::file_latex_data;
		rspfile = game_config::preferences_dir + "/latex.rsp";

	} else {
		VALIDATE(false, null_str);
		return false;
	}

	// const std::string rspfile = game_config::preferences_dir + "/cert/pinyin.rsp";

	return download_rsp(zip_type, remote_src, null_str, nposm, curr_version, new_version, rspfile);
}

bool download_materialrsp(const std::string& remote_src, const std::string& uuid, int64_t uid, const std::string& rspfile)
{
	VALIDATE(!remote_src.empty(), null_str);
	VALIDATE(utils::is_uuid(uuid, false), null_str);
	VALIDATE(!rspfile.empty(), null_str);
	VALIDATE(uid != nposm, null_str);

	const version_info curr_version;
	const version_info new_version;
	return download_rsp(zipt_material, remote_src, uuid, uid, curr_version, new_version, rspfile);
}

}