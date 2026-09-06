#ifndef FACEPRINT_UTIL_HPP_INCLUDED
#define FACEPRINT_UTIL_HPP_INCLUDED

#include "rose_sdl_utils.hpp"
#include "rose_thread.hpp"

#ifdef USE_DFACE

#define DF_FEATURE_SIZE         512

#define DEFAULT_FEATURE_BLOB_SIZE	512 // 701, 1385
#define FEATURE_BLOB_SIZE_1385	1385 // 1385
#define FEATURE_BLOB_SIZE_MULTIVLG	FEATURE_BLOB_SIZE_1385 // 
#define MAX_CS_CARDNUMBER_SIZE	20
#define MAX_CS_CARDNUMBER_VSIZE	15	// MAX_CS_CARDNUMBER_SIZE - 1 - 4, 1 used to '\0'
#define MAX_CS_HOUSE_SIZE		36
#define MAX_AGBOX_NAME_SIZE		20
#define MAX_CS_NAME_SIZE		24 // name in agbox's person/update is 20(MAX_AGBOX_NAME_SIZE)
#define MAX_CS_IDNUMBER_SIZE	40
// #define MAX_CS_UUID_SIZE		36

#define MAX_CS_LIFTS			4
#define MAX_CS_BYTESPERLIFTFLOOR	40

#define cs_feature_ptr_from_list2(list, at)	\
	((faceprint::tcs_feature*)((uint8_t*)(list) + faceprint::sizeof_tcs_feature * (at)))

#define cs_feature_ptr_from_list(at)	cs_feature_ptr_from_list2(faceprint::list_cs_feature, at)

#define cs_feature_ptr_variable(feature_len, name)	tcs_featur##feature_len* name

#define list_feature2(at)	(faceprint::list_feature_base + MAXIMUM_FACES * (at))

#define support_subfacestore(personversion)	((personversion) >= personversion_v9)

namespace faceprint {

enum {gat517_idcard = 111, gat517_employee = 131, gat517_tempo_pass = 153, gat517_passport = 414, 
	gat517_foreigner = 554, gat517_other = 990, gat517_openid = 991, gat517_faceid = 992};
enum {st_sdk1, st_sdk2, face_sdk_count};
enum {personversion_v0, personversion_v9, personversion_count};
enum {inputcardtype_wg26base10 = 1, inputcardtype_wg34base10 = 2, inputcardtype_wg34base16 = 3};
enum {ffreason_area, ffreason_angle, ffreason_similarity, ffreason_singleliveness, ffreason_inouttimes, ffreason_count};

#define MAX_CAMERAS	15 // reader number of f4 is 0xfx, so max is 15(from 1 to 0xf).
#define FAKE_MODEL_STR			"fake.model"

// when villaged <= 1.0.2, app's multiple cameras share a facestore, so only one list_features[x] is used, 
// LE102_INDEX used to indicate which element in list_features is used, and it must be < length of list_features(MAX_CAMERAS).
#define LE102_INDEX		(MAX_CAMERAS - 1)

struct tst_model {
	explicit tst_model(int feature_size, const std::string& verify_model_path, const std::string& example_feature_dat)
		: feature_size(feature_size)
		, verify_model_path(verify_model_path)
		, example_feature_dat(example_feature_dat)
	{}

