// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2014-2015 AURA/LSST.
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
  * @brief Executive. It executes things.
  *
  * TODO: Consider merging RequesterMap and EntryMap. Originally, RequesterMap
  * was separate from StatusMap to reduce contention when things are just
  * updating statuses, but if the contention is small, we can simplify by
  * combining all three (Requester, status, queryresource) into a single map.
  *
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "qdisp/Executive.h"

// Third-party headers
#include "boost/make_shared.hpp"

// System headers
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

// Third-party headers
#include "boost/format.hpp"
#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiService.hh" // Resource
#include "XrdOuc/XrdOucTrace.hh"

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "global/Bug.h"
#include "global/ResourceUnit.h"
#include "log/msgCode.h"
#include "qdisp/MessageStore.h"
#include "qdisp/QueryResource.h"
#include "qdisp/ResponseRequester.h"

namespace {
std::string getErrorText(XrdSsiErrInfo & e) {
    std::ostringstream os;
    int errCode;
    os << "XrdSsiError " << e.Get(errCode);
    os <<  " Code=" << errCode;
    return os.str();
}
void populateState(lsst::qserv::qdisp::ExecStatus& es,
                   lsst::qserv::qdisp::ExecStatus::State s,
                   XrdSsiErrInfo& e) {
    int code;
    std::string desc(e.Get(code));
    es.report(s, code, desc);
}
} // anonymous namespace

// Declare to allow force-on XrdSsi tracing
#define TRACE_ALL       0xffff
#define TRACE_Debug     0x0001
namespace XrdSsi {
extern XrdOucTrace     Trace;
}

namespace lsst {
namespace qserv {
namespace qdisp {

template <typename Ptr>
struct printMapSecond {
    printMapSecond(std::ostream& os_, std::string sep_)
        : os(os_), sep(sep_), first(true)  {}

    void operator()(Ptr const& p) {
        if(!first) {
            os << sep;
        }
        os << *(p.second);
        first = false;
    }
    std::ostream& os;
    std::string sep;
    bool first;
};

////////////////////////////////////////////////////////////////////////
// class Executive implementation
////////////////////////////////////////////////////////////////////////
Executive::Executive(Config::Ptr c, boost::shared_ptr<MessageStore> ms)
    : _config(*c),
      _messageStore(ms) {
    _setup();
}

void Executive::add(int refNum, TransactionSpec const& t,
                    std::string const& resultName) {
    throw Bug("Unsupported old transactions in Executive");
}

class Executive::DispatchAction : public util::VoidCallable<void> {
public:
    typedef boost::shared_ptr<DispatchAction> Ptr;
    DispatchAction(Executive& executive,
                   int refNum,
                   Executive::Spec const& spec,
                   Entry& entry)
        : _executive(executive), _refNum(refNum),
          _spec(spec), _entry(entry) {
    }
    virtual ~DispatchAction() {}
    virtual void operator()() {
        if(_spec.requester->reset()) { // Must be able to reset state
            _executive._dispatchQuery(_refNum, _spec, _entry);
        }
        // If the reset fails, do nothing-- can't retry.
    }
private:
    Executive& _executive;
    int _refNum;
    Spec _spec;
    Entry& _entry; ///< Reference back to exec Entry
};

class NotifyExecutive : public util::UnaryCallable<void, bool> {
public:
    typedef boost::shared_ptr<NotifyExecutive> Ptr;

    NotifyExecutive(qdisp::Executive& e, int refNum)
        : _executive(e), _refNum(refNum) {}

    virtual void operator()(bool success) {
        _executive.markCompleted(_refNum, success);
    }

