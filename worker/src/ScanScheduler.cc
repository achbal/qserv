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
 /**
  * @file ScanScheduler.cc
  *
  * @brief A scheduler implementation that limits disk scans to one at
  * a time, but allows multiple queries to share I/O.
  *
  * @author Daniel L. Wang, SLAC
  */
#include "lsst/qserv/worker/ScanScheduler.h"
#include <iostream>
#include <sstream>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>

#include "lsst/qserv/worker/ChunkDisk.h"
#include "lsst/qserv/worker/Foreman.h"
#include "lsst/qserv/worker/Logger.h"

lsst::qserv::worker::ScanScheduler* dbgScanScheduler = 0; //< A symbol for gdb
lsst::qserv::worker::ChunkDisk* dbgChunkDisk1 = 0; //< A symbol for gdb

namespace lsst {
namespace qserv {
namespace worker {

////////////////////////////////////////////////////////////////////////
// class ChunkDiskWatcher
// Lets the scheduler listen to a Foreman's Runners and pass to a
// ChunkDisk instance.
////////////////////////////////////////////////////////////////////////
class ChunkDiskWatcher : public Foreman::RunnerWatcher {
public:
    typedef ScanScheduler::ChunkDiskList ChunkDiskList;

    ChunkDiskWatcher(ChunkDiskList& chunkDiskList, boost::mutex& mutex)
        : _disks(chunkDiskList), _mutex(mutex) {}
    virtual void handleStart(Task::Ptr t) {
        boost::lock_guard<boost::mutex> guard(_mutex);
        assert(!_disks.empty());
        _disks.front()->registerInflight(t);
    }
    virtual void handleFinish(Task::Ptr t) {
        boost::lock_guard<boost::mutex> guard(_mutex);
        assert(!_disks.empty());
        _disks.front()->removeInflight(t);
    }
private:
    ChunkDiskList& _disks;
    boost::mutex& _mutex;
};

////////////////////////////////////////////////////////////////////////
// class ScanScheduler
////////////////////////////////////////////////////////////////////////
ScanScheduler::ScanScheduler(Logger::Ptr logger)
    : _maxRunning(32), // FIXME: set to some multiple of system proc count.
      _logger(logger)
{

    _disks.push_back(boost::make_shared<ChunkDisk>(logger));
    dbgChunkDisk1 = _disks.front().get();
    dbgScanScheduler = this;
    assert(!_disks.empty());
}

void ScanScheduler::queueTaskAct(Task::Ptr incoming) {
    boost::lock_guard<boost::mutex> guard(_mutex);
    _enqueueTask(incoming);
}

TaskQueuePtr ScanScheduler::nopAct(TaskQueuePtr running) {
    if(!running) { throw std::invalid_argument("null run list"); }
    boost::lock_guard<boost::mutex> guard(_mutex);
    assert(_integrityHelper());
    int available = _maxRunning - running->size();
    return _getNextTasks(available);
}

/// @return a queue of all tasks ready to run.
///
TaskQueuePtr ScanScheduler::newTaskAct(Task::Ptr incoming,
                                       TaskQueuePtr running) {
    boost::lock_guard<boost::mutex> guard(_mutex);
    assert(_integrityHelper());
    if(!running) { throw std::invalid_argument("null run list"); }

    _enqueueTask(incoming);
    // No free threads? Exit.
    // FIXME: Can do an I/O bound scan if there is memory and an idle
    // spindle.
    int available = _maxRunning - running->size();
    if(available <= 0) {
        return TaskQueuePtr();
    }
    return _getNextTasks(available);
}

TaskQueuePtr ScanScheduler::taskFinishAct(Task::Ptr finished,
                                          TaskQueuePtr running) {

    boost::lock_guard<boost::mutex> guard(_mutex);
    assert(_integrityHelper());

    // No free threads? Exit.
    // FIXME: Can do an I/O bound scan if there is memory and an idle
    // spindle.
    std::ostringstream os;
    os << "Completed: " << "(" << finished->msg->chunkid()
       << ")" << finished->msg->fragment(0).query(0);
    _logger->debug(os.str());
    int available = _maxRunning - running->size();
    if(available <= 0) {
        return TaskQueuePtr();
    }
    return _getNextTasks(available);
}

boost::shared_ptr<Foreman::RunnerWatcher> ScanScheduler::getWatcher() {
    boost::shared_ptr<ChunkDiskWatcher> w;
    w.reset(new ChunkDiskWatcher(_disks, _mutex));
    return w;
}

/// @return true if data is okay.
bool ScanScheduler::checkIntegrity() {
    boost::lock_guard<boost::mutex> guard(_mutex);
    return _integrityHelper();
}

/// @return true if data is okay
/// precondition: _mutex is locked.
bool ScanScheduler::_integrityHelper() {
    ChunkDiskList::iterator i, e;
    for(i=_disks.begin(), e=_disks.end(); i != e; ++i) {
        if(!(**i).checkIntegrity()) return false;
    }
    return true;
}

/// Precondition: _mutex is already locked.
/// @return new tasks to run
/// TODO: preferential treatment for chunkId just run?
/// or chunkId that are currently running?
TaskQueuePtr ScanScheduler::_getNextTasks(int max) {
    // FIXME: Select disk based on chunk location.
    assert(!_disks.empty());
    assert(_disks.front());
    std::ostringstream os;
    os << "_getNextTasks(" << max << ")>->->";
    _logger->debug(os.str());
    os.str("");
    TaskQueuePtr tq;
    ChunkDisk& disk = *_disks.front();

    // Check disks for candidate ones.
    // Pick one. Prefer a less-loaded disk: want to make use of i/o
    // from both disks. (for multi-disk support)
    bool allowNewChunk = (!disk.busy() && !disk.empty());
    while(max > 0) {
        Task::Ptr p = disk.getNext(allowNewChunk);
        if(!p) { break; }
        allowNewChunk = false; // Only allow one new chunk
        if(!tq) {
            tq.reset(new TaskQueue());
        }
        tq->push_back(p);

        os << "Making ready: " << *(tq->front());
        _logger->debug(os.str());
        os.str("");
        --max;
    }
    if(tq) {
        os << "Returning " << tq->size() << " to launch";
        _logger->debug(os.str());
    }
    assert(_integrityHelper());
    _logger->debug("_getNextTasks <<<<<");
    return tq;
}

/// Precondition: _mutex is locked.
void ScanScheduler::_enqueueTask(Task::Ptr incoming) {
    if(!incoming) { throw std::invalid_argument("No task to enqueue"); }
    // FIXME: Select disk based on chunk location.
    assert(!_disks.empty());
    assert(_disks.front());
    _disks.front()->enqueue(incoming);
    std::ostringstream os;
    TaskMsg const& msg = *(incoming->msg);
    os << "Adding new task: " << msg.chunkid()
       << " : " << msg.fragment(0).query(0);
    _logger->debug(os.str());
}

}}} // lsst::qserv::worker