	const int feature_size;
	const std::string verify_model_path;
	const std::string example_feature_dat;
};

extern std::string res_dir;
extern std::string preferences_dir;
extern std::string sn_path;
extern bool oem_3s2;
extern bool use_facestore;
extern std::set<int> valid_feature_blob_sizes;
extern std::map<int, tst_model> st_models;
extern std::string activecode_file_name;
extern int dtype;
extern int FEATURE_BLOB_SIZE;
extern int MAXIMUM_FACES;

void copy_model_2_preferences(const std::string& src_dir);
std::map<int, tst_model> get_st_models(const std::string& str);
bool authorize(bool onlinestsdk);

enum {
	feature_flag_visitor = 0x01000000,
	feature_flag_fake = 0x02000000
};
#define PERSONTYPE_FROM_PERSONTYPE2(type)	((type) & 0x00ffffff)
#define VISITOR_FROM_PERSONTYPE2(type)		(!!((type) & feature_flag_visitor))
#define FAKE_FROM_PERSONTYPE2(type)		(!!((type) & feature_flag_fake))

#pragma pack(1)
struct tcs_feature_v0 { // tcs_feature_v0
	int32_t faceid;
	int64_t ts;
	// in iaccess, bit31 of persontype indicate whether owner or visitor.
	int32_t persontype;
	int32_t idtype;
	// all 'char' type fields maybe no '\0'.
	char idnumber[MAX_CS_IDNUMBER_SIZE];
	char card[MAX_CS_CARDNUMBER_SIZE];
	char house[MAX_CS_HOUSE_SIZE];
	char name[MAX_CS_NAME_SIZE];
	// feature must be last field of tcs_feature_v0.
	char feature[DEFAULT_FEATURE_BLOB_SIZE];
};
// sizeof(tcs_feature_v0) = 841

struct tcs_lift {
	uint8_t floors[MAX_CS_BYTESPERLIFTFLOOR];
	uint8_t floor;
	uint8_t flags;
};

struct tcs_feature_v9 { // tcs_feature_v9
	int32_t faceid;
	int64_t ts;
	int32_t persontype;
	int32_t idtype;
	char idnumber[MAX_CS_IDNUMBER_SIZE];
	char card[MAX_CS_CARDNUMBER_SIZE];
	char house[MAX_CS_HOUSE_SIZE];
	char name[MAX_CS_NAME_SIZE];
	char feature[DEFAULT_FEATURE_BLOB_SIZE];
	uint32_t camera_mask;
	tcs_lift lifts[MAX_CS_LIFTS];
};
// sizeof(tcs_feature_v9) = 1013

struct tcs_feature_bh_v9 {
	char house[MAX_CS_HOUSE_SIZE];
	char name[MAX_CS_NAME_SIZE];
	uint32_t camera_mask;
	tcs_lift lifts[MAX_CS_LIFTS];
};

#pragma pack()

typedef tcs_feature_v0	tcs_feature;

extern int sizeof_tcs_feature;

extern bool facestore_initialized;
extern bool gauth_facestore_init_called;
extern int list_size;
extern char** list_feature_base;
extern int list_cs_vsize;
extern tcs_feature* list_cs_feature;
extern int64_t facestore_ts;

struct tlist_feature {
	char** features;
	int vsize;
	int validate_tmp; // only used when validate_facestore
};
extern tlist_feature list_features[MAX_CAMERAS];
extern std::unique_ptr<threading::mutex> facestore_mutex;

struct tfacestore_header {
	uint32_t fourcc;
	uint32_t version;
	uint32_t count;
	uint64_t facestore_ts;
};

int slot_positions();
int slot_personversion();
int slot_inputcardtype();

void recalculate_sizeof_tcs_feature();
void allocate_list_feature(int max_faces);
void free_list_feature();
void load_facestore(int64_t server_facestore_ts, const char* data, int size, bool sha1);
void clear_list_feature();
void sync_facestore(int64_t server_facestore_ts, const char* data, int size, int modifies, bool sha1);
const tcs_feature* is_card_existed(const uint32_t card);
const tcs_feature* is_faceid_existed(const int faceid);
uint32_t faceid_2_card(int faceid);
bool in_blacklist(int faceid);
const tcs_feature_bh_v9* faceid_2_cs_feature_bh_v9(int faceid, bool must_exist);

struct tcs_person {
	tcs_person()
	{
		clear();
	}

	bool valid() const { return faceid != nposm; }
	void clear()
	{
		faceid = nposm;
		ts = 0;
		persontype = 0; // default is 0, not nposm. reference to calculate_agbox_eventcode
		idtype = nposm;
		idnumber.clear();
		card = 0;
		name.clear();
		house.clear();
	}

