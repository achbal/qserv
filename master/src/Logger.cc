/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */
// See Logger.h

#include "lsst/qserv/master/Logger.h"

using lsst::qserv::master::Logger;

namespace qMaster=lsst::qserv::master;

// Thread local storage to ensure one instance of Logger per thread.
boost::thread_specific_ptr<Logger> Logger::_instancePtr;

// Application-wide severity threshold.
Logger::Severity Logger::_severityThreshold = Info;
boost::mutex Logger::_mutex;

std::ostream* volatile Logger::logStreamPtr = &(std::cout);

////////////////////////////////////////////////////////////////////////
// public
////////////////////////////////////////////////////////////////////////

Logger& Logger::Instance() {
    if (_instancePtr.get() == NULL) _instancePtr.reset(new Logger);
    return *_instancePtr;
}

// Allows retrieval of singleton and setting of severity
// with single function call.
Logger& Logger::Instance(Logger::Severity severity) {
    if (_instancePtr.get() == NULL) _instancePtr.reset(new Logger);
    _instancePtr.get()->setSeverity(severity);
    return *_instancePtr;
}

void Logger::setSeverity(Logger::Severity severity) {
    if (severity != _severity) {
        flush();
        _severity = severity;
    }
}

Logger::Severity Logger::getSeverity() const {
    return _severity;
}

void Logger::setSeverityThreshold(Logger::Severity severity) {
    if (severity != _severityThreshold) {
        flush();
        {
            boost::mutex::scoped_lock lock(_mutex);
            _severityThreshold = severity;
        }
    }
}

Logger::Severity Logger::getSeverityThreshold() const {
    return _severityThreshold;
}

////////////////////////////////////////////////////////////////////////
// private
////////////////////////////////////////////////////////////////////////

Logger::Logger() : boost::iostreams::filtering_ostream() {
    _severity = Info;
    Logger::SeverityFilter severityFilter(this);
    Logger::LogFilter logFilter(this);
    push(severityFilter);
    push(logFilter);
    push(*logStreamPtr);
}

Logger::SeverityFilter::SeverityFilter(Logger* loggerPtr) 
    : boost::iostreams::multichar_output_filter() {
    _loggerPtr = loggerPtr;
}

template<typename Sink>
std::streamsize Logger::SeverityFilter::write(Sink& dest, const char* s, std::streamsize n) {
    if (_loggerPtr->getSeverity() >= _loggerPtr->getSeverityThreshold()) {
        std::streamsize z;
        for (z = 0; z < n; ++z) {
            if (!boost::iostreams::put(dest, s[z]))
                break;
        }
        return z;
    } else
        return n;
}

Logger::LogFilter::LogFilter(Logger* loggerPtr) : boost::iostreams::line_filter() {
    _loggerPtr = loggerPtr;
}

std::string Logger::LogFilter::do_filter(const std::string& line) {
    return getTimeStamp() + " " + getThreadId() + " " + getSeverity() + " " + line;
}

std::string Logger::LogFilter::getThreadId() {
    return boost::lexical_cast<std::string>(boost::this_thread::get_id());
}

std::string Logger::LogFilter::getTimeStamp() {
    char fmt[64], buf[64];
    struct timeval tv;
    struct tm* tm;
    gettimeofday(&tv, NULL);
    if ((tm = localtime(&tv.tv_sec)) != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%06u", tm);
        snprintf(buf, sizeof buf, fmt, tv.tv_usec);
        return std::string(buf);
    } else {
        return NULL;
    }
}

std::string Logger::LogFilter::getSeverity() {
    switch (_loggerPtr->getSeverity()) {
    case Logger::Debug:
        return "DBG";
    case Logger::Info:
        return "INF";
    case Logger::Warning:
        return "WRN";
    case Logger::Error:
        return "ERR";
    default:
        return NULL;
    }
}
