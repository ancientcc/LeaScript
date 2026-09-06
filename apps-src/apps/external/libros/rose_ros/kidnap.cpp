
#include "rose_ros/kidnap.hpp"
#include <angles/angles.h>

#include "rose_exception.hpp"
#include <SDL_timer.h>
#include <SDL_log.h>
#include <rose_ros/utils.hpp>


tkidnap::tkidnap()
	: has_imu_(false)
	, estimate_(nposm)
	, position_changed_(false)
	, vel_state_(vel_finished)
	, last_estimated_ticks_(0)
	, apply_immediately_(false)
	, applied_(false)
	, timeout_ticks_(0)
	, equals_(0)
	, require_equals_(0)
	, navigation_start_ticks_(0)
	, dbg_msgs_(nullptr)
	, dbg_msg_vsize_(0)
	, dbg_msg_size_(0)
	, dbg_msgs_buf_(nullptr)
	, dbg_msg_buf_vsize_(0)
	, dbg_msg_buf_size_(0)
	, MAX_MOVE_FAILS(5) // add one fixed, total has 6 reposition.
	, TIMEOUT_MS(30000) // 30000, assume require move 5 time, 3 seconds every move, require 15 second just to move.
	, MIN_REQUIRE_EQUALS(2)
	, MOVE_TIMEOUT_MS(3000)
	, ESTIMATE_THRESHOLD(5.0) // 10 10m
	, PROHIBIT_REESTIMATE(2.0) // 2m
	, NEAR_GOAL(1.5) // 1.5m
	, dbg_msgs_dirty(false)
	, moved_times(0)
	, estimated_times(0)
{
	resize_dbg_msgs(128);
}

tkidnap::~tkidnap()
{
	if (dbg_msgs_ != nullptr) {
		free(dbg_msgs_);
		dbg_msgs_ = nullptr;
	}

	if (dbg_msgs_buf_ != nullptr) {
		free(dbg_msgs_buf_);
		dbg_msgs_buf_ = nullptr;
	}
}

void tkidnap::set_has_imu(bool has) 
{ 
	has_imu_ = has;
}

void tkidnap::did_start_navigation()
{
	navigation_start_ticks_ = SDL_GetTicks();
	estimated_times = 0;
	reset_dbg_msgs();
}

void tkidnap::set_estimate(int reason, const tpose2d& robot_pose2d)
{
	VALIDATE(reason >= 0 && reason < reason_count, null_str);
	VALIDATE(estimate_ == nposm, null_str);
	VALIDATE(has_imu_, null_str);
	VALIDATE(timeout_ticks_ == 0, null_str);
	VALIDATE(!apply_immediately_, null_str);
	VALIDATE(!applied_, null_str);
	// VALIDATE(vel_state_ == vel_finished, null_str);

	SDL_Log("%u {kestimate}set_estimate(reason: %i) robot_pose2d: %s", SDL_GetTicks(), reason, robot_pose2d.to_string().c_str());

	threading::lock lock(last_pose2d_mutex);
	timeout_ticks_ = SDL_GetTicks() + TIMEOUT_MS;
	// set_vel_state(...); this require (estimate_ != nposm)
	vel_state_ = vel_finished;

	// clear_equals(); this require (estimate_ != nposm)
	equals_ = 0;
	require_equals_ = MIN_REQUIRE_EQUALS;

	moved_times = -1;
	last_reposition_pose2d_ = robot_pose2d;

	// estimate_ is set last
	estimate_ = reason;

	if (estimated_times == 0) {
		set_apply_immediately();
	}

	add_dbg_msg(msg_set_estimate, reason, nposm, tpose2d_C{robot_pose2d.x, robot_pose2d.y, robot_pose2d.yaw, robot_pose2d.valid});
}

