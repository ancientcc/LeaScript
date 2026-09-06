#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "rose_global.hpp"
#include "gettext.hpp"
#include "block_task.hpp"
#include "rose_config_3rdparty.hpp"

#include "rose_ros/utils.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_clazz.hpp"

#include <rose_ros/aplt.hpp>
#include "rose_net_api.hpp"

#include "common.hpp"

using namespace std::placeholders;

// it is insert to 'aplt_leagor_khome__cpp' table, so allow same name.
const char vblock2MetatableKey[] = "cpp.vlua_block";


namespace aplt {

void rsp_write_positions(const std::map<std::string, std::string>& positions, tuint8data2_C& query_data)
{
	for (std::map<std::string, std::string>::const_iterator it = positions.begin(); it != positions.end(); ++ it) {
		const std::string& name = it->first;
		const std::string& uuid = it->second;

		// VALIDATE(positions_in_items.count(name) != 0, null_str);

		int name_size = name.size();
		int uuid_size = uuid.size();
		VALIDATE(name_size > 0, null_str);

		int item_len = sizeof(int) + name_size + sizeof(int) + uuid_size;

		const int orig_vsize = query_data.vsize;

		utils::resize_uint8data(query_data, query_data.vsize + item_len, query_data.vsize);
		// name
		memcpy(query_data.ptr + query_data.vsize, &name_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, name.c_str(), name_size);
		query_data.vsize += sizeof(int) + name_size;
		// uuid
		memcpy(query_data.ptr + query_data.vsize, &uuid_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, uuid.c_str(), uuid_size);
		query_data.vsize += sizeof(int) + uuid_size;

		VALIDATE(query_data.vsize - orig_vsize == item_len, null_str);
	}
}

bool did_write_rsp_khome(tfile& file, const std::string& bundleid, const version_info& rose_version, 
	const std::vector<tquery_item>& items, const std::map<std::string, std::string>& positions, 
	const std::map<std::string, std::string>& cooking_positions)
{
	//
	// 1/2: generate query_data and progress_data
	//
	tuint8data2_C query_data;
	memset(&query_data, 0, sizeof(tuint8data2_C));

	utils::resize_uint8data(query_data, query_data.vsize + sizeof(trsp_query12bytes), query_data.vsize);

	trsp_query12bytes* query12bytes = (trsp_query12bytes*)query_data.ptr;
	memset(query12bytes, 0, sizeof(trsp_query12bytes));
	query12bytes->items = items.size();
	query12bytes->positions = positions.size();
	query_data.vsize += sizeof(trsp_query12bytes);

	std::set<std::string> positions_in_items;
	for (std::vector<tquery_item>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const tquery_item& item = *it;

		if (positions_in_items.count(item.position) == 0) {
			positions_in_items.insert(item.position);
		}

		int id_size = item.id.size();
		int position_size = item.position.size();
		VALIDATE(id_size > 0 && id_size <= KHOME_MAX_QUERY_IDBYTES, null_str);
		VALIDATE(position_size > 0 && position_size <= KHOME_MAX_QUERY_POSITIONBYTES, null_str);

		const std::string names = utils::join(item.names, ";");
		int names_size = names.size();
		VALIDATE(names_size > 0, null_str);

		int desc_size = item.desc.size();
		VALIDATE(desc_size > 0, null_str);

		const std::string brands = utils::join(item.brands, ";");
		int brands_size = brands.size();

		int item_len = sizeof(int) + id_size + sizeof(int) + position_size;
		item_len += sizeof(int) + names_size + sizeof(int) + desc_size + sizeof(int) + brands_size;

		const int orig_vsize = query_data.vsize;

		utils::resize_uint8data(query_data, query_data.vsize + item_len, query_data.vsize);
		// id
		memcpy(query_data.ptr + query_data.vsize, &id_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, item.id.c_str(), id_size);
		query_data.vsize += sizeof(int) + id_size;
		// position
		memcpy(query_data.ptr + query_data.vsize, &position_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, item.position.c_str(), position_size);
		query_data.vsize += sizeof(int) + position_size;
		// names
		memcpy(query_data.ptr + query_data.vsize, &names_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, names.c_str(), names_size);
		query_data.vsize += sizeof(int) + names_size;
		// desc
		memcpy(query_data.ptr + query_data.vsize, &desc_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, item.desc.c_str(), desc_size);
		query_data.vsize += sizeof(int) + desc_size;
		// brands
		memcpy(query_data.ptr + query_data.vsize, &brands_size, sizeof(int));
		if (brands_size != 0) {
			memcpy(query_data.ptr + query_data.vsize + 4, brands.c_str(), brands_size);
		}
		query_data.vsize += sizeof(int) + brands_size;

		VALIDATE(query_data.vsize - orig_vsize == item_len, null_str);
	}

	VALIDATE(positions_in_items.size() == positions.size(), null_str);

	for (std::map<std::string, std::string>::const_iterator it = positions.begin(); it != positions.end(); ++ it) {
		const std::string& name = it->first;
		VALIDATE(positions_in_items.count(name) != 0, null_str);
	}
	rsp_write_positions(positions, query_data);
/*
	for (std::map<std::string, std::string>::const_iterator it = positions.begin(); it != positions.end(); ++ it) {
		const std::string& name = it->first;
		const std::string& uuid = it->second;

		VALIDATE(positions_in_items.count(name) != 0, null_str);

		int name_size = name.size();
		int uuid_size = uuid.size();
		VALIDATE(name_size > 0, null_str);

		int item_len = sizeof(int) + name_size + sizeof(int) + uuid_size;

		const int orig_vsize = query_data.vsize;

		utils::resize_uint8data(query_data, query_data.vsize + item_len, query_data.vsize);
		// name
		memcpy(query_data.ptr + query_data.vsize, &name_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, name.c_str(), name_size);
		query_data.vsize += sizeof(int) + name_size;
		// uuid
		memcpy(query_data.ptr + query_data.vsize, &uuid_size, sizeof(int));
		memcpy(query_data.ptr + query_data.vsize + 4, uuid.c_str(), uuid_size);
		query_data.vsize += sizeof(int) + uuid_size;

		VALIDATE(query_data.vsize - orig_vsize == item_len, null_str);
	}
*/
	tuint8data2_C cooking_data;
	memset(&cooking_data, 0, sizeof(tuint8data2_C));

	utils::resize_uint8data(cooking_data, cooking_data.vsize + sizeof(trsp_cooking4bytes), cooking_data.vsize);

	trsp_cooking4bytes* cooking4bytes = (trsp_cooking4bytes*)cooking_data.ptr;
	memset(cooking4bytes, 0, sizeof(trsp_cooking4bytes));
	cooking4bytes->positions = cooking_positions.size();
	cooking_data.vsize += sizeof(trsp_cooking4bytes);

	rsp_write_positions(cooking_positions, cooking_data);

	//
	// 2/2 write to file
	//
	const int64_t ts = time(nullptr);

	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_khome));
	header.version = SDL_FOURCC(0, 0, 0, RSP_KHOME_VER);
	header.build_date = ts_2_build_date(ts);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());

	header.zip_size = sizeof(trsp_khome20bytes) + query_data.vsize + cooking_data.vsize;
	posix_fwrite(file.fp, &header, sizeof(header));

	trsp_khome20bytes khome;
	memset(&khome, 0, sizeof(trsp_khome20bytes));
	khome.ts = ts;
	khome.query_bytes = query_data.vsize;
	khome.cooking_bytes = cooking_data.vsize;
	posix_fwrite(file.fp, &khome, sizeof(trsp_khome20bytes));

	if (query_data.vsize > 0) {
		posix_fwrite(file.fp, query_data.ptr, query_data.vsize);
		free(query_data.ptr);

	} else {
		VALIDATE(query_data.ptr == nullptr, null_str);
	}

	if (cooking_data.vsize > 0) {
		posix_fwrite(file.fp, cooking_data.ptr, cooking_data.vsize);
		free(cooking_data.ptr);

	} else {
		VALIDATE(cooking_data.ptr == nullptr, null_str);
	}
	
	return true;
}

