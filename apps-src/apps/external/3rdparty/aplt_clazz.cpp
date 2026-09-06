

#include "aplt_clazz.hpp"
#include "aplt_common.hpp"
#include "rose_exception.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_string_utils_dll.hpp"


namespace aplt {

std::map<int, tcode3> amp_modes;

int amp_mode_from_str(const std::string& str, bool nposm_to_1x)
{
	VALIDATE(!amp_modes.empty(), null_str);

    if (!str.empty()) {
	    for (std::map<int, tcode3>::const_iterator it = amp_modes.begin(); it != amp_modes.end(); ++ it) {
		    const tcode3& mode = it->second;
		    if (mode.id == str) {
			    return it->first;
		    }
	    }
    }
	return nposm_to_1x? ampmode_1x: nposm;
}

//
// pinyin section
//
static tpinyin* curr_pinyin = nullptr;

tpinyin& get_curr_pinyin()
{
	VALIDATE(curr_pinyin != nullptr, null_str);
	return *curr_pinyin;
}

tpinyin::tpinyin()
	: sid_(speakid_nposm)
{
	VALIDATE(curr_pinyin == nullptr, null_str);
	curr_pinyin = this;
}

tpinyin::~tpinyin()
{
	VALIDATE(curr_pinyin != nullptr, null_str);
	curr_pinyin = nullptr;
}

uint32_t tpinyin::next_id()
{
	do {
		++ sid_;
	} while (sid_ == speakid_nposm || sid_ == speakid_repeat);

	return sid_;
}

//
// tvaluex
//
void tvaluex::nposm_NMTHREAD_battery_level()
{
	VALIDATE_IN_MAIN_THREAD();
	NMTHREAD_battery_level_ = float_nposm;
}

void tvaluex::set_NMTHREAD_battery_level(double level)
{
	VALIDATE_NOT_MAIN_THREAD();

	threading::lock lock(mutex_);
	NMTHREAD_battery_level_ = level;
	last_NMTHREAD_battery_level_ticks_ = SDL_GetTicks();
}

void tvaluex::flip_battery_level()
{
	VALIDATE_IN_MAIN_THREAD();

	threading::lock lock(mutex_);
	if (!is_float_nposm(NMTHREAD_battery_level_)) {
		battery_level = NMTHREAD_battery_level_;
	}
}

uint32_t tvaluex::last_NMTHREAD_battery_level_ticks()
{ 
	VALIDATE_IN_MAIN_THREAD();

	threading::lock lock(mutex_);
	return last_NMTHREAD_battery_level_ticks_; 
}

tvaluex valuex;

//
// trpy_sensor
//

static trpy_sensor* curr_rpy_sensor = nullptr;
LIB3RDPARTY_DECL trpy_sensor& get_rpy_sensor()
{
    VALIDATE(curr_rpy_sensor != nullptr, null_str);
	return *curr_rpy_sensor;
}

trpy_sensor::trpy_sensor()
    : accel_sensor_(nullptr)
    , pitch_(0.0f)
    , roll_(0.0f)
    , has_data_(false)
    , initialized_(false)
{
    VALIDATE(curr_rpy_sensor == nullptr, null_str);
	curr_rpy_sensor = this;
}

trpy_sensor::~trpy_sensor()
{
    quit();

    VALIDATE(curr_rpy_sensor != nullptr, null_str);
	curr_rpy_sensor = nullptr;
}

bool trpy_sensor::init()
{
    VALIDATE(!initialized_, null_str);
    // If already initialized, clean up first.
    if (initialized_) {
        quit();
    }
    
    // Find and open the accelerometer.
    int sensorCount = SDL_NumSensors();
    if (sensorCount <= 0) {
        SDL_Log("No sensors found.");
        return false;
    }
    
    for (int i = 0; i < sensorCount; i++) {
        SDL_SensorType type = SDL_SensorGetDeviceType(i);
        if (type == SDL_SENSOR_ACCEL) {
            accel_sensor_ = SDL_SensorOpen(i);
            if (accel_sensor_) {
                SDL_Log("Accelerometer opened: %s", SDL_SensorGetDeviceName(i));
                initialized_ = true;
                has_data_ = false;
                pitch_ = 0.0f;
                roll_ = 0.0f;
                return true;
            }
        }
    }
    
    SDL_Log("Accelerometer not found.");
    return false;
}

void trpy_sensor::update()
{
/*
    if (!initialized_ || accel_sensor_ == nullptr) {
        return;
    }
*/   
    VALIDATE(initialized_ && accel_sensor_ != nullptr, null_str);

    float data[3];
    int result = SDL_SensorGetData(accel_sensor_, data, 3);
    if (result == 0) {
        float ax = data[0];
        float ay = data[1];
        float az = data[2];
        
        // Calculate Pitch (tilt angle): rotation around the X-axis.
        float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
		// float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
		pitch_ = pitch_filter_.update(pitch);
        
        // Calculate Roll (bank angle): rotation around the Y-axis.
        roll_ = atan2f(ay, az) * 180.0f / M_PI;
		// roll_ = atan2f(ay, az);

		if (game_config::os == os_ios) {
			pitch_ = -pitch_;
		}
        
		// SDL_Log("trpy_sensor(num == 3)roll: %.3f, pitch: %.3f", RAD2DEG(roll_), RAD2DEG(pitch_));

        has_data_ = true;

    } else {
		// SDL_Log("trpy_sensor(num(%i) != 3)", num);
	}
}

int trpy_sensor::pitch_vertical_level(float* pitch_ptr) const
{
    if (pitch_ptr != nullptr) {
        *pitch_ptr = pitch_;
    }
    int result = level_fail;
    if (!has_data_) {
        return result;
    }
    
	if (pitch_ <= -87) {
		result = level_ok;

	} else if (pitch_ <= -85) {
		result = level_warn;
	}
	return result;
}

void trpy_sensor::quit()
{
    if (accel_sensor_ != nullptr) {
        SDL_SensorClose(accel_sensor_);
        accel_sensor_ = nullptr;
    }
    
    initialized_ = false;
    has_data_ = false;
    pitch_ = 0.0f;
    roll_ = 0.0f;
}

}