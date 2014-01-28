/* 
 * LSST Data Management System
 * Copyright 2012-2013 LSST Corporation.
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
  * @file ValueFactorFactory.cc
  *
  * @brief ValueFactorFactory constructs ValueFactor instances from
  * ANTLR subtrees.
  *
  * @author Daniel L. Wang, SLAC
  */ 
#include "parser/ValueFactorFactory.h"
#include "log/Logger.h"

#include <stdexcept>

#include "parser/ColumnRefH.h"
#include "query/ColumnRef.h"
#include "query/FuncExpr.h"
#include "parser/parseTreeUtil.h"
#include "parser/ParseException.h" 
#include "query/ValueExpr.h" // For ValueExpr
#include "query/ValueFactor.h" // For ValueFactor
#include "SqlSQL2TokenTypes.hpp" 

// namespace modifiers
using antlr::RefAST;

////////////////////////////////////////////////////////////////////////
// Anonymous helpers
////////////////////////////////////////////////////////////////////////
namespace {

inline RefAST walkToSiblingBefore(RefAST node, int typeId) {
    RefAST last = node;
    for(; node.get(); node = node->getNextSibling()) {
        if(node->getType() == typeId) return last;
        last = node;
    }
    return RefAST();
}

inline std::string getSiblingStringBounded(RefAST left, RefAST right) {
    lsst::qserv::master::CompactPrintVisitor<RefAST> p;
    for(; left.get(); left = left->getNextSibling()) {
        p(left);
        if(left == right) break;
    }
    return p.result;
}
} // anonymous