static void khome_clear(std::vector<tquery_item>& items, std::map<std::string, std::string>& positions)
{
	items.clear();
	positions.clear();
}

const char* rsp_load_positions(uint32_t position_count, const char* tmp, int& resi, const std::set<std::string>* positions_in_items, std::map<std::string, std::string>& positions)
{
	VALIDATE(positions.empty(), null_str);

	int field_size;
	// std::string field;
	std::string key;
	std::string val;
	std::set<std::string> absent_positions_in_items;
	if (positions_in_items != nullptr) {
		absent_positions_in_items = *positions_in_items;
	}
	for (int at = 0; at < (int)position_count; at ++) {
		for (int n = 0; n < 2; n ++) {
			if (resi < 4) {
				return nullptr;
			}
			memcpy(&field_size, tmp, sizeof(4));

			if (field_size < 0 || resi < field_size) {
				return nullptr;
			}
			if (n == 0) {
				key.assign(tmp + 4, field_size);

			} else if (n == 1) {
				val.assign(tmp + 4, field_size);
			}
			tmp += 4 + field_size;
			resi -= 4 + field_size;
		}
		if (positions_in_items != nullptr) {
			if (positions_in_items->count(key) != 0) {
				absent_positions_in_items.erase(key);
				positions.insert(std::make_pair(key, val));
			}
		} else {
			positions.insert(std::make_pair(key, val));
		}
	}

	for (std::set<std::string>::const_iterator it = absent_positions_in_items.begin(); it != absent_positions_in_items.end(); ++ it) {
		const std::string& position = *it;
		positions.insert(std::make_pair(position, null_str));
	}
	return tmp;
}

bool load_khome_from_rsp(tpinyin& pinyin, int tone, bool eng_lowercase, const std::string& path_to_rsp, 
	std::vector<tquery_item>& items, std::map<std::string, std::string>& positions, std::map<std::string, std::string>& cooking_positions)
{
	items.clear();
	positions.clear();
	cooking_positions.clear();

	tauto_destruct_executor destruct_executor(std::bind(&khome_clear, std::ref(items), std::ref(positions)));

    tsha1reader src(path_to_rsp, false, NULL);
	if (!src.valid()) {
		return false;
	}
	int payload_size = src.verify_sha1();
	if (payload_size == nposm) {
		return false;
	}
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_khome20bytes)) {
		return false;
	}
	if (payload_size >= 10 * 1024 * 1024) { // 10M
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_khome))) {
		return false;
	}
    if (header.version != SDL_FOURCC(0, 0, 0, RSP_KHOME_VER)) {
        return false;
    }
	if (header.zip_size != payload_size - sizeof(trsp_header)) {
		return false;
	}

	trsp_khome20bytes rosmap;
	memset(&rosmap, 0, sizeof(rosmap));
	posix_fread(src.fp, &rosmap, sizeof(rosmap));

	if (header.zip_size != sizeof(trsp_khome20bytes) + rosmap.query_bytes + rosmap.cooking_bytes) {
		return false;
	}

	if (rosmap.query_bytes < sizeof(trsp_query12bytes)) {
		return false;
	}

	int resi = header.zip_size - sizeof(trsp_khome20bytes);
	src.resize_data(resi);
	int bytes = posix_fread(src.fp, src.data, resi);
	VALIDATE(bytes == resi, null_str);

	const trsp_query12bytes* query12bytes = (const trsp_query12bytes*)src.data;

	resi = rosmap.query_bytes; // update resi to rosmap.query_bytes only.
	const char* tmp = src.data + sizeof(trsp_query12bytes);
	resi -= sizeof(trsp_query12bytes);

	int field_size;
	std::string field;
	std::set<std::string> positions_in_items;
	for (int at = 0; at < (int)query12bytes->items; at ++) {
		items.push_back(tquery_item());
		tquery_item& item = items.back();

		for (int n = 0; n < 5; n ++) {
			if (resi < 4) {
				return false;
			}
			memcpy(&field_size, tmp, sizeof(4));

			if (field_size < 0 || resi < field_size) {
				return false;
			}
			field.assign(tmp + 4, field_size);
			if (n == 0) {
				item.id = field;

			} else if (n == 1) {
				item.position = field;

			} else if (n == 2) {
				item.names = utils::split(field, ';');
				for (std::vector<std::string>::const_iterator it = item.names.begin(); it != item.names.end(); ++ it) {
					const std::string& name = *it;
					item.py_names.push_back(pinyin.from_utf8str2(name, tone, eng_lowercase));
				}

			} else if (n == 3) {
				item.desc = field;

			} else if (n == 4) {
				item.brands = utils::split(field, ';');
			} 
			tmp += 4 + field_size;
			resi -= 4 + field_size;
		}
		positions_in_items.insert(item.position);
	}

	tmp = rsp_load_positions(query12bytes->positions, tmp, resi, &positions_in_items, positions);
	if (tmp == nullptr) {
		return false;
	}
/*
	std::string key;
	std::string val;
	std::set<std::string> absent_positions_in_items = positions_in_items;
	for (int at = 0; at < (int)query12bytes->positions; at ++) {
		for (int n = 0; n < 2; n ++) {
			if (resi < 4) {
				return false;
			}
			memcpy(&field_size, tmp, sizeof(4));

			if (field_size < 0 || resi < field_size) {
				return false;
			}
			if (n == 0) {
				key.assign(tmp + 4, field_size);

			} else if (n == 1) {
				val.assign(tmp + 4, field_size);
			}
			tmp += 4 + field_size;
			resi -= 4 + field_size;
		}
		if (positions_in_items.count(key) != 0) {
			absent_positions_in_items.erase(key);
			positions.insert(std::make_pair(key, val));
		}
	}

	for (std::set<std::string>::const_iterator it = absent_positions_in_items.begin(); it != absent_positions_in_items.end(); ++ it) {
		const std::string& position = *it;
		positions.insert(std::make_pair(position, null_str));
	}
*/
	if (resi != 0) {
		return false;
	}

	VALIDATE(tmp - src.data == rosmap.query_bytes, null_str);
	resi = rosmap.cooking_bytes; // update resi to rosmap.cooking_bytes only.
	const trsp_cooking4bytes* cooking4bytes = (const trsp_cooking4bytes*)tmp;
	tmp = tmp + sizeof(trsp_cooking4bytes);
	resi -= sizeof(trsp_cooking4bytes);

	tmp = rsp_load_positions(cooking4bytes->positions, tmp, resi, nullptr, cooking_positions);
	if (tmp == nullptr) {
		return false;
	}
	if (resi != 0) {
		return false;
	}

	destruct_executor.cancel_execute();
    return true;
}

