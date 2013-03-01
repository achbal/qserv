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
 
#include "lsst/qserv/worker/MySqlFsDirectory.h"

#include "XrdSys/XrdSysError.hh"
#include "lsst/qserv/worker/Logger.h"

#include <errno.h>

namespace qWorker = lsst::qserv::worker;
using lsst::qserv::worker::Logger;

qWorker::MySqlFsDirectory::MySqlFsDirectory(boost::shared_ptr<Logger> log, 
                                            char* user) :
    XrdSfsDirectory(user), _log(log) {
}

qWorker::MySqlFsDirectory::~MySqlFsDirectory(void) {
}

int qWorker::MySqlFsDirectory::open(
    char const* dirName, XrdSecEntity const* client,
    char const* opaque) {
    error.setErrInfo(ENOTSUP, "Operation not supported");
    return SFS_ERROR;
}

char const* qWorker::MySqlFsDirectory::nextEntry(void) {
    return 0;
}

int qWorker::MySqlFsDirectory::close(void) {
    error.setErrInfo(ENOTSUP, "Operation not supported");
    return SFS_ERROR;
}

char const* qWorker::MySqlFsDirectory::FName(void) {
    _log->info("In MySqlFsDirectory::Fname()");
    return 0;
}