namespace lsst { 
namespace qserv { 
namespace master { 

boost::shared_ptr<ValueFactor> 
newConstFactor(RefAST t) {
    return ValueFactor::newConstFactor(walkTreeString(t));
}

////////////////////////////////////////////////////////////////////////
// ValueFactorFactory implementation
////////////////////////////////////////////////////////////////////////
ValueFactorFactory::ValueFactorFactory(boost::shared_ptr<ColumnRefNodeMap> cMap) 
    : _columnRefNodeMap(cMap) {
}


/* VALUE_EXP               */
/* |             \         */
/* TERM   (TERM_OP TERM)*  */
boost::shared_ptr<ValueFactor> 
ValueFactorFactory::newFactor(antlr::RefAST a) {
    if(!_columnRefNodeMap) {
        throw std::logic_error("ValueFactorFactory missing _columnRefNodeMap");
    }
    boost::shared_ptr<ValueFactor> vt;
    int eType = a->getType();
    if(a->getType() == SqlSQL2TokenTypes::FACTOR) {
        a = a->getFirstChild(); // FACTOR is a parent placeholder element
    }
    eType = a->getType();
    LOGGER_DBG << "new ValueFactor: " << tokenText(a) << std::endl;
    switch(a->getType()) {
    case SqlSQL2TokenTypes::COLUMN_REF:
        a = a->getFirstChild();
        // COLUMN_REF should have REGULAR_ID as only child.
        // Fall through to REGULAR_ID handler
    case SqlSQL2TokenTypes::REGULAR_ID:
    case SqlSQL2TokenTypes::FUNCTION_SPEC:
        vt = _newColumnFactor(a);
        break;
    case SqlSQL2TokenTypes::SET_FCT_SPEC:
        vt = _newSetFctSpec(a);
        break;
    default: 
        vt = newConstFactor(a);
        break;
    }
    if(!vt) {
        throw std::logic_error("Faled to construct ValueFactor");
    }
    return vt;
}

boost::shared_ptr<ValueFactor> 
ValueFactorFactory::_newColumnFactor(antlr::RefAST t) {
    assert(_columnRefNodeMap);
    ColumnRefNodeMap& cMap = *_columnRefNodeMap;
    RefAST child = t->getFirstChild();
    if(t->getType() == SqlSQL2TokenTypes::FACTOR) {
        t = child;
        child = t->getFirstChild();
    } 
    boost::shared_ptr<ValueFactor> vt(new ValueFactor());
    boost::shared_ptr<FuncExpr> fe;
    RefAST last;
    // LOGGER_INF << "colterm: " << t->getType() << " "
    //           << t->getText() << std::endl;
    int tType = t->getType();
    switch(tType) {
    case SqlSQL2TokenTypes::COLUMN_REF: 
        t = child;
        child = t->getFirstChild();
        // Fall-through to REGULAR_ID handler
    case SqlSQL2TokenTypes::REGULAR_ID: 
        // make column ref. (no further children)
        {
            ColumnRefNodeMap::Map::const_iterator it = cMap.map.find(t);
            if(it == cMap.map.end()) {
                throw std::logic_error("Expected to find REGULAR_ID in table map");
            }
            ColumnRefNodeMap::Ref r = it->second;

            boost::shared_ptr<ColumnRef> newColumnRef;
            newColumnRef.reset(new ColumnRef(tokenText(r.db),
                                                      tokenText(r.table),
                                                      tokenText(r.column)));
            vt = ValueFactor::newColumnRefFactor(newColumnRef);
        }
        return vt;
    case SqlSQL2TokenTypes::FUNCTION_SPEC:
        //LOGGER_INF << "col child (fct): " << child->getType() << " "
        //          << child->getText() << std::endl;
        fe.reset(new FuncExpr());
        last = walkToSiblingBefore(child, SqlSQL2TokenTypes::LEFT_PAREN);
        fe->name = getSiblingStringBounded(child, last);
        last = last->getNextSibling(); // Advance to LEFT_PAREN
        if(!last.get()) { 
            throw ParseException("Expected LEFT_PAREN", last);
        }
        // Now fill params. 
        for(antlr::RefAST current = last->getNextSibling();
            current.get(); current = current->getNextSibling()) {
            // Should be a * or a value expr.
            boost::shared_ptr<ValueFactor> pvt;
            //LOGGER_INF << "fctspec param: " << current->getType() << " "
            //          << current->getText() << std::endl;
        
            switch(current->getType()) {
            case SqlSQL2TokenTypes::VALUE_EXP: 
                pvt = newFactor(current->getFirstChild());
                // pvt = newColumnFactor(current->getFirstChild(), cMap);
                break;
            case SqlSQL2TokenTypes::COMMA: continue;
            case SqlSQL2TokenTypes::RIGHT_PAREN: continue;
            default: 
                throw ParseException(
                    "ValueFactorFactory::newColumnFactor fct spec with ", 
                    current);
                break;
            }
            fe->params.push_back(ValueExpr::newSimple(pvt));
        }
        vt = ValueFactor::newFuncFactor(fe);
        return vt;
        
        break;
    default: 
        throw ParseException("ValueFactorFactory::newColumnFactor with ", t);
        break;
    }
    return boost::shared_ptr<ValueFactor>();
}

boost::shared_ptr<ValueFactor> 
ValueFactorFactory::_newSetFctSpec(antlr::RefAST expr) {
    assert(_columnRefNodeMap);
    ColumnRefNodeMap& cMap = *_columnRefNodeMap;
    boost::shared_ptr<FuncExpr> fe(new FuncExpr());
    //    LOGGER_INF << "set_fct_spec " << walkTreeString(expr) << std::endl;
    RefAST nNode = expr->getFirstChild();
    if(!nNode.get()) {
        throw ParseException("Missing name node of function spec", expr);
    }
    fe->name = nNode->getText();
    // Now fill params.
    antlr::RefAST current = nNode->getFirstChild();
    // Aggregation functions can only have one param.
    if(current->getType() != SqlSQL2TokenTypes::LEFT_PAREN) {
        throw ParseException("Expected LEFT_PAREN", current);
    }
    current = current->getNextSibling();
    // Should be a * or a value expr.
    boost::shared_ptr<ValueFactor> pvt;
    switch(current->getType()) {
    case SqlSQL2TokenTypes::VALUE_EXP: 
        pvt = _newColumnFactor(current->getFirstChild());
        break;
    case SqlSQL2TokenTypes::ASTERISK: 
        pvt = ValueFactor::newStarFactor("");
        break;
    default: break;
    }
    current = current->getNextSibling();
    if(current->getType() != SqlSQL2TokenTypes::RIGHT_PAREN) {
        throw ParseException("Expected RIGHT_PAREN", current);
    }
    fe->params.push_back(ValueExpr::newSimple(pvt));
    return ValueFactor::newAggFactor(fe);
}
}}} // lsst::qserv::master
