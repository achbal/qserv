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

#include <sstream>
#include <cstdio>
// Boost
#include <boost/thread.hpp> // for mutex. 
#include <boost/format.hpp> // for mutex. 

#include "SqlConnection.hh"

using lsst::qserv::SqlConnection;
using lsst::qserv::SqlResults;

bool SqlConnection::_isReady = false;
boost::mutex SqlConnection::_sharedMutex;

void
SqlResults::freeResults() {
    int i, s = _results.size();
    for (i=0 ; i<s ; i++) {
        mysql_free_result(_results[i]);
    }
    _results.clear();
}

void 
SqlResults::addResult(MYSQL_RES* r) {
    if ( _discardImmediately ) {
        mysql_free_result(r);
    } else {
        _results.push_back(r);
    }
}

bool
SqlResults::extractFirstColumn(std::vector<std::string>& ret,
                               SqlErrorObject& errObj) {
    int i, s = _results.size();
    for (i=0 ; i<s ; i++) {
        MYSQL_ROW row;
        while (row = mysql_fetch_row(_results[i])) {
            ret.push_back(row[0]);
        }
        mysql_free_result(_results[i]);
    }
    _results.clear();
    return true;
}

bool
SqlResults::extractFirst2Columns(std::vector<std::string>& col1,
                                 std::vector<std::string>& col2,
                                 SqlErrorObject& errObj) {
    int i, s = _results.size();
    for (i=0 ; i<s ; i++) {
        MYSQL_ROW row;
        while (row = mysql_fetch_row(_results[i])) {
            col1.push_back(row[0]);
            col2.push_back(row[1]);
        }
        mysql_free_result(_results[i]);
    }
    _results.clear();
    return true;
}

bool
SqlResults::extractFirstValue(std::string& ret, SqlErrorObject& errObj) {
    if (_results.size() != 1) {
        std::stringstream ss;
        ss << "Expecting one row, found " << _results.size() << " results"
           << std::endl;
        return errObj.addErrMsg(ss.str());
    }
    MYSQL_ROW row = mysql_fetch_row(_results[0]);
    if (!row) {
        return errObj.addErrMsg("Expecting one row, found no rows");
    }
    ret = *(row[0]);
    freeResults();
    return true;
}

SqlConnection::SqlConnection(SqlConfig const& sc, bool useThreadMgmt) 
    : _conn(NULL), _config(sc), 
      _connected(false), _useThreadMgmt(useThreadMgmt) { 
    {
        boost::lock_guard<boost::mutex> g(_sharedMutex);
        if(!_isReady) {
            int rc = mysql_library_init(0, NULL, NULL);
            assert(0 == rc);
        }
    }
    if(useThreadMgmt) {
        mysql_thread_init();
    }
}

SqlConnection::~SqlConnection() {
    if(_conn) {
        mysql_close(_conn);
    }
    if(_useThreadMgmt) {
        mysql_thread_end();
    }
}

bool 
SqlConnection::connectToDb(SqlErrorObject& errObj) {
    if(_connected) return true;
    return _init(errObj) && _connect(errObj);
}

bool 
SqlConnection::selectDb(std::string const& dbName, SqlErrorObject& errObj) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    if (_config.dbName == dbName) {
        return true; // nothing to do
    }
    if (!dbExists(dbName, errObj)) {
        return errObj.addErrMsg (std::string("Can't switch to db ") 
                                 + dbName + " (does not exist)");
    }
    
    if (mysql_select_db(_conn, dbName.c_str())) {
        return _setErrorObject(errObj, "Problem selecting db " + dbName);
    }
    _config.dbName = dbName;
    return true;
}

