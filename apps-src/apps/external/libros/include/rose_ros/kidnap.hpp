#ifndef LIBROS_ROS_KIDNAP_HPP
#define LIBROS_ROS_KIDNAP_HPP

#include <ros/common.h>

#include "rose_util.hpp"
#include "rose_thread.hpp"


class ROSCPP_DECL tkidnap
{
public:
	struct tresult4_C
	{
		int ms;
		int score;
		int points;
		int unknowns;
	};

	tkidnap();

	~tkidnap();

	void set_has_imu(bool has);
	bool has_imu() const { return has_imu_; }
	void did_start_navigation();

	enum {reason_goal, reason_relay, reason_neargoal, reason_clearing, reason_manual, reason_count};
	void set_estimate(int reason, const tpose2d& robot_pose2d);
	enum {clrreason_ok, clrreason_result, clrreason_stop, clrreason_timeout, clrreason_moveout, clrreason_noimu, clrreason_calcimu, clrreason_repositionfail, clrreason_count};
	void clear_estimate(int reason);
	bool estimating() const { return estimate_ != nposm; }
	int estimate() const { return estimate_; }

	bool is_allowed() const;
	bool can_estimate() const;
	bool can_estimate_CLEARING(double x, double y) const;

	void set_position_changed();
	// clear_position_changed maybe called non-main-thread, so cannot uaranteed synchronization.
	// don't verify estimate_, reposition_
	void clear_position_changed() { position_changed_ = false; }
	bool position_changed() const { return position_changed_; }

	// void set_next_move_ticks(uint32_t ticks);
	// uint32_t next_move_ticks() const { return next_move_ticks_; }
	enum vel_state_t {vel_finished, vel_req, vel_pub, vel_count};
	void set_vel_state(vel_state_t state, int move_base_state);
	vel_state_t vel_state() const { return vel_state_; }

	void set_last_reposition_pose2d(const tpose2d& pose2d);
	const tpose2d& last_reposition_pose2d() const { return last_reposition_pose2d_; }

	void set_last_ok_estimated_pose2d(const tpose2d& pose2d);
	void clear_last_ok_estimated_pose2d();
	const tpose2d& last_ok_estimated_pose2d() const { return last_ok_estimated_pose2d_; }
	bool require_relay_estimate(double x, double y) const;
	bool can_estimate_near_goal(double x, double y) const;

	void set_apply_immediately();
	// void clear_apply_immediately();
	bool apply_immediately() const { return apply_immediately_; }
	void set_applied();

	void set_timeout_ticks(uint32_t ticks);
	uint32_t timeout_ticks() const { return timeout_ticks_; }

	void increase_equals();
	void clear_equals();
	int equals() const { return equals_; }

	void set_require_equals(int value);
	int require_equals() const { return require_equals_; }

	enum {msg_set_estimate, msg_clear_estimate, msg_set_last_ok_estimated_pose2d, msg_set_last_reposition_pose2d,
		msg_calc_imu_yaw, msg_update_base_yaw, msg_reposition, msg_set_vel_state, msg_clear_equals, msg_count};
	struct tdbg_msg
	{
		int type;
		int reason;
		int state;
		double yaw;
		tpose2d_C pose2d;
		tresult4_C result4;
		uint32_t ticks;
		int estimated_times;
		int equals;
		int moved_times;
		tpose2d_C last_pose2d;
	};
	void add_dbg_msg_reason(int type, int reason);
	void add_dbg_msg(int type, int reason, int state, const tpose2d_C& pose2d);
	enum {reposition_full2, reposition_full, reposition_center, reposition_count};
	void add_dbg_msg_reposition(int type, int reason, const tpose2d_C& pose2d, const tresult4_C& result4);
	enum {calcimu_initial, calcimu_reposition, calcimu_count};
	void add_dbg_msg_update_base_yaw(int type, double from, double to);
	tcharcdata_C dbg_msgs_to_string(int max_disp_msgs, char nl);

private:
	bool can_estimate_time_interval(bool relay) const;
	void reset_dbg_msgs();
	void resize_dbg_msgs(int increment);
	void resize_dbg_msgs_buf(int size);
	void add_dbg_msg_internal(int type, int reason, int state, double yaw, const tpose2d_C* pose2d_ptr, const tresult4_C* result4_ptr);

private:
	bool has_imu_;
	int estimate_;
	bool position_changed_;
	// uint32_t next_move_ticks_;
	vel_state_t vel_state_;
	tpose2d last_reposition_pose2d_;
	tpose2d last_ok_estimated_pose2d_;
	uint32_t last_estimated_ticks_;
	bool apply_immediately_;
	bool applied_;
	uint32_t timeout_ticks_;
	int equals_;
	int require_equals_;

	uint32_t navigation_start_ticks_;
	tdbg_msg* dbg_msgs_;
	int dbg_msg_vsize_;
	int dbg_msg_size_;
	char* dbg_msgs_buf_;
	int dbg_msg_buf_vsize_;
	int dbg_msg_buf_size_;

public:
	const int MAX_MOVE_FAILS;
	const int TIMEOUT_MS;
	const int MIN_REQUIRE_EQUALS;
	const int MOVE_TIMEOUT_MS;
	const double ESTIMATE_THRESHOLD;
	const double PROHIBIT_REESTIMATE;
	const double NEAR_GOAL;
	tpose2d cartographer_pose2d;
	tpose2d last_pose2d;
	threading::mutex last_pose2d_mutex;

	bool dbg_msgs_dirty;

	uint32_t vel_req_start_ticks;
	tpose2d vel_req_start_pose2d;
	int vel_pub_times;

	int moved_times;
	int estimated_times;
	tresult4_C result4;
};

extern ROSVARIABLE_DECL tkidnap kidnap;

#endif // LIBROS_ROS_KIDNAP_HPP
