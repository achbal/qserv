// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014-2016 LSST Corporation.
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

// Class header
#include "xrdsvc/ChannelStream.h"

// Third-party headers
#include "boost/utility.hpp"

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "global/Bug.h"
#include "global/debugUtil.h"
#include "util/common.h"

namespace {
LOG_LOGGER _log = LOG_GET("lsst.qserv.xrdsvc.ChannelStream");
}


namespace lsst {
namespace qserv {
namespace xrdsvc {

/// SimpleBuffer is a really simple buffer for transferring data packets to
/// XrdSsi
class SimpleBuffer : public XrdSsiStream::Buffer, boost::noncopyable {
public:
    SimpleBuffer(std::string const& input) {
        data = new char[input.size()];
        memcpy(data, input.data(), input.size());
        next = 0;
    }

    //!> Call to recycle the buffer when finished
    virtual void Recycle() {
        delete this; // Self-destruct. FIXME: Not sure this is right.
    }

    // Inherited from XrdSsiStream:
    // char  *data; //!> -> Buffer containing the data
    // Buffer *next; //!> For chaining by buffer receiver

    virtual ~SimpleBuffer() {
        delete[] data;
    }
};

////////////////////////////////////////////////////////////////////////
// ChannelStream implementation
////////////////////////////////////////////////////////////////////////

/// Constructor
ChannelStream::ChannelStream()
    : XrdSsiStream(isActive),
      _closed(false) {}

/// Destructor
ChannelStream::~ChannelStream() {
#if 0 // Enable to debug ChannelStream lifetime
    try {
        LOGS(_log, LOG_LVL_DEBUG, "Stream (" << (void *) this << ") deleted");
    } catch (...) {} // Destructors have nowhere to throw exceptions
#endif
}

/// Push in a data packet
void
ChannelStream::append(char const* buf, int bufLen, bool last) {
    if (_closed) {
        throw Bug("ChannelStream::append: Stream closed, append(...,last=true) already received");
    }
    LOGS(_log, LOG_LVL_DEBUG, "last=" << last << " " << util::prettyCharBuf(buf, bufLen, 10));
    {
        std::unique_lock<std::mutex> lock(_mutex);
        LOGS(_log, LOG_LVL_DEBUG, "Trying to append message (flowing)");

        _msgs.push_back(std::string(buf, bufLen));
        _closed = last; // if last is true, then we are closed.
        _hasDataCondition.notify_one();
    }
}

/// Pull out a data packet as a Buffer object (called by XrdSsi code)
XrdSsiStream::Buffer*
ChannelStream::GetBuff(XrdSsiErrInfo &eInfo, int &dlen, bool &last) {
    std::unique_lock<std::mutex> lock(_mutex);
    while(_msgs.empty() && !_closed) { // No msgs, but we aren't done
        // wait.
        LOGS(_log, LOG_LVL_DEBUG, "Waiting, no data ready");
        _hasDataCondition.wait(lock);
    }
    if (_msgs.empty() && _closed) { // We are closed and no more
        // msgs are available.
        LOGS(_log, LOG_LVL_DEBUG, "Not waiting, but closed");
        dlen = 0;
        eInfo.Set("Not an active stream", EOPNOTSUPP);
        return 0;
    }
    SimpleBuffer* sb = new SimpleBuffer(_msgs.front());
    dlen = _msgs.front().size();
    _msgs.pop_front();
    last = _closed && _msgs.empty();
    LOGS(_log, LOG_LVL_DEBUG, "returning buffer (" << dlen << ", " << (last ? "(last)" : "(more)") << ")");
    return sb;
}

}}} // lsst::qserv::xrdsvc
