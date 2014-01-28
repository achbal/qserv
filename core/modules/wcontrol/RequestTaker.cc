/* 
 * LSST Data Management System
 * Copyright 2011-2013 LSST Corporation.
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
 /**
  * @file RequestTaker.cc
  *
  * @brief RequestTaker is a class that handles incoming request
  * streams. Migrates some functionality out of MySqlFsFile so that
  * qserv request handling is less dependent on Xrootd. (some
  * dependencies still exist in MySqlFs.)
  *
  * FIXME: Unfinished infrastructure for passing subchunk table name to worker.
  *
  * @author Daniel L. Wang, SLAC
  */ 
#include "wcontrol/RequestTaker.h"
#include "obsolete/QservPath.h"
#include "proto/worker.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
namespace gio = google::protobuf::io;
namespace qWorker = lsst::qserv::worker;


qWorker::RequestTaker::RequestTaker(TaskAcceptor::Ptr acceptor, QservPath const& path) 
    : _acceptor(acceptor), _chunk(path.chunk()), _db(path.db()) { 
}

bool qWorker::RequestTaker::receive(Size offset, char const* buffer, 
                                    Size bufferSize) {
    _queryBuffer.addBuffer(offset, buffer, bufferSize);
    return true;
}

bool qWorker::RequestTaker::complete() {
    boost::shared_ptr<TaskMsg> tm(new TaskMsg());
    gio::ArrayInputStream input(_queryBuffer.getData(), 
                                _queryBuffer.getLength());
    gio::CodedInputStream coded(&input);
    tm->MergePartialFromCodedStream(&coded);
    if((tm->has_chunkid() && tm->has_db()) 
       && (_chunk == tm->chunkid()) && (_db == tm->db())) {
        // Note: db is only available via path. 
        _acceptor->accept(tm);
        return true;
    } else return false;
}
