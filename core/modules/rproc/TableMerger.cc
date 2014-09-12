// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2009-2014 LSST Corporation.
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
  * @brief TableMerger implementation
  *
  * TableMerger is a class that is responsible for the organized
  * merging of query results into a single table that can be returned
  * to the user. The current strategy loads dumped chunk result tables
  * from workers into a single table, followed by a
  * merging/aggregation query (as needed) to produce the final user
  * result table.
  *
  * @author Daniel L. Wang, SLAC
  */

#include "rproc/TableMerger.h"

// System headers
#include <cassert>
#include <iostream>
#include <sstream>
#include <sys/time.h>

// Third-party headers
#include <boost/format.hpp>
#include <boost/regex.hpp>

// Local headers
#include "log/Logger.h"
#include "rproc/SqlInsertIter.h"
#include "sql/SqlConnection.h"
#include "util/MmapFile.h"

namespace { // File-scope helpers

using lsst::qserv::mysql::MySqlConfig;
using lsst::qserv::rproc::TableMergerConfig;

std::string getTimeStampId() {
    struct timeval now;
    int rc = gettimeofday(&now, NULL);
    if (rc != 0) throw "Failed to get timestamp.";
    std::stringstream s;
    s << (now.tv_sec % 10000) << now.tv_usec;
    return s.str();
    // Use the lower digits as pseudo-unique (usec, sec % 10000)
    // FIXME: is there a better idea?
}

boost::shared_ptr<MySqlConfig> makeSqlConfig(TableMergerConfig const& c) {
    boost::shared_ptr<MySqlConfig> sc(new MySqlConfig());
    assert(sc.get());
    sc->username = c.user;
    sc->dbName = c.targetDb;
    sc->socket = c.socket;
    return sc;
}

// In-place string replacement that pads to minimize string copying.
// Non-space characters surrounding original substring are assumed to be
// quotes and are retained.
void inplaceReplace(std::string& s, std::string const& old,
                    std::string const& replacement,
                    bool dropQuote) {
    std::string::size_type pos = s.find(old);
    char quoteChar = s[pos-1];
    std::string rplc = replacement;
    std::string::size_type rplcSize = old.size();;
    if((quoteChar != ' ') && (quoteChar == s[pos + rplcSize])) {
        if(!dropQuote) {
            rplc = quoteChar + rplc + quoteChar;
        }
        rplcSize += 2;
        --pos;
    }
    if(rplc.size() < rplcSize) { // do padding for in-place
        rplc += std::string(rplcSize - rplc.size(), ' ');
    }
#ifdef NEWLOG
    //LOGF_DEBUG("rplc %1% old=%2%" % rplc % s.substr(pos-1, rplcSize+2));
#else
    //LOGGER_DBG << "rplc " << rplc << " old=" << s.substr(pos-1, rplcSize+2) << std::endl;
#endif
    s.replace(pos, rplcSize, rplc);
#ifdef NEWLOG
    //LOGF_DEBUG("newnew: %1%" % s.substr(pos-5, rplcSize+10));
#else
    //LOGGER_DBG << "newnew: " << s.substr(pos-5, rplcSize+10) << std::endl;
#endif
    return;
}

std::string extractReplacedCreateStmt(char const* s, ::off_t size,
                                      std::string const& oldTable,
                                      std::string const& newTable,
                                      bool dropQuote) {
#ifdef NEWLOG
    LOGF_DEBUG("EXECUTING TableMerger::extractReplacedCreateStmt()");
#else
    LOGGER_DBG << "EXECUTING TableMerger::extractReplacedCreateStmt()" << std::endl;
#endif

    boost::regex createExp("(CREATE TABLE )(`?)(" + oldTable + ")(`?)( ?[^;]+?;)");
    std::string newForm;
    if(dropQuote) {
        newForm = "\\1" + newTable + "\\5";
    } else {
        newForm = "\\1\\2" + newTable + "\\4\\5";
    }
    std::string out;
    std::stringstream ss;
    std::ostream_iterator<char,char> oi(ss);
    regex_replace(oi, s, s+size, createExp, newForm,
                  boost::match_default | boost::format_perl
                  | boost::format_no_copy | boost::format_first_only);
    out = ss.str();
    return out;
}

std::string dropDbContext(std::string const& tableName,
                          std::string const& context) {
    std::string contextDot = context + ".";
    if(tableName.substr(0,contextDot.size()) == contextDot) {
        return tableName.substr(contextDot.size());
    }
    return tableName;
}

} // anonymous namespace


