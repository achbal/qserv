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

#ifndef LSST_QSERV_MASTER_AGGOP_H
#define LSST_QSERV_MASTER_AGGOP_H
#include <map>
#include <boost/shared_ptr.hpp>
#include "lsst/qserv/master/AggRecord.h"
#include "lsst/qserv/master/ValueExpr.h"

namespace lsst { namespace qserv { namespace master {

class AggOp {
public:
    typedef boost::shared_ptr<AggOp> Ptr;
    class Mgr;
    
    virtual AggRecord::Ptr operator()(ValueExpr const& orig) = 0;
    virtual ~AggOp() {}
protected:
    explicit AggOp(Mgr&m) : _mgr(m) {}
    Mgr& _mgr;
};

class AggOp::Mgr {
public:
    typedef std::map<std::string, AggOp::Ptr> OpMap;

    Mgr();
    AggOp::Ptr getOp(std::string const& name);
    AggRecord::Ptr applyOp(std::string const& name, ValueExpr const& orig);
    int getNextSeq() { return ++_seq; }
    std::string getAggName(std::string const& name);
    bool hasAggregate() const { return _seq > 0; };
private:
    OpMap _map;
    int _seq;
};

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_AGGREGATEOP_H