void tkidnap::clear_estimate(int reason)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(estimate_ != nposm, null_str);

	SDL_Log("%u {kestimate}clear_estimate", SDL_GetTicks());
	threading::lock lock(last_pose2d_mutex);

	// clear_apply_immediately();
	apply_immediately_ = false;
	applied_ = false;
	timeout_ticks_ = 0;
	last_estimated_ticks_ = SDL_GetTicks();

	if (reason != clrreason_ok) {
		// set_last_ok_estimated_pose2d(last_pose2d);
	}

	add_dbg_msg_reason(msg_clear_estimate, reason);
	estimated_times ++;

	estimate_ = nposm;
}

bool tkidnap::is_allowed() const
{
	if (!has_imu_) {
		return false;
	}
	if (ros::buildmap) {
		return false;
	}
	return true;
}

bool tkidnap::can_estimate() const
{
	if (estimate_ != nposm) {
		return false;
	}
	if (!has_imu_) {
		return false;
	}
	if (ros::buildmap) {
		return false;
	}
	return true;
}

bool tkidnap::can_estimate_time_interval(bool relay) const
{
	const int min_interval = relay? 20000: 10000; // 20/10 sec
	return (int)(SDL_GetTicks() - last_estimated_ticks_) >= min_interval;
}

bool tkidnap::can_estimate_CLEARING(double x, double y) const
{
	if (!can_estimate()) {
		SDL_Log("%u {kestimate}can_estimate_CLEARING fail. (3.1)!can_estimate()", SDL_GetTicks());
		return false;
	}

	if (!last_ok_estimated_pose2d_.valid) {
		// SDL_Log("%u {kestimate}can_estimate_CLEARING true. !last_ok_estimated_pose2d_.valid", SDL_GetTicks());
		return true;
	}

	const int min_CLEARING_interval_ms = 7000; // 7 sec (DEFAULT_OSCILLATION_TIMOUT - 3sec)
	const int elapse = SDL_GetTicks() - last_estimated_ticks_;
	if (elapse >= min_CLEARING_interval_ms) {
		return true;
	}

	double x_diff = last_ok_estimated_pose2d_.x - x;
	double y_diff = last_ok_estimated_pose2d_.y - y;
	double sq_dist = x_diff * x_diff + y_diff * y_diff;
	const double min_CLEARING_dist = 2.0; // 2.0 m
	bool result = sq_dist >= min_CLEARING_dist * min_CLEARING_dist;

	if (!result) {
		SDL_Log("%u {kestimate}can_estimate_CLEARING fail. (3.3)Both time(%i ms) and distance(%.3f) don't meet the criteria", 
			SDL_GetTicks(), elapse, sqrt(sq_dist));
	}
	return result;
}

bool tkidnap::require_relay_estimate(double x, double y) const
{
	if (!can_estimate()) {
		return false;
	}

	if (!last_ok_estimated_pose2d_.valid) {
		return false;
	}

	if (!can_estimate_time_interval(true)) {
		return false;
	}

	const int min_relay_interval_ms = 60000; // 60 sec
	if ((int)(SDL_GetTicks() - last_estimated_ticks_) >= min_relay_interval_ms) {
		// is base_yaw is error, robots may be constantly wandering around.
		return true;
	}

	double x_diff = last_ok_estimated_pose2d_.x - x;
	double y_diff = last_ok_estimated_pose2d_.y - y;
	bool result = x_diff * x_diff + y_diff * y_diff >= ESTIMATE_THRESHOLD * ESTIMATE_THRESHOLD;
	// dist > ESTIMATE_THRESHOLD && time_interval >= min_interval

	return result;
}

bool tkidnap::can_estimate_near_goal(double x, double y) const
{
	// Caller must be guaranteed to be no more than 'NEAR_GOAL' meters to goal-point
	if (!can_estimate()) {
		return false;
	}

	if (!last_ok_estimated_pose2d_.valid) {
		return false;
	}

	// Use a large time_interval to avoid frequent neargoal estimate.
	// It may be that the map will be inaccurate after time_interval, and it require multiple neargoal estimate,
	if (!can_estimate_time_interval(true)) {
		return false;
	}

	return true;
}

