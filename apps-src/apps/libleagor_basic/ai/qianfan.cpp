#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
// #include "rose_ros/deepseek.hpp"
#include "qianfan.hpp"
#include "gettext.hpp"

using namespace std::placeholders;


namespace aplt {

tnlp_model::tnlp_model(tai_slot& slot, const std::string& preferences_dir)
	: slot_(slot)
	, preferences_dir_(preferences_dir)
	, summary_into_aiagent_(true)
	, ds_retbool_(false)
	, ds_input_tokens_(0)
	, ds_output_tokens_(0)
{}

void tnlp_model::send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel)
{
	VALIDATE(src >= 0 && src < chatsrc_count, null_str);
	VALIDATE(!question.empty(), null_str);

	clear();
	add_question_log(ros, src, new_conversation, question, surf);

	app_send_question(ros, src, new_conversation, question, surf, cancel);
}

void tnlp_model::add_question_log(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf)
{
	if (src != chatsrc_ai_chat) {
		uint16_t flags = new_conversation? tokensflag_new_conversation: 0;
		uint64_t tokens = join_log_tokens(0, 0, flags);
		bool aiagent = src != chatsrc_summary || summary_into_aiagent_;
		const int64_t t = time(nullptr);
		if (surf.get() == nullptr) {
			ros.aplt_add_msg_log(t, question, tokens, aiagent);
		} else {
			const std::string devicename;
			std::vector<timage_pair> images;
			int image_format = img_jpg;
			images.push_back(timage_pair(question, surf, image_format));
			ros.aplt_add_msg_log(t, join_to_log_msg(t, devicename, null_str, images), tokens, aiagent);
		}
	}
}

std::string tnlp_model::handle_response(const Json::Value& json_object, bool& handled)
{
	std::stringstream err;
	utils::string_map symbols;
	
	handled = false;

	const Json::Value error = json_object["error"];
	if (error.isObject()) {
		err << "code: " << error["code"].asString();
		err << "\nmessage: " << error["message"].asString();
		handled = true;
	}

	return err.str();
}

void tnlp_model::set_task_result(aplt::tb_api& ros, int src, bool new_conversation, bool retbool, const std::string& answer, int input_tokens, int output_tokens, bool cancel)
{
	uint16_t flags = new_conversation? tokensflag_new_conversation: 0;
	uint64_t tokens = join_log_tokens(input_tokens, output_tokens, flags);
	if (!retbool) {
		VALIDATE(input_tokens == 0 && output_tokens == 0, null_str);
		utils::string_map symbols;
		std::string answer2 = answer;
		if (cancel) {
			answer2 = _("User canceled the question.");
		}
		symbols["err"] = answer2;
		std::string msg = vgettext2("reply error: $err", symbols);
		if (src == chatsrc_summary) {
			msg = vgettext2("generate summary fail: $err", symbols);
		}
		ros.aplt_add_msg_log(time(nullptr), msg, tokens, true);

	} else {
		bool aiagent = src != chatsrc_summary || summary_into_aiagent_;
		ros.aplt_add_msg_log(time(nullptr), answer, tokens, aiagent);
	}

	ds_retbool_ = retbool;
	ds_answer_ = answer;
	ds_input_tokens_ = input_tokens;
	ds_output_tokens_ = output_tokens;

	// finished_ = true;
}

//
// tqianfan
//
tqianfan::tqianfan(tai_slot& slot, const std::string& preferences_dir)
	: tnlp_model(slot, preferences_dir)
	// , question_()
	// , surf_()
{
	clear();
}

void tqianfan::app_send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel)
{
	xmit_with_img(ros, src, new_conversation, question, surf, cancel);
}

bool tqianfan::did_pre_deepseek(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& appid, const std::string& model, const std::string& question)
{	
	net_api.SetHeader("Authorization", api_key);
	net_api.SetHeader("appid", appid);
	net_api.SetHeader(HttpRequestHeaders_kContentType, "application/json");

	Json::Value json_root;
	json_root["model"] = model;

	Json::Value json_messages;

	Json::Value json_message0;
	json_message0["role"] = "user";
	json_message0["content"] = question;
	json_messages.append(json_message0);
	json_root["messages"] = json_messages;
	// json_root["messages1"] = json_messages;

	Json::Value json_web_search;
	json_web_search["enable"] = false;
	json_web_search["enable_citation"] = false;
	json_web_search["enable_trace"] = false;
	json_root["web_search"] = json_web_search;

	Json::FastWriter writer;
	body = writer.write(json_root);

	return true;
}

