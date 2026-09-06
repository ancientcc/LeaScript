#ifndef LIBROSE_QR_CODE_HPP_INCLUDED
#define LIBROSE_QR_CODE_HPP_INCLUDED

#include "rose_qr_code.hpp"

// The QR code is always square, so only one @size parameter is needed.
surface generate_qr(const std::string& text, int size, int margin = 10, int eccLevel = 2);

#endif
