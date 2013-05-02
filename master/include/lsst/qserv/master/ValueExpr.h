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
// ValueExpr is a parse element commonly found in SelectList elements.

#ifndef LSST_QSERV_MASTER_VALUETERM_H
#define LSST_QSERV_MASTER_VALUETERM_H

#include <iostream>
#include <list>
#include <string>
#include <boost/shared_ptr.hpp>

namespace lsst { namespace qserv { namespace master {
// Forward
class QueryTemplate;

class ValueExpr; 
typedef boost::shared_ptr<ValueExpr> ValueExprPtr;
typedef std::list<ValueExprPtr> ValueExprList;
class ValueFactor; 

class ValueExpr {
public:
    enum Op {NONE=200, UNKNOWN, PLUS, MINUS, MULTIPLY, DIVIDE};
    struct FactorOp {
        boost::shared_ptr<ValueFactor> term;
        Op op;
    };
    typedef std::list<FactorOp> FactorOpList;

    std::string const& getAlias() const { return _alias; }
    void setAlias(std::string const& a) { _alias = a; }

    FactorOpList& getFactorOps() { return _factorOps; }
    FactorOpList const& getFactorOps() const { return _factorOps; }

    ValueExprPtr clone() const;
    friend std::ostream& operator<<(std::ostream& os, ValueExpr const& vt);
    friend std::ostream& operator<<(std::ostream& os, ValueExpr const* vt);

    static ValueExprPtr newSimple(boost::shared_ptr<ValueFactor> vt);
   
    friend class ValueExprFactory;
    class render;
    friend class render;
private:
    ValueExpr();
    std::string _alias;
    std::list<FactorOp> _factorOps;
};

class ValueExpr::render : public std::unary_function<ValueExpr, void> {
public:
    render(QueryTemplate& qt, bool needsComma) 
        : _qt(qt), _needsComma(needsComma), _count(0) {}
    void operator()(ValueExpr const& ve);
    void operator()(ValueExpr const* vep) { 
        if(vep) (*this)(*vep); }
    void operator()(boost::shared_ptr<ValueExpr> const& vep) { 
        (*this)(vep.get()); }
    QueryTemplate& _qt;
    bool _needsComma;
    int _count;
};


}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_VALUEEXPR_H
