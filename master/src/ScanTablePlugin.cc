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
/**
  * @file
  *
  * @brief ScanTablePlugin implementation
  *
  * @author Daniel L. Wang, SLAC
  */

// No public interface 
#include "lsst/qserv/master/QueryPlugin.h" // Parent class


#include "lsst/qserv/master/ColumnRefMap.h"
#include "lsst/qserv/master/FromList.h"
#include "lsst/qserv/master/QsRestrictor.h"
#include "lsst/qserv/master/QueryContext.h"
#include "lsst/qserv/master/SelectList.h"
#include "lsst/qserv/master/SelectStmt.h"
#include "lsst/qserv/master/WhereClause.h"

#if 0
#include <deque>
#include <string>
#include <boost/pointer_cast.hpp>

#include "lsst/qserv/master/ColumnRef.h"
#include "lsst/qserv/master/FuncExpr.h"
#include "lsst/qserv/master/MetadataCache.h"
#include "lsst/qserv/master/Predicate.h"

#include "lsst/qserv/master/ValueFactor.h"
#include "lsst/qserv/master/ValueExpr.h"


namespace { // File-scope helpers
std::string const UDF_PREFIX = "scisql_";
} // anonymous
#endif

namespace lsst {
namespace qserv {
namespace master {
#if 0
typedef std::pair<std::string,std::string> StringPair;

ValueExprTerm::Ptr newColRef(std::string const& key) {
    // FIXME: should apply QueryContext.
    boost::shared_ptr<ColumnRef> cr(new ColumnRef("","", key));
    ValueExprTerm::Ptr p(new ValueExprTerm);
    p->_expr = ValueExpr::newSimple(ValueFactor::newColumnRefFactor(cr));
    return p;
}
PassTerm::Ptr newPass(std::string const& s) {
    PassTerm::Ptr p(new PassTerm);
    p->_text = s;
    return p;
}
template <typename C>
PassListTerm::Ptr newPassList(C& c) {
    PassListTerm::Ptr p(new PassListTerm);
    p->_terms.insert(p->_terms.begin(), c.begin(), c.end());
    return p;
}

template <typename C>
ValueExprTerm::Ptr newFunc(char const fName[],
                           std::string const& tableAlias,
                           StringPair const& chunkColumns,
                           C& c) {
    typedef boost::shared_ptr<ColumnRef> CrPtr;
    FuncExpr::Ptr fe(new FuncExpr);
    fe->name = UDF_PREFIX + fName;
    fe->params.push_back(ValueExpr::newSimple(
                             ValueFactor::newColumnRefFactor(
                                 CrPtr(new ColumnRef("", tableAlias,
                                                     chunkColumns.first)))
                             ));
    fe->params.push_back(ValueExpr::newSimple(
                             ValueFactor::newColumnRefFactor(
                                 CrPtr(new ColumnRef("", tableAlias,
                                                     chunkColumns.second)))
                             ));

    typename C::const_iterator i;
    for(i = c.begin(); i != c.end(); ++i) {
        fe->params.push_back(ValueExpr::newSimple(ValueFactor::newConstFactor(*i)));
    }

    ValueExprTerm::Ptr p(new ValueExprTerm);
    p->_expr = ValueExpr::newSimple(ValueFactor::newFuncFactor(fe));
    return p;
}


struct RestrictorEntry {
    RestrictorEntry(std::string const& alias_,
                 StringPair const& chunkColumns_,
                 std::string const& keyColumn_)
        : alias(alias_),
          chunkColumns(chunkColumns_),
          keyColumn(keyColumn_)
        {}
    std::string alias;
    StringPair chunkColumns;
    std::string keyColumn;
};
typedef std::deque<RestrictorEntry> RestrictorEntries;
class getTable {
public:

