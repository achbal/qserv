// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2008-2014 LSST Corporation.
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

// class SqlInsertIter -- A class that finds INSERT statements in
// mysqldump output and iterates over them.
// Should become obsolete with new  dump-less result transfer processing

#include "rproc/SqlInsertIter.h"

// System headers
#include <iostream>
#include <errno.h>

// Local headers
#include "log/Logger.h"


namespace {

// Helpers to make regex's
boost::regex makeLockInsertRegex(std::string const& tableName) {
    return boost::regex("LOCK TABLES `?" + tableName + "`? WRITE;"
                        "(.*?)(INSERT INTO[^;]*?;)+(.*?)"
                        "UNLOCK TABLES;");
}

boost::regex makeLockInsertOpenRegex(std::string const& tableName) {
    return boost::regex("LOCK TABLES `?" + tableName + "`? WRITE;"
                        "(.*?)(INSERT INTO[^;]*?;)+");
}

boost::regex makeLockOpenRegex(std::string const& tableName) {
    return boost::regex("LOCK TABLES `?" + tableName + "`? WRITE;");
}

boost::regex makeInsertRegex(std::string const& tableName) {
    return boost::regex("(INSERT INTO `?" + tableName +
                        "`? [^;]+?;)");// [~;]*?;)");
}

boost::regex makeNullInsertRegex(std::string const& tableName) {
    return boost::regex("(INSERT INTO `?" + tableName +
                        "`? +VALUES ?[(]NULL(,NULL)*[)];)");
}

// Helpful debugging
void printInserts(char const* buf, off_t bufSize,
                  std::string const& tableName)  {
    for(lsst::qserv::rproc::SqlInsertIter i(buf, bufSize, tableName, true);
        !i.isDone();
        ++i) {
        std::cout << "Sql[" << tableName << "]: "
                   << (void*)i->first << "  --->  "
                   << (void*)i->second << "  "
                   << *i;
        if(i.isNullInsert()) {
            std::cout << "Null match" << std::endl;
        } else { std::cout << std::endl; }
    }
}

} // anonymous namespace


