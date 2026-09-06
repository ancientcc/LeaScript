/* $Id: sha1.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010 by Benoit Timbert <benoit.timbert@free.fr>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_XFYUN_HPP_INCLUDED
#define LIBROSE_XFYUN_HPP_INCLUDED

#include <string>
#include "SDL_types.h"
#include "xfyun/include/msp_errors.h"
#include "../common.hpp"

namespace aplt {

class txfyun
{
public:
	txfyun(const std::string& res_path, txf3params& xf3params);
	~txfyun();

	std::string recognition_wav_file(const std::string& audio_file);

	bool recognition_start();
	void recognition_stop();
	bool started() const { return started_; }

	bool libmsc_loaded() const;

	std::string iat_piece(const uint8_t* wav, int len);
	int is_AUTH_NO_ENOUGH_LICENSE() const { return errcode_ == MSP_ERROR_AUTH_NO_ENOUGH_LICENSE; }
	int is_DB_INVALID_APPID() const { return errcode_ == MSP_ERROR_DB_INVALID_APPID; }
	void clear_errcode() { errcode_ = MSP_SUCCESS; };

private:
	txf3params& xf3params_;
	// const std::string login_params_;
	const std::string session_begin_params_;
	bool started_;
	int errcode_;
};

}

#endif
