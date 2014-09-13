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
  * @brief ChunkResource implementation
  *
  * @author Daniel L. Wang, SLAC
  */

#include "wdb/ChunkResource.h"

// System headers
#include <iostream>

// Third-party headers
#include <boost/format.hpp>
#include <boost/thread.hpp>

// Local headers
#include "global/constants.h"
#include "sql/SqlConnection.h"
#include "sql/SqlErrorObject.h"
#include "wbase/Base.h"
#include "wdb/QuerySql.h"

namespace {
template <typename T>
class ScScriptBuilder {
public:
    ScScriptBuilder(lsst::qserv::wdb::QuerySql& qSql_,
                    std::string const& db, std::string const& table,
                    std::string const& scColumn,
                    int chunkId) : qSql(qSql_) {
        buildT = (boost::format(lsst::qserv::wbase::CREATE_SUBCHUNK_SCRIPT)
                  % db % table % scColumn
                  % chunkId % "%1%").str();
        cleanT = (boost::format(lsst::qserv::wbase::CLEANUP_SUBCHUNK_SCRIPT)
                  % db % table
                  % chunkId % "%1%").str();

    }
    void operator()(T const& subc) {
        qSql.buildList.push_back((boost::format(buildT) % subc).str());
        qSql.cleanupList.push_back((boost::format(cleanT) % subc).str());
    }
    std::string buildT;
    std::string cleanT;
    lsst::qserv::wdb::QuerySql& qSql;
};
} // anonymous namespace

namespace lsst {
namespace qserv {
namespace wdb {
////////////////////////////////////////////////////////////////////////
// ChunkResource
////////////////////////////////////////////////////////////////////////
class ChunkResource::Info {
public:
    Info(std::string const& db_, int chunkId_,
         StringVector const& tables_, IntVector const& subChunkIds_)
        : db(db_), chunkId(chunkId_),
          tables(tables_), subChunkIds(subChunkIds_) {}

    Info(std::string const& db_, int chunkId_,
         StringVector const& tables_)
        : db(db_), chunkId(chunkId_), tables(tables_) {}

    Info(Info const& i)
        : db(i.db), chunkId(i.chunkId),
          tables(i.tables), subChunkIds(i.subChunkIds) {}
    std::string db;
    int chunkId;
    StringVector tables;
    IntVector subChunkIds;
};
std::ostream& operator<<(std::ostream& os, ChunkResource::Info const& i) {
    os << "CrInfo(" << i.chunkId << "; ";
    std::copy(i.subChunkIds.begin(), i.subChunkIds.end(),
              std::ostream_iterator<int>(os, ","));
    os << ")";
    return os;
}
////////////////////////////////////////////////////////////////////////
// ChunkResource
////////////////////////////////////////////////////////////////////////
ChunkResource::ChunkResource(ChunkResourceMgr& mgr)
    : _mgr(mgr) {
}

ChunkResource::ChunkResource(ChunkResourceMgr& mgr,
                             ChunkResource::Info* info)
    : _mgr(mgr), _info(info) {
    _mgr.acquireUnit(*_info);
}
ChunkResource::ChunkResource(ChunkResource const& cr)
    : _mgr(cr._mgr), _info(new Info(*cr._info)) {
    _mgr.acquireUnit(*_info);
}

ChunkResource& ChunkResource::operator=(ChunkResource const& cr) {
    _mgr = cr._mgr;
    _info = std::auto_ptr<Info>(new Info(*cr._info));
    _mgr.acquireUnit(*_info);
    return *this;
}

ChunkResource::~ChunkResource() {
    if(_info.get()) {
        _mgr.release(*_info);
    }
}
std::string const& ChunkResource::getDb() const {
    return _info->db;
}

int ChunkResource::getChunkId() const {
    return _info->chunkId;
}

StringVector const& ChunkResource::getTables() const {
    return _info->tables;
}

IntVector const& ChunkResource::getSubChunkIds() const {
    return _info->subChunkIds;
}

struct ScTable {
    ScTable(std::string const& db_, int chunkId_,
            std::string const& table_, int subChunkId_)
        : db(db_), chunkId(chunkId_), table(table_), subChunkId(subChunkId_) {
    }