//
// tquery_goods
//
tquery::tquery(tapplet& aplt, std::vector<tquery_item>& items, std::map<std::string, std::string>& positions)
	: thelper_block_task_slot(*lua_block, aplt)
	, r_api_(aplt::get_r_api())
	, lua_block2_(*lua_block)
	, items_(items)
	, positions_(positions)
	, query_csv_(aplt_.preferences_dir + "/saves/query.csv")
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
	, max_id_position_bytes_(15)
	, var_name_id_(utils::join_app_prefix_id(aplt_.bundleid, "id"))
	, var_name_position_(utils::join_app_prefix_id(aplt_.bundleid, "position"))
	, var_name_desc_(utils::join_app_prefix_id(aplt_.bundleid, "desc"))
	, var_name_brands_(utils::join_app_prefix_id(aplt_.bundleid, "brands"))
	, var_name_position_uuid_(utils::join_app_prefix_id(aplt_.bundleid, "position_uuid"))
	, var_name_ids_(utils::join_app_prefix_id(aplt_.bundleid, "ids"))
	, var_name_descs_(utils::join_app_prefix_id(aplt_.bundleid, "descs"))
{
	std::vector<std::string>& output_var_keys = output_var_keys_;

	output_var_keys.push_back(var_name_id_);
	output_var_keys.push_back(var_name_position_);
	output_var_keys.push_back(var_name_desc_);
	output_var_keys.push_back(var_name_brands_);
	output_var_keys.push_back(var_name_position_uuid_);
	output_var_keys.push_back(var_name_ids_);
	output_var_keys.push_back(var_name_descs_);

	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0x3002)); // chinese (.)
	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0xff01)); // chinese (!)
	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0xff0c)); // chinese (,)
	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0xff1a)); // chinese (:)
	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0xff1b)); // chinese (;)
	CN_delimiters_.push_back(utils::UCS2_to_UTF8(0xff1f)); // chinese (?)
}

tquery::~tquery()
{
}

std::string tquery::is_exist_CN_delimiter(const std::string& str) const
{
	if (str.empty()) {
		return null_str;
	}

	for (std::vector<std::string>::const_iterator it = CN_delimiters_.begin(); it != CN_delimiters_.end(); ++ it) {
		const std::string& delimiter = *it;
		if (str.find(delimiter) != std::string::npos) {
			return delimiter;
		}
	}

	return null_str;
}

std::string tquery::parse_query_csv(const std::string& csv, std::vector<tquery_item>& items, std::map<std::string, std::string>& positions) const
{
	VALIDATE(items.empty() && positions.empty(), null_str);
	utils::string_map symbols;
	std::string err_msg;

	tfile file(csv, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		symbols["file"] = csv;
		err_msg = vgettext2("Cannot open $file", symbols);
		return err_msg;
	}
	int fsize = file.read_2_data(0, 1);
	file.data[fsize ++] = '\n'; // allow last line no '\n'
	file.data[fsize] = '\0';

	int start = fsize > 3 && (uint8_t)(file.data[0]) == 0xef && (uint8_t)(file.data[1]) == 0xbb && (uint8_t)(file.data[2]) == 0xbf? 3: 0;
	int pos = start;
	const char* tmp_data = file.data;
	int line = 0;
	if (!utils::is_utf8str(tmp_data + start, fsize - start - 1)) {
		err_msg = vgettext2("import^nonutf8 remark", symbols);
	}

	std::string illegal_delimiter;
	std::set<std::string> existed_ids;
	std::vector<std::string> v_str;
	while (pos < fsize && err_msg.empty()) {
		uint8_t ch = tmp_data[pos];
		if (ch == '\n') {
			std::string span(tmp_data + start, pos - start);
			std::vector<std::string> vstr = utils::split(span, ',', utils::STRIP_SPACES);
			if (vstr.size() >= 5) {
				// strip ' at left/right edge.
				items.push_back(tquery_item());
				tquery_item& item = items.back();
				// id
				item.id = vstr[0];

				// position
				item.position = vstr[1];

				std::stringstream item_ss;
				item_ss << "#" << (line + 1) << " " << item.id << ", " << item.position;
				symbols["item"] = item_ss.str();

				// names
				illegal_delimiter = is_exist_CN_delimiter(vstr[2]);
				if (!illegal_delimiter.empty()) {
					symbols["field"] = _("object^Name");
					symbols["char"] = illegal_delimiter;
					err_msg = vgettext2("[$item] '$field' should not appear character $char", symbols);
					break;
				}
				v_str = utils::split(vstr[2], ';');
				item.names = v_str;
				for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it) {
					const std::string& name = *it;
					item.py_names.push_back(pinyin_.from_utf8str2(name, tone_, eng_lowercase_));
				}

				// desc
				item.desc = vstr[3];

				// brands
				illegal_delimiter = is_exist_CN_delimiter(vstr[4]);
				if (!illegal_delimiter.empty()) {
					symbols["field"] = _("Brand");
					symbols["char"] = illegal_delimiter;
					err_msg = vgettext2("[$item] '$field' should not appear character $char", symbols);
					break;
				}
				v_str = utils::split(vstr[4], ';');
				item.brands = v_str;

				if (item.id.empty() || (int)item.id.size() > max_id_position_bytes_) {
					symbols["field"] = _("ID");
					if (item.id.empty()) {
						err_msg = vgettext2("[$item] '$field' must not empty.", symbols);
					} else {
						symbols["max"] = str_cast(max_id_position_bytes_);
						err_msg = vgettext2("[$item] '$field' cannot exceed $max bytes. A chinese is about 3 bytes.", symbols);
					}
					break;
				}
				
				if (item.position.empty() || (int)item.position.size() > max_id_position_bytes_) {
					symbols["field"] = _("Position");
					if (item.position.empty()) {
						err_msg = vgettext2("[$item] '$field' must not empty.", symbols);
					} else {
						symbols["max"] = str_cast(max_id_position_bytes_);
						err_msg = vgettext2("[$item] '$field' cannot exceed $max bytes. A chinese is about 3 bytes.", symbols);
					}
					break;
				}

				if (item.names.empty()) {
					symbols["field"] = _("object^Name");
					err_msg = vgettext2("[$item] '$field' must not empty.", symbols);
					break;
				}

				if (item.desc.empty()) {
					symbols["field"] = _("Description");
					err_msg = vgettext2("[$item] '$field' must not empty.", symbols);
					break;
				}

				if (existed_ids.count(item.id) != 0) {
					symbols["field"] = _("ID");
					err_msg = vgettext2("[$item] '$field' must not be duplicated.", symbols);
					break;
				}
				existed_ids.insert(item.id);

				if (positions.count(item.position) == 0) {
					positions.insert(std::make_pair(item.position, null_str));
				}


			} else if (vstr.size() != 1 || !vstr[0].empty()) {
				std::stringstream err;
				symbols["line"] = str_cast(line + 1);
				symbols["count"] = str_cast(5);
				err_msg = vgettext2("[#$line] At least $count fields per row", symbols);
				break;
			}
			line ++;
			start = pos + 1;
		}
		pos ++;
	}
	file.close();
	if (!err_msg.empty()) {
		// gui2::show_message(null_str, err_msg);
		return err_msg;
	}

	symbols["csv"] = utils::extract_file(csv);
	if (items.empty()) {
		// gui2::show_message(null_str, vgettext2("There is no valid person in $csv.", symbols));
		return vgettext2("There is no valid query item in $csv.", symbols);
	}

	return null_str;
}

