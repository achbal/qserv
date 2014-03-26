/*
 * LSST Data Management System
 * Copyright 2011, 2012 LSST Corporation.
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
/// RequestTaker.h
/// A class that handles incoming request streams. Migrates some
/// functionality out of MySqlFsFile so that qserv request handling
/// is less dependent on Xrootd. (some dependencies still exist in
/// MySqlFs.)
/// @author Daniel L. Wang (danielw)
#ifndef LSST_QSERV_WCONTROL_ORDERTAKER_H
#define LSST_QSERV_WCONTROL_ORDERTAKER_H
#include "wbase/Base.h" // StringBuffer2

namespace lsst {
namespace qserv {

namespace obsolete {
    // Forward
    class QservPath;
}
    
namespace wcontrol {


class RequestTaker {
public:
    typedef int64_t Size;

    explicit RequestTaker(wbase::TaskAcceptor::Ptr acceptor, 
                          obsolete::QservPath const& path);
    bool receive(Size offset, char const* buffer, Size bufferSize);
    bool complete();
private:
    wbase::TaskAcceptor::Ptr _acceptor;
    wbase::StringBuffer2 _queryBuffer;
    std::string _db;
    int _chunk;
};

}}} // namespace lsst::qserv::wcontrol

#endif // LSST_QSERV_WCONTROL_ORDERTAKER_H
