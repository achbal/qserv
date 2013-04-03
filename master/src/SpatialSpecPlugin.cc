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
// SpatialSpecPlugin replaces a spatial specification with directives
// that can be executed on a qserv mysqld.
// This plugin should be execute after aliases for tables have been
// generates, so that the new spatial function clauses/phrases can use
// the aliases.

#include "lsst/qserv/master/SpatialSpecPlugin.h"
#include <deque>
#include <string>

#include "lsst/qserv/master/QueryPlugin.h"
#include "lsst/qserv/master/QueryContext.h"
#include "lsst/qserv/master/SelectStmt.h"
#include "lsst/qserv/master/TableRefChecker.h"
#include "lsst/qserv/master/ColumnRef.h"
#include "lsst/qserv/master/FromList.h"
#include "lsst/qserv/master/FuncExpr.h"
#include "lsst/qserv/master/WhereClause.h"

#if 0
#include "lsst/qserv/master/SelectList.h"
#include "lsst/qserv/master/FuncExpr.h"
#include "lsst/qserv/master/SphericalBoxStrategy.h"
#include "lsst/qserv/master/TableNamer.h"
#include "lsst/qserv/master/QueryMapping.h"
#endif

namespace qMaster=lsst::qserv::master;

namespace { // File-scope helpers
std::string const UDF_PREFIX = "scisql_";
} // anonymous

