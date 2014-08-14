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

#ifndef LSST_QSERV_WBASE_SENDCHANNEL_H
#define LSST_QSERV_WBASE_SENDCHANNEL_H

// System headers
#include <string>

// Third-party headers
#include <boost/shared_ptr.hpp>

// Qserv headers
#include "util/Callable.h"


namespace lsst {
namespace qserv {
namespace wbase {

class SendChannel {
public:
    typedef util::VoidCallable<void> ReleaseFunc;
    typedef boost::shared_ptr<ReleaseFunc> ReleaseFuncPtr;
    typedef long long Size;

    virtual void send(char const* buf, int bufLen) = 0;

    virtual void sendError(std::string const& msg, int code) = 0;
    virtual void sendFile(int fd, Size fSize) = 0;
    virtual void sendStream(char const* buf, int bufLen, bool last) {
        throw "unsupported streaming";
    }

    void setReleaseFunc(ReleaseFuncPtr r) { _release = r; }
    void release() {
        if(_release) {
            (*_release)();
        }
    }
protected:
    ReleaseFuncPtr _release;
};
}}} // lsst::qserv::wbase
#endif // LSST_QSERV_WBASE_SENDCHANNEL_H