bool tqianfan::did_post_deepseek(net::thttp_api& net_api, int status, const std::string& data_received, std::string& answer)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	bool retval = false;

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			bool handled = false;
			err << handle_response(json_object, handled);
			if (!handled && err.str().empty()) {
				const Json::Value& json_choices = json_object["choices"];
				if (json_choices.isArray()) {
					for (int at = 0; at < (int)json_choices.size(); at ++) {
						const Json::Value& json_choice = json_choices[at];
						int index = json_choice["index"].asInt();
						const Json::Value& message = json_choice["message"];
						if (message.isObject()) {
							answer = message["content"].asString();
							retval = true;
						}
					}
				}
			}

			if (game_config::os == os_windows) {
				tfile file(preferences_dir_ + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		VALIDATE(answer.empty(), null_str);
		answer = err.str();
	}

	return retval;
}

std::string tqianfan::xmit(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question)
{
	VALIDATE(!question.empty(), null_str);
	clear();

	// const std::string url2 = "https://qianfan.baidubce.com/v2/chat/completions";
	char url2[256];
	SDL_snprintf(url2, sizeof(url2), "https://qianfan.baidubce.com/v2/chat/completions");

	const std::string api_key = "Bearer bce-v3/ALTAK-jt1L4REAWPjKmNlGi7mhK/ba35f2e1897d0bcdbb5409f419c042608de7f7b6";
	const std::string appid = "app-SLaVgROU";
	const std::string model = "deepseek-v3";
	// const std::string model = "deepseek-r1";

	std::string answer;

	bool cancel = false;
	net::thttp_api* net_api = net::create_http_api(url2, "POST", 60000);

	net_api->did_pre = std::bind(&tqianfan::did_pre_deepseek, this, _1, _2, api_key, appid, model, question);
	net_api->did_post = std::bind(&tqianfan::did_post_deepseek, this, _1, _2, _3, std::ref(answer));
	bool retval = net_api->handle_http_request(cancel);

	delete net_api;

	set_task_result(ros, src, new_conversation, retval, answer, 0, 0, cancel);
	return null_str;
}

bool tqianfan::did_pre_new_conversation(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& app_id)
{	
	net_api.SetHeader("X-Appbuilder-Authorization", api_key);
	net_api.SetHeader(HttpRequestHeaders_kContentType, "application/json");

	Json::Value json_root;
	json_root["app_id"] = app_id;

	Json::FastWriter writer;
	body = writer.write(json_root);

	return true;
}

bool tqianfan::did_post_new_conversation(net::thttp_api& net_api, int status, const std::string& data_received, std::string& conversation_id)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	bool retval = false;

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			if (json_object.isMember("conversation_id")) {
				// {"request_id": "925b1980-e037-4a16-86b1-76bdb6d39827", 
				//  "conversation_id": "62255b54-199b-44f1-99e1-e0abb73db888"
				// }
				conversation_id = json_object["conversation_id"].asString();
				retval = true;

			} else {
				// {"code": "PermissionDeniedError", 
				//  "message": "resource ['app/29c5c4db-ef9b-4c42-a2da-d3e68a110a781'] have no permission in ['UseApp']", 
				//  "request_id": "dc1482d3-11cf-49ff-b69f-0409bcb8d889"
				// }
				err << json_object["code"].asString();
			}
			
			if (game_config::os == os_windows) {
				tfile file(preferences_dir_ + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		VALIDATE(conversation_id.empty(), null_str);
		conversation_id = err.str();
	}

	return retval;
}

std::string boundary_formdata_prefix(const std::string& boundary)
{
	std::stringstream ss;
	ss << "--" << boundary << "\r\n";
	ss << "Content-Disposition: form-data; name=";
	return ss.str();
}

