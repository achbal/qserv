// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2013-2014 LSST Corporation.
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
#ifndef LSST_QSERV_MASTER_QUERYCONTEXT_H
#define LSST_QSERV_MASTER_QUERYCONTEXT_H
/**
  * @file
  *
  * @author Daniel L. Wang, SLAC
  */
#include <list>
#include <string>
#include <boost/shared_ptr.hpp>

#include "css/Facade.h"
#include "qana/QueryMapping.h"
#include "query/TableAlias.h"
#include "util/common.h"

namespace lsst {
namespace qserv {
namespace master {

class ColumnRef;
class QsRestrictor;

/// QueryContext is a value container for query state related to analyzing,
/// rewriting, and generating queries. It is the primary mechanism for
/// QueryPlugin instances to share information. It contains the user context of
/// a query, but not the query itself.
///
/// TODO: Think about QueryMapping's home. It is used during query manipulation,
/// contains information derived during analysis, and is used to generate
/// materialized query text.
class QueryContext {
public:
    typedef boost::shared_ptr<QueryContext> Ptr;

    QueryContext() {}
    typedef std::list<boost::shared_ptr<QsRestrictor> > RestrList;

    boost::shared_ptr<css::Facade> cssFacade; ///< Unowned, assumed to be alive 
                                              ///  for this lifetime.
    std::string defaultDb; ///< Implicit db context
    std::string dominantDb; ///< "dominant" database for this query
    std::string anonymousTable; ///< Implicit table context
    std::string username; ///< unused, but reserved.

    StringPairList scanTables; // Tables scanned (for shared scans)

    // Table aliasing
    TableAlias tableAliases;
    TableAliasReverse tableAliasReverses;

    // Owned QueryMapping and query restrictors
    boost::shared_ptr<QueryMapping> queryMapping;
    boost::shared_ptr<RestrList> restrictors;

    int chunkCount; //< -1: all, 0: none, N: #chunks

    bool needsMerge; ///< Does this query require a merge/post-processing step?

    lsst::qserv::css::StripingParams getDbStriping() {
        return cssFacade->getDbStriping(dominantDb); }
    bool hasChunks() const {
        return queryMapping.get() && queryMapping->hasChunks(); }
    bool hasSubChunks() const {
        return queryMapping.get() && queryMapping->hasSubChunks(); }
    DbTablePair resolve(boost::shared_ptr<ColumnRef> cr);
};

}}} // namespace lsst::qserv::master
#endif // LSST_QSERV_MASTER_QUERYCONTEXT_H
