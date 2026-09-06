#include "rose_net_api.hpp"
#include <SDL.h>
#include "rose_exception.hpp"
#include "rose_string_utils.hpp"

using namespace std::placeholders;

namespace net {

fncreate_http_api create_http_api = nullptr;

static std::map<SDL_threadID, const thttp_api*> thread_http_apis;

thttp_api::thttp_api()
	: OK(0)
{
	SDL_threadID tid = SDL_ThreadID();

	VALIDATE(thread_http_apis.count(tid) == 0, "At any given time, only one thttp_api can be running");
	thread_http_apis.insert(std::make_pair(tid, this));
}

thttp_api::~thttp_api()
{
	SDL_threadID tid = SDL_ThreadID();
	VALIDATE(thread_http_apis.count(tid) != 0, "The thttp_api of this thread has been destroyed");

	std::map<SDL_threadID, const thttp_api*>::iterator it = thread_http_apis.find(tid);
	thread_http_apis.erase(tid);
}

void rose_set_create_http_api(fncreate_http_api fcreate)
{
	create_http_api = fcreate;
}


bool tcswamp_dish::operator<(const tcswamp_dish& that) const noexcept
{
	if (time != that.time) {
		return time < that.time;
	}
	if (count != that.count) {
		return count > that.count;
	}
	return SDL_strcmp(name.c_str(), that.name.c_str()) < 0;
}

std::string tcswamp_dish::to_string(const std::string category, int at) const
{
	char buf[256];
	int pos = 0;
	char* msg_buf = buf + pos;
	int msg_len = SDL_snprintf(msg_buf, sizeof(buf) - pos, "---%s[%i]---\nname: %s\ncount: %i\n", category.c_str(), at,
		name.c_str(), count);
	pos += msg_len;
    msg_buf = buf + pos;

	if (time >= 0) {
		SDL_snprintf(msg_buf, sizeof(buf) - pos, "%s", utils::format_elapse_hms(time).c_str());
	} else {
		SDL_snprintf(msg_buf, sizeof(buf) - pos, "%i", time);
	}

	return buf;
}

std::string tcswamp_table_result::to_string() const
{
	if (not_existed) {
		return "table isn't existed";
	}

	std::stringstream ss;
	ss << "table_name: " << table_name;
	ss << "\ntotal_amount: " << total_amount;
	ss << "\norder_time: " << utils::format_time_ymdhms2(order_time);
	int at = 0;
	for (std::set<tcswamp_dish>::const_iterator it = cooking.begin(); it != cooking.end(); ++ it, at ++) {
		const tcswamp_dish& dish = *it;
		ss << "\n" << dish.to_string("cooking", at);
	}
	at = 0;
	for (std::vector<tcswamp_dish>::const_iterator it = done.begin(); it != done.end(); ++ it, at ++) {
		const tcswamp_dish& dish = *it;
		ss << "\n" << dish.to_string("done", at);
	}
	return ss.str();
}

}