std::string tquery::reload_from_file()
{
	const std::string csv = query_csv_;
	// *.csv maybe not existed. parse_query_csv will report this error.
	// VALIDATE(SDL_IsFile(csv.c_str()), null_str);

	std::vector<tquery_item> items;
	std::map<std::string, std::string> positions;
	const std::string err_msg = parse_query_csv(csv, items, positions);
	if (err_msg.empty()) {
		items_ = items;
		positions_ = positions;

		lua_block2_.write_khome_rsp();
	}
	return err_msg;
}

void tquery::lua_push_item(lua_State* L, const tquery_item& item)
{
	// tstack_size_lock lock(L, 1);

	lua_createtable(L, 5, 0);
	lua_pushstring(L, item.id.c_str());
	lua_rawseti(L, -2, 1); // <== 0: id

	lua_pushstring(L, item.position.c_str());
	lua_rawseti(L, -2, 2); // <== 1: poistion

	lua_pushstring(L, utils::join(item.names, ";").c_str());
	lua_rawseti(L, -2, 3); // <== 2: names

	lua_pushstring(L, item.desc.c_str());
	lua_rawseti(L, -2, 4); // <== 3: desc

	lua_pushstring(L, utils::join(item.brands, ";").c_str());
	lua_rawseti(L, -2, 5); // <== 4: brands
}

void tquery::set_position(const std::string& goods_position, const std::string& uuid)
{
	VALIDATE(positions_.count(goods_position) != 0, null_str);
	VALIDATE(r_api_.curmap().positions.count(uuid) != 0, null_str);

	std::map<std::string, std::string>::iterator hit_it = positions_.find(goods_position);
	const std::string orig_uuid = hit_it->second;
	hit_it->second = uuid;

	if (orig_uuid != hit_it->second) {
		lua_block2_.write_khome_rsp();
	}
}

std::string tquery::start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	const std::string var_name_query_id = utils::join_app_prefix_id(aplt_.bundleid, "query_id");
	const std::string var_name_query_name = utils::join_app_prefix_id(aplt_.bundleid, "query_name");

	std::string desire_key;
	bool key_is_name = false;
	if (task_vars.existed(var_name_query_id)) {
		desire_key = task_vars.get_string(var_name_query_id);
	}

	if (desire_key.empty() && task_vars.existed(var_name_query_name)) {
		desire_key = task_vars.get_string(var_name_query_name);
		key_is_name = true;
	}

	if (desire_key.empty()) {
		utils::string_map symbols;
		symbols["query_id"] = var_name_query_id;
		symbols["query_name"] = var_name_query_name;
		return vgettext2("At least one of $query_id and $query_name cannot be empty.", symbols);
	}

	find_matched(key_is_name, desire_key, task_vars);
	return null_str;
}

void tquery::find_matched(bool key_is_name, const std::string& desire_key, ttask_vars& task_vars) const
{
	VALIDATE(!desire_key.empty(), null_str);

	clear_output_vars(task_vars);

	const std::string py_desire_key = pinyin_.from_utf8str2(desire_key, tone_, eng_lowercase_);

	SDL_Log("find_matched use: %s, goods: %s, py: %s", key_is_name? "name": "id", desire_key.c_str(), py_desire_key.c_str());

	std::vector<const tquery_item*> hit_items;
	for (std::vector<tquery_item>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const tquery_item& item = *it;
		if (key_is_name) {
			for (std::vector<std::string>::const_iterator it2 = item.py_names.begin(); it2 != item.py_names.end(); ++ it2) {
				const std::string& py_name = *it2;
				if (py_desire_key.find(py_name) != std::string::npos) {
					SDL_Log("#%i, find_matched item, id: %s", (int)hit_items.size(), item.id.c_str());
					hit_items.push_back(&item);
					break;
				}
			}

		} else {
			if (item.id == desire_key) {
				SDL_Log("#%i, find_matched item, id: %s", (int)hit_items.size(), item.id.c_str());
				hit_items.push_back(&item);
			}
		}
	}

	if (hit_items.empty()) {
		SDL_Log("find_matched goods: %s, py: %s, cannot find", desire_key.c_str(), py_desire_key.c_str());
		return;
	}

	int size = hit_items.size();
	if (!key_is_name) {
		VALIDATE(size == 1, null_str);
	}
	if (size == 1) {
		const tquery_item& hit_item = *hit_items[0];
		VALIDATE(positions_.count(hit_item.position) != 0, null_str);

		const std::string& uuid = positions_.find(hit_item.position)->second;
		// if (ros_.curmap().positions.count(uuid) != 0) {
			std::map<std::string, std::string> pairs;
			pairs.insert(std::make_pair(var_name_id_, hit_item.id));
			pairs.insert(std::make_pair(var_name_position_, hit_item.position));
			pairs.insert(std::make_pair(var_name_desc_, hit_item.desc));
			pairs.insert(std::make_pair(var_name_brands_, utils::join(hit_item.brands, ";")));
			// pairs.insert(std::make_pair(var_name_position_uuid_, map_position.uuid));

			for (std::map<std::string, std::string>::const_iterator it = pairs.begin(); it != pairs.end(); ++ it) {
				const std::string& name = it->first;
				const std::string& val = it->second;
				task_vars.insert_string(name, false, val);
			}
		// }
		if (r_api_.curmap().positions.count(uuid) != 0) {
			const tmap_position& map_position = r_api_.curmap().positions.find(uuid)->second;
			SDL_Log("find_matched position, name: %s", map_position.name.c_str());

			task_vars.insert_string(var_name_position_uuid_, false, map_position.uuid);
		}

	} else {
		for (std::vector<const tquery_item*>::const_iterator it = hit_items.begin(); it != hit_items.end(); ++ it) {
			const tquery_item& item = **it;

			task_vars.insert_string(var_name_ids_, true, item.id);
			task_vars.insert_string(var_name_descs_, true, item.desc);
		}
	}
}

//
// tcooking
//
tcooking::tcooking(tapplet& aplt, std::map<std::string, std::string>& positions)
	: thelper_block_task_slot(*lua_block, aplt)
	, r_api_(aplt::get_r_api())
	, lua_block2_(*lua_block)
	, positions_(positions)
	, var_name_table_at_(utils::join_app_prefix_id(aplt_.bundleid, "table_at"))
	, var_name_table_uuid_(utils::join_app_prefix_id(aplt_.bundleid, "table_uuid"))
	, var_name_table_name_(utils::join_app_prefix_id(aplt_.bundleid, "table_name"))
	, var_name_total_amount_(utils::join_app_prefix_id(aplt_.bundleid, "total_amount"))
	, var_name_cooking_name_(utils::join_app_prefix_id(aplt_.bundleid, "cooking_name"))
	, var_name_cooking_count_(utils::join_app_prefix_id(aplt_.bundleid, "cooking_count"))
	, var_name_cooking_time_(utils::join_app_prefix_id(aplt_.bundleid, "cooking_time"))
	, var_name_cooking_time_hms_(utils::join_app_prefix_id(aplt_.bundleid, "cooking_time_hms"))
	, var_name_done_name_(utils::join_app_prefix_id(aplt_.bundleid, "done_name"))
	, var_name_done_count_(utils::join_app_prefix_id(aplt_.bundleid, "done_count"))
{
	std::vector<std::string>& output_var_keys = output_var_keys_;

	output_var_keys.push_back(var_name_table_at_);
	output_var_keys.push_back(var_name_table_uuid_);
	output_var_keys.push_back(var_name_table_name_);
	output_var_keys.push_back(var_name_total_amount_);
	output_var_keys.push_back(var_name_cooking_name_);
	output_var_keys.push_back(var_name_cooking_count_);
	output_var_keys.push_back(var_name_cooking_time_);
	output_var_keys.push_back(var_name_cooking_time_hms_);
	output_var_keys.push_back(var_name_done_name_);
	output_var_keys.push_back(var_name_done_count_);
}

