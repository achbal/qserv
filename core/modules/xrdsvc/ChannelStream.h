// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 LSST Corporation.
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
#ifndef LSST_QSERV_XRDSVC_CHANNELSTREAM_H
#define LSST_QSERV_XRDSVC_CHANNELSTREAM_H

// System headers
#include <deque>

// Third-party headers
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include "XrdSsi/XrdSsiErrInfo.hh" // required by XrdSsiStream
#include "XrdSsi/XrdSsiStream.hh"

namespace lsst {
namespace qserv {
namespace xrdsvc {
/// ChannelStream is an implementation of an XrdSsiStream that accepts
/// SendChannel streamed data.
class ChannelStream : public XrdSsiStream {
public:
    ChannelStream();
    virtual ~ChannelStream();

    /// Push in a data packet
    void append(char const* buf, int bufLen, bool last);

    /// Pull out a data packet as a Buffer object (called by XrdSsi code)
    virtual Buffer *GetBuff(XrdSsiErrInfo &eInfo, int &dlen, bool &last);

    bool closed() const { return _closed; }

private:
    bool _closed; ///< Closed to new append() calls?
    // Can keep a deque of (buf, bufsize) to reduce copying, if needed.
    std::deque<std::string> _msgs; ///< Message queue
    boost::mutex _mutex; ///< _msgs protection
    boost::condition_variable _hasDataCondition; ///< _msgs condition
};

}}} // namespace lsst::qserv::xrdsvc

#endif // LSST_QSERV_XRDSVC_CHANNELSTREAM_H
