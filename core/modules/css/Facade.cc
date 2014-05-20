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

/**
  * @file
  *
  * @brief A facade to the Central State System used by all Qserv core modules.
  *
  * @Author Jacek Becla, SLAC
  *
  */

#include "css/Facade.h"

// System headers
#include <fstream>
#include <iostream>

// Third-party headers
#include <boost/lexical_cast.hpp>

// Local headers
#include "css/CssException.h"
#include "css/KvInterfaceImplMem.h"
#include "css/KvInterfaceImplZoo.h"
#include "log/Logger.h"

using std::endl;
using std::map;
using std::string;
using std::vector;

namespace lsst {
namespace qserv {
namespace css {

/**
  * Initialize the Facade, the Facade will use Zookeeper-based interface, this is
  * for production use.
  *
  * @param connInfo connection information in a form supported by Zookeeper:
  *                 comma separated host:port pairs, each corresponding to
  *                 a Zookeeper server.
  */
Facade::Facade(string const& connInfo) {
    _kvI = new KvInterfaceImplZoo(connInfo);
}

/**
  * Initialize the Facade, the Facade will use Zookeeper-based interface, but will
  * place all data in some non-standard location, use this constructor for testing.
  *
  * @param connInfo connection information in a form supported by Zookeeper:
  *                 comma separated host:port pairs, each corresponding to
  *                 a Zookeeper server.
  * @param prefix, for testing, to avoid polluting production setup
  */
Facade::Facade(string const& connInfo, string const& prefix) :
    _prefix(prefix) {
    _kvI = new KvInterfaceImplZoo(connInfo);
}

/**
  * Initialize the Facade with dummy interface, use this constructor for testing.
  *
  * @param mapPath path to the map dumped using ./admin/bin/qserv-admin.py
  * @param isMap   unusued argument to differentiate between different c'tors
  */
Facade::Facade(std::istream& mapStream) {
    _kvI = new KvInterfaceImplMem(mapStream);
}

Facade::~Facade() {
    delete _kvI;
}

/** Checks if a given database is registered in the qserv metadata.
  *
  * @param dbName database name
  *
  * @return returns if given database is registered.
  */
bool
Facade::containsDb(string const& dbName) const {
    if (dbName.empty()) {
        LOGGER_INF << "Empty database name passed." << endl;
        throw CssException_DbDoesNotExist("<empty>");
    }
    string p = _prefix + "/DBS/" + dbName;
    bool ret =  _kvI->exists(p);
    LOGGER_INF << "*** containsDb(" << dbName << "): " << ret << endl;
    return ret;
}

/** Checks if a given table is registered in the qserv metadata. Throws exception
  * if the database does not exist.
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return returns if the database contains the table.
  */
bool
Facade::containsTable(string const& dbName, string const& tableName) const {
    LOGGER_INF << "*** containsTable(" << dbName << ", " << tableName
               << ")" << endl;
    _throwIfNotDbExists(dbName);
    return _containsTable(dbName, tableName);
}

/** Checks if a given table is chunked. Throws exception if a given database and/or
  * table does not exist.
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return returns true if the table is chunked.
  */
bool
Facade::tableIsChunked(string const& dbName, string const& tableName) const {
    LOGGER_INF << "tableIsChunked " << dbName << " " << tableName << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    return _tableIsChunked(dbName, tableName);
}

/** Checks if a given table is subchunked
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return returns true if the table is subchunked.
  */
bool
Facade::tableIsSubChunked(string const& dbName,
                          string const& tableName) const {
    LOGGER_INF << "tableIsSubChunked(" << dbName << ", " << tableName << ")" << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    return _tableIsSubChunked(dbName, tableName);
}

/** Returns true if the given table is a match table; that is, if it is a
  * table that relates two director tables.
  */
bool
Facade::isMatchTable(std::string const& dbName,
                     std::string const& tableName) const {
    LOGGER_INF << "isMatchTable(" << dbName << ", " << tableName << ")" << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    string k = _prefix + "/DBS/" + dbName + "/TABLES/" + tableName + "/match";
    string v = _kvI->get(k, "0");
    bool m = (v == "1");
    LOGGER_INF << "*** " << dbName << "." << tableName << " is "
               << (m ? "" : "not ") << " a match table.";
    return m;
}

/** Gets allowed databases (database that are configured for qserv)
  *
  * @return returns a vector of database names that are configured for qserv
  */
vector<string>
Facade::getAllowedDbs() const {
    string p = _prefix + "/DBS";
    return _kvI->getChildren(p);
}

/** Gets chunked tables
  *
  * @param dbName database name
  *
  * @return Returns a vector of table names that are chunked.
  */
vector<string>
Facade::getChunkedTables(string const& dbName) const {
    LOGGER_INF << "*** getChunkedTables(" << dbName << ")" << endl;
    _throwIfNotDbExists(dbName);
    string p = _prefix + "/DBS/" + dbName + "/TABLES";
    vector<string> ret, v = _kvI->getChildren(p);
    vector<string>::const_iterator itr;
    for (itr = v.begin() ; itr != v.end(); ++itr) {
        if (tableIsChunked(dbName, *itr)) {
            LOGGER_INF << "*** getChunkedTables: " << *itr << endl;
            ret.push_back(*itr);
        }
    }
    return ret;
}

/** Gets subchunked tables
  *
  * @param dbName database name
  *
  * @return Returns a vector of table names that are subchunked.
  */
vector<string>
Facade::getSubChunkedTables(string const& dbName) const {
    LOGGER_INF << "*** getSubChunkedTables(" << dbName << ")" << endl;
    _throwIfNotDbExists(dbName);
    string p = _prefix + "/DBS/" + dbName + "/TABLES";
    vector<string> ret, v = _kvI->getChildren(p);
    vector<string>::const_iterator itr;
    for (itr = v.begin() ; itr != v.end(); ++itr) {
        if (_tableIsSubChunked(dbName, *itr)) {
            LOGGER_INF << "*** getSubChunkedTables: " << *itr << endl;
            ret.push_back(*itr);
        }
    }
    return ret;
}

/** Gets names of partition columns (ra, decl, objectId) for a given database/table.
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return Returns a 3-element vector with column names for the lon, lat,
  *         and secIndex column names (e.g. [ra, decl, objectId]).
  *         or empty string(s) for columns that do not exist.
  */
vector<string>
Facade::getPartitionCols(string const& dbName, string const& tableName) const {
    LOGGER_INF << "*** getPartitionCols(" << dbName << ", " << tableName << ")"
               << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    string p = _prefix + "/DBS/" + dbName + "/TABLES/" +
               tableName + "/partitioning/";
    vector<string> v, ret;
    v.push_back("lonColName");
    v.push_back("latColName");
    v.push_back("secIndexColName");
    vector<string>::const_iterator itr;
    for (itr = v.begin() ; itr != v.end(); ++itr) {
        string p1 = p + *itr;
        string s = _kvI->get(p1, "");
        ret.push_back(s);
    }
    LOGGER_INF << "*** getPartitionCols: " << v[0] << ", " << v[1] << ", "
               << v[2] << endl;
    return ret;
}

/** Gets chunking level for a particular database.table.
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return Returns 0 if not partitioned, 1 if chunked, 2 if subchunked.
  */
int
Facade::getChunkLevel(string const& dbName, string const& tableName) const {
    LOGGER_INF << "getChunkLevel(" << dbName << ", " << tableName << ")" << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    bool isChunked = _tableIsChunked(dbName, tableName);
    bool isSubChunked = _tableIsSubChunked(dbName, tableName);
    if (isSubChunked) {
        LOGGER_INF << "getChunkLevel returns 2" << endl;
        return 2;
    }
    if (isChunked) {
        LOGGER_INF << "getChunkLevel returns 1" << endl;
        return 1;
    }
        LOGGER_INF << "getChunkLevel returns 0" << endl;
    return 0;
}

/** Retrieve the key column for a database. Throws exception if the database
  * or table does not exist.
  *
  * @param db database name
  * @param table table name
  *
  * @return the name of the partitioning key column, or an empty string if
  * there is no key column.
  */
string
Facade::getKeyColumn(string const& dbName, string const& tableName) const {
    LOGGER_INF << "*** Facade::getKeyColumn(" << dbName << ", " << tableName
               << ")" << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    string ret, p = _prefix + "/DBS/" + dbName + "/TABLES/" + tableName +
                    "/partitioning/secIndexColName";
    ret = _kvI->get(p, "");
    LOGGER_INF << "Facade::getKeyColumn, returning: " << ret << endl;
    return ret;
}

/** Retrieve # stripes and subStripes for a database. Throws exception if
  * the database does not exist. Returns (0,0) for non-partitioned databases.
  *
  * @param db database name
  */
StripingParams
Facade::getDbStriping(string const& dbName) const {
    LOGGER_INF << "*** getDbStriping(" << dbName << ")" << endl;
    _throwIfNotDbExists(dbName);
    StripingParams striping;
    string v = _kvI->get(_prefix + "/DBS/" + dbName + "/partitioningId", "");
    if (v == "") {
        return striping;
    }
    string p = _prefix + "/PARTITIONING/_" + v + "/";
    striping.stripes = _getIntValue(p+"nStripes", 0);
    striping.subStripes = _getIntValue(p+"nSubStripes", 0);
    return striping;
}

/** Retrieve match-table specific metadata for a table. Throws an
  * exception if the database or table does not exist, and returns
  * a MatchTableParams containing only empty strings if the given
  * table is not a match table.
  */
MatchTableParams
Facade::getMatchTableParams(std::string const& dbName,
                            std::string const& tableName) const {
    LOGGER_INF << "*** getMatchTableParams(" << dbName << ", "
               << tableName << ")" << endl;
    _throwIfNotDbTbExists(dbName, tableName);
    MatchTableParams p;
    string k = _prefix + "/DBS/" + dbName + "/TABLES/" + tableName + "/match";
    string v = _kvI->get(k, "0");
    if (v == "1") {
        try {
            p.dirTable1 = _kvI->get(k + "/dirTable1");
            p.dirColName1 = _kvI->get(k + "/dirColName1");
            p.dirTable2 = _kvI->get(k + "/dirTable2");
            p.dirColName2 = _kvI->get(k + "/dirColName2");
            p.flagColName = _kvI->get(k + "/flagColName");
        } catch (CssException_KeyDoesNotExist & ex) {
            throw CssException_InternalRunTimeError(
                string("Invalid match-table metadata for table ") +
                dbName + "." + tableName);
        }
    }
    return p;
}

int
Facade::_getIntValue(string const& key, int defaultValue) const {
    string s = boost::lexical_cast<string>(defaultValue);
    string v = _kvI->get(key, s);
    return v == s ? defaultValue : boost::lexical_cast<int>(v);
}

/** Validates if database exists. Throw exception if it does not.
  */
void
Facade::_throwIfNotDbExists(string const& dbName) const {
    if (!containsDb(dbName)) {
        LOGGER_INF << "Db '" << dbName << "' not found." << endl;
        throw CssException_DbDoesNotExist(dbName);
    }
}

/** Validates if table exists. Throw exception if it does not.
    Does not check if the database exists.
  */
void
Facade::_throwIfNotTbExists(string const& dbName, string const& tableName) const {
    if (!containsTable(dbName, tableName)) {
        LOGGER_INF << "table " << dbName << "." << tableName << " not found"
                   << endl;
        throw CssException_TableDoesNotExist(dbName+"."+tableName);
    }
}

/** Validate if database and table exist. Throw exception if either of them
    does not.
  */
void
Facade::_throwIfNotDbTbExists(string const& dbName, string const& tableName) const {
    _throwIfNotDbExists(dbName);
    _throwIfNotTbExists(dbName, tableName);
}

/** Checks if a given database contains a given table. Does not check if the
    database exists.
  */
bool
Facade::_containsTable(string const& dbName, string const& tableName) const {
    string p = _prefix + "/DBS/" + dbName + "/TABLES/" + tableName;
    bool ret = _kvI->exists(p);
    LOGGER_INF << "*** containsTable returns: " << ret << endl;
    return ret;
}

/** Checks if a given table is chunked. Does not check if database and/or table
    exist.
  *
  * @param dbName database name
  * @param tableName table name
  *
  * @return returns true if the table is chunked, false otherwise.
  */
bool
Facade::_tableIsChunked(string const& dbName, string const& tableName) const{
    string p = _prefix + "/DBS/" + dbName + "/TABLES/" +
               tableName + "/partitioning";
    bool ret = _kvI->exists(p);
    LOGGER_INF << "*** " << dbName << "." << tableName << " is ";
    if (!ret) LOGGER_INF << "NOT ";
    LOGGER_INF << "chunked." << endl;
    return ret;
}

/** Returns true if the given table is subchunked.
  * Does not check for database or table existence.
  */
bool
Facade::_tableIsSubChunked(string const& dbName,
                           string const& tableName) const {
    string p = _prefix + "/DBS/" + dbName + "/TABLES/" +
               tableName + "/partitioning/" + "subChunks";
    string retS = _kvI->get(p, "0");
    bool retV = (retS == "1");
    LOGGER_INF << "*** " << dbName << "." << tableName << " is "
               << (retV ? "" : "NOT ") << "subchunked.";
    return retV;
}

boost::shared_ptr<Facade>
FacadeFactory::createZooFacade(string const& connInfo) {
    boost::shared_ptr<css::Facade> cssFPtr(new css::Facade(connInfo));
    return cssFPtr;
}

boost::shared_ptr<Facade>
FacadeFactory::createMemFacade(string const& mapPath) {
    std::ifstream f(mapPath.c_str());
    if(f.fail()) {
        throw CssException_ConnFailure();
    }
    return FacadeFactory::createMemFacade(f);
}

boost::shared_ptr<Facade>
FacadeFactory::createMemFacade(std::istream& mapStream) {
    boost::shared_ptr<css::Facade> cssFPtr(new css::Facade(mapStream));
    return cssFPtr;
}

boost::shared_ptr<Facade>
FacadeFactory::createZooTestFacade(string const& connInfo, string const& prefix) {
    boost::shared_ptr<css::Facade> cssFPtr(new css::Facade(connInfo, prefix));
    return cssFPtr;
}

}}} // namespace lsst::qserv::css
