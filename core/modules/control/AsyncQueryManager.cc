/*
 * LSST Data Management System
 * Copyright 2008-2013 LSST Corporation.
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
  * @file AsyncQueryManager.cc
  *
  * @brief AsyncQueryManager: Manages/dispatches individual chunk
  * queries, waits for their completions, collects results, and
  * invokes result merging. Initiates query squashing when faults are
  * detected.  "Async" refers to the use of asynchronous xrootd client
  * API, which required some state management and liberal use of
  * callbacks.
  *
  * @author Daniel L. Wang, SLAC
  */

#include <iostream>

#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "control/AsyncQueryManager.h"
#include "qdisp/ChunkQuery.h"
#include "log/Logger.h"
#include "qdisp/MessageStore.h"
#include "log/msgCode.h"
#include "merger/TableMerger.h"
#include "util/Timer.h"
#include "qproc/QuerySession.h"
#include "util/WorkQueue.h"
#include "xrdc/PacketIter.h"

#include "protolog/ProtoLog.h"
#include <omp.h> // used to benchmark protolog

// Part of a demonstration of hierarchical logging
#define TRACE() LOG("tracer.control.AsynchQueryManager", LOG_LVL_TRACE, "")

// Namespace modifiers
using boost::make_shared;

namespace lsst {
namespace qserv {
namespace master {

// Local Helpers --------------------------------------------------
namespace {

// TODO(smm): These should be created elsewhere, and the thread counts
//            should come from a configuration file.
static DynamicWorkQueue globalReadQueue(25, 5, 50, 0);
static DynamicWorkQueue globalWriteQueue(50, 2, 60, 0);

// Doctors the query path to specify the async path.
// Modifies the string in-place.
void doctorQueryPath(std::string& path) {
    std::string::size_type pos;
    std::string before("/query/");
    std::string after("/query2/");

    pos = path.find(before);
    if(pos != std::string::npos) {
        path.replace(pos, before.size(), after);
    } // Otherwise, don't doctor.
}

}

////////////////////////////////////////////////////////////
// AsyncQueryManager nested classes
////////////////////////////////////////////////////////////

class AsyncQueryManager::printQueryMapValue {
public:
    printQueryMapValue(std::ostream& os_) : os(os_) {}
    void operator()(QueryMap::value_type const& qv) {
        os << "Query with id=" << qv.first;
        os << ": ";
        if(qv.second.first) {
            os << qv.second.first->getDesc();
        } else {
            os << "(NULL)";
        }
        os << ", " << qv.second.second << std::endl;
    }
    std::ostream& os;
};

class AsyncQueryManager::squashQuery {
public:
    squashQuery(boost::mutex& mutex_, QueryMap& queries_)
        :mutex(mutex_), queries(queries_) {}
    void operator()(QueryMap::value_type const& qv) {
        boost::shared_ptr<ChunkQuery> cq = qv.second.first;
        if(!cq.get()) return;
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            QueryMap::iterator i = queries.find(qv.first);
            if(i != queries.end()) {
                cq = i->second.first; // Get the shared version.
                if(!cq.get()) {
                    //qv.second.first.reset(); // Erase ours too.
                    return;
                }
                //idInProgress = i->first;
            }
        }
        // Query may have been completed, and its memory freed,
        // but still exist briefly before it is deleted from the map.
        Timer t;
        t.start();
        cq->requestSquash();
        t.stop();
        LOGGER_INF << "qSquash " << t << std::endl;
    }
    boost::mutex& mutex;
    QueryMap& queries;
};

////////////////////////////////////////////////////////////
// AsyncQueryManager
////////////////////////////////////////////////////////////
int AsyncQueryManager::add(TransactionSpec const& t,
                           std::string const& resultName) {
    TRACE();
    LOGGER_DBG << "EXECUTING AsyncQueryManager::add(TransactionSpec, "
               << resultName << ")" << std::endl;



    /*************************************************
     * PROTOLOG DEMO CODE
     *************************************************/

    // basic demonstration of protolog
    LOG("root", LOG_LVL_INFO, "Hello from root logger!");
    LOG("qserv", LOG_LVL_INFO, "Hello from qserv logger!");
    const char* info_stuff = "important stuff";
    LOG_INFO("Here's some information: %s", info_stuff);
    int debug_stuff = 99;
    LOG_DEBUG("Here's some debugging: %d", debug_stuff);

    // Using logger object for better performance (e.g. no logger lookups).
    log4cxx::LoggerPtr logger = ProtoLog::getLogger("czar.control");
    LOG(logger, LOG_LVL_INFO, "Logging with logger object.");

    // The following is a simple example of logging contexts and
    // hierarchical logging
    LOG_INFO("About to demonstrate logging contexts.");
    int levels = 5;
    for (int i=0; i < levels; i++) {
        std::stringstream ss;
        ss << "loglevel_" << i;
        LOG_PUSHCTX(ss.str());
        LOG_DEBUG("debugging statement at level %d.", i);
    }
    for (int i=0; i < levels; i++) {
        LOG_POPCTX();
    }
    LOG_INFO("Finished with demonstration.");

    // Benchmark
    int iterations = 100;
    double start, stop;
    start = omp_get_wtime();
    for (int i=0; i < iterations; i++)
        LOG("root", LOG_LVL_INFO, "Hello from root logger!");
    stop = omp_get_wtime();
    LOG_INFO("LOG(\"root\", LOG_LVL_INFO, ...): stop - start = %f", (stop - start)/iterations);
    start = omp_get_wtime();
    for (int i=0; i < iterations; i++)
        LOG_INFO("Hello from default logger!");
    stop = omp_get_wtime();
    LOG_INFO("LOG_INFO(...): stop - start = %f", (stop - start)/iterations);
    start = omp_get_wtime();
    sleep(1);
    stop = omp_get_wtime();
    LOG_INFO("sleep(1): stop - start = %f", stop - start);
    
    /***********************************************
     * PROTOLOG DEMO END
     ***********************************************/



    int id = t.chunkId;
    // Use chunkId as id, and assume that it will be unique for the
    // AsyncQueryManager instance.
    if(id == -1) {
        id = _getNextId();
    }
    if(t.isNull() || _isExecFaulty) {
        // If empty spec or fault already detected, refuse to run.
        return -1;
    }
    TransactionSpec ts(t);

    doctorQueryPath(ts.path);
    QuerySpec qs(boost::make_shared<ChunkQuery>(ts, id, this),
                 resultName);
    {
        boost::lock_guard<boost::mutex> lock(_queriesMutex);
        _queries[id] = qs;
        ++_queryCount;
    }
    std::string msg = std::string("Query Added: url=") + ts.path + ", savePath=" + ts.savePath;
    getMessageStore()->addMessage(id, MSG_MGR_ADD, msg);
    LOGGER_INF << "Added query id=" << id << " url=" << ts.path
               << " with save " << ts.savePath << "\n";
    qs.first->run();
    return id;
}

void AsyncQueryManager::finalizeQuery(int id,
                                      XrdTransResult r,
                                      bool aborted) {
    TRACE();
    std::stringstream ss;
    Timer t1;
    t1.start();
    /// Finalize a query.
    /// Note that all parameters should be copies and not const references.
    /// We delete the ChunkQuery (the caller) here, so a ref would be invalid.
    std::string dumpFile;
    std::string tableName;
    int dumpSize;
    LOGGER_DBG << "finalizing. read=" << r.read << " and status is "
               << (aborted ? "ABORTED" : "okay") << std::endl;
    LOGGER_DBG << ((void*)this) << "Finalizing query (" << id << ")" << std::endl;
    if((!aborted) && (r.open >= 0) && (r.queryWrite >= 0)
       && (r.read >= 0)) {
        Timer t2;
        t2.start();
        boost::shared_ptr<PacketIter> resIter;
        { // Lock scope for reading
            boost::lock_guard<boost::mutex> lock(_queriesMutex);
            QuerySpec& s = _queries[id];
            // Set resIter equal to PacketIter associated with ChunkQuery.
            resIter = s.first->getResultIter();
            dumpFile = s.first->getSavePath();
            dumpSize = s.first->getSaveSize();
            tableName = s.second;
            //assert(r.localWrite == dumpSize); // not valid when using iter
            s.first.reset(); // clear out chunkquery.
        }
        // Lock-free merge
        if(resIter) {
            _addNewResult(id, resIter, tableName);
        } else {
            _addNewResult(id, dumpSize, dumpFile, tableName);
        }
        // Erase right before notifying.
        t2.stop();
        ss << id << " QmFinalizeMerge " << t2 << std::endl;
        getMessageStore()->addMessage(id, MSG_MERGED, "Results Merged.");
    } // end if
    else {
        Timer t2e;
        t2e.start();
        if(!aborted) {
            _isExecFaulty = true;
            LOGGER_INF << "Requesting squash " << id
                       << " because open=" << r.open
                       << " queryWrite=" << r.queryWrite
                       << " read=" << r.read << std::endl;
            _squashExecution();
            LOGGER_INF << " Skipped merge (read failed for id="
                       << id << ")" << std::endl;
        }
        t2e.stop();
        ss << id << " QmFinalizeError " << t2e << std::endl;
    }
    Timer t3;
    t3.start();
    {
        boost::lock_guard<boost::mutex> lock(_resultsMutex);
        _results.push_back(Result(id,r));
        if(aborted) ++_squashCount; // Borrow result mutex to protect counter.
        { // Lock again to erase.
            Timer t2e1;
            t2e1.start();
            boost::lock_guard<boost::mutex> lock(_queriesMutex);
            _queries.erase(id);
            if(_queries.empty()) _queriesEmpty.notify_all();
            t2e1.stop();
            ss << id << " QmFinalizeErase " << t2e1 << std::endl;
            getMessageStore()->addMessage(id, MSG_ERASED, "Query Resources Erased.");
        }
    }
    t3.stop();
    ss << id << " QmFinalizeResult " << t3 << std::endl;
    LOGGER_DBG << (void*)this << " Done finalizing query (" << id << ")" << std::endl;
    t1.stop();
    ss << id << " QmFinalize " << t1 << std::endl;
    LOGGER_INF << ss.str();
    getMessageStore()->addMessage(id, MSG_FINALIZED, "Query Finalized.");
}

// FIXME: With squashing, we should be able to return the result earlier.
// So, clients will call joinResult(), to get the result, and let a reaper
// thread call joinEverything, since that ensures that this object has
// ceased activity and can recycle resources.
// This is a performance optimization.
void AsyncQueryManager::joinEverything() {
    TRACE();
    boost::unique_lock<boost::mutex> lock(_queriesMutex);
    int lastCount = -1;
    int count;
    int moreDetailThreshold = 5;
    int complainCount = 0;
    _printState(LOG_STRM(Debug));
    while(!_queries.empty()) {
        count = _queries.size();
        if(count != lastCount) {
            LOGGER_INF << "Still " << count
                       << " in flight." << std::endl;
            count = lastCount;
            ++complainCount;
            if(complainCount > moreDetailThreshold) {
                _printState(LOG_STRM(Warning));
                complainCount = 0;
            }
        }
        _queriesEmpty.timed_wait(lock, boost::posix_time::seconds(5));
    }
    _merger->finalize();
    _merger.reset();
    LOGGER_INF << "Query finish. " << _queryCount << " dispatched." << std::endl;
}

void AsyncQueryManager::configureMerger(TableMergerConfig const& c) {
    TRACE();
    _merger = boost::make_shared<TableMerger>(c);
}

void AsyncQueryManager::configureMerger(MergeFixup const& m,
                                        std::string const& resultTable) {
    TRACE();
    // Can we configure the merger without involving settings
    // from the python layer? Historically, the Python layer was
    // needed to generate the merging SQL statements, but we are now
    // creating them without Python.
    std::string mysqlBin="obsolete";
    std::string dropMem;
    TableMergerConfig cfg(_resultDbDb,  // cfg result db
                          resultTable, // cfg resultname
                          m, // merge fixup obj
                          _resultDbUser, // result db credentials
                          _resultDbSocket, // result db credentials
                          mysqlBin,  // Obsolete
                          dropMem // cfg
        );
    _merger = boost::make_shared<TableMerger>(cfg);
}

std::string AsyncQueryManager::getMergeResultName() const {
    TRACE();
    if(_merger.get()) {
        return _merger->getTargetTable();
    }
    return std::string();
}

void AsyncQueryManager::addToReadQueue(DynamicWorkQueue::Callable * callable) {
    TRACE();
    globalReadQueue.add(this, callable);
}

void AsyncQueryManager::addToWriteQueue(DynamicWorkQueue::Callable * callable) {
    TRACE();
    globalWriteQueue.add(this, callable);
}

boost::shared_ptr<MessageStore> AsyncQueryManager::getMessageStore() {
    if (!_messageStore) {
        // Lazy instantiation of MessageStore.
        _messageStore = boost::make_shared<MessageStore>();
    }
    return _messageStore;
}

////////////////////////////////////////////////////////////////////////
// private: ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

inline int coerceInt(std::string const& s, int defaultValue) {
    try {
        std::istringstream ss(s);
        int output;
        ss >> output;
        return output;
    } catch (...) {
        return defaultValue;
    }
}
inline std::string getConfigElement(std::map<std::string,
                                             std::string> const& cfg,
                                    std::string const& key,
                                    std::string const& errorMsg,
                                    std::string const& defaultValue) {
    TRACE();
    std::map<std::string,std::string>::const_iterator i = cfg.find(key);
    if(i != cfg.end()) {
        return i->second;
    } else {
        LOGGER_ERR << errorMsg << std::endl;
        return defaultValue;
    }
}

void AsyncQueryManager::_readConfig(std::map<std::string,
                                    std::string> const& cfg) {
    TRACE();
    /// localhost:1094 is the most reasonable default, even though it is
    /// the wrong choice for all but small developer installations.
    _xrootdHostPort = getConfigElement(
        cfg, "frontend.xrootd",
        "WARNING! No xrootd spec. Using localhost:1094",
        "localhost:1094");
    _scratchPath =  getConfigElement(
        cfg, "frontend.scratch_path",
        "Error, no scratch path found. Using /tmp.",
        "/tmp");
    // This should be overriden by the installer properly.
    _resultDbSocket =  getConfigElement(
        cfg, "resultdb.unix_socket",
        "Error, resultdb.unix_socket not found. Using /u1/local/mysql.sock.",
        "/u1/local/mysql.sock");
    _resultDbUser =  getConfigElement(
        cfg, "resultdb.user",
        "Error, resultdb.user not found. Using qsmaster.",
        "qsmaster");
    _resultDbDb =  getConfigElement(
        cfg, "resultdb.db",
        "Error, resultdb.db not found. Using qservResult.",
        "qservResult");
    std::string metaStr =  getConfigElement(
        cfg, "runtime.metaCacheSession",
        "No runtime.metaCacheSession. using default.",
        "");
    int metaCacheSession = coerceInt(metaStr, -1);
    // Setup session
    _qSession.reset(new QuerySession(metaCacheSession));
}

void AsyncQueryManager::_addNewResult(int id, PacIterPtr pacIter,
                                      std::string const& tableName) {
    TRACE();
    LOGGER_DBG << "EXECUTING AsyncQueryManager::_addNewResult(" << id
               << ", pacIter, " << tableName << ")" << std::endl;
    bool mergeResult = _merger->merge(pacIter, tableName);
    ssize_t sz = pacIter->getTotalSize();
    {
        boost::lock_guard<boost::mutex> lock(_totalSizeMutex);
        _totalSize += sz;
    }

    if(_shouldLimitResult && (_totalSize > _resultLimit)) {
        _squashRemaining();
    }
    if(!mergeResult) {
        TableMergerError e = _merger->getError();
        getMessageStore()->addMessage(id, e.errorCode != 0 ? -abs(e.errorCode) : -1,
                                      "Failed to merge results.");
        if(e.resultTooBig()) {
            _squashRemaining();
        }
    }
}

void AsyncQueryManager::_addNewResult(int id, ssize_t dumpSize,
                                      std::string const& dumpFile,
                                      std::string const& tableName) {
    TRACE();
    if(dumpSize < 0) {
        throw std::invalid_argument("dumpSize < 0");
    }
    {
        boost::lock_guard<boost::mutex> lock(_totalSizeMutex);
        _totalSize += dumpSize;
    }

    if(_shouldLimitResult && (_totalSize > _resultLimit)) {
        _squashRemaining();
    }

    if(dumpSize > 0) {
        bool mergeResult = _merger->merge(dumpFile, tableName);
        int res = unlink(dumpFile.c_str()); // Hurry and delete dump file.
        if(0 != res) {
            LOGGER_ERR << "Error removing dumpFile " << dumpFile
                       << " errno=" << errno << std::endl;
        }
        if(!mergeResult) {
            TableMergerError e = _merger->getError();
            getMessageStore()->addMessage(id, e.errorCode != 0 ? -abs(e.errorCode) : -1,
                                          "Failed to merge results.");
            if(e.resultTooBig()) {
                _squashRemaining();
            }
        }
        LOGGER_DBG << "Merge of " << dumpFile << " into "
                   << tableName
                   << (mergeResult ? " OK----" : " FAIL====")
                   << std::endl;
    }
}

void AsyncQueryManager::_printState(std::ostream& os) {
    typedef std::map<int, boost::shared_ptr<ChunkQuery> > QueryMap;
    std::for_each(_queries.begin(), _queries.end(), printQueryMapValue(os));
}

void AsyncQueryManager::_squashExecution() {
    TRACE();
    // Halt new query dispatches and cancel the ones in flight.
    // This attempts to save on resources and latency, once a query
    // fault is detected.

    if(_isSquashed) return;
    _isSquashed = true; // Mark before acquiring lock--faster.
    LOGGER_DBG << "Squash requested by "<<(void*)this << std::endl;
    Timer t;
    // Squashing is dependent on network latency and remote worker
    // responsiveness, so make a copy so others don't have to wait.
    std::vector<std::pair<int, QuerySpec> > myQueries;
    {
        boost::unique_lock<boost::mutex> lock(_queriesMutex);
        t.start();
        myQueries.resize(_queries.size());
        LOGGER_INF << "AsyncQM squashExec copy " <<  std::endl;
        std::copy(_queries.begin(), _queries.end(), myQueries.begin());
    }
    LOGGER_INF << "AsyncQM squashQueued" << std::endl;
    globalWriteQueue.cancelQueued(this);
    LOGGER_INF << "AsyncQM squashExec iteration " <<  std::endl;
    std::for_each(myQueries.begin(), myQueries.end(),
                  squashQuery(_queriesMutex, _queries));
    t.stop();
    LOGGER_INF << "AsyncQM squashExec " << t << std::endl;
    _isSquashed = true; // Ensure that flag wasn't trampled.

    getMessageStore()->addMessage(-1, MSG_EXEC_SQUASHED, "Query Execution Squashed.");
}

void AsyncQueryManager::_squashRemaining() {
    _squashExecution();  // Not sure if this is right. FIXME
}

}}} // namespace lsst::qserv::master