    std::string db;
    int chunkId;
    std::string table;
    int subChunkId;
};
std::ostream& operator<<(std::ostream& os, ScTable const& st) {
    return os << "Subchunks_" << st.db << "_" << st.chunkId << "."
              << st.table << "_" << st.subChunkId;
}
typedef std::vector<ScTable> ScTableVector;

class Backend {
public:
    typedef boost::shared_ptr<Backend> Ptr;
    void load(ScTableVector const& v) {
        if(_isFake) {
            std::cout << "Pretending to load:";
            std::copy(v.begin(), v.end(),
                      std::ostream_iterator<ScTable>(std::cout, ","));
            std::cout << std::endl;
        } else {
            for(ScTableVector::const_iterator i=v.begin(), e=v.end();
                i != e; ++i) {
                std::string create =
                    (boost::format(lsst::qserv::wbase::CREATE_SUBCHUNK_SCRIPT)
                     % i->db % i->table % SUB_CHUNK_COLUMN
                     % i->chunkId % i->subChunkId).str();
                sql::SqlErrorObject err;
                if(!_sqlConn.runQuery(create, err)) {
                    throw err; // FIXME
                }
            }
        }
    }
    void discard(ScTableVector const& v) {
        if(_isFake) {
            std::cout << "Pretending to discard:";
            std::copy(v.begin(), v.end(),
                      std::ostream_iterator<ScTable>(std::cout, ","));
            std::cout << std::endl;
        } else {
            for(ScTableVector::const_iterator i=v.begin(), e=v.end();
                i != e; ++i) {
                std::string discard =
                    (boost::format(lsst::qserv::wbase::CLEANUP_SUBCHUNK_SCRIPT)
                     % i->db % i->table
                     % i->chunkId % i->subChunkId).str();
                sql::SqlErrorObject err;
                if(!_sqlConn.runQuery(discard, err)) {
                    throw err; // FIXME
                }
            }
        }
    }
    static boost::shared_ptr<Backend>
    newInstance(mysql::MySqlConfig const& mc) {
        return boost::shared_ptr<Backend>(new Backend(mc));
    }
    static boost::shared_ptr<Backend>
    newFakeInstance() {
        return boost::shared_ptr<Backend>(new Backend('f'));
    }
private:
    /// Construct a fake instance
    Backend(char)
        : _isFake(true) {}
    Backend(mysql::MySqlConfig const& mc)
        : _isFake(false), _sqlConn(mc) {}

    bool _isFake;
    sql::SqlConnection _sqlConn;
};

// ChunkEntry
class ChunkEntry {
public:
    typedef std::map<int, int> SubChunkMap; // subchunkid -> count
    typedef std::map<std::string, SubChunkMap> TableMap; // tablename -> subchunk map

    typedef boost::shared_ptr<ChunkEntry> Ptr;

    ChunkEntry(int chunkId)
        : _chunkId(chunkId) {}

    void acquire(std::string const& db,
                 StringVector const& tables,
                 IntVector const& sc, Backend::Ptr backend) {
        ScTableVector needed;
        boost::lock_guard<boost::mutex> lock(_mutex);
        ++_refCount; // Increase usage count
        StringVector::const_iterator ti, te;
        for(ti=tables.begin(), te=tables.end(); ti != te; ++ti) {
            SubChunkMap& scm = _tableMap[*ti]; // implicit creation OK.
            IntVector::const_iterator i, e;
            for(i=sc.begin(), e=sc.end(); i != e; ++i) {
                SubChunkMap::iterator it = scm.find(*i);
                int last = 0;
                if(it == scm.end()) {
                    needed.push_back(ScTable(db, _chunkId, *ti, *i));
                } else {
                    last = it->second;
                }
                scm[*i] = last + 1; // write new value
            } // All subchunks
        } // All tables
        // For now, every other user of this chunk must wait while
        // we fetch the resource.
        if(needed.size() > 0) {
            backend->load(needed);
        }
    }
    void release(std::string const& db,
                 StringVector const& tables,
                 IntVector const& sc, Backend::Ptr backend) {
        {
            boost::lock_guard<boost::mutex> lock(_mutex);
            StringVector::const_iterator ti, te;
            for(ti=tables.begin(), te=tables.end(); ti != te; ++ti) {
                SubChunkMap& scm = _tableMap[*ti]; // Should be in there.
                IntVector::const_iterator i, e;
                for(i=sc.begin(), e=sc.end(); i != e; ++i) {
                    SubChunkMap::iterator it = scm.find(*i); // Should be there
                    if(it == scm.end()) {
                        throw std::runtime_error("Error releasing un-acquired resource");
                    }
                    scm[*i] = it->second - 1; // write new value
                } // All subchunks
            } // All tables
        }
        flush(db, backend); // Discard resources no longer needed by anyone.
        // flush could be detached from the release function, to be called at a
        // high-water mark and/or on periodic intervals
        --_refCount;
    }
    void flush(std::string const& db, Backend::Ptr backend) {
        ScTableVector discardable;

        boost::lock_guard<boost::mutex> lock(_mutex);
        TableMap::iterator ti, te;
        for(ti=_tableMap.begin(), te=_tableMap.end(); ti != te; ++ti) {
            IntVector mapDiscardable;
            SubChunkMap& scm = ti->second;
            SubChunkMap::iterator si, se;
            for(si=scm.begin(), se=scm.end(); si != se; ++si) {
                if(si->second == 0) {
                    discardable.push_back(ScTable(db, _chunkId,
                                                  ti->first,
                                                  si->second));
                    mapDiscardable.push_back(si->second);
                } else if(si->second < 0) {
                    throw std::runtime_error("Invalid negative use count when flushing subchunks");
                }
            } // All subchunks
            // Prune zero elements for this db+table+chunk
            // (invalidates iterators)
            IntVector::iterator di, de;
            for(di=mapDiscardable.begin(), de=mapDiscardable.end();
                di != de; ++di) {
                scm.erase(*di);
            }
        } // All tables
        // Delegate actual table dropping to the backend.
        if(discardable.size() > 0) {
            backend->discard(discardable);
        }
    }
private:
    boost::shared_ptr<Backend> _backend; //< Delegate stage/unstage
    int _chunkId;
    int _refCount; //< Number of known users
    TableMap _tableMap; //< tables in use
    boost::mutex _mutex;
};

////////////////////////////////////////////////////////////////////////
// ChunkResourceMgr::Impl
////////////////////////////////////////////////////////////////////////
class ChunkResourceMgr::Impl : public ChunkResourceMgr {
public:
    typedef std::map<int, ChunkEntry::Ptr> Map;
    typedef std::map<std::string, Map> DbMap;

