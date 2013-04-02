// -*- LSST-C++ -*-
/* 
 * LSST Data Management System
 * Copyright 2012 LSST Corporation.
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
// X is a ...

#ifndef LSST_QSERV_MASTER_QUERYMSG_H
#define LSST_QSERV_MASTER_QUERYMSG_H
#include <string>

namespace lsst { namespace qserv { namespace master {

int queryMsgGetCount(int session);

// Python call: msg, chunkId, code = queryMsgGetMsg(sessionId, msgNum)
// int* code matches with %apply directive to help SWIG
std::string queryMsgGetMsg(int session, int idx, int* chunkId, int* code);

int queryMsgAddMsg(int session, int msgCode, std::string const& message);

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_QUERYMSG_H

