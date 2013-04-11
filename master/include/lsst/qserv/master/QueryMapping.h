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
// QueryMapping is a value class that stores a mapping that can be
// consulted for a partitioning-strategy-agnostic query generation
// stage that substitutes real table names for placeholders, according
// to a query's specified partition coverage.

#ifndef LSST_QSERV_MASTER_QUERYMAPPING_H
#define LSST_QSERV_MASTER_QUERYMAPPING_H
#include <boost/shared_ptr.hpp>
#include <map>

namespace lsst { namespace qserv { namespace master {
class ChunkSpec;
class QueryTemplate;

class QueryMapping {
public:
    typedef boost::shared_ptr<QueryMapping> Ptr;
    enum Parameter {INVALID, CHUNK=100, SUBCHUNK, HTM1=200};
    typedef std::map<std::string,Parameter> ParameterMap;

    explicit QueryMapping();

    std::string apply(ChunkSpec const& s, QueryTemplate const& t);

    void addEntry(std::string const& s, Parameter p) { _subs[s] = p; }
    void update(QueryMapping const& qm);
    bool hasChunks() const { return hasParameter(CHUNK); }
    bool hasSubChunks() const { return hasParameter(SUBCHUNK); }
    bool hasParameter(Parameter p) const;

private:
    ParameterMap _subs;
};
}}} // namespace lsst::qserv::master

#endif // LSST_QSERV_MASTER_QUERYMAPPING_H

