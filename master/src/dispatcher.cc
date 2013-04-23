// -*- LSST-C++ -*-

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
// See also: dispatcher.h 
// Basic usage: 
//
// initDispatcher() // Set things up.
// newSession() // Init a new session
// setupQuery(int session, std::string const& query) // setup the session with a query. This triggers a parse.
// getSessionError(int session) // See if there are errors
// getConstraints(int session)  // Retrieve the detected constraints so that we can apply them to see which chunks we need. (done in Python)
// addChunk(int session, lsst::qserv::master::ChunkSpec const& cs ) // add the computed chunks to the query
// submitQuery3(int session)  // Trigger the dispatch of all chunk queries for the session.

#include "lsst/qserv/master/xrdfile.h"
#include "lsst/qserv/master/dispatcher.h"
#include "lsst/qserv/master/thread.h"
#include "lsst/qserv/master/xrootd.h"
#include "lsst/qserv/master/SessionManager.h"
#include "lsst/qserv/master/AsyncQueryManager.h"
#include "lsst/qserv/master/ChunkSpec.h"
#include "lsst/qserv/master/QuerySession.h"
#include "lsst/qserv/QservPath.hh"
#include "lsst/qserv/master/TaskMsgFactory2.h"
#include <sstream>

namespace qMaster = lsst::qserv::master;

using lsst::qserv::master::SessionManager;
typedef SessionManager<qMaster::AsyncQueryManager::Ptr> SessionMgr;
typedef boost::shared_ptr<SessionMgr> SessionMgrPtr;

namespace {

SessionMgr& getSessionManager() {
    // Singleton for now.
    static SessionMgrPtr sm;
    if(sm.get() == NULL) {
        sm = boost::make_shared<SessionMgr>();
        qMaster::initQuerySession();
    }
    assert(sm.get() != NULL);
    return *sm;
}

// Deprecated
qMaster::QueryManager& getManager(int session) {
    // Singleton for now. //
    static boost::shared_ptr<qMaster::QueryManager> qm;
    if(qm.get() == NULL) {
        qm = boost::make_shared<qMaster::QueryManager>();
    }
    return *qm;
}

qMaster::AsyncQueryManager& getAsyncManager(int session) {
    return *(getSessionManager().getSession(session));
}

std::string makeSavePath(std::string const& dir,
                         int sessionId,
                         int chunkId,
                         unsigned seq=0) {
    std::stringstream ss;
    ss << dir << "/" << sessionId << "_" << chunkId << "_" << seq;
    return ss.str();
}

class TmpTableName {
public:
    TmpTableName(int sessionId, std::string const& query) {
        std::stringstream ss;
        ss << "r_" << sessionId 
           << qMaster::hashQuery(query.data(), query.size())
           << "_";
        _prefix = ss.str();
    }
    std::string make(int chunkId, int seq=0) {
        std::stringstream ss;
        ss << _prefix << chunkId << "_" << seq;
        return ss.str();
    }
private:
    std::string _prefix;
};

} // anonymous namespace

void qMaster::initDispatcher() {
    xrdInit();
}

/// @param session Int for the session (the top-level query)
/// @param chunk Chunk number within this session (query)
/// @param str Query string (c-string)
/// @param len Query string length
/// @param savePath File path (with name) which will store the result (file, not dir)
/// @return a token identifying the session
int qMaster::submitQuery(int session, int chunk, char* str, int len, 
                         char* savePath, std::string const& resultName) {
    TransactionSpec t;
    AsyncQueryManager& qm = getAsyncManager(session);
    std::string const hp = qm.getXrootdHostPort();
    t.chunkId = chunk;
    t.query = std::string(str, len);
    t.bufferSize = 8192000;
    t.path = qMaster::makeUrl(hp.c_str(), "query", chunk);
    t.savePath = savePath;
    return submitQuery(session, TransactionSpec(t), resultName);
}

/// @param session Int for the session (the top-level query)
/// @param chunk Chunk number within this session (query)
/// @param str Query string (c-string)
/// @param len Query string length
/// @param savePath File path (with name) which will store the result (file, not dir)
/// @return a token identifying the session
int qMaster::submitQueryMsg(int session, char* dbName, int chunk,
                            char* str, int len, 
                            char* savePath, std::string const& resultName) {
    // Most dbName, chunk, resultName can be derived from msg, 
    // but we avoid unpacking it here.  Not sure how safe it is to
    // pass a python Protobuf msg through SWIG for use in c++.
    TransactionSpec t;
    AsyncQueryManager& qm = getAsyncManager(session);
    std::string const hp = qm.getXrootdHostPort();
    QservPath qp;
    qp.setAsCquery(dbName, chunk);
    std::string path=qp.path();
    t.chunkId = chunk;
    t.query = std::string(str, len);
    t.bufferSize = 8192000;
    t.path = qMaster::makeUrl(hp.c_str(), qp.path());
    t.savePath = savePath;
    return submitQuery(session, TransactionSpec(t), resultName);    
}