std::string boundary_formdata_postfix(const std::string& boundary)
{
	std::stringstream ss;
	ss << "--" << boundary << "--\r\n";
	return ss.str();
}

std::string boundary_formdata_end_item(const std::string& boundary)
{
	std::stringstream ss;
	ss <<  "\r\n--" << boundary;
	return ss.str();
}

std::string formdata_body(const std::string& boundary, const std::map<std::string, tcode2>& data)
{
	VALIDATE(!data.empty(), null_str);
	std::stringstream result_ss;
	const std::string prefix = boundary_formdata_prefix(boundary);
	for (std::map<std::string, tcode2>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		const std::string& key = it->first;
		const tcode2& val = it->second;
		if (val.code == nposm) {
			result_ss << prefix << "\"" << key << "\"";
		} else {
			VALIDATE(val.code == img_jpg || val.code == img_png, null_str);
			const std::string filename = val.code == img_jpg? "tmp.jpg": "tmp.png";
			const std::string type = val.code == img_jpg? "image/jpeg": "image/png";

			// Content-Disposition: form-data; name="file"; filename="tmp.jpg"\r\n
			result_ss << prefix << "\"" << key << "\"; filename=\"" << filename << "\"\r\n";
			// Content-Type: image/jpeg\r\n\r\n;
			result_ss << "Content-Type: " << type;
		}
		result_ss << "\r\n\r\n";

		result_ss << val.id << "\r\n";
	}
	const std::string postfix = boundary_formdata_postfix(boundary);
	result_ss << postfix;
	return result_ss.str();
}

bool tqianfan::did_pre_upload_file(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& app_id, const std::string& conversation_id, const surface& surf)
{	
	VALIDATE(!app_id.empty(), null_str);
	VALIDATE(!conversation_id.empty(), null_str);
	VALIDATE(surf.get() != nullptr, null_str);

	std::map<std::string, tcode2> data;
	data.insert(std::make_pair("app_id", tcode2(nposm, app_id)));
	data.insert(std::make_pair("conversation_id", tcode2(nposm, conversation_id)));

	const int img_format = img_jpg;
	timage_pair pair(null_str, surf, img_format);
	std::string file_data((const char*)pair.image.ptr, pair.image.len);
	data.insert(std::make_pair("file", tcode2(img_format, file_data)));

	// const std::string formdata_boundary = "----RoseFormBoundaryrGKCBY7qhFd3TrwA";
	const std::string formdata_boundary = utils::create_uuid(false);
	body = formdata_body(formdata_boundary, data);

	const std::string contenttype_value = std::string("multipart/form-data; boundary=") + formdata_boundary;

	net_api.SetHeader("X-Appbuilder-Authorization", api_key);
	net_api.SetHeader(HttpRequestHeaders_kContentType, contenttype_value);

	return true;
}

bool tqianfan::did_post_upload_file(net::thttp_api& net_api, int status, const std::string& data_received, std::string& file_id)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	bool retval = false;

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			if (json_object.isMember("id")) {
				// {"request_id": "10a257ec-a96a-4f01-be14-95f6980e6052", 
				//  "id": "df8e88d3-7928-4f72-a3b5-7819c5b17d08", 
				//  "conversation_id": "759f3411-4820-42db-9907-4cb32050299e"
				// }
				const std::string request_id = json_object["request_id"].asString();
				file_id = json_object["id"].asString();
				retval = true;
			} else {
				// {"code": "InvalidRequestArgumentError", 
				//  "message": "conversation_id 29c5c4db-ef9b-4c42-a2da-d3e68a110a78 is not found", 
				//  "request_id": "a77a799a-def6-4944-aa2e-9dcc5a99792e"
				// }
				err << json_object["code"].asString();
			}

			if (game_config::os == os_windows) {
				tfile file(preferences_dir_ + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		VALIDATE(file_id.empty(), null_str);
		file_id = err.str();
	}

	return retval;
}

