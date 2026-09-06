// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_ROSE_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_ROSE_H_

#include "net/url_request/url_request_rose_util.h"
#include "rose_net_api.hpp"

using namespace std::placeholders;

namespace net {

// in general for test.
// @url: https://www.github.com
bool fetch_url_data(const std::string& _url, const std::string& path, int timeout);
bool fetch_url_2_mem(const std::string& _url, std::string& received_data, int timeout);
std::string err_2_description(int err);
std::unique_ptr<UploadDataStream> CreateSimpleUploadData(const char* data, int size);

struct thttp_agent
{
	thttp_agent(const std::string& _url, const std::string& _method, const std::string& _cert, int timeout);

	const GURL url;
	std::function<bool (URLRequest&, HttpRequestHeaders&, std::string&)> did_pre;
	std::function<bool (const URLRequest&, const RoseDelegate&, int)> did_post;

	const std::string method;
	const std::string cert;
	int timeout;
	int32_t app_received_bytes;
	int64_t app_expected_bytes;
	bool* cancel_ptr;
};

bool handle_http_request(thttp_agent& agent);


class tchromium_http_api: public thttp_api
{
public:
	tchromium_http_api(const std::string& url, const std::string& method, int timeout);

private:
	bool did_pre_agent(URLRequest&, net::HttpRequestHeaders& headers, std::string& body);
	bool did_post_agent(const URLRequest&, const net::RoseDelegate& delegate, int status);
	bool handle_http_request(bool& cancel) override;

	void SetHeader(const std::string& key, const std::string& value) override;
	std::string err_2_description(int err) override;

private:
	thttp_agent agent_;
	net::HttpRequestHeaders* curr_headers_;
};

thttp_api* chromium_create_http_api(const std::string& url, const std::string& method, int timeout);

}

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_ROSE_H_
