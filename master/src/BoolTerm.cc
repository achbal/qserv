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
/**
  * @file BoolTerm.cc
  *
  * @brief BoolTerm and BoolTermFactory implementations.
  *
  * @author Daniel L. Wang, SLAC
  */
#include "lsst/qserv/master/BoolTerm.h"
#include <stdexcept>
#include "lsst/qserv/master/QueryTemplate.h"
#include "lsst/qserv/master/ValueExpr.h"

namespace lsst {
namespace qserv {
namespace master {
////////////////////////////////////////////////////////////////////////
// BoolTerm section
////////////////////////////////////////////////////////////////////////
std::ostream& operator<<(std::ostream& os, BoolTerm const& bt) {
    return bt.putStream(os);
}

std::ostream& OrTerm::putStream(std::ostream& os) const {
    // FIXME
    return os;
}
std::ostream& AndTerm::putStream(std::ostream& os) const {
    // FIXME
    return os;
}
std::ostream& BoolFactor::putStream(std::ostream& os) const {
    // FIXME
    return os;
}
std::ostream& UnknownTerm::putStream(std::ostream& os) const {
    return os << "--UNKNOWNTERM--";
}
std::ostream& PassTerm::putStream(std::ostream& os) const {
    return os << _text;
}
std::ostream& PassListTerm::putStream(std::ostream& os) const {
    // FIXME
    return os;
}
std::ostream& BoolTermFactor::putStream(std::ostream& os) const {
    if(_term) { return _term->putStream(os); }
    return os;
}
namespace {
template <typename Plist>
inline void renderList(QueryTemplate& qt,
                       Plist const& lst,
                       std::string const& sep) {
    int count=0;
    typename Plist::const_iterator i;
    for(i = lst.begin(); i != lst.end(); ++i) {
        if(!sep.empty() && ++count > 1) { qt.append(sep); }
        if(!*i) { throw std::logic_error("Bad list term"); }
        (**i).renderTo(qt);
    }
}
}
void OrTerm::renderTo(QueryTemplate& qt) const {
    renderList(qt, _terms, "OR");
}
void AndTerm::renderTo(QueryTemplate& qt) const {
    renderList(qt, _terms, "AND");
}
void BoolFactor::renderTo(QueryTemplate& qt) const {
    std::string s;
    renderList(qt, _terms, s);
}
void UnknownTerm::renderTo(QueryTemplate& qt) const {
    qt.append("unknown");
}
void PassTerm::renderTo(QueryTemplate& qt) const {
    qt.append(_text);
}
void PassListTerm::renderTo(QueryTemplate& qt) const {
    qt.append("(");
    StringList::const_iterator i;
    bool isFirst=true;
    for(i=_terms.begin(); i != _terms.end(); ++i) {
        if(!isFirst) {
            qt.append(",");
        }
        qt.append(*i);
        isFirst = false;
    }
    qt.append(")");
}
void BoolTermFactor::renderTo(QueryTemplate& qt) const {
    if(_term) { _term->renderTo(qt); }
}
void BoolFactor::findColumnRefs(ColumnRefMap::List& list) {
    BfTerm::PtrList::const_iterator i;
    for(i = _terms.begin(); i != _terms.end(); ++i) {
        (**i).findColumnRefs(list);
    }
}

void BoolTermFactor::findColumnRefs(ColumnRefMap::List& cList) {
    struct btFind : public BfTerm::Op {
        btFind(ColumnRefMap::List& cList_) : cList(cList_) {}
        virtual void operator()(BfTerm& t) { t.findColumnRefs(cList); }
        ColumnRefMap::List& cList;
    };
    if(_term) {
        btFind find(cList);
        _term->visitBfTerm(find);
    }
}
boost::shared_ptr<BoolTerm> OrTerm::getReduced() {
    if(_terms.size() == 1) {
        boost::shared_ptr<BoolTerm> reduced = _terms.front();
        if(reduced) { return reduced; }
        else { return _terms.front(); }
    }
    return boost::shared_ptr<BoolTerm>();
}

boost::shared_ptr<BoolTerm> AndTerm::getReduced() {
    // Get reduced versions of my children.
    // Can I eliminate myself?
    if(_terms.size() == 1) {
        boost::shared_ptr<BoolTerm> reduced = _terms.front();
        if(reduced) { return reduced; }
        else { return _terms.front(); }
    }
    return boost::shared_ptr<BoolTerm>();
}

bool BoolFactor::_reduceTerms(BfTerm::PtrList& newTerms,
                              BfTerm::PtrList& oldTerms) {
    typedef BfTerm::PtrList::iterator Iter;
    bool hasReduction = false;
    for(Iter i=oldTerms.begin(), e=oldTerms.end(); i != e; ++i) {
        BfTerm& term = **i;
        QueryTemplate qt;
        term.renderTo(qt);
        //std::cout << "reduce? " << qt.generate() << std::endl;
        BoolTermFactor* btf = dynamic_cast<BoolTermFactor*>(&term);

        if(btf) {
            if(btf->_term) {
                boost::shared_ptr<BoolTerm> reduced = btf->_term->getReduced();
                if(reduced) {
                    BoolFactor* f =  dynamic_cast<BoolFactor*>(reduced.get());
                    if(f) { // factor in a term in a factor --> factor
                        newTerms.insert(newTerms.end(),
                                        f->_terms.begin(), f->_terms.end());
                        hasReduction = true;
                    } else {
                        // still a reduction in the term, replace
                        boost::shared_ptr<BoolTermFactor> newBtf;
                        newBtf.reset(new BoolTermFactor());
                        newBtf->_term = reduced;
                        newTerms.push_back(newBtf);
                        hasReduction = true;
                    }
                } else { // The bfterm's term couldn't be reduced,
                    // so just add it.
                    newTerms.push_back(*i);
                }
            } else { // Term-less bool term factor. Ignore.
                hasReduction = true;
            }
        } else {
            // add old bfterm
            newTerms.push_back(*i);
        }
    }
    return hasReduction;
}

bool BoolFactor::_checkParen(BfTerm::PtrList& terms) {
    if(terms.size() != 3) { return false; }

    PassTerm* pt = dynamic_cast<PassTerm*>(terms.front().get());
    if(!pt || (pt->_text != "(")) { return false; }

    pt = dynamic_cast<PassTerm*>(terms.back().get());
    if(!pt || (pt->_text != ")")) { return false; }

    return true;
}

boost::shared_ptr<BoolTerm> BoolFactor::getReduced() {
    // Get reduced versions of my children.
    BfTerm::PtrList newTerms;
    bool hasReduction = false;
    hasReduction = _reduceTerms(newTerms, _terms);
    // Parentheses reduction
    if(_checkParen(newTerms)) {
        newTerms.pop_front();
        newTerms.pop_back();
        hasReduction = true;
    }
    if(hasReduction) {
        BoolFactor* bf = new BoolFactor();
        bf->_terms = newTerms;
        QueryTemplate qt;
        bf->renderTo(qt);
        //std::cout << "reduced. " << qt.generate() << std::endl;

        return boost::shared_ptr<BoolFactor>(bf);
    } else {
        return boost::shared_ptr<BoolTerm>();
    }
#if 0
    if(_terms.size() >= 1) {
        QueryTemplate qt;
        std::string s;
        renderList(qt, _terms, "---");

        std::cout << "Can I reduce " << qt.generate() << std::endl;
    }
#endif

}


boost::shared_ptr<BoolTerm> OrTerm::copySyntax() {
    boost::shared_ptr<OrTerm> ot(new OrTerm());
    ot->_terms = _terms; // shallow copy for now
    return ot;
}
boost::shared_ptr<BoolTerm> AndTerm::copySyntax() {
    boost::shared_ptr<AndTerm> at(new AndTerm());
    at->_terms = _terms; // shallow copy for now
    return at;
}
boost::shared_ptr<BoolTerm> BoolFactor::copySyntax() {
    boost::shared_ptr<BoolFactor> bf(new BoolFactor());
    bf->_terms = _terms; // shallow copy for now
    return bf;
}
}}} // lsst::qserv::master
