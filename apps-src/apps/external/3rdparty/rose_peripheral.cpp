#include "rose_global.hpp"
#include "rose_peripheral.hpp"
#include <SDL.h>
#include <SDL_peripheral.h>
// #include "rose_config.hpp"
#include "rose_exception.hpp"
#include "rose_string_utils.hpp"

using namespace std::placeholders;

//
// tserial
//
tserial::tserial(const std::string& path, int baudrate)
	: serial_path_(path)
	, serial_(SDL_INVALID_HANDLE_VALUE)
	, baundrate_(baudrate)
	// , invalids_(0)
	, fails_(0)
	, recv_data_(nullptr)
	, recv_data_size_(0)
	, recv_data_vsize_(0)
{
	if (!serial_path_.empty() && baundrate_ != nposm) {
		serial_ = SDL_OpenSerialPort(serial_path_.c_str(), baundrate_);
	}

	resize_recv_data(8192);
}

tserial::~tserial()
{
	SDL_CloseSerialPort(serial_);
	serial_ = SDL_INVALID_HANDLE_VALUE;

	if (recv_data_) {
		free(recv_data_);
		recv_data_ = nullptr;
	}
}

bool tserial::send_data(const uint8_t* data, int len)
{
	if (serial_ == SDL_INVALID_HANDLE_VALUE) {
		// invalids_ ++;
		return false;
	}

	VALIDATE(data && len > 0, null_str);

	// std::string str = utils::hex_encode_cstyle((const char*)data, len, ' ');
	// SDL_Log("%i, tserial::send_data: %s", SDL_GetTicks(), str.c_str());

	return SDL_WriteSerialPort(serial_, data, len) == len;
}

void tserial::resize_recv_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > recv_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (recv_data_ != nullptr) {
			if (recv_data_vsize_ != 0) {
				memcpy(tmp, recv_data_, recv_data_vsize_);
			}
			free(recv_data_);
		}
		recv_data_ = tmp;
		recv_data_size_ = size;
	}
}
/*
size_t read_serial_port_rand(uint8_t* ptr)
{
	VALIDATE(game_config::os == os_windows, null_str);
	static int read_bytes = 0;
	tfile file(game_config::preferences_dir + "/serial.dat", GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (read_bytes == fsize) {
		return 0;
	}
	VALIDATE(read_bytes < fsize, null_str);

	int count = 0;
	while (count == 0) {
		count = rand() % 15;
	}
	if (read_bytes + count > fsize) {
		count = fsize - read_bytes;
	}
	memcpy(ptr, file.data + read_bytes, count);
	read_bytes += count;

	return count;
}
*/

/*
size_t read_serial_port(uint8_t* ptr)
{
	VALIDATE(game_config::os == os_windows, null_str);
	
	const uint8_t header_ = 0xff;
	const uint8_t version_ = 0xfe;

	int len = 0;
	ptr[len ++] = 0x00;
	ptr[len ++] = 0x00;
	ptr[len ++] = 0x00;

	ptr[len ++] = 0xef;

	// ---one time
	ptr[len ++] = header_;
	ptr[len ++] = version_;

	ptr[len ++] = 0x00;
	ptr[len ++] = 0x00;
	ptr[len ++] = 0xff; // checksum

	ptr[len ++] = 0x00; // lo(topid)
	ptr[len ++] = 0x00; // hi(topid)
	ptr[len ++] = 0xff;
	// ---one time

	ptr[len ++] = header_;
	ptr[len ++] = version_;

	// ---one time
	ptr[len ++] = header_;
	ptr[len ++] = version_;

	ptr[len ++] = 0x05;
	ptr[len ++] = 0x00;
	ptr[len ++] = 0xfa; // checksun

	ptr[len ++] = 0x10; // lo(topid)
	ptr[len ++] = 0x00; // hi(topid)

	ptr[len ++] = 0x00;
	ptr[len ++] = 0x00;

	// len == 8

	return len;
}
*/

// >0: The number of bytes read this time
// -1: seiral doesn't open or EFAULT
int tserial::pool_read()
{
	if (serial_ == SDL_INVALID_HANDLE_VALUE) {
		return -1;
	}

	size_t ret = SDL_ReadSerialPort(serial_, recv_data_ + recv_data_vsize_, recv_data_size_ - recv_data_vsize_);

	if (ret == 0) {
		if (errno == EFAULT) {
			fails_ ++;
			SDL_Log("serial_port_slice, SDL_ReadSerialPort fail, errno: %i(EFAULT:%i), total fails: %i", errno, EFAULT, fails_);
			// SDL_CloseSerialPort(serial_);
			// serial_ = SDL_OpenSerialPort(serial_path_.c_str(), baundrate_);
		}
		return errno == EFAULT? -1: 0;
	}
	VALIDATE(ret > 0, null_str);

	{
		std::string str = utils::hex_encode_cstyle((const char*)recv_data_, ret, ' ');
		SDL_Log("%u serial read(1.1): %s, ret: %i, recv_data_vsize_: %i", SDL_GetTicks(), str.c_str(), (int)ret, recv_data_vsize_);
	}

	recv_data_vsize_ += (int)ret;
	app_pool_read_bh();

	return (int)ret;
}

//
// tl07serial
//
