// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015-2016 LSST Corporation.
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
 *
 *  @author: John Gates,
 */

// Class header
#include "util/EventThread.h"

// System headers
#include <algorithm>

// Third-party headers

// LSST headers
#include "lsst/log/Log.h"

namespace {
LOG_LOGGER _log = LOG_GET("lsst.qserv.util.EventThread");
}

namespace lsst {
namespace qserv {
namespace util {

/// Handle commands as they arrive until queEnd() is called.
void EventThread::handleCmds() {
    startup();
    while(_loop) {
        Command::Ptr cmd = _q->getCmd();
        if (cmd != nullptr) {
            _q->commandStart(cmd);
            cmd->action(this);
            _q->commandFinish(cmd);
            // Reset _func in case it has a captured Command::Ptr,
            // which would keep it alive indefinitely.
            cmd->resetFunc();
        }
    }
    finishup();
}

/// call this to start the thread
void EventThread::run() {
    std::thread t{&EventThread::handleCmds, this};
    _t = std::move(t);
}

PoolEventThread::~PoolEventThread() {
    LOGS(_log, LOG_LVL_DEBUG, "PoolEventThread::~PoolEventThread()");
}

void PoolEventThread::finishup() {
    // 'pet' will keep this PoolEventThread instance alive until this thread completes,
    // otherwise it would likely be deleted when _threadPool->release(this) is called.
    // It's probably overly cautious but it is safe.
    PoolEventThread::Ptr pet = _threadPool->release(this);
    if (pet != nullptr) {
        auto f = [pet](){ pet->join(); };
        std::thread t{f};
        t.detach();
    } else {
        LOGS(_log, LOG_LVL_WARN, "The pool failed to find this PoolEventThread.");
    }
}

ThreadPool::~ThreadPool() {
    endAll();
    waitForResize(0);
}

/// Release the thread from the thread pool and return a shared pointer to the
/// released thread.
PoolEventThread::Ptr ThreadPool::release(PoolEventThread *thrd) {
    // Search for the the thread to free
    auto func = [thrd](PoolEventThread::Ptr const& pt)->bool {
        return pt.get() == thrd;
    };

    PoolEventThread::Ptr thrdPtr;
    {
        std::lock_guard<std::mutex> lock(_poolMutex);
        auto iter = std::find_if(_pool.begin(), _pool.end(), func);
        if (iter == _pool.end()) {
            LOGS(_log, LOG_LVL_WARN, "ThreadPool::release thread not found " << thrd);
        } else {
            thrdPtr = *iter;
            LOGS(_log, LOG_LVL_DEBUG, "ThreadPool::release erasing " << thrd);
            _pool.erase(iter);
        }
    }
    _resize(); // Check if more threads need to be released.
    return thrdPtr;
}

/// Change the size of the thread pool.
void ThreadPool::resize(unsigned int targetThrdCount) {
    {
        std::lock_guard<std::mutex> lock(_countMutex);
        _targetThrdCount = targetThrdCount;
    }
    _resize();
}

/// Do the work of changing the size of the thread pool.
/// Making the pool larger is just a matter of adding threads.
/// Shrinking the pool requires ending one thread at a time.
void ThreadPool::_resize() {
    std::lock_guard<std::mutex> lock(_poolMutex);
    auto target = getTargetThrdCount();
    while (target > _pool.size()) {
        auto t = std::make_shared<PoolEventThread>(shared_from_this(), _q);
        _pool.push_back(t);
        t->run();
    }
    // Shrinking the thread pool is much harder. Adding a message to end one thread
    // is sent. When that thread ends, it calls release(), which will then call
    // this function again to check if more threads need to be ended.
    if (target < _pool.size()) {
        auto thrd = _pool.front();
        if (thrd != nullptr) {
            LOGS(_log, LOG_LVL_DEBUG, "ThreadPool::_resize sending thrd->queEnd()");
            thrd->queEnd(); // Since all threads share the same queue, this could be answered by any thread.
        } else {
            LOGS(_log, LOG_LVL_WARN, "ThreadPool::_resize thrd == nullptr");
        }
    }
    LOGS(_log, LOG_LVL_DEBUG, "_resize target=" << target << " size=" << _pool.size());
    {
        std::unique_lock<std::mutex> countlock(_countMutex);
        _countCV.notify_all();
    }
}

/// Wait for the pool to reach the _targetThrdCount number of threads.
/// It will wait forever if 'millisecs' is zero, otherwise it will timeout
/// after that number of milliseconds.
/// Note that this wont detect changes to _targetThrdCount.
void ThreadPool::waitForResize(int millisecs) {
    auto eqTest = [this](){ return _targetThrdCount == _pool.size(); };
    std::unique_lock<std::mutex> lock(_countMutex);
    if (millisecs > 0) {
        _countCV.wait_for(lock, std::chrono::milliseconds(millisecs), eqTest);
    } else {
        _countCV.wait(lock, eqTest);
    }
}

}}} // namespace


