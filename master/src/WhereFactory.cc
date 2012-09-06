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
// WhereFactory.cc houses the implementation of the WhereFactory, which
// is responsible for constructing
// WhereList (and SelectList, etc.) from an ANTLR parse tree.  For
// now, the code for other factories is included here as well.

#include "lsst/qserv/master/WhereFactory.h"

// Std
#include<iterator>

// Package
#include "SqlSQL2Parser.hpp" // applies several "using antlr::***".
#include "lsst/qserv/master/parserBase.h" // Handler base classes
#include "lsst/qserv/master/SelectList.h" // WhereClause decl.



// namespace modifiers
namespace qMaster = lsst::qserv::master;

// Anonymous helpers
namespace {
class ParamGenerator {
public:
    struct Check {
        bool operator()(RefAST r) {
            return (r->getType() == SqlSQL2TokenTypes::RIGHT_PAREN) 
                || (r->getType() == SqlSQL2TokenTypes::COMMA);
        }
    };
    class Iter  {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::string value_type;
        typedef int difference_type;
        typedef std::string* pointer;
        typedef std::string& reference;

        RefAST start;
        RefAST current;
        RefAST nextCache;
        Iter operator++(int) {
            //std::cout << "advancingX..: " << current->getText() << std::endl;
            Iter tmp = *this;
            ++*this;
            return tmp;
        }
        Iter& operator++() {
            //std::cout << "advancing..: " << current->getText() << std::endl;
            Check c;
            if(nextCache.get()) { 
                current = nextCache; 
            } else {
                current = qMaster::findSibling(current, c);
                if(current.get()) {
                    // Move to next value
                    current = current->getNextSibling(); 
                }
            }
            return *this; 
        }

        std::string operator*() {
            Check c;
            assert(current.get());
            qMaster::CompactPrintVisitor<antlr::RefAST> p;
            for(;current.get() && !c(current); 
                current = current->getNextSibling()) {
                p(current);                
            }
            return p.result;
        }
        bool operator==(Iter const& rhs) const {
            return (start == rhs.start) && (current == rhs.current);
        }
        bool operator!=(Iter const& rhs) const {
            return !(*this == rhs);
        }

    };
    ParamGenerator(RefAST a) {
        _beginIter.start = a;
        if(a.get() && (a->getType() == SqlSQL2TokenTypes::LEFT_PAREN)) {
            _beginIter.current = a->getNextSibling(); // Move past paren.
        } else { // else, set current as end.
            _beginIter.current = RefAST();
        }
        _endIter.start = a;
        _endIter.current = RefAST();
    }

    Iter begin() {
        return _beginIter;
    }

    Iter end() {
        return _endIter;
    }
private:
    Iter _beginIter;
    Iter _endIter;
};
} // anonymouse namespace

////////////////////////////////////////////////////////////////////////
// WhereFactory::WhereCondH
////////////////////////////////////////////////////////////////////////
class lsst::qserv::master::WhereFactory::WhereCondH : public VoidOneRefFunc {
public:
    WhereCondH(lsst::qserv::master::WhereFactory& wf) : _wf(wf) {}
    virtual ~WhereCondH() {}
    virtual void operator()(antlr::RefAST where) {
        _wf._import(where);
    }
private:
    lsst::qserv::master::WhereFactory& _wf;
};
////////////////////////////////////////////////////////////////////////
// WhereFactory
////////////////////////////////////////////////////////////////////////
using qMaster::WhereClause;
using qMaster::WhereFactory;
WhereFactory::WhereFactory() {
    // FIXME
}

boost::shared_ptr<WhereClause> WhereFactory::getProduct() {
    // FIXME
    return boost::shared_ptr<WhereClause>(new WhereClause());
}

void 
WhereFactory::attachTo(SqlSQL2Parser& p) {
    boost::shared_ptr<WhereCondH> wch(new WhereCondH(*this));
    p._whereCondHandler = wch;
#if 0
    // Should trigger when the WHERE clause is detected.
    // Modifiers too? (ORDER, GROUP, LIMIT)
#endif
}

void 
WhereFactory::_import(antlr::RefAST a) {
    _clause.reset(new WhereClause());
    // std::cout << "WHERE starts with: " << a->getText() 
    //           << " (" << a->getType() << ")" << std::endl;    

    // std::cout << "WHERE indented: " << walkIndentedString(a) << std::endl;
    assert(a->getType() == SqlSQL2TokenTypes::SQL2RW_where);
    RefAST first = a->getFirstChild();
    assert(first.get());
    switch(first->getType()) {
    case SqlSQL2TokenTypes::QSERV_FCT_SPEC:
        while(first->getType() == SqlSQL2TokenTypes::QSERV_FCT_SPEC) {
            _addQservRestrictor(first->getFirstChild());
            first = first->getNextSibling();
            if(first.get() 
               && (first->getType() == SqlSQL2TokenTypes::SQL2RW_and)) {
                first = first->getNextSibling();
                assert(first.get());
            } else { break; }
        }
        _addOrSibs(first->getFirstChild());
        break;
    case SqlSQL2TokenTypes::OR_OP:
        _addOrSibs(first->getFirstChild());
        break;
    default:
        break;
    }
    // FIXME: walk the tree and add elements.
}
void 
WhereFactory::_addQservRestrictor(antlr::RefAST a) {
    std::string r(a->getText()); // e.g. qserv_areaspec_box
    std::cout << "Adding from " << r << " : ";
    ParamGenerator pg(a->getNextSibling());
    std::vector<std::string> params;

    // for(ParamGenerator::Iter it = pg.begin();
    //     it != pg.end();
    //     ++it) {
    //     std::cout << "iterating:" << *it << std::endl;
    // }
    std::copy(pg.begin(), pg.end(), std::back_inserter(params));
    std::copy(params.begin(),params.end(),
              std::ostream_iterator<std::string>(std::cout,", "));
    // FIXME: add restrictor spec.
}

template <typename Check>
struct PrintExcept : public qMaster::PrintVisitor<antlr::RefAST> {
public:
    PrintExcept(Check c_) : c(c_) {}
    void operator()(antlr::RefAST a) {
        if(!c(a)) qMaster::PrintVisitor<antlr::RefAST>::operator()(a);
    }
    Check& c;
};
struct MetaCheck {
    bool operator()(antlr::RefAST a) {
        if(!a.get()) return false;
        switch(a->getType()) {
        case SqlSQL2TokenTypes::OR_OP:
        case SqlSQL2TokenTypes::AND_OP:
        case SqlSQL2TokenTypes::BOOLEAN_FACTOR:
        case SqlSQL2TokenTypes::VALUE_EXP:
            return true;
        default:
            return false;
        }
        return false;
    }
};

void 
WhereFactory::_addOrSibs(antlr::RefAST a) {
    MetaCheck mc;
    PrintExcept<MetaCheck> p(mc);
    
    assert(_clause.get());

    walkTreeVisit(a, p);
    std::cout << "Adding orsibs: " << p.result << std::endl;
    _clause->_original = p.result;
    // FIXME: Store template.
    // Template must allow table substitution.
    // For now, reuse old templating scheme.
}
