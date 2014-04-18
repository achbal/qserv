// -*- LSST-C++ -*-
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
#ifndef LSST_QSERV_MASTER_TABLEALIAS_H
#define LSST_QSERV_MASTER_TABLEALIAS_H
/**
  * @file TableAlias.h
  *
  * @brief DbTablePair, TableAlias, and TableAliasReverse declarations.
  *
  * @author Daniel L. Wang, SLAC
  */
#include <sstream>

namespace lsst {
namespace qserv {
namespace master {

struct DbTablePair {
    DbTablePair(std::string const& db_, std::string const& table_)
        : db(db_), table(table_) {}
    DbTablePair() {}
    bool empty() const { return db.empty() && table.empty(); }
    inline bool lessThan(DbTablePair const& b) const {
        if(db < b.db) return true;
        else if(db == b.db) { return table < b.table; }
        else return false;
    }
    std::string db;
    std::string table;
};

/// TableAlias is a mapping from an alias to a (db, table)
/// name. TableAlias is a forward mapping, and TableAliasReverse is a
/// backwards mapping.
class TableAlias {
public:
    typedef std::map<std::string, DbTablePair> Map;

    DbTablePair get(std::string const& alias) {
        Map::const_iterator i = _map.find(alias);
        if(i != _map.end()) { return i->second; }
        return DbTablePair();
    }
    void set(std::string const& db, std::string const& table,
             std::string const& alias) {
        _map[alias] = DbTablePair(db, table);
    }
private:
    Map _map;
};

/// Stores a reverse alias mapping:  (db,table) -> alias
class TableAliasReverse {
public:
    std::string const& get(std::string db, std::string table) {
        return get(DbTablePair(db, table));
    }
    inline std::string const& get(DbTablePair const& p) {
        std::string const& val = _map[p]; // Empty entries inserted here.
        // Try slow lookup for inexact search
        if(val.empty() && p.db.empty()) {
            // FIXME: no precedence handling for entries.
            typedef Map::const_iterator Iter;
            for(Iter i=_map.begin(), e=_map.end(); i != e; ++i) {
                if((i->first.table == p.table)
                   && !i->second.empty()) {
                    return i->second;
                }
            }
        }
        return val;
    }
    void set(std::string const& db, std::string const& table,
             std::string const& alias) {
        _map[DbTablePair(db, table)] = alias;
    }
private:
    struct pairLess {
    inline bool operator()(DbTablePair const& a, DbTablePair const& b) {
            return a.lessThan(b);
        }
    };
    typedef std::map<DbTablePair, std::string, pairLess> Map;
    Map _map;
};

}}} // namespace lsst::qserv::master

#endif // LSST_QSERV_MASTER_TABLEALIAS_H
