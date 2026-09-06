/*
 * Copyright (C) 2009, Willow Garage, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the names of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ros/master.h"
#include "ros/xmlrpc_manager.h"
#include "ros/this_node.h"
#include "ros/init.h"
#include "ros/network.h"

#include <ros/console.h>
#include <ros/assert.h>

#include "xmlrpcpp/XmlRpc.h"
#include "rose_exception.hpp"
#include "rose_string_utils.hpp"
#include "rose_thread.hpp"

namespace ros
{

namespace master
{

uint32_t g_port = 0;
std::string g_host;
std::string g_uri;
ros::WallDuration g_retry_timeout;

void init(const M_string& remappings)
{
  M_string::const_iterator it = remappings.find("__master");
  if (it != remappings.end())
  {
    g_uri = it->second;
  }

  if (g_uri.empty())
  {
    char *master_uri_env = NULL;
    #ifdef _MSC_VER
      _dupenv_s(&master_uri_env, NULL, "ROS_MASTER_URI");
    #else
      master_uri_env = getenv("ROS_MASTER_URI");
    #endif

    if (master_uri_env)
    {
      g_uri = master_uri_env;
    }
    else
    {
      g_uri = ros::getDefaultMasterURI();
    }

#ifdef _MSC_VER
    // http://msdn.microsoft.com/en-us/library/ms175774(v=vs.80).aspx
    free(master_uri_env);
#endif
  }

  // Split URI into
  if (!network::splitURI(g_uri, g_host, g_port))
  {
    ROS_FATAL( "Couldn't parse the master URI [%s] into a host:port pair.", g_uri.c_str());
    ROS_BREAK();
  }
}

const std::string& getHost()
{
  return g_host;
}

uint32_t getPort()
{
  return g_port;
}

const std::string& getURI()
{
  return g_uri;
}

void setRetryTimeout(ros::WallDuration timeout)
{
  if (timeout < ros::WallDuration(0))
  {
    ROS_FATAL("retry timeout must not be negative.");
    ROS_BREAK();
  }
  g_retry_timeout = timeout;
}

bool check()
{
  XmlRpc::XmlRpcValue args, result, payload;
  args[0] = this_node::getName();
  return execute("getPid", args, result, payload, false);
}

bool getTopics(V_TopicInfo& topics)
{
  XmlRpc::XmlRpcValue args, result, payload;
  args[0] = this_node::getName();
  args[1] = ""; //TODO: Fix this

  if (!execute("getPublishedTopics", args, result, payload, true))
  {
    return false;
  }

  topics.clear();
  for (int i = 0; i < payload.size(); i++)
  {
    topics.push_back(TopicInfo(std::string(payload[i][0]), std::string(payload[i][1])));
  }

  return true;
}

bool getNodes(V_string& nodes)
{
  XmlRpc::XmlRpcValue args, result, payload;
  args[0] = this_node::getName();

  if (!execute("getSystemState", args, result, payload, true))
  {
    return false;
  }

  S_string node_set;
  for (int i = 0; i < payload.size(); ++i)
  {
    for (int j = 0; j < payload[i].size(); ++j)
    {
      XmlRpc::XmlRpcValue val = payload[i][j][1];
      for (int k = 0; k < val.size(); ++k)
      {
        std::string name = payload[i][j][1][k];
        node_set.insert(name);
      }
    }
  }

  nodes.insert(nodes.end(), node_set.begin(), node_set.end());

  return true;
}

#if defined(__APPLE__)
boost::mutex g_xmlrpc_call_mutex;
#endif

class trosmaster
{
public:
    enum {method_getparam, method_hasparam, method_searchparam, method_setparam,
        method_subscribeparam, method_unsubscribeparam, method_deleteparam,
        method_getpublishedtopics, method_registerpublisher, method_unregisterpublisher,
        method_registersubscriber, method_unregistersubscriber,
        method_registerservice, method_unregisterservice, method_lookupservice,
    };
    trosmaster();
    int methodid(const std::string& method) const
    {
        std::map<std::string, int>::const_iterator it = methods_.find(method);
        return it != methods_.end()? it->second: nposm;
    }
    bool handle(int method, const XmlRpc::XmlRpcValue& request, XmlRpc::XmlRpcValue& payload);

private:
    bool check_request(const XmlRpc::XmlRpcValue& request) const;
    bool has_param(const std::string& key) const;
    std::string search_param(const std::string& ns, const std::string& key) const;

public:
    threading::mutex mutex_;

private:
    const std::string registerpublisher_url_;
    const std::string registersubscriber_url_;
    std::map<std::string, int> methods_;
    std::map<std::string, XmlRpc::XmlRpcValue> params_;
    std::set<std::string> subscribed_params_;
    std::set<std::string> namespaces_;
    std::vector<std::pair<std::string, std::string> > publishedtopics_;
    std::map<std::string, std::string> subscribers_;
    std::map<std::string, std::string> services_;
};
static trosmaster rosmaster;

trosmaster::trosmaster()
    : registerpublisher_url_("http://127.0.0.1:8088/")
    , registersubscriber_url_("http://127.0.0.1:8089/")
{
    methods_.insert(std::make_pair("getParam", method_getparam));
    methods_.insert(std::make_pair("hasParam", method_hasparam));
    methods_.insert(std::make_pair("searchParam", method_searchparam));
    methods_.insert(std::make_pair("setParam", method_setparam));
    methods_.insert(std::make_pair("subscribeParam", method_subscribeparam));
    methods_.insert(std::make_pair("unsubscribeParam", method_unsubscribeparam));
    methods_.insert(std::make_pair("deleteParam", method_deleteparam));
    methods_.insert(std::make_pair("getPublishedTopics", method_getpublishedtopics));
    methods_.insert(std::make_pair("registerPublisher", method_registerpublisher));
    methods_.insert(std::make_pair("unregisterPublisher", method_unregisterpublisher));
    methods_.insert(std::make_pair("registerSubscriber", method_registersubscriber));
    methods_.insert(std::make_pair("unregisterSubscriber", method_unregistersubscriber));
    methods_.insert(std::make_pair("registerService", method_registerservice));
    methods_.insert(std::make_pair("unregisterService", method_unregisterservice));
    methods_.insert(std::make_pair("lookupService", method_lookupservice));
}

bool trosmaster::check_request(const XmlRpc::XmlRpcValue& request) const
{
    if (request.getType() != XmlRpc::XmlRpcValue::TypeArray) {
        return false;
    }
    if (request[0].getType() != XmlRpc::XmlRpcValue::TypeString) {
        return false;
    }
    if (request[1].getType() != XmlRpc::XmlRpcValue::TypeString) {
        return false;
    }
    return true;
}

bool trosmaster::has_param(const std::string& key) const
{
    if (params_.count(key)) {
        return true;
    }
    bool found = false;
    // request[1] is namespace?
    const std::string key2 = key + "/";
    for (std::map<std::string, XmlRpc::XmlRpcValue>::const_iterator it = params_.begin(); it != params_.end(); ++ it) {
        if (it->first.find(key2) == 0) {
            found = true;
            break;
        }
    }
    return found;
}

std::string trosmaster::search_param(const std::string& ns, const std::string& key) const
{
    // search_param(self, ns, key) in <opt>\ros\noetic\x64\Lib\site-packages\rosmaster\paramserver.py
    if (ns.empty() || key.empty()) {
        return null_str;
    }
    // if is_global(key):
    //        if self.has_param(key):
    //            return key
    //        else:
    //            return None
    if (key.at(0) == '/') {
        if (has_param(key)) {
            return key;
        }
        return null_str;
    }

    // key_namespaces = [x for x in key.split(SEP) if x]
    // key_ns = key_namespaces[0]
    std::vector<std::string> key_namespaces = utils::split(key, '/');
    const std::string& key_ns = key_namespaces[0];

    // search_key = ns_join(ns, key_ns)
    // if self.has_param(search_key):
    //        # resolve to full key
    //        return ns_join(ns, key) 
    std::string search_key = ns + "/" + key_ns;
    if (has_param(search_key)) {
        return ns + "/" + key;
    }

    // namespaces = [x for x in ns.split(SEP) if x]
    //    for i in range(1, len(namespaces)+1):
    //        search_key = SEP + SEP.join(namespaces[0:-i] + [key_ns])
    //        if self.has_param(search_key):
    //            # we have a match on the namespace of the key, so
    //            # compose the full key and return it
    //            full_key = SEP + SEP.join(namespaces[0:-i] + [key]) 
    //            return full_key
    //    return None
    std::vector<std::string> namespaces = utils::split(ns, '/');
    std::string tmp_ns = ns;
    while (!tmp_ns.empty()) {
        // "/launch" ==> ""
        size_t pos = tmp_ns.rfind('/');
        tmp_ns = tmp_ns.substr(0, pos);
        search_key = tmp_ns + "/" + key_ns;
        if (has_param(search_key)) {
            return tmp_ns + "/" + key;
        }
    }
    return null_str;
}

bool trosmaster::handle(int method, const XmlRpc::XmlRpcValue& request, XmlRpc::XmlRpcValue& payload)
{
    if (!check_request(request)) {
        return false;
    }
    const std::string& key = request[1];

    std::map<std::string, XmlRpc::XmlRpcValue>::iterator find_it = params_.find(key);

    bool ret = true;
    if (method == method_getparam) {
        if (find_it != params_.end()) {
            payload = find_it->second;
        } else {
            bool found = false;
            const std::string key2 = key + "/";
            for (std::map<std::string, XmlRpc::XmlRpcValue>::const_iterator it = params_.begin(); it != params_.end(); ++ it) {
                const std::string& name = it->first;
                if (name.find(key2) == 0) {
                    payload[name.substr(key2.size())] = it->second;
                }
            }
            if (payload.getType() == XmlRpc::XmlRpcValue::TypeInvalid) {
                ret = false;
            }
        }

    } else if (method == method_hasparam) {
        payload = has_param(key);

    } else if (method == method_searchparam) {
        const std::string result = search_param(request[0], request[1]);
        if (!result.empty()) {
            payload = result;
        } else {
            ret = false;
        }

    } else if (method == method_setparam) {
        if (find_it == params_.end()) {
            if (request[2].getType() != XmlRpc::XmlRpcValue::TypeStruct) {
                params_.insert(std::make_pair(key, request[2]));
            } else {
                const XmlRpc::XmlRpcValue& xml_value = request[2];
                for (XmlRpc::XmlRpcValue::ValueStruct::const_iterator it = xml_value.begin(); it != xml_value.end(); ++it) {
                    // Make sure this element is the right type
                    const std::string new_key = key + "/" + it->first;
                    std::map<std::string, XmlRpc::XmlRpcValue>::iterator find_it2 = params_.find(new_key);
                    if (find_it2 == params_.end()) {
                        params_.insert(std::make_pair(new_key, it->second));
                    } else {
                        find_it2->second = it->second;
                    }
                }
            }
        } else {
            find_it->second = request[2];
        }
        // always [int]0
        payload = 0;

    } else if (method == method_subscribeparam) {
        const std::string& key2 = request[2];
        if (!subscribed_params_.count(key2)) {
            subscribed_params_.insert(key2); 
        }
        // Ros require payload's type is XmlRpc::XmlRpcValue::TypeStruct and empty.
        // I no way to construct empty XmlRpc::XmlRpcValue::TypeStruct, and caller don't use payload
        // use payload = 1 same as unsubscribe
        // payload = XmlRpc::XmlRpcValue().assertStruct();
        payload = 0;

    } else if (method == method_unsubscribeparam) {
        const std::string& key2 = request[2];
        std::set<std::string>::iterator it = subscribed_params_.find(key2);
        if (it != subscribed_params_.end()) {
            subscribed_params_.erase(it);
        }
        payload = 1;

    } else if (method == method_deleteparam) {
        if (find_it != params_.end()) {
            params_.erase(find_it);
        } else {
            const std::string key2 = key + "/";
            for (std::map<std::string, XmlRpc::XmlRpcValue>::iterator it = params_.begin(); it != params_.end(); ) {
                if (it->first.find(key2) == 0) {
                    params_.erase(it ++);
                } else {
                    ++ it;
                }
            }
        }
        // always [int]0
        payload = 0;

    } else if (method == method_getpublishedtopics) {
        int at = 0;
        const std::string known_absent = "/rosout_agg";
        {
            XmlRpc::XmlRpcValue val;
            val[0] = known_absent;
            val[1] = "rosgraph_msgs/Log";
            payload[at ++] = val;
        }
        for (std::vector<std::pair<std::string, std::string> >::const_iterator it = publishedtopics_.begin(); it != publishedtopics_.end(); ++ it) {
            VALIDATE(it->first != known_absent, null_str);
            XmlRpc::XmlRpcValue val;
            val[0] = it->first;
            val[1] = it->second;
            payload[at ++] = val;
        }

    } else if (method == method_registerpublisher) {
        const std::string& topic = request[1];
        const std::string& msgtype = request[2];
        if (topic == "/rosout_agg") {
            int ii = 0;
        }
        bool updated = false;
        for (std::vector<std::pair<std::string, std::string> >::iterator it = publishedtopics_.begin(); it != publishedtopics_.end(); ++ it) {
            if (it->first == topic) {
                it->second = msgtype;
                updated = true;
                break;
            }
        }
        if (!updated) {
            publishedtopics_.push_back(std::make_pair(topic, msgtype));
        }
        payload[0] = registerpublisher_url_;

    } else if (method == method_unregisterpublisher) {
        const std::string& topic = request[1];
        if (topic == "/rosout_agg") {
            int ii = 0;
        }
        for (std::vector<std::pair<std::string, std::string> >::iterator it = publishedtopics_.begin(); it != publishedtopics_.end(); ++ it) {
            if (it->first == topic) {
                publishedtopics_.erase(it);
                break;
            }
        }
        payload = 1;

    } else if (method == method_registersubscriber) {
        const std::string& topic = request[1];
        const std::string& msgtype = request[2];
        bool updated = false;
        std::map<std::string, std::string>::iterator find_it = subscribers_.find(topic);
        if (find_it == subscribers_.end()) {
            subscribers_.insert(std::make_pair(topic, msgtype));
        } else {
            find_it->second = msgtype;
        }
        payload[0] = registersubscriber_url_;

    } else if (method == method_unregistersubscriber) {
        const std::string& topic = request[1];
        std::map<std::string, std::string>::iterator find_it = subscribers_.find(topic);
        if (find_it != subscribers_.end()) {
            subscribers_.erase(find_it);
        }
        payload = 1;

    } else if (method == method_registerservice) {
        const std::string& service = request[1];
        const std::string& service_url = request[2];
        bool updated = false;
        std::map<std::string, std::string>::iterator find_it = services_.find(service);
        if (find_it == services_.end()) {
            services_.insert(std::make_pair(service, service_url));
        } else {
            find_it->second = service_url;
        }
        payload = 1;

    } else if (method == method_unregisterservice) {
        const std::string& service = request[1];
        std::map<std::string, std::string>::iterator find_it = services_.find(service);
        if (find_it != services_.end()) {
            services_.erase(find_it);
        }
        payload = 1;

    } else if (method == method_lookupservice) {
        const std::string& service = request[1];
        std::map<std::string, std::string>::iterator find_it = services_.find(service);
        if (find_it != services_.end()) {
            payload = find_it->second;
        } else {
            ret = false;
        }

    } else {
        VALIDATE(false, null_str);
        return false;
    }

    return ret;
}

bool execute(const std::string& method, const XmlRpc::XmlRpcValue& request, XmlRpc::XmlRpcValue& response, XmlRpc::XmlRpcValue& payload, bool wait_for_master)
{
    threading::lock lock(rosmaster.mutex_);

    VALIDATE(!method.empty(), null_str);
    VALIDATE(request.size() >= 1, null_str);
    VALIDATE(request[0].getType() == XmlRpc::XmlRpcValue::TypeString, null_str);

    XmlRpc::XmlRpcValue payload2;
    int methodid = rosmaster.methodid(method);
    if (methodid != nposm) {
        XmlRpc::XmlRpcValue& _payload = ros::nocopy_intra? payload: payload2;
        bool ret = rosmaster.handle(methodid, request, _payload);
        if (ros::nocopy_intra) {
            return ret;
        }
    } else {
        VALIDATE(!ros::nocopy_intra, null_str);
    }


  ros::SteadyTime start_time = ros::SteadyTime::now();

  std::string master_host = getHost();
  uint32_t master_port = getPort();
  XmlRpc::XmlRpcClient *c = XMLRPCManager::instance()->getXMLRPCClient(master_host, master_port, "/");
  bool printed = false;
  bool slept = false;
  bool ok = true;
  bool b = false;
  do
  {
    {
#if defined(__APPLE__)
      boost::mutex::scoped_lock lock(g_xmlrpc_call_mutex);
#endif

      b = c->execute(method.c_str(), request, response);
    }

    ok = !ros::isShuttingDown() && !XMLRPCManager::instance()->isShuttingDown();

    if (!b && ok)
    {
      if (!printed && wait_for_master)
      {
        ROS_ERROR("[%s] Failed to contact master at [%s:%d].  %s", method.c_str(), master_host.c_str(), master_port, wait_for_master ? "Retrying..." : "");
        printed = true;
      }

      if (!wait_for_master)
      {
        XMLRPCManager::instance()->releaseXMLRPCClient(c);
        return false;
      }

      if (!g_retry_timeout.isZero() && (ros::SteadyTime::now() - start_time) >= g_retry_timeout)
      {
        ROS_ERROR("[%s] Timed out trying to connect to the master after [%f] seconds", method.c_str(), g_retry_timeout.toSec());
        XMLRPCManager::instance()->releaseXMLRPCClient(c);
        return false;
      }

      ros::WallDuration(0.05).sleep();
      slept = true;
    }
    else
    {
      if (!XMLRPCManager::instance()->validateXmlrpcResponse(method, response, payload))
      {
        XMLRPCManager::instance()->releaseXMLRPCClient(c);

        return false;
      }

      break;
    }

    ok = !ros::isShuttingDown() && !XMLRPCManager::instance()->isShuttingDown();
  } while(ok);

  if (ok && slept)
  {
    ROS_INFO("Connected to master at [%s:%d]", master_host.c_str(), master_port);
  }

  XMLRPCManager::instance()->releaseXMLRPCClient(c);

  return b;
}

} // namespace master

} // namespace ros