void tkidnap::set_position_changed()
{
	// VALIDATE(estimate_ == nposm, null_str);

	position_changed_ = true;
}

void tkidnap::set_vel_state(vel_state_t state, int move_base_state)
{
	VALIDATE(state == vel_finished || state == vel_req, null_str);
	if (estimate_ == nposm) {
		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "estimate_: %i, curr vel_state_: %i, desire state: %i", estimate_, vel_state_, state);
		VALIDATE(false, buf);
	}
	VALIDATE(state != vel_state_, null_str);
	if (state == vel_req) {
		VALIDATE_IN_MAIN_THREAD();
		vel_req_start_ticks = SDL_GetTicks();
		vel_req_start_pose2d = last_pose2d;
		vel_pub_times = 0;
	}

	vel_state_ = state;

	tpose2d_C pose2d{0, 0, 0};
	add_dbg_msg(msg_set_vel_state, state, move_base_state, pose2d);
}

void tkidnap::set_last_reposition_pose2d(const tpose2d& pose2d)
{
	// VALIDATE(pose2d.valid, null_str);
	VALIDATE(estimate_ != nposm, null_str);
	last_reposition_pose2d_ = pose2d;

	add_dbg_msg(msg_set_last_reposition_pose2d, nposm, nposm, tpose2d_C{pose2d.x, pose2d.y, pose2d.yaw});
}

void tkidnap::set_last_ok_estimated_pose2d(const tpose2d& pose2d)
{
	VALIDATE(pose2d.valid, null_str);
	VALIDATE(estimate_ != nposm, null_str);

	SDL_Log("%u {kestimate}set_last_ok_estimated_pose2d(pose2d: %s)", SDL_GetTicks(), pose2d.to_string().c_str());
	last_ok_estimated_pose2d_ = pose2d;

	add_dbg_msg(msg_set_last_ok_estimated_pose2d, nposm, nposm, tpose2d_C{pose2d.x, pose2d.y, pose2d.yaw});
}

void tkidnap::clear_last_ok_estimated_pose2d()
{
	last_ok_estimated_pose2d_ = tpose2d();
}

void tkidnap::set_apply_immediately()
{
	VALIDATE(estimate_ != nposm, null_str);
	if (!applied_) {
		apply_immediately_ = true;
	} else {
		VALIDATE(!apply_immediately_, null_str);
	}
}
/*
void tkidnap::clear_apply_immediately()
{
	VALIDATE(estimate_ != nposm, null_str);
	apply_immediately_ = false;
}
*/
void tkidnap::set_applied()
{
	VALIDATE(estimate_ != nposm, null_str);
	VALIDATE(apply_immediately_, null_str);
	VALIDATE(!applied_, null_str);

	apply_immediately_ = false;
	applied_ = true;
}

void tkidnap::set_timeout_ticks(uint32_t ticks)
{
	VALIDATE(estimate_ != nposm, null_str);
	VALIDATE(timeout_ticks_ != 0 && ticks > timeout_ticks_, null_str);

	timeout_ticks_ = ticks;
}

void tkidnap::increase_equals()
{
	VALIDATE(estimate_ != nposm, null_str);
	equals_ ++;
}

void tkidnap::clear_equals()
{
	VALIDATE(estimate_ != nposm, null_str);
	equals_ = 0;

	add_dbg_msg_reason(msg_clear_equals, nposm);
}

void tkidnap::set_require_equals(int value)
{
	VALIDATE(estimate_ != nposm, null_str);
	VALIDATE(value >= MIN_REQUIRE_EQUALS, null_str);

	require_equals_ = value;
}

void tkidnap::reset_dbg_msgs()
{
	dbg_msg_vsize_ = 0;
	// dbg_msgs_dirty = false;
}

