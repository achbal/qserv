// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2013-2015 AURA/LSST.
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
  * @brief Implementation of a SelectParser
  *
  *  SelectParser is the top-level manager for everything attached to
  *  parsing the top-level SQL query. Given an input query and a
  *  configuration, computes a query info structure, name ref list,
  *  and a "query plan".
  *
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "parser/SelectParser.h"

// System headers
#include <cstdio>
#include <functional>
#include <strings.h>

// Third-party headers
#include <antlr/CommonAST.hpp>

#include "boost/bind.hpp"
#include "boost/make_shared.hpp"

// Qserv headers
#include "global/Bug.h"
#include "parser/ParseException.h"
#include "parser/parseExceptions.h"
#include "parser/parseTreeUtil.h"
#include "parser/SelectFactory.h"
#include "query/SelectStmt.h"
#include "SqlSQL2Parser.hpp"
#include "SqlSQL2Lexer.hpp"
#include "SqlSQL2TokenTypes.hpp"

// ANTLR headers, declare after auto-generated headers to
// reduce namespace pollution
#include <antlr/MismatchedCharException.hpp>
#include <antlr/MismatchedTokenException.hpp>
#include <antlr/NoViableAltException.hpp>
#include <antlr/NoViableAltForCharException.hpp>
#include <antlr/RecognitionException.hpp>
#include <antlr/SemanticException.hpp>

namespace lsst {
namespace qserv {
namespace parser {

////////////////////////////////////////////////////////////////////////
// AntlrParser -- Antlr parsing complex
////////////////////////////////////////////////////////////////////////
class AntlrParser {
public:
    AntlrParser(std::string const& q)
        : statement(q),
          stream(q, std::stringstream::in | std::stringstream::out),
          lexer(stream),
          parser(lexer)
        {}
    void run() {
        parser.initializeASTFactory(factory);
        parser.setASTFactory(&factory);
        try {
            parser.qserv_stmt();
        } catch(antlr::NoViableAltException& e) {
            throw ParseException("Parse error(ANTLR):"
                                 + e.getMessage(), e.node);
        } catch(antlr::NoViableAltForCharException& e) {
            throw ParseException("Parse error(unexpected char lex):"
                                 + e.getMessage(), e);
        } catch(antlr::MismatchedCharException& e) {
            throw ParseException("Parse char mismatch error:"
                                 + e.getMessage(), e);
        } catch(antlr::MismatchedTokenException& e) {
            throw ParseException("Parse token mismatch error:"
                                 + e.getMessage(), e.node);
        } catch(antlr::SemanticException& e) {
            throw ParseException("Parse error(corrupted, semantic):"
                                 + e.getMessage(), e);
        } catch(antlr::RecognitionException& e) {
            throw ParseException("Parse error(corrupted, recognition):"
                                 + e.getMessage(), e);
        } catch(antlr::ANTLRException& e) {
            throw ParseException("Unknown ANTLR error:" + e.getMessage(), e);
        } catch(ParseException& e) {
            throw;
        } catch (Bug& b) {
            throw ParseException(b);
        } catch (std::logic_error& e) {
            throw Bug(e.what());
        } catch (...) {
            // leading underscores in literals as value_expr throw this.
            throw Bug("Unknown parsing error");
        }
        RefAST a = parser.getAST();
    }

    std::string statement;
    std::stringstream stream;
    ASTFactory factory;
    SqlSQL2Lexer lexer;
    SqlSQL2Parser parser;
};

////////////////////////////////////////////////////////////////////////
// class SelectParser
////////////////////////////////////////////////////////////////////////

// Static factory function
SelectParser::Ptr
SelectParser::newInstance(std::string const& statement) {
    return boost::shared_ptr<SelectParser>(new SelectParser(statement));
}

// Construtor
SelectParser::SelectParser(std::string const& statement)
    :_statement(statement) {
}

void
SelectParser::setup() {
    _selectStmt = boost::make_shared<query::SelectStmt>();
    _aParser = boost::make_shared<AntlrParser>(_statement);
    // model 3: parse tree construction to build intermediate expr.
    SelectFactory sf;
    sf.attachTo(_aParser->parser);
    _aParser->run();
    _selectStmt = sf.getStatement();
    // _selectStmt->diagnose(); // helpful for debugging.
}

}}} // namespace lsst::qserv::parser
