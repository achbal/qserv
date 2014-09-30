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
  * @brief Executive. It executes things.
  *
  * @author Daniel L. Wang, SLAC
  */

#include "qdisp/Executive.h"

// System headers
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

// Third-party headers
#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiService.hh" // Resource
#include "XrdOuc/XrdOucTrace.hh"

// LSST headers
#include "lsst/log/Log.h"

// Local headers
#include "global/ResourceUnit.h"
#include "log/msgCode.h"
#include "qdisp/MessageStore.h"
#include "qdisp/MergeAdapter.h"
#include "qdisp/QueryResource.h"

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

std::ostream& operator<<(std::ostream& os, QueryReceiver const& qr) {
    return qr.print(os);
}

template <typename Ptr>
struct printMapSecond {
    printMapSecond(std::ostream& os_, std::string sep_)
        : os(os_), sep(sep_), first(true)  {}

    void operator()(Ptr const& p) {
        if(!first) { os << sep; }
        os << *(p.second);
        first = true;
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
    LOGF_INFO("EXECUTING Executive::add(TransactionSpec, %1%)" % 
              resultName);
    Spec s;
    s.resource = ResourceUnit(t.path);
    if(s.resource.unitType() == ResourceUnit::CQUERY) {
        s.resource.setAsDbChunk(s.resource.db(), s.resource.chunk());
    }
    s.request = t.query;
    // FIXME: finish this out if we want to do a hybrid
    // old/new dispatch to new workers.
    s.receiver = MergeAdapter::newInstance(); //savePath, resultName);
    add(refNum, s);
}

class Executive::DispatchAction : public util::VoidCallable<void> {
public:
    typedef boost::shared_ptr<DispatchAction> Ptr;
    DispatchAction(Executive& executive,
                   int refNum,
                   std::string const& path,
                   std::string const& payload,
                   boost::shared_ptr<QueryReceiver> receiver,
                   ExecStatus& status)
        : _executive(executive), _refNum(refNum), _path(path),
          _payload(payload), _receiver(receiver), _status(status) {
    }
    virtual ~DispatchAction() {}
    virtual void operator()() {
        if(_receiver->reset()) { // Must be able to reset receiver state
            _executive._dispatchQuery(_refNum, _path, _payload,
                                      _receiver, _status);
        } else { // Do nothing-- can't retry.
        }
    }
private:
    Executive& _executive;
    int _refNum;
    std::string const _path;
    std::string const _payload;
    boost::shared_ptr<QueryReceiver> _receiver;
    ExecStatus& _status; ///< Reference back to exec status
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

    bool trackOk =_track(refNum, s.receiver); // Remember so we can join.
    if(!trackOk) {
        LOGF_WARN("Ignoring duplicate add(%1%)" % refNum);
        return;
    }
    ExecStatus& status = _insertNewStatus(refNum, s.resource);
    ++_requestCount;
    std::string msg = "Exec add pth=" + s.resource.path();
    LOGF_INFO("%1%" % msg);
    _messageStore->addMessage(s.resource.chunk(), log::MSG_MGR_ADD, msg);