    static Ptr newInstance(qdisp::Executive& e, int refNum) {
        return Ptr(new NotifyExecutive(e, refNum));
    }
private:
    qdisp::Executive& _executive;
    int _refNum;
};

/// Add a spec to be executed. Not thread-safe.
void Executive::add(int refNum, Executive::Spec const& s) {
    bool trackOk = _track(refNum, s.requester); // Remember so we can join
    if(!trackOk) {
        LOGF_WARN("Ignoring duplicate add(%1%)" % refNum);
        return;
    }
    //ExecStatus& status = _insertNewStatus(refNum, s.resource);
    Entry entry = _insertNewEntry(refNum, s.resource);
    //ExecStatus& status = *entry.status;

    ++_requestCount;
    std::string msg = "Exec add pth=" + s.resource.path();
    LOGF_INFO("%1%" % msg);
    _messageStore->addMessage(s.resource.chunk(), log::MSG_MGR_ADD, msg);

    _dispatchQuery(refNum,
                   s,
                   entry);
}

bool Executive::join() {
    // To join, we make sure that all of the chunks added so far are complete.
    // Check to see if _requesters is empty, if not, then sleep on a condition.
    _waitUntilEmpty();
    // Okay to merge. probably not the Executive's responsibility
    struct successF {
        // static bool f(Executive::StatusMap::value_type const& entry) {
        //     ExecStatus::Info const& esI = entry.second->getInfo();
        //     LOGF_INFO("entry state:%1% %2%)" % (void*)entry.second.get() % esI);
        //     return (esI.state == ExecStatus::RESPONSE_DONE)
        //         || (esI.state == ExecStatus::COMPLETE); }
        static bool f(Executive::EntryMap::value_type const& entry) {
            ExecStatus::Info const& esI = entry.second.status->getInfo();
            LOGF_INFO("entry state:%1% %2%)" % (void*)entry.second.status.get() % esI);
            return (esI.state == ExecStatus::RESPONSE_DONE)
                || (esI.state == ExecStatus::COMPLETE); }
    };

    //int sCount = std::count_if(_statuses.begin(), _statuses.end(), successF::f)
    int sCount = 0;
    {
        boost::lock_guard<boost::mutex> lock(_entriesMutex);
        sCount = std::count_if(_entries.begin(), _entries.end(), successF::f);
    }

    LOGF_INFO("Query exec finish. %1% dispatched." % _requestCount);
    _reportStatuses();
    if(sCount != _requestCount) {
        LOGF_INFO("Query exec error:. %1% != %2%" % _requestCount % sCount);
    }
    return sCount == _requestCount;
}

void Executive::markCompleted(int refNum, bool success) {
    ResponseRequester::Error e;
    LOGF_INFO("Executive::markCompletion(%1%,%2%)" % refNum % success);
    if(!success) {
        {
            boost::lock_guard<boost::mutex> lock(_requestersMutex);
            RequesterMap::iterator i = _requesters.find(refNum);
            if(i != _requesters.end()) {
                e = i->second->getError();
            } else {
                std::string msg =
                    (boost::format("Executive::markCompleted(%1%) "
                                   "failed to find tracked id=%2% size=%3%")
                     % (void*)this % refNum % _requesters.size()).str();
                throw Bug(msg);
            }
        }
        {
            boost::lock_guard<boost::mutex> lock(_entriesMutex);
            _entries[refNum].status->report(ExecStatus::RESULT_ERROR, 1);
        }
        //_statuses[refNum]->report(ExecStatus::RESULT_ERROR, 1);
        LOGF_ERROR("Executive: error executing refnum=%1%. Code=%2% %3%" %
                   refNum % e.code % e.msg);
    }
    _unTrack(refNum);
    if(!success) {
        LOGF_ERROR("Executive: requesting squash (cause refnum=%1% with code=%2% %3%)" % refNum % e.code % e.msg);
        squash(); // ask to squash
    }
}

void Executive::requestSquash(int refNum) {
    RequesterPtr toSquash;
    bool needToWarn = false;
    ResponseRequester::Error e;
    {
        boost::lock_guard<boost::mutex> lock(_requestersMutex);
        RequesterMap::iterator i = _requesters.find(refNum);
        if(i != _requesters.end()) {
            ResponseRequester::Error e = i->second->getError();
            if(e.code) {
                needToWarn = true;
            } else {
                toSquash = i->second; // Remember which one to squash
            }
        } else {
            throw Bug("Executive::requestSquash() with invalid refNum");
        }
    }
    if(needToWarn) {
        LOGF_WARN("Warning, requestSquash(%1%), but %2% has already failed (%3%, %4%)." % refNum % refNum % e.code % e.msg);
    }

    if(toSquash) { // Squash outside of the mutex
        toSquash->cancel();
    }
}

void Executive::squash() {
    LOGF_INFO("Trying to cancel all queries...");
    std::vector<RequesterPtr> pendingRequesters;
    std::ostringstream os;
    os << "STATE=";
    {
        boost::lock_guard<boost::mutex> lock(_requestersMutex);
        _printState(os);
        RequesterMap::iterator i,e;
        for(i=_requesters.begin(), e=_requesters.end(); i != e; ++i) {
            pendingRequesters.push_back(i->second);
        }
    }
    LOGF_INFO("%1%\nLoop cancel all queries..." % os.str());
    LOGF_INFO("Enqueued requesters for cancelling...done");
    {
        std::vector<RequesterPtr>::iterator i, e;
        for(i=pendingRequesters.begin(), e=pendingRequesters.end(); i != e; ++i) {
            // Could get stuck because it waits on xrootd,
            // which may be waiting on a thread blocked in _unTrack().
            // Don't do this while holding _requestersMutex
            (**i).cancel();
        }
        LOGF_INFO("Cancelled all query requesters...done");
    }
}

struct printMapEntry {
    printMapEntry(std::ostream& os_, std::string const& sep_)
        : os(os_), sep(sep_), first(true) {}
    void operator()(Executive::StatusMap::value_type const& entry) {
        if(!first) { os << sep; }
        os << "Ref=" << entry.first << " ";
        ExecStatus const& es = *entry.second;
        os << es;
        first = false;
    }
    void operator()(Executive::EntryMap::value_type const& entry) {
        if(!first) { os << sep; }
        os << "Ref=" << entry.first << " ";
        ExecStatus const& es = *entry.second.status;
        os << es;
        first = false;
    }
    std::ostream& os;
    std::string const& sep;
    bool first;
};

int Executive::getNumInflight() {
    boost::unique_lock<boost::mutex> lock(_requestersMutex);
    return _requesters.size();
}

std::string Executive::getProgressDesc() const {
    std::ostringstream os;
    //    std::for_each(_statuses.begin(), _statuses.end(), printMapEntry(os, "\n"));
    std::for_each(_entries.begin(), _entries.end(), printMapEntry(os, "\n"));
    LOGF_ERROR("%1%" % os.str());
    return os.str();
}

////////////////////////////////////////////////////////////////////////
// class Executive (private)
////////////////////////////////////////////////////////////////////////
void Executive::_dispatchQuery(int refNum,
                               Executive::Spec const& spec,
                               Entry& entry) {
    boost::shared_ptr<DispatchAction> retryFunc;
    if(_shouldRetry(refNum)) { // limit retries for each request.
        retryFunc.reset(new DispatchAction(*this, refNum,
                                           spec,
                                           entry));
    }
    QueryResource* r = new QueryResource(spec.resource.path(),
                                         spec.request,
                                         spec.requester,
                                         NotifyExecutive::newInstance(*this, refNum),
                                         retryFunc,
                                         *entry.status);
    entry.status->report(ExecStatus::PROVISION);
    bool provisionOk = _service->Provision(r);  // 2

    if(!provisionOk) {
        //lock.release();
        // handle error
        LOGF_ERROR("Resource provision error %1%" % spec.resource.path());
        populateState(*entry.status, ExecStatus::PROVISION_ERROR, r->eInfo);
        _unTrack(refNum);
        delete r;
        return;
    }
    entry.resource = r;

    LOGF_DEBUG("Provision was ok");
    // FIXME: For squashing, need to hold ptr to QueryResource, so we can
    // instruct it to call XrdSsiRequest::Finished(cancel=true). Also, can send
    // cancellation into requester.
}

void Executive::_setup() {
    XrdSsi::Trace.What = TRACE_ALL | TRACE_Debug;

    XrdSsiErrInfo eInfo;
    _service = XrdSsiGetClientService(eInfo, _config.serviceUrl.c_str()); // Step 1
    if(!_service) {
        LOGF_ERROR("Error obtaining XrdSsiService in Executive: "
                   %  getErrorText(eInfo));
    }
    assert(_service);
    _requestCount = 0;
}

/// Check to see if a requester should retry, based on how many retries have
/// been attempted. Increments the retry counter as a side effect.
bool Executive::_shouldRetry(int refNum) {
    const int MAX_RETRY = 5;
    boost::lock_guard<boost::mutex> lock(_retryMutex);
    IntIntMap::iterator i = _retryMap.find(refNum);
    if(i == _retryMap.end()) {
        _retryMap[refNum] = 1;
    } else if(i->second < MAX_RETRY) {
        _retryMap[refNum] = 1 + i->second;
    } else {
        return false;
    }
    return true;
}

ExecStatus& Executive::_insertNewStatus(int refNum,
                                        ResourceUnit const& r) {
    ExecStatus::Ptr es = boost::make_shared<ExecStatus>(r);
    _statuses.insert(StatusMap::value_type(refNum, es));
    return *es;
}

Executive::Entry Executive::_insertNewEntry(int refNum,
                                  ResourceUnit const& r) {
    // Thread-safe. Please don't hold _entriesMutex before entering
    boost::lock_guard<boost::mutex> lock(_entriesMutex);
    ExecStatus::Ptr es = boost::make_shared<ExecStatus>(r);
    Entry e = {es, NULL};
    _entries.insert(EntryMap::value_type(refNum, e));
    return e;
}

template <typename Map, typename Ptr>
bool trackHelper(void* caller, Map& m, typename Map::key_type key,
                 Ptr ptr,
                 boost::mutex& mutex) {
    assert(ptr);
    {
        LOGF_DEBUG("Executive (%1%) tracking id=%2%" % (void*)caller % key);
        boost::lock_guard<boost::mutex> lock(mutex);
        if(m.find(key) == m.end()) {
            m[key] = ptr;
        } else {
            return false;
        }
    }
    return true;
}

bool Executive::_track(int refNum, RequesterPtr r) {
    assert(r);
    {
        LOGF_DEBUG("Executive (%1%) tracking id=%2%" % (void*)this % refNum);
        boost::lock_guard<boost::mutex> lock(_requestersMutex);
        if(_requesters.find(refNum) == _requesters.end()) {
            _requesters[refNum] = r;
        } else {
            return false;
        }
    }
    return true;
}

void Executive::_unTrack(int refNum) {
    bool untracked = false;
    {
        boost::lock_guard<boost::mutex> lock(_requestersMutex);
        RequesterMap::iterator i = _requesters.find(refNum);
        if(i != _requesters.end()) {
            _requesters.erase(i);
            untracked = true;
            if(_requesters.empty()) _requestersEmpty.notify_all();
        }
    }
    if(untracked) {
        LOGF_INFO("Executive (%1%) UNTRACKING id=%2%" % (void*)this % refNum);
    }
}

void Executive::_reapRequesters(boost::unique_lock<boost::mutex> const&) {
    RequesterMap::iterator i, e;
    while(true) {
        bool reaped = false;
        for(i=_requesters.begin(), e=_requesters.end(); i != e; ++i) {
            if(!i->second->getError().msg.empty()) {
                // Requester should have logged the error to the messageStore
                LOGF_INFO("Executive (%1%) REAPED id=%2%"
                          % (void*)this % i->first);
                _requesters.erase(i);
                reaped = true;
                break;
            }
        }
        if(!reaped) {
            break;
        }
    }
}

void Executive::_reportStatuses() {
//    StatusMap::const_iterator i,e;
//    for(i=_statuses.begin(), e=_statuses.end(); i != e; ++i) {
//        ExecStatus::Info info = i->second->getInfo();
    boost::lock_guard<boost::mutex> lock(_entriesMutex);
    EntryMap::const_iterator i,e;
    for(i=_entries.begin(), e=_entries.end(); i != e; ++i) {
        ExecStatus::Info info = i->second.status->getInfo();

        std::ostringstream os;
        os << ExecStatus::stateText(info.state)
           << " " << info.stateCode;
        if(!info.stateDesc.empty()) {
            os << " (" << info.stateDesc << ")";
        }
        os << " " << info.stateTime;
        _messageStore->addMessage(info.resourceUnit.chunk(),
                                  info.state, os.str());
    }
}

void Executive::_waitUntilEmpty() {
    boost::unique_lock<boost::mutex> lock(_requestersMutex);
    int lastCount = -1;
    int count;
    int moreDetailThreshold = 5;
    int complainCount = 0;
    const boost::posix_time::seconds statePrintDelay(5);
    //_printState(LOG_STRM(Debug));
    while(!_requesters.empty()) {
        count = _requesters.size();
        _reapRequesters(lock);
        if(count != lastCount) {
            lastCount = count;
            ++complainCount;
            if (LOG_CHECK_INFO()) {
                std::ostringstream os;
                if(complainCount > moreDetailThreshold) {
                    _printState(os);
                    os << std::endl;
                }
                os << "Still " << count << " in flight.";
                complainCount = 0;
                lock.unlock(); // release the lock while we trigger logging.
                LOGF_INFO("%1%" % os.str());
                lock.lock();
            }
        }
        _requestersEmpty.timed_wait(lock, statePrintDelay);
    }
}

std::ostream& operator<<(std::ostream& os,
                         Executive::StatusMap::value_type const& v) {
    ExecStatus const& es = *(v.second);
    os << v.first << ": " << es;
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         Executive::EntryMap::value_type const& v) {
    ExecStatus const& es = *(v.second.status);
    os << v.first << ": " << es;
    return os;
}

/// precondition: _requestersMutex is held by current thread.
void Executive::_printState(std::ostream& os) {
    std::for_each(_requesters.begin(), _requesters.end(),
                  printMapSecond<RequesterMap::value_type>(os, "\n"));
    os << std::endl << getProgressDesc() << std::endl;

    std::copy(_entries.begin(), _entries.end(),
              std::ostream_iterator<EntryMap::value_type>(os, "\n"));
}

}}} // namespace lsst::qserv::qdisp