    explicit getTable(MetadataCache& metadata, RestrictorEntries& entries)
        : _metadata(metadata),
          _entries(entries) {}
    void operator()(TableRefN::Ptr t) {
        if(!t) {
            throw std::invalid_argument("NULL TableRefN::Ptr");
        }
        std::string const& db = t->getDb();
        std::string const& table = t->getTable();

        // Is table chunked?
        if(!_metadata.checkIfTableIsChunked(db, table)) {
            return; // Do nothing for non-chunked tables
        }
        // Now save an entry for WHERE clause processing.
        std::string alias = t->getAlias();
        if(alias.empty()) {
            // For now, only accept aliased tablerefs (should have
            // been done earlier)
            throw std::logic_error("Unexpected unaliased table reference");
        }
        std::vector<std::string> pCols = _metadata.getPartitionCols(db, table);
        RestrictorEntry se(alias,
                        StringPair(pCols[0], pCols[1]),
                        pCols[2]);
        _entries.push_back(se);
    }
    MetadataCache& _metadata;
    RestrictorEntries& _entries;
};
#endif
////////////////////////////////////////////////////////////////////////
// ScanTablePlugin declaration
////////////////////////////////////////////////////////////////////////
/// ScanTablePlugin replaces a qserv restrictor spec with directives
/// that can be executed on a qserv mysqld. This plugin should be
/// execute after aliases for tables have been generates, so that the
/// new restrictor function clauses/phrases can use the aliases.
class ScanTablePlugin : public lsst::qserv::master::QueryPlugin {
public:
    // Types
    typedef boost::shared_ptr<ScanTablePlugin> Ptr;

    virtual ~ScanTablePlugin() {}

    virtual void prepare() {}

    virtual void applyLogical(SelectStmt& stmt, QueryContext&);
    virtual void applyFinal(QueryContext& context);

private:
    StringPairList _findScanTables(SelectStmt& stmt, QueryContext& context);
    StringPairList _scanTables;
};

////////////////////////////////////////////////////////////////////////
// ScanTablePluginFactory declaration+implementation
////////////////////////////////////////////////////////////////////////
class ScanTablePluginFactory : public lsst::qserv::master::QueryPlugin::Factory {
public:
    // Types
    typedef boost::shared_ptr<ScanTablePluginFactory> Ptr;
    ScanTablePluginFactory() {}
    virtual ~ScanTablePluginFactory() {}

