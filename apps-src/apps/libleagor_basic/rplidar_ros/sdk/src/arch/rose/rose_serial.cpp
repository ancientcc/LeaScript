/*
 *  RPLIDAR SDK
 *
 *  Copyright (c) 2009 - 2014 RoboPeak Team
 *  http://www.robopeak.com
 *  Copyright (c) 2014 - 2019 Shanghai Slamtec Co., Ltd.
 *  http://www.slamtec.com
 *
 */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "../../sdkcommon.h"
#include "rose_serial.h"

#include <SDL_timer.h>
#include <SDL_log.h>
#include <wml_exception.hpp>

namespace rp{ namespace arch{ namespace net{

raw_serial::raw_serial()
    : rp::hal::serial_rxtx()
    , _serial_handle(NULL)
    , _baudrate(0)
    , _flags(0)
    , data_(nullptr)
	, data_size_(0)
    , data_vsize_(0)
{
}

raw_serial::~raw_serial()
{
    close();
    if (data_ != nullptr) {
        free(data_);
    }
}

bool raw_serial::open()
{
    return open(_portName, _baudrate, _flags);
}

bool raw_serial::bind(const char * portname, _u32 baudrate, _u32 flags)
{   
    strncpy(_portName, portname, sizeof(_portName));
    _baudrate = baudrate;
    _flags    = flags;
    return true;
}

bool raw_serial::open(const char * portname, _u32 baudrate, _u32 flags)
{
    if (isOpened()) close();
    
    _serial_handle = SDL_OpenSerialPort(portname, baudrate);
    
    if (_serial_handle == SDL_INVALID_HANDLE_VALUE) return false;

    // Sleep(30); 
    SDL_Delay(30);
    _is_serial_opened = true;

    // Clear the DTR bit set DTR=high
    clearDTR();

    return true;
}

void raw_serial::close()
{
    SDL_CloseSerialPort(_serial_handle);
    _serial_handle = SDL_INVALID_HANDLE_VALUE;
    
    _is_serial_opened = false;
}

int raw_serial::senddata(const unsigned char * data, size_t size)
{
    size_t w_len = 0;
    if (!isOpened()) return ANS_DEV_ERR;

    if (data == NULL || size ==0) return 0;
    w_len = SDL_WriteSerialPort(_serial_handle, data, size);

    return w_len;
}

int raw_serial::recvdata(unsigned char * data, size_t size)
{
    if (data_vsize_ == 0) {
        return 0;
    }
    VALIDATE((int)size >= data_vsize_, null_str);
    memcpy(data, data_, data_vsize_);
    return data_vsize_;
}

void raw_serial::flush( _u32 flags)
{
    // PurgeComm(_serial_handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR );
}

int raw_serial::waitforsent(_u32 timeout, size_t * returned_size)
{
    if (!isOpened() ) return ANS_DEV_ERR;
    size_t w_len = 0;
    int ans =0;

    if (returned_size) *returned_size = w_len;
    return ans;
}

int raw_serial::waitforrecv(_u32 timeout, size_t * returned_size)
{
    if (!isOpened() ) return -1;
    DWORD r_len = 0;
    _word_size_t ans =0;

    if (returned_size) *returned_size = r_len;
    return ans;
}

void raw_serial::resize_data(int size, int vsize)
{
    if (size <= data_size_) {
        return;
    }

	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	char* tmp = (char*)malloc(size);
	if (data_) {
		if (vsize) {
			memcpy(tmp, data_, vsize);
		}
		free(data_);
	}
	data_ = tmp;
	data_size_ = size;
}

int raw_serial::waitfordata(size_t data_count, _u32 timeout, size_t * returned_size)
{
    size_t dummy_length;
    if (returned_size==NULL) returned_size=(size_t *)&dummy_length;

    if (!isOpened()) {
        *returned_size = 0;
        return ANS_DEV_ERR;
    }

    // timeout is ms.
    _u32 startTs = SDL_GetTicks();
    _u32 waitTime;

    data_vsize_ = 0;
    resize_data(data_count, 0);
    while ((waitTime = SDL_GetTicks() - startTs) <= timeout) {
        size_t remain_size = data_count - data_vsize_;
        size_t bytes;
        bytes = SDL_ReadSerialPort(_serial_handle, data_ + data_vsize_, remain_size);
        if (bytes != 0) {
            data_vsize_ += bytes;
            if (data_vsize_ == data_count) {
                break;
            }
        } else if (bytes == 0) {
            SDL_Delay(2);
        }
    }
    
    *returned_size = data_vsize_;
    return 0;
}

size_t raw_serial::rxqueue_count()
{
    return 0;
}

void raw_serial::setDTR()
{
    if ( !isOpened() ) return;

    EscapeCommFunction(_serial_handle, SETDTR);
}

void raw_serial::clearDTR()
{
    if ( !isOpened() ) return;

    EscapeCommFunction(_serial_handle, CLRDTR);
}

}}} //end rp::arch::net


//begin rp::hal
namespace rp{ namespace hal{

serial_rxtx * serial_rxtx::CreateRxTx()
{
    return new rp::arch::net::raw_serial();
}

void  serial_rxtx::ReleaseRxTx( serial_rxtx * rxtx)
{
    delete rxtx;
}


}} //end rp::hal
