/*
 * LSST Data Management System
 * Copyright 2012-2014 LSST Corporation.
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
  * @file
  *
  * @brief Implementation of OrderByTerm and OrderByClause
  *
  * @author Daniel L. Wang, SLAC
  */

#include "query/OrderByClause.h"

// System headers
#include <iostream>
#include <iterator>
#include <sstream>

// Third-party headers
#include <boost/make_shared.hpp>

// Local headers
#include "query/QueryTemplate.h"


namespace lsst {
namespace qserv {
namespace query {

char const* getOrderStr(OrderByTerm::Order o) {
    switch(o) {
    case OrderByTerm::ASC: return "ASC";
    case OrderByTerm::DESC: return "DESC";
    case OrderByTerm::DEFAULT: return "";
    default: return "UNKNOWN_ORDER";
    }
}
////////////////////////////////////////////////////////////////////////
// OrderByTerm
////////////////////////////////////////////////////////////////////////
void
OrderByTerm::renderTo(QueryTemplate& qt) const {
    ValueExpr::render r(qt, true);
    r(_expr);
    if(!_collate.empty()) {
        qt.append("COLLATE");
        qt.append(_collate);
    }
    char const* orderStr = getOrderStr(_order);
    if(orderStr && orderStr[0] != '\0') {
        qt.append(orderStr);
    }
}

std::ostream&
operator<<(std::ostream& os, OrderByTerm const& t) {
    os << *(t._expr);
    if(!t._collate.empty()) os << " COLLATE " << t._collate;
    char const* orderStr = getOrderStr(t._order);
    if(orderStr && orderStr[0] != '\0') {
        os << " " << orderStr << " ";
    }
    return os;
}
////////////////////////////////////////////////////////////////////////
// OrderByClause
////////////////////////////////////////////////////////////////////////
std::ostream&
operator<<(std::ostream& os, OrderByClause const& c) {
    if(c._terms.get()) {
        os << "ORDER BY ";
        std::copy(c._terms->begin(),c._terms->end(),
                  std::ostream_iterator<OrderByTerm>(os,", "));
    }
    return os;
}

std::string
OrderByClause::getGenerated() {
    QueryTemplate qt;
    renderTo(qt);
    return qt.dbgStr();
}

void
OrderByClause::renderTo(QueryTemplate& qt) const {
    if(_terms.get() && _terms->size() > 0) {
        bool first = true;
        List::const_iterator i,e;
        OrderByTerm::render r(qt);
        for(i=_terms->begin(), e=_terms->end(); i != e; ++i) {
            if(!first) {
                qt.append(",");
            }
            else {
                first = false;
            }
            r(*i);
        }
    }
}

boost::shared_ptr<OrderByClause> OrderByClause::clone() const {
    return boost::make_shared<OrderByClause>(*this); // FIXME
}
boost::shared_ptr<OrderByClause> OrderByClause::copySyntax() {
    return boost::make_shared<OrderByClause>(*this);
}

}}} // namespace lsst::qserv::query