void tkidnap::add_dbg_msg_reason(int type, int reason)
{
	add_dbg_msg_internal(type, reason, nposm, 0.0, nullptr, nullptr);
}

void tkidnap::add_dbg_msg(int type, int reason, int state, const tpose2d_C& pose2d)
{
	add_dbg_msg_internal(type, reason, state, 0.0, &pose2d, nullptr);
}

void tkidnap::add_dbg_msg_reposition(int type, int reason, const tpose2d_C& pose2d, const tresult4_C& result4)
{
	add_dbg_msg_internal(type, reason, nposm, 0.0, &pose2d, &result4);
}

void tkidnap::add_dbg_msg_update_base_yaw(int type, double from, double to)
{
	tpose2d_C to_pose2d{0.0, 0.0, to, true};
	add_dbg_msg_internal(type, nposm, nposm, from, &to_pose2d, nullptr);
}

void tkidnap::add_dbg_msg_internal(int type, int reason, int state, double yaw, const tpose2d_C* pose2d_ptr, const tresult4_C* result4_ptr)
{
	resize_dbg_msgs(1);
	tdbg_msg* to_msg = dbg_msgs_ + dbg_msg_vsize_;

	to_msg->type = type;
	to_msg->reason = reason;
	to_msg->state = state;
	to_msg->yaw = yaw;
	if (pose2d_ptr != nullptr) {
		to_msg->pose2d.x = pose2d_ptr->x;
		to_msg->pose2d.y = pose2d_ptr->y;
		to_msg->pose2d.yaw = pose2d_ptr->yaw;
		to_msg->pose2d.valid = pose2d_ptr->valid;
	}
	if (result4_ptr != nullptr) {
		to_msg->result4.ms = result4_ptr->ms;
		to_msg->result4.score = result4_ptr->score;
		to_msg->result4.points = result4_ptr->points;
		to_msg->result4.unknowns = result4_ptr->unknowns;
	}

	to_msg->ticks = SDL_GetTicks() - navigation_start_ticks_;
	to_msg->estimated_times = estimated_times;
	to_msg->equals = equals_;
	to_msg->moved_times = moved_times;
	to_msg->last_pose2d = tpose2d_C{last_pose2d.x, last_pose2d.y, last_pose2d.yaw, last_pose2d.valid};

	dbg_msg_vsize_ ++;

	dbg_msgs_dirty = true;
}

void tkidnap::resize_dbg_msgs(int increment)
{
	int desire_size = dbg_msg_vsize_ + increment;
	if (dbg_msg_vsize_ + increment <= dbg_msg_size_) {
		return;
	}

	desire_size = posix_align_ceil(desire_size, 32);

	tdbg_msg* tmp = (tdbg_msg*)malloc(desire_size * sizeof(tdbg_msg));
	if (dbg_msg_vsize_ != 0) {
		memcpy(tmp, dbg_msgs_, dbg_msg_vsize_ * sizeof(tdbg_msg));
		free(dbg_msgs_);
	}
	dbg_msgs_ = tmp;
	dbg_msg_size_ = desire_size;
}

void tkidnap::resize_dbg_msgs_buf(int size)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > dbg_msg_buf_size_) {
		char* tmp = (char*)malloc(size);
		if (dbg_msgs_buf_ != nullptr) {
			if (dbg_msg_buf_vsize_ != 0) {
				memcpy(tmp, dbg_msgs_buf_, dbg_msg_buf_vsize_);
			}
			free(dbg_msgs_buf_);
		}
		dbg_msgs_buf_ = tmp;
		dbg_msg_buf_size_ = size;
	}
}

