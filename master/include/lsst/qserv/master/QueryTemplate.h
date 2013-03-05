// -*- LSST-C++ -*-
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
// QueryTemplate

// A query template is render-able query. This replaces the
// string-based Substitution class that was used to perform fast chunk
// substitutions in generating queries. 
//
// The Substition/SqlSubstitution model employed a single template
// string along with an index to the string regions that were
// substitutable. Callers provided a mapping (i.e., {Object ->
// Object_2031, Object_s2 -> Object_2031_232} ) that was used to
// perform the subsitution.
// 
// that can accept query components as an intermediate stage to
// generated queries. The copy-to-output-iterator paradigm was a very
// useful paradigm for outputting components, but it is clear that a
// vanilla c++ stream iterator is not enough--there is a benefit to
// using the paradigm for: a debugging (or machine-readable)
// output, a human readable output, and a generated query.
#ifndef LSST_QSERV_MASTER_QUERYTEMPLATE_H
#define LSST_QSERV_MASTER_QUERYTEMPLATE_H
#include <string>
#include <list>
#include <boost/shared_ptr.hpp>

namespace lsst { namespace qserv { namespace master {
// Forward
class ColumnRef;
class TableRef;

class QueryTemplate {
public:
    class Entry {
    public:
        virtual ~Entry() {}
        virtual std::string getValue() const = 0;
        virtual bool getIsDynamic() const { return false; }
    };

    QueryTemplate() {}

    void append(std::string const& s);
    void append(ColumnRef const& cr);
    void append(TableRef const& tr);

    std::string dbgStr() const;
private:
    void _optimize() const; // not really const.
    // Later, make this a list of the templatable.
    std::list<std::string> _elements; 
    std::list<boost::shared_ptr<Entry> > _entries;
};
}}} // lsst::qserv::master
#endif // LSST_QSERV_MASTER_QUERYTEMPLATE_H