namespace lsst { namespace qserv { namespace master {
typedef std::pair<std::string,std::string> StringPair;

ValueExprTerm::Ptr newColRef(std::string const& key) {
    // FIXME: should apply QueryContext.
    boost::shared_ptr<ColumnRef> cr(new ColumnRef("","", key));
    ValueExprTerm::Ptr p(new ValueExprTerm);
    p->_expr = ValueExpr::newColumnRefExpr(cr);
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
                           StringPair const& chunkColumns, 
                           C& c) {
    typedef boost::shared_ptr<ColumnRef> CrPtr;
    FuncExpr::Ptr fe(new FuncExpr);
    fe->name = UDF_PREFIX + fName;
    fe->params.push_back(ValueExpr::newColumnRefExpr(
                             CrPtr(new ColumnRef("", "", chunkColumns.first))
                             ));
    fe->params.push_back(ValueExpr::newColumnRefExpr(
                             CrPtr(new ColumnRef("", "", chunkColumns.second))
                             ));

    typename C::const_iterator i;
    for(i = c.begin(); i != c.end(); ++i) {
        fe->params.push_back(ValueExpr::newConstExpr(*i));
    }

    ValueExprTerm::Ptr p(new ValueExprTerm);
    p->_expr = ValueExpr::newFuncExpr(fe);
    return p;
}


struct SpatialEntry {
    SpatialEntry(std::string const& alias_, 
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
typedef std::deque<SpatialEntry> SpatialEntries;
class getTable {
public:
    
    explicit getTable(TableRefChecker& trc, SpatialEntries& entries) 
        : _tableRefChecker(trc), 
          _entries(entries) {}
    void operator()(qMaster::TableRefN::Ptr t) {
        assert(t.get());
        std::string db = t->getDb();
        std::string table = t->getTable();
        
        // Is table chunked?
        if(!_tableRefChecker.isChunked(db, table)) {
            return; // Do nothing for non-chunked tables            
        }
        // Now save an entry for WHERE clause processing.
        std::string alias = t->getAlias();
        assert(!alias.empty()); // For now, only accept aliased
                                // tablerefs (should have been done
                                // earlier)
        SpatialEntry se(alias,
                        _tableRefChecker.getSpatialColumns(db, table),
                        _tableRefChecker.getKeyColumn(db,table));
        _entries.push_back(se);
    }
    TableRefChecker& _tableRefChecker;
    SpatialEntries& _entries;
};
////////////////////////////////////////////////////////////////////////
// SpatialSpec declaration
////////////////////////////////////////////////////////////////////////
class SpatialSpecPlugin : public lsst::qserv::master::QueryPlugin {
public:
    // Types
    typedef boost::shared_ptr<SpatialSpecPlugin> Ptr;
    class Restriction;
    
    virtual ~SpatialSpecPlugin() {}

    /// Prepare the plugin for a query
    virtual void prepare() {}

    /// Apply the plugin's actions to the parsed, but not planned query
    virtual void applyLogical(SelectStmt& stmt, QueryContext&);

    /// Apply the plugins's actions to the concrete query plan.
    virtual void applyPhysical(QueryPlugin::Plan& p, QueryContext& context);
private:
    BoolTerm::Ptr _makeCondition(boost::shared_ptr<QsRestrictor> const restr,
                                 SpatialEntry const& spatialEntry);


};

////////////////////////////////////////////////////////////////////////
// SpatialSpecPlugin::Restriction
// Generates WHERE clause terms from restriction specs. Borrowed from
// older parsing framework. 
////////////////////////////////////////////////////////////////////////
class SpatialSpecPlugin::Restriction {
public:
    Restriction(QsRestrictor const& r) 
        : _name(r._name) {
        _setGenerator(r);
    }
    virtual BoolFactor::Ptr generate(SpatialEntry const& e) {
        return (*_generator)(e);
    }

    // std::string getUdfCallString(std::string const& tName, 
    //                              StringMap const& tableConfig) const {
    //     if(_generator.get()) {
    //         return (*_generator)(tName, tableConfig);
    //     }
    //     return std::string();
    // }
    class Generator {
    public:
        virtual ~Generator() {}
        virtual BoolFactor::Ptr operator()(SpatialEntry const& e) = 0;
    };
private:
    class ObjectIdGenerator : public Generator {
    public:
        ObjectIdGenerator(QsRestrictor::StringList const& params_)
            : params(params_.begin(), params_.end()) {
        }

        virtual BoolFactor::Ptr operator()(SpatialEntry const& e) {
            BoolFactor::Ptr newFactor(new BoolFactor);
            newFactor->_terms.push_back(newColRef(e.keyColumn));
            newFactor->_terms.push_back(newPass("IN"));
            newFactor->_terms.push_back(newPassList(params));
            return newFactor;
        }
        std::vector<std::string> params;
    };

    class AreaGenerator : public Generator {
    public:
        AreaGenerator(char const* fName_, int paramCount_,
                      QsRestrictor::StringList const& params_) 
            :  fName(fName_), paramCount(paramCount_), params(params_) {
            if(paramCount_ == USE_STRING) {
                // Convert param list to one quoted string.
                // This bundles a variable-sized list into a single
                // parameter to work with the MySQL UDF facility.
            }
        }

        virtual BoolFactor::Ptr operator()(SpatialEntry const& e) {
            BoolFactor::Ptr newFactor(new BoolFactor);
            BfTerm::PtrList& terms = newFactor->_terms;
            
            terms.push_back(newFunc(fName, 
                                    e.chunkColumns,
                                    params));
            terms.push_back(newPass("="));
            terms.push_back(newPass("1"));
            return newFactor;

        }
        char const* const fName;
        int const paramCount;
        QsRestrictor::StringList const& params; 
        static const int USE_STRING = -999;
    };

    void _setGenerator(QsRestrictor const& r) {
        if(r._name == "qserv_areaspec_box") {
            _generator.reset(dynamic_cast<Generator*>
                             (new AreaGenerator("s2PtInBox", 
                                                4, r._params)));
        } else if(r._name == "qserv_areaspec_circle") {
            _generator.reset(dynamic_cast<Generator*>
                             (new AreaGenerator("s2PtInCircle", 
                                                3, r._params)));
        } else if(r._name == "qserv_areaspec_ellipse") {
            _generator.reset(dynamic_cast<Generator*>
                             (new AreaGenerator("s2PtInEllipse", 
                                                5, r._params)));
        } else if(r._name == "qserv_areaspec_poly") {
            _generator.reset(dynamic_cast<Generator*>
                             (new AreaGenerator("s2PtInCPoly", 
                                                AreaGenerator::USE_STRING,
                                                r._params)));
        } else if(_name == "qserv_objectId") {
            ObjectIdGenerator* g = new ObjectIdGenerator(r._params);
            _generator.reset(dynamic_cast<Generator*>(g));
        } else {
            std::cout << "Unmatched restriction spec: " << _name 
                      << ", ignoring." << std::endl;
        }
    }
    std::string _name;
    std::vector<double> _params;
    boost::shared_ptr<Generator> _generator;
};


////////////////////////////////////////////////////////////////////////
// SpatialSpecPluginFactory declaration+implementation
////////////////////////////////////////////////////////////////////////
class SpatialSpecPluginFactory : public lsst::qserv::master::QueryPlugin::Factory {
public:
    // Types
    typedef boost::shared_ptr<SpatialSpecPluginFactory> Ptr;
    SpatialSpecPluginFactory() {}
    virtual ~SpatialSpecPluginFactory() {}

    virtual std::string getName() const { return "SpatialSpec"; }
    virtual lsst::qserv::master::QueryPlugin::Ptr newInstance() {
        return lsst::qserv::master::QueryPlugin::Ptr(new SpatialSpecPlugin());
    }
};

////////////////////////////////////////////////////////////////////////
// registarSpatialSpecPlugin implementation
////////////////////////////////////////////////////////////////////////
// factory registration
void 
registerSpatialSpecPlugin() {
    SpatialSpecPluginFactory::Ptr f(new SpatialSpecPluginFactory());
    QueryPlugin::registerClass(f);
}

////////////////////////////////////////////////////////////////////////
// SpatialSpecPlugin implementation
////////////////////////////////////////////////////////////////////////
void 
SpatialSpecPlugin::applyLogical(SelectStmt& stmt, QueryContext& context) {
    // Idea: For each of the spatial specs in the WHERE clause,
    // rewrite in the context of whatever chunked tables exist in the
    // FROM list.

    TableRefChecker trc; // Lookup oracle for table characteristics
                         // (should share with others)
 
    // First, get a list of the chunked tables.
    FromList& fList = stmt.getFromList();    
    TableRefnList& tList = fList.getTableRefnList();
    SpatialEntries entries;
    getTable gt(trc, entries);
    std::for_each(tList.begin(), tList.end(), gt);
    
    // Prepare to patch the WHERE clause
    WhereClause& wc = stmt.getWhereClause();

    boost::shared_ptr<QsRestrictor::List const> rListP = wc.getRestrs();
    if(!rListP.get()) return; // No spatial restrictions -> nothing to do

    QsRestrictor::List const& rList = *rListP;
    AndTerm::Ptr newTerm(new AndTerm);

    // Now, for each of the spatial restrictors:
    for(QsRestrictor::List::const_iterator i=rList.begin();
        i != rList.end(); ++i) {
        // for each spatial entry
        // generate a restrictor condition.
        for(SpatialEntries::const_iterator j = entries.begin();
            j != entries.end(); ++j) {
            newTerm->_terms.push_back(_makeCondition(*i, *j));
        }
    }
    wc.resetRestrs();
    wc.prependAndTerm(newTerm);
}

void
SpatialSpecPlugin::applyPhysical(QueryPlugin::Plan& p, QueryContext& context) {
    // Probably nothing is needed here...
}

BoolTerm::Ptr 
SpatialSpecPlugin::_makeCondition(boost::shared_ptr<QsRestrictor> const restr,
                                  SpatialEntry const& spatialEntry) {
    Restriction r(*restr);
    return r.generate(spatialEntry);
}

}}} // namespace lsst::qserv::master
