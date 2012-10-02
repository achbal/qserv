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
// ModFactory constructs representations of misc modifier clauses in SQL such as
// ORDER BY, GROUP BY, LIMIT, and HAVING (not yet).
// LIMIT is assumed to only permit unsigned integers.

#ifndef LSST_QSERV_MASTER_MODFACTORY_H
#define LSST_QSERV_MASTER_MODFACTORY_H

// #include <list>
// #include <map>
#include <antlr/AST.hpp>
#include <boost/shared_ptr.hpp>

// Forward
class SqlSQL2Parser;

namespace lsst {
namespace qserv {
namespace master {
// Forward
class SelectFactory;
class ValueExprFactory;
class OrderByClause;
class GroupByClause;

class ModFactory {
public:
    // ANTLR handlers
    friend class SelectFactory;
    class GroupByH;
    friend class GroupByH;
    class OrderByH;
    friend class OrderByH;
    class LimitH;
    friend class LimitH;

    ModFactory(boost::shared_ptr<ValueExprFactory> vf);

    int getLimit() { return _limit; } // -1: not specified.
    boost::shared_ptr<OrderByClause> getOrderBy() { return _orderBy; }
    boost::shared_ptr<GroupByClause> getGroupBy() { return _groupBy; }
private:
    void attachTo(SqlSQL2Parser& p);
    void _importLimit(antlr::RefAST a);
    void _importOrderBy(antlr::RefAST a);
    void _importGroupBy(antlr::RefAST a);

    // Fields
    boost::shared_ptr<ValueExprFactory> _vFactory;
    int _limit;
    boost::shared_ptr<OrderByClause> _orderBy;
    boost::shared_ptr<GroupByClause> _groupBy;
};


}}} // namespace lsst::qserv::master

#endif // LSST_QSERV_MASTER_MODFACTORY_H

