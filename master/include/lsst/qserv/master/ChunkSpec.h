// -*- LSST-C++ -*-
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
// ChunkSpec is a type that bundles the per-chunk information that is used to
// compose a concrete chunk query for a specific chunk from an input parsed
// query statement. 

#ifndef LSST_QSERV_MASTER_CHUNKSPEC_H
#define LSST_QSERV_MASTER_CHUNKSPEC_H
#include <iostream>
#include <list>
#include <vector>

namespace lsst { namespace qserv { namespace master {

class ChunkSpec {
public:
    int chunkId;
    std::vector<int> subChunks;
    void addSubChunk(int s) { subChunks.push_back(s); }
};
std::ostream& operator<<(std::ostream& os, ChunkSpec const& c);

typedef std::list<ChunkSpec> ChunkSpecList;
typedef std::vector<ChunkSpec> ChunkSpecVector;

}}} // namespace lsst::qserv::master


#endif // LSST_QSERV_MASTER_CHUNKSPEC_H