	int faceid;
	int64_t ts;
	int persontype;
	int idtype;
	std::string idnumber;
	uint32_t card;
	std::string card_str;
	std::string name;
	std::string house;
};
void tcs_feature_2_cs_person(const tcs_feature* cs_feature, tcs_person& person);
tcs_person faceid_2_cs_person(int faceid);
tcs_person cardnumber_2_cs_person(const uint32_t cardnumber);
void recalculate_card_u32();

extern bool facestore_dirty;
extern uint32_t last_write_facestore_ticks;
void write_facestore_dat();
void load_facestore_dat();

void gauth_feature_task_2_facestore();
void gauth_green_local_facestore();
void gauth_sync_facestore_erase(const std::set<std::string>& signouted_ids, int64_t max_ts);

struct tmodify_task
{
	tmodify_task(int _camera_at, const int _idtype, const std::string& _idnumber, uint32_t _flags, int64_t _ts, 
		int _persontype, const std::string& _name, const std::string& _card, const char* _feature_data, int _feature_size)
		: camera_at(_camera_at)
		, faceid(nposm)
		, idtype(_idtype)
		, idnumber(_idnumber)
		, flags(_flags)
		, ts(_ts)
		, name(_name)
		, persontype(_persontype)
		, card(_card)
		, feature_data(_feature_data)
		, feature_size(_feature_size)
	{
		if (idtype != nposm) {
			VALIDATE((idtype == gat517_idcard || idtype == gat517_employee) && !idnumber.empty(), null_str);
		} else {
			// (idtype, idnumber) will generate by insert_or_modify_facestore_v9.
			// idtype is gat517_faceid always, and idnumber is facid's string format.
			VALIDATE(idnumber.empty(), null_str);
		}
		if (flags & FLAG_FEATURE_DATA) {
			VALIDATE(feature_data != nullptr && feature_size == FEATURE_BLOB_SIZE, null_str);
		}
		if (flags & FLAG_NAME) {
			VALIDATE(!name.empty(), null_str);
		}
	}

	tmodify_task(int _camera_at, int _faceid, uint32_t _flags, int64_t val)
		: camera_at(_camera_at)
		, faceid(_faceid)
		, flags(_flags)
	{
		VALIDATE(faceid != nposm, null_str);
		if (flags == FLAG_TS) {
			ts = val;

		} else if (flags == FLAG_PERSONTYPE) {
			persontype = val;

		} else {
			VALIDATE(false, null_str);
		}
	}

	tmodify_task(int _camera_at, int _faceid, uint32_t _flags, const std::string& val)
		: camera_at(_camera_at)
		, faceid(_faceid)
		, flags(_flags)
	{
		VALIDATE(faceid != nposm, null_str);
		if (flags == FLAG_NAME) {
			name = val;

		} else if (flags == FLAG_CARD) {
			card = val;

		} else {
			VALIDATE(false, null_str);
		}
	}

	enum {FLAG_TS = 0x1, FLAG_PERSONTYPE = 0x2, FLAG_NAME = 0x4, FLAG_CARD = 0x8, 
		FLAG_FEATURE_DATA = 0x10, 
	};

	int camera_at;
	int faceid;
	int idtype;
	std::string idnumber;

	uint32_t flags;
	int64_t ts;
	int persontype;
	std::string name;
	std::string card;

	const char* feature_data;
	int feature_size;
};
void facestore_insert_or_modify_v9(const std::vector<tmodify_task>& tasks, std::vector<int>* faceids);

struct terase_task
{
	terase_task(int _faceid, int _idtype, const std::string& _idnumber)
		: faceid(_faceid)
		, idtype(_idtype)
		, idnumber(_idnumber)
	{
		if (faceid == nposm) {
			VALIDATE(idtype != nposm && !idnumber.empty(), null_str);
		} else {
			VALIDATE(idtype == nposm && idnumber.empty(), null_str);
		}
	}

	bool operator<(const terase_task& that) const noexcept
	{
		// first faceid != nposm
		if (faceid != nposm || that.faceid != nposm) {
			// at least one faceid != nposm
			if (faceid != nposm) {
				if (that.faceid != nposm) {
					return faceid < that.faceid;
				} else {
					return true;
				}
			}
			// faceid == nposm && that.faceid != nposm
			return false;
		}
		if (idtype != that.idtype) {
			return idtype < that.idtype;
		}
		int cmp = SDL_strcmp(idnumber.c_str(), that.idnumber.c_str());
		return cmp < 0;
	}

	int faceid;
	int idtype;
	std::string idnumber;
};
void facestore_erase(const std::set<terase_task>& erases, int64_t max_ts);

}

#endif

#endif