    virtual ChunkResource acquire(std::string const& db, int chunkId,
                                  StringVector const& tables) {
        // Make sure that the chunk is ready. (NOP right now.)
        ChunkResource cr(*this, new ChunkResource::Info(db, chunkId,
                                                        tables));
        return cr;
    }
    virtual ChunkResource acquire(std::string const& db, int chunkId,
                                  StringVector const& tables,
                                  IntVector const& subChunks) {
        ChunkResource cr(*this, new ChunkResource::Info(db, chunkId,
                                                        tables, subChunks));

        return cr;
    }

    virtual void release(ChunkResource::Info const& i) {
        if(_isFake) {
            std::cout << "Releasing: " << i << std::endl;
        }
        {
            boost::lock_guard<boost::mutex> lock(_mapMutex);
            Map& map = _getMap(i.db);
            ChunkEntry& ce = _getChunkEntry(map, i.chunkId);
            ce.release(i.db, i.tables, i.subChunkIds, _backend);
        }
    }
    virtual void acquireUnit(ChunkResource::Info const& i) {
        if(_isFake) {
            std::cout << "Acquiring: " << i << std::endl;
        }
        {
            boost::lock_guard<boost::mutex> lock(_mapMutex);
            Map& map = _getMap(i.db); // Select db
            ChunkEntry& ce = _getChunkEntry(map, i.chunkId);
            // Actually acquire
            ce.acquire(i.db, i.tables, i.subChunkIds, _backend);
        }

    }

private:
    Impl(mysql::MySqlConfig const& c)
        : _isFake(false), _backend(Backend::newInstance(c)) {
    }
    Impl() : _isFake(true), _backend(Backend::newFakeInstance()) {}

    /// precondition: _mapMutex is held (locked by the caller)
    /// Get the ChunkEntry map for a db, creating if necessary
    Map& _getMap(std::string const& db) {
        DbMap::iterator it = _dbMap.find(db);
        if(it == _dbMap.end()) {
            DbMap::value_type v(db, Map());
            _dbMap.insert(v);
            it = _dbMap.find(db);
        }
        return it->second;
    }
    /// precondition: _mapMutex is held (locked by the caller)
    /// Get the ChunkEntry for a chunkId, creating if necessary
    ChunkEntry& _getChunkEntry(Map& m, int chunkId) {
        Map::iterator it = m.find(chunkId); // Select chunkId
        if(it == m.end()) { // Insert if not exist
            Map::value_type v(chunkId,
                              ChunkEntry::Ptr(new ChunkEntry(chunkId)));
            m.insert(v);
            return *(v.second.get());
        }
        return *(it->second.get());
    }

    friend class ChunkResourceMgr;
    bool _isFake; // Fake versions don't issue any sql queries.
    DbMap _dbMap;
    // Consider having separate mutexes for each db's map if contention becomes
    // a problem.
    boost::shared_ptr<Backend> _backend;
    boost::mutex _mapMutex; // Do not alter map without this mutex
};

////////////////////////////////////////////////////////////////////////
// ChunkResourceMgr
////////////////////////////////////////////////////////////////////////
boost::shared_ptr<ChunkResourceMgr>
ChunkResourceMgr::newMgr(mysql::MySqlConfig const& c) {
    return boost::shared_ptr<ChunkResourceMgr>(new Impl(c));
}

boost::shared_ptr<ChunkResourceMgr>
ChunkResourceMgr::newFakeMgr() {
    return boost::shared_ptr<ChunkResourceMgr>(new Impl());
}

}}} // namespace lsst::qserv::wdb
