// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2008-2015 AURA/LSST.
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

/// Includes Config class implementation and declarations of key
/// environment variables for qserv worker instances.

// Class header
#include "wconfig/Config.h"

// System headers
#include <cassert>
#include <cstddef>
#include <sstream>
#include <unistd.h>

// Qserv headers
#include "mysql/MySqlConfig.h"
#include "sql/SqlConnection.h"

using lsst::qserv::mysql::MySqlConfig;
using lsst::qserv::wconfig::Config;

namespace {
// Settings declaration ////////////////////////////////////////////////
static const int settingsCount = 16;
// key, env var name, default, description
static const char* settings[settingsCount][4] = {
    {"mysqlSocket", "QSW_DBSOCK", "/var/lib/mysql/mysql.sock",
     "MySQL socket file path for db connections"},
    {"mysqlDefaultUser", "QSW_DEFUSER", "qsmaster",
     "Default username for mysql connections"},
    {"numThreads", "QSW_NUMTHREADS", "4",
     "Number of in-flight query threads allowed."},
    {"QSW_MEMMAN", "QSW_MEMMAN", "Undef",
     "MemMan class to use for managing memory for tables"},
    {"QSW_MEMMAN_MB", "QSW_MEMMAN_MB", "900",
     "Memory available for locking tables"},
    {"QSW_MEMMAN_LOCATION", "QSW_MEMMAN_LOCATION", "/u1/qserv-run/var/lib/mysql",
     "Path to database tables"},
    {"QSW_THRDPOOLSZ", "QSW_THRDPOOLSZ", "15",
     "Thread pool size"},
    {"QSW_GROUPSZ", "QSW_GROUPSZ", "10",
     "Maximum group size for GroupScheduler"},
    {"QSW_PRIORITYSLOW", "QSW_PRIORITYSLOW", "1",
     "Slow scheduler priority"},
    {"QSW_PRIORITYMED", "QSW_PRIORITYMED", "1",
     "Medium scheduler priority"},
    {"QSW_PRIORITYFAST", "QSW_PRIORITYFAST", "1",
     "Fast scheduler priority"},
    {"QSW_RESERVESLOW", "QSW_RESERVESLOW", "2",
     "Maximum number of threads to reserve for slow scan"},
    {"QSW_RESERVEMED", "QSW_RESERVEMED", "2",
     "Maximum number of threads to reserve for medium scan"},
    {"QSW_RESERVEFAST", "QSW_RESERVEFAST", "2",
     "Maximum number of threads to reserve for fast scan"}
};


// Validator code /////////////////////////////////////////////////////
std::string validateMysql(Config const& c) {
    // Check config
    MySqlConfig sc;
    sc.hostname = "invalidhostname_unresolved";
    sc.username = c.getString("mysqlDefaultUser");
    sc.password = "";
    sc.dbName = c.getString("scratchDb");
    sc.port = 9999;
    sc.socket = c.getString("mysqlSocket");
    if (!sc.isValid()) return "Invalid MySQL config:" + sc.asString();

    { // Check connection
        lsst::qserv::sql::SqlConnection scn(sc);
        lsst::qserv::sql::SqlErrorObject eo;
        if (!scn.connectToDb(eo)) {
            return "Unable to connect to MySQL with config:" + sc.asString();
        }
    }
    return std::string(); // All checks passed.
}
} // anonymous namespace


namespace lsst {
namespace qserv {
namespace wconfig {

////////////////////////////////////////////////////////////////////////
// class Config
////////////////////////////////////////////////////////////////////////
Config::Config() {
    _load();
    _validate();
}

int Config::getInt(std::string const& key, int defVal) const {
    int ret = defVal;
    StringMap::const_iterator i = _map.find(key);
    if (i == _map.end()) {
        return defVal;
    }
    // coerce the string to int.
    std::stringstream s(i->second);
    s >> ret;
    return ret;
}

std::string const& Config::getString(std::string const& key) const {
    static const std::string n;
    StringMap::const_iterator i = _map.find(key);
    if (i == _map.end()) {
        return n;
    }
    return i->second;
}

MySqlConfig const& Config::getSqlConfig() const {
    assert(_sqlConfig.get());
    return *_sqlConfig;
}
////////////////////////////////////////////////////////////////////////
// class Config private
////////////////////////////////////////////////////////////////////////
char const* Config::_getEnvDefault(char const* varName,
                                   char const* defVal) {
    char const* s = ::getenv(varName);
    if (s != nullptr) {
        return s;
    } else {
        return defVal;
    }
}

void Config::_load() {
    // assume we're thread-protected.
    for(int i = 0; i < settingsCount; ++i) {
        _map[settings[i][0]] = _getEnvDefault(settings[i][1], settings[i][2]);
    }
    _sqlConfig = std::make_shared<MySqlConfig>();
    MySqlConfig& sc = *_sqlConfig;
    sc.hostname = "";
    sc.username = "qsmaster"; /// Empty default for now.
                              /// Consider "qworker" or "qsw"
    sc.password = "";
    // Sanity checks require default db, even for queries that don't use it.
    sc.dbName = "mysql";
    sc.port = 0;
    sc.socket = getString("mysqlSocket").c_str();
}

void Config::_validate() {
    // assume we're thread-protected
    _error = validateMysql(*this);
    _isValid = _error.empty();
}

////////////////////////////////////////////////////////////////////////
Config& getConfig() {
    static Config c;
    return c;
}

}}} // namespace lsst::qserv::wconfig