    virtual std::string getName() const { return "ScanTable"; }
    virtual lsst::qserv::master::QueryPlugin::Ptr newInstance() {
        return lsst::qserv::master::QueryPlugin::Ptr(new ScanTablePlugin());
    }
};

////////////////////////////////////////////////////////////////////////
// registerScanTablePlugin implementation
////////////////////////////////////////////////////////////////////////
namespace {
struct registerPlugin {
    registerPlugin() {
        ScanTablePluginFactory::Ptr f(new ScanTablePluginFactory());
        QueryPlugin::registerClass(f);
    }
};
// Static registration
registerPlugin registerScanTablePlugin;
}

////////////////////////////////////////////////////////////////////////
// ScanTablePlugin implementation
////////////////////////////////////////////////////////////////////////
void
ScanTablePlugin::applyLogical(SelectStmt& stmt, QueryContext& context) {
    _scanTables = _findScanTables(stmt, context);
    context.scanTables = _scanTables;
}

void 
ScanTablePlugin::applyFinal(QueryContext& context) {
    int const scanThreshold = 2;
    if(context.chunkCount < scanThreshold) {
        context.scanTables.clear();
        std::cout << "Squash scan tables: <" << scanThreshold
                  << " chunks." << std::endl;
    }
}

struct getPartitioned : public TableRefN::Func {
    getPartitioned(StringPairList& sList_) : sList(sList_) {}
    virtual void operator()(TableRefN& t) { 
        (*this)(const_cast<TableRefN const&>(t));
    }
    virtual void operator()(TableRefN const& tRef) {
        SimpleTableN const* t = dynamic_cast<SimpleTableN const*>(&tRef);
        if(t) {
            StringPair entry(t->getDb(), t->getTable());
            if(found.end() != found.find(entry)) return;
            sList.push_back(entry);
            found.insert(entry);
        } else {
            throw std::logic_error("Unexpected non-simple table in apply()");
        }
    }
    std::set<StringPair> found;
    StringPairList& sList;
};

// helper
StringPairList filterPartitioned(TableRefnList const& tList) {
    StringPairList list;
    getPartitioned gp(list);
    for(TableRefnList::const_iterator i=tList.begin(), e=tList.end();
        i != e; ++i) {
        (**i).apply(gp);
    }
    return list;
}

StringPairList
ScanTablePlugin::_findScanTables(SelectStmt& stmt, QueryContext& context) {
    // Might be better as a separate plugin

    // All tables of a query are scan tables if the statement both:
    // a. has non-trivial spatial scope (all chunks? >1 chunk?)
    // b. requires column reading

    // a. means that the there is a spatial scope specification in the
    // WHERE clause or none at all (everything matches). However, an
    // objectId specification counts as a trivial spatial scope,
    // because it evaluates to a specific set of subchunks. We limit
    // the objectId specification, but the limit can be large--each
    // concrete objectId incurs at most the cost of one subchunk.

    // b. means that columns are needed to process the query.
    // In the SelectList, count(*) does not need columns, but *
    // does. So do ra_PS and iFlux_SG*10
    // In the WhereClause, this means that we have expressions that
    // require columns to evaluate.

    // When there is no WHERE clause that requires column reading,
    // the presence of a small-valued LIMIT should be enough to
    // de-classify a query as a scanning query.

    bool hasSelectColumnRef = false; // Requires row-reading for
                                     // results
    bool hasSelectStar = false; // Requires reading all rows
    bool hasSpatialSelect = false; // Recognized chunk restriction
    bool hasWhereColumnRef = false; // Makes count(*) non-trivial
    bool hasSecondaryKey = false; // Using secondaryKey to restrict
                                  // coverage, e.g., via objectId=123
                                  // or objectId IN (123,133) ?

    if(stmt.hasWhereClause()) {
        WhereClause& wc = stmt.getWhereClause();
        // Check WHERE for spatial select
        boost::shared_ptr<QsRestrictor::List const> restrs = wc.getRestrs();
        hasSpatialSelect = restrs && !restrs->empty();


        // Look for column refs
        boost::shared_ptr<ColumnRefMap::List const> crl = wc.getColumnRefs();
        if(crl) {
            hasWhereColumnRef = !crl->empty();
#if 0
            boost::shared_ptr<AndTerm> aterm = wc.getRootAndTerm();
            if(aterm) {
                // Look for secondary key matches
                typedef BoolTerm::PtrList PtrList;
                for(PtrList::iterator i = aterm->iterBegin();
                    i != aterm->iterEnd(); ++i) {
                    if(testIfSecondary(**i)) {
                        hasSecondaryKey = true;
                        break;
                    }
                }
            }
#endif
        }

    }
    SelectList& sList = stmt.getSelectList();
    boost::shared_ptr<ValueExprList> sVexpr = sList.getValueExprList();

    if(sVexpr) {
        ColumnRef::List cList; // For each expr, get column refs.

        typedef ValueExprList::const_iterator Iter;
        for(Iter i=sVexpr->begin(), e=sVexpr->end(); i != e; ++i) {
            (*i)->findColumnRefs(cList);
        }
        // Resolve column refs, see if they include partitioned
        // tables.
        typedef ColumnRef::List::const_iterator ColIter;
        for(ColIter i=cList.begin(), e=cList.end(); i != e; ++i) {
            // FIXME: Need to resolve and see if it's a partitioned table.
            hasSelectColumnRef = true;
        }
    }

    StringPairList scanTables;
    // Right now, if there is any spatial restriction, it's not a shared scan.
    // In the future, we may want a threshold area percentage, above
    // which a query gets relegated as a "partial scan". Maybe 1% ?
    if(hasSelectColumnRef || hasSelectStar) {
        if(hasSpatialSelect || hasSecondaryKey) {
            std::cout << "**** Not a scan ****" << std::endl;
            // Not a scan? Leave scanTables alone
        }
        // 
        std::cout << "**** SCAN (column ref, non-spatial-idx)****" << std::endl;
        // Scan tables = all partitioned tables
        scanTables = filterPartitioned(stmt.getFromList().getTableRefnList());

        // FIXME: Add scan tables to scanTables
        
    } else {
        // count(*): still a scan with a non-trivial where.
        std::cout << "**** SCAN (filter) ****" << std::endl;
        scanTables = filterPartitioned(stmt.getFromList().getTableRefnList());
    }

    return scanTables;
}

}}} // namespace lsst::qserv::master
