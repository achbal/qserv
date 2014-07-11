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

#ifndef LSST_QSERV_QDISP_TASKSPEC_H
#define LSST_QSERV_QDISP_TASKSPEC_H
/**
  * @file
  *
  * @brief Value classes for SWIG-mediated interaction between Python
  * and C++. Includes TransactionSpec.
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers
#include <list>
#include <string>
#include <vector>

// Third-party headers
#include <boost/shared_ptr.hpp>

// Qserv headers
#include "global/ResourceUnit.h"

namespace lsst {
namespace qserv {
namespace qdisp {

class QueryReceiver; // Forward

/// class TaskSpec - A request unit to be sent to the worker.
class TaskSpec {
public:
TaskSpec() : chunkId(-1) {}
    std::string serviceHostPort; //< xrootd host:port
    ResourceUnit unit; //< Resource requested from remote
    std::string msg; //< Encoded request message
    boost::shared_ptr<QueryReceiver> receiver;
};

}}} // namespace lsst::qserv::control

#endif // LSST_QSERV_QDISP_TASKSPEC_H