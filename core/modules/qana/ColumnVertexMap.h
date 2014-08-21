// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014 LSST Corporation.
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

#ifndef LSST_QSERV_QANA_COLUMNVERTEXMAP_H
#define LSST_QSERV_QANA_COLUMNVERTEXMAP_H

/// \file
/// \brief A class for resolving column references to table references

// System headers
#include <algorithm>
#include <string>
#include <vector>

// Third-party headers
#include <boost/shared_ptr.hpp>

// Local headers
#include "query/ColumnRef.h"


namespace lsst {
namespace qserv {
namespace qana {

typedef boost::shared_ptr<query::ColumnRef const> ColumnRefConstPtr;
class Vertex;

/// A `ColumnVertexMap` is a mapping from column references to the relation
/// graph vertices for the table references that column values can originate
/// from. Most of the time values come from a single table, but there are two
/// exceptions. Firstly, a column reference can be ambiguous, in which case its
/// presence in the query text must be treated as an error. Secondly, natural
/// join columns map to two or more table references. This is because the
/// result of `A NATURAL JOIN B` contains a single column `c` for every common
/// column `c` of `A` and `B`. Its value is `COALESCE(A.c, B.c)`, defined as
/// `CASE WHEN A.c IS NOT NULL THEN A.c ELSE B.c END`.
class ColumnVertexMap {
public:
    struct Entry {
        ColumnRefConstPtr cr;
        std::vector<Vertex*> vertices; // unowned

        Entry() {}
        Entry(ColumnRefConstPtr const& c, Vertex* t) :
            cr(c), vertices(1, t) {}
        void swap(Entry& e) {
            cr.swap(e.cr);
            vertices.swap(e.vertices);
        }
    };

    ColumnVertexMap() {}

    /// This constructor creates a mapping containing all column references
    /// for a single vertex.
    explicit ColumnVertexMap(Vertex& v);
    /// This constructor creates a mapping containing the given column references
    /// for the given vertex.
    template <typename InputIterator>
    ColumnVertexMap(Vertex& v, InputIterator first, InputIterator last) {
        _init(v, first, last);
    }

    void swap(ColumnVertexMap& m) {
        _entries.swap(m._entries);
    }

    /// `find` returns the vertices for table references corresponding to the
    /// given column reference. An empty vector is returned for unrecognized
    /// columns. If `c` is ambiguous, an exception is thrown.
    std::vector<Vertex*> const& find(query::ColumnRef const& c) const;

    /// `splice` transfers the entries of `m` to this map, emptying `m`.
    /// If `m` contains a column reference `c` that is already in this map,
    /// then `c` is marked ambiguous unless `c` is an unqualified reference,
    /// in which case behavior depends on the `natural` flag argument and
    /// whether or not `c` is a member of `cols` (a vector of column names):
    ///
    /// - If `natural` is false and `c` is not a member of `cols`,
    ///   then `c` is marked as ambiguous.
    /// - Otherwise, table references for `c` from both maps are concatenated
    ///   unless `c` is already ambiguous in either map, in which case an
    ///   exception is thrown.
    void splice(ColumnVertexMap& m,
                bool natural,
                std::vector<std::string> const& cols);

    /// `computeCommonColumns` returns all unqualified column names that are
    /// common to this map and `m`. If any such column is ambiguous in either
    /// map, an exception is thrown.
    std::vector<std::string> const computeCommonColumns(
        ColumnVertexMap const& m) const;

private:
    std::vector<Entry> _entries; // sorted

    // Not implemented
    ColumnVertexMap(ColumnVertexMap const&);
    ColumnVertexMap& operator=(ColumnVertexMap const&);

    template <typename InputIterator>
    inline void _init(Vertex& v, InputIterator first, InputIterator last);
};


/// `ColumnRefLt` is a less-than comparison functor for column references
/// and ColumnVertexMap::Entry objects.
struct ColumnRefLt {
    bool operator()(query::ColumnRef const& a,
                    query::ColumnRef const& b) const {
        int c = a.column.compare(b.column);
        if (c == 0) {
            c = a.table.compare(b.table);
            if (c == 0) {
                c = a.db.compare(b.db);
            }
        }
        return c < 0;
    }
    bool operator()(ColumnVertexMap::Entry const& a,
                    ColumnVertexMap::Entry const& b) const {
        return (*this)(*a.cr, *b.cr);
    }
    bool operator()(query::ColumnRef const& a,
                    ColumnVertexMap::Entry const& b) const {
        return (*this)(a, *b.cr);
    }
    bool operator()(ColumnVertexMap::Entry const& a,
                    query::ColumnRef const& b) const {
        return (*this)(*a.cr, b);
    }
    bool operator()(ColumnRefConstPtr const& a,
                    ColumnRefConstPtr const& b) const {
        return (*this)(*a, *b);
    }
};


/// `ColumnRefEq` is an equality comparison functor that is compatible with
/// ColumnRefLt, meaning that two objects are equal iff neither is less than
/// the other according to ColumnRefLt.
struct ColumnRefEq {
    bool operator()(query::ColumnRef const& a,
                    query::ColumnRef const& b) const {
        return a.column == b.column && a.table == b.table && a.db == b.db;
    }
    bool operator()(ColumnVertexMap::Entry const& a,
                    ColumnVertexMap::Entry const& b) const {
        return (*this)(*a.cr, *b.cr);
    }
    bool operator()(query::ColumnRef const& a,
                    ColumnVertexMap::Entry const& b) const {
        return (*this)(a, *b.cr);
    }
    bool operator()(ColumnVertexMap::Entry const& a,
                    query::ColumnRef const& b) const {
        return (*this)(*a.cr, b);
    }
    bool operator()(ColumnRefConstPtr const& a,
                    ColumnRefConstPtr const& b) const {
        return (*this)(*a, *b);
    }
};

inline void swap(ColumnVertexMap::Entry& a,
                 ColumnVertexMap::Entry& b) {
    a.swap(b);
}

template <typename InputIterator>
inline void ColumnVertexMap::_init(Vertex& v,
                                   InputIterator first,
                                   InputIterator last)
{
    for (; first != last; ++first) {
        _entries.push_back(Entry(*first, &v));
    }
    std::sort(_entries.begin(), _entries.end(), ColumnRefLt());
}

}}} // namespace lsst::qserv::qana

#endif // LSST_QSERV_QANA_COLUMNVERTEXMAP_H