bool tqianfan::did_pre_conversation_run(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& app_id, const std::string& conversation_id, const std::string& file_id, const std::string& question)
{	
	net_api.SetHeader("X-Appbuilder-Authorization", api_key);
	net_api.SetHeader(HttpRequestHeaders_kContentType, "application/json");

	Json::Value json_root;
	json_root["app_id"] = app_id;
	json_root["conversation_id"] = conversation_id;
	json_root["stream"] = false;
	json_root["query"] = question;

	if (!file_id.empty()) {
		Json::Value json_file_ids;
		Json::Value json_file0 = file_id;
		json_file_ids.append(json_file0);
		json_root["file_ids"] = json_file_ids;
	}

	Json::FastWriter writer;
	body = writer.write(json_root);

	return true;
}

bool tqianfan::did_post_conversation_run(net::thttp_api& net_api, int status, const std::string& data_received, std::string& answer)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	bool retval = false;

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			if (json_object.isMember("answer")) {
				// {"request_id": "ab8c4a54-7b9d-4e6e-a39a-d8ef9b7119ad", 
				//  "date": "2025-03-12T12:42:03Z", 
				//  "answer": ".....",
				//  ...
				// }
				const std::string request_id = json_object["request_id"].asString();
				answer = json_object["answer"].asString();
				retval = true;

			} else {
				// {"code": "InvalidRequestArgumentError", 
				//  "message": "conversation_id 29c5c4db-ef9b-4c42-a2da-d3e68a110a78 is not found", 
				//  "request_id": "3cd308d7-8965-45c8-9934-aad395d642ab"
				// }
				err << json_object["code"].asString();
			}
			
			if (game_config::os == os_windows) {
				tfile file(preferences_dir_ + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		VALIDATE(answer.empty(), null_str);
		answer = err.str();
	}

	return retval;
}

std::string tqianfan::xmit_with_img(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel)
{
	VALIDATE(!question.empty(), null_str);

	// const std::string url2 = "https://qianfan.baidubce.com/v2/app/conversation";
	char url2[256];
	SDL_snprintf(url2, sizeof(url2), "https://qianfan.baidubce.com/v2/app/conversation");

	const std::string api_key = "Bearer bce-v3/ALTAK-zHvBMOkgZEu7fF9fLRX1M/abd02adb07c28fdf46b36e73c7e1e33d3363b849";
	const std::string app_id = "29c5c4db-ef9b-4c42-a2da-d3e68a110a78";

	//
	// 1/3 create conversation
	//
	std::string conversation_id;

	std::unique_ptr<net::thttp_api> net_api(net::create_http_api(url2, "POST", 10000));
	net_api->did_pre = std::bind(&tqianfan::did_pre_new_conversation, this, _1, _2, api_key, app_id);
	net_api->did_post = std::bind(&tqianfan::did_post_new_conversation, this, _1, _2, _3, std::ref(conversation_id));
	bool retval = net_api->handle_http_request(cancel);
	if (!retval) {
		set_task_result(ros, src, new_conversation, retval, conversation_id, 0, 0, cancel);
		return null_str;
	}
	net_api.reset();

	std::string file_id;
	if (surf.get() != nullptr) {
		//
		// 2/3 upload image file
		//
		SDL_snprintf(url2, sizeof(url2), "https://qianfan.baidubce.com/v2/app/conversation/file/upload");
		net_api.reset(net::create_http_api(url2, "POST", 10000));
		net_api->did_pre = std::bind(&tqianfan::did_pre_upload_file, this, _1, _2, api_key, app_id, conversation_id, std::ref(surf));
		net_api->did_post = std::bind(&tqianfan::did_post_upload_file, this, _1, _2, _3, std::ref(file_id));
		retval = net_api->handle_http_request(cancel);
		if (!retval) {
			set_task_result(ros, src, new_conversation, retval, file_id, 0, 0, cancel);
			return null_str;
		}
		net_api.reset();
	}
	
	//
	// 3/3 run conversation
	//
	std::string answer;

	SDL_snprintf(url2, sizeof(url2), "https://qianfan.baidubce.com/v2/app/conversation/runs");
	net_api.reset(net::create_http_api(url2, "POST", 60000));
	net_api->did_pre = std::bind(&tqianfan::did_pre_conversation_run, this, _1, _2, api_key, app_id, conversation_id, 
		file_id, std::ref(question));
	net_api->did_post = std::bind(&tqianfan::did_post_conversation_run, this, _1, _2, _3, std::ref(answer));
	retval = net_api->handle_http_request(cancel);

	set_task_result(ros, src, new_conversation, retval, answer, 0, 0, cancel);

	return null_str;
}