void tcooking::reload_iot_devices2(const std::string& alias_id)
{
	VALIDATE(is_valid_iot_alias_id(alias_id), null_str);
	if (alias_id == alias_id_) {
		return;
	}

	iot_devices2_.clear();

	std::set<std::string> alias_names;
	// sync positions_ with iot_devices
	const std::map<tiot_device_key, tiot_device>& iot_devices = b_api_.iot_devices();
	for (std::map<tiot_device_key, tiot_device>::const_iterator it = iot_devices.begin(); it != iot_devices.end(); ++ it) {
		const tiot_device& device = it->second;
		int num;
		std::string name;
		if (!split_iot_alias(device.alias, alias_id, &num, &name)) {
			continue;
		}
		// SDL_Log("alias(%s) ==> num: %i, name: %s", device.alias.c_str(), num, name.c_str());
		// iot_devices2_.insert(std::make_pair(tiot_device_key2(device.src, device.device_id, device.alias), device));
		tiot_device device2 = device;
		device2.number = num;
		device2.alias_name = name;
		alias_names.insert(name);
		iot_devices2_.insert(device2);
	}

	if (game_config::os == os_windows && iot_devices2_.size() < 10) {
		char alias_name[16];
		char alias[48];
		std::string device_id_prefix = "abcdefghij-";
		for (int at = 0; at < 20; at ++) {
			std::string device_id = device_id_prefix + str_cast(at);
			int number = iot_devices.size() + 1 + at;
			SDL_snprintf(alias_name, sizeof(alias_name), "fake%i", at);  
			SDL_snprintf(alias, sizeof(alias), "%s%i-%s", alias_id.c_str(), number, alias_name);  
			tiot_device device(iot_src_doorbell, device_id, alias, null_str, 0, 0, number);
			device.number = number;
			device.alias_name = alias_name;
			alias_names.insert(alias_name);
			iot_devices2_.insert(device);
		}
	}

	adjust_positions(alias_names);
	alias_id_ = alias_id;
}

void tcooking::lua_push_device(lua_State* L, const tiot_device& device)
{
	tstack_size_lock lock(L, 1);

	int num;
	std::string name;
	bool ret = split_iot_alias(device.alias, alias_id_, &num, &name);
	VALIDATE(ret, null_str);
	VALIDATE(device.number == num && name == device.alias_name, null_str);

	lua_createtable(L, 2, 0);
	lua_pushstring(L, device.alias.c_str());
	lua_rawseti(L, -2, 1); // <== 0: alias

	const std::string uuid = positions_.count(name) != 0? positions_.find(name)->second: null_str;
	lua_pushstring(L, uuid.c_str());
	lua_rawseti(L, -2, 2); // <== 1: position_uuid

	// lua_pushinteger(L, num);
	// lua_rawseti(L, -2, 3); // <== 2: num

	// lua_pushstring(L, name.c_str());
	// lua_rawseti(L, -2, 4); // <== 3: name
}

void tcooking::set_position(int device_at, const std::string& uuid)
{
	VALIDATE(device_at >= 0 && device_at < (int)iot_devices2_.size(), null_str);
	VALIDATE(r_api_.curmap().positions.count(uuid) != 0, null_str);

	std::set<tiot_device>::const_iterator device_it = iot_devices2_.begin();
	if (device_at != 0) {
		std::advance(device_it, device_at);
	}
	const tiot_device& device = *device_it;

	std::string name;
	bool ret = split_iot_alias(device.alias, alias_id_, nullptr, &name);
	VALIDATE(ret, null_str);

	// VALIDATE(positions_.count(name) != 0, null_str);
	VALIDATE(r_api_.curmap().positions.count(uuid) != 0, null_str);

	std::string orig_uuid;
	if (positions_.count(name) != 0) {
		std::map<std::string, std::string>::iterator hit_it = positions_.find(name);
		orig_uuid = hit_it->second;
		hit_it->second = uuid;

	} else {
		std::pair<std::map<std::string, std::string>::iterator, bool> ins = positions_.insert(std::make_pair(name, uuid));
		VALIDATE(ins.second, null_str);
	}

	if (orig_uuid != uuid) {
		lua_block2_.write_khome_rsp();
	}
}

void tcooking::adjust_positions(const std::set<std::string>& alias_names)
{
	const int max_threshold = 100;
	const int min_keep_size = iot_devices2_.size() + 10;
	const int may_max_positions = SDL_max(max_threshold, min_keep_size);

	if ((int)positions_.size() <= may_max_positions) {
		return;
	}

	for (std::map<std::string, std::string>::iterator it = positions_.begin(); it != positions_.end(); ) {
		const std::string& alias_name = it->first;
		if (alias_names.count(alias_name) != 0) {
			++ it;
		} else {
			positions_.erase(it ++);
			if ((int)positions_.size() <= min_keep_size) {
				break;
			}
		}
	}
}

std::string tcooking::start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	const std::string var_name_iot_alias = utils::join_app_prefix_id(aplt_.bundleid, "iot_alias");
	const std::string var_name_table = utils::join_app_prefix_id(aplt_.bundleid, "table");

	std::string cooking_iot_alias;
	int cooking_table_at = nposm;

	if (task_vars.existed(var_name_iot_alias)) {
		cooking_iot_alias = task_vars.get_string(var_name_iot_alias);
	}

	if (cooking_iot_alias.empty() && task_vars.existed(var_name_table)) {
		cooking_table_at = task_vars.get_int(var_name_table);
		if (cooking_table_at < 0) {
			utils::string_map symbols;
			symbols["var"] = var_name_table;
			return vgettext2("Input var($var)'s value must >= 0", symbols);
		}
	}

	if (cooking_table_at == nposm) {
		return query_position(cooking_iot_alias, task_vars);
	} else {
		return query_table(cooking_table_at, task_vars);
	}
}

std::string tcooking::query_position(const std::string& iot_alias, ttask_vars& task_vars)
{
	clear_output_vars(task_vars);

	utils::string_map symbols;
	symbols["alias"] = iot_alias;

	int num;
	std::string name;
	if (!split_iot_alias(iot_alias, alias_id_, &num, &name)) {
		b_api_.aplt_add_msg_log(time(nullptr), vgettext2("Alias($alias) does not conform to the required format", symbols), 0, false);
		return null_str;
	}

	const tiot_device* hit_device = nullptr;
	for (std::set<tiot_device>::const_iterator it = iot_devices2_.begin(); it != iot_devices2_.end(); ++ it) {
		const tiot_device& device = *it;
		if (device.alias == iot_alias) {
			hit_device = &device;
			break;
		}
	}

	if (hit_device == nullptr) {
		b_api_.aplt_add_msg_log(time(nullptr), vgettext2("Cannot find alias($alias), check if it is in IoT device list", symbols), 0, false);
		return null_str;
	}

	int table_at = num - 1;
	task_vars.insert_integer(var_name_table_at_, false, table_at);

	const std::string uuid = positions_.count(name) != 0? positions_.find(name)->second: null_str;
	if (r_api_.curmap().positions.count(uuid) != 0) {
		const tmap_position& map_position = r_api_.curmap().positions.find(uuid)->second;
		SDL_Log("find_matched position, name: %s", map_position.name.c_str());

		task_vars.insert_string(var_name_table_uuid_, false, map_position.uuid);
	}

	return null_str;
}

