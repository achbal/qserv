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
// Implementation of helper printers for ChunkSpec
#include "lsst/qserv/master/ChunkSpec.h"
#include <iterator>

namespace qMaster=lsst::qserv::master;

namespace { // File-scope helpers
}
namespace lsst { namespace qserv { namespace master {
std::ostream& operator<<(std::ostream& os, ChunkSpec const& c) {
    os << "ChunkSpec[" 
       << "chunkId=" << c.chunkId
       << " subChunks:";
    std::copy(c.subChunks.begin(), c.subChunks.end(), 
              std::ostream_iterator<int>(os, ","));
    os << "]";
    return os;
}
}}} // namespace lsst::qserv::master

