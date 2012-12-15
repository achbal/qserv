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
// FuncExpr.cc houses the implementation of FuncExpr (a parsed
// function call expression)
#include "lsst/qserv/master/FuncExpr.h"

#include "lsst/qserv/master/ColumnRef.h"
#include "lsst/qserv/master/ValueExpr.h"
#include "lsst/qserv/master/QueryTemplate.h"
#include <iostream>
namespace qMaster=lsst::qserv::master;
using lsst::qserv::master::FuncExpr;

namespace { // File-scope helpers
}


FuncExpr::Ptr 
FuncExpr::newLike(FuncExpr const& src, std::string const& newName) {
    FuncExpr::Ptr e(new FuncExpr()); 
    e->name = newName;
    e->params = src.params; // Shallow list copy.
    return e;
}
FuncExpr::Ptr 
FuncExpr::newArg1(std::string const& newName, std::string const& arg1) {
    FuncExpr::Ptr e(new FuncExpr()); 
    e->name = newName;
    boost::shared_ptr<ColumnRef> cr(new ColumnRef("","",arg1));
    e->params.push_back(ValueExpr::newColumnRefExpr(cr));
    return e;
}

std::ostream& qMaster::operator<<(std::ostream& os, FuncExpr const& fe) {
    os << "(" << fe.name << ",";
    output(os, fe.params);
    os << ")";
    return os;
}
std::ostream& qMaster::operator<<(std::ostream& os, FuncExpr const* fe) {
    return os << *fe;
}

void qMaster::FuncExpr::render(qMaster::QueryTemplate& qt) const {
    qt.append(name); 
    qt.append("(");
    renderList(qt, params);
    qt.append(")");
}

