// -*- LSST-C++ -*-
/* 
 * LSST Data Management System
 * Copyright 2013 LSST Corporation.
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
// OrderByClause is a representation of a SQL ORDER BY clause.
// It consists of OrderByTerm objects.
// HavingClause is defined here as well, but real support has not yet
// been implemented.
#ifndef LSST_QSERV_MASTER_ORDERBYCLAUSE_H
#define LSST_QSERV_MASTER_ORDERBYCLAUSE_H

#include <deque>
#include <string>
#include <boost/shared_ptr.hpp>
#include "lsst/qserv/master/ValueExpr.h"

namespace lsst {
namespace qserv {
namespace master {
#if 0
// Forward
class ColumnRefMap;
class ColumnAliasMap;
class QueryTemplate;
class BoolTerm;
class GroupByClause;
#endif

class OrderByTerm {
public:
    enum Order {DEFAULT, ASC, DESC};
        
    OrderByTerm() {}
    OrderByTerm(boost::shared_ptr<ValueExpr> val,
                Order _order,
                std::string _collate);

    ~OrderByTerm() {}

    boost::shared_ptr<const ValueExpr> getExpr();
    Order getOrder() const;
    std::string getCollate() const;
    void renderTo(QueryTemplate& qt) const;
    class render;

private:
    friend std::ostream& operator<<(std::ostream& os, OrderByTerm const& ob);
    friend class ModFactory;

    boost::shared_ptr<ValueExpr> _expr;
    Order _order;
    std::string _collate;
};
class OrderByTerm::render : public std::unary_function<OrderByTerm, void> {
public:
    render(QueryTemplate& qt) : _qt(qt), _count(0) {}
    void operator()(OrderByTerm const& t) { t.renderTo(_qt); }
    QueryTemplate& _qt;
    int _count;
};


class OrderByClause {
public:
    typedef std::deque<OrderByTerm> List;

    OrderByClause() : _terms (new List()) {}
    ~OrderByClause() {}

    std::string getGenerated();
    void renderTo(QueryTemplate& qt) const;
    boost::shared_ptr<OrderByClause> copyDeep();
    boost::shared_ptr<OrderByClause> copySyntax();

private:
    friend std::ostream& operator<<(std::ostream& os, OrderByClause const& oc);
    friend class ModFactory;

    void _addTerm(OrderByTerm const& t) {_terms->push_back(t); }
    boost::shared_ptr<List> _terms;
};

class HavingClause {
public:
    HavingClause() {}
    ~HavingClause() {}

    std::string getGenerated();
    void renderTo(QueryTemplate& qt) const;
    boost::shared_ptr<HavingClause> copyDeep();
    boost::shared_ptr<HavingClause> copySyntax();

private:
    friend std::ostream& operator<<(std::ostream& os, HavingClause const& h);
    friend class ModFactory;
    std::string _expr;
};

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_ORDERBYCLAUSE_H