//
// tdeepseek
//
tdeepseek::tdeepseek(tai_slot& slot, const std::string& preferences_dir, threading::mutex& cpp_id_mutex)
	: tnlp_model(slot, preferences_dir)
	, cpp_id_mutex_(cpp_id_mutex)
	, EfficientChatManager_(*this)
{
	clear();
}

void tdeepseek::app_send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel)
{
	if (new_conversation) {
		EfficientChatManager_.clearHistory();
	}

	std::vector<Message> messages;
	messages.push_back(Message("user", question));

	// xmit2(ros, messages, cancel);
	EfficientChatManager_.sendMessage(ros, src, new_conversation, question, cancel);
}

bool tdeepseek::did_pre_deepseek2(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& model, const std::vector<Message>& messages)
{	
	net_api.SetHeader("Authorization", api_key);
	net_api.SetHeader(HttpRequestHeaders_kContentType, "application/json");

	Json::Value json_root;
	json_root["model"] = "deepseek-chat";

	Json::Value json_messages;
    for (const auto& msg : messages) {
        Json::Value json_msg;
        json_msg["role"] = msg.role;
        json_msg["content"] = msg.content;
        json_messages.append(json_msg);
    }

/*
	Json::Value json_message0;
	json_message0["role"] = "user";
	json_message0["content"] = question;
	json_messages.append(json_message0);
*/
	json_root["messages"] = json_messages;

	json_root["temperature"] = 0.7;
	// json_root["max_tokens"] = 2000;
	// json_root["max_tokens"] = 30000; // ????
	json_root["max_tokens"] = 8192; // ????

	Json::FastWriter writer;
	body = writer.write(json_root);

	if (game_config::os == os_windows) {
		tfile file(preferences_dir_ + "/1-req.dat", GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, body.c_str(), body.size());
	}
	return true;
}

bool tdeepseek::did_post_deepseek2(net::thttp_api& net_api, int status, const std::string& data_received, 
	std::string& answer, int& input_tokens, int& output_tokens)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	bool retval = false;

	if (game_config::os == os_windows) {
		tfile file(preferences_dir_ + "/2.dat", GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, data_received.c_str(), data_received.size());
	}

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			bool handled = false;
			err << handle_response(json_object, handled);
			if (!handled && err.str().empty()) {
				const Json::Value& json_choices = json_object["choices"];
				if (json_choices.isArray()) {
					for (int at = 0; at < (int)json_choices.size(); at ++) {
						const Json::Value& json_choice = json_choices[at];
						int index = json_choice["index"].asInt();
						const Json::Value& message = json_choice["message"];
						if (message.isObject()) {
							answer = message["content"].asString();
							retval = true;
						}
					}
				}
				const Json::Value& json_usage = json_object["usage"];
				if (json_usage.isObject()) {
					input_tokens = json_usage["prompt_tokens"].asInt();
					output_tokens = json_usage["completion_tokens"].asInt();
				}
			}

			if (game_config::os == os_windows) {
				tfile file(preferences_dir_ + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
				VALIDATE(file.valid(), null_str);
				posix_fwrite(file.fp, data_received.c_str(), data_received.size());
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		VALIDATE(answer.empty(), null_str);
		answer = err.str();
	}

	return retval;
}

