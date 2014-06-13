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

#ifndef LSST_QSERV_PROTO_PROTOIMPORTER_H
#define LSST_QSERV_PROTO_PROTOIMPORTER_H

// System headers
#include <string>

// Third-party headers
#include <boost/shared_ptr.hpp>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

// Qserv headers
#include "util/Callable.h"

namespace lsst {
namespace qserv {
namespace proto {

/// ProtoImporter
/// Minimal-copy import of an arbitrary proto msg from a raw buffer.
/// Example:
/// struct TaskMsgAcceptor : public ProtoImporter<TaskMsg> {
///  virtual void operator()(boost::shared_ptr<TaskMsg> m) { ...}
/// };
/// ProtoImporter<TaskMsg> p(boost::shared_ptr<TaskMsgAcceptor>());
/// p(data,size); // calls operator() defined above.
template <typename Msg>
class ProtoImporter {
public:
    typedef util::UnaryCallable<void, boost::shared_ptr<Msg> > Acceptor;
    typedef boost::shared_ptr<Acceptor> AcceptorPtr;

    ProtoImporter(AcceptorPtr a) : _numAccepted(0), _acceptor(a) {}
    
    bool operator()(char* data, int size) {
        namespace gio = google::protobuf::io;

        boost::shared_ptr<Msg> m(new Msg());
        gio::ArrayInputStream input(data, size);
        gio::CodedInputStream coded(&input);
        //        m->MergePartialFromCodedStream(&coded);
        bool isClean = m->MergeFromCodedStream(&coded);
        if(isClean) {
            (*_acceptor)(m);
            ++_numAccepted;
        }
        return isClean;
    }
    inline int numAccepted() const { return _numAccepted; }

private:
    int _numAccepted;
    AcceptorPtr _acceptor;
};
}}} // lsst::qserv::proto

#endif // #define LSST_QSERV_PROTO_PROTOIMPORTER_H

