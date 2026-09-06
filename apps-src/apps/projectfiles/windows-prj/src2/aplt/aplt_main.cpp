#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "aplt_common.hpp"
#include "rose_exception.hpp"
#include "aplt2.hpp"
#include <rose_ros/aplt.hpp>

namespace aplt {
aplt::tapplet* curr_aplt = nullptr;
}

void aplt_load(int src, const char* bundleid)
{
    aplt::tb_api& b_api = aplt::get_b_api();
    const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api.const_applets();

    const aplt::tapplet* aplt = aplt::aplt_from_id2(applets, src, bundleid);
    VALIDATE(aplt != nullptr, null_str);
    aplt::curr_aplt = const_cast<aplt::tapplet*>(aplt);
}

void aplt_unload()
{
    VALIDATE(aplt::curr_aplt != nullptr, null_str);
    aplt::curr_aplt = nullptr;
}
