/* 
 * LSST Data Management System
 * Copyright 2012-2013 LSST Corporation.
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
  * @file FifoScheduler.cc
  *
  * @brief A simple scheduler implementation for ordering query tasks
  * to send to the mysqld.
  *
  * @author Daniel L. Wang, SLAC
  */ 
#include "lsst/qserv/worker/FifoScheduler.h"
#include <boost/thread.hpp>
// #include <iostream> // Enable for debugging.

namespace qWorker = lsst::qserv::worker;
typedef qWorker::Foreman::TaskQueuePtr TaskQueuePtr;

qWorker::FifoScheduler::FifoScheduler() 
    : _maxRunning(4) 
      // FIXME: _maxRunning needs some design. The optimal value can
      // be quite complex, and is probably dynamic. This is noted as a
      // long-term design issue on
      // https://dev.lsstcorp.org/trac/wiki/db/Qserv/WorkerParallelism
{}    

TaskQueuePtr qWorker::FifoScheduler::nopAct(TodoList::Ptr todo, 
                                            TaskQueuePtr running) {
    // For now, do nothing when there is no event.  

    // Perhaps better: Check to see how many are running, and schedule
    // a task if the number of running jobs is below a threshold.
    return TaskQueuePtr();
}

TaskQueuePtr qWorker::FifoScheduler::newTaskAct(Task::Ptr incoming,
                                                TodoList::Ptr todo, 
                                                TaskQueuePtr running) {
    boost::lock_guard<boost::mutex> guard(_mutex);
    TaskQueuePtr tq;
    assert(running.get());
    assert(todo.get());
    assert(incoming.get());
    if(running->size() < _maxRunning) { // if we have space, start
                                      // running.
        // Prefer tasks already in the todo list, although there
        // shouldn't be... 
        tq.reset(new TodoList::TaskQueue());
        boost::shared_ptr<qWorker::Task> t = todo->popTask();
        if(t) {
            tq->push_back(t);
        } else {
            tq->push_back(todo->popTask(incoming));
        }
        //std::cout << "FIFO scheduling " << *(tq->front()) << std::endl;
        return tq;
    }
    return TaskQueuePtr();
}

TaskQueuePtr qWorker::FifoScheduler::taskFinishAct(Task::Ptr finished,
                                                   TodoList::Ptr todo, 
                                                   TaskQueuePtr running) {
    boost::lock_guard<boost::mutex> guard(_mutex);
    TaskQueuePtr tq;
    assert(running.get());
    assert(todo.get());
    assert(finished.get());

    // FIFO always replaces a finishing task with a new task, always
    // maintaining a constant number of running threads (as long as
    // there is work to do)    

    // Try to pop a task, checking if we got a null Task.  This is
    // better than a size-check followed by pop (because the size may
    // change between the two calls).
    boost::shared_ptr<qWorker::Task> t = todo->popTask();
    if(t) {
        tq.reset(new TodoList::TaskQueue());
        tq->push_back(t);
        return tq;
    } 
    // No more work to do--> don't schedule anything.
    return TaskQueuePtr();
}    
