#ifndef LEAGOR_KHOME_BLOCK_TASK_HPP
#define LEAGOR_KHOME_BLOCK_TASK_HPP


// #include "rose_thread.hpp"

#include "aplt2.hpp"
#include "rose_lua.hpp"
#include "rose_ros/aplt.hpp"
#include "rose_net_api.hpp"
#include "rose_sdl_utils.hpp"

#include "so_aplt_task_helper.hpp"

namespace aplt {

struct tquery_item
{
	tquery_item()
	{
		clear();
	}

	bool valid() const { return !id.empty() && !position.empty() && !names.empty(); }

	void clear()
	{
		id.clear();
		position.clear();
		names.clear();
		desc.clear();
		brands.clear();

		py_names.clear();
	}

	
	std::string id;
	std::string position;
	std::vector<std::string> names;
	std::string desc;
	std::vector<std::string> brands;

	std::vector<std::string> py_names;
};

#pragma pack(1)

#define KHOME_MAX_QUERY_IDBYTES		19
#define KHOME_MAX_QUERY_POSITIONBYTES	19

#define RSP_KHOME_VER	1

struct trsp_khome20bytes {
    int64_t ts;
	uint32_t query_bytes;
	uint32_t cooking_bytes;
	uint32_t reserve0;
};

struct trsp_query12bytes {
	uint32_t items;
	uint32_t positions;
	uint32_t reserve0;
};

struct trsp_cooking4bytes {
	uint32_t positions;
};

#pragma pack()

// bool did_write_rsp_khome(tfile& file, const std::string& bundleid, const version_info& rose_version, 
//	const std::string& desc, const tpose2d& charge, const uint8_t* map_data, int map_len, const std::map<std::string, tmap_position>& positions, const std::map<std::string, tmap_marker>& markers);

class tlua_block;

class tquery: public thelper_block_task_slot
{
public:
	tquery(tapplet& aplt, std::vector<tquery_item>& items, std::map<std::string, std::string>& positions);
	~tquery();

	std::string reload_from_file();
	void lua_push_item(lua_State* L, const tquery_item& item);

	const std::vector<tquery_item>& items() const { return items_; }
	const std::map<std::string, std::string>& positions() const { return positions_; }
	std::map<std::string, std::string>& mutable_positions() { return positions_; }

	void set_position(const std::string& goods_position, const std::string& uuid);

	std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) override;
	void find_matched(bool key_is_name, const std::string& desire_key, ttask_vars& task_vars) const;

private:
	std::string is_exist_CN_delimiter(const std::string& str) const;
	std::string parse_query_csv(const std::string& csv, std::vector<tquery_item>& items, std::map<std::string, std::string>& positions) const;

private:
	aplt::tr_api& r_api_;
	tlua_block& lua_block2_;
	std::vector<tquery_item>& items_; // object in tblock2
	std::map<std::string, std::string>& positions_; // object in tblock2
	const std::string query_csv_;
	const int tone_;
	const bool eng_lowercase_;
	const int max_id_position_bytes_;

	const std::string var_name_id_;
	const std::string var_name_position_;
	const std::string var_name_desc_;
	const std::string var_name_brands_;
	const std::string var_name_position_uuid_;

	const std::string var_name_ids_;
	const std::string var_name_descs_;

	std::vector<std::string> CN_delimiters_;
};

class tcooking: public thelper_block_task_slot
{
public:
	tcooking(tapplet& aplt, std::map<std::string, std::string>& positions);
	~tcooking() {}

	void reload_iot_devices2(const std::string& alias_id);
	void lua_push_device(lua_State* L, const tiot_device& device);
	void set_position(const int device_at, const std::string& uuid);

	std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) override;

	std::string query_position(const std::string& iot_alias, ttask_vars& task_vars);
	std::string query_table(int table_at, ttask_vars& task_vars);

	const std::set<tiot_device>& iot_devices2() const { return iot_devices2_; }

private:
	void adjust_positions(const std::set<std::string>& alias_names);

