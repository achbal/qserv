// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 AURA/LSST.
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
#ifndef LSST_QSERV_QPROC_INDEXMAP_H
#define LSST_QSERV_QPROC_INDEXMAP_H
/**
  * @file
  *
  * @brief IndexMap to look up chunk numbers
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers
// Temporary
//#include "qproc/fakeGeometry.h"

// Qserv headers
#include "css/StripingParams.h"
#include "query/Constraint.h"
#include "qproc/ChunkSpec.h"

namespace lsst {
namespace qserv {
namespace qproc {

class PartitioningMap;
class SecondaryIndex;

//boost::shared_ptr<Region> getRegion(query::Constraint const& c);

class IndexMap {
public:
    IndexMap(boost::shared_ptr<PartitioningMap> pm,
             boost::shared_ptr<SecondaryIndex> si)
        : _pm(pm), _si(si) {}
    IndexMap(css::StripingParams const& sp,
             boost::shared_ptr<SecondaryIndex> si)
        : _si(si) {
        throw "FIXME, need to construct pm.!";

    }
    ChunkSpecVector getIntersect(query::ConstraintVector const& cv);

private:
    boost::shared_ptr<PartitioningMap> _pm;
    boost::shared_ptr<SecondaryIndex> _si;
};

}}} // namespace lsst::qserv::qproc
#endif // LSST_QSERV_QPROC_INDEXMAP_H