std::string tdeepseek::xmit2(aplt::tb_api& ros, int src, bool new_conversation, const std::vector<Message>& messages, bool& cancel)
{
	char url2[256];
	SDL_snprintf(url2, sizeof(url2), "https://api.deepseek.com/v1/chat/completions");

	std::string short_api_key;
	{
		threading::lock lock(cpp_id_mutex_);
		short_api_key = api_key_;
	}
	if (short_api_key.empty()) {
		std::string err_msg = _("'API key' cannot be empty.");
		set_task_result(ros, src, new_conversation, false, err_msg, 0, 0, cancel);
		return err_msg;
	}

	char api_key_buf[64];
	SDL_snprintf(api_key_buf, sizeof(api_key_buf), "Bearer %s", api_key_.c_str());
	const std::string api_key = api_key_buf;
	const std::string model = "deepseek-chat";

	std::string answer;
	int input_tokens = 0;
	int output_tokens = 0;

	net::thttp_api* net_api = net::create_http_api(url2, "POST", 480000); // 60000

	net_api->did_pre = std::bind(&tdeepseek::did_pre_deepseek2, this, _1, _2, api_key, model, std::ref(messages));
	net_api->did_post = std::bind(&tdeepseek::did_post_deepseek2, this, _1, _2, _3, std::ref(answer), std::ref(input_tokens), std::ref(output_tokens));
	uint32_t start_ticks = SDL_GetTicks();
	bool retval = net_api->handle_http_request(cancel);
	SDL_Log("%u, tdeepseek::xmit2, handle http request took %u ms", SDL_GetTicks(), SDL_GetTicks() - start_ticks);

	delete net_api;

	set_task_result(ros, src, new_conversation, retval, answer, input_tokens, output_tokens, cancel);
	return ds_answer_;
}

class TokenEstimator {
public:
    static int estimateTokens(const std::string& text) {
        int chineseCount = 0;
        int englishCount = 0;
        
		try {
			utils::utf8_iterator curr_itor = text;
			utils::utf8_iterator end_itor = utils::utf8_iterator::end(text);
			for (; curr_itor != end_itor; ++ curr_itor) {
				wchar_t wch = *curr_itor;
				if (isChineseChar(wch)) {
					chineseCount++;

				} else if (isalpha(wch)) {
					englishCount++;
				}
			}
		} catch (utils::invalid_utf8_exception&) {
			chineseCount = 0;
			englishCount = 0;
		}
/*
        for (char c : text) {
            if (isChineseChar(c)) chineseCount++;
            else if (isalpha(c)) englishCount++;
        }
 */     
		// Simple estimation: 
		// approximately 1 Chinese character = 1.5 tokens, 
		// approximately 1 English word = 0.75 tokens.
        return static_cast<int>(chineseCount * 1.5 + englishCount * 0.75 + text.length() * 0.1);
    }
    
private:
	static bool isChineseChar(wchar_t c) {
        return (c >= 0x4E00 && c <= 0x9FFF) || 
               (c >= 0x3400 && c <= 0x4DBF);
    }
/*
    static bool isChineseChar(wchar_t c) {
        return (c >= 0x4E00 && c <= 0x9FFF) || 
               (c >= 0x3400 && c <= 0x4DBF) ||
               (c >= 0x20000 && c <= 0x2A6DF);
    }
*/
};

std::string EfficientChatManager::sendMessage(tb_api& ros, int src, bool new_conversation, const std::string& userMessage, bool& cancel)
{
	if (needSummaryUpdate()) {
        generateConversationSummary(ros, cancel);
    }
    std::vector<Message> optimizedMessages = prepareOptimizedMessages(userMessage);
        
    int inputTokens = calculateMessagesTokens(optimizedMessages);
        
    // std::string response = callDeepSeekAPI(optimizedMessages);
	std::string response = deepseek_.xmit2(ros, src, new_conversation, optimizedMessages, cancel);
	if (!deepseek_.ds_retbool()) {
		response.clear();
	}
        
    int outputTokens = TokenEstimator::estimateTokens(response);
    updateCost(inputTokens, outputTokens);
        
    updateConversationHistory(userMessage, response, inputTokens + outputTokens);
        
    return response;
}

std::vector<Message> EfficientChatManager::prepareOptimizedMessages(const std::string& userMessage)
{
    std::vector<Message> messages;

	if (!conversationSummary.empty()) {
		utils::string_map symbols;
		symbols["summary"] = conversationSummary;
		// You are the DeepSeek Assistant. Current dialogue summary: $summary
		std::string systemPrompt = vgettext2("deepseek^conversation summary, $summary", symbols);

		messages.push_back(Message("system", systemPrompt));
	}

    addSelectedHistory(messages);
        
    messages.push_back(Message("user", userMessage));
        
    return messages;
}
    