bool 
SqlConnection::runQuery(char const* query, 
                        int qSize,
                        SqlResults& results,
                        SqlErrorObject& errObj) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    if (mysql_real_query(_conn, query, qSize) != 0) {
        MYSQL_RES* result = mysql_store_result(_conn);
        if (result) mysql_free_result(result);
        std::string msg = std::string("Unable to execute query: ") + query;
        //    + "\nQuery = " + std::string(query, qSize);
        return _setErrorObject(errObj, msg);
    }
    int status = 0;
    do {
        MYSQL_RES* result = mysql_store_result(_conn);
        if (result) {
            results.addResult(result);
        } else if (mysql_field_count(_conn) != 0) {
            return _setErrorObject(errObj, 
                    std::string("Unable to store result for query: ") + query);
        }
        status = mysql_next_result(_conn);
        if (status > 0) {
            return _setErrorObject(errObj,
                  std::string("Error retrieving results for query: ") + query);
        }
    } while (status == 0);
    return true;
}

bool 
SqlConnection::runQuery(char const* query, int qSize, SqlErrorObject& errObj) {
    SqlResults results(true); // true - discard results immediately
    return runQuery(query, qSize, results, errObj);
}

bool 
SqlConnection::runQuery(std::string const query,
                        SqlResults& results,
                        SqlErrorObject& errObj) {
    return runQuery(query.data(), query.size(), results, errObj);
}

bool 
SqlConnection::runQuery(std::string const query,
                        SqlErrorObject& errObj) {
    SqlResults results(true); // true - discard results immediately
    return runQuery(query.data(), query.size(), results, errObj);
}

bool 
SqlConnection::dbExists(std::string const& dbName, SqlErrorObject& errObj) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    std::string sql = "SELECT COUNT(*) FROM information_schema.schemata ";
    sql += "WHERE schema_name = '";
    sql += dbName + "'";
    
    SqlResults results;
    if ( !runQuery(sql, results, errObj) ) {
        return errObj.addErrMsg("Failed to run: " + sql);
    }
    std::string s;
    if ( !results.extractFirstValue(s, errObj)) {
        return false;
    }
    return s[0] == '1';
}

bool 
SqlConnection::createDb(std::string const& dbName, 
                        SqlErrorObject& errObj, 
                        bool failIfExists) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    if (dbExists(dbName, errObj)) {
        if (failIfExists) {
            return errObj.addErrMsg(std::string("Can't create db ") 
                                    + dbName + ", it already exists");
        }
        return true;
    }
    std::string sql = "CREATE DATABASE " + dbName;
    if (!runQuery(sql, errObj)) {
        return _setErrorObject(errObj, "Problem executing: " + sql);
    }
    return true;
}

bool 
SqlConnection::createDbAndSelect(std::string const& dbName, 
                                 SqlErrorObject& errObj, 
                                 bool failIfExists) {
    if ( ! createDb(dbName, errObj, failIfExists) ) {
        return errObj.addErrMsg("Failed to create db " + dbName);
    }
    return selectDb(dbName, errObj);
}

bool 
SqlConnection::dropDb(std::string const& dbName, 
                      SqlErrorObject& errObj,
                      bool failIfDoesNotExist) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    if (!dbExists(dbName, errObj)) {
        if ( failIfDoesNotExist ) {
            return errObj.addErrMsg(std::string("Can't drop db ")
                                    + dbName + ", it does not exist");
        }
        return true;
    }
    std::string sql = "DROP DATABASE " + dbName;
    if (!runQuery(sql, errObj)) {
        return _setErrorObject(errObj, "Problem executing: " + sql);
    }
    if ( getActiveDbName() == dbName ) {
        _config.dbName.clear();
    }
    return true;
}

bool 
SqlConnection::tableExists(std::string const& tableName, 
                           SqlErrorObject& errObj,
                           std::string const& dbName) {
    if (!_connected) if (!connectToDb(errObj)) return false;

    std::string _dbName;
    if ( ! dbName.empty() ) {
        _dbName = dbName;
    } else {
        _dbName = getActiveDbName();
        if (_dbName.empty() ) {
            return errObj.addErrMsg("Can't check if table exist, db not selected");
        }        
    }
    if (!dbExists(_dbName, errObj)) {
        return errObj.addErrMsg(std::string("Db ")+_dbName+" does not exist");
    }
    std::string sql = "SELECT COUNT(*) FROM information_schema.tables ";
    sql += "WHERE table_schema = '";
    sql += _dbName + "' AND table_name = '" + tableName + "'";
    SqlResults results;
    if (!runQuery(sql, results, errObj)) {
        return _setErrorObject(errObj, "Problem executing: " + sql);
    }
    std::string s;
    if ( !results.extractFirstValue(s, errObj) ) {
        return errObj.addErrMsg("Query " + sql + " did not return result");
    }
    return s[0] == '1';
}

