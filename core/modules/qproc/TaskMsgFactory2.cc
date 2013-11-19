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
  * @file TaskMsgFactory2.cc
  *
  * @brief TaskMsgFactory2 is a factory for TaskMsg (protobuf)
  * objects. This functionality exists in the python later as
  * TaskMsgFactory, but we are pushing the functionality to C++ so
  * that we can avoid the Python/C++ for each chunk query. This should
  * dramatically improve query dispatch speed (and also reduce overall
  * user query latency).
  *
  * @author Daniel L. Wang, SLAC
  */ 
#include "qproc/TaskMsgFactory2.h"

#include "qdisp/ChunkQuerySpec.h"
#include "proto/worker.pb.h"

namespace qMaster=lsst::qserv::master;

namespace { // File-scope helpers
}

namespace lsst {
namespace qserv {
namespace master {
////////////////////////////////////////////////////////////////////////
// class TaskMsgFactory2::Impl
////////////////////////////////////////////////////////////////////////
class TaskMsgFactory2::Impl {
public:
    Impl(int session, std::string const& resultTable) 
        : _session(session), _resultTable(resultTable) {
    }
    boost::shared_ptr<TaskMsg> makeMsg(ChunkQuerySpec const& s,
                                       std::string const& chunkResultName);
private:
    template <class C1, class C2, class C3>
    void addFragment(TaskMsg& m, std::string const& resultName,
                     C1 const& subChunkTables,
                     C2 const& subChunkIds,
                     C3 const& queries) {
        TaskMsg::Fragment* frag = m.add_fragment();
        frag->set_resulttable(resultName);
        // For each query, apply: frag->add_query(q)
        for(typename C3::const_iterator i=queries.begin(); 
            i != queries.end(); ++i) {
            frag->add_query(*i); 
        }
        TaskMsg_Subchunk sc;
        for(typename C1::const_iterator i=subChunkTables.begin();
            i != subChunkTables.end(); ++i) {
            sc.add_table(*i);
        }
        // For each int in subChunks, apply: sc.add_subchunk(1000000); 
        std::for_each(
            subChunkIds.begin(), subChunkIds.end(), 
            std::bind1st(std::mem_fun(&TaskMsg_Subchunk::add_id), &sc));
        frag->mutable_subchunks()->CopyFrom(sc);
    }
    
    int _session;
    std::string _resultTable;
    boost::shared_ptr<TaskMsg> _taskMsg;
};

boost::shared_ptr<TaskMsg> 
TaskMsgFactory2::Impl::makeMsg(ChunkQuerySpec const& s, 
                               std::string const& chunkResultName) {
        
    std::string resultTable = _resultTable;
    if(!chunkResultName.empty()) { resultTable = chunkResultName; }
    _taskMsg.reset(new TaskMsg);
    // shared
    _taskMsg->set_session(_session);
    _taskMsg->set_db(s.db);
    // per-chunk
    _taskMsg->set_chunkid(s.chunkId);
    // per-fragment
    if(s.nextFragment.get()) {
        ChunkQuerySpec const* sPtr = &s;
        while(sPtr) {
            // Linked fragments will not have valid subChunkTables vectors,
            // So, we reuse the root fragment's vector.
            addFragment(*_taskMsg, resultTable, 
                        s.subChunkTables,    
                        sPtr->subChunkIds, 
                        sPtr->queries);
            sPtr = sPtr->nextFragment.get();
        }
    } else {
        addFragment(*_taskMsg, resultTable, 
                    s.subChunkTables, s.subChunkIds, s.queries);
    }
    return _taskMsg;
}


////////////////////////////////////////////////////////////////////////
// class TaskMsgFactory2
////////////////////////////////////////////////////////////////////////
TaskMsgFactory2::TaskMsgFactory2(int session) 
    : _impl(new Impl(session, "Asdfasfd" )) {

}
void TaskMsgFactory2::serializeMsg(ChunkQuerySpec const& s,
                                   std::string const& chunkResultName,
                                   std::ostream& os) {
    boost::shared_ptr<TaskMsg> m = _impl->makeMsg(s, chunkResultName);
    m->SerializeToOstream(&os);
}
      
}}} // namespace lsst::qserv::master