namespace lsst {
namespace qserv {
namespace rproc {

std::string const TableMerger::_dropSql("DROP TABLE IF EXISTS %s;");
std::string const TableMerger::_createSql("CREATE TABLE IF NOT EXISTS %s SELECT * FROM %s;");
std::string const TableMerger::_createFixSql("CREATE TABLE IF NOT EXISTS %s SELECT %s FROM %s %s;");
std::string const TableMerger::_insertSql("INSERT INTO %s SELECT * FROM %s;");
std::string const TableMerger::_cleanupSql("DROP TABLE IF EXISTS %s;");
std::string const TableMerger::_cmdBase("%1% --socket=%2% -u %3% %4%");

////////////////////////////////////////////////////////////////////////
// TableMergerError
////////////////////////////////////////////////////////////////////////
bool TableMergerError::resultTooBig() const {
    return (status == MYSQLEXEC) && (errorCode == 1114);
}

////////////////////////////////////////////////////////////////////////
// CreateStmt : helper class for extracting create statements
// from the dump.
////////////////////////////////////////////////////////////////////////
class TableMerger::CreateStmt {
public:
    CreateStmt(PacketBufferPtr pb,
               std::string const& table,
               std::string const& targetDb,
               std::string const& targetTable)
        : _pacBuffer(pb),
          _table(table) {
        _setup(targetDb, targetTable);
    }

    CreateStmt(char const* buf, std::size_t size,
               std::string const& table,
               std::string const& targetDb,
               std::string const& targetTable)
        : _buf(buf), _size(size),
          _table(table) {
        _setup(targetDb, targetTable);
    }

    std::string const& getTable() const { return _table; }

    std::string getStmt() {
        if(_pacBuffer) {
            return _makeStmt(_pacBuffer);
        } else {
            assert(_buf);
            return _makeStmt(_buf, _size);
        }
    }
private:
    void _setup(std::string const& targetDb, std::string const& targetTable) {
        _dropQuote = (std::string::npos != targetTable.find("."));
        _realTarget = dropDbContext(targetTable, targetDb);
    }