bool 
SqlConnection::dropTable(std::string const& tableName,
                         SqlErrorObject& errObj,
                         bool failIfDoesNotExist,
                         std::string const& dbName) {
    if (!_connected) if (!connectToDb(errObj)) return false;
    if ( getActiveDbName().empty() ) {
        return errObj.addErrMsg("Can't drop table, db not selected");
    }

    std::string _dbName = (dbName == "" ? getActiveDbName() : dbName);
    if (!tableExists(tableName, errObj, _dbName)) {
        if (failIfDoesNotExist) {
            return errObj.addErrMsg(std::string("Can't drop table ")
                                    + tableName + " (does not exist)");
        }
        return true;
    }
    std::string sql = "DROP TABLE " + _dbName + "." + tableName;
    if (!runQuery(sql, errObj)) {
        return _setErrorObject(errObj, "Problem executing: " + sql);
    }
    return true;
}

bool
SqlConnection::listTables(std::vector<std::string>& v, 
                          SqlErrorObject& errObj,
                          std::string const& prefixed,
                          std::string const& dbName) {
    v.clear();
    if (!_connected) if (!connectToDb(errObj)) return false;
    if ( getActiveDbName().empty() ) {
        return errObj.addErrMsg("Can't list tables, db not selected. ");
    }

    std::string _dbName = (dbName == "" ? getActiveDbName() : dbName);
    if (!dbExists(_dbName, errObj)) {
        return errObj.addErrMsg("Can't list tables for db " + _dbName
                                + " because the database does not exist. ");
    }
    std::string sql = "SELECT table_name FROM information_schema.tables ";
    sql += "WHERE table_schema = '" + _dbName + "'";
    if (prefixed != "") {
        sql += " AND table_name LIKE '" + prefixed + "%'";
    }
    SqlResults results;
    if (!runQuery(sql, results, errObj)) {
        return _setErrorObject(errObj, "Problem executing: " + sql);
    }
    return results.extractFirstColumn(v, errObj);
}

////////////////////////////////////////////////////////////////////////
// private
////////////////////////////////////////////////////////////////////////

bool 
SqlConnection::_init(SqlErrorObject& errObj) {
    assert(_conn == NULL);
    _conn = mysql_init(NULL);
    if (_conn == NULL) {
        return _setErrorObject(errObj);
    }
    return true;
}

bool 
SqlConnection::_connect(SqlErrorObject& errObj) {
    assert(_conn != NULL);
    unsigned long clientFlag = CLIENT_MULTI_STATEMENTS;
    MYSQL* c = mysql_real_connect
        (_conn, 
         _config.socket.empty() ?_config.hostname.c_str() : 0, 
         _config.username.empty() ? 0 : _config.username.c_str(), 
         _config.password.empty() ? 0 : _config.password.c_str(), 
         _config.dbName.empty() ? 0 : _config.dbName.c_str(), 
         _config.port,
         _config.socket.empty() ? 0 : _config.socket.c_str(), 
         clientFlag);
    if(c == NULL) {
        return _setErrorObject(errObj, "Failed to connect");
    }
    _connected = true;
    return true;
}

bool 
SqlConnection::_setErrorObject(SqlErrorObject& errObj, 
                               std::string const& extraMsg) {
    errObj.setErrNo( mysql_errno(_conn) );
    errObj.addErrMsg( mysql_error(_conn) );
    if ( ! extraMsg.empty() ) {
        errObj.addErrMsg(extraMsg);
    }
    return false;
}
