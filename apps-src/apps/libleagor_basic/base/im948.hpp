#ifndef LIBROS_IM948_HPP_INCLUDED
#define LIBROS_IM948_HPP_INCLUDED

#include "rose_peripheral.hpp"
#include <set>
#include "base_slot.hpp"

class tim948serial: public tserial, public aplt::texternal_imu
{
public:
	enum {ID_SLEEP_SENSOR = 0x2, ID_WAKEUP_SENSOR = 0x3, ID_REPORT_DATA = 0x11, 
		ID_SET_CONFIG = 0x12, ID_DISABLE_REPORT = 0x18, ID_ENABLE_REPORT = 0x19,
		ID_START_CALIBRATION = 0x32, ID_STOP_CALIBRATION = 0x04};
	enum status_t {status_idle, wait_wakeup, wait_enable_report, status_report, status_count};

	tim948serial(const std::string& path, int baudrate, bool use_magnetometer);
	~tim948serial();

	status_t status() const { return status_; }
	bool reporting() const { return status_ == status_report; }

	void send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len);

	void slice() override;
	bool can_read() const override;

private:
	void app_pool_read_bh() override;
	void did_read_im948(const uint8_t* data_ptr, int len2);
	void state_slice();

public:
	double magnetic[4];
	double angular_velocity[4];
	double euler[3];

private:
	const uint8_t prefix_;
	const uint8_t address_;
	const uint8_t postfix_;
	const int send_mask_ms_;
	const bool prefix4_is_magnetic_;
	std::set<uint8_t> rx_cmds_;
	uint32_t last_report_ticks_;
	uint32_t next_send_ticks_;

	status_t status_;
	uint8_t* send_data_;
	int send_data_size_;
};

tim948serial* im948serial_open(const std::string& serial_path, int baudrate, bool use_magnetometer);

#endif // LIBROS_IM948_HPP_INCLUDED
