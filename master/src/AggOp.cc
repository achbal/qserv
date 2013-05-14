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
#include "lsst/qserv/master/AggOp.h"

#include <sstream>
#include "lsst/qserv/master/FuncExpr.h"
#include "lsst/qserv/master/ValueExpr.h"
#include "lsst/qserv/master/ValueFactor.h"

namespace qMaster=lsst::qserv::master;
using lsst::qserv::master::AggOp;
using lsst::qserv::master::AggRecord;
using lsst::qserv::master::ValueExpr;
using lsst::qserv::master::ValueFactor;
using lsst::qserv::master::FuncExpr;

namespace { // File-scope helpers

}
namespace lsst { namespace qserv {namespace master {
////////////////////////////////////////////////////////////////////////
// AggOp specializations
////////////////////////////////////////////////////////////////////////
class PassAggOp : public AggOp {
public:
    explicit PassAggOp(AggOp::Mgr& mgr) : AggOp(mgr) {}

    virtual AggRecord2::Ptr operator()(ValueFactor const& orig) {
        AggRecord2::Ptr arp(new AggRecord2());
        arp->orig = orig.clone();
        arp->pass.push_back(ValueExpr::newSimple(orig.clone()));
        arp->fixup = orig.clone();
        // Alias handling left to caller.
        return arp;
    } 
};
class CountAggOp : public AggOp {
public:
    explicit CountAggOp(AggOp::Mgr& mgr) : AggOp(mgr) {}

    virtual AggRecord2::Ptr operator()(ValueFactor const& orig) {
        AggRecord2::Ptr arp(new AggRecord2());
        std::string interName = _mgr.getAggName("COUNT");
        arp->orig = orig.clone();
        boost::shared_ptr<FuncExpr> fe;
        boost::shared_ptr<ValueFactor> vt;
        boost::shared_ptr<ValueExpr> passExpr;
        
        passExpr = ValueExpr::newSimple(orig.clone());
        passExpr->setAlias(interName);
        arp->pass.push_back(passExpr);

        fe = FuncExpr::newArg1("SUM", interName);
        vt = ValueFactor::newFuncFactor(fe);
        // orig alias handled by caller.
        arp->fixup = vt;
        return arp;
    }
};
class AvgAggOp : public AggOp {
public:
    explicit AvgAggOp(AggOp::Mgr& mgr) : AggOp(mgr) {}

    virtual AggRecord2::Ptr operator()(ValueFactor const& orig) {
        
        AggRecord2::Ptr arp(new AggRecord2());
        arp->orig = orig.clone();
        // Pass: get each aggregation subterm.
        boost::shared_ptr<FuncExpr> fe;
        boost::shared_ptr<ValueFactor const> origVf(orig.clone());
        boost::shared_ptr<ValueExpr> ve;
        std::string cAlias = _mgr.getAggName("COUNT"); 
        fe = FuncExpr::newLike(*origVf->getFuncExpr(), "COUNT");
        ve = ValueExpr::newSimple(ValueFactor::newFuncFactor(fe));
        ve->setAlias(cAlias);
        arp->pass.push_back(ve);

        std::string sAlias = _mgr.getAggName("SUM"); 
        fe = FuncExpr::newLike(*origVf->getFuncExpr(), "SUM");
        ve = ValueExpr::newSimple(ValueFactor::newFuncFactor(fe));
        ve->setAlias(sAlias);
        arp->pass.push_back(ve);

        boost::shared_ptr<FuncExpr> feSum;
        boost::shared_ptr<FuncExpr> feCount;
        feSum = FuncExpr::newArg1("SUM", sAlias);
        feCount = FuncExpr::newArg1("SUM", cAlias);
        ve.reset(new ValueExpr());
        ve->setAlias(orig.getAlias());
        ValueExpr::FactorOpList& factorOps = ve->getFactorOps();
        factorOps.clear();
        ValueExpr::FactorOp fo;
        fo.factor = ValueFactor::newFuncFactor(feSum);
        fo.op = ValueExpr::DIVIDE;
        factorOps.push_back(fo);
        fo.factor = ValueFactor::newFuncFactor(feCount);
        fo.op = ValueExpr::NONE;
        factorOps.push_back(fo);
        arp->fixup = ValueFactor::newExprFactor(ve);
        
#if 0        // FIXME!!
        // ArithExpr::newExpr(fe1,fe2);
        // Broken placeholder

        ve = ValueExpr::newSimple(ValueFactor::newFuncFactor(fe1));
        ve->setAlias(orig.getAlias());
        arp->fixup.push_back(ve);
#endif

        // arp->origAlias = orig.getAlias(); // No longer needed
        return arp;
    }
};

////////////////////////////////////////////////////////////////////////
// class AggOp::Mgr
////////////////////////////////////////////////////////////////////////
AggOp::Mgr::Mgr() {
    // Load the map
    AggOp::Ptr pass(new PassAggOp(*this));
    _map["COUNT"].reset(new CountAggOp(*this));
    _map["AVG"].reset(new AvgAggOp(*this));
    _map["MAX"] = pass;
    _map["MIN"] = pass;
    _map["SUM"] = pass;
    _seq = 0; // Note: accessor return ++_seq
}

AggOp::Ptr 
AggOp::Mgr::getOp(std::string const& name) {
    OpMap::const_iterator i = _map.find(name);
    if(i != _map.end()) return i->second;
    else return AggOp::Ptr();
}

AggRecord2::Ptr 
AggOp::Mgr::applyOp(std::string const& name, ValueFactor const& orig) {
    std::string n(name);
    std::transform(name.begin(), name.end(), n.begin(), ::toupper);
    AggOp::Ptr p = getOp(n);
    assert(p.get());
    return (*p)(orig);
}

std::string AggOp::Mgr::getAggName(std::string const& name) {
    std::stringstream ss;
    int s = getNextSeq();
    ss << "QS" << s << "_" << name;
    return ss.str();
}
}}} // lsst::qserv::master