    _dispatchQuery(refNum,
                   s.resource.path(),
                   s.request,
                   s.receiver,
                   status);
}

bool Executive::join() {
    // To join, we make sure that all of the chunks added so far are complete.
    // Check to see if _receivers is empty, if not, then sleep on a condition.
    _waitUntilEmpty();
    // Okay to merge. probably not the Executive's responsibility
    struct successF {
        static bool f(Executive::StatusMap::value_type const& entry) {
            ExecStatus::Info const& esI = entry.second->getInfo();
            LOGGER_INF << "entry state:" << (void*)entry.second.get()
                       << " " << esI << std::endl;
            return esI.state == ExecStatus::RESPONSE_DONE; } };
    int sCount = std::count_if(_statuses.begin(), _statuses.end(), successF::f);

    LOGF_INFO("Query exec finish. %1% dispatched." % _requestCount);
    _reportStatuses();
    if(sCount != _requestCount) {
        LOGGER_INF << "Query exec error:. " << _requestCount << " !="
                   << sCount << std::endl;
    }
    return sCount == _requestCount;
}

void Executive::markCompleted(int refNum, bool success) {
    QueryReceiver::Error e;
    LOGF_INFO("Executive::markCompletion(%1%,%2%)" % refNum % success);
    if(!success) {
        boost::lock_guard<boost::mutex> lock(_receiversMutex);
        ReceiverMap::iterator i = _receivers.find(refNum);
        if(i != _receivers.end()) {
            e = i->second->getError();
        } else {
            LOGF_ERROR("Executive(%1%) failed to find tracked id=%2% size=%3%" %
                       (void*)this % refNum % _receivers.size());
            assert(i != _receivers.end());
        }
        _statuses[refNum]->report(ExecStatus::RESULT_ERROR, 1);
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
    boost::lock_guard<boost::mutex> lock(_receiversMutex);
    ReceiverMap::iterator i = _receivers.find(refNum);
    if(i != _receivers.end()) {
        QueryReceiver::Error e = i->second->getError();
        LOGF_WARN("Warning, requestSquash(%1%), but %2% has already failed (%3%, %4%)." % refNum % refNum % e.code % e.msg);
    } else {
        i->second->cancel();
    }
}

void Executive::squash() {
    LOGF_INFO("Trying to cancel all queries...");
    std::vector<ReceiverPtr> pendingReceivers;
    {
        boost::lock_guard<boost::mutex> lock(_receiversMutex);
        std::ostringstream os;
        os << "STATE=";
        _printState(os);
        LOGF_INFO("%1%\nLoop cancel all queries..." % os.str());
        ReceiverMap::iterator i,e;
        for(i=_receivers.begin(), e=_receivers.end(); i != e; ++i) {
            pendingReceivers.push_back(i->second);
        }
        LOGF_INFO("Enqueued receivers for cancelling...done");
    }
    {
        std::vector<ReceiverPtr>::iterator i, e;
        for(i=pendingReceivers.begin(), e=pendingReceivers.end(); i != e; ++i) {
            // Could get stuck because it waits on xrootd,
            // which may be waiting on a thread blocked in _unTrack().
            // Don't do this while holding _receiversMutex
            (**i).cancel();
        }
        LOGGER_INF << "Cancelled all query receivers...done\n";
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
    std::ostream& os;
    std::string const& sep;
    bool first;
};

int Executive::getNumInflight() {
    boost::unique_lock<boost::mutex> lock(_receiversMutex);
    return _receivers.size();
}

std::string Executive::getProgressDesc() const {
    std::ostringstream os;
    std::for_each(_statuses.begin(), _statuses.end(), printMapEntry(os, "\n"));
    LOGF_ERROR("%1%" % os.str());
    return os.str();
}

////////////////////////////////////////////////////////////////////////
// class Executive (private)
////////////////////////////////////////////////////////////////////////
void Executive::_dispatchQuery(int refNum,
                              std::string const& path,
                              std::string const& payload,
                              boost::shared_ptr<QueryReceiver> receiver,
                              ExecStatus& status) {
    boost::shared_ptr<DispatchAction> retryFunc;
    if(_shouldRetry(refNum)) { // limit retries for each request.
        retryFunc.reset(new DispatchAction(*this, refNum,
                                           path, payload, receiver,
                                           status));
    }
    QueryResource* r = new QueryResource(path,
                                         payload,
                                         receiver,
                                         NotifyExecutive::newInstance(*this, refNum),
                                         retryFunc,
                                         status);
    status.report(ExecStatus::PROVISION);
    bool provisionOk = _service->Provision(r);  // 2
    if(!provisionOk) {
        LOGF_ERROR("Resource provision error %1%" % path);
        populateState(status, ExecStatus::PROVISION_ERROR, r->eInfo);
        _unTrack(refNum);
        delete r;
        // handle error
    }
    LOGF_DEBUG("Provision was ok");
}

void Executive::_setup() {
    XrdSsi::Trace.What = TRACE_ALL | TRACE_Debug;

    XrdSsiErrInfo eInfo;
    _service = XrdSsiGetClientService(eInfo, _config.serviceUrl.c_str()); // Step 1
    if(!_service) {
        LOGGER_ERR << "Error obtaining XrdSsiService in Executive: "
                   << getErrorText(eInfo);
    }
    assert(_service);
    _requestCount = 0;
}

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
    ExecStatus::Ptr es(new ExecStatus(r));
    _statuses.insert(StatusMap::value_type(refNum, es));
    return *es;
}

bool Executive::_track(int refNum, ReceiverPtr r) {
    assert(r);
    {
        boost::lock_guard<boost::mutex> lock(_receiversMutex);
        LOGF_DEBUG("Executive (%1%) tracking id=%2%" % (void*)this % refNum);
        if(_receivers.find(refNum) == _receivers.end()) {
            _receivers[refNum] = r;
        } else {
            return false;
        }
    }
    return true;
}

void Executive::_unTrack(int refNum) {
    boost::lock_guard<boost::mutex> lock(_receiversMutex);
    ReceiverMap::iterator i = _receivers.find(refNum);
    if(i != _receivers.end()) {
        LOGF_INFO("Executive (%1%) UNTRACKING id=%2%" % (void*)this % refNum);
        _receivers.erase(i);
        if(_receivers.empty()) _receiversEmpty.notify_all();
    }
}

void Executive::_reapReceivers(boost::unique_lock<boost::mutex> const&) {
    ReceiverMap::iterator i, e;
    while(true) {
        bool reaped = false;
        for(i=_receivers.begin(), e=_receivers.end(); i != e; ++i) {
            if(!i->second->getError().msg.empty()) {
                // Receiver should have logged the error to the messageStore
                LOGF_INFO("Executive (%1%) REAPED id=%2%" % (void*)this % i->first);
                _receivers.erase(i);
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
    StatusMap::const_iterator i,e;
    for(i=_statuses.begin(), e=_statuses.end(); i != e; ++i) {
        ExecStatus::Info info = i->second->getInfo();

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
    boost::unique_lock<boost::mutex> lock(_receiversMutex);
    int lastCount = -1;
    int count;
    int moreDetailThreshold = 5;
    int complainCount = 0;
    const boost::posix_time::seconds statePrintDelay(5);
    //_printState(LOG_STRM(Debug));
    while(!_receivers.empty()) {
        count = _receivers.size();
        _reapReceivers(lock);
        if(count != lastCount) {
            LOGF_INFO("Still %1% in flight." % count);
            count = lastCount;
            ++complainCount;
            if(complainCount > moreDetailThreshold) {
                if (LOG_CHECK_WARN()) {
                    std::stringstream ss;
                    _printState(ss);
                    LOGF_WARN("%1%" % ss.str());
                }
                complainCount = 0;
            }
        }
        _receiversEmpty.timed_wait(lock, statePrintDelay);
    }
}

std::ostream& operator<<(std::ostream& os,
                         Executive::StatusMap::value_type const& v) {
    ExecStatus const& es = *(v.second);
    os << v.first << ": " << es;
    return os;
}

/// precondition: _receiversMutex is held by current thread.
void Executive::_printState(std::ostream& os) {
    std::for_each(_receivers.begin(), _receivers.end(),
                  printMapSecond<ReceiverMap::value_type>(os, "\n"));
    os << std::endl << getProgressDesc() << std::endl;

    std::copy(_statuses.begin(), _statuses.end(),
              std::ostream_iterator<StatusMap::value_type>(os, "\n"));
}

}}} // namespace lsst::qserv::qdisp