    std::string _makeStmt(PacketBufferPtr pb) {
        // Perform the (patched) CREATE TABLE, then process as an INSERT.
        std::string createSql;
        while(true) {
//            ::off_t sz = (*pb)->second;
            createSql = extractReplacedCreateStmt((*pb)->first,
                                                  (*pb)->second,
                                                  _table,
                                                  _realTarget,
                                                  _dropQuote);
            if(!createSql.empty()) {
                break;
            }
            // Extend, since we didn't find the CREATE statement.
            if(!pb->incrementExtend()) {
                errno = ENOTRECOVERABLE;
                throw "Create statement not found.";
            }
        }
        return createSql;
    }
    std::string _makeStmt(char const* buf, std::size_t size) {
        return extractReplacedCreateStmt(buf, size,
                                         _table,
                                         _realTarget,
                                         _dropQuote);
    }
    PacketBufferPtr _pacBuffer;
    char const* _buf;
    std::size_t _size;
    std::string _table;
    bool _dropQuote;
    std::string _realTarget;
};

////////////////////////////////////////////////////////////////////////
// public
////////////////////////////////////////////////////////////////////////
TableMerger::TableMerger(TableMergerConfig const& c)
    : _config(c),
      _sqlConfig(makeSqlConfig(c)),
      _tableCount(0),
      _isFinished(false) {
    _fixupTargetName();
    _loadCmd = (boost::format(_cmdBase)
		% c.mySqlCmd % c.socket % c.user % c.targetDb).str();
}

bool TableMerger::merge(std::string const& dumpFile,
                        std::string const& tableName) {
    return merge2(dumpFile, tableName);
}

off_t TableMerger::merge(char const* dumpBuffer, int dumpLength,
                         std::string const& tableName) {
#ifdef NEWLOG
    LOGF_DEBUG("EXECUTING TableMerger::merge(packetbuffer, %1%)" % tableName);
#else
    LOGGER_DBG << "EXECUTING TableMerger::merge(packetbuffer, " << tableName << ")" << std::endl;
#endif
    bool allowNull = true;
    CreateStmt cs(dumpBuffer, dumpLength, tableName,
                  _config.targetDb, _mergeTable);
    _createTableIfNotExists(cs);
    SqlInsertIter sii(dumpBuffer, dumpLength, tableName, allowNull);
    bool successful = _importIter(sii, tableName);
    if(!successful) {
#ifdef NEWLOG
        LOGF_DEBUG("UNSUCCESSFUL TableMerger::merge(buffer), %1%)" % tableName);
#else
        LOGGER_DBG << "UNSUCCESSFUL TableMerger::merge(buffer), " << tableName << ")" << std::endl;
#endif
        _error.status = TableMergerError::IMPORT;
        _error.errorCode = 0;
        _error.description = "Unknown result import error.";
        return 0;
    } else if(sii.getLastUsed() == 0) {
        // Tried to import, no errors, but didn't use anything from the buffer.
        return 0;
    }
    off_t used = sii.getLastUsed() - dumpBuffer;
    return used;
}

bool TableMerger::merge(boost::shared_ptr<util::PacketBuffer> pb,
                        std::string const& tableName) {
#ifdef NEWLOG
    LOGF_DEBUG("EXECUTING TableMerger::merge(packetbuffer, %1%)" % tableName);
#else
    LOGGER_DBG << "EXECUTING TableMerger::merge(packetbuffer, " << tableName << ")" << std::endl;
#endif
    bool allowNull = false;
    CreateStmt cs(pb, tableName, _config.targetDb, _mergeTable);
    _createTableIfNotExists(cs);
    // No locking needed if not first, after updating the counter.
    // Once the table is created, everyone should insert.
    SqlInsertIter sii(pb, tableName, allowNull);
    return _importIter(sii, tableName);
}

bool TableMerger::finalize() {
    if(_isFinished) {
#ifdef NEWLOG
        LOGF_ERROR("TableMerger::finalize(), but _isFinished == true");
#else
        LOGGER_ERR << "TableMerger::finalize(), but _isFinished == true"
                   << std::endl;
#endif
    }
    if(_mergeTable != _config.targetTable) {
        std::string cleanup = (boost::format(_cleanupSql) % _mergeTable).str();
        std::string fixupSuffix = _config.mFixup.post + _buildOrderByLimit();

        // Need to perform fixup for aggregation.
        std::string sql = (boost::format(_createFixSql)
                           % _config.targetTable
                           % _config.mFixup.select
                           % _mergeTable
                           % fixupSuffix).str() + cleanup;
#ifdef NEWLOG
        LOGF_INFO("Merging w/%1%" % sql);
#else
        LOGGER_INF << "Merging w/" << sql << std::endl;
#endif
        return _applySql(sql);
    }
#ifdef NEWLOG
    LOGF_INFO("Merged %1% into %2%" % _mergeTable % _config.targetTable);
#else
    LOGGER_INF << "Merged " << _mergeTable << " into " << _config.targetTable
               << std::endl;
#endif
    _isFinished = true;
    return true;
}

bool TableMerger::isFinished() const {
    return _isFinished;
}

////////////////////////////////////////////////////////////////////////
// private
////////////////////////////////////////////////////////////////////////
bool TableMerger::_applySql(std::string const& sql) {
    return _applySqlLocal(sql); //try local impl now.
    FILE* fp;
    {
        boost::lock_guard<boost::mutex> m(_popenMutex);
        fp = popen(_loadCmd.c_str(), "w"); // check error
    }
    if(fp == NULL) {
        _error.status = TableMergerError::MYSQLOPEN;
        _error.errorCode = 0;
        _error.description = "Error starting mysql process.";
        return false;
    }
    int written = fwrite(sql.c_str(), sizeof(std::string::value_type),
                         sql.size(), fp);
    if(((unsigned)written) != (sql.size() * sizeof(std::string::value_type))) {
        _error.status = TableMergerError::MERGEWRITE;
        _error.errorCode = written;
        _error.description = "Error writing sql to mysql process.." + sql;
        {
            boost::lock_guard<boost::mutex> m(_popenMutex);
            pclose(fp); // cleanup
        }
        return false;
    }
    int r;
    {
        boost::lock_guard<boost::mutex> m(_popenMutex);
        r = pclose(fp);
    }
    if(r == -1) {
        _error.status = TableMergerError::TERMINATE;
        _error.errorCode = r;
        _error.description = "Error finalizing merge step..";
        return false;
    }
    return true;
}

bool TableMerger::_applySqlLocal(std::string const& sql) {
    boost::lock_guard<boost::mutex> m(_sqlMutex);
    sql::SqlErrorObject errObj;
    if(!_sqlConn.get()) {
        _sqlConn.reset(new sql::SqlConnection(*_sqlConfig, true));
        if(!_sqlConn->connectToDb(errObj)) {
            _error.status = TableMergerError::MYSQLCONNECT;
            _error.errorCode = errObj.errNo();
            _error.description = "Error connecting to db. " + errObj.printErrMsg();
            _sqlConn.reset();
            return false;
        } else {
#ifdef NEWLOG
            LOGF_INFO("TableMerger %1% connected to db." % (void*) this);
#else
            LOGGER_INF << "TableMerger " << (void*) this
                       << " connected to db." << std::endl;
#endif
        }
    }
    if(!_sqlConn->runQuery(sql, errObj)) {
        _error.status = TableMergerError::MYSQLEXEC;
        _error.errorCode = errObj.errNo();
        _error.description = "Error applying sql. " + errObj.printErrMsg();
        return false;
    }
    return true;
}

std::string TableMerger::_buildMergeSql(std::string const& tableName,
                                        bool create) {
    std::string cleanup = (boost::format(_cleanupSql) % tableName).str();

    if(create) {
        return (boost::format(_dropSql) % _mergeTable).str()
            + (boost::format(_createSql) % _mergeTable
               % tableName).str() + cleanup;
    } else {
        return (boost::format(_insertSql) %  _mergeTable
                % tableName).str() + cleanup;
    }
}

std::string TableMerger::_buildOrderByLimit() {
    std::stringstream ss;
    if(!_config.mFixup.orderBy.empty()) {
        ss << "ORDER BY " << _config.mFixup.orderBy;
    }
    if(_config.mFixup.limit != -1) {
        if(!_config.mFixup.orderBy.empty()) { ss << " "; }
        ss << "LIMIT " << _config.mFixup.limit;
    }
    return ss.str();
}

bool TableMerger::_createTableIfNotExists(TableMerger::CreateStmt& cs) {
#ifdef NEWLOG
    LOGF_DEBUG("Importing %1%" % cs.getTable());
#else
    LOGGER_DBG << "Importing " << cs.getTable() << std::endl;
#endif
    boost::lock_guard<boost::mutex> g(_countMutex);
    ++_tableCount;
    if(_tableCount == 1) {
        bool isOk = _dropAndCreate(cs.getTable(), cs.getStmt());
        if(!isOk) {
            --_tableCount; // We failed merging the table.
            return false;
        }
    }
    return true;
}

void TableMerger::_fixupTargetName() {
    if(_config.targetTable.empty()) {
        assert(!_config.targetDb.empty());
        _config.targetTable = (boost::format("%1%.result_%2%")
                               % _config.targetDb % getTimeStampId()).str();
    }

    if(_config.mFixup.needsFixup) {
        // Set merging temporary if needed.
        _mergeTable = _config.targetTable + "_m";
    } else {
        _mergeTable = _config.targetTable;
    }
}

bool TableMerger::_importResult(std::string const& dumpFile) {
    int rc = system((_loadCmd + "<" + dumpFile).c_str());
    if(rc != 0) {
        _error.status = TableMergerError::IMPORT;
        _error.errorCode = rc;
        _error.description = "Error importing result db.";
        return false;
    }
    return true;
}

bool TableMerger::merge2(std::string const& dumpFile,
                        std::string const& tableName) {
    boost::shared_ptr<util::MmapFile> m =
        util::MmapFile::newMap(dumpFile, true, false);
    if(!m.get()) {
        // Fallback to non-mmap version.
        return _slowImport(dumpFile, tableName);
    }
    char const* buf = static_cast<char const*>(m->getBuf());
    ::off_t size = m->getSize();
    bool allowNull = false;
    CreateStmt cs(buf, size, tableName, _config.targetDb, _mergeTable);
    _createTableIfNotExists(cs);
    // No locking needed if not first, after updating the counter.
    // Once the table is created, everyone should insert.
    return _importBufferInsert(buf, size, tableName, allowNull);
}

bool TableMerger::_dropAndCreate(std::string const& tableName,
                                 std::string createSql) {

    std::string dropSql = "DROP TABLE IF EXISTS " + tableName + ";";
    if(_config.dropMem) {
        std::string const memSpec = "ENGINE=MEMORY";
        std::string::size_type pos = createSql.find(memSpec);
        if(pos != std::string::npos) {
            createSql.erase(pos, memSpec.size());
        }
    }
#ifdef NEWLOG
    LOGF_DEBUG("CREATE-----%1%" % _mergeTable);
#else
    LOGGER_DBG << "CREATE-----" << _mergeTable << std::endl;
#endif
    return _applySql(dropSql + createSql);
}

bool TableMerger::_importIter(SqlInsertIter& sii,
                              std::string const& tableName) {
#ifdef NEWLOG
    LOGF_DEBUG("EXECUTING TableMerger::_importIter(sii, %1%)" % tableName);
#else
    LOGGER_DBG << "EXECUTING TableMerger::_importIter(sii, " << tableName << ")" << std::endl;
#endif
    int insertsCompleted = 0;
    // Search the buffer for the insert statement,
    // patch it (and future occurrences for the old table name,
    // and merge directly.
#ifdef NEWLOG
    LOGF_DEBUG("MERGE INTO-----%1%" % _mergeTable);
#else
    LOGGER_DBG << "MERGE INTO-----" << _mergeTable << std::endl;
#endif
    for(; !sii.isDone(); ++sii) {
        char const* stmtBegin = sii->first;
        std::size_t stmtSize = sii->second - stmtBegin;
        std::string q(stmtBegin, stmtSize);
        bool dropQuote = (std::string::npos != _mergeTable.find("."));
        inplaceReplace(q, tableName,
                       dropDbContext(_mergeTable, _config.targetDb),
                       dropQuote);
        if(!_applySql(q)) {
            if(!_error.resultTooBig()) {
                std::stringstream errStrm;
                errStrm << "Failed importing! " << tableName << " "
                        << _error.description << "(code="
                        << _error.errorCode << ")";
#ifdef NEWLOG
                LOGF_ERROR("%1%" % errStrm.str());
#else
                LOGGER_ERR << errStrm.str() << std::endl;
#endif
                throw std::runtime_error(errStrm.str());
            } else {
                std::stringstream errStrm;
                errStrm << "Error importing to " << tableName << " "
                        << _error.description << "(Result too big)";
#ifdef NEWLOG
                LOGF_ERROR("%1%" % errStrm.str());
#else
                LOGGER_ERR << errStrm.str() << std::endl;
#endif
            }
            return false;
        }
        ++insertsCompleted;
    }
    return true;
}

bool TableMerger::_importBufferInsert(char const* buf, std::size_t size,
                                      std::string const& tableName,
                                      bool allowNull) {
    SqlInsertIter sii(buf, size, tableName, allowNull);
    bool successful = _importIter(sii, tableName);
    if(!successful) {
#ifdef NEWLOG
        LOGF_ERROR("Error importing to %1% buffer of size=%2%" % 
                   tableName % size);
#else
        LOGGER_ERR << "Error importing to " << tableName
                   << " buffer of size=" << size << std::endl;
#endif
        return false;
    }
    return true;
}

bool TableMerger::_slowImport(std::string const& dumpFile,
                              std::string const& tableName) {
    assert(false);
    bool isOk = true;
    std::string sql;
    _importResult(dumpFile);
    {
#ifdef NEWLOG
        LOGF_DEBUG("Importing %1%" % tableName);
#else
        LOGGER_DBG << "Importing " << tableName << std::endl;
#endif
        boost::lock_guard<boost::mutex> g(_countMutex);
        ++_tableCount;
        if(_tableCount == 1) {
            sql = _buildMergeSql(tableName, true);
            isOk = _applySql(sql);
            if(!isOk) {
#ifdef NEWLOG
                LOGF_ERROR("Failed importing! %1% %2%"
                           % tableName % _error.description);
#else
                LOGGER_ERR << "Failed importing! " << tableName
                           << " " << _error.description << std::endl;
#endif
                --_tableCount; // We failed merging the table.
            }
	    return isOk; // must happen first.
        }
    }
    // No locking needed if not first, after updating the counter.
    sql = _buildMergeSql(tableName, false);
    return _applySql(sql);
}

}}} // namespace lsst::qserv::rproc
