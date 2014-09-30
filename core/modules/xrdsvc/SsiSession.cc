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
#include "xrdsvc/SsiSession.h"

// System headers
#include <iostream>
#include <string>

// Third-party
#include "XrdSsi/XrdSsiRequest.hh"

// Qserv headers
#include "global/ResourceUnit.h"
#include "lsst/log/Log.h"
#include "proto/ProtoImporter.h"
#include "proto/worker.pb.h"
#include "util/Timer.h"
#include "wbase/MsgProcessor.h"
#include "wbase/SendChannel.h"
#include "xrdsvc/SsiSession_ReplyChannel.h"

namespace lsst {
namespace qserv {
namespace xrdsvc {

typedef proto::ProtoImporter<proto::TaskMsg> Importer;
typedef boost::shared_ptr<Importer> ImporterPtr;

////////////////////////////////////////////////////////////////////////
// class SsiProcessor
////////////////////////////////////////////////////////////////////////

/// Feed ProtoImporter results to msgprocessor by bundling the responder as a
/// SendChannel
class SsiProcessor : public Importer::Acceptor {
public:
    typedef boost::shared_ptr<SsiProcessor> Ptr;

    SsiProcessor(ResourceUnit const& ru,
                 wbase::MsgProcessor::Ptr mp,
                 boost::shared_ptr<wbase::SendChannel> sc,
                 std::vector<SsiSession::CancelFuncPtr>& cancellers)
        : _ru(ru),
          _msgProcessor(mp),
          _sendChannel(sc),
          _cancellers(cancellers) {}

    virtual void operator()(boost::shared_ptr<proto::TaskMsg> m) {
        util::Timer t;
        if(m->has_db() && m->has_chunkid()
           && (_ru.db() == m->db()) && (_ru.chunk() == m->chunkid())) {
            t.start();
            SsiSession::CancelFuncPtr p = (*_msgProcessor)(m, _sendChannel);
            _cancellers.push_back(p);
            t.stop();
            std::cerr << "SsiProcessor msgProcessor call took "
                      << t.getElapsed() << " seconds" << std::endl;
        } else {
            std::ostringstream os;
            os << "Mismatched db/chunk in msg on resource db="
               << _ru.db() << " chunkId=" << _ru.chunk();
            _sendChannel->sendError(os.str().c_str(), EINVAL);
        }
    }
private:
    ResourceUnit const& _ru;
    wbase::MsgProcessor::Ptr _msgProcessor;
    boost::shared_ptr<wbase::SendChannel> _sendChannel;
    std::vector<SsiSession::CancelFuncPtr>& _cancellers;
};
////////////////////////////////////////////////////////////////////////
// class SsiSession
////////////////////////////////////////////////////////////////////////

// Step 4
/// Called by XrdSsi to actually process a request.
bool
SsiSession::ProcessRequest(XrdSsiRequest* req, unsigned short timeout) {
    util::Timer t;
    // Figure out what the request is.
    LOGF_INFO("ProcessRequest, service=%1%" % sessName);
    t.start();
    BindRequest(req, this); // Step 5
    t.stop();
    LOGF_INFO("BindRequest took %1% seconds" % t.getElapsed());

    char *reqData = 0;
    int   reqSize;
    t.start();
    reqData = req->GetRequest(reqSize);
    t.stop();
    LOGF_INFO("GetRequest took %1% seconds" % t.getElapsed());

    LOGF_INFO("### %1% byte request: %2%" % reqSize %
            std::string(reqData, reqSize));
    ResourceUnit ru(sessName);
    if(ru.unitType() == ResourceUnit::DBCHUNK) {
        if(!(*_validator)(ru)) {
            LOGF_WARN("WARNING: unowned chunk query detected: %1%" % ru.path());
            //error.setErrInfo(ENOENT, "File does not exist");
            return false;
        }

        t.start();
        enqueue(ru, reqData, reqSize);
        t.stop();
        LOGF_INFO("SsiSession::enqueue took %1% seconds" % t.getElapsed());

        ReleaseRequestBuffer();
    } else {
        // Ignore this request.
        // Should send an error...
        LOGF_INFO("TODO: Should send an error for Garbage request: %1%" %
                sessName);
        ReleaseRequestBuffer();
        return false;
    }
    return true;
}

/// Called by XrdSsi to free resources.
void
SsiSession::RequestFinished(XrdSsiRequest* req, XrdSsiRespInfo const& rinfo,
                            bool cancel) { // Step 8
    // This call is sync (blocking).
    // client finished retrieving response, or cancelled.
    // release response resources (e.g. buf)

    if(cancel) {
        // Do cancellation.
        typedef std::vector<CancelFuncPtr>::iterator Iter;
        for(Iter i=_cancellers.begin(), e=_cancellers.end(); i != e; ++i) {
            assert(*i);
            (**i)();
        }
    }
    // No buffers allocated, so don't need to free.
    // We can release/unlink the file now
    const char* type = "";
    switch(rinfo.rType) {
    case XrdSsiRespInfo::isNone: type = "type=isNone"; break;
    case XrdSsiRespInfo::isData: type = "type=isData"; break;
    case XrdSsiRespInfo::isError: type = "type=isError"; break;
    case XrdSsiRespInfo::isFile: type = "type=isFile"; break;
    case XrdSsiRespInfo::isStream: type = "type=isStream"; break;
    }
    // We can't do much other than close the file.
    // It should work (on linux) to unlink the file after we open it, though.
    LOGF_INFO("RequestFinished %1%" % type);
}

bool
SsiSession::Unprovision(bool forced) {
    // all requests guaranteed to be finished or cancelled.
    delete this;
    return true; // false if we can't unprovision now.
}

/// Accept an incoming request addressed to a ResourceUnit, with the particulars
/// defined in the reqData payload
void SsiSession::enqueue(ResourceUnit const& ru, char* reqData, int reqSize) {

    // reqData has the entire request, so we can unpack it without waiting for
    // more data.
    ReplyChannel::Ptr rc(new ReplyChannel(*this));
    SsiProcessor::Ptr sp(new SsiProcessor(ru, _processor, rc, _cancellers));
    LOGF_INFO("Importing TaskMsg of size %1%" % reqSize);
    proto::ProtoImporter<proto::TaskMsg> pi(sp);

    pi(reqData, reqSize);
    if(pi.getNumAccepted() < 1) {
        // TODO Report error.
    } else {
        LOGF_ERROR("enqueued task ok: %1%" % ru);
    }
}

}}} // lsst::qserv::xrdsvc
