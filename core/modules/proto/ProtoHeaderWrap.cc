// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015 LSST Corporation.
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

// System headers

// Third-party headers
#include "lsst/log/Log.h"

// Qserv headers
#include "proto/ProtoHeaderWrap.h"
#include "util/common.h"




namespace lsst {
namespace qserv {
namespace proto {

// 255 is the maximum size of the proto header and we need 1 byte for message size.
const size_t ProtoHeaderWrap::PROTO_HEADER_SIZE = 256;
// Google protobuffers are more efficient below this size.
const size_t ProtoHeaderWrap::PROTOBUFFER_DESIRED_LIMIT = 2000000;
// A single Google protobuffer can't be larger than this.
const size_t ProtoHeaderWrap::PROTOBUFFER_HARD_LIMIT = 64000000;

std::string ProtoHeaderWrap::wrap(std::string& protoHeaderString) {
    unsigned char phSize = static_cast<unsigned char>(protoHeaderString.size());
    std::string msgBuf{phSize};
    msgBuf += protoHeaderString;
    while (msgBuf.size() < PROTO_HEADER_SIZE) {
        msgBuf +='0'; // pad the buffer
    }
    LOGF_INFO("msgBuf size=%1% -> %2%" % msgBuf.size() % util::prettyCharList(msgBuf, 5));
    return msgBuf;
}

bool ProtoHeaderWrap::unwrap(std::shared_ptr<proto::WorkerResponse>& response, std::vector<char>& buffer) {
    response->headerSize = static_cast<unsigned char const>(buffer[0]);
    if(!proto::ProtoImporter<proto::ProtoHeader>::setMsgFrom(
            response->protoHeader, &buffer[1], response->headerSize)) {
	LOGF_ERROR("ProtoHeaderWrap::unwrap failed to parse protobuffer");
        return false;
    }
    LOGF_INFO("buffer size=%1% -> %2%" % buffer.size() % util::prettyCharList(buffer, 5));
    return true;
}


}}} // end namespace

