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

#ifndef LSST_QSERV_QDISP_MERGEADAPTER_H
#define LSST_QSERV_QDISP_MERGEADAPTER_H

// System headers
// #include <list>
// #include <string>

// Third-party
#include <boost/make_shared.hpp>

#include "qdisp/QueryReceiver.h"

namespace lsst {
namespace qserv {
namespace qdisp {

class MergeAdapter : public QueryReceiver {
public:
    virtual int bufferSize() const { return 0; }
    virtual char* buffer() { return NULL; }
    virtual void flush(int bLen, bool last) {}
    virtual void errorFlush() {}
    virtual bool finished() const {return true; }
    static boost::shared_ptr<MergeAdapter> newInstance() {
        return boost::make_shared<MergeAdapter>();
    }
};

}}} // namespace lsst::qserv::qdisp

#endif // LSST_QSERV_QDISP_MERGEADAPTER_H