namespace lsst {
namespace qserv {
namespace rproc {


////////////////////////////////////////////////////////////////////////
// SqlInsertIter
////////////////////////////////////////////////////////////////////////
// Static
SqlInsertIter::Iter SqlInsertIter::_nullIter;

SqlInsertIter::SqlInsertIter(char const* buf, off_t bufSize,
                             std::string const& tableName,
                             bool allowNull)
    : _allowNull(allowNull), _pBuffer(0) {
    _blockExpr = makeLockInsertRegex(tableName);
    _init(buf, bufSize, tableName);
}

SqlInsertIter::SqlInsertIter(xrdc::PacketIter::Ptr p,
                             std::string const& tableName,
                             bool allowNull)
    : _allowNull(allowNull), _pacIterP(p) {
    // We will need to keep our own buffer.  This is because the regex
    // iterator needs a continuous piece of memory.

    // The idea is to keep a sliding window where we can run the regex search.
    // While blocks can be found in the block, iterate over them.
    // When the search fails, remember where the last match
    // terminated.
    // Now increment the packet iter.
    // realloc the buffer to fit the unmatched remainder +
    // packetIter's advance (if needed). Note that buffer may be
    // bigger than what we need.
    // slide the unmatched to the beginning, memcpy the packetIter
    // data into our buffer, and setup the regex match again.
    // Continue.
#ifdef NEWLOG
    LOGF_DEBUG("EXECUTING SqlInsertIter(PacketIter::Ptr, %1%, %2%)" 
               % tableName % allowNull);
#else
    LOGGER_DBG << "EXECUTING SqlInsertIter(PacketIter::Ptr, " << tableName <<
                  ", " << allowNull << ")" << std::endl;
#endif
    assert(!(*p).isDone());
    _pBufStart = 0;
    _pBufEnd = (*p)->second;
    _pBufSize = 2 * _pBufEnd; // Size to 2x first fragment size
    // (which may be bigger than average)
    _pBuffer = static_cast<char*>(malloc(_pBufSize));

    memcpy(_pBuffer,(*p)->first, _pBufEnd);
    boost::regex lockInsertExpr(makeLockInsertOpenRegex(tableName));
    boost::regex lockExpr(makeLockOpenRegex(tableName));
    bool found = false;
    while(!found) {
        char const* buf = _pBuffer; // need to add const to help compiler
        found = boost::regex_search(buf,
                                    buf + _pBufEnd,
                                    _blockMatch, lockInsertExpr);
        if(found) {
#ifdef NEWLOG
            LOGF_DEBUG("Matched Lock statement within SqlInsertIter");
#else
            LOGGER_DBG << "Matched Lock statement within SqlInsertIter" << std::endl;
#endif
            break;
        } else {
#ifdef NEWLOG
            LOGF_DEBUG("Did not match Lock statement within SqlInsertIter");
#else
            LOGGER_DBG << "Did not match Lock statement within SqlInsertIter" << std::endl;
#endif
        }
        //Add next fragment, if available.
        if(!_incrementFragment()) {
            // Verify presence of Lock statement.
            if(boost::regex_search(buf, buf + _pBufEnd, _blockMatch,
                                   lockExpr)) {
                return;
            } else {
                errno = ENOTRECOVERABLE;
                // FIXME need a real exception here
                throw "Failed to match Lock statement within SqlInsertIter.";
            }
        }
    }
    _blockFound = found;
    _initRegex(tableName);
    // Might try _blockMatch[3].first, _blockMatch[3].second
    _setupIter();
}

SqlInsertIter::~SqlInsertIter() {
    if(_pBuffer) free(_pBuffer);
}

void SqlInsertIter::_setupIter() {
    _iter = Iter(_pBuffer + _pBufStart, _pBuffer + _pBufEnd, _insExpr);
}

bool SqlInsertIter::_incrementFragment() {
    // Advance iterator.
    ++(*_pacIterP);
    if(_pacIterP->isDone()) return false; // Any more?
    xrdc::PacketIter::Value v = **_pacIterP;
    // Make sure there is room in the buffer
    BufOff keepSize = _pBufEnd - _pBufStart;
    BufOff needSize = v.second + keepSize;
    if(needSize > (_pBufSize - _pBufEnd)) {
        if(needSize > _pBufSize) {
#ifdef NEWLOG
            LOGF_DEBUG("%1% is too small. sqliter Realloc to %2%" 
                       % _pBufSize % needSize);
#else
            LOGGER_DBG << _pBufSize << " is too small" << std::endl
                       << "sqliter Realloc to " << needSize << std::endl;
#endif
            void* res = realloc(_pBuffer, needSize);
            if (!res) {
                errno = ENOMEM;
                throw "Failed to realloc for SqlInsertIter.";
            }
            _pBufSize = needSize;
            _pBuffer = static_cast<char*>(res);
        }
        memmove(_pBuffer, _pBuffer+_pBufStart, keepSize);
        _pBufEnd = keepSize;
        _pBufStart = 0;
    }
    memcpy(_pBuffer + _pBufEnd, v.first, v.second);
    _pBufEnd += v.second;
    return true;
}

void SqlInsertIter::_initRegex(std::string const& tableName) {
    _insExpr = makeInsertRegex(tableName);
    _nullExpr = makeNullInsertRegex(tableName);
}

void SqlInsertIter::_init(char const* buf, off_t bufSize,
                          std::string const& tableName) {
    boost::regex lockInsertRegex(makeLockInsertRegex(tableName));
    assert(buf < (buf+bufSize));
    _blockFound = boost::regex_search(buf, buf+bufSize,
                                      _blockMatch, lockInsertRegex);
    if(_blockFound) {
        _initRegex(tableName);
        _iter = Iter(_blockMatch[2].first, _blockMatch[3].second,
                     _insExpr);
    }
}

bool SqlInsertIter::isNullInsert() const {
    // Avoid constructing a string > 1MB just to check for null.
    if(_iter->length() > (1<<20)) return false;
    return boost::regex_match(_iter->str(), _nullExpr);
}

SqlInsertIter& SqlInsertIter::operator++() {
    do {
        _increment();
    } while(!isDone() && (!_allowNull) && isNullInsert());
        return *this;
}

bool SqlInsertIter::isDone() const {
    if(_pacIterP) {
        return (_iter == _nullIter) || _pacIterP->isDone();
    } else {
        return _iter == _nullIter;
    }
}

/// Increment this iterator to point at the next INSERT statement.
/// If our buffer includes the full data dump, then this is easy--we
/// can just advance the regex iterator _iter.  However, when we are
/// iterating over the dump in "packets", we may need to advance
/// the packet iterator.
void SqlInsertIter::_increment() {
    if(_pacIterP) {
        // Set _pBufStart to end of last match.
        _pBufStart = static_cast<BufOff>((*_iter)[0].second - _pBuffer);
        ++_iter; // Advance the regex to the next INSERT stmt
        while((_iter == _nullIter) && !_pacIterP->isDone()) {
            _incrementFragment(); // Extend buffer
            _setupIter(); // Reset the iterator.
        }
        // Either we found an insert or there are no more packets.
    } else { // If fully buffered.
        ++_iter;
    }
}

}}} // namespace lsst::qserv::rproc