// tcharcdata_C.len: Although it exists, but 'tcharcdata.len' don't include '\0'.
tcharcdata_C tkidnap::dbg_msgs_to_string(int max_disp_msgs, char nl) // max_disp_msgs: 22
{
	VALIDATE_IN_MAIN_THREAD();

	tcharcdata_C result{nullptr, 0};
    if (dbg_msg_vsize_ == 0) {
        return result;
    }

	const int maxlen = 128;
	resize_dbg_msgs_buf(maxlen * dbg_msg_vsize_);

	char setreasons[][20] = { "goal", "relay", "neargoal", "clearing", "manual"};
	VALIDATE(sizeof(setreasons) / sizeof(setreasons[0]) == reason_count, null_str);

	char clrreasons[][20] = {"ok", "result", "stop", "timeout", "moveout", "noimu", "calcimu", "repositionfail"};
	VALIDATE(sizeof(clrreasons) / sizeof(clrreasons[0]) == clrreason_count, null_str);

    char repositions[][16] = {"full2", "full", "center"};
	VALIDATE(sizeof(repositions) / sizeof(repositions[0]) == reposition_count, null_str);

	char calcimus[][20] = {"initial", "reposition"};
	VALIDATE(sizeof(calcimus) / sizeof(calcimus[0]) == calcimu_count, null_str);

	char vel_states[][16] = {"finished", "request", "publish"};
	VALIDATE(sizeof(vel_states) / sizeof(vel_states[0]) == vel_count, null_str);

	char MoveBaseStates[][20] = {"PLANNING", "CONTROLLING", "CLEARING"};

    int pos = 0;
    {
        threading::lock lock(last_pose2d_mutex);

        int msg_len = 0;
        int start = 0;
		if (max_disp_msgs != nposm) {
			start = dbg_msg_vsize_ < max_disp_msgs? 0: dbg_msg_vsize_ - max_disp_msgs;
		}
        for (int at = 0; at < dbg_msg_vsize_; at ++) {
            if (at < start) {
                continue;
            }
            if (pos > 0) {
                dbg_msgs_buf_[pos - 1] = '\n';
            }

            char* msg_buf = dbg_msgs_buf_ + pos;
            const tdbg_msg& msg = dbg_msgs_[at];
            int ms = msg.ticks % 1000;

            int ticks = (msg.ticks / 1000) % 3600;
            int sec = ticks % 60;
	        int min = ticks / 60;

            if (msg.type == msg_set_estimate) {
				VALIDATE(msg.reason >= 0 && msg.reason < reason_count, null_str);
                if (msg.estimated_times != 0) {
                    dbg_msgs_buf_[pos ++] = '\n';
                    msg_buf ++;
                }
                if (msg.pose2d.valid) {
                    msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i [#%i]SET: %s%c  {t[%.3f, %.3f]q:%.3f}", 
                        min, sec, ms, msg.estimated_times, setreasons[msg.reason], nl,
						msg.pose2d.x, msg.pose2d.y, RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)));
                } else {
                    msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i [#%i]SET: %s", 
						min, sec, ms, msg.estimated_times, setreasons[msg.reason]);
                }

            } else if (msg.type == msg_clear_estimate) {
				VALIDATE(msg.reason >= 0 && msg.reason < clrreason_count, null_str);
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)CLEAR: %s", 
                    min, sec, ms, msg.equals, msg.moved_times, clrreasons[msg.reason]);

            } else if (msg.type == msg_reposition) {
                VALIDATE(msg.reason >= 0 && msg.reason < reposition_count, null_str);
                if (msg.pose2d.valid) {
                    msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)reposition(%s)%c  {t[%.3f, %.3f]q:%.3f}%c  %ims %i-un:%i %i}", 
                        min, sec, ms, msg.equals, msg.moved_times, repositions[msg.reason], nl,
						msg.pose2d.x, msg.pose2d.y, RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)), nl,
                        msg.result4.ms, msg.result4.points, msg.result4.unknowns, msg.result4.score);
                } else {
                    msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)reposition(%s) fail%c  %ims %i-un:%i %i}",
                        min, sec, ms, msg.equals, msg.moved_times, repositions[msg.reason], nl,
						msg.result4.ms, msg.result4.points, msg.result4.unknowns, msg.result4.score);
                }

            } else if (msg.type == msg_set_last_ok_estimated_pose2d) {
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)set_last_ok_estimated:%c  {t[%.3f, %.3f]q:%.3f}", 
                    min, sec, ms, msg.equals, msg.moved_times, nl, msg.pose2d.x, msg.pose2d.y, RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)));

            } else if (msg.type == msg_set_last_reposition_pose2d) {
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)set_last_reposition:%c  {t[%.3f, %.3f]q:%.3f}", 
                    min, sec, ms, msg.equals, msg.moved_times, nl, msg.pose2d.x, msg.pose2d.y, RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)));

            } else if (msg.type == msg_set_vel_state) {
				VALIDATE(msg.reason >= 0 && msg.reason < vel_count, null_str);
				if (msg.reason == vel_req) {
					msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)vel_state: %s  robot{t[%.3f, %.3f]q:%.3f}", 
						min, sec, ms, msg.equals, msg.moved_times, vel_states[msg.reason],
						msg.last_pose2d.x, msg.last_pose2d.y, RAD2DEG(angles::normalize_angle(msg.last_pose2d.yaw)));
				} else if (msg.reason == vel_finished) {
					VALIDATE(msg.state >= 0 && msg.state < (int)(sizeof(MoveBaseStates) / sizeof(MoveBaseStates[0])), null_str);
					msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)vel_state: %s.%s  robot{t[%.3f, %.3f]q:%.3f}", 
						min, sec, ms, msg.equals, msg.moved_times, vel_states[msg.reason], MoveBaseStates[msg.state],
						msg.last_pose2d.x, msg.last_pose2d.y, RAD2DEG(angles::normalize_angle(msg.last_pose2d.yaw)));
				} else {
					// msg.reason == vel_publish
					VALIDATE(msg.state >= 0 && msg.state < (int)(sizeof(MoveBaseStates) / sizeof(MoveBaseStates[0])), null_str);
					msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)vel_state: %s.%s  vel(%.3f, %.3f, theta:%.3f)  robot{t[%.3f, %.3f]q:%.3f}", 
						min, sec, ms, msg.equals, msg.moved_times, vel_states[msg.reason], MoveBaseStates[msg.state],
						msg.pose2d.x, msg.pose2d.y, RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)),
						msg.last_pose2d.x, msg.last_pose2d.y, RAD2DEG(angles::normalize_angle(msg.last_pose2d.yaw)));
				}

            } else if (msg.type == msg_calc_imu_yaw) {
				VALIDATE(msg.reason >= 0 && msg.reason < calcimu_count, null_str);
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)calc_IMU: %s", 
                    min, sec, ms, msg.equals, msg.moved_times, calcimus[msg.reason]);

            } else if (msg.type == msg_update_base_yaw) {
				VALIDATE(msg.reason == nposm, null_str);
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)update base_yaw:%c  %.3f -> %.3f", 
                    min, sec, ms, msg.equals, msg.moved_times, nl, RAD2DEG(angles::normalize_angle(msg.yaw)), RAD2DEG(angles::normalize_angle(msg.pose2d.yaw)));

            } else if (msg.type == msg_clear_equals) {
				VALIDATE(msg.reason == nposm, null_str);
                msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i (%i&%i)clear_equals", 
                    min, sec, ms, msg.equals, msg.moved_times);

            } else {
                VALIDATE(false, null_str);
            }
 

            // '+1' is '\0'
            // memcpy(total_buf + pos, msg_buf, msg_len + 1);
            pos += msg_len + 1;
        }
        dbg_msgs_dirty = false;
    }

	result.ptr = dbg_msgs_buf_;
	// Although it exists, but 'tcharcdata_C.len' don't include '\0'.
	result.len = pos - 1;

    return result;
}

tkidnap kidnap;
