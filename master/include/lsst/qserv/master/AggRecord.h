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

#ifndef LSST_QSERV_MASTER_AGGRECORD_H
#define LSST_QSERV_MASTER_AGGRECORD_H
#include "lsst/qserv/master/ValueExpr.h"
#include "lsst/qserv/master/ValueFactor.h"

namespace lsst { namespace qserv { namespace master {

// Record is derived from the previous parse framework's
// AggregateRecord class.  It is a value class for the information 
// needed to successfully perform aggregation of distributed queries.
// lbl and meaning record the original aggregation invocation (+alias)
// orig, pass, and fixup record SQL expressions
class AggRecord {
public:
    typedef boost::shared_ptr<AggRecord> Ptr;
    std::string origAlias;
    lsst::qserv::master::ValueExprPtr orig;
    lsst::qserv::master::ValueExprList pass;
    lsst::qserv::master::ValueExprList fixup;
    std::ostream& printTo(std::ostream& os);
    std::string getFuncParam() const;
    std::string getLabelText() const;
};
class AggRecord2 { // Test
public:
    typedef boost::shared_ptr<AggRecord2> Ptr;
    lsst::qserv::master::ValueFactorPtr orig;
    lsst::qserv::master::ValueExprList pass;
    lsst::qserv::master::ValueFactorPtr fixup;
    std::ostream& printTo(std::ostream& os);
    std::string getFuncParam() const;
    std::string getLabelText() const;
};

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_AGGRECORD_H