std::string tcooking::query_table(int table_at, ttask_vars& task_vars)
{
	VALIDATE(table_at >= 0, null_str);
	clear_output_vars(task_vars);

	net::tcswamp_table_result result;

	bool ret = b_api_.cswamp_querytablecooking(table_at, result, true);
	time_t t = time(nullptr);
	if (!ret) {
		b_api_.aplt_add_msg_log(t, _("Send fail"), 0, false);
		return null_str;
	}

	SDL_Log("%s", result.to_string().c_str());

	task_vars.insert_string(var_name_table_name_, false, result.table_name);
	task_vars.insert_double(var_name_total_amount_, false, result.total_amount);

	for (std::set<net::tcswamp_dish>::const_iterator it = result.cooking.begin(); it != result.cooking.end(); ++ it) {
		const net::tcswamp_dish& dish = *it;

		task_vars.insert_string(var_name_cooking_name_, true, dish.name);
		task_vars.insert_integer(var_name_cooking_count_, true, dish.count);
		task_vars.insert_integer(var_name_cooking_time_, true, dish.time);
		task_vars.insert_string(var_name_cooking_time_hms_, true, utils::format_elapse_hms(dish.time, utils::timesep_i18n, true));
	}

	for (std::vector<net::tcswamp_dish>::const_iterator it = result.done.begin(); it != result.done.end(); ++ it) {
		const net::tcswamp_dish& dish = *it;

		task_vars.insert_string(var_name_done_name_, true, dish.name);
		task_vars.insert_integer(var_name_done_count_, true, dish.count);
		VALIDATE(dish.time == nposm, null_str);
	}

	return null_str;
}

//
// ttime_parser
//
ttime_parser::ttime_parser(tapplet& aplt)
	: thelper_block_task_slot(*lua_block, aplt)
	, var_name_hour_(utils::join_app_prefix_id(aplt_.bundleid, "hour"))
	, var_name_minute_(utils::join_app_prefix_id(aplt_.bundleid, "minute"))
	, var_name_second_(utils::join_app_prefix_id(aplt_.bundleid, "second"))
	, var_name_period_(utils::join_app_prefix_id(aplt_.bundleid, "period"))
	, var_name_day_(utils::join_app_prefix_id(aplt_.bundleid, "day"))
{
	std::vector<std::string>& output_var_keys = output_var_keys_;

	output_var_keys.push_back(var_name_hour_);
	output_var_keys.push_back(var_name_minute_);
	output_var_keys.push_back(var_name_second_);

	output_var_keys.push_back(var_name_period_);

	output_var_keys.push_back(var_name_day_);
}

std::string ttime_parser::start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	const std::string var_name_is_elapse = utils::join_app_prefix_id(aplt_.bundleid, "is_elapse");
	const std::string var_name_t_val = utils::join_app_prefix_id(aplt_.bundleid, "t_val");

	bool is_elapse = false;
	int64_t t = nposm;

	if (task_vars.existed(var_name_is_elapse)) {
		is_elapse = task_vars.get_bool(var_name_is_elapse);
	}

	if (task_vars.existed(var_name_t_val)) {
		t = task_vars.get_int64(var_name_t_val);
		if (t < 0) {
			utils::string_map symbols;
			symbols["var"] = var_name_t_val;
			return vgettext2("Input var($var)'s value must >= 0", symbols);
		}
	}

	return parse(is_elapse, t, task_vars);
}

std::string ttime_parser::parse(bool is_elapse, int64_t _t, ttask_vars& task_vars)
{	
	utils::string_map symbols;
	if (is_elapse) {
		if (_t < 0) {
			symbols["t"] = str_cast(_t);
			return vgettext2("for elapse, t($t) must >= 0", symbols);
		}
		int64_t elapse = _t;

		int64_t day = elapse / (3600 * 24);
		int64_t sec = elapse % 60;
		int64_t min = (elapse / 60) % 60;
		int64_t hour = (elapse / 3600) % 24;

		task_vars.insert_integer(var_name_day_, false, day);
		task_vars.insert_integer(var_name_hour_, false, hour);
		task_vars.insert_integer(var_name_minute_, false, min);
		task_vars.insert_integer(var_name_second_, false, sec);

	} else {
		if (_t < 0 && _t != nposm) {
			symbols["t"] = str_cast(_t);
			return vgettext2("for UTC-Time, t($t) must >= 0", symbols);
		}
		int64_t t = _t == nposm? time(nullptr): _t;

		// yyyymmddhhmmss => 20181206154225
		const struct tm* timeptr = localtime(&t);
		// char buf[64];
		// SDL_snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		//	timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
		task_vars.insert_integer(var_name_hour_, false, timeptr->tm_hour);
		task_vars.insert_integer(var_name_minute_, false, timeptr->tm_min);
		task_vars.insert_integer(var_name_second_, false, timeptr->tm_sec);

		std::string period;
		int hour = timeptr->tm_hour;
		if (hour <= 5) {
			// [0, 5]
			period = _("Before dawn");

		} else if (hour <= 11) {
			// [6, 11]
			period = _("Morning");

		} else if (hour == 12) {
			// [12]
			period = _("Noon");

		} else if (hour <= 18) {
			// [13, 18]
			period = _("Afternoon");

		} else if (hour <= 21) {
			// [19, 21]
			period = _("Evening");

		} else {
			// [22, 23]
			period = _("Late night");
		}
		task_vars.insert_string(var_name_period_, false, period);	
	}

	return null_str;
}

//
// tkbook
//
tkbook::tkbook(tapplet& aplt)
	: thelper_block_task_slot(*lua_block, aplt)
	, kbook_csv_(aplt_.preferences_dir + "/saves/kbook.csv")
	, var_name_retbool_(utils::join_app_prefix_id(aplt_.bundleid, "kbook_retbool"))
	, var_name_question_(utils::join_app_prefix_id(aplt_.bundleid, "kbook_question"))
	, var_name_image_(utils::join_app_prefix_id(aplt_.bundleid, "kbook_image"))
	, var_name_keynotes_(utils::join_app_prefix_id(aplt_.bundleid, "kbook_keynotes"))
{
	std::vector<std::string>& output_var_keys = output_var_keys_;

	output_var_keys.push_back(var_name_retbool_);
	output_var_keys.push_back(var_name_question_);
	output_var_keys.push_back(var_name_image_);
	output_var_keys.push_back(var_name_keynotes_);
}

int64_t make_question2_key(int page, int number)
{
	return posix_mki64(number, page);
}

std::string tkbook::start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	// kbook
	const std::string var_name_kbook_page = utils::join_app_prefix_id(aplt_.bundleid, "kbook_page");
	const std::string var_name_kbook_number = utils::join_app_prefix_id(aplt_.bundleid, "kbook_number");

	int kbook_page = nposm;
	int kbook_number = nposm;

	utils::string_map symbols;
	symbols["var"] = var_name_kbook_page;
	if (task_vars.existed(var_name_kbook_page)) {
		kbook_page = task_vars.get_int(var_name_kbook_page);
		if (kbook_page < 1) {
			return vgettext2("Input var($var)'s value must >= 1", symbols);
		}
	} else {
		return vgettext2("Missing input var: $var", symbols);
	}

	if (task_vars.existed(var_name_kbook_number)) {
		kbook_number = task_vars.get_int(var_name_kbook_number);
		if (kbook_number < 1) {
			return vgettext2("Input var($var)'s value must >= 1", symbols);
		}
	} else {
		return vgettext2("Missing input var: $var", symbols);
	}

	return parse(kbook_page, kbook_number, task_vars);
}

