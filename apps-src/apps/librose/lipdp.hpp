/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBROSE_LIPDP_H
#define LIBROSE_LIPDP_H

#include <string>
#include <set>

#include "util.hpp"

#define LEAGOR_BLE_MTU_HEADER_SIZE	4 // 1(prefix) + 1(cmd) + 2(len)
#define LEAGOR_BLE_PREFIX_BYTE		0x5a

#define LEAGOR_BLE_VERSION_UNSPEC	0
#define LEAGOR_BLE_VERSION			1 // current versin, 1
#define LEAGOR_BLE_VERSION_SIZE		1

// value same as socket. x:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/shared\ws2def.h
// AF=address family
// LEAGOR_BLE_AF_xxx must >= 0. some code use nposm to indicate invalid.
enum {LEAGOR_BLE_AF_ERR_VER = 0, // leagorerr_versiondismatch
	LEAGOR_BLE_AF_ERR_PRIVACY = 1, // leagorerr_privacyprotect

	LEAGOR_BLE_AF_INET = 2, // internetwork: UDP, TCP, etc. (bytes: 4+1)
	LEAGOR_BLE_AF_INET6 = 23, // Internetwork Version 6 (bytes: 16+1)
	LEAGOR_BLE_AF_MAX = 255,
};
#define LEAGOR_BLE_AF_SIZE			1

enum {lipdp_tn8, lipdp_tn32, lipdp_tn64, lipdp_tstring, lipdp_thexstring, lipdp_tbinary, lipdp_tip};

struct tlipdp_item {
	int type;
	int32_t int32;	// lipdp_int32/lipdp_tstring
	int64_t int64;	// lipdp_int64
	const uint8_t* data;	// lipdp_tstring
	uint8_t u8;	// lipdp_tint8
};

#define LIPDP_PUSH_ITEM_n8(n8)	\
	items[index].type = lipdp_tn8;	\
	items[index].u8 = (uint8_t)(n8);	\
	payload_len += 1 + 1;	\
	index ++;

// I think, Both int32_t and uint23_t are OK.
#define LIPDP_PUSH_ITEM_n32(n32)	\
	items[index].type = lipdp_tn32;	\
	items[index].int32 = (int32_t)(n32);	\
	index ++;	\
	payload_len += 1 + 4;

#define LIPDP_PUSH_ITEM_n64(n64)	\
	items[index].type = lipdp_tn64;	\
	items[index].int64 = n64;	\
	payload_len += 1 + 8;	\
	index ++;

#define LIPDP_PUSH_ITEM_string(_data, _len)	\
	items[index].type = lipdp_tstring;	\
	items[index].data = (const uint8_t*)(_data);	\
	items[index].int32 = _len;	\
	payload_len += 1 + items[index].int32 + 1;	\
	index ++;

// payload_len += 1 + items[index].int32 / 2 + 1;
#define LIPDP_PUSH_ITEM_hexstring(_data, _len)	\
	items[index].type = lipdp_thexstring;	\
	items[index].data = (const uint8_t*)(_data);	\
	items[index].int32 = _len;	\
	payload_len += 1 + 2 + items[index].int32 / 2;	\
	index ++;

#define LIPDP_PUSH_ITEM_binary(_data, _len)	\
	items[index].type = lipdp_tbinary;	\
	items[index].data = (const uint8_t*)(_data);	\
	items[index].int32 = _len;	\
	payload_len += 1 + 2 + items[index].int32;	\
	index ++;

#define LIPDP_PUSH_ITEM_ipv4(ipv4)	\
	items[index].type = lipdp_tip;	\
	items[index].u8 = LEAGOR_BLE_AF_INET;	\
	items[index].int32 = ipv4;	\
	payload_len += 1 + LEAGOR_BLE_AF_SIZE + 4;	\
	index ++;

#define LIPDP_PUSH_ITEM_ipv6(ipv6)	\
	items[index].type = lipdp_tip;	\
	items[index].u8 = LEAGOR_BLE_AF_INET6;	\
	items[index].data = (const uint8_t*)(ipv6);	\
	payload_len += 1 + LEAGOR_BLE_AF_SIZE + 16;	\
	index ++;

#define LIPDP_PUSH_ITEM_ipunspec(af)	\
	items[index].type = lipdp_tip;	\
	items[index].u8 = af;	\
	payload_len += 1 + LEAGOR_BLE_AF_SIZE;	\
	index ++;

class tlipdp_items_lock
{
public:
	tlipdp_items_lock(int count, tlipdp_item** ppitems);

	~tlipdp_items_lock()
	{
		free(items_);
	}

private:
	tlipdp_item* items_;
};

class tlipdp_packer
{
public:
	tlipdp_packer()
		: packet_data_(nullptr)
		, packet_data_size_(0)
	{}

	virtual ~tlipdp_packer();

	// must not: tclazz clazz2(that)/clazz2 = tclazz(...);
    tlipdp_packer(const tlipdp_packer& that) = delete;
    const tlipdp_packer& operator=(const tlipdp_packer& that) = delete;

	int items_2_data(int cmd, int payload_len, const tlipdp_item* items, int count, uint8_t* caller_packet_data = nullptr);

private:
	void resize_packet_data(int size);
	uint8_t* fill_mtu_4bytes(uint8_t* data, int cmd, int payload_len);

protected:
	uint8_t* packet_data_;
	int packet_data_size_;
};

class tlipdp_parser
{
public:
	tlipdp_parser()
		: items(nullptr)
		, count(0)
		, fail(false)
		, items_per_alloc_(20)
		, item_count_(0)
	{}

	virtual ~tlipdp_parser()
	{
		if (items != nullptr) {
			free(items);
		}
	}

	// must not: tclazz clazz2(that)/clazz2 = tclazz(...);
    tlipdp_parser(const tlipdp_parser& that) = delete;
    const tlipdp_parser& operator=(const tlipdp_parser& that) = delete;

	int handle(const uint8_t* data, int len);

public:
	tlipdp_item* items;
	int count;
	bool fail;

private:
	void expand_items(int vcount);

private:
	const int items_per_alloc_;
	int item_count_;
};

class tlipdp_receiver
{
public:
	tlipdp_receiver()
		: recv_data_(nullptr)
		, recv_data_size_(0)
		, recv_data_vsize_(0)
	{}
	virtual ~tlipdp_receiver();

	// must not: tclazz clazz2(that)/clazz2 = tclazz(...);
    tlipdp_receiver(const tlipdp_receiver& that) = delete;
    const tlipdp_receiver& operator=(const tlipdp_receiver& that) = delete;

	void set_did_read(const std::function<void (int cmd, const uint8_t* data, int len)>& did)
	{
		did_read_ = did;
	}
	void enqueue(const uint8_t* data, int len);

private:
	void resize_recv_data(int size);

protected:
	std::set<int> recv_cmds_;

private:
	uint8_t* recv_data_;
	int recv_data_size_;
	int recv_data_vsize_;
	std::function<void (int cmd, const uint8_t* data, int len)> did_read_;
};

#define MIN_BLEPASSWORD_SIZE     6
#define MAX_BLEPASSWORD_SIZE     10
bool leagor_verify_blepassword(const std::string& label);

#endif // LIBROSE_LIPDP
