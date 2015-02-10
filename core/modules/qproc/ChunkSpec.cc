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

/**
  * @file
  *
  * @brief Implementation of helpers for ChunkSpec
  *
  * @author Daniel L. Wang, SLAC
  */

#include "qproc/ChunkSpec.h"

// System headers
#include <algorithm>
#include <cassert>
#include <iterator>
#include <stdexcept>

// Qserv headers
#include "global/Bug.h"

namespace { // File-scope helpers
/// A "good" number of subchunks to include in a chunk query.  This is
/// a guess. The best value is an open question
int const GOOD_SUBCHUNK_COUNT = 20;
} // annonymous namespace

namespace lsst {
namespace qserv {
namespace qproc {

std::ostream& operator<<(std::ostream& os, ChunkSpec const& c) {
    os << "ChunkSpec["
       << "chunkId=" << c.chunkId
       << " subChunks:";
    std::copy(c.subChunks.begin(), c.subChunks.end(),
              std::ostream_iterator<int>(os, ","));
    os << "]";
    return os;
}


/// ChunkSpecVector intersection.
/// Computes ChunkSpecVector intersection, overwriting dest with the result.
///
/// precondition: Elements in ChunkSpecVector should be minimized in the sense
/// that there should be only one element for a particular chunkId. i.e., for
/// all ChunkSpec A element, there is no ChunkSpec B element in the same vector
/// where A.chunkId == B.chunkId.
void intersectSorted(ChunkSpecVector& dest, ChunkSpecVector const& a) {
    ChunkSpecVector tmp;
    tmp.swap(dest);
    ChunkSpecVector::iterator di = tmp.begin();
    ChunkSpecVector::iterator de = tmp.end();
    ChunkSpecVector::const_iterator ai = a.begin();
    ChunkSpecVector::const_iterator ae = a.end();
    for(; (di != de) && (ai != ae); ++di) { // march down dest vector
        // For each item in dest, advance through a to find a matching chunkId.
        while(ai->chunkId < di->chunkId) {
            ++ai;
        }

        if(ai->chunkId == di->chunkId) {
            ChunkSpec cs = *di;
            // On a match, perform the intersection.
            cs.restrict(*ai);
            if(di->chunkId != ChunkSpec::CHUNKID_INVALID) {
                dest.push_back(cs);
            }
        }
    }
}


ChunkSpecVector intersect(ChunkSpecVector const& a, ChunkSpecVector const& b) {
    ChunkSpecVector asort, bsort;
    asort = a;
    std::sort(asort.begin(), asort.end());
    bsort = b;
    std::sort(bsort.begin(), bsort.end());
    intersectSorted(asort, bsort);
    return asort;
}

////////////////////////////////////////////////////////////////////////
// ChunkSpec
////////////////////////////////////////////////////////////////////////
bool ChunkSpec::shouldSplit() const {
    return subChunks.size() > (unsigned)GOOD_SUBCHUNK_COUNT;
}

ChunkSpec ChunkSpec::intersect(ChunkSpec const& cs) const {
    ChunkSpec output(*this);
    output.restrict(cs);
    return output;
}

void ChunkSpec::restrict(ChunkSpec const& rhs) {
    if(chunkId != rhs.chunkId) {
        throw Bug("ChunkSpec::merge with different chunkId");
    }
    std::sort(subChunks.begin(), subChunks.end());
    Int32Vector rhsSort, output;
    rhsSort.assign(rhs.subChunks.begin(), rhs.subChunks.end());
    std::sort(rhsSort.begin(), rhsSort.end());
    std::set_intersection(
        subChunks.begin(), subChunks.end(),
        rhsSort.begin(), rhsSort.end(),
        std::insert_iterator<Int32Vector>(output, output.end()));
    subChunks.swap(output);
}

bool ChunkSpec::operator<(ChunkSpec const& rhs) const {
    typedef std::vector<int32_t>::const_iterator VecIter;

    if(chunkId < rhs.chunkId) return true;
    else if(chunkId > rhs.chunkId) return false;
    else {
        std::pair<VecIter,VecIter> mism
            = std::mismatch(subChunks.begin(), subChunks.end(),
                            rhs.subChunks.begin());
        if(mism.first == subChunks.end()) {
            return mism.second != rhs.subChunks.end();
        } else if(mism.second == rhs.subChunks.end()) {
            return false;
        } else { // both are valid;
            return ((*mism.first) < (*mism.second));
        }
    }
}

bool ChunkSpec::operator==(ChunkSpec const& rhs) const {
    typedef std::vector<int32_t>::const_iterator VecIter;

    if(chunkId != rhs.chunkId) return false;
    return subChunks == rhs.subChunks;
}

ChunkSpec ChunkSpec::makeFake(int chunkId, bool withSubChunks) {
    ChunkSpec cs;
    cs.chunkId = chunkId;
    assert(chunkId < 1000000);
    if(withSubChunks) {
        int base = 1000 * chunkId;
        cs.subChunks.push_back(base);
        cs.subChunks.push_back(base+10);
        cs.subChunks.push_back(base+20);
    }
    return cs;
}

////////////////////////////////////////////////////////////////////////
// ChunkSpecFragmenter
////////////////////////////////////////////////////////////////////////
ChunkSpecFragmenter::ChunkSpecFragmenter(ChunkSpec const& s)
    : _original(s), _pos(_original.subChunks.begin()) {
}
ChunkSpec ChunkSpecFragmenter::get() const {
    ChunkSpec c;
    c.chunkId = _original.chunkId;
    Iter posEnd = _pos + GOOD_SUBCHUNK_COUNT;
    Iter end = _original.subChunks.end();
    if(posEnd >= end) {
        posEnd = end;
    }
    c.subChunks.resize(posEnd - _pos);
    std::copy(_pos, posEnd, c.subChunks.begin());
    return c;
}

void ChunkSpecFragmenter::next() {
    _pos += GOOD_SUBCHUNK_COUNT;
}

bool ChunkSpecFragmenter::isDone() {
    return _pos >= _original.subChunks.end();
}
////////////////////////////////////////////////////////////////////////
// ChunkSpecSingle
////////////////////////////////////////////////////////////////////////
// precondition: !spec.subChunks.empty()
ChunkSpecSingle::List ChunkSpecSingle::makeList(ChunkSpec const& spec) {
    List list;
    if(spec.subChunks.empty()) {
        throw Bug("Attempted subchunk spec list without subchunks.");
    }
    ChunkSpecSingle s;
    s.chunkId = spec.chunkId;
    std::vector<int>::const_iterator i;
    for(i = spec.subChunks.begin();
        i != spec.subChunks.end(); ++i) {
        s.subChunkId = *i;
        list.push_back(s);
    }
    return list;
}

std::ostream& operator<<(std::ostream& os, ChunkSpecSingle const& c) {
    os << "(" << c.chunkId << "," << c.subChunkId << ")";
    return os;
}

}}} // namespace lsst::qserv::qproc