std::string tkbook::parse(int page, int number, ttask_vars& task_vars)
{	
	utils::string_map symbols;

	VALIDATE(page > 0 && number > 0, null_str);

	reload_from_file();

	int64_t key = make_question2_key(page, number);
	if (questions_.count(key) == 0) {
		task_vars.insert_bool(var_name_retbool_, false, false);	
		return null_str;
	}

	const tquestion2& question2 = questions_.find(key)->second;

	task_vars.insert_bool(var_name_retbool_, false, true);
	task_vars.insert_string(var_name_question_, false, question2.question);
	task_vars.insert_string(var_name_image_, false, question2.image);
	task_vars.insert_string(var_name_question_, false, question2.question);

	for (std::vector<std::string>::const_iterator it = question2.keynotes.begin(); it != question2.keynotes.end(); ++ it) {
		const std::string& keynote = *it;
		task_vars.insert_string(var_name_keynotes_, true, keynote);
	}
	
	return null_str;
}

std::string tkbook::parse_kbook_csv(const std::string& csv, std::map<int64_t, tkbook::tquestion2>& questions) const
{
	VALIDATE(questions.empty(), null_str);
	utils::string_map symbols;
	std::string err_msg;

	tfile file(csv, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		symbols["file"] = csv;
		err_msg = vgettext2("Cannot open $file", symbols);
		return err_msg;
	}
	int fsize = file.read_2_data(0, 1);
	file.data[fsize ++] = '\n'; // allow last line no '\n'
	file.data[fsize] = '\0';

	int start = fsize > 3 && (uint8_t)(file.data[0]) == 0xef && (uint8_t)(file.data[1]) == 0xbb && (uint8_t)(file.data[2]) == 0xbf? 3: 0;
	int pos = start;
	const char* tmp_data = file.data;
	int line = 0;
	if (!utils::is_utf8str(tmp_data + start, fsize - start - 1)) {
		err_msg = vgettext2("import^nonutf8 remark", symbols);
	}

	const std::string prefix = "{{";
	const std::string postfix = "}}";

	std::string illegal_delimiter;
	std::set<std::string> existed_ids;
	std::vector<std::string> v_str;
	while (pos < fsize && err_msg.empty()) {
		const char* start_ptr = SDL_strstr(tmp_data + pos, prefix.c_str());
		if (start_ptr == nullptr) {
			break;
		}
		const char* end_ptr = SDL_strstr(start_ptr, postfix.c_str());
		if (end_ptr == nullptr) {
			break;
		}

		start_ptr = start_ptr + prefix.size();
		const char* field_start_ptr = SDL_strchr(start_ptr, ',');
		if (field_start_ptr == nullptr || field_start_ptr > end_ptr) {
			break;
		}

		std::string span(start_ptr, field_start_ptr - start_ptr);
		v_str = utils::split(span, '-');
		if (v_str.size() != 2) {
			break;
		}
		int page = utils::to_int(v_str[0]);
		int number = utils::to_int(v_str[1]);
		if (page < 1 || number < 1) {
			break;
		}

		int64_t key = make_question2_key(page, number);
		if (questions.count(key) != 0) {
			pos = end_ptr - tmp_data;
			continue;
		}

		std::pair<std::map<int64_t, tkbook::tquestion2>::iterator, bool> ins = questions.insert(
			std::make_pair(make_question2_key(page, number), tquestion2()));
		tkbook::tquestion2& question2 = ins.first->second;
		question2.page = page;
		question2.number = number;

		char image[256];
		SDL_snprintf(image, sizeof(image), "%s/saves/kbook/%i-%i.jpg", aplt_.preferences_dir.c_str(), page, number);
		if (SDL_IsFile(image)) {
			question2.image = image;
		}

		start_ptr = field_start_ptr + 1; // 1 -> the ',' after page-number
		span.assign(start_ptr, end_ptr - start_ptr);

		const std::vector<std::string> fields {"question", "keynote", "also"};
		for (std::vector<std::string>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
			const std::string& field = *it;
			const std::string field_prefix = "[" + field + "]";
			const std::string field_postfix = "[/" + field + "]";

			const char* field_start_ptr = span.c_str();
			while (field_start_ptr != nullptr) {
				field_start_ptr = SDL_strstr(field_start_ptr, field_prefix.c_str());
				if (field_start_ptr == nullptr) {
					continue;
				}
				const char* field_end_ptr = SDL_strstr(field_start_ptr, field_postfix.c_str());
				if (field_end_ptr == nullptr) {
					field_start_ptr = nullptr;
					continue;
				}

				field_start_ptr = field_start_ptr + field_prefix.size();
				int s = field_end_ptr - field_start_ptr;
				if (s != 0) {
					VALIDATE(s > 0, null_str);
					std::string val(field_start_ptr, s);
					if (field == "question") {
						// at max 1 queestion
						question2.question = val;
						field_start_ptr = nullptr;
						continue;

					} else if (field == "keynote") {
						question2.keynotes.push_back(val);

					} else {
						VALIDATE(field == "also", null_str);
						question2.alsos.push_back(val);
					}
				}
				field_start_ptr = field_end_ptr + field_postfix.size();
			}
		} // for (fields
		
		pos = end_ptr + postfix.size() - tmp_data ;
	}
	file.close();
	if (!err_msg.empty()) {
		// gui2::show_message(null_str, err_msg);
		return err_msg;
	}

	symbols["csv"] = utils::extract_file(csv);
	if (questions.empty()) {
		// gui2::show_message(null_str, vgettext2("There is no valid person in $csv.", symbols));
		return vgettext2("There is no valid query item in $csv.", symbols);
	}

	return null_str;
}

std::string tkbook::reload_from_file()
{
	const std::string csv = kbook_csv_;
	// *.csv maybe not existed. parse_query_csv will report this error.
	// VALIDATE(SDL_IsFile(csv.c_str()), null_str);

	std::map<int64_t, tquestion2> questions;
	const std::string err_msg = parse_kbook_csv(csv, questions);
	if (err_msg.empty()) {
		questions_ = questions;

		// block2_.write_khome_rsp();
	}
	return err_msg;
}

void tkbook::lua_push_question2(lua_State* L, const tquestion2& question2)
{
	// tstack_size_lock lock(L, 1);

	lua_createtable(L, 6, 0);
	lua_pushinteger(L, question2.page);
	lua_rawseti(L, -2, 1); // <== 0: id

	lua_pushinteger(L, question2.number);
	lua_rawseti(L, -2, 2); // <== 1: poistion

	lua_pushstring(L, question2.question.c_str());
	lua_rawseti(L, -2, 3); // <== 3: desc

	lua_pushstring(L, question2.image.c_str());
	lua_rawseti(L, -2, 4); // <== 3: desc

	lua_pushstring(L, utils::join(question2.keynotes, ";").c_str());
	lua_rawseti(L, -2, 5); // <== 2: names

	lua_pushstring(L, utils::join(question2.alsos, ";").c_str());
	lua_rawseti(L, -2, 6); // <== 4: brands
}

