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
// ModFactory.cc houses the implementation of the ModFactory, which
// is responsible for constructing representations of LIMIT, ORDER BY,
// and GROUP BY clauses. It has a placeholder to support HAVING.

#include "lsst/qserv/master/ModFactory.h"

// Std
#include <iterator>

#include <boost/make_shared.hpp>

// Package
#include "SqlSQL2Parser.hpp" // applies several "using antlr::***".
#include "lsst/qserv/master/parserBase.h" // Handler base classes
#include "lsst/qserv/master/parseTreeUtil.h" 
#include "lsst/qserv/master/ValueExprFactory.h"
#include "lsst/qserv/master/SelectListFactory.h" // ValueExpr
#include "lsst/qserv/master/SelectList.h" // Clauses

// namespace modifiers
namespace qMaster = lsst::qserv::master;

// Anonymous helpers
namespace {
} // anonymous namespace

////////////////////////////////////////////////////////////////////////
// ModFactory::LimitH
////////////////////////////////////////////////////////////////////////
class lsst::qserv::master::ModFactory::LimitH : public VoidOneRefFunc {
public:
    LimitH(lsst::qserv::master::ModFactory& mf) : _mf(mf) {}
    virtual void operator()(antlr::RefAST n) {
        _mf._importLimit(n);
    }
private:
    lsst::qserv::master::ModFactory& _mf;
};
////////////////////////////////////////////////////////////////////////
// ModFactory::OrderByH
////////////////////////////////////////////////////////////////////////
class lsst::qserv::master::ModFactory::OrderByH : public VoidOneRefFunc {
public:
    OrderByH(lsst::qserv::master::ModFactory& mf) : _mf(mf) {}
    virtual void operator()(antlr::RefAST n) {
        _mf._importOrderBy(n);
    }
private:
    lsst::qserv::master::ModFactory& _mf;
};
////////////////////////////////////////////////////////////////////////
// ModFactory::GroupByH
////////////////////////////////////////////////////////////////////////
class lsst::qserv::master::ModFactory::GroupByH : public VoidOneRefFunc {
public:
    GroupByH(lsst::qserv::master::ModFactory& mf) : _mf(mf) {}
    virtual void operator()(antlr::RefAST n) {
        _mf._importGroupBy(n);
    }
private:
    lsst::qserv::master::ModFactory& _mf;
};
////////////////////////////////////////////////////////////////////////
// ModFactory::HavingH
////////////////////////////////////////////////////////////////////////
class lsst::qserv::master::ModFactory::HavingH : public VoidOneRefFunc {
public:
    HavingH(lsst::qserv::master::ModFactory& mf) : _mf(mf) {}
    virtual void operator()(antlr::RefAST n) {
        _mf._importHaving(n);
    }
private:
    lsst::qserv::master::ModFactory& _mf;
};
////////////////////////////////////////////////////////////////////////
// ModFactory
////////////////////////////////////////////////////////////////////////
using qMaster::ModFactory;
ModFactory::ModFactory(boost::shared_ptr<ValueExprFactory> vf)
    : _vFactory(vf),
      _limit(-1)
{
    assert(vf.get());
    // FIXME
}

void ModFactory::attachTo(SqlSQL2Parser& p) {
    p._limitHandler.reset(new LimitH(*this));
    p._orderByHandler.reset(new OrderByH(*this));
    p._groupByHandler.reset(new GroupByH(*this));
    p._havingHandler.reset(new HavingH(*this));
}

void ModFactory::_importLimit(antlr::RefAST a) {
    // Limit always has an int.
    std::cout << "Limit got " << walkTreeString(a) << std::endl;
    assert(a.get());
    std::stringstream ss(a->getText());
    ss >> _limit;
}

void ModFactory::_importOrderBy(antlr::RefAST a) {
    _orderBy.reset(new OrderByClause());
    // ORDER BY takes a column ref (expression)
    std::cout << "orderby got " << walkTreeString(a) << std::endl;
    assert(a.get());
    while(a.get()) {
        assert(a->getType() == SqlSQL2TokenTypes::SORT_SPEC);
        RefAST key = a->getFirstChild();
        OrderByTerm ob;
        ob._order = OrderByTerm::DEFAULT;
        boost::shared_ptr<ValueExpr> ve;
        if(key->getType() == SqlSQL2TokenTypes::SORT_KEY) {
            ob._expr = _vFactory->newExpr(key->getFirstChild());
            RefAST sib = key->getNextSibling();
            if(sib.get() 
               && (sib->getType() == SqlSQL2TokenTypes::COLLATE_CLAUSE)) {
                RefAST cc = sib->getFirstChild();
                ob._collate = walkTreeString(cc);
                //orderby.addCollateClause(cc);
                sib = sib->getNextSibling();
            }
            if(sib.get()) {
                switch(sib->getType()) {
                case SqlSQL2TokenTypes::SQL2RW_asc:
                    ob._order = OrderByTerm::ASC;
                    break;
                case SqlSQL2TokenTypes::SQL2RW_desc:
                    ob._order = OrderByTerm::DESC;
                    break;
                default:
                    // throw "unknown order-by syntax";
                    break;
                }
            }
            _orderBy->_addTerm(ob);
        } else if(key->getType() == SqlSQL2TokenTypes::UNSIGNED_INTEGER) {
            // throw "positional order-by not allowed";
        } else {
            // throw "unknown order-by syntax";
        }
        a = a->getNextSibling();
    }
}

void ModFactory::_importGroupBy(antlr::RefAST a) {
    _groupBy = boost::make_shared<GroupByClause>();
    // GROUP BY takes a column reference (expression?)
    std::cout << "groupby got " << walkTreeString(a) << std::endl;
    assert(a.get());
    while(a.get()) {
        assert(a->getType() == SqlSQL2TokenTypes::GROUPING_COLUMN_REF);
        GroupByTerm gb;
        RefAST key = a->getFirstChild();
        boost::shared_ptr<ValueExpr> ve;
        if(key->getType() == SqlSQL2TokenTypes::COLUMN_REF) {
            gb._expr = _vFactory->newExpr(key->getFirstChild());
            RefAST sib = key->getNextSibling();
            if(sib.get() 
               && (sib->getType() == SqlSQL2TokenTypes::COLLATE_CLAUSE)) {
                RefAST cc = sib->getFirstChild();
                gb._collate = walkTreeString(cc);
                sib = sib->getNextSibling();
            }
        } else {
            // throw an exception
        }
        _groupBy->_addTerm(gb);
        a = a->getNextSibling();
    }
    
}

void ModFactory::_importHaving(antlr::RefAST a) {
    _having = boost::make_shared<HavingClause>();
    // HAVING takes an boolean expression that is dependent on an
    // aggregation expression that was specified in the select list.
    // Online examples for SQL HAVING always have only one aggregation
    // and only one boolean, so this code will only accept single
    // aggregation, single boolean expressions.
    // e.g. HAVING count(obj.ra_PS_sigma) > 0.04
    assert(a.get());
    std::cout << "having got " << walkTreeString(a) << std::endl;
    // For now, we will silently traverse and recognize but ignore.
    _having->_expr = walkTreeString(a); // FIXME
    
}
