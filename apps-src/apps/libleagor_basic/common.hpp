#ifndef LIBLEAGOR_COMMON_HPP
#define LIBLEAGOR_COMMON_HPP


#include "rose_util.hpp"
#include "aplt_clazz.hpp"
#include "rose_thread.hpp"

enum {product_doll, product_wheeltec_box, product_wheeltec_open, product_wheeltec_open_m, 
	product_rosserial, product_yahboom, base_product_count};

#define base_product_is_moveable(product)	((product) > product_doll)
// #define base_product_use_camera(product)	((product) == product_doll)

int rosserial_cpp__node(bool& exit, const std::string& serial_path, int baudrate);
int wheeltec_base__node(bool& exit, const std::string& serial_path, int baudrate);
int yahboom_base__node(bool& exit, const std::string& serial_path, int baudrate);

int base_product_from_str(const std::string& product);
void start_base__node(bool& exit, const std::string& serial_path, int baudrate, int product);

namespace aplt {

// same as home.lua
enum {cpp_id_save_xfyun_3fields = cpp_id_aplt_min, cpp_id_save_iot_5fields, cpp_id_save_ai_1fields};

struct txf3params
{
	txf3params()
		: disable_recognition(false)
		, disable_question(false)
		, voice_threshold(nposm)
		, goaling_enable(false)
		, maxtoken(50)
	{}

	threading::mutex mutex;

	bool disable_recognition;
	bool disable_question;
	int voice_threshold;
	bool goaling_enable;

	std::string spark_ver;
	std::string appid;
	std::string apisecret;
	std::string apikey;
	int maxtoken;
};

}


#endif