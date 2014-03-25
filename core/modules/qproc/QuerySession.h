// -*- LSST-C++ -*-
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
#ifndef LSST_QSERV_MASTER_QUERYSESSION_H
#define LSST_QSERV_MASTER_QUERYSESSION_H
/**
  * @file
  *
  * @author Daniel L. Wang, SLAC
  */
#include <list>
#include <string>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/shared_ptr.hpp>

#include "css/Facade.h"
#include "query/Constraint.h"
#include "qproc/ChunkQuerySpec.h"
#include "qproc/ChunkSpec.h"

#include "qana/QueryPlugin.h"
#include "merger/mergeTypes.h"

namespace lsst {
namespace qserv {
namespace master {
class SelectStmt; // forward
class QueryPlugin; // forward

///  QuerySession contains state and behavior for operating on user queries. It
///  contains much of the query analysis-side of AsyncQueryManager's
///  responsibility, including the text of the original query, a parsed query
///  tree, and other user state/context.
class QuerySession {
public:
    class Iter;
    friend class Iter;
    friend class AsyncQueryManager; // factory for QuerySession.

    explicit QuerySession(boost::shared_ptr<css::Facade>);

    std::string const& getOriginal() const { return _original; }
    void setQuery(std::string const& q);
    bool hasAggregate() const;

    boost::shared_ptr<ConstraintVector> getConstraints() const;
    void addChunk(ChunkSpec const& cs);

    SelectStmt const& getStmt() const { return *_stmt; }

    // Resulttable concept will be obsolete after we implement query result
    // marshalling/transfer (at which point, table dump and restore will also be
    // obsolete).
    void setResultTable(std::string const& resultTable);
    std::string const& getResultTable() const { return _resultTable; }

    /// Dominant database is the database that will be used for query
    /// dispatch. This is distinct from the default database, which is what is
    /// used for unqualified table and column references
    std::string const& getDominantDb() const;
    css::IntPair getDbStriping();
    std::string const& getError() const { return _error; }

    MergeFixup makeMergeFixup() const;

    /// Finalize a query after chunk coverage has been updated
    void finalize();
    // Iteration
    Iter cQueryBegin();
    Iter cQueryEnd();

    // For test harnesses.
    boost::shared_ptr<QueryContext> dbgGetContext() { return _context; }

private:
    typedef std::list<QueryPlugin::Ptr> PluginList;

    // Pipeline helpers
    void _initContext();
    void _preparePlugins();
    void _applyLogicPlugins();
    void _generateConcrete();
    void _applyConcretePlugins();
    void _showFinal(); // Debug

    // Iterator help
    std::vector<std::string> _buildChunkQueries(ChunkSpec const& s);

    // Fields
    boost::shared_ptr<css::Facade> _cssFacade;
    std::string _original;
    boost::shared_ptr<QueryContext> _context;
    boost::shared_ptr<SelectStmt> _stmt;
    /// Group of parallel statements (not a sequence)
    std::list<boost::shared_ptr<SelectStmt> > _stmtParallel;
    boost::shared_ptr<SelectStmt> _stmtMerge;
    bool _hasMerge;
    std::string _tmpTable;
    std::string _resultTable;
    std::string _error;
    int _isFinal;

    ChunkSpecList _chunks;
    boost::shared_ptr<PluginList> _plugins;
};

/// Iterates over a ChunkSpecList to return ChunkQuerySpecs for execution
class QuerySession::Iter : public boost::iterator_facade <
    QuerySession::Iter, ChunkQuerySpec, boost::forward_traversal_tag> {
public:
    Iter() : _qs(NULL) {}

private:
    Iter(QuerySession& qs, ChunkSpecList::iterator i);
    friend class QuerySession;
    friend class boost::iterator_core_access;

    void increment() { ++_pos; _dirty = true; }

    bool equal(Iter const& other) const {
        return (this->_qs == other._qs) && (this->_pos == other._pos);
    }

    ChunkQuerySpec& dereference() const;

    void _buildCache() const;
    void _updateCache() const {
        if(_dirty) {
            _buildCache();
            _dirty = false;
        }
    }
    boost::shared_ptr<ChunkQuerySpec> _buildFragment(ChunkSpecFragmenter& f) const;

    QuerySession* _qs;
    ChunkSpecList::const_iterator _pos;
    bool _hasChunks;
    bool _hasSubChunks;
    mutable ChunkQuerySpec _cache;
    mutable bool _dirty;
};

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_QUERYSESSION_H