int qMaster::submitQuery(int session, qMaster::TransactionSpec const& s, 
                         std::string const& resultName) {
    int queryId = 0;
#if 1
    AsyncQueryManager& qm = getAsyncManager(session);
    qm.add(s, resultName); 
    //std::cout << "Dispatcher added  " << s.chunkId << std::endl;
#else
    QueryManager& qm = getManager(session);
    qm.add(s); 
#endif
    /* queryId = */ 
    return queryId;
}

qMaster::QueryState qMaster::joinQuery(int session, int id) {
    // Block until specific query id completes.
#if 1
    //AsyncQueryManager& qm = getAsyncManager(session);
#else
    //QueryManager& qm = getManager(session);
#endif
    //qm.join(id);
    //qm.status(id); // get status
    // If error, report
    return UNKNOWN; // FIXME: convert status to querystate.
}

qMaster::QueryState qMaster::tryJoinQuery(int session, int id) {
#if 0 // Not implemented yet
    // Just get the status and return it.
#if 1
    AsyncQueryManager& qm = getAsyncManager(session);
#else
    QueryManager& qm = getManager(session);
#endif
    if(qm.tryJoin(id)) {
        return SUCCESS; 
    } else {
        return ERROR;
    }   
#endif
    // FIXME...consider dropping this.
    // need return val.
    return UNKNOWN;
}

struct mergeStatus {
    mergeStatus(bool& success, bool shouldPrint_=false, int firstN_=5) 
        : isSuccessful(success), shouldPrint(shouldPrint_), 
          firstN(firstN_) {
        isSuccessful = true;
    }
    void operator() (qMaster::AsyncQueryManager::Result const& x) { 
        if(!x.second.isSuccessful()) {
            if(shouldPrint || (firstN > 0)) {
                std::cout << "Chunk " << x.first << " error " << std::endl
                          << "open: " << x.second.open 
                          << " qWrite: " << x.second.queryWrite 
                          << " read: " << x.second.read 
                          << " lWrite: " << x.second.localWrite << std::endl;
                --firstN;
            }
            isSuccessful = false;
        } else {
            if(shouldPrint) {
                std::cout << "Chunk " << x.first << " OK ("
                          << x.second.localWrite << ")\t";
            }
        }
    }
    bool& isSuccessful;
    bool shouldPrint;
    int firstN;
};

void qMaster::pauseReadTrans(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    qm.pauseReadTrans();
}

void qMaster::resumeReadTrans(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    qm.resumeReadTrans();
}

void qMaster::setupQuery(int session, std::string const& query,
                         std::string const& resultTable) {
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    qs.setResultTable(resultTable);
    qs.setQuery(query);

}


std::string const& qMaster::getSessionError(int session) {
    static const std::string empty;
    // TODO: Retrieve from QuerySession.
    return empty;
}

lsst::qserv::master::Constraint getC(int base) {
    // SWIG test.
    std::stringstream ss;
    qMaster::Constraint c;
    ss << "box" << base; c.name = ss.str(); ss.str("");
    ss << base << "1"; c.params.push_back(ss.str()); ss.str("");
    ss << base << "2"; c.params.push_back(ss.str()); ss.str("");
    ss << base << "3"; c.params.push_back(ss.str()); ss.str("");
    ss << base << "4"; c.params.push_back(ss.str()); ss.str("");
    return c; // SWIG test.
 }

lsst::qserv::master::ConstraintVec
lsst::qserv::master::getConstraints(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    return ConstraintVec(qs.getConstraints());
}

std::string const&
lsst::qserv::master::getDominantDb(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    return qs.getDominantDb();
}

void 
qMaster::addChunk(int session, lsst::qserv::master::ChunkSpec const& cs ) {
#if 1 // SWIG plumbing debug
    std::cout << "Received chunk=" << cs.chunkId << " ";
    typedef std::vector<int> Vect;
    int count=0;
    for(Vect::const_iterator i = cs.subChunks.begin(); 
        i != cs.subChunks.end(); ++i) {
        if(++count > 1) std::cout << ", ";
        std::cout << *i;
    }
    std::cout << std::endl;
#endif
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    qs.addChunk(cs);
}