private:
	aplt::tr_api& r_api_;
	tlua_block& lua_block2_;
	std::map<std::string, std::string>& positions_; // object in tblock2

	const std::string var_name_table_at_;
	const std::string var_name_table_uuid_;
	const std::string var_name_table_name_;
	const std::string var_name_total_amount_;
	const std::string var_name_cooking_name_;
	const std::string var_name_cooking_count_;
	const std::string var_name_cooking_time_;
	const std::string var_name_cooking_time_hms_;
	const std::string var_name_done_name_;
	const std::string var_name_done_count_;

	std::string alias_id_;

	// std::map<tiot_device_key2, tiot_device> iot_devices2_;
	std::set<tiot_device> iot_devices2_;
};

class ttime_parser: public thelper_block_task_slot
{
public:
	ttime_parser(tapplet& aplt);
	~ttime_parser() {}

	std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) override;
	std::string parse(bool is_elapse, int64_t t, ttask_vars& task_vars);

private:
	const std::string var_name_hour_;
	const std::string var_name_minute_;
	const std::string var_name_second_;
	const std::string var_name_period_;
	const std::string var_name_day_;
};

class tkbook: public thelper_block_task_slot
{
public:
	tkbook(tapplet& aplt);
	~tkbook() {}

	std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) override;

	std::string parse(int page, int number, ttask_vars& task_vars);

	struct tquestion2
	{
		tquestion2()
		{
			clear();
		}

		bool valid() const { return page != nposm && number != nposm && !question.empty(); }

		void clear()
		{
			page = nposm;
			number = nposm;

			question.clear();
			image.clear();
			keynotes.clear();
			alsos.clear();
		}

	
		int page;
		int number;
		std::string question;
		std::string image;
		std::vector<std::string> keynotes;
		std::vector<std::string> alsos;
	};

	std::string parse_kbook_csv(const std::string& csv, std::map<int64_t, tquestion2>& questions) const;
	std::string reload_from_file();
	void lua_push_question2(lua_State* L, const tquestion2& question2);

	const std::map<int64_t, tquestion2>& questions() { return questions_; }

private:
	const std::string kbook_csv_;
	std::map<int64_t, tquestion2> questions_;

	const std::string var_name_retbool_;
	const std::string var_name_question_;
	const std::string var_name_image_;
	const std::string var_name_keynotes_;
};

class tlua_block: public thelper_lua_block
{
public:
	tlua_block();
	~tlua_block();

	tquery& query() 
	{
		if (query_ == nullptr) {
			query_ = new tquery(aplt_, query_items_, query_positions_);
		}
		return *query_; 
	}

	tcooking& cooking() 
	{ 
		if (cooking_ == nullptr) {
			cooking_ = new tcooking(aplt_, cooking_positions_);
		}
		return *cooking_; 
	}
	ttime_parser& time_parser() 
	{ 
		if (time_parser_ == nullptr) {
			time_parser_ = new ttime_parser(aplt_);
		}
		return *time_parser_;
	}

	tkbook& book() 
	{ 
		if (book_ == nullptr) {
			book_ = new tkbook(aplt_);
		}
		return *book_; 
	}

	void write_khome_rsp();
/*
	void clear_output_vars(ttask_vars& task_vars) const
	{
		for (std::vector<std::string>::const_iterator it = output_var_keys_.begin(); it != output_var_keys_.end(); ++ it) {
			const std::string& key = *it;
			if (task_vars.existed(key) != 0) {
				task_vars.erase(key);
			}
		}
	}
*/
private:
	tapplet& aplt_;
	const std::string khome_rsp_;
	// std::vector<std::string> output_var_keys_;

	std::vector<tquery_item> query_items_;
	std::map<std::string, std::string> query_positions_;
	tquery* query_;

	std::map<std::string, std::string> cooking_positions_;
	tcooking* cooking_;

	ttime_parser* time_parser_;
	tkbook* book_;
};

void luaW_pushvblock2(lua_State* L, tlua_block& widget);

}

#endif // LEAGOR_KHOME_QUERY_HPP_INCLUDED
