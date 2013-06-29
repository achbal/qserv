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
  * @file AggregatePlugin.cc
  *
  * @brief Implemenation of AggregatePlugin that primarily operates in
  * the second phase of query manipulation. It rewrites the
  * select-list of a query in their parallel and merging instances so
  * that a SUM() becomes a SUM() followed by another SUM(), AVG()
  * becomes SUM() and COUNT() followed by SUM()/SUM(), etc.  
  *
  * @author Daniel L. Wang, SLAC
  */
#include "lsst/qserv/master/AggregatePlugin.h"
#include <string>
#include "lsst/qserv/master/common.h"
#include "lsst/qserv/master/QueryContext.h"
#include "lsst/qserv/master/QueryPlugin.h"
#include "lsst/qserv/master/QueryTemplate.h"
#include "lsst/qserv/master/ValueExpr.h"
#include "lsst/qserv/master/ValueFactor.h"
#include "lsst/qserv/master/FuncExpr.h"

#include "lsst/qserv/master/SelectList.h"
#include "lsst/qserv/master/SelectStmt.h"
#include "lsst/qserv/master/AggOp.h"

namespace { // Anonymous helpers
} // Anonymous namespace

namespace lsst { 
namespace qserv { 
namespace master {

template <class C>
class convertAgg {
public:
    typedef typename C::value_type T;
    convertAgg(C& pList_, C& mList_, AggOp::Mgr& aMgr_) 
        : pList(pList_), mList(mList_), aMgr(aMgr_) {}
    void operator()(T const& e) {
        _makeRecord2(*e);
    }

private:
    class checkAgg {
    public: // Simply check for aggregation functions
        checkAgg(bool& hasAgg_) : hasAgg(hasAgg_) {}
        inline void operator()(ValueExpr::FactorOp const& fo) {
            if(!fo.factor.get());
            if(fo.factor->getType() == ValueFactor::AGGFUNC) { 
                hasAgg = true; }
        }
        bool& hasAgg;
    };

    void _makeRecord2(ValueExpr const& e) {
        bool hasAgg = false;
        checkAgg ca(hasAgg);
        ValueExpr::FactorOpList const& factorOps = e.getFactorOps();
        std::for_each(factorOps.begin(), factorOps.end(), ca);

        if(!ca.hasAgg) {
            pList.push_back(e.clone()); // Is this sufficient?
            return;
        } 
        // For exprs with aggregation, we must separate out the
        // expression into pieces.
        // Split the elements of a ValueExpr into its
        // constituent ValueFactors, compute the lists in parallel, and
        // then compute the expression result from the parallel
        // results during merging.
        ValueExprPtr mergeExpr(new ValueExpr);
        ValueExpr::FactorOpList& mergeFactorOps = mergeExpr->getFactorOps();
        for(ValueExpr::FactorOpList::const_iterator i=factorOps.begin();
            i != factorOps.end(); ++i) {
            ValueFactorPtr newFactor = i->factor->clone();
            if(newFactor->getType() != ValueFactor::AGGFUNC) {
                pList.push_back(ValueExpr::newSimple(newFactor));
            } else {
                AggRecord r;
                r.orig = newFactor;
                assert(newFactor->getFuncExpr().get());        
                AggRecord::Ptr p = aMgr.applyOp(newFactor->getFuncExpr()->name,
                                                 *newFactor);
                assert(p.get());
                pList.insert(pList.end(), p->parallel.begin(), p->parallel.end());
                ValueExpr::FactorOp m;
                m.factor = p->merge;
                m.op = i->op;
                mergeFactorOps.push_back(m);
            }
        }
        mList.push_back(mergeExpr);
    }
    
    C& pList;
    C& mList;
    AggOp::Mgr& aMgr;
};

////////////////////////////////////////////////////////////////////////
// AggregatePlugin declaration
////////////////////////////////////////////////////////////////////////
class AggregatePlugin : public QueryPlugin {
public:
    // Types
    typedef boost::shared_ptr<AggregatePlugin> Ptr;
    
    virtual ~AggregatePlugin() {}

    virtual void prepare() {}

    virtual void applyLogical(SelectStmt& stmt, QueryContext&) {}
    virtual void applyPhysical(QueryPlugin::Plan& p, QueryContext&);
private:
    AggOp::Mgr _aMgr;
};

////////////////////////////////////////////////////////////////////////
// AggregatePluginFactory declaration+implementation
////////////////////////////////////////////////////////////////////////
class AggregatePluginFactory : public QueryPlugin::Factory {
public:
    // Types
    typedef boost::shared_ptr<AggregatePluginFactory> Ptr;
    AggregatePluginFactory() {}
    virtual ~AggregatePluginFactory() {}

    virtual std::string getName() const { return "Aggregate"; }
    virtual lsst::qserv::master::QueryPlugin::Ptr newInstance() {
        return lsst::qserv::master::QueryPlugin::Ptr(new AggregatePlugin());
    }
};

////////////////////////////////////////////////////////////////////////
// registerAggregatePlugin implementation
////////////////////////////////////////////////////////////////////////
// factory registration
void 
registerAggregatePlugin() {
    AggregatePluginFactory::Ptr f(new AggregatePluginFactory());
    lsst::qserv::master::QueryPlugin::registerClass(f);
}

////////////////////////////////////////////////////////////////////////
// AggregatePlugin implementation
////////////////////////////////////////////////////////////////////////
void
AggregatePlugin::applyPhysical(QueryPlugin::Plan& p, 
                               QueryContext&  context) {
    // For each entry in original's SelectList, modify the SelectList
    // for the parallel and merge versions.
    // Set hasMerge to true if aggregation is detected.
    SelectList& oList = p.stmtOriginal.getSelectList();
    SelectList& pList = p.stmtParallel.getSelectList();
    SelectList& mList = p.stmtMerge.getSelectList();
    boost::shared_ptr<ValueExprList> vlist;
    vlist = oList.getValueExprList();
    assert(vlist.get());
    
    printList(std::cout, "aggr origlist", *vlist) << std::endl;
    // Clear out select lists, since we are rewriting them.
    pList.getValueExprList()->clear();
    mList.getValueExprList()->clear();
    AggOp::Mgr m; // Eventually, this can be shared?
    convertAgg<ValueExprList> ca(*pList.getValueExprList(), 
                                 *mList.getValueExprList(),
                                 m);
    std::for_each(vlist->begin(), vlist->end(), ca);
    QueryTemplate qt;
    pList.renderTo(qt);
    std::cout << "pass: " << qt.dbgStr() << std::endl;
    qt.clear();
    mList.renderTo(qt);
    std::cout << "fixup: " << qt.dbgStr() << std::endl;

    // Also need to operate on GROUP BY.
    // update context.
    if(m.hasAggregate()) { context.needsMerge = true; }
}
}}} // lsst::qserv::master
