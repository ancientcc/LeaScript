#ifndef LIBROSE_LLAMPCP_HPP_INCLUDED
#define LIBROSE_LLAMPCP_HPP_INCLUDED

#include "rose_peripheral.hpp"
#include <set>
#include "base_slot.hpp"

class tl07serial: public tserial
{
public:
	tl07serial(const std::string& path, int baudrate);
	virtual ~tl07serial();

protected:
	void resize_send_data(int size);
	uint8_t calculate_sum(const uint8_t* data, int len) const;

	void app_pool_read_bh() override;

	void send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len);

protected:
	const uint8_t prefix_;
	const int fixed_4bytes_;
	std::set<uint16_t> len_cmds_;

	uint8_t* send_data_;
	int send_data_size_;
};

class tllampcpserial: public tl07serial
{
public:
	enum {SET_GLOBAL_SETTINGS = 0x20,
		SET_BRIGHTNESS = 0x21,
		SET_COLOR_TEMPERATURE = 0x22,
		CTRL_LED = 0x23,
		SET_BUTTON_THRESHOLD = 0x24
	};

	tllampcpserial(aplt::tbase_ext_lamp& ext_lamp, const std::string& path, int baudrate);
	~tllampcpserial();


	void set_global_settings(int longpress_mul10);
	void set_brightness(int value);
	void set_color_temperature(int index);
	void ctrl_led(int type, uint8_t value);
	void set_button_threshold(int power, int non_power);

private:
	void app_pool_read_bh();
	void did_read_cmd(const uint8_t* data_ptr, int len2) override;

private:
	aplt::tbase_ext_lamp& ext_lamp_;
	bool initialized_;
	bool new_fw_;
};

tllampcpserial* llampcpserial_open(aplt::tbase_ext_lamp& ext_lamp, const std::string& serial_path, int baudrate);

#endif // LIBROSE_LLAMPCP_HPP_INCLUDED
