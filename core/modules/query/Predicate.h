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
#ifndef LSST_QSERV_QUERY_PREDICATE_H
#define LSST_QSERV_QUERY_PREDICATE_H
/**
  * @file
  *
  * @brief Predicate is a representation of a boolean term in a WHERE clause
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers
#include <list>
#include <string>

// Third party headers
#include <boost/shared_ptr.hpp>

// Local headers
#include "query/BoolTerm.h"

namespace lsst {
namespace qserv {
namespace query {

// Forward
class QueryTemplate;
class ValueExpr;

typedef boost::shared_ptr<ValueExpr> ValueExprPtr;
typedef std::list<ValueExprPtr> ValueExprList;

///  Predicate is a representation of a SQL predicate.
/// predicate :
///       row_value_constructor
///         ( comp_predicate
///         | ("not")? ( between_predicate
///                    | in_predicate
///                    | like_predicate
///                    )
///         | null_predicate
///         | quantified_comp_predicate
///         | match_predicate
///         | overlaps_predicate
///         ) {#predicate = #([PREDICATE, "PREDICATE"],predicate);}
///     | exists_predicate
///     | unique_predicate
class Predicate : public BfTerm {
public:
    typedef boost::shared_ptr<Predicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~Predicate() {}
    virtual char const* getName() const { return "Predicate"; }

    friend std::ostream& operator<<(std::ostream& os, Predicate const& bt);
    virtual std::ostream& putStream(std::ostream& os) const = 0;
    virtual void renderTo(QueryTemplate& qt) const = 0;

    virtual BfTerm::Ptr copySyntax() const { return BfTerm::Ptr(); }
};

/// GenericPredicate is a Predicate whose structure whose semantic meaning
/// is unimportant for qserv
class GenericPredicate : public Predicate {
public:
    typedef boost::shared_ptr<GenericPredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~GenericPredicate() {}
    virtual char const* getName() const { return "GenericPredicate"; }

    virtual std::ostream& putStream(std::ostream& os) = 0;
    virtual void renderTo(QueryTemplate& qt) const = 0;
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone(); }
};

/// CompPredicate is a Predicate involving a row value compared to another row value.
/// (literals can be row values)
class CompPredicate : public Predicate {
public:
    typedef boost::shared_ptr<CompPredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~CompPredicate() {}
    virtual char const* getName() const { return "CompPredicate"; }

    virtual void findValueExprs(ValueExprList& list);
    virtual void findColumnRefs(ColumnRef::List& list);

    virtual std::ostream& putStream(std::ostream& os) const;
    virtual void renderTo(QueryTemplate& qt) const;
    /// Deep copy this term.
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone(); }

    static int reverseOp(int op); // Reverses operator token
    static char const* lookupOp(int op);
    static int lookupOp(char const* op);

    ValueExprPtr left;
    int op; // Parser token type of operator
    ValueExprPtr right;
};

/// InPredicate is a Predicate comparing a row value to a set
class InPredicate : public Predicate {
public:
    typedef boost::shared_ptr<InPredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~InPredicate() {}
    virtual char const* getName() const { return "InPredicate"; }

    virtual void findValueExprs(ValueExprList& list);
    virtual void findColumnRefs(ColumnRef::List& list);

    virtual std::ostream& putStream(std::ostream& os) const;
    virtual void renderTo(QueryTemplate& qt) const;
    /// Deep copy this term.
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone();}

    ValueExprPtr value;
    ValueExprList cands;
};

/// BetweenPredicate is a Predicate comparing a row value to a range
class BetweenPredicate : public Predicate {
public:
    typedef boost::shared_ptr<BetweenPredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~BetweenPredicate() {}
    virtual char const* getName() const { return "BetweenPredicate"; }

    virtual void findValueExprs(ValueExprList& list);
    virtual void findColumnRefs(ColumnRef::List& list);
    virtual std::ostream& putStream(std::ostream& os) const;
    virtual void renderTo(QueryTemplate& qt) const;
    /// Deep copy this term.
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone(); }

    ValueExprPtr value;
    ValueExprPtr minValue;
    ValueExprPtr maxValue;
};

/// LikePredicate is a Predicate involving a row value compared to a pattern
/// (pattern is a char-valued value expression
class LikePredicate : public Predicate {
public:
    typedef boost::shared_ptr<LikePredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~LikePredicate() {}
    virtual char const* getName() const { return "LikePredicate"; }

    virtual void findValueExprs(ValueExprList& list);
    virtual void findColumnRefs(ColumnRef::List& list);

    virtual std::ostream& putStream(std::ostream& os) const;
    virtual void renderTo(QueryTemplate& qt) const;
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone(); }

    ValueExprPtr value;
    ValueExprPtr charValue;
};

/// NullPredicate is a Predicate involving a row value compared to NULL
class NullPredicate : public Predicate {
public:
    typedef boost::shared_ptr<NullPredicate> Ptr;
    typedef std::list<Ptr> PtrList;

    virtual ~NullPredicate() {}
    virtual char const* getName() const { return "NullPredicate"; }

    virtual void findValueExprs(ValueExprList& list);
    virtual void findColumnRefs(ColumnRef::List& list);

    virtual std::ostream& putStream(std::ostream& os) const;
    virtual void renderTo(QueryTemplate& qt) const;
    virtual BfTerm::Ptr clone() const;
    virtual BfTerm::Ptr copySyntax() const { return clone(); }

    static int reverseOp(int op); // Reverses operator token

    ValueExprPtr value;
    bool hasNot;
};

}}} // namespace lsst::qserv::query

#endif // LSST_QSERV_QUERY_PREDICATE_H
