/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
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

#ifndef LSST_QSERV_MASTER_TRANSACTION_H
#define LSST_QSERV_MASTER_TRANSACTION_H
#include <string>
#include <vector>
#include <list>
#include <boost/shared_ptr.hpp>

namespace lsst {
namespace qserv {
namespace master {

/// class TransactionSpec - A value class for the minimum
/// specification of a subquery, as far as the xrootd layer is
/// concerned.
class TransactionSpec {
public:
 TransactionSpec() : chunkId(-1) {}
    int chunkId;
    std::string path;
    std::string query;
    int bufferSize;
    std::string savePath;
    
    bool isNull() const { return path.length() == 0; }
    
    class Reader;  // defined in thread.h
};

class Constraint {
public:
    std::string name;
    std::vector<std::string> params;
    std::string paramsGet(int i) const {
        return params[i];
    }
    int paramsSize() const {
        return params.size();
    }
};
typedef std::vector<Constraint> ConstraintVector;
class ConstraintVec { // Wrapper for SWIG.
public:
    ConstraintVec(boost::shared_ptr<ConstraintVector > v)
        : _vec(v) {}
    Constraint const& get(int i) const {
        return (*_vec)[i];
    }
    int size() const {
        return _vec->size();
    }
private:
    boost::shared_ptr<ConstraintVector> _vec;
};

class ChunkSpec {
public:
    int chunkId;
    std::vector<int> subChunks;
};
typedef std::list<ChunkSpec> ChunkSpecList;
typedef std::vector<ChunkSpec> ChunkSpecVector;

}}} // namespace lsst::qserv::master

#endif // LSST_QSERV_MASTER_TRANSACTION_H