void EfficientChatManager::addSelectedHistory(std::vector<Message>& messages)
{
    int currentTokens = calculateMessagesTokens(messages);
    std::vector<Message> selectedHistory;
     
	VALIDATE(min_history_msgs_ >= 1, null_str);
	int push_history_msgs = min_history_msgs_;
    for (auto it = conversationHistory.rbegin(); it != conversationHistory.rend(); ++ it) {
        int messageTokens = it->tokens > 0 ? it->tokens: TokenEstimator::estimateTokens(it->content);
        
        if (currentTokens + messageTokens <= maxHistoryTokens * 0.7 || push_history_msgs > 0) {
			// why 'min_history_msgs_ > 0'?
			// --ensure there is at least one.
			push_history_msgs --;

            selectedHistory.push_back(*it);
            currentTokens += messageTokens;
        } else {
            break;
        }
    }
        
    for (auto it = selectedHistory.rbegin(); it != selectedHistory.rend(); ++it) {
        messages.push_back(*it);
    }
}
    

int EfficientChatManager::calculateMessagesTokens(const std::vector<Message>& messages)
{
    int total = 0;
    for (const auto& msg : messages) {
        total += msg.tokens > 0 ? msg.tokens : TokenEstimator::estimateTokens(msg.content);
    }
    return total;
}

void EfficientChatManager::updateConversationHistory(const std::string& userMessage, 
                                const std::string& assistantResponse,
                                int tokensUsed)
{

    conversationHistory.push_back(Message("user", userMessage, 
        TokenEstimator::estimateTokens(userMessage)));
        

    conversationHistory.push_back(Message("assistant", assistantResponse, 
        TokenEstimator::estimateTokens(assistantResponse)));
        
    cleanupOldMessages();
}
    
void EfficientChatManager::cleanupOldMessages()
{
    const int MAX_HISTORY_COUNT = 20; 
    const auto MAX_AGE = std::chrono::hours(2); 
        
    auto now = std::chrono::system_clock::now();
        
    conversationHistory.erase(
        std::remove_if(conversationHistory.begin(), conversationHistory.end(),
            [&](const Message& msg) {
                return now - msg.timestamp > MAX_AGE;
            }),
        conversationHistory.end()
    );
        
    while (conversationHistory.size() > MAX_HISTORY_COUNT) {
        conversationHistory.erase(conversationHistory.begin());
    }
}

bool EfficientChatManager::needSummaryUpdate() const
{
    if (conversationHistory.size() >= 6) {
        int totalTokens = 0;
        for (const auto& msg : conversationHistory) {
            totalTokens += msg.tokens;
        }
        return totalTokens > maxHistoryTokens;
    }
    return false;
}
    
void EfficientChatManager::generateConversationSummary(tb_api& ros, bool& cancel)
{
	utils::string_map symbols;
	std::stringstream history_ss;
	for (const auto& msg : conversationHistory) {
        history_ss << msg.role << ": " << msg.content << "\n";
    }
	symbols["history"] = history_ss.str();
	// Please compress the following dialogue into a summary of less than 100 words, 
	// retaining the important information: $history
	std::string prompt = vgettext2("deepseek^conversation summary prompt, $history", symbols);
        
	std::vector<Message> summaryMessages = {
		// You are a professional dialogue summarization generator
        Message("system", _("deepseek^conversation summary role")),
        Message("user", prompt)
    };

	int src = chatsrc_summary;
	bool new_conversation = false;
    deepseek_.add_question_log(ros, src, new_conversation, prompt, nullptr);
	conversationSummary = deepseek_.xmit2(ros, src, new_conversation, summaryMessages, cancel);
	deepseek_.slot().post_did_nlp_answer(src, deepseek_.ds_retbool(), deepseek_.ds_answer(), deepseek_.ds_input_tokens(), deepseek_.ds_output_tokens());

	if (!deepseek_.ds_retbool()) {
		conversationSummary.clear();
	}
            
    if (conversationHistory.size() > 4) {
        conversationHistory.erase(conversationHistory.begin(), 
                                conversationHistory.begin() + 2);
    }
}

}