/// Submit the query.
void 
qMaster::submitQuery3(int session) {
    // Using the QuerySession, generate query specs (text, db, chunkId) and then
    // create query messages and send them to the async query manager.
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    TaskMsgFactory2 f(session);


    std::string const hp = qm.getXrootdHostPort();
    TmpTableName ttn(session, qs.getOriginal());
    std::ostringstream ss;
    QuerySession::Iter i;
    QuerySession::Iter e = qs.cQueryEnd();
    for(i = qs.cQueryBegin(); i != e; ++i) {
        qMaster::ChunkQuerySpec& cs = *i;
        std::string chunkResultName = ttn.make(cs.chunkId);
        f.serializeMsg(cs, chunkResultName, ss);        
        
        TransactionSpec t;
        QservPath qp;
        qp.setAsCquery(cs.db, cs.chunkId);
        std::string path=qp.path();
        t.chunkId = cs.chunkId;
        t.query = ss.str();
        std::cout << "Msg cid=" << cs.chunkId << " with size=" 
                  << t.query.size() << std::endl;
        t.bufferSize = 8192000;
        t.path = qMaster::makeUrl(hp.c_str(), qp.path());
        t.savePath = makeSavePath(qm.getScratchPath(), session, cs.chunkId);
        ss.str(""); // reset stream
        submitQuery(session, t, chunkResultName);
    }
}

qMaster::QueryState qMaster::joinSession(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    qm.joinEverything();
    AsyncQueryManager::ResultDeque const& d = qm.getFinalState();
    bool successful;
    std::for_each(d.begin(), d.end(), mergeStatus(successful));
    
    if(successful) {
        std::cout << "Joined everything (success)" << std::endl;
        return SUCCESS;
    } else {
        std::cout << "Joined everything (failure!)" << std::endl;
        return ERROR;
    }
}

std::string const& 
qMaster::getQueryStateString(QueryState const& qs) {
    static const std::string unknown("unknown");
    static const std::string waiting("waiting");
    static const std::string dispatched("dispatched");
    static const std::string success("success");
    static const std::string error("error");
    switch(qs) {
    case UNKNOWN:
        return unknown;
    case WAITING:
        return waiting;
    case DISPATCHED:
        return dispatched;
    case SUCCESS:
        return success;
    case ERROR:
        return error;
    default:
        return unknown;
    }
}

std::string
qMaster::getErrorDesc(int session) {

    class _ErrMsgStr_ {
    public:
        _ErrMsgStr_(const std::string& name): _name(name) {}
            
        void add(int x) { 
            if (_ss.str().length() == 0) {
                _ss << _name << " failed for chunk(s):";
            }
            _ss << ' ' << x;
        }
        std::string toString() {
            return _ss.str();
        }
    private:
        const std::string _name;
        std::stringstream _ss;
    };    
    
    std::stringstream ss;
    AsyncQueryManager& qm = getAsyncManager(session);
    AsyncQueryManager::ResultDeque const& d = qm.getFinalState();
    AsyncQueryManager::ResultDequeCItr itr;
    _ErrMsgStr_ openV("open");
    _ErrMsgStr_ qwrtV("queryWrite");
    _ErrMsgStr_ readV("read");
    _ErrMsgStr_ lwrtV("localWrite");

    for (itr=d.begin() ; itr!=d.end(); ++itr) {
        if (itr->second.open <= 0) {
            openV.add(itr->first);
        } else if (itr->second.queryWrite <= 0) {
            qwrtV.add(itr->first);
        } else if (itr->second.read < 0) {
            readV.add(itr->first);
        } else if (itr->second.localWrite <= 0) {
            lwrtV.add(itr->first);
        }
    }
    // Handle open, write, read errors first. If we have 
    // any of these errors, we will get localWrite errors 
    // for every chunk, because we are not writing result, 
    // so don't bother reporting them.
    ss << openV.toString();
    ss << qwrtV.toString();
    ss << readV.toString();
    if (ss.str().empty()) {
        ss << lwrtV.toString();
    }
    return ss.str();
}

int qMaster::newSession(std::map<std::string,std::string> const& config) {
    AsyncQueryManager::Ptr m = 
        boost::make_shared<qMaster::AsyncQueryManager>(config);
    int id = getSessionManager().newSession(m);
    return id;
}

void qMaster::configureSessionMerger(int session, TableMergerConfig const& c) {
    getAsyncManager(session).configureMerger(c);    
}
void qMaster::configureSessionMerger3(int session) {
    AsyncQueryManager& qm = getAsyncManager(session);
    QuerySession& qs = qm.getQuerySession();
    std::string const& resultTable = qs.getResultTable();
    MergeFixup m = qs.makeMergeFixup();
    qm.configureMerger(m, resultTable);
}

std::string qMaster::getSessionResultName(int session) {
    return getAsyncManager(session).getMergeResultName();
}

void qMaster::discardSession(int session) {
    getSessionManager().discardSession(session);
    return; 
}

qMaster::XrdTransResult qMaster::getQueryResult(int session, int chunk) {
    return XrdTransResult();    // FIXME
}
