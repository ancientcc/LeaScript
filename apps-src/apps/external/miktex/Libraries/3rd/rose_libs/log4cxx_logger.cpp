/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/source/src/main/include/log4cxx/logstring.h>
#include <log4cxx/source/src/main/include/log4cxx/logger.h>
// #include <log4cxx/spi/loggingevent.h>
// #include <log4cxx/logmanager.h>
// #include <log4cxx/spi/loggerfactory.h>
// #include <log4cxx/appender.h>
// #include <log4cxx/level.h>
// #include <log4cxx/helpers/loglog.h>
// #include <log4cxx/hierarchy.h>
// #include <log4cxx/helpers/stringhelper.h>
// #include <log4cxx/helpers/transcoder.h>
// #include <log4cxx/helpers/appenderattachableimpl.h>
// #include <log4cxx/helpers/exception.h>
#if !defined(LOG4CXX)
	#define LOG4CXX 1
#endif
// #include <log4cxx/private/log4cxx_private.h>
// #include <log4cxx/helpers/aprinitializer.h>

using namespace log4cxx;
// using namespace log4cxx::helpers;
// using namespace log4cxx::spi;

// IMPLEMENT_LOG4CXX_OBJECT(Logger)

// Logger::Logger(Pool& p, const LogString& name1)
Logger::Logger(const std::string& name1)
	: // m_priv(std::make_unique<LoggerPrivate>(p, name1))
	m_threshold(0)
{
}

Logger::~Logger()
{
}

// static std::map<std::string, std::shared_ptr<Logger> > loggers_;
static std::shared_ptr<Logger> logger_;

LoggerPtr Logger::getLogger(const std::string& name)
{
	// loggers_.insert(std::make_pair<name, std::make_shared<Logger>(name)>);
	logger_ = std::make_shared<Logger>(name);
	return logger_;
}

