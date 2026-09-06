#ifndef LIBROS_YAHBOOMSERIAL_HPP_INCLUDED
#define LIBROS_YAHBOOMSERIAL_HPP_INCLUDED

#include "rose_peripheral.hpp"
#include <set>

class tyahboomserial: public tserial
{
public:
	enum {FUNC_AUTO_REPORT = 0x01,
        FUNC_BEEP = 0x02,
        FUNC_PWM_SERVO = 0x03,
        FUNC_PWM_SERVO_ALL = 0x04,
        FUNC_RGB = 0x05,
        FUNC_RGB_EFFECT = 0x06,

        FUNC_REPORT_SPEED = 0x0A,
        FUNC_REPORT_IMU_RAW = 0x0B,
        FUNC_REPORT_IMU_ATT = 0x0C,
        FUNC_REPORT_ENCODER = 0x0D,
        
        FUNC_RESET_STATE = 0x0F,

        FUNC_MOTOR = 0x10,
        FUNC_CAR_RUN = 0x11,
        FUNC_MOTION = 0x12,
        FUNC_SET_MOTOR_PID = 0x13,
        FUNC_SET_YAW_PID = 0x14,
        FUNC_SET_CAR_TYPE = 0x15,

        FUNC_UART_SERVO = 0x20,
        FUNC_UART_SERVO_ID = 0x21,
        FUNC_UART_SERVO_TORQUE = 0x22,
        FUNC_ARM_CTRL = 0x23,
        FUNC_ARM_OFFSET = 0x24,

        FUNC_AKM_DEF_ANGLE = 0x30,
        FUNC_AKM_STEER_ANGLE = 0x31,


        FUNC_REQUEST_DATA = 0x50,
        FUNC_VERSION = 0x51,

        FUNC_RESET_FLASH = 0xA0};

    enum {CARTYPE_X3 = 0x01, CARTYPE_MIN = CARTYPE_X3,
        CARTYPE_X3_PLUS = 0x02,
        CARTYPE_X1 = 0x04,
        CARTYPE_R2 = 0x05, CARTYPE_MAX = CARTYPE_R2,};

	enum status_t {status_idle, wait_wakeup, wait_enable_report, status_report, status_count};

	tyahboomserial(const std::string& path, int baudrate, uint8_t car_type);
	virtual ~tyahboomserial();

	status_t status() const { return status_; }
	bool reporting() const { return status_ == status_report; }

	void send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len);
	void slice();

protected:
    void CAR_RUN_stop();

private:
	void app_pool_read_bh() override;
	void did_read_im948(const uint8_t* data_ptr, int len2);
	void state_slice();

public:
	double euler[3];

protected:
    const uint8_t car_type_;
	const uint8_t prefix_;
	const uint8_t prefix_req_;
	const uint8_t prefix_resp_;
	const uint8_t postfix_;

    double voltage_;


	const int send_mask_ms_;
	std::set<uint8_t> rx_cmds_;
	uint32_t last_report_ticks_;
	uint32_t next_send_ticks_;
    uint32_t next_pub_voltage_ticks_;

	status_t status_;
	uint8_t* send_data_;
	int send_data_size_;
};


#endif // LIBROS_YAHBOOMSERIAL_HPP_INCLUDED