tlua_block::tlua_block()
	: aplt_(*curr_aplt)
	, khome_rsp_(aplt_.preferences_dir + "/saves/khome.rsp")
	, query_(nullptr)
	, cooking_(nullptr)
	, time_parser_(nullptr)
	, book_(nullptr)
{
}

tlua_block::~tlua_block()
{
	if (query_ != nullptr) {
		delete(query_);
		query_ = nullptr;
	}

	if (cooking_ != nullptr) {
		delete(cooking_);
		cooking_ = nullptr;
	}

	if (time_parser_ != nullptr) {
		delete(time_parser_);
		time_parser_ = nullptr;
	}

	if (book_ != nullptr) {
		delete(book_);
		book_ = nullptr;
	}
}

void tlua_block::write_khome_rsp()
{
	// version_info rose_version("1.0.1-20240819"); // it must be reload during load_config

	const std::string bundleid = aplt_.bundleid;
	tsha1writer sha1file(khome_rsp_, nposm, std::bind(&did_write_rsp_khome, _1, bundleid, std::ref(game_config::rose_version), 
		query_items_, query_positions_, cooking_positions_));
	sha1file.write();
}

//
// lua api
//
static int impl_vblock2_collect(lua_State* L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_vblock2_get(lua_State* L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);

	return ret? 1: 0;
}

static int impl_vblock2_query_reload(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	bool from_file = luaL_checkboolean(L, 2);
	int max_disp_size = luaL_checkinteger(L, 3);

	VALIDATE(max_disp_size <= 200, null_str);

	std::string err_msg;
	if (from_file) {
		err_msg = v->query().reload_from_file();
	}
	
	lua_pushstring(L, err_msg.c_str());
	if (err_msg.empty()) {
		const std::vector<tquery_item>& items = v->query().items();
		lua_pushinteger(L, items.size());

		// items
		int lua_item_size = SDL_min(max_disp_size, (int)items.size());
		lua_createtable(L, lua_item_size, 0);

		if (lua_item_size != 0) {
			const tquery_item* items_ptr = &items[0];
			tstack_size_lock lock(L, 0);
			for (int at = 0; at < lua_item_size; at ++) {
				const tquery_item& item = items_ptr[at];
				v->query().lua_push_item(L, item);

				lua_rawseti(L, -2, at + 1);
			}
		}

		// positions
		const std::map<std::string, std::string>& positions = v->query().positions();
		int at = 0;
		lua_createtable(L, positions.size(), 0);

		{
			tstack_size_lock lock(L, 0);
			for (std::map<std::string, std::string>::const_iterator it = positions.begin(); it != positions.end(); ++ it, at ++) {
				const std::string& name = it->first;
				const std::string& uuid = it->second;
				VALIDATE(!name.empty(), null_str);
				if (from_file) {
					 VALIDATE(uuid.empty(), null_str);
				}

				lua_createtable(L, 2, 0);
				lua_pushstring(L, name.c_str());
				lua_rawseti(L, -2, 1); // <== 0: goods's name

				// now both map's uuid and name must be empty.
				lua_pushstring(L, uuid.c_str());
				lua_rawseti(L, -2, 2); // <== 1: map's uuid

				lua_rawseti(L, -2, at + 1);
			}
		}

	} else {
		lua_pushinteger(L, 0); // count
		lua_pushnil(L); // items
		lua_pushnil(L); // positions
	}
	return 4;
}

static int impl_vblock2_query_set_position(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	const char* goods_position = luaL_checkstring(L, 2);
	const char* uuid = luaL_checkstring(L, 3);

	v->query().set_position(goods_position, uuid);
	return 0;
}

static int impl_vblock2_cooking_reload(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	const std::string alias_id = luaL_checkstring(L, 2);
	VALIDATE(is_valid_iot_alias_id(alias_id), null_str);
	
	v->cooking().reload_iot_devices2(alias_id);

	// const std::map<tcooking::tiot_device_key2, tiot_device>& iot_devices2 = v->cooking().iot_devices2();
	const std::set<tiot_device>& iot_devices2 = v->cooking().iot_devices2();
	lua_pushinteger(L, iot_devices2.size());

	// items
	int lua_device_size = iot_devices2.size();
	lua_createtable(L, lua_device_size, 0);

	{
		tstack_size_lock lock(L, 0);
		int at = 0;
		for (std::set<tiot_device>::const_iterator it = iot_devices2.begin(); it != iot_devices2.end(); ++ it, at ++) {
			const tiot_device& device = *it;
			VALIDATE(device.number >= 1, null_str);
			v->cooking().lua_push_device(L, device);

			lua_rawseti(L, -2, at + 1);
		}
	}

	return 2;
}

static int impl_vblock2_cooking_set_position(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	const int device_at = luaL_checkinteger(L, 2);
	const char* uuid = luaL_checkstring(L, 3);

	v->cooking().set_position(device_at, uuid);
	return 0;
}

static int impl_vblock2_kbook_reload(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	bool from_file = luaL_checkboolean(L, 2);
	int max_disp_size = luaL_checkinteger(L, 3);

	VALIDATE(max_disp_size <= 200, null_str);

	std::string err_msg;
	if (from_file) {
		VALIDATE(false, null_str);
		err_msg = v->book().reload_from_file();
	}
	err_msg = v->book().reload_from_file();
	
	lua_pushstring(L, err_msg.c_str());
	if (err_msg.empty()) {
		const std::map<int64_t, tkbook::tquestion2>& questions = v->book().questions();
		lua_pushinteger(L, questions.size());

		// questions
		int lua_item_size = SDL_min(max_disp_size, (int)questions.size());
		// const tkbook::tquestion2* items_ptr = &questions[0];
		lua_createtable(L, lua_item_size, 0);

		{
			tstack_size_lock lock(L, 0);
			int at = 0;
			for (std::map<int64_t, tkbook::tquestion2>::const_iterator it = questions.begin(); it != questions.end(); ++ it, at ++) {
				const tkbook::tquestion2& question2 = it->second;
				v->book().lua_push_question2(L, question2);

				lua_rawseti(L, -2, at + 1);
			}
		}

	} else {
		const int count = 0;
		lua_pushinteger(L, count); // count
		lua_createtable(L, count, 0); // questions
		// lua_pushnil(L); // positions
	}
	return 3;
}

void luaW_pushvblock2(lua_State* L, tlua_block& widget)
{
	aplt::tb_api& b_api = aplt::get_b_api();

	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	tlua_block** v = (tlua_block**)lua_newuserdata(L, sizeof(tlua_block*));
	*v = &widget;

	// b_api.call_lua_breakpoint();
	nil_metatable(b_api.get_lua_State(), vblock2MetatableKey);
	// b_api.call_lua_breakpoint();
	
	// see https://www.cswamp.com/post/261
	if (luaL_newmetatable(L, vblock2MetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vblock2_collect},
			{"__index", impl_vblock2_get},

			// tquery_goods
			{"query_reload", impl_vblock2_query_reload},
			{"query_set_position", impl_vblock2_query_set_position},

			// tcooking
			{"cooking_reload", impl_vblock2_cooking_reload},
			{"cooking_set_position", impl_vblock2_cooking_set_position},

			// tkbook
			{"kbook_reload", impl_vblock2_kbook_reload},

			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vblock2MetatableKey);

	} else {
		VALIDATE(false, null_str);
	}

	lua_setmetatable(L, -2);
}

}
